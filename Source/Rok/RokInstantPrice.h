#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// Unity CurrencyProxy and server RoleLogic.cancleCost. Amounts are time and
// the four resource shortfalls, in InstantPrice type order.
namespace RokInstantPrice
{
inline bool AddQuote(const TArray<TSharedPtr<FJsonObject>>& Rows, const int64 (&Amounts)[5], double& Total)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Amounts); ++Index)
	{
		const int64 Amount = Amounts[Index];
		if (Amount <= 0) continue;
		int32 BestThreshold = 0;
		double Quoted = 0;
		for (const auto& Row : Rows)
		{
			int32 Type = -1, Threshold = 0;
			double Base = 0, Increment = 0;
			if (Row && Row->TryGetNumberField(TEXT("type"), Type) && Type == Index * 100
				&& Row->TryGetNumberField(TEXT("num"), Threshold) && Threshold > BestThreshold && Threshold <= Amount
				&& Row->TryGetNumberField(TEXT("price"), Base) && Row->TryGetNumberField(TEXT("priceAdd"), Increment))
			{
				BestThreshold = Threshold;
				Quoted = Base + (Amount - Threshold) * Increment;
			}
		}
		if (BestThreshold == 0) return false;
		Total += Quoted;
	}
	return FMath::IsFinite(Total) && Total >= 0 && Total <= MAX_int32;
}
}
