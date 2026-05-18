#pragma once

#include "PlayerMatterState.h"

class LiquidState: public IPlayerState
{
public:
	virtual void SwitchToState() override;
	virtual void EnterState() override;
	virtual void ExitState() override;
	virtual void UpdateState(float DeltaTime) override;
	
	virtual void ApplyVisuals() override;
};