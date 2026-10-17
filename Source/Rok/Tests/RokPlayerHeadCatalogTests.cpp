#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokPlayerHeadCatalog.h"
#include "../RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPlayerHeadCatalogTest,
	"Rok.Ui.PlayerHead.SourceCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPlayerHeadCatalogTest::RunTest(const FString&)
{
	TArray<FRokPlayerHeadRow> Rows; FString Error;
	const bool bLoaded = RokPlayerHeadCatalog::Load(Rows, Error);
	if (!TestTrue(TEXT("Load generated PlayerHead catalog"), bLoaded)) { AddError(Error); return false; }
	TestEqual(TEXT("Complete source catalog"), Rows.Num(), 28);
	const auto* Default = Rows.FindByPredicate([](const auto& Row) { return Row.Id == 2000; });
	const auto* Free = Rows.FindByPredicate([](const auto& Row) { return Row.Id == 2001; });
	const auto* Locked = Rows.FindByPredicate([](const auto& Row) { return Row.Id == 2015; });
	if (!TestNotNull(TEXT("Default frame"), Default) || !TestNotNull(TEXT("First catalog frame"), Free)
		|| !TestNotNull(TEXT("Last catalog frame"), Locked)) return false;
	TestTrue(TEXT("2000 is free and uses real default sprite"), Default->bFree && Default->bArtAvailable);
	TestEqual(TEXT("2000 exact atlas rect"), Default->Rect, FIntRect(214, 984, 425, 1195));
	TestTrue(TEXT("2001 free does not mean its missing sprite can be replaced"), Free->bFree && !Free->bArtAvailable);
	TestEqual(TEXT("2001 real atlas identity"), Free->Icon, FString(TEXT("player_headfra[player_headfra_1000]")));
	TestFalse(TEXT("2015 requires ownership query"), Locked->bFree);
	TestFalse(TEXT("Missing atlas not replaced with another portrait frame"), Locked->bArtAvailable);

	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize(); Modal->TakeWidget();
	Modal->OpenPlayerHeadSelection(); Modal->AdvanceWindowAnimationForTesting(1);
	auto* Tab = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerHeadPageButton_1")));
	if (!TestNotNull(TEXT("Actual frame tab"), Tab)) return false;
	Tab->OnClicked.Broadcast();
	auto* Last = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerHeadChoiceButton_15")));
	if (!TestNotNull(TEXT("All sixteen frame choices built"), Last)) return false;
	Last->OnClicked.Broadcast();
	TestNotNull(TEXT("Missing frame shown explicitly"), Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewMissingArt")));
	auto* Name = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewName")));
	if (TestNotNull(TEXT("Selected row name"), Name)) TestEqual(TEXT("Selection uses source localized name"), Name->GetText().ToString(), Locked->Name);
	auto* Confirm = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerHeadConfirmButton")));
	if (TestNotNull(TEXT("Confirm boundary"), Confirm)) TestFalse(TEXT("Browsing locked frame cannot submit"), Confirm->GetIsEnabled());
	TestNotNull(TEXT("Four rows scroll"), Modal->GetWidgetFromName(TEXT("PlayerHeadGridScroll")));
	Modal->CloseModal();
	return true;
}
#endif
