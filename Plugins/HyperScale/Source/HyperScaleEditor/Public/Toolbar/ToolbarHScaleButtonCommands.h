// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ToolbarHScaleButtonStyle.h"

class FHScaleToolbarCommands : public TCommands<FHScaleToolbarCommands>
{
public:

	FHScaleToolbarCommands()
		: TCommands<FHScaleToolbarCommands>(TEXT("HScaleToolbar"), NSLOCTEXT("Contexts", "HScaleToolbar", "HScaleToolbar Plugin"), NAME_None, FHScaleToolbarStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginEnableAction;
};
