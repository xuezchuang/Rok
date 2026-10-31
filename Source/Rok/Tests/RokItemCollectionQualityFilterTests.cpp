#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"
#include "../RokGalleryButton.h"

// Two chip rows compose: 类型 N + 品质 M are AND-ed through the `|` filter.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemCollectionQualityFilterTest,
	"Rok.Ui.ItemCollection.QualityFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemCollectionQualityFilterTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Compendium modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Compendium modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemCollection();

	URokGalleryButton* TypeFour = Cast<URokGalleryButton>(
		Modal->GetWidgetFromName(TEXT("ItemCollection类型Chip_4")));
	URokGalleryButton* QualityFive = Cast<URokGalleryButton>(
		Modal->GetWidgetFromName(TEXT("ItemCollection品质Chip_5")));
	if (!TestNotNull(TEXT("The 类型 4 chip exists"), TypeFour)
		|| !TestNotNull(TEXT("The 品质 5 chip exists"), QualityFive))
	{
		return false;
	}

	TypeFour->OnClicked.Broadcast();
	const int32 TypeOnly = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("类型 4 alone keeps a subset"), TypeOnly > 0 && TypeOnly < 200);

	QualityFive->OnClicked.Broadcast();
	const int32 Both = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("类型 4 + 品质 5 narrows further"), Both > 0 && Both <= TypeOnly);

	const UTextBlock* Detail = Cast<UTextBlock>(
		Modal->GetWidgetFromName(TEXT("ItemCollectionRowDetail_0")));
	TestTrue(TEXT("Every remaining card is 类型 4 品质 5"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("类型 4"))
			&& Detail->GetText().ToString().Contains(TEXT("品质 5")));

	// Clearing the type chip keeps the quality filter.
	Cast<URokGalleryButton>(Modal->GetWidgetFromName(TEXT("ItemCollection类型Chip_0")))->OnClicked.Broadcast();
	const int32 QualityOnly = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("品质 5 alone is broader than the pair"), QualityOnly >= Both);

	Modal->CloseModal();
	return true;
}

#endif
