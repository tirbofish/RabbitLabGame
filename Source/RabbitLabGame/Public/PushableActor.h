#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PushableActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UBoxComponent;

UCLASS(Blueprintable)
class RABBITLABGAME_API APushableActor : public AActor
{
	GENERATED_BODY()

public:
	APushableActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Pushable")
	UStaticMeshComponent* GetPushableMesh() const { return PushableMeshComponent; }

	UFUNCTION(BlueprintPure, Category="Pushable")
	UBoxComponent* GetPushableCollision() const { return PushableCollisionComponent; }

	UFUNCTION(BlueprintPure, Category="Pushable")
	UStaticMesh* GetPushableStaticMesh() const { return PushableStaticMesh; }

	UFUNCTION(BlueprintCallable, Category="Pushable")
	void SetPushableStaticMesh(UStaticMesh* NewStaticMesh);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pushable")
	TObjectPtr<UBoxComponent> PushableCollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pushable")
	TObjectPtr<UStaticMeshComponent> PushableMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pushable", meta=(DisplayName="Static Mesh"))
	TObjectPtr<UStaticMesh> PushableStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pushable|Physics", meta=(ClampMin="0.1", UIMin="1.0"))
	float PushableMassKg = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pushable|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float LinearDamping = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pushable|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float AngularDamping = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pushable|Physics")
	bool bLockRotation = true;

private:
	void ApplyConfiguredMesh();
	void FitCollisionToMesh();
	void ConfigurePushableCollision();
	void ConfigurePushablePhysics();
};
