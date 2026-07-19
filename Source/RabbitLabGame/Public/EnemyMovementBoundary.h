#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyMovementBoundary.generated.h"

class USplineComponent;

/** Placeable closed spline used to constrain primitive enemies in XY. */
UCLASS(BlueprintType)
class RABBITLABGAME_API AEnemyMovementBoundary : public AActor
{
	GENERATED_BODY()

public:
	AEnemyMovementBoundary();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy Boundary")
	TObjectPtr<USplineComponent> BoundarySpline;
};
