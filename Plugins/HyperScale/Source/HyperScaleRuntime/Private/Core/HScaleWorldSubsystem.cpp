// Copyright 2024 Metagravity. All Rights Reserved.


#include "Core/HScaleWorldSubsystem.h"

#include "NetworkLayer/HScaleNetDriver.h"
#include "GameFramework/GameModeBase.h"
#include "Core/HScaleResources.h"
#include "NetworkLayer/HScaleConnection.h"

#define NAME_HyperScaleNetDriver TEXT("HScaleNetDriver")

void UHScaleWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Not a game world
	if (!GetWorldRef().IsGameWorld() || !GetWorld()->GetGameInstance()) return;

	// Wait for game mode initialization, because only after that has world a valid URL
	// The URL contains level options that are responsible for starting HScaleNetDriver
	FGameModeEvents::OnGameModeInitializedEvent().AddUObject(this, &ThisClass::HandleGameModeInitialization);
}

void UHScaleWorldSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	}

	Super::Deinitialize();
}

bool UHScaleWorldSubsystem::IsHyperScaleNetworkingActive() const
{
	if (IsValid(HyperScaleDriver))
	{
		return HyperScaleDriver->IsNetworkSessionActive();
	}

	return false;
}

void UHScaleWorldSubsystem::HandleGameModeInitialization(AGameModeBase* GameMode)
{
	FGameModeEvents::OnGameModeInitializedEvent().RemoveAll(this);

	FURL& Url = GetWorldRef().URL;
	if (Url.HasOption(HYPERSCALE_LEVEL_OPTION))
	{
		if (!GetWorldRef().IsNetMode(NM_Standalone))
		{
			// Allow only standalone mode for hyperscale to ensure there will not be any other NetDriver except UHScaleNetDriver
			UE_LOG(Log_HyperScaleGlobals, Warning, TEXT("Hyperscale in other than standalone mode is not allowed."));
			return;
		}

		// Retrieve hyperscale server and address
		FString HyperscaleAddressAndPort = Url.GetOption(HYPERSCALE_LEVEL_OPTION, nullptr);
		HyperscaleAddressAndPort.ReplaceInline(TEXT("="), TEXT(""));
		HyperscaleAddressAndPort = HyperscaleAddressAndPort.TrimStart().TrimEnd(); // Remove white spaces
		UE_LOG(Log_HyperScaleGlobals, Log, TEXT("Used hyperscale address: %s"), *HyperscaleAddressAndPort);

		FString ServerAddress, Port;
		HyperscaleAddressAndPort.Split(":", &ServerAddress, &Port, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

		Url.Host = ServerAddress;
		Url.Port = FCString::Atoi(*Port);

		// Create UHScaleNetDriver
		GEngine->CreateNamedNetDriver(GetWorld(), NAME_HyperScaleNetDriver, NAME_HyperScaleNetDriver);
		UNetDriver* TempNetDriver = GEngine->FindNamedNetDriver(GetWorld(), NAME_HyperScaleNetDriver);
		HyperScaleDriver = Cast<UHScaleNetDriver>(TempNetDriver);

		if (IsValid(TempNetDriver))
		{
			// #todo ... do not know if we need this. Most of game stuff is related to NAME_GameNetDriver name, but for now we dont need to use that
			// TempNetDriver->SetNetDriverName(NAME_GameNetDriver);

			if (IsValid(HyperScaleDriver))
			{
				UE_LOG(Log_HyperScaleGlobals, Log, TEXT("Hyperscale NetDriver created successfully. Starting with driver initialization."));

				FString Error;
				constexpr bool bAsClient = false; // We try initialization as server, because each client is kind of server that will sends updates to hyperscale server
				HyperScaleDriver->InitBase(bAsClient, GetWorld(), Url, false, Error);
				HyperScaleDriver->SetWorld(GetWorld());

				GetWorldRef().SetNetDriver(HyperScaleDriver);

				FLevelCollection& SourceLevels = GetWorld()->FindOrAddCollectionByType(ELevelCollectionType::DynamicSourceLevels);
				SourceLevels.SetNetDriver(HyperScaleDriver);

				FLevelCollection& StaticLevels = GetWorld()->FindOrAddCollectionByType(ELevelCollectionType::StaticLevels);
				StaticLevels.SetNetDriver(HyperScaleDriver);

				FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &ThisClass::HandleActorSpawned);
			}
			else
			{
				UE_LOG(Log_HyperScaleGlobals, Error, TEXT("Target NetDriver is not derived from UHScaleNetDriver."));
			}
		}
		else
		{
			UE_LOG(Log_HyperScaleGlobals, Error, TEXT("Creating of HScaleNetDriver failed. Please check if you have defined HScale driver class in DefaultEngine.ini under section [/Script/Engine.Engine]"));
			UE_LOG(Log_HyperScaleGlobals, Warning, TEXT("Example: NetDriverDefinitions=(DefName=\"HScaleNetDriver\",DriverClassName=\"/Script/HyperScale.HScaleNetDriver\",DriverClassNameFallback=\"/Script/HyperScale.HScaleNetDriver\")"));
		}
	}
}

void UHScaleWorldSubsystem::HandleActorSpawned(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	check(GameMode);
	check(NewPlayer);

	// #todo .... @michal - I dont like this way how to assign net connection owner to player controller, but without engine modification I didnt found a better solution
	// When standalone game starts, it creates ULocalPlayer (with player controller) which is not derived from UNetConnection that is created separated
	// and because of that we need to listen for actor initialization

	// #todo ... each spawned player controller create custom HScaleNetConnection
	// NewPlayer->SetRole(ROLE_Authority);
	// NewPlayer->SetReplicates(false); // Sets not replicated by default

	// GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, NewPlayer]()
	// {
	// 	UHScaleConnection* NetConnection = HyperScaleDriver->GetHyperScaleConnection();
	// 	NewPlayer->Player = NetConnection;
	// }));

	// NetConnection->OwningActor = NewPlayer;
	// NewPlayer->SetPlayer(HyperScaleDriver->GetHyperScaleConnection());
}

#undef NAME_HyperScaleNetDriver