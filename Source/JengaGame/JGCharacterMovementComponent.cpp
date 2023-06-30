// Fill out your copyright notice in the Description page of Project Settings.


#include "JGCharacterMovementComponent.h"
#include <Runtime/Engine/Classes/Kismet/KismetMathLibrary.h>
#include <Runtime/Engine/Classes/GameFramework/Character.h>

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
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);
	//CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CharPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	FallAcceleration.Z = 0.f;
	const bool bHasLimitedAirControl = ShouldLimitAirControl(deltaTime, FallAcceleration);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		const FVector OldVelocityWithRootMotion = Velocity;

		RestorePreAdditiveRootMotionVelocity();

		const FVector OldVelocity = Velocity;

		// Tether forces

		if (TetherCutoffTimer < TetherCutoff)
		{
			FVector baseForce = FVector((this->TetherForce) / timeTick, 0, 0); UCharacterMovementComponent::PhysFalling(deltaTime, Iterations);
			FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(this->GetActorLocation(), this->TetherActor.Get()->GetActorLocation()) ;
			FVector Force = LookAtRot.RotateVector(baseForce);
			this->AddForce(Force);
			AActor* actor = TetherActor.Get();
			AActor* parent = actor->GetAttachParentActor();
			if (parent != nullptr) {
				parent->FindComponentByClass<UPrimitiveComponent>()->AddForce(-Force);
			}
			
		}

		//UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) OldVelocityWithRootMotion=(%s) NewVelocity=(%s)"), timeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *OldVelocityWithRootMotion.ToString(), *Velocity.ToString());
		ApplyRootMotionToVelocity(timeTick);
		DecayFormerBaseVelocity(timeTick);

		// Compute change in position (using midpoint integration method).
		FVector Adjusted = 0.5f * (OldVelocityWithRootMotion + Velocity) * timeTick;

		// Move
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

		if (Hit.GetActor() == TetherActor.Get()->GetAttachParentActor() && Hit.GetActor() != nullptr) {
			TetherCutoffTimer = 0;
			// todo: this shouldn't be hardcoded, unify behavior between cpp/bp
			GravityScale = 3.33;
			GroundFriction = 8;
			SetMovementMode(EMovementMode::MOVE_Falling);
		}

		if (!HasValidData())
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);


		if (Hit.bBlockingHit)
		{
			if (Hit.ImpactNormal.Z < 0) 
			{
				TetherCutoffTimer = TetherCutoff;
			}
			// Compute impact deflection based on final velocity, not integration step.
			// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
			Adjusted = Velocity * timeTick;

			// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
			if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
			{
				const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
				FFindFloorResult FloorResult;
				FindFloor(PawnLocation, FloorResult, false);
				if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
				{
					remainingTime += subTimeTickRemaining;
					ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
					return;
				}
			}

			HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

			// If we've changed physics mode, abort.
			if (!HasValidData())
			{
				return;
			}

			const FVector OldHitNormal = Hit.Normal;
			const FVector OldHitImpactNormal = Hit.ImpactNormal;
			FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

			// Compute velocity after deflection (only gravity component for RootMotion)
			const UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!Velocity.IsNearlyZero() && MovementBaseUtility::IsSimulatedBase(HitComponent))
			{
				const FVector ContactVelocity = MovementBaseUtility::GetMovementBaseVelocity(HitComponent, NAME_None) + MovementBaseUtility::GetMovementBaseTangentialVelocity(HitComponent, NAME_None, Hit.ImpactPoint);
				const FVector NewVelocity = Velocity - Hit.ImpactNormal * FVector::DotProduct(Velocity - ContactVelocity, Hit.ImpactNormal);
				Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
			}
			else if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
			{
				const FVector NewVelocity = (Delta / subTimeTickRemaining);
				Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
			}

			if (subTimeTickRemaining > KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
			{
				// Move in deflected direction.
				SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

				if (Hit.bBlockingHit)
				{
					// hit second wall
					LastMoveTimeSlice = subTimeTickRemaining;
					subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

					if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(Hit, remainingTime, Iterations);
						return;
					}

					HandleImpact(Hit, LastMoveTimeSlice, Delta);

					// If we've changed physics mode, abort.
					if (!HasValidData() || !IsFalling())
					{
						return;
					}

					FVector PreTwoWallDelta = Delta;
					TwoWallAdjust(Delta, Hit, OldHitNormal);

					// Compute velocity after deflection (only gravity component for RootMotion)
					if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
					{
						const FVector NewVelocity = (Delta / subTimeTickRemaining);
						Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
					}

					// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
					bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
					if (Hit.Time == 0.f)
					{
						// if we are stuck then try to side step
						FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
						if (SideDelta.IsNearlyZero())
						{
							SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
						}
						SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
					}
				}
			}
		}

		if (this->GetActorLocation().Z > TetherActor->GetActorLocation().Z) 
		{
			Velocity.Z = 0.f;
		}
		if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{			
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
			TetherCutoffTimer += timeTick;
		}
		else 
		{	
			TetherCutoffTimer = 0;
		}

		if (TetherCutoffTimer >= TetherCutoff)
		{
			TetherCutoffTimer = 0;
			// todo: this shouldn't be hardcoded, unify behavior between cpp/bp
			GravityScale = 3.33;
			GroundFriction = 8;
			SetMovementMode(EMovementMode::MOVE_Falling);
		}
	}
}