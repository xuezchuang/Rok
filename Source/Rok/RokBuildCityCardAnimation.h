#pragma once
#include "CoreMinimal.h"

namespace RokBuildCityCardAnimation
{
	inline float Scale(float Elapsed, int32 SourceIndex)
	{
		const float Time = Elapsed - 0.05f * (SourceIndex + 1);
		if (Time <= 0 || Time >= 1.0f / 3.0f) return 1;
		const bool bRising = Time < 7.0f / 60.0f;
		const float T = bRising ? Time / (7.0f / 60.0f) : (Time - 7.0f / 60.0f) / (13.0f / 60.0f);
		const float A = T * T * (3 - 2 * T);
		return bRising ? FMath::Lerp(1.0f, 1.2f, A) : FMath::Lerp(1.2f, 1.0f, A);
	}
}
