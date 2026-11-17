#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
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
		RokUiFind::Find(Modal, TEXT("GainEffectTab_0")));
	TestNotNull(TEXT("Military tab exists"),
		RokUiFind::Find(Modal, TEXT("GainEffectTab_1")));
	const UWidget* FirstRow = RokUiFind::Find(Modal, TEXT("GainEffectRow_0"));
	const UWidget* SecondRow = RokUiFind::Find(Modal, TEXT("GainEffectRow_1"));
	if (!TestTrue(TEXT("Economy reference row 0 is visible"), FirstRow && UWidget::ConvertSerializedVisibilityToRuntime(FirstRow->GetVisibility()).IsVisible())
		|| !TestTrue(TEXT("Economy reference row 1 is visible"), SecondRow && UWidget::ConvertSerializedVisibilityToRuntime(SecondRow->GetVisibility()).IsVisible()))
	{
		return false;
	}

	const UTextBlock* RowTitle = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GainEffectTitle_0")));
	TestTrue(TEXT("Row 0 shows the source CityBuff title"),
		RowTitle != nullptr
			&& RowTitle->GetText().ToString() == TEXT("食物采集速度+10%"));
	const UTextBlock* RowFields = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GainEffectFields_0")));
	TestTrue(TEXT("Row 0 states the configured duration is not remaining time"),
		RowFields != nullptr
			&& RowFields->GetText().ToString().Contains(TEXT("非剩余时间")));

	UButton* MilitaryTab = Cast<UButton>(
		RokUiFind::Find(Modal, TEXT("GainEffectTab_1")));
	if (TestNotNull(TEXT("Military tab is a real UButton"), MilitaryTab))
	{
		MilitaryTab->OnClicked.Broadcast();
		const UWidget* EconomyRow = RokUiFind::Find(Modal, TEXT("GainEffectRow_0"));
		TestFalse(TEXT("Military tab hides the economy rows"),
			EconomyRow && UWidget::ConvertSerializedVisibilityToRuntime(EconomyRow->GetVisibility()).IsVisible());
		const UWidget* Notice = RokUiFind::Find(Modal, TEXT("GainEffectMilitaryNotice"));
		TestTrue(TEXT("Military tab shows its server-boundary notice"),
			Notice && UWidget::ConvertSerializedVisibilityToRuntime(Notice->GetVisibility()).IsVisible());
	}

	UButton* EconomyTab = Cast<UButton>(
		RokUiFind::Find(Modal, TEXT("GainEffectTab_0")));
	if (TestNotNull(TEXT("Economy tab is a real UButton"), EconomyTab))
	{
		EconomyTab->OnClicked.Broadcast();
		const UWidget* EconomyRow = RokUiFind::Find(Modal, TEXT("GainEffectRow_0"));
		TestTrue(TEXT("Returning to the economy tab shows its rows"),
			EconomyRow && UWidget::ConvertSerializedVisibilityToRuntime(EconomyRow->GetVisibility()).IsVisible());
		const UWidget* Notice = RokUiFind::Find(Modal, TEXT("GainEffectMilitaryNotice"));
		TestFalse(TEXT("Economy tab hides the military notice"),
			Notice && UWidget::ConvertSerializedVisibilityToRuntime(Notice->GetVisibility()).IsVisible());
	}

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the gain-effect mode"), Modal->IsModalOpen());
	return true;
}

#endif
