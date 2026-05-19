#include "MeltableSurface.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "NaiveSurfaceNets/surface_nets.h"
#include "StaticMeshResources.h"

namespace
{
constexpr float SurfaceNetsIsoValue = 0.0f;

// Compute closest distance from a point to a triangle (unsigned)
float DistanceToTriangle(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	const FVector AB = B - A;
	const FVector AC = C - A;
	const FVector AP = Point - A;

	const double d1 = FVector::DotProduct(AB, AP);
	const double d2 = FVector::DotProduct(AC, AP);
	if (d1 <= 0.0 && d2 <= 0.0)
	{
		return FVector::Dist(Point, A);
	}

	const FVector BP = Point - B;
	const double d3 = FVector::DotProduct(AB, BP);
	const double d4 = FVector::DotProduct(AC, BP);
	if (d3 >= 0.0 && d4 <= d3)
	{
		return FVector::Dist(Point, B);
	}

	const double vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
	{
		const double v = d1 / (d1 - d3);
		return FVector::Dist(Point, A + AB * v);
	}

	const FVector CP = Point - C;
	const double d5 = FVector::DotProduct(AB, CP);
	const double d6 = FVector::DotProduct(AC, CP);
	if (d6 >= 0.0 && d5 <= d6)
	{
		return FVector::Dist(Point, C);
	}

	const double vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
	{
		const double w = d2 / (d2 - d6);
		return FVector::Dist(Point, A + AC * w);
	}

	const double va = d3 * d6 - d5 * d4;
	if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
	{
		const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return FVector::Dist(Point, B + (C - B) * w);
	}

	const double denom = 1.0 / (va + vb + vc);
	const double v = vb * denom;
	const double w = vc * denom;
	const FVector ClosestPoint = A + AB * v + AC * w;
	return FVector::Dist(Point, ClosestPoint);
}
}

AMeltableSurface::AMeltableSurface()
{
	PrimaryActorTick.bCanEverTick = true;
	
	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;

	DynamicMeshComp = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComp"));
	DynamicMeshComp->SetupAttachment(RootComponent);

	DynamicMeshComp->SetVisibility(false);
}

void AMeltableSurface::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Deferred collision rebuild to avoid per-frame physics instability
	if (bCollisionDirty && bRegenerateCollisionAfterMelt)
	{
		const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		if (CurrentTime - LastCollisionRebuildTime >= CollisionRebuildCooldown)
		{
			DynamicMeshComp->UpdateCollision(false);
			LastCollisionRebuildTime = CurrentTime;
			bCollisionDirty = false;
		}
	}
}

void AMeltableSurface::BeginPlay()
{
	Super::BeginPlay();

	ConvertStaticMeshToDynamicMesh();
	if (bSurfaceNetsInitialized)
	{
		SetupDynamicMeshPhysics(bEnablePhysicsSimulation);
	}
}

void AMeltableSurface::ConvertStaticMeshToDynamicMesh()
{
	if (!StaticMeshComp || !DynamicMeshComp)
	{
		return;
	}

	UStaticMesh* MeshToCopy = StaticMeshComp->GetStaticMesh();
	if (!MeshToCopy)
	{
		UE_LOG(LogTemp, Warning, TEXT("MeltableSurface: StaticMeshComp has no StaticMesh."));
		return;
	}

	if (!InitializeVoxelField())
	{
		UE_LOG(LogTemp, Warning, TEXT("MeltableSurface: Failed to initialize voxel field."));
		return;
	}

	RebuildSurfaceNets();

	if (!DynamicMeshComp->GetDynamicMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("MeltableSurface: Surface nets produced no dynamic mesh."));
		return;
	}

	const int32 MaterialCount = StaticMeshComp->GetNumMaterials();
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		DynamicMeshComp->SetMaterial(Index, StaticMeshComp->GetMaterial(Index));
	}

	StaticMeshComp->SetVisibility(false, true);
	StaticMeshComp->SetHiddenInGame(true, true);
	StaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DynamicMeshComp->SetVisibility(true, true);
	DynamicMeshComp->SetHiddenInGame(false, true);
}

void AMeltableSurface::SubdivideDynamicMesh()
{
	// No-op: surface nets handles mesh resolution via VoxelSize parameter.
	// Reduce VoxelSize for higher resolution output.
}

void AMeltableSurface::SetupDynamicMeshPhysics(bool bSimulatePhysics)
{
	if (!DynamicMeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("MeltableSurface: Cannot setup physics. DynamicMeshComp is null."));
		return;
	}

	if (!DynamicMeshComp->GetDynamicMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("MeltableSurface: Cannot setup physics. No DynamicMesh assigned."));
		return;
	}

	DynamicMeshComp->SetCollisionEnabled(DynamicMeshCollisionEnabled);
	DynamicMeshComp->SetCollisionObjectType(DynamicMeshObjectType);

	DynamicMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	DynamicMeshComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	DynamicMeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);

	DynamicMeshComp->SetGenerateOverlapEvents(false);
	DynamicMeshComp->SetNotifyRigidBodyCollision(true);
	DynamicMeshComp->SetComplexAsSimpleCollisionEnabled(true, true);
	DynamicMeshComp->UpdateCollision(false);
	DynamicMeshComp->SetSimulatePhysics(bSimulatePhysics);

	if (bSimulatePhysics)
	{
		DynamicMeshComp->SetCollisionObjectType(ECC_PhysicsBody);
		DynamicMeshComp->SetEnableGravity(true);
	}
}

void AMeltableSurface::RebuildSurfaceNets()
{
	if (!DynamicMeshComp || !bSurfaceNetsInitialized || ScalarField.IsEmpty())
	{
		return;
	}

	isosurface::regular_grid_t Grid;
	Grid.x = LocalFieldBounds.Min.X;
	Grid.y = LocalFieldBounds.Min.Y;
	Grid.z = LocalFieldBounds.Min.Z;
	Grid.dx = VoxelSize;
	Grid.dy = VoxelSize;
	Grid.dz = VoxelSize;
	Grid.sx = static_cast<std::size_t>(CellCounts.X);
	Grid.sy = static_cast<std::size_t>(CellCounts.Y);
	Grid.sz = static_cast<std::size_t>(CellCounts.Z);

	const common::igl_triangle_mesh SurfaceMesh = isosurface::surface_nets(
		[this](float X, float Y, float Z)
		{
			return SampleField(X, Y, Z);
		},
		Grid,
		SurfaceNetsIsoValue
	);

	UDynamicMesh* NewDynamicMesh = NewObject<UDynamicMesh>(DynamicMeshComp);
	NewDynamicMesh->EditMesh([&](UE::Geometry::FDynamicMesh3& Mesh)
	{
		TArray<int32> VertexIDs;
		VertexIDs.Reserve(static_cast<int32>(SurfaceMesh.V.size()));

		for (const std::array<double, 3>& Vertex : SurfaceMesh.V)
		{
			VertexIDs.Add(Mesh.AppendVertex(FVector3d(Vertex[0], Vertex[1], Vertex[2])));
		}

		for (const std::array<int, 3>& Triangle : SurfaceMesh.F)
		{
			if (!VertexIDs.IsValidIndex(Triangle[0]) ||
				!VertexIDs.IsValidIndex(Triangle[1]) ||
				!VertexIDs.IsValidIndex(Triangle[2]))
			{
				continue;
			}

			Mesh.AppendTriangle(
				VertexIDs[Triangle[0]],
				VertexIDs[Triangle[1]],
				VertexIDs[Triangle[2]]
			);
		}
	});

	DynamicMeshComp->SetDynamicMesh(NewDynamicMesh);
	DynamicMeshComp->NotifyMeshUpdated();

	// Mark collision as dirty - actual rebuild happens in Tick with throttling
	bCollisionDirty = true;

	DirtyChunks.Reset();
}

void AMeltableSurface::ResetMeltField()
{
	if (InitialScalarField.IsEmpty())
	{
		return;
	}

	ScalarField = InitialScalarField;
	MarkAllChunksDirty();
	RebuildSurfaceNets();
}

void AMeltableSurface::ApplySphericalCrater(
	FVector WorldHitLocation,
	float Radius
)
{
	if (!bSurfaceNetsInitialized || ScalarField.IsEmpty() || Radius <= 0.0f)
	{
		return;
	}

	const FTransform& ComponentTransform = DynamicMeshComp->GetComponentTransform();
	const FVector LocalHitLocation = ComponentTransform.InverseTransformPosition(WorldHitLocation);

	const FBox SphereBounds(
		LocalHitLocation - FVector(Radius),
		LocalHitLocation + FVector(Radius)
	);
	const FBox InflatedBounds = SphereBounds.ExpandBy(VoxelSize);

	const int32 MinX = FMath::Clamp(FMath::FloorToInt((InflatedBounds.Min.X - LocalFieldBounds.Min.X) / VoxelSize), 0, CornerCounts.X - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt((InflatedBounds.Min.Y - LocalFieldBounds.Min.Y) / VoxelSize), 0, CornerCounts.Y - 1);
	const int32 MinZ = FMath::Clamp(FMath::FloorToInt((InflatedBounds.Min.Z - LocalFieldBounds.Min.Z) / VoxelSize), 0, CornerCounts.Z - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt((InflatedBounds.Max.X - LocalFieldBounds.Min.X) / VoxelSize), 0, CornerCounts.X - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt((InflatedBounds.Max.Y - LocalFieldBounds.Min.Y) / VoxelSize), 0, CornerCounts.Y - 1);
	const int32 MaxZ = FMath::Clamp(FMath::CeilToInt((InflatedBounds.Max.Z - LocalFieldBounds.Min.Z) / VoxelSize), 0, CornerCounts.Z - 1);

	bool bChangedField = false;
	for (int32 Z = MinZ; Z <= MaxZ; ++Z)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const FVector SamplePoint = LocalFieldBounds.Min + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
				const float DistToCenter = FVector::Dist(SamplePoint, LocalHitLocation);

				// Smooth spherical falloff: positive inside sphere (material removed), negative outside
				const float MeltValue = Radius - DistToCenter;
				if (MeltValue <= 0.0f)
				{
					continue;
				}

				const int32 FieldIndex = GetFieldIndex(X, Y, Z);
				if (ScalarField.IsValidIndex(FieldIndex) && MeltValue > ScalarField[FieldIndex])
				{
					ScalarField[FieldIndex] = MeltValue;
					bChangedField = true;
				}
			}
		}
	}

	if (bChangedField)
	{
		MarkDirtyChunksInBounds(InflatedBounds);
		RebuildSurfaceNets();
	}
}

void AMeltableSurface::ApplyAcidCrater(
	FVector WorldHitLocation,
	FVector WorldHitNormal,
	float Radius,
	float Depth,
	float RimHeight
)
{
	(void)RimHeight;
	(void)WorldHitNormal;
	(void)Depth;

	// Delegate to spherical crater - the radius controls the size of the sphere carved out
	ApplySphericalCrater(WorldHitLocation, Radius);
}

bool AMeltableSurface::InitializeVoxelField()
{
	if (!StaticMeshComp || VoxelSize <= UE_SMALL_NUMBER)
	{
		return false;
	}

	UStaticMesh* Mesh = StaticMeshComp->GetStaticMesh();
	if (!Mesh)
	{
		return false;
	}

	// Extract triangles from the static mesh LOD0
	const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[0];
	const FPositionVertexBuffer& PositionBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;

	const int32 NumVertices = PositionBuffer.GetNumVertices();
	MeshVertices.SetNum(NumVertices);
	for (int32 i = 0; i < NumVertices; ++i)
	{
		MeshVertices[i] = FVector(PositionBuffer.VertexPosition(i));
	}

	TArray<uint32> Indices;
	IndexBuffer.GetCopy(Indices);
	const int32 NumTriangles = Indices.Num() / 3;
	MeshTriangles.SetNum(NumTriangles);
	for (int32 i = 0; i < NumTriangles; ++i)
	{
		MeshTriangles[i] = FIntVector(Indices[i * 3], Indices[i * 3 + 1], Indices[i * 3 + 2]);
	}

	// Compute mesh local bounds from actual vertices
	MeshLocalBounds.Init();
	for (const FVector& V : MeshVertices)
	{
		MeshLocalBounds += V;
	}

	if (MeshLocalBounds.GetExtent().IsNearlyZero() || MeshTriangles.IsEmpty())
	{
		return false;
	}

	LocalFieldBounds = MeshLocalBounds.ExpandBy(FMath::Max(SurfacePadding, VoxelSize * 2.0f));

	const FVector Size = LocalFieldBounds.GetSize();
	CellCounts = FIntVector(
		FMath::Max(2, FMath::CeilToInt(Size.X / VoxelSize)),
		FMath::Max(2, FMath::CeilToInt(Size.Y / VoxelSize)),
		FMath::Max(2, FMath::CeilToInt(Size.Z / VoxelSize))
	);
	CornerCounts = CellCounts + FIntVector(1, 1, 1);

	const int32 TotalCorners = CornerCounts.X * CornerCounts.Y * CornerCounts.Z;
	ScalarField.SetNumUninitialized(TotalCorners);

	// Compute signed distance field from actual mesh geometry
	for (int32 Z = 0; Z < CornerCounts.Z; ++Z)
	{
		for (int32 Y = 0; Y < CornerCounts.Y; ++Y)
		{
			for (int32 X = 0; X < CornerCounts.X; ++X)
			{
				const FVector SamplePoint = LocalFieldBounds.Min + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize);
				ScalarField[GetFieldIndex(X, Y, Z)] = ComputeMeshSignedDistance(SamplePoint);
			}
		}
	}

	InitialScalarField = ScalarField;
	bSurfaceNetsInitialized = true;
	MarkAllChunksDirty();
	return true;
}

float AMeltableSurface::ComputeMeshSignedDistance(const FVector& LocalPosition) const
{
	if (MeshTriangles.IsEmpty())
	{
		return 1.0f;
	}

	float MinDist = MAX_FLT;
	FVector ClosestNormal = FVector::UpVector;

	for (const FIntVector& Tri : MeshTriangles)
	{
		const FVector& A = MeshVertices[Tri.X];
		const FVector& B = MeshVertices[Tri.Y];
		const FVector& C = MeshVertices[Tri.Z];

		const float Dist = DistanceToTriangle(LocalPosition, A, B, C);
		if (Dist < MinDist)
		{
			MinDist = Dist;
			// Triangle face normal for sign determination
			ClosestNormal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		}
	}

	// Determine sign using pseudo-normal: if the vector from the closest surface to the point
	// is in the same direction as the face normal, the point is outside
	// We use a ray-casting approach for robustness: cast a ray and count intersections
	// Simple heuristic: dot product with closest triangle normal
	const FVector ToPoint = (LocalPosition - MeshLocalBounds.GetCenter()).GetSafeNormal();
	
	// Use winding number approximation via ray cast along +X axis
	int32 CrossingCount = 0;
	for (const FIntVector& Tri : MeshTriangles)
	{
		const FVector& A = MeshVertices[Tri.X];
		const FVector& B = MeshVertices[Tri.Y];
		const FVector& C = MeshVertices[Tri.Z];

		// Ray from LocalPosition in +X direction
		// Check if the triangle is in front of the point in X and the ray intersects YZ bounds
		const float MinY = FMath::Min3(A.Y, B.Y, C.Y);
		const float MaxY = FMath::Max3(A.Y, B.Y, C.Y);
		const float MinZ = FMath::Min3(A.Z, B.Z, C.Z);
		const float MaxZ = FMath::Max3(A.Z, B.Z, C.Z);

		if (LocalPosition.Y < MinY || LocalPosition.Y > MaxY ||
			LocalPosition.Z < MinZ || LocalPosition.Z > MaxZ)
		{
			continue;
		}

		// Moller-Trumbore ray-triangle intersection
		const FVector Edge1 = B - A;
		const FVector Edge2 = C - A;
		const FVector RayDir(1.0, 0.0, 0.0);
		const FVector H = FVector::CrossProduct(RayDir, Edge2);
		const double Det = FVector::DotProduct(Edge1, H);

		if (FMath::Abs(Det) < UE_SMALL_NUMBER)
		{
			continue;
		}

		const double InvDet = 1.0 / Det;
		const FVector S = LocalPosition - A;
		const double U = InvDet * FVector::DotProduct(S, H);

		if (U < 0.0 || U > 1.0)
		{
			continue;
		}

		const FVector Q = FVector::CrossProduct(S, Edge1);
		const double V = InvDet * FVector::DotProduct(RayDir, Q);

		if (V < 0.0 || U + V > 1.0)
		{
			continue;
		}

		const double T = InvDet * FVector::DotProduct(Edge2, Q);
		if (T > UE_SMALL_NUMBER)
		{
			CrossingCount++;
		}
	}

	// Odd number of crossings = inside (negative), even = outside (positive)
	const float Sign = (CrossingCount % 2 == 1) ? -1.0f : 1.0f;
	return Sign * MinDist;
}

float AMeltableSurface::SampleField(float X, float Y, float Z) const
{
	const FVector LocalPosition(X, Y, Z);
	const FVector GridPosition = (LocalPosition - LocalFieldBounds.Min) / VoxelSize;

	const int32 X0 = FMath::FloorToInt(GridPosition.X);
	const int32 Y0 = FMath::FloorToInt(GridPosition.Y);
	const int32 Z0 = FMath::FloorToInt(GridPosition.Z);
	const int32 X1 = X0 + 1;
	const int32 Y1 = Y0 + 1;
	const int32 Z1 = Z0 + 1;

	if (!IsValidFieldCoordinate(X0, Y0, Z0) || !IsValidFieldCoordinate(X1, Y1, Z1))
	{
		return FMath::Max(VoxelSize, ComputeMeshSignedDistance(LocalPosition));
	}

	const float Tx = FMath::Clamp(GridPosition.X - X0, 0.0, 1.0);
	const float Ty = FMath::Clamp(GridPosition.Y - Y0, 0.0, 1.0);
	const float Tz = FMath::Clamp(GridPosition.Z - Z0, 0.0, 1.0);

	const auto ValueAt = [this](int32 SX, int32 SY, int32 SZ)
	{
		return ScalarField[GetFieldIndex(SX, SY, SZ)];
	};

	const float C00 = FMath::Lerp(ValueAt(X0, Y0, Z0), ValueAt(X1, Y0, Z0), Tx);
	const float C10 = FMath::Lerp(ValueAt(X0, Y1, Z0), ValueAt(X1, Y1, Z0), Tx);
	const float C01 = FMath::Lerp(ValueAt(X0, Y0, Z1), ValueAt(X1, Y0, Z1), Tx);
	const float C11 = FMath::Lerp(ValueAt(X0, Y1, Z1), ValueAt(X1, Y1, Z1), Tx);
	const float C0 = FMath::Lerp(C00, C10, Ty);
	const float C1 = FMath::Lerp(C01, C11, Ty);
	return FMath::Lerp(C0, C1, Tz);
}

int32 AMeltableSurface::GetFieldIndex(int32 X, int32 Y, int32 Z) const
{
	return X + (Y * CornerCounts.X) + (Z * CornerCounts.X * CornerCounts.Y);
}

bool AMeltableSurface::IsValidFieldCoordinate(int32 X, int32 Y, int32 Z) const
{
	return X >= 0 && Y >= 0 && Z >= 0 &&
		X < CornerCounts.X && Y < CornerCounts.Y && Z < CornerCounts.Z;
}

void AMeltableSurface::MarkDirtyChunksInBounds(const FBox& LocalBounds)
{
	const int32 ChunkSize = FMath::Max(1, ChunkVoxelSize);
	const FIntVector MinCell(
		FMath::Clamp(FMath::FloorToInt((LocalBounds.Min.X - LocalFieldBounds.Min.X) / VoxelSize), 0, CellCounts.X - 1),
		FMath::Clamp(FMath::FloorToInt((LocalBounds.Min.Y - LocalFieldBounds.Min.Y) / VoxelSize), 0, CellCounts.Y - 1),
		FMath::Clamp(FMath::FloorToInt((LocalBounds.Min.Z - LocalFieldBounds.Min.Z) / VoxelSize), 0, CellCounts.Z - 1)
	);
	const FIntVector MaxCell(
		FMath::Clamp(FMath::CeilToInt((LocalBounds.Max.X - LocalFieldBounds.Min.X) / VoxelSize), 0, CellCounts.X - 1),
		FMath::Clamp(FMath::CeilToInt((LocalBounds.Max.Y - LocalFieldBounds.Min.Y) / VoxelSize), 0, CellCounts.Y - 1),
		FMath::Clamp(FMath::CeilToInt((LocalBounds.Max.Z - LocalFieldBounds.Min.Z) / VoxelSize), 0, CellCounts.Z - 1)
	);

	for (int32 Z = MinCell.Z / ChunkSize; Z <= MaxCell.Z / ChunkSize; ++Z)
	{
		for (int32 Y = MinCell.Y / ChunkSize; Y <= MaxCell.Y / ChunkSize; ++Y)
		{
			for (int32 X = MinCell.X / ChunkSize; X <= MaxCell.X / ChunkSize; ++X)
			{
				DirtyChunks.Add(FIntVector(X, Y, Z));
			}
		}
	}
}

void AMeltableSurface::MarkAllChunksDirty()
{
	DirtyChunks.Reset();

	const int32 ChunkSize = FMath::Max(1, ChunkVoxelSize);
	const FIntVector ChunkCounts(
		FMath::Max(1, FMath::DivideAndRoundUp(CellCounts.X, ChunkSize)),
		FMath::Max(1, FMath::DivideAndRoundUp(CellCounts.Y, ChunkSize)),
		FMath::Max(1, FMath::DivideAndRoundUp(CellCounts.Z, ChunkSize))
	);

	for (int32 Z = 0; Z < ChunkCounts.Z; ++Z)
	{
		for (int32 Y = 0; Y < ChunkCounts.Y; ++Y)
		{
			for (int32 X = 0; X < ChunkCounts.X; ++X)
			{
				DirtyChunks.Add(FIntVector(X, Y, Z));
			}
		}
	}
}
