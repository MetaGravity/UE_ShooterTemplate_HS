// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchemaDataAsset.h"

#include "ReplicationLayer/Schema/HScaleSchemaData_Object.h"
#include "UObject/ObjectSaveContext.h"


void UHScaleSchemaDataAsset::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	// Do our changes first before saving
	UpdateSchemaContent();

	Super::PreSave(ObjectSaveContext);
}

void UHScaleSchemaDataAsset::UpdateSchemaContent()
{
	// Dynamic arrays scheme data
	{
		FHScaleSchemaObject& DynamicArrayData = GetClassData_Mutable(HSCALE_DYNAMIC_ARRAY_CLASS_ID);

		DynamicArrayData.bStrict = false;
		DynamicArrayData.bGlobal = false;
		DynamicArrayData.Ownership = EHScale_Ownership::Creator;
		DynamicArrayData.Lifetime = EHScale_Lifetime::Owner; // The dynamic array object has to be destroyed automatically when the owner is destroyed
		DynamicArrayData.Blend = EHScale_Blend::Owner;
	}
}

void UHScaleSchemaDataAsset::ExtractData()
{
	// Just for be sure, there will be static data
	UpdateSchemaContent();
	
	const TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());

	TArray<TSharedPtr<FJsonValue>> Objects;
	for (auto It = SchemaData.CreateIterator(); It; ++It)
	{
		const FHScaleSchemaObject& Data = It.Value();
		if (Data.IsValid())
		{
			const TSharedPtr<FJsonObject> Object = MakeShareable(new FJsonObject());

			if(Data.Class.IsSet())
			{
				Object->SetStringField(TEXT("name"), Data.Class->GetAssetPath().GetPackageName().ToString());
			}
			
			Object->SetNumberField(TEXT("class_id"), Data.ClassId);

			if (Data.bStrict)
			{
				Object->SetBoolField(TEXT("strict"), Data.bStrict);
			}

			if (Data.bGlobal)
			{
				Object->SetBoolField(TEXT("global"), Data.bGlobal);
			}

			Object->SetStringField(TEXT("ownership"), Data.GetOwnershipStr());
			Object->SetStringField(TEXT("lifetime"), Data.GetLifetimeStr());
			Object->SetStringField(TEXT("blend"), Data.GetBlendStr());

			if (!Data.Tags.IsEmpty())
			{
				TArray<TSharedPtr<FJsonValue>> Tags;
				for (const FName Tag : Data.Tags)
				{
					TSharedPtr<FJsonValue> Value = MakeShareable(new FJsonValueString(Tag.ToString()));
					Tags.Add(Value);
				}

				Object->SetArrayField(TEXT("tags"), Tags);
			}

			// Add object to list of objects
			TSharedPtr<FJsonValue> ObjValue = MakeShareable(new FJsonValueObject(Object));
			Objects.Add(ObjValue);
		}
		else
		{
			It.RemoveCurrent();
		}
	}

	if (!Objects.IsEmpty())
	{
		Root->SetArrayField(TEXT("objects"), Objects);
	}

	// Translate JSON into string
	// ----------------------------------------

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	const bool bSerializeResult = FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	check(bSerializeResult);

	// Save data to file
	// ----------------------------------------

	static FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Schemas"), (GetName() + TEXT(".json")));
	const bool bIsFileSaved = FFileHelper::SaveStringToFile(Output, *SaveDirectory);
	check(bIsFileSaved);
}

FHScaleSchemaObject UHScaleSchemaDataAsset::GetClassData(const FSoftClassPath ClassId)
{
	return GetClassData_Mutable(ClassId);
}

void UHScaleSchemaDataAsset::SetClassData(const FSoftClassPath InClass, const FHScaleSchemaObject& Data)
{
	FHScaleSchemaObject& InData = GetClassData_Mutable(InClass);

	InData.bStrict = Data.bStrict;
	InData.bGlobal = Data.bGlobal;
	InData.Ownership = Data.Ownership;
	InData.Lifetime = Data.Lifetime;
	InData.Blend = Data.Blend;
	InData.Tags = Data.Tags;
}

FHScaleSchemaObject& UHScaleSchemaDataAsset::GetClassData_Mutable(const FSoftClassPath& InClass)
{
	const uint64 HashValue = GetTypeHash(InClass.GetAssetPath().GetPackageName().ToString());
	FHScaleSchemaObject& Data = SchemaData.FindOrAdd(HashValue);

	// If is 0, then is not yet initialized
	if (Data.ClassId == 0)
	{
		Data.ClassId = HashValue;
		Data.Class = InClass;
		// #todo ... init parent
	}

	return Data;
}

FHScaleSchemaObject& UHScaleSchemaDataAsset::GetClassData_Mutable(const uint64 ClassId)
{
	FHScaleSchemaObject& Data = SchemaData.FindOrAdd(ClassId);

	// If is 0, then is not yet initialized
	if (Data.ClassId == 0)
	{
		Data.ClassId = ClassId;
		// #todo ... init parent
	}

	return Data;
}

FPrimaryAssetType UHScaleSchemaDataAsset::GetPrimaryAssetType()
{
	return StaticClass()->GetFName();
}