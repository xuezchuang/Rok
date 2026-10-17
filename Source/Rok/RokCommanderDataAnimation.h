#pragma once

#include "CoreMinimal.h"

namespace RokCommanderDataAnimation
{
	// UI_Item_CaptainData_SubView.Open, non-Arabic branch:
	// UA_IFCaptain_Left.controller/OpenNoArb -> UA_IFCaptain_Right_On.anim.
	// The current Chinese runtime uses this source direction; RTL layout is not
	// implemented here. Both curves are unweighted, with zero tangent ramps.
	constexpr float Duration = 0.33333334f;
	struct FSample { float X; float Opacity; };
	inline FSample Evaluate(float Seconds)
	{
		const float T = FMath::Clamp((Seconds - 0.083333336f) / (0.31666666f - 0.083333336f), 0.0f, 1.0f);
		const float S = T * T * (3.0f - 2.0f * T);
		// Source alpha peaks at 1.1 then settles to 1; CanvasGroup clamps it.
		return { FMath::Lerp(200.0f, 0.0f, S), FMath::Min(1.0f, 1.1f * S) };
	}
}
