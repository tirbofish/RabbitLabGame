// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SurfaceNetsBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "MeltableActor.generated.h"

class UProceduralMeshComponent;

/** Cached UV/material lookup for one surface-nets cell, keyed by the cell's grid index. */
struct FMeltableCachedVertexAttributes
{
	FVector Position = FVector::ZeroVector;
	FVector2D UV = FVector2D::ZeroVector;
	int32 MaterialIndex = 0;
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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bAddOuterVoxelPadding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoFitGridPadding = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bHideSourceMeshAfterConversion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Surface Nets")
	bool bFlipGeneratedTriangleWinding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Collision")
	bool bEnableGeneratedMeshCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Meltable|Melting", meta=(ClampMin="0.0", UIMin="0.0"))
	float MeltRegenerationInterval = 0.05f;

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
	float GetMeltThroughDepth(const FVector& SurfaceNormal, float MeltRadius) const;
	bool RegenerateSurfaceNetsMesh();
	void UpdateGeneratedMesh();

	UPROPERTY()
	TArray<float> ScalarFieldValues;

	/** Reused across melt regenerations so only vertices that moved re-run the closest-triangle search. */
	TMap<int32, FMeltableCachedVertexAttributes> VertexAttributeCache;

	double LastMeltRegenerationTime = -1.0;
};
