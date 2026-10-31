#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "../RokUnityModalWidget.h"

// UI_Win_TheTower: the prefab is a status window (bar + state + tip text).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokTheTowerStatusTest,
	"Rok.Ui.TheTower.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTheTowerStatusTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Tower modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Tower modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenTheTower();

	UProgressBar* Bar = Cast<UProgressBar>(Modal->GetWidgetFromName(TEXT("TheTowerHpBar")));
	if (!TestNotNull(TEXT("The tower HP bar exists"), Bar))
	{
		return false;
	}
	TestEqual(TEXT("The bar is full in the local preview"), Bar->GetPercent(), 1.0f);

	const UTextBlock* State = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("TheTowerStateText")));
	TestTrue(TEXT("The state line names level, attack and HP"),
		State != nullptr
			&& State->GetText().ToString().Contains(TEXT("级"))
			&& State->GetText().ToString().Contains(TEXT("攻击"))
			&& State->GetText().ToString().Contains(TEXT("生命")));

	const UTextBlock* Tip = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("TheTowerTipText")));
	TestTrue(TEXT("The tip lists the HP-section rules"),
		Tip != nullptr
			&& Tip->GetText().ToString().Contains(TEXT("减伤"))
			&& Tip->GetText().ToString().Contains(TEXT("生命分段生效")));
	TestNull(TEXT("The old level table is gone"),
		Modal->GetWidgetFromName(TEXT("TheTowerRow_0")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the tower mode"), Modal->IsModalOpen());
	return true;
}

#endif
