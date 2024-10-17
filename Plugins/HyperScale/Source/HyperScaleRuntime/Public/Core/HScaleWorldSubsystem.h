// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HScaleWorldSubsystem.generated.h"

class UHScaleNetDriver;

/**
 * This subsystem is responsible for starting hyperscale net driver
 * based on level options defined when opening new level in game
 */
UCLASS(NotBlueprintable)
class UHScaleWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

	// ~Begin of USubsystem interface
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~Begin of USubsystem interface

public:
	bool IsHyperScaleNetworkingActive() const;

private:
	/** Called when game mode is initialized */
	void HandleGameModeInitialization(AGameModeBase* GameMode);
	void HandleActorSpawned(AGameModeBase* GameMode, APlayerController* NewPlayer);

	UPROPERTY(Transient)
	TObjectPtr<UHScaleNetDriver> HyperScaleDriver;

	FDelegateHandle ActorInitHandle;	
};
