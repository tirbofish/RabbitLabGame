#include "PlayerMatterState.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GasState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LiquidState.h"
#include "MeltableActor.h"
#include "PushableActor.h"
#include "RabbitLabCheatManager.h"
#include "SolidState.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogRabbitVitals, Log, All);

namespace
{
	const FName MatterSwitchBlueprintEventName(TEXT("On Matter Switch"));
	const FName BlocksGasConversionTag(TEXT("BlocksGasConversion"));
	const FName BlocksLiquidConversionTag(TEXT("BlocksLiquidConversion"));

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
		Owner->RequestMatterStateChange(StateType);
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
	if (Owner)
	{
		Owner->TriggerMatterSwitchVisuals();
	}
}

APlayerMatterState::APlayerMatterState()
{
	PrimaryActorTick.bCanEverTick = true;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetGenerateOverlapEvents(true);
	}

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

	static ConstructorHelpers::FObjectFinder<UInputAction> HealAsset(
		TEXT("/Game/Input/Actions/IA_Heal.IA_Heal")
	);
	if (HealAsset.Succeeded())
	{
		HealAction = HealAsset.Object;
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

	ClampVitals();
	BroadcastVitalsChanged();
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
	ApplyGasEnergyDrain(DeltaTime);
	ApplyLiquidEnergyDepletionRule();
	ApplyHealKeyFallback();
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

	if (HealAction)
	{
		EnhancedInput->BindAction(
			HealAction,
			ETriggerEvent::Triggered,
			this,
			&APlayerMatterState::RestoreEnergyFromHealth
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

	ApplySolidPushNudge(Other, OtherComp, Hit);

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

	// Keep Blueprint callers from bypassing the same zone rule enforced by
	// RequestMatterStateChange.
	if (IsMatterStateBlocked(NewState))
	{
		return;
	}

	ExitMatterState(CurrentMatterState);
	CurrentMatterState = NewState;
	EnterMatterState(CurrentMatterState);

	OnMatterStateChanged.Broadcast(CurrentMatterState);
}

bool APlayerMatterState::RequestMatterStateChange(EPlayerMatterState NewState)
{
	if (CurrentMatterState == NewState)
	{
		return true;
	}

	if (IsMatterStateBlocked(NewState))
	{
		return false;
	}

	if (EnergyPoints <= 0.0f)
	{
		return false;
	}

	ConsumeEnergy(MatterStateChangeEnergyCost);
	SetMatterState(NewState);
	return true;
}

void APlayerMatterState::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	UpdateMatterStateBlockerOverlap(OtherActor, 1);
}

void APlayerMatterState::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	UpdateMatterStateBlockerOverlap(OtherActor, -1);
}

void APlayerMatterState::UpdateMatterStateBlockerOverlap(AActor* OtherActor, int32 CountDelta)
{
	if (!IsValid(OtherActor))
	{
		return;
	}

	if (OtherActor->ActorHasTag(BlocksGasConversionTag))
	{
		GasBlockingZoneOverlapCount = FMath::Max(0, GasBlockingZoneOverlapCount + CountDelta);
	}

	if (OtherActor->ActorHasTag(BlocksLiquidConversionTag))
	{
		LiquidBlockingZoneOverlapCount = FMath::Max(0, LiquidBlockingZoneOverlapCount + CountDelta);
	}
}

bool APlayerMatterState::IsMatterStateBlocked(EPlayerMatterState State) const
{
	switch (State)
	{
	case EPlayerMatterState::Gas:
		return GasBlockingZoneOverlapCount > 0;
	case EPlayerMatterState::Liquid:
		return LiquidBlockingZoneOverlapCount > 0;
	default:
		return false;
	}
}

void APlayerMatterState::SwitchToSolid()
{
	RequestMatterStateChange(EPlayerMatterState::Solid);
}

void APlayerMatterState::SwitchToLiquid()
{
	RequestMatterStateChange(EPlayerMatterState::Liquid);
}

void APlayerMatterState::SwitchToGas()
{
	RequestMatterStateChange(EPlayerMatterState::Gas);
}

void APlayerMatterState::MatterOrdinalUp()
{
	CycleMatterState(1);
}

void APlayerMatterState::MatterOrdinalDown()
{
	CycleMatterState(-1);
}

void APlayerMatterState::ApplyMatterSwitchVisuals()
{
	SetVisibilityOfMesh(GetMesh(), IsSolid());
	SetVisibilityOfMesh(GetGasMatterMesh(), IsGas());
	SetVisibilityOfMesh(GetLiquidMatterMesh(), IsLiquid());
}

void APlayerMatterState::SetVisibilityOfMesh(USkeletalMeshComponent* TargetMesh, bool bVisibility)
{
	if (!TargetMesh)
	{
		return;
	}

	TargetMesh->SetVisibility(bVisibility, true);
	TargetMesh->SetHiddenInGame(!bVisibility, true);
}

void APlayerMatterState::TriggerMatterSwitchVisuals()
{
	ApplyMatterSwitchVisuals();
	CallMatterSwitchBlueprintEvent();
}

void APlayerMatterState::CallMatterSwitchBlueprintEvent()
{
	if (UFunction* MatterSwitchEvent = FindFunction(MatterSwitchBlueprintEventName))
	{
		ProcessEvent(MatterSwitchEvent, nullptr);
	}
}

void APlayerMatterState::EnterMatterState(EPlayerMatterState State)
{
	if (IPlayerState* StateObject = GetStateObject(State))
	{
		StateObject->EnterState();
	}

	ConfigurePhysicsInteractionForCurrentState();
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

	RequestMatterStateChange(MatterOrdinal[NextIndex]);
}

void APlayerMatterState::RestoreEnergyFromHealth()
{
	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : -1.0f;
	if (CurrentTime >= 0.0f && FMath::IsNearlyEqual(CurrentTime, LastEnergyRestoreAppliedTime))
	{
		return;
	}

	if (EnergyRestoreRate <= 0.0f || EnergyRestoredPerHealthSpent <= 0.0f)
	{
		UE_LOG(
			LogRabbitVitals,
			Warning,
			TEXT("Energy restore skipped: invalid tuning values. EnergyRestoreRate=%.2f EnergyRestoredPerHealthSpent=%.2f"),
			EnergyRestoreRate,
			EnergyRestoredPerHealthSpent
		);
		return;
	}

	if (EnergyPoints >= MaxEnergyPoints)
	{
		UE_LOG(LogRabbitVitals, Log, TEXT("Energy restore skipped: EP already full at %.2f/%.2f."), EnergyPoints, MaxEnergyPoints);
		return;
	}

	const float HealthReserve = MaxHealthPoints * (1.0f - FMath::Clamp(MaxHealthFractionForEnergyRestore, 0.0f, 1.0f));
	const float SpendableHealth = HealthPoints - HealthReserve;
	if (SpendableHealth <= 0.0f)
	{
		UE_LOG(LogRabbitVitals, Log, TEXT("Energy restore skipped: no HP available above reserve %.2f. HP %.2f, EP %.2f/%.2f."), HealthReserve, HealthPoints, EnergyPoints, MaxEnergyPoints);
		return;
	}

	const float DeltaTime = World ? World->GetDeltaSeconds() : 0.0f;
	if (DeltaTime <= 0.0f)
	{
		UE_LOG(LogRabbitVitals, Verbose, TEXT("Energy restore skipped: invalid DeltaTime %.4f."), DeltaTime);
		return;
	}

	const float RequestedEnergy = EnergyRestoreRate * DeltaTime;
	const float MissingEnergy = FMath::Max(0.0f, MaxEnergyPoints - EnergyPoints);
	const float AffordableEnergy = SpendableHealth * EnergyRestoredPerHealthSpent;
	const float EnergyToRestore = FMath::Min(RequestedEnergy, FMath::Min(MissingEnergy, AffordableEnergy));

	if (EnergyToRestore <= 0.0f)
	{
		UE_LOG(
			LogRabbitVitals,
			Log,
			TEXT("Energy restore skipped: calculated restore amount was zero. HP %.2f/%.2f, EP %.2f/%.2f."),
			HealthPoints,
			MaxHealthPoints,
			EnergyPoints,
			MaxEnergyPoints
		);
		return;
	}

	const float HealthToSpend = EnergyToRestore / EnergyRestoredPerHealthSpent;
	const float PreviousHealth = HealthPoints;
	const float PreviousEnergy = EnergyPoints;
	HealthPoints = FMath::Clamp(HealthPoints - HealthToSpend, HealthReserve, MaxHealthPoints);
	EnergyPoints = FMath::Clamp(EnergyPoints + EnergyToRestore, 0.0f, MaxEnergyPoints);
	LastEnergyRestoreAppliedTime = CurrentTime;
	UE_LOG(
		LogRabbitVitals,
		Log,
		TEXT("Energy restored: HP %.2f -> %.2f / %.2f, EP %.2f -> %.2f / %.2f."),
		PreviousHealth,
		HealthPoints,
		MaxHealthPoints,
		PreviousEnergy,
		EnergyPoints,
		MaxEnergyPoints
	);
	BroadcastVitalsChanged();
}

bool APlayerMatterState::ConsumeEnergy(float Amount)
{
	if (Amount <= 0.0f)
	{
		return true;
	}

	const float PreviousEnergy = EnergyPoints;
	EnergyPoints = FMath::Clamp(EnergyPoints - Amount, 0.0f, MaxEnergyPoints);

	if (!FMath::IsNearlyEqual(PreviousEnergy, EnergyPoints))
	{
		BroadcastVitalsChanged();
	}

	return EnergyPoints > 0.0f;
}

void APlayerMatterState::RestoreEnergy(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	const float PreviousEnergy = EnergyPoints;
	EnergyPoints = FMath::Clamp(EnergyPoints + Amount, 0.0f, MaxEnergyPoints);

	if (!FMath::IsNearlyEqual(PreviousEnergy, EnergyPoints))
	{
		BroadcastVitalsChanged();
	}
}

void APlayerMatterState::RestoreHealth(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	const float PreviousHealth = HealthPoints;
	HealthPoints = FMath::Clamp(HealthPoints + Amount, 0.0f, MaxHealthPoints);

	if (!FMath::IsNearlyEqual(PreviousHealth, HealthPoints))
	{
		BroadcastVitalsChanged();
	}
}

void APlayerMatterState::AddHealth(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	HealthPoints = FMath::Clamp(HealthPoints + Amount, 0.0f, MaxHealthPoints);
}

void APlayerMatterState::ClampVitals()
{
	MaxHealthPoints = FMath::Max(1.0f, MaxHealthPoints);
	MaxEnergyPoints = FMath::Max(1.0f, MaxEnergyPoints);
	HealthPoints = FMath::Clamp(HealthPoints, 0.0f, MaxHealthPoints);
	EnergyPoints = FMath::Clamp(EnergyPoints, 0.0f, MaxEnergyPoints);
}

void APlayerMatterState::BroadcastVitalsChanged()
{
	UE_LOG(
		LogRabbitVitals,
		Log,
		TEXT("Vitals changed: HP %.2f/%.2f (%.0f%%), EP %.2f/%.2f (%.0f%%)."),
		HealthPoints,
		MaxHealthPoints,
		GetHealthPercent() * 100.0f,
		EnergyPoints,
		MaxEnergyPoints,
		GetEnergyPercent() * 100.0f
	);
	OnVitalsChanged.Broadcast(GetHealthPercent(), GetEnergyPercent());
}

void APlayerMatterState::ApplyHealKeyFallback()
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const bool bIsHealKeyDown = PlayerController->IsInputKeyDown(EKeys::H);
	const bool bWasHealKeyDown = bWasHealFallbackKeyDown;
	if (bIsHealKeyDown && !bWasHealKeyDown)
	{
		UE_LOG(LogRabbitVitals, Log, TEXT("Heal key H pressed."));
	}
	else if (!bIsHealKeyDown && bWasHealKeyDown)
	{
		UE_LOG(LogRabbitVitals, Log, TEXT("Heal key H released."));
	}

	bWasHealFallbackKeyDown = bIsHealKeyDown;
	if (!bIsHealKeyDown)
	{
		return;
	}

	const bool bCanApplyHeal = EnergyRestoreRate > 0.0f
		&& EnergyRestoredPerHealthSpent > 0.0f
		&& EnergyPoints < MaxEnergyPoints
		&& HealthPoints > 0.0f;

	if (bCanApplyHeal || !bWasHealKeyDown)
	{
		RestoreEnergyFromHealth();
	}
}

bool APlayerMatterState::CanMeltContact(AMeltableActor* MeltableActor) const
{
	if (!MeltableActor)
	{
		return false;
	}

	if (EnergyPoints > 0.0f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World || LiquidZeroEnergyMeltContinuationSeconds <= 0.0f)
	{
		return false;
	}

	return ActiveLiquidMeltableActor.Get() == MeltableActor
		&& LastMeltContactTime >= 0.0f
		&& World->GetTimeSeconds() - LastMeltContactTime <= LiquidZeroEnergyMeltContinuationSeconds;
}

bool APlayerMatterState::IsCurrentlyMelting() const
{
	if (!bCanMeltObjects || !IsLiquid())
	{
		return false;
	}

	AMeltableActor* MeltableActor = nullptr;
	FVector ContactLocation = FVector::ZeroVector;
	FVector ContactNormal = FVector::UpVector;
	return GetCurrentMeltableContact(MeltableActor, ContactLocation, ContactNormal) && CanMeltContact(MeltableActor);
}

bool APlayerMatterState::GetCurrentMeltableContact(
	AMeltableActor*& OutMeltableActor,
	FVector& OutLocation,
	FVector& OutNormal
) const
{
	OutMeltableActor = nullptr;
	OutLocation = FVector::ZeroVector;
	OutNormal = FVector::UpVector;

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent && MovementComponent->CurrentFloor.bBlockingHit)
	{
		const FHitResult& FloorHit = MovementComponent->CurrentFloor.HitResult;
		if (AMeltableActor* FloorMeltableActor = FindMeltableActor(FloorHit.GetActor(), FloorHit.GetComponent()))
		{
			const FVector ImpactPoint(FloorHit.ImpactPoint);
			const FVector ImpactNormal(FloorHit.ImpactNormal);
			OutMeltableActor = FloorMeltableActor;
			OutLocation = ImpactPoint.IsNearlyZero() ? GetActorLocation() : ImpactPoint;
			OutNormal = ImpactNormal.IsNearlyZero() ? FVector::UpVector : ImpactNormal;
			return true;
		}
	}

	const UWorld* World = GetWorld();
	AMeltableActor* MeltableActor = ActiveDebugMeltableActor.Get();
	if (!World || !MeltableActor)
	{
		return false;
	}

	if (World->GetTimeSeconds() - LastDebugMeltContactTime <= 0.2f)
	{
		OutMeltableActor = MeltableActor;
		OutLocation = ActiveDebugMeltLocation;
		OutNormal = ActiveDebugMeltNormal.IsNearlyZero() ? FVector::UpVector : ActiveDebugMeltNormal;
		return true;
	}

	return false;
}

void APlayerMatterState::DrawContinuousMeltableContactDebug()
{
	if (!URabbitLabCheatManager::IsMeltableDebugEnabled())
	{
		return;
	}

	AMeltableActor* MeltableActor = nullptr;
	FVector ContactLocation = FVector::ZeroVector;
	FVector ContactNormal = FVector::UpVector;
	if (GetCurrentMeltableContact(MeltableActor, ContactLocation, ContactNormal) && MeltableActor)
	{
		MeltableActor->DrawMeltCollisionDebug(ContactLocation, ContactNormal, GetLiquidMeltRadius());
	}
}

void APlayerMatterState::ApplyLiquidMelt(float DeltaTime)
{
	if (!bCanMeltObjects || !IsLiquid() || DeltaTime <= 0.0f)
	{
		return;
	}

	AMeltableActor* MeltableActor = nullptr;
	FVector ContactLocation = FVector::ZeroVector;
	FVector ContactNormal = FVector::UpVector;
	if (!GetCurrentMeltableContact(MeltableActor, ContactLocation, ContactNormal) || !MeltableActor)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!CanMeltContact(MeltableActor))
	{
		return;
	}

	LastMeltContactTime = World->GetTimeSeconds();
	ActiveLiquidMeltableActor = MeltableActor;
	const float MeltDepth = GetLiquidMeltRate() * DeltaTime;
	if (EnergyPoints > 0.0f)
	{
		ConsumeEnergy(MeltDepth * LiquidEnergyCostPerMeltDepth);
	}
	MeltableActor->ApplyMeltCrater(
		ContactLocation,
		ContactNormal,
		GetLiquidMeltRadius(),
		MeltDepth
	);
}

void APlayerMatterState::ApplyGasEnergyDrain(float DeltaTime)
{
	if (!IsGas() || DeltaTime <= 0.0f || GasEnergyDrainRate <= 0.0f)
	{
		return;
	}

	ConsumeEnergy(GasEnergyDrainRate * DeltaTime);
	if (EnergyPoints <= 0.0f)
	{
		SetMatterState(EPlayerMatterState::Solid);
	}
}

void APlayerMatterState::ApplyLiquidEnergyDepletionRule()
{
	if (!IsLiquid() || EnergyPoints > 0.0f || IsCurrentlyMelting())
	{
		return;
	}

	SetMatterState(EPlayerMatterState::Solid);
}

void APlayerMatterState::ConfigurePhysicsInteractionForCurrentState()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	const bool bCanPushInCurrentState = IsSolid() && bSolidCanPushPhysicsObjects;
	MovementComponent->bEnablePhysicsInteraction = bCanPushInCurrentState;
	MovementComponent->InitialPushForceFactor = bCanPushInCurrentState ? SolidInitialPushForceFactor : 0.0f;
	MovementComponent->PushForceFactor = bCanPushInCurrentState ? SolidPushForceFactor : 0.0f;
	MovementComponent->TouchForceFactor = bCanPushInCurrentState ? SolidPushTouchForceFactor : 0.0f;
	MovementComponent->MinTouchForce = 0.0f;
	MovementComponent->MaxTouchForce = bCanPushInCurrentState ? SolidPushForceFactor : 0.0f;
	MovementComponent->bPushForceScaledToMass = false;
	MovementComponent->bTouchForceScaledToMass = false;
	MovementComponent->bPushForceUsingZOffset = false;
	MovementComponent->bScalePushForceToVelocity = true;
}

void APlayerMatterState::ApplySolidPushNudge(AActor* Other, UPrimitiveComponent* OtherComp, const FHitResult& Hit)
{
	if (!bSolidCanPushPhysicsObjects || !IsSolid() || SolidPushNudgeVelocity <= 0.0f)
	{
		return;
	}

	const APushableActor* PushableActor = Cast<APushableActor>(Other);
	if (!PushableActor && OtherComp)
	{
		PushableActor = Cast<APushableActor>(OtherComp->GetOwner());
	}

	if (!PushableActor)
	{
		return;
	}

	UPrimitiveComponent* PushableComponent = OtherComp;
	if (!PushableComponent || !PushableComponent->IsSimulatingPhysics(Hit.BoneName))
	{
		PushableComponent = PushableActor->GetPushableMesh();
	}

	if (!PushableComponent || !PushableComponent->IsSimulatingPhysics(Hit.BoneName))
	{
		return;
	}

	FVector PushDirection = GetVelocity();
	PushDirection.Z = 0.0f;
	if (PushDirection.IsNearlyZero())
	{
		PushDirection = -Hit.ImpactNormal;
		PushDirection.Z = 0.0f;
	}

	if (PushDirection.Normalize())
	{
		PushableComponent->AddImpulse(PushDirection * SolidPushNudgeVelocity, Hit.BoneName, true);
	}
}

USkeletalMeshComponent* APlayerMatterState::FindMatterMeshByName(FName ComponentName) const
{
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	GetComponents<USkeletalMeshComponent>(SkeletalMeshComponents);

	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent && SkeletalMeshComponent->GetFName() == ComponentName)
		{
			return SkeletalMeshComponent;
		}
	}

	return nullptr;
}

USkeletalMeshComponent* APlayerMatterState::GetGasMatterMesh() const
{
	return FindMatterMeshByName(TEXT("SkeletalMesh-Gas"));
}

USkeletalMeshComponent* APlayerMatterState::GetLiquidMatterMesh() const
{
	return FindMatterMeshByName(TEXT("SkeletalMesh-Liquid"));
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
