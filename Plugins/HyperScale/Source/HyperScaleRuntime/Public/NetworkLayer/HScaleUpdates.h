#pragma once
#include "quark.h"


struct FHScaleAttributesUpdate
{
	quark_attribute_id_t AttributeId;
	quark::value Value;
	quark::qos Qos = quark::qos::reliable;
};

struct FHScaleLocalUpdate
{
	bool bIsPlayer;
	quark_object_id_t ObjectId;
	TArray<FHScaleAttributesUpdate> Attributes;
};