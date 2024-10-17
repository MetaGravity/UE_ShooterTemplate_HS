// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/HScaleReplicationLibrary.h"

#include "AbstractNavData.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "Core/HScaleResources.h"
#include "Core/HScaleStaticsLibrary.h"
#include "Engine/LevelScriptActor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "WorldPartition/WorldPartitionReplay.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"

#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScaleConnection.h"

TArray<FHScale_ReplicationClassOptions> UHScaleReplicationLibrary::BuildClassOptions()
{
	TArray<FHScale_ReplicationClassOptions> Result;

	// Classes that should be excluded from replication and reason why they are handled in other way:
	// AParticleEventManager - The class is not exported for other modules, so we have to handle this specially in game logic

	// ----------------------------------------------------------------------------------
	// STATIC ENGINE OBJECTS REPLICATION SETUP ------------------------------------------
	// ----------------------------------------------------------------------------------

	{
		FString Note = TEXT("Initialized by 'World Initializator' agent. For optimization can be set the replication rule to 'NeverReplicate' if needed. \nDo not recommend to use any other replication rules except these two.");
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(MoveTemp(Note), true, true);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(ALevelScriptActor::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_RoleReplicated>();
		ExcludedClass.ReplicationCondition.GetMutablePtr<FHyperScale_ReplicationRule_RoleReplicated>()->Role = HScaleGameplayTag::HyperScale_Role_WorldInitAgent;
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(TEXT("This is note about level script actor..."), true, true);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AWorldSettings::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_RoleReplicated>();
		ExcludedClass.ReplicationCondition.GetMutablePtr<FHyperScale_ReplicationRule_RoleReplicated>()->Role = HScaleGameplayTag::HyperScale_Role_WorldInitAgent;
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(TEXT("This is note about level script actor..."), true, true);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AWorldDataLayers::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_RoleReplicated>();
		ExcludedClass.ReplicationCondition.GetMutablePtr<FHyperScale_ReplicationRule_RoleReplicated>()->Role = HScaleGameplayTag::HyperScale_Role_WorldInitAgent;
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(TEXT("This is note about level script actor..."), true, true);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AGameStateBase::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_RoleReplicated>();
		ExcludedClass.ReplicationCondition.GetMutablePtr<FHyperScale_ReplicationRule_RoleReplicated>()->Role = HScaleGameplayTag::HyperScale_Role_WorldInitAgent;
		Result.Add(MoveTemp(ExcludedClass));
	}

	// ----------------------------------------------------------------------------------
	// DYNAMIC ENGINE OBJECTS REPLICATION SETUP -----------------------------------------
	// ----------------------------------------------------------------------------------

	{
		// All controllers are excluded, because server doesn't need to know local controllers states
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AController::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AAbstractNavData::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(APlayerCameraManager::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AHUD::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AGameNetworkManager::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		// The class is not exported from engine module, so we have to find it in this way to add it into list of classes
		UClass* ParticleEventManagerClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.ParticleEventManager"));

		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(ParticleEventManagerClass);
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AWorldPartitionReplay::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_RoleReplicated>();
		ExcludedClass.ReplicationCondition.GetMutablePtr<FHyperScale_ReplicationRule_RoleReplicated>()->Role = HScaleGameplayTag::HyperScale_Role_WorldInitAgent;
		Result.Add(MoveTemp(ExcludedClass));
	}
	{
		FHScale_ReplicationClassOptions ExcludedClass = FHScale_ReplicationClassOptions(true, false);
		ExcludedClass.ActorClass = TSoftClassPtr<AActor>(AGameplayDebuggerCategoryReplicator::StaticClass());
		ExcludedClass.ReplicationCondition = FInstancedStruct::Make<FHyperScale_ReplicationRule_NeverReplicate>();
		Result.Add(MoveTemp(ExcludedClass));
	}

	return Result;
}

FGameplayTagContainer UHScaleReplicationLibrary::GetRolesFromURL(const FURL& Url)
{
	FGameplayTagContainer Result;

	const TCHAR* Options = Url.GetOption(HYPERSCALE_ROLE_OPTION, nullptr);
	if (Options)
	{
		TArray<FString> OutArr;
		FString(Options).Mid(1).ParseIntoArray(OutArr, TEXT(","));

		for (FString& TagName : OutArr)
		{
			TagName.TrimStartInline();
			TagName.TrimEndInline();

			// Remove all dots from start or end
			while (!TagName.IsEmpty() && (TagName[0] == '.' || TagName[TagName.Len() - 1] == '.'))
			{
				if (TagName[0] == '.')
				{
					TagName.RemoveAt(0);
				}

				// Check if after last remove is still not empty
				if (!TagName.IsEmpty() && TagName[TagName.Len() - 1] == '.')
				{
					TagName.RemoveAt(TagName.Len() - 1);
				}
			}

			// If the tag is empty -> PASS
			if (TagName.IsEmpty()) continue;

			FGameplayTag FoundTag = UHScaleStaticsLibrary::GetHyperscaleRoleTagFromName(*TagName);
			if (FoundTag.IsValid())
			{
				Result.AddTag(FoundTag);
			}
		}
	}

	return Result;
}

void UHScaleReplicationLibrary::SetIDCacheLowTreshold(int32 NewTreshold)
{
	UHScaleNetDriver* netDriver = Cast<UHScaleNetDriver>(GEngine->GameViewport->GetWorld()->GetNetDriver());
	if (!netDriver) return;

	netDriver->GetHyperScaleConnection()->FreePlayerObjectIDCache.SetLowItemTreshold(NewTreshold);
}