#include "RokTaskChapterConfigPreviewData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& Out, bool bPositive = false)
{
	double Number = 0;
	if (!Object->TryGetNumberField(Field, Number) || !FMath::IsFinite(Number)
		|| Number < (bPositive ? 1 : 0) || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) return false;
	Out = static_cast<int32>(Number);
	return true;
}

bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out, bool bRequired)
{
	return Object->TryGetStringField(Field, Out) && (!bRequired || !Out.IsEmpty());
}

bool ReadLocale(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FRokTaskChapterLocaleText& Out)
{
	const TSharedPtr<FJsonObject>* Locale = nullptr;
	FString Status;
	return Object->TryGetObjectField(Field, Locale) && Locale && Locale->IsValid()
		&& ReadInt(*Locale, TEXT("id"), Out.Id, true)
		&& ReadString(*Locale, TEXT("zh_cn"), Out.ZhCn, true)
		&& ReadString(*Locale, TEXT("status"), Status, true) && Status == TEXT("resolved");
}

bool ReadIntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<int32>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values) || !Values) return false;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		double Number = 0;
		if (!Value.IsValid() || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)
			|| Number < 1 || Number > MAX_int32 || Number != FMath::FloorToDouble(Number)) return false;
		Out.Add(static_cast<int32>(Number));
	}
	return true;
}

bool ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FString>& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object->TryGetArrayField(Field, Values) || !Values) return false;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		FString String;
		if (!Value.IsValid() || !Value->TryGetString(String) || String.IsEmpty()) return false;
		Out.Add(MoveTemp(String));
	}
	return true;
}

bool IsNull(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
	return Value.IsValid() && Value->Type == EJson::Null;
}

bool ParsePinnedBytes(TConstArrayView<uint8> Bytes,
	TArray<FRokTaskChapterPreviewRow>& OutChapters,
	TArray<FRokTaskChapterTaskPreviewRow>& OutTasks, FString& Error)
{
	OutChapters.Reset(); OutTasks.Reset(); Error.Reset();
	static const FString ExpectedSha256 = TEXT("3f3848e9f8898668ff2a8ebeddeace1dfc1d557f110cb1339c74882c396b7bb5");
	if (Bytes.IsEmpty() || Bytes.Num() > 128 * 1024 || RokTrainArmyMesh::Sha256(Bytes) != ExpectedSha256)
	{
		Error = TEXT("Task chapter source identity invalid: regenerate and verify the v2 catalog");
		return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* ChapterValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* TaskValues = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MissingIds = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
	FString Schema, Mode, AccountState, RewardPolicy;
	int32 ChapterCount = 0, TaskCount = 0, MetadataCount = 0;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !ReadString(Root, TEXT("schema"), Schema, true) || Schema != TEXT("rok.task_chapter_config_preview.v2")
		|| !ReadString(Root, TEXT("mode"), Mode, true) || Mode != TEXT("source_configuration_preview")
		|| !ReadString(Root, TEXT("account_state"), AccountState, true) || AccountState != TEXT("unknown")
		|| !ReadString(Root, TEXT("reward_policy"), RewardPolicy, true)
		|| RewardPolicy != TEXT("configured_group_id_only_no_claim_or_contents")
		|| !ReadInt(Root, TEXT("chapter_count"), ChapterCount) || ChapterCount != 7
		|| !ReadInt(Root, TEXT("task_count"), TaskCount) || TaskCount != 39
		|| !ReadInt(Root, TEXT("chapter_metadata_row_count"), MetadataCount) || MetadataCount != 6
		|| !Root->TryGetArrayField(TEXT("missing_chapter_metadata_ids"), MissingIds) || !MissingIds
		|| MissingIds->Num() != 1 || !(*MissingIds)[0].IsValid() || (*MissingIds)[0]->AsNumber() != 7
		|| !Root->TryGetArrayField(TEXT("sources"), Sources) || !Sources || Sources->Num() != 9
		|| !Root->TryGetArrayField(TEXT("chapters"), ChapterValues) || !ChapterValues || ChapterValues->Num() != 7
		|| !Root->TryGetArrayField(TEXT("tasks"), TaskValues) || !TaskValues || TaskValues->Num() != 39)
	{
		Error = TEXT("Task chapter preview schema, policy or counts invalid");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& SourceValue : *Sources)
	{
		const TSharedPtr<FJsonObject>* Source = nullptr;
		FString Path, Sha;
		if (!SourceValue.IsValid() || !SourceValue->TryGetObject(Source) || !Source || !Source->IsValid()
			|| !ReadString(*Source, TEXT("path"), Path, true)
			|| !ReadString(*Source, TEXT("sha256"), Sha, true) || Sha.Len() != 64)
		{
			Error = TEXT("Task chapter source reference invalid");
			return false;
		}
	}
	TArray<FRokTaskChapterPreviewRow> Chapters;
	TArray<FRokTaskChapterTaskPreviewRow> Tasks;
	const int32 ExpectedCounts[7] = {3, 4, 6, 6, 7, 7, 6};
	for (int32 ChapterIndex = 0; ChapterIndex < 7; ++ChapterIndex)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		const TSharedPtr<FJsonValue>& Value = (*ChapterValues)[ChapterIndex];
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !Object->IsValid()) break;
		FRokTaskChapterPreviewRow Row;
		FString MetadataStatus;
		if (!ReadInt(*Object, TEXT("id"), Row.Id, true) || Row.Id != ChapterIndex + 1
			|| !ReadString(*Object, TEXT("metadata_status"), MetadataStatus, true)
			|| !ReadIntArray(*Object, TEXT("task_ids"), Row.TaskIds)
			|| Row.TaskIds.Num() != ExpectedCounts[ChapterIndex]) break;
		Row.bMetadataKnown = ChapterIndex < 6;
		if (Row.bMetadataKnown)
		{
			if (MetadataStatus != TEXT("source_row")
				|| !ReadInt(*Object, TEXT("source_csv_line"), Row.SourceCsvLine, true)
				|| Row.SourceCsvLine != ChapterIndex + 3
				|| !ReadLocale(*Object, TEXT("title"), Row.Title)
				|| !ReadLocale(*Object, TEXT("description_1"), Row.Description1)
				|| !ReadLocale(*Object, TEXT("description_2"), Row.Description2)
				|| !ReadLocale(*Object, TEXT("preview_text"), Row.PreviewText)
				|| !ReadString(*Object, TEXT("preview_image"), Row.PreviewImage, true)
				|| !ReadString(*Object, TEXT("preview_background"), Row.PreviewBackground, true)
				|| !ReadString(*Object, TEXT("preview_sound"), Row.PreviewSound, false)
				|| !ReadIntArray(*Object, TEXT("dialog_begin_ids"), Row.DialogBeginIds)
				|| !ReadIntArray(*Object, TEXT("dialog_end_ids"), Row.DialogEndIds)
				|| !ReadInt(*Object, TEXT("reward_group_id"), Row.RewardGroupId, true)
				|| !ReadStringArray(*Object, TEXT("civilization_character_assets"), Row.CivilizationCharacterAssets)
				|| !ReadStringArray(*Object, TEXT("age_character_assets"), Row.AgeCharacterAssets)) break;
		}
		else
		{
			static const TCHAR* NullableFields[] = {TEXT("source_csv_line"), TEXT("title"), TEXT("description_1"),
				TEXT("description_2"), TEXT("preview_text"), TEXT("preview_image"), TEXT("preview_background"),
				TEXT("preview_sound"), TEXT("dialog_begin_ids"), TEXT("dialog_end_ids"),
				TEXT("reward_group_id"), TEXT("civilization_character_assets"), TEXT("age_character_assets")};
			if (MetadataStatus != TEXT("missing_in_TaskChapterData")) break;
			bool bAllNull = true;
			for (const TCHAR* Field : NullableFields) bAllNull &= IsNull(*Object, Field);
			if (!bAllNull) break;
		}
		Chapters.Add(MoveTemp(Row));
	}
	if (Chapters.Num() != 7)
	{
		Error = TEXT("Task chapter metadata or chapter task groups invalid");
		return false;
	}
	TSet<int32> SeenIds;
	TSet<int32> SeenSourceLines;
	int32 ChapterIndex = 0;
	int32 ExpectedOrder = 1;
	for (const TSharedPtr<FJsonValue>& Value : *TaskValues)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object || !Object->IsValid()) break;
		FRokTaskChapterTaskPreviewRow Row;
		if (!ReadInt(*Object, TEXT("id"), Row.Id, true)
			|| !ReadInt(*Object, TEXT("chapter_id"), Row.ChapterId, true)
			|| !ReadInt(*Object, TEXT("order"), Row.Order, true)
			|| !ReadInt(*Object, TEXT("source_csv_line"), Row.SourceCsvLine, true)
			|| !ReadLocale(*Object, TEXT("title"), Row.Title)
			|| !ReadLocale(*Object, TEXT("description"), Row.Description)
			|| !ReadInt(*Object, TEXT("type"), Row.Type, true)
			|| !ReadInt(*Object, TEXT("param1"), Row.Param1)
			|| !ReadInt(*Object, TEXT("param2"), Row.Param2)
			|| !ReadInt(*Object, TEXT("require"), Row.Require, true)
			|| !ReadInt(*Object, TEXT("reward_group_id"), Row.RewardGroupId, true)
			|| !ReadString(*Object, TEXT("icon_source_key"), Row.IconSourceKey, true)
			|| Row.ChapterId != ChapterIndex + 1 || Row.Order != ExpectedOrder
			|| Chapters[ChapterIndex].TaskIds[ExpectedOrder - 1] != Row.Id
			|| Row.SourceCsvLine < 3 || Row.SourceCsvLine > 41
			|| SeenIds.Contains(Row.Id) || SeenSourceLines.Contains(Row.SourceCsvLine)) break;
		SeenIds.Add(Row.Id);
		SeenSourceLines.Add(Row.SourceCsvLine);
		Tasks.Add(MoveTemp(Row));
		if (++ExpectedOrder > ExpectedCounts[ChapterIndex]) { ++ChapterIndex; ExpectedOrder = 1; }
	}
	if (Tasks.Num() != 39 || ChapterIndex != 7 || SeenSourceLines.Num() != 39)
	{
		Error = TEXT("Task chapter source tasks, IDs or ordering invalid");
		return false;
	}
	OutChapters = MoveTemp(Chapters);
	OutTasks = MoveTemp(Tasks);
	return true;
}
}

FString FRokTaskChapterPreviewRow::DisplayTitle() const
{
	return bMetadataKnown ? Title.ZhCn : TEXT("第7组 · 章节元数据缺失");
}

bool RokTaskChapterConfigPreviewData::LoadAll(TArray<FRokTaskChapterPreviewRow>& OutChapters,
	TArray<FRokTaskChapterTaskPreviewRow>& OutTasks, FString& Error)
{
	TArray<uint8> Bytes;
	const FString Path = RokDataPaths::Resolve(TEXT("TaskChapterConfigPreview/v2/catalog.json"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutChapters.Reset(); OutTasks.Reset();
		Error = TEXT("Task chapter preview missing: run BuildRokTaskChapterConfigPreview.py");
		return false;
	}
	return ParsePinnedBytes(Bytes, OutChapters, OutTasks, Error);
}

bool RokTaskChapterConfigPreviewData::FindChapterById(const TArray<FRokTaskChapterPreviewRow>& Chapters,
	int32 Id, FRokTaskChapterPreviewRow& OutChapter)
{
	if (Id < 1 || Id > Chapters.Num() || Chapters[Id - 1].Id != Id) return false;
	OutChapter = Chapters[Id - 1];
	return true;
}

bool RokTaskChapterConfigPreviewData::FindTaskById(const TArray<FRokTaskChapterTaskPreviewRow>& Tasks,
	int32 Id, FRokTaskChapterTaskPreviewRow& OutTask)
{
	for (const FRokTaskChapterTaskPreviewRow& Row : Tasks)
	{
		if (Row.Id == Id) { OutTask = Row; return true; }
	}
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
bool RokTaskChapterConfigPreviewData::LoadBytesForTesting(TConstArrayView<uint8> Bytes,
	TArray<FRokTaskChapterPreviewRow>& OutChapters,
	TArray<FRokTaskChapterTaskPreviewRow>& OutTasks, FString& Error)
{
	return ParsePinnedBytes(Bytes, OutChapters, OutTasks, Error);
}
#endif
