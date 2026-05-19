#include "GasState.h"

#include "TimerManager.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"

void GasState::SwitchToState()
{
	IPlayerState::SwitchToState();
}

void GasState::EnterState()
{
	if (!Owner)
	{
		return;
	}
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("GasState::EnterState"));

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Flying);
	Movement->MaxFlySpeed = Owner->GasFlySpeed;
	Movement->MaxWalkSpeed = Owner->GasFlySpeed;
	Movement->GravityScale = 0.0f;
	Owner->bCanMeltObjects = false;
	
	// start timer
	FTimerDelegate GasTimerDelegate;
	GasTimerDelegate.BindRaw(this, &GasState::GasTimer);

	Owner->GetWorldTimerManager().SetTimer(
		TimerHandle,
		GasTimerDelegate,
		5.0f,
		false
	);

	IPlayerState::EnterState();
}

void GasState::GasTimer()
{
	if (!Owner)
	{
		return;
	}
	
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("GasState::ExitState (kick from exceeding timer)"));

	Owner->SetMatterState(EPlayerMatterState::Solid);
}

void GasState::ExitState()
{
	if (!Owner)
	{
		return;
	}
	
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("GasState::ExitState"));

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	Movement->GravityScale = 1.0f;
	Movement->Velocity.Z = FMath::Min(Movement->Velocity.Z, Owner->SolidJumpVelocity);
	
	// cancel the gas timer
	Owner->GetWorldTimerManager().ClearTimer(TimerHandle);

	IPlayerState::ExitState();
}

void GasState::UpdateState(float DeltaTime)
{
	if (!Owner)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	const float NewVerticalSpeed = FMath::Clamp(
		Movement->Velocity.Z + Owner->GasUpwardAcceleration * DeltaTime,
		-Owner->GasMaxFallSpeed,
		Owner->GasMaxRiseSpeed
	);

	Movement->Velocity.Z = NewVerticalSpeed;

	IPlayerState::UpdateState(DeltaTime);
}

void GasState::ApplyVisuals()
{
	IPlayerState::ApplyVisuals();
}
