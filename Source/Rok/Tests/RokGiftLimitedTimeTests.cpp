#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"
#include "../RokCsv.h"

// UI_Win_GiftLimitedTime: RechargeLimitTimeBag.csv carries a multi-line quoted
// description, so the record reader must keep quoted newlines inside one field.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGiftLimitedTimeLayoutTest,
	"Rok.Ui.GiftLimitedTime.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGiftLimitedTimeLayoutTest::RunTest(const FString& Parameters)
{
	TArray<TArray<FString>> Records;
	FRokCsv::ParseRecords(TEXT("a,b\n\"line1\nline2\",2\n"), Records);
	TestEqual(TEXT("Two records are read"), Records.Num(), 2);
	TestEqual(TEXT("The quoted newline stays inside the field"), Records[1][0], FString(TEXT("line1\nline2")));
	TestEqual(TEXT("The column helper returns the first match"), FRokCsv::FindColumn(Records[0], TEXT("b")), 1);
	TestEqual(TEXT("Missing columns report INDEX_NONE"), FRokCsv::FindColumn(Records[0], TEXT("z")), INDEX_NONE);

	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Gift modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Gift modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGiftLimitedTime();

	if (!TestNotNull(TEXT("The first pack row exists"),
		Modal->GetWidgetFromName(TEXT("GiftLimitedTimeRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GiftLimitedTimeRowTitle_0")));
	TestTrue(TEXT("The first row names a pack"),
		Title != nullptr && !Title->GetText().ToString().IsEmpty());
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("GiftLimitedTimeRowDetail_0")));
	TestTrue(TEXT("The detail states price and unlock level"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("价格"))
			&& Detail->GetText().ToString().Contains(TEXT("解锁等级")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the gift mode"), Modal->IsModalOpen());
	return true;
}

#endif
