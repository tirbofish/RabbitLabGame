#include "PushableActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/ConstructorHelpers.h"

APushableActor::APushableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PushableCollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("PushableCollision"));
	RootComponent = PushableCollisionComponent;

	PushableMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PushableMesh"));
	PushableMeshComponent->SetupAttachment(PushableCollisionComponent);
	PushableMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PushableMeshComponent->SetGenerateOverlapEvents(false);

	PushableCollisionComponent->SetMobility(EComponentMobility::Movable);
	ConfigurePushableCollision();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshAsset.Succeeded())
	{
		PushableStaticMesh = CubeMeshAsset.Object;
	}

	ApplyConfiguredMesh();
	FitCollisionToMesh();
}

void APushableActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguredMesh();
	FitCollisionToMesh();
	ConfigurePushableCollision();
}

void APushableActor::BeginPlay()
{
	Super::BeginPlay();

	ConfigurePushablePhysics();
}

void APushableActor::SetPushableStaticMesh(UStaticMesh* NewStaticMesh)
{
	PushableStaticMesh = NewStaticMesh;

	const bool bWasSimulatingPhysics = PushableCollisionComponent && PushableCollisionComponent->IsSimulatingPhysics();
	if (bWasSimulatingPhysics)
	{
		PushableCollisionComponent->SetSimulatePhysics(false);
	}

	ApplyConfiguredMesh();
	FitCollisionToMesh();
	ConfigurePushableCollision();

	if (bWasSimulatingPhysics)
	{
		ConfigurePushablePhysics();
	}
}

void APushableActor::ApplyConfiguredMesh()
{
	if (!PushableMeshComponent)
	{
		return;
	}

	PushableMeshComponent->SetStaticMesh(PushableStaticMesh);
	PushableMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PushableMeshComponent->SetGenerateOverlapEvents(false);
}

void APushableActor::FitCollisionToMesh()
{
	if (!PushableCollisionComponent || !PushableMeshComponent || !PushableStaticMesh)
	{
		return;
	}

	const FBoxSphereBounds MeshBounds = PushableStaticMesh->GetBounds();
	const FVector BoxExtent = MeshBounds.BoxExtent.ComponentMax(FVector(1.0f));

	PushableCollisionComponent->SetBoxExtent(BoxExtent, false);
	PushableMeshComponent->SetRelativeLocation(-MeshBounds.Origin);
}

void APushableActor::ConfigurePushableCollision()
{
	if (!PushableCollisionComponent)
	{
		return;
	}

	PushableCollisionComponent->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	PushableCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PushableCollisionComponent->SetCollisionObjectType(ECC_PhysicsBody);
	PushableCollisionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	PushableCollisionComponent->CanCharacterStepUpOn = ECB_No;
	PushableCollisionComponent->BodyInstance.bLockXRotation = bLockRotation;
	PushableCollisionComponent->BodyInstance.bLockYRotation = bLockRotation;
	PushableCollisionComponent->BodyInstance.bLockZRotation = bLockRotation;
}

void APushableActor::ConfigurePushablePhysics()
{
	if (!PushableCollisionComponent)
	{
		return;
	}

	ApplyConfiguredMesh();
	FitCollisionToMesh();
	ConfigurePushableCollision();

	PushableCollisionComponent->SetMobility(EComponentMobility::Movable);
	PushableCollisionComponent->SetEnableGravity(true);
	PushableCollisionComponent->SetLinearDamping(LinearDamping);
	PushableCollisionComponent->SetAngularDamping(AngularDamping);
	PushableCollisionComponent->SetMassOverrideInKg(NAME_None, PushableMassKg, true);
	PushableCollisionComponent->SetSimulatePhysics(true);
}
