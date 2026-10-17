#pragma once

#include "CoreMinimal.h"

// UI_Model_Ck_MainIFLod.prefab ColorBlock; Unity Selectable state priority.
struct FRokWorldMapFilterTint
{
	FLinearColor Start = FLinearColor::White;
	FLinearColor Current = FLinearColor::White;
	FLinearColor Target = FLinearColor::White;
	float Seconds = 0.1f;

	static FLinearColor Resolve(bool bEnabled, bool bPressed, bool bFocused, bool bHovered)
	{
		if (!bEnabled) return FLinearColor(0.78431374f, 0.78431374f, 0.78431374f, 0.5019608f);
		const float Value = bPressed ? 0.78431374f : ((bFocused || bHovered) ? 0.9607843f : 1.0f);
		return FLinearColor(Value, Value, Value, 1.0f);
	}

	void Advance(FLinearColor Next, float DeltaSeconds)
	{
		if (!Target.Equals(Next))
		{
			Start = Current;
			Target = Next;
			Seconds = 0;
		}
		if (FMath::IsFinite(DeltaSeconds))
			Seconds = FMath::Clamp(Seconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.1f);
		Current = FMath::Lerp(Start, Target, Seconds / 0.1f);
	}
};
