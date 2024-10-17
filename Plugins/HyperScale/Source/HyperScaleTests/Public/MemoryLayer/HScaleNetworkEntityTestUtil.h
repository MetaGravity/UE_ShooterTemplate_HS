#pragma once
#include "MemoryLayer/HScaleNetworkEntity.h"

class FHScaleNetworkEntity;

class HScaleNetworkEntityTestUtil
{
public:
	static TSharedPtr<FHScaleNetworkEntity> CreateUnitEntity(const uint64 ObjectId, const uint64 ClassId);

	static bool CheckIsPropertyExists(const TSharedPtr<FHScaleNetworkEntity> Entity, const uint16 PropertyId);

	static FHScaleProperty* GetProperty(const TSharedPtr<FHScaleNetworkEntity> Entity, const uint16 PropertyId);
};
