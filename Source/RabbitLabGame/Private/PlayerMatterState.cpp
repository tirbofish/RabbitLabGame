#include "PlayerMatterState.h"

#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GasState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LiquidState.h"
#include "MeltableActor.h"
#include "RabbitLabCheatManager.h"
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

	AMeltableActor* FindMeltableActor(AActor* Other, UPrimitiveComponent* OtherComp)
	{
		if (AMeltableActor* MeltableActor = Cast<AMeltableActor>(Other))
		{
			return MeltableActor;
		}

		if (OtherComp)
		{
			if (AMeltableActor* MeltableActor = Cast<AMeltableActor>(OtherComp->GetOwner()))
			{
				return MeltableActor;
			}
		}

		return nullptr;
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

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->CheatClass = URabbitLabCheatManager::StaticClass();
		if (!PlayerController->CheatManager || !PlayerController->CheatManager->IsA<URabbitLabCheatManager>())
		{
			PlayerController->CheatManager = NewObject<URabbitLabCheatManager>(PlayerController, PlayerController->CheatClass);
			PlayerController->CheatManager->InitCheatManager();
		}
	}

	EnterMatterState(CurrentMatterState);
}

void APlayerMatterState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IPlayerState* StateObject = GetStateObject(CurrentMatterState))
	{
		StateObject->UpdateState(DeltaTime);
	}

	DrawContinuousMeltableContactDebug();
	ApplyLiquidMelt(DeltaTime);
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

void APlayerMatterState::NotifyHit(
	UPrimitiveComponent* MyComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	bool bSelfMoved,
	FVector HitLocation,
	FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (AMeltableActor* MeltableActor = FindMeltableActor(Other, OtherComp))
	{
		const FVector ImpactPoint(Hit.ImpactPoint);
		const FVector ImpactNormal(Hit.ImpactNormal);
		const FVector DebugLocation = ImpactPoint.IsNearlyZero() ? HitLocation : ImpactPoint;
		const FVector DebugNormal = ImpactNormal.IsNearlyZero() ? HitNormal : ImpactNormal;
		ActiveDebugMeltableActor = MeltableActor;
		ActiveDebugMeltLocation = DebugLocation;
		ActiveDebugMeltNormal = DebugNormal;
		if (const UWorld* World = GetWorld())
		{
			LastDebugMeltContactTime = World->GetTimeSeconds();
		}
		if (URabbitLabCheatManager::IsMeltableDebugEnabled())
		{
			MeltableActor->DrawMeltCollisionDebug(DebugLocation, DebugNormal, GetLiquidMeltRadius());
		}
	}

	if (!bCanMeltObjects || !IsLiquid())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	LastMeltContactTime = World->GetTimeSeconds();
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

void APlayerMatterState::DrawContinuousMeltableContactDebug()
{
	if (!URabbitLabCheatManager::IsMeltableDebugEnabled())
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent && MovementComponent->CurrentFloor.bBlockingHit)
	{
		const FHitResult& FloorHit = MovementComponent->CurrentFloor.HitResult;
		if (AMeltableActor* FloorMeltableActor = FindMeltableActor(FloorHit.GetActor(), FloorHit.GetComponent()))
		{
			const FVector ImpactPoint(FloorHit.ImpactPoint);
			const FVector ImpactNormal(FloorHit.ImpactNormal);
			FloorMeltableActor->DrawMeltCollisionDebug(
				ImpactPoint.IsNearlyZero() ? GetActorLocation() : ImpactPoint,
				ImpactNormal.IsNearlyZero() ? FVector::UpVector : ImpactNormal,
				GetLiquidMeltRadius()
			);
			return;
		}
	}

	const UWorld* World = GetWorld();
	AMeltableActor* MeltableActor = ActiveDebugMeltableActor.Get();
	if (!World || !MeltableActor)
	{
		return;
	}

	if (World->GetTimeSeconds() - LastDebugMeltContactTime <= 0.2f)
	{
		MeltableActor->DrawMeltCollisionDebug(ActiveDebugMeltLocation, ActiveDebugMeltNormal, GetLiquidMeltRadius());
	}
}

void APlayerMatterState::ApplyLiquidMelt(float DeltaTime)
{
	if (!bCanMeltObjects || !IsLiquid() || DeltaTime <= 0.0f)
	{
		return;
	}

	AMeltableSurface* MeltableSurface = ActiveMeltableSurface.Get();
	if (!MeltableSurface)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - LastMeltContactTime > 0.3f)
	{
		ActiveMeltableSurface.Reset();
		ActiveMeltAccumulatedDepth = 0.0f;
		return;
	}
	
	return;
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
