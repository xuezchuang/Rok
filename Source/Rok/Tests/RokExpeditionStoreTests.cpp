#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ExpeditionStore: ExpeditionShop.csv rows joined with Item.csv names.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokExpeditionStoreLayoutTest,
	"Rok.Ui.ExpeditionStore.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokExpeditionStoreLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Expedition-store modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Expedition-store modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenExpeditionStore();

	if (!TestNotNull(TEXT("The first shop row exists"),
		Modal->GetWidgetFromName(TEXT("ExpeditionStoreRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ExpeditionStoreRowTitle_0")));
	TestTrue(TEXT("The row names an item"),
		Title != nullptr && !Title->GetText().ToString().IsEmpty());
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ExpeditionStoreRowDetail_0")));
	TestTrue(TEXT("The detail shows the price"),
		Detail != nullptr && Detail->GetText().ToString().Contains(TEXT("价格")));
	TestTrue(TEXT("The list is long"),
		Modal->GetWidgetFromName(TEXT("ExpeditionStoreRow_50")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the expedition-store mode"), Modal->IsModalOpen());
	return true;
}

#endif
