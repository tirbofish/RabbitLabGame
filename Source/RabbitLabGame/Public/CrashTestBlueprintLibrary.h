#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CrashTestBlueprintLibrary.generated.h"

/**
 * Developer utilities for intentionally crashing the game, e.g. to test the
 * crash reporter or error-handling flows.
 */
UCLASS()
class RABBITLABGAME_API UCrashTestBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Immediately crashes the game with a fatal error containing the given
	 * description. The description appears in the log and crash report.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crash Test", meta = (Keywords = "crash exception fatal assert"))
	static void CrashWithDescription(const FString& Description);
};
