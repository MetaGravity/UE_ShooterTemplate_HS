// Copyright 2024 Metagravity. All Rights Reserved.


#include "Debug/HScaleDebugLib.h"

#include "Engine/ActorChannel.h"
#include "MemoryLayer/HScalePropertyIdConverters.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "Utils/HScalePrivateAccessors.h"

DEFINE_LOG_CATEGORY(LogHScaleDebugLib)

static FString ENetworkEntityStateToString(const ENetworkEntityState State)
{
	switch (State)
	{
		case ENetworkEntityState::NotInitialized:
			return TEXT("NotInitialized");
		case ENetworkEntityState::PendingInitialization:
			return TEXT("PendingInitialization");
		case ENetworkEntityState::Initialized:
			return TEXT("Initialized");
		default:
			return TEXT("Unknown");
	}
}

static FString EHScaleMemoryAttributeTypeToString(const EHScaleMemoryTypeId Type)
{
	switch (Type)
	{
		case EHScaleMemoryTypeId::None:
			return TEXT("None");
		case EHScaleMemoryTypeId::Boolean:
			return TEXT("Boolean");
		case EHScaleMemoryTypeId::Uint8:
			return TEXT("Uint8");
		case EHScaleMemoryTypeId::Uint16:
			return TEXT("Uint16");
		case EHScaleMemoryTypeId::Uint32:
			return TEXT("Uint32");
		case EHScaleMemoryTypeId::Uint64:
			return TEXT("Uint64");
		case EHScaleMemoryTypeId::Int8:
			return TEXT("Int8");
		case EHScaleMemoryTypeId::Int16:
			return TEXT("Int16");
		case EHScaleMemoryTypeId::Int32:
			return TEXT("Int32");
		case EHScaleMemoryTypeId::Int64:
			return TEXT("Int64");
		case EHScaleMemoryTypeId::Float32:
			return TEXT("Float32");
		case EHScaleMemoryTypeId::Float64:
			return TEXT("Float64");
		case EHScaleMemoryTypeId::String:
			return TEXT("String");
		case EHScaleMemoryTypeId::Bytes:
			return TEXT("Bytes");
		case EHScaleMemoryTypeId::Vec2:
			return TEXT("Vec2");
		case EHScaleMemoryTypeId::Vec3:
			return TEXT("Vec3");
		case EHScaleMemoryTypeId::Vec2d:
			return TEXT("Vec2d");
		case EHScaleMemoryTypeId::Vec3d:
			return TEXT("Vec3d");
		case EHScaleMemoryTypeId::Vec4:
			return TEXT("Vec4");
		case EHScaleMemoryTypeId::Vec4d:
			return TEXT("Vec4d");
		case EHScaleMemoryTypeId::Undefined:
			return TEXT("Undefined");
		case EHScaleMemoryTypeId::SystemPosition:
			return TEXT("FVector3");
		case EHScaleMemoryTypeId::SplitString:
			return TEXT("FString");
		case EHScaleMemoryTypeId::SplitByte:
			return TEXT("SplitByte");
		default:
			return TEXT("Unknown");
	}
}

TSharedPtr<FRepLayout> UHScaleDebugLib::GetRepLayoutForObject(UObject* Object, UHScaleNetDriver* NetDriver)
{
	AActor* Actor = Cast<AActor>(Object);
	if (!Actor) return nullptr;
	UNetConnection* NetConnection = NetDriver->GetHyperScaleConnection();
	if (!NetConnection)
	{
		UE_LOG(LogHScaleDebugLib, Warning, TEXT("NetConnection is null"));
		return nullptr;
	}

	UActorChannel* ActorChannel = nullptr;
	// Iterate over all open channels to find the actor channel
	for (UChannel* Channel : NetConnection->OpenChannels)
	{
		UActorChannel* Ach = Cast<UActorChannel>(Channel);
		if (Ach && Ach->Actor == Actor)
		{
			ActorChannel = Ach;
			break;
		}
	}

	if (!ActorChannel)
	{
		UE_LOG(LogHScaleDebugLib, Warning, TEXT("No actor channel found for actor"))
		return nullptr;
	}
	TSharedPtr<FRepLayout> RepLayout = ActorChannel->ActorReplicator->RepLayout;
	return RepLayout;
}

TSharedPtr<FJsonObject> UHScaleDebugLib::GetEquivalentEntityProps(UObject* Object)
{
	if (!Object)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("Received null pointe for entity lookup"))
		return nullptr;
	}

	const UWorld* World = Object->GetWorld();
	if (!World)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("No associated world found for given object"))
		return nullptr;
	}

	UNetDriver* NamedNetDriver = World->GetNetDriver();
	if (!NamedNetDriver)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("No net driver found associated with the world"))
		return nullptr;
	}
	UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(NamedNetDriver);
	if (!NetDriver)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("HScale net driver not found"))
		return nullptr;
	}

	const UHScaleConnection* HyperScaleConnection = NetDriver->GetHyperScaleConnection();
	if (!HyperScaleConnection)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("No active Net Connection with HScale"))
		return nullptr;
	}

	const TObjectPtr<UHScalePackageMap> PackageMap = Cast<UHScalePackageMap>(HyperScaleConnection->PackageMap);
	if (!PackageMap)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("HScale Package Map not found"))
		return nullptr;
	}

	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();
	if (!Bibliothec)
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("Bibliothec does not exists"))
		return nullptr;
	}

	const FHScaleNetGUID GUID = PackageMap->FindEntityNetGUID(Object);
	if (!GUID.IsValid() || !Bibliothec->IsEntityExists(GUID))
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("Entity does not exist in memory layer"))
		return nullptr;
	}

	const TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(GUID);
	if (!Entity.IsValid())
	{
		UE_LOG(LogHScaleDebugLib, Error, TEXT("Entity in memory layer is not valid"))
		return nullptr;
	}
	
	TSharedPtr<FRepLayout> RepLayout = GetRepLayoutForObject(Object, NetDriver);
	if (!RepLayout.IsValid()) return nullptr;

	TSharedPtr<FJsonObject> RootJson = MakeShareable(new FJsonObject());
	UClass* Clazz = Entity->FetchEntityUClass();

	RootJson->SetField("entityId", MakeShareable(new FJsonValueNumber(Entity->EntityId.Get())));
	RootJson->SetField("class", MakeShareable(new FJsonValueString(Clazz ? Clazz->GetName() : "NULL")));
	RootJson->SetField("ownerId", MakeShareable(new FJsonValueNumber(Entity->Owner.Get())));
	RootJson->SetField("state", MakeShareable(new FJsonValueString(ENetworkEntityStateToString(Entity->EntityState))));

	TSharedPtr<FJsonObject> ObjectJsonProperties = MakeShareable(new FJsonObject());
	RootJson->SetObjectField("Properties", ObjectJsonProperties);

	for (auto It = Entity->CreatePropertiesConstIterator(); !It.IsEnd(); ++It)
	{
		const uint16 PropertyId = *It;
		FHScaleProperty* HScaleProperty = Entity->FindExistingProperty(PropertyId);
		if (!HScaleProperty) { continue; }

		TSharedPtr<FJsonObject> JsonProp = MakeShareable(new FJsonObject);

		JsonProp->SetStringField("type", EHScaleMemoryAttributeTypeToString(HScaleProperty->GetType()));
		JsonProp->SetStringField("mem_value", HScaleProperty->ToDebugString());

		ObjectJsonProperties->SetObjectField(FString::FromInt(PropertyId), JsonProp);

		if (FHScalePropertyIdConverters::IsReservedProperty(PropertyId) || FHScalePropertyIdConverters::IsSystemProperty(PropertyId))
		{
			if (HScaleProperty->GetType() == EHScaleMemoryTypeId::SystemPosition)
			{
				JsonProp->SetStringField("name", TEXT("Server position"));
			}
			else if (HScaleProperty->GetType() == EHScaleMemoryTypeId::SplitString)
			{
				JsonProp->SetStringField("name", TEXT("Class/Instance name"));
			}

			JsonProp->SetStringField("inst_value", HScaleProperty->ToDebugString());
			continue;
		}

		TArray<FRepLayoutCmd>& Cmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Cmds);
		TArray<FRepParentCmd>& ParentCmds = HSCALE_GET_PRIVATE(FRepLayout, RepLayout.Get(), Parents);

		uint16 PropertyHandle = FHScalePropertyIdConverters::GetPropertyHandleFromPropertyId(PropertyId);
		if (Cmds.Num() < PropertyHandle) continue;
		const FRepLayoutCmd RepCmd = Cmds[PropertyHandle - 1];

		FProperty* Property = RepCmd.Property;

		FString PropertyName = Property->GetName();

		uint16 ParentIndex = RepCmd.ParentIndex;
		const FRepParentCmd Parent = ParentCmds[ParentIndex];

		const FString ParentPropertyName = Parent.Property->GetName();
		const bool bIsChildProperty = ParentPropertyName != PropertyName;

		FString InstValue;
		void* InStructMemoryPtr = bIsChildProperty ? Parent.Property->ContainerPtrToValuePtr<void>(Object) : Object;
		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			UObject* TargetObject = ObjectProperty->GetObjectPropertyValue_InContainer(InStructMemoryPtr);
			FNetworkGUID NetGUIDFromObject = PackageMap->GetNetGUIDFromObject(TargetObject);
			JsonProp->SetStringField("inst_value", FString::Printf(TEXT("%s"), *NetGUIDFromObject.ToString()));
			JsonProp->SetStringField("type", TEXT("Object"));
		}
		else
		{
			Property->ExportTextItem_InContainer(InstValue, InStructMemoryPtr, nullptr, nullptr, PPF_ConsoleVariable | PPF_PropertyWindow);
			JsonProp->SetStringField("inst_value", InstValue);
		}

		if (bIsChildProperty)
			JsonProp->SetStringField("parent", ParentPropertyName);

		JsonProp->SetStringField("name", PropertyName);
	}

	return RootJson;
}