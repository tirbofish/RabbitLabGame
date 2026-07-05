#include "HealingComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerMatterState.h"

DEFINE_LOG_CATEGORY_STATIC(LogHealing, Log, All);

UHealingComponent::UHealingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHealingComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Owner->OnActorBeginOverlap.AddDynamic(this, &UHealingComponent::HandleOwnerBeginOverlap);
	Owner->OnActorEndOverlap.AddDynamic(this, &UHealingComponent::HandleOwnerEndOverlap);
	Owner->OnActorHit.AddDynamic(this, &UHealingComponent::HandleOwnerHit);

	// Diagnostics: warn about collision setups that will never produce events.
	bool bAnyOverlapCapable = false;
	bool bAnyCollisionEnabled = false;
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			bAnyCollisionEnabled = true;
			if (Primitive->GetGenerateOverlapEvents()
				&& Primitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap)
			{
				bAnyOverlapCapable = true;
			}
		}
	}

	if (!bAnyCollisionEnabled)
	{
		UE_LOG(LogHealing, Warning, TEXT("%s: owner '%s' has no collision-enabled primitive components; healing will never trigger."), *GetName(), *Owner->GetName());
	}
	else if (!bAnyOverlapCapable)
	{
		UE_LOG(LogHealing, Warning, TEXT("%s: owner '%s' has no component that overlaps Pawns with 'Generate Overlap Events' enabled. Only blocking-hit healing (while the player pushes against it) will work."), *GetName(), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogHealing, Log, TEXT("%s: bound to '%s', overlap healing ready."), *GetName(), *Owner->GetName());
	}

	// Catch the case where the player already overlaps us when play starts.
	if (APlayerMatterState* Player = Cast<APlayerMatterState>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (Owner->IsOverlappingActor(Player))
		{
			HandleOwnerBeginOverlap(Owner, Player);
		}
	}
}

void UHealingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnActorBeginOverlap.RemoveDynamic(this, &UHealingComponent::HandleOwnerBeginOverlap);
		Owner->OnActorEndOverlap.RemoveDynamic(this, &UHealingComponent::HandleOwnerEndOverlap);
		Owner->OnActorHit.RemoveDynamic(this, &UHealingComponent::HandleOwnerHit);
	}

	Super::EndPlay(EndPlayReason);
}

void UHealingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (APlayerMatterState* Player = OverlappingPlayer.Get())
	{
		RestorePlayerHealth(Player, DeltaTime);
	}
	else
	{
		SetComponentTickEnabled(false);
	}
}

void UHealingComponent::HandleOwnerBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (APlayerMatterState* Player = Cast<APlayerMatterState>(OtherActor))
	{
		if (Player->GetHealthPercent() >= 1.0f)
		{
			UE_LOG(LogHealing, Log, TEXT("%s: player began overlap but health is already full; nothing to restore."), *GetName());
		}
		else
		{
			UE_LOG(LogHealing, Log, TEXT("%s: player began overlap, healing at %.1f health/s."), *GetName(), HealthRestorePerSecond);
		}
		OverlappingPlayer = Player;
		SetComponentTickEnabled(true);
	}
}

void UHealingComponent::HandleOwnerEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor && OtherActor == OverlappingPlayer.Get())
	{
		UE_LOG(LogHealing, Log, TEXT("%s: player ended overlap, healing stopped."), *GetName());
		OverlappingPlayer = nullptr;
		SetComponentTickEnabled(false);
	}
}

void UHealingComponent::HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	APlayerMatterState* Player = Cast<APlayerMatterState>(OtherActor);
	if (!Player)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Hit events can fire multiple times per frame; only restore once per frame.
	const float CurrentTime = World->GetTimeSeconds();
	if (FMath::IsNearlyEqual(CurrentTime, LastHitRestoreTime))
	{
		return;
	}
	LastHitRestoreTime = CurrentTime;

	RestorePlayerHealth(Player, World->GetDeltaSeconds());
}

void UHealingComponent::RestorePlayerHealth(APlayerMatterState* Player, float DeltaTime)
{
	if (HealthRestorePerSecond <= 0.0f || DeltaTime <= 0.0f)
	{
		return;
	}

	if (Player->GetHealthPercent() >= 1.0f)
	{
		UE_LOG(LogHealing, Verbose, TEXT("%s: player health already full."), *GetName());
		return;
	}

	Player->RestoreHealth(HealthRestorePerSecond * DeltaTime);
}
