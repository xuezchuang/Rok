#include "RokCityConstructionSources.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString ExactKey(int32 Type, int32 Civilization, int32 Age)
{
	return FString::Printf(TEXT("%d:%d:%d"), Type, Civilization, Age);
}

bool OptionalPath(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FString& Out)
{
	const TSharedPtr<FJsonValue> Value = Object->TryGetField(Name);
	if (!Value.IsValid()) return false;
	if (Value->Type == EJson::Null) { Out.Reset(); return true; }
	return Value->Type == EJson::String && Object->TryGetStringField(Name, Out) && !Out.IsEmpty();
}

bool ParseModel(const TSharedPtr<FJsonObject>& Object, const TSet<int32>& BuildingTypeIds,
	FRokCityConstructionModel& Row)
{
	if (!Object.IsValid()
		|| !Object->TryGetNumberField(TEXT("id"), Row.Id)
		|| !Object->TryGetNumberField(TEXT("type"), Row.Type)
		|| !Object->TryGetNumberField(TEXT("civilization"), Row.Civilization)
		|| !Object->TryGetNumberField(TEXT("age"), Row.Age)
		|| !Object->TryGetNumberField(TEXT("source_row"), Row.SourceRow)
		|| !Object->TryGetStringField(TEXT("model_id"), Row.ModelId)
		|| !Object->TryGetStringField(TEXT("image_resource"), Row.ImageResource)
		|| !Object->TryGetBoolField(TEXT("has_building_type_config"), Row.bHasBuildingTypeConfig)
		|| !OptionalPath(Object, TEXT("scene_prefab"), Row.ScenePrefabSource)
		|| !OptionalPath(Object, TEXT("ui_prefab"), Row.UiPrefabSource)
		|| Row.Type <= 0 || Row.Civilization <= 0 || Row.Age <= 0
		|| Row.ModelId.IsEmpty() || Row.ImageResource.IsEmpty()) return false;
	const FString SourceId = FString::Printf(TEXT("%d%d%d"), Row.Type, Row.Civilization, Row.Age);
	if (Row.Id != FCString::Atoi(*SourceId)) return false;
	const TArray<TSharedPtr<FJsonValue>>* Missing = nullptr;
	if (!Object->TryGetArrayField(TEXT("missing_visual_sources"), Missing) || !Missing) return false;
	TArray<FString> Expected;
	if (Row.ScenePrefabSource.IsEmpty()) Expected.Add(TEXT("scene_prefab"));
	if (Row.UiPrefabSource.IsEmpty()) Expected.Add(TEXT("ui_prefab"));
	if (Missing->Num() != Expected.Num()) return false;
	for (int32 Index = 0; Index < Expected.Num(); ++Index)
	{
		if (!(*Missing)[Index].IsValid() || (*Missing)[Index]->Type != EJson::String
			|| (*Missing)[Index]->AsString() != Expected[Index]) return false;
	}
	return Row.bHasBuildingTypeConfig == BuildingTypeIds.Contains(Row.Type);
}
}

bool FRokCityConstructionSources::Load(FString& Error)
{
	Models.Reset(); ByExactKey.Reset(); Error.Reset();
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/CityConstructionSources/catalog.json");
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("City construction source catalog missing/invalid: run BuildRokCityConstructionSources.py");
		return false;
	}
	FString Schema;
	const TArray<TSharedPtr<FJsonValue>>* Source = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SourceTypes = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.city_construction_sources.v1")
		|| !Root->TryGetArrayField(TEXT("models"), Source) || !Source || Source->Num() != 870
		|| !Root->TryGetArrayField(TEXT("building_type_ids"), SourceTypes) || !SourceTypes || SourceTypes->Num() != 30)
	{
		Error = TEXT("City construction source schema/count invalid");
		return false;
	}
	TSet<int32> BuildingTypeIds;
	int32 PreviousTypeId = 0;
	for (const TSharedPtr<FJsonValue>& Value : *SourceTypes)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number)
		{
			Error = TEXT("City construction building type IDs invalid");
			return false;
		}
		const double Number = Value->AsNumber();
		if (!FMath::IsFinite(Number) || Number <= PreviousTypeId || Number > MAX_int32
			|| Number != FMath::FloorToDouble(Number))
		{
			Error = TEXT("City construction building type IDs duplicated or unsorted");
			return false;
		}
		const int32 TypeId = static_cast<int32>(Number);
		BuildingTypeIds.Add(TypeId);
		PreviousTypeId = TypeId;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Source)
	{
		FRokCityConstructionModel Row;
		const TSharedPtr<FJsonObject>* SourceObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(SourceObject) || !SourceObject || !SourceObject->IsValid()
			|| !ParseModel(*SourceObject, BuildingTypeIds, Row)
			|| ByExactKey.Contains(ExactKey(Row.Type, Row.Civilization, Row.Age)))
		{
			Error = TEXT("City construction model identity or visual source state invalid");
			Models.Reset(); ByExactKey.Reset();
			return false;
		}
		ByExactKey.Add(ExactKey(Row.Type, Row.Civilization, Row.Age), Models.Num());
		Models.Add(MoveTemp(Row));
	}
	return true;
}

bool FRokCityConstructionSources::GetModel(int32 Type, int32 Civilization, int32 Age, FRokCityConstructionModel& Out) const
{
	const int32* Index = ByExactKey.Find(ExactKey(Type, Civilization, Age));
	if (!Index) { Out = FRokCityConstructionModel(); return false; }
	Out = Models[*Index];
	return true;
}
