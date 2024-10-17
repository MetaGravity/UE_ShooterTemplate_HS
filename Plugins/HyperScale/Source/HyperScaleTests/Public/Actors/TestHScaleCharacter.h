// Copyright 2024 Metagravity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "TestHScaleCharacter.generated.h"

UCLASS()
class HYPERSCALETESTS_API ATestHScaleCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestHScaleCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;


	UPROPERTY(ReplicatedUsing=OnRep_Value1)
	float ReplicatedValue1 = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Value2)
	FString ReplicatedValue2 = "def";

	UFUNCTION()
	void OnRep_Value1();

	UFUNCTION()
	void OnRep_Value2();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool IsNameStableForNetworking() const override { return true; };
};
