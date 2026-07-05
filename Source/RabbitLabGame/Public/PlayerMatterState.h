#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "GameFramework/Character.h"
#include "Templates/UniquePtr.h"
#include "PlayerMatterState.generated.h"

UENUM(BlueprintType)
enum class EPlayerMatterState : uint8
{
	Liquid UMETA(DisplayName = "Liquid"),
	Solid UMETA(DisplayName = "Solid"),
	Gas UMETA(DisplayName = "Gas"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMatterStateChanged,
	EPlayerMatterState,
	NewState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVitalsChanged,
	float,
	HealthPercent,
	float,
	EnergyPercent
);

class APlayerMatterState;
class AMeltableActor;
class AMeltableSurface;
class USkeletalMeshComponent;
class SolidState;
class LiquidState;
class GasState;

class IPlayerState
{
public:
	IPlayerState() = default;
	virtual ~IPlayerState() = default;

	void Initialize(APlayerMatterState* InOwner, EPlayerMatterState InState);
	EPlayerMatterState GetStateType() const { return StateType; }

	virtual void SwitchToState();
	virtual void EnterState();
	virtual void ExitState();
	virtual void UpdateState(float DeltaTime);
	virtual void ApplyVisuals();

protected:
	APlayerMatterState* Owner = nullptr;
	EPlayerMatterState StateType = EPlayerMatterState::Solid;
};

UCLASS(BlueprintType)
class RABBITLABGAME_API APlayerMatterState : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerMatterState();
	virtual ~APlayerMatterState() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(EditAnywhere, Category="Matter State|Liquid")
	float LiquidMeltTraceDistance = 80.0f;

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void SetMatterState(EPlayerMatterState NewState);

	UFUNCTION(BlueprintCallable, Category="Matter State")
	bool RequestMatterStateChange(EPlayerMatterState NewState);

	UFUNCTION(BlueprintPure, Category="Matter State")
	EPlayerMatterState GetMatterState() const { return CurrentMatterState; }

	UFUNCTION(BlueprintPure, Category="Matter State")
	bool IsSolid() const { return CurrentMatterState == EPlayerMatterState::Solid; }

	UFUNCTION(BlueprintPure, Category="Matter State")
	bool IsLiquid() const { return CurrentMatterState == EPlayerMatterState::Liquid; }

	UFUNCTION(BlueprintPure, Category="Matter State")
	bool IsGas() const { return CurrentMatterState == EPlayerMatterState::Gas; }

	UFUNCTION(BlueprintPure, Category="Matter State|Liquid")
	float GetLiquidMeltRadius() const { return LiquidMeltRadius; }

	UFUNCTION(BlueprintPure, Category="Matter State|Liquid")
	float GetLiquidMeltRate() const { return LiquidMeltRate; }

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void SwitchToSolid();

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void SwitchToLiquid();

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void SwitchToGas();

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void MatterOrdinalUp();

	UFUNCTION(BlueprintCallable, Category="Matter State")
	void MatterOrdinalDown();

	UFUNCTION(BlueprintCallable, Category="Matter State|Visuals")
	void ApplyMatterSwitchVisuals();

	UFUNCTION(BlueprintCallable, Category="Matter State|Visuals", meta=(DisplayName="Set Visibility of Mesh"))
	void SetVisibilityOfMesh(USkeletalMeshComponent* TargetMesh, bool bVisibility);

	UFUNCTION(BlueprintPure, Category="Vitals")
	float GetHealthPercent() const { return MaxHealthPoints > 0.0f ? HealthPoints / MaxHealthPoints : 0.0f; }

	UFUNCTION(BlueprintPure, Category="Vitals")
	float GetEnergyPercent() const { return MaxEnergyPoints > 0.0f ? EnergyPoints / MaxEnergyPoints : 0.0f; }

	UFUNCTION(BlueprintPure, Category="Vitals")
	float GetHealthPoints() const { return HealthPoints; }

	UFUNCTION(BlueprintPure, Category="Vitals")
	float GetEnergyPoints() const { return EnergyPoints; }

	UFUNCTION(BlueprintCallable, Category="Vitals")
	void RestoreEnergy(float Amount);

	UFUNCTION(BlueprintCallable, Category="Vitals")
	void RestoreHealth(float Amount);

	UPROPERTY(BlueprintAssignable, Category="Matter State")
	FOnMatterStateChanged OnMatterStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Vitals")
	FOnVitalsChanged OnVitalsChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Matter State")
	EPlayerMatterState CurrentMatterState = EPlayerMatterState::Solid;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid")
	float SolidWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid")
	float SolidJumpVelocity = 700.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid|Pushing")
	bool bSolidCanPushPhysicsObjects = true;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid|Pushing", meta=(ClampMin="0.0", UIMin="0.0"))
	float SolidInitialPushForceFactor = 350.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid|Pushing", meta=(ClampMin="0.0", UIMin="0.0"))
	float SolidPushForceFactor = 750000.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid|Pushing", meta=(ClampMin="0.0", UIMin="0.0"))
	float SolidPushTouchForceFactor = 0.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid|Pushing", meta=(ClampMin="0.0", UIMin="0.0"))
	float SolidPushNudgeVelocity = 45.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Liquid")
	float LiquidWalkSpeed = 420.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Liquid")
	float LiquidJumpVelocity = 700.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Liquid")
	float LiquidMeltRadius = 50.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Liquid")
	float LiquidMeltRate = 75.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Gas")
	float GasFlySpeed = 300.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Gas")
	float GasUpwardAcceleration = 180.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Gas")
	float GasMaxRiseSpeed = 160.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Gas")
	float GasMaxFallSpeed = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="1.0", UIMin="1.0"))
	float MaxHealthPoints = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="1.0", UIMin="1.0"))
	float MaxEnergyPoints = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="0.0", UIMin="0.0"))
	float HealthPoints = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals", meta=(ClampMin="0.0", UIMin="0.0"))
	float EnergyPoints = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Energy", meta=(ClampMin="0.0", UIMin="0.0"))
	float MatterStateChangeEnergyCost = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Energy", meta=(ClampMin="0.0", UIMin="0.0"))
	float GasEnergyDrainRate = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Energy", meta=(ClampMin="0.0", UIMin="0.0"))
	float LiquidEnergyCostPerMeltDepth = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Energy", meta=(ClampMin="0.0", UIMin="0.0"))
	float LiquidZeroEnergyMeltContinuationSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Healing", meta=(ClampMin="0.0", UIMin="0.0"))
	float EnergyRestoreRate = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vitals|Healing", meta=(ClampMin="0.0", UIMin="0.0"))
	float EnergyRestoredPerHealthSpent = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category="Matter State")
	bool bCanMeltObjects = false;

	void EnterMatterState(EPlayerMatterState State);
	void ExitMatterState(EPlayerMatterState State);
	void CycleMatterState(int32 Direction);
	void RestoreEnergyFromHealth();
	bool ConsumeEnergy(float Amount);
	void AddHealth(float Amount);
	void ClampVitals();
	void BroadcastVitalsChanged();
	void ApplyHealKeyFallback();
	bool CanMeltContact(AMeltableActor* MeltableActor) const;
	bool IsCurrentlyMelting() const;
	bool GetCurrentMeltableContact(AMeltableActor*& OutMeltableActor, FVector& OutLocation, FVector& OutNormal) const;
	void DrawContinuousMeltableContactDebug();
	void ApplyLiquidMelt(float DeltaTime);
	void ApplyGasEnergyDrain(float DeltaTime);
	void ApplyLiquidEnergyDepletionRule();
	void ConfigurePhysicsInteractionForCurrentState();
	void ApplySolidPushNudge(AActor* Other, UPrimitiveComponent* OtherComp, const FHitResult& Hit);
	void TriggerMatterSwitchVisuals();
	void CallMatterSwitchBlueprintEvent();
	USkeletalMeshComponent* FindMatterMeshByName(FName ComponentName) const;
	USkeletalMeshComponent* GetGasMatterMesh() const;
	USkeletalMeshComponent* GetLiquidMatterMesh() const;
	IPlayerState* GetStateObject(EPlayerMatterState State) const;

	TUniquePtr<IPlayerState> SolidStateObject;
	TUniquePtr<IPlayerState> LiquidStateObject;
	TUniquePtr<IPlayerState> GasStateObject;

	TWeakObjectPtr<AMeltableSurface> ActiveMeltableSurface;
	TWeakObjectPtr<AMeltableActor> ActiveLiquidMeltableActor;
	FVector ActiveMeltLocation = FVector::ZeroVector;
	FVector ActiveMeltNormal = FVector::UpVector;
	float ActiveMeltAccumulatedDepth = 0.0f;
	float LastMeltContactTime = -1.0f;

	TWeakObjectPtr<AMeltableActor> ActiveDebugMeltableActor;
	FVector ActiveDebugMeltLocation = FVector::ZeroVector;
	FVector ActiveDebugMeltNormal = FVector::UpVector;
	float LastDebugMeltContactTime = -1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MatterOrdinalUpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MatterOrdinalDownAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> HealAction;

	float LastEnergyRestoreAppliedTime = -1.0f;
	bool bWasHealFallbackKeyDown = false;

	friend class IPlayerState;
	friend class SolidState;
	friend class LiquidState;
	friend class GasState;
};
