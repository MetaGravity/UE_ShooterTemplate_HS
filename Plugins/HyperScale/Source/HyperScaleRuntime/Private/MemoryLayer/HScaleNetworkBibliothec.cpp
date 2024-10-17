// Copyright 2024 Metagravity. All Rights Reserved.


#include "MemoryLayer/HScaleNetworkBibliothec.h"

#include "Core/HScaleResources.h"
#include "MemoryLayer/HScaleNetworkEntity.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleUpdates.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "ReplicationLayer/HScaleRepDriver.h"
#include "Utils/HScaleStatics.h"

#include "Engine/PackageMapClient.h"
#include "RelevancyManager/HScaleRelevancyManager.h"

DEFINE_LOG_CATEGORY(Log_HyperScaleMemory)

void FHScaleNetworkBibliothec::Push(FHScaleInBunch& Bunch)
{
	bool bValid = !!Bunch.ReadBit();
	check(bValid) // should be a valid HScaleNetGUID
	FHScaleNetGUID GUID;
	Bunch << GUID;
	Push(Bunch, GUID);
}

void FHScaleNetworkBibliothec::AddLocalDirty(FHScaleNetworkEntity* Entity)
{
	if (!Entity) return;
	LocalDirtyEntities.Add(Entity->EntityId);
}


void FHScaleNetworkBibliothec::AddServerDirty(FHScaleNetworkEntity* Entity)
{
	if (!Entity) return;
	ServerDirtyEntities.Add(Entity->EntityId);
}

void FHScaleNetworkBibliothec::AddNetworkEntity(const TSharedPtr<FHScaleNetworkEntity> Entity)
{
	if (!Entity.IsValid()) return;

	NetworkEntities.Add(Entity->EntityId, Entity);
	Entity->SetNetDriver(GetNetDriver());
}

void FHScaleNetworkBibliothec::AddFlagsEntity(const FHScaleNetGUID& EntityId, uint16 Flags)
{
	uint16 Index = 0;
	while (Flags > 0)
	{
		if (Flags & 1)
		{
			EntityPerFlags[Index].Add(EntityId);
		}
		Flags >>= 1;
		Index++;
	}
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleNetworkBibliothec::FetchIteratorPerFlag(uint16 Flag) const
{
	uint16 Index = 0;
	while (Flag > 0)
	{
		if (Flag & 1) break;
		Flag >>= 1;
		Index++;
	}

	return EntityPerFlags[Index].CreateConstIterator();
}

void FHScaleNetworkBibliothec::DestroyEntity(const FHScaleNetGUID& EntityId)
{
	if (!IsEntityExists(EntityId)) return;
	const TSharedPtr<FHScaleNetworkEntity>* CachedEntity = NetworkEntities.Find(EntityId);

	if (!CachedEntity || !CachedEntity->IsValid()) return;

	FHScaleNetworkEntity* Entity = CachedEntity->Get();
	Entity->Destroy();

	NetworkEntities.Remove(EntityId);
	LocalDirtyEntities.Remove(EntityId);
	ServerDirtyEntities.Remove(EntityId);

	UHScaleConnection* Connection = GetNetDriver()->GetHyperScaleConnection();
	check(Connection)
	
	UHScaleRelevancyManager* RelevancyManager = Connection->GetRelevancyManager();
	if (RelevancyManager)
	{
		RelevancyManager->ClearEntity(EntityId);
	}

	FHScaleEventsDriver* EventsDriver = Connection->GetEventsDriver();
	if (EventsDriver)
	{
		EventsDriver->OnEntityDestroyed(EntityId);
	}

	UHScalePackageMap* PkgMp = Cast<UHScalePackageMap>(Connection->PackageMap);

	PkgMp->RemoveGUIDsFromMap(EntityId);
	// Remove EntityId in EntityPerFlags
	uint16 Flags = Entity->Flags;
	uint16 Index = 0;
	while (Flags > 0)
	{
		if (Flags & 1)
		{
			EntityPerFlags[Index].Remove(EntityId);
		}
		Flags >>= 1;
		Index++;
	}
}

void FHScaleNetworkBibliothec::Push(FHScaleInBunch& Bunch, const FHScaleNetGUID ObjectId)
{
	const TSharedPtr<FHScaleNetworkEntity> Entity = FetchEntity(ObjectId);

	if (!ensure(Entity.IsValid())) { return; }

	Entity->ChannelIndex = Bunch.Channel->ChIndex;
	// update entity with new changes
	Entity->Push(Bunch);
	// mark the entity as dirty
	Entity->MarkEntityLocalDirty();
}

void FHScaleNetworkBibliothec::PullAndClearLocalPlayerChanges(TArray<FHScaleLocalUpdate>& LocalUpdates)
{
	if (!LocalPlayerEntity.IsValid())
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("Local reference to Player Entity is invalid"))
		return;
	}

	// if (LocalPlayerEntity->NumLocalDirtyProps() == 0) { return; }

	// #todo: Instead of sending it always, optimize it by checking last position. This would go away, once centroid delegates are implemented
	// Send position
	const UHScaleNetDriver* Driver = GetNetDriver();
	check(Driver)
	FVector Location;
	FRotator Rotation;
	Driver->GetPlayerViewPoint(Location, Rotation);
	FHScaleLocalUpdate PlayerUpdate;
	PlayerUpdate.bIsPlayer = true;
	PlayerUpdate.Attributes.Add({QUARK_KNOWN_ATTRIBUTE_POSITION, quark::vec3(Location.X, Location.Y, Location.Z)});
	LocalPlayerEntity->Pull(PlayerUpdate.Attributes);

	LocalUpdates.Add(PlayerUpdate);
	LocalPlayerEntity->ClearLocalDirtyProps();
}

void FHScaleNetworkBibliothec::PullAndClearLocalEntityChanges(TArray<FHScaleLocalUpdate>& LocalUpdates)
{
	const int DirtyEntitiesCount = LocalDirtyEntities.Num();
	if (DirtyEntitiesCount == 0)
	{
		return;
	}

	TSet<TSharedPtr<FHScaleNetworkEntity>> DirtySet;
	// Filter out stale objects, unreferenced objects
	for (const auto ObjectId : LocalDirtyEntities)
	{
		const TSharedPtr<FHScaleNetworkEntity>* CachedEntity = NetworkEntities.Find(ObjectId);
		if (CachedEntity == nullptr or !CachedEntity->IsValid())
		{
			UE_LOG(Log_HyperScaleMemory, Warning, TEXT("ObjectId %llu is marked local dirty, but object is not present"), ObjectId.Get());
			continue;
		}
		TSharedPtr<FHScaleNetworkEntity> Entity = *CachedEntity;
		UE_CLOG(!Entity->IsReadyForReplication(), Log_HyperScaleMemory, Verbose, TEXT("Entity %llu is not ready for replication"), ObjectId.Get())
		if (Entity->NumLocalDirtyProps() == 0 || !Entity->IsReadyForReplication()) { continue; }
		DirtySet.Add(Entity);
	}

	const int ActualDirtyCount = DirtySet.Num();
	if (ActualDirtyCount == 0)
	{
		return;
	}

	for (const auto Entity : DirtySet)
	{
		const FHScaleNetGUID ObjectId = Entity->EntityId;
		FHScaleLocalUpdate Update;
		Update.bIsPlayer = false;
		Update.ObjectId = ObjectId.Get();
		Entity->Pull(Update.Attributes);
		LocalUpdates.Push(Update);
		// Once attributes are pulled clear properties marked as dirty
		Entity->ClearLocalDirtyProps();
		LocalDirtyEntities.Remove(ObjectId);
	}
}

bool FHScaleNetworkBibliothec::Pull(FInBunch& Bunch, const FHScaleNetGUID ObjectId, const bool bDelta)
{
	const TSharedPtr<FHScaleNetworkEntity>* CachedEntity = NetworkEntities.Find(ObjectId);
	if (CachedEntity == nullptr or !CachedEntity->IsValid())
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("ObjectId %llu is marked local dirty, but object is not present"), ObjectId.Get());
		return false;
	}
	const TSharedPtr<FHScaleNetworkEntity> Entity = *CachedEntity;

	if (!Entity->IsReadyForReplication())
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("ObjectId %llu is not yet ready for replication"), ObjectId.Get());
		return false;
	}
	bool bResult;
	if (bDelta)
	{
		bResult = Entity->PullUpdate(Bunch);
	}
	else
	{
		bResult = Entity->PullEntire(Bunch);
	}

	Entity->ClearServerDirtyProps();
	return bResult;
}

void FHScaleNetworkBibliothec::Pull(TArray<FHScaleLocalUpdate>& LocalUpdates)
{
	PullAndClearLocalPlayerChanges(LocalUpdates);
	PullAndClearLocalEntityChanges(LocalUpdates);
}

TSharedPtr<FHScaleNetworkEntity> FHScaleNetworkBibliothec::FetchEntity(const FHScaleNetGUID ObjectId)
{
	if (!ObjectId.IsValid()) return nullptr;

	TSharedPtr<FHScaleNetworkEntity> Result;

	// check if the entity is present in network cache
	const TSharedPtr<FHScaleNetworkEntity>* CachedEntity = NetworkEntities.Find(ObjectId);
	if (CachedEntity == nullptr or !CachedEntity->IsValid())
	{
		// create a default entity and put in cache
		Result = FHScaleNetworkEntity::Create(ObjectId);
		NetworkEntities.Add(ObjectId, Result);
		Result->SetNetDriver(GetNetDriver());
	}
	else
	{
		Result = *CachedEntity;
	}

	return Result;
}

TSharedPtr<FHScaleNetworkEntity> FHScaleNetworkBibliothec::FindExistingEntity(const FHScaleNetGUID EntityId)
{
	if (!EntityId.IsValid() || !NetworkEntities.Contains(EntityId)) return nullptr;

	// check if the entity is present in network cache
	const TSharedPtr<FHScaleNetworkEntity>* CachedEntity = NetworkEntities.Find(EntityId);
	return CachedEntity ? *CachedEntity : nullptr;
}

void FHScaleNetworkBibliothec::CreateLocalPlayerEntity(const uint32 PlayerId)
{
	const FHScaleNetGUID LocalNetId = FHScaleNetGUID::Create_Player(PlayerId);

	LocalPlayerEntity = FHScaleNetworkEntity::Create(LocalNetId);
	NetworkEntities.Add(LocalNetId, LocalPlayerEntity);
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleNetworkBibliothec::GetServerDirtyEntitiesIterator() const
{
	return ServerDirtyEntities.CreateConstIterator();
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleNetworkBibliothec::GetLocalDirtyEntitiesIterator() const
{
	return LocalDirtyEntities.CreateConstIterator();
}

uint64 FHScaleNetworkBibliothec::NumLocalDirtyEntities() const
{
	return LocalDirtyEntities.Num();
}

uint64 FHScaleNetworkBibliothec::NumServerDirtyEntities() const
{
	return ServerDirtyEntities.Num();
}

bool FHScaleNetworkBibliothec::IsEntityExists(const FHScaleNetGUID ObjectId) const
{
	return NetworkEntities.Contains(ObjectId);
}

void FHScaleNetworkBibliothec::ClearServerDirtyEntities()
{
	ServerDirtyEntities.Empty();
}

void FHScaleNetworkBibliothec::ClearServerDirtyEntities(const TSet<FHScaleNetGUID>& FilteredList)
{
	for (const auto& EntityId : FilteredList)
	{
		ServerDirtyEntities.Remove(EntityId);
	}
}

UHScaleNetDriver* FHScaleNetworkBibliothec::GetNetDriver() const
{
	return NetDriver;
}

void FHScaleNetworkBibliothec::SetNetDriver(UHScaleNetDriver* Driver)
{
	this->NetDriver = Driver;
}

void FHScaleNetworkBibliothec::SetPlayerClassId(const uint64 ClassId)
{
	PlayerClassId = ClassId;
}

void FHScaleNetworkBibliothec::HandlePlayersNetworkUpdate(const quark::remote_player_update& Update)
{
	const PlayerId CurrentPlayerId = Update.player_id();
	if (CurrentPlayerId == LocalPlayerEntity->EntityId.Get())
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("On receive, no handling of local player, skipping"))
		return;
	}
	const FHScaleNetGUID TempPlayerId = FHScaleNetGUID::Create_Player(CurrentPlayerId);
	const TSharedPtr<FHScaleNetworkEntity> Entity = FetchEntity(TempPlayerId);
	Entity->Push(Update.attribute_id(), Update.value(), Update.timestamp());
	Entity->MarkEntityServerDirty();
}

void FHScaleNetworkBibliothec::HandleObjectsNetworkUpdate(const quark::remote_object_update& Update)
{
	const ObjectId ObjectId = Update.object_id();

	const uint32 SessionId = GetNetDriver()->GetHyperScaleConnection()->GetSessionId();

	const FHScaleNetGUID TempObjectId = FHScaleNetGUID::Create_Object(ObjectId);

	const TSharedPtr<FHScaleNetworkEntity> Entity = FetchEntity(TempObjectId);

	// // #todo: replace with proper roles later
	// if (FHScaleStatics::IsPlayerOwnedObject(ObjectId, SessionId))
	// {
	// 	Entity->PushPlayerOwnedUpdate(Update.attribute_id(), Update.value(), Update.timestamp());
	// 	return;
	// }

	Entity->Push(Update.attribute_id(), Update.value(), Update.timestamp());

	Entity->MarkEntityServerDirty();
}