// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HScaleEditorInitSubsystem.generated.h"

/**
 * This subsystem is responsible for setting-up hyperscale tech for editor
 * when starting a game from top menu button
 *
 * Adds 'hscale' option to editor options list so the UHScaleWorldSubsystem
 * can read this parameter and starts the 'hscale' tech
 *
 * @warning - In build version is this subsystem disabled
 */
UCLASS()
class HYPERSCALEEDITOR_API UHScaleEditorInitSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	// ~Begin of USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// ~End of USubsystem interface

};
