#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_VipStore: VipStore.csv carries localized names directly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokVipStoreLayoutTest,
	"Rok.Ui.VipStore.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokVipStoreLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("VIP-store modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("VIP-store modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenVipStore();

	if (!TestNotNull(TEXT("The first VIP row exists"),
		Modal->GetWidgetFromName(TEXT("VipStoreRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("VipStoreRowTitle_0")));
	TestTrue(TEXT("The first row shows the source item name"),
		Title != nullptr && Title->GetText().ToString() == TEXT("初级行动力恢复"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("VipStoreRowDetail_0")));
	TestTrue(TEXT("The detail lists VIP level, price and count"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("VIP"))
			&& Detail->GetText().ToString().Contains(TEXT("价格")));
	TestTrue(TEXT("The list holds more than six rows"),
		Modal->GetWidgetFromName(TEXT("VipStoreRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the VIP-store mode"), Modal->IsModalOpen());
	return true;
}

#endif
