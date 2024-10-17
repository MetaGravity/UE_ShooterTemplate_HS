#include "MemoryLayer/HScaleNetworkEntityTestUtil.h"

#include "MemoryLayer/HScaleNetworkEntity.h"

TSharedPtr<FHScaleNetworkEntity> HScaleNetworkEntityTestUtil::CreateUnitEntity(const uint64 ObjectId, const uint64 ClassId)
{
	return FHScaleNetworkEntity::Create(FHScaleNetGUID::Create_Object(ObjectId), ClassId);
}

bool HScaleNetworkEntityTestUtil::CheckIsPropertyExists(const TSharedPtr<FHScaleNetworkEntity> Entity, const uint16 PropertyId)
{
	if (!Entity.IsValid()) { return false; }
	return Entity->Properties.contains(PropertyId);
}

FHScaleProperty* HScaleNetworkEntityTestUtil::GetProperty(const TSharedPtr<FHScaleNetworkEntity> Entity, const uint16 PropertyId)
{
	if (!CheckIsPropertyExists(Entity, PropertyId)) { return nullptr; }
	const auto& It = Entity->Properties.find(PropertyId);
	return It->second.get();
}
