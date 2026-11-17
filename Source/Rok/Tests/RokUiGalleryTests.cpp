#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "../RokUnityModalWidget.h"
#include "../RokGalleryButton.h"

// Developer gallery: each button opens its own modal through the public entry.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokUiGalleryTest,
	"Rok.Ui.Gallery.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiGalleryTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Gallery modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Gallery modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenUiGallery();

	URokGalleryButton* FirstButton = Cast<URokGalleryButton>(
		RokUiFind::Find(Modal, TEXT("GalleryButton_0")));
	if (!TestNotNull(TEXT("The first gallery button exists"), FirstButton))
	{
		return false;
	}
	TestEqual(TEXT("The button carries its gallery index"), FirstButton->EntryIndex, 0);
	TestNotNull(TEXT("The last gallery button exists"),
		RokUiFind::Find(Modal, TEXT("GalleryButton_18")));

	// Pressing a slot must open that slot's window (index 6 is the compendium).
	URokGalleryButton* CompendiumButton = Cast<URokGalleryButton>(
		RokUiFind::Find(Modal, TEXT("GalleryButton_6")));
	if (!TestNotNull(TEXT("The compendium slot exists"), CompendiumButton))
	{
		return false;
	}
	CompendiumButton->OnClicked.Broadcast();
	TestNotNull(TEXT("Pressing the slot opens the compendium"),
		RokUiFind::Find(Modal, TEXT("ItemCollectionRow_0")));

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the modal"), Modal->IsModalOpen());
	return true;
}

#endif
