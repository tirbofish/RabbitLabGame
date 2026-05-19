#include "LiquidState.h"

#include "GameFramework/CharacterMovementComponent.h"

void LiquidState::SwitchToState()
{
	IPlayerState::SwitchToState();
}

void LiquidState::EnterState()
{
	if (!Owner)
	{
		return;
	}
	
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("LiquidState::EnterState"));

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->MaxWalkSpeed = Owner->LiquidWalkSpeed;
	Movement->JumpZVelocity = Owner->LiquidJumpVelocity;
	Movement->GravityScale = 1.0f;
	Owner->bCanMeltObjects = true;

	IPlayerState::EnterState();
}

void LiquidState::ExitState()
{
	if (Owner)
	{
		Owner->bCanMeltObjects = false;
	}
	
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.0, FColor::Red, TEXT("LiquidState::ExitState"));

	IPlayerState::ExitState();
}

void LiquidState::UpdateState(float DeltaTime)
{
	IPlayerState::UpdateState(DeltaTime);
}

void LiquidState::ApplyVisuals()
{
	IPlayerState::ApplyVisuals();
}
