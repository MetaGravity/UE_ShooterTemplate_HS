// Copyright 2024 Metagravity. All Rights Reserved.


#include "Actors/TestHScaleCharacter.h"

#include "Net/UnrealNetwork.h"


// Sets default values
ATestHScaleCharacter::ATestHScaleCharacter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

// Called when the game starts or when spawned
void ATestHScaleCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATestHScaleCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void ATestHScaleCharacter::OnRep_Value1()
{
	UE_LOG(LogTemp, Display, TEXT("Received on rep value 1 %f"), ReplicatedValue1)
}

void ATestHScaleCharacter::OnRep_Value2()
{
	UE_LOG(LogTemp, Display, TEXT("Received on rep value 2 %s"), *ReplicatedValue2)
}

void ATestHScaleCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ATestHScaleCharacter, ReplicatedValue1);
	DOREPLIFETIME(ATestHScaleCharacter, ReplicatedValue2);
}
