#pragma once
#include "Core/HScaleResources.h"

class FHScaleNetworkBibliothec;
class UHScaleConnection;

enum class EHScaleEventRadius
{
	None,
	Low,
	Medium,
	Max,
};

class HYPERSCALERUNTIME_API FHScaleEventsDriver
{
public:
	FHScaleEventsDriver(UHScaleConnection* Connection)
		: Connection(Connection) {}

	virtual ~FHScaleEventsDriver() {}
	void OnEntityDestroyed(const FHScaleNetGUID& EntityId);

	void Tick(float DeltaSeconds);

	bool SendObjectDespawnEvent(const FHScaleNetGUID& EntityId) const;
	bool SendForgetObjectsEvent(const FHScaleNetGUID& EntityId) const;
	bool SendForgetPlayerEvent(const FHScaleNetGUID& EntityId) const;
	bool SendGetFreeObjectIds(bool ForPlayerObjects = true) const;
	void HandleEvents(const quark::remote_event& Update);

	void ProcessLocalRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject);

	void ClearNetworkDestructionEntities()
	{
		NetworkDestructionEntities.Empty();
	}

	void ClearLocalDestructionEntities()
	{
		LocalDestructionEntities.Empty();
	}

	void ClearToDestroyEntities()
	{
		ToDestroyEntities.Empty();
	}

	TSet<FHScaleNetGUID>::TConstIterator GetNetworkDestructionEntitiesIterator() const;
	TSet<FHScaleNetGUID>::TConstIterator GetLocalDestructionEntitiesIterator() const;
	TSet<FHScaleNetGUID>::TConstIterator GetToBeDestroyedEntitiesIterator() const;

	void MarkEntityForNetworkDestruction(const FHScaleNetGUID& EntityId);
	void MarkEntityForLocalDestruction(const FHScaleNetGUID& EntityId);
	void MarkEntityForToDestroy(const FHScaleNetGUID& EntityId);

private:
	quark::session* GetNetworkSession() const;

	bool SendEvent(const quark_event_class_t EventId, const FHScaleNetGUID& EntityId, FBitWriter& Ar) const;
	bool SendEvent(const quark_event_class_t EventId, const EHScaleEventRadius Radius, FBitWriter& Ar) const;
	bool SendEvent(const quark_event_class_t EventId, const FHScaleNetGUID& EntityId, const uint8* Data = nullptr, const size_t Size = 0) const;
	bool SendEvent(const quark_event_class_t EventId, const EHScaleEventRadius Radius, const uint8* Data = nullptr, const size_t Size = 0) const;
	FHScaleNetworkBibliothec* GetBibliothec() const;

	void SendNetworkDestructionEvents();
	void DestroyMarkedObjectsLocally();
	void RemoveToDestroyEntities();

	bool SendObjectDespawnEvent(const uint8* Data, const size_t Size) const;

	void HandleDespawnEvent(const quark::remote_event& Update);
	void HandleApplicationEvents(const quark::remote_event& Update);
	void HandleDisconnectedEvent(const quark::remote_event& Update);
	void HandleSystemEvent(const quark::remote_event& Update);
	void HandleReservedEvent(const quark::remote_event& Update);

	bool ReadRPCParams(::FBitWriter& Writer, ::FBitReader& Reader, const TSharedPtr<FRepLayout>& RepLayout) const;
	bool WriteRPCParams(::FBitWriter& Writer, ::FBitReader& Reader, const TSharedPtr<FRepLayout>& RepLayout) const;
	void ProcessRemoteFunctionForChannelPrivate_HS(UActorChannel* Ch, const FClassNetCache* ClassCache,
		const FFieldNetCache* FieldCache, UObject* TargetObject,
		UFunction* Function, void* Parameters) const;

	UHScaleConnection* Connection;

	TSet<FHScaleNetGUID> NetworkDestructionEntities;

	TSet<FHScaleNetGUID> LocalDestructionEntities;

	TSet<FHScaleNetGUID> ToDestroyEntities;
};