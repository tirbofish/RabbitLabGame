// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeltableSurface.generated.h"

class UStaticMeshComponent;
class UDynamicMeshComponent;
class UStaticMesh;

namespace UE { namespace Geometry { class FDynamicMesh3; class TMeshAABBTree3; } }

UCLASS()
class RABBITLABGAME_API AMeltableSurface : public AActor
{
	GENERATED_BODY()
public:
	AMeltableSurface();
	
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void BeginPlay() override;
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meshes")
	TObjectPtr<UStaticMeshComponent> StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Physics")
	bool bEnablePhysicsSimulation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Physics")
	TEnumAsByte<ECollisionEnabled::Type> DynamicMeshCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Physics")
	TEnumAsByte<ECollisionChannel> DynamicMeshObjectType = ECC_WorldStatic;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Deformation")
	float DefaultCraterRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Deformation")
	float DefaultCraterDepth = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Deformation")
	float DefaultCraterRimHeight = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Deformation", meta=(ClampMin="0", ClampMax="5", UIMin="0", UIMax="4"))
	int32 InitialTessellationLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets", meta=(ClampMin="5.0", UIMin="5.0"))
	float VoxelSize = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets", meta=(ClampMin="4", UIMin="4"))
	int32 ChunkVoxelSize = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfacePadding = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets", meta=(ClampMin="1", UIMin="1"))
	int32 MaxDirtyChunksPerFrame = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets")
	bool bRegenerateCollisionAfterMelt = true;
	
public:
	UFUNCTION(BlueprintCallable)
	void ConvertStaticMeshToDynamicMesh();

	UFUNCTION(BlueprintCallable, Category="Meltable Surface")
	void SubdivideDynamicMesh();

	UFUNCTION(BlueprintCallable)
	void SetupDynamicMeshPhysics(bool bSimulatePhysics = false);

	UFUNCTION(BlueprintCallable, Category="Meltable Surface|Surface Nets")
	void RebuildSurfaceNets();

	UFUNCTION(BlueprintCallable, Category="Meltable Surface|Surface Nets")
	void ResetMeltField();
	
	UFUNCTION(BlueprintCallable, Category="Meltable Surface")
	void ApplySphericalCrater(
		FVector WorldHitLocation,
		float Radius = 100.0f
	);

	UFUNCTION(BlueprintCallable, Category="Meltable Surface")
	void ApplyAcidCrater(
		FVector WorldHitLocation,
		FVector WorldHitNormal,
		float Radius = 100.0f,
		float Depth = 30.0f,
		float RimHeight = 5.0f
	);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable Surface|Surface Nets")
	float CollisionRebuildCooldown = 0.1f;

private:
	bool InitializeVoxelField();
	float ComputeMeshSignedDistance(const FVector& LocalPosition) const;
	float SampleField(float X, float Y, float Z) const;
	int32 GetFieldIndex(int32 X, int32 Y, int32 Z) const;
	bool IsValidFieldCoordinate(int32 X, int32 Y, int32 Z) const;
	void MarkDirtyChunksInBounds(const FBox& LocalBounds);
	void MarkAllChunksDirty();

	FBox LocalFieldBounds = FBox(EForceInit::ForceInit);
	FIntVector CellCounts = FIntVector::ZeroValue;
	FIntVector CornerCounts = FIntVector::ZeroValue;
	TArray<float> ScalarField;
	TArray<float> InitialScalarField;
	TSet<FIntVector> DirtyChunks;
	bool bSurfaceNetsInitialized = false;
	float LastCollisionRebuildTime = -1.0f;
	bool bCollisionDirty = false;

	// Mesh SDF data: triangles from the original static mesh for distance queries
	TArray<FVector> MeshVertices;
	TArray<FIntVector> MeshTriangles;
	FBox MeshLocalBounds = FBox(EForceInit::ForceInit);
};
