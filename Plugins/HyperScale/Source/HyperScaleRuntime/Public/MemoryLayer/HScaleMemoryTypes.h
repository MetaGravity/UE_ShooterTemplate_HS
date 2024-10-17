#pragma once

#include "HScalePropertyIdConverters.h"
#include "Core/HScaleCommons.h"
#include "Core/HScaleResources.h"
#include "Net/RepLayout.h"
#include "NetworkLayer/HScaleUpdates.h"
#include "Utils/HScaleObjectSerializationHelpers.h"

class FHScaleStatics;

#define OVERRIDE_HSCALE_TYPE(TypeId) \
public: \
static EHScaleMemoryTypeId __GetType() { return TypeId; } \
virtual EHScaleMemoryTypeId GetType() const override { return TypeId; }

class FHScaleProperty
{
public:
	FHScaleProperty()
		: LastUpdatedTs(0) {}

	virtual ~FHScaleProperty() {}

	static EHScaleMemoryTypeId __GetType() { return EHScaleMemoryTypeId::Undefined; }

	virtual EHScaleMemoryTypeId GetType() const = 0;

	static std::unique_ptr<FHScaleProperty> CreateFromCmd(const FRepLayoutCmd& Cmd);
	static std::unique_ptr<FHScaleProperty> CreateFromTypeId(EHScaleMemoryTypeId TypeId);

	template<typename T>
	bool IsA() const { return T::__GetType() == GetType(); }

	/**
	 * Writes the property value into the builder with given propertyId
	 */
	virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) = 0;


	/**
	 * Reads the value from 
	 */
	virtual void Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp)
	{
		// #TODO: should below statement be enabled?
		// if (LastUpdatedTs > Timestamp) return;
		LastUpdatedTs = Timestamp;
		Deserialize_R(CachedValue, PropertyId);
	}

	virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) = 0;

	virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) = 0;

	virtual FString ToDebugString() = 0;

	virtual uint8 NumProps() const = 0;

	virtual bool IsCompleteForReceive() const { return true; }

	uint64 LastUpdatedTs;

};

/**
 * All derived PropertyTypes are enclosed in a namespace for modularity
 */
namespace HScaleTypes
{
	class FHScaleDefaultProperty final : public FHScaleProperty
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Undefined)

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override {}

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override {}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override { return false; }

		virtual FString ToDebugString() override { return TEXT(""); }
		virtual uint8 NumProps() const override { return 0; }
	};

	template<typename T>
	class THScaleGenericProperty : public FHScaleProperty
	{
	public:
		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override
		{
			Attributes.Push({PropertyId, Value});
		}

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override
		{
			Value = CachedValue.as<T>().value();
		}

		void SetValue(const T& NewValue)
		{
			this->Value = NewValue;
		}

		T GetValue() const
		{
			return Value;
		}

		virtual uint8 NumProps() const override { return 1; }

	protected:
		T Value;
	};

	template<typename T>
	class THScaleGenericProperty_S : public THScaleGenericProperty<T>
	{
	public:
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override
		{
			if (Ar.IsLoading())
			{
				T PrevValue = THScaleGenericProperty<T>::Value;
				Ar << THScaleGenericProperty<T>::Value;
				return PrevValue != THScaleGenericProperty<T>::Value;
			}
			// if writing into Archive, always true
			Ar << THScaleGenericProperty<T>::Value;
			return true;
		}
	};

	// ------------ Basic Data Types
	class FHScaleBoolProperty : public THScaleGenericProperty<bool>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Boolean)

		virtual FString ToDebugString() override
		{
			return Value ? TEXT("True") : TEXT("False");
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;
	};

	class FHScaleUInt8Property : public THScaleGenericProperty<uint8>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Uint8)

		virtual FString ToDebugString() override
		{
			return FString::FromInt(Value);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;
	};

	class FHScaleUInt16Property : public THScaleGenericProperty_S<uint16>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Uint16)

		virtual FString ToDebugString() override
		{
			return FString::FromInt(Value);
		}
	};

	class FHScaleUInt32Property : public THScaleGenericProperty_S<uint32>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Uint32)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("%u"), Value);
		}
	};

	class FHScaleUInt64Property : public THScaleGenericProperty_S<uint64>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Uint64)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("%llu"), Value);
		}
	};

	class FHScaleInt8Property : public THScaleGenericProperty_S<int8>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Int8)

		virtual FString ToDebugString() override
		{
			return FString::FromInt(Value);
		}
	};

	class FHScaleInt16Property : public THScaleGenericProperty_S<int16>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Int16)

		virtual FString ToDebugString() override
		{
			return FString::FromInt(Value);
		}
	};

	class FHScaleInt32Property : public THScaleGenericProperty_S<int32>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Int32)

		virtual FString ToDebugString() override
		{
			return FString::FromInt(Value);
		}
	};

	class FHScaleInt64Property : public THScaleGenericProperty_S<int64>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Int64)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("%lld"), Value);
		}
	};

	class FHScaleFloatProperty : public THScaleGenericProperty_S<float>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Float32)

		virtual FString ToDebugString() override
		{
			return FString::SanitizeFloat(Value, 6);
		}
	};

	class FHScaleFloat64Property : public THScaleGenericProperty_S<double>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Float64)

		virtual FString ToDebugString() override
		{
			return FString::SanitizeFloat(Value, 6);
		}
	};

	class FHScaleVector2Property : public THScaleGenericProperty<quark::vec2>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec2)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f"), Value.x, Value.y);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override;
	};

	class FHScaleVector2DProperty : public THScaleGenericProperty<quark::vec2d>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec2d)


		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f"), Value.x, Value.y);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override;
	};

	class FHScaleVector3Property : public THScaleGenericProperty<quark::vec3>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec3)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f"), Value.x, Value.y, Value.z);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override;
	};

	class FHScaleVector3DProperty : public THScaleGenericProperty<quark::vec3d>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec3d)

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f"), Value.x, Value.y, Value.z);
		}
	};

	class FHScaleVector4Property : public THScaleGenericProperty<quark::vec4>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec4)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), Value.x, Value.y, Value.z, Value.w);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override;
	};

	class FHScaleVector4DProperty : public THScaleGenericProperty<quark::vec4d>
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Vec4d)

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), Value.x, Value.y, Value.z, Value.w);
		}

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override;
	};

	class FHScaleStringProperty : public FHScaleProperty
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::String)

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;

		virtual FString ToDebugString() override
		{
			return FString(UTF8_TO_TCHAR(Value.c_str()));
		}

		void SetValue(const std::string& NewValue)
		{
			this->Value = NewValue;
		}

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override
		{
			Attributes.Push({PropertyId, quark::value(Value.c_str(), std::min(Value.size(), static_cast<size_t>(HSCALE_MAX_STRING_LENGTH)))});
		}

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override
		{
			Value = CachedValue.as<quark::string>().value();
		}

		std::string GetValue() const
		{
			return Value;
		}

		virtual uint8 NumProps() const override { return 1; }

	protected:
		std::string Value;
	};

	class FHScaleBytesProperty : public FHScaleProperty
	{
	public:
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Bytes)

		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;

		virtual FString ToDebugString() override;

		void ClearBuffer() { Value.clear(); }

		std::vector<uint8>& GetBuffer() { return Value; }

		void SetValue(const std::vector<uint8>& NewValue)
		{
			this->Value = NewValue;
		}

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override
		{
			Attributes.Push({PropertyId, quark::value(Value.data(), std::min(Value.size(), static_cast<size_t>(HSCALE_MAX_BUFFER_LENGTH)))});
		}

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override
		{
			quark::vector<uint8> Holder = CachedValue.as<quark::vector<uint8>>().value();
			Value = std::vector<uint8>(Holder.begin(), Holder.end(), std::allocator<uint8>());
		}

		std::vector<uint8> GetValue() const
		{
			return Value;
		}

		virtual uint8 NumProps() const override { return 1; }

	protected:
		std::vector<uint8> Value;
	};

	// ** End of Quark Basic Types Properties


	// ** Start of Custom Plugin properties not in use/part of Unreal

	class FHScaleOwnerProperty : public FHScaleUInt64Property
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::Owner)

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override;
		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override;
	};

	class FHScaleSplitStringProperty : public FHScaleProperty
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::SplitString)

		FHScaleSplitStringProperty(const uint8_t MaxLength);

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override;
		virtual void Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp) override;
		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override;
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;
		virtual bool SerializeFString(FArchive& Ar);
		virtual FString ToDebugString() override { return FullStringValue; }

		FString GetValue() { return FullStringValue; }
		bool SetFullString(const FString& Value);
		bool IsValid() const { return !FullStringValue.IsEmpty(); }
		virtual uint8 NumProps() const override { return Count; }
		bool bIsPartial;

		virtual bool IsCompleteForReceive() const override;

	protected:
		bool UpdateSplitStringFromFullString();

		uint8 MaxLength;
		std::vector<std::unique_ptr<FHScaleProperty>> SplitStrings;
		FString FullStringValue;
		uint8 Count;
	};

	class FHScaleSplitByteProperty : public FHScaleProperty
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::SplitByte)

		FHScaleSplitByteProperty(const uint8_t MaxLength);

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override;
		virtual void Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp) override;
		virtual void Deserialize_R(const quark::value& Value, const uint16 PropertyId) override;
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;
		virtual FString ToDebugString() override;

		bool IsValid() const;
		virtual uint8 NumProps() const override { return Count; }
		bool bIsPartial;

		virtual bool IsCompleteForReceive() const override;

	protected:
		bool UpdateSplitBytesFromFullBytes();
		virtual void DeserializeForIndex(const quark::value& Value, uint16 PropertyId, uint8 Index);
		uint8 MaxLength;
		// #todo: instead of uniqueptrs, directly can keep fixed number of properties in memory
		std::vector<std::unique_ptr<FHScaleProperty>> SplitBytes;
		std::vector<uint8> FullBuffer;
		uint8 Count;
		uint8 OnReceiveHashID = UINT8_MAX;
		uint8 HashForSend = UINT8_MAX;
	};

	class FHScaleObjectDataChunkProperty : public FHScaleSplitByteProperty
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::ObjectPtrChunkData)

		FHScaleObjectDataChunkProperty();

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override;
		virtual void Deserialize_R(const quark::value& Value, const uint16 PropertyId) override;
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;
		void SerializeChunk(FHScaleOuterChunk& Chunk, FHScaleOuterChunk* NextChunk, const uint8 CurIndex, const uint16 PropertyId);
		void Clear();

		bool bIsContinued;

		uint8 ExportFlags;
		uint64 NextValue;
		FString ObjectName;
	};

	class FHScaleObjectDataProperty : public FHScaleProperty
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::ObjectPtrData)

		FHScaleObjectDataProperty(const uint8 MaxLength);

		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override;
		virtual void Deserialize(const quark::value& CachedValue, const uint16 PropertyId, const uint64 Timestamp) override;
		virtual void Deserialize_R(const quark::value& Value, const uint16 PropertyId) override;
		virtual FString ToDebugString() override;
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& Cmd) override;

		virtual bool SerializeChunks(TArray<FHScaleOuterChunk>& OuterChunks, const uint16 PropertyId, const FNetworkGUID& NetGUID, const FHScaleNetGUID& HSNetGUID);
		virtual void DeserializeChunks(TArray<FHScaleOuterChunk>& OuterChunks);
		virtual void WriteOut(FArchive& Ar);

		bool IsValid() const;
		virtual bool IsCompleteForReceive() const override;
		virtual uint8 NumProps() const override { return Count; }
		bool bIsPartial;
		FNetworkGUID NetworkGUID;
		FHScaleNetGUID HScaleNetGUID;

	protected:
		uint8 MaxLength;
		std::vector<std::unique_ptr<FHScaleProperty>> SplitChunks;
		std::unique_ptr<FHScaleProperty> DynamicProperty;
		uint8 Count;
		bool bIsValid;
	};

	class FHScaleNonApplicationProperty : public FHScaleProperty
	{
	public:
		virtual bool SerializeUE(FArchive& Ar, const FRepLayoutCmd& UEProperty) override
		{
			checkf(false, TEXT("For non application property unreal serialization not allowed"))
			return false;
		}
	};

	template<typename T>
	class THScaleNonApplicationProperty_S : public FHScaleNonApplicationProperty
	{
	public:
		virtual void Serialize(TArray<FHScaleAttributesUpdate>& Attributes, uint16 PropertyId) override
		{
			Attributes.Push({PropertyId, Value});
		}

		virtual void Deserialize_R(const quark::value& CachedValue, const uint16 PropertyId) override
		{
			Value = CachedValue.as<T>().value();
		}

		void SetValue(const T& NewValue)
		{
			this->Value = NewValue;
		}

		T GetValue() const
		{
			return Value;
		}

	public:
		virtual uint8 NumProps() const override { return 1; }

	protected:
		T Value;
	};

	class FHScalePositionSystemProperty : public THScaleNonApplicationProperty_S<quark::vec3>
	{
		OVERRIDE_HSCALE_TYPE(EHScaleMemoryTypeId::SystemPosition)

		bool SerializePosition(FArchive& Ar);

		virtual FString ToDebugString() override
		{
			return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f"), Value.x, Value.y, Value.z);
		}
	};
}