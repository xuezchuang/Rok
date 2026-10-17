#pragma once
#include "CoreMinimal.h"
#include "String/LexFromString.h"

namespace RokTrainArmyQuantity
{
	// CommonTools.InputSliderControl: int.Parse failure -> MinVal; normalize
	// leading zeros, clamp, and derive slider as count/MaxVal (not min/max lerp).
	inline int32 FromText(const FString& Input, int32 Maximum)
	{
		const FString Text = Input.TrimStartAndEnd();
		if (Text.IsEmpty()) return 1;
		const int32 Start = Text[0] == '+' || Text[0] == '-' ? 1 : 0;
		if (Start == Text.Len()) return 1;
		for (int32 Index = Start; Index < Text.Len(); ++Index)
			if (Text[Index] < '0' || Text[Index] > '9') return 1;
		int64 Value = 0;
		if (!LexTryParseString(Value, *Text) || Value < MIN_int32 || Value > MAX_int32) return 1;
		return FMath::Clamp(static_cast<int32>(Value), 1, FMath::Max(1, Maximum));
	}
	inline int32 FromSlider(float Value, int32 Maximum)
	{
		if (!FMath::IsFinite(Value)) return 1;
		return FMath::Clamp(FMath::FloorToInt(FMath::Clamp(Value, 0.f, 1.f) * FMath::Max(1, Maximum)), 1, FMath::Max(1, Maximum));
	}
	inline float SliderPositionAfterDrag(float Value, int32 Maximum)
	{
		if (!FMath::IsFinite(Value)) return 0;
		Value = FMath::Clamp(Value, 0.f, 1.f);
		const int32 Max = FMath::Max(1, Maximum);
		return Value > 0 && Value * Max < 1 ? 1.f / Max : Value;
	}
}
