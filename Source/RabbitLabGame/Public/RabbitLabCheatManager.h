#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RabbitLabCheatManager.generated.h"

UCLASS()
class RABBITLABGAME_API URabbitLabCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** Opens the requested map and spawns the player at that map's PlayerStart. */
	UFUNCTION(exec)
	void SpawnInLevel(const FString& LevelName);

	UFUNCTION(exec)
	void EnableMeltableDebug();

	UFUNCTION(exec)
	void DisableMeltableDebug();

	UFUNCTION(exec)
	void ToggleMeltableDebug();

	UFUNCTION(exec)
	void SetMeltableDebug(bool bEnabled);

	static bool IsMeltableDebugEnabled();

private:
	static bool bMeltableDebugEnabled;
};
