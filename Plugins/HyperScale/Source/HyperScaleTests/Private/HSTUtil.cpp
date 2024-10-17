#include "HSTUtil.h"

#include "Engine/ActorChannel.h"
#include "NetworkLayer/HScaleConnection.h"
#include "NetworkLayer/HScaleNetDriver.h"
#include "ReplicationLayer/HScaleActorChannel.h"

UWorld* FHSTUtil::GetPrimaryWorld()
{
	UWorld* ReturnVal = NULL;

	if (GEngine != NULL)
	{
		for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
		{
			const FWorldContext& Context = *It;

			if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
			{
				ReturnVal = Context.World();
				break;
			}
		}
	}

	return ReturnVal;
}

UWorld* FHSTUtil::CreateUnitTestWorld()
{
	UWorld* ReturnVal = NULL;

	ReturnVal = UWorld::CreateWorld(EWorldType::None, false);

	if (ReturnVal != NULL)
	{
		// Hack-mark the world as having begun play (when it has not)
		ReturnVal->bBegunPlay = true;

		// Hack-mark the world as having initialized actors (to allow RPC hooks)
		ReturnVal->bActorsInitialized = true;

		if (!GIsEditor)
		{
			AWorldSettings* CurSettings = ReturnVal->GetWorldSettings();

			if (CurSettings != NULL)
			{
				ULocalPlayer* PrimLocPlayer = GEngine->GetFirstGamePlayer(FHSTUtil::GetPrimaryWorld());
				APlayerController* PrimPC = (PrimLocPlayer != NULL ? ToRawPtr(PrimLocPlayer->PlayerController) : NULL);
				APlayerState* PrimState = (PrimPC != NULL ? ToRawPtr(PrimPC->PlayerState) : NULL);

				if (PrimState != NULL)
				{
					CurSettings->SetPauserPlayerState(PrimState);
				}
			}
		}

		// Create a blank world context, to prevent crashes
		FWorldContext& CurContext = GEngine->CreateNewWorldContext(EWorldType::None);
		CurContext.SetCurrentWorld(ReturnVal);
	}

	return ReturnVal;
}

UNetDriver* FHSTUtil::CreateUnitNetDriver(UWorld* World)
{
	GEngine->CreateNamedNetDriver(World,TEXT("HScaleNetDriver"), TEXT("HScaleNetDriver"));
	UNetDriver* TempNetDriver = GEngine->FindNamedNetDriver(World, TEXT("HScaleNetDriver"));
	UHScaleNetDriver* HyperScaleDriver = Cast<UHScaleNetDriver>(TempNetDriver);
	HyperScaleDriver->SetWorld(World);
	World->SetNetDriver(HyperScaleDriver);

	return HyperScaleDriver;
}

UNetConnection* FHSTUtil::CreateUnitNetConnection(UNetDriver* NetDriver)
{
	UHScaleConnection* Connection = NewObject<UHScaleConnection>(UHScaleConnection::StaticClass());
	NetDriver->ServerConnection = Connection;
	Connection->MaxPacket = 1024;
	Connection->Driver = NetDriver;

	return Connection;
}

UActorChannel* FHSTUtil::CreateUnitActorChannelForActor(AActor* Actor, TObjectPtr<UNetConnection> NetConnection)
{
	UActorChannel* ActorChannel = NewObject<UHScaleActorChannel>();
	ActorChannel->Connection = NetConnection;
	ActorChannel->ChName = NAME_Actor;
	ActorChannel->SetChannelActor(Actor, ESetChannelActorFlags::None);

	return ActorChannel;
}

FNetworkGUID FHSTUtil::CreateUnitNetGuid(uint64 ObjectId)
{
	FNetworkGUID NetGUID;
	FBitWriter Writer(8192);
	Writer.SerializeIntPacked64(ObjectId);
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	Reader << NetGUID;

	return NetGUID;
}
