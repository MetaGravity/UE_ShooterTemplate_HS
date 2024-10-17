// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HScaleDebugLib.generated.h"

class UHScaleNetDriver;
DECLARE_LOG_CATEGORY_EXTERN(LogHScaleDebugLib, Log, All)

/**
 * 
 */
UCLASS()
class HYPERSCALEEDITOR_API UHScaleDebugLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static TSharedPtr<FJsonObject> GetEquivalentEntityProps(UObject* Object);

private:
	static TSharedPtr<FRepLayout> GetRepLayoutForObject(UObject* Object, UHScaleNetDriver* NetDriver);
};