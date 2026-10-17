#pragma once

#include "CoreMinimal.h"

namespace RokArmyShowPanelAnimation
{
	// UI_Win_ArmyShowMediator.ShowList / ShowArmys:
	// UA_BuildingDesc_LevelShow (+50) / ContentShow (-50), root alpha 0->1.
	constexpr float Duration = 0.25f;
	struct FSample { float X; float Opacity; };
	inline FSample Evaluate(float Seconds, bool bDescription)
	{
		const float T = FMath::Clamp(Seconds / Duration, 0.0f, 1.0f);
		const float S = T * T * (3.0f - 2.0f * T);
		return { (bDescription ? 50.0f : -50.0f) * (1.0f - S), S };
	}
}
