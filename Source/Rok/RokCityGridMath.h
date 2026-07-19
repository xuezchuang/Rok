#pragma once

#include "CoreMinimal.h"

/**
 * Exact Unity main-city footprint conversion used by building placement,
 * selection colliders, move-bottom tiles, and debug grid rendering.
 */
struct FRokCityGridMath
{
	static constexpr float UnityToUnrealScale = 500.0f;
	static constexpr float UnityCellSize = 0.1f;
	static constexpr float CellSize = UnityToUnrealScale * UnityCellSize;
	static constexpr float HalfCellSize = CellSize * 0.5f;

	// Unity city-local +X after the project's (-x, z, y) handedness conversion
	// and the source building/container yaw of -45 degrees.
	static FVector GetWidthAxis()
	{
		return FVector(-UE_INV_SQRT_2, UE_INV_SQRT_2, 0.0f);
	}

	// Unity city-local +Z after the same conversion.
	static FVector GetLengthAxis()
	{
		return FVector(UE_INV_SQRT_2, UE_INV_SQRT_2, 0.0f);
	}

	static bool TryGetFootprintCorners(
		const FVector& Center,
		FIntPoint FootprintSize,
		TStaticArray<FVector, 4>& OutCorners,
		float ZOffset = 0.0f)
	{
		if (FootprintSize.X <= 0 || FootprintSize.Y <= 0)
		{
			return false;
		}

		const FVector RaisedCenter = Center + FVector(0.0f, 0.0f, ZOffset);
		const FVector WidthExtent = GetWidthAxis() * (FootprintSize.X * HalfCellSize);
		const FVector LengthExtent = GetLengthAxis() * (FootprintSize.Y * HalfCellSize);
		OutCorners[0] = RaisedCenter - WidthExtent - LengthExtent;
		OutCorners[1] = RaisedCenter + WidthExtent - LengthExtent;
		OutCorners[2] = RaisedCenter + WidthExtent + LengthExtent;
		OutCorners[3] = RaisedCenter - WidthExtent + LengthExtent;
		return true;
	}

	static bool IsPointInsideFootprint(
		const FVector& WorldPoint,
		const FVector& Center,
		FIntPoint FootprintSize,
		float EdgeTolerance = 0.1f)
	{
		if (FootprintSize.X <= 0 || FootprintSize.Y <= 0)
		{
			return false;
		}

		const FVector Delta = WorldPoint - Center;
		const float WidthDistance = FMath::Abs(FVector::DotProduct(Delta, GetWidthAxis()));
		const float LengthDistance = FMath::Abs(FVector::DotProduct(Delta, GetLengthAxis()));
		return WidthDistance <= FootprintSize.X * HalfCellSize + EdgeTolerance
			&& LengthDistance <= FootprintSize.Y * HalfCellSize + EdgeTolerance;
	}
};
