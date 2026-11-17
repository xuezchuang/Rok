#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_BattlePassRewards: rows come from BattlePassReward.csv (age 0) and the
// item names join Item.csv -> Language.csv.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokBattlePassRewardsLayoutTest,
	"Rok.Ui.BattlePassRewards.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBattlePassRewardsLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Battle-pass modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Battle-pass modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenBattlePassRewards();

	if (!TestNotNull(TEXT("The first battle-pass row exists"),
		RokUiFind::Find(Modal, TEXT("BattlePassRow_0"))))
	{
		return false;
	}
	const UTextBlock* LevelText = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("BattlePassRowLevel_0")));
	TestTrue(TEXT("The first row names level 1"),
		LevelText != nullptr && LevelText->GetText().ToString().Contains(TEXT("等级 1")));

	const UTextBlock* FreeText = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("BattlePassRowFree_0")));
	TestTrue(TEXT("The free track joins the reward item name"),
		FreeText != nullptr
			&& FreeText->GetText().ToString().Contains(TEXT("等级1自选资源宝箱")));
	const UTextBlock* SuperText = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("BattlePassRowSuper_0")));
	TestTrue(TEXT("The paid track joins the reward item name"),
		SuperText != nullptr
			&& SuperText->GetText().ToString().Contains(TEXT("传说统帅奖章")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the battle-pass mode"), Modal->IsModalOpen());
	return true;
}

#endif
