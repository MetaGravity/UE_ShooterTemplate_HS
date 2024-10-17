#pragma once

#include "quark.h"
#include "Net/RepLayout.h"

class HYPERSCALERUNTIME_API FHScaleStatics
{
public:
	static bool IsStaticObject(const uint64 ObjId)
	{
		// checks for if first 32 bits are all 1s
		// #todo@self in future versions replace with quark low level sdk exposed fuction
		constexpr uint64_t Mask = 0xFFFFFFFF00000000;
		return (ObjId & Mask) == Mask;
	}

	static bool IsObjectDataRepCmd(const FRepLayoutCmd& Cmd)
	{
		return Cmd.Type == ERepLayoutCmdType::PropertyObject || Cmd.Type == ERepLayoutCmdType::PropertyWeakObject || Cmd.Type == ERepLayoutCmdType::PropertyInterface;
	}

	static bool AreVectorsWithinRadius(const quark::vec3d& V1, const quark::vec3d& V2, const float SqrdRadius = 0.01f)
	{
		// Calculate squared distance
		double dx = V1.x - V2.x;
		double dy = V1.y - V2.y;
		double dz = V1.z - V2.z;
		double SquaredDistance = dx * dx + dy * dy + dz * dz;

		return SquaredDistance <= SqrdRadius;
	}

	static bool AreVectorsWithinRadius(const quark::vec3& V1, const quark::vec3& V2, const float SqrdRadius = 0.01f)
	{
		// Calculate squared distance
		double dx = V1.x - V2.x;
		double dy = V1.y - V2.y;
		double dz = V1.z - V2.z;
		double SquaredDistance = dx * dx + dy * dy + dz * dz;

		return SquaredDistance <= SqrdRadius;
	}

	static bool AreVectorsWithinRadius(const quark::vec4d& V1, const quark::vec4d& V2, const float SqrdRadius = 0.01f)
	{
		// Calculate squared distance
		double dx = V1.x - V2.x;
		double dy = V1.y - V2.y;
		double dz = V1.z - V2.z;
		double dw = V1.w - V2.w;
		double SquaredDistance = dx * dx + dy * dy + dz * dz + dw * dw;

		return SquaredDistance <= SqrdRadius;
	}

	static bool AreVectorsWithinRadius(const quark::vec4& V1, const quark::vec4& V2, const float SqrdRadius = 0.01f)
	{
		// Calculate squared distance
		double dx = V1.x - V2.x;
		double dy = V1.y - V2.y;
		double dz = V1.z - V2.z;
		double dw = V1.w - V2.w;
		double SquaredDistance = dx * dx + dy * dy + dz * dz + dw * dw;

		return SquaredDistance <= SqrdRadius;
	}

	static void AppendUint64ToBuffer(const uint64_t Value, std::vector<uint8_t>& Buffer);

	static void AppendFStringToBuffer(const FString& String, std::vector<uint8>& Buffer);

	static uint64 ConvertBufferToUInt64(const std::vector<uint8>& Buffer, const size_t StartIndex = 0);

	static void ConvertBufferToFString(FString& Value, const std::vector<uint8>& Buffer, size_t Offset = 0);


	static FString PrintBytesFormat(const std::vector<uint8>& ByteBuffer);

	static bool IsPlayerOwnedObject(const uint64 ObjectId, const uint32 SessionId)
	{
		return static_cast<uint32_t>(ObjectId >> 32) == SessionId;
	}

	static bool IsClassSupportedForReplication(const UClass* Class);

	static bool IsObjectReplicated(const UObject* Object);

	static int32 GetFunctionCallspace(AActor* Actor, UFunction* Function, FFrame* Stack);
};