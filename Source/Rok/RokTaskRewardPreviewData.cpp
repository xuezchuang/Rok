#include "RokTaskRewardPreviewData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadPair(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FIntPoint& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	double X = 0.0, Y = 0.0;
	if (!Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 2
		|| !(*Values)[0].IsValid() || !(*Values)[0]->TryGetNumber(X)
		|| !(*Values)[1].IsValid() || !(*Values)[1]->TryGetNumber(Y)
		|| !FMath::IsFinite(X) || !FMath::IsFinite(Y)
		|| X < 0 || Y < 0 || X > MAX_int32 || Y > MAX_int32
		|| X != FMath::FloorToDouble(X) || Y != FMath::FloorToDouble(Y)) return false;
	Out = FIntPoint(static_cast<int32>(X), static_cast<int32>(Y));
	return true;
}

bool ReadRect(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FIntRect& Out)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	double Numbers[4] = {};
	if (!Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 4) return false;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Numbers[Index])
			|| !FMath::IsFinite(Numbers[Index]) || Numbers[Index] < 0 || Numbers[Index] > MAX_int32
			|| Numbers[Index] != FMath::FloorToDouble(Numbers[Index])) return false;
	}
	if (Numbers[0] + Numbers[2] > MAX_int32 || Numbers[1] + Numbers[3] > MAX_int32) return false;
	Out = FIntRect(static_cast<int32>(Numbers[0]), static_cast<int32>(Numbers[1]),
		static_cast<int32>(Numbers[0] + Numbers[2]), static_cast<int32>(Numbers[1] + Numbers[3]));
	return true;
}

bool ParsePinnedGroup(int32 GroupId, TConstArrayView<uint8> Bytes,
	TArray<FRokTaskRewardPreviewRow>& OutRows, FString& Error)
{
	OutRows.Reset(); Error.Reset();
	if (GroupId != 200900)
	{
		Error = FString::Printf(TEXT("Task reward group %d has no source preview"), GroupId);
		return false;
	}
	// The bounded TaskMain 200900 source catalog has been independently verified against
	// ItemPackageShow, Currency, Language and ui_common. Pin exact generated UTF-8 bytes,
	// including row IDs, quantities, locale names and sprite rectangles, before parsing.
	static const FString ExpectedSha256 = TEXT("899c8a8715dde654dc562772886b4971b07c4ae77e0d82bd486f2576ee0ede5f");
	if (Bytes.IsEmpty() || Bytes.Num() > 64 * 1024 || RokTrainArmyMesh::Sha256(Bytes) != ExpectedSha256)
	{
		Error = TEXT("Task reward preview source identity invalid: regenerate and verify the catalog");
		return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("Task reward preview missing/invalid: run BuildRokUnityTaskRewardPreview.py");
		return false;
	}
	FString Schema, AtlasAsset;
	int32 TaskId = 0, RewardGroupId = 0;
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.task_reward_preview.v1")
		|| !Root->TryGetNumberField(TEXT("task_id"), TaskId) || TaskId != 200900
		|| !Root->TryGetNumberField(TEXT("reward_group_id"), RewardGroupId) || RewardGroupId != GroupId
		|| !Root->TryGetStringField(TEXT("atlas_asset"), AtlasAsset)
		|| AtlasAsset != TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common")
		|| !Root->TryGetArrayField(TEXT("entries"), Entries) || !Entries || Entries->Num() != 3)
	{
		Error = TEXT("Task reward preview schema/group invalid");
		return false;
	}
	TArray<FRokTaskRewardPreviewRow> Parsed;
	for (const TSharedPtr<FJsonValue>& Value : *Entries)
	{
		const TSharedPtr<FJsonObject>* ObjectPointer = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(ObjectPointer) || !ObjectPointer || !ObjectPointer->IsValid()) break;
		const TSharedPtr<FJsonObject>& Object = *ObjectPointer;
		FRokTaskRewardPreviewRow Row;
		FString Kind, NameKey;
		if (!Object->TryGetNumberField(TEXT("package_row_id"), Row.PackageRowId)
			|| !Object->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("currency")
			|| !Object->TryGetNumberField(TEXT("currency_id"), Row.CurrencyId)
			|| !Object->TryGetNumberField(TEXT("quantity"), Row.Quantity)
			|| !Object->TryGetNumberField(TEXT("locale_id"), Row.LocaleId)
			|| !Object->TryGetStringField(TEXT("name_key"), NameKey) || NameKey.IsEmpty()
			|| !Object->TryGetStringField(TEXT("display_name_zh_cn"), Row.DisplayName) || Row.DisplayName.IsEmpty()
			|| !Object->TryGetStringField(TEXT("icon_key"), Row.IconKey)
			|| !ReadRect(Object, TEXT("icon_rect"), Row.IconRect)
			|| !ReadPair(Object, TEXT("icon_atlas_size"), Row.IconAtlasSize)
			|| Row.PackageRowId <= 0 || Row.CurrencyId <= 0 || Row.Quantity <= 0 || Row.LocaleId <= 0
			|| Row.IconRect.Min.X < 0 || Row.IconRect.Min.Y < 0 || Row.IconRect.Width() <= 0 || Row.IconRect.Height() <= 0
			|| Row.IconRect.Max.X > Row.IconAtlasSize.X || Row.IconRect.Max.Y > Row.IconAtlasSize.Y)
		{
			break;
		}
		const FString ExpectedIconKey = FString::Printf(TEXT("ui_common[img_com_cur%d]"), Row.CurrencyId - 99);
		if (Row.IconKey != ExpectedIconKey || (Parsed.Num() && Row.CurrencyId <= Parsed.Last().CurrencyId)) break;
		Parsed.Add(MoveTemp(Row));
	}
	if (Parsed.Num() != 3 || Parsed[0].CurrencyId != 100 || Parsed[1].CurrencyId != 101
		|| Parsed[2].CurrencyId != 104)
	{
		Error = TEXT("Task reward preview entries invalid");
		return false;
	}
	OutRows = MoveTemp(Parsed);
	return true;
}
}

bool RokTaskRewardPreviewData::LoadGroup(int32 GroupId, TArray<FRokTaskRewardPreviewRow>& OutRows, FString& Error)
{
	TArray<uint8> Bytes;
	const FString Path = RokDataPaths::Resolve(TEXT("task_reward_preview.json"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutRows.Reset();
		Error = TEXT("Task reward preview missing: run BuildRokUnityTaskRewardPreview.py");
		return false;
	}
	return ParsePinnedGroup(GroupId, Bytes, OutRows, Error);
}

#if WITH_DEV_AUTOMATION_TESTS
bool RokTaskRewardPreviewData::LoadGroupBytesForTesting(int32 GroupId, TConstArrayView<uint8> Bytes,
	TArray<FRokTaskRewardPreviewRow>& OutRows, FString& Error)
{
	return ParsePinnedGroup(GroupId, Bytes, OutRows, Error);
}
#endif
