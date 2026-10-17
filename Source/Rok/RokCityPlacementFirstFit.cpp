#include "RokCityPlacementFirstFit.h"

bool FRokCityPlacementFirstFit::Find(int32 SourceGridSize,
	const FIntPoint& FootprintSize, TFunctionRef<bool(const FIntPoint&)> IsBuildable,
	FIntPoint& OutOrigin)
{
	OutOrigin = FIntPoint::ZeroValue;
	if (SourceGridSize < 3 || SourceGridSize % 2 == 0
		|| FootprintSize.X <= 0 || FootprintSize.Y <= 0
		|| FootprintSize.X > SourceGridSize || FootprintSize.Y > SourceGridSize)
	{
		return false;
	}
	// CityGlobalMediator.offBuildPos: (+,+),(-,+),(-,-),(+,-).
	static constexpr int32 Corners[4][2] = {{1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
	const int32 Half = SourceGridSize / 2;
	for (int32 Radius = 1; Radius < Half; ++Radius)
	{
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const int32 Next = (Corner + 1) % 4;
			const int32 StartX = Corners[Corner][0] * Radius;
			const int32 StartY = Corners[Corner][1] * Radius;
			const int32 EndX = Corners[Next][0] * Radius;
			const int32 EndY = Corners[Next][1] * Radius;
			for (int32 X = FMath::Min(StartX, EndX); X < FMath::Max(StartX, EndX); ++X)
			{
				const FIntPoint Candidate(X, EndY);
				if (IsBuildable(Candidate))
				{
					OutOrigin = Candidate;
					return true;
				}
			}
			for (int32 Y = FMath::Min(StartY, EndY); Y < FMath::Max(StartY, EndY); ++Y)
			{
				const FIntPoint Candidate(EndX, Y);
				if (IsBuildable(Candidate))
				{
					OutOrigin = Candidate;
					return true;
				}
			}
		}
	}
	// Unity returns (-width,-length) here; it is not an approved placement.
	return false;
}
