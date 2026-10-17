#pragma once

#include "CoreMinimal.h"

/** Source: UIAnimation/UA_MainIF_HotTask_On/Off and Rotation_ROn/ROff.
 * Unity zero-tangent keys use cubic Hermite interpolation, not linear lerp. */
namespace RokHudAnimation
{
	// UA_InterfaceChat.anim: alpha 0 -> 1 -> 0, zero tangents, one-second loop.
	inline float ChatUnreadAlpha(float Seconds)
	{
		const float Phase = FMath::Fmod(FMath::Max(0.0f, Seconds), 1.0f);
		const float T = Phase <= 0.5f ? Phase * 2.0f : (1.0f - Phase) * 2.0f;
		return T * T * (3.0f - 2.0f * T);
	}
	constexpr float TaskDuration = 0.33333334f;
	constexpr float FeatureDuration = 0.25f;
	constexpr float FeatureTravel = 440.5f;
	constexpr float RewardDuration = 1.5f;

	inline float RewardPulse(float Time)
	{
		// UA_TaskBox: 1 -> 1.2 -> 1 -> 1.2 -> 1, then rest.
		const float Phase = FMath::Fmod(FMath::Max(Time, 0.0f), RewardDuration) * 6.0f;
		if (Phase >= 4.0f) return 0.0f;
		const int32 Segment = FMath::FloorToInt(Phase);
		const float T = Phase - Segment;
		const float Value = T * T * (3.0f - 2.0f * T);
		return Segment % 2 == 0 ? Value : 1.0f - Value;
	}

	inline float Smooth(float Time, float Duration)
	{
		const float T = FMath::Clamp(Time / Duration, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	inline float FeatureOpenOffset(float Time)
	{
		// ROn's extra key at frame 2 has a nonzero tangent. Preserve it.
		constexpr float MidTime = 0.033333335f;
		constexpr float MidValue = 425.9f;
		constexpr float MidSlope = -233.6001f;
		if (Time <= MidTime)
		{
			return FMath::CubicInterp(FeatureTravel, 0.0f, MidValue,
				MidSlope * MidTime, FMath::Clamp(Time / MidTime, 0.0f, 1.0f));
		}
		const float Duration = FeatureDuration - MidTime;
		return FMath::CubicInterp(MidValue, MidSlope * Duration, 0.0f, 0.0f,
			FMath::Clamp((Time - MidTime) / Duration, 0.0f, 1.0f));
	}
}
