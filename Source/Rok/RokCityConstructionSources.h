#pragma once

#include "CoreMinimal.h"

// BuildingModelData configuration. A matching row does not imply that either
// source prefab exists, nor that a building can be constructed by this account.
struct FRokCityConstructionModel
{
	int32 Id = 0;
	int32 Type = 0;
	int32 Civilization = 0;
	int32 Age = 0;
	FString ModelId;
	FString ImageResource;
	FString ScenePrefabSource;
	FString UiPrefabSource;
	bool bHasBuildingTypeConfig = false;
	int32 SourceRow = 0;

	bool HasVisualSource() const { return !ScenePrefabSource.IsEmpty() && !UiPrefabSource.IsEmpty(); }
};

// Exact (type, civilization, age) source lookup; no fallback civilization,
// age, first-build cost, actor creation or account state is supplied here.
class ROK_API FRokCityConstructionSources
{
public:
	bool Load(FString& Error);
	bool GetModel(int32 Type, int32 Civilization, int32 Age, FRokCityConstructionModel& Out) const;
	int32 NumModels() const { return Models.Num(); }

private:
	TArray<FRokCityConstructionModel> Models;
	TMap<FString, int32> ByExactKey;
};
