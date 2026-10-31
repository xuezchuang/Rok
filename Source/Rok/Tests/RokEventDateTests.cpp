#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_EventDate: rows come from ActivityCalendar.csv (l_nameID, startTime,
// durationTime) and share the definition-list layout with the other previews.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokEventDateLayoutTest,
	"Rok.Ui.EventDate.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokEventDateLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Event-date modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Event-date modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenEventDate();

	if (!TestNotNull(TEXT("The first activity row exists"),
		Modal->GetWidgetFromName(TEXT("EventDateRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("EventDateRowTitle_0")));
	TestTrue(TEXT("The first row shows a resolved activity name"),
		Title != nullptr && !Title->GetText().ToString().IsEmpty());
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("EventDateRowDetail_0")));
	TestTrue(TEXT("The row states its start time and duration"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("开始"))
			&& Detail->GetText().ToString().Contains(TEXT("小时")));
	TestNotNull(TEXT("The footer note is present"),
		Modal->GetWidgetFromName(TEXT("EventDateFooter")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the event-date mode"), Modal->IsModalOpen());
	return true;
}

#endif
