#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Containers/StringConv.h"
#include "../RokDataPaths.h"
#include "../RokTaskChapterRewardPreviewData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskChapterRewardPreviewTest,
	"Rok.Ui.QuestChapter.SourceRewardPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskChapterRewardPreviewTest::RunTest(const FString&)
{
	TArray<FRokTaskChapterRewardGroup> Groups;
	FString Error;
	if (!TestTrue(TEXT("Verified source catalog loads"), RokTaskChapterRewardPreviewData::LoadAll(Groups, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("45 source reward groups"), Groups.Num(), 45);
	int32 RowCount = 0;
	for (const FRokTaskChapterRewardGroup& Group : Groups) RowCount += Group.Rows.Num();
	TestEqual(TEXT("123 actual ItemPackageShow rows"), RowCount, 123);
	FRokTaskChapterRewardGroup Group;
	if (!TestTrue(TEXT("Chapter 1 aggregate exists"),
		RokTaskChapterRewardPreviewData::FindForChapter(Groups, 1, Group))) return false;
	TestTrue(TEXT("Aggregate is configuration metadata"), Group.bChapterAggregate && Group.bMetadataKnown);
	TestEqual(TEXT("Chapter 1 source group ID"), Group.RewardGroupId, 100001);
	TestEqual(TEXT("Chapter 1 source row identity"), Group.Rows[0].PackageRowId, 100001001);
	TestEqual(TEXT("Chapter 1 source diamond quantity"), Group.Rows[0].Quantity, int64(50));
	TestEqual(TEXT("Chapter 1 source atlas identity"), Group.Rows[0].IconKey,
		FString(TEXT("ui_common[img_com_cur5]")));
	TestEqual(TEXT("Chapter 1 source atlas rect x"), Group.Rows[0].IconRect.Min.X, 783);
	TestFalse(TEXT("Unknown chapter 7 aggregate must stay absent"),
		RokTaskChapterRewardPreviewData::FindForChapter(Groups, 7, Group));
	TestEqual(TEXT("Failed aggregate lookup clears output"), Group.Rows.Num(), 0);
	if (!TestTrue(TEXT("Task 101001 remains source-backed"),
		RokTaskChapterRewardPreviewData::FindForTask(Groups, 101001, Group))) return false;
	TestEqual(TEXT("Task reward group source ID"), Group.RewardGroupId, 111001);
	TestEqual(TEXT("Task first package source ID"), Group.Rows[0].PackageRowId, 111001001);
	TestEqual(TEXT("Task food source quantity"), Group.Rows[0].Quantity, int64(2000));
	TestFalse(TEXT("Unknown task receives no invented preview"),
		RokTaskChapterRewardPreviewData::FindForTask(Groups, 999999, Group));
	TestEqual(TEXT("Failed task lookup clears output"), Group.Rows.Num(), 0);

	TArray<uint8> SourceBytes;
	const FString CatalogPath = RokDataPaths::Resolve(TEXT("TaskChapterRewardPreview/v1/catalog.json"));
	if (!TestTrue(TEXT("Read generated catalog, never mutate it"),
		FFileHelper::LoadFileToArray(SourceBytes, *CatalogPath))) return false;
	if (!TestTrue(TEXT("Exact bytes pass identity and schema"),
		RokTaskChapterRewardPreviewData::LoadBytesForTesting(SourceBytes, Groups, Error))) return false;
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(SourceBytes.GetData()), SourceBytes.Num());
	const FString Original(Converted.Length(), Converted.Get());
	auto RejectSourceEdit = [this, &Original, &Groups, &Error](const TCHAR* Label,
		const TCHAR* Before, const TCHAR* After)
	{
		const FString Needle(Before);
		const int32 At = Original.Find(Needle, ESearchCase::CaseSensitive);
		if (!TestTrue(FString::Printf(TEXT("%s source field exists"), Label), At != INDEX_NONE)) return false;
		const FString Changed = Original.Left(At) + FString(After) + Original.Mid(At + Needle.Len());
		if (!TestTrue(FString::Printf(TEXT("%s actually changes business bytes"), Label), Changed != Original)) return false;
		const FTCHARToUTF8 Encoded(*Changed);
		TArray<uint8> ChangedBytes;
		ChangedBytes.Append(reinterpret_cast<const uint8*>(Encoded.Get()), Encoded.Length());
		const bool bAccepted = RokTaskChapterRewardPreviewData::LoadBytesForTesting(ChangedBytes, Groups, Error);
		TestFalse(FString::Printf(TEXT("%s tampered catalog rejected"), Label), bAccepted);
		TestTrue(FString::Printf(TEXT("%s fails closed"), Label), Groups.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s gives identity error"), Label), Error.Contains(TEXT("identity")));
		return !bAccepted && Groups.IsEmpty();
	};
	return RejectSourceEdit(TEXT("Quantity"), TEXT("\"quantity\": 50"), TEXT("\"quantity\": 99"))
		&& RejectSourceEdit(TEXT("Package identity"), TEXT("\"package_row_id\": 100001001"),
			TEXT("\"package_row_id\": 100001099"))
		&& RejectSourceEdit(TEXT("Icon UV"), TEXT("        783,"), TEXT("        784,"));
}
#endif
