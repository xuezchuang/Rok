#pragma once

#include "CoreMinimal.h"

// Unity Study.csv prerequisite slot; zero/zero means the source slot is unused.
struct FRokResearchPrerequisite
{
	int32 Slot = 0;
	int32 StudyType = 0;
	int32 Level = 0;
};

struct FRokResearchRow
{
	int32 Id = 0;
	int32 Type = 0;
	int32 StudyType = 0;
	int32 Level = 0;
	int32 MaxLevel = 0;
	int32 CampusLevel = 0;
	TArray<FRokResearchPrerequisite> Preconditions;
	int64 BaseFood = 0, BaseWood = 0, BaseStone = 0, BaseGold = 0;
	int64 BaseSeconds = 0, Power = 0;
	int32 NameLocaleId = 0, DescriptionLocaleId = 0, BuffDescriptionLocaleId = 0;
	FString Name, Description, FunctionLocaleRaw, FunctionText;
	FString BuffType, BuffTypeNew, BuffData, BuffDescription, DescriptionData;
	FString IconSource, IconAtlas, IconSprite;
	FIntRect IconRect;
	FIntPoint IconAtlasSize = FIntPoint::ZeroValue;
	int32 Columns = 0;
	bool bHasLocation = false;
	int32 Location = 0;
	int32 SourceRow = 0;
};

// StudyData.csv overrides are keyed by civilization ID plus Study *row ID*.
struct FRokResearchCivilizationOverride
{
	int32 Id = 0, CivilizationId = 0, StudyRowId = 0;
	int32 NameLocaleId = 0, DescriptionLocaleId = 0;
	FString Name, Description, IconSource, IconAtlas, IconSprite;
	FIntRect IconRect;
	FIntPoint IconAtlasSize = FIntPoint::ZeroValue;
	int32 SourceRow = 0;
};

// Static configuration only. Account research levels, queue and cost modifiers
// must be supplied separately by the caller; this class never creates them.
class ROK_API FRokResearchCatalog
{
public:
	bool Load(FString& Error);
	bool GetByRowId(int32 RowId, FRokResearchRow& Out) const;
	bool GetByStudyLevel(int32 StudyType, int32 Level, FRokResearchRow& Out) const;
	void GetRowsByTypeAndLevel(int32 Type, int32 Level, TArray<FRokResearchRow>& Out) const;
	bool GetCivilizationOverride(int32 CivilizationId, int32 StudyRowId, FRokResearchCivilizationOverride& Out) const;
	int32 NumRows() const { return Rows.Num(); }
	int32 NumCivilizationOverrides() const { return Overrides.Num(); }

private:
	TArray<FRokResearchRow> Rows;
	TArray<FRokResearchCivilizationOverride> Overrides;
	TMap<int32, int32> ByRowId;
	TMap<int64, int32> ByStudyLevel;
	TMap<int64, int32> ByCivilizationAndRow;
};
