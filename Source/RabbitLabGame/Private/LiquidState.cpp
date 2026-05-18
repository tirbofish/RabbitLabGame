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
