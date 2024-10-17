// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include <map>
#include <memory>

#include "Core/HScaleResources.h"
#include "MemoryLayer/HScaleMemoryTypes.h"
#include "Net/RepLayout.h"
#include "NetworkLayer/HScaleInBunch.h"
#include "Utils/CommonAdapters.h"

class FHScaleEventsDriver;
class FHScaleRepCmdIterator;
class UHScaleActorChannel;
struct FHScaleOuterChunk;
struct FHScaleAttributesUpdate;
class UHScaleConnection;
class UHScaleNetDriver;
class FHScaleStatics;

enum class ENetworkEntityState : uint8
{
	NotInitialized,
	PendingInitialization,
	Initialized,
	MarkedForNetworkDestruction,
	MarkedForLocalDestruction,
	ToBeDestroyed,
};


namespace EHScaleEntityFlags
{
	static constexpr uint16 None = 0;                       //! No Flags
	static constexpr uint16 IsActor = (1 << 1);             //! Indicates whether the network entity is actor or object
	static constexpr uint16 IsComponent = (1 << 2);         //! This is a component
	static constexpr uint16 IsStaticComponent = (1 << 3);   //! See if current entity is static or dynamic component
	static constexpr uint16 HasArchetypeData = (1 << 4);    //! Entity has Class Path info
	static constexpr uint16 HasObjectPath = (1 << 5);       //! Entity has full object path
	static constexpr uint16 IsDataHoldingStruct = (1 << 6); //! Is it a data holding struct like dynamic arrays. Holds data of a property 
};

/**
 * The class defines one entity object in hyperscale network
 */
class HYPERSCALERUNTIME_API FHScaleNetworkEntity final
{
	// Only Bibliothec should have access to create Network entities
	friend class FHScaleNetworkBibliothec;
	// Unit testing class that needs private objects access
	friend class HScaleNetworkEntityTestUtil;

public:
	// Destructor needs to be public, smart pointers can destruct the object if reference is lost
	~FHScaleNetworkEntity() {}

	// #todo@self: implement proper copy and move constructors
	// copy and move constructor because of the Properties holding unique ptrs
	// if needed these needs to be implemented later on
	FHScaleNetworkEntity(const FHScaleNetworkEntity&) = delete;
	FHScaleNetworkEntity& operator=(const FHScaleNetworkEntity&) = delete;
	FHScaleNetworkEntity(FHScaleNetworkEntity&&) = delete;
	FHScaleNetworkEntity& operator=(FHScaleNetworkEntity&&) = delete;

private:
	FHScaleNetworkEntity(const FHScaleNetGUID InEntityId, const uint16 InFlags);

	static TSharedPtr<FHScaleNetworkEntity> Create(const FHScaleNetGUID EntityId);
	static TSharedPtr<FHScaleNetworkEntity> Create(const FHScaleNetGUID EntityId, const uint16 InFlags);

public:
	// Unique Network identifier for the entity with hyperscale network object/player
	const FHScaleNetGUID EntityId;

	uint16 Flags;

	// Set of Child Object ids for which this NetworkEntity is owner
	TSet<FHScaleNetGUID> ChildrenIds;

	// Owner Object id of current entity,if any 
	FHScaleNetGUID Owner;

	// State of Network entity, InitState is used in cases of async loading to identify valid entities
	ENetworkEntityState EntityState;

	int32 ChannelIndex;

private:
	// All properties for entity are stored in this map. This map should be unique owner for properties
	// memory lifetime
	std::map<uint16, std::unique_ptr<FHScaleProperty>> Properties;

	// List of property ids that are changed locally, yet to pushed to server 
	TSet<uint16> LocalDirtyProps;

	// List of property ids that received from server, but yet to be pushed to replication layer
	TSet<uint16> ServerDirtyProps;

	UHScaleNetDriver* NetDriver;

	TMap<uint16, FHScaleNetGUID> SubStructEntities;

	// Class type identifier
	uint64 ClassId;

	// Unreal class pointer of the current entity
	UClass* Clazz;

	// Finds the property from cache if exists, or creates a property with given value type 
	FHScaleProperty* FetchPropertyOnReceive(const uint16 PropertyId, const quark::value& CachedValue);

	// Finds the property from cache if exits, or creates a property with given Cmd type
	FHScaleProperty* FetchApplicationProperty(uint16 PropertyId, const FRepLayoutCmd& Cmd);
	FHScaleProperty* FetchApplicationProperty(uint16 PropertyId, const EHScaleMemoryTypeId TypeId);


	FHScaleProperty* FetchNonApplicationProperty(const uint16 PropertyId);

	void HandleClassSplitStringUpdate();

	void HandleObjectPathUpdate(HScaleTypes::FHScaleObjectDataProperty* Property);

	void HandleArchetypeUpdate(HScaleTypes::FHScaleObjectDataProperty* Property);

	void LoadObjectPtrData(HScaleTypes::FHScaleObjectDataProperty* Property, UObject*& Object) const;

	bool IsHeadersValid() const;

	void HandleReservedProperty(uint16_t PropertyId, FHScaleProperty* Property);

	void HandleSystemPropertyReceive(uint16_t Key, FHScaleProperty* Property);

	void WriteOutSoftObject(FHScaleProperty* Property, FBitWriter& Writer, uint16 PropertyId) const;

	void WriteProperties_R(THScaleUSetSMapIterator<uint16, std::unique_ptr<FHScaleProperty>>& It, FBitWriter& Writer, UClass* Class) const;
	void WriteOutObjPtrData(FHScaleProperty* Property, FBitWriter& Writer, const uint16 PropertyId) const;

	FHScaleProperty* SwitchPropertyWithNewType(uint16 PropertyId, EHScaleMemoryTypeId NewMemoryTypeId);
	bool ReadSoftObjectFromBunch(FBitReader& Bunch, FHScaleProperty*& Property, uint16 PropertyId);
	void ReadProperties_R(FBitReader& Bunch, const TSharedPtr<FObjectReplicator>& ActorReplicator);
	void OnFlagsUpdate() const;
	void CheckAndMarkFlagsDirty();

	void PostReadSerializedData();

	void WriteObjectPath(FBitWriter& Writer);
	void WriteArchetypeData(FBitWriter& Writer);

	bool PullComponentData(FBitWriter& Writer, bool bIncludeSpawnInfo, const TSet<uint16>& AttrToPull);
	bool PullData(FInBunch& Bunch, const bool bIncludeSpawnInfo, const TMap<FHScaleNetGUID, TSet<uint16>>& AttrToPull);

	FHScaleNetworkBibliothec* GetBibliothec() const;
	FHScaleEventsDriver* GetEventsDriver() const;

	UHScaleConnection* GetNetConnection() const;

	void CheckAndUpdateOwner(const TArray<FRepParentCmd>& ParentCmds, const FRepLayoutCmd& RepCmd, FHScaleProperty* Property);

	bool SerializeObjectFromBunch(FBitReader& Ar, FHScaleProperty* Property, uint16 PropertyId);

	void ReadOuterData(FHScaleInBunch& Ar, const UActorChannel* Channel);

	void ReadArchetypeData(FHScaleInBunch& Bunch, const UActorChannel* Channel);
	void UpdateOwner(const FHScaleNetGUID& NewOwner);
	void UpdateClassId(uint64 ClazzId);

	void CheckAndDoFallbackOwnerUpdate(const UHScaleActorChannel* Channel);

	FHScaleNetGUID FetchComponentOwner(const UObject* SubObj) const;
	void ReadComponentsData(FHScaleInBunch& Bunch, UHScaleActorChannel* Channel);

	void LoadStaticComponent();

	void ReadSubObjectPtrChunks(TArray<FHScaleOuterChunk>& OuterChunks, FHScaleNetGUID& HSNetGUID, FNetworkGUID& NetGUID, UObject*& Object) const;

	template<typename T>
	static T* CastPty(FHScaleProperty* Property);

	bool UpdateState(ENetworkEntityState State);

	void ReadSubObjectDeleteUpdate(FHScaleInBunch& Bunch, UHScaleActorChannel* Channel);

public:
	// Returns where the entity is ready for replication
	bool IsReadyForReplication() const
	{
		return EntityState >= ENetworkEntityState::Initialized &&
		       EntityState < ENetworkEntityState::MarkedForNetworkDestruction;
	};

	bool IsInitialized() const { return EntityState >= ENetworkEntityState::Initialized; }

	void ClearLocalDirtyProps();

	void ClearServerDirtyProps();

	/**
	 * This will be called after entity gets into staging mode,
	 * so it will lose actor/object representation in a game world (actor channel is closed)
	 */
	void MarkAsStaging();

	int32 NumLocalDirtyProps() const;

	int32 NumServerDirtyProps() const;

	bool IsPlayer() const { return EntityId.IsValid() && EntityId.IsPlayer(); }

	bool IsStatic() const { return EntityId.IsStatic(); }
	bool IsActor() const { return Flags & EHScaleEntityFlags::IsActor; }

	bool IsComponent() const { return Flags & EHScaleEntityFlags::IsComponent; }

	bool IsSubStructEntity() const { return Flags & EHScaleEntityFlags::IsDataHoldingStruct; }

	bool IsDynArrayEntity() const;

	FHScaleProperty* FindExistingProperty(const uint16 PropertyId) const;

	void AddChild(const FHScaleNetGUID& ChildId);

	/**
	 * Read from supplied network reader, and update properties locally.
	 * All properties updated locally are marked as server dirty
	 */
	void Push(const uint16_t PropertyId, const quark::value& Value, const uint64 Timestamp);

	void PushPlayerOwnedUpdate(const uint16_t Key, const quark::value& Value, const uint64 Timestamp);

	/**
	 * Read from outbunch and update properties locally in entity.
	 * These properties are marked local dirty(network dirty) and replicated to server.
	 * @param Bunch 
	 */
	void Push(FHScaleInBunch& Bunch);

	/**
	 * Writes the local dirty properties into attribute builder
	 */
	void Pull(TArray<FHScaleAttributesUpdate>& Attributes);

	/**
	 * @param Bunch 
	 */
	bool PullUpdate(FInBunch& Bunch);

	bool PullEntire(FInBunch& Bunch);

	bool PullUpdate(FInBunch& Bunch, const TMap<FHScaleNetGUID, TSet<uint16>>& AttrToPull);

	THScaleStdMapPairIterator<uint16, std::unique_ptr<FHScaleProperty>> CreatePropertiesConstIterator() const;

	void DeleteProperty(const uint16 PropertyId);

	void SetNetDriver(UHScaleNetDriver* Driver);

	UHScaleNetDriver* GetNetDriver() const;

	template<typename T>
	T* GetPropertyValueFromCmd(const FRepLayoutCmd& Cmd) const;

	bool GetEntityLocation(FVector& Location) const;

	void MarkEntityLocalDirty();

	void MarkEntityServerDirty();

	bool MarkEntityForNetworkDestruction();

	bool MarkEntityForLocalDestruction();

	bool MarkEntityForDestroy();

	void Destroy();

	void AddLocalDirtyProperty(const uint16 PropertyId);

	void AddServerDirtyProperty(const uint16 PropertyId);

	UClass* FetchEntityUClass();

	FHScaleNetworkEntity* GetOwner() const;

	const FHScaleNetworkEntity* GetNetOwner() const;

	FHScaleNetworkEntity* GetChannelOwner();

	/**
	 * Updates state of the current entity, based on various checks. 
	 */
	void CheckAndReviseStates();

	void PrintHeaders() const;


	// Methods related to dynamic array
private:
	void Dyn_Init(const FHScaleNetGUID& OwnerNetGUID, const uint16 LinkPropertyId);
	bool Dyn_UpdateArraySize(uint16 NewArraySize);
	void Dyn_EnforceArraySize();
	uint16 Dyn_GetArraySize() const;
	bool Dyn_IsHeadersValid() const;
	bool Dyn_ReadUEArrayData(FBitReader& Reader, const FRepLayoutCmd& Cmd);
	bool Dyn_IsValidForWrite() const;
	void Dyn_WriteUEArrayData(FBitWriter& Writer, const FRepLayoutCmd& Cmd);
	void Dyn_MarkEntityServerDirty();
	bool Dyn_ReadDynamicArray(FBitReader& Bunch, FHScaleProperty*& Property, const uint16 PropertyId, FHScaleRepCmdIterator& CmdIterator);
	FHScaleNetworkEntity* Dyn_FetchSubStructDynArrayEntity(uint16 PropertyId);
	FHScaleNetworkEntity* Dyn_FindExistingDynArrayEntity(uint16 PropertyId) const;
	bool Dyn_IsDynamicArrayReadyForWriteOut(uint16 PropertyId) const;
	void Dyn_WriteOutDynArrayData(FBitWriter& Writer, uint16 PropertyId, FHScaleRepCmdIterator& CmdIterator) const;
};


class FHScaleRepCmdIterator
{
public:
	FHScaleRepCmdIterator(const TArray<FRepLayoutCmd>* Cmds)
		: CurIndex(0), Cmds(Cmds) {}

	uint16 FetchCmdIndex(const uint16 PropertyHandle);

	uint16 FetchNextIndex();

	uint16 CurIndex;
	const TArray<FRepLayoutCmd>* Cmds;
};