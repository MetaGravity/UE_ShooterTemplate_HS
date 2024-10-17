// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HScaleResources.h"
#include "ReplicationLayer/HScaleReplicationResources.h"
#include "Engine/DeveloperSettings.h"
#include "HScaleDevSettings.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnabledChangedSignature, bool /* NewState */)

UCLASS(config = Game, defaultconfig, meta=(DisplayName= "HyperScale"))
class HYPERSCALERUNTIME_API UHScaleDevSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UHScaleDevSettings();

	// ~Begin of UObject interface 
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~End of UObject interface 

#if WITH_EDITORONLY_DATA
	inline static FOnEnabledChangedSignature OnEnabledChangedDelegate;
	
protected:
	/** If true, the game in editor standalone mode will starts with hyperscale networking */
	UPROPERTY(EditAnywhere, config, Category="General Settings")
	uint8 bEnableHyperScale : 1;
	
	UPROPERTY(EditAnywhere, config, meta = (EditCondition = "bEnableHyperScale"), Category="General")
	FHScale_ServerConfigList EditorServerList;
#endif

#if WITH_EDITOR

public:
	/** Returns True, if the plugin is enabled in editor by default */
	static bool GetIsHyperScaleEnabled();

	/** Sets hyperscale plugin enabled or disabled */
	static void SetHyperScaleEnabled(const bool bEnabled);

	/** Returns list of all available editor server */
	static const FHScale_ServerConfigList& GetEditorServerList();

	/** Returns mutable list of all available editor server */
	static FHScale_ServerConfigList& GetEditorServerList_Mutable();
#endif

protected:
	UPROPERTY(EditAnywhere, EditFixedSize, Config, Category="Replication Settings")
	TArray<FHScale_ReplicationClassOptions> ClassesReplicationOptions;

public:
	static const TArray<FHScale_ReplicationClassOptions>& GetClassesReplicationOptions();
};
