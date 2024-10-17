// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "HScaleSchemaToolWidget.generated.h"

class UHScaleSchemaDataAsset;

/**
 * 
 */
UCLASS(Abstract)
class HYPERSCALEEDITOR_API UHScaleSchemaToolWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent)
	void OnConstructed();

public:
	UFUNCTION(BlueprintImplementableEvent)
	void OnClassAssigned(TSubclassOf<UObject> InObjectClass);
	

private:
	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess="true"))
	TArray<UHScaleSchemaDataAsset*> SchemaAssets;
};