// Copyright 2024 Metagravity. All Rights Reserved.

#include "Core/HScaleEditorInitSubsystem.h"

#include "Editor/EditorEngine.h"
#include "Kismet/GameplayStatics.h"
#include "Core/HScaleDevSettings.h"
#include "Debug/HScaleDebugLib.h"

#define MAKE_HSCALE_OPTION(Address) FString::Printf(TEXT("?%s=%s"), HYPERSCALE_LEVEL_OPTION, *Address);
#define MAKE_ROLE_OPTION(Role) FString::Printf(TEXT("?%s=%s"), HYPERSCALE_ROLE_OPTION, *Role);

void UHScaleEditorInitSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if WITH_EDITOR

	if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
	{
		FString& EditorOptions = EditorEngine->InEditorGameURLOptions;
		EditorOptions.ReplaceInline(TEXT(" "), TEXT(""));

		// ---------------------------------------------------------------------------------------------------
		// REMOVE OLD OPTIONS
		// ---------------------------------------------------------------------------------------------------

		// Ensures the editor doesn't contain any 'hscale' option
		while (UGameplayStatics::HasOption(EditorOptions, HYPERSCALE_LEVEL_OPTION))
		{
			const FString OptionValue = UGameplayStatics::ParseOption(EditorOptions, HYPERSCALE_LEVEL_OPTION);
			const FString Substring = MAKE_HSCALE_OPTION(OptionValue);

			// Removes hyperscale tech option
			EditorOptions.ReplaceInline(*Substring, TEXT(""));
		}

		while (UGameplayStatics::HasOption(EditorOptions, HYPERSCALE_ROLE_OPTION))
		{
			const FString OptionValue = UGameplayStatics::ParseOption(EditorOptions, HYPERSCALE_ROLE_OPTION);
			const FString Substring = MAKE_ROLE_OPTION(OptionValue);

			// Removes hyperscale role option
			EditorOptions.ReplaceInline(*Substring, TEXT(""));
		}

		// ---------------------------------------------------------------------------------------------------
		// ADD NEW OPTIONS
		// ---------------------------------------------------------------------------------------------------

		if (UHScaleDevSettings::GetIsHyperScaleEnabled())
		{
			// Do not add more than 1 'hscale' option into editor option string
			const FHScale_ServerConfig& SelectedServer = UHScaleDevSettings::GetEditorServerList().GetSelectedServer();
			const FString HyperscaleOption = MAKE_HSCALE_OPTION(SelectedServer.ToString());

			// Adds hyperscale tech option
			EditorOptions += HyperscaleOption;

			EditorOptions.Append("?Role=Client");
		}
	}

#endif
}

bool UHScaleEditorInitSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR

	return Super::ShouldCreateSubsystem(Outer);

#else
	
	return false;
	
#endif
}