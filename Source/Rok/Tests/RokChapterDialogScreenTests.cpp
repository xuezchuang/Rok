#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ChapterDialog: with a group id the window becomes a dialogue screen
// (left/right portrait plates + line text + advance button), not a list.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokChapterDialogScreenTest,
	"Rok.Ui.ChapterDialog.Screen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChapterDialogScreenTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Chapter modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Chapter modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenChapterDialog(20050);

	const UTextBlock* Line = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ChapterDialogLineText")));
	if (!TestNotNull(TEXT("The dialogue line text exists"), Line))
	{
		return false;
	}
	TestFalse(TEXT("The first line has text"), Line->GetText().ToString().IsEmpty());
	const UTextBlock* Speaker = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ChapterDialogSpeaker")));
	TestTrue(TEXT("The speaker line reports progress through the group"),
		Speaker != nullptr
			&& Speaker->GetText().ToString().Contains(TEXT("第 1 /"))
			&& Speaker->GetText().ToString().Contains(TEXT("NPC_01_SD")));
	TestNotNull(TEXT("Both portrait plates exist"),
		Modal->GetWidgetFromName(TEXT("ChapterPortraitLeftPlate")));
	TestNotNull(TEXT("The right portrait plate exists"),
		Modal->GetWidgetFromName(TEXT("ChapterPortraitRightPlate")));
	TestNotNull(TEXT("The advance button exists"),
		Modal->GetWidgetFromName(TEXT("ChapterDialogContinue")));
	TestTrue(TEXT("No chapter list rows are rendered in dialogue mode"),
		Modal->GetWidgetFromName(TEXT("ChapterDialogRow_0")) == nullptr);

	Modal->CloseModal();
	return true;
}

#endif
