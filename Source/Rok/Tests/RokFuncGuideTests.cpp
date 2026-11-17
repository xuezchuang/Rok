#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_FuncGuide: Guide.csv step rows (duplicate `step` header, Chinese titles).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokFuncGuideLayoutTest,
	"Rok.Ui.FuncGuide.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFuncGuideLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Guide modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Guide modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenFuncGuide();

	if (!TestNotNull(TEXT("The first guide row exists"),
		RokUiFind::Find(Modal, TEXT("FuncGuideRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("FuncGuideRowTitle_0")));
	TestTrue(TEXT("The first row is the opening guide step"),
		Title != nullptr
			&& Title->GetText().ToString().Contains(TEXT("开场引导")));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("FuncGuideRowDetail_0")));
	TestTrue(TEXT("The detail states the stage"),
		Detail != nullptr && Detail->GetText().ToString().Contains(TEXT("阶段")));
	TestTrue(TEXT("The list holds more than six steps"),
		RokUiFind::Find(Modal, TEXT("FuncGuideRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the guide mode"), Modal->IsModalOpen());
	return true;
}

#endif
