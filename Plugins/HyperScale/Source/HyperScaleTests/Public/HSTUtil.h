#pragma once

class FHSTUtil
{
public:
	static UWorld* GetPrimaryWorld();
	static UWorld* CreateUnitTestWorld();

	static UNetDriver* CreateUnitNetDriver(UWorld* World);
	static UNetConnection* CreateUnitNetConnection(UNetDriver* Driver);
	static UActorChannel* CreateUnitActorChannelForActor(AActor* Actor, TObjectPtr<UNetConnection> NetConnection);
	static FNetworkGUID CreateUnitNetGuid(uint64 ObjectId);
};
