#include "Events/HScaleEventsSerializer.h"

#include "Core/HScaleCommons.h"
#include "Net/RepLayout.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "Utils/HScaleObjectSerializationHelpers.h"
#include "Utils/HScaleStatics.h"

bool FHScaleEventsSerializer::Serialize(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap)
{
	switch (Cmd.Type)
	{
		case ERepLayoutCmdType::PropertyBool:
		case ERepLayoutCmdType::PropertyNativeBool:
			return SerializeBoolProperty(Writer, Reader);

		case ERepLayoutCmdType::PropertyFloat:
			return SerializeDataType<float>(Writer, Reader);

		case ERepLayoutCmdType::PropertyInt:
			return SerializeDataType<int32>(Writer, Reader);

		case ERepLayoutCmdType::PropertyByte:
			return SerializeByteProperty(Writer, Reader, Cmd, PkgMap);

		case ERepLayoutCmdType::PropertyUInt32:
			return SerializeDataType<uint32>(Writer, Reader);

		case ERepLayoutCmdType::PropertyUInt64:
			return SerializeDataType<uint64>(Writer, Reader);

		case ERepLayoutCmdType::PropertyString:
			return SerializeDataType<FString>(Writer, Reader);

		case ERepLayoutCmdType::PropertyName:
			return SerializeDataType<FName>(Writer, Reader);

		case ERepLayoutCmdType::Property:
		{
			if (Cmd.Property->IsA(FDoubleProperty::StaticClass()))
			{
				return SerializeDataType<double>(Writer, Reader);
			}

			if (Cmd.Property->IsA(FInt64Property::StaticClass()))
			{
				return SerializeDataType<int64>(Writer, Reader);
			}

			if (Cmd.Property->IsA(FUInt16Property::StaticClass()))
			{
				return SerializeDataType<uint16>(Writer, Reader);
			}

			if (Cmd.Property->IsA(FInt16Property::StaticClass()))
			{
				return SerializeDataType<int16>(Writer, Reader);
			}

			if (Cmd.Property->IsA(FTextProperty::StaticClass()))
			{
				return SerializeDataType<FText>(Writer, Reader);
			}
			if (Cmd.Property->IsA(FStructProperty::StaticClass()))
			{
				FStructProperty* StructProp = CastField<FStructProperty>(Cmd.Property);
				if (StructProp->Struct->StructFlags & STRUCT_NetSerializeNative)
				{
					return SerializeNetSerializeStruct(Reader, Writer, Cmd, PkgMap);
				}
			}
		}
		case ERepLayoutCmdType::RepMovement:
			return SerializeNetSerializeTypes<FRepMovement>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyVector:
			return SerializeNetSerializeTypes<FVector>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyRotator:
			return SerializeNetSerializeTypes<FRotator>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyVector100:
			return SerializeNetSerializeTypes<FVector_NetQuantize100>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyVector10:
			return SerializeNetSerializeTypes<FVector_NetQuantize10>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyVectorNormal:
			return SerializeNetSerializeTypes<FVector_NetQuantizeNormal>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyVectorQ:
			return SerializeNetSerializeTypes<FVector_NetQuantize>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyPlane:
			return SerializeNetSerializeTypes<FPlane>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyNetId:
			return SerializeNetSerializeTypes<FUniqueNetIdRepl>(Writer, Reader, PkgMap);

		case ERepLayoutCmdType::PropertyObject:
		case ERepLayoutCmdType::PropertyInterface:
		case ERepLayoutCmdType::PropertyWeakObject:
		case ERepLayoutCmdType::PropertySoftObject:
			UE_LOG(Log_HyperScaleEvents, Warning, TEXT("Object Ptrs should be handled separately"))
			return false;

		case ERepLayoutCmdType::DynamicArray:
			UE_LOG(Log_HyperScaleEvents, Warning, TEXT("Dynamic arrays should be handled separately"))
			return false;

		case ERepLayoutCmdType::NetSerializeStructWithObjectReferences:
		case ERepLayoutCmdType::Return:
		default:
			UE_LOG(Log_HyperScaleEvents, Warning, TEXT("Received not yet implemented serialization type"));
			return false;
	}
}

bool FHScaleEventsSerializer::SerializeDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const FRepLayoutCmd& Cmd, const bool bOnSend)
{
	if (EnumHasAnyFlags(Cmd.Flags, ERepLayoutCmdFlags::IsEmptyArrayStruct))
	{
		return false;
	}

	uint16 OutArrayNum;
	Reader << OutArrayNum;
	Writer << OutArrayNum;

	bool bResult = true;
	for (int32 i = 0; i < OutArrayNum && !Writer.IsError() && !Reader.IsError(); i++)
	{
		if (FHScaleStatics::IsObjectDataRepCmd(Cmd))
		{
			if (bOnSend) { bResult &= ReadObjectPtr(Writer, Reader, PkgMap); }
			else { bResult &= WriteObjectPtr(Writer, Reader, PkgMap); }
			continue;
		}
		if (Cmd.Type == ERepLayoutCmdType::PropertySoftObject)
		{
			if (bOnSend) { bResult &= ReadSoftObjectPtr(Writer, Reader, Cmd, PkgMap); }
			else { bResult &= WriteSoftObjectPtr(Writer, Reader, Cmd, PkgMap); }
			continue;
		}
		bResult &= Serialize(Writer, Reader, Cmd, PkgMap);
	}

	return bResult;
}

bool FHScaleEventsSerializer::ReadObjectPtr(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap)
{
	TArray<FHScaleOuterChunk> OuterChunks;
	FHScaleObjectSerializationHelper::ReadOuterChunkFromBunch(Reader, OuterChunks, PkgMap);

	if (OuterChunks.IsEmpty()) { return false; }
	const FNetworkGUID NetGUID = OuterChunks[OuterChunks.Num() - 1].NetGUID;

	UObject* Object = PkgMap->GetObjectFromNetGUID(NetGUID, true);
	if (!Object) // this would be a null pointer transfer
	{
		Writer.WriteBit(0); // signifying its a NetGUID
		FHScaleNetGUID GUID;
		Writer << GUID;
	}
	else if (Object->IsFullNameStableForNetworking())
	{
		Writer.WriteBit(1); // signifying contains full name
		FString FullName = Object->GetFullName();
		Writer << FullName;
	}
	else
	{
		FHScaleNetGUID HScaleNetGUID = PkgMap->FindEntityNetGUID(NetGUID);
		if (!HScaleNetGUID.IsObject()) { return false; }

		Writer.WriteBit(0); // signifying it contains HScaleNetGUID
		Writer << HScaleNetGUID;
	}
	return true;
}

bool FHScaleEventsSerializer::WriteObjectPtr(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap)
{
	if (!!Reader.ReadBit())
	{
		FString FullStableName;
		Reader << FullStableName;

		const UObject* LoadedObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FullStableName);
		if (!LoadedObject) return false;
		FNetworkGUID ObjectGUID = PkgMap->GetNetGUIDFromObject(LoadedObject);
		Writer.WriteBit(0); // bHasHScaleNetGUID
		Writer << ObjectGUID;
		FHScaleExportFlags ExportFlags;
		ExportFlags.bHasPath = 0;
		Writer << ExportFlags.Value;
	}
	else
	{
		FHScaleNetGUID HScaleNetGUID;
		Reader << HScaleNetGUID;

		if (!HScaleNetGUID.IsValid()) // this suggests its a null pointer
		{
			// writes null object pointer
			Writer.WriteBit(0);
			FNetworkGUID NetworkGUID;
			Writer << NetworkGUID;
		}
		else
		{
			UObject* Object = PkgMap->FindObjectFromEntityID(HScaleNetGUID);
			if (!Object) { return false; } // Object might not have reached yet, no need to trigger RPC in these cases
			Writer.WriteBit(1);
			Writer << HScaleNetGUID;
			FHScaleExportFlags ExportFlags;
			ExportFlags.bHasPath = 0;
			Writer << ExportFlags.Value;
		}
	}
	return true;
}


bool FHScaleEventsSerializer::ReadSoftObjectPtr(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap)
{
	bool bObjectPtr = false;;
	Reader.UsingCustomVersion(FEngineNetworkCustomVersion::Guid);

	if (Reader.EngineNetVer() >= FEngineNetworkCustomVersion::SoftObjectPtrNetGuids)
	{
		const bool bUsePath = !!Reader.ReadBit();
		if (!bUsePath) bObjectPtr = true;;
	}

	if (bObjectPtr)
	{
		Writer.WriteBit(1); // signifying its a objectptr
		return ReadObjectPtr(Writer, Reader, PkgMap);
	}

	Writer.WriteBit(0); // signifying its a String
	return SerializeDataType<FString>(Writer, Reader);
}

bool FHScaleEventsSerializer::WriteSoftObjectPtr(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap)
{
	Writer.UsingCustomVersion(FEngineNetworkCustomVersion::Guid);

	const bool bObjectPtr = !!Reader.ReadBit();

	if (Writer.EngineNetVer() >= FEngineNetworkCustomVersion::SoftObjectPtrNetGuids)
	{
		Writer.WriteBit(bObjectPtr ? 0 : 1);
	}

	if (bObjectPtr)
	{
		return WriteObjectPtr(Writer, Reader, PkgMap);
	}

	return SerializeDataType<FString>(Writer, Reader);
}

bool FHScaleEventsSerializer::ReadDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const TArray<FRepLayoutCmd>::ElementType& Cmd)
{
	return SerializeDynArray(Writer, Reader, PkgMap, Cmd, true);
}

bool FHScaleEventsSerializer::WriteDynArray(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap, const TArray<FRepLayoutCmd>::ElementType& Cmd)
{
	return SerializeDynArray(Writer, Reader, PkgMap, Cmd, false);
}

bool FHScaleEventsSerializer::SerializeNetSerializeStruct(FBitReader& Reader, FBitWriter& Writer, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap)
{
	const FStructProperty* StructProp = CastField<FStructProperty>(Cmd.Property);
	check(StructProp->Struct->StructFlags & STRUCT_NetSerializeNative)
	UScriptStruct::ICppStructOps* CppStructOps = StructProp->Struct->GetCppStructOps();
	const int32 Size = CppStructOps->GetSize();
	uint8* Holder = static_cast<uint8*>(FMemory::Malloc(Size));
	CppStructOps->Construct(Holder);
	bool bOutSuccess;
	CppStructOps->NetSerialize(Reader, PkgMap, bOutSuccess, Holder);
	CppStructOps->NetSerialize(Writer, PkgMap, bOutSuccess, Holder);

	return true;
}

bool FHScaleEventsSerializer::SerializeBoolProperty(FBitWriter& Writer, FBitReader& Reader)
{
	const uint8 BitRead = Reader.ReadBit();
	Writer.WriteBit(BitRead);
	return true;
}

bool FHScaleEventsSerializer::SerializeByteProperty(FBitWriter& Writer, FBitReader& Reader, const FRepLayoutCmd& Cmd, UHScalePackageMap* PkgMap)
{
	uint8 Value;
	check(Cmd.Property)
	Cmd.Property->NetSerializeItem(Reader, PkgMap, &Value);
	Cmd.Property->NetSerializeItem(Writer, PkgMap, &Value);
	return true;
}

template<typename T>
bool FHScaleEventsSerializer::SerializeDataType(FBitWriter& Writer, FBitReader& Reader)
{
	T Val;
	Reader << Val;
	Writer << Val;
	return true;
}

template<typename T>
bool FHScaleEventsSerializer::SerializeNetSerializeTypes(FBitWriter& Writer, FBitReader& Reader, UHScalePackageMap* PkgMap)
{
	T Val;
	bool bOutSuccess;
	Val.NetSerialize(Reader, PkgMap, bOutSuccess);
	Val.NetSerialize(Writer, PkgMap, bOutSuccess);
	return true;
}