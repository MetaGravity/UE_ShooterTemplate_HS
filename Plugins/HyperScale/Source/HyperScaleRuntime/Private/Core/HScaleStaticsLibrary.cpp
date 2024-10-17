// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/HScaleStaticsLibrary.h"

#include "Core/HScaleWorldSubsystem.h"
#include "ReplicationLayer/HScaleReplicationLibrary.h"

bool UHScaleStaticsLibrary::IsHyperScaleNetworkingActive(const UObject* WorldContextObject)
{
	if (!ensure(IsValid(WorldContextObject))) return false;
	if (!ensure(IsValid(WorldContextObject->GetWorld()))) return false;

	const UHScaleWorldSubsystem* HScaleSubsystem = WorldContextObject->GetWorld()->GetSubsystem<UHScaleWorldSubsystem>();
	check(HScaleSubsystem);

	return HScaleSubsystem->IsHyperScaleNetworkingActive();
}

FGameplayTagContainer UHScaleStaticsLibrary::GetHyperScaleRoles(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject)) return FGameplayTagContainer();
	if (!IsValid(WorldContextObject->GetWorld())) return FGameplayTagContainer();

	const FGameplayTagContainer Result = UHScaleReplicationLibrary::GetRolesFromURL(WorldContextObject->GetWorld()->URL);
	return Result;
}

FGameplayTag UHScaleStaticsLibrary::GetHyperscaleRoleTagFromName(const FName RoleName)
{
	if (RoleName.IsNone()) return FGameplayTag();

	const FString ParentTag = UHScaleStaticsLibrary::GetHyperscaleRoleTag().ToString();
	const FString RequestTag = FString::Printf(TEXT("%s.%s"), *ParentTag, *RoleName.ToString());
	const FGameplayTag RoleTag = UGameplayTagsManager::Get().RequestGameplayTag(*RequestTag);
	return RoleTag;
}

FName UHScaleStaticsLibrary::GetNameFromHyperscaleRoleTag(const FGameplayTag RoleTag)
{
	if (!RoleTag.IsValid()) return FName();

	const FString ParentTag = UHScaleStaticsLibrary::GetHyperscaleRoleTag().ToString();
	const FString ReplacePart = FString::Printf(TEXT("%s."), *ParentTag);

	FString TagString = RoleTag.ToString();
	const bool FoundAndRemoved = TagString.RemoveFromStart(*ReplacePart);

	FName TagName;
	if (FoundAndRemoved)
	{
		TagName = *TagString;
	}

	return TagName;
}

FGameplayTag UHScaleStaticsLibrary::GetHyperscaleRoleTag()
{
	return HScaleGameplayTag::HyperScale_Role;
}

bool UHScaleStaticsLibrary::IsRoleActive(const UObject* WorldContextObject, const FName RoleName)
{
	if (RoleName.IsNone()) return false;

	const FGameplayTagContainer WorldRoles = GetHyperScaleRoles(WorldContextObject);
	if (WorldRoles.IsEmpty()) return false;

	const FGameplayTag RoleTag = UHScaleStaticsLibrary::GetHyperscaleRoleTagFromName(RoleName);
	if (!RoleTag.IsValid()) return false;

	const bool bResult = WorldRoles.HasTag(RoleTag);
	return bResult;
}
