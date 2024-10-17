// Copyright 2024 Metagravity. All Rights Reserved.


#include "NetworkLayer/HScaleNetDriver.h"

#include "NetworkLayer/HScaleConnection.h"
#include "Core/HScaleResources.h"
#include "Engine/NetworkObjectList.h"
#include "Engine/ActorChannel.h"
#include "Kismet/GameplayStatics.h"
#include "Net/DataChannel.h"
#include "Net/Core/Trace/NetTrace.h"
#include "ReplicationLayer/HScaleActorChannel.h"
#include "ReplicationLayer/HScaleRepDriver.h"

DECLARE_CYCLE_STAT(TEXT("NetDriver TickFlush"), STAT_NetTickFlush, STATGROUP_Game);


UHScaleNetDriver::UHScaleNetDriver()
{
	bActAsClient = false;
}

bool UHScaleNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	bActAsClient = false;
	const bool bSuperResult = Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error);

	if (!bSuperResult)
	{
		UE_LOG(Log_HyperScaleGlobals, Error, TEXT("UNetDriver initialization failed."));
		return false;
	}
	
	UHScaleConnection* Connection = NewObject<UHScaleConnection>(NetConnectionClass);
	checkf(Connection, TEXT("Property NetConnectionClassName in DefaultEngine.ini has not defined class or is not derived from UHScaleConnection"));
	
	Connection->InitBase(this, nullptr, URL, USOCK_Pending);
	AddClientConnection(Connection);

	(Cast<UHScaleRepDriver>(GetReplicationDriver()))->PostInitDriver();

	const bool bConnectionEstablished = Connection->InitHyperScaleConnection();
	if (bConnectionEstablished)
	{
		UE_LOG(Log_HyperScaleGlobals, Log, TEXT("Session successfully established with server."));

		// Prepare network cache for game
		// Has to be initialized before replication layer
		Bibliothec = MakeUnique<FHScaleNetworkBibliothec>();
		Bibliothec->SetNetDriver(this);

		const uint32 SessionId = Connection->GetSessionId();
		Bibliothec->CreateLocalPlayerEntity(SessionId);

		// ReplicationLayer = NewObject<UHScaleReplicationLayer>(this);
		// ReplicationLayer->Initialize(this);
	}
	else
	{
		UE_LOG(Log_HyperScaleGlobals, Error, TEXT("Session not established with server, creation of server cache FAILED."));
	}

	return IsNetworkSessionActive();
}

bool UHScaleNetDriver::ShouldReplicateFunction(AActor* Actor, UFunction* Function) const
{
	return (Actor && (Actor->GetNetDriverName() == NetDriverName || Actor->GetNetDriverName() == NAME_GameNetDriver));
}

bool UHScaleNetDriver::ShouldReplicateActor(AActor* Actor) const
{
	return IsValid(Actor) && (Actor->GetNetDriverName() == NAME_GameNetDriver || Super::ShouldReplicateActor(Actor));
}

bool UHScaleNetDriver::IsServer() const
{
	return Super::IsServer() && !bActAsClient;
}

int32 UHScaleNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	return Super::ServerReplicateActors(DeltaSeconds);

	// SCOPE_CYCLE_COUNTER(STAT_NetServerRepActorsTime);
	// CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ServerReplicateActors);

#if WITH_SERVER_CODE
	if (ClientConnections.Num() == 0)
	{
		return 0;
	}

	// SET_DWORD_STAT(STAT_NumReplicatedActors, 0);
	// SET_DWORD_STAT(STAT_NumReplicatedActorBytes, 0);

	// #if CSV_PROFILER
	// 	FScopedNetDriverStats NetDriverStats(OutBytes, this);
	// 	GNumClientConnections = ClientConnections.Num();
	// #endif
	// 	
	// 	if (ReplicationDriver)
	// 	{
	// 		return ReplicationDriver->ServerReplicateActors(DeltaSeconds);
	// 	}

	check(World);

	// Bump the ReplicationFrame value to invalidate any properties marked as "unchanged" for this frame.
	ReplicationFrame++;

	int32 Updated = 0;

	const bool bConnectionPrepared = ReplicateActors_PrepConnections(DeltaSeconds);

	if (!bConnectionPrepared)
	{
		// No connections are ready this frame
		return 0;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();

	bool bCPUSaturated = false;
	float ServerTickTime = GEngine->GetMaxTickRate(DeltaSeconds);
	if (ServerTickTime == 0.f)
	{
		ServerTickTime = DeltaSeconds;
	}
	else
	{
		ServerTickTime = 1.f / ServerTickTime;
		bCPUSaturated = DeltaSeconds > 1.2f * ServerTickTime;
	}

	TArray<FNetworkObjectInfo*> ConsiderList;
	ConsiderList.Reserve(GetNetworkObjectList().GetActiveObjects().Num());

	// Build the consider list (actors that are ready to replicate)
	ReplicateActors_BuildConsiderList(ConsiderList, ServerTickTime);

	TSet<UNetConnection*> ConnectionsToClose;

	if (OnPreConsiderListUpdateOverride.IsBound())
	{
		OnPreConsiderListUpdateOverride.Execute({DeltaSeconds, nullptr, bCPUSaturated}, Updated, ConsiderList);
	}

	// for (int32 i = 0; i < ClientConnections.Num(); i++)
	// {
	// 	UNetConnection* Connection = ClientConnections[i];
	// 	check(Connection);
	//
	// 	// #todo ... check if needed
	// 	// // net.DormancyValidate can be set to 2 to validate all dormant actors against last known state before going dormant
	// 	// if ( GNetDormancyValidate == 2 )
	// 	// {
	// 	// 	auto ValidateFunction = [](FObjectKey OwnerActorKey, FObjectKey ObjectKey, const TSharedRef<FObjectReplicator>& ReplicatorRef)
	// 	// 	{
	// 	// 		FObjectReplicator& Replicator = ReplicatorRef.Get();
	// 	//
	// 	// 		// We will call FObjectReplicator::ValidateAgainstState multiple times for
	// 	// 		// the same object (once for itself and again for each subobject).
	// 	// 		if (Replicator.OwningChannel != nullptr)
	// 	// 		{
	// 	// 			Replicator.ValidateAgainstState(Replicator.OwningChannel->GetActor());
	// 	// 		}
	// 	// 	};
	// 	// 		
	// 	// 	Connection->ExecuteOnAllDormantReplicators(ValidateFunction);
	// 	// }
	//
	// 	if (Connection->ViewTarget)
	// 	{
	// 		const int32 LocalNumSaturated = GNumSaturatedConnections;
	//
	// 		// Make a list of viewers this connection should consider (this connection and children of this connection)
	// 		TArray<FNetViewer>& ConnectionViewers = WorldSettings->ReplicationViewers;
	//
	// 		ConnectionViewers.Reset();
	// 		new(ConnectionViewers)FNetViewer(Connection, DeltaSeconds);
	//
	// 		// send ClientAdjustment if necessary
	// 		// we do this here so that we send a maximum of one per packet to that client; there is no value in stacking additional corrections
	// 		if (Connection->PlayerController)
	// 		{
	// 			Connection->PlayerController->SendClientAdjustment();
	// 		}
	//
	// 		FMemMark RelevantActorMark(FMemStack::Get());
	//
	// 		const bool bProcessConsiderListIsBound = OnProcessConsiderListOverride.IsBound();
	//
	// 		if (bProcessConsiderListIsBound)
	// 		{
	// 			OnProcessConsiderListOverride.Execute({DeltaSeconds, Connection, bCPUSaturated}, Updated, ConsiderList);
	// 		}
	//
	// 		// #todo ... send request to rep layer
	//
	// 		// if (!bProcessConsiderListIsBound)
	// 		// {
	// 		// 	FActorPriority* PriorityList = NULL;
	// 		// 	FActorPriority** PriorityActors = NULL;
	// 		//
	// 		// 	// Get a sorted list of actors for this connection
	// 		// 	const int32 FinalSortedCount = ServerReplicateActors_PrioritizeActors(Connection, ConnectionViewers, ConsiderList, bCPUSaturated, PriorityList, PriorityActors);
	// 		//
	// 		// 	// Process the sorted list of actors for this connection
	// 		// 	TInterval<int32> ActorsIndexRange(0, FinalSortedCount);
	// 		// 	const int32 LastProcessedActor = ServerReplicateActors_ProcessPrioritizedActorsRange(Connection, ConnectionViewers, PriorityActors, ActorsIndexRange, Updated);
	// 		//
	// 		// 	ServerReplicateActors_MarkRelevantActors(Connection, ConnectionViewers, LastProcessedActor, FinalSortedCount, PriorityActors);
	// 		// }
	//
	// 		RelevantActorMark.Pop();
	//
	// 		ConnectionViewers.Reset();
	//
	// 		Connection->LastProcessedFrame = ReplicationFrame;
	//
	// 		const bool bWasSaturated = GNumSaturatedConnections > LocalNumSaturated;
	// 		Connection->TrackReplicationForAnalytics(bWasSaturated);
	// 	}
	//
	// 	if (Connection->GetPendingCloseDueToReplicationFailure())
	// 	{
	// 		ConnectionsToClose.Add(Connection);
	// 	}
	// }
	//
	// if (OnPostConsiderListUpdateOverride.IsBound())
	// {
	// 	OnPostConsiderListUpdateOverride.ExecuteIfBound({DeltaSeconds, nullptr, bCPUSaturated}, Updated, ConsiderList);
	// }

#if NET_DEBUG_RELEVANT_ACTORS
	// if (DebugRelevantActors)
	// {
	// 	PrintDebugRelevantActors();
	// 	LastPrioritizedActors.Empty();
	// 	LastSentActors.Empty();
	// 	LastRelevantActors.Empty();
	// 	LastNonRelevantActors.Empty();
	//
	// 	DebugRelevantActors = false;
	// }
#endif // NET_DEBUG_RELEVANT_ACTORS

	// for (UNetConnection* ConnectionToClose : ConnectionsToClose)
	// {
	// 	ConnectionToClose->Close();
	// }

	return Updated;
#else
	return 0;
#endif // WITH_SERVER_CODE
}

void UHScaleNetDriver::PostTickDispatch()
{
	Super::PostTickDispatch();

	// #todo ... call replication layer to gather data into memory layer
}

void UHScaleNetDriver::TickFlush(float DeltaSeconds)
{
	Super::TickFlush(DeltaSeconds);
	// Use replication layer to replicate all changes into memory layer, where will be ready for send to network

	// DeltaReplication += DeltaSeconds;
	//
	// LLM_SCOPE_BYTAG(NetDriver);
	//
	// CSV_SCOPED_TIMING_STAT_EXCLUSIVE(NetworkOutgoing);
	// SCOPE_CYCLE_COUNTER(STAT_NetTickFlush);
	// TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*GetNetDriverDefinition().ToString());
	//
	// if (ClientConnections.IsEmpty())
	// {
	// 	// No Connection ready
	// 	return;
	// }
	//
	// // This is maybe not relevant for the hyperscale networking
	// ReplicateActors_PrepConnections(DeltaSeconds);
	//
	// if (IsValid(ReplicationLayer) && !bSkipServerReplicateActors && DeltaReplication >= ReplicationTick)
	// {
	// 	ReplicationLayer->ReplicationTick(DeltaReplication);
	// 	DeltaReplication = 0.f;
	// }
	//
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_NetDriver_TickClientConnections)

		for (UNetConnection* Connection : ClientConnections)
		{
			Connection->Tick(DeltaSeconds);
		}
	}
}

bool UHScaleNetDriver::ReplicateActors_PrepConnections(const float DeltaSeconds)
{
	// Most of the function body is copy of the parent one
	// The hyperscale server connection is different so we had to make some changes
	// To prepare that for different type of connection with server

	const int32 NumClientsToTick = ClientConnections.Num();
	check(NumClientsToTick == 1); // We should have only one connection with the server

	bool bFoundReadyConnection = false;

	for (int32 ConnIdx = 0; ConnIdx < ClientConnections.Num(); ConnIdx++)
	{
		UNetConnection* Connection = ClientConnections[ConnIdx];
		check(Connection);
		check(Connection->GetConnectionState() == USOCK_Pending || Connection->GetConnectionState() == USOCK_Open || Connection->GetConnectionState() == USOCK_Closed);
		checkSlow(Connection->GetUChildConnection() == NULL);

		// Handle not ready channels.
		//@note: we cannot add Connection->IsNetReady(0) here to check for saturation, as if that's the case we still want to figure out the list of relevant actors
		//			to reset their NetUpdateTime so that they will get sent as soon as the connection is no longer saturated
		AActor* OwningActor = Connection->OwningActor;
		if (OwningActor != NULL && Connection->GetConnectionState() == USOCK_Open)
		{
			check(World == OwningActor->GetWorld());

			bFoundReadyConnection = true;

			// the view target is what the player controller is looking at OR the owning actor itself when using beacons
			AActor* DesiredViewTarget = OwningActor;
			if (Connection->PlayerController)
			{
				if (AActor* ViewTarget = Connection->PlayerController->GetViewTarget())
				{
					if (ViewTarget->GetWorld())
					{
						// It is safe to use the player controller's view target.
						DesiredViewTarget = ViewTarget;
					}
					else
					{
						// Log an error, since this means the view target for the player controller no longer has a valid world (this can happen
						// if the player controller's view target was in a sublevel instance that has been unloaded).
						UE_LOG(LogNet, Warning, TEXT("Player controller %s's view target (%s) no longer has a valid world! Was it unloaded as part a level instance?"),
							*Connection->PlayerController->GetName(), *ViewTarget->GetName());
					}
				}
			}
			Connection->ViewTarget = DesiredViewTarget;
		}
		else
		{
			Connection->ViewTarget = NULL;
		}
	}

	return bFoundReadyConnection;
}

void UHScaleNetDriver::ReplicateActors_BuildConsiderList(TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime)
{
	// Most of the function body is copy of the parent one
	// The hyperscale server connection is different so we had to make some changes

	//SCOPE_CYCLE_COUNTER(STAT_NetConsiderActorsTime);

	UE_LOG(LogNetTraffic, Log, TEXT( "ServerReplicateActors_BuildConsiderList, Building ConsiderList %4.2f" ), World->GetTimeSeconds());

	int32 NumInitiallyDormant = 0;

	const bool bUseAdapativeNetFrequency = IsAdaptiveNetUpdateFrequencyEnabled();

	TArray<AActor*> ActorsToRemove;

	for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetActiveObjects())
	{
		FNetworkObjectInfo* ActorInfo = ObjectInfo.Get();

		if (!ActorInfo->bPendingNetUpdate && World->TimeSeconds <= ActorInfo->NextUpdateTime)
		{
			continue; // It's not time for this actor to perform an update, skip it
		}

		AActor* Actor = ActorInfo->Actor;

		// #todo ... we should also check, if the actor is in actor pool or not -> actor in pool will be newer send over the network
		// #todo ... or/and the actor in pool should be set as non replicated, it will optimize the network and iterations of these actors
		// #todo ... maybe find a way how we can dynamically change list of actors with ownership to the client that will be checked (we do not want to replicate actors with ownership to other clients, so we dont need to check them for replication)
		// #todo ... maybe we should set active/inactive replication value for each FNetworkObjectInfo

		if (Actor->IsPendingKillPending())
		{
			// Actors aren't allowed to be placed in the NetworkObjectList if they are PendingKillPending.
			// Actors should also be unconditionally removed from the NetworkObjectList when UWorld::DestroyActor is called.
			// If this is happening, it means code is not destructing Actors properly, and that's not OK.
			UE_LOG(LogNet, Warning, TEXT( "Actor %s was found in the NetworkObjectList, but is PendingKillPending" ), *Actor->GetName());
			ActorsToRemove.Add(Actor);
			continue;
		}

		// #todo ... we should add check ... if is client owner of the object, otherwise it should not be send
		if (Actor->GetRemoteRole() == ROLE_None)
		{
			ActorsToRemove.Add(Actor);
			continue;
		}

		// This actor may belong to a different net driver, make sure this is the correct one
		// (this can happen when using beacon net drivers for example)
		if (Actor->GetNetDriverName() != NetDriverName && Actor->GetNetDriverName() != NAME_GameNetDriver)
		{
			UE_LOG(LogNetTraffic, Error, TEXT("Actor %s in wrong network actors list! (Has net driver '%s', expected '%s')"),
				*Actor->GetName(), *Actor->GetNetDriverName().ToString(), *NetDriverName.ToString());

			continue;
		}

		// Verify the actor is actually initialized (it might have been intentionally spawn deferred until a later frame)
		if (!Actor->IsActorInitialized())
		{
			continue;
		}

		// Don't send actors that may still be streaming in or out
		ULevel* Level = Actor->GetLevel();
		if (Level->HasVisibilityChangeRequestPending() || Level->bIsAssociatingLevel)
		{
			continue;
		}

		// if (IsDormInitialStartupActor(Actor))
		// {
		// 	// This stat isn't that useful in its current form when using NetworkActors list
		// 	// We'll want to track initially dormant actors some other way to track them with stats
		// 	//SCOPE_CYCLE_COUNTER(STAT_NetInitialDormantCheckTime);
		// 	NumInitiallyDormant++;
		// 	ActorsToRemove.Add(Actor);
		// 	//UE_LOG(LogNetTraffic, Log, TEXT("Skipping Actor %s - its initially dormant!"), *Actor->GetName() );
		// 	continue;
		// }

		checkSlow(Actor->NeedsLoadForClient()); // We have no business sending this unless the client can load
		checkSlow(World == Actor->GetWorld());

		// Set defaults if this actor is replicating for first time
		if (ActorInfo->LastNetReplicateTime == 0)
		{
			ActorInfo->LastNetReplicateTime = World->TimeSeconds;
			ActorInfo->OptimalNetUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
		}

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;

		const float LastReplicateDelta = World->TimeSeconds - ActorInfo->LastNetReplicateTime;

		if (LastReplicateDelta > ScaleDownStartTime)
		{
			if (Actor->MinNetUpdateFrequency == 0.0f)
			{
				Actor->MinNetUpdateFrequency = 2.0f;
			}

			// Calculate min delta (max rate actor will update), and max delta (slowest rate actor will update)
			const float MinOptimalDelta = 1.0f / Actor->NetUpdateFrequency;                                 // Don't go faster than NetUpdateFrequency
			const float MaxOptimalDelta = FMath::Max(1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta); // Don't go slower than MinNetUpdateFrequency (or NetUpdateFrequency if it's slower)

			// Interpolate between MinOptimalDelta/MaxOptimalDelta based on how long it's been since this actor actually sent anything
			const float Alpha = FMath::Clamp((LastReplicateDelta - ScaleDownStartTime) / ScaleDownTimeRange, 0.0f, 1.0f);
			ActorInfo->OptimalNetUpdateDelta = FMath::Lerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}

		// Setup ActorInfo->NextUpdateTime, which will be the next time this actor will replicate properties to connections
		// NOTE - We don't do this if bPendingNetUpdate is true, since this means we're forcing an update due to at least one connection
		//	that wasn't to replicate previously (due to saturation, etc)
		// NOTE - This also means all other connections will force an update (even if they just updated, we should look into this)
		if (!ActorInfo->bPendingNetUpdate)
		{
			UE_LOG(LogNetTraffic, Log, TEXT( "actor %s requesting new net update, time: %2.3f" ), *Actor->GetName(), World->TimeSeconds);

			const float NextUpdateDelta = bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : 1.0f / Actor->NetUpdateFrequency;

			// then set the next update time
			ActorInfo->NextUpdateTime = World->TimeSeconds + UpdateDelayRandomStream.FRand() * ServerTickTime + NextUpdateDelta;

			// and mark when the actor first requested an update
			//@note: using ElapsedTime because it's compared against UActorChannel.LastUpdateTime which also uses that value
			ActorInfo->LastNetUpdateTimestamp = GetElapsedTime();
		}

		// and clear the pending update flag assuming all clients will be able to consider it
		ActorInfo->bPendingNetUpdate = false;

		// add it to the list to consider below
		// For performance reasons, make sure we don't resize the array. It should already be appropriately sized above!
		ensure(OutConsiderList.Num() < OutConsiderList.Max());
		OutConsiderList.Add(ActorInfo);

		// Call PreReplication on all actors that will be considered
		Actor->CallPreReplication(this);
	}

	for (AActor* Actor : ActorsToRemove)
	{
		RemoveNetworkActor(Actor);
	}

	// Update stats
	//SET_DWORD_STAT(STAT_NumInitiallyDormantActors, NumInitiallyDormant);
	//SET_DWORD_STAT(STAT_NumConsideredActors, OutConsiderList.Num());
}

bool UHScaleNetDriver::IsNetworkSessionActive() const
{
	const UHScaleConnection* Connection = GetHyperScaleConnection();
	return IsValid(Connection) && Connection->IsConnectionActive();
}

UHScaleConnection* UHScaleNetDriver::GetHyperScaleConnection() const
{
	for (UNetConnection* Connection : ClientConnections)
	{
		if (UHScaleConnection* CastedConnection = Cast<UHScaleConnection>(Connection))
		{
			return CastedConnection;
		}
	}

	return nullptr;
}

void UHScaleNetDriver::GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const
{
	if (!IsNetworkSessionActive()) { return; }
	const UHScaleConnection* Connection = GetHyperScaleConnection();
	if (Connection->PlayerController)
	{
		Connection->PlayerController->GetPlayerViewPoint(Location, Rotation);
	}
	else
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
		if (PC)
		{
			PC->GetPlayerViewPoint(Location, Rotation);
		}
	}
}

TSharedPtr<FRepLayout> UHScaleNetDriver::GetObjectClassRepLayout_Copy(UClass* Class)
{
	TSharedPtr<FRepLayout>* RepLayoutPtr = RepLayoutMap.Find(Class);

	if (!RepLayoutPtr)
	{
		ECreateRepLayoutFlags Flags = MaySendProperties() ? ECreateRepLayoutFlags::MaySendProperties : ECreateRepLayoutFlags::None;
		RepLayoutPtr = &RepLayoutMap.Add(Class, FRepLayout::CreateFromClass(Class, ServerConnection, Flags));
	}

	return *RepLayoutPtr;
}

void UHScaleNetDriver::ProcessRemoteFunction(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject)
{
	if (Actor->IsActorBeingDestroyed())
	{
		UE_LOG(LogNet, Warning, TEXT("UNetDriver::ProcessRemoteFunction: Remote function %s called from actor %s while actor is being destroyed. Function will not be processed."), *Function->GetName(), *Actor->GetName());
		return;
	}

	++TotalRPCsCalled;

	UHScaleConnection* Connection = GetHyperScaleConnection();
	if (!Connection) return;

	// get the top most function
	while (Function->GetSuperFunction())
	{
		Function = Function->GetSuperFunction();
	}

	// If saturated and function is unimportant, skip it. Note unreliable multicasts are queued at the actor channel level so they are not gated here.
	if (!(Function->FunctionFlags & FUNC_NetReliable) && (!(Function->FunctionFlags & FUNC_NetMulticast)) && (!Connection->IsNetReady(0)))
	{
		UE_LOG(LogNet, VeryVerbose, L"Network saturated, not calling %s::%s", *GetNameSafe(Actor), *GetNameSafe(Function));;
		return;
	}

	if (Connection->GetConnectionState() == USOCK_Closed)
	{
		UE_LOG(LogNet, VeryVerbose, TEXT("Attempting to call RPC on a closed connection. Not calling %s::%s"), *GetNameSafe(Actor), *GetNameSafe(Function));
		return;
	}

	if (World == nullptr)
	{
		UE_LOG(LogNet, VeryVerbose, TEXT("Attempting to call RPC with a null World on the net driver. Not calling %s::%s"), *GetNameSafe(Actor), *GetNameSafe(Function));
		return;
	}

	FHScaleEventsDriver* EventsDriver = Connection->GetEventsDriver();
	if (!EventsDriver)
	{
		return;
	}
	EventsDriver->ProcessLocalRPC(Actor, Function, Parameters, OutParms, Stack, SubObject);
}