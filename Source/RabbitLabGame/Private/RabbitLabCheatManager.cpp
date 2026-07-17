#include "RabbitLabCheatManager.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogRabbitLabCheatManager, Log, All);

bool URabbitLabCheatManager::bMeltableDebugEnabled = false;

void URabbitLabCheatManager::SpawnInLevel(const FString& LevelName)
{
	FString PackageName = LevelName;
	PackageName.TrimStartAndEndInline();
	PackageName.RemoveFromEnd(TEXT(".umap"), ESearchCase::IgnoreCase);

	if (PackageName.IsEmpty())
	{
		UE_LOG(
			LogRabbitLabCheatManager,
			Warning,
			TEXT("SpawnInLevel requires a map name, for example: SpawnInLevel Level2")
		);
		return;
	}

	if (FPackageName::IsShortPackageName(PackageName))
	{
		PackageName = FString::Printf(TEXT("/Game/%s"), *PackageName);
	}

	if (!FPackageName::IsValidLongPackageName(PackageName) || !FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(
			LogRabbitLabCheatManager,
			Warning,
			TEXT("SpawnInLevel could not find map '%s'. Use a map name such as Level2 or /Game/Level2."),
			*PackageName
		);
		return;
	}

	UE_LOG(LogRabbitLabCheatManager, Log, TEXT("Travelling to level '%s'"), *PackageName);
	UGameplayStatics::OpenLevel(GetPlayerController(), FName(*PackageName));
}

void URabbitLabCheatManager::EnableMeltableDebug()
{
	SetMeltableDebug(true);
}

void URabbitLabCheatManager::DisableMeltableDebug()
{
	SetMeltableDebug(false);
}

void URabbitLabCheatManager::ToggleMeltableDebug()
{
	SetMeltableDebug(!bMeltableDebugEnabled);
}

void URabbitLabCheatManager::SetMeltableDebug(bool bEnabled)
{
	bMeltableDebugEnabled = bEnabled;

	UE_LOG(
		LogRabbitLabCheatManager,
		Log,
		TEXT("Meltable debug %s"),
		bMeltableDebugEnabled ? TEXT("enabled") : TEXT("disabled")
	);
}

bool URabbitLabCheatManager::IsMeltableDebugEnabled()
{
	return bMeltableDebugEnabled;
}
