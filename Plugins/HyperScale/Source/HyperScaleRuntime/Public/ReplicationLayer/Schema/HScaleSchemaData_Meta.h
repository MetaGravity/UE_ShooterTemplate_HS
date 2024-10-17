// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HScaleSchemaData.h"
#include "HScaleSchemaData_Meta.generated.h"

/**
 * 
 */
UCLASS()
class UHScaleSchemaData_Meta : public UHScaleSchemaData
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	FString SchemaName;

	UPROPERTY()
	FString ServerVersion;

public:
	virtual void Parse(TSharedPtr<FJsonObject> Data) override;

public:
	const FString& GetSchemaName() const { return SchemaName; }
	const FString& GetServerVersion() const { return ServerVersion; }
};
