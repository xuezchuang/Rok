#pragma once
#include "CoreMinimal.h"

namespace RokTrainResourceCost
{
// ClientUtils.CurrencyFormat: truncate the first decimal, never round it.
inline FString Format(int64 Value)
{
	Value = FMath::Max<int64>(0, Value);
	static const TCHAR* Units[] = { TEXT(""), TEXT("K"), TEXT("M"), TEXT("G"), TEXT("T"), TEXT("P"), TEXT("E") };
	int32 Unit = 0; int64 Divisor = 1;
	while (Unit < 6 && Value / Divisor >= 1000) { Divisor *= 1000; ++Unit; }
	if (Unit == 0) return FString::Printf(TEXT("%lld"), Value);
	return FString::Printf(TEXT("%lld.%lld%s"), Value / Divisor, (Value % Divisor) / (Divisor / 10), Units[Unit]);
}

inline float CellX(int32 VisibleCount, int32 VisibleIndex)
{
	// TrainResCost overrides grid cell width to 137. Chinese Arab mask 512 changes corner 1 to 0.
	return (548.f - VisibleCount * 137.f) * .5f + VisibleIndex * 137.f;
}
}
