#include "RokTaskChapterRewardPreviewData.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 ExpectedGroups = 45;
constexpr int32 ExpectedRows = 123;
const TCHAR* ExpectedHash = TEXT("eab124c2aea9cc265dd2a185c7383f7ab5df02493303bba6dcd18c7c42041d4e");
const TCHAR* ExpectedChapterHash = TEXT("3f3848e9f8898668ff2a8ebeddeace1dfc1d557f110cb1339c74882c396b7bb5");

bool GetObject(const TSharedPtr<FJsonValue>& Value, TSharedPtr<FJsonObject>& Out)
{
	const TSharedPtr<FJsonObject>* Pointer = nullptr;
	if (!Value.IsValid() || !Value->TryGetObject(Pointer) || !Pointer || !Pointer->IsValid()) return false;
	Out = *Pointer;
	return true;
}

bool ReadIntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Count, int32* Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (!Object->TryGetArrayField(Key, Array) || !Array || Array->Num() != Count) return false;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		double Number = 0.0;
		if (!(*Array)[Index].IsValid() || !(*Array)[Index]->TryGetNumber(Number)
			|| !FMath::IsFinite(Number) || Number < 0 || Number > MAX_int32
			|| Number != FMath::FloorToDouble(Number)) return false;
		Out[Index] = static_cast<int32>(Number);
	}
	return true;
}

bool Parse(TConstArrayView<uint8> Bytes, TArray<FRokTaskChapterRewardGroup>& OutGroups, FString& Error)
{
	OutGroups.Reset(); Error.Reset();
	if (Bytes.IsEmpty() || Bytes.Num() > 512 * 1024 || RokTrainArmyMesh::Sha256(Bytes) != ExpectedHash)
	{
		Error = TEXT("Chapter reward preview source identity invalid; regenerate and verify catalog");
		return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("Chapter reward preview JSON invalid");
		return false;
	}
	FString Schema, Policy, ChapterHash;
	int32 GroupCount = 0, RowCount = 0, TaskGroups = 0, ChapterGroups = 0;
	const TArray<TSharedPtr<FJsonValue>>* Groups = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Chapters = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.task_chapter_reward_preview.v1")
		|| !Root->TryGetStringField(TEXT("policy"), Policy)
		|| Policy != TEXT("source_configuration_only_no_account_progress_ownership_claim_or_roll")
		|| !Root->TryGetStringField(TEXT("chapter_catalog_sha256"), ChapterHash) || ChapterHash != ExpectedChapterHash
		|| !Root->TryGetNumberField(TEXT("group_count"), GroupCount) || GroupCount != ExpectedGroups
		|| !Root->TryGetNumberField(TEXT("reward_row_count"), RowCount) || RowCount != ExpectedRows
		|| !Root->TryGetNumberField(TEXT("task_group_count"), TaskGroups) || TaskGroups != 39
		|| !Root->TryGetNumberField(TEXT("chapter_aggregate_group_count"), ChapterGroups) || ChapterGroups != 6
		|| !Root->TryGetArrayField(TEXT("chapters"), Chapters) || !Chapters || Chapters->Num() != 7
		|| !Root->TryGetArrayField(TEXT("groups"), Groups) || !Groups || Groups->Num() != ExpectedGroups)
	{
		Error = TEXT("Chapter reward preview contract invalid");
		return false;
	}
	for (int32 Index = 0; Index < Chapters->Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Chapter;
		int32 Id = 0;
		bool bKnown = false;
		if (!GetObject((*Chapters)[Index], Chapter)
			|| !Chapter->TryGetNumberField(TEXT("chapter_id"), Id) || Id != Index + 1
			|| !Chapter->TryGetBoolField(TEXT("metadata_known"), bKnown) || bKnown != (Id != 7))
		{
			Error = TEXT("Chapter reward metadata invalid"); return false;
		}
		const TSharedPtr<FJsonValue> GroupId = Chapter->TryGetField(TEXT("reward_group_id"));
		int32 Number = 0;
		if (!GroupId.IsValid() || (Id == 7 ? GroupId->Type != EJson::Null
			: !GroupId->TryGetNumber(Number) || Number <= 0))
		{
			Error = TEXT("Chapter reward metadata absence invalid"); return false;
		}
	}
	TSet<int32> SeenGroupIds, SeenTasks, SeenPackageRows;
	TArray<FRokTaskChapterRewardGroup> Parsed;
	int32 ParsedRows = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Groups)
	{
		TSharedPtr<FJsonObject> Object;
		FRokTaskChapterRewardGroup Group;
		FString Owner;
		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!GetObject(Value, Object)
			|| !Object->TryGetStringField(TEXT("owner_kind"), Owner)
			|| !Object->TryGetNumberField(TEXT("reward_group_id"), Group.RewardGroupId)
			|| !Object->TryGetNumberField(TEXT("chapter_id"), Group.ChapterId)
			|| !Object->TryGetBoolField(TEXT("metadata_known"), Group.bMetadataKnown)
			|| !Object->TryGetArrayField(TEXT("entries"), Entries) || !Entries || Entries->IsEmpty()
			|| Group.RewardGroupId <= 0 || Group.ChapterId < 1 || Group.ChapterId > 7
			|| SeenGroupIds.Contains(Group.RewardGroupId))
		{
			Error = TEXT("Chapter reward group invalid"); return false;
		}
		SeenGroupIds.Add(Group.RewardGroupId);
		Group.bChapterAggregate = Owner == TEXT("chapter");
		const TSharedPtr<FJsonValue> TaskField = Object->TryGetField(TEXT("task_id"));
		if (!TaskField.IsValid() || (Group.bChapterAggregate
			? (Group.ChapterId == 7 || !Group.bMetadataKnown || TaskField->Type != EJson::Null)
			: (Owner != TEXT("task") || !TaskField->TryGetNumber(Group.TaskId) || Group.TaskId <= 0
				|| SeenTasks.Contains(Group.TaskId) || Group.bMetadataKnown != (Group.ChapterId != 7))))
		{
			Error = TEXT("Chapter reward owner invalid"); return false;
		}
		if (!Group.bChapterAggregate) SeenTasks.Add(Group.TaskId);
		for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
		{
			TSharedPtr<FJsonObject> Entry;
			FRokTaskChapterRewardPreviewRow Row;
			FString Kind;
			int32 Rect[4] = {}, Size[2] = {};
			if (!GetObject(EntryValue, Entry)
				|| !Entry->TryGetNumberField(TEXT("package_row_id"), Row.PackageRowId)
				|| !Entry->TryGetNumberField(TEXT("reward_group_id"), Row.RewardGroupId)
				|| !Entry->TryGetNumberField(TEXT("reward_type"), Row.RewardType)
				|| !Entry->TryGetStringField(TEXT("kind"), Kind)
				|| !Entry->TryGetNumberField(TEXT("type_data"), Row.TypeData)
				|| !Entry->TryGetNumberField(TEXT("quantity"), Row.Quantity)
				|| !Entry->TryGetNumberField(TEXT("locale_id"), Row.LocaleId)
				|| !Entry->TryGetStringField(TEXT("display_name_zh_cn"), Row.DisplayNameZhCn)
				|| !Entry->TryGetStringField(TEXT("icon_key"), Row.IconKey)
				|| !Entry->TryGetStringField(TEXT("atlas_object_path"), Row.AtlasObjectPath)
				|| !Entry->TryGetNumberField(TEXT("stable_order"), Row.StableOrder)
				|| !ReadIntArray(Entry, TEXT("icon_rect"), 4, Rect)
				|| !ReadIntArray(Entry, TEXT("icon_atlas_size"), 2, Size)
				|| Row.RewardGroupId != Group.RewardGroupId
				|| Row.PackageRowId != Group.RewardGroupId * 1000 + Group.Rows.Num() + 1
				|| Row.StableOrder != Group.Rows.Num() + 1 || Row.Quantity <= 0 || Row.TypeData <= 0
				|| Row.LocaleId <= 0 || Row.DisplayNameZhCn.IsEmpty() || Row.IconKey.IsEmpty()
				|| !Row.AtlasObjectPath.StartsWith(TEXT("/Game/RokPrototype/UI/"))
				|| SeenPackageRows.Contains(Row.PackageRowId)
				|| Rect[2] <= 0 || Rect[3] <= 0 || Size[0] <= 0 || Size[1] <= 0
				|| static_cast<int64>(Rect[0]) + Rect[2] > Size[0]
				|| static_cast<int64>(Rect[1]) + Rect[3] > Size[1]
				|| (Row.RewardType == 100 ? Kind != TEXT("currency")
					: Row.RewardType == 200 ? Kind != TEXT("item")
					: Row.RewardType == 300 ? Kind != TEXT("soldier") : true))
			{
				Error = TEXT("Chapter reward row invalid"); return false;
			}
			SeenPackageRows.Add(Row.PackageRowId);
			Row.IconRect = FIntRect(Rect[0], Rect[1], Rect[0] + Rect[2], Rect[1] + Rect[3]);
			Row.IconAtlasSize = FIntPoint(Size[0], Size[1]);
			Group.Rows.Add(MoveTemp(Row));
			++ParsedRows;
		}
		Parsed.Add(MoveTemp(Group));
	}
	if (ParsedRows != ExpectedRows || SeenTasks.Num() != 39)
	{
		Error = TEXT("Chapter reward count invalid"); return false;
	}
	OutGroups = MoveTemp(Parsed);
	return true;
}
}

bool RokTaskChapterRewardPreviewData::LoadAll(TArray<FRokTaskChapterRewardGroup>& OutGroups, FString& Error)
{
	TArray<uint8> Bytes;
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/TaskChapterRewardPreview/v1/catalog.json");
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutGroups.Reset(); Error = TEXT("Chapter reward catalog missing: run BuildRokTaskChapterRewardPreview.py");
		return false;
	}
	return Parse(Bytes, OutGroups, Error);
}

bool RokTaskChapterRewardPreviewData::FindForTask(const TArray<FRokTaskChapterRewardGroup>& Groups,
	int32 TaskId, FRokTaskChapterRewardGroup& Out)
{
	Out = FRokTaskChapterRewardGroup();
	for (const FRokTaskChapterRewardGroup& Group : Groups)
	{
		if (!Group.bChapterAggregate && Group.TaskId == TaskId) { Out = Group; return true; }
	}
	return false;
}

bool RokTaskChapterRewardPreviewData::FindForChapter(const TArray<FRokTaskChapterRewardGroup>& Groups,
	int32 ChapterId, FRokTaskChapterRewardGroup& Out)
{
	Out = FRokTaskChapterRewardGroup();
	for (const FRokTaskChapterRewardGroup& Group : Groups)
	{
		if (Group.bChapterAggregate && Group.ChapterId == ChapterId) { Out = Group; return true; }
	}
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
bool RokTaskChapterRewardPreviewData::LoadBytesForTesting(TConstArrayView<uint8> Bytes,
	TArray<FRokTaskChapterRewardGroup>& OutGroups, FString& Error)
{
	return Parse(Bytes, OutGroups, Error);
}
#endif
