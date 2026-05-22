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

class APlayerMatterState;
class AMeltableActor;
class AMeltableSurface;
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

	UPROPERTY(BlueprintAssignable, Category="Matter State")
	FOnMatterStateChanged OnMatterStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Matter State")
	EPlayerMatterState CurrentMatterState = EPlayerMatterState::Solid;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid")
	float SolidWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category="Matter State|Solid")
	float SolidJumpVelocity = 700.0f;

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

	UPROPERTY(EditAnywhere, Category="Matter State|Gas", meta=(ClampMin="0.0", UIMin="0.0"))
	float GasDurationSeconds = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category="Matter State")
	bool bCanMeltObjects = false;

	void EnterMatterState(EPlayerMatterState State);
	void ExitMatterState(EPlayerMatterState State);
	void CycleMatterState(int32 Direction);
	bool GetCurrentMeltableContact(AMeltableActor*& OutMeltableActor, FVector& OutLocation, FVector& OutNormal) const;
	void DrawContinuousMeltableContactDebug();
	void ApplyLiquidMelt(float DeltaTime);
	IPlayerState* GetStateObject(EPlayerMatterState State) const;

	TUniquePtr<IPlayerState> SolidStateObject;
	TUniquePtr<IPlayerState> LiquidStateObject;
	TUniquePtr<IPlayerState> GasStateObject;

	TWeakObjectPtr<AMeltableSurface> ActiveMeltableSurface;
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

	friend class SolidState;
	friend class LiquidState;
	friend class GasState;
};
