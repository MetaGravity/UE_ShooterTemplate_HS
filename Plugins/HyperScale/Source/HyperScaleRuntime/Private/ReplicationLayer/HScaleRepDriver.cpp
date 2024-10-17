// Copyright 2024 Metagravity. All Rights Reserved.


#include "ReplicationLayer/HScaleRepDriver.h"

#include "Core/HScaleDevSettings.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetworkObjectList.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/PlayerState.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "NetworkLayer/HScalePackageMap.h"
#include "RelevancyManager/HScaleRelevancyManager.h"
#include "ReplicationLayer/HScaleActorChannel.h"
#include "ReplicationLayer/HScaleReplicationResources.h"
#include "ReplicationLayer/Schema/HScaleSchemaRequest.h"

void UHScaleRepDriver::PostInitDriver()
{
	CachedConnection = CachedNetDriver->GetHyperScaleConnection();
	check(CachedConnection); // <- Cast failed, used connection class is not derived from UHScaleConnection class

	CachedPackageMap = Cast<UHScalePackageMap>(CachedConnection->PackageMap);
	check(CachedPackageMap); // <- Cast failed, used package class is not derived from UHScalePackageMap class
}

void UHScaleRepDriver::SetRepDriverWorld(UWorld* InWorld)
{
	/*if (!IsValid(GetWorld())) return;

	const FURL& Url = NetDriver->GetWorld()->URL;
	FString HyperscaleAddressAndPort = Url.GetOption(HYPERSCALE_LEVEL_OPTION, TEXT(""));
	HyperscaleAddressAndPort.ReplaceInline(TEXT("="), TEXT(""));
	HyperscaleAddressAndPort = HyperscaleAddressAndPort.TrimStart().TrimEnd(); // Remove white spaces

	// Try to download scheme from server to get replication rules
	SchemaRequest = UHScaleSchemaRequest::BuildRequest(this, HyperscaleAddressAndPort);
	SchemaRequest->OnCompleted.BindUObject(this, &ThisClass::OnSchemeDownloaded);
	UE_LOG(Log_HyperScaleReplication, Log, TEXT("Starting to download scheme from server."));
	SchemaRequest->ProcessRequest();*/
}

void UHScaleRepDriver::InitForNetDriver(UNetDriver* InNetDriver)
{
	check(InNetDriver);

	CachedNetDriver = Cast<UHScaleNetDriver>(InNetDriver);
	check(CachedNetDriver); // <- Cast failed, used driver is not derived from UHScaleNetDriver class
}

void UHScaleRepDriver::InitializeActorsInWorld(UWorld* InWorld)
{
	FNetworkObjectList& RepObjectList = CachedNetDriver->GetNetworkObjectList();
	const TSet<TSharedPtr<FNetworkObjectInfo>, FNetworkObjectKeyFuncs>& ActiveRepObjects = RepObjectList.GetActiveObjects();

	TSet<AActor*> ActorsToRemove;
	ActorsToRemove.Reserve(ActiveRepObjects.Num());

	for (const TSharedPtr<FNetworkObjectInfo> NetworkObject : ActiveRepObjects)
	{
		AActor* ActorToCheck = NetworkObject->Actor;
		if (!IsValid(ActorToCheck)) continue;

		if (!IsAllowedToReplicate(ActorToCheck))
		{
			ActorsToRemove.Add(ActorToCheck);
			continue;
		}
	}

	// Remove not allowed replicated actors
	for (AActor* Item : ActorsToRemove)
	{
		RepObjectList.Remove(Item);
	}
}

void UHScaleRepDriver::AddNetworkActor(AActor* Actor)
{
	if (!IsValid(Actor)) return;
	if (!IsValid(CachedNetDriver)) return;

	// Check if the actor is supported by engine for replication or based on current hscale level role
	if (!IsAllowedToReplicate(Actor))
	{
		FNetworkObjectList& RepObjectList = CachedNetDriver->GetNetworkObjectList();
		RepObjectList.Remove(Actor);
		return;
	}

	UHScaleConnection* Connection = CachedNetDriver->GetHyperScaleConnection();
	if (!IsValid(Connection) || !Connection->IsConnectionActive()) return;

	// Find or assign new actor channel to replicate the actor properly
	if (Connection->FindActorChannel(Actor) == nullptr)
	{
		// handling for the case of actors created from 
		FNetworkGUID ObjectNetGUID = Connection->Driver->GuidCache->GetOrAssignNetGUID(Actor);
		if (ObjectNetGUID.IsDefault() || !ObjectNetGUID.IsValid())
		{
			return;
		}
		UActorChannel* Channel = (UActorChannel*)Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally);
		if (Channel)
		{
			Channel->SetChannelActor(Actor, ESetChannelActorFlags::None);
		}
	}
}

void UHScaleRepDriver::RemoveNetworkActor(AActor* Actor)
{
	// this checks if it is a world tear off event and does not send network destruction event in that case
	if (!Actor) return;
	UWorld* World = Actor->GetWorld();
	if (!World) return;
	if (World->bIsTearingDown)
	{
		UE_LOG(Log_HyperScaleReplication, VeryVerbose, TEXT("World is tearing down, skipping actor destroy handling for %s"), *Actor->GetName())
		return;
	}

	UHScaleConnection* Connection = CachedNetDriver->GetHyperScaleConnection();

	if (!IsValid(Connection)) return;

	UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(Connection->PackageMap);
	if (!PkgMap) return;

	FHScaleNetGUID ActorNetGUID = PkgMap->FindEntityNetGUID(Actor);
	if (!ActorNetGUID.IsValid()) return;

	FHScaleEventsDriver* EventsDriver = Connection->GetEventsDriver();
	check(EventsDriver)

	if (Actor->HasAuthority())
	{
		// #todo... because of this we are not able to spawn again the character back from staging mode
		EventsDriver->MarkEntityForNetworkDestruction(ActorNetGUID);
	}
}

int32 UHScaleRepDriver::ServerReplicateActors(float DeltaSeconds)
{
	if (!IsValid(CachedNetDriver)) return 0;
	if (!IsValid(Schema)) return 0;
	if (!IsValid(CachedNetDriver->GetWorld())) return 0;
	if (!CachedNetDriver->GetBibliothec()) return 0;

	UHScaleConnection* Connection = CachedNetDriver->GetHyperScaleConnection();
	if (!IsValid(Connection)) return 0;
	if (!Connection->IsConnectionFullyEstablished()) return 0;

	int32 Result = 0;

	if (!bIsWorldInitAgent.IsSet())
	{
		// Cache, if the client starts a level with Role = WorldInitAgent
		bIsWorldInitAgent = CachedNetDriver->GetHyperScaleConnection()->GetLevelRoles().HasTag(HScaleGameplayTag::HyperScale_Role_WorldInitAgent);
	}

	UHScalePackageMap* PackageMap = (UHScalePackageMap*)Connection->PackageMap;

	FNetworkObjectList& RepObjectList = CachedNetDriver->GetNetworkObjectList();
	const TSet<TSharedPtr<FNetworkObjectInfo>, FNetworkObjectKeyFuncs>& ActiveRepObjects = RepObjectList.GetActiveObjects();

	// #todo ... iterate only group actors
	for (const TSharedPtr<FNetworkObjectInfo> NetworkObject : ActiveRepObjects)
	{
		// Skip not valid actors or actors with paused replication
		if (!IsValid(NetworkObject->Actor) || !NetworkObject->Actor->GetIsReplicated()) continue;

		AActor* ActorToCheck = NetworkObject->Actor;
		const ENetRole ActorRole = ActorToCheck->GetLocalRole();

		if (ActorToCheck->IsFullNameStableForNetworking())
		{
			// Pass all static objects from level until they are initialized from WorldInitAgent or until the client has a valid network GUID for that object
			if (!bIsWorldInitAgent.GetValue() && !PackageMap->DoesNetEntityExist(ActorToCheck))
			{
				continue;
			}
		}

		// Only owner of the player state object can replicate it into network
		// The other players can just see the object's data on their site
		if (Cast<APlayerState>(ActorToCheck))
		{
			// Only states with authority role can be replicated
			if (ActorRole != ROLE_Authority) continue;
		}
		else if (IsValid(Schema))
		{
			if (!Schema->CanReplicateActor(ActorToCheck))
			{
				continue;
			}
		}

		// Find or assign new actor channel to replicate the actor properly
		UHScaleActorChannel* ActorChannel;
		if (UHScaleActorChannel** FoundChannel = (UHScaleActorChannel**)Connection->FindActorChannel(ActorToCheck))
		{
			ActorChannel = *FoundChannel;
		}
		else
		{
			ActorChannel = Cast<UHScaleActorChannel>(Connection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally));
			if (ActorChannel)
			{
				ActorChannel->SetChannelActor(ActorToCheck, ESetChannelActorFlags::None);
			}
		}

		check(ActorChannel);

		ActorChannel->ReplicateActorToMemoryLayer();
	}

	return Result;
}

bool UHScaleRepDriver::SetEntityInStagingMode(const FHScaleNetGUID EntityGUID)
{
	if (!IsValid(CachedNetDriver)) return false;
	if (!IsValid(CachedConnection)) return false;
	if (!IsValid(CachedPackageMap)) return false;
	if (!EntityGUID.IsValid()) return false;

	const FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec) return false;
	if (!Bibliothec->IsEntityExists(EntityGUID)) return false;

	AActor* ActorFromGUID = Cast<AActor>(CachedPackageMap->FindObjectFromEntityID(EntityGUID));
	if (!IsValid(ActorFromGUID))
	{
		UE_LOG(Log_HyperScaleReplication, Warning, TEXT("Trying to stage entity that is not relevant anymore. (Entity ID = %s)"), *EntityGUID.ToString());
		return true; // <<< --- If it is already done, we can assume it is successful
	}

	// If it has authority, then the object cannot be spawn, otherwise it could remove actors that are own by this connection
	if (ActorFromGUID->HasAuthority())
	{
		return false;
	}

	if (ActorFromGUID->IsNameStableForNetworking())
	{
		UE_LOG(Log_HyperScaleReplication, Log, TEXT("Stable actor %s cannot be staged (Entity ID = %s)"), *ActorFromGUID->GetName(), *EntityGUID.ToString());
		return true;
	}

	// Find or assign new actor channel to replicate the actor properly
	UHScaleActorChannel* ActorChannel;
	if (UHScaleActorChannel** FoundChannel = (UHScaleActorChannel**)CachedConnection->FindActorChannel(ActorFromGUID))
	{
		ActorChannel = *FoundChannel;
	}
	else
	{
		ActorChannel = Cast<UHScaleActorChannel>(CachedConnection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::OpenedLocally));
		if (ActorChannel)
		{
			ActorChannel->SetChannelActor(ActorFromGUID, ESetChannelActorFlags::None);
		}
	}

	check(ActorChannel);
	ActorChannel->ConditionalCleanUp(false, EChannelCloseReason::Destroyed);
	// ActorChannel->SetChannelActor(nullptr, ESetChannelActorFlags::None);
	return true;
}

bool UHScaleRepDriver::SpawnStagedEntity(const FHScaleNetGUID EntityGUID)
{
	if (!IsValid(CachedNetDriver)) return false;
	if (!IsValid(CachedConnection)) return false;
	if (!IsValid(CachedPackageMap)) return false;
	if (!EntityGUID.IsValid()) return false;

	FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec) return false;
	if (!Bibliothec->IsEntityExists(EntityGUID)) return false;

	AActor* ActorFromGUID = Cast<AActor>(CachedPackageMap->FindObjectFromEntityID(EntityGUID));
	if (IsValid(ActorFromGUID))
	{
		UE_LOG(Log_HyperScaleReplication, Log, TEXT("Trying to spawn entity, but its already spawned. (Entity ID = %s)"), *EntityGUID.ToString());
		return true; // <<< --- If it is already done, we can assume it is successful
	}

	// check for owner relevance with relevancy manager
	TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FetchEntity(EntityGUID);
	check(Entity);

	// If there is any owner, (of course it should be always the actor owner) we have to spawn it first
	if (FHScaleNetworkEntity* OwnerEntity = Entity->GetOwner())
	{
		// Get the channel owner because if the owner is just a component, we have to spawn the actor
		if (const FHScaleNetworkEntity* ChannelOwner = OwnerEntity->GetChannelOwner())
		{
			check(ChannelOwner->IsActor());

			// Check also simple the owning actor here if is spawned, if yes then just pass spawning
			const AActor* OwningActor = Cast<AActor>(CachedPackageMap->FindObjectFromEntityID(ChannelOwner->EntityId));
			if (!IsValid(OwningActor) && !SpawnStagedEntity(ChannelOwner->EntityId))
			{
				ensure(false);
				return false; // <<< --- If the owner actor is not spawned, then the entity cannot be spawned (This should be very rare or never happen)
			}
		}
	}
	// FHScaleNetworkEntity* ChanelOwner = Entity->GetChannelOwner();
	// check(ChanelOwner);
	// ChanelOwner->ChannelIndex = INDEX_NONE; // Reset index, because the last one is already invalid

	const int32 ChIndex = CachedConnection->GetOrCreateChannelIndexForEntity(Entity.Get());
	if (ChIndex == INDEX_NONE)
	{
		FHScaleNetworkEntity* ChannelOwner = Entity->GetChannelOwner();

		UE_LOG(Log_HyperScaleReplication, Warning, TEXT("Channel Index is none for Entity:%llu ChannelOwner: %s"), EntityGUID.Get(), ChannelOwner != nullptr ? *ChannelOwner->EntityId.ToString() : *FString("No Owner"));
		return false;
	}

	UChannel* Channel = CachedConnection->Channels[ChIndex];
	FInBunch Bunch(CachedConnection);
	Bunch.bOpen = true;
	Bunch.bClose = 0;
	// Bunch.bReliable = 1;
	Bunch.ChIndex = ChIndex;
	Bunch.bPartial = 0;
	Bunch.ChName = NAME_Actor;
	Bunch.PackageMap = CachedPackageMap;

	UE_LOG(Log_HyperScaleMemory, VeryVerbose, TEXT("PullDataFromMemoryLayer:: Writing properties for entity=%s ChIndex=%d bIsNewChannel=%d SessionId=%s"), *EntityGUID.ToString(), ChIndex, Channel==nullptr, *CachedConnection->GetSessionNetGUID().ToString())
	if (Channel == nullptr)
	{
		Channel = CachedConnection->CreateChannelByName(NAME_Actor, EChannelCreateFlags::None, ChIndex);
		if (CachedNetDriver->Notify != nullptr)
		{
			CachedNetDriver->Notify->NotifyAcceptingChannel(Channel);
		}
	}

	Bibliothec->PullEntire(Bunch, EntityGUID);

	if (Channel != nullptr && Bunch.GetNumBits() > 0)
	{
		Channel->ReceivedBunch(Bunch);
		CachedConnection->PullUnmappedEntityUpdate(EntityGUID);
	}

	Bibliothec->ClearServerDirtyEntities({EntityGUID});
	return true;
}

bool UHScaleRepDriver::FlushEntity(const FHScaleNetGUID EntityGUID, const bool bSendForgetEvent)
{
	if (!EntityGUID.IsValid()) return false;
	if (!IsValid(CachedNetDriver)) return false;
	if (!IsValid(CachedPackageMap)) return false;
	if (!IsValid(CachedConnection)) return false;
	if (!CachedConnection->GetEventsDriver()) return false;

	FHScaleNetworkBibliothec* Bibliothec = CachedNetDriver->GetBibliothec();
	if (!Bibliothec) return false;
	if (!Bibliothec->IsEntityExists(EntityGUID)) return false;

	const AActor* ActorFromGUID = Cast<AActor>(CachedPackageMap->FindObjectFromEntityID(EntityGUID));
	if (IsValid(ActorFromGUID))
	{
		SetEntityInStagingMode(EntityGUID);
	}

	if(bSendForgetEvent)
	{
		CachedConnection->GetEventsDriver()->SendForgetObjectsEvent(EntityGUID);
	}
	
	Bibliothec->DestroyEntity(EntityGUID);
	return true;
}

void UHScaleRepDriver::OnConnectionEstablished(const FURL& URL)
{
	FString HyperscaleAddressAndPort = URL.GetOption(HYPERSCALE_LEVEL_OPTION, TEXT(""));
	HyperscaleAddressAndPort.ReplaceInline(TEXT("="), TEXT(""));
	HyperscaleAddressAndPort = HyperscaleAddressAndPort.TrimStart().TrimEnd(); // Remove white spaces

	// Try to download scheme from server to get replication rules
	SchemaRequest = UHScaleSchemaRequest::BuildRequest(this, HyperscaleAddressAndPort);
	SchemaRequest->OnCompleted.BindUObject(this, &ThisClass::OnSchemeDownloaded);
	UE_LOG(Log_HyperScaleReplication, Log, TEXT("Starting to download scheme from server."));
	SchemaRequest->ProcessRequest();
}

void UHScaleRepDriver::ActorRemotelyDestroyed(const FHScaleNetGUID& EntityId)
{
	// UHScalePackageMap* PkgMap = Cast<UHScalePackageMap>(CachedNetDriver->GetHyperScaleConnection()->PackageMap);
	// AActor* actor = Cast<AActor>(PkgMap->FindObjectFromEntityID(EntityId));
	// if (IsValid(actor))
	// {
	// 	FlushEntity(EntityId);
	// }
	FlushEntity(EntityId, false);
}

void UHScaleRepDriver::OnSchemeDownloaded(const bool bSuccessful, UHScaleSchema* Content)
{
	ensure(Content);
	Schema = Content;

	RelevancyManager = UHScaleRelevancyManager::Create(CachedNetDriver->GetHyperScaleConnection());

	if (bSuccessful)
	{
		UE_LOG(Log_HyperScaleReplication, Log, TEXT("Scheme obtained succesfully."));
	}
	else
	{
		UE_LOG(Log_HyperScaleReplication, Warning, TEXT("Scheme is not available on server."));
	}
}

bool UHScaleRepDriver::IsAllowedToReplicate(const AActor* Actor) const
{
	if (!IsValid(CachedNetDriver)) return false;
	if (!IsValid(Actor)) return false;

	const TArray<FHScale_ReplicationClassOptions>& RepOptions = UHScaleDevSettings::GetClassesReplicationOptions();

	const TSubclassOf<AActor> ActorClass = Actor->GetClass();
	const FHScale_ReplicationClassOptions* ClassOptions = RepOptions.FindByPredicate([ActorClass](const FHScale_ReplicationClassOptions& Other)
	{
		return Other.ActorClass.IsValid() && (ActorClass.Get() == Other.ActorClass.Get() || ActorClass->IsChildOf(Other.ActorClass.Get()));
	});

	bool bResult = true;
	if (ClassOptions && ClassOptions->ReplicationCondition.IsValid())
	{
		const FHyperScale_ReplicationRule& RepCondition = ClassOptions->ReplicationCondition.Get<FHyperScale_ReplicationRule>();
		bResult = RepCondition.IsReplicated(CachedNetDriver->GetHyperScaleConnection());

		// #todo ... maybe we could cache the result into RepDriver and not to compute it all the time when requesting this function for result
	}

	const FNetGUIDCache* GuidCache = CachedNetDriver->GuidCache.Get();
	if (!GuidCache->SupportsObject(Actor->GetClass()) || !GuidCache->SupportsObject(Actor->IsNetStartupActor() ? Actor : Actor->GetArchetype()))
	{
		// Actor is not supported by default from engine
		bResult = false;
	}

	return bResult;
}