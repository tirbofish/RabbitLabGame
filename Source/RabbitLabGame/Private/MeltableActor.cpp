// Fill out your copyright notice in the Description page of Project Settings.


#include "MeltableActor.h"

#include "TimerManager.h"

#include "ProceduralMeshComponent.h"
#include "RawIndexBuffer.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

class FPositionVertexBuffer;

DEFINE_LOG_CATEGORY_STATIC(LogMeltableActor, Log, All);

namespace
{
struct FMeshTriangle
{
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	FVector C = FVector::ZeroVector;
	FVector2D UVA = FVector2D::ZeroVector;
	FVector2D UVB = FVector2D::ZeroVector;
	FVector2D UVC = FVector2D::ZeroVector;
	int32 MaterialIndex = 0;
};

bool AddStaticMeshTriangle(
	const FPositionVertexBuffer& PositionVertexBuffer,
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer,
	const FRawStaticIndexBuffer& IndexBuffer,
	int32 Index,
	int32 MaterialIndex,
	bool bHasUVs,
	const FTransform& ComponentTransform,
	TArray<FMeshTriangle>& Triangles)
{
	const uint32 IndexA = IndexBuffer.GetIndex(Index);
	const uint32 IndexB = IndexBuffer.GetIndex(Index + 1);
	const uint32 IndexC = IndexBuffer.GetIndex(Index + 2);

	if (IndexA >= PositionVertexBuffer.GetNumVertices() ||
		IndexB >= PositionVertexBuffer.GetNumVertices() ||
		IndexC >= PositionVertexBuffer.GetNumVertices())
	{
		return false;
	}

	Triangles.Add({
		ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexA))),
		ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexB))),
		ComponentTransform.TransformPosition(static_cast<FVector>(PositionVertexBuffer.VertexPosition(IndexC))),
		bHasUVs ? FVector2D(StaticMeshVertexBuffer.GetVertexUV(IndexA, 0)) : FVector2D::ZeroVector,
		bHasUVs ? FVector2D(StaticMeshVertexBuffer.GetVertexUV(IndexB, 0)) : FVector2D::ZeroVector,
		bHasUVs ? FVector2D(StaticMeshVertexBuffer.GetVertexUV(IndexC, 0)) : FVector2D::ZeroVector,
		FMath::Max(0, MaterialIndex)
	});
	return true;
}

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
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = LODResources.VertexBuffers.StaticMeshVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;
	const int32 IndexCount = IndexBuffer.GetNumIndices();
	const bool bHasUVs = StaticMeshVertexBuffer.GetNumTexCoords() > 0;

	if (PositionVertexBuffer.GetNumVertices() == 0 || IndexCount < 3)
	{
		return false;
	}

	const FTransform ComponentTransform = MeshComponent->GetComponentTransform();
	Triangles.Reserve(IndexCount / 3);

	for (const FStaticMeshSection& Section : LODResources.Sections)
	{
		const int32 SectionEndIndex = Section.FirstIndex + Section.NumTriangles * 3;
		for (int32 Index = Section.FirstIndex; Index + 2 < SectionEndIndex && Index + 2 < IndexCount; Index += 3)
		{
			AddStaticMeshTriangle(
				PositionVertexBuffer,
				StaticMeshVertexBuffer,
				IndexBuffer,
				Index,
				Section.MaterialIndex,
				bHasUVs,
				ComponentTransform,
				Triangles
			);
		}
	}

	if (Triangles.IsEmpty())
	{
		for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
		{
			AddStaticMeshTriangle(
				PositionVertexBuffer,
				StaticMeshVertexBuffer,
				IndexBuffer,
				Index,
				0,
				bHasUVs,
				ComponentTransform,
				Triangles
			);
		}
	}

	return !Triangles.IsEmpty();
}

bool TryGetBarycentricCoordinates(
	const FVector& Point,
	const FMeshTriangle& Triangle,
	FVector& OutBarycentricCoordinates)
{
	const FVector EdgeAB = Triangle.B - Triangle.A;
	const FVector EdgeAC = Triangle.C - Triangle.A;
	const FVector PointOffset = Point - Triangle.A;

	const double D00 = FVector::DotProduct(EdgeAB, EdgeAB);
	const double D01 = FVector::DotProduct(EdgeAB, EdgeAC);
	const double D11 = FVector::DotProduct(EdgeAC, EdgeAC);
	const double D20 = FVector::DotProduct(PointOffset, EdgeAB);
	const double D21 = FVector::DotProduct(PointOffset, EdgeAC);
	const double Denominator = D00 * D11 - D01 * D01;

	if (FMath::IsNearlyZero(Denominator))
	{
		return false;
	}

	const double BaryB = (D11 * D20 - D01 * D21) / Denominator;
	const double BaryC = (D00 * D21 - D01 * D20) / Denominator;
	const double BaryA = 1.0 - BaryB - BaryC;

	OutBarycentricCoordinates = FVector(BaryA, BaryB, BaryC);
	return true;
}

bool TryGetClosestSourceMeshAttributes(
	const FVector& WorldPosition,
	const TArray<FMeshTriangle>& SourceTriangles,
	FVector2D* OutUV,
	int32* OutMaterialIndex)
{
	double ClosestDistanceSquared = TNumericLimits<double>::Max();
	FVector ClosestBarycentricCoordinates = FVector::ZeroVector;
	const FMeshTriangle* ClosestTriangle = nullptr;

	for (const FMeshTriangle& Triangle : SourceTriangles)
	{
		const FVector ClosestPoint = FMath::ClosestPointOnTriangleToPoint(
			WorldPosition,
			Triangle.A,
			Triangle.B,
			Triangle.C
		);

		const double DistanceSquared = FVector::DistSquared(WorldPosition, ClosestPoint);
		if (DistanceSquared >= ClosestDistanceSquared)
		{
			continue;
		}

		FVector BarycentricCoordinates = FVector::ZeroVector;
		if (!TryGetBarycentricCoordinates(ClosestPoint, Triangle, BarycentricCoordinates))
		{
			continue;
		}

		ClosestDistanceSquared = DistanceSquared;
		ClosestBarycentricCoordinates = BarycentricCoordinates;
		ClosestTriangle = &Triangle;
	}

	if (!ClosestTriangle)
	{
		return false;
	}

	if (OutUV)
	{
		*OutUV =
			ClosestTriangle->UVA * ClosestBarycentricCoordinates.X +
			ClosestTriangle->UVB * ClosestBarycentricCoordinates.Y +
			ClosestTriangle->UVC * ClosestBarycentricCoordinates.Z;
	}

	if (OutMaterialIndex)
	{
		*OutMaterialIndex = ClosestTriangle->MaterialIndex;
	}

	return true;
}

void GeneratePlanarUVs(const TArray<FVector>& WorldVertices, TArray<FVector2D>& OutUVs)
{
	OutUVs.Reset(WorldVertices.Num());

	if (WorldVertices.IsEmpty())
	{
		return;
	}

	FBox Bounds(ForceInit);
	for (const FVector& WorldVertex : WorldVertices)
	{
		Bounds += WorldVertex;
	}

	const FVector Size = Bounds.GetSize();
	const bool bDropX = Size.X <= Size.Y && Size.X <= Size.Z;
	const bool bDropY = Size.Y <= Size.X && Size.Y <= Size.Z;

	for (const FVector& WorldVertex : WorldVertices)
	{
		const FVector Offset = WorldVertex - Bounds.Min;
		const FVector2D ProjectedUV = bDropX
			? FVector2D(Offset.Y, Offset.Z)
			: bDropY
				? FVector2D(Offset.X, Offset.Z)
				: FVector2D(Offset.X, Offset.Y);
		const FVector2D UVScale = bDropX
			? FVector2D(Size.Y, Size.Z)
			: bDropY
				? FVector2D(Size.X, Size.Z)
				: FVector2D(Size.X, Size.Y);
		const FVector2D SafeUVScale(
			FMath::Max(static_cast<float>(UVScale.X), UE_SMALL_NUMBER),
			FMath::Max(static_cast<float>(UVScale.Y), UE_SMALL_NUMBER)
		);

		OutUVs.Add(ProjectedUV / SafeUVScale);
	}
}

int32 GetGridCellKey(const FSurfaceNetsGrid& Grid, const FVector& WorldPosition)
{
	const FVector SafeCellSize(
		FMath::Max(Grid.CellSize.X, static_cast<double>(UE_SMALL_NUMBER)),
		FMath::Max(Grid.CellSize.Y, static_cast<double>(UE_SMALL_NUMBER)),
		FMath::Max(Grid.CellSize.Z, static_cast<double>(UE_SMALL_NUMBER))
	);
	const int32 X = FMath::Clamp(FMath::FloorToInt32((WorldPosition.X - Grid.Origin.X) / SafeCellSize.X), 0, FMath::Max(0, Grid.VoxelCountX - 1));
	const int32 Y = FMath::Clamp(FMath::FloorToInt32((WorldPosition.Y - Grid.Origin.Y) / SafeCellSize.Y), 0, FMath::Max(0, Grid.VoxelCountY - 1));
	const int32 Z = FMath::Clamp(FMath::FloorToInt32((WorldPosition.Z - Grid.Origin.Z) / SafeCellSize.Z), 0, FMath::Max(0, Grid.VoxelCountZ - 1));

	return USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldIndex(X, Y, Z, Grid);
}

void ComputeNormalsAndTangents(
	const TArray<FVector>& Vertices,
	const TArray<int32>& Triangles,
	const TArray<FVector2D>& UVs,
	TArray<FVector>& OutNormals,
	TArray<FProcMeshTangent>& OutTangents)
{
	OutNormals.Init(FVector::ZeroVector, Vertices.Num());

	TArray<FVector> AccumulatedTangents;
	AccumulatedTangents.Init(FVector::ZeroVector, Vertices.Num());

	const bool bHasUVs = UVs.Num() == Vertices.Num();

	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const int32 IndexA = Triangles[Index];
		const int32 IndexB = Triangles[Index + 1];
		const int32 IndexC = Triangles[Index + 2];

		if (!Vertices.IsValidIndex(IndexA) || !Vertices.IsValidIndex(IndexB) || !Vertices.IsValidIndex(IndexC))
		{
			continue;
		}

		const FVector& A = Vertices[IndexA];
		const FVector& B = Vertices[IndexB];
		const FVector& C = Vertices[IndexC];

		// Area-weighted face normal. Matches the engine's triangle winding convention.
		const FVector FaceNormal = FVector::CrossProduct(B - C, A - C);
		OutNormals[IndexA] += FaceNormal;
		OutNormals[IndexB] += FaceNormal;
		OutNormals[IndexC] += FaceNormal;

		if (bHasUVs)
		{
			const FVector EdgeAB = B - A;
			const FVector EdgeAC = C - A;
			const FVector2D DeltaUVAB = UVs[IndexB] - UVs[IndexA];
			const FVector2D DeltaUVAC = UVs[IndexC] - UVs[IndexA];
			const double Determinant = DeltaUVAB.X * DeltaUVAC.Y - DeltaUVAB.Y * DeltaUVAC.X;

			if (!FMath::IsNearlyZero(Determinant))
			{
				const FVector Tangent = (EdgeAB * DeltaUVAC.Y - EdgeAC * DeltaUVAB.Y) / Determinant;
				AccumulatedTangents[IndexA] += Tangent;
				AccumulatedTangents[IndexB] += Tangent;
				AccumulatedTangents[IndexC] += Tangent;
			}
		}
	}

	OutTangents.SetNum(Vertices.Num());

	for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
	{
		FVector Normal = OutNormals[VertexIndex].GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::UpVector;
		}
		OutNormals[VertexIndex] = Normal;

		// Gram-Schmidt orthogonalize the accumulated tangent against the normal.
		FVector Tangent =
			AccumulatedTangents[VertexIndex] -
			Normal * FVector::DotProduct(AccumulatedTangents[VertexIndex], Normal);

		if (!Tangent.Normalize())
		{
			Tangent = FVector::CrossProduct(Normal, FVector::UpVector);
			if (!Tangent.Normalize())
			{
				Tangent = FVector::ForwardVector;
			}
		}

		OutTangents[VertexIndex] = FProcMeshTangent(Tangent, false);
	}
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

float SmoothCraterFalloff(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return ClampedAlpha * ClampedAlpha * (3.0f - 2.0f * ClampedAlpha);
}
}

AMeltableActor::AMeltableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SourceMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SourceMesh"));
	RootComponent = SourceMeshComponent;
	SourceMeshComponent->SetCanEverAffectNavigation(false);

	GeneratedMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
	GeneratedMeshComponent->SetupAttachment(RootComponent);
	GeneratedMeshComponent->SetCanEverAffectNavigation(false);
	GeneratedMeshComponent->bUseComplexAsSimpleCollision = true;
	GeneratedMeshComponent->bUseAsyncCooking = true;
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

void AMeltableActor::DrawMeltCollisionDebug(
	const FVector& CollisionLocation,
	const FVector& CollisionNormal,
	float MeltRadius
) const
{
	const UWorld* World = GetWorld();
	if (!World || MeltRadius <= 0.0f)
	{
		return;
	}

	const FVector Normal = CollisionNormal.IsNearlyZero() ? FVector::UpVector : CollisionNormal.GetSafeNormal();
	FVector AxisY = FVector::RightVector;
	FVector AxisZ = FVector::UpVector;
	Normal.FindBestAxisVectors(AxisY, AxisZ);

	DrawDebugCircle(
		World,
		CollisionLocation,
		MeltRadius,
		64,
		FColor::Cyan,
		false,
		DebugCollisionDuration,
		0,
		DebugCollisionLineThickness,
		AxisY,
		AxisZ,
		false
	);

	DrawDebugString(
		World,
		CollisionLocation + Normal * 24.0f,
		FString::Printf(TEXT("Melt radius %.1f"), MeltRadius),
		nullptr,
		FColor::Cyan,
		DebugCollisionDuration,
		true
	);
}

void AMeltableActor::ApplyMeltCrater(
	const FVector& CollisionLocation,
	const FVector& CollisionNormal,
	float MeltRadius,
	float MeltAmount
)
{
	if (ScalarFieldValues.IsEmpty() || MeltRadius <= 0.0f || MeltAmount <= 0.0f)
	{
		return;
	}

	const FVector Normal = CollisionNormal.IsNearlyZero() ? FVector::UpVector : CollisionNormal.GetSafeNormal();
	const FVector CraterCenter = CollisionLocation - Normal * (MeltRadius * 0.5f);
	const FVector MeltSegmentStart = CollisionLocation + Normal * (MeltRadius * 0.25f);
	const FVector MeltSegmentEnd = CollisionLocation - Normal * GetMeltThroughDepth(Normal, MeltRadius);
	const float RadiusSquared = FMath::Square(MeltRadius);
	bool bChangedScalarField = false;

	const FVector RadiusExtent(MeltRadius);
	const FVector AffectedMin = bMeltThroughSurface
		? MeltSegmentStart.ComponentMin(MeltSegmentEnd) - RadiusExtent
		: CraterCenter - RadiusExtent;
	const FVector AffectedMax = bMeltThroughSurface
		? MeltSegmentStart.ComponentMax(MeltSegmentEnd) + RadiusExtent
		: CraterCenter + RadiusExtent;

	const auto GetMinGridIndex = [](float Coordinate, float Origin, float CellSize, int32 MaxIndex)
	{
		return FMath::Clamp(FMath::FloorToInt32((Coordinate - Origin) / CellSize), 0, MaxIndex);
	};
	const auto GetMaxGridIndex = [](float Coordinate, float Origin, float CellSize, int32 MaxIndex)
	{
		return FMath::Clamp(FMath::CeilToInt32((Coordinate - Origin) / CellSize), 0, MaxIndex);
	};

	const int32 MinX = GetMinGridIndex(AffectedMin.X, SurfaceNetsGrid.Origin.X, SurfaceNetsGrid.CellSize.X, SurfaceNetsGrid.VoxelCountX);
	const int32 MinY = GetMinGridIndex(AffectedMin.Y, SurfaceNetsGrid.Origin.Y, SurfaceNetsGrid.CellSize.Y, SurfaceNetsGrid.VoxelCountY);
	const int32 MinZ = GetMinGridIndex(AffectedMin.Z, SurfaceNetsGrid.Origin.Z, SurfaceNetsGrid.CellSize.Z, SurfaceNetsGrid.VoxelCountZ);
	const int32 MaxX = GetMaxGridIndex(AffectedMax.X, SurfaceNetsGrid.Origin.X, SurfaceNetsGrid.CellSize.X, SurfaceNetsGrid.VoxelCountX);
	const int32 MaxY = GetMaxGridIndex(AffectedMax.Y, SurfaceNetsGrid.Origin.Y, SurfaceNetsGrid.CellSize.Y, SurfaceNetsGrid.VoxelCountY);
	const int32 MaxZ = GetMaxGridIndex(AffectedMax.Z, SurfaceNetsGrid.Origin.Z, SurfaceNetsGrid.CellSize.Z, SurfaceNetsGrid.VoxelCountZ);

	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector WorldPoint =
					SurfaceNetsGrid.Origin +
					FVector(
						X * SurfaceNetsGrid.CellSize.X,
						Y * SurfaceNetsGrid.CellSize.Y,
						Z * SurfaceNetsGrid.CellSize.Z
					);

				const float DistanceSquared = bMeltThroughSurface
					? FMath::PointDistToSegmentSquared(WorldPoint, MeltSegmentStart, MeltSegmentEnd)
					: FVector::DistSquared(WorldPoint, CraterCenter);
				if (DistanceSquared > RadiusSquared)
				{
					continue;
				}

				const int32 Index = USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldIndex(
					X,
					Y,
					Z,
					SurfaceNetsGrid
				);

				if (!ScalarFieldValues.IsValidIndex(Index))
				{
					continue;
				}

				const float Distance = FMath::Sqrt(DistanceSquared);
				const float Alpha = 1.0f - (Distance / MeltRadius);
				const float Falloff = SmoothCraterFalloff(Alpha);
				ScalarFieldValues[Index] += MeltAmount * Falloff;
				bChangedScalarField = true;
			}
		}
	}

	if (!bChangedScalarField)
	{
		return;
	}

	bHasBeenMelted = true;
	bMeltRegenerationPending = true;
	QueueMeltRegeneration();
}

void AMeltableActor::QueueMeltRegeneration()
{
	UWorld* World = GetWorld();
	if (!World || MeltRegenerationInterval <= 0.0f || LastMeltRegenerationTime < 0.0)
	{
		RegeneratePendingMelt();
		return;
	}

	const double Elapsed = World->GetTimeSeconds() - LastMeltRegenerationTime;
	if (Elapsed >= MeltRegenerationInterval)
	{
		RegeneratePendingMelt();
		return;
	}

	if (!World->GetTimerManager().IsTimerActive(MeltRegenerationTimerHandle))
	{
		const float Delay = FMath::Max(UE_KINDA_SMALL_NUMBER, static_cast<float>(MeltRegenerationInterval - Elapsed));
		World->GetTimerManager().SetTimer(
			MeltRegenerationTimerHandle,
			this,
			&AMeltableActor::RegeneratePendingMelt,
			Delay,
			false
		);
	}
}

void AMeltableActor::RegeneratePendingMelt()
{
	if (!bMeltRegenerationPending)
	{
		return;
	}

	if (RegenerateSurfaceNetsMesh())
	{
		bMeltRegenerationPending = false;
		const UWorld* World = GetWorld();
		LastMeltRegenerationTime = World ? World->GetTimeSeconds() : 0.0;
	}
}

bool AMeltableActor::HasBeenMelted() const
{
	return bHasBeenMelted;
}

bool AMeltableActor::IsFullyMelted() const
{
	return bHasBeenMelted && (SurfaceNetsMesh.Vertices.IsEmpty() || SurfaceNetsMesh.Triangles.Num() < 3);
}

void AMeltableActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFitGridToSourceMesh)
	{
		AutoFitSurfaceNetsGridToSourceMesh();
	}

	VertexAttributeCache.Reset();
	BuildScalarFieldFromStaticMesh(ScalarFieldValues);

	if (!RegenerateSurfaceNetsMesh())
	{
		return;
	}

	DisableSourceMeshAfterConversion();
}

bool AMeltableActor::RegenerateSurfaceNetsMesh()
{
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
		return false;
	}

	UpdateGeneratedMesh();
	return true;
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

void AMeltableActor::DisableSourceMeshAfterConversion()
{
	if (!bHideSourceMeshAfterConversion ||
		!SourceMeshComponent ||
		!GeneratedMeshComponent ||
		GeneratedMeshComponent->GetNumSections() <= 0)
	{
		return;
	}

	SourceMeshComponent->SetVisibility(false, false);
	SourceMeshComponent->SetHiddenInGame(true, false);
	SourceMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SourceMeshComponent->SetGenerateOverlapEvents(false);
}

float AMeltableActor::GetMeltThroughDepth(const FVector& SurfaceNormal, float MeltRadius) const
{
	const FVector Normal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	const FVector AbsNormal(FMath::Abs(Normal.X), FMath::Abs(Normal.Y), FMath::Abs(Normal.Z));
	const FBox Bounds = SourceMeshComponent ? SourceMeshComponent->Bounds.GetBox() : GetComponentsBoundingBox(true);
	const FVector Extent = Bounds.GetExtent();
	const float ProjectedThickness =
		2.0f * (AbsNormal.X * Extent.X + AbsNormal.Y * Extent.Y + AbsNormal.Z * Extent.Z);
	const float MinimumDepth = MeltRadius * FMath::Max(1.0f, MeltThroughDepthMultiplier);

	return FMath::Max(ProjectedThickness + MeltRadius, MinimumDepth);
}

void AMeltableActor::BuildScalarFieldFromStaticMesh(TArray<float>& OutScalarFieldValues)
{
	OutScalarFieldValues.Reset();

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

	OutScalarFieldValues.SetNumZeroed(ValueCount);

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
				OutScalarFieldValues[Index] = SignedDistance;

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
		GeneratedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
	TArray<TArray<int32>> TrianglesByMaterialIndex;

	if (SourceMeshComponent)
	{
		TArray<FMeshTriangle> SourceTriangles;
		if (ExtractStaticMeshTriangles(SourceMeshComponent, SourceTriangles))
		{
			TArray<int32> VertexMaterialIndices;
			UV0.Reserve(SurfaceNetsMesh.Vertices.Num());
			VertexMaterialIndices.Reserve(SurfaceNetsMesh.Vertices.Num());

			for (const FVector& WorldVertex : SurfaceNetsMesh.Vertices)
			{
				// Surface nets emit one vertex per cell, so cache attributes per cell and only
				// re-run the expensive closest-triangle search for vertices that actually moved.
				const int32 CellKey = GetGridCellKey(SurfaceNetsGrid, WorldVertex);
				const FMeltableCachedVertexAttributes* CachedAttributes = VertexAttributeCache.Find(CellKey);

				if (!CachedAttributes ||
					FVector::DistSquared(CachedAttributes->Position, WorldVertex) > UE_KINDA_SMALL_NUMBER)
				{
					FMeltableCachedVertexAttributes NewAttributes;
					NewAttributes.Position = WorldVertex;
					TryGetClosestSourceMeshAttributes(
						WorldVertex,
						SourceTriangles,
						&NewAttributes.UV,
						&NewAttributes.MaterialIndex
					);
					NewAttributes.MaterialIndex = FMath::Max(0, NewAttributes.MaterialIndex);
					CachedAttributes = &VertexAttributeCache.Add(CellKey, NewAttributes);
				}

				UV0.Add(CachedAttributes->UV);
				VertexMaterialIndices.Add(CachedAttributes->MaterialIndex);
			}

			for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
			{
				if (!VertexMaterialIndices.IsValidIndex(Triangles[Index]) ||
					!VertexMaterialIndices.IsValidIndex(Triangles[Index + 1]) ||
					!VertexMaterialIndices.IsValidIndex(Triangles[Index + 2]))
				{
					continue;
				}

				// Majority vote across the triangle's vertices, falling back to the first vertex.
				const int32 MaterialA = VertexMaterialIndices[Triangles[Index]];
				const int32 MaterialB = VertexMaterialIndices[Triangles[Index + 1]];
				const int32 MaterialC = VertexMaterialIndices[Triangles[Index + 2]];
				const int32 MaterialIndex = (MaterialB == MaterialC) ? MaterialB : MaterialA;

				if (TrianglesByMaterialIndex.Num() <= MaterialIndex)
				{
					TrianglesByMaterialIndex.SetNum(MaterialIndex + 1);
				}

				TrianglesByMaterialIndex[MaterialIndex].Add(Triangles[Index]);
				TrianglesByMaterialIndex[MaterialIndex].Add(Triangles[Index + 1]);
				TrianglesByMaterialIndex[MaterialIndex].Add(Triangles[Index + 2]);
			}
		}
	}

	if (TrianglesByMaterialIndex.IsEmpty())
	{
		TrianglesByMaterialIndex.SetNum(1);
		TrianglesByMaterialIndex[0] = Triangles;
	}

	if (UV0.Num() != LocalVertices.Num())
	{
		GeneratePlanarUVs(SurfaceNetsMesh.Vertices, UV0);
	}

	ComputeNormalsAndTangents(LocalVertices, Triangles, UV0, Normals, Tangents);

	for (int32 MaterialIndex = 0; MaterialIndex < TrianglesByMaterialIndex.Num(); ++MaterialIndex)
	{
		if (TrianglesByMaterialIndex[MaterialIndex].IsEmpty())
		{
			continue;
		}

		GeneratedMeshComponent->CreateMeshSection(
			MaterialIndex,
			LocalVertices,
			TrianglesByMaterialIndex[MaterialIndex],
			Normals,
			UV0,
			VertexColors,
			Tangents,
			bEnableGeneratedMeshCollision
		);
	}

	GeneratedMeshComponent->bUseComplexAsSimpleCollision = true;
	GeneratedMeshComponent->SetCollisionEnabled(
		bEnableGeneratedMeshCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision
	);
	GeneratedMeshComponent->SetCollisionObjectType(ECC_WorldStatic);
	GeneratedMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	GeneratedMeshComponent->CanCharacterStepUpOn = ECB_Yes;
	// CreateMeshSection already updates/cooks collision. Recreating the physics
	// state here forced a second synchronous rebuild on every melt update.

	if (SourceMeshComponent)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < SourceMeshComponent->GetNumMaterials(); ++MaterialIndex)
		{
			GeneratedMeshComponent->SetMaterial(MaterialIndex, SourceMeshComponent->GetMaterial(MaterialIndex));
		}
	}

	UE_LOG(
		LogMeltableActor,
		Verbose,
		TEXT("%s generated surface-nets mesh with %d vertices and %d triangles."),
		*GetName(),
		SurfaceNetsMesh.Vertices.Num(),
		SurfaceNetsMesh.Triangles.Num() / 3
	);

	for (int32 SectionIndex = 0; SectionIndex < GeneratedMeshComponent->GetNumSections(); ++SectionIndex)
	{
		const FProcMeshSection* Section = GeneratedMeshComponent->GetProcMeshSection(SectionIndex);
		const int32 SectionTriangles = Section ? Section->ProcIndexBuffer.Num() / 3 : 0;
		UE_LOG(
			LogMeltableActor,
			Log,
			TEXT("%s   section %d: %d triangles, material %s"),
			*GetName(),
			SectionIndex,
			SectionTriangles,
			*GetNameSafe(GeneratedMeshComponent->GetMaterial(SectionIndex))
		);
	}
}

