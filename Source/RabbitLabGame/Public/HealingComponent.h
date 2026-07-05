#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealingComponent.generated.h"

class APlayerMatterState;

/**
 * Attach to any actor to make it restore the player's health while the
 * player is overlapping it, or on blocking hit contact.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName="Healing")
class RABBITLABGAME_API UHealingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealingComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Health restored per second of overlap/contact with the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Healing", meta=(ClampMin="0.0", UIMin="0.0"))
	float HealthRestorePerSecond = 25.0f;

	UFUNCTION()
	void HandleOwnerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void HandleOwnerEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

private:
	void RestorePlayerHealth(APlayerMatterState* Player, float DeltaTime);

	TWeakObjectPtr<APlayerMatterState> OverlappingPlayer;
	float LastHitRestoreTime = -1.0f;
};
