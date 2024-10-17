#include "MemoryLayer/HScaleMemoryTypes.h"

#include "Core/HScaleResources.h"
#include "MemoryLayer/HScalePropertyIdConverters.h"
#include "Utils/HScaleConversionUtils.h"
#include "Utils/HScaleStatics.h"

template<typename T>
static T* CastPty(FHScaleProperty* Property)
{
	if (Property->IsA<T>()) { return static_cast<T*>(Property); }
	return nullptr;
}

std::unique_ptr<FHScaleProperty> FHScaleProperty::CreateFromCmd(const FRepLayoutCmd& Cmd)
{
	const EHScaleMemoryTypeId TypeId = FHScalePropertyIdConverters::GetRepLayoutCmdToHScaleMemoryTypeId(Cmd);
	return std::move(CreateFromTypeId(TypeId));
}

std::unique_ptr<FHScaleProperty> FHScaleProperty::CreateFromTypeId(const EHScaleMemoryTypeId TypeId)
{
	switch (TypeId)
	{
		case EHScaleMemoryTypeId::Boolean:
			return std::move(std::make_unique<HScaleTypes::FHScaleBoolProperty>());

		case EHScaleMemoryTypeId::Uint8:
			return std::move(std::make_unique<HScaleTypes::FHScaleUInt8Property>());

		case EHScaleMemoryTypeId::Uint16:
			return std::move(std::make_unique<HScaleTypes::FHScaleUInt16Property>());

		case EHScaleMemoryTypeId::Uint32:
			return std::move(std::make_unique<HScaleTypes::FHScaleUInt32Property>());

		case EHScaleMemoryTypeId::Uint64:
			return std::move(std::make_unique<HScaleTypes::FHScaleUInt64Property>());

		case EHScaleMemoryTypeId::Int8:
			return std::move(std::make_unique<HScaleTypes::FHScaleInt8Property>());

		case EHScaleMemoryTypeId::Int16:
			return std::move(std::make_unique<HScaleTypes::FHScaleInt16Property>());

		case EHScaleMemoryTypeId::Int32:
			return std::move(std::make_unique<HScaleTypes::FHScaleInt32Property>());

		case EHScaleMemoryTypeId::Int64:
			return std::move(std::make_unique<HScaleTypes::FHScaleInt64Property>());

		case EHScaleMemoryTypeId::Float32:
			return std::move(std::make_unique<HScaleTypes::FHScaleFloatProperty>());

		case EHScaleMemoryTypeId::Float64:
			return std::move(std::make_unique<HScaleTypes::FHScaleFloat64Property>());

		case EHScaleMemoryTypeId::String:
			return std::move(std::make_unique<HScaleTypes::FHScaleStringProperty>());

		case EHScaleMemoryTypeId::Bytes:
			return std::move(std::make_unique<HScaleTypes::FHScaleBytesProperty>());

		case EHScaleMemoryTypeId::Vec2:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector2Property>());

		case EHScaleMemoryTypeId::Vec3:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector3Property>());

		case EHScaleMemoryTypeId::Vec2d:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector2DProperty>());

		case EHScaleMemoryTypeId::Vec3d:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector3DProperty>());

		case EHScaleMemoryTypeId::Vec4:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector4Property>());

		case EHScaleMemoryTypeId::Vec4d:
			return std::move(std::make_unique<HScaleTypes::FHScaleVector4DProperty>());

		case EHScaleMemoryTypeId::SystemPosition:
			return std::move(std::make_unique<HScaleTypes::FHScalePositionSystemProperty>());

		case EHScaleMemoryTypeId::ObjectPtrData:
			return std::move(std::make_unique<HScaleTypes::FHScaleObjectDataProperty>(HS_SPLIT_PROPERTY_MAX_LENGTH));

		case EHScaleMemoryTypeId::ObjectPtrChunkData:
			return std::move(std::make_unique<HScaleTypes::FHScaleObjectDataChunkProperty>());

		case EHScaleMemoryTypeId::SplitString:
			return std::move(std::make_unique<HScaleTypes::FHScaleSplitStringProperty>(HS_SPLIT_PROPERTY_MAX_LENGTH));

		case EHScaleMemoryTypeId::SplitByte:
			return std::move(std::make_unique<HScaleTypes::FHScaleSplitByteProperty>(HS_SPLIT_PROPERTY_MAX_LENGTH));

		case EHScaleMemoryTypeId::Owner:
			return std::move(std::make_unique<HScaleTypes::FHScaleOwnerProperty>());

		case EHScaleMemoryTypeId::Default:
		case EHScaleMemoryTypeId::None:
		case EHScaleMemoryTypeId::Undefined:
		default:
			return std::move(std::make_unique<HScaleTypes::FHScaleDefaultProperty>());
	}
}

bool HScaleTypes::FHScaleBoolProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	if (Ar.IsLoading())
	{
		bool PrevValue = Value;
		uint8 Temp;
		Ar.SerializeBits(&Temp, 1);
		Value = !!Temp;
		return PrevValue != Value;
	}
	else
	{
		uint8 Temp = Value;
		Ar.SerializeBits(&Temp, 1);
		return true;
	}
}

bool HScaleTypes::FHScaleUInt8Property::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	check(Cmd.Property)
	const uint8 PrevValue = Value;
	Cmd.Property->NetSerializeItem(Ar, nullptr, &Value);
	if (Ar.IsLoading())
	{
		return PrevValue != Value;
	}
	return true;
}

bool HScaleTypes::FHScaleStringProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	const std::string Prev = Value;
	if (Cmd.Type == ERepLayoutCmdType::PropertyString)
	{
		FString Holder(UTF8_TO_TCHAR(Value.c_str()));
		Ar << Holder;
		Value = std::string(TCHAR_TO_UTF8(*Holder));
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyName)
	{
		FName Holder(UTF8_TO_TCHAR(Value.c_str()));
		Ar << Holder;
		Value = std::string(TCHAR_TO_UTF8(*Holder.ToString()));
	}
	else if (Cmd.Property->IsA(FTextProperty::StaticClass()))
	{
		FText Holder = FText::FromString(FString(UTF8_TO_TCHAR(Value.c_str())));
		Ar << Holder;
		Value = std::string(TCHAR_TO_UTF8(*Holder.ToString()));
	}
	else
	{
		check(false)
		return false;
	}
	if (Ar.IsLoading()) { return Prev != Value; }
	return true;
}


bool HScaleTypes::FHScaleVector3Property::SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty)
{
	return false;
}

bool HScaleTypes::FHScaleVector3DProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	quark::vec3d Prev = Value;

	if (Cmd.Type == ERepLayoutCmdType::PropertyVector10)
	{
		if (Ar.IsLoading())
		{
			FVector_NetQuantize10 LocationTemp;
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = LocationTemp.X;
			Value.y = LocationTemp.Y;
			Value.z = LocationTemp.Z;
		}
		else
		{
			FVector_NetQuantize10 LocationTemp(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyVector100)
	{
		if (Ar.IsLoading())
		{
			FVector_NetQuantize100 LocationTemp;
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = LocationTemp.X;
			Value.y = LocationTemp.Y;
			Value.z = LocationTemp.Z;
		}
		else
		{
			FVector_NetQuantize100 LocationTemp(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyVector)
	{
		if (Ar.IsLoading())
		{
			FVector LocationTemp;
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = LocationTemp.X;
			Value.y = LocationTemp.Y;
			Value.z = LocationTemp.Z;
		}
		else
		{
			FVector LocationTemp(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyRotator)
	{
		if (Ar.IsLoading())
		{
			FRotator Rotator;
			bool bOutSuccess;
			Rotator.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = Rotator.Pitch;
			Value.y = Rotator.Yaw;
			Value.z = Rotator.Roll;
		}
		else
		{
			FRotator Rotator(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			Rotator.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyVectorNormal)
	{
		if (Ar.IsLoading())
		{
			FVector_NetQuantizeNormal LocationTemp;
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = LocationTemp.X;
			Value.y = LocationTemp.Y;
			Value.z = LocationTemp.Z;
		}
		else
		{
			FVector_NetQuantizeNormal LocationTemp(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyVectorQ)
	{
		if (Ar.IsLoading())
		{
			FVector_NetQuantize LocationTemp;
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
			Value.x = LocationTemp.X;
			Value.y = LocationTemp.Y;
			Value.z = LocationTemp.Z;
		}
		else
		{
			FVector_NetQuantize LocationTemp(Value.x, Value.y, Value.z);
			bool bOutSuccess;
			LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		}
	}

	if (Ar.IsLoading())
	{
		return !FHScaleStatics::AreVectorsWithinRadius(Prev, Value);
	}
	return true;
}

bool HScaleTypes::FHScaleVector4Property::SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty)
{
	return false;
}

bool HScaleTypes::FHScaleBytesProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	check(Cmd.Property)
	if (Cmd.Type == ERepLayoutCmdType::RepMovement)
	{
		bool bOutSuccess;
		if (Ar.IsLoading())
		{
			// we will not be able to directly copy the buffer from Archive, as we are not aware
			// of the length of the data and position bits might not be multiple of 8
			// read movement data from archive and load into a temporary holder
			FRepMovement Holder;
			Holder.NetSerialize(Ar, nullptr, bOutSuccess);
			check(bOutSuccess);
			// write the repmovement data in holder onto a temporary writer
			FBitWriter Writer(8192);
			Holder.NetSerialize(Writer, nullptr, bOutSuccess);
			// memcopy the writer data into byte buffer
			Value.assign(Writer.GetData(), Writer.GetData() + Writer.GetNumBytes());
		}
		else
		{
			// we will not be directly writing the buffer into Archive, as we are not sure of end bits in buffer
			// copy the data buffer into a temporary reader
			FBitReader Reader(Value.data(), Value.size() * 8);

			// load data from reader in holder repmovement
			FRepMovement Holder;
			Holder.NetSerialize(Reader, nullptr, bOutSuccess);
			// now holder is loaded with rep movement data
			// write out the repmovemnt data into Archive
			Holder.NetSerialize(Ar, nullptr, bOutSuccess);
		}
		return true;
	}

	return true;
}

FString HScaleTypes::FHScaleBytesProperty::ToDebugString()
{
	return FHScaleStatics::PrintBytesFormat(Value);
}

bool HScaleTypes::FHScaleVector2Property::SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty)
{
	return false;
}

bool HScaleTypes::FHScaleVector2DProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty)
{
	return false;
}

bool HScaleTypes::FHScalePositionSystemProperty::SerializePosition(FArchive& Ar)
{
	if (Ar.IsLoading())
	{
		quark::vec3 Prev = Value;
		FVector_NetQuantize10 LocationTemp;
		bool bOutSuccess;
		LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		Value.x = LocationTemp.X;
		Value.y = LocationTemp.Y;
		Value.z = LocationTemp.Z;
		return !FHScaleStatics::AreVectorsWithinRadius(Prev, Value);
	}
	else
	{
		FVector_NetQuantize10 LocationTemp(Value.x, Value.y, Value.z);
		bool bOutSuccess;
		LocationTemp.NetSerialize(Ar, nullptr, bOutSuccess);
		return true;
	}
}

bool HScaleTypes::FHScaleVector4DProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	bool bOutSuccess;
	if (Ar.IsLoading())
	{
		quark::vec4d Prev = Value;
		FPlane Plane;
		Plane.NetSerialize(Ar, nullptr, bOutSuccess);
		Value.x = Plane.X;
		Value.y = Plane.Y;
		Value.z = Plane.Z;
		Value.w = Plane.W;
		return !FHScaleStatics::AreVectorsWithinRadius(Prev, Value);
	}
	else
	{
		FPlane Plane(Value.x, Value.y, Value.z, Value.w);
		Plane.NetSerialize(Ar, nullptr, bOutSuccess);
		return true;
	}
}

void HScaleTypes::FHScaleOwnerProperty::Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId)
{
	if (CachedValue.type() == quark::value_type::uint32)
	{
		Value = CachedValue.as<uint32>().value();
	}
	else
	{
		Value = CachedValue.as<uint64>().value();
	}
}

void HScaleTypes::FHScaleOwnerProperty::Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId)
{
	if (Value <= UINT32_MAX)
	{
		const uint32 Value_R = Value;
		Attributes.Push({PropertyId, Value_R});
	}
	else
	{
		Attributes.Push({PropertyId, Value});
	}
}

HScaleTypes::FHScaleSplitStringProperty::FHScaleSplitStringProperty(const uint8_t MaxLength)
	: bIsPartial(false), MaxLength(MaxLength), Count(0)
{
	for (uint8 i = 0; i < MaxLength; ++i)
	{
		SplitStrings.push_back(std::move(CreateFromTypeId(EHScaleMemoryTypeId::String)));
	}
}

void HScaleTypes::FHScaleSplitStringProperty::Serialize(TArray<FHScaleAttributesUpdate>& Attributes, const uint16 PropertyId)
{
	const uint16 Offset = FHScalePropertyIdConverters::GetSplitStringOffsetFromPropertyId(PropertyId, MaxLength);
	for (uint8 i = 0; i < Count; i++)
	{
		SplitStrings[i]->Serialize(Attributes, Offset + i);
	}
}

void HScaleTypes::FHScaleSplitStringProperty::Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp)
{
	LastUpdatedTs = Timestamp;
	Deserialize_R(CachedValue, PropertyId);
}

void HScaleTypes::FHScaleSplitStringProperty::Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId)
{
	const uint8 Index = FHScalePropertyIdConverters::GetSplitPropertyIndexFromPropertyId(PropertyId, MaxLength);

	// if on receive, it is first partial update, then clear out all partial values held
	if (!bIsPartial)
	{
		bIsPartial = true;
		for (auto& PropertyPtr : SplitStrings)
		{
			FHScaleStringProperty* Property = CastPty<FHScaleStringProperty>(PropertyPtr.get());
			Property->SetValue("");
		}
		Count = 0;
	}

	// update the property value
	SplitStrings[Index]->Deserialize(CachedValue, PropertyId, LastUpdatedTs);

	const uint8 AssumptionCount = Index + 1;
	Count = std::max(Count, AssumptionCount);

	// now check if all values are received and if so update FullString and bIsPartial
	bool bIsValid = false;
	for (uint8 i = 0; i < Count; i++)
	{
		auto& PropertyPtr = SplitStrings[i];
		const FHScaleStringProperty* Property = CastPty<FHScaleStringProperty>(PropertyPtr.get());
		std::string SplitValue = Property->GetValue();
		if (!FHScaleConversionUtils::IsValidSplitString(SplitValue)) { break; }
		if (!FHScaleConversionUtils::IsHScaleSplitStringContd(SplitValue))
		{
			bIsValid = true;
			break;
		}
	}

	if (bIsValid)
	{
		// all partial updates are received, now update the FullString and bIsPartial
		bIsPartial = false;
		std::vector<std::string> Parts;
		for (uint8 i = 0; i < Count; i++)
		{
			auto& PropertyPtr = SplitStrings[i];
			const FHScaleStringProperty* Property = CastPty<FHScaleStringProperty>(PropertyPtr.get());
			Parts.push_back(Property->GetValue());
		}
		FullStringValue = FHScaleConversionUtils::CombinePartStrings(Parts);
	}
}

bool HScaleTypes::FHScaleSplitStringProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	const FString Prev = FullStringValue;
	if (Cmd.Type == ERepLayoutCmdType::PropertyString)
	{
		Ar << FullStringValue;
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyName)
	{
		FName Holder(FullStringValue);
		Ar << Holder;
		FullStringValue = Holder.ToString();
	}
	else if (Cmd.Property->IsA(FTextProperty::StaticClass()))
	{
		FText Holder = FText::FromString(FullStringValue);
		Ar << Holder;
		FullStringValue = Holder.ToString();
	}
	else
	{
		check(false)
		return false;
	}

	if (Ar.IsLoading() && Prev != FullStringValue)
	{
		UpdateSplitStringFromFullString();
	}
	return Prev != FullStringValue;
}

bool HScaleTypes::FHScaleSplitStringProperty::SerializeFString(FArchive& Ar)
{
	const FString Prev = FullStringValue;
	Ar << FullStringValue;
	if (Ar.IsLoading() && Prev != FullStringValue)
	{
		UpdateSplitStringFromFullString();
	}
	return Prev != FullStringValue;
}

bool HScaleTypes::FHScaleSplitStringProperty::SetFullString(const FString& Value)
{
	if (FullStringValue == Value) return false;
	FullStringValue = Value;
	UpdateSplitStringFromFullString();
	return true;
}

bool HScaleTypes::FHScaleSplitStringProperty::IsCompleteForReceive() const
{
	return IsValid() && !bIsPartial;
}

bool HScaleTypes::FHScaleSplitStringProperty::UpdateSplitStringFromFullString()
{
	std::vector<std::string> SplitValues;
	FHScaleConversionUtils::SplitFStringToHScaleStrings(FullStringValue, SplitValues);
	Count = SplitValues.size();

	check(Count <= MaxLength)

	for (uint8 i = 0; i < Count; ++i)
	{
		auto& PropertyPtr = SplitStrings[i];
		FHScaleStringProperty* Property = CastPty<FHScaleStringProperty>(PropertyPtr.get());
		Property->SetValue(SplitValues[i]);
	}

	bIsPartial = false;

	return true;
}

HScaleTypes::FHScaleSplitByteProperty::FHScaleSplitByteProperty(const uint8_t MaxLength)
	: bIsPartial(false), MaxLength(MaxLength), Count(0)
{
	for (uint8 i = 0; i < MaxLength; ++i)
	{
		SplitBytes.push_back(std::move(CreateFromTypeId(EHScaleMemoryTypeId::Bytes)));
	}
}

void HScaleTypes::FHScaleSplitByteProperty::Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId)
{
	const uint16 Offset = FHScalePropertyIdConverters::GetSplitBytesOffsetFromPropertyId(PropertyId, MaxLength);
	for (uint8 i = 0; i < Count; i++)
	{
		SplitBytes[i]->Serialize(Attributes, Offset + i);
	}
}

void HScaleTypes::FHScaleSplitByteProperty::Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp)
{
	LastUpdatedTs = Timestamp;
	Deserialize_R(CachedValue, PropertyId);
}

void HScaleTypes::FHScaleSplitByteProperty::DeserializeForIndex(const quark::value& Value, const uint16 PropertyId, const uint8 Index)
{
	quark::vector<uint8> Holder = Value.as<quark::vector<uint8>>().value();
	const uint8 ReceivedHashId = FHScaleConversionUtils::FetchHashIdFromBuffer(Holder);

	bool bIsReceivedHasIdValid = FHScaleConversionUtils::IsReceivedHasIdValid(OnReceiveHashID, ReceivedHashId);

	if (!bIsReceivedHasIdValid) { return; } // this signifies we received stale data, and we will not proceed with it

	OnReceiveHashID = ReceivedHashId;
	// if on receive, it is first partial update, then clear out all partial values held
	if (!bIsPartial)
	{
		bIsPartial = true;
		for (auto& PropertyPtr : SplitBytes)
		{
			FHScaleBytesProperty* Property = CastPty<FHScaleBytesProperty>(PropertyPtr.get());
			Property->ClearBuffer();
		}
		Count = 0;
	}

	SplitBytes[Index]->Deserialize(Value, PropertyId, LastUpdatedTs);

	const uint8 AssumptionCount = Index + 1;
	Count = std::max(Count, AssumptionCount);

	// now check if all values are received and if so update FullString and bIsPartial
	bool bIsValid = false;
	for (uint8 i = 0; i < Count; i++)
	{
		auto& PropertyPtr = SplitBytes[i];
		FHScaleBytesProperty* Property = CastPty<FHScaleBytesProperty>(PropertyPtr.get());
		std::vector<uint8>& SplitValue = Property->GetBuffer();
		if (!FHScaleConversionUtils::IsValidSplitBuffer(SplitValue)) { break; }
		if (!FHScaleConversionUtils::IsHScaleSplitBufferContd(SplitValue))
		{
			bIsValid = true;
			break;
		}
	}

	if (bIsValid)
	{
		// all partial updates are received, now update the FullString and bIsPartial
		bIsPartial = false;
		std::vector<std::vector<uint8>> Parts;
		for (uint8 i = 0; i < Count; i++)
		{
			auto& PropertyPtr = SplitBytes[i];
			FHScaleBytesProperty* Property = CastPty<FHScaleBytesProperty>(PropertyPtr.get());
			Parts.push_back(Property->GetBuffer());
		}
		FHScaleConversionUtils::CombinePartBuffers(Parts, FullBuffer);
	}
}

void HScaleTypes::FHScaleSplitByteProperty::Deserialize_R(const quark::value& Value, const uint16 PropertyId)
{
	const uint8 Index = FHScalePropertyIdConverters::GetSplitPropertyIndexFromPropertyId(PropertyId, MaxLength);

	DeserializeForIndex(Value, PropertyId, Index);
}

bool HScaleTypes::FHScaleSplitByteProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	check(Cmd.Property)

	if (Cmd.Property->IsA(FStructProperty::StaticClass()))
	{
		bool bOutSuccess;

		FStructProperty* StructProp = CastField<FStructProperty>(Cmd.Property);
		check(StructProp->Struct->StructFlags & STRUCT_NetSerializeNative)
		UScriptStruct::ICppStructOps* CppStructOps = StructProp->Struct->GetCppStructOps();
		const int32 Size = CppStructOps->GetSize();
		uint8* Holder = static_cast<uint8*>(FMemory::Malloc(Size));
		CppStructOps->Construct(Holder);

		if (Ar.IsLoading())
		{
			// we will not be able to directly copy the buffer from Archive, as we are not aware
			// of the length of the data and position bits might not be multiple of 8
			// read movement data from archive and load into a temporary holder
			CppStructOps->NetSerialize(Ar, nullptr, bOutSuccess, Holder);
			// write the struct data in holder onto a temporary writer
			FBitWriter Writer(Size * 8);
			CppStructOps->NetSerialize(Writer, nullptr, bOutSuccess, Holder);
			// memcopy the writer data into byte buffer
			if (Writer.GetNumBytes() > 0)
			{
				FullBuffer.assign(Writer.GetData(), Writer.GetData() + Writer.GetNumBytes());
				bOutSuccess &= UpdateSplitBytesFromFullBytes();
			}
		}
		else
		{
			// we will not be directly writing the buffer into Archive, as we are not sure of end bits in buffer
			// copy the data buffer into a temporary reader
			FBitReader Reader(FullBuffer.data(), FullBuffer.size() * 8);

			// load data from reader in holder struct
			CppStructOps->NetSerialize(Reader, nullptr, bOutSuccess, Holder);
			// now holder is loaded with struct data
			// write out the data into Archive
			CppStructOps->NetSerialize(Ar, nullptr, bOutSuccess, Holder);
		}
		FMemory::Free(Holder);

		return true;
	}
	else if (Cmd.Type == ERepLayoutCmdType::PropertyNetId)
	{
		bool bOutSuccess;
		if (Ar.IsLoading())
		{
			FUniqueNetIdRepl Holder;
			Holder.NetSerialize(Ar, nullptr, bOutSuccess);
			FBitWriter Writer(8192);
			Holder.NetSerialize(Writer, nullptr, bOutSuccess);
			if (Writer.GetNumBytes() > 0)
			{
				FullBuffer.assign(Writer.GetData(), Writer.GetData() + Writer.GetNumBytes());
				bOutSuccess &= UpdateSplitBytesFromFullBytes();
			}
		}
		else
		{
			FBitReader Reader(FullBuffer.data(), FullBuffer.size() * 8);
			FUniqueNetIdRepl Holder;
			Holder.NetSerialize(Reader, nullptr, bOutSuccess);
			Holder.NetSerialize(Ar, nullptr, bOutSuccess);
		}
		return true;
	}

	return false;
}

FString HScaleTypes::FHScaleSplitByteProperty::ToDebugString()
{
	return FHScaleStatics::PrintBytesFormat(FullBuffer);
}

bool HScaleTypes::FHScaleSplitByteProperty::IsValid() const
{
	return !FullBuffer.empty();
}

bool HScaleTypes::FHScaleSplitByteProperty::IsCompleteForReceive() const
{
	return IsValid() && !bIsPartial;
}

bool HScaleTypes::FHScaleSplitByteProperty::UpdateSplitBytesFromFullBytes()
{
	std::vector<std::vector<uint8>> SplitBuffers;
	HashForSend = FHScaleConversionUtils::FetchNextHashIdForSend(HashForSend);
	FHScaleConversionUtils::SplitBufferToHScaleBuffers(FullBuffer, SplitBuffers, HashForSend);
	Count = SplitBuffers.size();

	check(Count <= MaxLength)

	for (uint8 i = 0; i < Count; ++i)
	{
		auto& PropertyPtr = SplitBytes[i];
		FHScaleBytesProperty* Property = CastPty<FHScaleBytesProperty>(PropertyPtr.get());
		Property->SetValue(SplitBuffers[i]);
	}
	bIsPartial = false;

	return true;
}

HScaleTypes::FHScaleObjectDataChunkProperty::FHScaleObjectDataChunkProperty()
	: FHScaleSplitByteProperty(HS_SPLIT_PROPERTY_MAX_LENGTH), bIsContinued(false), ExportFlags(0), NextValue(0) {}

void HScaleTypes::FHScaleObjectDataChunkProperty::Serialize(TArray<FHScaleAttributesUpdate>& Attributes, const uint16 PropertyId)
{
	const uint16 Offset = PropertyId;
	for (uint8 i = 0; i < Count; i++)
	{
		SplitBytes[i]->Serialize(Attributes, Offset + i);
	}
}

void HScaleTypes::FHScaleObjectDataChunkProperty::Deserialize_R(const quark::value& Value, const uint16 PropertyId)
{
	const uint8 Index = FHScalePropertyIdConverters::GetOuterChunkPropertyIndexFromPropertyId(PropertyId);

	DeserializeForIndex(Value, PropertyId, Index);

	if (!bIsPartial && FullBuffer.size() >= (sizeof(ExportFlags) + sizeof(NextValue)))
	{
		ExportFlags = FullBuffer[0];
		NextValue = FHScaleStatics::ConvertBufferToUInt64(FullBuffer, sizeof(ExportFlags));
		FHScaleStatics::ConvertBufferToFString(ObjectName, FullBuffer, sizeof(ExportFlags) + sizeof(NextValue));
		bIsContinued = NextValue <= UINT16_MAX && NextValue > 0;
	}
}

bool HScaleTypes::FHScaleObjectDataChunkProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	checkf(false, TEXT("Not implemented"))
	return false;
}


void HScaleTypes::FHScaleObjectDataChunkProperty::SerializeChunk(FHScaleOuterChunk& Chunk, FHScaleOuterChunk* NextChunk, const uint8 CurIndex, const uint16 PropertyId)
{
	if (NextChunk != nullptr)
	{
		if (NextChunk->bIsCachedAttrId) // points to already cached attributeId
		{
			NextValue = NextChunk->AttributeId;
			bIsContinued = true;
		}
		else if (NextChunk->HS_NetGUID.IsValid()) // points to dynamic object in the network
		{
			NextValue = NextChunk->HS_NetGUID.Get();
			bIsContinued = false;
		}
		else // this should be next attributeId
		{
			NextValue = FHScalePropertyIdConverters::GetNextOuterAttrId(CurIndex, PropertyId);
			bIsContinued = true;
		}
	}
	else
	{
		NextValue = 0; // this signifies end of the outer
		bIsContinued = false;
	}

	ExportFlags = Chunk.ExportFlags;
	ObjectName = Chunk.ObjectName;

	FullBuffer.clear();
	FullBuffer.push_back(Chunk.ExportFlags);
	FHScaleStatics::AppendUint64ToBuffer(NextValue, FullBuffer);
	FHScaleStatics::AppendFStringToBuffer(Chunk.ObjectName, FullBuffer);

	UpdateSplitBytesFromFullBytes();
}

void HScaleTypes::FHScaleObjectDataChunkProperty::Clear()
{
	bIsPartial = false;
	bIsContinued = false;
	FullBuffer.clear();
	ExportFlags = 0;
	NextValue = 0;
}

HScaleTypes::FHScaleObjectDataProperty::FHScaleObjectDataProperty(const uint8 MaxLength)
	: bIsPartial(false), MaxLength(MaxLength), Count(0), bIsValid(false)
{
	for (uint8 i = 0; i < MaxLength; ++i)
	{
		SplitChunks.push_back(std::move(CreateFromTypeId(EHScaleMemoryTypeId::ObjectPtrChunkData)));
	}
	DynamicProperty = std::move(CreateFromTypeId(EHScaleMemoryTypeId::Uint64));
}

void HScaleTypes::FHScaleObjectDataProperty::Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId)
{
	const uint16 Offset = FHScalePropertyIdConverters::GetOuterOffsetFromPropertyId(PropertyId, MaxLength);

	if (HScaleNetGUID.IsValid())
	{
		DynamicProperty->Serialize(Attributes, Offset);
		return;
	}

	for (uint8 i = 0; i < Count; i++)
	{
		SplitChunks[i]->Serialize(Attributes, FHScalePropertyIdConverters::GetOuterChunkOffsetFromOffsetAndIndex(Offset, i));
	}
}

void HScaleTypes::FHScaleObjectDataProperty::Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp)
{
	LastUpdatedTs = Timestamp;
	Deserialize_R(CachedValue, PropertyId);
}

void HScaleTypes::FHScaleObjectDataProperty::Deserialize_R(const quark::value& Value, const uint16 PropertyId)
{
	if (Value.type() == quark::value_type::uint64)
	{
		DynamicProperty->Deserialize(Value, PropertyId, LastUpdatedTs);
		FHScaleUInt64Property* DynProp = CastPty<FHScaleUInt64Property>(DynamicProperty.get());
		HScaleNetGUID = FHScaleNetGUID::Create(DynProp->GetValue());
		bIsValid = true;
		bIsPartial = false;
		return;
	}
	// if on receive, it is first partial update, then clear out all partial values held
	if (!bIsPartial)
	{
		bIsPartial = true;
		for (auto& PropertyPtr : SplitChunks)
		{
			FHScaleObjectDataChunkProperty* Property = CastPty<FHScaleObjectDataChunkProperty>(PropertyPtr.get());
			Property->Clear();
		}
		Count = 0;
	}
	const uint8 Index = FHScalePropertyIdConverters::GetOuterPropertyIndexFromPropertyId(PropertyId);
	SplitChunks[Index]->Deserialize(Value, PropertyId, LastUpdatedTs);

	const uint8 AssumptionCount = Index + 1;
	Count = std::max(Count, AssumptionCount);

	bIsValid = false;
	for (uint8 i = 0; i < Count; i++)
	{
		auto& PropertyPtr = SplitChunks[i];
		const FHScaleObjectDataChunkProperty* Property = CastPty<FHScaleObjectDataChunkProperty>(PropertyPtr.get());
		if (!Property->IsValid()) { break; }
		if (!Property->bIsContinued)
		{
			bIsValid = true;
			break;
		}
	}

	if (bIsValid)
	{
		bIsPartial = false;
	}
}

FString HScaleTypes::FHScaleObjectDataProperty::ToDebugString()
{
	return FString::Printf(TEXT("NetGUID= %s HScaleNetGUID=%s"), *NetworkGUID.ToString(), *HScaleNetGUID.ToString());
}

bool HScaleTypes::FHScaleObjectDataProperty::SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd)
{
	checkf(false, TEXT("Not implemented"))
	return false;
}

bool HScaleTypes::FHScaleObjectDataProperty::SerializeChunks(TArray<FHScaleOuterChunk>& OuterChunks, const uint16 PropertyId, const FNetworkGUID& NetGUID, const FHScaleNetGUID& HSNetGUID)
{
	NetworkGUID = NetGUID;
	HScaleNetGUID = HSNetGUID;
	bIsValid = true;

	if (HScaleNetGUID.IsValid())
	{
		FHScaleUInt64Property* DynProp = CastPty<FHScaleUInt64Property>(DynamicProperty.get());
		const uint64 PrevValue = DynProp->GetValue();
		DynProp->SetValue(HScaleNetGUID.Get());
		return PrevValue != DynProp->GetValue();
	}

	check(OuterChunks.Num() <= SplitChunks.size())
	Count = OuterChunks.Num();
	uint8 Index = 0;
	for (int8 i = OuterChunks.Num() - 1; i >= 0; --i)
	{
		const bool bIsLast = i == 0;
		auto& PropertyPtr = SplitChunks[Index];
		FHScaleObjectDataChunkProperty* Property = CastPty<FHScaleObjectDataChunkProperty>(PropertyPtr.get());

		Property->SerializeChunk(OuterChunks[i], bIsLast ? nullptr : &OuterChunks[i - 1], Index, PropertyId);
		if (!Property->bIsContinued) { break; }
		Index++;
	}

	for (uint8 i = 0; i < Count; i++)
	{
		auto& PropertyPtr = SplitChunks[Index];
		FHScaleObjectDataChunkProperty* Property = CastPty<FHScaleObjectDataChunkProperty>(PropertyPtr.get());
		if (Property->NextValue > UINT16_MAX) { return true; }
		if (!Property->ObjectName.IsEmpty()) { return true; }
	}
	return false;
}

void HScaleTypes::FHScaleObjectDataProperty::DeserializeChunks(TArray<FHScaleOuterChunk>& OuterChunks)
{
	if (HScaleNetGUID.IsValid())
	{
		FHScaleOuterChunk Chunk;
		Chunk.HS_NetGUID = HScaleNetGUID;
		OuterChunks.Add(Chunk);
		return;
	}
	uint64 PrevValue = 0;

	for (int8 i = Count - 1; i >= 0; i--)
	{
		auto& PropertyPtr = SplitChunks[i];
		const FHScaleObjectDataChunkProperty* Property = CastPty<FHScaleObjectDataChunkProperty>(PropertyPtr.get());

		// if first element is pointing to another dynamic object then  
		if (i == Count - 1 && Property->NextValue > UINT16_MAX)
		{
			FHScaleOuterChunk InitChunk;
			InitChunk.HS_NetGUID = FHScaleNetGUID::Create_Object(Property->NextValue);
			OuterChunks.Add(InitChunk);
		}
		FHScaleOuterChunk Chunk;
		Chunk.ExportFlags = Property->ExportFlags;
		Chunk.ObjectName = Property->ObjectName;

		if (PrevValue)
		{
			if (PrevValue > UINT16_MAX)
			{
				Chunk.HS_NetGUID = FHScaleNetGUID::Create_Object(PrevValue);
			}
			else
			{
				Chunk.AttributeId = PrevValue;
			}
		}
		PrevValue = Property->NextValue;
		OuterChunks.Add(Chunk);
	}
}

void HScaleTypes::FHScaleObjectDataProperty::WriteOut(FArchive& Ar)
{
	bool bHasNetworkId = false;
	Ar.SerializeBits(&bHasNetworkId, 1);
	Ar << NetworkGUID;
	if (!bHasNetworkId && !NetworkGUID.IsValid()) { return; }

	FHScaleExportFlags ExportFlags;
	ExportFlags.bHasPath = 0;
	Ar << ExportFlags.Value;
}

bool HScaleTypes::FHScaleObjectDataProperty::IsValid() const
{
	if (NetworkGUID.IsValid() || HScaleNetGUID.IsValid()) { return true; }

	return bIsValid;
}

bool HScaleTypes::FHScaleObjectDataProperty::IsCompleteForReceive() const
{
	return bIsValid && !bIsPartial;
}