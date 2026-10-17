#pragma once
#include "CoreMinimal.h"

namespace RokPowerActionTip
{
	constexpr float Width = 420.f; // UI_Pop_PowerTextTip 380 + HelpTip.Frame.x 40
	constexpr float Height = 280.f; // pl_content 240 + HelpTip.Frame.y 40
	constexpr float BodyWidth = 380.f;
	constexpr float FontPoints(float SourcePixels) { return SourcePixels * 72.f / 96.f; }
	// Shared UA_TextTipAnime_On: scale only; zero tangents, no opacity track.
	inline float Scale(float Seconds)
	{
		const float T = FMath::Clamp(Seconds / .11666667f, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
	constexpr float DescriptionY = 20.f;
	constexpr float DescriptionHeight = 38.050003f;
	constexpr float RecoveryY = 71.15f; // padding + 120 - 14.4 - 108.9 / 2
	constexpr float RecoveryHeight = 108.9f;
	constexpr float WarningY = 198.9f; // padding + 240 - 61.1
	constexpr float WarningHeight = 61.1f;
}
