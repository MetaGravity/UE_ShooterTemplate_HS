// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolbar/ToolbarHScaleButtonCommands.h"

#define LOCTEXT_NAMESPACE "FHScaleToolbarModule"

void FHScaleToolbarCommands::RegisterCommands()
{
	UI_COMMAND(PluginEnableAction, "HScaleToolbarEnableAction", "Enable/Disable Hyperscale networking", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
