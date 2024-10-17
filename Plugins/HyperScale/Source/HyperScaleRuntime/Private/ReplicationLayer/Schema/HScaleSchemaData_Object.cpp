// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaData_Object.h"

#include "Core/HScaleResources.h"

void UHScaleSchemaData_Object::Parse(TSharedPtr<FJsonObject> Data)
{
	if (!Data) return;

	Data->TryGetStringField("name", Name);
	Data->TryGetNumberField("class_id", ClassId);
	Data->TryGetBoolField("strict", bStrict);
	Data->TryGetBoolField("global", bGlobal);

	// Ownership
	{
		FString OwnerShipValue;
		Ownership = EHScale_Ownership::Creator;
		Data->TryGetStringField("ownership", OwnerShipValue);
		if (OwnerShipValue.Equals("Creator", ESearchCase::IgnoreCase))
		{
			Ownership = EHScale_Ownership::Creator;
		}
		else if (OwnerShipValue.Equals("none", ESearchCase::IgnoreCase))
		{
			Ownership = EHScale_Ownership::None;
		}
	}

	// Lifetime
	{
		FString LifetimeValue;
		Lifetime = EHScale_Lifetime::Owner;
		Data->TryGetStringField("lifetime", LifetimeValue);
		if (LifetimeValue.Equals("Owner", ESearchCase::IgnoreCase))
		{
			Lifetime = EHScale_Lifetime::Owner;
		}
		else if (LifetimeValue.Equals("Detached", ESearchCase::IgnoreCase))
		{
			Lifetime = EHScale_Lifetime::Detached;
		}
	}

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

	// Tags
	{
		const TArray<TSharedPtr<FJsonValue>>* TagsArr = nullptr;
		Data->TryGetArrayField("tags", TagsArr);
		if (TagsArr)
		{
			for (const TSharedPtr<FJsonValue> Item : *TagsArr)
			{
				if (!Item) continue;
				if (Item->Type != EJson::String) continue;

				Tags.Add(*Item->AsString());
			}
		}
	}

	// Attributes
	{
		const TArray<TSharedPtr<FJsonValue>>* AttributesArr = nullptr;
		Data->TryGetArrayField("attributes", AttributesArr);
		if (AttributesArr)
		{
			for (const TSharedPtr<FJsonValue> Item : *AttributesArr)
			{
				if (!Item) continue;
				if (Item->Type != EJson::Object) continue;

				UHScaleSchemaData_Attribute* ItemObject = NewObject<UHScaleSchemaData_Attribute>(this);
				ItemObject->Parse(Item->AsObject());

				Attributes.Add(ItemObject->GetKey(), ItemObject);
			}
		}
	}
}
