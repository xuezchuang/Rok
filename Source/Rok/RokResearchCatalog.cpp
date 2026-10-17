#include "RokResearchCatalog.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
int64 PairKey(int32 First, int32 Second)
{
	return (static_cast<int64>(First) << 32) | static_cast<uint32>(Second);
}

bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& Out)
{
	return Object.IsValid() && Object->TryGetNumberField(Key, Out);
}

bool Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Out)
{
	return Object.IsValid() && Object->TryGetNumberField(Key, Out);
}

bool String(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Out)
{
	return Object.IsValid() && Object->TryGetStringField(Key, Out);
}

bool Child(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TSharedPtr<FJsonObject>& Out)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	if (!Object.IsValid() || !Object->TryGetObjectField(Key, Value) || !Value || !Value->IsValid()) return false;
	Out = *Value;
	return true;
}

bool IntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, TArray<int32>& Out, int32 Expected)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || !Values || Values->Num() != Expected) return false;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number) return false;
		Out.Add(static_cast<int32>(Value->AsNumber()));
	}
	return true;
}

bool Icon(const TSharedPtr<FJsonObject>& Object, FString& Source, FString& Atlas, FString& Sprite,
	FIntRect& Rect, FIntPoint& AtlasSize)
{
	TSharedPtr<FJsonObject> Data;
	TArray<int32> Bounds, Size;
	if (!Child(Object, TEXT("icon"), Data)
		|| !String(Data, TEXT("source"), Source) || !String(Data, TEXT("atlas"), Atlas)
		|| !String(Data, TEXT("sprite"), Sprite) || !IntArray(Data, TEXT("rect"), Bounds, 4)
		|| !IntArray(Data, TEXT("atlas_size"), Size, 2)
		|| Source.IsEmpty() || Atlas.IsEmpty() || Sprite.IsEmpty() || Bounds[2] <= 0 || Bounds[3] <= 0
		|| Size[0] <= 0 || Size[1] <= 0) return false;
	Rect = FIntRect(Bounds[0], Bounds[1], Bounds[0] + Bounds[2], Bounds[1] + Bounds[3]);
	AtlasSize = FIntPoint(Size[0], Size[1]);
	return true;
}

bool ParseRow(const TSharedPtr<FJsonObject>& Object, FRokResearchRow& Row)
{
	TSharedPtr<FJsonObject> Costs;
	const TArray<TSharedPtr<FJsonValue>>* Preconditions = nullptr;
	if (!Number(Object, TEXT("id"), Row.Id) || !Number(Object, TEXT("type"), Row.Type)
		|| !Number(Object, TEXT("study_type"), Row.StudyType) || !Number(Object, TEXT("level"), Row.Level)
		|| !Number(Object, TEXT("max_level"), Row.MaxLevel) || !Number(Object, TEXT("campus_level"), Row.CampusLevel)
		|| !Number(Object, TEXT("base_seconds"), Row.BaseSeconds) || !Number(Object, TEXT("power"), Row.Power)
		|| !Number(Object, TEXT("name_locale_id"), Row.NameLocaleId) || !Number(Object, TEXT("description_locale_id"), Row.DescriptionLocaleId)
		|| !Number(Object, TEXT("buff_description_locale_id"), Row.BuffDescriptionLocaleId)
		|| !Number(Object, TEXT("columns"), Row.Columns) || !Number(Object, TEXT("source_row"), Row.SourceRow)
		|| !String(Object, TEXT("name"), Row.Name) || !String(Object, TEXT("description"), Row.Description)
		|| !String(Object, TEXT("function_locale_raw"), Row.FunctionLocaleRaw) || !String(Object, TEXT("function_text"), Row.FunctionText)
		|| !String(Object, TEXT("buff_type"), Row.BuffType) || !String(Object, TEXT("buff_type_new"), Row.BuffTypeNew)
		|| !String(Object, TEXT("buff_data"), Row.BuffData) || !String(Object, TEXT("buff_description"), Row.BuffDescription)
		|| !String(Object, TEXT("description_data"), Row.DescriptionData)
		|| !Child(Object, TEXT("base_cost"), Costs)
		|| !Number(Costs, TEXT("food"), Row.BaseFood) || !Number(Costs, TEXT("wood"), Row.BaseWood)
		|| !Number(Costs, TEXT("stone"), Row.BaseStone) || !Number(Costs, TEXT("gold"), Row.BaseGold)
		|| !Icon(Object, Row.IconSource, Row.IconAtlas, Row.IconSprite, Row.IconRect, Row.IconAtlasSize)
		|| !Object->TryGetArrayField(TEXT("preconditions"), Preconditions) || !Preconditions || Preconditions->Num() != 4)
		return false;
	const TSharedPtr<FJsonValue> LocationValue = Object->TryGetField(TEXT("location"));
	if (!LocationValue.IsValid()) return false;
	if (LocationValue->Type == EJson::Number)
	{
		Row.bHasLocation = true;
		Row.Location = static_cast<int32>(LocationValue->AsNumber());
	}
	else if (LocationValue->Type != EJson::Null) return false;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const TSharedPtr<FJsonObject> Item = (*Preconditions)[Index].IsValid() ? (*Preconditions)[Index]->AsObject() : nullptr;
		FRokResearchPrerequisite Pre;
		if (!Number(Item, TEXT("slot"), Pre.Slot) || !Number(Item, TEXT("study_type"), Pre.StudyType)
			|| !Number(Item, TEXT("level"), Pre.Level) || Pre.Slot != Index + 1
			|| (Pre.StudyType == 0) != (Pre.Level == 0)) return false;
		Row.Preconditions.Add(Pre);
	}
	return Row.Id > 0 && (Row.Type == 1 || Row.Type == 2) && Row.StudyType > 0 && Row.Level > 0
		&& Row.MaxLevel >= Row.Level && Row.CampusLevel > 0 && Row.Columns > 0;
}

bool ParseOverride(const TSharedPtr<FJsonObject>& Object, FRokResearchCivilizationOverride& Row)
{
	return Number(Object, TEXT("id"), Row.Id) && Number(Object, TEXT("civilization_id"), Row.CivilizationId)
		&& Number(Object, TEXT("study_row_id"), Row.StudyRowId)
		&& Number(Object, TEXT("name_locale_id"), Row.NameLocaleId)
		&& Number(Object, TEXT("description_locale_id"), Row.DescriptionLocaleId)
		&& Number(Object, TEXT("source_row"), Row.SourceRow)
		&& String(Object, TEXT("name"), Row.Name) && String(Object, TEXT("description"), Row.Description)
		&& Icon(Object, Row.IconSource, Row.IconAtlas, Row.IconSprite, Row.IconRect, Row.IconAtlasSize)
		&& Row.Id > 0 && Row.CivilizationId > 0 && Row.StudyRowId > 0;
}
}

bool FRokResearchCatalog::Load(FString& Error)
{
	Rows.Reset(); Overrides.Reset(); ByRowId.Reset(); ByStudyLevel.Reset(); ByCivilizationAndRow.Reset(); Error.Reset();
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/research_catalog.json");
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("Research catalog missing/invalid: run BuildRokResearchCatalog.py");
		return false;
	}
	FString Schema;
	const TArray<TSharedPtr<FJsonValue>>* SourceRows = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SourceOverrides = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.research_catalog.v1")
		|| !Root->TryGetArrayField(TEXT("rows"), SourceRows) || !SourceRows || SourceRows->Num() != 410
		|| !Root->TryGetArrayField(TEXT("civilization_overrides"), SourceOverrides) || !SourceOverrides || SourceOverrides->Num() != 8)
	{
		Error = TEXT("Research catalog schema or source counts invalid");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *SourceRows)
	{
		FRokResearchRow Row;
		if (!Value.IsValid() || !ParseRow(Value->AsObject(), Row)
			|| ByRowId.Contains(Row.Id) || ByStudyLevel.Contains(PairKey(Row.StudyType, Row.Level)))
		{
			Error = TEXT("Research catalog row invalid or duplicated");
			Rows.Reset(); ByRowId.Reset(); ByStudyLevel.Reset();
			return false;
		}
		ByRowId.Add(Row.Id, Rows.Num());
		ByStudyLevel.Add(PairKey(Row.StudyType, Row.Level), Rows.Num());
		Rows.Add(MoveTemp(Row));
	}
	for (const TSharedPtr<FJsonValue>& Value : *SourceOverrides)
	{
		FRokResearchCivilizationOverride Row;
		if (!Value.IsValid() || !ParseOverride(Value->AsObject(), Row) || !ByRowId.Contains(Row.StudyRowId)
			|| ByCivilizationAndRow.Contains(PairKey(Row.CivilizationId, Row.StudyRowId)))
		{
			Error = TEXT("Research civilization override invalid or duplicated");
			Rows.Reset(); Overrides.Reset(); ByRowId.Reset(); ByStudyLevel.Reset(); ByCivilizationAndRow.Reset();
			return false;
		}
		ByCivilizationAndRow.Add(PairKey(Row.CivilizationId, Row.StudyRowId), Overrides.Num());
		Overrides.Add(MoveTemp(Row));
	}
	return true;
}

bool FRokResearchCatalog::GetByRowId(int32 RowId, FRokResearchRow& Out) const
{
	const int32* Index = ByRowId.Find(RowId);
	if (!Index) return false;
	Out = Rows[*Index];
	return true;
}

bool FRokResearchCatalog::GetByStudyLevel(int32 StudyType, int32 Level, FRokResearchRow& Out) const
{
	const int32* Index = ByStudyLevel.Find(PairKey(StudyType, Level));
	if (!Index) return false;
	Out = Rows[*Index];
	return true;
}

void FRokResearchCatalog::GetRowsByTypeAndLevel(int32 Type, int32 Level, TArray<FRokResearchRow>& Out) const
{
	Out.Reset();
	for (const FRokResearchRow& Row : Rows)
		if (Row.Type == Type && Row.Level == Level) Out.Add(Row);
}

bool FRokResearchCatalog::GetCivilizationOverride(int32 CivilizationId, int32 StudyRowId, FRokResearchCivilizationOverride& Out) const
{
	const int32* Index = ByCivilizationAndRow.Find(PairKey(CivilizationId, StudyRowId));
	if (!Index) return false;
	Out = Overrides[*Index];
	return true;
}
