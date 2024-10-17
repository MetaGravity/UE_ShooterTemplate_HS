// Copyright 2024 Metagravity. All Rights Reserved.


#include "Core/HScaleDevSettings.h"

#include "ReplicationLayer/HScaleReplicationLibrary.h"


UHScaleDevSettings::UHScaleDevSettings()
{
	CategoryName = FName(TEXT("Plugins"));
	SectionName = FName(TEXT("HyperScale"));

#if WITH_EDITORONLY_DATA

	bEnableHyperScale = true;

#endif
	
	ClassesReplicationOptions = UHScaleReplicationLibrary::BuildClassOptions();
	ClassesReplicationOptions.AddDefaulted();
}

#if WITH_EDITOR

void UHScaleDevSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EditorServerList.ValidateServerList();

	TArray<FHScale_ReplicationClassOptions> TempClassOptions = ClassesReplicationOptions;
	ClassesReplicationOptions = UHScaleReplicationLibrary::BuildClassOptions();

	for (FHScale_ReplicationClassOptions& Item : TempClassOptions)
	{
		if (!Item.IsValid()) continue;

		FHScale_ReplicationClassOptions* FoundItem = ClassesReplicationOptions.FindByPredicate([Item](const FHScale_ReplicationClassOptions& Other)
		{
			return Other <= Item.ActorClass;
		});

		if (!FoundItem)
		{
			ClassesReplicationOptions.Add(MoveTemp(Item));
		}
		else if (FoundItem->IsEditable() && FoundItem->ActorClass == Item.ActorClass)
		{
			FoundItem->ReplicationCondition = Item.ReplicationCondition;
		}
	}

	ClassesReplicationOptions.AddDefaulted();
	
	TryUpdateDefaultConfigFile();
}
#endif

#if WITH_EDITOR

bool UHScaleDevSettings::GetIsHyperScaleEnabled()
{
	return GetDefault<UHScaleDevSettings>()->bEnableHyperScale;
}

void UHScaleDevSettings::SetHyperScaleEnabled(const bool bEnabled)
{
	GetMutableDefault<UHScaleDevSettings>()->bEnableHyperScale = bEnabled;

	OnEnabledChangedDelegate.Broadcast(bEnabled);
}

const FHScale_ServerConfigList& UHScaleDevSettings::GetEditorServerList()
{
	return GetDefault<UHScaleDevSettings>()->EditorServerList;
}

FHScale_ServerConfigList& UHScaleDevSettings::GetEditorServerList_Mutable()
{
	return GetMutableDefault<UHScaleDevSettings>()->EditorServerList;
}

#endif

const TArray<FHScale_ReplicationClassOptions>& UHScaleDevSettings::GetClassesReplicationOptions()
{
	return GetDefault<UHScaleDevSettings>()->ClassesReplicationOptions;
}
