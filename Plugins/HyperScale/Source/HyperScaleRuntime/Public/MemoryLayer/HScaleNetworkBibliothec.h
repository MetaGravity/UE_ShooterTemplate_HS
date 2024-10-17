// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleNetworkEntity.h"
#include "remote.h"

// #todo the log category value should be a macro, defined from plugin .cs file
#if UE_BUILD_SHIPPING
DECLARE_LOG_CATEGORY_EXTERN(Log_HyperScaleMemory, Error, All);
#else
DECLARE_LOG_CATEGORY_EXTERN(Log_HyperScaleMemory, Log, All);
#endif

struct FHScaleLocalUpdate;
class UHScaleNetDriver;

class FHScaleNetworkEntity;
/**
 * This class is used to store all data received from HyperScale server
 */
class HYPERSCALERUNTIME_API FHScaleNetworkBibliothec
{
public:
	FHScaleNetworkBibliothec()
		: NetDriver(nullptr) {}

	~FHScaleNetworkBibliothec() {}

	void Push(FHScaleInBunch& Bunch);

	void Pull(TArray<FHScaleLocalUpdate>& LocalUpdates);

	/**
	 * @param Bunch
	 * @param ObjectId 
	 */
	bool PullUpdate(FInBunch& Bunch, const FHScaleNetGUID ObjectId)
	{
		return Pull(Bunch, ObjectId, true);
	}

	bool PullEntire(FInBunch& Bunch, const FHScaleNetGUID ObjectId)
	{
		return Pull(Bunch, ObjectId, false);
	}

	TSharedPtr<FHScaleNetworkEntity> FetchEntity(const FHScaleNetGUID ObjectId);
	TSharedPtr<FHScaleNetworkEntity> FindExistingEntity(const FHScaleNetGUID EntityId);

	void CreateLocalPlayerEntity(const uint32 PlayerId);

	TSet<FHScaleNetGUID>::TConstIterator GetServerDirtyEntitiesIterator() const;

	TSet<FHScaleNetGUID>::TConstIterator GetLocalDirtyEntitiesIterator() const;

	uint64 NumLocalDirtyEntities() const;

	uint64 NumServerDirtyEntities() const;

	bool IsEntityExists(const FHScaleNetGUID ObjectId) const;

	void ClearServerDirtyEntities();
	/**
	 * Removes the entities passed in filteredlist from ServerDirtyList
	 * @param FilteredList List of entityIds to remove from ServerDirtyList
	 */
	void ClearServerDirtyEntities(const TSet<FHScaleNetGUID>& FilteredList);

	UHScaleNetDriver* GetNetDriver() const;

	void SetNetDriver(UHScaleNetDriver* Driver);
	void AddLocalDirty(FHScaleNetworkEntity* Entity);
	void AddServerDirty(FHScaleNetworkEntity* Entity);

	void AddNetworkEntity(const TSharedPtr<FHScaleNetworkEntity> Entity);

	TMap<FHScaleNetGUID, TSharedPtr<FHScaleNetworkEntity>>::TConstIterator GetConstIterator() const
	{
		return NetworkEntities.CreateConstIterator();
	}

	void AddFlagsEntity(const FHScaleNetGUID& EntityId, uint16 Flags);

	TSet<FHScaleNetGUID>::TConstIterator FetchIteratorPerFlag(uint16 Flag) const;

	void DestroyEntity(const FHScaleNetGUID& EntityId);

	void HandlePlayersNetworkUpdate(const quark::remote_player_update& Update);
	void HandleObjectsNetworkUpdate(const quark::remote_object_update& Update);
	
protected:
	void Push(FHScaleInBunch& Bunch, const FHScaleNetGUID ObjectId);
	/**
	 * Stored all network entities received by the client and local connection stored in LocalPlayerEntity
	 */
	TMap<FHScaleNetGUID, TSharedPtr<FHScaleNetworkEntity>> NetworkEntities;

	// Contains Reference to network entity representation of local player
	TSharedPtr<FHScaleNetworkEntity> LocalPlayerEntity;

	// List of entities changed locally, yet to push to server
	TSet<FHScaleNetGUID> LocalDirtyEntities;

	// List of entities received update from server, yet to push to replication layer
	TSet<FHScaleNetGUID> ServerDirtyEntities;

	uint64 PlayerClassId = 0;

	// #todo make this method available to only to replication layer through friend keyword
	void SetPlayerClassId(const uint64 ClassId);

	void PullAndClearLocalPlayerChanges(TArray<FHScaleLocalUpdate>& LocalUpdates);
	void PullAndClearLocalEntityChanges(TArray<FHScaleLocalUpdate>& LocalUpdates);

	bool Pull(FInBunch& Bunch, const FHScaleNetGUID ObjectId, const bool bDelta = true);

	UHScaleNetDriver* NetDriver;

	TSet<FHScaleNetGUID> EntityPerFlags[16]; // hardcoding to 16, as it is the max number of flags possible
};