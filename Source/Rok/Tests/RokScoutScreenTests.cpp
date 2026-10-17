#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokScoutCatalog.h"
#include "../RokScoutScreenWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokScoutSourceLevelTest, "Rok.Ui.Scout.LevelAndAuthorityBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokScoutSourceLevelTest::RunTest(const FString&)
{
	FRokScoutCatalog Catalog; FString Error;
	if (!TestTrue(TEXT("Source catalog loads"), Catalog.Load(Error))) { AddError(Error); return false; }
	TestEqual(TEXT("All source camp levels loaded"), Catalog.Levels.Num(), 25);
	TestNull(TEXT("Unknown level cannot use level one"), Catalog.FindLevel(INDEX_NONE));
	TestNull(TEXT("Absent camp is not level one"), Catalog.FindLevel(0));
	TestNull(TEXT("Unsupported level cannot clamp to max"), Catalog.FindLevel(26));
	for (int32 Level = 1; Level <= 25; ++Level)
	{
		const auto* Row = Catalog.FindLevel(Level);
		if (!TestNotNull(TEXT("Source config level exists"), Row)) return false;
		TestEqual(TEXT("Source capacity boundary"), Row->Capacity, Level < 5 ? 1 : Level < 11 ? 2 : 3);
	}
	auto* Screen = NewObject<URokScoutScreenWidget>(); Screen->Initialize(); Screen->Open();
	auto* Camp = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ScoutCampConfiguration")));
	if (!TestNotNull(TEXT("Unknown camp state is visible"), Camp)) return false;
	TestEqual(TEXT("Unknown camp has no invented level"), Camp->GetText().ToString(), FString(TEXT("营地等级待同步")));
	TestNotNull(TEXT("Unknown server queue is explicit"), Screen->GetWidgetFromName(TEXT("ScoutQueueUnavailable")));
	TestNull(TEXT("Capacity does not create an idle Scout A"), Screen->GetWidgetFromName(TEXT("ScoutRow_1")));
	TestNull(TEXT("No dispatch button without a queue source"), Screen->GetWidgetFromName(TEXT("ScoutDispatchButton")));
	Screen->Open(5);
	Camp = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ScoutCampConfiguration")));
	TestTrue(TEXT("Known level shows source capacity, labelled capacity"), Camp && Camp->GetText().ToString().Contains(TEXT("可容纳 2 名斥候")));
	TestNotNull(TEXT("Known building still does not imply known queue"), Screen->GetWidgetFromName(TEXT("ScoutQueueUnavailable")));
	Screen->Open(0);
	Camp = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ScoutCampConfiguration")));
	TestEqual(TEXT("Known absent camp distinct from unknown"), Camp->GetText().ToString(), FString(TEXT("斥候营地尚未建造")));
	Screen->CloseImmediately(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokScoutPageLifecycleTest, "Rok.Ui.Scout.PageLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokScoutPageLifecycleTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokScoutScreenWidget>(); Screen->Initialize(); Screen->Open();
	int32 ClosedCount = 0; Screen->OnClosed.AddLambda([&ClosedCount]() { ++ClosedCount; });
	auto Click = [this, Screen](const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast(); return true;
	};
	auto* List = Screen->GetWidgetFromName(TEXT("ScoutListScroll"));
	auto* Description = Screen->GetWidgetFromName(TEXT("ScoutDescriptionScroll"));
	auto* Root = Screen->GetWidgetFromName(TEXT("ScoutRootCanvas"));
	if (!TestNotNull(TEXT("Actual list"), List) || !TestNotNull(TEXT("Actual description"), Description)) return false;
	TestNull(TEXT("Source description has no scroll/overscroll behavior"), Cast<UScrollBox>(Description));
	auto* DescriptionBounds = Cast<USizeBox>(Description);
	if (!TestNotNull(TEXT("Source fixed description bounds"), DescriptionBounds)) return false;
	TestEqual(TEXT("Description preserves source height while pages animate"), DescriptionBounds->GetHeightOverride(), 520.0f);
	TestEqual(TEXT("Description cannot paint below its source text rectangle"), DescriptionBounds->GetClipping(), EWidgetClipping::ClipToBoundsAlways);
	TestEqual(TEXT("Default Empty source state means initial list fully opaque"), List->GetRenderOpacity(), 1.0f);
	TestEqual(TEXT("Input mask parent remains unscaled"), Root->GetRenderTransform().Scale, FVector2D(1));
	Screen->AdvanceForTesting(0.3f);
	if (!Click(TEXT("ScoutInfoButton"))) return false;
	TestTrue(TEXT("Description navigation state"), Screen->IsDescriptionVisible());
	TestEqual(TEXT("Hidden list stops intercepting input"), List->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Description Show starts with alpha zero"), Description->GetRenderOpacity(), 0.0f);
	TestEqual(TEXT("Description enters source right"), Description->GetRenderTransform().Translation.X, 50.0);
	Screen->AdvanceForTesting(0.125f);
	TestEqual(TEXT("Description source midpoint alpha"), Description->GetRenderOpacity(), 0.5f);
	TestEqual(TEXT("Description source midpoint position"), Description->GetRenderTransform().Translation.X, 25.0);
	if (!Click(TEXT("ScoutBackButton"))) return false;
	TestFalse(TEXT("Back restores list state"), Screen->IsDescriptionVisible());
	TestEqual(TEXT("Back hides description"), Description->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Return list starts source left"), List->GetRenderTransform().Translation.X, -50.0);
	Screen->AdvanceForTesting(0.125f);
	TestEqual(TEXT("Return list midpoint"), List->GetRenderOpacity(), 0.5f);
	if (!Click(TEXT("ScoutInfoButton"))) return false;
	TestEqual(TEXT("Quick reverse starts target source clip afresh"), Description->GetRenderOpacity(), 0.0f);
	Screen->AdvanceForTesting(0.3f);
	if (!Click(TEXT("ScoutCloseButton"))) return false;
	TestTrue(TEXT("Close keeps window through source exit clip"), Screen->IsScreenOpen());
	Screen->AdvanceForTesting(0.2f);
	TestFalse(TEXT("Exit clip closes window"), Screen->IsScreenOpen());
	TestEqual(TEXT("Close delegate fired once"), ClosedCount, 1);
	Screen->CloseImmediately(); TestEqual(TEXT("Repeated close is safe"), ClosedCount, 1);
	Screen->Open(); List = Screen->GetWidgetFromName(TEXT("ScoutListScroll"));
	TestEqual(TEXT("Reopen clears old child motion"), List->GetRenderOpacity(), 1.0f);
	if (!Click(TEXT("ScoutBackgroundClose"))) return false;
	Screen->AdvanceForTesting(0.2f);
	TestEqual(TEXT("Source touch-close mask also closes"), ClosedCount, 2);
	Screen->OnClosed.Clear(); return true;
}
#endif
