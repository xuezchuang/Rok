#pragma once

#include "CoreMinimal.h"

namespace RokCommanderRosterAnimation
{
	constexpr float Duration = 0.33333334f;
	struct FSample { float X; float Opacity; };
	inline FSample Evaluate(float Seconds)
	{
		// UA_IFCaptain_Right.controller/OpenNoArb -> UA_IFCaptain_Left_On.anim.
		// Unweighted zero-tangent keys: hold until 5/60, arrive at 19/60,
		// alpha 1.1 settles to 1 at 20/60. Unity CanvasGroup alpha clamps to 1.
		const float T = FMath::Clamp((Seconds - 0.083333336f) / (0.31666666f - 0.083333336f), 0.0f, 1.0f);
		const float S = T * T * (3.0f - 2.0f * T);
		return { FMath::Lerp(-200.0f, 0.0f, S), FMath::Min(1.0f, 1.1f * S) };
	}
}
