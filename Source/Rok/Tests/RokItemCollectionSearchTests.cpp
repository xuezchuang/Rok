#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// The shared definition list can filter its rows; long windows (item compendium)
// expose a search box wired to the same filter.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemCollectionSearchTest,
	"Rok.Ui.ItemCollection.Search",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemCollectionSearchTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Compendium modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Compendium modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemCollection();

	TestNotNull(TEXT("The search box exists"),
		RokUiFind::Find(Modal, TEXT("ItemCollectionSearch")));
	const UTextBlock* Before = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ItemCollectionRowTitle_0")));
	TestTrue(TEXT("The unfiltered first row shows an item"),
		Before != nullptr && !Before->GetText().ToString().IsEmpty());

	const int32 FullCount = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("The unfiltered list renders the 200-row cap"), FullCount == 200);

	Modal->SetDefinitionFilter(TEXT("加速"));
	const int32 FilteredCount = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("The filter keeps a non-empty subset"), FilteredCount > 0);
	TestTrue(TEXT("The filter drops rows"), FilteredCount < FullCount);

	Modal->SetDefinitionFilter(TEXT("不存在的东西"));
	TestEqual(TEXT("A filter with no match renders nothing"), Modal->GetDefinitionRowCount(), 0);

	Modal->SetDefinitionFilter(FString());
	TestEqual(TEXT("Clearing the filter restores the full list"),
		Modal->GetDefinitionRowCount(), FullCount);

	Modal->CloseModal();
	return true;
}

#endif
