// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/HScaleResources.h"
#include "Engine/ReplicationDriver.h"
#include "HScaleRepDriver.generated.h"

class UHScalePackageMap;
class UHScaleRelevancyManager;
class UHScaleSchema;
class UHScaleSchemaRequest;
class UHScaleConnection;
class UHScaleNetDriver;
/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleRepDriver : public UReplicationDriver
{
	GENERATED_BODY()

public:
	virtual void PostInitDriver();

	// ~Begin of UReplicationDriver interface
public:
	virtual void SetRepDriverWorld(UWorld* InWorld) override;
	virtual void InitForNetDriver(UNetDriver* InNetDriver) override;
	virtual void InitializeActorsInWorld(UWorld* InWorld) override;

	virtual void ResetGameWorldState() override {}

	virtual void AddClientConnection(UNetConnection* NetConnection) override {}

	virtual void RemoveClientConnection(UNetConnection* NetConnection) override {}

	virtual void AddNetworkActor(AActor* Actor) override;

	virtual void RemoveNetworkActor(AActor* Actor) override;

	virtual void ForceNetUpdate(AActor* Actor) override {}

	virtual void FlushNetDormancy(AActor* Actor, bool WasDormInitial) override {}

	virtual void NotifyActorTearOff(AActor* Actor) override {}

	virtual void NotifyActorFullyDormantForConnection(AActor* Actor, UNetConnection* Connection) override {}

	virtual void NotifyActorDormancyChange(AActor* Actor, ENetDormancy OldDormancyState) override {}

	/** Called when a destruction info is created for an actor. Can be used to override some of the destruction info struct */
	virtual void NotifyDestructionInfoCreated(AActor* Actor, FActorDestructionInfo& DestructionInfo) override {}

	virtual void SetRoleSwapOnReplicate(AActor* Actor, bool bSwapRoles) override {}

	/** The main function that will actually replicate actors. Called every server tick. */
	virtual int32 ServerReplicateActors(float DeltaSeconds) override;
	// ~End of UReplicationDriver interface

	// ~Begin of UHScaleRepDriver interface
public:
	/**
	 * Moves entity into staging state, that means the related actor
	 * for entity is detached (removed from game world) while the entity still lives
	 * in a memory cache
	 * 
	 * @param EntityGUID - Entity that will be staged
	 * @return - True, if actor was successfully detached from entity or is already detached
	 */
	virtual bool SetEntityInStagingMode(const FHScaleNetGUID EntityGUID);

	virtual bool SpawnStagedEntity(const FHScaleNetGUID EntityGUID);

	/**
	 * Called from UHScaleRelevancyManager when an entity doesn't meet server
	 * relevancy conditions, and it is out of relevancy range for the client simulation
	 *
	 * This function flush the entity data from memory layer, and IF the related actor/object
	 * exists in a game world, it will be removed
	 */
	virtual bool FlushEntity(const FHScaleNetGUID EntityGUID, const bool bSendForgetEvent = true);

	UHScaleRelevancyManager* GetRelevancyManager() const { return RelevancyManager; }
	// ~End of UHScaleRepDriver interface

public:
	void OnConnectionEstablished(const FURL& URL);
	void ActorRemotelyDestroyed(const FHScaleNetGUID& EntityId);

	bool IsWorldInitAgent() const { return bIsWorldInitAgent.Get(false); }

protected:
	virtual void OnSchemeDownloaded(const bool bSuccessful, UHScaleSchema* Content);

	bool IsAllowedToReplicate(const AActor* Actor) const;

public:
	UHScaleSchema* GetSchema() const { return Schema; }

private:
	UPROPERTY()
	TObjectPtr<UHScaleNetDriver> CachedNetDriver;

	UPROPERTY()
	TObjectPtr<UHScaleConnection> CachedConnection;

	UPROPERTY()
	TObjectPtr<UHScalePackageMap> CachedPackageMap;

	UPROPERTY()
	UHScaleSchemaRequest* SchemaRequest;

	UPROPERTY()
	TObjectPtr<UHScaleSchema> Schema;

	UPROPERTY()
	TObjectPtr<UHScaleRelevancyManager> RelevancyManager;

	/**
	 * Cached data from connection object
	 * True, if the map was started with role WorldInitAgent
	 */
	TOptional<bool> bIsWorldInitAgent;
};