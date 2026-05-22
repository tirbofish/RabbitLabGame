#include "RabbitLabCheatManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRabbitLabCheatManager, Log, All);

bool URabbitLabCheatManager::bMeltableDebugEnabled = false;

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
