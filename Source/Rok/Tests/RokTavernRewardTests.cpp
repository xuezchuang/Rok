#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_TavernReward: TavernRank.csv pool rows (name in `comments`, chest in
// `remark`), read through the record reader because the table repeats `type`.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokTavernRewardLayoutTest,
	"Rok.Ui.TavernReward.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTavernRewardLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Tavern-pool modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Tavern-pool modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenTavernReward();

	if (!TestNotNull(TEXT("The first pool row exists"),
		RokUiFind::Find(Modal, TEXT("TavernRewardRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("TavernRewardRowTitle_0")));
	TestTrue(TEXT("The first row names 兰斯洛特"),
		Title != nullptr && Title->GetText().ToString() == TEXT("兰斯洛特"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("TavernRewardRowDetail_0")));
	TestTrue(TEXT("The detail states the entry class and pool"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("统帅"))
			&& Detail->GetText().ToString().Contains(TEXT("池")));
	TestTrue(TEXT("The list holds more than six entries"),
		RokUiFind::Find(Modal, TEXT("TavernRewardRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the tavern-pool mode"), Modal->IsModalOpen());
	return true;
}

#endif
