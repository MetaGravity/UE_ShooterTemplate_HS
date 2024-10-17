// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleSchemaData.h"
#include "HScaleSchemaData_Attribute.h"
#include "Core/HScaleCommons.h"
#include "HScaleSchemaData_Object.generated.h"


enum class EHScale_Lifetime : uint8;
enum class EHScale_Ownership : uint8;

/**
 * The scheme data object contain all class definition data important for server,
 * that defines behavior for specific class on the server site
 *
 * For more detail info check this:
 * @see - https://github.com/MetaGravity/Quark/blob/develop/docs/src/schema/index.md
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleSchemaData_Object : public UHScaleSchemaData
{
	GENERATED_BODY()

	friend class UHScaleSchemaData_Root;

protected:
	/** The name of  */
	UPROPERTY()
	FString Name;

	/** Unique Class ID that can be translated by using function FHSClassTranslator::GetClassId */
	UPROPERTY()
	uint64 ClassId;

	/** If set to true, the object is deemed strict and only the attributes specified can be propagated */
	UPROPERTY()
	bool bStrict;

	/**
	 * If set to true, the object is deemed global and relevant to everyone
	 * (that means, when player will join a game, the server sends the object always on start)
	 * @warning - In this case, the related actor should be set as bAlwaysRelevant = true
	 */
	UPROPERTY()
	bool bGlobal;

	/**
	 * Specifies the object owner in the beginning. If the object has no owner, its ownership can be acquired by any player
	 * The default value is Creator
	 */
	UPROPERTY()
	EHScale_Ownership Ownership;

	/** Specifies the object server lifetime */
	UPROPERTY()
	EHScale_Lifetime Lifetime;

	/** Specifies the blending method for object on server site */
	UPROPERTY()
	EHScale_Blend Blend;

	/** Object tags can be used to search only for objects with specific tags on server site */
	UPROPERTY()
	TSet<FName> Tags;

	/** Attributes replication setup */
	UPROPERTY()
	TMap<uint16, TObjectPtr<UHScaleSchemaData_Attribute>> Attributes;

protected:
	virtual void Parse(TSharedPtr<FJsonObject> Data) override;

public:
	const FString& GetClassName() const { return Name; }
	HSClassId GetClassId() const { return ClassId; }
	bool GetIsStrict() const { return bStrict; }
	bool GetIsGlobal() const { return bGlobal; }
	EHScale_Ownership GetOwnership() const { return Ownership; }
	EHScale_Lifetime GetLifetime() const { return Lifetime; }
	EHScale_Blend GetBlendMode() const { return Blend; }
	const TSet<FName>& GetClassTags() const { return Tags; }
	const TMap<uint16, TObjectPtr<UHScaleSchemaData_Attribute>>& GetClassAttributes() const { return Attributes; }
};