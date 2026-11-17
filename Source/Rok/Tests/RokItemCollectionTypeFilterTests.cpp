#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"
#include "../RokGalleryButton.h"

// Item compendium category chips reuse the list filter (`类型 N` substring).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokItemCollectionTypeFilterTest,
	"Rok.Ui.ItemCollection.TypeFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokItemCollectionTypeFilterTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Compendium modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Compendium modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenItemCollection();

	URokGalleryButton* TypeFourChip = Cast<URokGalleryButton>(
		RokUiFind::Find(Modal, TEXT("ItemCollection类型Chip_4")));
	URokGalleryButton* AllChip = Cast<URokGalleryButton>(
		RokUiFind::Find(Modal, TEXT("ItemCollection类型Chip_0")));
	if (!TestNotNull(TEXT("The 类型 4 chip exists"), TypeFourChip)
		|| !TestNotNull(TEXT("The 全部 chip exists"), AllChip))
	{
		return false;
	}
	const int32 FullCount = Modal->GetDefinitionRowCount();
	TestEqual(TEXT("The unfiltered grid renders the 200-row cap"), FullCount, 200);

	TypeFourChip->OnClicked.Broadcast();
	const int32 FilteredCount = Modal->GetDefinitionRowCount();
	TestTrue(TEXT("The 类型 4 chip keeps a non-empty subset"), FilteredCount > 0);
	TestTrue(TEXT("The 类型 4 chip drops other types"), FilteredCount < FullCount);
	const UTextBlock* FirstDetail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("ItemCollectionRowDetail_0")));
	TestTrue(TEXT("The first filtered card is a 类型 4 item"),
		FirstDetail != nullptr && FirstDetail->GetText().ToString().Contains(TEXT("类型 4")));

	AllChip->OnClicked.Broadcast();
	TestEqual(TEXT("The 全部 chip restores the full grid"),
		Modal->GetDefinitionRowCount(), FullCount);

	Modal->CloseModal();
	return true;
}

#endif
