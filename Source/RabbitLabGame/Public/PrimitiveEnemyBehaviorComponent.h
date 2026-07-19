#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PrimitiveEnemyBehaviorComponent.generated.h"

class ACharacter;
class APlayerMatterState;
class USceneComponent;
class USplineComponent;

/**
 * Deliberately simple enemy behaviour that chases a solid player, shoots a
 * nearby solid player, and shoots (without chasing) a gas player.
 *
 * Movement is driven directly through ACharacter::AddMovementInput instead of
 * the navigation system. This keeps the enemy primitive and lets normal
 * CharacterMovement gravity react to holes cut into procedural collision.
 */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent))
class RABBITLABGAME_API UPrimitiveEnemyBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPrimitiveEnemyBehaviorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Actor containing the closed spline that bounds this enemy in XY. Assign this per placed enemy instance. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Primitive Enemy|Boundary")
	TObjectPtr<AActor> MovementBoundaryActor;

	/** Component name to use as the projectile muzzle. Falls back to ProjectileSpawnOffset when it is not found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat")
	FName ProjectileSpawnComponentName = TEXT("SpawnProjectileLocation");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat")
	TSubclassOf<AActor> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat", meta=(ClampMin="0.0", UIMin="0.0"))
	float SolidShootRange = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat", meta=(ClampMin="0.0", UIMin="0.0"))
	float GasShootRange = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat", meta=(ClampMin="0.01", UIMin="0.01"))
	float FireInterval = 1.5f;

	/** Local-space fallback muzzle offset used if ProjectileSpawnComponentName cannot be found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Combat")
	FVector ProjectileSpawnOffset = FVector(100.0f, 0.0f, 30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float MovementSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float StoppingDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float RotationSpeedDegrees = 360.0f;

	/** Smaller values follow curved splines more closely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primitive Enemy|Boundary", meta=(ClampMin="10.0", UIMin="10.0"))
	float BoundarySampleSpacing = 50.0f;

	/** Rebuilds the cached XY polygon after assigning or changing the spline. */
	UFUNCTION(BlueprintCallable, Category="Primitive Enemy|Boundary")
	bool RefreshMovementBoundary();

	/** Returns true when a world location lies inside the assigned closed spline. Z is ignored. */
	UFUNCTION(BlueprintPure, Category="Primitive Enemy|Boundary")
	bool IsLocationInsideMovementBoundary(const FVector& WorldLocation) const;

private:
	APlayerMatterState* FindTargetPlayer() const;
	USceneComponent* FindProjectileSpawnComponent() const;
	void StopHorizontalMovement() const;
	void FaceTarget(const FVector& TargetLocation, float DeltaTime) const;
	void MoveTowardTarget(const FVector& TargetLocation, float DeltaTime);
	void TryFireAtTarget(APlayerMatterState* TargetPlayer);
	bool ConstrainOwnerToBoundary();
	bool IsPointInsideBoundary2D(const FVector2D& Point) const;

	TWeakObjectPtr<ACharacter> OwnerCharacter;
	TWeakObjectPtr<APlayerMatterState> TargetPlayer;
	TWeakObjectPtr<USplineComponent> BoundarySpline;
	TArray<FVector2D> BoundaryPolygon;
	FVector2D LastValidHorizontalLocation = FVector2D::ZeroVector;
	bool bHasLastValidHorizontalLocation = false;
	bool bWarnedMissingBoundary = false;
	bool bWarnedMissingProjectile = false;
	double NextFireTime = 0.0;
};
