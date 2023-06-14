// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Math/Vector.h"
#include "JGCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class JENGAGAME_API UJGCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

public:

	/** Custom air speed cap. */
	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
		float AirSpeedCap;

	/** Tether actor. */
	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
		TObjectPtr<AActor> TetherActor;

	/** Tether force. */
	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
		float TetherForce;

	/** Tether cutoff time. */
	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
		float TetherCutoff;

private:
	float TetherCutoffTimer;
};
