#include "MemoryLayer/HScaleNetworkEntity.h"

#include "MemoryLayer/HScaleNetworkBibliothec.h"
#include "NetworkLayer/HScaleConnection.h"
#include "Utils/HScaleStatics.h"

bool FHScaleNetworkEntity::Dyn_UpdateArraySize(uint16 NewArraySize)
{
	if (NewArraySize == Dyn_GetArraySize()) return false; // nothing changed no need to proceed

	HScaleTypes::FHScaleUInt16Property* ArrSizeProperty = CastPty<HScaleTypes::FHScaleUInt16Property>(FetchNonApplicationProperty(HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID));
	ArrSizeProperty->SetValue(NewArraySize);
	AddLocalDirtyProperty(HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID);
	Dyn_EnforceArraySize();
	return true;
}

void FHScaleNetworkEntity::Dyn_EnforceArraySize()
{
	uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(Dyn_GetArraySize());
	while (Properties.contains(PropertyId))
	{
		DeleteProperty(PropertyId);
		PropertyId++;
	}
}

uint16 FHScaleNetworkEntity::Dyn_GetArraySize() const
{
	if (!Properties.contains(HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID)) return 0;
	
	HScaleTypes::FHScaleUInt16Property* LinkProperty = CastPty<HScaleTypes::FHScaleUInt16Property>(FindExistingProperty(HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID));
	return LinkProperty->GetValue();
}

bool FHScaleNetworkEntity::Dyn_IsHeadersValid() const
{
	if (!Properties.contains(HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID)) return false;

	return true;
}

bool FHScaleNetworkEntity::Dyn_ReadUEArrayData(FBitReader& Reader, const FRepLayoutCmd& Cmd)
{
	uint16 ArraySize_R;
	Reader << ArraySize_R;
	bool bAnyChanged = Dyn_UpdateArraySize(ArraySize_R);

	uint32 Handle = 0;
	Reader.SerializeIntPacked(Handle);
	while (Handle)
	{
		const uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(Handle);
		FHScaleProperty* Property = FetchApplicationProperty(PropertyId, Cmd);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Dyn_ReadUEArrayData:: _Before_ EntityId:%llu PropertyHandle %d Cmd Type %d  Name: %s Value: %s  Pos Bits: %llu  NumBits: %llu"),
			EntityId.Get(), Handle, (uint8)Cmd.Type, *Cmd.Property->GetName(), *Property->ToDebugString(), Reader.GetPosBits(), Reader.GetNumBits())

		bool bIsChanged;
		// Object ptrs are handled separately
		if (FHScaleStatics::IsObjectDataRepCmd(Cmd))
		{
			bIsChanged = SerializeObjectFromBunch(Reader, Property, PropertyId);
		}
		else if (Cmd.Type == ERepLayoutCmdType::PropertySoftObject)
		{
			bIsChanged = ReadSoftObjectFromBunch(Reader, Property, PropertyId);
		}
		else
		{
			bIsChanged = Property->SerializeUE(Reader, Cmd);
		}

		if (bIsChanged) { AddLocalDirtyProperty(PropertyId); }

		Reader.SerializeIntPacked(Handle);
		bAnyChanged |= bIsChanged;

		UE_LOG(Log_HyperScaleMemory, Verbose, TEXT("Dyn_ReadUEArrayData:: _After_ EntityId:%llu PropertyHandle %d Cmd Type %d  Name: %s Value: %s  Pos Bits: %llu  NumBits: %llu"),
			EntityId.Get(), Handle, (uint8)Cmd.Type, *Cmd.Property->GetName(), *Property->ToDebugString(), Reader.GetPosBits(), Reader.GetNumBits())
	}
	return bAnyChanged;
}

bool FHScaleNetworkEntity::Dyn_IsValidForWrite() const
{
	const uint16 ArraySize = Dyn_GetArraySize();
	// #TODO: use timestamps to ensure atomicity 
	for (uint16 i = 0; i < ArraySize; i++)
	{
		const uint32 Handle = i + 1;
		const uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(Handle);
		if (!Properties.contains(PropertyId)) return false;
	}

	return true;
}

void FHScaleNetworkEntity::Dyn_WriteUEArrayData(FBitWriter& Writer, const FRepLayoutCmd& Cmd)
{
	uint16 ArraySize = Dyn_GetArraySize();
	
	Writer << ArraySize;

	for (uint16 i = 0; i < ArraySize; i++)
	{
		uint32 Handle = i + 1;
		const uint16 PropertyId = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(Handle);
		FHScaleProperty* Property = FetchApplicationProperty(PropertyId, Cmd);
		UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("Dyn_WriteUEArrayData:: _Before_ EntityId:%llu PropertyHandle %d Cmd Type %d  Name: %s Value: %s NumBits: %llu"),
			EntityId.Get(), Handle, (uint8)Cmd.Type, *Cmd.Property->GetName(), *Property->ToDebugString(), Writer.GetNumBits())

		Writer.SerializeIntPacked(Handle);
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

	uint32 Zero = 0;
	Writer.SerializeIntPacked(Zero); // signifies the end of array serialization

}

void FHScaleNetworkEntity::Dyn_MarkEntityServerDirty()
{
	FHScaleNetworkEntity* OwnerPtr = GetChannelOwner();
	if (!OwnerPtr) return;

	const HScaleTypes::FHScaleUInt16Property* LinkProperty = CastPty<HScaleTypes::FHScaleUInt16Property>(FetchNonApplicationProperty(HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID));
	uint16 PropertyId = LinkProperty->GetValue();
	if (!FHScalePropertyIdConverters::IsApplicationProperty(PropertyId)) return;
	OwnerPtr->AddServerDirtyProperty(PropertyId);

	OwnerPtr->SubStructEntities.Add(PropertyId, EntityId);
	GetBibliothec()->AddServerDirty(OwnerPtr);
}


void FHScaleNetworkEntity::Dyn_Init(const FHScaleNetGUID& OwnerNetGUID, const uint16 LinkPropertyId)
{
	Flags |= EHScaleEntityFlags::IsDataHoldingStruct;
	UpdateClassId(HSCALE_DYNAMIC_ARRAY_CLASS_ID);
	UpdateOwner(OwnerNetGUID);
	CheckAndMarkFlagsDirty();

	HScaleTypes::FHScaleUInt16Property* LinkProperty = CastPty<HScaleTypes::FHScaleUInt16Property>(FetchNonApplicationProperty(HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID));
	LinkProperty->SetValue(LinkPropertyId);
	AddLocalDirtyProperty(HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID);

	CheckAndReviseStates();
}


bool FHScaleNetworkEntity::Dyn_ReadDynamicArray(FBitReader& Bunch, FHScaleProperty*& Property, const uint16 PropertyId, FHScaleRepCmdIterator& CmdIterator)
{
	FHScaleNetworkEntity* DynEntity = Dyn_FetchSubStructDynArrayEntity(PropertyId);
	if (!DynEntity) return false;

	const uint16 ElementIndex = CmdIterator.FetchNextIndex();
	const FRepLayoutCmd& LayoutCmd = (*CmdIterator.Cmds)[ElementIndex];
	const bool bIsChanged = DynEntity->Dyn_ReadUEArrayData(Bunch, LayoutCmd);
	DynEntity->CheckAndReviseStates();

	if (bIsChanged)
	{
		DynEntity->MarkEntityLocalDirty();
	}

	if (!Property->IsA<HScaleTypes::FHScaleUInt64Property>())
	{
		Property = SwitchPropertyWithNewType(PropertyId, EHScaleMemoryTypeId::Uint64);
	}
	HScaleTypes::FHScaleUInt64Property* LinkProperty = CastPty<HScaleTypes::FHScaleUInt64Property>(Property);
	const uint64 PrevValue = LinkProperty->GetValue();
	LinkProperty->SetValue(DynEntity->EntityId.Get());
	return PrevValue != LinkProperty->GetValue();
}

FHScaleNetworkEntity* FHScaleNetworkEntity::Dyn_FetchSubStructDynArrayEntity(uint16 PropertyId)
{
	FHScaleNetworkEntity* DynEntityPtr = Dyn_FindExistingDynArrayEntity(PropertyId);
	if (DynEntityPtr) return DynEntityPtr;

	const FHScaleNetGUID NewNetGUID = GetNetConnection()->FetchNextAvailableDynamicEntityId();
	if (!NewNetGUID.IsValid())
		return nullptr;	//There was no available ID yet

	TSharedPtr<FHScaleNetworkEntity> DynEntity = GetBibliothec()->FetchEntity(NewNetGUID);
	DynEntityPtr = DynEntity.Get();
	DynEntityPtr->Dyn_Init(EntityId, PropertyId);
	SubStructEntities.Add(PropertyId, NewNetGUID);
	return DynEntityPtr;
}


FHScaleNetworkEntity* FHScaleNetworkEntity::Dyn_FindExistingDynArrayEntity(const uint16 PropertyId) const
{
	if (!SubStructEntities.Contains(PropertyId)) return nullptr;

	FHScaleNetGUID StructNetGUID = SubStructEntities[PropertyId];
	if (!StructNetGUID.IsObject()) return nullptr;

	if (!GetBibliothec()->IsEntityExists(StructNetGUID))return nullptr;

	const TSharedPtr<FHScaleNetworkEntity> SubStructEntity = GetBibliothec()->FetchEntity(StructNetGUID);
	if (!SubStructEntity.IsValid() || !SubStructEntity->IsDynArrayEntity()) return nullptr;
	return SubStructEntity.Get();
}

bool FHScaleNetworkEntity::Dyn_IsDynamicArrayReadyForWriteOut(const uint16 PropertyId) const
{
	const FHScaleNetworkEntity* SubStructPtr = Dyn_FindExistingDynArrayEntity(PropertyId);
	if (!SubStructPtr || !SubStructPtr->Dyn_IsValidForWrite()) return false;
	return true;
}

void FHScaleNetworkEntity::Dyn_WriteOutDynArrayData(FBitWriter& Writer, uint16 PropertyId, FHScaleRepCmdIterator& CmdIterator) const
{
	FHScaleNetworkEntity* DynEntity = Dyn_FindExistingDynArrayEntity(PropertyId);
	check(DynEntity)
	const uint16 ElementIndex = CmdIterator.FetchNextIndex();
	const FRepLayoutCmd& LayoutCmd = (*CmdIterator.Cmds)[ElementIndex];
	DynEntity->Dyn_WriteUEArrayData(Writer, LayoutCmd);
}
