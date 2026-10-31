#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_ItemCollection: the scroll list must expose far more than six rows.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemCollectionLayoutTest,
	"Rok.Ui.ItemCollection.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemCollectionLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Item-collection modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Item-collection modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemCollection();

	if (!TestNotNull(TEXT("The first item row exists"),
		Modal->GetWidgetFromName(TEXT("ItemCollectionRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ItemCollectionRowTitle_0")));
	TestTrue(TEXT("The first row shows a resolved item name"),
		Title != nullptr && !Title->GetText().ToString().IsEmpty());
	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ItemCollectionRowDetail_0")));
	TestTrue(TEXT("The detail shows type and quality"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("类型"))
			&& Detail->GetText().ToString().Contains(TEXT("品质")));
	TestNotNull(TEXT("The scroll view exists"),
		Modal->GetWidgetFromName(TEXT("ItemCollectionScroll")));
	TestTrue(TEXT("The list is long (row 199 present)"),
		Modal->GetWidgetFromName(TEXT("ItemCollectionRow_199")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the compendium mode"), Modal->IsModalOpen());
	return true;
}

#endif
