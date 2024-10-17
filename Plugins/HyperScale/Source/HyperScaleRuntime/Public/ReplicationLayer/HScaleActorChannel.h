// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Core/HScaleResources.h"
#include "Engine/ActorChannel.h"
#include "HScaleActorChannel.generated.h"

#ifdef SUBOBJECT_TRANSITION_VALIDATION
#undef SUBOBJECT_TRANSITION_VALIDATION
#endif

#define SUBOBJECT_TRANSITION_VALIDATION 0

/**
 * 
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleActorChannel : public UActorChannel
{
	GENERATED_BODY()

public:
	UHScaleActorChannel();

	// ~Begin of UChannel interface.
public:
	// virtual void Tick() override;
	virtual void ReceivedBunch(FInBunch& Bunch) override;
	virtual FPacketIdRange SendBunch(FOutBunch* Bunch, bool Merge) override;
	virtual void NotifyActorChannelOpen(AActor* InActor, FInBunch& InBunch) override;
	/** Custom implementation for ReplicateSubobject when RepFlags.bUseCustomSubobjectReplication is true */
	virtual bool ReplicateSubobjectCustom(UObject* Obj, FOutBunch& Bunch, const FReplicationFlags& RepFlags) override;
	void ProcessBunch_HS(FInBunch& Bunch);
	virtual bool CleanUp(const bool bForDestroy, EChannelCloseReason CloseReason) override;
	virtual void AddedToChannelPool() override;
	// ~End of UChannel interface.

	void ReplicateActorToMemoryLayer();

	/**
	 * Cached redirectories for actor and its subobjects (maybe will be removed later)
	 * #todo.... (actor pooling system) needs to be reset when owning entity is changed
	 */
	UPROPERTY()
	TMap<FHScaleNetGUID, TWeakObjectPtr<UObject>> ReplicationRedirectories;

	/**
	 * Cached outers data from actor bunches
	 * It will be used for finding the actors instances based on outers
	 * #todo.... (actor pooling system) needs to be reset when owning entity is changed
	 */
	UPROPERTY()
	TMap<FHScaleNetGUID, TWeakObjectPtr<UObject>> OutersData;

	FHScaleNetGUID EntityId;

	/** Cached ownership of the actor channel actor */
	TOptional<EHScale_Ownership> SchemeOwnership;

	TOptional<EHScale_Blend> SchemeObjectBlendMode;


	// @pavan accessing this value from Entity
	FHScaleNetGUID PlayerNetOwnerGUID;

	static uint8 GetSubObjectDeleteTearOffFlag()
	{
		return static_cast<uint8>(ESubObjectDeleteFlag::TearOff);
	} 

	TSharedRef<FObjectReplicator>* FindObjectReplicator(UObject* Object);

	UObject* ReadContentBlockPayload_HS(FInBunch& Bunch, FNetBitReader& OutPayload, bool& bOutHasRepLayout);

	UObject* ReadContentBlockHeader_HS(FInBunch& Bunch, bool& bObjectDeleted, bool& bOutHasRepLayout);

	TSharedRef<FObjectReplicator>& FindOrCreateReplicator_HS(UObject* Obj);

private:
	/** Tracks whether or not our actor has been seen as pending kill. */
	uint32 bCustomActorIsPendingKill : 1;

	// @pavan added this for to filter out first time and existing actors
	bool bNetInitial;

	// @pavan refactored some code
	void WriteExistingActorHeader(bool& bWroteSomethingImportant, FOutBunch& Bunch);
	void ReadExistingActorHeader(FInBunch& Bunch);
	bool UpdateDeletedSubObjects_HS(FOutBunch& Bunch);

	uint16 ChannelSubObjectDirtyCount_HS = 0;
};