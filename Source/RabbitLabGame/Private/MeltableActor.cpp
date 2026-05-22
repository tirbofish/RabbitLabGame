// Fill out your copyright notice in the Description page of Project Settings.


#include "MeltableActor.h"

#include "ProceduralMeshComponent.h"
#include "RawIndexBuffer.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

class FPositionVertexBuffer;

DEFINE_LOG_CATEGORY_STATIC(LogMeltableActor, Log, All);

namespace
{
struct FMeshTriangle
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	FVector C = FVector::ZeroVector;
};

bool ExtractStaticMeshTriangles(const UStaticMeshComponent* MeshComponent, TArray<FMeshTriangle>& Triangles)
{
	Triangles.Reset();

	if (!MeshComponent)
	{
		return false;
	}

	const UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
	if (!StaticMesh || !StaticMesh->GetRenderData() || StaticMesh->GetRenderData()->LODResources.IsEmpty())
	{
		return false;
	}

	const FStaticMeshLODResources& LODResources = StaticMesh->GetRenderData()->LODResources[0];
	const FPositionVertexBuffer& PositionVertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
	const int32 IndexCount = IndexBuffer.GetNumIndices();

	if (PositionVertexBuffer.GetNumVertices() == 0 || IndexCount < 3)
	{
		return false;
	}

	const FTransform ComponentTransform = MeshComponent->GetComponentTransform();
	Triangles.Reserve(IndexCount / 3);

	for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
	{
		const uint32 IndexA = IndexBuffer.GetIndex(Index);
		const uint32 IndexB = IndexBuffer.GetIndex(Index + 1);
		const uint32 IndexC = IndexBuffer.GetIndex(Index + 2);

		if (IndexA >= PositionVertexBuffer.GetNumVertices() ||
			IndexB >= PositionVertexBuffer.GetNumVertices() ||
			IndexC >= PositionVertexBuffer.GetNumVertices())
		{
			continue;
		}

		Triangles.Add({
			ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexA))),
			ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexB))),
			ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexC)))
		});
	}

	return !Triangles.IsEmpty();
}

bool RayIntersectsTriangle(
	const FVector& RayOrigin,
	const FVector& RayDirection,
	const FMeshTriangle& Triangle,
	double& OutDistance)
{
	constexpr double Epsilon = 1.0e-8;

	const FVector Edge1 = Triangle.B - Triangle.A;
	const FVector Edge2 = Triangle.C - Triangle.A;
	const FVector H = FVector::CrossProduct(RayDirection, Edge2);
	const double Determinant = FVector::DotProduct(Edge1, H);

	if (FMath::Abs(Determinant) < Epsilon)
	{
		return false;
	}

	const double InverseDeterminant = 1.0 / Determinant;
	const FVector S = RayOrigin - Triangle.A;
	const double U = InverseDeterminant * FVector::DotProduct(S, H);
	if (U < 0.0 || U > 1.0)
	{
		return false;
	}

	const FVector Q = FVector::CrossProduct(S, Edge1);
	const double V = InverseDeterminant * FVector::DotProduct(RayDirection, Q);
	if (V < 0.0 || U + V > 1.0)
	{
		return false;
	}

	const double Distance = InverseDeterminant * FVector::DotProduct(Edge2, Q);
	if (Distance <= Epsilon)
	{
		return false;
	}

	OutDistance = Distance;
	return true;
}

bool IsPointInsideClosedMesh(const FVector& Point, const TArray<FMeshTriangle>& Triangles)
{
	const FVector RayDirection = FVector(1.0, 0.3713906764, 0.5298129428).GetSafeNormal();
	TArray<double> HitDistances;

	for (const FMeshTriangle& Triangle : Triangles)
	{
		double Distance = 0.0;
		if (RayIntersectsTriangle(Point, RayDirection, Triangle, Distance))
		{
			HitDistances.Add(Distance);
		}
	}

	HitDistances.Sort();

	int32 IntersectionCount = 0;
	double LastCountedDistance = -TNumericLimits<double>::Max();
	for (const double HitDistance : HitDistances)
	{
		if (FMath::Abs(HitDistance - LastCountedDistance) > 1.0e-4)
		{
			++IntersectionCount;
			LastCountedDistance = HitDistance;
		}
	}

	return (IntersectionCount % 2) == 1;
}

float GetSignedDistanceToMesh(const FVector& Point, const TArray<FMeshTriangle>& Triangles)
{
	double ClosestDistanceSquared = TNumericLimits<double>::Max();

	for (const FMeshTriangle& Triangle : Triangles)
	{
		const FVector ClosestPoint = FMath::ClosestPointOnTriangleToPoint(
			Point,
			Triangle.A,
			Triangle.B,
			Triangle.C
		);
		ClosestDistanceSquared = FMath::Min(ClosestDistanceSquared, FVector::DistSquared(Point, ClosestPoint));
	}

	if (ClosestDistanceSquared == TNumericLimits<double>::Max())
	{
		return 0.0f;
	}

	const float Distance = static_cast<float>(FMath::Sqrt(ClosestDistanceSquared));
	return IsPointInsideClosedMesh(Point, Triangles) ? -Distance : Distance;
}
}

AMeltableActor::AMeltableActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SourceMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceMesh"));
	RootComponent = SourceMeshComponent;

	GeneratedMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	GeneratedMeshComponent->SetupAttachment(RootComponent);
	GeneratedMeshComponent->bUseComplexAsSimpleCollision = true;
	GeneratedMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeneratedMeshComponent->SetCollisionObjectType(ECC_WorldStatic);
	GeneratedMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	GeneratedMeshComponent->CanCharacterStepUpOn = ECB_Yes;

	SurfaceNetsGrid.Origin = FVector::ZeroVector;
	SurfaceNetsGrid.CellSize = FVector(10.0f, 10.0f, 10.0f);
	SurfaceNetsGrid.VoxelCountX = 32;
	SurfaceNetsGrid.VoxelCountY = 32;
	SurfaceNetsGrid.VoxelCountZ = 32;

	SurfaceNetsIsovalue = 0.0f;
}

void AMeltableActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFitGridToSourceMesh)
	{
		AutoFitSurfaceNetsGridToSourceMesh();
	}

	TArray<float> ScalarFieldValues;
	BuildScalarFieldFromStaticMesh(ScalarFieldValues);

	const bool bGeneratedMesh = USurfaceNetsBlueprintLibrary::GenerateSurfaceNetsMesh(
		SurfaceNetsGrid,
		ScalarFieldValues,
		SurfaceNetsMesh,
		SurfaceNetsIsovalue
	);

	if (!bGeneratedMesh)
	{
		UE_LOG(
			LogMeltableActor,
			Warning,
			TEXT("%s failed to generate a surface-nets mesh. Scalar field values: %d, expected: %d."),
			*GetName(),
			ScalarFieldValues.Num(),
			USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldValueCount(SurfaceNetsGrid)
		);
		return;
	}

	UpdateGeneratedMesh();

	if (bHideSourceMeshAfterConversion && SourceMeshComponent && GeneratedMeshComponent && GeneratedMeshComponent->GetNumSections() > 0)
	{
		SourceMeshComponent->SetVisibility(false, false);
		SourceMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AMeltableActor::AutoFitSurfaceNetsGridToSourceMesh()
{
	if (!SourceMeshComponent || !SourceMeshComponent->GetStaticMesh())
	{
		UE_LOG(LogMeltableActor, Warning, TEXT("%s cannot auto-fit surface-nets grid because SourceMesh has no static mesh."), *GetName());
		return;
	}

	const FBoxSphereBounds Bounds = SourceMeshComponent->Bounds;
	const FVector Padding(AutoFitGridPadding);
	const FVector BoundsMin = Bounds.GetBox().Min - Padding;
	const FVector BoundsMax = Bounds.GetBox().Max + Padding;
	const FVector BoundsSize = BoundsMax - BoundsMin;

	if (BoundsSize.X <= UE_SMALL_NUMBER || BoundsSize.Y <= UE_SMALL_NUMBER || BoundsSize.Z <= UE_SMALL_NUMBER)
	{
		UE_LOG(LogMeltableActor, Warning, TEXT("%s cannot auto-fit surface-nets grid because SourceMesh bounds are invalid."), *GetName());
		return;
	}

	const FVector CellSize(
		BoundsSize.X / FMath::Max(1, SurfaceNetsGrid.VoxelCountX),
		BoundsSize.Y / FMath::Max(1, SurfaceNetsGrid.VoxelCountY),
		BoundsSize.Z / FMath::Max(1, SurfaceNetsGrid.VoxelCountZ)
	);

	SurfaceNetsGrid.Origin = BoundsMin;
	SurfaceNetsGrid.CellSize = CellSize;

	if (bAddOuterVoxelPadding)
	{
		SurfaceNetsGrid.Origin -= CellSize;
		SurfaceNetsGrid.VoxelCountX += 2;
		SurfaceNetsGrid.VoxelCountY += 2;
		SurfaceNetsGrid.VoxelCountZ += 2;
	}

	UE_LOG(
		LogMeltableActor,
		Log,
		TEXT("%s auto-fit surface-nets grid. Origin=%s CellSize=%s Voxels=(%d, %d, %d) OuterPadding=%s"),
		*GetName(),
		*SurfaceNetsGrid.Origin.ToString(),
		*SurfaceNetsGrid.CellSize.ToString(),
		SurfaceNetsGrid.VoxelCountX,
		SurfaceNetsGrid.VoxelCountY,
		SurfaceNetsGrid.VoxelCountZ,
		bAddOuterVoxelPadding ? TEXT("yes") : TEXT("no")
	);
}

void AMeltableActor::BuildScalarFieldFromStaticMesh(TArray<float>& ScalarFieldValues)
{
	ScalarFieldValues.Reset();

	if (!SourceMeshComponent)
	{
		UE_LOG(LogMeltableActor, Warning, TEXT("%s cannot build scalar field because SourceMeshComponent is missing."), *GetName());
		return;
	}

	TArray<FMeshTriangle> Triangles;
	if (!ExtractStaticMeshTriangles(SourceMeshComponent, Triangles))
	{
		UE_LOG(LogMeltableActor, Warning, TEXT("%s cannot build scalar field because no static mesh triangles were extracted."), *GetName());
		return;
	}

	const int32 ValueCount =
		USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldValueCount(SurfaceNetsGrid);

	ScalarFieldValues.SetNumZeroed(ValueCount);

	bool bHasNegativeSample = false;
	bool bHasPositiveSample = false;
	
	for (int32 Z = 0; Z <= SurfaceNetsGrid.VoxelCountZ; ++Z)
	{
		for (int32 Y = 0; Y <= SurfaceNetsGrid.VoxelCountY; ++Y)
		{
			for (int32 X = 0; X <= SurfaceNetsGrid.VoxelCountX; ++X)
			{
				const int32 Index =
					USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldIndex(
						X,
						Y,
						Z,
						SurfaceNetsGrid
					);

				const FVector WorldPoint =
					SurfaceNetsGrid.Origin +
					FVector(
						X * SurfaceNetsGrid.CellSize.X,
						Y * SurfaceNetsGrid.CellSize.Y,
						Z * SurfaceNetsGrid.CellSize.Z
					);

				const float SignedDistance = GetSignedDistanceToMesh(WorldPoint, Triangles);
				ScalarFieldValues[Index] = SignedDistance;

				bHasNegativeSample |= SignedDistance < SurfaceNetsIsovalue;
				bHasPositiveSample |= SignedDistance >= SurfaceNetsIsovalue;
			}
		}
	}

	if (!bHasNegativeSample || !bHasPositiveSample)
	{
		UE_LOG(
			LogMeltableActor,
			Warning,
			TEXT("%s sampled scalar field does not cross isovalue %.3f. Negative side: %s, positive side: %s. Check grid origin/cell size or enable auto-fit."),
			*GetName(),
			SurfaceNetsIsovalue,
			bHasNegativeSample ? TEXT("yes") : TEXT("no"),
			bHasPositiveSample ? TEXT("yes") : TEXT("no")
		);
	}
}

void AMeltableActor::UpdateGeneratedMesh()
{
	if (!GeneratedMeshComponent)
	{
		return;
	}

	GeneratedMeshComponent->ClearAllMeshSections();

	if (SurfaceNetsMesh.Vertices.IsEmpty() || SurfaceNetsMesh.Triangles.Num() < 3)
	{
		return;
	}

	TArray<FVector> LocalVertices;
	LocalVertices.Reserve(SurfaceNetsMesh.Vertices.Num());

	const FTransform GeneratedMeshTransform = GeneratedMeshComponent->GetComponentTransform();
	for (const FVector& WorldVertex : SurfaceNetsMesh.Vertices)
	{
		LocalVertices.Add(GeneratedMeshTransform.InverseTransformPosition(WorldVertex));
	}

	TArray<int32> Triangles = SurfaceNetsMesh.Triangles;
	if (bFlipGeneratedTriangleWinding)
	{
		for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
		{
			Swap(Triangles[Index + 1], Triangles[Index + 2]);
		}
	}

	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	GeneratedMeshComponent->CreateMeshSection(
		0,
		LocalVertices,
		Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		bEnableGeneratedMeshCollision
	);

	GeneratedMeshComponent->bUseComplexAsSimpleCollision = true;
	GeneratedMeshComponent->SetCollisionEnabled(
		bEnableGeneratedMeshCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision
	);
	GeneratedMeshComponent->SetCollisionObjectType(ECC_WorldStatic);
	GeneratedMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	GeneratedMeshComponent->CanCharacterStepUpOn = ECB_Yes;

	if (SourceMeshComponent)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SourceMeshComponent->GetNumMaterials(); ++MaterialIndex)
		{
			GeneratedMeshComponent->SetMaterial(MaterialIndex, SourceMeshComponent->GetMaterial(MaterialIndex));
		}
	}

	UE_LOG(
		LogMeltableActor,
		Log,
		TEXT("%s generated surface-nets mesh with %d vertices and %d triangles."),
		*GetName(),
		SurfaceNetsMesh.Vertices.Num(),
		SurfaceNetsMesh.Triangles.Num() / 3
	);
}

