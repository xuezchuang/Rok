#include "RokAllianceRankingReadonlyCatalog.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
constexpr int32 ExpectedIds[] = {300, 301, 302, 303, 304, 305};
constexpr int32 ExpectedTypes[] = {4, 5, 7, 8, 9, 10};
constexpr int32 ExpectedShowLimit = 999;
const TCHAR* ExpectedIcons[] = {
	TEXT("ui_player[img_player_201]"), TEXT("ui_player[img_player_205]"),
	TEXT("ui_player[img_player_204]"), TEXT("ui_player[img_player_206]"),
	TEXT("ui_player[img_player_210]"), TEXT("ui_player[img_player_209]")
};
constexpr int32 ExpectedRects[][4] = {
	{301, 141, 148, 176}, {151, 319, 148, 176},
	{301, 497, 148, 176}, {1, 141, 148, 176},
	{301, 675, 148, 176}, {1, 497, 148, 176}
};

bool ReadInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out)
{
	const TSharedPtr<FJsonValue> FieldValue = Object.IsValid() ? Object->TryGetField(Field) : nullptr;
	double Value = 0.0;
	if (!FieldValue.IsValid() || FieldValue->Type != EJson::Number
		|| !Object->TryGetNumberField(Field, Value)
		|| !FMath::IsFinite(Value) || FMath::FloorToDouble(Value) != Value
		|| Value < 0.0 || Value > static_cast<double>(MAX_int32))
	{
		return false;
	}
	Out = static_cast<int32>(Value);
	return true;
}

bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
{
	const TSharedPtr<FJsonValue> FieldValue = Object.IsValid() ? Object->TryGetField(Field) : nullptr;
	return FieldValue.IsValid() && FieldValue->Type == EJson::String
		&& Object->TryGetStringField(Field, Out);
}
}

bool RokAllianceRankingReadonlyCatalog::Load(TArray<FRokLeaderboardRow>& Rows, FString& Error)
{
	Rows.Reset();
	Error.Reset();
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/alliance_ranking_readonly_catalog.json");
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid() || !Root->TryGetArrayField(TEXT("entries"), Entries)
		|| Entries->Num() != UE_ARRAY_COUNT(ExpectedIds))
	{
		Error = TEXT("Alliance ranking readonly catalog missing/invalid; run BuildAllianceRankingReadonlyCatalog.py");
		return false;
	}

	for (int32 Index = 0; Index < Entries->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = (*Entries)[Index].IsValid()
			? (*Entries)[Index]->AsObject() : nullptr;
		FRokLeaderboardRow Row;
		int32 List = 0;
		int32 Target = 0;
		const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Columns = nullptr;
		if (!Object.IsValid()
			|| !ReadInt32(Object, TEXT("id"), Row.Id) || Row.Id != ExpectedIds[Index]
			|| !ReadInt32(Object, TEXT("list"), List) || List != 3
			|| !ReadInt32(Object, TEXT("target"), Target) || Target != 3
			|| !ReadInt32(Object, TEXT("type"), Row.Type) || Row.Type != ExpectedTypes[Index]
			|| !ReadInt32(Object, TEXT("show_limit"), Row.ShowLimit) || Row.ShowLimit != ExpectedShowLimit
			|| !ReadString(Object, TEXT("name"), Row.Name) || Row.Name.IsEmpty()
			|| !ReadString(Object, TEXT("icon"), Row.Icon) || Row.Icon != ExpectedIcons[Index]
			|| !Object->TryGetArrayField(TEXT("rect"), Rect) || Rect->Num() != 4
			|| !Object->TryGetArrayField(TEXT("columns"), Columns) || Columns->Num() != 2)
		{
			Rows.Reset();
			Error = FString::Printf(TEXT("Alliance ranking readonly catalog row %d is invalid"), Index);
			return false;
		}

		int32 Coordinates[4] = {};
		for (int32 CoordinateIndex = 0; CoordinateIndex < UE_ARRAY_COUNT(Coordinates); ++CoordinateIndex)
		{
			const TSharedPtr<FJsonValue>& Value = (*Rect)[CoordinateIndex];
			if (!Value.IsValid() || Value->Type != EJson::Number)
			{
				Rows.Reset();
				Error = FString::Printf(TEXT("Alliance ranking readonly catalog row %d has a non-number icon coordinate"), Index);
				return false;
			}
			const double Number = Value->AsNumber();
			if (!FMath::IsFinite(Number) || FMath::FloorToDouble(Number) != Number
				|| Number < 0.0 || Number > static_cast<double>(MAX_int32))
			{
				Rows.Reset();
				Error = FString::Printf(TEXT("Alliance ranking readonly catalog row %d has invalid icon coordinates"), Index);
				return false;
			}
			Coordinates[CoordinateIndex] = static_cast<int32>(Number);
		}
		const int32 X = Coordinates[0];
		const int32 Y = Coordinates[1];
		const int32 Width = Coordinates[2];
		const int32 Height = Coordinates[3];
		if (Width <= 0 || Height <= 0 || X > 512 || Y > 1024
			|| Width > 512 - X || Height > 1024 - Y
			|| X != ExpectedRects[Index][0] || Y != ExpectedRects[Index][1]
			|| Width != ExpectedRects[Index][2] || Height != ExpectedRects[Index][3])
		{
			Rows.Reset();
			Error = FString::Printf(TEXT("Alliance ranking readonly catalog row %d icon is outside the source atlas"), Index);
			return false;
		}
		Row.Target = Target;
		for (const TSharedPtr<FJsonValue>& Column : *Columns)
		{
			if (!Column.IsValid() || Column->Type != EJson::String || Column->AsString().IsEmpty())
			{
				Rows.Reset();
				Error = FString::Printf(TEXT("Alliance ranking readonly catalog row %d columns are invalid"), Index);
				return false;
			}
			Row.Columns.Add(Column->AsString());
		}
		Row.IconRect = FIntRect(X, Y, X + Width, Y + Height);
		Rows.Add(MoveTemp(Row));
	}
	return true;
}
