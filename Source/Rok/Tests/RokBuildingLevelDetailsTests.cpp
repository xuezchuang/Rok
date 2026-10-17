#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokBuildingLevelDetails.h"
#include "../RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingLevelDetailsTest,
	"Rok.Ui.Building.LevelDetails", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokBuildingLevelDetailsTest::RunTest(const FString&)
{
	TArray<FString> Headers; TArray<FRokBuildingLevelDetailsRow> Rows; FString Error;
	for (int32 Type = 2; Type <= 5; ++Type)
	{
		if (!TestTrue(TEXT("Source table loads"), RokBuildingLevelDetails::Load(Type, Headers, Rows, Error))) return false;
		TestEqual(TEXT("Four columns"), Headers.Num(), 4);
		TestEqual(TEXT("25 levels"), Rows.Num(), 25);
		TestEqual(TEXT("Last level identity"), Rows.Last().Level, 25);
	}
	TArray<FRokBarracksLevelDetailsRow> BarracksRows;
	FString BarracksName, BarracksDescription;
	if (!TestTrue(TEXT("Barracks source table loads"), RokBuildingLevelDetails::LoadBarracks(Headers, BarracksRows, Error, &BarracksName, &BarracksDescription))) return false;
	TestEqual(TEXT("Barracks has four source headers"), Headers.Num(), 4);
	TestEqual(TEXT("Barracks has all source levels"), BarracksRows.Num(), 25);
	TestTrue(TEXT("Barracks has localized identity"), !BarracksName.IsEmpty() && !BarracksDescription.IsEmpty());
	if (BarracksRows.Num() == 25)
	{
		TestEqual(TEXT("Level 1 train capacity"), BarracksRows[0].InfantryTrainNumber, 20);
		TestEqual(TEXT("Level 2 train capacity"), BarracksRows[1].InfantryTrainNumber, 50);
		TestTrue(TEXT("Zero attack multiplier is visually blank"), BarracksRows[0].InfantryAttackText.IsEmpty());
		TestEqual(TEXT("Level 1 row power"), BarracksRows[0].Power, 5);
		TestEqual(TEXT("Level 2 power is direct row value"), BarracksRows[1].Power, 10);
		TestEqual(TEXT("First nonzero attack raw permille"), BarracksRows[9].InfantryAttackMulti, 5);
		TestEqual(TEXT("First nonzero attack display"), BarracksRows[9].InfantryAttackText, FString(TEXT("0.5%")));
		TestEqual(TEXT("Level 21 fractional percent"), BarracksRows[20].InfantryAttackText, FString(TEXT("1.5%")));
		TestEqual(TEXT("Level 25 train capacity"), BarracksRows[24].InfantryTrainNumber, 2000);
		TestEqual(TEXT("Level 25 percent"), BarracksRows[24].InfantryAttackText, FString(TEXT("2%")));
		TestEqual(TEXT("Level 25 row power"), BarracksRows[24].Power, 592326);
	}
	TestFalse(TEXT("Resource row API stays type-specific"), RokBuildingLevelDetails::Load(9, Headers, Rows, Error));
	TestFalse(TEXT("Unsupported type explicitly fails"), RokBuildingLevelDetails::Load(1, Headers, Rows, Error));
	TestTrue(TEXT("Failure has diagnostic"), !Error.IsEmpty());
	TestEqual(TEXT("Level source entry"), RokBuildingLevelDetails::Animate(0, true).X, 50.f);
	TestEqual(TEXT("Description source entry"), RokBuildingLevelDetails::Animate(0, false).X, -50.f);
	TestEqual(TEXT("Source Hermite midpoint alpha"), RokBuildingLevelDetails::Animate(.125f, true).Alpha, .5f);
	TestEqual(TEXT("Source midpoint translation"), RokBuildingLevelDetails::Animate(.125f, false).X, -25.f);
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	// This isolated test has no viewport owner. Retain the Slate tree instead of
	// discarding TakeWidget's temporary reference before dynamic children are built.
	const TSharedRef<SWidget> ModalSlate = Modal->TakeWidget();
	Modal->OpenResourceBuildingInfoReferencePreview(2); Modal->AdvanceWindowAnimationForTesting(1);
	auto* Info = Cast<UButton>(Modal->GetWidgetFromName(TEXT("BuildingLevelInfoButton")));
	auto* Back = Cast<UButton>(Modal->GetWidgetFromName(TEXT("BuildingLevelBackButton")));
	auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BuildingLevelDetailsScroll")));
	if (!TestNotNull(TEXT("Source detail entry"), Info) || !TestNotNull(TEXT("Source back"), Back) || !TestNotNull(TEXT("Scrollable source rows"), Scroll)) return false;
	TestNull(TEXT("Unknown current level does not highlight a fake level one"), Modal->GetWidgetFromName(TEXT("BuildingLevelCurrentRow")));
	TestEqual(TEXT("Initial description skips child Show"), Modal->BuildingDescriptionRoot->GetRenderOpacity(), 1.f);
	Info->OnClicked.Broadcast();
	TestEqual(TEXT("Level page begins at +50"), Modal->BuildingLevelDetailsRoot->GetRenderTransform().Translation.X, 50.0);
	TestEqual(TEXT("Back is outside animated level subtree"), Back->GetParent(), static_cast<UPanelWidget*>(Modal->ModeCanvas));
	Modal->AdvanceWindowAnimationForTesting(.125f);
	TestEqual(TEXT("Real animation clock drives alpha"), Modal->BuildingLevelDetailsRoot->GetRenderOpacity(), .5f);
	const TSharedRef<SWidget> ScrollSlate = Scroll->TakeWidget();
	TestTrue(TEXT("Scroll has a live Slate control"), Scroll->GetCachedWidget().IsValid());
	Scroll->SetScrollOffset(400);
	TestEqual(TEXT("Scroll request is observable before navigation"), Scroll->GetScrollOffset(), 400.f);
	Back->OnClicked.Broadcast();
	TestEqual(TEXT("Immediate reversal uses content clip start"), Modal->BuildingDescriptionRoot->GetRenderTransform().Translation.X, -50.0);
	Info->OnClicked.Broadcast();
	TestEqual(TEXT("Returning to levels retains the same list"), Modal->BuildingLevelDetailsScroll.Get(), Scroll);
	TestEqual(TEXT("Returning retains the live Slate scroll control"), Scroll->GetCachedWidget().Get(), ScrollSlate.operator->());
	TestEqual(TEXT("Returning retains requested scroll offset"), Scroll->GetScrollOffset(), 400.f);
	TestNotNull(TEXT("All levels are built"), Modal->GetWidgetFromName(TEXT("BuildingLevelCell_25_3")));
	Modal->OpenPlayerProfile();
	TestNull(TEXT("Switching modal clears animation target"), Modal->BuildingLevelDetailsRoot.Get());
	// Explicit isolated presentation fixture tests highlight semantics without inventing an owned actor.
	auto* Known = NewObject<URokUnityModalWidget>(); Known->Initialize(); Known->TakeWidget();
	Known->BuildResourceBuildingInfo(2, 25);
	TestNotNull(TEXT("Known current level highlights source row"), Known->GetWidgetFromName(TEXT("BuildingLevelCurrentRow")));
	return true;
}
#endif
