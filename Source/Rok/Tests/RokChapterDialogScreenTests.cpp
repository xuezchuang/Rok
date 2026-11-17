#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "UObject/UObjectIterator.h"
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
		RokUiFind::Find(Modal, TEXT("ChapterDialogLineText")));
	if (!TestNotNull(TEXT("The dialogue line text exists"), Line))
	{
		return false;
	}
	TestFalse(TEXT("The first line has text"), Line->GetText().ToString().IsEmpty());
	const UTextBlock* Speaker = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ChapterDialogSpeaker")));
	TestTrue(TEXT("The speaker line reports progress through the group"),
		Speaker != nullptr
			&& Speaker->GetText().ToString().Contains(TEXT("第 1 /"))
			&& Speaker->GetText().ToString().Contains(TEXT("NPC_01_SD")));
	TestNotNull(TEXT("Both portrait plates exist"),
		RokUiFind::Find(Modal, TEXT("ChapterPortraitLeftPlate")));
	TestNotNull(TEXT("The right portrait plate exists"),
		RokUiFind::Find(Modal, TEXT("ChapterPortraitRightPlate")));
	TestNotNull(TEXT("The advance button exists"),
		RokUiFind::Find(Modal, TEXT("ChapterDialogContinue")));
	TestTrue(TEXT("No chapter list rows are rendered in dialogue mode"),
		RokUiFind::Find(Modal, TEXT("ChapterDialogRow_0")) == nullptr);

	// Exercise the missing-group state through the same public modal entry.
	Modal->OpenChapterDialog(MAX_int32);
	Modal->AdvanceWindowAnimationForTesting(1.0f);
	const FName EmptyTextName = URokUiSettings::IsLegacyForced()
		? FName(TEXT("ChapterDialogEmpty")) : FName(TEXT("ChapterDialogEmptyText"));
	const UTextBlock* Empty = Cast<UTextBlock>(RokUiFind::Find(Modal, EmptyTextName));
	if (!TestNotNull(TEXT("Missing dialogue group presents an empty-state message"), Empty))
	{
		return false;
	}
	TestTrue(TEXT("Empty-state message includes the requested group"),
		Empty->GetText().ToString().Contains(FString::FromInt(MAX_int32)));
	const URokUiPageWidget* EmptyPage = Empty->GetTypedOuter<URokUiPageWidget>();
	if (URokUiSettings::IsLegacyForced())
	{
		TestNull(TEXT("Legacy branch keeps the native empty-state fallback"), EmptyPage);
	}
	else
	{
		TestNotNull(TEXT("WBP branch mounts the editable empty-state page"), EmptyPage);
		TestTrue(TEXT("Empty-state widget belongs to the registered WBP"),
			EmptyPage && EmptyPage->GetClass()->GetPathName().Contains(TEXT("WBP_RokModal_ChapterDialogEmpty_C")));
		const auto* PageSlot = EmptyPage ? Cast<UCanvasPanelSlot>(EmptyPage->Slot) : nullptr;
		TestTrue(TEXT("Empty-state page paints above the frame layer"), PageSlot && PageSlot->GetZOrder() > 4);
	}
	UButton* EmptyDialogCloseButton = nullptr;
	if (URokUiSettings::IsLegacyForced())
	{
		EmptyDialogCloseButton = Cast<UButton>(RokUiFind::Find(Modal, TEXT("Type2CloseButton")));
	}
	else
	{
		URokModalWindowFrame* EmptyDialogFrame = nullptr;
		for (TObjectIterator<URokModalWindowFrame> It; It; ++It)
		{
			if (It->GetTypedOuter<URokUnityModalWidget>() == Modal
				&& It->GetParent() == Modal->GetModeCanvas()
				&& It->Title && It->Title->GetText().ToString() == TEXT("剧情对话"))
			{
				EmptyDialogFrame = *It;
				break;
			}
		}
		EmptyDialogCloseButton = EmptyDialogFrame ? EmptyDialogFrame->CloseButton.Get() : nullptr;
	}
	if (!TestNotNull(TEXT("The active dialogue frame exposes its close button"), EmptyDialogCloseButton))
	{
		return false;
	}
	TestTrue(TEXT("Empty state keeps the modal close action enabled"),
		EmptyDialogCloseButton->GetIsEnabled());
	EmptyDialogCloseButton->OnClicked.Broadcast();
	Modal->AdvanceWindowAnimationForTesting(1.0f);
	TestFalse(TEXT("The actual frame close button closes the empty-state modal"), Modal->IsModalOpen());

	Modal->CloseModal();
	return true;
}

#endif
