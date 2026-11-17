#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ItemExchange: quantity stepper (`btn_substract` / `ipt_num` / `btn_add`).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemExchangeStepperTest,
	"Rok.Ui.ItemExchange.Stepper",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemExchangeStepperTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Exchange modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Exchange modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemExchange();

	UButton* Plus = Cast<UButton>(RokUiFind::Find(Modal, TEXT("ItemExchangePlus")));
	UButton* Minus = Cast<UButton>(RokUiFind::Find(Modal, TEXT("ItemExchangeMinus")));
	const UTextBlock* Count = Cast<UTextBlock>(RokUiFind::Find(Modal, TEXT("ItemExchangeCount")));
	if (!TestNotNull(TEXT("The plus button exists"), Plus)
		|| !TestNotNull(TEXT("The minus button exists"), Minus)
		|| !TestNotNull(TEXT("The count label exists"), Count))
	{
		return false;
	}
	TestEqual(TEXT("The count starts at one"), Count->GetText().ToString(), FString(TEXT("1")));

	Plus->OnClicked.Broadcast();
	Plus->OnClicked.Broadcast();
	TestEqual(TEXT("Two plus clicks make three"), Count->GetText().ToString(), FString(TEXT("3")));

	Minus->OnClicked.Broadcast();
	TestEqual(TEXT("One minus click makes two"), Count->GetText().ToString(), FString(TEXT("2")));

	// Clamp at one.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		Minus->OnClicked.Broadcast();
	}
	TestEqual(TEXT("The count never drops below one"), Count->GetText().ToString(), FString(TEXT("1")));

	const UTextBlock* Change = Cast<UTextBlock>(RokUiFind::Find(Modal, TEXT("ItemExchangeChange")));
	TestTrue(TEXT("The summary states the local-preview boundary"),
		Change != nullptr && Change->GetText().ToString().Contains(TEXT("服务器")));

	Modal->CloseModal();
	return true;
}

#endif
