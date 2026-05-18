#include "PlayerMatterState.h"

#include "EnhancedInputComponent.h"
#include "GasState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LiquidState.h"
#include "SolidState.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
constexpr EPlayerMatterState MatterOrdinal[] =
{
	EPlayerMatterState::Liquid,
	EPlayerMatterState::Solid,
	EPlayerMatterState::Gas,
};

int32 FindMatterOrdinalIndex(EPlayerMatterState State)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(MatterOrdinal); ++Index)
	{
		if (MatterOrdinal[Index] == State)
		{
			return Index;
		}
	}

	return 0;
}
}

void IPlayerState::Initialize(APlayerMatterState* InOwner, EPlayerMatterState InState)
{
	Owner = InOwner;
	StateType = InState;
}

void IPlayerState::SwitchToState()
{
	if (Owner)
	{
		Owner->SetMatterState(StateType);
	}
}

void IPlayerState::EnterState()
{
	ApplyVisuals();
}

void IPlayerState::ExitState()
{
}

void IPlayerState::UpdateState(float DeltaTime)
{
}

void IPlayerState::ApplyVisuals()
{
}

APlayerMatterState::APlayerMatterState()
{
	PrimaryActorTick.bCanEverTick = true;

	SolidStateObject = MakeUnique<SolidState>();
	LiquidStateObject = MakeUnique<LiquidState>();
	GasStateObject = MakeUnique<GasState>();

	SolidStateObject->Initialize(this, EPlayerMatterState::Solid);
	LiquidStateObject->Initialize(this, EPlayerMatterState::Liquid);
	GasStateObject->Initialize(this, EPlayerMatterState::Gas);

	static ConstructorHelpers::FObjectFinder<UInputAction> MatterOrdinalUpAsset(
		TEXT("/Game/Input/Actions/IA_MatterOrdUp.IA_MatterOrdUp")
	);
	if (MatterOrdinalUpAsset.Succeeded())
	{
		MatterOrdinalUpAction = MatterOrdinalUpAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MatterOrdinalDownAsset(
		TEXT("/Game/Input/Actions/IA_MatterOrdDown.IA_MatterOrdDown")
	);
	if (MatterOrdinalDownAsset.Succeeded())
	{
		MatterOrdinalDownAction = MatterOrdinalDownAsset.Object;
	}
}

APlayerMatterState::~APlayerMatterState() = default;

void APlayerMatterState::BeginPlay()
{
	Super::BeginPlay();

	EnterMatterState(CurrentMatterState);
}

void APlayerMatterState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IPlayerState* StateObject = GetStateObject(CurrentMatterState))
	{
		StateObject->UpdateState(DeltaTime);
	}
}

void APlayerMatterState::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (MatterOrdinalUpAction)
	{
		EnhancedInput->BindAction(
			MatterOrdinalUpAction,
			ETriggerEvent::Started,
			this,
			&APlayerMatterState::MatterOrdinalUp
		);
	}

	if (MatterOrdinalDownAction)
	{
		EnhancedInput->BindAction(
			MatterOrdinalDownAction,
			ETriggerEvent::Started,
			this,
			&APlayerMatterState::MatterOrdinalDown
		);
	}
}

void APlayerMatterState::SetMatterState(EPlayerMatterState NewState)
{
	if (CurrentMatterState == NewState)
	{
		return;
	}

	ExitMatterState(CurrentMatterState);
	CurrentMatterState = NewState;
	EnterMatterState(CurrentMatterState);

	OnMatterStateChanged.Broadcast(CurrentMatterState);
}

void APlayerMatterState::SwitchToSolid()
{
	SetMatterState(EPlayerMatterState::Solid);
}

void APlayerMatterState::SwitchToLiquid()
{
	SetMatterState(EPlayerMatterState::Liquid);
}

void APlayerMatterState::SwitchToGas()
{
	SetMatterState(EPlayerMatterState::Gas);
}

void APlayerMatterState::MatterOrdinalUp()
{
	CycleMatterState(1);
}

void APlayerMatterState::MatterOrdinalDown()
{
	CycleMatterState(-1);
}

void APlayerMatterState::EnterMatterState(EPlayerMatterState State)
{
	if (IPlayerState* StateObject = GetStateObject(State))
	{
		StateObject->EnterState();
	}
}

void APlayerMatterState::ExitMatterState(EPlayerMatterState State)
{
	if (IPlayerState* StateObject = GetStateObject(State))
	{
		StateObject->ExitState();
	}
}

void APlayerMatterState::CycleMatterState(int32 Direction)
{
	const int32 CurrentIndex = FindMatterOrdinalIndex(CurrentMatterState);
	const int32 NextIndex = FMath::Clamp(
		CurrentIndex + Direction,
		0,
		static_cast<int32>(UE_ARRAY_COUNT(MatterOrdinal)) - 1
	);

	SetMatterState(MatterOrdinal[NextIndex]);
}

IPlayerState* APlayerMatterState::GetStateObject(EPlayerMatterState State) const
{
	switch (State)
	{
	case EPlayerMatterState::Solid:
		return SolidStateObject.Get();
	case EPlayerMatterState::Liquid:
		return LiquidStateObject.Get();
	case EPlayerMatterState::Gas:
		return GasStateObject.Get();
	default:
		return nullptr;
	}
}
