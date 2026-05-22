#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RabbitLabCheatManager.generated.h"

UCLASS()
class RABBITLABGAME_API URabbitLabCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
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
