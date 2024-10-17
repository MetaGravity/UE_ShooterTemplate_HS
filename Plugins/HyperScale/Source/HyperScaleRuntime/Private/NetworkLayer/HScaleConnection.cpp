// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkLayer/HScaleConnection.h"

#include "quark.h"
#include "Core/HScaleCommons.h"
#include "Core/HScaleResources.h"
#include "Engine/ActorChannel.h"
#include "Events/HScaleEventsDriver.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "NetworkLayer/HScaleUpdates.h"
#include "ReplicationLayer/HScaleRepDriver.h"
#include "ReplicationLayer/HScaleReplicationLibrary.h"
#include "Runtime/Engine/Private/Net/NetSubObjectRegistryGetter.h"
#include "Utils/HScalePrivateAccessors.h"
#include "Utils/HScaleStatics.h"


using namespace quark;

HSCALE_IMPLEMENT_GET_PRIVATE_VAR(UNetConnection, NetworkCustomVersions, FCustomVersionContainer);

#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)
#define WINSOCK_MAX_PACKET (512)

UHScaleConnection::UHScaleConnection()
{
	PackageMapClass = UHScalePackageMap::StaticClass();
}

void UHScaleConnection::InitBase(UNetDriver* InDriver, FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// #todo ... maybe just copy part of the super function here because we probably do not need everything from the NetConnection for hyperscale
	Super::InitBase(InDriver, InSocket, InURL, InState, WINSOCK_MAX_PACKET, UDP_HEADER_SIZE);

	LevelRoles = UHScaleReplicationLibrary::GetRolesFromURL(URL);

	// PlayerController

	// NewPlayerController->NetPlayerIndex = InNetPlayerIndex;
	// NewPlayerController->SetRole(ROLE_Authority);
	// NewPlayerController->SetReplicates(RemoteRole != ROLE_None);
	// if (RemoteRole == ROLE_AutonomousProxy)
	// {
	// 	NewPlayerController->SetAutonomousProxy(true);
	// }
	// NewPlayerController->SetPlayer(NewPlayer);
	// GameMode->PostLogin(NewPlayerController);
}


void UHScaleConnection::Receive()
{
	check(IsConnectionActive())
	session* QuarkSession = GetNetworkSession();
	const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Driver);
	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();
	for (auto Update = QuarkSession->try_receive(); Update;
	     Update = QuarkSession->try_receive())
	{
		if (!Update.has_value()) continue;

		const update_type UpdateType = Update.value().type();
		if (UpdateType == update_type::player)
		{
			const std::optional<remote_player_update> PlayerUpdate = Update.value().player();
			if (PlayerUpdate.has_value()) { Bibliothec->HandlePlayersNetworkUpdate(PlayerUpdate.value()); }
		}
		else if (UpdateType == update_type::object)
		{
			const std::optional<remote_object_update> ObjectUpdate = Update.value().object();
			if (ObjectUpdate.has_value()) { Bibliothec->HandleObjectsNetworkUpdate(ObjectUpdate.value()); }
		}
		else if (UpdateType == update_type::event)
		{
			const std::optional<remote_event> EventUpdate = Update.value().event();
			if (EventUpdate.has_value()) { EventsDriver->HandleEvents(EventUpdate.value()); }
		}
	}
}

void UHScaleConnection::Send()
{
	check(IsConnectionActive())
	session* QuarkSession = GetNetworkSession();
	const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Driver);
	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();
	TArray<FHScaleLocalUpdate> Updates;
	Bibliothec->Pull(Updates);

	for (const FHScaleLocalUpdate& Update : Updates)
	{
		if (Update.bIsPlayer)
		{
			for (const auto& Atb : Update.Attributes)
			{
				QuarkSession->send(quark::local_update::player(Atb.AttributeId, Atb.Value), Atb.Qos);
			}
		}
		else
		{
			for (const auto& Atb : Update.Attributes)
			{
				QuarkSession->send(quark::local_update::object(Update.ObjectId, Atb.AttributeId, Atb.Value), Atb.Qos);
			}
		}
	}
}

int32 UHScaleConnection::GetOrCreateChannelIndexForEntity(FHScaleNetworkEntity* Entity)
{
	// fetch owner entity, that owns the channel
	FHScaleNetworkEntity* ChannelEntity = Entity->GetChannelOwner();
	if (!ChannelEntity || !ChannelEntity->IsReadyForReplication()) { return INDEX_NONE; }
	uint32 ChIndex = ChannelEntity->ChannelIndex;
	if (ChIndex == INDEX_NONE)
	{
		ChIndex = GetFreeChannelIndex(NAME_Actor);
	}
	ChannelEntity->ChannelIndex = ChIndex;
	return ChIndex;
}

void UHScaleConnection::PullUnmappedEntityUpdate(const FHScaleNetGUID& EntityId)
{
	if (!UnMappedObjPtrs.Contains(EntityId)) { return; }

	const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Driver);
	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();
	UHScalePackageMap* PkgMp = Cast<UHScalePackageMap>(PackageMap);

	TSet<TTuple<FHScaleNetGUID, uint16>> Tuples = *UnMappedObjPtrs.Find(EntityId);

	for (TTuple<FHScaleNetGUID, uint16>& Tup : Tuples)
	{
		FHScaleNetGUID RefEntityId = Tup.Key;
		TSharedPtr<FHScaleNetworkEntity> RefEntity = Bibliothec->FetchEntity(RefEntityId);
		if (!RefEntity.IsValid() || !RefEntity->IsReadyForReplication()) { continue; }

		FHScaleNetworkEntity* OwnerPtr = RefEntity->GetChannelOwner();
		if (!OwnerPtr || !OwnerPtr->IsReadyForReplication()) continue;

		const int32 RefChIndex = GetOrCreateChannelIndexForEntity(RefEntity.Get());
		if (RefChIndex == INDEX_NONE) { continue; }
		UChannel* RefChannel = Channels[RefChIndex];
		if (RefChannel == nullptr) { continue; }
		if (const UObject* RefObj = PkgMp->FindObjectFromEntityID(RefEntityId); !RefObj) { continue; }
		FInBunch RefBunch(this);
		RefBunch.bOpen = 0;
		RefBunch.bClose = 0;
		// Bunch.bReliable = 1;
		RefBunch.ChIndex = RefChIndex;
		RefBunch.bPartial = 0;
		RefBunch.ChName = NAME_Actor;
		RefBunch.PackageMap = PackageMap;

		TMap<FHScaleNetGUID, TSet<uint16>> AttributesToPull;
		TSet<uint16> Attr;
		Attr.Add(Tup.Value);
		AttributesToPull.Add(RefEntityId, Attr);

		OwnerPtr->PullUpdate(RefBunch, AttributesToPull);

		RefChannel->ReceivedBunch(RefBunch);
	}

	UnMappedObjPtrs.Remove(EntityId);
}

void UHScaleConnection::PullDataFromMemoryLayer()
{
	const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Driver);
	check(NetDriver);

	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();
	check(Bibliothec);

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(PackageMap);
	check(PkgMap);

	// <<< --- Start of collecting of all actors that needs to be updated in a game simulation
	TArray<FHScaleNetGUID> FilteredList;
	for (TSet<FHScaleNetGUID>::TConstIterator It = Bibliothec->GetServerDirtyEntitiesIterator(); It; ++It)
	{
		const FHScaleNetGUID EntityId = *It;
		TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(EntityId);

		if (!Entity.IsValid()) continue;
		if (Entity->IsPlayer()) continue;
		if (!Entity->IsActor()) continue;

		if (Entity->IsStatic())
		{
			// #todo ... map static entity with memory data
			// If this is not successful, then ensure(false) and continue;
			continue;
		}
		else if (!Cast<AActor>(PkgMap->FindObjectFromEntityID(EntityId)))
		{
			continue; // <<< --- Do not include not spawned entities
		}

		FilteredList.Add(EntityId);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("EntityId %llu is added to filtered list"), EntityId.Get())
	}
	// <<< --- End of collecting of all actors that needs to be updated in a game simulation

	/*
	TArray<FHScaleNetGUID> FilteredListWithOwners;
	// Go through the filtered list and see if any of the owners are not spawned yet and if so add them to the front of the list 
	for (const FHScaleNetGUID& EntityId : FilteredList)
	{
		TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(EntityId);
		FHScaleNetGUID OwnerId = Entity->Owner;
		FHScaleNetGUID ChildId = EntityId;
	
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("EntityId %llu is added to filtered owner list"), EntityId.Get())
		FilteredListWithOwners.Add(ChildId);
		// #todo: skipping static objects currently
		// #todo: Currently checking if a list contains a value in O(N), needs better optimization here
		while (OwnerId.IsValid() && !OwnerId.IsStatic() && !OwnerId.IsPlayer() &&
		       !FilteredListWithOwners.Contains(OwnerId)) // if its already part of the dirty set, then nothing to do
		{
			TSharedPtr<FHScaleNetworkEntity> Owner = Bibliothec->FetchEntity(OwnerId);
			if (!Owner.IsValid())
			{
				UE_LOG(Log_HyperScaleReplication, Warning, TEXT("Found Invalid owner %llu for Entity %llu"), OwnerId.Get(), ChildId.Get())
				break;
			}
	
			const UObject* OwnerObj = PkgMap->FindObjectFromEntityID(OwnerId);
			if (!IsValid(OwnerObj))
			{
				FilteredListWithOwners.Insert(OwnerId, 0); // this will insert the entityId to the front
				UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("EntityId %llu is added to filtered owner list"), OwnerId.Get())
			}
	
			ChildId = OwnerId;
			OwnerId = Owner->Owner;
		}
	}
	*/

	TSet<FHScaleNetGUID> ProcessedList;
	// Now iterate through filtered list and pull updates from memory layer
	//for (const FHScaleNetGUID& EntityId : FilteredListWithOwners)
	for (const FHScaleNetGUID& EntityId : FilteredList)
	{
		TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(EntityId);
		check(Entity);

		AActor* Actor = Cast<AActor>(PkgMap->FindObjectFromEntityID(EntityId));
		check(Actor);

		UActorChannel* ActorChannel = FindActorChannelRef(Actor);
		check(ActorChannel);

		// check for owner relevance with relevancy manager
		const int32 ChIndex = GetOrCreateChannelIndexForEntity(Entity.Get());

		FInBunch Bunch(this);
		Bunch.bOpen = false;
		Bunch.bClose = 0; // #todo: this is the place for actor destroy, along with Bunch.CloseReason
		// Bunch.bReliable = 1;
		Bunch.ChIndex = ChIndex;
		Bunch.bPartial = 0;
		Bunch.ChName = NAME_Actor;
		Bunch.PackageMap = PackageMap;

		const bool bResult = Bibliothec->PullUpdate(Bunch, EntityId);

		if (Bunch.GetNumBits() > 0)
		{
			ActorChannel->ReceivedBunch(Bunch);

			if (bResult)
			{
				ProcessedList.Add(EntityId);
			}

			PullUnmappedEntityUpdate(EntityId);
		}
	}

	Bibliothec->ClearServerDirtyEntities(ProcessedList);
}

int32 UHScaleConnection::GetFreeChannelIndex(const FName& ChName)
{
	int32 ChIndex;
	int32 FirstChannel = 1;

	const int32 StaticChannelIndex = Driver->ChannelDefinitionMap[ChName].StaticChannelIndex;
	if (StaticChannelIndex != INDEX_NONE)
	{
		FirstChannel = StaticChannelIndex;
	}

	// Search the channel array for an available location
	for (ChIndex = FirstChannel; ChIndex < Channels.Num(); ChIndex++)
	{
		// const bool bIgnoreReserved = bIgnoreReservedChannels && ReservedChannels.Contains(ChIndex);
		// const bool bIgnoreRemapped = bAllowExistingChannelIndex && ChannelIndexMap.Contains(ChIndex);

		if (!Channels[ChIndex])
		{
			break;
		}
	}

	if (ChIndex == Channels.Num())
	{
		ChIndex = INDEX_NONE;
	}

	return ChIndex;
}

void UHScaleConnection::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsConnectionActive()) { return; }
	Send();
	Receive();
	PullDataFromMemoryLayer();
	EventsDriver->Tick(DeltaSeconds);
}

FString UHScaleConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	const FString WideServerAddress = FString::Printf(TEXT("%s:%s"), *URL.Host, *FString::FromInt(URL.Port));
	return WideServerAddress;
}

void UHScaleConnection::NotifyActorDestroyed(AActor* Actor, bool IsSeamlessTravel)
{
	Super::NotifyActorDestroyed(Actor, IsSeamlessTravel);

	// #todo .. check if this function is required? we are already handling actor destruction logic in UHScaleRepDriver::RemoveNetworkActor
	// if (!IsConnectionActive()) return;

	// UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(PackageMap);
	// check(PkgMap);
	//
	// FHScaleNetGUID ActorNetGUID = PkgMap->FindEntityNetGUID(Actor);
	// if (!ActorNetGUID.IsValid())
	// {
	// 	UE_LOG(Log_HyperScaleReplication, Error, TEXT("Actor has no valid NetGUID"));
	// 	return;
	// }

	// #todo ... If player moves into another level, it will destroy all actors, we cannot send any destroy event except actors that are owned by connection
	// if(GetRelevancyManager() && !GetRelevancyManager()->IsEntityDestroyedByRelevancy(ActorNetGUID))
	// {
	// 	UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(Driver->GetReplicationDriver());
	// 	check(RepDriver);
	// 	
	// 	RepDriver->FlushEntity(ActorNetGUID);
	// 	GetEventsDriver()->SendObjectDespawnEvent(ActorNetGUID);
	// }
}

// TSharedPtr<FObjectReplicator> UHScaleConnection::CreateReplicatorForNewActorChannel(UObject* Object)
// {
// 	TSharedPtr<FObjectReplicator> NewReplicator = MakeShareable(new HScaleObjectReplicator());
// 	NewReplicator->InitWithObject(Object, this, true);
// 	return NewReplicator;
// }

void UHScaleConnection::SubscribeRelevancy()
{
	// first close players at high frequency
	NetworkSession->subscribe(
		quark::query()
		.with_radius(HSCALE_SUBSCRIPTION_SHORT_RADIUS)
		.with_interval(std::chrono::milliseconds(HSCALE_SUBSCRIPTION_SHORT_RADIUS_INTERVAL_MS)),
		quark::qos::unreliable);

	// then distant players at lower frequency
	NetworkSession->subscribe(
		quark::query()
		.with_radius(HSCALE_SUBSCRIPTION_LONG_RADIUS)
		.with_interval(std::chrono::milliseconds(HSCALE_SUBSCRIPTION_LONG_RADIUS_INTERVAL_MS)),
		quark::qos::unreliable);
}

void UHScaleConnection::CleanUp()
{
	Super::CleanUp();
}

bool UHScaleConnection::InitHyperScaleConnection()
{
	// The URL is stored in master class so we just read data from there
	check(Driver);

	// Address in format 'address:port'
	const FString ServerAddressAndPort = FString::Printf(TEXT("%s:%s"), *URL.Host, *FString::FromInt(URL.Port));
	quark::string ServerAddress = TCHAR_TO_UTF8(*ServerAddressAndPort);

	// Creates a new session with hyperscale server
	expected<session> NewSession = quark::session::start(ServerAddress); // #todo ... add auth_buffer
	if (NewSession.has_value())
	{
		NetworkSession = MakeUnique<session>(MoveTemp(*NewSession));
		EventsDriver = MakeUnique<FHScaleEventsDriver>(this);

		SubscribeRelevancy();
		NetworkSession->set_tick_interval(std::chrono::milliseconds(HSCALE_DEFAULT_RELIABLE_SEND_TICK_INTERVAL));

		SetConnectionState(USOCK_Open);
		UE_LOG(Log_HyperScaleGlobals, Log, TEXT("Session successfully created with server %s"), *ServerAddressAndPort);

		UHScaleRepDriver* RepDriver = (UHScaleRepDriver*)Driver->GetReplicationDriver();
		check(RepDriver);
		RepDriver->OnConnectionEstablished(URL);

		// Try to receive initial data from server
		TryReceiveData();

		if (RepDriver->IsWorldInitAgent())
		{
			FreeGlobalObjectIDCache.BindOnLowItemCountDelegate([this]() { return GetEventsDriver()->SendGetFreeObjectIds(false); });	
		}
		FreePlayerObjectIDCache.BindOnLowItemCountDelegate([this]() { return GetEventsDriver()->SendGetFreeObjectIds(true); });

		return true;
	}

#if !UE_BUILD_SHIPPING

	UE_LOG(Log_HyperScaleGlobals, Error, TEXT("Session could not be started with server %s. Failed with error: %hs"),
		*ServerAddressAndPort, NewSession.error().message());

#endif

	return false;
}

quark_session_id_t UHScaleConnection::GetNetworkSessionId() const
{
	quark_session_id_t Result = 0;

	const quark::session* Session = GetNetworkSession();
	if (Session)
	{
		expected<quark_session_id_t> SessionId = Session->id();
		if (SessionId.has_value())
		{
			Result = SessionId.value<quark_session_id_t>();
		}
	}

	return Result;
}

bool UHScaleConnection::IsConnectionFullyEstablished() const
{
	return IsConnectionActive() && Driver->GetReplicationDriver() && ((UHScaleRepDriver*)Driver->GetReplicationDriver())->GetSchema();
}

quark_session_id_t UHScaleConnection::GetSessionId() const
{
	const session* Session = GetNetworkSession();
	if (Session == nullptr)
	{
		return 0;
	}

	const expected<quark_session_id_t> AssignedId = Session->id();
	if (!AssignedId.has_value())
	{
		return 0;
	}
	return *AssignedId;
}

FHScaleNetGUID UHScaleConnection::GetSessionNetGUID() const
{
	return FHScaleNetGUID::Create_Player(GetSessionId());
}

uint32 UHScaleConnection::GetNetworkCustomVersion_HS(const FGuid& VersionGuid) const
{
	const FCustomVersion* CustomVer = (HSCALE_GET_PRIVATE(UNetConnection, this, NetworkCustomVersions)).GetVersion(VersionGuid);
	return CustomVer != nullptr ? CustomVer->Version : 0;
}

int32 UHScaleConnection::GetNecessaryObjectIdCount(AActor* Actor) const
{
	if (!IsValid(Actor) || !Actor->GetIsReplicated()) return 0;

	int32 idCounter = 1;	// One for the actor itself

	// Add all the dynamic arrays
	UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Driver);
	const TSharedPtr<FRepLayout> RepLayout = NetDriver->GetObjectClassRepLayout_Copy(Actor->GetClass());

	const TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Cmds);
	for (const FRepLayoutCmd &Cmd : Cmds)
	{
		if (Cmd.Type == ERepLayoutCmdType::DynamicArray) ++idCounter;
	}

	// Add the the actors's subobjects
	const UE::Net::FSubObjectRegistry& subObjects = UE::Net::FSubObjectRegistryGetter::GetSubObjects(Actor);
	idCounter += subObjects.GetRegistryList().Num();

	// Add the components' subobjects
	const TArray<UE::Net::FReplicatedComponentInfo>& replicatedComponents = UE::Net::FSubObjectRegistryGetter::GetReplicatedComponents(Actor);
	for (const UE::Net::FReplicatedComponentInfo& component : replicatedComponents)
	{
		idCounter += component.SubObjects.GetRegistryList().Num();
	}

	return idCounter;
}

int32 UHScaleConnection::GetAvailableObjectIdCount(const UObject* Object) const
{
	const TGUID_Cache& RelevantCache = (Object && Object->IsFullNameStableForNetworking()) ? FreeGlobalObjectIDCache : FreePlayerObjectIDCache;
	return RelevantCache.Num();
}

uint64 UHScaleConnection::GetNewHScaleObjectId(UObject* Object)
{
	if (!IsConnectionActive()) { return 0; }

	session* Session = GetNetworkSession();

	uint64 ObjectId;
	TGUID_Cache& RelevantCache = (Object && Object->IsFullNameStableForNetworking()) ? FreeGlobalObjectIDCache : FreePlayerObjectIDCache;
	if (!RelevantCache.Dequeue(ObjectId))
	{
		UE_LOG(Log_HyperScaleReplication, Warning, TEXT("UHScaleConnection::GetNewHScaleObjectId - Empty ID Cache: %s"), UTF8_TO_TCHAR((&RelevantCache == &FreeGlobalObjectIDCache) ? "FreeGlobalObjectIDCache" : "FreePlayerObjectIDCache"));
	}
	return ObjectId;
}

UHScaleRelevancyManager* UHScaleConnection::GetRelevancyManager() const
{
	const UHScaleRepDriver* RetDriver = (UHScaleRepDriver*)Driver->GetReplicationDriver();
	check(RetDriver);

	return RetDriver->GetRelevancyManager();
}

void UHScaleConnection::AddUnmappedObjectPtr(const FHScaleNetGUID& ObjPtrId, const TTuple<FHScaleNetGUID, uint16> Pair)
{
	TSet<TTuple<FHScaleNetGUID, uint16>> Pairs;
	if (UnMappedObjPtrs.Contains(ObjPtrId)) { Pairs = UnMappedObjPtrs[ObjPtrId]; }
	Pairs.Add(Pair);

	UnMappedObjPtrs.Add(ObjPtrId, Pairs);
}

void UHScaleConnection::RemoveUnmappedObjectPtrs(const FHScaleNetGUID& ObjPtrGUID)
{
	UnMappedObjPtrs.Remove(ObjPtrGUID);
}

FHScaleNetGUID UHScaleConnection::FetchNextAvailableDynamicEntityId()
{
	uint64_t NewId;
	FreePlayerObjectIDCache.Dequeue(NewId);
	check(NewId > 0ull);	// #todo: maybe an UE_LOG instead?
	return FHScaleNetGUID::Create_Object(NewId);
}

#undef IP_HEADER_SIZE
#undef UDP_HEADER_SIZE
#undef WINSOCK_MAX_PACKET