#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "../RokUnityModalWidget.h"

// UI_Win_TheWall: durability status window; the prefab's hero/skill plates have
// no backing table, which the tip text states explicitly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokTheWallStatusTest,
	"Rok.Ui.TheWall.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTheWallStatusTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Wall modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Wall modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenTheWall();

	UProgressBar* Bar = Cast<UProgressBar>(RokUiFind::Find(Modal, TEXT("TheWallDurabilityBar")));
	if (!TestNotNull(TEXT("The durability bar exists"), Bar))
	{
		return false;
	}
	TestEqual(TEXT("The bar is full in the local preview"), Bar->GetPercent(), 1.0f);

	const UTextBlock* State = Cast<UTextBlock>(RokUiFind::Find(Modal, TEXT("TheWallStateText")));
	TestTrue(TEXT("The state line names level and durability"),
		State != nullptr
			&& State->GetText().ToString().Contains(TEXT("级"))
			&& State->GetText().ToString().Contains(TEXT("耐久上限")));

	const UTextBlock* Tip = Cast<UTextBlock>(RokUiFind::Find(Modal, TEXT("TheWallTipText")));
	TestTrue(TEXT("The tip states the durability curve and the missing hero table"),
		Tip != nullptr
			&& Tip->GetText().ToString().Contains(TEXT("耐久曲线"))
			&& Tip->GetText().ToString().Contains(TEXT("尚无对应配置表")));
	TestNull(TEXT("The old level table is gone"),
		RokUiFind::Find(Modal, TEXT("TheWallRow_0")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the wall mode"), Modal->IsModalOpen());
	return true;
}

#endif
