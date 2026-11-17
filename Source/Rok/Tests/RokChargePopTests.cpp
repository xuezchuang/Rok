#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ChargePop: RechargeSale.csv rows (duplicate l_nameID, quoted blurbs).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokChargePopLayoutTest,
	"Rok.Ui.ChargePop.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChargePopLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Charge modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Charge modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenChargePop();

	if (!TestNotNull(TEXT("The first package row exists"),
		RokUiFind::Find(Modal, TEXT("ChargePopRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ChargePopRowTitle_0")));
	TestTrue(TEXT("The first row names a package"),
		Title != nullptr && Title->GetText().ToString() == TEXT("王之加冕1"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ChargePopRowDetail_0")));
	TestTrue(TEXT("The detail shows the price"),
		Detail != nullptr && Detail->GetText().ToString().Contains(TEXT("价格")));
	TestTrue(TEXT("The list holds more than six packages"),
		RokUiFind::Find(Modal, TEXT("ChargePopRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the charge mode"), Modal->IsModalOpen());
	return true;
}

#endif
