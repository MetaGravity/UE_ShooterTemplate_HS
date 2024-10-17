// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/HScaleResources.h"
#include "Engine/DataAsset.h"
#include "HScaleSchemaDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FHScaleSchemaObject
{
	GENERATED_BODY()

	uint64 ClassId{0};
	TOptional<uint64> ParentClassId;
	TOptional<FSoftClassPath> Class;

	/** If set to true, the object is deemed strict and only the attributes specified can be propagated */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	uint8 bStrict : 1 {false};

	/**
	 * If set to true, the object is deemed global and relevant to everyone
	 * (that means, when player will join a game, the server sends the object always on start)
	 * @warning - In this case, the related actor should be set as bAlwaysRelevant = true
	 */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	uint8 bGlobal : 1 {false};

	/**
	 * Specifies the object owner in the beginning. If the object has no owner, its ownership can be acquired by any player
	 * The default value is Creator
	 */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	EHScale_Ownership Ownership;

	/** Specifies the object server lifetime */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	EHScale_Lifetime Lifetime;

	/** Specifies the blending method for object on server site */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	EHScale_Blend Blend;

	/** Object tags can be used to search only for objects with specific tags on server site */
	UPROPERTY(BlueprintReadWrite, VisibleDefaultsOnly)
	TSet<FName> Tags;

public:
	bool IsValid() const { return ClassId > 0; }
	
	const FString& GetOwnershipStr() const
	{
		if (Ownership == EHScale_Ownership::None)
		{
			static FString None = TEXT("none");
			return None;
		}

		static FString Creator = TEXT("creator");
		return Creator;
	}

	const FString& GetLifetimeStr() const
	{
		if (Lifetime == EHScale_Lifetime::Detached)
		{
			static FString Detached = TEXT("detached");
			return Detached;
		}

		static FString Owner = TEXT("owner");
		return Owner;
	}

	const FString& GetBlendStr() const
	{
		if (Blend == EHScale_Blend::Average)
		{
			static FString Average = TEXT("average");
			return Average;
		}

		if (Blend == EHScale_Blend::Select)
		{
			static FString Select = TEXT("select");
			return Select;
		}

		static FString Owner = TEXT("owner");
		return Owner;
	}
};


/**
 * Asset contains all the 
 */
UCLASS(DisplayName= "Hyperscale Schema")
class HYPERSCALERUNTIME_API UHScaleSchemaDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

	// ~Begin of UObject interface	
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	// ~End of UObject interface

protected:
	virtual void UpdateSchemaContent();

public:
	void ExtractData();

	UFUNCTION(BlueprintCallable)
	FHScaleSchemaObject GetClassData(const FSoftClassPath ClassId);

	UFUNCTION(BlueprintCallable)
	void SetClassData(const FSoftClassPath InClass, const FHScaleSchemaObject& Data);

protected:
	FHScaleSchemaObject& GetClassData_Mutable(const FSoftClassPath& InClass);
	FHScaleSchemaObject& GetClassData_Mutable(const uint64 ClassId);

public:
	static FPrimaryAssetType GetPrimaryAssetType();

private:
	UPROPERTY(VisibleDefaultsOnly, meta = (AllowPrivateAccess = "true"))
	TMap<uint64, FHScaleSchemaObject> SchemaData;
};