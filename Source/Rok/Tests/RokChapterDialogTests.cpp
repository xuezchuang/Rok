#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"
#include "../RokUiTextTable.h"

// UI_Win_ChapterDialog: chapter rows from TaskChapter.csv. Unity rich-text
// markup (`<b>`, `<color=…>`) and `{pN}` placeholders must not reach UMG.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokChapterDialogLayoutTest,
	"Rok.Ui.ChapterDialog.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChapterDialogLayoutTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Markup and placeholders are stripped"),
		FRokUiTextTable::SanitizeRichText(TEXT("<b>将市政厅升至等级{p2}</b>")),
		FString(TEXT("将市政厅升至等级")));
	TestEqual(TEXT("Color spans keep their text"),
		FRokUiTextTable::SanitizeRichText(TEXT("<color=orange>整军备武</color>")),
		FString(TEXT("整军备武")));

	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Chapter modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Chapter modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenChapterDialog();

	if (!TestNotNull(TEXT("The first chapter row exists"),
		Modal->GetWidgetFromName(TEXT("ChapterDialogRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ChapterDialogRowTitle_0")));
	TestTrue(TEXT("The first row shows a chapter name"),
		Title != nullptr && !Title->GetText().ToString().IsEmpty());
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ChapterDialogRowDetail_0")));
	TestTrue(TEXT("The objective text has no markup or placeholders"),
		Detail != nullptr
			&& !Detail->GetText().ToString().Contains(TEXT("<"))
			&& !Detail->GetText().ToString().Contains(TEXT("{")));

	Modal->CloseModal();

	// The dialogue variant is covered by Rok.Ui.ChapterDialog.Screen.
	TestFalse(TEXT("Closing clears the chapter mode"), Modal->IsModalOpen());
	return true;
}

#endif
