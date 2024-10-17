// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "HScaleReplicationResources.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HScaleReplicationLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UHScaleReplicationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static TArray<FHScale_ReplicationClassOptions> BuildClassOptions();

	static FGameplayTagContainer GetRolesFromURL(const FURL& Url);

	/**
	 * Changes the ID cache size for new replicated actors.
	 * Increase it when you expect to create a lot of replicated actors/components
	 * under a server response time period.
	 * Otherwise their replication might be slightly delayed until new available IDs are received.
	 */
	UFUNCTION(BlueprintCallable, meta = (ShortToolTip = "Changes the ID cache size for new replicated actors.",
		ToolTip = "Changes the ID cache size for new replicated actors.\n\nIncrease it when you expect to create a lot of replicated actors/components under a server response time period.\nOtherwise their replication might be slightly delayed until new available IDs are received."))
	static void SetIDCacheLowTreshold(int32 NewTreshold);
};
