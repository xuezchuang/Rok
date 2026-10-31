#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_GuildGift: AllianceGiftReward.csv levels joined with AllianceGiftType
// names (宝箱名).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGuildGiftLayoutTest,
	"Rok.Ui.GuildGift.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGuildGiftLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Guild-gift modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Guild-gift modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGuildGift();

	if (!TestNotNull(TEXT("The first gift row exists"),
		Modal->GetWidgetFromName(TEXT("GuildGiftRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GuildGiftRowTitle_0")));
	TestTrue(TEXT("The first row names a chest and level"),
		Title != nullptr
			&& Title->GetText().ToString().Contains(TEXT("Lv")));
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GuildGiftRowDetail_0")));
	TestTrue(TEXT("The detail states the point thresholds"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("礼物积分"))
			&& Detail->GetText().ToString().Contains(TEXT("钥匙积分")));
	TestTrue(TEXT("The list holds more than six rows"),
		Modal->GetWidgetFromName(TEXT("GuildGiftRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the guild-gift mode"), Modal->IsModalOpen());
	return true;
}

#endif
