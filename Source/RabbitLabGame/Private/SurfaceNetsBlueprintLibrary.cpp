#include "SurfaceNetsBlueprintLibrary.h"

#include "NaiveSurfaceNets/surface_nets.h"

namespace
{
isosurface::regular_grid_t ToSurfaceNetsGrid(const FSurfaceNetsGrid& Grid)
{
	return isosurface::regular_grid_t{
		static_cast<float>(Grid.Origin.X),
		static_cast<float>(Grid.Origin.Y),
		static_cast<float>(Grid.Origin.Z),
		static_cast<float>(Grid.CellSize.X),
		static_cast<float>(Grid.CellSize.Y),
		static_cast<float>(Grid.CellSize.Z),
		static_cast<std::size_t>(Grid.VoxelCountX),
		static_cast<std::size_t>(Grid.VoxelCountY),
		static_cast<std::size_t>(Grid.VoxelCountZ)
	};
}

void CopyMesh(const common::igl_triangle_mesh& Source, FSurfaceNetsMesh& Target)
{
	Target.Vertices.Reset(Source.V.size());
	Target.Triangles.Reset(Source.F.size() * 3);

	for (const std::array<double, 3>& Vertex : Source.V)
	{
		Target.Vertices.Add(FVector(Vertex[0], Vertex[1], Vertex[2]));
	}

	for (const std::array<int, 3>& Triangle : Source.F)
	{
		Target.Triangles.Add(Triangle[0]);
		Target.Triangles.Add(Triangle[1]);
		Target.Triangles.Add(Triangle[2]);
	}
}

bool HasValidScalarField(const FSurfaceNetsGrid& Grid, const TArray<float>& ScalarFieldValues)
{
	return USurfaceNetsBlueprintLibrary::IsSurfaceNetsGridValid(Grid) &&
		ScalarFieldValues.Num() == USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldValueCount(Grid);
}

std::function<float(float, float, float)> MakeScalarSampler(
	const FSurfaceNetsGrid& Grid,
	const TArray<float>& ScalarFieldValues)
{
	return [&Grid, &ScalarFieldValues](float X, float Y, float Z) -> float
	{
		const int32 GridX = FMath::Clamp(
			FMath::RoundToInt((X - static_cast<float>(Grid.Origin.X)) / static_cast<float>(Grid.CellSize.X)),
			0,
			Grid.VoxelCountX
		);
		const int32 GridY = FMath::Clamp(
			FMath::RoundToInt((Y - static_cast<float>(Grid.Origin.Y)) / static_cast<float>(Grid.CellSize.Y)),
			0,
			Grid.VoxelCountY
		);
		const int32 GridZ = FMath::Clamp(
			FMath::RoundToInt((Z - static_cast<float>(Grid.Origin.Z)) / static_cast<float>(Grid.CellSize.Z)),
			0,
			Grid.VoxelCountZ
		);

		const int32 ValueIndex = USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldIndex(
			GridX,
			GridY,
			GridZ,
			Grid
		);

		return ScalarFieldValues.IsValidIndex(ValueIndex) ? ScalarFieldValues[ValueIndex] : 0.0f;
	};
}
}

int32 USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldIndex(
	int32 X,
	int32 Y,
	int32 Z,
	const FSurfaceNetsGrid& Grid
)
{
	const int32 PointCountX = Grid.VoxelCountX + 1;
	const int32 PointCountY = Grid.VoxelCountY + 1;
	return X + (Y * PointCountX) + (Z * PointCountX * PointCountY);
}

int32 USurfaceNetsBlueprintLibrary::GetSurfaceNetsScalarFieldValueCount(const FSurfaceNetsGrid& Grid)
{
	return (Grid.VoxelCountX + 1) * (Grid.VoxelCountY + 1) * (Grid.VoxelCountZ + 1);
}

bool USurfaceNetsBlueprintLibrary::GenerateSurfaceNetsMesh(
	const FSurfaceNetsGrid& Grid,
	const TArray<float>& ScalarFieldValues,
	FSurfaceNetsMesh& Mesh,
	float Isovalue
)
{
	Mesh.Vertices.Reset();
	Mesh.Triangles.Reset();

	if (!HasValidScalarField(Grid, ScalarFieldValues))
	{
		return false;
	}

	const isosurface::regular_grid_t SurfaceNetsGrid = ToSurfaceNetsGrid(Grid);
	const common::igl_triangle_mesh SurfaceNetsMesh = isosurface::surface_nets(
		MakeScalarSampler(Grid, ScalarFieldValues),
		SurfaceNetsGrid,
		Isovalue
	);

	CopyMesh(SurfaceNetsMesh, Mesh);
	return true;
}

bool USurfaceNetsBlueprintLibrary::GenerateSurfaceNetsMeshWithHint(
	const FSurfaceNetsGrid& Grid,
	const TArray<float>& ScalarFieldValues,
	FVector Hint,
	FSurfaceNetsMesh& Mesh,
	float Isovalue,
	int32 MaxBreadthFirstSearchQueueSize
)
{
	Mesh.Vertices.Reset();
	Mesh.Triangles.Reset();

	if (!HasValidScalarField(Grid, ScalarFieldValues) || MaxBreadthFirstSearchQueueSize <= 0)
	{
		return false;
	}

	const isosurface::regular_grid_t SurfaceNetsGrid = ToSurfaceNetsGrid(Grid);
	const isosurface::point_t SurfaceNetsHint{
		static_cast<float>(Hint.X),
		static_cast<float>(Hint.Y),
		static_cast<float>(Hint.Z)
	};
	const common::igl_triangle_mesh SurfaceNetsMesh = isosurface::surface_nets(
		MakeScalarSampler(Grid, ScalarFieldValues),
		SurfaceNetsGrid,
		SurfaceNetsHint,
		Isovalue,
		static_cast<std::size_t>(MaxBreadthFirstSearchQueueSize)
	);

	CopyMesh(SurfaceNetsMesh, Mesh);
	return true;
}

bool USurfaceNetsBlueprintLibrary::IsSurfaceNetsGridValid(const FSurfaceNetsGrid& Grid)
{
	return Grid.VoxelCountX > 0 &&
		Grid.VoxelCountY > 0 &&
		Grid.VoxelCountZ > 0 &&
		Grid.CellSize.X > UE_SMALL_NUMBER &&
		Grid.CellSize.Y > UE_SMALL_NUMBER &&
		Grid.CellSize.Z > UE_SMALL_NUMBER;
}
