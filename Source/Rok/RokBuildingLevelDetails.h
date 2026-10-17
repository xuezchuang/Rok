#pragma once
#include "CoreMinimal.h"

struct FRokBuildingLevelDetailsRow
{
	int32 Level = 0, ProduceSpeed = 0, GatherMax = 0, Power = 0;
};
struct FRokBarracksLevelDetailsRow
{
	int32 Level = 0, InfantryTrainNumber = 0, InfantryAttackMulti = 0, Power = 0;
	FString InfantryAttackText;
};
namespace RokBuildingLevelDetails
{
	ROK_API bool Load(int32 Type, TArray<FString>& Headers, TArray<FRokBuildingLevelDetailsRow>& Rows, FString& Error, FString* Name = nullptr, FString* Description = nullptr);
	ROK_API bool LoadBarracks(TArray<FString>& Headers, TArray<FRokBarracksLevelDetailsRow>& Rows, FString& Error, FString* Name = nullptr, FString* Description = nullptr);
	struct FAnimationSample { float X; float Alpha; };
	inline FAnimationSample Animate(float Time, bool bLevelPage)
	{
		// UA_BuildingDesc_LevelShow/ContentShow: root X +/-50 -> 0,
		// CanvasGroup alpha 0 -> 1, zero tangents over 0.25 seconds.
		const float T = FMath::Clamp(Time / 0.25f, 0.0f, 1.0f);
		const float S = T * T * (3.0f - 2.0f * T);
		return { (bLevelPage ? 50.0f : -50.0f) * (1.0f - S), S };
	}
}
