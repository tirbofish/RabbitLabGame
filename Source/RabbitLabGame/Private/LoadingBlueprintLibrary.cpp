#include "LoadingBlueprintLibrary.h"

#include "UObject/UObjectGlobals.h"

float ULoadingBlueprintLibrary::GetAsyncLoadPercent(const FName PackagePath)
{
	return GetAsyncLoadPercentage(PackagePath);
}
