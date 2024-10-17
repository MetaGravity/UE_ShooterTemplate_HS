#include "Events/HScaleEventsDriver.h"

#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "ReplicationLayer/HScaleRepDriver.h"

void FHScaleEventsDriver::OnEntityDestroyed(const FHScaleNetGUID& EntityId)
{
	NetworkDestructionEntities.Remove(EntityId);
	LocalDestructionEntities.Remove(EntityId);
}

void FHScaleEventsDriver::Tick(float DeltaSeconds)
{
	if (!IsValid(Connection) || !Connection->IsConnectionActive()) return;
	SendNetworkDestructionEvents();
	DestroyMarkedObjectsLocally();
	RemoveToDestroyEntities();
}

bool FHScaleEventsDriver::SendObjectDespawnEvent(const FHScaleNetGUID& EntityId) const
{
	if (!EntityId.IsObject())
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received Invalid EntityId to despawn %s"), *EntityId.ToString())
		return false;
	}

	uint64_t ObjectId = EntityId.Get();
	return SendEvent(ObjectsDespawned, EntityId, reinterpret_cast<uint8_t*>(&ObjectId), sizeof(ObjectId));
}

bool FHScaleEventsDriver::SendObjectDespawnEvent(const uint8* Data, const size_t Size) const
{
	if (!Size) return false;

	return SendEvent(ObjectsDespawned, EHScaleEventRadius::Max, Data, Size);
}

// #TODO similar to objects despawn, this can also be batch sent
bool FHScaleEventsDriver::SendForgetObjectsEvent(const FHScaleNetGUID& EntityId) const
{
	if (!EntityId.IsObject())
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received Invalid ObjectId to Forget %s"), *EntityId.ToString())
		return false;
	}

	uint64_t ObjectId = EntityId.Get();
	return SendEvent(ForgetObjects, EntityId, reinterpret_cast<uint8_t*>(&ObjectId), sizeof(ObjectId));
}

bool FHScaleEventsDriver::SendForgetPlayerEvent(const FHScaleNetGUID& EntityId) const
{
	if (!EntityId.IsPlayer())
	{
		UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received Invalid PlayerId to Forget %s"), *EntityId.ToString())
		return false;
	}

	uint32 PlayerId = EntityId.Get();
	return SendEvent(ForgetPlayers, EntityId, reinterpret_cast<uint8_t*>(&PlayerId), sizeof(PlayerId));
}

bool FHScaleEventsDriver::SendGetFreeObjectIds(bool ForPlayerObjects) const
{
	return SendEvent(ForPlayerObjects ? GetFreePlayerObjectIds : GetFreeGlobalObjectIds, EHScaleEventRadius::None);
}

void FHScaleEventsDriver::HandleDisconnectedEvent(const quark::remote_event& Update)
{
	const uint8_t* Data = Update.data();
	const size_t DataSize = Update.size();

	size_t Index = 0;
	while (Index + sizeof(uint32) <= DataSize)
	{
		uint32 PlayerSessionId = 0;
		std::memcpy(&PlayerSessionId, Data + Index, sizeof(uint32));
		Index += sizeof(uint32);

		if (!PlayerSessionId) continue;
		FHScaleNetGUID PlayerNetGUID = FHScaleNetGUID::Create_Player(PlayerSessionId);
		MarkEntityForToDestroy(PlayerNetGUID);
	}
}

void FHScaleEventsDriver::HandleDespawnEvent(const quark::remote_event& Update)
{
	const uint8_t* Data = Update.data();
	const size_t DataSize = Update.size();

	size_t Index = 0;
	while (Index + sizeof(uint64) <= DataSize)
	{
		uint64 ObjectId = 0;
		std::memcpy(&ObjectId, Data + Index, sizeof(uint64));
		Index += sizeof(uint64);

		if (!ObjectId) continue;
		FHScaleNetGUID EntityId = FHScaleNetGUID::Create_Object(ObjectId);

		if (!EntityId.IsObject())
		{
			UE_LOG(Log_HyperScaleReplication, Warning, TEXT("Received invalid EntityId to despawn %s"), *EntityId.ToString());
			continue;
		}
		MarkEntityForLocalDestruction(EntityId);
	}
}

void FHScaleEventsDriver::HandleEvents(const quark::remote_event& Update)
{
	uint16_t EventId = Update.event_class();

	if (FHScalePropertyIdConverters::IsSystemEvent(EventId))
	{
		HandleSystemEvent(Update);
	}
	else if (FHScalePropertyIdConverters::IsReservedEvent(EventId))
	{
		HandleReservedEvent(Update);
	}
	else if (FHScalePropertyIdConverters::IsApplicationEvent(EventId))
	{
		HandleApplicationEvents(Update);
	}
}

void FHScaleEventsDriver::HandleSystemEvent(const quark::remote_event& Update)
{
	switch (const uint16_t EventId = Update.event_class())
	{
		case ObjectsDespawned:
			HandleDespawnEvent(Update);
			break;

		case PlayerDisconnected:
			HandleDisconnectedEvent(Update);
			break;
		case EHScale_QuarkEventType::GetFreeGlobalObjectIds:
		{
			int ID_amount = Update.size() / sizeof(uint64_t);
			const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
			const uint64_t* ids = reinterpret_cast<const uint64_t*>(Update.data());
			for (int i = 0; i < ID_amount; ++i)
			{
				NetDriver->GetHyperScaleConnection()->FreeGlobalObjectIDCache.Enqueue(ids[i]);
			}
			break;
		}
		case EHScale_QuarkEventType::GetFreePlayerObjectIds:
		{
			UE_LOG(Log_HyperScaleEvents, Log, TEXT("GetFreePlayerObjectIds received"));
			int ID_amount = Update.size() / sizeof(uint64_t);
			const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
			const uint64_t* ids = reinterpret_cast<const uint64_t*>(Update.data());
			for (int i = 0; i < ID_amount; ++i)
			{
				NetDriver->GetHyperScaleConnection()->FreePlayerObjectIDCache.Enqueue(ids[i]);
			}
			break;
		}
		default:
			UE_LOG(Log_HyperScaleEvents, Verbose, TEXT("Received unhandled system event of type %d"), EventId)
	}
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleEventsDriver::GetNetworkDestructionEntitiesIterator() const
{
	return NetworkDestructionEntities.CreateConstIterator();
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleEventsDriver::GetLocalDestructionEntitiesIterator() const
{
	return LocalDestructionEntities.CreateConstIterator();
}

TSet<FHScaleNetGUID>::TConstIterator FHScaleEventsDriver::GetToBeDestroyedEntitiesIterator() const
{
	return ToDestroyEntities.CreateConstIterator();
}

void FHScaleEventsDriver::MarkEntityForNetworkDestruction(const FHScaleNetGUID& EntityId)
{
	if (LocalDestructionEntities.Contains(EntityId)) return;

	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
	check(Bibliothec)
	TSharedPtr<FHScaleNetworkEntity> CachedEntity = Bibliothec->FindExistingEntity(EntityId);

	if (!CachedEntity.IsValid()) return;

	FHScaleNetworkEntity* Entity = CachedEntity.Get();

	if (Entity->MarkEntityForNetworkDestruction())
	{
		NetworkDestructionEntities.Add(EntityId);
	}
}

void FHScaleEventsDriver::MarkEntityForLocalDestruction(const FHScaleNetGUID& EntityId)
{
	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
	check(Bibliothec)
	TSharedPtr<FHScaleNetworkEntity> CachedEntity = Bibliothec->FindExistingEntity(EntityId);

	if (!CachedEntity.IsValid()) return;

	FHScaleNetworkEntity* Entity = CachedEntity.Get();

	if (Entity->MarkEntityForLocalDestruction())
	{
		LocalDestructionEntities.Add(EntityId);
	}
}

void FHScaleEventsDriver::MarkEntityForToDestroy(const FHScaleNetGUID& EntityId)
{
	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
	check(Bibliothec)
	TSharedPtr<FHScaleNetworkEntity> CachedEntity = Bibliothec->FindExistingEntity(EntityId);

	if (!CachedEntity.IsValid()) return;

	FHScaleNetworkEntity* Entity = CachedEntity.Get();

	if (Entity->MarkEntityForDestroy())
	{
		ToDestroyEntities.Add(EntityId);
	}
}

quark::session* FHScaleEventsDriver::GetNetworkSession() const
{
	if (!IsValid(Connection) || !Connection->IsConnectionActive()) return nullptr;

	return Connection->GetNetworkSession();
}

bool FHScaleEventsDriver::SendEvent(const quark_event_class_t EventId, const FHScaleNetGUID& EntityId, FBitWriter& Ar) const
{
	return SendEvent(EventId, EntityId, Ar.GetData(), Ar.GetNumBytes());
}

bool FHScaleEventsDriver::SendEvent(const quark_event_class_t EventId, const EHScaleEventRadius Radius, FBitWriter& Ar) const
{
	return SendEvent(EventId, Radius, Ar.GetData(), Ar.GetNumBytes());
}

bool FHScaleEventsDriver::SendEvent(const quark_event_class_t EventId, const FHScaleNetGUID& EntityId, const uint8* Data, const size_t Size) const
{
	const quark::local_event Event(EventId, quark::recipient::object(EntityId.Get()), Data, Size);
	quark::session* Session = GetNetworkSession();
	if (!Session) return false;

	const quark::error ResultError = Session->send(quark::local_update::event(Event)).error();
	if (ResultError.is_error())
	{
		UE_LOG(Log_HyperScaleEvents, Error, TEXT("Quark event send error: %hs"), ResultError.message());
		return false;
	}

	return true;
}

bool FHScaleEventsDriver::SendEvent(const quark_event_class_t EventId, const EHScaleEventRadius Radius, const uint8* Data, const size_t Size) const
{
	quark::recipient Recipient = quark::recipient::radius(quark::radius(HS_EVENT_RADIUS_MEDIUM));
	if (Radius == EHScaleEventRadius::None)
	{
		Recipient = quark::recipient::radius(quark::radius::none());
	}
	else if (Radius == EHScaleEventRadius::Low)
	{
		Recipient = quark::recipient::radius(quark::radius(HS_EVENT_RADIUS_LOW));
	}
	else if (Radius == EHScaleEventRadius::Medium)
	{
		// Recipient = quark::recipient::radius(quark::radius(HS_EVENT_RADIUS_MEDIUM));
	}
	else
	{
		Recipient = quark::recipient::radius(quark::radius::max());
	}
	const quark::local_event Event(EventId, Recipient, Data, Size);
	quark::session* Session = GetNetworkSession();
	if (!Session) return false;

	const quark::error ResultError = Session->send(quark::local_update::event(Event)).error();
	if (ResultError.is_error())
	{
		UE_LOG(Log_HyperScaleEvents, Error, TEXT("Quark event send error: %hs"), ResultError.message());
		return false;
	}

	return true;
}

FHScaleNetworkBibliothec* FHScaleEventsDriver::GetBibliothec() const
{
	if (!IsValid(Connection)) return nullptr;
	const UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
	return NetDriver->GetBibliothec();
}

void FHScaleEventsDriver::SendNetworkDestructionEvents()
{
	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();

	uint8 ObjectIdsData[QUARK_MAX_PAYLOAD_LEN];
	size_t Index = 0;
	bool bSuccess = true;

	for (TSet<FHScaleNetGUID>::TConstIterator It = GetNetworkDestructionEntitiesIterator(); It; ++It)
	{
		const FHScaleNetGUID EntityId = *It;
		TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FindExistingEntity(EntityId);
		if (!Entity.IsValid()) continue;

		uint64 Obj = EntityId.Get();
		std::memcpy(&ObjectIdsData[Index], &Obj, sizeof(uint64));
		Index += sizeof(uint64_t);

		if (Index + sizeof(uint64) > QUARK_MAX_PAYLOAD_LEN)
		{
			bSuccess &= SendObjectDespawnEvent(ObjectIdsData, Index);
			Index = 0;
			UE_LOG(Log_HyperScaleEvents, VeryVerbose, TEXT("Object Despawn event send status %d"), bSuccess)
		}

		UE_LOG(Log_HyperScaleEvents, VeryVerbose, TEXT("Object Despawn Event for %s"), *EntityId.ToString())

		if (!Entity->IsComponent() && !Entity->IsSubStructEntity())
		{
			MarkEntityForToDestroy(EntityId);
		}
	}

	if (Index > 0)
	{
		bSuccess &= SendObjectDespawnEvent(ObjectIdsData, Index);
		UE_LOG(Log_HyperScaleEvents, VeryVerbose, TEXT("Object Despawn event send status %d"), bSuccess)
	}

	ClearNetworkDestructionEntities();
}

void FHScaleEventsDriver::DestroyMarkedObjectsLocally()
{
	if (!IsValid(Connection)) return;
	UHScaleNetDriver* NetDriver = Cast<UHScaleNetDriver>(Connection->Driver);
	FHScaleNetworkBibliothec* Bibliothec = NetDriver->GetBibliothec();

	UHScaleRepDriver* RepDriver = Cast<UHScaleRepDriver>(NetDriver->GetReplicationDriver());
	check(RepDriver)

	uint8 PrevActClient = NetDriver->bActAsClient;
	NetDriver->bActAsClient = 1;
	for (TSet<FHScaleNetGUID>::TConstIterator It = GetLocalDestructionEntitiesIterator(); It; ++It)
	{
		const FHScaleNetGUID EntityId = *It;
		TSharedPtr<FHScaleNetworkEntity> Entity = Bibliothec->FindExistingEntity(EntityId);
		if (!Entity.IsValid()) continue;

		if (Entity->IsActor())
		{
			RepDriver->ActorRemotelyDestroyed(EntityId);
		}
		if (!Entity->IsComponent() && !Entity->IsSubStructEntity())
		{
			MarkEntityForToDestroy(EntityId);
		}
	}

	NetDriver->bActAsClient = PrevActClient;
	ClearLocalDestructionEntities();
}

void FHScaleEventsDriver::RemoveToDestroyEntities()
{
	FHScaleNetworkBibliothec* Bibliothec = GetBibliothec();
	check(Bibliothec)

	for (TSet<FHScaleNetGUID>::TConstIterator It = GetToBeDestroyedEntitiesIterator(); It; ++It)
	{
		const FHScaleNetGUID EntityId = *It;
		Bibliothec->DestroyEntity(EntityId);
	}

	ClearToDestroyEntities();
}

void FHScaleEventsDriver::HandleReservedEvent(const quark::remote_event& Update) {}