#include "Events/HScaleEventsDriver.h"

#include "Events/HScaleEventsSerializer.h"
#include "Net/DataChannel.h"
#include "Net/Core/Trace/NetTrace.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "ReplicationLayer/HScaleActorChannel.h"
#include "Net/RepLayout.h"
#include "Utils/HScaleStatics.h"

void FHScaleEventsDriver::HandleApplicationEvents(const quark::remote_event& Update)
{
	if (!IsValid(Connection)) return;
	TObjectPtr<UHScaleNetDriver> NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
	if (!IsValid(NetDriver)) return;
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	if (!IsValid(PkgMap)) return;

	UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received event of type %d and sender isplayer %d of size %llu"), Update.event_class(), Update.sender().is_player(), Update.size())

	if (Update.size() == 0)
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received event is of empty size exiting"))
		return;
	}

	const uint8_t* Data = Update.data();
	FBitReader Reader(Data, Update.size() * 8);

	FHScaleNetGUID ReceiverId;
	Reader << ReceiverId;

	UE_LOG(Log_HyperScaleEvents, VeryVerbose, TEXT("Received event for HScaleNetGUID %s"), *ReceiverId.ToString())

	UObject* Object = PkgMap->FindObjectFromEntityID(ReceiverId);
	if (!Object)
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received event for unknown EntityId %s"), *ReceiverId.ToString())
		return;
	}
	const uint16 EventId = Update.event_class();
	const uint16 Handle = FHScalePropertyIdConverters::GetHandleFromAppEventId(EventId);

	const FClassNetCache* ClassCache = NetDriver->NetCache->GetClassNetCache(Object->GetClass());

	const FFieldNetCache* FieldNetCache = ClassCache->GetFromIndex(Handle);
	if (!FieldNetCache)
	{
		UE_LOG(Log_HyperScaleEvents, Warning, TEXT("Received event for incorrect index handle %d"), Handle)
		return;
	}

	UFunction* Function = Cast<UFunction>(FieldNetCache->Field.ToUObject());
	if (!Function)
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No UFunction exists for received handle %d"), Handle)
		return;
	}

	UFunction* LayoutFunction = Function;
	while (LayoutFunction->GetSuperFunction())
	{
		LayoutFunction = LayoutFunction->GetSuperFunction();
	}
	const TSharedPtr<FRepLayout> FuncRepLayout = NetDriver->GetFunctionRepLayout(LayoutFunction);

	if (!FuncRepLayout.IsValid())
	{
		UE_LOG(Log_HyperScaleEvents, Warning, TEXT("ReceivedRPC: GetFunctionRepLayout returned an invalid layout."));
		return;
	}

	const uint8 PrevActValue = NetDriver->bActAsClient;
	NetDriver->bActAsClient = 1;
	if (Function->NumParms == 0)
	{
		Object->ProcessEvent(Function, nullptr);
	}
	else
	{
		TSharedPtr<FHScaleNetworkEntity> Entity = GetBibliothec()->FindExistingEntity(ReceiverId);
		if (!Entity.IsValid())
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No entity exists in memory layer for given entity Id %s"), *ReceiverId.ToString())
			return;
		}

		FHScaleNetworkEntity* OwningActorEntity = Entity->GetChannelOwner();
		if (!OwningActorEntity)
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Owning actor entity is invalid for %s"), *ReceiverId.ToString())
			return;
		}
		UObject* ActorObj = PkgMap->FindObjectFromEntityID(OwningActorEntity->EntityId);

		if (!ActorObj)
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No Corresponding object exists for owning entity %s"), *OwningActorEntity->EntityId.ToString())
			return;
		}

		AActor* Actor = Cast<AActor>(ActorObj);

		if (!Actor)
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No Corresponding valid actor exists for owning entity %s"), *OwningActorEntity->EntityId.ToString())
			return;
		}

		UActorChannel** Ch_R = Connection->FindActorChannel(Actor);
		if (!Ch_R || !(*Ch_R))
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No Corresponding actor channel exists for owning entity %s"), *OwningActorEntity->EntityId.ToString())
			return;
		}

		UHScaleActorChannel* Ch = Cast<UHScaleActorChannel>(*(Ch_R));

		if (!Ch)
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("No Corresponding actor channel exists for owning entity %s"), *OwningActorEntity->EntityId.ToString())
			return;
		}

		FBitWriter Writer(8 * QUARK_MAX_PAYLOAD_LEN);

		if (WriteRPCParams(Writer, Reader, FuncRepLayout))
		{
			FNetBitReader TempReader(PkgMap, Writer.GetData(), Writer.GetNumBits());

			TSet<FNetworkGUID> UnmappedGuids;
			TSharedRef<FObjectReplicator> ObjectReplicator = Ch->FindOrCreateReplicator_HS(Object);
			FReplicationFlags RepFlags;
			bool bDelayFunction;
			ObjectReplicator->ReceivedRPC(TempReader, RepFlags, FieldNetCache, false, bDelayFunction, UnmappedGuids);
		}
		else
		{
			UE_LOG(Log_HyperScaleEvents, Warning, TEXT("Failed to write RPC params for Entity %s with eventId %d"), *ReceiverId.ToString(), EventId)
		}
	}

	NetDriver->bActAsClient = PrevActValue;
}

void FHScaleEventsDriver::ProcessRemoteFunctionForChannelPrivate_HS(UActorChannel* Ch, const FClassNetCache* ClassCache,
	const FFieldNetCache* FieldCache, UObject* TargetObject,
	UFunction* Function, void* Parameters) const
{
	TObjectPtr<UNetDriver> NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
	const bool bIsServer = NetDriver->IsServer();

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	if (!PkgMap) return;

	FHScaleNetGUID EntityNetGUID = PkgMap->FindEntityNetGUID(TargetObject);
	if (!EntityNetGUID.IsObject())
	{
		UE_LOG(Log_HyperScaleEvents, Warning, TEXT("RPC triggered for an object with no corresponding EntityId"))
		return;
	}

	if (Ch->Closing)
	{
		return;
	}

	// Clients may be "closing" this connection but still processing bunches, we can't send anything if we have an invalid ChIndex.
	if (Ch->ChIndex == -1)
	{
		ensure(!bIsServer);
		return;
	}

	// Form the RPC preamble.
	FOutBunch Bunch(Ch, false);

	// TODO: In high scale SDK, use bReliable to send reliable events
	// Reliability.
	//warning: RPCs might overflow, preventing reliable functions from getting thorough.
	if (Function->FunctionFlags & FUNC_NetReliable)
	{
		Bunch.bReliable = 1;
	}

	// verify we haven't overflowed unacked bunch buffer (Connection is not net ready)
	//@warning: needs to be after parameter evaluation for script stack integrity
	if (Bunch.IsError())
	{
		if (!Bunch.bReliable)
		{
			// Not reliable, so not fatal. This can happen a lot in debug builds at startup if client is slow to get in game
			UE_LOG(LogNet, Warning, TEXT("Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d"), *GetNameSafe(Function), *GetFullNameSafe(TargetObject), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket);
		}
		else
		{
			// The connection has overflowed the reliable buffer. We cannot recover from this. Disconnect this user.
			UE_LOG(LogNet, Warning, TEXT("Closing connection. Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d."), *GetNameSafe(Function), *GetFullNameSafe(TargetObject), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket);

			FString ErrorMsg = NSLOCTEXT("NetworkErrors", "ClientReliableBufferOverflow", "Outgoing reliable buffer overflow").ToString();

			Connection->SendCloseReason(ENetCloseResult::RPCReliableBufferOverflow);
			FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
			Connection->FlushNet(true);
			Connection->Close(ENetCloseResult::RPCReliableBufferOverflow);
		}
		return;
	}

	// Init bunch for rpc
#if UE_NET_TRACE_ENABLED
	SetTraceCollector(Bunch, UE_NET_TRACE_CREATE_COLLECTOR(ENetTraceVerbosity::Trace));
#endif

	FNetBitWriter TempWriter(Bunch.PackageMap, 0);

#if UE_NET_TRACE_ENABLED
	// Create trace collector if tracing is enabled for the target bunch
	SetTraceCollector(TempWriter, GetTraceCollector(Bunch) ? UE_NET_TRACE_CREATE_COLLECTOR(ENetTraceVerbosity::Trace) : nullptr);
	ON_SCOPE_EXIT { UE_NET_TRACE_DESTROY_COLLECTOR(GetTraceCollector(TempWriter)); };
#endif // UE_NET_TRACE_ENABLED

	// Use the replication layout to send the rpc parameter values
	TSharedPtr<FRepLayout> RepLayout = NetDriver->GetFunctionRepLayout(Function);

	RepLayout->SendPropertiesForRPC(Function, Ch, TempWriter, Parameters);

	if (TempWriter.IsError())
	{
		UE_LOG(LogNet, Log, TEXT("Error: Can't send function '%s' on '%s': Failed to serialize properties"), *GetNameSafe(Function), *GetFullNameSafe(TargetObject));
	}

	const int32 MaxFieldNetIndex = ClassCache->GetMaxIndex() + 1;
	check(FieldCache->FieldNetIndex < MaxFieldNetIndex);
	check(FieldCache->FieldNetIndex < UINT16_MAX)

	const bool bIsServerMulticast = bIsServer && (Function->FunctionFlags & FUNC_NetMulticast);

	uint16 EventId = FHScalePropertyIdConverters::GetAppEventIdFromHandle(FieldCache->FieldNetIndex);
	FBitWriter Writer(8 * QUARK_MAX_PAYLOAD_LEN);
	Writer << EntityNetGUID;

	if (bIsServerMulticast)
	{
		FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
		TSharedPtr<FHScaleNetworkEntity> EntityPtr = Bibliothec->FindExistingEntity(EntityNetGUID);
		if (!EntityPtr.IsValid())
		{
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Entity %s does not exist in bibliothec, rpc failed to send"), *EntityNetGUID.ToString())
			return;
		}
		const FHScaleNetworkEntity* NetOwnerEntity = EntityPtr->GetNetOwner();
		
		if (NetOwnerEntity && NetOwnerEntity->EntityId.IsValid())
		{
			FHScaleNetGUID OwningSessionId = Connection->GetSessionNetGUID();
			if (NetOwnerEntity->EntityId != OwningSessionId)
			{
				UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("NetMultiCast RPC triggered for non-owning client for Entity %s"), *EntityNetGUID.ToString())
				return;
			}
		}
	}

	
	bool bToSend = true;
	if (Function->NumParms > 0)
	{
		FBitReader Reader(TempWriter.GetData(), TempWriter.GetNumBits());
		bToSend &= ReadRPCParams(Writer, Reader, RepLayout);
	}
	if (bToSend)
	{
		bool bSuccess;
		if (bIsServerMulticast)
		{
			// #todo: In production SDK this should be configurable per RPC, we can fetch it from 
			bSuccess = SendEvent(EventId, EHScaleEventRadius::Medium, Writer);
		}
		else
		{
			bSuccess = SendEvent(EventId, EntityNetGUID, Writer);
		}
		
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Event sending result for EventId %d bSuccess %d"), EventId, bSuccess)
	}
}

bool FHScaleEventsDriver::ReadRPCParams(::FBitWriter& Writer, ::FBitReader& Reader, const TSharedPtr<FRepLayout>& RepLayout) const
{
	if (Reader.AtEnd()) return false;

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	// payload contains series of property handles(packed int) and property data
	const TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Cmds);
	const TArray<FRepParentCmd>& Parents = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Parents);

	bool bSuccess = true;
	for (int32 i = 0; i < Parents.Num(); i++)
	{
		bool bToRead = false;
		if (CastField<FBoolProperty>(Parents[i].Property)) { bToRead = true; }
		if (!bToRead)
		{
			bToRead = !!Reader.ReadBit();
		}
		Writer.WriteBit(bToRead ? 1 : 0);

		if (bToRead)
		{
			for (int32 CmdIndex = Parents[i].CmdStart; CmdIndex < Parents[i].CmdEnd && !Writer.IsError() && bSuccess; CmdIndex++)
			{
				const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

				if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
				{
					bSuccess &= FHScaleEventsSerializer::ReadDynArray(Writer, Reader, PkgMap, Cmds[CmdIndex + 1]);
					CmdIndex = Cmd.EndCmd - 1; // The -1 to handle the ++ in the for loop
					continue;
				}
				if (FHScaleStatics::IsObjectDataRepCmd(Cmd))
				{
					bSuccess &= FHScaleEventsSerializer::ReadObjectPtr(Writer, Reader, PkgMap);
					continue;
				}

				if (Cmd.Type == ERepLayoutCmdType::PropertySoftObject)
				{
					bSuccess &= FHScaleEventsSerializer::ReadSoftObjectPtr(Writer, Reader, Cmd, PkgMap);
					continue;
				}

				bSuccess &= FHScaleEventsSerializer::Serialize(Writer, Reader, Cmd, PkgMap);
			}
		}
	}

	return bSuccess;
}

bool FHScaleEventsDriver::WriteRPCParams(::FBitWriter& Writer, ::FBitReader& Reader, const TSharedPtr<FRepLayout>& RepLayout) const
{
	if (Reader.AtEnd()) return false;

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);

	// payload contains series of property handles(packed int) and property data
	const TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Cmds);
	const TArray<FRepParentCmd>& Parents = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Parents);

	bool bSuccess = true;
	for (int32 i = 0; i < Parents.Num(); i++)
	{
		const bool bToRead = !!Reader.ReadBit();
		if (!CastField<FBoolProperty>(Parents[i].Property))
		{
			Writer.WriteBit(bToRead ? 1 : 0);
		}

		if (bToRead)
		{
			for (int32 CmdIndex = Parents[i].CmdStart; CmdIndex < Parents[i].CmdEnd && !Writer.IsError(); CmdIndex++)
			{
				const FRepLayoutCmd& Cmd = Cmds[CmdIndex];

				if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
				{
					bSuccess &= FHScaleEventsSerializer::WriteDynArray(Writer, Reader, PkgMap, Cmds[CmdIndex + 1]);
					CmdIndex = Cmd.EndCmd - 1; // The -1 to handle the ++ in the for loop
					continue;
				}
				if (FHScaleStatics::IsObjectDataRepCmd(Cmd))
				{
					bSuccess &= FHScaleEventsSerializer::WriteObjectPtr(Writer, Reader, PkgMap);
					continue;
				}

				if (Cmd.Type == ERepLayoutCmdType::PropertySoftObject)
				{
					bSuccess &= FHScaleEventsSerializer::WriteSoftObjectPtr(Writer, Reader, Cmd, PkgMap);
					continue;
				}

				bSuccess &= FHScaleEventsSerializer::Serialize(Writer, Reader, Cmd, PkgMap);
			}
		}
	}

	return bSuccess;
}

void FHScaleEventsDriver::ProcessLocalRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject)
{
	if (!IsValid(Connection)) return;
	TObjectPtr<UNetDriver> NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
	if (!IsValid(NetDriver)) return;
	const bool bIsServer = NetDriver->IsServer();
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	check(PkgMap)
	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
	check(Bibliothec)

	// If we have a subobject, thats who we are actually calling this on. If no subobject, we are calling on the actor.
	UObject* TargetObj = SubObject ? SubObject : Actor;

	// Make sure this function exists for both parties.
	const FClassNetCache* ClassCache = NetDriver->NetCache->GetClassNetCache(TargetObj->GetClass());
	if (!ClassCache)
	{
		UE_LOG(LogNet, VeryVerbose, TEXT("ClassNetCache empty, not calling %s::%s"), *GetNameSafe(Actor), *GetNameSafe(Function));
		return;
	}

	const FFieldNetCache* FieldCache = ClassCache->GetFromField(Function);
	if (!FieldCache)
	{
		UE_LOG(LogNet, VeryVerbose, TEXT("FieldCache empty, not calling %s::%s"), *GetNameSafe(Actor), *GetNameSafe(Function));
		return;
	}

	// Get the actor channel.
	UHScaleActorChannel* Ch = Cast<UHScaleActorChannel>(Connection->FindActorChannelRef(Actor));

	if (!Ch)
	{
		if (bIsServer)
		{
			if (Actor->IsPendingKillPending())
			{
				// Don't try opening a channel for me, I am in the process of being destroyed. Ignore my RPCs.
				return;
			}

			if (NetDriver->IsLevelInitializedForActor(Actor, Connection))
			{
				Ch = Cast<UHScaleActorChannel>(Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally));
			}
			else
			{
				UE_LOG(LogNet, Verbose, TEXT("Can't send function '%s' on actor '%s' because client hasn't loaded the level '%s' containing it"), *GetNameSafe(Function), *GetNameSafe(Actor), *GetNameSafe(Actor->GetLevel()));
				return;
			}
		}

		if (!Ch)
		{
			return;
		}

		if (bIsServer)
		{
			Ch->SetChannelActor(Actor, ESetChannelActorFlags::None);
		}
	}

	FHScaleNetGUID ObjHSNetGUID = PkgMap->FindEntityNetGUID(TargetObj);
	if (!ObjHSNetGUID.IsObject() || !Bibliothec->IsEntityExists(ObjHSNetGUID))
	{
		Ch->ReplicateActorToMemoryLayer();
	}

	// if there is no corresponding 
	EProcessRemoteFunctionFlags UnusedFlags = EProcessRemoteFunctionFlags::None;
	ProcessRemoteFunctionForChannelPrivate_HS(Ch, ClassCache, FieldCache, TargetObj, Function, Parameters);
}