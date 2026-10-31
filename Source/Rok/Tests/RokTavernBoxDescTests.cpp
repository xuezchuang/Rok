#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_TavernBoxDesc: TavernProbility.csv rows expose a percentage.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokTavernBoxDescLayoutTest,
	"Rok.Ui.TavernBoxDesc.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTavernBoxDescLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Tavern modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Tavern modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenTavernBoxDesc();

	if (!TestNotNull(TEXT("The first probability row exists"),
		Modal->GetWidgetFromName(TEXT("TavernBoxDescRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("TavernBoxDescRowTitle_0")));
	TestTrue(TEXT("The row names the reward tier"),
		Title != nullptr && Title->GetText().ToString() == TEXT("传说统帅"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("TavernBoxDescRowDetail_0")));
	TestTrue(TEXT("The detail shows a percentage"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("概率"))
			&& Detail->GetText().ToString().Contains(TEXT("%")));
	TestTrue(TEXT("Both tavern tiers are listed"),
		Modal->GetWidgetFromName(TEXT("TavernBoxDescRow_12")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the tavern mode"), Modal->IsModalOpen());
	return true;
}

#endif
