// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HScaleSchemaData.h"
#include "HScaleSchemaData_Meta.h"
#include "HScaleSchemaData_Object.h"
#include "HScaleSchemaData_Root.generated.h"

/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleSchemaData_Root : public UHScaleSchemaData
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	TObjectPtr<UHScaleSchemaData_Meta> Meta;

	UPROPERTY()
	TMap<uint64, TObjectPtr<UHScaleSchemaData_Object>> Objects;

public:
	virtual void Parse(TSharedPtr<FJsonObject> Data) override;

	const UHScaleSchemaData_Meta* GetMetaData() const { return Meta; }
	const TMap<uint64, TObjectPtr<UHScaleSchemaData_Object>>& GetObjects() const { return Objects; }
	const UHScaleSchemaData_Object* GetObjectById(const int64 ClassId) const;
};
