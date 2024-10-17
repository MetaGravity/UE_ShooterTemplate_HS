// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleSchemaData_Root.h"
#include "Core/HScaleCommons.h"
#include "UObject/Object.h"
#include "HScaleSchema.generated.h"

/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleSchema : public UObject
{
	GENERATED_BODY()

	friend class UHScaleSchemaRequest;

	UPROPERTY()
	TObjectPtr<UHScaleSchemaData_Root> SchemaData;

protected:
	virtual void Initialize(TSharedPtr<FJsonObject> Data);

public:
	virtual bool CanReplicateActor(const AActor* Actor) const { return true; }

	virtual EHScale_Ownership GetObjectOwnershipValue_ByClass(const TSubclassOf<UObject> InClass) const;
	virtual EHScale_Ownership GetObjectOwnershipValue_ById(const HSClassId InClassId) const;
	
	virtual EHScale_Blend GetObjectBlendModeValue_ByClass(const TSubclassOf<UObject> InClass) const;
	virtual EHScale_Blend GetObjectBlendModeValue_ById(const HSClassId InClassId) const;

	virtual EHScale_Lifetime GetObjectLifetimeValue_ByClass(const TSubclassOf<UObject> InClass) const;
	virtual EHScale_Lifetime GetObjectLifetimeValue_ById(const HSClassId InClassId) const;
	
private:
	void ParseSchemaData(TSharedPtr<FJsonObject> SchemaData);
};