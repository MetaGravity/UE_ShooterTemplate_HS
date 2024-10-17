// Copyright 2024 Metagravity. All Rights Reserved.


#include "RelevancyManager/HScaleRelevancyManager.h"

#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "NetworkLayer/HScaleConnection.h"
#include "ReplicationLayer/HScaleRepDriver.h"

UHScaleRelevancyManager::UHScaleRelevancyManager() {}

UHScaleRelevancyManager* UHScaleRelevancyManager::Create(UHScaleConnection* NetConnection)
{
	UHScaleRelevancyManager* Result = nullptr;

	if (IsValid(NetConnection) && ensure(NetConnection->IsConnectionActive()))
	{
		Result = NewObject<UHScaleRelevancyManager>(NetConnection);
		Result->Initialize(NetConnection);
	}

	return Result;
}

void UHScaleRelevancyManager::Initialize(UHScaleConnection* NetConnection)
{
	CachedNetConnection = NetConnection;
	check(IsValid(CachedNetConnection));

	CachedNetDriver = Cast<UHScaleNetDriver>(CachedNetConnection->Driver);
	check(IsValid(CachedNetDriver));

	CachedPackageMap = Cast<UHScalePackageMap>(CachedNetConnection->PackageMap);
	check(IsValid(CachedPackageMap));
}

void UHScaleRelevancyManager::Tick(float DeltaTime)
{
	// We dont need to call the relevancy checks all the time each tick now
	// #todo ... later we can experiment with different thread and make it each tick

	if (!IsValid(CachedNetConnection) || !CachedNetConnection->IsConnectionActive()) return;

	CurrentRelevancyDeltaTime += DeltaTime;
	CurrentDormancyDeltaTime += DeltaTime;

	// Cache view target location
	bool bLocationIsChanged = false;

	FVector ViewLocation;
	FRotator ViewRotation;
	CachedNetDriver->GetPlayerViewPoint(ViewLocation, ViewRotation);

	if (LastViewTargetLocation.IsSet())
	{
		constexpr float MinDiffDistanceSquared = 100.f; // This is experimental value and it can be changed
		const float LocDistance = FVector::DistSquared(ViewLocation, LastViewTargetLocation.GetValue());
		if (LocDistance > MinDiffDistanceSquared)
		{
			LastViewTargetLocation = ViewLocation;
			bLocationIsChanged = true;
		}
	}
	else
	{
		LastViewTargetLocation = ViewLocation;
		bLocationIsChanged = true;
	}

	if (bLocationIsChanged || CurrentRelevancyDeltaTime >= RelevancyCheckPeriod)
	{
		CurrentRelevancyDeltaTime = 0;
		CheckRelevancy(bLocationIsChanged);
	}

	if (CurrentDormancyDeltaTime >= DormancyCheckPeriod)
	{
		CurrentDormancyDeltaTime = 0;
		CheckDormancy();
	}
}

void UHScaleRelevancyManager::CheckRelevancy(const bool bLocationIsChanged)
{
	check(CachedNetDriver);
	check(CachedPackageMap);

	// --- CHECK IF IRRELEVANT ACTORS ARE REMOVED ---
	FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec)
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Bibliothec does not exists"));
		return;
	}

	UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(CachedNetDriver->GetReplicationDriver());
	if (!RepDriver)
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Bibliothec does not exists"));
		return;
	}

	// --- GATHER NEWLY IRRELEVANT ACTORS ---
	EntitiesInDestructionMode.Reserve(RelevantEntities.Num());
	for (TSet<FHScaleNetGUID>::TIterator It = RelevantEntities.CreateIterator(); It; ++It)
	{
		const FHScaleNetGUID NetGUID = *It;

		if (!IsActorNetRelevantToPlayer(NetGUID))
		{
			EntitiesInDestructionMode.Add(NetGUID);
			const bool bSuccessfullyStaged = RepDriver->SetEntityInStagingMode(NetGUID);
			if (bSuccessfullyStaged)
			{
				It.RemoveCurrent();
			}
		}
	}
	EntitiesInDestructionMode.Empty();

	if (bLocationIsChanged)
	{
		// Iterate over all network actor entities, because if the owning view target was changed
		// it can affect any entity around a player

		// --- GATHER NEW RELEVANT NETGUIDS IN SEPARATE SET ---

		TSet<FHScaleNetGUID>::TConstIterator ActorEntitiesIt = Bibliothec->FetchIteratorPerFlag(EHScaleEntityFlags::IsActor);
		for (; ActorEntitiesIt; ++ActorEntitiesIt)
		{
			const FHScaleNetGUID NetGUID = *ActorEntitiesIt;
			// UE_LOG(Log_HyperScaleReplication, Log, TEXT("Checking entity with id: %s"), *NetGUID.ToString());
			CheckRelevancy_Internal(NetGUID, *RepDriver, *Bibliothec);
		}
	}
	else
	{
		// Iterate only over network actor entities, that were changed
		// it can affect any entity around a player

		TSet<FHScaleNetGUID>::TConstIterator ChangedEntitiesIt = Bibliothec->GetServerDirtyEntitiesIterator();
		for (; ChangedEntitiesIt; ++ChangedEntitiesIt)
		{
			const FHScaleNetGUID NetGUID = *ChangedEntitiesIt;
			// UE_LOG(Log_HyperScaleReplication, Log, TEXT("Checking entity 2 with id: %s"), *NetGUID.ToString());
			CheckRelevancy_Internal(NetGUID, *RepDriver, *Bibliothec);
		}
	}

	UE_LOG(Log_HyperScaleReplication, VeryVerbose, TEXT("Number of relevant entities: %d"), RelevantEntities.Num());
}

void UHScaleRelevancyManager::CheckDormancy()
{
	check(CachedNetDriver);
	check(CachedPackageMap);
	check(CachedNetConnection);

	FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec)
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Bibliothec does not exists"));
		return;
	}

	UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(CachedNetDriver->GetReplicationDriver());
	if (!RepDriver)
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Bibliothec does not exists"));
		return;
	}

	TSet<FHScaleNetGUID>::TConstIterator ActorEntitiesIt = Bibliothec->FetchIteratorPerFlag(EHScaleEntityFlags::IsActor);

	// --- GATHER NEW RELEVANT NETGUIDS IN SEPARATE SET ---
	for (; ActorEntitiesIt; ++ActorEntitiesIt)
	{
		const FHScaleNetGUID& NetGUID = *ActorEntitiesIt;

		// Add newly relevant entities
		if (RelevantEntities.Contains(NetGUID))
		{
			EntitiesInDormantState.Remove(NetGUID);
		}
		else
		{
			const TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(NetGUID);

			FVector RelevantEntityPos;
			if (Entity->GetEntityLocation(RelevantEntityPos))
			{
				const float Distance = FVector::Dist(LastViewTargetLocation.GetValue(), RelevantEntityPos);
				if (Distance > MAX_SERVER_RELEVANCY_DISTANCE)
				{
					if (EntitiesInDormantState.Contains(NetGUID))
					{
						const bool bFlushResult = RepDriver->FlushEntity(NetGUID);
						if (bFlushResult)
						{
							EntitiesInDormantState.Remove(NetGUID);
						}
					}
					else
					{
						// Save the entity as a dormant and in the next dormant check we will remove it
						EntitiesInDormantState.Add(NetGUID);
					}
				}
				else
				{
					EntitiesInDormantState.Remove(NetGUID);
				}
			}
		}
	}
}

void UHScaleRelevancyManager::ClearEntity(const FHScaleNetGUID NetGUID)
{
	if (NetGUID.IsValid())
	{
		RelevantEntities.Remove(NetGUID);
		EntitiesInDormantState.Remove(NetGUID);
	}
}

void UHScaleRelevancyManager::CheckRelevancy_Internal(const FHScaleNetGUID& NetGUID, UHScaleRepDriver& RepDriver, FHScaleNetworkBibliothec& Bibliothec)
{
	// Add newly relevant entities
	if (!RelevantEntities.Contains(NetGUID)/** && IsActorNetRelevantToPlayer(NetGUID)*/)
	{
		if (IsActorNetRelevantToPlayer(NetGUID))
		{
			const bool bSuccessfullySpawned = RepDriver.SpawnStagedEntity(NetGUID);
			if (bSuccessfullySpawned)
			{
				RelevantEntities.Add(NetGUID);
			}
		}
	}

	// After we set relevancy, now check if the entity can be in dormant state
	if (RelevantEntities.Contains(NetGUID) || IsEntityServerRelevantToPlayer(Bibliothec.FetchEntity(NetGUID)))
	{
		// If the entity is relevant or in server relevant distance, then cannot be in dormant state
		EntitiesInDormantState.Remove(NetGUID);
	}
}

bool UHScaleRelevancyManager::IsActorNetRelevantToPlayer(const FHScaleNetGUID NetGUID) const
{
	if (!NetGUID.IsValid()) return false;
	if (!IsValid(CachedNetDriver)) return false;
	if (!LastViewTargetLocation.IsSet()) return false;

	// --- GET NETWORK ENTITY ---
	FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec)
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Bibliothec does not exists"));
		return false;
	}

	if (!Bibliothec->IsEntityExists(NetGUID))
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Entity does not exist in memory layer"));
		return false;
	}

	const TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(NetGUID);

	if (!Entity->IsReadyForReplication())
	{
		UE_LOG(Log_HyperScaleReplication, VeryVerbose, TEXT("Entity %llu is not ready for replication"), NetGUID.Get())
		return false;
	}

	const UClass* EntityClass = Entity->FetchEntityUClass();
	if (!IsValid(EntityClass) || !EntityClass->IsChildOf(AActor::StaticClass()))
	{
		// Do not accept anything else than just actors classes
		return false;
	}

	for (const FHScaleNetGUID& ChildId : Entity->ChildrenIds)
	{
		// The entity is relevant until its all children are relevant
		if (IsActorNetRelevantToPlayer(ChildId))
		{
			return true;
		}
	}

	const AActor* DefaultActor = EntityClass->GetDefaultObject<AActor>();

	// --- CHECK FOR DISTANCE ---
	FVector RelevantEntityPos;
	if (!Entity->GetEntityLocation(RelevantEntityPos))
	{
		UE_LOG(Log_HyperScaleReplication, Error, TEXT("Entity does not have a location"));
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(LastViewTargetLocation.GetValue(), RelevantEntityPos);
	const float Distance = FMath::Sqrt(DistanceSquared);

	// Relevant only in the case if the distance doest exceed max server distance value
	if (Distance > MAX_SERVER_RELEVANCY_DISTANCE)
	{
		return false;
	}

	// --- CHECK IF ALWAYS RELEVANT ---
	if (DefaultActor->bAlwaysRelevant)
	{
		return true;
	}

	return DistanceSquared < DefaultActor->NetCullDistanceSquared;
}

bool UHScaleRelevancyManager::IsEntityServerRelevantToPlayer(const TSharedPtr<FHScaleNetworkEntity> NetworkEntity) const
{
	if (!NetworkEntity) return false;

	const FHScaleNetworkEntity* EntityToCheck = NetworkEntity.Get();
	if (!NetworkEntity->IsActor())
	{
		EntityToCheck = NetworkEntity->GetChannelOwner(); // Retrieve owning actor if the input param was component or any other subobject
	}
	if (!EntityToCheck) return false;

	bool bResult = false;
	FVector RelevantEntityPos;
	if (EntityToCheck->GetEntityLocation(RelevantEntityPos))
	{
		const float Distance = FVector::Dist(LastViewTargetLocation.GetValue(), RelevantEntityPos);

		// Relevant only in the case if the distance doest exceed max server distance value
		bResult = Distance <= MAX_SERVER_RELEVANCY_DISTANCE;
	}

	return bResult;
}