#include "SolidState.h"

#include "GameFramework/CharacterMovementComponent.h"

void SolidState::SwitchToState()
{
	IPlayerState::SwitchToState();
}

void SolidState::EnterState()
{
	if (!Owner)
	{
		return;
	}
	
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("SolidState::EnterState"));

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Walking);
	Movement->MaxWalkSpeed = Owner->SolidWalkSpeed;
	Movement->JumpZVelocity = Owner->SolidJumpVelocity;
	Movement->GravityScale = 1.0f;
	Owner->bCanMeltObjects = false;

	IPlayerState::EnterState();
}

void SolidState::ExitState()
{
	if (GEngine) 
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("SolidState::EnterState"));
	
	IPlayerState::ExitState();
}

void SolidState::UpdateState(float DeltaTime)
{
	IPlayerState::UpdateState(DeltaTime);
}

void SolidState::ApplyVisuals()
{
	IPlayerState::ApplyVisuals();
}
