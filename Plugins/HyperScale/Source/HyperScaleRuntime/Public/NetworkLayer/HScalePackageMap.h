// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Core/HScaleResources.h"
#include "Engine/PackageMapClient.h"
#include "HScalePackageMap.generated.h"

class FHScaleNetworkEntity;
class UHScaleConnection;
/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScalePackageMap : public UPackageMapClient
{
	GENERATED_BODY()

	friend class UHScaleRepDriver;

public:
	// ~Begin of UPackageMap interface
	virtual bool SerializeObject(FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID* OutNetGUID = NULL) override;
	virtual bool SerializeNewActor(FArchive& Ar, class UActorChannel* Channel, class AActor*& Actor) override;
	// ~Begin of UPackageMap interface

	// @pavan: Renamed these with _HS, as the base class functions are not virtual. To avoid confusion in future
	// added one more parameter, bAlreadyAssignedId. If the id is already assigned no need to write out outer info again
	void InternalWriteObject_HS(FArchive& Ar, FNetworkGUID NetGUID, UObject* Object, FString ObjectPathName, UObject* ObjectOuter);
	FNetworkGUID InternalLoadObject_HS(FArchive& Ar, UObject*& Object, const int32 InternalLoadObjectRecursionCount);

	virtual bool WriteActorHeader(FArchive& Ar, AActor* Actor);
	virtual void ReadNewActorHeader(FArchive& Ar, FVector& Location, FRotator& Rotation, TOptional<FVector>& Scale, TOptional<FVector>& Velocity);

	virtual bool SerializeObjectId(FArchive& Ar, UObject* Object, FHScaleNetGUID& OutObjectGUID);
	virtual FHScaleNetGUID FindEntityNetGUID(UObject* Object) const;
	virtual FHScaleNetGUID FindEntityNetGUID(const FNetworkGUID& NetGUID) const;
	virtual bool DoesNetEntityExist(UObject* Object) const;

	virtual UObject* FindObjectFromEntityID(const FHScaleNetGUID& NetGUID);
	virtual FNetworkGUID FindNetGUIDFromHSNetGUID(const FHScaleNetGUID& NetGUID);

	bool ShouldSendFullPath_HS(const UObject* Object, const FNetworkGUID& NetGUID);

	void AssignNetGUID(FNetworkGUID& GUID, UObject* Object);

	void AssignOrGenerateHSNetGUIDForObject(const FHScaleNetGUID& NetGUID, UObject* Object);
	virtual void RemoveGUIDsFromMap(const FHScaleNetGUID& HScaleGUID);

	void CleanUpObjectGuid(const FNetworkGUID NetGUID);

protected:
	virtual void PreRemoteActorSpawn(AActor* InActor);

	UHScaleConnection* GetHScaleConnection();

	virtual void AddGUIDsToMap(const FHScaleNetGUID& HScaleGUID, const FNetworkGUID& NetGUID);

	// #todo.... (actor pooling system) ObjectGuidMapToNetGuid and NetGuidMapToObjectGuid needs to be removed when actor changes entity dependency

	/** All the references that are converting UE object GUIDs to quark GUIDs */
	TMap<FNetworkGUID, FHScaleNetGUID> ObjectGuidMapToNetGuid;
	/** The opposite to the ObjectGuidMapToNetGuid */
	TMap<FHScaleNetGUID, FNetworkGUID> NetGuidMapToObjectGuid;
};