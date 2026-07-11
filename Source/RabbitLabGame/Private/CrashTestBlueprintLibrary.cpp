#include "CrashTestBlueprintLibrary.h"

void UCrashTestBlueprintLibrary::CrashWithDescription(const FString& Description)
{
	UE_LOG(LogTemp, Fatal, TEXT("Intentional crash requested: %s"), *Description);
}
