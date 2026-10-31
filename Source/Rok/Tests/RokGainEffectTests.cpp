#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_GainEffect: town hall third menu action
// (BuildingMenuData[129] -> openUI 1001 -> UI.s_GainEffect). Economy tab shows
// the static CityBuff reference definitions; the military tab has no fixture.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGainEffectLayoutTest,
	"Rok.Ui.GainEffect.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGainEffectLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Gain-effect modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Gain-effect modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGainEffect(0);

	TestNotNull(TEXT("Economy tab exists"),
		Modal->GetWidgetFromName(TEXT("GainEffectTab_0")));
	TestNotNull(TEXT("Military tab exists"),
		Modal->GetWidgetFromName(TEXT("GainEffectTab_1")));
	if (!TestNotNull(TEXT("Economy reference row 0"),
		Modal->GetWidgetFromName(TEXT("GainEffectRow_0")))
		|| !TestNotNull(TEXT("Economy reference row 1"),
			Modal->GetWidgetFromName(TEXT("GainEffectRow_1"))))
	{
		return false;
	}

	const UTextBlock* RowTitle = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GainEffectTitle_0")));
	TestTrue(TEXT("Row 0 shows the source CityBuff title"),
		RowTitle != nullptr
			&& RowTitle->GetText().ToString() == TEXT("食物采集速度+10%"));
	const UTextBlock* RowFields = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GainEffectFields_0")));
	TestTrue(TEXT("Row 0 states the configured duration is not remaining time"),
		RowFields != nullptr
			&& RowFields->GetText().ToString().Contains(TEXT("非剩余时间")));

	UButton* MilitaryTab = Cast<UButton>(
		Modal->GetWidgetFromName(TEXT("GainEffectTab_1")));
	if (TestNotNull(TEXT("Military tab is a real UButton"), MilitaryTab))
	{
		MilitaryTab->OnClicked.Broadcast();
		TestNull(TEXT("Military tab removes the economy rows"),
			Modal->GetWidgetFromName(TEXT("GainEffectRow_0")));
		TestNotNull(TEXT("Military tab shows its server-boundary notice"),
			Modal->GetWidgetFromName(TEXT("GainEffectMilitaryNotice")));
	}

	UButton* EconomyTab = Cast<UButton>(
		Modal->GetWidgetFromName(TEXT("GainEffectTab_0")));
	if (TestNotNull(TEXT("Economy tab is a real UButton"), EconomyTab))
	{
		EconomyTab->OnClicked.Broadcast();
		TestNotNull(TEXT("Returning to the economy tab restores its rows"),
			Modal->GetWidgetFromName(TEXT("GainEffectRow_0")));
	}

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the gain-effect mode"), Modal->IsModalOpen());
	return true;
}

#endif
