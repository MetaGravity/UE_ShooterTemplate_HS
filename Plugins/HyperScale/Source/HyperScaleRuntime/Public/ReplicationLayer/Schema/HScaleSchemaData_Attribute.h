// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HScaleSchemaData.h"
#include "HScaleSchemaData_Attribute.generated.h"

enum class EHScale_Blend : uint8;
/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleSchemaData_Attribute : public UHScaleSchemaData
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	FString Name;
	
	UPROPERTY()
	uint16 Key;

	UPROPERTY()
	bool bStream;

	UPROPERTY()
	EHScale_Blend Blend;
	
	UPROPERTY()
	FName Type;

public:
	virtual void Parse(TSharedPtr<FJsonObject> Data) override;

	
	uint16 GetKey() const { return Key; }
};
