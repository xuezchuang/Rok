#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../RokDataPaths.h"
#include "../RokExpeditionStagePreviewData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokExpeditionStagePreviewSourceTest,
	"Rok.Ui.Expedition.SourceStagePreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokExpeditionStagePreviewSourceTest::RunTest(const FString&)
{
	TArray<FRokExpeditionStagePreviewRow> Rows;
	FString Error;
	if (!TestTrue(TEXT("80 source Expedition stages load"),
		RokExpeditionStagePreviewData::LoadAll(Rows, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("Source stage count"), Rows.Num(), 80)) return false;
	FRokExpeditionStagePreviewRow One, Two;
	TestTrue(TEXT("Stage 1 lookup"), RokExpeditionStagePreviewData::FindById(Rows, 1, One));
	TestTrue(TEXT("Stage 2 lookup"), RokExpeditionStagePreviewData::FindById(Rows, 2, Two));
	TestFalse(TEXT("Unknown stage rejects"), RokExpeditionStagePreviewData::FindById(Rows, 81, Two));
	TestEqual(TEXT("Stage 1 source X"), One.BattleCoordinate.X, 466);
	TestEqual(TEXT("Stage 1 source Y"), One.BattleCoordinate.Y, 79);
	TestEqual(TEXT("Stage 2 source X"), Two.BattleCoordinate.X, 601);
	TestEqual(TEXT("Stage 2 source Y"), Two.BattleCoordinate.Y, 101);
	TestEqual(TEXT("Stage 1 battle ID"), One.BattleId, 1);
	TestEqual(TEXT("Stage 2 battle ID"), Two.BattleId, 2);
	TestEqual(TEXT("Stage 1 source description ID"), One.StarDescriptionIds[0], 805038);
	TestEqual(TEXT("Stage 1 localized text"), One.StarDescriptions[0], FString(TEXT("全歼敌人")));
	TestEqual(TEXT("Stage 1 reward group is a config key"), One.FirstRewardGroup, 2000001);
	TArray<uint8> SourceBytes;
	const FString Path = RokDataPaths::Resolve(TEXT("expedition_stage_preview.json"));
	if (!TestTrue(TEXT("Read generated source bytes"), FFileHelper::LoadFileToArray(SourceBytes, *Path))) return false;
	TestTrue(TEXT("Exact bytes accepted"),
		RokExpeditionStagePreviewData::LoadBytesForTesting(SourceBytes, Rows, Error));
	// Mutate one ASCII byte in-place; retain the source file's UTF-8 and line endings.
	const ANSICHAR Needle[] = "\"id\": 1,";
	const int32 NeedleLength = UE_ARRAY_COUNT(Needle) - 1;
	int32 Match = INDEX_NONE, Matches = 0;
	for (int32 Index = 0; Index <= SourceBytes.Num() - NeedleLength; ++Index)
	{
		if (FMemory::Memcmp(SourceBytes.GetData() + Index, Needle, NeedleLength) == 0)
		{
			Match = Index;
			++Matches;
		}
	}
	if (!TestEqual(TEXT("Unique stage 1 ID byte sequence"), Matches, 1)) return false;
	SourceBytes[Match + NeedleLength - 2] = '9';
	TestFalse(TEXT("Modified source bytes rejected"),
		RokExpeditionStagePreviewData::LoadBytesForTesting(SourceBytes, Rows, Error));
	TestEqual(TEXT("Rejected source exposes no stages"), Rows.Num(), 0);
	return true;
}
#endif
