#pragma once
#include "CoreMinimal.h"

namespace RokLeaderboardOverviewLayout
{
	// pl_Total: 1040x664, 3x3 GridLayoutGroup, padding top=20,
	// MiddleCenter alignment. Chinese ArabLayout flips saved UpperRight.
	inline FVector2D TilePosition(int32 Index) { return FVector2D((Index % 3) * 350.f, 66.f + (Index / 3) * 204.f); }
	inline FVector2D BackgroundPosition() { return FVector2D(86.16998f, 0.f); }
	inline FVector2D BackgroundSize() { return FVector2D(253.83002f, 145.6f); }
	inline FVector2D IconPosition() { return FVector2D(-3.900024f, 1.f); }
	inline FVector2D IconSize() { return FVector2D(148, 176); }
	inline FVector2D TitlePosition() { return FVector2D(144.4f, 2.1f); }
	inline FVector2D TitleSize() { return FVector2D(194.7f, 55.8f); }
	inline FVector2D ScorePosition() { return FVector2D(144.8f, 44.9f); }
	inline FVector2D ScoreSize() { return FVector2D(174.71f, 50.f); }
	inline FVector2D OwnerPosition() { return FVector2D(146.f, 95.065f); }
	inline FVector2D OwnerSize() { return FVector2D(178.64f, 41.07f); }
	constexpr float FontPoints(float Pixels) { return Pixels * .75f; }
	struct FBackgroundSlice { FIntRect Rect; FVector2D Position, Size; };
	inline FBackgroundSlice BackgroundSlice(int32 Index)
	{
		// Atlas UV subregions do not change Slate Box's resource pixel size.
		// Use Image patches to retain Unity's fixed 17/15/46/49 pixel borders.
		const int32 Column = Index % 3, Row = Index / 3;
		const int32 X[] = {1952, 1969, 1977}, Y[] = {858, 807, 758};
		const int32 W[] = {17, 8, 15}, H[] = {46, 51, 49};
		const FVector2D Total = BackgroundSize();
		const double DestX[] = {0, 17, Total.X - 15}, DestY[] = {0, 46, Total.Y - 49};
		const double DestW[] = {17, Total.X - 32, 15}, DestH[] = {46, Total.Y - 95, 49};
		return { FIntRect(X[Column], Y[Row], X[Column] + W[Column], Y[Row] + H[Row]),
			FVector2D(DestX[Column], DestY[Row]), FVector2D(DestW[Column], DestH[Row]) };
	}
}
