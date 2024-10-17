// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/Schema/HScaleSchema.h"

#include "BookKeeper/HSClassTranslator.h"

void UHScaleSchema::Initialize(TSharedPtr<FJsonObject> Data)
{
	ParseSchemaData(Data);

	// #todo ... maybe somehow to cache the data
}

EHScale_Ownership UHScaleSchema::GetObjectOwnershipValue_ByClass(const TSubclassOf<UObject> InClass) const
{
	if (!InClass)
	{
		return EHScale_Ownership::Creator;
	}

	const HSClassId ClassID = FHSClassTranslator::GetInstance().GetClassId(InClass);
	const EHScale_Ownership Result = GetObjectOwnershipValue_ById(ClassID);
	return Result;
}

EHScale_Ownership UHScaleSchema::GetObjectOwnershipValue_ById(const HSClassId InClassId) const
{
	constexpr EHScale_Ownership DefaultValue = EHScale_Ownership::Creator;

	if (!IsValid(SchemaData))
	{
		// If server doesnt have any scheme data, then return just default value
		return DefaultValue;
	}

	const UHScaleSchemaData_Object* ObjectData = SchemaData->GetObjectById(InClassId);
	if (!IsValid(ObjectData))
	{
		// Object is not defined in the scheme, return the default value
		return DefaultValue;
	}

	const EHScale_Ownership Result = ObjectData->GetOwnership();
	return Result;
}

EHScale_Blend UHScaleSchema::GetObjectBlendModeValue_ByClass(const TSubclassOf<UObject> InClass) const
{
	if (!InClass)
	{
		return EHScale_Blend::Owner;
	}

	const HSClassId ClassID = FHSClassTranslator::GetInstance().GetClassId(InClass);
	const EHScale_Blend Result = GetObjectBlendModeValue_ById(ClassID);
	return Result;
}

EHScale_Blend UHScaleSchema::GetObjectBlendModeValue_ById(HSClassId InClassId) const
{
	constexpr EHScale_Blend DefaultValue = EHScale_Blend::Select;

	if (!IsValid(SchemaData))
	{
		// If server doesnt have any scheme data, then return just default value
		return DefaultValue;
	}

	const UHScaleSchemaData_Object* ObjectData = SchemaData->GetObjectById(InClassId);
	if (!IsValid(ObjectData))
	{
		// Object is not defined in the scheme, return the default value
		return DefaultValue;
	}

	const EHScale_Blend Result = ObjectData->GetBlendMode();
	return Result;
}

EHScale_Lifetime UHScaleSchema::GetObjectLifetimeValue_ByClass(const TSubclassOf<UObject> InClass) const
{
	if (!InClass)
	{
		return EHScale_Lifetime::Owner;
	}

	const HSClassId ClassID = FHSClassTranslator::GetInstance().GetClassId(InClass);
	const EHScale_Lifetime Result = GetObjectLifetimeValue_ById(ClassID);
	return Result;
}

EHScale_Lifetime UHScaleSchema::GetObjectLifetimeValue_ById(const HSClassId InClassId) const
{
	constexpr EHScale_Lifetime DefaultValue = EHScale_Lifetime::Owner;

	if (!IsValid(SchemaData))
	{
		// If server doesnt have any scheme data, then return just default value
		return DefaultValue;
	}

	const UHScaleSchemaData_Object* ObjectData = SchemaData->GetObjectById(InClassId);
	if (!IsValid(ObjectData))
	{
		// Object is not defined in the scheme, return the default value
		return DefaultValue;
	}

	const EHScale_Lifetime Result = ObjectData->GetLifetime();
	return Result;
}

void UHScaleSchema::ParseSchemaData(TSharedPtr<FJsonObject> Data)
{
	if (Data)
	{
		SchemaData = NewObject<UHScaleSchemaData_Root>();
		SchemaData->Parse(Data);
	}
}