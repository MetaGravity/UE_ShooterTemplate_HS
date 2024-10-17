// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/HScaleReplicationResources.h"

#include "NetworkLayer/HScaleConnection.h"

namespace HScaleGameplayTag
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HyperScale, "HyperScale", "All gameplay tags related to hyperscale plugin.");

	//~ Begin Roles
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HyperScale_Role, "HyperScale.Role", "Defines user roles in game.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HyperScale_Role_Client, "HyperScale.Role.Client", "Standard UE client player.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HyperScale_Role_WorldInitAgent, "HyperScale.Role.WorldInitAgent", "Client with authority to initialize world replicated static objects.");
	//~ End Roles
}

bool FHyperScale_ReplicationRule_RoleReplicated::IsReplicated(UHScaleConnection* Connection) const
{
	if (!IsValid(Connection)) return false;

	const bool bResult = Connection->GetLevelRoles().HasTag(Role);
	return bResult;
}

bool FHyperScale_ReplicationRule_RoleQueryReplicated::IsReplicated(UHScaleConnection* Connection) const
{
	if (!IsValid(Connection)) return false;

	const bool bResult = Query.Matches(Connection->GetLevelRoles());
	return bResult;
}

bool FHScale_ReplicationClassOptions::operator<=(const TSoftClassPtr<AActor>& Other) const
{
	const TSubclassOf<AActor> Left = Other.LoadSynchronous();
	const TSubclassOf<AActor> Right = ActorClass.LoadSynchronous();

	if (Left == Right) return true;
	const bool bResult = Left->IsChildOf(Right);
	return bResult;
}
