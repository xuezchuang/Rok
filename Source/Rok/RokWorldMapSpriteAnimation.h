#pragma once

#include "CoreMinimal.h"

// Client/Compoment/SpriteAnimations/AnimationBase.cs: OnEnable,
// InvokeRepeatingSelf, Update and the looping IDLE UpdateAnimation branch.
// One clock belongs to one SpriteRenderer, including each formation clone.
struct FRokWorldMapSpriteAnimation
{
	// Troops prefab m_lod_array; MatrixRow.ResetUnit recreates all cards
	// when Troops.UpdateLodNow observes a different formation LOD.
	static int32 FormationLod(float Dxf)
	{
		// LevelDetailBase.GetCurrentLodLevel uses inclusive upper bounds.
		return Dxf <= 501.0f ? 0 : (Dxf <= 900.0f ? 1 : 2);
	}

	static float AdvanceSpawnFade(float Alpha, float DeltaSeconds)
	{
		// AnimationBase.UpdateFade: linear alpha += Time.deltaTime * 3.
		return FMath::Min(1.0f, Alpha + DeltaSeconds * 3.0f);
	}

	float Elapsed = 0.0f;
	int32 Frame = 0;

	void Reset(FRandomStream& Random, int32 FrameCount, float RandomOffset)
	{
		Frame = Random.RandRange(0, FMath::Max(0, FrameCount - 1));
		Elapsed = Random.FRandRange(0.0f, RandomOffset);
		// All migrated unit prefabs use ObjectPoolItem. AssetService's
		// OnObjectSpawn follows OnEnable and AnimationBase.OnSpawn resets
		// the frame to zero while preserving the randomized timer phase.
		Frame = 0;
	}

	bool Advance(float DeltaSeconds, float UpdateRate, int32 FrameCount)
	{
		if (FrameCount <= 0 || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
		{
			return false;
		}
		const float Rate = FMath::Max(UpdateRate, 0.0416f);
		Elapsed += DeltaSeconds;
		if (Elapsed <= Rate) return false; // Unity uses strictly greater.
		const int32 Steps = FMath::FloorToInt(Elapsed / Rate);
		Elapsed -= Rate * Steps;
		// Unity advances the skipped frames before UpdateAnimation, whose
		// looping branch resets an overshoot to zero (it does not modulo).
		Frame += Steps - 1;
		Frame = Frame >= FrameCount - 1 ? 0 : Frame + 1;
		return true;
	}
};
