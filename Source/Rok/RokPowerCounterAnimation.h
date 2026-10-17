#pragma once

#include "CoreMinimal.h"

/** UI_Item_PlayerPowerInfo_SubView.SetSequence: 20 callbacks spaced by
 * 0.06s, with OutCirc applied to the sequence timeline, then exact target.
 * Presentation only. Retargeting cancels the old display sequence so stale
 * callbacks cannot overwrite a newer authoritative/local-preview value. */
struct FRokPowerCounterAnimation
{
	int32 Value = 0;
	int32 Target = 0;
	float Elapsed = 1.2f;

	void SetTarget(int32 InValue)
	{
		if (InValue == Target) return;
		if (InValue <= 0 || Target <= 0)
		{
			Value = Target = FMath::Max(0, InValue);
			Elapsed = 1.2f;
			return;
		}
		Start = Value;
		Target = InValue;
		Elapsed = 0;
	}
	void Advance(float DeltaSeconds)
	{
		if (Elapsed >= 1.2f) return;
		Elapsed = FMath::Min(1.2f, Elapsed + FMath::Max(0.0f, DeltaSeconds));
		if (Elapsed >= 1.2f) { Value = Target; return; }
		const double T = Elapsed / 1.2;
		const double Progress = FMath::Sqrt(1.0 - FMath::Square(T - 1.0));
		const int32 Step = FMath::Max(0, FMath::FloorToInt(Progress * 20.0) - 1);
		Value = Start + int32((int64(Target) - Start) * Step / 20);
	}
private:
	int32 Start = 0;
};
