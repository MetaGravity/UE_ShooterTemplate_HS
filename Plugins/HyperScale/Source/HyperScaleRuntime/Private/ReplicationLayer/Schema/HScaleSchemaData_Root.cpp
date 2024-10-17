// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaData_Root.h"

void UHScaleSchemaData_Root::Parse(TSharedPtr<FJsonObject> Data)
{
	if (!Data)
	{
		return;
	}

	if (Data->HasTypedField<EJson::Object>("meta"))
	{
		Meta = NewObject<UHScaleSchemaData_Meta>(this);
		Meta->Parse(Data->GetObjectField("meta"));
	}

	if (Data->HasTypedField<EJson::Array>("objects"))
	{
		const TArray<TSharedPtr<FJsonValue>>& ArrayData = Data->GetArrayField("objects");
		Objects.Reserve(ArrayData.Num());
		for (const TSharedPtr<FJsonValue> Item : ArrayData)
		{
			if (!Item) continue;
			if (Item->Type != EJson::Object) continue;

			UHScaleSchemaData_Object* ItemObject = NewObject<UHScaleSchemaData_Object>(this);
			ItemObject->Parse(Item->AsObject());

			Objects.Add(ItemObject->GetClassId(), ItemObject);
		}
	}
}

const UHScaleSchemaData_Object* UHScaleSchemaData_Root::GetObjectById(const int64 ClassId) const
{
	const TObjectPtr<UHScaleSchemaData_Object>* Result = Objects.Find(ClassId);
	return Result ? *Result : nullptr;
}
