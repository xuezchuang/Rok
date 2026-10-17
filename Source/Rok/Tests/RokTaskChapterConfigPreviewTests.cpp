#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../RokTaskChapterConfigPreviewData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskChapterConfigPreviewSourceTest,
	"Rok.Ui.QuestChapter.SourceConfigPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskChapterConfigPreviewSourceTest::RunTest(const FString&)
{
	TArray<FRokTaskChapterPreviewRow> Chapters;
	TArray<FRokTaskChapterTaskPreviewRow> Tasks;
	FString Error;
	if (!TestTrue(TEXT("Verified chapter source loads"),
		RokTaskChapterConfigPreviewData::LoadAll(Chapters, Tasks, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Seven chapter groups"), Chapters.Num(), 7)
		|| !TestEqual(TEXT("All 39 source tasks"), Tasks.Num(), 39)) return false;
	const int32 ExpectedCounts[] = {3, 4, 6, 6, 7, 7, 6};
	int32 Total = 0;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		TestEqual(TEXT("Chapter source group size"), Chapters[Index].TaskIds.Num(), ExpectedCounts[Index]);
		Total += Chapters[Index].TaskIds.Num();
		TestEqual(TEXT("Source chapter identity"), Chapters[Index].Id, Index + 1);
	}
	TestEqual(TEXT("All chapter task IDs retained"), Total, 39);
	TestTrue(TEXT("Chapter 6 has Unity metadata"), Chapters[5].bMetadataKnown);
	TestFalse(TEXT("Chapter 7 has no Unity metadata"), Chapters[6].bMetadataKnown);
	TestEqual(TEXT("Chapter 7 retains six task IDs"), Chapters[6].TaskIds.Num(), 6);
	TestEqual(TEXT("Chapter 7 first source task"), Chapters[6].TaskIds[0], 107001);
	TestEqual(TEXT("Chapter 7 last source task"), Chapters[6].TaskIds[5], 107006);
	TestEqual(TEXT("Missing metadata title stays absent"), Chapters[6].Title.ZhCn, FString());
	TestEqual(TEXT("Missing metadata reward stays unknown"), Chapters[6].RewardGroupId, 0);
	TestEqual(TEXT("Local fallback labels the absence"), Chapters[6].DisplayTitle(),
		FString(TEXT("第7组 · 章节元数据缺失")));
	FRokTaskChapterPreviewRow Chapter;
	FRokTaskChapterTaskPreviewRow Task;
	TestTrue(TEXT("Known chapter lookup"),
		RokTaskChapterConfigPreviewData::FindChapterById(Chapters, 1, Chapter));
	TestEqual(TEXT("Known chapter Unity title"), Chapter.Title.ZhCn, FString(TEXT("第一章")));
	TestFalse(TEXT("Unknown chapter is not inferred"),
		RokTaskChapterConfigPreviewData::FindChapterById(Chapters, 8, Chapter));
	TestTrue(TEXT("Known task lookup"),
		RokTaskChapterConfigPreviewData::FindTaskById(Tasks, 101001, Task));
	TestEqual(TEXT("Task source order"), Task.Order, 1);
	TestEqual(TEXT("Task source type"), Task.Type, 8);
	TestEqual(TEXT("Task source parameter"), Task.Param2, 2);
	TestEqual(TEXT("Reward remains a source group reference"), Task.RewardGroupId, 111001);
	TestFalse(TEXT("Unknown task is not inferred"),
		RokTaskChapterConfigPreviewData::FindTaskById(Tasks, 999999, Task));

	TArray<uint8> Bytes;
	const FString Path = FPaths::ProjectSavedDir() /
		TEXT("RokUnityMigration/TaskChapterConfigPreview/v2/catalog.json");
	if (!TestTrue(TEXT("Read immutable source catalog"), FFileHelper::LoadFileToArray(Bytes, *Path))) return false;
	TestTrue(TEXT("Pinned source bytes accepted"),
		RokTaskChapterConfigPreviewData::LoadBytesForTesting(Bytes, Chapters, Tasks, Error));
	if (!TestTrue(TEXT("Source has bytes to corrupt"), Bytes.Num() > 32)) return false;
	Bytes[Bytes.Num() / 2] ^= 1;
	TestFalse(TEXT("Corrupt source bytes rejected"),
		RokTaskChapterConfigPreviewData::LoadBytesForTesting(Bytes, Chapters, Tasks, Error));
	TestEqual(TEXT("Failure exposes no chapters"), Chapters.Num(), 0);
	TestEqual(TEXT("Failure exposes no tasks"), Tasks.Num(), 0);
	TestTrue(TEXT("Failure reports source identity"), Error.Contains(TEXT("identity")));
	return true;
}
#endif
