#pragma once

#include "CoreMinimal.h"

// operation_2009.prefab: three prewarmed continuous particles and one burst.
namespace RokWorldMapCityPulse
{
inline float Alpha(float Age, float Peak)
{
	if (Age < 0.0f || Age >= 1.0f) return 0.0f;
	return Age < Peak ? Age / Peak : (1.0f - Age) / (1.0f - Peak);
}

inline FVector2D Evaluate(double Seconds, int32 Particle)
{
	if (!FMath::IsFinite(Seconds) || Seconds < 0.0 || Particle < 0 || Particle > 3)
		return FVector2D::ZeroVector;
	if (Particle < 3)
	{
		const float Age = static_cast<float>((FMath::Fmod(Seconds, 1.0) + Particle) / 3.0);
		return FVector2D(1.0f, 0.3137255f * Alpha(Age, 32768.0f / 65535.0f));
	}
	const float Age = static_cast<float>(FMath::Fmod(Seconds, 2.2) / 1.5);
	if (Age >= 1.0f) return FVector2D::ZeroVector;
	const float Size = FMath::CubicInterp(0.49377325f, 1.1168576f, 1.0f, 0.0f, Age);
	return FVector2D(Size, 0.3137255f * Alpha(Age, 8288.0f / 65535.0f));
}
}
