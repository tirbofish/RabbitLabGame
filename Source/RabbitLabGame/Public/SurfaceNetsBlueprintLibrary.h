#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SurfaceNetsBlueprintLibrary.generated.h"

/**
 * Describes the regular voxel grid used by surface nets.
 *
 * Scalar field values are sampled at grid points, so each axis has one more scalar sample than
 * voxel count. For example, 16 voxels in X require 17 scalar values along X.
 */
USTRUCT(BlueprintType)
struct RABBITLABGAME_API FSurfaceNetsGrid
{
	GENERATED_BODY()

	/** World-space position of the grid point at X=0, Y=0, Z=0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface Nets")
	FVector Origin = FVector::ZeroVector;

	/** World-space distance between neighboring scalar samples on each axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface Nets", meta=(ClampMin="0.0001", UIMin="0.0001"))
	FVector CellSize = FVector::OneVector;

	/** Number of voxels on the X axis. The scalar field needs VoxelCountX + 1 samples on this axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface Nets", meta=(ClampMin="1", UIMin="1"))
	int32 VoxelCountX = 16;

	/** Number of voxels on the Y axis. The scalar field needs VoxelCountY + 1 samples on this axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface Nets", meta=(ClampMin="1", UIMin="1"))
	int32 VoxelCountY = 16;

	/** Number of voxels on the Z axis. The scalar field needs VoxelCountZ + 1 samples on this axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface Nets", meta=(ClampMin="1", UIMin="1"))
	int32 VoxelCountZ = 16;
};

/** Surface nets output mesh data using Unreal-friendly arrays. */
USTRUCT(BlueprintType)
struct RABBITLABGAME_API FSurfaceNetsMesh
{
	GENERATED_BODY()

	/** Generated mesh vertices in world-space grid coordinates. */
	UPROPERTY(BlueprintReadOnly, Category="Surface Nets")
	TArray<FVector> Vertices;

	/** Flat triangle index buffer. Every three indices form one triangle into Vertices. */
	UPROPERTY(BlueprintReadOnly, Category="Surface Nets")
	TArray<int32> Triangles;
};

/** Blueprint-callable helpers for building meshes from scalar fields with the surface nets algorithm. */
UCLASS()
class RABBITLABGAME_API USurfaceNetsBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the flat array index for a scalar sample at grid point X, Y, Z.
	 *
	 * The scalar field uses X-fastest order:
	 * Index = X + Y * (VoxelCountX + 1) + Z * (VoxelCountX + 1) * (VoxelCountY + 1).
	 */
	UFUNCTION(BlueprintPure, Category="Surface Nets")
	static int32 GetSurfaceNetsScalarFieldIndex(
		int32 X,
		int32 Y,
		int32 Z,
		const FSurfaceNetsGrid& Grid
	);

	/** Returns the exact number of scalar values required by Grid. */
	UFUNCTION(BlueprintPure, Category="Surface Nets")
	static int32 GetSurfaceNetsScalarFieldValueCount(const FSurfaceNetsGrid& Grid);

	/**
	 * Generates a triangle mesh from scalar field values using surface nets.
	 *
	 * ScalarFieldValues must contain GetSurfaceNetsScalarFieldValueCount(Grid) values.
	 * Values below Isovalue are treated as one side of the surface, and values greater than or
	 * equal to Isovalue are treated as the other side. Returns false and clears Mesh when the grid
	 * or scalar field size is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category="Surface Nets")
	static bool GenerateSurfaceNetsMesh(
		const FSurfaceNetsGrid& Grid,
		const TArray<float>& ScalarFieldValues,
		FSurfaceNetsMesh& Mesh,
		float Isovalue = 0.0f
	);

	/**
	 * Generates a triangle mesh from the connected surface region near Hint.
	 *
	 * This is faster when Hint is close to the surface, but it can miss disconnected regions.
	 * If the initial search grows to MaxBreadthFirstSearchQueueSize, it falls back to the full
	 * surface nets pass. Returns false and clears Mesh when the grid, scalar field size, or queue
	 * limit is invalid.
	 */
	UFUNCTION(BlueprintCallable, Category="Surface Nets")
	static bool GenerateSurfaceNetsMeshWithHint(
		const FSurfaceNetsGrid& Grid,
		const TArray<float>& ScalarFieldValues,
		FVector Hint,
		FSurfaceNetsMesh& Mesh,
		float Isovalue = 0.0f,
		int32 MaxBreadthFirstSearchQueueSize = 32768
	);

	/** Returns true when voxel counts are positive and all cell size components are greater than zero. */
	UFUNCTION(BlueprintPure, Category="Surface Nets")
	static bool IsSurfaceNetsGridValid(const FSurfaceNetsGrid& Grid);
};
