#pragma once

#include "CoreMinimal.h"

// UI_IF_Captain.prefab: Hero (0,-3.85,4), scale 1.3; Camera (0,0,-9.2),
// identity rotation, perspective vertical FOV 41.7. UIRenderTexture uses the
// full stretched UI_3D_Scene rect. Geometry stays in original Spine XY units.
namespace RokCommanderCamera
{
	// CreateCharView/pl_horeSpine: settled X=-485, Y=-447, scale=1.5.
	// SkeletonGraphic scales source skeleton vertices by canvas.referencePixelsPerUnit=100.
	inline FVector2f ProjectCivilization(FVector2f Point, FVector2f ViewSize, float AboutOffset)
	{
		return ViewSize * 0.5f + FVector2f(-485.0f + 1.5f * AboutOffset + Point.X * 150.0f,
			447.0f - Point.Y * 150.0f);
	}
	inline FVector2f Project(FVector2f Point, FVector2f ViewSize, float CameraX)
	{
		const float PixelsPerUnit = ViewSize.Y / (2.0f * 13.2f * FMath::Tan(FMath::DegreesToRadians(41.7f * 0.5f)));
		return ViewSize * 0.5f + FVector2f(
			(Point.X * 1.3f - CameraX) * PixelsPerUnit,
			-(Point.Y * 1.3f - 3.85f) * PixelsPerUnit);
	}

	inline float Advance(float CameraX, float Delta)
	{
		if (CameraX == 0 || !FMath::IsFinite(Delta) || Delta <= 0) return CameraX;
		// CaptainMediator.Update: CameraDamping keys (0,10),(1,20), slopes 10;
		// SwitchSpeed keys (0,20),(1,20), slopes 0. Both unweighted.
		// These source curves simplify exactly to 10+10*t and constant 20.
		const float Progress = FMath::Clamp(1.0f - FMath::Abs(CameraX) / 10.0f, 0.0f, 1.0f);
		const float Step = Delta * (20.0f - 0.9f * Progress * (10.0f + 10.0f * Progress));
		return CameraX > 0 ? FMath::Max(0.0f, CameraX - Step) : FMath::Min(0.0f, CameraX + Step);
	}
}
