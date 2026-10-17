#pragma once

#include "CoreMinimal.h"
#include "RokHudAnimation.h"

/** UA_IFSearchRes_On/Off: pl_btns anchored Y and pl_Pop XY scale.
 * No alpha track. Off only restores the dock position and calls close at frame 1. */
struct FRokResourceSearchAnimation
{
	static constexpr float OpenDuration = 17.0f / 60.0f;
	static constexpr float CloseDuration = 1.0f / 60.0f;
	float Elapsed = 0;
	float PopupScale = 1;
	float DockTranslationY = 0;
	bool bAnimating = false;
	bool bClosing = false;
	bool IsClosing() const { return bAnimating && bClosing; }
	void Reset() { *this = FRokResourceSearchAnimation(); }
	void Open() { Reset(); bAnimating = true; Advance(0); }
	void Close() { if (IsClosing()) return; Elapsed = 0; bClosing = bAnimating = true; DockTranslationY = 0; }
	bool Advance(float DeltaSeconds)
	{
		if (!bAnimating) return false;
		Elapsed = FMath::Min(Elapsed + FMath::Max(0.0f, DeltaSeconds), bClosing ? CloseDuration : OpenDuration);
		if (!bClosing)
		{
			const float SourceY = Elapsed <= 10.0f / 60.0f
				? FMath::Lerp(-130.0f, 50.0f, RokHudAnimation::Smooth(Elapsed, 10.0f / 60.0f))
				: FMath::Lerp(50.0f, 15.0f, RokHudAnimation::Smooth(Elapsed - 10.0f / 60.0f, 7.0f / 60.0f));
			DockTranslationY = 15.0f - SourceY; // Unity up -> UMG down, relative to settled layout.
			PopupScale = RokHudAnimation::Smooth(Elapsed - 8.0f / 60.0f, 7.0f / 60.0f);
		}
		if (Elapsed >= (bClosing ? CloseDuration : OpenDuration))
		{
			bAnimating = false;
			return bClosing;
		}
		return false;
	}
};
