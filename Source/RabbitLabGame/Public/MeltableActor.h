// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "SurfaceNetsBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "MeltableActor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class RABBITLABGAME_API AMeltableActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMeltableActor();

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

	UPROPERTY(BlueprintReadOnly, Category="Meltable|Surface Nets")
	FSurfaceNetsMesh SurfaceNetsMesh;

private:
	void AutoFitSurfaceNetsGridToSourceMesh();
	void BuildScalarFieldFromStaticMesh(TArray<float>& ScalarFieldValues);
	void UpdateGeneratedMesh();
};
