#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "../RokUnityModalWidget.h"

// UI_Win_GuildStore: AllianceShopItem.csv rows joined with Item.csv names.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokGuildStoreLayoutTest,
	"Rok.Ui.GuildStore.Layout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGuildStoreLayoutTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Guild-store modal can be constructed"), Modal))
	{
		return false;
	}
	TestTrue(TEXT("Guild-store modal initializes its runtime WidgetTree"), Modal->Initialize());
	Modal->TakeWidget();
	Modal->OpenGuildStore();

	if (!TestNotNull(TEXT("The first shop row exists"),
		RokUiFind::Find(Modal, TEXT("GuildStoreRow_0"))))
	{
		return false;
	}
	const UTextBlock* Title = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GuildStoreRowTitle_0")));
	TestTrue(TEXT("The first row shows the item name from Item.csv"),
		Title != nullptr && Title->GetText().ToString() == TEXT("天赋重置"));
	const UTextBlock* Detail = Cast<UTextBlock>(
		RokUiFind::Find(Modal, TEXT("GuildStoreRowDetail_0")));
	TestTrue(TEXT("The detail shows both prices"),
		Detail != nullptr
			&& Detail->GetText().ToString().Contains(TEXT("进货价"))
			&& Detail->GetText().ToString().Contains(TEXT("售价")));
	TestTrue(TEXT("The list holds more than six rows"),
		RokUiFind::Find(Modal, TEXT("GuildStoreRow_6")) != nullptr);

	Modal->CloseModal();
	TestFalse(TEXT("Closing clears the guild-store mode"), Modal->IsModalOpen());
	return true;
}

#endif
