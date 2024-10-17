// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HScaleStaticsLibrary.generated.h"

/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleStaticsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns True, if the game has active hyperscale session with server */
	UFUNCTION(BlueprintPure, meta = (WorldContext = WorldContextObject), Category = "Hyperscale")
	static bool IsHyperScaleNetworkingActive(const UObject* WorldContextObject);

	/** Returns list of all level roles used with active hyperscale session with server */
	UFUNCTION(BlueprintPure, meta = (WorldContext = WorldContextObject), Category = "Hyperscale|Role")
	static FGameplayTagContainer GetHyperScaleRoles(const UObject* WorldContextObject);

	/** Function that will convert level role to gameplay tag */
	UFUNCTION(BlueprintPure, Category = "Hyperscale|Role")
	static FGameplayTag GetHyperscaleRoleTagFromName(const FName RoleName);

	/** Function takes role tag and converts it into role name */
	UFUNCTION(BlueprintPure, Category = "Hyperscale|Role")
	static FName GetNameFromHyperscaleRoleTag(const FGameplayTag RoleTag);

	/** Returns the root tag of HyperScale role */
	UFUNCTION(BlueprintPure, Category = "Hyperscale|Role")
	static FGameplayTag GetHyperscaleRoleTag();
	
	/**
	 * Function for async nodes
	 * Checks if the object's world is running with specific role
	 * @warning - do not rename it
	 */
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category = "Hyperscale|Role")
	static bool IsRoleActive(const UObject* WorldContextObject, FName RoleName);
};
