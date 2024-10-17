// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "K2Node_AddPinInterface.h"
#include "HScale_SwitchOnRole.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class HYPERSCALEEDITOR_API UHScale_SwitchOnRole : public UK2Node, public IK2Node_AddPinInterface
{
	GENERATED_BODY()
	
	// UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	// End of UObject interface
	
	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void ReconstructNode() override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual ERedirectType DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const override;
	//~ End K2Node Interface

	//~ IK2Node_AddPinInterface
	virtual bool CanAddPin() const override;
	virtual void AddInputPin() override;
	//~ End IK2Node_AddPinInterface

public:
	UEdGraphPin* GetFunctionPin() const;
	
protected:
	void CreateFunctionPin();
	void SetFunctionPinDefaultObject();
	void SetFunctionNameAndClass();

	virtual bool UseWorldContext() const;

private:
	/* The function underpining the switch. */
	UPROPERTY()
	FName FunctionName;

	/** The class that the function is from. */
	UPROPERTY()
	TSubclassOf<UObject> FunctionClass;
};
