// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/HScaleActorChannel.h"

#include "Engine/NetworkObjectList.h"
#include "Net/DataChannel.h"
#include "Net/NetworkProfiler.h"
#include "Net/RepLayout.h"
#include "Net/Core/Trace/NetTrace.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "ReplicationLayer/HScaleRepDriver.h"
#include "ReplicationLayer/HScaleReplicationResources.h"
#include "ReplicationLayer/Schema/HScaleSchema.h"

class FRoleCorrection
{
public:
	FRoleCorrection(AActor* InActor, const FHScaleRepFlags RepFlags)
	{
		if (RepFlags.bOwnerBlendMode)
		{
			if (RepFlags.bHasAnyNetOwner)
			{
				// The current machine is an owner of the target actor
				InActor->SetRole(ROLE_Authority);
				InActor->SetAutonomousProxy(false, false);

				if (!RepFlags.bNetOwner)
				{
					InActor->bExchangedRoles = false;
					InActor->ExchangeNetRoles(true);
				}
			}
			else
			{
				// The target actor doesn't have an owner, it should not be replicated to network
				// but set roles correctly to the quark architecture
				InActor->SetRole(ROLE_SimulatedProxy);
				InActor->SetAutonomousProxy(false, false);
			}
		}
		else
		{
			InActor->SetRole(ROLE_AutonomousProxy);
			InActor->SetAutonomousProxy(true, false);
		}
	}
};

// Helper class to downgrade a non owner of an actor to simulated while replicating
class FScopedRoleDowngrade
{
	// The net owner indicates if the actor can be controlled only from owning client
public:
	FScopedRoleDowngrade(AActor* InActor, const FHScaleRepFlags RepFlags)
		: Actor(InActor), ActualLocalRole(Actor->GetLocalRole()), ActualRemoteRole(Actor->GetRemoteRole())
	{
		if (RepFlags.bOwnerBlendMode)
		{
			if (RepFlags.bNetOwner)
			{
				ensure(ActualLocalRole == ROLE_Authority);
				ensure(ActualRemoteRole == ROLE_SimulatedProxy);

				// The current machine is an owner of the actor target
				Actor->bExchangedRoles = false;
				Actor->ExchangeNetRoles(true);
			}
#if !UE_BUILD_SHIPPING
			else if (RepFlags.bHasAnyNetOwner)
			{
				ensure(ActualLocalRole == ROLE_SimulatedProxy);
				ensure(ActualRemoteRole == ROLE_SimulatedProxy);
			}
#endif
		}
#if !UE_BUILD_SHIPPING
		else
		{
			// This should be always the same for average/select blend modes
			ensure(ActualLocalRole == ROLE_AutonomousProxy);
			ensure(ActualRemoteRole == ROLE_AutonomousProxy);
		}
#endif
	}

	~FScopedRoleDowngrade()
	{
		// If roles were exchanged, just take it back
		if (Actor->bExchangedRoles)
		{
			// Swap back
			Actor->bExchangedRoles = false;
			Actor->ExchangeNetRoles(true);
			Actor->bExchangedRoles = false; // Changed in function ExchangeNetRoles, but it is just temporary
		}
	}

private:
	AActor* Actor;
	const ENetRole ActualLocalRole;
	const ENetRole ActualRemoteRole;
};

UHScaleActorChannel::UHScaleActorChannel()
	: bNetInitial(true)
{
	bCustomActorIsPendingKill = false;
}

void UHScaleActorChannel::ReceivedBunch(FInBunch& Bunch)
{
	check(!Closing);

	if (Broken || bTornOff)
	{
		return;
	}

	NET_CHECKSUM(Bunch);

	UHScaleNetDriver* NetDriver = StaticCast<UHScaleNetDriver*>(Connection->Driver);
	NetDriver->bActAsClient = 1;
	ProcessBunch_HS(Bunch);
	NetDriver->bActAsClient = 0;
}

FPacketIdRange UHScaleActorChannel::SendBunch(FOutBunch* Bunch, bool Merge)
{
	check(Bunch);
	check(Connection);
	check(!Closing);
	check(!Bunch->IsError());

	UHScaleNetDriver* Driver = (UHScaleNetDriver*)Connection->Driver;
	check(Driver);

	FHScaleNetworkBibliothec* Bibliothec = Driver->GetBibliothec();
	check(Bibliothec);

	uint8* Data = Bunch->GetData();
	const int64 BunchNumBits = Bunch->GetNumBits();

	FHScaleInBunch InBunch = FHScaleInBunch(Connection, Data, BunchNumBits);
	InBunch.SetChannel(this);

	Bibliothec->Push(InBunch); // remove id

	if (!ensure(ChIndex != -1))
	{
		// Client "closing" but still processing bunches. Client->Server RPCs should avoid calling this, but perhaps more code needs to check this condition.
		return FPacketIdRange(INDEX_NONE);
	}

	//
	// 	check(!Closing);
	// 	check(!Bunch->IsError());
	//
	// 	// Set bunch flags.
	//
	// 	const bool bDormancyClose = Bunch->bClose && (Bunch->CloseReason == EChannelCloseReason::Dormancy);
	//
	// 	if (OpenedLocally && ((OpenPacketId.First == INDEX_NONE) || ((Connection->ResendAllDataState != EResendAllDataState::None) && !bDormancyClose)))
	// 	{
	// 		bool bOpenBunch = true;
	//
	// 		if (Connection->ResendAllDataState == EResendAllDataState::SinceCheckpoint)
	// 		{
	// 			bOpenBunch = !bOpenedForCheckpoint;
	// 			bOpenedForCheckpoint = true;
	// 		}
	// 		
	// 		if (bOpenBunch)
	// 		{
	// 			Bunch->bOpen = 1;
	// 			OpenTemporary = !Bunch->bReliable;
	// 		}
	// 	}
	//
	// 	// If channel was opened temporarily, we are never allowed to send reliable packets on it.
	// 	check(!OpenTemporary || !Bunch->bReliable);
	//
	// 	// This is the max number of bits we can have in a single bunch
	// 	const int64 MAX_SINGLE_BUNCH_SIZE_BITS  = Connection->GetMaxSingleBunchSizeBits();
	//
	// 	// Max bytes we'll put in a partial bunch
	// 	const int64 MAX_SINGLE_BUNCH_SIZE_BYTES = MAX_SINGLE_BUNCH_SIZE_BITS / 8;
	//
	// 	// Max bits will put in a partial bunch (byte aligned, we dont want to deal with partial bytes in the partial bunches)
	// 	const int64 MAX_PARTIAL_BUNCH_SIZE_BITS = MAX_SINGLE_BUNCH_SIZE_BYTES * 8;
	//
	// 	TArray<FOutBunch*>& OutgoingBunches = Connection->GetOutgoingBunches();
	// 	OutgoingBunches.Reset();
	//
	// 	// Add any export bunches
	// 	// Replay connections will manage export bunches separately.
	// 	if (!Connection->IsInternalAck())
	// 	{
	// 		AppendExportBunches( OutgoingBunches );
	// 	}
	//
	// 	if ( OutgoingBunches.Num() )
	// 	{
	// 		// Don't merge if we are exporting guid's
	// 		// We can't be for sure if the last bunch has exported guids as well, so this just simplifies things
	// 		Merge = false;
	// 	}
	//
	// 	if ( Connection->Driver->IsServer() )
	// 	{
	// 		// This is a bit special, currently we report this is at the end of bunch event though AppendMustBeMappedGuids rewrites the entire bunch
	// 		UE_NET_TRACE_SCOPE(MustBeMappedGuids_IsAtStartOfBunch, *Bunch, GetTraceCollector(*Bunch), ENetTraceVerbosity::Trace);
	//
	// 		// Append any "must be mapped" guids to front of bunch from the packagemap
	// 		AppendMustBeMappedGuids( Bunch );
	//
	// 		if ( Bunch->bHasMustBeMappedGUIDs )
	// 		{
	// 			// We can't merge with this, since we need all the unique static guids in the front
	// 			Merge = false;
	// 		}
	// 	}
	//
	// 	//-----------------------------------------------------
	// 	// Contemplate merging.
	// 	//-----------------------------------------------------
	// 	int32 PreExistingBits = 0;
	// 	FOutBunch* OutBunch = NULL;
	// 	if
	// 	(	Merge
	// 	&&	Connection->LastOut.ChIndex == Bunch->ChIndex
	// 	&&	Connection->LastOut.bReliable == Bunch->bReliable	// Don't merge bunches of different reliability, since for example a reliable RPC can cause a bunch with properties to become reliable, introducing unnecessary latency for the properties.
	// 	&&	Connection->AllowMerge
	// 	&&	Connection->LastEnd.GetNumBits()
	// 	&&	Connection->LastEnd.GetNumBits()==Connection->SendBuffer.GetNumBits()
	// 	&&	Connection->LastOut.GetNumBits() + Bunch->GetNumBits() <= MAX_SINGLE_BUNCH_SIZE_BITS )
	// 	{
	// 		// Merge.
	// 		check(!Connection->LastOut.IsError());
	// 		PreExistingBits = Connection->LastOut.GetNumBits();
	// 		Connection->LastOut.SerializeBits( Bunch->GetData(), Bunch->GetNumBits() );
	// 		Connection->LastOut.bOpen     |= Bunch->bOpen;
	// 		Connection->LastOut.bClose    |= Bunch->bClose;
	//
	// #if UE_NET_TRACE_ENABLED		
	// 		SetTraceCollector(Connection->LastOut, GetTraceCollector(*Bunch));
	// 		SetTraceCollector(*Bunch, nullptr);
	// #endif
	//
	// 		OutBunch                       = Connection->LastOutBunch;
	// 		Bunch                          = &Connection->LastOut;
	// 		check(!Bunch->IsError());
	// 		Connection->PopLastStart();
	// 		Connection->Driver->OutBunches--;
	// 	}
	//
	// 	OutgoingBunches.Add(Bunch);
	//
	// 	//-----------------------------------------------------
	// 	// Send all the bunches we need to
	// 	//	Note: this is done all at once. We could queue this up somewhere else before sending to Out.
	// 	//-----------------------------------------------------
	// 	FPacketIdRange PacketIdRange;
	//
	// 	const bool bOverflowsReliable = (NumOutRec + OutgoingBunches.Num() >= RELIABLE_BUFFER + Bunch->bClose);
	//
	// 	if ((GCVarNetPartialBunchReliableThreshold > 0) && (OutgoingBunches.Num() >= GCVarNetPartialBunchReliableThreshold) && !Connection->IsInternalAck())
	// 	{
	// 		if (!bOverflowsReliable)
	// 		{
	// 			UE_LOG(LogNetPartialBunch, Log, TEXT("	OutgoingBunches.Num (%d) exceeds reliable threashold (%d). Making bunches reliable. Property replication will be paused on this channel until these are ACK'd."), OutgoingBunches.Num(), GCVarNetPartialBunchReliableThreshold);
	// 			Bunch->bReliable = true;
	// 			bPausedUntilReliableACK = true;
	// 		}
	// 		else
	// 		{
	// 			// The threshold was hit, but making these reliable would overflow the reliable buffer. This is a problem: there is just too much data.
	// 			UE_LOG(LogNetPartialBunch, Warning, TEXT("	OutgoingBunches.Num (%d) exceeds reliable threashold (%d) but this would overflow the reliable buffer! Consider sending less stuff. Channel: %s"), OutgoingBunches.Num(), GCVarNetPartialBunchReliableThreshold, *Describe());
	// 		}
	// 	}
	//
	// 	if (Bunch->bReliable && bOverflowsReliable)
	// 	{
	// 		UE_LOG(LogNetPartialBunch, Warning, TEXT("SendBunch: Reliable partial bunch overflows reliable buffer! %s"), *Describe() );
	// 		UE_LOG(LogNetPartialBunch, Warning, TEXT("   Num OutgoingBunches: %d. NumOutRec: %d"), OutgoingBunches.Num(), NumOutRec );
	// 		PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// 		PrintReliableBunchBuffer();
	// 		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	//
	// 		// Bail out, we can't recover from this (without increasing RELIABLE_BUFFER)
	// 		FString ErrorMsg = NSLOCTEXT("NetworkErrors", "ClientReliableBufferOverflow", "Outgoing reliable buffer overflow").ToString();
	//
	// 		Connection->SendCloseReason(ENetCloseResult::ReliableBufferOverflow);
	// 		FNetControlMessage<NMT_Failure>::Send(Connection, ErrorMsg);
	// 		Connection->FlushNet(true);
	// 		Connection->Close(ENetCloseResult::ReliableBufferOverflow);
	// 	
	// 		return PacketIdRange;
	// 	}
	//
	// 	// Update open range if necessary
	// 	if (Bunch->bOpen && (Connection->ResendAllDataState == EResendAllDataState::None))
	// 	{
	// 		OpenPacketId = PacketIdRange;		
	// 	}
	//
	// 	// Destroy outgoing bunches now that they are sent, except the one that was passed into ::SendBunch
	// 	//	This is because the one passed in ::SendBunch is the responsibility of the caller, the other bunches in OutgoingBunches
	// 	//	were either allocated in this function for partial bunches, or taken from the package map, which expects us to destroy them.
	// 	for (auto It = OutgoingBunches.CreateIterator(); It; ++It)
	// 	{
	// 		FOutBunch *DeleteBunch = *It;
	// 		if (DeleteBunch != Bunch)
	// 			delete DeleteBunch;
	// 	}
	//
	// 	return PacketIdRange;
	bNetInitial = false;
	return FPacketIdRange(INDEX_NONE);
}

void UHScaleActorChannel::NotifyActorChannelOpen(AActor* InActor, FInBunch& InBunch)
{
	// This function is the exact copy of the parent one, but
	// for actor function OnActorChannelOpen() is send an empty bunch
	// because we don't want to break the bunch structure 
	UNetDriver* const NetDriver = (Connection && Connection->Driver) ? Connection->Driver : nullptr;
	UWorld* const World = (NetDriver && NetDriver->World) ? NetDriver->World : nullptr;

	FWorldContext* const Context = GEngine->GetWorldContextFromWorld(World);
	if (Context != nullptr)
	{
		for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
		{
			if (Driver.NetDriver != nullptr)
			{
				Driver.NetDriver->NotifyActorChannelOpen(this, InActor);
			}
		}
	}

	FInBunch InEmptyBunch(InBunch.Connection); // <- THIS is the only change from the parent call
	Actor->OnActorChannelOpen(InEmptyBunch, Connection);

	// Do not update client dormancy or other net drivers if this is a destruction info, those should be handled already in UNetDriver::NotifyActorDestroyed
	if (NetDriver && !NetDriver->IsServer() && !InBunch.bClose)
	{
		if (Actor->NetDormancy > DORM_Awake)
		{
			ENetDormancy OldDormancy = Actor->NetDormancy;

			Actor->NetDormancy = DORM_Awake;

			if (Context != nullptr)
			{
				for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
				{
					if (Driver.NetDriver != nullptr && Driver.NetDriver != NetDriver && Driver.NetDriver->ShouldReplicateActor(InActor))
					{
						Driver.NetDriver->NotifyActorClientDormancyChanged(InActor, OldDormancy);
					}
				}
			}
		}
	}
}

bool UHScaleActorChannel::ReplicateSubobjectCustom(UObject* Obj, FOutBunch& Bunch, const FReplicationFlags& RepFlags)
{
	TSharedRef<FObjectReplicator>* FoundReplicator = FindReplicator(Obj);
	const bool bFoundReplicator = (FoundReplicator != nullptr);

	TWeakObjectPtr<UObject> WeakObj(Obj);

	// Hack for now: subobjects are SupportsObject==false until they are replicated via ::ReplicateSUbobject, and then we make them supported
	// here, by forcing the packagemap to give them a NetGUID.
	//
	// Once we can lazily handle unmapped references on the client side, this can be simplified.
	if (!Connection->Driver->GuidCache->SupportsObject(Obj, &WeakObj))
	{
		Connection->Driver->GuidCache->AssignNewNetGUID_Server(Obj); //Make sure it gets a NetGUID so that it is now 'supported'

		FHScaleNetGUID NetEntityGUID = ((UHScalePackageMap*)Connection->PackageMap)->FindEntityNetGUID(Obj);
		if (ensure(NetEntityGUID.IsValid()))
		{
			ReplicationRedirectories.Add(NetEntityGUID, Obj);
		}
	}

	bool NewSubobject = false;

	FReplicationFlags ObjRepFlags = RepFlags;
	TSharedRef<FObjectReplicator>& ObjectReplicator = !bFoundReplicator ? CreateReplicator(Obj) : *FoundReplicator;

	if (!bFoundReplicator)
	{
		Bunch.bReliable = true;
		NewSubobject = true;
		ObjRepFlags.bNetInitial = true;
	}

	bool bWroteSomething = ObjectReplicator.Get().ReplicateProperties(Bunch, ObjRepFlags);

	if (!bWroteSomething)
	{
		// Write empty payload to force object creation
		FNetBitWriter EmptyPayload;
		WriteContentBlockPayload(Obj, Bunch, true, EmptyPayload);
		bWroteSomething = true;
	}
	return bWroteSomething;
}

void UHScaleActorChannel::ProcessBunch_HS(FInBunch& Bunch)
{
	if (Broken)
	{
		return;
	}

	UHScalePackageMap* PackageMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	check(PackageMap);

	PackageMap->SerializeObjectId(Bunch, nullptr, EntityId);

	// We can process this bunch now
	// when processing bunch, if actor is not spawned it will be spawned
	if (!EntityId.IsValid())
	{
		UE_LOG(LogNet, Warning, TEXT("Received invalid NetGUID in bunch"))
		return;
	}

	FReplicationFlags RepFlags;

	// ------------------------------------------------------------
	// Initialize client if first time through.
	// ------------------------------------------------------------
	bool bSpawnedNewActor = false; // If this turns to true, we know an actor was spawned (rather than found)
	if (Actor == NULL)
	{
		if (!Bunch.bOpen)
		{
			// This absolutely shouldn't happen anymore, since we no longer process packets until channel is fully open early on
			UE_LOG(LogNetTraffic, Error, TEXT( "UActorChannel::ProcessBunch: New actor channel received non-open packet. bOpen: %i, bClose: %i, bReliable: %i, bPartial: %i, bPartialInitial: %i, bPartialFinal: %i, ChName: %s, ChIndex: %i, Closing: %i, OpenedLocally: %i, OpenAcked: %i, NetGUID: %s" ), (int)Bunch.bOpen, (int)Bunch.bClose, (int)Bunch.bReliable, (int)Bunch.bPartial, (int)Bunch.bPartialInitial, (int)Bunch.bPartialFinal, *ChName.ToString(), ChIndex, (int)Closing,
				(int)OpenedLocally,
				(int)OpenAcked, *ActorNetGUID.ToString());
			return;
		}

		UE_NET_TRACE_SCOPE(NewActor, Bunch, Connection->GetInTraceCollector(), ENetTraceVerbosity::Trace);

		AActor* NewChannelActor = NULL;
		bSpawnedNewActor = Connection->PackageMap->SerializeNewActor(Bunch, this, NewChannelActor);

		// We are unsynchronized. Instead of crashing, let's try to recover.
		if (!IsValid(NewChannelActor))
		{
			// got a redundant destruction info, possible when streaming
			if (!bSpawnedNewActor && Bunch.bReliable && Bunch.bClose && Bunch.AtEnd())
			{
				// Do not log during replay, since this is a valid case
				if (!Connection->IsReplay())
				{
					UE_LOG(LogNet, Verbose, TEXT("UActorChannel::ProcessBunch: SerializeNewActor received close bunch for destroyed actor. Actor: %s, Channel: %i"), *GetFullNameSafe(NewChannelActor), ChIndex);
				}

				SetChannelActor(nullptr, ESetChannelActorFlags::None);
				return;
			}

			check(!bSpawnedNewActor);
			UE_LOG(LogNet, Warning, TEXT("UActorChannel::ProcessBunch: SerializeNewActor failed to find/spawn actor. Actor: %s, Channel: %i"), *GetFullNameSafe(NewChannelActor), ChIndex);
			Broken = 1;

			if (!Connection->IsInternalAck()
#if !UE_BUILD_SHIPPING
			    && !bBlockChannelFailure
#endif
			)
			{
				FNetControlMessage<NMT_ActorChannelFailure>::Send(Connection, ChIndex);
			}
			return;
		}
		// 		else
		// 		{
		// 			if (UE::Net::bDiscardTornOffActorRPCs && NewChannelActor->GetTearOff())
		// 			{
		// 				UE_LOG(LogNet, Warning, TEXT("UActorChannel::ProcessBunch: SerializeNewActor received an open bunch for a torn off actor. Actor: %s, Channel: %i"), *GetFullNameSafe(NewChannelActor), ChIndex);
		// 				Broken = 1;
		//
		// 				if (!Connection->IsInternalAck()
		// #if !UE_BUILD_SHIPPING
		// 					&& !bBlockChannelFailure
		// #endif
		// 					)
		// 				{
		// 					FNetControlMessage<NMT_ActorChannelFailure>::Send(Connection, ChIndex);
		// 				}
		// 				return;
		// 			}
		// 		}

		ESetChannelActorFlags Flags = ESetChannelActorFlags::None;
		// if (GSkipReplicatorForDestructionInfos != 0 && Bunch.bClose && Bunch.AtEnd())
		// {
		// 	Flags |= ESetChannelActorFlags::SkipReplicatorCreation;
		// }

		UE_LOG(LogNetTraffic, Log, TEXT("      Channel Actor %s:"), *NewChannelActor->GetFullName());
		SetChannelActor(NewChannelActor, Flags);

		NotifyActorChannelOpen(Actor, Bunch);

		RepFlags.bNetInitial = true;
		RepFlags.bSkipRoleSwap = true;

		Actor->CustomTimeDilation = CustomTimeDilation;
	}
	else
	{
		ReadExistingActorHeader(Bunch);

		UE_LOG(LogNetTraffic, Log, TEXT("      Actor %s:"), *Actor->GetFullName());
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bool bLatestIsReplicationPaused = Bunch.bIsReplicationPaused != 0;
	if (bLatestIsReplicationPaused != IsReplicationPaused())
	{
		Actor->OnReplicationPausedChanged(bLatestIsReplicationPaused);
		SetReplicationPaused(bLatestIsReplicationPaused);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Owned by connection's player?
	UNetConnection* ActorConnection = Actor->GetNetConnection();
	if (ActorConnection == Connection || (ActorConnection != NULL && ActorConnection->IsA(UChildConnection::StaticClass()) && ((UChildConnection*)ActorConnection)->Parent == Connection))
	{
		RepFlags.bNetOwner = true;
	}

	RepFlags.bIgnoreRPCs = Bunch.bIgnoreRPCs;
	// RepFlags.bSkipRoleSwap = bSkipRoleSwap;

	// ----------------------------------------------
	//	Read chunks of actor content
	// ----------------------------------------------
	while (!Bunch.AtEnd() && Connection != NULL && Connection->GetConnectionState() != USOCK_Closed)
	{
		FNetBitReader Reader(Bunch.PackageMap, 0);

		bool bHasRepLayout = false;

		UE_NET_TRACE_NAMED_OBJECT_SCOPE(ContentBlockScope, FNetworkGUID(), Bunch, Connection->GetInTraceCollector(), ENetTraceVerbosity::Trace);

		// To process the received bunch, the netdriver needs to temporarily act as a client
		// setting the net driver as client here and reverting back post the op
		UHScaleNetDriver* NetDriver = StaticCast<UHScaleNetDriver*>(Connection->Driver);
		NetDriver->bActAsClient = 1;

		// Read the content block header and payload
		UObject* RepObj = ReadContentBlockPayload_HS(Bunch, Reader, bHasRepLayout);

		// Special case where we offset the events to avoid having to create a new collector for reading from the Reader
		UE_NET_TRACE_OFFSET_SCOPE(Bunch.GetPosBits() - Reader.GetNumBits(), Connection->GetInTraceCollector());

		if (Bunch.IsError())
		{
			if (Connection->IsInternalAck())
			{
				UE_LOG(LogNet, Warning, TEXT( "UActorChannel::ReceivedBunch: ReadContentBlockPayload FAILED. Bunch.IsError() == TRUE. (IsInternalAck) Breaking actor. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);
				Broken = 1;
				// break;
			}

			UE_LOG(LogNet, Error, TEXT( "UActorChannel::ReceivedBunch: ReadContentBlockPayload FAILED. Bunch.IsError() == TRUE. Closing connection. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);

			Connection->Close(AddToAndConsumeChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockFail));

			return;
		}

		if (Reader.GetNumBits() == 0)
		{
			// Set the scope name
			UE_NET_TRACE_SET_SCOPE_OBJECTID(ContentBlockScope, Connection->Driver->GuidCache->GetNetGUID(RepObj));

			// Nothing else in this block, continue on (should have been a delete or create block)
			// continue;
		}

		if (!IsValid(RepObj))
		{
			if (!IsValid(Actor))
			{
				// If we couldn't find the actor, that's pretty bad, we need to stop processing on this channel
				UE_LOG(LogNet, Warning, TEXT( "UActorChannel::ProcessBunch: ReadContentBlockPayload failed to find/create ACTOR. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);
				Broken = 1;
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT( "UActorChannel::ProcessBunch: ReadContentBlockPayload failed to find/create object. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);
			}

			// continue; // Since content blocks separate the payload from the main stream, we can skip to the next one
		}

		TSharedRef<FObjectReplicator>& Replicator = FindOrCreateReplicator(RepObj);

		bool bHasUnmapped = false;

		if (!Replicator->ReceivedBunch(Reader, RepFlags, bHasRepLayout, bHasUnmapped))
		{
			NetDriver->bActAsClient = 0;
			if (Connection->IsInternalAck())
			{
				UE_LOG(LogNet, Warning, TEXT( "UActorChannel::ProcessBunch: Replicator.ReceivedBunch failed (Ignoring because of IsInternalAck). RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);
				Broken = 1;
				// continue; // Don't consider this catastrophic in replays
			}

			// For now, with regular connections, consider this catastrophic, but someday we could consider supporting backwards compatibility here too
			UE_LOG(LogNet, Error, TEXT( "UActorChannel::ProcessBunch: Replicator.ReceivedBunch failed.  Closing connection. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);
			Connection->Close(ENetCloseResult::ObjectReplicatorReceivedBunchFail);
			return;
		}
		NetDriver->bActAsClient = 0;

		// Check to see if the actor was destroyed
		// If so, don't continue processing packets on this channel, or we'll trigger an error otherwise
		// note that this is a legitimate occurrence, particularly on client to server RPCs
		if (!IsValid(Actor))
		{
			UE_LOG(LogNet, VeryVerbose, TEXT( "UActorChannel::ProcessBunch: Actor was destroyed during Replicator.ReceivedBunch processing" ));
			// If we lose the actor on this channel, we can no longer process bunches, so consider this channel broken
			Broken = 1;
			// break;
		}

		// Set the scope name now that we can lookup the NetGUID from the replicator
		UE_NET_TRACE_SET_SCOPE_OBJECTID(ContentBlockScope, Replicator->ObjectNetGUID);

		if (bHasUnmapped)
		{
			LLM_SCOPE_BYTAG(NetDriver);
			Connection->Driver->UnmappedReplicators.Add(&Replicator.Get());
		}
	}

	// #if UE_REPLICATED_OBJECT_REFCOUNTING
	// 	TArray<TWeakObjectPtr<UObject>, TInlineAllocator<16>> ReferencesToRemove;
	// #endif

	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	{
		TSharedRef<FObjectReplicator>& ObjectReplicator = RepComp.Value();
		if (!IsValid(ObjectReplicator->GetObject()))
		{
			if (!Connection->Driver->IsServer())
			{
				// #if UE_REPLICATED_OBJECT_REFCOUNTING
				// 				ReferencesToRemove.Add(ObjectReplicator.Get().GetWeakObjectPtr());
				// #endif

				RepComp.RemoveCurrent(); // This should cause the replicator to be cleaned up as there should be no outstandings refs. 
			}
			continue;
		}

		// <<< --- Starts here
		// Instead of calling the PostReceivedBunch() function, we just copied the code from function here (because in the original function epic checks ServerConnection instead of IsServer())
		// #todo ... (epic change required) the function can be used after epic uses IsServer() instead of ServerConnection
		// ObjectReplicator->PostReceivedBunch();

		if (ObjectReplicator->bHasReplicatedProperties)
		{
			ObjectReplicator->PostNetReceive();
			ObjectReplicator->bHasReplicatedProperties = false;
		}
		// <<< --- Ends here

		// Call RepNotifies
		ObjectReplicator->CallRepNotifies(true);
	}

	// #if UE_REPLICATED_OBJECT_REFCOUNTING
	// 	if (ReferencesToRemove.Num() > 0 )
	// 	{
	// 		Connection->Driver->GetNetworkObjectList().RemoveMultipleSubObjectChannelReference(Actor, ReferencesToRemove, this);
	// 	}
	// #endif

	// After all properties have been initialized, call PostNetInit. This should call BeginPlay() so initialization can be done with proper starting values.
	if (Actor && bSpawnedNewActor)
	{
		// SCOPE_CYCLE_COUNTER(Stat_PostNetInit);
		Actor->PostNetInit();
	}
}

bool UHScaleActorChannel::CleanUp(const bool bForDestroy, EChannelCloseReason CloseReason)
{
	UHScaleNetDriver* Driver = (UHScaleNetDriver*)Connection->GetDriver();
	check(Driver);
	check(Driver->GetBibliothec());

	UHScalePackageMap* PackageMap = (UHScalePackageMap*)Connection->PackageMap;
	check(PackageMap);

	const bool bActAsServer = Driver->IsServer();
	if (bActAsServer)
	{
		Driver->bActAsClient = true;
	}

	// Cache all FNetGUIDs that will be removed

	TSet<FNetworkGUID> GuidsToRemoveFromMap;
	auto CacheGUID = [&GuidsToRemoveFromMap,PackageMap](UObject* SubObject)
	{
		if (SubObject != nullptr)
		{
			FNetworkGUID FoundGuid = PackageMap->GetNetGUIDFromObject(SubObject);
			if (!FoundGuid.IsValid())
			{
				const FHScaleNetGUID NetworkGuid = PackageMap->FindEntityNetGUID(SubObject);
				if (NetworkGuid.IsValid())
				{
					FoundGuid = PackageMap->FindNetGUIDFromHSNetGUID(NetworkGuid);
				}
			}

			if (FoundGuid.IsValid())
			{
				GuidsToRemoveFromMap.Add(FoundGuid);
			}
		}
	};

	// When game in editor ends, then the World could be null and it will crash engine
	// So this is just a quick check for this case
	if (IsValid(Driver) && IsValid(Driver->GetWorld()))
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		for (UObject* SubObject : CreateSubObjects)
		{
			CacheGUID(SubObject);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if (Actor != nullptr)
		{
			for (UActorComponent* Component : Actor->GetComponents())
			{
				CacheGUID(Component);
			}

			// Cache actor guid as the last one of all
			CacheGUID(Actor);
		}
	}

	// <<< ---- Now we have cached all NetGUIDs that can be removed after successful clean up

	// Some of super functionality is focused for client behavior
	// so before calling this, we're acting as a client and then reverting the state back
	const bool bResult = Super::CleanUp(bForDestroy, CloseReason);

	// Remove all network GUIDs from cache, because from this point are deprecated
	for (const FNetworkGUID Guid : GuidsToRemoveFromMap)
	{
		PackageMap->CleanUpObjectGuid(Guid);
	}

	FHScaleNetworkEntity* Entity = Driver->GetBibliothec()->FetchEntity(EntityId).Get();
	if(Entity)
	{
		Entity->MarkAsStaging();
	}

	if (bActAsServer)
	{
		Driver->bActAsClient = false;
	}

	return bResult;
}

void UHScaleActorChannel::AddedToChannelPool()
{
	Super::AddedToChannelPool();

	// Reset properties to its default values for new actor
	ReplicationRedirectories.Reset();
	OutersData.Reset();
	EntityId = FHScaleNetGUID();
	SchemeOwnership.Reset();
	SchemeObjectBlendMode.Reset();
	PlayerNetOwnerGUID = FHScaleNetGUID();
	bCustomActorIsPendingKill = false;
	bNetInitial = true;
	ChannelSubObjectDirtyCount_HS = 0;
}

TSharedRef<FObjectReplicator>* UHScaleActorChannel::FindObjectReplicator(UObject* Object)
{
	return FindReplicator(Object);
}

UObject* UHScaleActorChannel::ReadContentBlockPayload_HS(FInBunch& Bunch, FNetBitReader& OutPayload, bool& bOutHasRepLayout)
{
	const int32 StartHeaderBits = Bunch.GetPosBits();
	bool bObjectDeleted = false;
	UObject* RepObj = ReadContentBlockHeader_HS(Bunch, bObjectDeleted, bOutHasRepLayout);
	if (Bunch.IsError())
	{
		UE_LOG(LogNet, Error, TEXT( "UActorChannel::ReadContentBlockPayload: ReadContentBlockHeader FAILED. Bunch.IsError() == TRUE. Closing connection. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);

		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderFail);

		return nullptr;
	}

	if (bObjectDeleted)
	{
		OutPayload.SetData(Bunch, 0);

		// Nothing else in this block, continue on
		return nullptr;
	}

	uint32 NumPayloadBits = 0;
	Bunch.SerializeIntPacked(NumPayloadBits);

	UE_NET_TRACE(ContentBlockHeader, Connection->GetInTraceCollector(), StartHeaderBits, Bunch.GetPosBits(), ENetTraceVerbosity::Trace);

	if (Bunch.IsError())
	{
		UE_LOG(LogNet, Error, TEXT( "UActorChannel::ReceivedBunch: Read NumPayloadBits FAILED. Bunch.IsError() == TRUE. Closing connection. RepObj: %s, Channel: %i" ), RepObj ? *RepObj->GetFullName() : TEXT( "NULL" ), ChIndex);

		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockPayloadBitsFail);

		return nullptr;
	}

	OutPayload.SetData(Bunch, NumPayloadBits);

	return RepObj;
}

UObject* UHScaleActorChannel::ReadContentBlockHeader_HS(FInBunch& Bunch, bool& bObjectDeleted, bool& bOutHasRepLayout)
{
	CA_ASSUME(Connection != nullptr);

	const bool IsServer = Connection->Driver->IsServer();
	bObjectDeleted = false;

	UHScalePackageMap* PackageMap = Cast<UHScalePackageMap>(Connection->PackageMap);

	bOutHasRepLayout = Bunch.ReadBit() != 0 ? true : false;

	if (Bunch.IsError())
	{
		UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after bOutHasRepLayout. Actor: %s"), *Actor->GetName());

		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderRepLayoutFail);

		return nullptr;
	}

	const bool bIsActor = Bunch.ReadBit() != 0 ? true : false;

	if (Bunch.IsError())
	{
		UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after reading actor bit. Actor: %s"), *Actor->GetName());

		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderIsActorFail);

		return nullptr;
	}

	if (bIsActor)
	{
		// If this is for the actor on the channel, we don't need to read anything else
		return Actor;
	}

	//
	// We need to handle a sub-object
	//

	FHScaleNetGUID CompGUID;
	Bunch << CompGUID;

	// Note this heavily mirrors what happens in UPackageMapClient::SerializeNewActor
	FNetworkGUID NetGUID;
	UObject* SubObj = nullptr;

	// Manually serialize the object so that we can get the NetGUID (in order to assign it if we spawn the object here)
	Connection->PackageMap->SerializeObject(Bunch, UObject::StaticClass(), SubObj, &NetGUID);

	NET_CHECKSUM_OR_END(Bunch);

	if (Bunch.IsError())
	{
		UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after SerializeObject. SubObj: %s, Actor: %s"), SubObj ? *SubObj->GetName() : TEXT("Null"), *Actor->GetName());

		Bunch.SetError();
		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderObjFail);

		return nullptr;
	}

	if (Bunch.AtEnd())
	{
		UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.AtEnd() == true after SerializeObject. SubObj: %s, Actor: %s"), SubObj ? *SubObj->GetName() : TEXT("Null"), *Actor->GetName());

		Bunch.SetError();
		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderPrematureEnd);

		return nullptr;
	}

	// Validate existing sub-object
	if (SubObj)
	{
		// Sub-objects can't be actors (should just use an actor channel in this case)
		if (Cast<AActor>(SubObj) != nullptr)
		{
			UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Sub-object not allowed to be actor type. SubObj: %s, Actor: %s"), *SubObj->GetName(), *Actor->GetName());

			Bunch.SetError();
			AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderSubObjectActor);

			return nullptr;
		}

		// Sub-objects must reside within their actor parents
		UActorComponent* Component = Cast<UActorComponent>(SubObj);
		if (Component && !SubObj->IsIn(Actor))
		{
			UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Actor component not in parent actor. SubObj: %s, Actor: %s"), *SubObj->GetFullName(), *Actor->GetFullName());

			if (IsServer)
			{
				Bunch.SetError();
				AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderBadParent);

				return nullptr;
			}
		}
	}

	if (IsServer)
	{
		// The server should never need to create sub objects
		if (!SubObj)
		{
			UE_LOG(LogNetTraffic, Error, TEXT("ReadContentBlockHeader: Client attempted to create sub-object. Actor: %s"), *Actor->GetName());

			Bunch.SetError();
			AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderInvalidCreate);

			return nullptr;
		}

		return SubObj;
	}

	const bool bStablyNamed = Bunch.ReadBit() != 0 ? true : false;

	if (Bunch.IsError())
	{
		UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after reading stably named bit. Actor: %s"), *Actor->GetName());

		AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderStablyNamedFail);

		return nullptr;
	}

	if (bStablyNamed)
	{
		// If this is a stably named sub-object, we shouldn't need to create it. Don't raise a bunch error though because this may happen while a level is streaming out.
		if (!SubObj)
		{
			// (ignore though if this is for replays)
			if (!Connection->IsInternalAck())
			{
				UE_LOG(LogNetTraffic, Log, TEXT("ReadContentBlockHeader: Stably named sub-object not found. Its level may have streamed out. Component: %s, Actor: %s"), *Connection->Driver->GuidCache->FullNetGUIDPath(NetGUID), *Actor->GetName());
			}

			return nullptr;
		}

		return SubObj;
	}

	bool bDeleteSubObject = false;
	bool bSerializeClass = true;
	ESubObjectDeleteFlag DeleteFlag = ESubObjectDeleteFlag::Destroyed;

	if (Bunch.EngineNetVer() >= FEngineNetworkCustomVersion::SubObjectDestroyFlag)
	{
		const bool bIsDestroyMessage = Bunch.ReadBit() != 0 ? true : false;

		if (bIsDestroyMessage)
		{
			bDeleteSubObject = true;
			bSerializeClass = false;

			uint8 ReceivedDestroyFlag;
			Bunch << ReceivedDestroyFlag;
			DeleteFlag = static_cast<ESubObjectDeleteFlag>(ReceivedDestroyFlag);

			NET_CHECKSUM(Bunch);
		}
	}
	else
	{
		bSerializeClass = true;
	}

	UObject* SubObjClassObj = nullptr;
	if (bSerializeClass)
	{
		// Serialize the class in case we have to spawn it.
		// Manually serialize the object so that we can get the NetGUID (in order to assign it if we spawn the object here)
		FNetworkGUID ClassNetGUID;
		Connection->PackageMap->SerializeObject(Bunch, UObject::StaticClass(), SubObjClassObj, &ClassNetGUID);

		// When ClassNetGUID is empty it means the sender requested to delete the subobject
		bDeleteSubObject = !ClassNetGUID.IsValid();
	}

	if (bDeleteSubObject)
	{
		if (SubObj)
		{
			// Unmap this object so we can remap it if it becomes relevant again in the future
			MoveMappedObjectToUnmapped(SubObj);

			// Stop tracking this sub-object
			// GetCreatedSubObjects().Remove(SubObj);
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			CreateSubObjects.Remove(SubObj);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS

			// if (Connection != nullptr && Connection->Driver != nullptr)
			// {
			// 	Connection->Driver->NotifySubObjectDestroyed(SubObj);
			// }

			UE_LOG(LogNetSubObject, Verbose, TEXT("NetSubObject: Client received request to %s %s::%s (0x%p) NetGuid %s"), ToString(DeleteFlag), *Actor->GetName(), *GetNameSafe(SubObj), SubObj, *NetGUID.ToString());

			if (DeleteFlag != ESubObjectDeleteFlag::TearOff)
			{
				const FHScaleNetGUID CachedEntityGuid = PackageMap->FindEntityNetGUID(SubObj);
				if (CachedEntityGuid.IsValid())
				{
					PackageMap->CleanUpObjectGuid(PackageMap->GetNetGUIDFromObject(SubObj));

					// Reset properties to its default values for new actor
					ReplicationRedirectories.Remove(CachedEntityGuid);
				}

				Actor->OnSubobjectDestroyFromReplication(SubObj);

				SubObj->PreDestroyFromReplication();
				SubObj->MarkAsGarbage();
			}
		}

		bObjectDeleted = true;
		return nullptr;
	}

	UClass* SubObjClass = Cast<UClass>(SubObjClassObj);

	if (!SubObjClass)
	{
		UE_LOG(LogNetTraffic, Warning, TEXT("UActorChannel::ReadContentBlockHeader: Unable to read sub-object class. Actor: %s"), *Actor->GetName());

		// Valid NetGUID but no class was resolved - this is an error
		if (!SubObj)
		{
			// (unless we're using replays, which could be backwards compatibility kicking in)
			if (!Connection->IsInternalAck())
			{
				UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Unable to read sub-object class (SubObj == NULL). Actor: %s"), *Actor->GetName());

				Bunch.SetError();
				AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderNoSubObjectClass);
				return nullptr;
			}
		}
	}
	else
	{
		if (SubObjClass == UObject::StaticClass())
		{
			UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: SubObjClass == UObject::StaticClass(). Actor: %s"), *Actor->GetName());

			Bunch.SetError();
			AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderUObjectSubObject);

			return nullptr;
		}

		if (SubObjClass->IsChildOf(AActor::StaticClass()))
		{
			UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Sub-object cannot be actor class. Actor: %s"), *Actor->GetName());

			Bunch.SetError();
			AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderAActorSubObject);

			return nullptr;
		}
	}

	UObject* ObjOuter = Actor;
	if (Bunch.EngineNetVer() >= FEngineNetworkCustomVersion::SubObjectOuterChain)
	{
		const bool bActorIsOuter = Bunch.ReadBit() != 0;

		if (Bunch.IsError())
		{
			UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after reading actor is outer bit. Actor: %s"), *Actor->GetName());
			return nullptr;
		}

		if (!bActorIsOuter)
		{
			// If the channel's actor isn't the object's outer, serialize the object's outer here
			Bunch << ObjOuter;

			if (Bunch.IsError())
			{
				UE_LOG(LogNetTraffic, Error, TEXT("UActorChannel::ReadContentBlockHeader: Bunch.IsError() == true after serializing the subobject's outer. Actor: %s"), *Actor->GetName());
				return nullptr;
			}

			if (ObjOuter == nullptr && SubObj == nullptr)
			{
				// When replicating subobjects, the engine will by default replicate lower subobjects before their outers, using the owning actor as the outer on the client.
				// To have a chain of subobjects maintain their correct outers on the client, the subobjects should be replicated top-down, so the outers are received before any subobjects they own.
				UE_LOG(LogNetTraffic, Log, TEXT("UActorChannel::ReadContentBlockHeader: Unable to serialize subobject's outer, using owning actor as outer instead. Class: %s, Actor: %s"), *GetNameSafe(SubObjClass), *GetNameSafe(Actor));

				ObjOuter = Actor;
			}
		}
	}

	// in this case we may still have needed to serialize the outer above, so check again now
	if ((SubObjClass == nullptr) && (SubObj == nullptr) && Connection->IsInternalAck())
	{
		return nullptr;
	}

	if (!SubObj)
	{
		check(!IsServer);

		// Construct the sub-object
		UE_LOG(LogNetTraffic, Log, TEXT( "UActorChannel::ReadContentBlockHeader: Instantiating sub-object. Class: %s, Actor: %s, Outer: %s" ), *GetNameSafe(SubObjClass), *Actor->GetName(), *ObjOuter->GetName());

		SubObj = NewObject<UObject>(ObjOuter, SubObjClass);

		// Sanity check some things
		checkf(SubObj != nullptr, TEXT("UActorChannel::ReadContentBlockHeader: Subobject is NULL after instantiating. Class: %s, Actor %s"), *GetNameSafe(SubObjClass), *Actor->GetName());
		checkf(SubObj->IsIn(ObjOuter), TEXT("UActorChannel::ReadContentBlockHeader: Subobject is not in Outer. SubObject: %s, Actor: %s, Outer: %s"), *SubObj->GetName(), *Actor->GetName(), *ObjOuter->GetName());
		checkf(Cast< AActor >(SubObj) == nullptr, TEXT("UActorChannel::ReadContentBlockHeader: Subobject is an Actor. SubObject: %s, Actor: %s"), *SubObj->GetName(), *Actor->GetName());

		// Notify actor that we created a component from replication
		Actor->OnSubobjectCreatedFromReplication(SubObj);

		// Register the component guid

		UHScaleNetDriver* NetDriver = StaticCast<UHScaleNetDriver*>(Connection->Driver);
		uint8 PrevAction = NetDriver->bActAsClient;
		NetDriver->bActAsClient = 0;
		NetGUID = Connection->Driver->GuidCache->GetOrAssignNetGUID(SubObj);
		NetDriver->bActAsClient = PrevAction;

		// #todo ... add object into ReplicationRedirectories, but we are not propably able to read entity GUID
		// FHScaleNetGUID EntityGuid;
		// ((UHScalePackageMap*)Connection->PackageMap)->AssignOrGenerateHSNetGUIDForObject(EntityGuid, SubObj);
		// ReplicationRedirectories.Add(EntityGuid, )

		// Track which sub-object guids we are creating
		// #tod
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		CreateSubObjects.Add(SubObj);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		// Add this sub-object to the ImportedNetGuids list so we can possibly map this object if needed
		if (ensureMsgf(NetGUID.IsValid(), TEXT("Channel tried to add an invalid GUID to the import list: %s"), *Describe()))
		{
			LLM_SCOPE_BYTAG(GuidCache);
			Connection->Driver->GuidCache->ImportedNetGuids.Add(NetGUID);
		}
	}

	// in case of newly spawned dynamic sub objects, we need to register the object with HScaleNetGUID
	if (CompGUID.IsValid() && IsValid(SubObj))
	{
		FHScaleNetGUID FoundEntityId = PackageMap->FindEntityNetGUID(SubObj);
		if (!FoundEntityId.IsValid())
		{
			PackageMap->AssignOrGenerateHSNetGUIDForObject(CompGUID, SubObj);
		}
	}

	return SubObj;
}

TSharedRef<FObjectReplicator>& UHScaleActorChannel::FindOrCreateReplicator_HS(UObject* Obj)
{
	return FindOrCreateReplicator(Obj);
}

void UHScaleActorChannel::WriteExistingActorHeader(bool& bWroteSomethingImportant, FOutBunch& Bunch)
{
	UHScalePackageMap* ScalePackageMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	FNetworkGUID AssignedId;
	UObject* InTarget = Actor;
	bWroteSomethingImportant = ScalePackageMap->SerializeObject(Bunch, UObject::StaticClass(), InTarget, &AssignedId);
	bool bContainsArchetypeData = false;
	Bunch.WriteBit(bContainsArchetypeData ? 1 : 0);
	bWroteSomethingImportant &= ScalePackageMap->WriteActorHeader(Bunch, Actor);
}

void UHScaleActorChannel::ReadExistingActorHeader(FInBunch& Bunch)
{
	Bunch.ReadBit();
	FHScaleNetGUID NetGUID;
	Bunch << NetGUID;
	FHScaleExportFlags Flags;
	Bunch << Flags.Value;

	bool bIncludeSpawnInfo = !!Bunch.ReadBit();
	check(!bIncludeSpawnInfo)
}

bool UHScaleActorChannel::UpdateDeletedSubObjects_HS(FOutBunch& Bunch)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NetDeletedSubObjects);

	bool bWroteSomethingImportant = false;

	auto DeleteSubObject = [this, &bWroteSomethingImportant, &Bunch](FNetworkGUID ObjectNetGUID, const TWeakObjectPtr<UObject>& ObjectPtrToRemove, ESubObjectDeleteFlag DeleteFlag)
	{
		if (ObjectNetGUID.IsValid())
		{
			UE_NET_TRACE_SCOPE(ContentBlockForSubObjectDelete, Bunch, GetTraceCollector(Bunch), ENetTraceVerbosity::Trace);
			UE_NET_TRACE_OBJECT_SCOPE(ObjectNetGUID, Bunch, GetTraceCollector(Bunch), ENetTraceVerbosity::Trace);

			UE_LOG(LogNetSubObject, Verbose, TEXT("NetSubObject: Sending request to %s %s::%s (0x%p) NetGUID %s"), ToString(DeleteFlag), *Actor->GetName(), *GetNameSafe(ObjectPtrToRemove.GetEvenIfUnreachable()), ObjectPtrToRemove.GetEvenIfUnreachable(), *ObjectNetGUID.ToString());

			// Write a deletion content header:
			WriteContentBlockForSubObjectDelete(Bunch, ObjectNetGUID, DeleteFlag);

			bWroteSomethingImportant = true;
			Bunch.bReliable = true;
		}
		else
		{
			UE_LOG(LogNetTraffic, Error, TEXT("Unable to write subobject delete for %s::%s (0x%p), object replicator has invalid NetGUID"), *GetPathNameSafe(Actor), *GetNameSafe(ObjectPtrToRemove.GetEvenIfUnreachable()), ObjectPtrToRemove.GetEvenIfUnreachable());
		}
	};

	// UE::Net::FDormantObjectMap* DormantObjects = Connection->GetDormantFlushedObjectsForActor(Actor);
	//
	// if (DormantObjects)
	// {
	// 	// no need to track these if they're in the replication map
	// 	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	// 	{
	// 		DormantObjects->Remove(RepComp.Value()->ObjectNetGUID);
	// 	}
	// }

#if UE_REPLICATED_OBJECT_REFCOUNTING
	TArray<TWeakObjectPtr<UObject>, TInlineAllocator<16>> SubObjectsRemoved;

	// Check if the dirty count is different from our last update
	FNetworkObjectList::FActorInvalidSubObjectView InvalidSubObjects = Connection->Driver->GetNetworkObjectList().FindActorInvalidSubObjects(Actor);
	if (InvalidSubObjects.HasInvalidSubObjects() && ChannelSubObjectDirtyCount_HS != InvalidSubObjects.GetDirtyCount())
	{
		ChannelSubObjectDirtyCount_HS = InvalidSubObjects.GetDirtyCount();

		for (const FNetworkObjectList::FSubObjectChannelReference& SubObjectRef : InvalidSubObjects.GetInvalidSubObjects())
		{
			ensure(SubObjectRef.Status != ENetSubObjectStatus::Active);

			// The TearOff won't be sent if the Object pointer is fully deleted once we get here.
			// This would be fixed by converting the ReplicationMap to stop using raw pointers as the key.
			// Instead the ReplicationMap iteration below should pick this deleted object and send for it to be destroyed.
			if (UObject* ObjectToRemove = SubObjectRef.SubObjectPtr.GetEvenIfUnreachable())
			{
				if (TSharedRef<FObjectReplicator>* SubObjectReplicator = ReplicationMap.Find(ObjectToRemove))
				{
					const ESubObjectDeleteFlag DeleteFlag = SubObjectRef.IsTearOff() ? ESubObjectDeleteFlag::TearOff : ESubObjectDeleteFlag::ForceDelete;
					SubObjectsRemoved.Add(SubObjectRef.SubObjectPtr);
					DeleteSubObject((*SubObjectReplicator)->ObjectNetGUID, SubObjectRef.SubObjectPtr, DeleteFlag);
					(*SubObjectReplicator)->CleanUp();
					ReplicationMap.Remove(ObjectToRemove);
				}
			}
		}

		if (!SubObjectsRemoved.IsEmpty())
		{
			Connection->Driver->GetNetworkObjectList().RemoveMultipleInvalidSubObjectChannelReference(Actor, SubObjectsRemoved, this);
			SubObjectsRemoved.Reset();
		}
	}
#endif //#if UE_REPLICATED_OBJECT_REFCOUNTING

	// Look for deleted subobjects
	for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
	{
		TSharedRef<FObjectReplicator>& SubObjectReplicator = RepComp.Value();

		const TWeakObjectPtr<UObject> WeakObjPtr = SubObjectReplicator->GetWeakObjectPtr();
		if (!WeakObjPtr.IsValid())
		{
			// The only way this case would be possible is if someone tried destroying the Actor as a part of
			// a Subobject's Pre / Post replication, during Replicate Subobjects, or OnSerializeNewActor.
			// All of those are bad.
			if (!ensureMsgf(ActorReplicator.Get() != &SubObjectReplicator.Get(), TEXT("UActorChannel::ReplicateActor: Actor was deleting during replication: %s"), *Describe()))
			{
				ActorReplicator.Reset();
			}

#if UE_REPLICATED_OBJECT_REFCOUNTING
			SubObjectsRemoved.Add(WeakObjPtr);
#endif

			DeleteSubObject(SubObjectReplicator->ObjectNetGUID, WeakObjPtr, ESubObjectDeleteFlag::Destroyed);
			SubObjectReplicator->CleanUp();

			RepComp.RemoveCurrent();
		}
	}

	// if (DormantObjects)
	// {
	// 	for (auto DormComp = DormantObjects->CreateConstIterator(); DormComp; ++DormComp)
	// 	{
	// 		const FNetworkGUID& NetGuid = DormComp.Key();
	// 		const TWeakObjectPtr<UObject>& WeakObjPtr = DormComp.Value();
	//
	// 		if (!WeakObjPtr.IsValid())
	// 		{
	// 			DeleteSubObject(NetGuid, WeakObjPtr, ESubObjectDeleteFlag::Destroyed);
	// 		}
	// 	}
	//
	// 	Connection->ClearDormantFlushedObjectsForActor(Actor);
	// }

#if UE_REPLICATED_OBJECT_REFCOUNTING
	if (!SubObjectsRemoved.IsEmpty())
	{
		Connection->Driver->GetNetworkObjectList().RemoveMultipleSubObjectChannelReference(Actor, SubObjectsRemoved, this);
	}
#endif

	return bWroteSomethingImportant;
}

void UHScaleActorChannel::ReplicateActorToMemoryLayer()
{
	LLM_SCOPE_BYTAG(NetChannel);
	//SCOPE_CYCLE_COUNTER(STAT_NetReplicateActorTime);

	check(Actor);
	check(!Closing);
	check(Connection);

	const UWorld* const ActorWorld = Actor->GetWorld();
	checkf(ActorWorld, TEXT("ActorWorld for Actor [%s] is Null"), *GetPathNameSafe(Actor));

#if STATS || ENABLE_STATNAMEDEVENTS
	UClass* ParentNativeClass = GetParentNativeClass(Actor->GetClass());
	SCOPE_CYCLE_UOBJECT(ParentNativeClass, ParentNativeClass);
#endif

	const bool bReplay = Connection->IsReplay();
	const bool bEnableScopedCycleCounter = !bReplay && GReplicateActorTimingEnabled;
	FSimpleScopeSecondsCounter ScopedSecondsCounter(GReplicateActorTimeSeconds, bEnableScopedCycleCounter);

	// ignore hysteresis during checkpoints
	if (bIsInDormancyHysteresis && (Connection->ResendAllDataState == EResendAllDataState::None))
	{
		return;
	}

	// triggering replication of an Actor while already in the middle of replication can result in invalid data being sent and is therefore illegal
	if (bIsReplicatingActor)
	{
		FString Error(FString::Printf(TEXT("ReplicateActor called while already replicating! %s"), *Describe()));
		UE_LOG(LogNet, Log, TEXT("%s"), *Error);
		ensureMsgf(false, TEXT("%s"), *Error);
		return;
	}

	if (bCustomActorIsPendingKill)
	{
		// Don't need to do anything, because it should have already been logged.
		return;
	}

	// If our Actor is PendingKill, that's bad. It means that somehow it wasn't properly removed
	// from the NetDriver or ReplicationDriver.
	if (!IsValidChecked(Actor) || Actor->IsUnreachable())
	{
		bCustomActorIsPendingKill = true;
		ActorReplicator.Reset();
		FString Error(FString::Printf(TEXT("ReplicateActor called with PendingKill Actor! %s"), *Describe()));
		UE_LOG(LogNet, Log, TEXT("%s"), *Error);
		ensureMsgf(false, TEXT("%s"), *Error);
		return;
	}

	if (!IsActorReadyForReplication())
	{
		UE_LOG(LogNet, Verbose, TEXT("ReplicateActor ignored since actor is not BeginPlay yet: %s"), *Describe());
		return;
	}

	// The package map shouldn't have any carry over guids
	// Static cast is fine here, since we check above.
	UHScalePackageMap* ScalePackageMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	if (ScalePackageMap->GetMustBeMappedGuidsInLastBunch().Num() != 0)
	{
		UE_LOG(LogNet, Warning, TEXT("ReplicateActor: PackageMap->GetMustBeMappedGuidsInLastBunch().Num() != 0: %i: Channel: %s"), ScalePackageMap->GetMustBeMappedGuidsInLastBunch().Num(), *Describe());
	}

	// Create an outgoing bunch, and skip this actor if the channel is saturated.
	FOutBunch Bunch(this, false);

	if (Bunch.IsError())
	{
		return;
	}

	FGuardValue_Bitfield(bIsReplicatingActor, true);
	// FScopedRepContext RepContext(Connection, Actor);

	FHScaleRepFlags RepFlags;
	RepFlags.bUseCustomSubobjectReplication = true;

	// Send initial stuff.
	if (OpenPacketId.First != INDEX_NONE && (Connection->ResendAllDataState == EResendAllDataState::None))
	{
		if (!SpawnAcked && OpenAcked)
		{
			// After receiving ack to the spawn, force refresh of all subsequent unreliable packets, which could
			// have been lost due to ordering problems. Note: We could avoid this by doing it in FActorChannel::ReceivedAck,
			// and avoid dirtying properties whose acks were received *after* the spawn-ack (tricky ordering issues though).
			SpawnAcked = 1;
			for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
			{
				RepComp.Value()->ForceRefreshUnreliableProperties();
			}
		}
	}
	else
	{
		if (Connection->ResendAllDataState == EResendAllDataState::SinceCheckpoint)
		{
			RepFlags.bNetInitial = !bOpenedForCheckpoint;
		}
		else
		{
			RepFlags.bNetInitial = true;
		}

		Bunch.bClose = Actor->bNetTemporary;
		Bunch.bReliable = true; // Net temporary sends need to be reliable as well to force them to retry
	}

	// @warning - Be careful, do not modify the owning actor
	const AActor* NetOwningActor = Actor->GetNetOwner();
	const FHScaleNetGUID LocalConnectionId = ((UHScaleConnection*)Connection)->GetSessionNetGUID();

	if (Cast<APlayerController>(NetOwningActor))
	{
		// The player controller is not replicated to network, so we translate it as an quark player connection
		if (LocalConnectionId.IsValid())
		{
			PlayerNetOwnerGUID = LocalConnectionId;
		}
	}
	else
	{
		// Fetch with player connection memory
		// const FHScaleNetGUID NetGuid = ScalePackageMap->FindEntityNetGUID(const_cast<AActor*>(NetOwningActor));
		// if (NetGuid.IsValid())
		// {
		// 	FHScaleNetworkBibliothec* Bibliothec = ((UHScaleNetDriver*)Connection->Driver)->GetBibliothec();
		// 	check(Bibliothec);
		//
		// 	TSharedPtr<FHScaleNetworkEntity> NetOwningEntity = Bibliothec->FetchEntity(NetGuid);
		// 	check(NetOwningEntity); // Now should be valid and relevant because actor exists with valid NetGuid
		//
		// 	// it should be player connection entity now
		// 	const FHScaleNetworkEntity* OwningPlayerEntity = NetOwningEntity->GetNetOwner();
		// 	if (OwningPlayerEntity)
		// 	{
		// 		check(OwningPlayerEntity->IsPlayer());
		//
		// 		PlayerNetOwnerGUID = OwningPlayerEntity->EntityId;
		// 	}
		// }

		bool bAnotherPlayerIsOwner = false;
		const FHScaleNetGUID NetGuid = ScalePackageMap->FindEntityNetGUID(Actor);
		if (NetGuid.IsValid())
		{
			// This means, its already in memory layer, and it can have a different owner (diff client from network,
			// because player controllers are not replicated)
			
			FHScaleNetworkBibliothec* Bibliothec = ((UHScaleNetDriver*)Connection->Driver)->GetBibliothec();
			check(Bibliothec);

			TSharedPtr<FHScaleNetworkEntity> NetOwningEntity = Bibliothec->FetchEntity(NetGuid);
			check(NetOwningEntity); // Now should be valid and relevant because actor exists with valid NetGuid

			// it should be player connection entity now
			const FHScaleNetworkEntity* OwningPlayerEntity = NetOwningEntity->GetNetOwner();
			if (OwningPlayerEntity && OwningPlayerEntity->IsPlayer())
			{
				bAnotherPlayerIsOwner = true;
				PlayerNetOwnerGUID = OwningPlayerEntity->EntityId;
			}
		}
		
		if(!bAnotherPlayerIsOwner)
		{
			// the entity in memory doesn't exist, check the scheme if there exist an owner
			if (!SchemeOwnership.IsSet())
			{
				UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(Connection->Driver->GetReplicationDriver());
				check(RepDriver);
				check(RepDriver->GetSchema());

				SchemeOwnership = RepDriver->GetSchema()->GetObjectOwnershipValue_ByClass(Actor->GetClass());
			}

			if (SchemeOwnership.GetValue() == EHScale_Ownership::Creator)
			{
				PlayerNetOwnerGUID = LocalConnectionId;
			}
			else
			{
				// No owner there, maybe not needed this else branch
				RepFlags.bHasAnyNetOwner = false;
				RepFlags.bNetOwner = false;
			}
		}
	}

	if (PlayerNetOwnerGUID.IsValid())
	{
		RepFlags.bHasAnyNetOwner = true;
		if (LocalConnectionId == PlayerNetOwnerGUID)
		{
			RepFlags.bNetOwner = true;
		}
	}

	// Setup blend mode
	if (!SchemeObjectBlendMode.IsSet())
	{
		UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(Connection->Driver->GetReplicationDriver());
		check(RepDriver);
		check(RepDriver->GetSchema());

		SchemeObjectBlendMode = RepDriver->GetSchema()->GetObjectBlendModeValue_ByClass(Actor->GetClass());
		SchemeObjectBlendMode = EHScale_Blend::Owner; // #todo .... this is temporary until we will have tool that creates scheme
	}

	RepFlags.bOwnerBlendMode = SchemeObjectBlendMode.GetValue() == EHScale_Blend::Owner;

	// ----------------------------------------------------------
	// If initial, send init data.
	// ----------------------------------------------------------

	// Correct actor role based on the flags, and check if it can be still replicated to memory layer
	FRoleCorrection(Actor, RepFlags);
	const ENetRole LocalActorRole = Actor->GetLocalRole();
	if (LocalActorRole != ROLE_Authority && LocalActorRole != ROLE_AutonomousProxy)
	{
		// Not authority to replicate actor into memory
		return;
	}
	Actor->CallPreReplication(Connection->Driver);

	bool bWroteSomethingImportant = false;

	if (RepFlags.bNetInitial && OpenedLocally && bNetInitial)
	{
		UE_NET_TRACE_SCOPE(NewActor, Bunch, GetTraceCollector(Bunch), ENetTraceVerbosity::Trace);

		bWroteSomethingImportant = !((UHScaleConnection*)Connection)->FreePlayerObjectIDCache.IsEmpty() && ScalePackageMap->SerializeNewActor(Bunch, this, static_cast<AActor*&>(Actor));
	}
	else
	{
		// Serialize actor's existing id
		WriteExistingActorHeader(bWroteSomethingImportant, Bunch);
	}

	// If header data were not set correctly, pass
	if (!bWroteSomethingImportant)
	{
		// we should check if we have enough free IDs that can be assigned to the new actor
		ensure(false);
		return;
	}

	// Possibly downgrade role of actor if this connection doesn't own it
	FScopedRoleDowngrade ScopedRoleDowngrade(Actor, RepFlags);

	// RepFlags.bNetSimulated = (Actor->GetRemoteRole() == ROLE_SimulatedProxy);
	RepFlags.bNetSimulated = true; // #todo: put proper condition 
	RepFlags.bRepPhysics = Actor->GetReplicatedMovement().bRepPhysics;
	RepFlags.bReplay = bReplay;
	RepFlags.bClientReplay = ActorWorld->IsRecordingClientReplay();
	RepFlags.bForceInitialDirty = Connection->IsForceInitialDirty();
	if (EnumHasAnyFlags(ActorReplicator->RepLayout->GetFlags(), ERepLayoutFlags::HasDynamicConditionProperties))
	{
		if (const FRepState* RepState = ActorReplicator->RepState.Get())
		{
			const FSendingRepState* SendingRepState = RepState->GetSendingRepState();
			if (const FRepChangedPropertyTracker* PropertyTracker = SendingRepState ? SendingRepState->RepChangedPropertyTracker.Get() : nullptr)
			{
				RepFlags.CondDynamicChangeCounter = PropertyTracker->GetDynamicConditionChangeCounter();
			}
		}
	}

	UE_LOG(LogNetTraffic, Log, TEXT("Replicate %s, bNetInitial: %d, bNetOwner: %d"), *Actor->GetName(), RepFlags.bNetInitial, RepFlags.bNetOwner);

	FMemMark MemMark(FMemStack::Get()); // The calls to ReplicateProperties will allocate memory on FMemStack::Get(), and use it in ::PostSendBunch. we free it below

	// ----------------------------------------------------------
	// Replicate Actor and Component properties and RPCs
	// ---------------------------------------------------

#if USE_NETWORK_PROFILER
	const uint32 ActorReplicateStartTime = GNetworkProfiler.IsTrackingEnabled() ? FPlatformTime::Cycles() : 0;
#endif

	// The Actor
	{
		UE_NET_TRACE_OBJECT_SCOPE(ActorReplicator->ObjectNetGUID, Bunch, GetTraceCollector(Bunch), ENetTraceVerbosity::Trace);

		const bool bCanSkipUpdate = ActorReplicator->CanSkipUpdate(RepFlags);

		if (!bCanSkipUpdate)
		{
			if (ActorReplicator->RepState->GetSendingRepState())
			{
				bWroteSomethingImportant |= ActorReplicator->ReplicateProperties(Bunch, RepFlags);
			}
		}

		bWroteSomethingImportant |= DoSubObjectReplication(Bunch, RepFlags);

		bWroteSomethingImportant |= UpdateDeletedSubObjects_HS(Bunch);
	}

	// Create an empty new bunch that will be send into this function, but inside of ReplicateSubobjectCustom() function
	// write data into a bunch defined inside of this class and has to be reset before this function. After DoSubObjectReplication90 function
	// copy data into the main bunch. This will ensure, the main bunch will not contain any user dev data included from actor component replication
	// that will destroy the formating of the main bunch defined for our networking. If we would use the main bunch inside of this function, user can add anything
	// into bunch in function AActor::ReplicateSubobjects that will destroy the formating of the main bunch
	// bWroteSomethingImportant |= DoSubObjectReplication(Bunch, RepFlags);

	if (Connection->ResendAllDataState != EResendAllDataState::None)
	{
		if (bWroteSomethingImportant)
		{
			// #todo check
			SendBunch(&Bunch, true);
			OpenPacketId.First++;
		}

		MemMark.Pop();
		NETWORK_PROFILER(GNetworkProfiler.TrackReplicateActor(Actor, RepFlags, FPlatformTime::Cycles() - ActorReplicateStartTime, Connection));

		return;
	}

	{
		// #todo check
		//bWroteSomethingImportant |= UpdateDeletedSubObjects(Bunch);
	}

	NETWORK_PROFILER(GNetworkProfiler.TrackReplicateActor(Actor, RepFlags, FPlatformTime::Cycles() - ActorReplicateStartTime, Connection));

	// -----------------------------
	// Send if necessary
	// -----------------------------

	if (bWroteSomethingImportant)
	{
		// #todo check

		// We must exit the collection scope to report data correctly
		FPacketIdRange PacketRange = SendBunch(&Bunch, true);

		for (auto RepComp = ReplicationMap.CreateIterator(); RepComp; ++RepComp)
		{
			RepComp.Value()->PostSendBunch(PacketRange, Bunch.bReliable);
		}
	}

	// If we evaluated everything, mark LastUpdateTime, even if nothing changed.
	LastUpdateTime = Connection->Driver->GetElapsedTime();

	MemMark.Pop();

	bForceCompareProperties = false; // Only do this once per frame when set
}