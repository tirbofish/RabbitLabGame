#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LoadingBlueprintLibrary.generated.h"

/**
 * Blueprint helpers for monitoring async level/package loading.
 */
UCLASS()
class RABBITLABGAME_API ULoadingBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the async load progress of a package (e.g. a streaming level)
	 * as 0-100, or -1 if the package is not currently being async loaded.
	 *
	 * @param PackagePath Full package path, e.g. "/Game/Level1".
	 */
	UFUNCTION(BlueprintPure, Category = "Loading", meta = (Keywords = "loading progress percent stream level"))
	static float GetAsyncLoadPercent(FName PackagePath);
};
