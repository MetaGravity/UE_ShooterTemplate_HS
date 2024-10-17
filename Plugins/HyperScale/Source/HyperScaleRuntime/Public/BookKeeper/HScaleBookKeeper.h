#pragma once
#include "abi.h"

class FHScaleBookKeeper
{
public:
	FHScaleBookKeeper() {}
	~FHScaleBookKeeper() = default;

private:
	TMap<FNetworkGUID, ObjectId> GUIDObjCache;
	TMap<ObjectId, FNetworkGUID> ReverseGUIDObjCache;

public:
	uint64 GetClassId(const FNetworkGUID& NetGUID);
	uint64 GetClassId(const ObjectId ObjId);

	UClass* GetClass(const uint64 ClassId);
	UClass* GetClass(const FNetworkGUID& NetGUID);

	ObjectId GetOrCreateObjectId(const FNetworkGUID& NetGUID);
};