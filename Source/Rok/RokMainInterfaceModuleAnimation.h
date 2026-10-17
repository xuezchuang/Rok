#pragma once

#include "CoreMinimal.h"

// OpenLodUi.csv Map/unArbAnimator -> UA_MainIF_Right_On/Off.anim.
// All source tangents are zero: Hermite interpolation is smoothstep.
namespace RokMainInterfaceModuleAnimation
{
// Chinese HotTask/unArbAnimator -> UA_MainIF_Left_On/Off.anim.
inline FVector2D EvaluateLeft(bool bOpening, float Seconds)
{
	const float T = FMath::IsFinite(Seconds) ? FMath::Clamp(Seconds, 0.0f, 0.25f) : 0.25f;
	const float Split = bOpening ? 0.15f : 0.1f;
	const float Start = bOpening ? -80.0f : 0.0f;
	const float Middle = bOpening ? 20.0f : 30.0f;
	const float End = bOpening ? 0.0f : -300.0f;
	const float X = T <= Split
		? FMath::CubicInterp(Start, 0.0f, Middle, 0.0f, T / Split)
		: FMath::CubicInterp(Middle, 0.0f, End, 0.0f, (T - Split) / (0.25f - Split));
	const float Alpha = FMath::CubicInterp(0.0f, 0.0f, 1.0f, 0.0f, T / 0.25f);
	return FVector2D(X, bOpening ? Alpha : 1.0f - Alpha);
}

inline FVector2D EvaluateRight(bool bOpening, float Seconds)
{
	const float T = FMath::IsFinite(Seconds) ? FMath::Clamp(Seconds, 0.0f, 0.25f) : 0.25f;
	const float Split = bOpening ? 0.15f : 0.1f;
	const float Start = bOpening ? 100.0f : 0.0f;
	const float End = bOpening ? 0.0f : 100.0f;
	const float X = T <= Split
		? FMath::CubicInterp(Start, 0.0f, -20.0f, 0.0f, T / Split)
		: FMath::CubicInterp(-20.0f, 0.0f, End, 0.0f, (T - Split) / (0.25f - Split));
	const float Alpha = FMath::CubicInterp(0.0f, 0.0f, 1.0f, 0.0f, T / 0.25f);
	return FVector2D(X, bOpening ? Alpha : 1.0f - Alpha);
}
}
