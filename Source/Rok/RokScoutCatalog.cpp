#include "RokScoutCatalog.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FRokScoutCatalog::Load(FString& Error)
{
	Levels.Reset(); Text.Reset(); Error.Reset();
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	const TSharedPtr<FJsonObject>* Strings = nullptr;
	int32 BuildingType = 0;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/scout_catalog.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetNumberField(TEXT("building_type"), BuildingType) || BuildingType != 28
		|| !Root->TryGetArrayField(TEXT("levels"), Rows) || !Root->TryGetObjectField(TEXT("text"), Strings))
	{
		Error = TEXT("Scout catalog missing or invalid; run BuildRokScoutCatalog.py"); return false;
	}
	TSet<int32> Seen;
	TArray<FRokScoutCampLevel> Validated;
	for (const auto& Value : *Rows)
	{
		const auto Row = Value->AsObject();
		FRokScoutCampLevel Entry;
		int32 Id = 0;
		if (!Row.IsValid() || !Row->TryGetNumberField(TEXT("ID"), Id)
			|| !Row->TryGetNumberField(TEXT("level"), Entry.Level) || Id != Entry.Level
			|| !Row->TryGetNumberField(TEXT("scoutNumber"), Entry.Capacity)
			|| !Row->TryGetNumberField(TEXT("scoutView"), Entry.View)
			|| !Row->TryGetNumberField(TEXT("scoutSpeedMulti"), Entry.SpeedMulti)
			|| Entry.Level <= 0 || Seen.Contains(Entry.Level) || Entry.Capacity <= 0 || Entry.View <= 0 || Entry.SpeedMulti < 0)
		{
			Error = TEXT("Scout catalog contains invalid level data"); return false;
		}
		Seen.Add(Entry.Level); Validated.Add(Entry);
	}
	if (Validated.IsEmpty()) { Error = TEXT("Scout catalog has no levels"); return false; }
	for (int32 Id : {180021, 181002, 181144})
	{
		FString Label;
		if (!(*Strings)->TryGetStringField(FString::FromInt(Id), Label) || Label.IsEmpty())
		{
			Text.Reset(); Error = TEXT("Scout catalog missing source localization"); return false;
		}
		Text.Add(Id, MoveTemp(Label));
	}
	Levels = MoveTemp(Validated);
	return true;
}

const FRokScoutCampLevel* FRokScoutCatalog::FindLevel(int32 Level) const
{
	return Level > 0 ? Levels.FindByPredicate([Level](const auto& Row) { return Row.Level == Level; }) : nullptr;
}
