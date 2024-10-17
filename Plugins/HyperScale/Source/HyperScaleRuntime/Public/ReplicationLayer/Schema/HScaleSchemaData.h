// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HScaleSchemaData.generated.h"

/**
 * 
 */
UCLASS(Abstract)
class UHScaleSchemaData : public UObject
{
	GENERATED_BODY()

public:
	virtual void Parse(TSharedPtr<FJsonObject> Data) PURE_VIRTUAL(UHScale_SchemaData::Parse);
};
