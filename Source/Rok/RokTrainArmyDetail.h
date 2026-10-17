#pragma once
#include "CoreMinimal.h"

struct FRokTrainArmyAttribute
{
	FString Label, Value, Tip;
	FString Texture;
	FIntRect Rect;
	FIntPoint AtlasSize;
	float UnityY = 0;
	bool bModifierUnknown = false;
};
struct FRokTrainArmyDetailSkill
{
	int32 Id = 0;
	FString Name, Tip, Texture;
	FIntRect Rect;
	FIntPoint AtlasSize;
};
struct FRokTrainArmyDetail
{
	int32 Id = 0;
	FString Name, Description;
	TArray<FRokTrainArmyAttribute> Attributes;
	TArray<FRokTrainArmyDetailSkill> Skills;
};
namespace RokTrainArmyDetail
{
	bool Load(int32 TroopId, FRokTrainArmyDetail& Detail, FString& Error);
	inline float SkillCellX(int32 Count, int32 DataIndex)
	{
		// Chinese Grid startCorner=0, but source siblings are Skill_3,2,1.
		return (624.0f - (Count * 140.0f - 40.0f)) * 0.5f + (Count - 1 - DataIndex) * 140.0f;
	}
	// UA_TrainArmy_DataShow / UA_TrainArmy_TrainShow: zero-tangent curves.
	inline FVector2D Evaluate(float Seconds, bool bDetail)
	{
		const float T = FMath::Clamp(Seconds / 0.25f, 0.0f, 1.0f);
		const float Alpha = T * T * (3.0f - 2.0f * T);
		return FVector2D((bDetail ? 40.0f : -40.0f) * (1.0f - Alpha), Alpha);
	}
}
