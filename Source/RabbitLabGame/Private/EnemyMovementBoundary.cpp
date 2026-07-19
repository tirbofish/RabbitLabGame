#include "EnemyMovementBoundary.h"

#include "Components/SplineComponent.h"

AEnemyMovementBoundary::AEnemyMovementBoundary()
{
	PrimaryActorTick.bCanEverTick = false;

	BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
	RootComponent = BoundarySpline;
	BoundarySpline->ClearSplinePoints(false);
	BoundarySpline->AddSplinePoint(FVector(-500.0f, -500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(-500.0f, 500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(500.0f, 500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->AddSplinePoint(FVector(500.0f, -500.0f, 0.0f), ESplineCoordinateSpace::Local, false);
	BoundarySpline->SetClosedLoop(true, true);
	BoundarySpline->SetCanEverAffectNavigation(false);
}
