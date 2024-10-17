#pragma once


#define HS_APPLICATION_ATTRIBUTES_OFFSET 2000
#define HS_RESERVED_ATTRIBUTES_OFFSET 1000

#define HS_SPLIT_STRING_CLASSES 0
#define HS_SPLIT_PROPERTY_MAX_LENGTH 10

// Plugin Reserved properties classes start range
#define HS_RESERVED_OBJECT_FLAGS_ATTRIBUTE_ID (HS_RESERVED_ATTRIBUTES_OFFSET + 1)
#define HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID (HS_RESERVED_ATTRIBUTES_OFFSET + 2)
#define HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID (HS_RESERVED_ATTRIBUTES_OFFSET + 3)

#define HS_RESERVED_OBJECT_ARCHETYPE_OFFSET (HS_RESERVED_ATTRIBUTES_OFFSET + 100)
#define HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID HS_RESERVED_OBJECT_ARCHETYPE_OFFSET

#define HS_RESERVED_OBJECT_PATH_OFFSET (HS_RESERVED_ATTRIBUTES_OFFSET + 200)
#define HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID HS_RESERVED_OBJECT_PATH_OFFSET

// Application properties ranges
#define HS_APPLICATION_SPLIT_STRINGS_OFFSET 5000
#define HS_APPLICATION_SPLIT_BYTES_OFFSET 6000
#define HS_APPLICATION_OUTER_OFFSET 7000
#define HS_APPLICATION_OUTER_SIZE 100

#define HS_APPLICATION_EVENTS_OFFSET HS_APPLICATION_ATTRIBUTES_OFFSET
#define HS_RESERVED_EVENTS_OFFSET HS_RESERVED_ATTRIBUTES_OFFSET

#include "quark.h"
#include "Net/RepLayout.h"

enum class EHScaleMemoryTypeId : uint16
{
	// Quark properties reserved for first 8 bits
	None = static_cast<uint16>(quark::value_type::none),
	Boolean = static_cast<uint16>(quark::value_type::bool_),
	Uint8 = static_cast<uint16>(quark::value_type::uint8),
	Uint16 = static_cast<uint16>(quark::value_type::uint16),
	Uint32 = static_cast<uint16>(quark::value_type::uint32),
	Uint64 = static_cast<uint16>(quark::value_type::uint64),
	Int8 = static_cast<uint16>(quark::value_type::int8),
	Int16 = static_cast<uint16>(quark::value_type::int16),
	Int32 = static_cast<uint16>(quark::value_type::int32),
	Int64 = static_cast<uint16>(quark::value_type::int64),
	Float32 = static_cast<uint16>(quark::value_type::float32),
	Float64 = static_cast<uint16>(quark::value_type::float64),
	String = static_cast<uint16>(quark::value_type::string),
	Bytes = static_cast<uint16>(quark::value_type::bytes),
	Vec2 = static_cast<uint16>(quark::value_type::vec2),
	Vec3 = static_cast<uint16>(quark::value_type::vec3),
	Vec2d = static_cast<uint16>(quark::value_type::vec2d),
	Vec3d = static_cast<uint16>(quark::value_type::vec3d),
	Vec4 = static_cast<uint16>(quark::value_type::vec4),
	Vec4d = static_cast<uint16>(quark::value_type::vec4d),

	// Plugin defined attributes, start from MAX value of uint8+1
	Undefined = 256,
	SystemPosition,
	Owner,
	SplitString,
	SplitByte,
	ObjectPtrChunkData,
	ObjectPtrData,
	Default,
};

class FHScalePropertyIdConverters
{
public:
	static uint16 GetAppEventIdFromHandle(const uint16 Handle)
	{
		return Handle + HS_APPLICATION_EVENTS_OFFSET;
	}

	static uint16 GetHandleFromAppEventId(const uint16 EventId)
	{
		check(EventId >= HS_APPLICATION_EVENTS_OFFSET)
		return EventId - HS_APPLICATION_EVENTS_OFFSET;
	}

	static bool IsReservedEvent(const uint16 EventId)
	{
		return EventId < HS_APPLICATION_EVENTS_OFFSET && EventId >= HS_RESERVED_EVENTS_OFFSET;
	}

	static bool IsSystemEvent(const uint16_t EventId)
	{
		return EventId < HS_RESERVED_EVENTS_OFFSET;
	}

	static bool IsApplicationEvent(const uint16_t EventId)
	{
		return EventId >= HS_APPLICATION_EVENTS_OFFSET;
	}

	static uint16 GetAppPropertyIdFromHandle(const uint16 PropertyHandle)
	{
		return PropertyHandle + HS_APPLICATION_ATTRIBUTES_OFFSET;
	}

	static uint16 GetPropertyHandleFromPropertyId(const uint16 PropertyId)
	{
		check(PropertyId >= HS_APPLICATION_ATTRIBUTES_OFFSET)
		return PropertyId - HS_APPLICATION_ATTRIBUTES_OFFSET;
	}

	static bool IsReservedProperty(const uint16 PropertyId)
	{
		return PropertyId < HS_APPLICATION_ATTRIBUTES_OFFSET && PropertyId >= HS_RESERVED_ATTRIBUTES_OFFSET;
	}

	static bool IsSystemProperty(const uint16_t EventId)
	{
		return EventId < HS_RESERVED_ATTRIBUTES_OFFSET;
	}

	static bool IsApplicationProperty(const uint16_t PropertyId)
	{
		return !IsSystemProperty(PropertyId) && !IsReservedProperty(PropertyId);
	}

	static bool IsApplicationSplitStringPropertyRange(const uint16 PropertyId)
	{
		return PropertyId >= HS_APPLICATION_SPLIT_STRINGS_OFFSET && PropertyId < HS_APPLICATION_SPLIT_BYTES_OFFSET;
	}

	static bool IsApplicationSplitBytePropertyRange(const uint16 PropertyId)
	{
		return PropertyId >= HS_APPLICATION_SPLIT_BYTES_OFFSET && PropertyId < HS_APPLICATION_OUTER_OFFSET;
	}

	static bool IsApplicationObjectPtrPropertyRange(const uint16 PropertyId)
	{
		return PropertyId >= HS_APPLICATION_OUTER_OFFSET;
	}

	static uint16 GetSplitStringOffsetFromPropertyId(const uint16 PropertyId, const uint8 MaxLength)
	{
		check(!IsSystemProperty(PropertyId))
		// if reserved property return the propertyId as offset, there is no need of offsetting with some values
		if (IsReservedProperty(PropertyId)) { return PropertyId; }

		const uint16 Handle = GetPropertyHandleFromPropertyId(PropertyId);

		return HS_APPLICATION_SPLIT_STRINGS_OFFSET + Handle * MaxLength;
	}

	static uint16 GetSplitBytesOffsetFromPropertyId(const uint16 PropertyId, const uint8 MaxLength)
	{
		check(!IsSystemProperty(PropertyId))
		// if reserved property return the propertyId as offset, there is no need of offsetting with some values
		if (IsReservedProperty(PropertyId)) { return PropertyId; }

		const uint16 Handle = GetPropertyHandleFromPropertyId(PropertyId);

		return HS_APPLICATION_SPLIT_BYTES_OFFSET + Handle * MaxLength;
	}

	static uint16 GetOuterOffsetFromPropertyId(const uint16 PropertyId, const uint8 MaxLength)
	{
		check(!IsSystemProperty(PropertyId))
		// if reserved property return the propertyId as offset, there is no need of offsetting with some values
		if (IsReservedProperty(PropertyId)) { return PropertyId; }

		const uint16 Handle = GetPropertyHandleFromPropertyId(PropertyId);

		return HS_APPLICATION_OUTER_OFFSET + Handle * HS_APPLICATION_OUTER_SIZE;
	}

	static uint16 GetOuterChunkOffsetFromOffsetAndIndex(const uint16 OuterOffset, const uint8 Index)
	{
		return OuterOffset + Index * HS_SPLIT_PROPERTY_MAX_LENGTH;
	}

	static uint16 GetNextOuterAttrId(uint8 CurIndex, uint16 PropertyId)
	{
		uint16 Offset = GetOuterOffsetFromPropertyId(PropertyId, HS_SPLIT_PROPERTY_MAX_LENGTH);
		return GetOuterChunkOffsetFromOffsetAndIndex(Offset, CurIndex + 1);
	}

	static uint8 GetSplitPropertyIndexFromPropertyId(const uint16 PropertyId, const uint8 MaxLength)
	{
		check(!IsSystemProperty(PropertyId))
		// if reserved property return the propertyId as offset, there is no need of offsetting with some values
		if (IsReservedProperty(PropertyId)) { return PropertyId % MaxLength; }

		if (IsApplicationSplitStringPropertyRange(PropertyId))
		{
			return (PropertyId - HS_APPLICATION_SPLIT_STRINGS_OFFSET) % MaxLength;
		}

		return (PropertyId - HS_APPLICATION_SPLIT_BYTES_OFFSET) % MaxLength;
	}

	static uint8 GetOuterPropertyIndexFromPropertyId(const uint16 PropertyId)
	{
		if (IsApplicationObjectPtrPropertyRange(PropertyId))
		{
			return (PropertyId - HS_APPLICATION_OUTER_OFFSET) % HS_APPLICATION_OUTER_SIZE / HS_SPLIT_PROPERTY_MAX_LENGTH;
		}

		if (IsEntityArchetypeRange(PropertyId))
		{
			return (PropertyId - HS_RESERVED_OBJECT_ARCHETYPE_OFFSET) % HS_APPLICATION_OUTER_SIZE / HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		if (IsEntityObjectPathRange(PropertyId))
		{
			return (PropertyId - HS_RESERVED_OBJECT_PATH_OFFSET) % HS_APPLICATION_OUTER_SIZE / HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		return 0;
	}

	static uint8 GetOuterChunkPropertyIndexFromPropertyId(const uint16 PropertyId)
	{
		if (IsApplicationObjectPtrPropertyRange(PropertyId))
		{
			return ((PropertyId - HS_APPLICATION_OUTER_SIZE) % HS_APPLICATION_OUTER_SIZE) % HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		if (IsEntityArchetypeRange(PropertyId))
		{
			return ((PropertyId - HS_RESERVED_OBJECT_ARCHETYPE_OFFSET) % HS_APPLICATION_OUTER_SIZE) % HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		if (IsEntityObjectPathRange(PropertyId))
		{
			return ((PropertyId - HS_RESERVED_OBJECT_PATH_OFFSET) % HS_APPLICATION_OUTER_SIZE) % HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		return 0;
	}

	static uint16 GetEquivalentPropertyId(const uint16 PropertyId)
	{
		if (IsReservedProperty(PropertyId))
		{
			if (IsEntityArchetypeRange(PropertyId))
			{
				return HS_RESERVED_OBJECT_ARCHETYPE_ATTRIBUTE_ID;
			}

			if (IsEntityObjectPathRange(PropertyId))
			{
				return HS_RESERVED_OBJECT_PATH_ATTRIBUTE_ID;
			}
		}

		if (IsApplicationSplitStringPropertyRange(PropertyId))
		{
			return HS_APPLICATION_ATTRIBUTES_OFFSET + (PropertyId - PropertyId % HS_SPLIT_PROPERTY_MAX_LENGTH - HS_APPLICATION_SPLIT_STRINGS_OFFSET) / HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		if (IsApplicationSplitBytePropertyRange(PropertyId))
		{
			return HS_APPLICATION_ATTRIBUTES_OFFSET + (PropertyId - PropertyId % HS_SPLIT_PROPERTY_MAX_LENGTH - HS_APPLICATION_SPLIT_BYTES_OFFSET) / HS_SPLIT_PROPERTY_MAX_LENGTH;
		}
		if (IsApplicationObjectPtrPropertyRange(PropertyId))
		{
			return HS_APPLICATION_ATTRIBUTES_OFFSET + (PropertyId - PropertyId % HS_APPLICATION_OUTER_SIZE - HS_APPLICATION_OUTER_OFFSET) / HS_APPLICATION_OUTER_SIZE;
		}
		return PropertyId;
	}

	static bool IsEntityObjectPathRange(const uint16 PropertyId)
	{
		return PropertyId < HS_RESERVED_OBJECT_PATH_OFFSET + HS_APPLICATION_OUTER_SIZE
		       && PropertyId >= HS_RESERVED_OBJECT_PATH_OFFSET;
	}

	static bool IsEntityArchetypeRange(const uint16 PropertyId)
	{
		return PropertyId < HS_RESERVED_OBJECT_ARCHETYPE_OFFSET + HS_APPLICATION_OUTER_SIZE
		       && PropertyId >= HS_RESERVED_OBJECT_ARCHETYPE_OFFSET;
	}

	static EHScaleMemoryTypeId GetQuarkToMemoryLayerMapping(quark::value_type TypeId)
	{
		switch (TypeId)
		{
			case quark::value_type::none:
				return EHScaleMemoryTypeId::None;

			case quark::value_type::bool_:
				return EHScaleMemoryTypeId::Boolean;

			case quark::value_type::uint8:
				return EHScaleMemoryTypeId::Uint8;

			case quark::value_type::uint16:
				return EHScaleMemoryTypeId::Uint16;

			case quark::value_type::uint32:
				return EHScaleMemoryTypeId::Uint32;

			case quark::value_type::uint64:
				return EHScaleMemoryTypeId::Uint64;

			case quark::value_type::int8:
				return EHScaleMemoryTypeId::Int8;

			case quark::value_type::int16:
				return EHScaleMemoryTypeId::Int16;

			case quark::value_type::int32:
				return EHScaleMemoryTypeId::Int32;

			case quark::value_type::int64:
				return EHScaleMemoryTypeId::Int64;

			case quark::value_type::float32:
				return EHScaleMemoryTypeId::Float32;

			case quark::value_type::float64:
				return EHScaleMemoryTypeId::Float64;

			case quark::value_type::string:
				return EHScaleMemoryTypeId::String;

			case quark::value_type::bytes:
				return EHScaleMemoryTypeId::Bytes;

			case quark::value_type::vec2:
				return EHScaleMemoryTypeId::Vec2;

			case quark::value_type::vec2d:
				return EHScaleMemoryTypeId::Vec2d;

			case quark::value_type::vec3:
				return EHScaleMemoryTypeId::Vec3;

			case quark::value_type::vec3d:
				return EHScaleMemoryTypeId::Vec3d;

			case quark::value_type::vec4:
				return EHScaleMemoryTypeId::Vec4;

			case quark::value_type::vec4d:
				return EHScaleMemoryTypeId::Vec4d;
			default:
				return EHScaleMemoryTypeId::Undefined;
		}
	}

	static EHScaleMemoryTypeId GetRepLayoutCmdToHScaleMemoryTypeId(const FRepLayoutCmd& Cmd)
	{
		switch (Cmd.Type)
		{
			case ERepLayoutCmdType::PropertyBool:
			case ERepLayoutCmdType::PropertyNativeBool:
				return EHScaleMemoryTypeId::Boolean;

			case ERepLayoutCmdType::PropertyFloat:
				return EHScaleMemoryTypeId::Float32;

			case ERepLayoutCmdType::PropertyInt:
				return EHScaleMemoryTypeId::Int32;

			case ERepLayoutCmdType::PropertyByte:
				return EHScaleMemoryTypeId::Uint8;

			case ERepLayoutCmdType::PropertyUInt32:
				return EHScaleMemoryTypeId::Uint32;

			case ERepLayoutCmdType::RepMovement:
				return EHScaleMemoryTypeId::Bytes;

			case ERepLayoutCmdType::PropertyUInt64:
				return EHScaleMemoryTypeId::Uint64;

			case ERepLayoutCmdType::PropertyObject:
			case ERepLayoutCmdType::PropertyInterface:
			case ERepLayoutCmdType::PropertyWeakObject:
				return EHScaleMemoryTypeId::ObjectPtrData;

			case ERepLayoutCmdType::PropertyString:
			case ERepLayoutCmdType::PropertyName:
				return EHScaleMemoryTypeId::SplitString;

			case ERepLayoutCmdType::Property:
			{
				if (Cmd.Property->IsA(FDoubleProperty::StaticClass()))
				{
					return EHScaleMemoryTypeId::Float64;
				}

				if (Cmd.Property->IsA(FInt64Property::StaticClass()))
				{
					return EHScaleMemoryTypeId::Int64;
				}

				if (Cmd.Property->IsA(FUInt16Property::StaticClass()))
				{
					return EHScaleMemoryTypeId::Uint16;
				}

				if (Cmd.Property->IsA(FInt16Property::StaticClass()))
				{
					return EHScaleMemoryTypeId::Int16;
				}

				if (Cmd.Property->IsA(FTextProperty::StaticClass()))
				{
					return EHScaleMemoryTypeId::SplitString;
				}
				if (Cmd.Property->IsA(FStructProperty::StaticClass()))
				{
					FStructProperty* StructProp = CastField<FStructProperty>(Cmd.Property);
					if (StructProp->Struct->StructFlags & STRUCT_NetSerializeNative)
					{
						return EHScaleMemoryTypeId::SplitByte;
					}
				}
			}
			case ERepLayoutCmdType::PropertyVector:
			case ERepLayoutCmdType::PropertyRotator:
			case ERepLayoutCmdType::PropertyVector100:
			case ERepLayoutCmdType::PropertyVector10:
			case ERepLayoutCmdType::PropertyVectorNormal:
			case ERepLayoutCmdType::PropertyVectorQ:
				return EHScaleMemoryTypeId::Vec3d;

			case ERepLayoutCmdType::PropertyPlane:
				return EHScaleMemoryTypeId::Vec4d;

			case ERepLayoutCmdType::PropertyNetId:
				return EHScaleMemoryTypeId::SplitByte;

			case ERepLayoutCmdType::PropertySoftObject:
			case ERepLayoutCmdType::DynamicArray:
				return EHScaleMemoryTypeId::Default;

			case ERepLayoutCmdType::NetSerializeStructWithObjectReferences:
			case ERepLayoutCmdType::Return:
			default:
				return EHScaleMemoryTypeId::Undefined;
		}
	}

	static EHScaleMemoryTypeId FetchNonApplicationMemoryTypeIdFromPropertyId(const uint16 PropertyId)
	{
		check(!IsApplicationProperty(PropertyId))

		if (IsSystemProperty(PropertyId))
		{
			if (PropertyId == QUARK_KNOWN_ATTRIBUTE_POSITION)
			{
				return EHScaleMemoryTypeId::SystemPosition;
			}
			if (PropertyId == QUARK_KNOWN_ATTRIBUTE_CLASS_ID)
			{
				return EHScaleMemoryTypeId::Uint64;
			}

			if (PropertyId == QUARK_KNOWN_ATTRIBUTE_OWNER_ID)
			{
				return EHScaleMemoryTypeId::Owner;
			}
			return EHScaleMemoryTypeId::Undefined;
		}

		// for reserved properties
		if (IsReservedProperty(PropertyId))
		{
			if (PropertyId == HS_RESERVED_OBJECT_FLAGS_ATTRIBUTE_ID)
			{
				return EHScaleMemoryTypeId::Uint16;
			}
			if (IsEntityArchetypeRange(PropertyId))
			{
				return EHScaleMemoryTypeId::ObjectPtrData;
			}

			if (IsEntityObjectPathRange(PropertyId))
			{
				return EHScaleMemoryTypeId::ObjectPtrData;
			}

			if (PropertyId == HS_RESERVED_STRUCT_LINK_ATTRIBUTE_ID)
			{
				return EHScaleMemoryTypeId::Uint16;
			}

			if (PropertyId == HS_RESERVED_DYN_ARRAY_SIZE_ATTRIBUTE_ID)
			{
				return EHScaleMemoryTypeId::Uint16;
			}
		}

		return EHScaleMemoryTypeId::Undefined;
	}

	static EHScaleMemoryTypeId FetchMemoryTypeIdForPropertyIdOnReceive(const uint16 PropertyId, const quark::value_type TypeId)
	{
		// const uint16 EqPropertyId = GetEquivalentPropertyIdOnReceive(PropertyId);

		if (!IsApplicationProperty(PropertyId))
		{
			return FetchNonApplicationMemoryTypeIdFromPropertyId(PropertyId);
		}
		if (IsApplicationSplitStringPropertyRange(PropertyId))
		{
			return EHScaleMemoryTypeId::SplitString;
		}
		if (IsApplicationSplitBytePropertyRange(PropertyId))
		{
			return EHScaleMemoryTypeId::SplitByte;
		}
		if (IsApplicationObjectPtrPropertyRange(PropertyId))
		{
			return EHScaleMemoryTypeId::ObjectPtrData;
		}

		return GetQuarkToMemoryLayerMapping(TypeId);
	}
};