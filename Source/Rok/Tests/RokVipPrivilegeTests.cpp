#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_Vip: VipAtt.csv privileges per level, with the search box enabled.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokVipPrivilegeLayoutTest,
	"Rok.Ui.VipPrivilege.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokVipPrivilegeLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("VIP modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("VIP modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenVipPrivilege();

	if (!TestNotNull(TEXT("The first privilege row exists"),
		RokUiFind::Find(Modal, TEXT("VipPrivilegeRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("VipPrivilegeRowTitle_0")));
	TestTrue(TEXT("The first row names 食物产量"),
		Title != nullptr && Title->GetText().ToString() == TEXT("食物产量"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("VipPrivilegeRowDetail_0")));
	TestTrue(TEXT("The detail shows the granted value"),
		Detail != nullptr && Detail->GetText().ToString().StartsWith(TEXT("+")));

	TestNotNull(TEXT("The search box exists"),
		RokUiFind::Find(Modal, TEXT("VipPrivilegeSearch")));
	const int32 FullCount = Modal->GetDefinitionRowCount();
	Modal->SetDefinitionFilter(TEXT("产量"));
	const int32 FilteredCount = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("The filter keeps production rows"),
		FilteredCount > 0 && FilteredCount < FullCount);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the VIP mode"), Modal->IsModalOpen());
	return true;
}

#endif
