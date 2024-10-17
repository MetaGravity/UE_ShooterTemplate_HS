// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/HScaleResources.h"

#include "HScaleRelevancyManager.generated.h"

// #todo ... this should be cached from schema
#define MAX_SERVER_RELEVANCY_DISTANCE 2000

class UHScaleRepDriver;
class FHScaleNetworkBibliothec;
class FHScaleNetworkEntity;
class UHScalePackageMap;
class UHScaleNetDriver;
class UHScaleConnection;

UENUM()
enum class EHScale_EntityState
{
	Relevant,
	Staging,
	Dormancy,
	Irrelevant
};

UCLASS()
class UHScaleRelevancyManager : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UHScaleRelevancyManager();

	static UHScaleRelevancyManager* Create(UHScaleConnection* NetConnection);

	float RelevancyCheckPeriod{0.05f};
	float DormancyCheckPeriod{50.f};

protected:
	virtual void Initialize(UHScaleConnection* NetConnection);

	// ~Begin of FTickableObjectBase interface
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UHScaleRelevancyManager, STATGROUP_Tickables); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	// ~End of FTickableObjectBase interface

	void CheckRelevancy(const bool bLocationIsChanged);
	void CheckDormancy();

	void ClearEntity(const FHScaleNetGUID NetGUID);

private:
	void CheckRelevancy_Internal(const FHScaleNetGUID& NetGUID, UHScaleRepDriver& RepDriver, FHScaleNetworkBibliothec& Bibliothec);

public:
	bool IsActorNetRelevantToPlayer(const FHScaleNetGUID NetGUID) const;
	bool IsEntityServerRelevantToPlayer(const TSharedPtr<FHScaleNetworkEntity> NetworkEntity) const;
	bool IsEntityDestroyedByRelevancy(const FHScaleNetGUID NetGUID) const { return EntitiesInDestructionMode.Contains(NetGUID); }

protected:
	TSet<FHScaleNetGUID> EntitiesInDestructionMode;
	TSet<FHScaleNetGUID> RelevantEntities;
	TSet<FHScaleNetGUID> EntitiesInDormantState;

private:
	float CurrentRelevancyDeltaTime{0.f};
	float CurrentDormancyDeltaTime{0.f};

	/** Cached net driver from Initialize() function */
	UPROPERTY()
	TObjectPtr<UHScaleNetDriver> CachedNetDriver;

	/** Cached net connection from Initialize() function */
	UPROPERTY()
	TObjectPtr<UHScaleConnection> CachedNetConnection;

	/** Cached package map from Initialize() function */
	UPROPERTY()
	TObjectPtr<UHScalePackageMap> CachedPackageMap;

	/**
	 * This is the last view target position when the update was made from
	 * 
	 * If the position is not changed so much, we can just iterate over network actors
	 * that are receiving delta updates from network and IDLE actors do not need to be checked
	 * 
	 * If the position is changed, we have to iterate over all actor entities because any of them
	 * can change relevancy dependent on our movement from place to place
	 */
	TOptional<FVector> LastViewTargetLocation;
};