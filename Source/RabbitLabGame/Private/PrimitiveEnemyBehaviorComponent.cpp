#include "PrimitiveEnemyBehaviorComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMatterState.h"

DEFINE_LOG_CATEGORY_STATIC(LogPrimitiveEnemy, Log, All);

UPrimitiveEnemyBehaviorComponent::UPrimitiveEnemyBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPrimitiveEnemyBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter.IsValid())
	{
		UE_LOG(LogPrimitiveEnemy, Error, TEXT("%s must be attached to a Character."), *GetNameSafe(this));
		SetComponentTickEnabled(false);
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, MovementSpeed);
	}

	RefreshMovementBoundary();
	TargetPlayer = FindTargetPlayer();
}

void UPrimitiveEnemyBehaviorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCharacter.IsValid() || DeltaTime <= 0.0f)
	{
		return;
	}

	const bool bInsideBoundary = ConstrainOwnerToBoundary();
	if (!TargetPlayer.IsValid())
	{
		TargetPlayer = FindTargetPlayer();
	}

	APlayerMatterState* Player = TargetPlayer.Get();
	if (!Player)
	{
		StopHorizontalMovement();
		return;
	}

	const FVector TargetLocation = Player->GetActorLocation();
	const float HorizontalDistance = FVector::Dist2D(OwnerCharacter->GetActorLocation(), TargetLocation);

	if (Player->IsSolid())
	{
		FaceTarget(TargetLocation, DeltaTime);
		if (bInsideBoundary && HorizontalDistance > StoppingDistance)
		{
			MoveTowardTarget(TargetLocation, DeltaTime);
		}
		else
		{
			StopHorizontalMovement();
		}

		if (HorizontalDistance <= SolidShootRange)
		{
			TryFireAtTarget(Player);
		}
		return;
	}

	StopHorizontalMovement();
	if (Player->IsGas())
	{
		FaceTarget(TargetLocation, DeltaTime);
		if (HorizontalDistance <= GasShootRange)
		{
			TryFireAtTarget(Player);
		}
	}
}

bool UPrimitiveEnemyBehaviorComponent::RefreshMovementBoundary()
{
	BoundarySpline.Reset();
	BoundaryPolygon.Reset();
	bHasLastValidHorizontalLocation = false;
	bWarnedMissingBoundary = false;

	if (!MovementBoundaryActor)
	{
		return false;
	}

	USplineComponent* Spline = MovementBoundaryActor->FindComponentByClass<USplineComponent>();
	if (!Spline || !Spline->IsClosedLoop() || Spline->GetNumberOfSplinePoints() < 3)
	{
		UE_LOG(
			LogPrimitiveEnemy,
			Warning,
			TEXT("%s requires MovementBoundaryActor to contain a closed spline with at least three points."),
			*GetNameSafe(GetOwner())
		);
		return false;
	}

	BoundarySpline = Spline;
	const float SplineLength = Spline->GetSplineLength();
	const float SampleSpacing = FMath::Max(10.0f, BoundarySampleSpacing);
	const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(SplineLength / SampleSpacing), 8, 512);
	BoundaryPolygon.Reserve(SampleCount);

	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Distance = SplineLength * static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
		const FVector SampleLocation = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		BoundaryPolygon.Emplace(SampleLocation.X, SampleLocation.Y);
	}

	if (OwnerCharacter.IsValid())
	{
		const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
		if (IsLocationInsideMovementBoundary(OwnerLocation))
		{
			LastValidHorizontalLocation = FVector2D(OwnerLocation.X, OwnerLocation.Y);
			bHasLastValidHorizontalLocation = true;
		}
		else
		{
			UE_LOG(
				LogPrimitiveEnemy,
				Warning,
				TEXT("%s starts outside its movement boundary. Place it inside the closed spline."),
				*GetNameSafe(GetOwner())
			);
		}
	}

	return BoundaryPolygon.Num() >= 3;
}

bool UPrimitiveEnemyBehaviorComponent::IsLocationInsideMovementBoundary(const FVector& WorldLocation) const
{
	return IsPointInsideBoundary2D(FVector2D(WorldLocation.X, WorldLocation.Y));
}

APlayerMatterState* UPrimitiveEnemyBehaviorComponent::FindTargetPlayer() const
{
	return Cast<APlayerMatterState>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

USceneComponent* UPrimitiveEnemyBehaviorComponent::FindProjectileSpawnComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner || ProjectileSpawnComponentName.IsNone())
	{
		return nullptr;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(Owner);
	for (USceneComponent* Component : SceneComponents)
	{
		if (Component && Component->GetFName() == ProjectileSpawnComponentName)
		{
			return Component;
		}
	}

	return nullptr;
}

void UPrimitiveEnemyBehaviorComponent::StopHorizontalMovement() const
{
	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->Velocity.X = 0.0f;
		Movement->Velocity.Y = 0.0f;
	}
}

void UPrimitiveEnemyBehaviorComponent::FaceTarget(const FVector& TargetLocation, float DeltaTime) const
{
	if (!OwnerCharacter.IsValid())
	{
		return;
	}

	FVector Direction = TargetLocation - OwnerCharacter->GetActorLocation();
	Direction.Z = 0.0f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRotation(0.0f, Direction.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(
		OwnerCharacter->GetActorRotation(),
		DesiredRotation,
		DeltaTime,
		FMath::Max(0.0f, RotationSpeedDegrees)
	);
	OwnerCharacter->SetActorRotation(NewRotation);
}

void UPrimitiveEnemyBehaviorComponent::MoveTowardTarget(const FVector& TargetLocation, float DeltaTime)
{
	if (!OwnerCharacter.IsValid() || BoundaryPolygon.Num() < 3)
	{
		if (!bWarnedMissingBoundary)
		{
			UE_LOG(
				LogPrimitiveEnemy,
				Warning,
				TEXT("%s will not move until a valid MovementBoundaryActor is assigned."),
				*GetNameSafe(GetOwner())
			);
			bWarnedMissingBoundary = true;
		}
		StopHorizontalMovement();
		return;
	}

	FVector Direction = TargetLocation - OwnerCharacter->GetActorLocation();
	Direction.Z = 0.0f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = FMath::Max(0.0f, MovementSpeed);
	}

	const FVector CurrentLocation = OwnerCharacter->GetActorLocation();
	const FVector CandidateLocation = CurrentLocation + Direction * MovementSpeed * DeltaTime;
	if (!IsLocationInsideMovementBoundary(CandidateLocation))
	{
		StopHorizontalMovement();
		return;
	}

	OwnerCharacter->AddMovementInput(Direction, 1.0f);
}

void UPrimitiveEnemyBehaviorComponent::TryFireAtTarget(APlayerMatterState* Player)
{
	UWorld* World = GetWorld();
	if (!World || !OwnerCharacter.IsValid() || !Player)
	{
		return;
	}

	if (!ProjectileClass)
	{
		if (!bWarnedMissingProjectile)
		{
			UE_LOG(LogPrimitiveEnemy, Warning, TEXT("%s has no ProjectileClass assigned."), *GetNameSafe(GetOwner()));
			bWarnedMissingProjectile = true;
		}
		return;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (CurrentTime < NextFireTime)
	{
		return;
	}

	FVector SpawnLocation;
	if (const USceneComponent* SpawnComponent = FindProjectileSpawnComponent())
	{
		SpawnLocation = SpawnComponent->GetComponentLocation();
	}
	else
	{
		SpawnLocation = OwnerCharacter->GetActorTransform().TransformPosition(ProjectileSpawnOffset);
	}

	const FVector AimDirection = (Player->GetActorLocation() - SpawnLocation).GetSafeNormal();
	if (AimDirection.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerCharacter.Get();
	SpawnParameters.Instigator = OwnerCharacter.Get();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (World->SpawnActor<AActor>(ProjectileClass, SpawnLocation, AimDirection.Rotation(), SpawnParameters))
	{
		NextFireTime = CurrentTime + FMath::Max(0.01f, FireInterval);
	}
}

bool UPrimitiveEnemyBehaviorComponent::ConstrainOwnerToBoundary()
{
	if (!OwnerCharacter.IsValid() || BoundaryPolygon.Num() < 3)
	{
		return false;
	}

	const FVector CurrentLocation = OwnerCharacter->GetActorLocation();
	const FVector2D CurrentHorizontalLocation(CurrentLocation.X, CurrentLocation.Y);
	if (IsPointInsideBoundary2D(CurrentHorizontalLocation))
	{
		LastValidHorizontalLocation = CurrentHorizontalLocation;
		bHasLastValidHorizontalLocation = true;
		return true;
	}

	StopHorizontalMovement();
	if (bHasLastValidHorizontalLocation)
	{
		const FVector CorrectedLocation(
			LastValidHorizontalLocation.X,
			LastValidHorizontalLocation.Y,
			CurrentLocation.Z
		);
		OwnerCharacter->SetActorLocation(CorrectedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	return false;
}

bool UPrimitiveEnemyBehaviorComponent::IsPointInsideBoundary2D(const FVector2D& Point) const
{
	if (BoundaryPolygon.Num() < 3)
	{
		return false;
	}

	bool bInside = false;
	for (int32 Index = 0, PreviousIndex = BoundaryPolygon.Num() - 1; Index < BoundaryPolygon.Num(); PreviousIndex = Index++)
	{
		const FVector2D& Current = BoundaryPolygon[Index];
		const FVector2D& Previous = BoundaryPolygon[PreviousIndex];
		const bool bCrossesHorizontalRay = (Current.Y > Point.Y) != (Previous.Y > Point.Y);
		if (!bCrossesHorizontalRay)
		{
			continue;
		}

		const double EdgeXAtPointY =
			(Previous.X - Current.X) * (Point.Y - Current.Y) / (Previous.Y - Current.Y) + Current.X;
		if (Point.X < EdgeXAtPointY)
		{
			bInside = !bInside;
		}
	}

	return bInside;
}
