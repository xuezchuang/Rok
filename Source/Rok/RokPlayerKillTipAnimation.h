#pragma once
#include "CoreMinimal.h"

namespace RokPlayerKillTipAnimation
{
	// shared_uianimation/UA_TextTipAnime_On: XY 0 -> 1 at frame 7,
	// zero tangents, hold through frame 15; no opacity animation or overshoot.
	inline float Scale(float Seconds)
	{
		const float T = FMath::Clamp(Seconds / 0.11666667f, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}
	constexpr float Width = 372.0f; // UI_KillTip 332 + HelpTip.Frame.x 40
	constexpr float Height = 203.37f; // UI_KillTip 163.37 + Frame.y 40
}
