// Fill out your copyright notice in the Description page of Project Settings.

#include "MemoryLayer/HScaleNetworkEntity.h"

#include "Core/HScaleResources.h"
#include "Engine/ActorChannel.h"
#include "MemoryLayer/HScaleNetworkBibliothec.h"
#include "MemoryLayer/HScalePropertyIdConverters.h"
#include "Net/RepLayout.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "ReplicationLayer/HScaleActorChannel.h"
#include "Utils/CommonAdapters.h"
#include "Utils/HScaleObjectSerializationHelpers.h"
#include "Utils/HScaleStatics.h"

FHScaleNetworkEntity::FHScaleNetworkEntity(const FHScaleNetGUID InEntityId, const uint16 InFlags)
	: EntityId(InEntityId), Flags(InFlags), EntityState(ENetworkEntityState::NotInitialized), ChannelIndex(INDEX_NONE),
	  NetDriver(nullptr), ClassId(0), Clazz(nullptr)
{
	check(EntityId.IsValid()); // Entity should always contain a valid ObjectId
}

TSharedPtr<FHScaleNetworkEntity> FHScaleNetworkEntity::Create(const FHScaleNetGUID EntityId, const uint16 InFlags)
{
	TSharedPtr<FHScaleNetworkEntity> Entity = MakeShareable(new FHScaleNetworkEntity(EntityId, InFlags));
	return Entity;
}

TSharedPtr<FHScaleNetworkEntity> FHScaleNetworkEntity::Create(const FHScaleNetGUID EntityId)
{
	return Create(EntityId, 0 | EHScaleEntityFlags::None);
}

FHScaleProperty* FHScaleNetworkEntity::FetchPropertyOnReceive(const uint16 PropertyId, const quark::value& CachedValue)
{
	const uint16 EqPropertyId = FHScalePropertyIdConverters::GetEquivalentPropertyId(PropertyId);
	// Check if the key exists in the map
	if (!Properties.contains(EqPropertyId))
	{
		EHScaleMemoryTypeId MemoryTypeId = FHScalePropertyIdConverters::FetchMemoryTypeIdForPropertyIdOnReceive(PropertyId, CachedValue.type());
		// Key not found, create a new entry
		Properties[EqPropertyId] = std::move(FHScaleProperty::CreateFromTypeId(MemoryTypeId));
	}

	const auto& Property = Properties.find(EqPropertyId);
	return Property->second.get();
}

FHScaleProperty* FHScaleNetworkEntity::FetchApplicationProperty(const uint16 PropertyId, const FRepLayoutCmd& Cmd)
{
	// Check if the key exists in the map
	if (!Properties.contains(PropertyId))
	{
		// Key not found, create a new entry
		Properties[PropertyId] = std::move(FHScaleProperty::CreateFromCmd(Cmd));
	}

	const auto& Property = Properties.find(PropertyId);
	return Property->second.get();
}

FHScaleProperty* FHScaleNetworkEntity::FetchApplicationProperty(const uint16 PropertyId, const EHScaleMemoryTypeId TypeId)
{
	// Check if the key exists in the map
	if (!Properties.contains(PropertyId))
	{
		// Key not found, create a new entry
		Properties[PropertyId] = std::move(FHScaleProperty::CreateFromTypeId(TypeId));
	}

	const auto& Property = Properties.find(PropertyId);
	return Property->second.get();
}

FHScaleProperty* FHScaleNetworkEntity::FetchNonApplicationProperty(const uint16 PropertyId)
{
	const uint16 EqPropertyId = FHScalePropertyIdConverters::GetEquivalentPropertyId(PropertyId);
	// Check if the key exists in the map
	if (!Properties.contains(EqPropertyId))
	{
		const EHScaleMemoryTypeId MemoryTypeId = FHScalePropertyIdConverters::FetchNonApplicationMemoryTypeIdFromPropertyId(PropertyId);
		// Key not found, create a new entry
		Properties[EqPropertyId] = std::move(FHScaleProperty::CreateFromTypeId(MemoryTypeId));
	}

	const auto& Property = Properties.find(EqPropertyId);
	return Property->second.get();
}

bool FHScaleNetworkEntity::IsDynArrayEntity() const
{
	return IsSubStructEntity() && ClassId == HSCALE_DYNAMIC_ARRAY_CLASS_ID;
}

FHScaleProperty* FHScaleNetworkEntity::FindExistingProperty(const uint16 PropertyId) const
{
	const uint16 EqPropertyId = FHScalePropertyIdConverters::GetEquivalentPropertyId(PropertyId);
	check(Properties.contains(PropertyId))
	const auto& Property = Properties.find(EqPropertyId);
	return Property->second.get();
}

void FHScaleNetworkEntity::AddChild(const FHScaleNetGUID& ChildId)
{
	ChildrenIds.Add(ChildId);
}

template<typename T>
T* FHScaleNetworkEntity::CastPty(FHScaleProperty* Property)
{
	if (Property->IsA<T>()) { return static_cast<T*>(Property); }
	return nullptr;
}

template<typename T>
T* FHScaleNetworkEntity::GetPropertyValueFromCmd(const FRepLayoutCmd& Cmd) const
{
	const uint16 PropertyHandle = Cmd.RelativeHandle;
	const uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(PropertyHandle);
	if (!Properties.contains(PropertyId)) { return nullptr; }
	FHScaleProperty* Property = FindExistingProperty(PropertyId);
	return CastPty<T>(Property);
}

THScaleStdMapPairIterator<uint16, std::unique_ptr<FHScaleProperty>> FHScaleNetworkEntity::CreatePropertiesConstIterator() const
{
	const THScaleStdMapPairIterator<uint16, std::unique_ptr<FHScaleProperty>> It(Properties.cbegin(), Properties.cend());
	return It;
}

void FHScaleNetworkEntity::DeleteProperty(const uint16 PropertyId)
{
	if (!Properties.contains(PropertyId)) return;

	const auto& It = Properties.find(PropertyId);
	It->second.reset();
	Properties.erase(It);
	ServerDirtyProps.Remove(PropertyId);
	LocalDirtyProps.Remove(PropertyId);
}

void FHScaleNetworkEntity::SetNetDriver(UHScaleNetDriver* Driver)
{
	this->NetDriver = Driver;
}

UHScaleNetDriver* FHScaleNetworkEntity::GetNetDriver() const
{
	return NetDriver;
}

bool FHScaleNetworkEntity::GetEntityLocation(FVector& Location) const
{
	if (!Properties.contains(QUARK_KNOWN_ATTRIBUTE_POSITION)) { return false; }
	HScaleTypes::FHScalePositionSystemProperty* PosProperty =
		CastPty<HScaleTypes::FHScalePositionSystemProperty>(FindExistingProperty(QUARK_KNOWN_ATTRIBUTE_POSITION));
	const auto [x, y, z] = PosProperty->GetValue();
	Location.X = x;
	Location.Y = y;
	Location.Z = z;
	return true;
}

void FHScaleNetworkEntity::MarkEntityLocalDirty()
{
	GetBibliothec()->AddLocalDirty(this);
}

void FHScaleNetworkEntity::MarkEntityServerDirty()
{
	if (!IsInitialized()) return;

	if (IsDynArrayEntity())
	{
		Dyn_MarkEntityServerDirty();
	}
	else if (IsComponent())
	{
		GetBibliothec()->AddServerDirty(GetChannelOwner());
	}
	else
	{
		GetBibliothec()->AddServerDirty(this);
	}
}

bool FHScaleNetworkEntity::MarkEntityForNetworkDestruction()
{
	const bool bSuccess = UpdateState(ENetworkEntityState::MarkedForNetworkDestruction);

	if (!bSuccess || IsPlayer()) return bSuccess;
	for (FHScaleNetGUID Child : ChildrenIds)
	{
		TSharedPtr<FHScaleNetworkEntity> ChildPtr = GetBibliothec()->FindExistingEntity(Child);
		if (!ChildPtr.IsValid() || ChildPtr->IsActor()) continue;

		GetEventsDriver()->MarkEntityForNetworkDestruction(Child);
	}
	return bSuccess;
}

bool FHScaleNetworkEntity::MarkEntityForLocalDestruction()
{
	const bool bSuccess = UpdateState(ENetworkEntityState::MarkedForLocalDestruction);

	if (!bSuccess || IsPlayer()) return bSuccess;

	// For components, we mark them as server dirty, which will take care of destruction in next bunch
	if (IsComponent())
	{
		MarkEntityServerDirty();
	}

	for (FHScaleNetGUID Child : ChildrenIds)
	{
		TSharedPtr<FHScaleNetworkEntity> ChildPtr = GetBibliothec()->FindExistingEntity(Child);
		if (!ChildPtr.IsValid() || ChildPtr->IsActor() || ChildPtr->IsSubStructEntity()) continue;

		GetEventsDriver()->MarkEntityForLocalDestruction(Child);
	}
	return bSuccess;
}

bool FHScaleNetworkEntity::MarkEntityForDestroy()
{
	const bool bSuccess = UpdateState(ENetworkEntityState::ToBeDestroyed);
	if (!bSuccess || IsPlayer()) return bSuccess;

	for (FHScaleNetGUID Child : ChildrenIds)
	{
		TSharedPtr<FHScaleNetworkEntity> ChildPtr = GetBibliothec()->FindExistingEntity(Child);
		if (!ChildPtr.IsValid() || ChildPtr->IsActor()) continue;

		GetEventsDriver()->MarkEntityForToDestroy(Child);
	}
	return bSuccess;
}

void FHScaleNetworkEntity::Destroy()
{
	for (auto& Entry : Properties)
	{
		Entry.second.reset();
	}
	Properties.clear();

	if(GetNetConnection())
	{
		GetNetConnection()->RemoveUnmappedObjectPtrs(EntityId);
	}
}

bool FHScaleNetworkEntity::SerializeObjectFromBunch(FBitReader& Ar, FHScaleProperty* Property, const uint16 PropertyId)
{
	TArray<FHScaleOuterChunk> OuterChunks;
	UHScaleConnection* NetConnection = GetNetConnection();
	check(NetConnection)
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
	check(PkgMap)

	FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Ar, OuterChunks, PkgMap);

	if (OuterChunks.IsEmpty()) { return false; }
	HScaleTypes::FHScaleObjectDataProperty* OuterProperty = CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property);
	const FNetworkGUID NetGUID = OuterChunks[OuterChunks.Num() - 1].NetGUID;
	const FHScaleNetGUID HSNetGUID = PkgMap->FindEntityNetGUID(NetGUID);
	const bool bChanged = OuterProperty->SerializeChunks(OuterChunks, PropertyId, NetGUID, HSNetGUID);
	return bChanged;
}

void FHScaleNetworkEntity::ReadOuterData(FHScaleInBunch& Ar, const UActorChannel* Channel)
{
	FHScaleExportFlags ExportFlags;
	Ar << ExportFlags.Value;

	TArray<FHScaleOuterChunk> OuterChunks;
	UHScaleConnection* NetConnection = GetNetConnection();
	check(NetConnection)
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
	check(PkgMap)

	if (!ExportFlags.bHasPath) { return; }

	FHScaleOuterChunk Chunk;
	Chunk.HS_NetGUID = EntityId;
	Chunk.ExportFlags = ExportFlags.Value;
	Chunk.NetGUID = Channel->ActorNetGUID;
	FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Ar, OuterChunks, PkgMap);
	Ar << Chunk.ObjectName;
	OuterChunks.Add(Chunk);

	constexpr uint16 PropertyId = HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID;
	HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FetchNonApplicationProperty(PropertyId));
	const FNetworkGUID NetGUID = OuterChunks[OuterChunks.Num() - 1].NetGUID;
	const FHScaleNetGUID HSNetGUID = PkgMap->FindEntityNetGUID(NetGUID);
	if (Property->SerializeChunks(OuterChunks, PropertyId, NetGUID, HSNetGUID))
	{
		Flags |= EHScaleEntityFlags::HasObjectPath;
		AddLocalDirtyProperty(PropertyId);
	}
}

UClass* FHScaleNetworkEntity::FetchEntityUClass()
{
	return Clazz;
}

FHScaleNetworkEntity* FHScaleNetworkEntity::GetOwner() const
{
	if (Owner.IsValid())
	{
		const TSharedPtr<FHScaleNetworkEntity> Entity = GetBibliothec()->FetchEntity(Owner);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Fetching Owner Entity %llu Flags %d IsActor:%d"), Owner.Get(), Entity->Flags, Entity->IsActor())
		return Entity.Get();
	}
	return nullptr;
}

const FHScaleNetworkEntity* FHScaleNetworkEntity::GetNetOwner() const
{
	if (IsPlayer()) { return this; }
	FHScaleNetGUID TempOwner = Owner;
	TSharedPtr<FHScaleNetworkEntity> Entity;

	// recursively fetch net owner
	while (TempOwner.IsValid())
	{
		Entity = GetBibliothec()->FetchEntity(TempOwner);
		if (Entity.IsValid())
		{
			TempOwner = Entity->Owner;
		}
		else
		{
			return nullptr;
		}
	}

	// if valid owner was found return that
	if (Entity.IsValid() && Entity->IsPlayer())
	{
		return Entity.Get();
	}

	return nullptr;
}

FHScaleNetworkEntity* FHScaleNetworkEntity::GetChannelOwner()
{
	if (IsActor()) { return this; }

	FHScaleNetworkEntity* OwnerEntity = GetOwner();
	if (OwnerEntity) { return OwnerEntity->GetChannelOwner(); }
	return nullptr;
}

void FHScaleNetworkEntity::Push(const uint16_t PropertyId, const quark::value& Value, const uint64 Timestamp)
{
	if (Value.type() == quark::value_type::none)
	{
		DeleteProperty(PropertyId);
		return;
	}

	FHScaleProperty* Property = FetchPropertyOnReceive(PropertyId, Value);
	Property->Deserialize(Value, PropertyId, Timestamp);

	if (FHScalePropertyIdConverters::IsReservedProperty(PropertyId)) // Reserved properties are for plugin usage, they are by default not included as dirty props
	{
		HandleReservedProperty(PropertyId, Property);
	}
	else if (FHScalePropertyIdConverters::IsSystemProperty(PropertyId))
	{
		HandleSystemPropertyReceive(PropertyId, Property);
	}
	else
	{
		if (Property->IsCompleteForReceive())
		{
			AddServerDirtyProperty(PropertyId);

			if (Property->IsA<HScaleTypes::FHScaleObjectDataProperty>())
			{
				HScaleTypes::FHScaleObjectDataProperty* ObjPtr = CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property);
				UObject* Object = nullptr;
				LoadObjectPtrData(ObjPtr, Object);

				FString ObjName = Object ? Object->GetPathName() : FString();
				UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Before write out obj ptr data %s"), *ObjName)
			}
		}
	}
}


void FHScaleNetworkEntity::HandleClassSplitStringUpdate()
{
	CheckAndReviseStates();
}

void FHScaleNetworkEntity::HandleObjectPathUpdate(HScaleTypes::FHScaleObjectDataProperty* Property)
{
	if (!Property->IsCompleteForReceive()) return;
	UObject* Object = nullptr;
	LoadObjectPtrData(Property, Object);
	if (Object)
	{
		Clazz = Object->GetClass();
		UHScaleConnection* NetConnection = GetNetConnection();
		check(NetConnection)
		UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
		check(PkgMap)

		PkgMap->AssignOrGenerateHSNetGUIDForObject(EntityId, Object);
		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Loaded object Path Update, for clazz %s"), *Clazz->GetName())
	}
}

void FHScaleNetworkEntity::HandleArchetypeUpdate(HScaleTypes::FHScaleObjectDataProperty* Property)
{
	if (!Property->IsCompleteForReceive()) return;
	UObject* Object = nullptr;
	LoadObjectPtrData(Property, Object);
	if (Object)
	{
		if (Object->IsA<UClass>())
		{
			Clazz = Cast<UClass>(Object);
		}
		else
		{
			Clazz = Object->GetClass();
		}
		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Loaded archetype Update, for clazz %s"), *Clazz->GetName())
	}
}


void FHScaleNetworkEntity::LoadStaticComponent()
{
	check(IsComponent())
	check(Flags & EHScaleEntityFlags::IsStaticComponent)

	UHScaleConnection* NetConnection = GetNetConnection();
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
	check(PkgMap)

	constexpr uint16 PropertyId = HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID;
	HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(PropertyId));
	check(Property)

	UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("About to load component for EntityId: %llu"), EntityId.Get())
	UObject* Object = nullptr;
	LoadObjectPtrData(Property, Object);
	if (Object)
	{
		Clazz = Object->GetClass();
		PkgMap->AssignOrGenerateHSNetGUIDForObject(EntityId, Object);
	}
}

void FHScaleNetworkEntity::LoadObjectPtrData(HScaleTypes::FHScaleObjectDataProperty* Property, UObject*& Object) const
{
	TArray<FHScaleOuterChunk> Chunks;
	Property->DeserializeChunks(Chunks);

	if (Chunks.IsEmpty()) return;

	Object = nullptr;
	FNetworkGUID NetGUID;

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(GetNetConnection()->PackageMap);
	check(PkgMap)
	
	FHScaleObjectSerializationHelper::LoadObject(Object, Chunks, NetGUID, PkgMap);
	Property->NetworkGUID = NetGUID;
}

bool FHScaleNetworkEntity::IsHeadersValid() const
{
	if (!Flags) return false;

	if (Flags & EHScaleEntityFlags::IsDataHoldingStruct)
	{
		if (!ClassId) return false;

		if (!Owner.IsValid()) return false;

		if (!Properties.contains(HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID)) return false;
		HScaleTypes::FHScaleUInt16Property* LinkProperty = CastPty<HScaleTypes::FHScaleUInt16Property>(FindExistingProperty(HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID));
		const uint16 LinkValue = LinkProperty->GetValue();
		if (!LinkValue) return false;

		if (ClassId == HSCALE_DYNAMIC_ARRAY_CLASS_ID)
		{
			return Dyn_IsHeadersValid();
		}
	}

	if (Flags & EHScaleEntityFlags::HasObjectPath)
	{
		if (!Properties.contains(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID)) return false;

		HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID));
		if (!Property->IsValid()) return false;
	}

	if (Flags & EHScaleEntityFlags::HasArchetypeData)
	{
		if (!Properties.contains(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID)) return false;

		HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID));
		if (!Property->IsValid()) return false;
	}

	if (Flags & EHScaleEntityFlags::IsComponent)
	{
		if (!Owner.IsValid()) return false;
	}

	return true;
}

void FHScaleNetworkEntity::HandleReservedProperty(const uint16_t PropertyId, FHScaleProperty* Property)
{
#if HS_SPLIT_STRING_CLASSES
	if (FHScalePropertyIdConverters::IsClassSplitStringRange(PropertyId))
	{
		HandleClassSplitStringUpdate();
	}
#endif

	if (FHScalePropertyIdConverters::IsEntityObjectPathRange(PropertyId))
	{
		HandleObjectPathUpdate(CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property));
	}

	if (FHScalePropertyIdConverters::IsEntityArchetypeRange(PropertyId))
	{
		HandleArchetypeUpdate(CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property));
	}

	if (PropertyId == HS_RESERVED_OBJECT_FLAGS_ATTRIBUTE_ID)
	{
		HScaleTypes::FHScaleUInt16Property* FlagsProp = CastPty<HScaleTypes::FHScaleUInt16Property>(Property);
		Flags = FlagsProp->GetValue();

		if (Flags & EHScaleEntityFlags::HasObjectPath && Properties.contains(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID))
		{
			FHScaleProperty* PropPtr = FindExistingProperty(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID);
			HScaleTypes::FHScaleObjectDataProperty* ObjProperty = CastPty<HScaleTypes::FHScaleObjectDataProperty>(PropPtr);
			HandleObjectPathUpdate(ObjProperty);
		}

		if (Flags & EHScaleEntityFlags::HasArchetypeData && Properties.contains(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID))
		{
			FHScaleProperty* PropPtr = FindExistingProperty(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID);
			HScaleTypes::FHScaleObjectDataProperty* ObjProperty = CastPty<HScaleTypes::FHScaleObjectDataProperty>(PropPtr);
			HandleArchetypeUpdate(ObjProperty);
		}

		OnFlagsUpdate();
	}
	CheckAndReviseStates();
}

void FHScaleNetworkEntity::AddLocalDirtyProperty(const uint16 PropertyId)
{
	const uint16 EqPropertyId = FHScalePropertyIdConverters::GetEquivalentPropertyId(PropertyId);
	LocalDirtyProps.Add(EqPropertyId);
}

void FHScaleNetworkEntity::AddServerDirtyProperty(const uint16 PropertyId)
{
	const uint16 EqPropertyId = FHScalePropertyIdConverters::GetEquivalentPropertyId(PropertyId);
	ServerDirtyProps.Add(EqPropertyId);
}

void FHScaleNetworkEntity::CheckAndReviseStates()
{
	switch (EntityState)
	{
		case ENetworkEntityState::NotInitialized:
			UpdateState(ENetworkEntityState::PendingInitialization);
			break;
		case ENetworkEntityState::PendingInitialization:
			if (IsHeadersValid()) { UpdateState(ENetworkEntityState::Initialized); }
			break;
		case ENetworkEntityState::Initialized:
			break;
		default:
			break;
	}
}

void FHScaleNetworkEntity::PrintHeaders() const
{
	UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("IsActor:%d IsComponent:%d IsArchetype:%d IsFullObjectPath:%d IsStaticComponent:%d"), Flags&EHScaleEntityFlags::IsActor, Flags&EHScaleEntityFlags::IsComponent, Flags&EHScaleEntityFlags::HasArchetypeData, Flags&EHScaleEntityFlags::HasObjectPath, Flags&EHScaleEntityFlags::IsStaticComponent)

	if (Flags & EHScaleEntityFlags::HasObjectPath)
	{
		if (!Properties.contains(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID))
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("FullObjectProperty is absent"))
			return;
		}

		HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID));
		if (!Property->IsValid())
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("FullObjectProperty is invalid"))
			return;
		}
	}

	if (Flags & EHScaleEntityFlags::HasArchetypeData)
	{
		if (!Properties.contains(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID))
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("ArchetypeProperty is absent"))
			return;
		}

		HScaleTypes::FHScaleObjectDataProperty* Property = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID));
		if (!Property->IsValid())
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("ArchetypeProperty is invalid"))
			return;
		}
	}

	if (Flags & EHScaleEntityFlags::IsComponent)
	{
		if (!Owner.IsValid())
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Owner value is invalid"))
			return;
		}

		TSharedPtr<FHScaleNetworkEntity> OwnerPtr = GetBibliothec()->FetchEntity(Owner);

		if (!OwnerPtr->IsReadyForReplication())
		{
			UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Owner %llu is not ready for replication"), Owner.Get())
			return;
		}

		if (Flags & EHScaleEntityFlags::IsStaticComponent)
		{
			if (!Properties.contains(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID))
			{
				UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("ObjectPath is absent"))
				return;
			}

			HScaleTypes::FHScaleObjectDataProperty* PathPtr = CastPty<HScaleTypes::FHScaleObjectDataProperty>(FindExistingProperty(HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID));
			if (!PathPtr->IsValid())
			{
				UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("FullObjectProperty is invalid"))
				return;
			}
		}
	}
}

uint16 FHScaleRepCmdIterator::FetchCmdIndex(const uint16 PropertyHandle)
{
	const uint16 ExpectedIndex = PropertyHandle - 1;
	CurIndex = FMath::Max(CurIndex, ExpectedIndex);
	while (CurIndex < Cmds->Num())
	{
		const FRepLayoutCmd& Cmd = (*Cmds)[CurIndex];
		if (Cmd.RelativeHandle == PropertyHandle)
		{
			return CurIndex;
		}
		CurIndex++;
	}

	return UINT16_MAX;
}

uint16 FHScaleRepCmdIterator::FetchNextIndex()
{
	CurIndex++;
	return CurIndex;
}

void FHScaleNetworkEntity::HandleSystemPropertyReceive(const uint16_t Key, FHScaleProperty* Property)
{
	if (Key == QUARK_KNOWN_ATTRIBUTE_CLASS_ID)
	{
		const HScaleTypes::FHScaleUInt64Property* ClassProperty = CastPty<HScaleTypes::FHScaleUInt64Property>(Property);
		check(ClassProperty)
		ClassId = ClassProperty->GetValue();
	}
	else if (Key == QUARK_KNOWN_ATTRIBUTE_OWNER_ID)
	{
		const HScaleTypes::FHScaleOwnerProperty* OwnerProperty = CastPty<HScaleTypes::FHScaleOwnerProperty>(Property);
		check(OwnerProperty)
		Owner = FHScaleNetGUID::Create(OwnerProperty->GetValue());

		if (Owner.IsObject())
		{
			TSharedPtr<FHScaleNetworkEntity> OwnerEntity = GetBibliothec()->FetchEntity(Owner);
			OwnerEntity->AddChild(this->EntityId);
		}
	}
	else if (Key == QUARK_KNOWN_ATTRIBUTE_POSITION)
	{
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("System position property received %d"), Key);
	}
	else
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("Unhandled system property id received %d"), Key);
	}
	CheckAndReviseStates();
}

void FHScaleNetworkEntity::WriteOutSoftObject(FHScaleProperty* Property, FBitWriter& Writer, uint16 PropertyId) const
{
	const bool bObjectPtr = Property->IsA<HScaleTypes::FHScaleObjectDataProperty>();
	Writer.UsingCustomVersion(FEngineNetworkCustomVersion::Guid);

	if (Writer.EngineNetVer() >= FEngineNetworkCustomVersion::SoftObjectPtrNetGuids)
	{
		Writer.WriteBit(bObjectPtr ? 0 : 1);
	}

	if (bObjectPtr)
	{
		WriteOutObjPtrData(Property, Writer, PropertyId);
	}
	else
	{
		HScaleTypes::FHScaleSplitStringProperty* StrProperty = CastPty<HScaleTypes::FHScaleSplitStringProperty>(Property);
		StrProperty->SerializeFString(Writer);
	}
}

void FHScaleNetworkEntity::WriteProperties_R(THScaleUSetSMapIterator<uint16, std::unique_ptr<FHScaleProperty>>& It, FBitWriter& Writer, UClass* ObjectClass) const
{
	Writer.WriteBit(0); // bEnablePropertyChecksum
	check(ObjectClass)
	UHScaleNetDriver* Driver = GetNetDriver();
	const TSharedPtr<FRepLayout> RepLayout = Driver->GetObjectClassRepLayout_Copy(ObjectClass);
	const TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Cmds);

	FHScaleRepCmdIterator CmdIterator(&Cmds);
	for (; !It.IsEnd(); ++It)
	{
		const uint16 PropertyId = *It;
		if (!Properties.contains(PropertyId)) { continue; }
		if (FHScalePropertyIdConverters::IsSystemProperty(PropertyId) || FHScalePropertyIdConverters::IsReservedProperty(PropertyId)) { continue; }
		FHScaleProperty* Property = FindExistingProperty(PropertyId);
		uint32 PropertyHandle = FHScalePropertyIdConverters::GetPropertyHandleFromPropertyId(PropertyId);

		const uint16 PropertyHandle_R = PropertyHandle;
		const uint16 CmdIndex = CmdIterator.FetchCmdIndex(PropertyHandle_R);
		if (!ensure(Cmds.Num() > CmdIndex)) { return; }
		const FRepLayoutCmd& Cmd = Cmds[CmdIndex];
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("WriteProperties_R:: PropertyHandle %d Cmd Type %d  Name: %s Value: %s"),
			PropertyHandle, (uint8)Cmd.Type, *Cmd.Property->GetName(), *Property->ToDebugString())

		if (Cmd.Type == ERepLayoutCmdType::DynamicArray)
		{
			if (Dyn_IsDynamicArrayReadyForWriteOut(PropertyId))
			{
				Writer.SerializeIntPacked(PropertyHandle);
				Dyn_WriteOutDynArrayData(Writer, PropertyId, CmdIterator);
			}
			continue;
		}

		Writer.SerializeIntPacked(PropertyHandle);
		if (FHScaleStatics::IsObjectDataRepCmd(Cmd))
		{
			WriteOutObjPtrData(Property, Writer, PropertyId);
		}
		else if (Cmd.Type == ERepLayoutCmdType::PropertySoftObject)
		{
			WriteOutSoftObject(Property, Writer, PropertyId);
		}
		else
		{
			Property->SerializeUE(Writer, Cmd);
		}
	}
	// write 0 at the end of rep properties
	uint32 EndProperty = 0;
	Writer.SerializeIntPacked(EndProperty);
}

void FHScaleNetworkEntity::WriteOutObjPtrData(FHScaleProperty* Property, FBitWriter& Writer, const uint16 PropertyId) const
{
	HScaleTypes::FHScaleObjectDataProperty* ObjPtr = CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property);
	check(ObjPtr)
	// check(Property->IsCompleteForReceive())
	UObject* Object = nullptr;
	if (!ObjPtr->NetworkGUID.IsValid() || ObjPtr->NetworkGUID.IsDefault())
	{
		LoadObjectPtrData(ObjPtr, Object);
	}
	if (!Object && ObjPtr->HScaleNetGUID.IsObject())
	{
		const TTuple<FHScaleNetGUID, uint16> Pair(EntityId, PropertyId);
		GetNetConnection()->AddUnmappedObjectPtr(ObjPtr->HScaleNetGUID, Pair);
		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Object Ptr %s is not loaded yet for Entity %s, adding to unmapped objects"), *ObjPtr->HScaleNetGUID.ToString(), *EntityId.ToString())
	}
	ObjPtr->WriteOut(Writer);
}

FHScaleProperty* FHScaleNetworkEntity::SwitchPropertyWithNewType(const uint16 PropertyId, EHScaleMemoryTypeId NewMemoryTypeId)
{
	DeleteProperty(PropertyId);
	Properties[PropertyId] = std::move(FHScaleProperty::CreateFromTypeId(NewMemoryTypeId));
	const auto& Property = Properties.find(PropertyId);
	return Property->second.get();
}

bool FHScaleNetworkEntity::ReadSoftObjectFromBunch(FBitReader& Bunch, FHScaleProperty*& Property, uint16 PropertyId)
{
	bool bObjectPtr = false;;
	Bunch.UsingCustomVersion(FEngineNetworkCustomVersion::Guid);

	if (Bunch.EngineNetVer() >= FEngineNetworkCustomVersion::SoftObjectPtrNetGuids)
	{
		const bool bUsePath = !!Bunch.ReadBit();
		if (!bUsePath) bObjectPtr = true;;
	}
	// soft object ptrs will either be a object ptr or the path as string
	if (bObjectPtr)
	{
		if (!Property->IsA<HScaleTypes::FHScaleObjectDataProperty>())
		{
			Property = SwitchPropertyWithNewType(PropertyId, EHScaleMemoryTypeId::ObjectPtrData);
		}
		return SerializeObjectFromBunch(Bunch, Property, PropertyId);
	}
	else
	{
		if (!Property->IsA<HScaleTypes::FHScaleSplitStringProperty>())
		{
			Property = SwitchPropertyWithNewType(PropertyId, EHScaleMemoryTypeId::SplitString);
		}
		HScaleTypes::FHScaleSplitStringProperty* StrProperty = CastPty<HScaleTypes::FHScaleSplitStringProperty>(Property);
		return StrProperty->SerializeFString(Bunch);
	}
}

void FHScaleNetworkEntity::WriteObjectPath(FBitWriter& Writer)
{
	FHScaleNetGUID GUID = EntityId;

	Writer.WriteBit(1); // bHasNetworkId
	Writer << GUID;
	FHScaleExportFlags ExportFlags;
	ExportFlags.bHasPath = 0;
	Writer << ExportFlags.Value;
}

void FHScaleNetworkEntity::WriteArchetypeData(FBitWriter& Writer)
{
	if (Flags & EHScaleEntityFlags::HasArchetypeData || (IsComponent() && !(Flags & EHScaleEntityFlags::IsStaticComponent)))
	{
		HScaleTypes::FHScaleObjectDataProperty* ArchetypeProperty =
			CastPty<HScaleTypes::FHScaleObjectDataProperty>(FetchNonApplicationProperty(HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID));
		ArchetypeProperty->WriteOut(Writer);
	}
	else
	{
		// writes null object pointer
		Writer.WriteBit(0);
		FNetworkGUID NetworkGUID;
		Writer << NetworkGUID;
	}
}

bool FHScaleNetworkEntity::PullComponentData(FBitWriter& Writer, bool bIncludeSpawnInfo, const TSet<uint16>& AttrToPull)
{
	check(IsComponent())

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(GetNetConnection()->PackageMap);
	check(PkgMap)

	const bool bIsLocalDelete = EntityState == ENetworkEntityState::MarkedForLocalDestruction;

	UObject* SubObj = PkgMap->FindObjectFromEntityID(EntityId);

	// If no update to write then skip
	if (!bIncludeSpawnInfo
	    && AttrToPull.IsEmpty()
	    && NumServerDirtyProps() == 0
	    && !bIsLocalDelete
	    && SubObj)
		return true;

	bool bIsStaticComponent = Flags & EHScaleEntityFlags::IsStaticComponent;

	// for static components directly load the component
	if (bIsStaticComponent) { LoadStaticComponent(); }
	
	// A valid case where underlying owning actor is not loaded yet, in those cases we mark t
	if (!IsValid(SubObj) && bIsStaticComponent)
	{
		MarkEntityServerDirty();
		return false;
	}

	// Unreal standard replication serialization starts from here
	Writer.WriteBit(bIsLocalDelete ? 0 : 1); // bHasRepLayout is false if sub-object is to be deleted
	Writer.WriteBit(0);                      // bIsActor

	FHScaleNetGUID CompEntityId = EntityId;
	Writer << CompEntityId;

	if (IsValid(SubObj))
	{
		// write out EntityId in Archive
		FHScaleNetGUID GUID = EntityId;
		Writer.WriteBit(1); // bHasNetworkId
		Writer << GUID;
		FHScaleExportFlags ExportFlags;
		ExportFlags.bHasPath = 0;
		Writer << ExportFlags.Value;
	}
	else
	{
		// write out Empty id this should happen only for first time dynamic objects
		check(!bIsStaticComponent)
		// writes null object pointer
		Writer.WriteBit(0);
		FNetworkGUID NetworkGUID;
		Writer << NetworkGUID;
	}
	// End of Serialize Object
	Writer.WriteBit(bIsStaticComponent ? 1 : 0); // bStablyNamed

	if (bIsLocalDelete)
	{
		Writer.WriteBit(1); // Stating its a destroy message

		uint8 DestroyFlags = UHScaleActorChannel::GetSubObjectDeleteTearOffFlag();
		Writer << DestroyFlags;
		return true;
	}

	if (!bIsStaticComponent)
	{
		Writer.WriteBit(0); // stating not a destroy message
		WriteArchetypeData(Writer);

		FHScaleNetworkEntity* OwnerEntity = GetOwner();
		bool bActorIsOuter = OwnerEntity->IsActor();
		Writer.WriteBit(bActorIsOuter ? 1 : 0); // bActorIsOuter

		if (!bActorIsOuter)
		{
			FHScaleNetGUID GUID = Owner;
			Writer.WriteBit(1); // bHasNetworkId
			Writer << GUID;
			FHScaleExportFlags ExportFlags;
			ExportFlags.bHasPath = 0;
			Writer << ExportFlags.Value;
		}
	}
	FBitWriter RepProperties(16384);
	// RepProperties.SetEngineNetVer(Bunch.EngineNetVer());
	UClass* ObjectClass = FetchEntityUClass();

	bool bClearServerDirty = true;
	TSet<uint16> DirtyPropsCopy;
	if (!AttrToPull.IsEmpty())
	{
		DirtyPropsCopy = AttrToPull;
		bClearServerDirty = false;
	}
	else { DirtyPropsCopy = ServerDirtyProps; }
	DirtyPropsCopy.Sort([](const uint16& A, const uint16& B) { return A < B; });
	THScaleUSetSMapIterator<uint16, std::unique_ptr<FHScaleProperty>> It(Properties.cbegin(),
		Properties.cend(), DirtyPropsCopy.CreateConstIterator(), !bIncludeSpawnInfo);

	WriteProperties_R(It, RepProperties, ObjectClass);

	// Write out paylod length
	uint32 PayloadLength = RepProperties.GetNumBits();
	Writer.SerializeIntPacked(PayloadLength);

	Writer.SerializeBits(RepProperties.GetData(), PayloadLength);

	// Clear ServerDirtyProps
	if (bClearServerDirty) { ClearServerDirtyProps(); }

	return true;
}

bool FHScaleNetworkEntity::PullData(FInBunch& Bunch, const bool bIncludeSpawnInfo, const TMap<FHScaleNetGUID, TSet<uint16>>& AttrToPull)
{
	check(IsReadyForReplication())
	check(IsActor())

	bool bResult = true;

	FBitWriter Writer(49152);
	// Writer.SetEngineNetVer(Bunch.EngineNetVer());
	FHScaleNetGUID GUID = EntityId;

	Writer << GUID;

	// SerializeActor
	// SerializeObject ObjectPath
	WriteObjectPath(Writer);

	Writer.WriteBit(bIncludeSpawnInfo ? 1 : 0);
	if (bIncludeSpawnInfo)
	{
		FHScaleNetGUID LevelId;
		Writer << LevelId; // LevelID is zero right now. #todo:for @later with proper level id

		// Serialize Archetype
		WriteArchetypeData(Writer);

		// Actor Header
		// write out position
		HScaleTypes::FHScalePositionSystemProperty* PosProperty =
			CastPty<HScaleTypes::FHScalePositionSystemProperty>(FetchNonApplicationProperty(QUARK_KNOWN_ATTRIBUTE_POSITION));
		check(PosProperty != nullptr)

		PosProperty->SerializePosition(Writer);

		// write out rotation also
		// #todo: implement rotation also
		bool bRotationSerialized;
		FRotator TempRotation;
		TempRotation.NetSerialize(Writer, nullptr, bRotationSerialized);

		Writer.WriteBit(0); // bContainsScale info as false right now
		Writer.WriteBit(0); // bContainsInstanceVelocity info as false right now
	}

	// Unreal standard replication serialization starts from here
	Writer.WriteBit(1); // bHasRepLayout
	Writer.WriteBit(1); // bIsActor

	FBitWriter RepProperties(40960);
	UClass* ObjectClass = FetchEntityUClass();

	bool bClearServerDirty = true;
	TSet<uint16> DirtyPropsCopy;
	if (AttrToPull.Contains(EntityId) && !AttrToPull[EntityId].IsEmpty())
	{
		DirtyPropsCopy = AttrToPull[EntityId];
		bClearServerDirty = false;
	}
	else { DirtyPropsCopy = ServerDirtyProps; }
	DirtyPropsCopy.Sort([](const uint16& A, const uint16& B) { return A < B; });
	THScaleUSetSMapIterator<uint16, std::unique_ptr<FHScaleProperty>> It(Properties.cbegin(),
		Properties.cend(), DirtyPropsCopy.CreateConstIterator(), !bIncludeSpawnInfo);

	WriteProperties_R(It, RepProperties, ObjectClass);

	// Write out paylod length
	uint32 PayloadLength = RepProperties.GetNumBits();
	Writer.SerializeIntPacked(PayloadLength);

	Writer.SerializeBits(RepProperties.GetData(), PayloadLength);

	for (FHScaleNetGUID Child : ChildrenIds)
	{
		if (!GetBibliothec()->IsEntityExists(Child)) continue;
		TSharedPtr<FHScaleNetworkEntity> ChildEntity = GetBibliothec()->FetchEntity(Child);
		if (!ChildEntity.IsValid() || !ChildEntity->IsInitialized() || !ChildEntity->IsComponent()) continue;

		TSet<uint16> AttrSet;
		if (AttrToPull.Contains(Child) && !AttrToPull[Child].IsEmpty())
		{
			AttrSet = AttrToPull[Child];
		}
		bResult &= ChildEntity->PullComponentData(Writer, bIncludeSpawnInfo, AttrSet);
	}

	Bunch.SetData(Writer.GetData(), Writer.GetNumBits());
	// Clear ServerDirtyProps
	if (bClearServerDirty) { ClearServerDirtyProps(); }

	return bResult;
}

FHScaleNetworkBibliothec* FHScaleNetworkEntity::GetBibliothec() const
{
	if (const UHScaleNetDriver* Driver = GetNetDriver())
	{
		return Driver->GetBibliothec();
	}
	return nullptr;
}

FHScaleEventsDriver* FHScaleNetworkEntity::GetEventsDriver() const
{
	if (const UHScaleConnection* Connection = GetNetConnection())
	{
		return Connection->GetEventsDriver();
	}
	return nullptr;
}

UHScaleConnection* FHScaleNetworkEntity::GetNetConnection() const
{
	if (const UHScaleNetDriver* Driver = GetNetDriver())
	{
		return Driver->GetHyperScaleConnection();
	}
	return nullptr;
}

bool FHScaleNetworkEntity::UpdateState(const ENetworkEntityState State)
{
	if (EntityState == State) { return true; }
	switch (EntityState)
	{
		case ENetworkEntityState::MarkedForNetworkDestruction:
			if (State == ENetworkEntityState::MarkedForLocalDestruction)
				return false;
		default:
			if (EntityState > State) return false;
			break;
	}

	EntityState = State;
	CheckAndReviseStates();
	return true;
}

void FHScaleNetworkEntity::ReadSubObjectDeleteUpdate(FHScaleInBunch& Bunch, UHScaleActorChannel* Channel)
{
	if (Bunch.AtEnd())
	{
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Reached end, no more component data is present"))
		return;
	}

	FNetworkGUID GuidToDelete;
	Bunch << GuidToDelete;

	const bool bIsStatic = !!Bunch.ReadBit();
	check(!bIsStatic)

	const bool bIsDelete = !!Bunch.ReadBit();
	check(bIsDelete)

	uint8 DeleteFlag;
	Bunch << DeleteFlag;

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(GetNetConnection()->PackageMap);
	check(PkgMap)

	// Mark Sub-object for network destruction
	FHScaleNetGUID CompGUID = PkgMap->FindEntityNetGUID(GuidToDelete);
	if (CompGUID.IsValid())
	{
		GetEventsDriver()->MarkEntityForNetworkDestruction(CompGUID);
	}

	if (Bunch.AtEnd()) { return; }
	// if there is still data to read in bunch, it should be replication data of next component
	bool bHasRepLayout = !!Bunch.ReadBit();
	bool bIsActor = !!Bunch.ReadBit();

	check(!bIsActor) // actors can't be sub-objects/components, they have their own channel

	if (bHasRepLayout) { ReadComponentsData(Bunch, Channel); }
	else { ReadSubObjectDeleteUpdate(Bunch, Channel); }
}

void FHScaleNetworkEntity::ClearLocalDirtyProps()
{
	LocalDirtyProps.Empty();
}

void FHScaleNetworkEntity::ClearServerDirtyProps()
{
	ServerDirtyProps.Empty();
}

void FHScaleNetworkEntity::MarkAsStaging()
{
	ChannelIndex = INDEX_NONE;
}

int FHScaleNetworkEntity::NumLocalDirtyProps() const
{
	return LocalDirtyProps.Num();
}

int FHScaleNetworkEntity::NumServerDirtyProps() const
{
	return ServerDirtyProps.Num();
}

void FHScaleNetworkEntity::PushPlayerOwnedUpdate(const uint16_t Key, const quark::value& Value, const uint64 Timestamp)
{
	if (Key == QUARK_KNOWN_ATTRIBUTE_OWNER_ID)
	{
		FHScaleProperty* Property = FetchPropertyOnReceive(Key, Value);
		Property->Deserialize(Value, Key, Timestamp);
		HandleReservedProperty(Key, Property);
	}
}

void FHScaleNetworkEntity::ReadArchetypeData(FHScaleInBunch& Bunch, const UActorChannel* Channel)
{
	bool bContainsArchetypeData = !!Bunch.ReadBit();
	if (!bContainsArchetypeData) return;

	constexpr uint16 PropertyId = HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID;
	FHScaleProperty* Property = FetchNonApplicationProperty(PropertyId);
	if (SerializeObjectFromBunch(Bunch, Property, PropertyId))
	{
		Flags |= EHScaleEntityFlags::HasArchetypeData;
		AddLocalDirtyProperty(PropertyId);
	}
}

void FHScaleNetworkEntity::UpdateOwner(const FHScaleNetGUID& NewOwner)
{
	if (Owner == NewOwner) { return; }
	HScaleTypes::FHScaleOwnerProperty* OwnerProperty = CastPty<HScaleTypes::FHScaleOwnerProperty>
		(FetchNonApplicationProperty(QUARK_KNOWN_ATTRIBUTE_OWNER_ID));
	OwnerProperty->SetValue(NewOwner.Get());
	Owner = NewOwner;
	AddLocalDirtyProperty(QUARK_KNOWN_ATTRIBUTE_OWNER_ID);

	const TSharedPtr<FHScaleNetworkEntity> OwnerEntity = GetBibliothec()->FetchEntity(Owner);
	OwnerEntity->AddChild(this->EntityId);
}

void FHScaleNetworkEntity::UpdateClassId(const uint64 ClazzId)
{
	if (ClassId == ClazzId) { return; }
	HScaleTypes::FHScaleUInt64Property* ClazzProperty = CastPty<HScaleTypes::FHScaleUInt64Property>
		(FetchNonApplicationProperty(QUARK_KNOWN_ATTRIBUTE_CLASS_ID));
	ClazzProperty->SetValue(ClazzId);
	ClassId = ClazzId;
	AddLocalDirtyProperty(QUARK_KNOWN_ATTRIBUTE_CLASS_ID);
}

void FHScaleNetworkEntity::CheckAndDoFallbackOwnerUpdate(const UHScaleActorChannel* Channel)
{
	if (Owner.IsValid()) { return; }

	// if there is no owner, update it with net owner value from  
	UpdateOwner(Channel->PlayerNetOwnerGUID);
}

FHScaleNetGUID FHScaleNetworkEntity::FetchComponentOwner(const UObject* SubObj) const
{
	check(SubObj)

	UObject* OuterObj = SubObj->GetOuter();
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(GetNetConnection()->PackageMap);
	check(PkgMap)

	return PkgMap->FindEntityNetGUID(OuterObj);
}

void FHScaleNetworkEntity::ReadSubObjectPtrChunks(TArray<FHScaleOuterChunk>& OuterChunks, FHScaleNetGUID& HSNetGUID, FNetworkGUID& NetGUID, UObject*& Object) const
{
	if (!HSNetGUID.IsValid()) { return; }

	UHScaleConnection* NetConnection = GetNetConnection();
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
	check(PkgMap)

	Object = PkgMap->FindObjectFromEntityID(HSNetGUID);
	if (!IsValid(Object)) { return; }
	NetGUID = PkgMap->FindNetGUIDFromHSNetGUID(HSNetGUID);
	OuterChunks.Empty();

	FHScaleOuterChunk Chunk;
	Chunk.NetGUID = NetGUID;
	Chunk.ObjectName = Object->GetName();

	FBitWriter Writer(8192);
	UObject* Outer = Object->GetOuter();
	FNetworkGUID OuterNetGUID;
	PkgMap->SerializeObject(Writer, UObject::StaticClass(), Outer, &OuterNetGUID);

	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Reader, OuterChunks, PkgMap);
	OuterChunks.Add(Chunk);
}

void FHScaleNetworkEntity::ReadComponentsData(FHScaleInBunch& Bunch, UHScaleActorChannel* Channel)
{
	if (Bunch.AtEnd())
	{
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Reached end, no more component data is present"))
		return;
	}

	TArray<FHScaleOuterChunk> ObjectChunks;
	UHScaleConnection* NetConnection = GetNetConnection();
	check(NetConnection)
	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(NetConnection->PackageMap);
	check(PkgMap)

	FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Bunch, ObjectChunks, PkgMap);

	if (ObjectChunks.IsEmpty())
	{
		UE_LOG(Log_HyperScaleMemory, Error, TEXT("Found empty outerchunks in bunch for components"))
		return;
	}

	FHScaleNetGUID CompHSGUID = ObjectChunks[ObjectChunks.Num() - 1].HS_NetGUID;

	if (!CompHSGUID.IsValid())
	{
		UE_LOG(Log_HyperScaleMemory, Warning, TEXT("HS_NetGUID should exist, something went wrong"))
		return;
	}

	FNetworkGUID NetGUID;
	UObject* Object;
	ReadSubObjectPtrChunks(ObjectChunks, CompHSGUID, NetGUID, Object);

	if (!IsValid(Object))
	{
		UE_LOG(Log_HyperScaleMemory, Error, TEXT("Unable to load component object while reading chunk"))
	}

	bool bIsStaticComponent = !!Bunch.ReadBit();
	TArray<FHScaleOuterChunk> SubObjClazzInfo;

	if (!bIsStaticComponent) // dynamic components handling here
	{
		Bunch.ReadBit(); // destroy message

		FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Bunch, SubObjClazzInfo, PkgMap);

		if (SubObjClazzInfo.IsEmpty())
		{
			UE_LOG(Log_HyperScaleMemory, Error, TEXT("Found empty clazz info for dynamic sub-object for entity %llu for subobject name %s"), EntityId.Get(), *Object->GetName())
			return;
		}

		bool bActorIsOuter = !!Bunch.ReadBit();

		if (!bActorIsOuter) // chain of sub-objects handling
		{
			// outers handling for owner is handled differently through OwnerId
			// we only read data from Archive and ignore it right now
			TArray<FHScaleOuterChunk> OuterChunks;
			FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Bunch, OuterChunks, PkgMap);
		}
	}

	TSharedPtr<FHScaleNetworkEntity> EntityPtr = GetBibliothec()->FetchEntity(CompHSGUID);

	const FHScaleNetGUID OwnerEntityId = FetchComponentOwner(Object);
	EntityPtr->UpdateOwner(OwnerEntityId.IsObject() ? OwnerEntityId : EntityId);

	EntityPtr->Flags |= EHScaleEntityFlags::IsComponent;
	EntityPtr->Flags |= bIsStaticComponent ? EHScaleEntityFlags::IsStaticComponent : EntityPtr->Flags;

	if (bIsStaticComponent)
	{
		// for static components only object path is required
		constexpr uint16 PropertyId = HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID;
		HScaleTypes::FHScaleObjectDataProperty* Property =
			CastPty<HScaleTypes::FHScaleObjectDataProperty>(EntityPtr->FetchNonApplicationProperty(PropertyId));
		const FHScaleNetGUID HSNetGUID;
		if (!Property->NetworkGUID.IsValid() && Property->SerializeChunks(ObjectChunks, PropertyId, NetGUID, HSNetGUID))
		{
			EntityPtr->AddLocalDirtyProperty(PropertyId);
			EntityPtr->Flags |= EHScaleEntityFlags::HasObjectPath;
		}
	}
	// for dynamic components, both object path and class path needs to be serialized
	else
	{
		// we will store classpath in archetype range
		constexpr uint16 ArchetypePropertyId = HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID;
		HScaleTypes::FHScaleObjectDataProperty* ArchetypeProperty =
			CastPty<HScaleTypes::FHScaleObjectDataProperty>(EntityPtr->FetchNonApplicationProperty(ArchetypePropertyId));
		FNetworkGUID ArchetypeNetGUID = SubObjClazzInfo[SubObjClazzInfo.Num() - 1].NetGUID;
		const FHScaleNetGUID HSNetGUID;
		if (!ArchetypeProperty->NetworkGUID.IsValid() && ArchetypeProperty->SerializeChunks(SubObjClazzInfo, ArchetypePropertyId, ArchetypeNetGUID, HSNetGUID))
		{
			EntityPtr->AddLocalDirtyProperty(ArchetypePropertyId);
			EntityPtr->Flags |= EHScaleEntityFlags::HasArchetypeData;
		}
	}

	uint32 PayloadLength = 0;
	Bunch.SerializeIntPacked(PayloadLength); // check bit compatibility with FArchive::SerializeIntPacked

	// could be empty payload signifying just to spawn data
	if (PayloadLength > 0)
	{
		const bool bEnablePropertyChecksum = !!Bunch.ReadBit();

		// checksum checks are not supported right now, ignoring
		check(!bEnablePropertyChecksum)
		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Reading properties of component EntityId %llu"), *Object->GetName(), EntityPtr->EntityId.Get())

		const TSharedRef<FObjectReplicator>* ReplicatorPtr = Channel->FindObjectReplicator(Object);
		check(ReplicatorPtr != nullptr)

		const TSharedRef<FObjectReplicator>& ObjectReplicator = *ReplicatorPtr;

		EntityPtr->ReadProperties_R(Bunch, ObjectReplicator);
	}

	EntityPtr->PostReadSerializedData();
	EntityPtr->MarkEntityLocalDirty();

	if (Bunch.AtEnd()) { return; }
	// if there is still data to read in bunch, it should be replication data of next component
	bool bHasRepLayout = !!Bunch.ReadBit();
	bool bIsActor = !!Bunch.ReadBit();

	check(!bIsActor) // actors can't be sub-objects/components, they have their own channel

	if (bHasRepLayout) { ReadComponentsData(Bunch, Channel); }
	else { ReadSubObjectDeleteUpdate(Bunch, Channel); }
}

void FHScaleNetworkEntity::CheckAndUpdateOwner(const TArray<FRepParentCmd>& ParentCmds, const FRepLayoutCmd& RepCmd, FHScaleProperty* Property)
{
	const uint16 ParentIndex = RepCmd.ParentIndex;
	const FRepParentCmd Parent = ParentCmds[ParentIndex];

	const FString ParentPropertyName = Parent.Property->GetName();
	const bool bIsChildProperty = ParentPropertyName != RepCmd.Property->GetName();
	if (RepCmd.Property->GetName() == TEXT("Owner") && !bIsChildProperty && FHScaleStatics::IsObjectDataRepCmd(RepCmd))
	{
		const HScaleTypes::FHScaleObjectDataProperty* R_OwnerProperty = CastPty<HScaleTypes::FHScaleObjectDataProperty>(Property);
		check(R_OwnerProperty)
		if (R_OwnerProperty->HScaleNetGUID.IsObject())
		{
			UpdateOwner(R_OwnerProperty->HScaleNetGUID);
		}
	}
}

void FHScaleNetworkEntity::ReadProperties_R(FBitReader& Bunch, const TSharedPtr<FObjectReplicator>& ActorReplicator)
{
	check(ActorReplicator.IsValid())
	// payload contains series of property handles(packed int) and property data
	const TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, ActorReplicator->RepLayout.Get(), Cmds);
	const TArray<FRepParentCmd>& ParentCmds = HSCALE_GET_PRIVATE(FRepLayout, ActorReplicator->RepLayout.Get(), Parents);

	uint32 PropertyHandle;
	Bunch.SerializeIntPacked(PropertyHandle);
	FHScaleRepCmdIterator CmdIterator(&Cmds);
	while (0 != PropertyHandle) // if we reach end, property handle would be 0
	{
		// Property handles are 16bit unsigned values, they are packed as 32 bit
		const uint16 PropertyHandle_R = PropertyHandle;
		const uint16 CmdIndex = CmdIterator.FetchCmdIndex(PropertyHandle_R);
		if (!ensure(Cmds.Num() > CmdIndex)) { return; }
		const FRepLayoutCmd& RepCmd = Cmds[CmdIndex];

		const uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(PropertyHandle);
		FHScaleProperty* Property = FetchApplicationProperty(PropertyId, RepCmd);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("ReadProperties_R:: _Before_ EntityId:%llu PropertyHandle %d Cmd Type %d  Name: %s Value: %s  Pos Bits: %llu  NumBits: %llu"),
			EntityId.Get(), PropertyHandle, (uint8)RepCmd.Type, *RepCmd.Property->GetName(), *Property->ToDebugString(), Bunch.GetPosBits(), Bunch.GetNumBits())

		bool bIsChanged;
		// Object ptrs are handled separately
		if (FHScaleStatics::IsObjectDataRepCmd(RepCmd))
		{
			bIsChanged = SerializeObjectFromBunch(Bunch, Property, PropertyId);
		}
		else if (RepCmd.Type == ERepLayoutCmdType::DynamicArray)
		{
			bIsChanged = Dyn_ReadDynamicArray(Bunch, Property, PropertyId, CmdIterator);
		}
		else if (RepCmd.Type == ERepLayoutCmdType::PropertySoftObject)
		{
			bIsChanged = ReadSoftObjectFromBunch(Bunch, Property, PropertyId);
		}
		else
		{
			bIsChanged = Property->SerializeUE(Bunch, RepCmd);
		}

		if (bIsChanged) { AddLocalDirtyProperty(PropertyId); }

		// see if owner is changed, and update owner
		CheckAndUpdateOwner(ParentCmds, RepCmd, Property);

		// read next property handle
		Bunch.SerializeIntPacked(PropertyHandle);
		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("ReadProperties_R:: _After_ EntityId:%llu PropertyHandle %d Cmd Type %d  Name: %s Value: %s  Pos Bits: %llu  NumBits: %llu"),
			EntityId.Get(), PropertyHandle, (uint8)RepCmd.Type, *RepCmd.Property->GetName(), *Property->ToDebugString(), Bunch.GetPosBits(), Bunch.GetNumBits())
	}
}

void FHScaleNetworkEntity::OnFlagsUpdate() const
{
	GetBibliothec()->AddFlagsEntity(EntityId, Flags);
}

void FHScaleNetworkEntity::CheckAndMarkFlagsDirty()
{
	HScaleTypes::FHScaleUInt16Property* FlagsProp = CastPty<HScaleTypes::FHScaleUInt16Property>
		(FetchNonApplicationProperty(HS_RESERVED_OBJECT_FLAGS_ATTRIBUTE_ID));
	uint16 PrevValue = FlagsProp->GetValue();
	if (PrevValue != Flags)
	{
		FlagsProp->SetValue(Flags);
		AddLocalDirtyProperty(HS_RESERVED_OBJECT_FLAGS_ATTRIBUTE_ID);
		OnFlagsUpdate();
	}
}

void FHScaleNetworkEntity::PostReadSerializedData()
{
	CheckAndMarkFlagsDirty();
	CheckAndReviseStates();
}

void FHScaleNetworkEntity::Push(FHScaleInBunch& Bunch)
{
	// Only actor channels are considered for replication. Control and Voice are not currently supported
	if (Bunch.ChName != NAME_Actor) { return; }
	checkf(Bunch.Channel != nullptr, TEXT("Found null channel pointer in outbunch"));
	UHScaleActorChannel* Channel = StaticCast<UHScaleActorChannel*>(Bunch.Channel);
	check(Channel->Actor)

	Clazz = Channel->Actor->GetClass();
	ReadOuterData(Bunch, Channel);
	ReadArchetypeData(Bunch, Channel);

	// #todo: Dummy level id data, later on store this value in vec4d of position
	FHScaleNetGUID LevelId = FHScaleNetGUID::GetDefault();
	Bunch << LevelId;

	HScaleTypes::FHScalePositionSystemProperty* PosProperty =
		CastPty<HScaleTypes::FHScalePositionSystemProperty>(FetchNonApplicationProperty(QUARK_KNOWN_ATTRIBUTE_POSITION));

	if (PosProperty->SerializePosition(Bunch))
	{
		AddLocalDirtyProperty(QUARK_KNOWN_ATTRIBUTE_POSITION);
	}

	if (Bunch.AtEnd()) { return; }

	// Rep Properties related serialization
	// first 2 bits about type of rep data
	bool bHasRepLayout = !!Bunch.ReadBit();
	bool bIsActor = !!Bunch.ReadBit();

	if (bIsActor)
	{
		Flags |= EHScaleEntityFlags::IsActor;

		uint32 PayloadLength = 0;
		Bunch.SerializeIntPacked(PayloadLength); // check bit compatibility with FArchive::SerializeIntPacked

		if (!ensure(PayloadLength > 0)) { return; }

		const bool bEnablePropertyChecksum = !!Bunch.ReadBit();

		// checksum checks are not supported right now, ignoring
		check(!bEnablePropertyChecksum)

		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Reading properties of object Name %s"), *Channel->Actor->GetName())
		ReadProperties_R(Bunch, Channel->ActorReplicator);
	}
	else
	{
		// For subobject data, for properties update bHasReplayout is always true
		// for deletion of subobject, bHasReplayout is always false
		if (bHasRepLayout) { ReadComponentsData(Bunch, Channel); }
		else { ReadSubObjectDeleteUpdate(Bunch, Channel); }
	}

	if (!Bunch.AtEnd() && bIsActor)
	{
		bHasRepLayout = !!Bunch.ReadBit();
		bIsActor = !!Bunch.ReadBit();

		check(!bIsActor)
		if (bHasRepLayout) { ReadComponentsData(Bunch, Channel); }
		else { ReadSubObjectDeleteUpdate(Bunch, Channel); }
	}

	CheckAndDoFallbackOwnerUpdate(Channel);

	// compare with initial flags, and if changed mark flags data for network send
	PostReadSerializedData();
}


void FHScaleNetworkEntity::Pull(TArray<FHScaleAttributesUpdate>& Attributes)
{
	for (const uint16 PropertyId : LocalDirtyProps)
	{
		if (!Properties.contains(PropertyId)) { continue; }
		FHScaleProperty* Property = FindExistingProperty(PropertyId);
		Property->Serialize(Attributes, PropertyId);
	}
}

bool FHScaleNetworkEntity::PullUpdate(FInBunch& Bunch)
{
	const TMap<FHScaleNetGUID, TSet<uint16>> EmptySet;
	return PullData(Bunch, false, EmptySet);
}

bool FHScaleNetworkEntity::PullEntire(FInBunch& Bunch)
{
	const TMap<FHScaleNetGUID, TSet<uint16>> EmptySet;
	return PullData(Bunch, true, EmptySet);
}

bool FHScaleNetworkEntity::PullUpdate(FInBunch& Bunch, const TMap<FHScaleNetGUID, TSet<uint16>>& AttrToPull)
{
	return PullData(Bunch, false, AttrToPull);
}