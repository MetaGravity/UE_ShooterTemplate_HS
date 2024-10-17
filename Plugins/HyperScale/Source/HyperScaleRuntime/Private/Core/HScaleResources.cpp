// Copyright 2024 Metagravity. All Rights Reserved.


#include "Core/HScaleResources.h"

DEFINE_LOG_CATEGORY(Log_HyperScaleGlobals);
DEFINE_LOG_CATEGORY(Log_HyperScaleReplication);
DEFINE_LOG_CATEGORY(Log_HyperScaleEditor);
DEFINE_LOG_CATEGORY(Log_HyperScaleEvents);

FHScale_ServerConfigList::FHScale_ServerConfigList()
{
	EditorServerList.Reset(HSCALE_MAX_ALLOWED_EDITOR_SERVERS);

	// Adds the default one (localhost server)
	EditorServerActiveIndex = EditorServerList.Add(FHScale_ServerConfig());
}

void FHScale_ServerConfigList::ValidateServerList()
{
	if (EditorServerList.IsEmpty())
	{
		// Adds the default one if the list is empty (localhost server)
		EditorServerActiveIndex = EditorServerList.Add(FHScale_ServerConfig());
		return;
	}

	// #todo ... Make something like gameplay tags, custom UI that will add a new server with error if needed
	TArray<FHScale_ServerConfig> TempConfigs;
	for (const FHScale_ServerConfig& Item : EditorServerList)
	{
		const bool bContains = TempConfigs.ContainsByPredicate([&Item](const FHScale_ServerConfig& Other)
		{
			return Item.ToString().Equals(Other.ToString());
		});

		if (!bContains)
		{
			TempConfigs.Add(Item);
		}
	}

	const int32 Index = TempConfigs.IndexOfByPredicate([](const FHScale_ServerConfig& Other)
	{
		return Other.IsLocal();
	});

	// Config file should always contains the local address in list
	if (Index == INDEX_NONE)
	{
		TempConfigs.Insert(FHScale_ServerConfig(), 0);
	}

	EditorServerList = TempConfigs;
}

void FHScale_ServerConfigList::SelectServer(const FHScale_ServerConfig& Server)
{
	const int32 ItemIndex = FindServerIndex(Server);

	// Adds new server if possible
	if (ItemIndex == INDEX_NONE && !HasLimitReached())
	{
		EditorServerList.Add(Server);
	}

	// Sets new server
	SelectExistingServer(Server);
}

void FHScale_ServerConfigList::SelectExistingServer(const FHScale_ServerConfig& Server)
{
	const int32 ItemIndex = FindServerIndex(Server);

	// Sets new server, if found
	if (ItemIndex != INDEX_NONE)
	{
		EditorServerActiveIndex = ItemIndex;
		SelectedServer = EditorServerList[ItemIndex];
	}
}