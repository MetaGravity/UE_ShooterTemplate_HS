// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Core/HScaleResources.h"
#include "Engine/NetConnection.h"
#include "Events/HScaleEventsDriver.h"
#include "MemoryLayer/HScaleNetworkEntity.h"
#include "HScaleConnection.generated.h"


class UHScaleRelevancyManager;

DECLARE_DELEGATE_RetVal(bool, FOnLowItemCountSignature);

class TGUID_Cache : protected TDoubleLinkedList<uint64_t>
{
	int32 LowItemThreshold = QUARK_MAX_SEQUENCE_LENGTH;

public:
	bool Enqueue(const uint64_t& Item)
	{
		bool success = AddHead(Item);
		if (success) bNewItemsRequested = false;

		return success;
	}

	bool Dequeue(uint64_t& Item)
	{
		TDoubleLinkedList<uint64_t>::TDoubleLinkedListNode *tail = GetTail();
		if (!tail)
		{
			RequestNewItems();
			Item = 0ull;
			return false;
		}

		Item = tail->GetValue();
		RemoveNode(tail);

		return true;
	}

	int32 GetLowItemTreshold() const { return LowItemThreshold; }
	void SetLowItemTreshold(int32 NewTreshold)
	{
		LowItemThreshold = NewTreshold;
		RequestNewItems();
	}

	int32 Num() const { return TDoubleLinkedList::Num(); }
	bool IsEmpty() const { return TDoubleLinkedList::IsEmpty(); }

	template<typename FunctorType>
	void BindOnLowItemCountDelegate(FunctorType&& Func)
	{
		OnLowItemCount.BindLambda(Func);
		RequestNewItems();
	}

protected:
	virtual void SetListSize(int32 NewListSize) override
	{
		int32 OldListSize = Num();
		TDoubleLinkedList::SetListSize(NewListSize);
		if (NewListSize <= OldListSize)
		{
			RequestNewItems();
		}
	}

private:
	bool bNewItemsRequested = false;

	bool RequestNewItems()
	{
		if ((Num() >= LowItemThreshold) || bNewItemsRequested || !OnLowItemCount.IsBound())
		{
			return false;
		}

		int32 requestCount = (LowItemThreshold - Num() - 1) / QUARK_MAX_SEQUENCE_LENGTH + 1;

		for (int32 i = 0; i < requestCount; ++i)
		{
			if (!OnLowItemCount.Execute()) return false;
		}

		bNewItemsRequested = true;
		return true;
	}

	FOnLowItemCountSignature OnLowItemCount;
};

namespace quark
{
	class session;
}

/**
 * The class will establish a new connection with hyperscale server and handling
 * and handling all connection issues with the server runtime
 *
 * The connection is established in function InitHyperScaleConnection()
 * and the connection instance is saved in UHScaleNetDriver, property ServerConnection
 */
UCLASS()
class HYPERSCALERUNTIME_API UHScaleConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	UHScaleConnection();

	TGUID_Cache FreeGlobalObjectIDCache;
	TGUID_Cache FreePlayerObjectIDCache;

	// ~Begin of UNetConnection interface
public:
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual void NotifyActorDestroyed(AActor* Actor, bool IsSeamlessTravel = false) override;
	void SubscribeRelevancy();
	virtual void CleanUp() override;
	// virtual TSharedPtr<FObjectReplicator> CreateReplicatorForNewActorChannel(UObject* Object) override;
	// ~End of UNetConnection interface

	/**
	 * Inits and stores session with hyperscale server
	 *
	 * @return - True, if session was established with the server and is ready to use
	 */
	virtual bool InitHyperScaleConnection();

	virtual void TryReceiveData() {}

	/** Returns established network session with hyperscale server */
	quark::session* GetNetworkSession() const { return NetworkSession.Get(); }

	quark_session_id_t GetNetworkSessionId() const;

	FHScaleEventsDriver* GetEventsDriver() const { return EventsDriver.Get(); }

	/** Returns true, if session was established with the server and is ready to use */
	bool IsConnectionActive() const { return NetworkSession.Get() != nullptr; }
	bool IsConnectionFullyEstablished() const;

	const FGameplayTagContainer& GetLevelRoles() const { return LevelRoles; }

	quark_session_id_t GetSessionId() const;
	FHScaleNetGUID GetSessionNetGUID() const;

	// These functions are here only because the master ones are not exposed for the other modules,
	// and they are needed for custom bunch serialization in class FHScaleInBunch
	uint32 GetNetworkCustomVersion_HS(const FGuid& VersionGuid) const;

	int32 GetNecessaryObjectIdCount(AActor* Actor) const;
	int32 GetAvailableObjectIdCount(const UObject* Object) const;
	// Returns quark equivalent object Id for current session
	uint64 GetNewHScaleObjectId(UObject* Object);
	UHScaleRelevancyManager* GetRelevancyManager() const;

	void AddUnmappedObjectPtr(const FHScaleNetGUID& ObjPtrId, TTuple<FHScaleNetGUID, uint16> Pair);
	void RemoveUnmappedObjectPtrs(const FHScaleNetGUID& ObjPtrGUID);

	FHScaleNetGUID FetchNextAvailableDynamicEntityId();

private:
	/**
	 * Stored server session from function InitHyperScaleConnection()
	 * The session is responsible for synchronization with hyperscale server
	 */
	TUniquePtr<quark::session> NetworkSession;

	TUniquePtr<FHScaleEventsDriver> EventsDriver;

	/**
	 * Cached level roles from URL options
	 */
	FGameplayTagContainer LevelRoles;

	void Receive();

	void Send();

public:
	int32 GetOrCreateChannelIndexForEntity(FHScaleNetworkEntity* Entity);

	void PullUnmappedEntityUpdate(const FHScaleNetGUID& EntityId);

private:
	void PullDataFromMemoryLayer();

	int32 GetFreeChannelIndex(const FName& ChName);


	TMap<FHScaleNetGUID, TSet<TTuple<FHScaleNetGUID, uint16>>> UnMappedObjPtrs;
};