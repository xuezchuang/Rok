#pragma once
#include "CoreMinimal.h"

namespace RokCivilizationAnimation
{
	constexpr float OutDuration = 1.0f / 6.0f;
	constexpr float ShowDuration = 0.5f;
	constexpr float SelectionDuration = 3.1333334f;
	struct FSelectionSample { float Scale; float Y; float Alpha; };
	inline FSelectionSample EvaluateSelection(float Time)
	{
		const float T = FMath::Fmod(FMath::Max(Time, 0.0f), SelectionDuration);
		const float U = T / SelectionDuration;
		const float Scale = FMath::CubicInterp(1.0038509f, 0.07131073f * SelectionDuration,
			1.2272912f, 0.07131073f * SelectionDuration, U);
		const float Y = FMath::CubicInterp(0.39077f, 7.2753925f * SelectionDuration,
			23.187f, 7.2753925f * SelectionDuration, U);
		const float Alpha = T < 0.4f
			? FMath::CubicInterp(0.0f, 0.0f, 1.0f, 2.5f * 0.4f, T / 0.4f)
			: FMath::CubicInterp(1.0f, -0.4632681f * (SelectionDuration - 0.4f),
				0.0f, 0.0f, (T - 0.4f) / (SelectionDuration - 0.4f));
		return {Scale, -Y, Alpha};
	}
	struct FSample { float HeroX; float HeroAlpha; float FlagY; float FlagAlpha; };
	inline FSample Evaluate(float Time, bool bSwitch)
	{
		// UA_CreateCharView_CharOut -> CharShow, zero-tangent curves.
		if (bSwitch && Time < OutDuration)
		{
			const float T = FMath::Clamp(Time / OutDuration, 0.0f, 1.0f);
			const float A = T * T * (3 - 2 * T);
			return {-200 * A, 0, -70 * A, 1 - A};
		}
		const float T = FMath::Clamp((Time - (bSwitch ? OutDuration : 0)) / ShowDuration, 0.0f, 1.0f);
		const float A = T * T * (3 - 2 * T);
		return {-200 * (1 - A), A, -70 * (1 - A), A};
	}
}
