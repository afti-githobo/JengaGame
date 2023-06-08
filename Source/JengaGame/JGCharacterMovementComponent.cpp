// Fill out your copyright notice in the Description page of Project Settings.


#include "JGCharacterMovementComponent.h"
#include <Runtime/Engine/Classes/Kismet/KismetMathLibrary.h>

void UJGCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	UCharacterMovementComponent::PhysFalling(deltaTime, Iterations);
	FVector lateralVelocity;
	lateralVelocity = this->Velocity;
	lateralVelocity.Z = 0;
	lateralVelocity = lateralVelocity.GetClampedToMaxSize(this->AirSpeedCap);
	this->Velocity.X = lateralVelocity.X;
	this->Velocity.Y = lateralVelocity.Y;
}

void UJGCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	//todo: should use subtypes but only one atm
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations)) {
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		if (!this->TetherActor.IsNull()) 
		{
			FVector baseForce = FVector((this->TetherForce) / timeTick, 0, 0); UCharacterMovementComponent::PhysFalling(deltaTime, Iterations);
			FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(this->GetActorLocation(), this->TetherActor.Get()->GetActorLocation());
			FVector Force = LookAtRot.RotateVector(baseForce);
			this->AddForce(Force);
			TetherActor.Get()->FindComponentByClass<UPrimitiveComponent>()->AddForce(-Force);
		}
	}
	// fall thru to falling behavior
	UCharacterMovementComponent::PhysFalling(deltaTime, Iterations);
}