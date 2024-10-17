// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetDriver.h"
#include "MemoryLayer/HScaleNetworkBibliothec.h"
#include "HScaleNetDriver.generated.h"

class UHScaleConnection;
class UHScaleReplicationLayer;

/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:
	UHScaleNetDriver();

	// ~Begin of UNetDriver interface

public:
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool IsAvailable() const override { return true; }

	virtual bool ShouldReplicateFunction(AActor* Actor, UFunction* Function) const override;
	virtual bool ShouldReplicateActor(AActor* Actor) const override;
	virtual bool IsServer() const override;

	virtual int32 ServerReplicateActors(float DeltaSeconds) override;

	/** Used to update local data from network */
	// virtual void TickDispatch(float DeltaTime) override;
	virtual void PostTickDispatch() override;

	/** Used to push local changes to network */
	virtual void TickFlush(float DeltaSeconds) override;

	virtual FNetworkGUID GetGUIDForActor(const AActor* InActor) const { return FNetworkGUID(); }
	//virtual void PostTickFlush() override {}

	/**
	* Helper functions for ServerReplicateActors
	* Originally they have parent ones starting with "Server..." but they are only declared WITH_Server
	* so we had to create new ones
	* @returns - True, if the connection is ready for replication
	*/
	bool ReplicateActors_PrepConnections(const float DeltaSeconds);
	void ReplicateActors_BuildConsiderList(TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime);

	// ~End of UNetDriver interface


	/** Returns True, if a game has an active session connection with hyperscale server */
	virtual bool IsNetworkSessionActive() const;

	FHScaleNetworkBibliothec* GetBibliothec() const { return Bibliothec.Get(); }
	UHScaleConnection* GetHyperScaleConnection() const;

public:
	/** @warning - Be carefully with this, it should be changed to true only when replication is happening from server */
	uint8 bActAsClient : 1;

	void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const;

	TSharedPtr<FRepLayout> GetObjectClassRepLayout_Copy(UClass* InClass);

	virtual void ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject) override;
private:
	float DeltaReplication = 0.f;

	/** This should be exposed to dev settings probably */
	float ReplicationTick = 0.4f;

	/**
	 * Bibliothec is a data manager that holds data received from server
	 * and is responsible to mark data dirty when are changed locally and prepared to send
	 * over created session stored in property ServerSession
	 *
	 * It is allocated within ServerSession and relevant only during this session
	 */
	TUniquePtr<FHScaleNetworkBibliothec> Bibliothec;

	// UPROPERTY()
	// TObjectPtr<UHScaleReplicationLayer> ReplicationLayer;
};