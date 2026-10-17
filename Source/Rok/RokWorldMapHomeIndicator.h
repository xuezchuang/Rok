#pragma once

#include "CoreMinimal.h"

namespace RokWorldMapHomeIndicator
{
// MapUIiconMyCityMediator: distance is Unity terrain distance / 10,
// and the cursor is separate from the city name/level inside the viewport.
inline bool ShouldShow(const FVector2D& Projected, const FVector2D& Size, float DistanceKm)
{
	return DistanceKm >= 1.0f && (Projected.X <= 0 || Projected.Y <= 0
		|| Projected.X >= Size.X || Projected.Y >= Size.Y);
}

inline FVector2D Position(const FVector2D& Target, const FVector2D& Size, bool bTactical)
{
	const FVector2D Center = Size * 0.5f;
	const FVector2D Direction = (Target - Center).GetSafeNormal();
	if (Direction.IsNearlyZero()) return Center;
	// Source HudRect uses 3% of screen width on all four edges. Inflate
	// UI exclusion rectangles by the cursor half-size so its artwork and
	// distance text, not just its anchor, stay clear of clickable controls.
	const float Margin = Size.X * 0.03f;
	const FVector2D HalfSize(50.0f, 60.0f);
	const FVector2D Min = FVector2D(Margin, Margin) + HalfSize;
	const FVector2D Max = Size - Min;
	float Travel = TNumericLimits<float>::Max();
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		if (FMath::Abs(Direction[Axis]) > UE_SMALL_NUMBER)
			Travel = FMath::Min(Travel, static_cast<float>(
				((Direction[Axis] > 0 ? Max[Axis] : Min[Axis]) - Center[Axis]) / Direction[Axis]));
	}
	TArray<FBox2D> Obstacles;
	// InitUIPolygonPoint: non-Arabic main HUD exclusion rectangles.
	Obstacles.Emplace(FVector2D(0, 0), FVector2D(540, 124));
	Obstacles.Emplace(FVector2D(0, Size.Y - 80), FVector2D(550, Size.Y));
	Obstacles.Emplace(FVector2D(0, Size.Y - 240), FVector2D(100, Size.Y));
	if (bTactical)
	{
		Obstacles.Emplace(FVector2D(0, 137), FVector2D(215, 500));
		Obstacles.Emplace(FVector2D(Size.X - 730, 0), FVector2D(Size.X, 52));
		Obstacles.Emplace(FVector2D(Size.X - 200, 0), FVector2D(Size.X, 178));
		Obstacles.Emplace(FVector2D(Size.X - 576, Size.Y - 124), Size);
	}
	else
	{
		Obstacles.Emplace(FVector2D(0, 137), FVector2D(167, 450));
		Obstacles.Emplace(FVector2D(Size.X - 220, 0), FVector2D(Size.X, 150));
		Obstacles.Emplace(FVector2D(Size.X - 90, Size.Y - 124), Size);
	}
	Obstacles.Emplace(FVector2D(Size.X - 90, Size.Y - 200), Size);
	for (const FBox2D& Box : Obstacles)
	{
		const FBox2D Expanded(Box.Min - HalfSize, Box.Max + HalfSize);
		float Enter = 0.0f, Exit = Travel;
		bool bHit = true;
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			if (FMath::Abs(Direction[Axis]) <= UE_SMALL_NUMBER)
			{
				if (Center[Axis] < Expanded.Min[Axis] || Center[Axis] > Expanded.Max[Axis]) bHit = false;
				continue;
			}
			const float A = (Expanded.Min[Axis] - Center[Axis]) / Direction[Axis];
			const float B = (Expanded.Max[Axis] - Center[Axis]) / Direction[Axis];
			Enter = FMath::Max(Enter, FMath::Min(A, B));
			Exit = FMath::Min(Exit, FMath::Max(A, B));
		}
		if (bHit && Enter <= Exit && Exit >= 0) Travel = FMath::Min(Travel, Enter);
	}
	return Center + Direction * FMath::Max(0.0f, Travel);
}
}
