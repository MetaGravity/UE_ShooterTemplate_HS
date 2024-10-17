// Copyright 2024 Metagravity. All Rights Reserved.


#include "Async/HScale_SwitchOnRole.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphUtilities.h"
#include "EditorCategoryUtils.h"
#include "GameplayTagsManager.h"
#include "GraphEditorSettings.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompiler.h"
#include "KismetCompilerMisc.h"
#include "Core/HScaleStaticsLibrary.h"

#define LOCTEXT_NAMESPACE "HyperScale_SwitchOnRole"

class FKCHandler_SwitchRole : public FNodeHandlingFunctor
{
protected:
	// Locals that can be shared between each GameplayTagDescendants switch in the graph.
	struct FFunctionScopedTerms
	{
		FFunctionScopedTerms()
			: BoolTerm(nullptr) {}

		FBPTerminal* BoolTerm;
	};

	TMap<UFunction*, FFunctionScopedTerms> FunctionTermMap;

public:
	FKCHandler_SwitchRole(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext) {}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		FNodeHandlingFunctor::RegisterNets(Context, Node);

		FFunctionScopedTerms& FuncLocals = FunctionTermMap.FindOrAdd(Context.Function);
		if (!FuncLocals.BoolTerm)
		{
			FuncLocals.BoolTerm = Context.CreateLocalTerminal();
			FuncLocals.BoolTerm->Type.PinCategory = UEdGraphSchema_K2::PC_Boolean;
			FuncLocals.BoolTerm->Source = Node;
			FuncLocals.BoolTerm->Name = Context.NetNameMap->MakeValidName(Node, TEXT("CmpSuccess"));
		}
	}

	virtual void Compile(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UHScale_SwitchOnRole* SwitchNode = CastChecked<UHScale_SwitchOnRole>(Node);

		FEdGraphPinType ExpectedExecPinType;
		ExpectedExecPinType.PinCategory = UEdGraphSchema_K2::PC_Exec;

		// Make sure that the input pin is connected and valid for this block
		UEdGraphPin* ExecTriggeringPin = Context.FindRequiredPinByName(SwitchNode, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
		if ((ExecTriggeringPin == nullptr) || !Context.ValidatePinType(ExecTriggeringPin, ExpectedExecPinType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("NoValidExecutionPinForSwitch_Error", "@@ must have a valid execution pin @@").ToString(), SwitchNode, ExecTriggeringPin);
			return;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = UObject::StaticClass();

		// Make sure that the selection pin is connected and valid for this block
		UEdGraphPin* ObjectSourcePin = SwitchNode->FindPin(UEdGraphSchema_K2::PN_Self);
		if ((ObjectSourcePin == nullptr) || !Context.ValidatePinType(ObjectSourcePin, PinType))
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("NoValidSelectionPinForSwitch_Error", "@@ must have a valid execution pin @@").ToString(), SwitchNode, ObjectSourcePin);
			return;
		}

		// Generate the output impulse from this node
		UEdGraphPin* SwitchSelectionNet = FEdGraphUtilities::GetNetFromPin(ObjectSourcePin);
		FBPTerminal* SwitchSelectionTerm = Context.NetMap.FindRef(SwitchSelectionNet);

		if (SwitchSelectionTerm != nullptr)
		{
			UEdGraphPin* FuncPin = SwitchNode->GetFunctionPin();
			FBPTerminal* FuncContext = Context.NetMap.FindRef(FuncPin);

			// Pull out function to use
			UClass* FuncClass = Cast<UClass>(FuncPin->PinType.PinSubCategoryObject.Get());
			UFunction* FunctionPtr = FindUField<UFunction>(FuncClass, FuncPin->PinName);
			check(FunctionPtr);

			// Collect all pins that are relevant separately 
			TArray<UEdGraphPin*> OutputPins;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Output && !Pin->LinkedTo.IsEmpty())
				{
					OutputPins.Add(Pin);
				}
			}

			if (OutputPins.Num() > 0)
			{
				FBlueprintCompiledStatement* PrevStatement = nullptr;
				FBlueprintCompiledStatement* PrevGotoIfNotStatement = nullptr;

				// Iterate over each pin in reversed order so the actual pin execution sequence runs from top to bottom
				// This is because statements added to FlowStack execute first in last out (KCST_PushState)  
				for (int32 Index = OutputPins.Num() - 1; Index >= 0; --Index)
				{
					UEdGraphPin* Pin = OutputPins[Index];

					// Create a term for the switch case value
					FBPTerminal* CaseValueTerm = new FBPTerminal();
					Context.Literals.Add(CaseValueTerm);

					FEdGraphPinType TempPinType;
					TempPinType.PinCategory = UEdGraphSchema_K2::PC_Name;

					// Pin->PinName is the full gameplay tag.
					CaseValueTerm->Name = Pin->PinName.ToString();
					CaseValueTerm->Type = TempPinType;
					CaseValueTerm->SourcePin = Pin;
					CaseValueTerm->bIsLiteral = true;

					FFunctionScopedTerms& FuncLocals = FunctionTermMap.FindOrAdd(Context.Function);
					check(FuncLocals.BoolTerm != nullptr);

					// Call the comparison function associated with this switch node
					FBlueprintCompiledStatement& Statement = Context.AppendStatementForNode(SwitchNode);
					Statement.Type = KCST_CallFunction;
					Statement.FunctionToCall = FunctionPtr;
					Statement.FunctionContext = FuncContext;
					Statement.bIsParentContext = false;

					Statement.LHS = FuncLocals.BoolTerm;    // Result of the operation is stored in BoolTerm
					Statement.RHS.Add(SwitchSelectionTerm); // GameplayTagContainer
					Statement.RHS.Add(CaseValueTerm);       // Full Gameplay Tag

					FBlueprintCompiledStatement& IfFailTest_SucceedAtBeingEqualGoto = Context.AppendStatementForNode(SwitchNode);
					IfFailTest_SucceedAtBeingEqualGoto.Type = KCST_GotoIfNot;
					IfFailTest_SucceedAtBeingEqualGoto.LHS = FuncLocals.BoolTerm;

					// If FuncLocals.BoolTerm was true:
					UEdGraphNode* NextNode = OutputPins[Index]->LinkedTo[0]->GetOwningNode(); // Keep this line
					FBlueprintCompiledStatement& PushExecutionState = Context.AppendStatementForNode(Node);
					PushExecutionState.Type = KCST_PushState; // Add to FlowStack instead of an immediate goto
					Context.GotoFixupRequestMap.Add(&PushExecutionState, Pin);
					//~

					if (PrevStatement && PrevGotoIfNotStatement)
					{
						// This jump target causes a loop until all the pins have been evaluated
						Statement.bIsJumpTarget = true;
						PrevGotoIfNotStatement->TargetLabel = &Statement;
					}

					PrevStatement = &Statement;
					PrevGotoIfNotStatement = &IfFailTest_SucceedAtBeingEqualGoto;
				}

				FBlueprintCompiledStatement& NopStatement = Context.AppendStatementForNode(Node);
				NopStatement.Type = KCST_Nop;
				NopStatement.bIsJumpTarget = true;
				PrevGotoIfNotStatement->TargetLabel = &NopStatement;
			}

			GenerateSimpleThenGoto(Context, *Node, nullptr);
		}
		else
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("ResolveTermPassed_Error", "Failed to resolve term passed into @@").ToString(), ObjectSourcePin);
		}
	}
};

void UHScale_SwitchOnRole::PostInitProperties()
{
	Super::PostInitProperties();
	SetFunctionNameAndClass();
	SetFunctionPinDefaultObject();
}

void UHScale_SwitchOnRole::PostLoad()
{
	Super::PostLoad();
	SetFunctionPinDefaultObject();
}

void UHScale_SwitchOnRole::AllocateDefaultPins()
{
	// Input pins
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);

	UEdGraphPin* SelfPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), UEdGraphSchema_K2::PN_Self);
	SelfPin->PinFriendlyName = LOCTEXT("Target", "Target");

	// Create a new function pin
	CreateFunctionPin();

	// Output pins
	const FGameplayTag RoleTag = UHScaleStaticsLibrary::GetHyperscaleRoleTag();
	const FGameplayTagContainer TagChildren = UGameplayTagsManager::Get().RequestGameplayTagChildren(RoleTag);
	for (const FGameplayTag Tag : TagChildren)
	{
		const FName TagName = UHScaleStaticsLibrary::GetNameFromHyperscaleRoleTag(Tag);
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TagName);
	}

	Super::AllocateDefaultPins();
}

FLinearColor UHScale_SwitchOnRole::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ExecBranchNodeTitleColor;
}

FText UHScale_SwitchOnRole::GetTooltipText() const
{
	return LOCTEXT("RoleSwitchTooltip", "The node allows you to control flow based on the world hyperscale roles.");
}

FText UHScale_SwitchOnRole::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("RoleSwitchTitle", "Switch On HyperScale Role");
}

FSlateIcon UHScale_SwitchOnRole::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Switch_16x");
	return Icon;
}

void UHScale_SwitchOnRole::ReconstructNode()
{
	// IMPORTANT: First call this to ensure CreateFunctionPin will be done correctly
	SetFunctionNameAndClass();

	Super::ReconstructNode();
}

FNodeHandlingFunctor* UHScale_SwitchOnRole::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_SwitchRole(CompilerContext);
}

void UHScale_SwitchOnRole::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	const UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UHScale_SwitchOnRole::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::FlowControl);
}

UK2Node::ERedirectType UHScale_SwitchOnRole::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	// the name of the outer pin was changed and its friendly name was updated to match
	// the legacy naming. Use this to identify the change
	if (NewPin->PinName == UEdGraphSchema_K2::PN_Self && OldPin->PinName == "Target")
	{
		return ERedirectType_Name;
	}
	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

bool UHScale_SwitchOnRole::CanAddPin() const
{
	const FGameplayTagContainer RoleTagChildren = UGameplayTagsManager::Get().RequestGameplayTagChildren(UHScaleStaticsLibrary::GetHyperscaleRoleTag());

	int32 NumOfPins = 0;
	for (const UEdGraphPin* Pin : Pins)
	{
		if (!Pin) continue;;

		if (Pin->Direction == EGPD_Output)
		{
			NumOfPins++;
		}
	}

	const bool bResult = RoleTagChildren.Num() != NumOfPins;
	return bResult;
}

void UHScale_SwitchOnRole::AddInputPin()
{
	ReconstructNode();
}

UEdGraphPin* UHScale_SwitchOnRole::GetFunctionPin() const
{
	return FindPin(FunctionName);
}

void UHScale_SwitchOnRole::CreateFunctionPin()
{
	// Set properties on the function pin
	UEdGraphPin* FunctionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, FunctionClass, FunctionName);
	FunctionPin->bDefaultValueIsReadOnly = true;
	FunctionPin->bNotConnectable = true;
	FunctionPin->bHidden = true;

	const UFunction* Function = FindUField<UFunction>(FunctionClass, FunctionName);
	const bool bIsStaticFunc = Function->HasAllFunctionFlags(FUNC_Static);
	if (bIsStaticFunc)
	{
		// Wire up the self to the CDO of the class if it's not us
		if (const UBlueprint* BP = GetBlueprint())
		{
			const UClass* FunctionOwnerClass = Function->GetOuterUClass();
			if (!BP->SkeletonGeneratedClass->IsChildOf(FunctionOwnerClass))
			{
				FunctionPin->DefaultObject = FunctionOwnerClass->GetDefaultObject();
			}
		}
	}
}

void UHScale_SwitchOnRole::SetFunctionPinDefaultObject()
{
	if (UEdGraphPin* FunctionPin = FindPin(FunctionName))
	{
		FunctionPin->DefaultObject = FunctionClass->GetDefaultObject();
	}
}

void UHScale_SwitchOnRole::SetFunctionNameAndClass()
{
	FunctionName = GET_FUNCTION_NAME_CHECKED(UHScaleStaticsLibrary, IsRoleActive);
	FunctionClass = UHScaleStaticsLibrary::StaticClass();
}

bool UHScale_SwitchOnRole::UseWorldContext() const
{
	const UBlueprint* BP = GetBlueprint();
	const UClass* ParentClass = BP ? BP->ParentClass : nullptr;
	return ParentClass ? ParentClass->HasMetaDataHierarchical(FBlueprintMetadata::MD_ShowWorldContextPin) != nullptr : false;
}

#undef LOCTEXT_NAMESPACE
