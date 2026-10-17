#include "RokExpeditionStagePreviewData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out)
{
	double Number = 0;
	if (!Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
		|| Number < 0 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) return false;
	Out = static_cast<int32>(Number);
	return true;
}

bool ReadIntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<int32>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Object->TryGetArrayField(Field, Array) || !Array) return false;
	Out.Reset();
	for (const TSharedPtr<FJsonValue>& Item : *Array)
	{
		double Number = 0;
		if (!Item.IsValid() || !Item->TryGetNumber(Number) || !FMath::IsFinite(Number)
			|| Number < 0 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) return false;
		Out.Add(static_cast<int32>(Number));
	}
	return true;
}
}

static bool ParsePinnedBytes(TConstArrayView<uint8> Bytes,
	TArray<FRokExpeditionStagePreviewRow>& OutRows, FString& Error)
{
	OutRows.Reset(); Error.Reset();
	static const FString ExpectedSha256 = TEXT("f5a90581a365696e5aa46249ae73db34e621dba0b6c93fbf094d30f604f469cc");
	if (Bytes.IsEmpty() || Bytes.Num() > 512 * 1024 || RokTrainArmyMesh::Sha256(Bytes) != ExpectedSha256)
	{
		Error = TEXT("Expedition source preview identity invalid: rebuild and verify the source package");
		return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	FString Schema;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.expedition_stage_preview.v1")
		|| !Root->TryGetArrayField(TEXT("stages"), Entries) || !Entries || Entries->Num() != 80)
	{
		Error = TEXT("Expedition preview schema or 80-stage count invalid");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Entry : *Entries)
	{
		const TSharedPtr<FJsonObject>* Pointer = nullptr;
		if (!Entry.IsValid() || !Entry->TryGetObject(Pointer) || !Pointer || !Pointer->IsValid()) break;
		const TSharedPtr<FJsonObject>& Object = *Pointer;
		FRokExpeditionStagePreviewRow Row;
		TArray<int32> Coordinate;
		const TArray<TSharedPtr<FJsonValue>>* TextArray = nullptr;
		if (!ReadInt(Object, TEXT("id"), Row.Id) || !ReadInt(Object, TEXT("level"), Row.Level)
			|| !ReadInt(Object, TEXT("front_number"), Row.FrontNumber)
			|| !ReadIntArray(Object, TEXT("battle_coordinate"), Coordinate) || Coordinate.Num() != 2
			|| !ReadInt(Object, TEXT("type"), Row.Type) || !ReadInt(Object, TEXT("battle_id"), Row.BattleId)
			|| !ReadInt(Object, TEXT("time_limit_seconds"), Row.TimeLimitSeconds)
			|| !ReadInt(Object, TEXT("troops_number"), Row.TroopsNumber)
			|| !ReadIntArray(Object, TEXT("star_description_ids"), Row.StarDescriptionIds)
			|| !ReadInt(Object, TEXT("first_reward_group"), Row.FirstRewardGroup)
			|| !ReadIntArray(Object, TEXT("reward_groups"), Row.RewardGroups)
			|| !Object->TryGetArrayField(TEXT("star_descriptions_zh_cn"), TextArray) || !TextArray)
		{
			break;
		}
		for (const TSharedPtr<FJsonValue>& Text : *TextArray)
		{
			FString Value;
			if (!Text.IsValid() || !Text->TryGetString(Value) || Value.IsEmpty()) break;
			Row.StarDescriptions.Add(MoveTemp(Value));
		}
		Row.BattleCoordinate = FIntPoint(Coordinate[0], Coordinate[1]);
		if (Row.Id != OutRows.Num() + 1 || Row.Level != Row.Id || Row.BattleId <= 0
			|| Row.Type < 1 || Row.Type > 2 || Row.StarDescriptionIds.Num() != 3
			|| Row.StarDescriptions.Num() != 3 || Row.RewardGroups.Num() != 3
			|| Row.BattleCoordinate.X <= 0 || Row.BattleCoordinate.Y <= 0) break;
		OutRows.Add(MoveTemp(Row));
	}
	if (OutRows.Num() != 80)
	{
		OutRows.Reset();
		Error = TEXT("Expedition source preview stage rows invalid");
		return false;
	}
	return true;
}

bool RokExpeditionStagePreviewData::LoadAll(TArray<FRokExpeditionStagePreviewRow>& OutRows, FString& Error)
{
	TArray<uint8> Bytes;
	const FString Path = RokDataPaths::Resolve(TEXT("expedition_stage_preview.json"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutRows.Reset();
		Error = TEXT("Expedition source preview missing: run BuildRokExpeditionStagePreview.py");
		return false;
	}
	return ParsePinnedBytes(Bytes, OutRows, Error);
}

#if WITH_DEV_AUTOMATION_TESTS
bool RokExpeditionStagePreviewData::LoadBytesForTesting(TConstArrayView<uint8> Bytes,
	TArray<FRokExpeditionStagePreviewRow>& OutRows, FString& Error)
{
	return ParsePinnedBytes(Bytes, OutRows, Error);
}
#endif

bool RokExpeditionStagePreviewData::FindById(const TArray<FRokExpeditionStagePreviewRow>& Rows,
	int32 Id, FRokExpeditionStagePreviewRow& OutRow)
{
	if (Id < 1 || Id > Rows.Num() || Rows[Id - 1].Id != Id) return false;
	OutRow = Rows[Id - 1];
	return true;
}
