#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ItemExchange: ItemRewardChoice.csv selectable entries.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemExchangeLayoutTest,
	"Rok.Ui.ItemExchange.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemExchangeLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Choice-box modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Choice-box modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemExchange();

	if (!TestNotNull(TEXT("The first choice row exists"),
		RokUiFind::Find(Modal, TEXT("ItemExchangeRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ItemExchangeRowTitle_0")));
	TestTrue(TEXT("The first row names 10000粮食"),
		Title != nullptr && Title->GetText().ToString() == TEXT("10000粮食"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ItemExchangeRowDetail_0")));
	TestTrue(TEXT("The detail states the group and count"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("组"))
			&& Detail->GetText().ToString().Contains(TEXT("数量")));
	TestTrue(TEXT("The list holds more than six entries"),
		RokUiFind::Find(Modal, TEXT("ItemExchangeRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the choice-box mode"), Modal->IsModalOpen());
	return true;
}

#endif
