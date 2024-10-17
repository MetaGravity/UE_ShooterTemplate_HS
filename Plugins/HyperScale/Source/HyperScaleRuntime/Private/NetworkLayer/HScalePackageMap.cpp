// Fill out your copyright notice in the Description page of Project Settings.


#include "NetworkLayer/HScalePackageMap.h"

#include "BookKeeper/HSClassTranslator.h"
#include "Core/HScaleResources.h"
#include "Engine/ActorChannel.h"
#include "Kismet/GameplayStatics.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "ReplicationLayer/HScaleActorChannel.h"
#include "Utils/HScaleStatics.h"

static const int INTERNAL_LOAD_OBJECT_RECURSION_LIMIT = 10;

struct FHScaleActorSpawnParameters
{
	/* A name to assign as the Name of the Actor being spawned. If no value is specified, the name of the spawned Actor will be automatically generated using the form [Class]_[Number]. */
	FName Name;

	/* An Actor to use as a template when spawning the new Actor. The spawned Actor will be initialized using the property values of the template Actor. If left NULL the class default object (CDO) will be used to initialize the spawned Actor. */
	AActor* Template;

	/* The Actor that spawned this Actor. (Can be left as NULL). */
	AActor* Owner;

	/* The APawn that is responsible for damage done by the spawned Actor. (Can be left as NULL). */
	APawn* Instigator;

	/* The ULevel to spawn the Actor in, i.e. the Outer of the Actor. If left as NULL the Outer of the Owner is used. If the Owner is NULL the persistent level is used. */
	class ULevel* OverrideLevel;

#if WITH_EDITOR
	/* The UPackage to set the Actor in. If left as NULL the Package will not be set and the actor will be saved in the same package as the persistent level. */
	class UPackage* OverridePackage;

	/** The Guid to set to this actor. Should only be set when reinstancing blueprint actors. */
	FGuid OverrideActorGuid;
#endif

	/* The parent component to set the Actor in. */
	class UChildActorComponent* OverrideParentComponent;

	/** Method for resolving collisions at the spawn point. Undefined means no override, use the actor's setting. */
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

	/** Determines whether to multiply or override root component with provided spawn transform */
	ESpawnActorScaleMethod TransformScaleMethod = ESpawnActorScaleMethod::MultiplyWithRoot;

	/* Is the actor remotely owned. This should only be set true by the package map when it is creating an actor on a client that was replicated from the server. */
	uint8 bRemoteOwned : 1;

	/* Determines whether spawning will not fail if certain conditions are not met. If true, spawning will not fail because the class being spawned is `bStatic=true` or because the class of the template Actor is not the same as the class of the Actor being spawned. */
	uint8 bNoFail : 1;

	/* Determines whether the construction script will be run. If true, the construction script will not be run on the spawned Actor. Only applicable if the Actor is being spawned from a Blueprint. */
	uint8 bDeferConstruction : 1;

	/* Determines whether or not the actor may be spawned when running a construction script. If true spawning will fail if a construction script is being run. */
	uint8 bAllowDuringConstructionScript : 1;

#if !WITH_EDITOR
	/* Force the spawned actor to use a globally unique name (provided name should be none). */
	uint8	bForceGloballyUniqueName:1;
#else
	/* Determines whether the begin play cycle will run on the spawned actor when in the editor. */
	uint8 bTemporaryEditorActor : 1;

	/* Determines whether or not the actor should be hidden from the Scene Outliner */
	uint8 bHideFromSceneOutliner : 1;

	/** Determines whether to create a new package for the actor or not, if the level supports it. */
	uint16 bCreateActorPackage : 1;
#endif

	/* In which way should SpawnActor should treat the supplied Name if not none. */
	uint8 NameMode;

	/* Flags used to describe the spawned actor/object instance. */
	EObjectFlags ObjectFlags;

	/* Custom function allowing the caller to specific a function to execute post actor construction but before other systems see this actor spawn. */
	TFunction<void(AActor*)> CustomPreSpawnInitalization;
};

bool UHScalePackageMap::SerializeObject(FArchive& Ar, UClass* Class, UObject*& Object, FNetworkGUID* OutNetGUID)
{
	if (Ar.IsSaving())
	{
		// If pending kill, just serialize as NULL.
		// TWeakObjectPtrs of PendingKill objects will behave strangely with TSets and TMaps
		//	PendingKill objects will collide with each other and with NULL objects in those data structures.
		if (Object && !IsValid(Object))
		{
			UObject* NullObj = NULL;
			return SerializeObject(Ar, Class, NullObj, OutNetGUID);
		}

		FNetworkGUID NetGUID = GuidCache->GetOrAssignNetGUID(Object);

		// Write out NetGUID to caller if necessary
		if (OutNetGUID)
		{
			*OutNetGUID = NetGUID;
		}

		// This is for quark networking
		if (NetGUID.IsValid())
		{
			// If Class is PlayerController use the connectionId
			// else if it is replicated object, then entity id
			// if replicated and in memory layer, then write owner entity id
			// if replicated but not in memory layer, then create net guid and write
			// if not replicated but in memory layer, then write net guid
			// else (not replicated and not in memory layer) write empty guid

			bool bAlreadyAssignedId = ObjectGuidMapToNetGuid.Contains(NetGUID);
			if (!bAlreadyAssignedId) // If already assigned, then do nothing
			{
				// @pavan: the logic is changed to not consider all dynamic ids for assigment of NetGUIDs
				const bool bCanAssignNewNetworkId = FHScaleStatics::IsObjectReplicated(Object) || Object->IsA(APlayerController::StaticClass());
				if (bCanAssignNewNetworkId)
				{
					FHScaleNetGUID AssignedId;
					if (Object->GetClass()->IsChildOf(APlayerController::StaticClass()))
					{
						AssignedId = FHScaleNetGUID::Create_Player(GetHScaleConnection()->GetSessionId());
					}
					else
					{
						AssignedId = FHScaleNetGUID::Create_Object(GetHScaleConnection()->GetNewHScaleObjectId(Object));
					}

					AddGUIDsToMap(AssignedId, NetGUID);
				}
			}
		}

		// Write object NetGUID to the given FArchive
		InternalWriteObject_HS(Ar, NetGUID, Object, TEXT(""), NULL);
		return true;
	}
	else if (Ar.IsLoading())
	{
		FNetworkGUID NetGUID;
		double LoadTime = 0.0;
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FScopedDurationTimer NetSerializeTime(LoadTime);
#endif

			// ----------------	
			// Read entity net GUID from stream
			// ----------------	
			// ----------------	
			// Read NetGUID from stream and resolve object
			// ----------------	
			NetGUID = InternalLoadObject_HS(Ar, Object, 0);

			// Write out NetGUID to caller if necessary
			if (OutNetGUID)
			{
				*OutNetGUID = NetGUID;
			}

			// ----------------	
			// Final Checks/verification
			// ----------------	

			// NULL if we haven't finished loading the objects level yet
			if (!ObjectLevelHasFinishedLoading(Object))
			{
				UE_LOG(LogNetPackageMap, Warning, TEXT("Using None instead of replicated reference to %s because the level it's in has not been made visible"), *Object->GetFullName());
				Object = NULL;
			}

			// Check that we got the right class
			if (Object && !(Class->HasAnyClassFlags(CLASS_Interface) ? Object->GetClass()->ImplementsInterface(Class) : Object->IsA(Class)))
			{
				UE_LOG(LogNetPackageMap, Warning, TEXT("Forged object: got %s, expecting %s"), *Object->GetFullName(), *Class->GetFullName());
				Object = NULL;
			}

			if (NetGUID.IsValid() && bShouldTrackUnmappedGuids && !GuidCache->IsGUIDBroken(NetGUID, false))
			{
				if (Object == nullptr)
				{
					TrackedUnmappedNetGuids.Add(NetGUID);
				}
				else if (NetGUID.IsDynamic())
				{
					TrackedMappedDynamicNetGuids.Add(NetGUID);
				}
			}

			UE_LOG(LogNetPackageMap, VeryVerbose, TEXT("UPackageMapClient::SerializeObject Serialized Object %s as <%s>"), Object ? *Object->GetPathName() : TEXT("NULL"), *NetGUID.ToString());
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static IConsoleVariable* LongLoadThreshholdCVAR = IConsoleManager::Get().FindConsoleVariable(TEXT("net.PackageMap.LongLoadThreshhold"));
		if (LongLoadThreshholdCVAR && ((float)LoadTime > LongLoadThreshholdCVAR->GetFloat()))
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT("Long net serialize: %fms, Serialized Object %s"), (float)LoadTime * 1000.0f, *GetNameSafe(Object));
		}
#endif

		// reference is mapped if it was not NULL (or was explicitly null)
		return (Object != NULL || !NetGUID.IsValid());
	}

	return true;
}

bool UHScalePackageMap::SerializeNewActor(FArchive& Ar, UActorChannel* Channel, AActor*& Actor)
{
	LLM_SCOPE(ELLMTag::EngineMisc);

	UE_LOG(LogNetPackageMap, VeryVerbose, TEXT( "SerializeNewActor START" ));

	uint8 bIsClosingChannel = 0;

	// #@pavan owner spawn checks are not needed, they are taken care at memory layer levels
	// // Check if the owner is already spawned
	// FHScaleNetGUID OwnerNetGuid;
	//
	// if (Ar.IsLoading())
	// {
	// 	FInBunch* InBunch = (FInBunch*)&Ar;
	// 	bIsClosingChannel = InBunch->bClose; // This is so we can determine that this channel was opened/closed for destruction
	// 	UE_LOG(LogNetPackageMap, Log, TEXT("UPackageMapClient::SerializeNewActor BitPos: %lld"), InBunch->GetPosBits());
	//
	// 	Ar << OwnerNetGuid;
	// 	ResetTrackedSyncLoadedGuids();
	// }
	//
	// NET_CHECKSUM(Ar);
	//
	// // Outer is valid but the entity is not spawned yet
	// if (OwnerNetGuid.IsValid() && !FindObjectFromEntityID(OwnerNetGuid))
	// {
	// }

	UHScaleActorChannel* HScaleChannel = Cast<UHScaleActorChannel>(Channel);
	check(HScaleChannel);

	{
		UHScaleConnection* HSConnection = GetHScaleConnection();

		// Check if there is enough free IDs for replication
		int32 availableIds = HSConnection->GetAvailableObjectIdCount(Actor);
		int32 necessaryIds = HSConnection->GetNecessaryObjectIdCount(Actor);
		if (availableIds < necessaryIds)
		{
			UE_LOG(LogNetPackageMap, Error, TEXT("UPackageMapClient::SerializeNewActor: Not enough available IDs. Available: %d, Necessary: %d"), availableIds, necessaryIds);
			return false;
		}
	}

	// Write object ID into bunch
	FNetworkGUID NetGUID;
	UObject* NewObj = Actor;
	bool bSuccessfulSerialization = SerializeObject(Ar, AActor::StaticClass(), NewObj, &NetGUID);

	Channel->ActorNetGUID = NetGUID;
	Actor = Cast<AActor>(NewObj);

	if (Ar.IsError())
	{
		UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: Ar.IsError after SerializeObject" ));
		return false;
	}
	if (!bSuccessfulSerialization)
	{
		UE_LOG(LogNetPackageMap, Error, TEXT("UPackageMapClient::SerializeNewActor: Failed to Serialize Object: %s"), *Actor->GetActorNameOrLabel())
		return false;
	}

	// if (GuidCache.IsValid())
	// {
	// 	if (ensureMsgf(NetGUID.IsValid(), TEXT("Channel tried to add an invalid GUID to the import list: %s"), *Channel->Describe()))
	// 	{
	// 		LLM_SCOPE_BYTAG(GuidCache);
	// 		GuidCache->ImportedNetGuids.Add(NetGUID);
	// 	}
	// }

	FHScaleNetGUID QuarkNetGUID;

	// When we return an actor, we don't necessarily always spawn it (we might have found it already in memory)
	// The calling code may want to know, so this is why we distinguish
	bool bActorWasSpawned = false;

	if (Ar.AtEnd() && NetGUID.IsDynamic())
	{
		// This must be a destruction info coming through or something is wrong
		// If so, we should be closing the channel
		// This can happen when dormant actors that don't have channels get destroyed
		// Not finding the actor can happen if the client streamed in this level after a dynamic actor has been spawned and deleted on the server side
		if (bIsClosingChannel == 0)
		{
			UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: bIsClosingChannel == 0 : %s [%s]" ), *GetNameSafe(Actor), *NetGUID.ToString());
			Ar.SetError();
			return false;
		}

		UE_LOG(LogNetPackageMap, Log, TEXT( "UPackageMapClient::SerializeNewActor:  Skipping full read because we are deleting dynamic actor: %s" ), *GetNameSafe(Actor));
		return false; // This doesn't mean an error. This just simply means we didn't spawn an actor.
	}

	// Write location and other stuff into header part of bunch
	bool bContainsSpawnData = false;
	if (Ar.IsLoading())
	{
		Ar.SerializeBits(&bContainsSpawnData, 1);
	}

	UObject* Archetype = nullptr;
	UObject* ActorLevel = nullptr;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	TOptional<FVector> Velocity;
	TOptional<FVector> Scale;

	if (Ar.IsSaving())
	{
		bool bContainsArchetypeData = NetGUID.IsDynamic();
		Ar.SerializeBits(&bContainsArchetypeData, 1);

		if (bContainsArchetypeData)
		{
			if (UChildActorComponent* CAC = Actor->GetParentComponent())
			{
				Archetype = CAC->GetChildActorTemplate();
			}
			if (Archetype == nullptr)
			{
				Archetype = Actor->GetArchetype();
			}

			FNetworkGUID ArchetypeNetGUID;
			SerializeObject(Ar, UObject::StaticClass(), Archetype, &ArchetypeNetGUID);
		}

		WriteActorHeader(Ar, Actor);

		if (Ar.IsError())
		{
			UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: Ar.IsError after Serialize actor header." ));
			return false;
		}

		QuarkNetGUID = FindEntityNetGUID(NewObj);
		ensure(QuarkNetGUID.IsValid());
		HScaleChannel->EntityId = QuarkNetGUID;
		HScaleChannel->ReplicationRedirectories.Add(QuarkNetGUID, NewObj);
	}
	else if (Ar.IsLoading() && bContainsSpawnData)
	{
		// if its loading, ensure the HScaleNetGUID EntityId is set on the channel, we cant proceed without that
		check(HScaleChannel->EntityId.IsValid())
		QuarkNetGUID = HScaleChannel->EntityId;

		// #todo ... fetch the world from object data
		FHScaleNetGUID LevelId; // Maybe we don't need The level ID as uint64
		Ar << LevelId;

		FNetworkGUID ArchetypeNetGUID;
		SerializeObject(Ar, UObject::StaticClass(), Archetype, &ArchetypeNetGUID);

#if WITH_EDITOR
		UObjectRedirector* ArchetypeRedirector = Cast<UObjectRedirector>(Archetype);
		if (ArchetypeRedirector)
		{
			// Redirectors not supported
			Archetype = nullptr;
		}
#endif // WITH_EDITOR

		if (ArchetypeNetGUID.IsValid() && Archetype == NULL)
		{
			const FNetGuidCacheObject* ExistingCacheObjectPtr = GuidCache->ObjectLookup.Find(ArchetypeNetGUID);

			if (ExistingCacheObjectPtr != NULL)
			{
				UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Path: %s, NetGUID: %s." ), *ExistingCacheObjectPtr->PathName.ToString(), *ArchetypeNetGUID.ToString());
			}
			else
			{
				UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Guid not registered! NetGUID: %s." ), *ArchetypeNetGUID.ToString());
			}
		}

		// Read objects spawn data
		ReadNewActorHeader(Ar, Location, Rotation, Scale, Velocity);

		if (Ar.IsError())
		{
			UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: Ar.IsError after Serialize actor header." ));
			return false;
		}
	}

	// Spawning only dynamic objects
	if (QuarkNetGUID.IsDynamic() && Ar.IsLoading())
	{
		// Spawn actor if necessary (we may have already found it if it was dormant)
		if (Actor == NULL)
		{
			if (Archetype)
			{
				// For streaming levels, it's possible that the owning level has been made not-visible but is still loaded.
				// In that case, the level will still be found but the owning world will be invalid.
				// If that happens, wait to spawn the Actor until the next time the level is streamed in.
				// At that point, the Server should resend any dynamic Actors.
				ULevel* SpawnLevel = Cast<ULevel>(ActorLevel);
				if (SpawnLevel == nullptr || SpawnLevel->GetWorld() != nullptr)
				{
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Template = Cast<AActor>(Archetype);
					SpawnInfo.OverrideLevel = SpawnLevel;
					SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnInfo.bNoFail = true;

					// This is hack because we cant override private property
					FHScaleActorSpawnParameters Custom;
					FMemory::Memcpy(&Custom, &SpawnInfo, sizeof(FActorSpawnParameters));
					Custom.bRemoteOwned = true;
					FMemory::Memcpy(&SpawnInfo, &Custom, sizeof(FActorSpawnParameters));

					UWorld* World = Connection->Driver->GetWorld();
					FVector SpawnLocation = FRepMovement::RebaseOntoLocalOrigin(Location, World->OriginLocation);
					Actor = World->SpawnActorAbsolute(Archetype->GetClass(), FTransform(Rotation, SpawnLocation), SpawnInfo);
					if (Actor)
					{
						// Velocity was serialized by the server
						if (Velocity.IsSet())
						{
							Actor->PostNetReceiveVelocity(Velocity.GetValue());
						}

						// Scale was serialized by the server
						if (Scale.IsSet())
						{
							Actor->SetActorRelativeScale3D(Scale.GetValue());
						}

						// Temporarily the dirver might have been marked as client, note the existing value override it as server
						// to assign new guid and set the value back
						UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
						check(NetDriver)
						uint8 ExistingMarker = NetDriver->bActAsClient;
						NetDriver->bActAsClient = 0;
						NetGUID = GuidCache->AssignNewNetGUID_Server(Actor);
						NetDriver->bActAsClient = ExistingMarker;

						AddGUIDsToMap(QuarkNetGUID, NetGUID);
						HScaleChannel->ReplicationRedirectories.Add(QuarkNetGUID, Actor);
						bActorWasSpawned = true;
					}
					else
					{
						UE_LOG(LogNetPackageMap, Warning, TEXT("SerializeNewActor: Failed to spawn actor for NetGUID: %s, Channel: %d"), *NetGUID.ToString(), Channel->ChIndex);
					}
				}
				else
				{
					UE_LOG(LogNetPackageMap, Log, TEXT("SerializeNewActor: Actor level has invalid world (may be streamed out). NetGUID: %s, Channel: %d"), *NetGUID.ToString(), Channel->ChIndex);
				}
			}
			else
			{
				UE_LOG(LogNetPackageMap, Error, TEXT("UPackageMapClient::SerializeNewActor Unable to read Archetype for NetGUID %s"), *NetGUID.ToString());
			}
		}
	}
	else if (Ar.IsLoading() && Actor == NULL)
	{
		// Do not log a warning during replay, since this is a valid case
		UE_CLOG(!Connection->IsReplay(), LogNetPackageMap, Log, TEXT("SerializeNewActor: Failed to find static actor: FullNetGuidPath: %s, Channel: %d"), *GuidCache->FullNetGUIDPath(NetGUID), Channel->ChIndex);
	}

	/*if (Ar.IsLoading())
	{
		ReportSyncLoadsForActorSpawn(Actor);
	}*/

	UE_LOG(LogNetPackageMap, Log, TEXT( "SerializeNewActor END: Finished Serializing. Actor: %s, FullNetGUIDPath: %s, Channel: %d, IsLoading: %i, IsDynamic: %i" ), Actor ? *Actor->GetName() : TEXT("NULL"), *GuidCache->FullNetGUIDPath( NetGUID ), Channel->ChIndex, (int)Ar.IsLoading(), (int)NetGUID.IsDynamic());

	return !Ar.IsLoading() || bActorWasSpawned;
}

void UHScalePackageMap::InternalWriteObject_HS(FArchive& Ar, FNetworkGUID NetGUID, UObject* Object, FString ObjectPathName, UObject* ObjectOuter)
{
	check(Ar.IsSaving());

	if (!NetGUID.IsValid())
	{
		// We're done writing
		uint8 Invalid = 0;
		Ar.SerializeBits(&Invalid, 1);
		Ar << NetGUID;
		return;
	}

	// Write export flags
	//   note: Default NetGUID is implied to always send path
	FHScaleExportFlags ExportFlags;

	FHScaleNetGUID* NetworkId = ObjectGuidMapToNetGuid.Find(NetGUID);
	if (NetworkId)
	{
		uint8 Valid = UINT8_MAX;
		Ar.SerializeBits(&Valid, 1);
		Ar << *NetworkId;
	}
	else
	{
		uint8 Invalid = 0;
		Ar.SerializeBits(&Invalid, 1);
		Ar << NetGUID;
	}

	// NET_CHECKSUM(Ar);

	// Set if the object can be loaded on client side
	// it is part of function "CanClientLoadObject" from parent class
	// but the function needs to have FNetGUID, which is not important for this type of networking
	ExportFlags.bNoLoad = !GuidCache->CanClientLoadObject(Object, NetGUID);

	if (NetworkId && NetworkId->IsDynamic())
	{
		ExportFlags.bHasPath = 0; // if already assigned signifies, path is already written out
	}
	else
	{
		if (Object != nullptr)
		{
			ExportFlags.bHasPath = ShouldSendFullPath_HS(Object, NetGUID) ? 1 : 0;
		}
		else
		{
			ExportFlags.bHasPath = ObjectPathName.IsEmpty() ? 0 : 1;
		}
	}

	Ar << ExportFlags.Value;

	if (ExportFlags.bHasPath)
	{
		if (Object != nullptr)
		{
			// If the object isn't nullptr, expect an empty path name, then fill it out with the actual info
			check(ObjectOuter == nullptr);
			check(ObjectPathName.IsEmpty());
			ObjectPathName = Object->GetName();
			ObjectOuter = Object->GetOuter();
		}
		else
		{
			// If we don't have an object, expect an already filled out path name
			checkf(ObjectOuter != nullptr, TEXT("ObjectOuter is null. NetGuid: %s. Object: %s. ObjectPathName: %s"), *NetGUID.ToString(), *GetPathNameSafe(Object), *ObjectPathName);
			checkf(!ObjectPathName.IsEmpty(), TEXT("ObjectPathName is empty. NetGuid: %s. Object: %s"), *NetGUID.ToString(), *GetPathNameSafe(Object));
		}

		// Serialize reference to outer. This is basically a form of compression.
		FNetworkGUID OuterNetGUID = GuidCache->GetOrAssignNetGUID(ObjectOuter);

		// @pavan Calling serialize object instead of internal write in recursive,
		// this solves the ordering problem for dynamic objects that are referenced as object pointers in another objects
		SerializeObject(Ar, UObject::StaticClass(), ObjectOuter, &OuterNetGUID);
		// InternalWriteObject_HS(Ar, OuterNetGUID, ObjectOuter, TEXT(""), nullptr);

		// Look for renamed startup actors
		if (Connection->Driver)
		{
			const FName SearchPath = FName(*ObjectPathName);
			const FName RenamedPath = Connection->Driver->RenamedStartupActors.FindRef(SearchPath);
			if (!RenamedPath.IsNone())
			{
				ObjectPathName = RenamedPath.ToString();
			}
		}

		GEngine->NetworkRemapPath(Connection, ObjectPathName, false);

		// Serialize Name of object
		Ar << ObjectPathName;

		if (FNetGuidCacheObject* CacheObject = GuidCache->ObjectLookup.Find(NetGUID))
		{
			if (CacheObject->PathName.IsNone())
			{
				CacheObject->PathName = FName(*ObjectPathName);
			}

			CacheObject->OuterGUID = OuterNetGUID;
			CacheObject->bNoLoad = ExportFlags.bNoLoad;
			CacheObject->bIgnoreWhenMissing = ExportFlags.bNoLoad;
		}
	}
}

FNetworkGUID UHScalePackageMap::InternalLoadObject_HS(FArchive& Ar, UObject*& Object, const int32 InternalLoadObjectRecursionCount)
{
	if (InternalLoadObjectRecursionCount > INTERNAL_LOAD_OBJECT_RECURSION_LIMIT)
	{
		UE_LOG(LogNetPackageMap, Warning, TEXT( "InternalLoadObject: Hit recursion limit." ));
		Ar.SetError();
		Object = NULL;
		return FNetworkGUID();
	}

	bool bHasNetworkId = false;
	Ar.SerializeBits(&bHasNetworkId, 1);

	// ----------------	
	// Read the NetGUID
	// ----------------

	FNetworkGUID NetGUID;

	if (bHasNetworkId)
	{
		FHScaleNetGUID NetworkGUID;
		Ar << NetworkGUID;

		const FNetworkGUID* FoundGUID = NetGuidMapToObjectGuid.Find(NetworkGUID);
		if (FoundGUID)
		{
			NetGUID = *FoundGUID;
		}
	}
	else
	{
		Ar << NetGUID;
	}

	NET_CHECKSUM_OR_END(Ar);

	if (Ar.IsError())
	{
		Object = NULL;
		return NetGUID;
	}

	if (!bHasNetworkId && !NetGUID.IsValid())
	{
		Object = NULL;
		return NetGUID;
	}

	// ----------------	
	// Try to resolve NetGUID
	// ----------------	
	if (NetGUID.IsValid() && !NetGUID.IsDefault())
	{
		Object = GetObjectFromNetGUID(NetGUID, false);

		UE_LOG(LogNetPackageMap, VeryVerbose, TEXT( "InternalLoadObject loaded %s from NetGUID <%s>" ), Object ? *Object->GetFullName() : TEXT( "NULL" ), *NetGUID.ToString());
	}

	// ----------------	
	// Read the full if its there
	// ----------------	
	FHScaleExportFlags ExportFlags;
	Ar << ExportFlags.Value;

	if (Ar.IsError())
	{
		Object = NULL;
		return NetGUID;
	}

	if (NetGUID.IsValid())
	{
		GuidCache->ImportedNetGuids.Add(NetGUID);
	}

	if (ExportFlags.bHasPath)
	{
		UObject* ObjOuter = NULL;

		FNetworkGUID OuterGUID = InternalLoadObject_HS(Ar, ObjOuter, InternalLoadObjectRecursionCount + 1);

		FString ObjectName;
		Ar << ObjectName;

		const bool bIsPackage = NetGUID.IsStatic() && !OuterGUID.IsValid();

		if (Ar.IsError())
		{
			UE_LOG(LogNetPackageMap, Error, TEXT( "InternalLoadObject: Failed to load path name" ));
			Object = NULL;
			return NetGUID;
		}

		// Remap name for PIE
		GEngine->NetworkRemapPath(Connection, ObjectName, true);

		if (NetGUID.IsDefault())
		{
			// This should be from the client
			// If we get here, we want to go ahead and assign a network guid, 
			// then export that to the client at the next available opportunity
			check(IsNetGUIDAuthority());

			// If the object is not a package and we couldn't find the outer, we have to bail out, since the
			// relative path name is meaningless. This may happen if the outer has been garbage collected.
			if (!bIsPackage && OuterGUID.IsValid() && ObjOuter == nullptr)
			{
				UE_LOG(LogNetPackageMap, Log, TEXT( "InternalLoadObject: couldn't find outer for non-package object. GUID: %s, ObjectName: %s" ), *NetGUID.ToString(), *ObjectName);
				Object = nullptr;
				return NetGUID;
			}

			Object = StaticFindObject(UObject::StaticClass(), ObjOuter, *ObjectName, false);

			// Try to load package if it wasn't found. Note load package fails if the package is already loaded.
			if (Object == nullptr && bIsPackage)
			{
				FPackagePath Path = FPackagePath::FromPackageNameChecked(ObjectName);
				Object = LoadPackage(nullptr, Path, LOAD_None);
			}

			if (Object == NULL)
			{
				UE_LOG(LogNetPackageMap, Warning, TEXT( "UPackageMapClient::InternalLoadObject: Unable to resolve default guid from client: ObjectName: %s, ObjOuter: %s " ), *ObjectName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ));
				return NetGUID;
			}

			if (!IsValid(Object))
			{
				UE_LOG(LogNetPackageMap, Warning, TEXT( "UPackageMapClient::InternalLoadObject: Received reference to invalid object from client: ObjectName: %s, ObjOuter: %s "), *ObjectName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ));
				Object = NULL;
				return NetGUID;
			}

			if (bIsPackage)
			{
				UPackage* Package = Cast<UPackage>(Object);

				if (Package == NULL)
				{
					UE_LOG(LogNetPackageMap, Error, TEXT( "UPackageMapClient::InternalLoadObject: Default object not a package from client: ObjectName: %s, ObjOuter: %s " ), *ObjectName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ));
					Object = NULL;
					return NetGUID;
				}
			}

			// Assign the guid to the object
			NetGUID = GuidCache->GetOrAssignNetGUID(Object);

			// Let this client know what guid we assigned
			HandleUnAssignedObject(Object);

			return NetGUID;
		}
		else if (Object != nullptr)
		{
			// If we already have the object, just do some sanity checking and return
			NetGUID = GuidCache->GetOrAssignNetGUID(Object);
			return NetGUID;
		}

		const bool bIgnoreWhenMissing = ExportFlags.bNoLoad;

		// Register this path and outer guid combo with the net guid
		GuidCache->RegisterNetGUIDFromPath_Client(NetGUID, ObjectName, OuterGUID, 0, ExportFlags.bNoLoad, bIgnoreWhenMissing);

		// Try again now that we've registered the path
		Object = GuidCache->GetObjectFromNetGUID(NetGUID, GuidCache->IsExportingNetGUIDBunch);

		if (Object == NULL && !GuidCache->ShouldIgnoreWhenMissing(NetGUID))
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT( "InternalLoadObject: Unable to resolve object from path. Path: %s, Outer: %s, NetGUID: %s" ), *ObjectName, ObjOuter ? *ObjOuter->GetPathName() : TEXT( "NULL" ), *NetGUID.ToString());
		}
	}

	return NetGUID;
}

bool UHScalePackageMap::WriteActorHeader(FArchive& Ar, AActor* Actor)
{
	// #todo ... add errors maybe

	// Writes all header data (object id excluded, it can be saved using SerializeObject function)
	check(Ar.IsSaving());

	if (!IsValid(Actor)) return false;
	check(Actor->NeedsLoadForClient()); // We have no business sending this unless the client can load

	FHScaleNetGUID LevelId = FHScaleNetGUID::GetDefault();
	Ar << LevelId;

	FVector Location = FVector::ZeroVector;
	const USceneComponent* RootComponent = Actor->GetRootComponent();
	if (RootComponent)
	{
		Location = FRepMovement::RebaseOntoZeroOrigin(Actor->GetActorLocation(), Actor);
	}
	else if (Actor->HasAuthority())
	{
		if (const APlayerController* Pc = UGameplayStatics::GetPlayerController(this, 0))
		{
			FRotator Rotation;
			Pc->GetPlayerViewPoint(Location, Rotation);
			Location = FRepMovement::RebaseOntoZeroOrigin(Location, Actor);
		}
	}

	bool SerSuccess = false;
	FVector_NetQuantize10 Temp = Location;
	Temp.NetSerialize(Ar, this, SerSuccess);

	return true;
}

void UHScalePackageMap::ReadNewActorHeader(FArchive& Ar, FVector& Location, FRotator& Rotation, TOptional<FVector>& Scale, TOptional<FVector>& Velocity)
{
	// @warning - Do not change the implementation of this function without coresponding changes in memory layer,
	// where the bunch is created and all the data collected

	check(Ar.IsLoading());

	// Serialized location
	{
		bool bLocationSerialized;
		FVector_NetQuantize10 TempLocation;
		TempLocation.NetSerialize(Ar, this, bLocationSerialized);
		if (bLocationSerialized)
		{
			Location = TempLocation;
		}
	}

	// Serialized Rotation
	{
		bool bRotationSerialized;
		FRotator TempRotation;
		TempRotation.NetSerialize(Ar, this, bRotationSerialized);
		if (bRotationSerialized)
		{
			Rotation = TempRotation;
		}
	}

	// Serialized Scale
	{
		bool bContainsScale;
		Ar.SerializeBits(&bContainsScale, 1);

		if (bContainsScale)
		{
			bool bScaleSerialized;
			FVector_NetQuantize10 TempScale;
			TempScale.NetSerialize(Ar, this, bScaleSerialized);
			if (bScaleSerialized)
			{
				Scale = TempScale;
			}
		}
	}

	// Serialized velocity
	{
		bool bContainsVelocity;
		Ar.SerializeBits(&bContainsVelocity, 1);

		if (bContainsVelocity)
		{
			bool bVelocitySerialized;
			FVector_NetQuantize10 TempVelocity;
			TempVelocity.NetSerialize(Ar, this, bVelocitySerialized);
			if (bVelocitySerialized)
			{
				Velocity = TempVelocity;
			}
		}
	}

	// // #todo ... spawn actors based on FindObjectFast, StaticFindObject, ...
	// if (!ClassName.IsEmpty())
	// {
	// 	UClass* LoadedClass = FHSClassTranslator::GetInstance().GetClassFromClassPath(ClassName);
	// 	check(LoadedClass);
	//
	// 	Archetype = LoadedClass;
	// }
	// else
	// {
	// 	// #todo ... find static objects
	// }

	// Object = FindObjectFast<UObject>(ObjOuter, CacheObjectPtr->PathName);

	// // Register this path and outer guid combo with the net guid
	// GuidCache->RegisterNetGUIDFromPath_Client( NetGUID, ObjectName, OuterGUID, NetworkChecksum, ExportFlags.bNoLoad, bIgnoreWhenMissing );
	//
	// // Try again now that we've registered the path
	// Object = GuidCache->GetObjectFromNetGUID( NetGUID, GuidCache->IsExportingNetGUIDBunch );

	//Object = StaticFindObject(UObject::StaticClass(), ObjOuter, *ObjectName, false);
}

bool UHScalePackageMap::SerializeObjectId(FArchive& Ar, UObject* Object, FHScaleNetGUID& OutObjectGUID)
{
	if (Ar.IsSaving())
	{
		if (!IsValid(Object))
		{
			return false;
		}

		const FNetworkGUID NetworkGUID = GuidCache->GetOrAssignNetGUID(Object);
		const FHScaleNetGUID* FoundId = ObjectGuidMapToNetGuid.Find(NetworkGUID);
		if (!FoundId && (GetHScaleConnection()->GetAvailableObjectIdCount(Object) > 0))
		{
			// No available ID for new object, skip until new ones are received
			 return false;
		}

		FHScaleNetGUID AssignedId;
		if (FoundId)
		{
			AssignedId = *FoundId;
		}
		else
		{
			AssignedId = FHScaleNetGUID::Create_Object(GetHScaleConnection()->GetNewHScaleObjectId(Object));
			AddGUIDsToMap(AssignedId, NetworkGUID);
		}

		Ar << AssignedId;
		OutObjectGUID = AssignedId;
		return true;
	}

	if (Ar.IsLoading())
	{
		if (Ar.AtEnd())
		{
			return false;
		}

		Ar << OutObjectGUID;
		return true;
	}

	return false;
}

FHScaleNetGUID UHScalePackageMap::FindEntityNetGUID(UObject* Object) const
{
	if (!IsValid(Object))
	{
		return FHScaleNetGUID();
	}

	const FNetworkGUID NetworkGUID = GuidCache->GetOrAssignNetGUID(Object);
	const FHScaleNetGUID* FoundId = ObjectGuidMapToNetGuid.Find(NetworkGUID);
	return FoundId ? *FoundId : FHScaleNetGUID();
}

bool UHScalePackageMap::DoesNetEntityExist(UObject* Object) const
{
	return FindEntityNetGUID(Object).IsValid();
}

UObject* UHScalePackageMap::FindObjectFromEntityID(const FHScaleNetGUID& NetGUID)
{
	if (!NetGUID.IsValid())
	{
		return nullptr;
	}

	UObject* Result = nullptr;

	const FNetworkGUID* NetworkGUID = NetGuidMapToObjectGuid.Find(NetGUID);
	if (NetworkGUID)
	{
		Result = GetObjectFromNetGUID(*NetworkGUID, false);
	}

	return Result;
}

FNetworkGUID UHScalePackageMap::FindNetGUIDFromHSNetGUID(const FHScaleNetGUID& NetGUID)
{
	const FNetworkGUID* FoundId = NetGuidMapToObjectGuid.Find(NetGUID);
	return FoundId ? *FoundId : FNetworkGUID();
}

FHScaleNetGUID UHScalePackageMap::FindEntityNetGUID(const FNetworkGUID& NetGUID) const
{
	const FHScaleNetGUID* FoundId = ObjectGuidMapToNetGuid.Find(NetGUID);
	return FoundId ? *FoundId : FHScaleNetGUID();
}

bool UHScalePackageMap::ShouldSendFullPath_HS(const UObject* Object, const FNetworkGUID& NetGUID)
{
	if (!Connection)
	{
		return false;
	}

	if (!NetGUID.IsValid())
	{
		return false;
	}

	if (!Object->IsNameStableForNetworking())
	{
		checkf(!NetGUID.IsDefault(), TEXT("Non-stably named object %s has a default NetGUID. %s"), *GetFullNameSafe(Object), *Connection->Describe());
		checkf(NetGUID.IsDynamic(), TEXT("Non-stably named object %s has static NetGUID [%s]. %s"), *GetFullNameSafe(Object), *NetGUID.ToString(), *Connection->Describe());
		return false; // We only export objects that have stable names
	}

	return true;
}

void UHScalePackageMap::AssignNetGUID(FNetworkGUID& GUID, UObject* Object)
{
	GUID = GuidCache->GetOrAssignNetGUID(Object);
}

void UHScalePackageMap::AssignOrGenerateHSNetGUIDForObject(const FHScaleNetGUID& HSNetGUID, UObject* Object)
{
	const FNetworkGUID NetworkGUID = GuidCache->GetOrAssignNetGUID(Object);
	AddGUIDsToMap(HSNetGUID, NetworkGUID);
}

void UHScalePackageMap::CleanUpObjectGuid(const FNetworkGUID NetGUID)
{
	if (!NetGUID.IsValid()) return;

	GuidCache->ObjectLookup.Remove(NetGUID);
	//GuidCache->NetGUIDLookup.Remove(nullptr); // #todo ... <<< --- Maybe do this only from time to time not after every object is removed
	GuidCache->ImportedNetGuids.Remove(NetGUID);
	GuidCache->PendingOuterNetGuids.Remove(NetGUID);

	if (FHScaleNetGUID* EntityGUID = ObjectGuidMapToNetGuid.Find(NetGUID))
	{
		NetGuidMapToObjectGuid.Remove(*EntityGUID);
	}

	ObjectGuidMapToNetGuid.Remove(NetGUID);
}


void UHScalePackageMap::PreRemoteActorSpawn(AActor* InActor)
{
	InActor->ExchangeNetRoles(true);
}

UHScaleConnection* UHScalePackageMap::GetHScaleConnection()
{
	return StaticCast<UHScaleConnection*>(GetConnection());
}

void UHScalePackageMap::AddGUIDsToMap(const FHScaleNetGUID& HScaleGUID, const FNetworkGUID& NetGUID)
{
	ObjectGuidMapToNetGuid.Add(NetGUID, HScaleGUID);
	NetGuidMapToObjectGuid.Add(HScaleGUID, NetGUID);
}

void UHScalePackageMap::RemoveGUIDsFromMap(const FHScaleNetGUID& HScaleGUID)
{
	if (NetGuidMapToObjectGuid.Contains(HScaleGUID))
	{
		const FNetworkGUID NetworkGUID = NetGuidMapToObjectGuid[HScaleGUID];
		ObjectGuidMapToNetGuid.Remove(NetworkGUID);
	}
	NetGuidMapToObjectGuid.Remove(HScaleGUID);
}