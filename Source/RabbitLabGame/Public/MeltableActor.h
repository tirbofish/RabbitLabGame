// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SurfaceNetsBlueprintLibrary.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "MeltableActor.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Cached UV/material lookup for one surface-nets cell, keyed by the cell's grid index. */
struct FMeltableCachedVertexAttributes
{
	FVector Position = FVector::ZeroVector;
	FVector2D UV = FVector2D::ZeroVector;
	int32 MaterialIndex = 0;
};

/** Precomputed local-space scalar field (+ optional mesh) cooked into the actor for instant runtime swap. */
USTRUCT(BlueprintType)
struct RABBITLABGAME_API FMeltableScalarFieldBake
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	TSoftObjectPtr<UStaticMesh> SourceMesh;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	FVector SourceRelativeScale = FVector::OneVector;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	float TargetCellSize = 10.0f;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	int32 MaxVoxelsPerAxis = 128;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	bool bAddOuterVoxelPadding = true;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	float AutoFitGridPadding = 10.0f;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	float Isovalue = 0.0f;

	UPROPERTY(VisibleAnywhere, Category="Meltable|Bake")
	FSurfaceNetsGrid Grid;

	UPROPERTY()
	TArray<float> Values;

	UPROPERTY()
	TArray<FVector> MeshVertices;

	UPROPERTY()
	TArray<int32> MeshTriangles;

	bool HasValues() const
	{
		return Values.Num() > 0;
	}

	bool HasMesh() const
	{
		return MeshVertices.Num() > 0 && MeshTriangles.Num() >= 3;
	}
};

UCLASS()
class RABBITLABGAME_API AMeltableActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMeltableActor();

	void DrawMeltCollisionDebug(const FVector& CollisionLocation, const FVector& CollisionNormal, float MeltRadius) const;
	void ApplyMeltCrater(const FVector& CollisionLocation, const FVector& CollisionNormal, float MeltRadius, float MeltAmount);

	/** Builds the surface-nets melt mesh if it has not been built yet. Safe to call multiple times. */
	UFUNCTION(BlueprintCallable, Category="Meltable|Surface Nets")
	void EnsureMeltRepresentation();

	/** Rebuilds and stores the scalar-field bake used for instant runtime conversion. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Meltable|Bake")
	void BakeScalarFieldCache();

	/** True once melting has modified this actor's shape at least once. */
	UFUNCTION(BlueprintPure, Category="Meltable|Melting")
	bool HasBeenMelted() const;

	/** True when melting has removed the entire generated mesh. */
	UFUNCTION(BlueprintPure, Category="Meltable|Melting")
	bool IsFullyMelted() const;

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Meltable")
	TObjectPtr<UStaticMeshComponent> SourceMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Meltable")
	TObjectPtr<UProceduralMeshComponent> GeneratedMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	FSurfaceNetsGrid SurfaceNetsGrid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	float SurfaceNetsIsovalue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bAutoFitGridToSourceMesh = true;

	/** Keep the authored static mesh until the first melt so fine holes/materials stay intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bDeferConversionUntilMelt = true;

	/** Desired voxel edge length in mesh-local cm when auto-fitting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets", meta=(ClampMin="1.0", UIMin="1.0", EditCondition="bAutoFitGridToSourceMesh"))
	float TargetCellSize = 10.0f;

	/** Caps voxels per axis so huge stretched actors cannot freeze baking / first melt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets", meta=(ClampMin="8", UIMin="8", ClampMax="256", UIMax="256", EditCondition="bAutoFitGridToSourceMesh"))
	int32 MaxVoxelsPerAxis = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bAddOuterVoxelPadding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoFitGridPadding = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bHideSourceMeshAfterConversion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bFlipGeneratedTriangleWinding = true;

	/** When true, editor/cook saves refresh the bake if the mesh or grid settings changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Bake")
	bool bBakeScalarFieldOnSave = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Meltable|Bake")
	FMeltableScalarFieldBake ScalarFieldBake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Collision")
	bool bEnableGeneratedMeshCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Melting", meta=(ClampMin="0.0", UIMin="0.0"))
	float MeltRegenerationInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Melting")
	bool bMeltThroughSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Melting", meta=(ClampMin="1.0", UIMin="1.0"))
	float MeltThroughDepthMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Debug", meta=(ClampMin="0.0", UIMin="0.0"))
	float DebugCollisionDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Debug", meta=(ClampMin="0.0", UIMin="0.0"))
	float DebugCollisionLineThickness = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category="Meltable|Surface Nets")
	FSurfaceNetsMesh SurfaceNetsMesh;

private:
	void AutoFitSurfaceNetsGridToSourceMesh();
	void BuildScalarFieldFromStaticMesh(TArray<float>& OutScalarFieldValues);
	void DisableSourceMeshAfterConversion();
	void EnsureSupportCollisionDuringConversion();
	void ClearSupportCollision();
	float GetMeltThroughDepth(const FVector& LocalSurfaceNormal, float LocalMeltRadius) const;
	void QueueMeltRegeneration();
	void RegeneratePendingMelt();
	bool RegenerateSurfaceNetsMesh();
	void UpdateGeneratedMesh();
	bool IsScalarFieldBakeValid() const;
	bool TryLoadScalarFieldBake();
	void StoreScalarFieldBake(const FSurfaceNetsMesh& BakedMesh);

	UPROPERTY()
	TArray<float> ScalarFieldValues;

	UPROPERTY()
	TObjectPtr<UBoxComponent> SupportCollisionComponent;

	bool bHasBeenMelted = false;
	bool bHasMeltRepresentation = false;

	/** Reused across melt regenerations so only vertices that moved re-run the closest-triangle search. */
	TMap<int32, FMeltableCachedVertexAttributes> VertexAttributeCache;

	FTimerHandle MeltRegenerationTimerHandle;
	bool bMeltRegenerationPending = false;
	double LastMeltRegenerationTime = -1.0;
};
