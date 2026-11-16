#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"

// Phase 3: Unity MainInterfaceMediator/OpenLodUi HUD rules.
// Unity source:
// - OpenLodUi.csv Position row: City = 0, Tactical = 1.
// - MainInterfaceMediator.SetCityButton(outSide) with limitDxf = city_bound.
// - btns: pl_BuildAndSearch swaps btn_build/btn_search, pl_MapAndCity swaps
//   btn_map/btn_city.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityHudBandSwitchTest,
	"Rok.City.Hud.CityBandSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityHudBandSwitchTest::RunTest(const FString& Parameters)
{
	// CameraParamLod city_bound is dxf 250; city_default is 150.
	TestTrue(TEXT("City default dxf stays inside the city band"),
		URokMainHudWidget::IsCityHudInsideBand(150.0f));
	TestTrue(TEXT("city_bound itself is still inside the city band"),
		URokMainHudWidget::IsCityHudInsideBand(250.0f));
	TestFalse(TEXT("Just past city_bound is the overview band"),
		URokMainHudWidget::IsCityHudInsideBand(250.1f));
	TestFalse(TEXT("The 313.6 zoomed-out reference state is the overview band"),
		URokMainHudWidget::IsCityHudInsideBand(313.6f));
	TestTrue(TEXT("A non-finite dxf is treated as the default city view"),
		URokMainHudWidget::IsCityHudInsideBand(FMath::Pow(10.0f, 100.0f) * 0.0f));

	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("City HUD can be constructed"), Hud))
	{
		return false;
	}
	TestTrue(TEXT("City HUD initializes its runtime WidgetTree"), Hud->Initialize());
	Hud->TakeWidget();

	UWidget* Position = Hud->GetWidgetFromName(TEXT("PositionGroup"));
	UButton* BuildButton = Cast<UButton>(Hud->GetWidgetFromName(TEXT("BuildCityButton")));
	UButton* GlobeButton = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldGlobeButton")));
	UButton* SearchButton = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldSearchButton")));
	UButton* CityButton = Cast<UButton>(Hud->GetWidgetFromName(TEXT("CityWorldButton")));
	if (!TestNotNull(TEXT("Coordinate box"), Position)
		|| !TestNotNull(TEXT("Hammer"), BuildButton)
		|| !TestNotNull(TEXT("World globe"), GlobeButton)
		|| !TestNotNull(TEXT("Search magnifier"), SearchButton)
		|| !TestNotNull(TEXT("Castle"), CityButton))
	{
		return false;
	}

	// The button pair flips on dxf OR a viewport loss; the coordinate box only
	// follows the camera band.
	TestFalse(TEXT("Inside band and city on screen keeps the hammer pair"),
		URokMainHudWidget::IsCityButtonOutsideBand(150.0f, true));
	TestTrue(TEXT("Inside band but city panned off screen flips to search/castle"),
		URokMainHudWidget::IsCityButtonOutsideBand(150.0f, false));
	TestTrue(TEXT("Outside band always flips the pair"),
		URokMainHudWidget::IsCityButtonOutsideBand(313.6f, true));

	Hud->ApplyCityHudBand(true);
	Hud->ApplyCityHudButtons(false);
	TestTrue(TEXT("Inside band records its state"), Hud->IsCityHudInsideBandAppliedForTesting());
	TestEqual(TEXT("Inside band hides the coordinate box"),
		Position->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Inside band shows the build hammer"),
		BuildButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Inside band shows the world globe"),
		GlobeButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Inside band hides the search magnifier"),
		SearchButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Inside band hides the castle"),
		CityButton->GetVisibility(), ESlateVisibility::Collapsed);

	// Same camera band, city panned out of view: Unity's viewport test flips
	// only the button pair and leaves the coordinate box hidden.
	Hud->ApplyCityHudButtons(true);
	TestEqual(TEXT("Panned-away city keeps the coordinate box hidden"),
		Position->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Panned-away city hides the build hammer"),
		BuildButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Panned-away city hides the world globe"),
		GlobeButton->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Panned-away city shows the search magnifier"),
		SearchButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Panned-away city shows the castle"),
		CityButton->GetVisibility(), ESlateVisibility::Visible);

	Hud->ApplyCityHudBand(false);
	TestFalse(TEXT("Outside band records its state"), Hud->IsCityHudInsideBandAppliedForTesting());
	TestEqual(TEXT("Outside band reveals the coordinate box"),
		Position->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Outside band keeps the search magnifier"),
		SearchButton->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Outside band keeps the castle"),
		CityButton->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityHudConstructionClockTest,
	"Rok.City.Hud.ConstructionClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityHudConstructionClockTest::RunTest(const FString& Parameters)
{
	// UI_Tip_BuildingBar lbl_time = ClientUtils.FormatCountDown((int)leftTime).
	TestEqual(TEXT("Zero seconds formats as 00:00:00"),
		URokMainHudWidget::FormatRemainingClock(0.0f), FString(TEXT("00:00:00")));
	TestEqual(TEXT("Seconds truncate like the source int cast"),
		URokMainHudWidget::FormatRemainingClock(1.9f), FString(TEXT("00:00:01")));
	TestEqual(TEXT("Under a minute"),
		URokMainHudWidget::FormatRemainingClock(59.4f), FString(TEXT("00:00:59")));
	TestEqual(TEXT("Past an hour shows the hour field"),
		URokMainHudWidget::FormatRemainingClock(3661.0f), FString(TEXT("01:01:01")));
	TestEqual(TEXT("Past a day uses Language 128004"),
		URokMainHudWidget::FormatRemainingClock(90061.0f), FString(TEXT("1天 01:01:01")));
	TestEqual(TEXT("Negative remaining clamps to zero"),
		URokMainHudWidget::FormatRemainingClock(-5.0f), FString(TEXT("00:00:00")));

	// RefreshhudView: (costTime - leftTime) / costTime, and 1 once leftTime < 0.
	TestEqual(TEXT("A just-started build has an empty bar"),
		URokMainHudWidget::GetBuildBarProgress(2.0f, 2.0f), 0.0f);
	TestEqual(TEXT("Half way"), URokMainHudWidget::GetBuildBarProgress(2.0f, 1.0f), 0.5f);
	TestEqual(TEXT("Overdue fills the bar"), URokMainHudWidget::GetBuildBarProgress(2.0f, 0.0f), 1.0f);
	TestEqual(TEXT("A zero-length build is full"), URokMainHudWidget::GetBuildBarProgress(0.0f, 0.0f), 1.0f);
	TestEqual(TEXT("Remaining above the total clamps to empty"),
		URokMainHudWidget::GetBuildBarProgress(2.0f, 3.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityHudDebugGridModeTest,
	"Rok.City.Hud.DebugGridMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityHudDebugGridModeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Normal play draws no building grid"),
		ARokPlayerController::ResolveBuildingGridDebugMode(0, false), 0);
	TestEqual(TEXT("Placement shows at least the selected footprint"),
		ARokPlayerController::ResolveBuildingGridDebugMode(0, true), 1);
	TestEqual(TEXT("A diagnostic override still draws in normal play"),
		ARokPlayerController::ResolveBuildingGridDebugMode(2, false), 2);
	TestEqual(TEXT("Placement keeps the all-buildings diagnostic"),
		ARokPlayerController::ResolveBuildingGridDebugMode(2, true), 2);
	TestEqual(TEXT("Out-of-range values clamp to the all-buildings mode"),
		ARokPlayerController::ResolveBuildingGridDebugMode(9, false), 2);
	TestEqual(TEXT("Negative values fall back to off"),
		ARokPlayerController::ResolveBuildingGridDebugMode(-4, false), 0);
	return true;
}

#endif
