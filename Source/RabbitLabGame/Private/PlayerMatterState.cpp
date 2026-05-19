#include "PlayerMatterState.h"

#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GasState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LiquidState.h"
#include "MeltableSurface.h"
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

AMeltableSurface* FindMeltableSurface(AActor* Other, UPrimitiveComponent* OtherComp)
{
	if (AMeltableSurface* MeltableSurface = Cast<AMeltableSurface>(Other))
	{
		return MeltableSurface;
	}

	if (OtherComp)
	{
		if (AMeltableSurface* MeltableSurface = Cast<AMeltableSurface>(OtherComp->GetOwner()))
		{
			return MeltableSurface;
		}
	}

	if (Other)
	{
		TArray<UChildActorComponent*> ChildActorComponents;
		Other->GetComponents<UChildActorComponent>(ChildActorComponents);

		for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
		{
			if (!ChildActorComponent)
			{
				continue;
			}

			if (AMeltableSurface* MeltableSurface = Cast<AMeltableSurface>(ChildActorComponent->GetChildActor()))
			{
				return MeltableSurface;
			}
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

	EnterMatterState(CurrentMatterState);
}

void APlayerMatterState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IPlayerState* StateObject = GetStateObject(CurrentMatterState))
	{
		StateObject->UpdateState(DeltaTime);
	}

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

	if (!bCanMeltObjects || !IsLiquid())
	{
		return;
	}

	AMeltableSurface* MeltableSurface = FindMeltableSurface(Other, OtherComp);
	if (!MeltableSurface)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Only update the target surface reference - actual melting happens via trace in ApplyLiquidMelt
	ActiveMeltableSurface = MeltableSurface;
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

	// Use a sphere trace downward from the player's center to find the exact melt point
	// This avoids the instability of using NotifyHit locations during physics resolution
	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, LiquidMeltTraceDistance);

	FHitResult TraceHit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	const bool bHit = World->SweepSingleByChannel(
		TraceHit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(LiquidMeltRadius * 0.25f),
		QueryParams
	);

	if (!bHit || TraceHit.GetActor() != MeltableSurface)
	{
		return;
	}

	const FVector HitLocation = TraceHit.ImpactPoint;
	const bool bSameSpot = FVector::DistSquared(ActiveMeltLocation, HitLocation) <= FMath::Square(LiquidMeltRadius * 0.5f);

	if (!bSameSpot)
	{
		ActiveMeltAccumulatedDepth = 0.0f;
	}

	ActiveMeltLocation = HitLocation;
	ActiveMeltNormal = TraceHit.ImpactNormal;
	ActiveMeltAccumulatedDepth += LiquidMeltRate * DeltaTime;

	// Apply a spherical crater - radius grows slightly with accumulated depth for sinking effect
	const float EffectiveRadius = LiquidMeltRadius + ActiveMeltAccumulatedDepth * 0.1f;
	MeltableSurface->ApplySphericalCrater(
		ActiveMeltLocation,
		FMath::Min(EffectiveRadius, LiquidMeltRadius * 2.0f)
	);
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
