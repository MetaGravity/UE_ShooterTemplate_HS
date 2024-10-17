// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaData_Attribute.h"

#include "Core/HScaleResources.h"

void UHScaleSchemaData_Attribute::Parse(TSharedPtr<FJsonObject> Data)
{
	if (!Data) return;

	Data->TryGetStringField("name", Name);
	Data->TryGetNumberField("key", Key);
	Data->TryGetBoolField("stream", bStream);

	// Blend
	{
		FString BlendValue;
		Blend = EHScale_Blend::Owner;
		Data->TryGetStringField("blend", BlendValue);
		if (BlendValue.Equals("Owner", ESearchCase::IgnoreCase))
		{
			Blend = EHScale_Blend::Owner;
		}
		else if (BlendValue.Equals("Select", ESearchCase::IgnoreCase))
		{
			Blend = EHScale_Blend::Select;
		}
	}

	// Type
	{
		FString TypeValue;
		Data->TryGetStringField("type", TypeValue);
		Type = *TypeValue;
	}
}
