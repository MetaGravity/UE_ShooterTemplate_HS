// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "HSTUtil.h"
#include "Actors/TestHScaleCharacter.h"
#include "Engine/ActorChannel.h"
#include "MemoryLayer/HScaleNetworkEntity.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "MemoryLayer/HScaleNetworkEntityTestUtil.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActorBunchSerializationTest, "HyperScale.MemoryLayer.NetworkEntity.Serialization",
	EAutomationTestFlags::Type(EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter))

bool FActorBunchSerializationTest::RunTest(const FString& Parameters)
{
	UWorld* UnitTestWorld = FHSTUtil::CreateUnitTestWorld();
	AActor* Actor = UnitTestWorld->SpawnActor(ATestHScaleCharacter::StaticClass());

	UNetDriver* Driver = FHSTUtil::CreateUnitNetDriver(UnitTestWorld);
	UNetConnection* NetConnection = FHSTUtil::CreateUnitNetConnection(Driver);

	constexpr uint64 ObjectId = 12346;
	constexpr uint64 ClassId = 1002;
	const FNetworkGUID NetGuid = FHSTUtil::CreateUnitNetGuid(ObjectId);
	Driver->GuidCache->RegisterNetGUID_Client(NetGuid, Actor);

	UActorChannel* ActorChannel = FHSTUtil::CreateUnitActorChannelForActor(Actor, NetConnection);

	// OutBunch Mock data for the character actor, copied from actual byte buffer sent during unreal standard replication
	TArray<uint8> ByteBuffer = {139, 1, 83, 143, 168, 247, 1, 0, 0, 0};

	// #todo fix this test, this one has linkage problems with FInBunch
	// FHScaleInBunch InBunch(NetConnection, ByteBuffer.GetData(), 59, ObjectId);
	// InBunch.Channel = ActorChannel;

	const TSharedPtr<FHScaleNetworkEntity> Entity = HScaleNetworkEntityTestUtil::CreateUnitEntity(ObjectId, ClassId);
	// Entity->Push(InBunch);

	TestEqual(TEXT("Expected number of local dirty props"), Entity->NumLocalDirtyProps(), 1);

	const uint16 PropertyToLook = FHScalePropertyIdConverters::GetAppPropertyIdFromHandle(48);
	if (!TestTrue(TEXT("Is property exists in entity?"), HScaleNetworkEntityTestUtil::CheckIsPropertyExists(Entity, PropertyToLook))) { return false; }

	const FHScaleProperty* Property = HScaleNetworkEntityTestUtil::GetProperty(Entity, PropertyToLook);
	TestTrue(TEXT("The property is of given type"), Property->IsA<HScaleTypes::FHScaleFloatProperty>());

	const HScaleTypes::FHScaleFloatProperty* FloatProperty = static_cast<const HScaleTypes::FHScaleFloatProperty*>(Property);
	TestEqual(TEXT("Property value"), FloatProperty->GetValue(), 0.478652298f);

	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
