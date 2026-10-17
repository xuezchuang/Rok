#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "../RokWorldMapHudWidget.h"

#include "../RokWorldMapHomeIndicator.h"
#include "../RokMainInterfaceModuleAnimation.h"
#include "../RokWorldExplorationState.h"
#include "../RokWorldMapPlayerController.h"
#include "../RokUiAnimatedButton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldChatMarkerTest,
	"Rok.WorldMap.Hud.ChatChannelMarker", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokWorldChatMarkerTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const auto Slate = Hud->TakeWidget();
	auto* Marker = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldMapChatWorldMarker")));
	if (!TestNotNull(TEXT("Persistent world chat source marker"), Marker)) return false;
	auto* Slot = Cast<UCanvasPanelSlot>(Marker->Slot);
	if (!TestNotNull(TEXT("Marker canvas slot"), Slot)) return false;
	TestEqual(TEXT("Source bar size"), Slot->GetSize(), FVector2D(30, 4));
	TestTrue(TEXT("Source mirrored chat-bar position"), Slot->GetPosition().Equals(FVector2D(202.6f, -84.3f), 0.001));
	TestEqual(TEXT("Bottom anchored"), Slot->GetAnchors().Minimum, FVector2D(0, 1));
	TestEqual(TEXT("Decoration cannot swallow chat clicks"), Marker->GetVisibility(), ESlateVisibility::HitTestInvisible);
	for (const float Dxf : {1537.0f, 3500.0f, 6000.0f})
	{
		Hud->UpdateZoomPresentation(Dxf);
		TestEqual(TEXT("Zoom does not hide the chat channel marker"), Marker->GetVisibility(), ESlateVisibility::HitTestInvisible);
	}
	TestNotNull(TEXT("Chat action remains a button"), Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMapChatButton"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldObjectPopupStyleTest,
	"Rok.WorldMap.Hud.ObjectPopupSourceStyle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldObjectPopupStyleTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	auto* Panel = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoPanel")));
	if (!TestNotNull(TEXT("Atlas-sliced popup panel"), Panel)) return false;
	TestEqual(TEXT("Nine source patches plus exclusive normal/description pages"), Panel->GetChildrenCount(), 11);
	auto* Corner = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoPatch_0_0")));
	if (!TestNotNull(TEXT("Top-left source patch"), Corner)) return false;
	auto* Slot = Cast<UCanvasPanelSlot>(Corner->Slot);
	if (!TestNotNull(TEXT("Corner canvas slot"), Slot)) return false;
	TestEqual(TEXT("Unity corner stays 25x21, not atlas-sized"), Slot->GetSize(), FVector2D(25, 21));
	TestEqual(TEXT("Atlas patches use Image, not whole-atlas Box margins"), Corner->GetBrush().DrawAs, ESlateBrushDrawType::Image);
	auto* Name = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoName")));
	if (!TestNotNull(TEXT("Source popup title"), Name)) return false;
	TestTrue(TEXT("Dark Unity title on light source paper"), Name->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1)));
	TestEqual(TEXT("No HUD-style text shadow on paper"), Name->GetShadowOffset(), FVector2D::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldObjectPageAnimationTest,
	"Rok.WorldMap.Hud.ObjectPageAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokWorldObjectPageAnimationTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const auto Slate = Hud->TakeWidget();
	auto* Normal = Hud->GetWidgetFromName(TEXT("WorldObjectInfoPanelContent"));
	auto* Desc = Hud->GetWidgetFromName(TEXT("WorldObjectInfoDetailsRoot"));
	auto* Details = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoDetailsButton")));
	auto* Back = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldObjectDescriptionBack")));
	if (!Normal || !Desc || !Details || !Back) { AddError(TEXT("Missing source-shaped pages or navigation")); return false; }
	Details->OnClicked.Broadcast();
	TestEqual(TEXT("Normal page deactivated, not stacked below description"), Normal->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Description enters from right"), Desc->GetRenderTransform().Translation.X, 100.0);
	Hud->AdvanceWorldObjectPageAnimation(0.16666667f);
	TestTrue(TEXT("Description half-time zero-tangent alpha"), FMath::IsNearlyEqual(Desc->GetRenderOpacity(), 0.5f));
	Back->OnClicked.Broadcast();
	TestEqual(TEXT("Description deactivated on return"), Desc->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Normal page enters from left"), Normal->GetRenderTransform().Translation.X, -100.0);
	Hud->AdvanceWorldObjectPageAnimation(1);
	TestEqual(TEXT("Normal page fully opaque after twenty frames"), Normal->GetRenderOpacity(), 1.0f);
	TestEqual(TEXT("Normal page restored to source position"), Normal->GetRenderTransform().Translation.X, 0.0);
	TestNotNull(TEXT("Long source descriptions scroll"), Cast<UScrollBox>(Hud->GetWidgetFromName(TEXT("WorldObjectDescriptionScroll"))));
	auto* RichText = Cast<URichTextBlock>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoDetailsText")));
	if (!TestNotNull(TEXT("Resource prose keeps source rich text"), RichText)) return false;
	auto* StyleTable = RichText->GetTextStyleSet();
	if (!TestNotNull(TEXT("Resource prose style table"), StyleTable)) return false;
	const auto* RegularStyle = StyleTable->FindRow<FRichTextStyleRow>(TEXT("Default"), TEXT("Source prose"));
	const auto* BoldStyle = StyleTable->FindRow<FRichTextStyleRow>(TEXT("b"), TEXT("Source prose"));
	if (!RegularStyle || !BoldStyle) { AddError(TEXT("Missing source normal/bold styles")); return false; }
	TestEqual(TEXT("Description restores source font size"), RegularStyle->TextStyle.Font.Size, 20.0f);
	TestEqual(TEXT("Bold emphasis keeps baseline size"), BoldStyle->TextStyle.Font.Size, RegularStyle->TextStyle.Font.Size);
	TestEqual(TEXT("Source b tag selects bold typeface"), BoldStyle->TextStyle.Font.TypefaceFontName, FName(TEXT("Bold")));
	auto* Gather = Cast<URokUiAnimatedButton>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoGatherButton")));
	if (!TestNotNull(TEXT("Source gather button has BtnAnimation"), Gather)) return false;
	TestEqual(TEXT("Source blue button size"), Cast<UCanvasPanelSlot>(Gather->Slot)->GetSize(), FVector2D(202, 84));
	const FBox2f NormalUv = Gather->GetStyle().Normal.GetUVRegion();
	const FBox2f PressedUv = Gather->GetStyle().Pressed.GetUVRegion();
	const FBox2f DisabledUv = Gather->GetStyle().Disabled.GetUVRegion();
	TestTrue(TEXT("SpriteSwap uses same blue art while pressed"), NormalUv == PressedUv);
	TestTrue(TEXT("Disabled state uses its source grey sprite"), NormalUv != DisabledUv);
	// This unit fixture has no selected actor; explicitly expose the popup as
	// real ShowWorldObjectInfo does. Hidden hierarchy correctly suppresses presses.
	Hud->GetWidgetFromName(TEXT("WorldObjectInfoRoot"))->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Gather->OnPressed.Broadcast();
	Gather->AdvancePressAnimationForTesting(0.1f);
	TestTrue(TEXT("Gather press reduces visual scale"), Gather->GetRenderTransform().Scale.X < 1);
	Details->OnClicked.Broadcast();
	TestEqual(TEXT("Hiding ordinary page resets held button scale"), Gather->GetRenderTransform().Scale.X, 1.0);
	TestFalse(TEXT("Hidden gather button cannot retain animation timer"), Gather->HasActiveAnimationTimerForTesting());
	TestNull(TEXT("Details icon does not invent source-absent scale animation"), Cast<URokUiAnimatedButton>(Details));
	auto* DetailsIcon = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldObjectInfoDetailsButtonIcon")));
	TestNotNull(TEXT("Details uses source information glyph"), DetailsIcon);
	TestNotNull(TEXT("Return uses source arrow glyph"), Hud->GetWidgetFromName(TEXT("WorldObjectDescriptionBackIcon")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldSelectionLodTest,
	"Rok.WorldMap.Hud.SelectionLodBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldSelectionLodTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Tactical boundary is inclusive"), ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(3000));
	TestFalse(TEXT("Strategic objects are presentation, not tactical click targets"), ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(3000.1f));
	TestFalse(TEXT("Explore filter zoom cannot select ordinary objects"), ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(6000));
	TestFalse(TEXT("Invalid distance rejected"), ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(-1));
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	auto* Panel = Hud->GetWidgetFromName(TEXT("WorldObjectInfoRoot"));
	if (!TestNotNull(TEXT("Object information root"), Panel)) return false;
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Hud->UpdateZoomPresentation(3000);
	TestEqual(TEXT("Tactical object information stays visible"), Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	Hud->UpdateZoomPresentation(3000.1f);
	TestEqual(TEXT("Zooming out dismisses object information"), Panel->GetVisibility(), ESlateVisibility::Collapsed);
	TestNotNull(TEXT("UI navigation remains present"), Hud->GetWidgetFromName(TEXT("WorldMapFilterHit_1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokExplorationStateTest,
	"Rok.WorldMap.Exploration.SourceProgressBits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokExplorationStateTest::RunTest(const FString& Parameters)
{
	using namespace RokWorldExplorationState;
	TMap<int32,uint64> Words;
	TestFalse(TEXT("Missing server word is unopened"), IsOpened(1, Words));
	Words.Add(1, (uint64(1)<<63) | uint64(1));
	Words.Add(2, uint64(1)<<1);
	TestTrue(TEXT("Bit63 is unsigned-safe"), IsOpened(63, Words));
	TestTrue(TEXT("ID64 stays in word1 bit0"), IsOpened(64, Words));
	TestTrue(TEXT("ID65 moves to word2 bit1"), IsOpened(65, Words));
	TestFalse(TEXT("Unset bit remains unopened"), IsOpened(62, Words));
	TestFalse(TEXT("Invalid zero ID"), IsOpened(0, Words));
	TestFalse(TEXT("Invalid negative ID"), IsOpened(-1, Words));
	TestEqual(TEXT("Positive max ID avoids signed addition overflow"), WordIndex(MAX_int32), 33554432);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceLegendTest,
	"Rok.WorldMap.Hud.ResourceLegend", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResourceLegendTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	auto* Alliance = Hud->GetWidgetFromName(TEXT("WorldMapAllianceLegend"));
	auto* Resource = Hud->GetWidgetFromName(TEXT("WorldMapResourceLegend"));
	auto* Button = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMapFilterHit_2")));
	if (!TestNotNull(TEXT("Alliance legend"), Alliance) || !TestNotNull(TEXT("Resource legend"), Resource)
		|| !TestNotNull(TEXT("Resource toggle"), Button)) return false;
	TestEqual(TEXT("Resource legend starts hidden"), Resource->GetVisibility(), ESlateVisibility::Collapsed);
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Resource toggle shows its legend"), Resource->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Alliance legend does not leak into resources"), Alliance->GetVisibility(), ESlateVisibility::Collapsed);
	for (int32 Index = 0; Index < 6; ++Index)
		TestNotNull(TEXT("Source richness row"), Hud->GetWidgetFromName(*FString::Printf(TEXT("WorldMapResourceLegendLabel_%d"), Index)));
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Toggle off hides legend"), Resource->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLegendSwitchingTest,
	"Rok.WorldMap.Hud.LegendSwitching", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLegendSwitchingTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	const TCHAR* Names[] = {TEXT("WorldMapAllianceLegend"), TEXT("WorldMapExploreLegend"),
		TEXT("WorldMapResourceLegend"), TEXT("WorldMapMarkerLegend")};
	// Start from resources because alliance is selected at construction.
	const int32 Sequence[] = {2,1,3,0,1,2,3};
	for (int32 Selected : Sequence)
	{
		auto* Button = Cast<UButton>(Hud->GetWidgetFromName(*FString::Printf(TEXT("WorldMapFilterHit_%d"), Selected)));
		if (!TestNotNull(TEXT("Real filter button"), Button)) return false;
		Button->OnClicked.Broadcast();
		for (int32 Index = 0; Index < 4; ++Index)
		{
			auto* Legend = Hud->GetWidgetFromName(Names[Index]);
			if (!TestNotNull(TEXT("Source legend exists"), Legend)) return false;
			TestEqual(TEXT("Exactly one legend follows selection"), Legend->GetVisibility(),
				Index == Selected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	const TCHAR* Labels[] = {TEXT("待调查"), TEXT("已调查"), TEXT("待拜访"), TEXT("已拜访")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Label = Cast<UTextBlock>(Hud->GetWidgetFromName(*FString::Printf(TEXT("WorldMapExploreLegendLabel_%d"), Index)));
		if (TestNotNull(TEXT("Explore source row"), Label)) TestEqual(TEXT("Source label"), Label->GetText().ToString(), FString(Labels[Index]));
	}
	auto* Marker = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMapFilterHit_3")));
	Marker->OnClicked.Broadcast();
	for (const TCHAR* Name : Names)
		TestEqual(TEXT("No legend after toggle off"), Hud->GetWidgetFromName(Name)->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFilterSoundAssetTest,
	"Rok.WorldMap.Hud.FilterSoundAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFilterSoundAssetTest::RunTest(const FString& Parameters)
{
	auto* Sound = LoadObject<USoundWave>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonSidePage.Sound_Ui_CommonSidePage"));
	if (!TestNotNull(TEXT("Exact Unity GameToggle clip imported"), Sound)) return false;
	TestEqual(TEXT("Source forceToMono import preserved"), Sound->NumChannels, 1);
	TestFalse(TEXT("Click is one-shot"), Sound->bLooping);
	TestTrue(TEXT("Decoded source clip duration"), FMath::IsNearlyEqual(Sound->Duration, 25344.0f / 44100.0f, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceClickSoundTest,
	"Rok.WorldMap.Hud.ResourceClickSound", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokResourceClickSoundTest::RunTest(const FString& Parameters)
{
	auto* Sound = LoadObject<USoundWave>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonClickButton.Sound_Ui_CommonClickButton"));
	if (!TestNotNull(TEXT("Exact GameButton sound imported"), Sound)) return false;
	TestEqual(TEXT("Unity forceToMono preserved"), Sound->NumChannels, 1);
	TestFalse(TEXT("Button click must not loop"), Sound->bLooping);
	TestTrue(TEXT("Source decoded sample duration"), FMath::IsNearlyEqual(Sound->Duration, 12672.0f / 44100.0f, 0.001f));
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const auto Slate = Hud->TakeWidget();
	TestEqual(TEXT("Constructing the UI does not play a click"), Hud->GetResourceClickSoundRequestsForTesting(), 0);
	const TCHAR* Names[] = {TEXT("WorldObjectInfoDetailsButton"), TEXT("WorldObjectDescriptionBack"), TEXT("WorldObjectInfoGatherButton")};
	int32 Expected = 0;
	for (const TCHAR* Name : Names)
	{
		auto* Button = Cast<UButton>(Hud->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnPressed.Broadcast();
		Button->OnReleased.Broadcast();
		TestEqual(TEXT("Press/release do not duplicate pointer-click sound"), Hud->GetResourceClickSoundRequestsForTesting(), Expected);
		Button->OnClicked.Broadcast();
		TestEqual(TEXT("Exactly one request per click"), Hud->GetResourceClickSoundRequestsForTesting(), ++Expected);
		Hud->AdvanceWorldObjectPageAnimation(1);
		Hud->AdvanceWorldObjectAnimation(1);
		TestEqual(TEXT("Animation progression is silent"), Hud->GetResourceClickSoundRequestsForTesting(), Expected);
	}
	Hud->HideWorldObjectInfo();
	TestEqual(TEXT("Programmatic close does not manufacture a click"), Hud->GetResourceClickSoundRequestsForTesting(), Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTacticalModuleAnimationTest,
	"Rok.WorldMap.Hud.TacticalModuleAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTacticalModuleAnimationTest::RunTest(const FString& Parameters)
{
	using namespace RokMainInterfaceModuleAnimation;
	TestTrue(TEXT("Left source opening starts offscreen"), EvaluateLeft(true, 0).Equals(FVector2D(-80, 0)));
	TestTrue(TEXT("Left source opening overshoot"), FMath::IsNearlyEqual(EvaluateLeft(true, 0.15f).X, 20.0));
	TestTrue(TEXT("Left source exit overshoot"), FMath::IsNearlyEqual(EvaluateLeft(false, 0.1f).X, 30.0));
	TestTrue(TEXT("Left exit endpoint"), EvaluateLeft(false, 0.25f).Equals(FVector2D(-300, 0)));
	auto* WorldHud = NewObject<URokWorldMapHudWidget>();
	WorldHud->Initialize();
	const TSharedRef<SWidget> Slate = WorldHud->TakeWidget();
	auto* Hud = Cast<URokMainHudWidget>(WorldHud->GetWidgetFromName(TEXT("WorldMapTacticalHud")));
	if (!TestNotNull(TEXT("Tactical module host"), Hud)) return false;
	const TSharedRef<SWidget> TacticalSlate = Hud->TakeWidget();
	auto* Left = Hud->GetWidgetFromName(TEXT("HotTaskGroup"));
	auto* Right = Hud->GetWidgetFromName(TEXT("WorldMapRightModules"));
	if (!TestNotNull(TEXT("Task module"), Left) || !TestNotNull(TEXT("Right motion parent"), Right)) return false;
	WorldHud->UpdateZoomPresentation(2997);
	TestTrue(TEXT("Tasks enter from left"), FMath::IsNearlyEqual(Left->GetRenderTransform().Translation.X, -80.0f));
	TestTrue(TEXT("Resources/features enter from right"), FMath::IsNearlyEqual(Right->GetRenderTransform().Translation.X, 100.0f));
	Hud->AdvanceWorldMapModuleAnimation(0.125f);
	TestTrue(TEXT("Source alpha halfway"), FMath::IsNearlyEqual(Right->GetRenderOpacity(), 0.5f));
	WorldHud->UpdateZoomPresentation(2997);
	TestTrue(TEXT("Same zoom does not restart animation"), FMath::IsNearlyEqual(Right->GetRenderOpacity(), 0.5f));
	Hud->AdvanceWorldMapModuleAnimation(0.125f);
	TestTrue(TEXT("Settled module retains position"), Left->GetRenderTransform().Translation.IsNearlyZero());
	WorldHud->UpdateZoomPresentation(2998);
	TestEqual(TEXT("UI threshold is dispatch, not entity icon threshold"), Hud->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Hud->AdvanceWorldMapModuleAnimation(0.1f);
	TestTrue(TEXT("Tasks exit with source anticipation"), FMath::IsNearlyEqual(Left->GetRenderTransform().Translation.X, 30.0f));
	TestTrue(TEXT("Right modules exit in opposite direction"), FMath::IsNearlyEqual(Right->GetRenderTransform().Translation.X, -20.0f));
	WorldHud->UpdateZoomPresentation(1509);
	TestTrue(TEXT("Reversal restarts Unity opening clip"), FMath::IsNearlyEqual(Left->GetRenderTransform().Translation.X, -80.0f));
	Hud->AdvanceWorldMapModuleAnimation(0.25f);
	WorldHud->UpdateZoomPresentation(3500);
	Hud->AdvanceWorldMapModuleAnimation(0.25f);
	TestEqual(TEXT("Finished exit collapses overlay"), Hud->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("No duplicate overview mail"), WorldHud->GetWidgetFromName(TEXT("WorldMapMailButton"))->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFilterInputTintTest,
	"Rok.WorldMap.Hud.FilterInputTint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFilterInputTintTest::RunTest(const FString& Parameters)
{
	FRokWorldMapFilterTint Tint;
	const FLinearColor Pressed = Tint.Resolve(true, true, true, true);
	TestTrue(TEXT("Pressed wins over focus and hover"), FMath::IsNearlyEqual(Pressed.R, 0.78431374f));
	Tint.Advance(Pressed, 0.05f);
	TestTrue(TEXT("Press midpoint is linear"), FMath::IsNearlyEqual(Tint.Current.R, (1.0f + Pressed.R) * 0.5f));
	const float Mid = Tint.Current.R;
	Tint.Advance(FLinearColor::White, 0.05f);
	TestTrue(TEXT("Interrupted release starts from current color"), FMath::IsNearlyEqual(Tint.Current.R, (1.0f + Mid) * 0.5f));
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> HudSlate = Hud->TakeWidget();
	Hud->UpdateZoomPresentation(6000);
	Hud->AdvanceStrategicModuleAnimation(0.25f);
	auto* Button = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMapFilterHit_0")));
	auto* Background = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldMapFilterBackground_0")));
	auto* Checkmark = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldMapFilterDot_0")));
	if (!Button || !Background || !Checkmark) return false;
	Button->TakeWidget()->OnMouseEnter(FGeometry(), FPointerEvent());
	Hud->AdvanceFilterInputFeedback(0.1f);
	TestTrue(TEXT("Slate hover updates the source target graphic"), FMath::IsNearlyEqual(Background->GetColorAndOpacity().R, 0.9607843f));
	Button->TakeWidget()->OnMouseLeave(FPointerEvent());
	Hud->AdvanceFilterInputFeedback(0.1f);
	TestTrue(TEXT("Leaving restores normal tint"), Background->GetColorAndOpacity().Equals(FLinearColor::White));
	Button->SetIsEnabled(false);
	Hud->AdvanceFilterInputFeedback(0.1f);
	TestTrue(TEXT("Disabled alpha matches ColorBlock"), FMath::IsNearlyEqual(Background->GetColorAndOpacity().A, 0.5019608f));
	TestTrue(TEXT("Checkmark does not inherit background tint"), Checkmark->GetColorAndOpacity().Equals(FLinearColor::White));
	Hud->UpdateZoomPresentation(1537);
	Hud->AdvanceStrategicModuleAnimation(0.25f);
	Hud->AdvanceFilterInputFeedback(0);
	TestTrue(TEXT("Hidden panel clears transient color"), Background->GetColorAndOpacity().Equals(FLinearColor::White));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokStrategicModuleAnimationTest,
	"Rok.WorldMap.Hud.StrategicModuleAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokStrategicModuleAnimationTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	auto* Filter = Hud->GetWidgetFromName(TEXT("WorldMapRelationFilters"));
	auto* Globe = Hud->GetWidgetFromName(TEXT("WorldMapGlobeButton"));
	if (!TestNotNull(TEXT("Filter root"), Filter) || !TestNotNull(TEXT("Server button"), Globe)) return false;
	for (float Dxf : {2997.0f, 3000.0f, 3500.0f, 5400.0f})
	{
		Hud->UpdateZoomPresentation(Dxf);
		TestEqual(TEXT("Transition band cannot overlap task exit with filters"), Filter->GetVisibility(), ESlateVisibility::Collapsed);
	}
	Hud->UpdateZoomPresentation(5401);
	TestEqual(TEXT("Strategic filter enters"), Filter->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("Left entry starts at -80"), FMath::IsNearlyEqual(Filter->GetRenderTransform().Translation.X, -80.0f));
	Hud->AdvanceStrategicModuleAnimation(0.125f);
	TestTrue(TEXT("Filter halfway alpha"), FMath::IsNearlyEqual(Filter->GetRenderOpacity(), 0.5f));
	Hud->UpdateZoomPresentation(6000);
	TestTrue(TEXT("Same band does not restart"), FMath::IsNearlyEqual(Filter->GetRenderOpacity(), 0.5f));
	Hud->AdvanceStrategicModuleAnimation(0.125f);
	TestTrue(TEXT("Globe settles at original layout position"), Globe->GetRenderTransform().Translation.IsNearlyZero());
	Hud->UpdateZoomPresentation(5400);
	TestEqual(TEXT("Exiting filters cannot intercept clicks"), Filter->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Exiting server cannot intercept clicks"), Globe->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Hud->AdvanceStrategicModuleAnimation(0.25f);
	TestEqual(TEXT("Exit finishes hidden"), Filter->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->UpdateZoomPresentation(6000);
	TestEqual(TEXT("Selection persists through zoom"), Hud->GetSelectedFilter(), ERokWorldMapHudFilter::Alliance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFilterSelectionAnimationTest,
	"Rok.WorldMap.Hud.FilterSelectionAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFilterSelectionAnimationTest::RunTest(const FString& Parameters)
{
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	Hud->TakeWidget();
	auto* Alliance = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldMapFilterDot_0")));
	auto* Explore = Cast<UImage>(Hud->GetWidgetFromName(TEXT("WorldMapFilterDot_1")));
	auto* Button = Cast<UButton>(Hud->GetWidgetFromName(TEXT("WorldMapFilterHit_1")));
	if (!TestNotNull(TEXT("Source checkmark is an image"), Alliance)
		|| !TestNotNull(TEXT("Explore checkmark image"), Explore)
		|| !TestNotNull(TEXT("Runtime selection button"), Button)) return false;
	TestEqual(TEXT("Alliance initially selected"), Alliance->GetRenderOpacity(), 1.0f);
	Button->OnClicked.Broadcast();
	Hud->AdvanceFilterSelectionAnimation(0.05f);
	TestEqual(TEXT("Previous checkmark halfway faded"), Alliance->GetRenderOpacity(), 0.5f);
	TestEqual(TEXT("New checkmark halfway visible"), Explore->GetRenderOpacity(), 0.5f);
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Second click toggles off"), Hud->GetSelectedFilter(), ERokWorldMapHudFilter::None);
	Hud->AdvanceFilterSelectionAnimation(0.05f);
	TestEqual(TEXT("Interrupted fade continues from current alpha"), Explore->GetRenderOpacity(), 0.25f);
	Hud->AdvanceFilterSelectionAnimation(0.05f);
	TestEqual(TEXT("Cleared selection finishes transparent"), Explore->GetRenderOpacity(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMiniMapAnimationTest,
	"Rok.WorldMap.Hud.MiniMapAnimation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMiniMapAnimationTest::RunTest(const FString& Parameters)
{
	using namespace RokMainInterfaceModuleAnimation;
	TestTrue(TEXT("Source right entry starts offscreen and transparent"), EvaluateRight(true, 0).Equals(FVector2D(100, 0)));
	TestTrue(TEXT("Source entry overshoots at 0.15"), FMath::IsNearlyEqual(EvaluateRight(true, 0.15f).X, -20.0));
	TestTrue(TEXT("Source entry settles at 0.25"), EvaluateRight(true, 0.25f).Equals(FVector2D(0, 1)));
	TestTrue(TEXT("Source exit anticipation key"), FMath::IsNearlyEqual(EvaluateRight(false, 0.1f).X, -20.0));
	TestTrue(TEXT("Source exit ends offscreen"), EvaluateRight(false, 0.25f).Equals(FVector2D(100, 0)));
	auto* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	Hud->TakeWidget();
	auto* Map = Hud->GetWidgetFromName(TEXT("WorldMapMiniMap"));
	if (!TestNotNull(TEXT("Runtime map root"), Map)) return false;
	Hud->UpdateZoomPresentation(2997);
	Hud->AdvanceMiniMapAnimation(0.25f);
	TestEqual(TEXT("Last tactical key hides map"), Map->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->UpdateZoomPresentation(2998);
	TestEqual(TEXT("Entry is interactive per source"), Map->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	Hud->AdvanceMiniMapAnimation(0.15f);
	TestTrue(TEXT("Real root uses overshoot translation"), FMath::IsNearlyEqual(Map->GetRenderTransform().Translation.X, -20.0));
	const float MidOpacity = Map->GetRenderOpacity();
	Hud->UpdateZoomPresentation(3500);
	TestEqual(TEXT("Repeated zoom update does not restart animation"), Map->GetRenderOpacity(), MidOpacity);
	Hud->UpdateZoomPresentation(1537);
	TestEqual(TEXT("Exit immediately disables child hit tests"), Map->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Hud->AdvanceMiniMapAnimation(0.25f);
	TestEqual(TEXT("Exit eventually collapses"), Map->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->UpdateZoomPresentation(3500);
	TestEqual(TEXT("Reopening restarts the source clip"), Map->GetRenderOpacity(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapCityTitleTest,
	"Rok.WorldMap.Hud.CityTitle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCityTitleTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Detailed view keeps city title"), URokWorldMapHudWidget::ShouldShowCityTitle(1537));
	TestTrue(TEXT("Exact Tactical upper bound retains title"), URokWorldMapHudWidget::ShouldShowCityTitle(3000.0f));
	for (const float Dxf : {3000.01f, 3500.0f, 4500.0f})
	{
		TestTrue(TEXT("Transition band retains title alongside portrait"),
			URokWorldMapHudWidget::ShouldShowCityTitle(Dxf) && URokWorldMapHudWidget::ShouldShowCityPortrait(Dxf));
	}
	TestFalse(TEXT("Title ends after inclusive transition boundary"), URokWorldMapHudWidget::ShouldShowCityTitle(4500.1f));
	TestFalse(TEXT("Invalid distance hides title"), URokWorldMapHudWidget::ShouldShowCityTitle(-1));
	TestEqual(TEXT("Title curve starts at -3.5"), URokWorldMapHudWidget::EvaluateCityTitleOffset(0), -3.5f);
	TestEqual(TEXT("Title curve clamps negative time"), URokWorldMapHudWidget::EvaluateCityTitleOffset(-10), -3.5f);
	TestTrue(TEXT("Midpoint retains the source nonzero arrival tangent"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateCityTitleOffset(1499.5f),
			-2.9f - 2999.0f*0.0012443735f/8.0f, 0.00001f));
	TestEqual(TEXT("Last detailed key is -2.3"), URokWorldMapHudWidget::EvaluateCityTitleOffset(2999), -2.3f);
	TestEqual(TEXT("Infinite tangent holds rather than interpolating"), URokWorldMapHudWidget::EvaluateCityTitleOffset(2999.5f), -2.3f);
	TestEqual(TEXT("Overview key jumps to -1.55"), URokWorldMapHudWidget::EvaluateCityTitleOffset(3000), -1.55f);
	TestEqual(TEXT("Curve clamps after the last key"), URokWorldMapHudWidget::EvaluateCityTitleOffset(5400), -1.55f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapCityPortraitTest,
	"Rok.WorldMap.Hud.CityPortrait", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCityPortraitTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Detailed city keeps name/level rather than portrait"), URokWorldMapHudWidget::ShouldShowCityPortrait(2999.9f));
	TestFalse(TEXT("Exact source upper bound remains Tactical"), URokWorldMapHudWidget::ShouldShowCityPortrait(3000.0f));
	TestTrue(TEXT("Portrait begins beyond source Tactical upper bound"), URokWorldMapHudWidget::ShouldShowCityPortrait(3000.01f));
	TestTrue(TEXT("Portrait remains in current maximum world view"), URokWorldMapHudWidget::ShouldShowCityPortrait(5400.0f));
	TestFalse(TEXT("Invalid distance cannot enable portrait"), URokWorldMapHudWidget::ShouldShowCityPortrait(-1));
	TestTrue(TEXT("TroopCurve starts last segment at 1.8"), FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateCityPortraitOffset(2997), 1.8f));
	TestTrue(TEXT("TroopCurve ends at 11"), FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateCityPortraitOffset(70000), 11.0f));
	TestTrue(TEXT("TroopCurve clamps beyond the last key"), FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateCityPortraitOffset(80000), 11.0f));
	URokWorldMapHudWidget* Hud = NewObject<URokWorldMapHudWidget>();
	Hud->Initialize();
	Hud->TakeWidget();
	TestNotNull(TEXT("Runtime HUD owns the source portrait image"), Hud->GetWidgetFromName(TEXT("WorldMapSelfPortraitImage")));
	TestNotNull(TEXT("Runtime HUD owns the independent portrait pointer"), Hud->GetWidgetFromName(TEXT("WorldMapSelfPortraitPointer")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapHomeIndicatorTest,
	"Rok.WorldMap.Hud.HomeIndicator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHomeIndicatorTest::RunTest(const FString& Parameters)
{
	const FVector2D Size(1440, 900);
	TestFalse(TEXT("Visible city does not need an edge cursor"),
		RokWorldMapHomeIndicator::ShouldShow(FVector2D(720, 450), Size, 3));
	TestFalse(TEXT("Unity hides the cursor below one kilometre"),
		RokWorldMapHomeIndicator::ShouldShow(FVector2D(2000, 1200), Size, 0.99f));
	TestTrue(TEXT("Offscreen city at one kilometre needs the cursor"),
		RokWorldMapHomeIndicator::ShouldShow(FVector2D(2000, 1200), Size, 1));
	// The menu occupies the lower right, not the middle of the bottom edge.
	const FVector2D Below = RokWorldMapHomeIndicator::Position(FVector2D(2000, 2000), Size, true);
	TestTrue(TEXT("Bottom pointer and text stay above the expanded menu"), Below.Y + 60 <= Size.Y - 124);
	const FVector2D Right = RokWorldMapHomeIndicator::Position(FVector2D(2500, 450), Size, true);
	TestTrue(TEXT("Right pointer preserves bearing and respects source 3 percent inset"),
		FMath::IsNearlyEqual(Right.Y, 450.0, 0.001) && Right.X + 50 <= Size.X * 0.97 + 0.001);
	for (const FVector2D Target : {FVector2D(-1000, 450), FVector2D(720, -1000), FVector2D(2200, 1800)})
	{
		for (const bool bTactical : {false, true})
		{
			const FVector2D Point = RokWorldMapHomeIndicator::Position(Target, Size, bTactical);
			TestTrue(TEXT("Pointer artwork remains inside the viewport"), Point.X >= 50 && Point.Y >= 60
				&& Point.X <= Size.X - 50 && Point.Y <= Size.Y - 60);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudMiniMapProjectionTest,
	"Rok.WorldMap.Hud.MiniMapProjection",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudMiniMapProjectionTest::RunTest(
	const FString& Parameters)
{
	const FRokWorldMapProjection Projection;
	const FVector2D MiniMapSize(193.88f, 138.16f);

	TestTrue(
		TEXT("World minimum maps to the minimap lower-left"),
		URokWorldMapHudWidget::ComputeMiniMapPoint(
			Projection,
			Projection.UnityBoundsMin,
			MiniMapSize)
			.Equals(FVector2D(0.0f, MiniMapSize.Y), 0.001f));
	TestTrue(
		TEXT("World maximum maps to the minimap upper-right"),
		URokWorldMapHudWidget::ComputeMiniMapPoint(
			Projection,
			Projection.UnityBoundsMax,
			MiniMapSize)
			.Equals(FVector2D(MiniMapSize.X, 0.0f), 0.001f));

	const FVector2D PlayerPoint =
		URokWorldMapHudWidget::ComputeMiniMapPoint(
			Projection,
			FVector2D(6876.0f, 942.0f),
			MiniMapSize);
	TestTrue(
		TEXT("Player X:1146 Y:157 is near the minimap lower-right"),
		PlayerPoint.Equals(
			FVector2D(185.1554f, 120.086f),
			0.01f));

	TestTrue(
		TEXT("Minimap upper-left converts back to the world upper-left"),
		URokWorldMapHudWidget::ComputeUnityPositionFromMiniMapPoint(
			Projection,
			FVector2D::ZeroVector,
			MiniMapSize)
			.Equals(
				FVector2D(
					Projection.UnityBoundsMin.X,
					Projection.UnityBoundsMax.Y),
				0.001f));
	TestTrue(
		TEXT("Minimap lower-right converts back to the world lower-right"),
		URokWorldMapHudWidget::ComputeUnityPositionFromMiniMapPoint(
			Projection,
			MiniMapSize,
			MiniMapSize)
			.Equals(
				FVector2D(
					Projection.UnityBoundsMax.X,
					Projection.UnityBoundsMin.Y),
				0.001f));
	TestTrue(
		TEXT("Minimap coordinates clamp before world conversion"),
		URokWorldMapHudWidget::ComputeUnityPositionFromMiniMapPoint(
			Projection,
			FVector2D(-100.0f, MiniMapSize.Y + 100.0f),
			MiniMapSize)
			.Equals(Projection.UnityBoundsMin, 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudMarkerClampTest,
	"Rok.WorldMap.Hud.MarkerClamp",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudMarkerClampTest::RunTest(
	const FString& Parameters)
{
	const FVector2D Clamped =
		URokWorldMapHudWidget::ClampMarkerTopLeft(
			FVector2D(-100.0f, 900.0f),
			FVector2D(110.0f, 130.0f),
			FVector2D(1440.0f, 810.0f),
			12.0f);
	TestTrue(
		TEXT("Self marker remains inside the 1440x810 safe area"),
		Clamped.Equals(FVector2D(12.0f, 668.0f), 0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudInteractiveControlsTest,
	"Rok.WorldMap.Hud.InteractiveControls",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudInteractiveControlsTest::RunTest(
	const FString& Parameters)
{
	URokWorldMapHudWidget* HudWidget =
		NewObject<URokWorldMapHudWidget>(GetTransientPackage());
	TestNotNull(TEXT("World-map HUD can be constructed"), HudWidget);
	if (!HudWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("World-map HUD initializes its runtime WidgetTree"),
		HudWidget->Initialize());
	HudWidget->TakeWidget();
	URokMainHudWidget* TacticalHudWidget = Cast<URokMainHudWidget>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapTacticalHud")));
	if (TestNotNull(TEXT("World-map tactical controls have a reusable child host"), TacticalHudWidget))
	{
		TacticalHudWidget->TakeWidget();
		TestNotNull(TEXT("World-map tactical ArmyQueueButton is a real UButton"),
			Cast<UButton>(TacticalHudWidget->GetWidgetFromName(TEXT("ArmyQueueButton"))));
		UWidget* QueueGroup = TacticalHudWidget->GetWidgetFromName(TEXT("ArmyQueueEntry"));
		if (TestNotNull(TEXT("Army queue presentation remains a separate state group"), QueueGroup))
		{
			TestEqual(TEXT("No active march snapshot does not show a diagnostic queue"),
				QueueGroup->GetVisibility(), ESlateVisibility::Collapsed);
		}
	}

	static const FName RequiredButtonNames[] = {
		TEXT("WorldMapPlayerProfileHitButton"),
		TEXT("WorldMapVipHitButton"),
		TEXT("WorldMapPlayerArrowHitButton"),
		TEXT("WorldMapCoordinateSearchButton"),
		TEXT("WorldMapFavoriteButton"),
		TEXT("WorldMapFilterHit_0"),
		TEXT("WorldMapFilterHit_1"),
		TEXT("WorldMapFilterHit_2"),
		TEXT("WorldMapFilterHit_3"),
		TEXT("WorldMapChatButton"),
		TEXT("WorldMapSearchButton"),
		TEXT("WorldMapHomeCityButton"),
		TEXT("WorldMapGlobeButton"),
		TEXT("WorldMapMailButton"),
	};
	for (const FName& ButtonName : RequiredButtonNames)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("%s is a real UButton"),
				*ButtonName.ToString()),
			Cast<UButton>(HudWidget->GetWidgetFromName(ButtonName)));
	}
	TestNotNull(
		TEXT("World map has a full-viewport left-click selection surface"),
		Cast<URokWorldMapSelectionInputWidget>(
			HudWidget->GetWidgetFromName(
				TEXT("WorldMapSelectionInput"))));
	TestNotNull(
		TEXT("Minimap has a pointer input surface for click and drag"),
		Cast<URokWorldMapMiniMapInputWidget>(
			HudWidget->GetWidgetFromName(TEXT("WorldMapMiniMapInput"))));
	TestNotNull(
		TEXT("Kingdom list has a real unavailable selection button"),
		Cast<UButton>(HudWidget->GetWidgetFromName(
			TEXT("WorldMapServerSelectUnavailableButton"))));
	TestNotNull(
		TEXT("Kingdom list has a real close button"),
		Cast<UButton>(HudWidget->GetWidgetFromName(
			TEXT("WorldMapServerListCloseButton"))));
	UCanvasPanel* WorldObjectInfoRoot = Cast<UCanvasPanel>(
		HudWidget->GetWidgetFromName(TEXT("WorldObjectInfoRoot")));
	TestNotNull(
		TEXT("World-map HUD owns the resource object info popup"),
		WorldObjectInfoRoot);
	if (WorldObjectInfoRoot)
	{
		TestEqual(
			TEXT("Resource object info popup starts closed"),
			WorldObjectInfoRoot->GetVisibility(),
			ESlateVisibility::Collapsed);
	}
	UButton* WorldObjectDetailsButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldObjectInfoDetailsButton")));
	UButton* WorldObjectGatherButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldObjectInfoGatherButton")));
	UCanvasPanel* WorldObjectDetailsRoot = Cast<UCanvasPanel>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldObjectInfoDetailsRoot")));
	UTextBlock* WorldObjectFeedback = Cast<UTextBlock>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldObjectInfoFeedback")));
	TestNotNull(
		TEXT("Resource object panel consumes clicks outside its actions"),
		Cast<UButton>(HudWidget->GetWidgetFromName(
			TEXT("WorldObjectInfoInputBlocker"))));
	TestNotNull(
		TEXT("Resource object info has a real details button"),
		WorldObjectDetailsButton);
	TestNotNull(
		TEXT("Resource object info has a real gather button"),
		WorldObjectGatherButton);
	TestNotNull(
		TEXT("Resource object info owns a details region"),
		WorldObjectDetailsRoot);
	TestNotNull(
		TEXT("Resource object info owns boundary feedback"),
		WorldObjectFeedback);
	if (WorldObjectDetailsButton && WorldObjectDetailsRoot)
	{
		WorldObjectDetailsButton->OnClicked.Broadcast();
		TestEqual(
			TEXT("Details button opens the interactive source description page"),
			WorldObjectDetailsRoot->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
		WorldObjectDetailsButton->OnClicked.Broadcast();
		TestEqual(
			TEXT("Details button collapses the local explanation"),
			WorldObjectDetailsRoot->GetVisibility(),
			ESlateVisibility::Collapsed);
	}
	if (WorldObjectGatherButton && WorldObjectFeedback)
	{
		WorldObjectGatherButton->OnClicked.Broadcast();
		TestFalse(
			TEXT("Gather boundary produces honest feedback"),
			WorldObjectFeedback->GetText().IsEmpty());
	}
	URokUnityFeatureScreenWidget* FeatureWidget =
		Cast<URokUnityFeatureScreenWidget>(
			HudWidget->GetWidgetFromName(
				TEXT("WorldMapUnityFeatureScreenWidget")));
	TestNotNull(
		TEXT("World-map HUD owns the shared feature screen"),
		FeatureWidget);
	URokUnityModalWidget* ModalWidget =
		Cast<URokUnityModalWidget>(
			HudWidget->GetWidgetFromName(
				TEXT("WorldMapUnityModalWidget")));
	TestNotNull(
		TEXT("World-map HUD owns the shared modal screen"),
		ModalWidget);
	UButton* ProfileEntry = Cast<UButton>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldMapPlayerProfileHitButton")));
	if (ModalWidget && ProfileEntry)
	{
		// Broadcast verifies the synchronous UButton delegate route and
		// rebuilt widget state. It is not a physical Slate hit-test
		// acceptance.
		ProfileEntry->OnClicked.Broadcast();
		TestTrue(
			TEXT("World-map player-profile entry opens the shared modal"),
			ModalWidget->IsModalOpen());

		static const FName ProfileButtonNames[] = {
			TEXT("ProfilePictureButton"),
			TEXT("ProfileModifyButton"),
			TEXT("ProfileRenameButton"),
			TEXT("ProfileCivilizationButton"),
			TEXT("ProfileKillsHelpButton"),
			TEXT("ProfileActionHelpButton"),
			TEXT("ActionAddButton"),
			TEXT("ProfileArmyButton"),
			TEXT("ProfileMoreButton"),
			TEXT("ProfileRankingButton"),
			TEXT("ProfileSettingsButton"),
		};
		for (const FName& ButtonName : ProfileButtonNames)
		{
			TestNotNull(
				*FString::Printf(
					TEXT("%s is a real world-map player-profile UButton"),
					*ButtonName.ToString()),
				Cast<UButton>(
					ModalWidget->GetWidgetFromName(ButtonName)));
		}

		UButton* CivilizationRoute = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ProfileCivilizationButton")));
		TestNotNull(
			TEXT("World-map profile exposes the civilization route"),
			CivilizationRoute);
		if (CivilizationRoute)
		{
			CivilizationRoute->OnClicked.Broadcast();
			for (const int32 Id : {101, 201, 301, 401, 501, 601, 801})
			{
				auto* ChoiceName = Cast<UTextBlock>(ModalWidget->GetWidgetFromName(*FString::Printf(TEXT("CivilizationChoiceName_%d"), Id)));
				if (TestNotNull(TEXT("Civilization flag has localized name"), ChoiceName))
					TestFalse(TEXT("Flag name is populated from catalog"), ChoiceName->GetText().IsEmpty());
			}
			for (const int32 Id : {101, 201, 501})
			{
				auto* Flag = ModalWidget->GetWidgetFromName(*FString::Printf(TEXT("CivilizationFlag_%d"), Id));
				auto* Hit = ModalWidget->GetWidgetFromName(*FString::Printf(TEXT("CivilizationChoiceButton_%d"), Id));
				if (TestNotNull(TEXT("Selectable flag"), Flag) && TestNotNull(TEXT("Selectable hit region"), Hit))
				{
					const auto* FlagSlot = CastChecked<UCanvasPanelSlot>(Flag->Slot);
					const auto* HitSlot = CastChecked<UCanvasPanelSlot>(Hit->Slot);
					TestTrue(TEXT("Button tracks actual flag position"), HitSlot->GetPosition().Equals(FlagSlot->GetPosition()));
					TestTrue(TEXT("Button tracks actual flag size"), HitSlot->GetSize().Equals(FlagSlot->GetSize()));
				}
			}
			UWidget* Pulse = ModalWidget->GetWidgetFromName(TEXT("CivilizationSelectionPulse"));
			if (TestNotNull(TEXT("Civilization has independent animated highlight"), Pulse))
			{
				TestEqual(TEXT("Highlight begins transparent"), Pulse->GetRenderOpacity(), 0.0f);
				TestTrue(TEXT("Highlight belongs to bottom list"), Pulse->GetParent() == ModalWidget->GetWidgetFromName(TEXT("CivilizationListRoot")));
			}
			const auto* CountryFlag = Cast<UImage>(ModalWidget->GetWidgetFromName(TEXT("CivilizationCountryFlag")));
			TestNotNull(TEXT("Source cloth flag replaces rectangular shade"), CountryFlag);
			if (CountryFlag)
			{
				TestNotNull(TEXT("Existing loading atlas resolves"), CountryFlag->GetBrush().GetResourceObject());
				TestTrue(TEXT("Source Simple flag image, not a sliced frame"), CountryFlag->GetBrush().DrawAs == ESlateBrushDrawType::Image);
				const auto* FlagSlot = CastChecked<UCanvasPanelSlot>(CountryFlag->Slot);
				TestTrue(TEXT("Source flag presentation size"), FlagSlot->GetSize().Equals(FVector2D(823, 819)));
			}
			UWidget* AnimatedHero = ModalWidget->GetWidgetFromName(TEXT("CivilizationAnimatedHero"));
			TestNotNull(TEXT("Civilization owns the source animated hero"), AnimatedHero);
			if (AnimatedHero)
			{
				TestEqual(TEXT("Hero starts transparent"), AnimatedHero->GetRenderOpacity(), 0.0f);
				TestTrue(TEXT("Hero starts 200 source units left"), FMath::IsNearlyEqual(AnimatedHero->GetRenderTransform().Translation.X, -200.0));
				ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
				TestTrue(TEXT("Hero source midpoint opacity"), FMath::IsNearlyEqual(AnimatedHero->GetRenderOpacity(), 0.5f));
				TestTrue(TEXT("Hero source midpoint position"), FMath::IsNearlyEqual(AnimatedHero->GetRenderTransform().Translation.X, -100.0));
				ModalWidget->AdvanceWindowAnimationForTesting(0.25f);
				TestEqual(TEXT("Hero fully visible at source duration"), AnimatedHero->GetRenderOpacity(), 1.0f);
				TestTrue(TEXT("Hero settles at source anchor"), AnimatedHero->GetRenderTransform().Translation.IsNearlyZero());
			}
			UButton* ArabiaChoice = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("CivilizationChoiceButton_101")));
			UButton* EgyptChoice = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("CivilizationChoiceButton_501")));
			TestNotNull(TEXT("World-map civilization page exposes Arabia"), ArabiaChoice);
			TestNotNull(TEXT("World-map civilization page exposes Egypt"), EgyptChoice);
			TestNotNull(
				TEXT("World-map civilization page keeps Rome locked"),
				Cast<UTextBlock>(ModalWidget->GetWidgetFromName(TEXT("CivilizationLockedState_301"))));
			if (EgyptChoice)
			{
				EgyptChoice->OnClicked.Broadcast();
				UWidget* EgyptianHero = ModalWidget->GetWidgetFromName(TEXT("CivilizationAnimatedHero"));
				TestNotNull(TEXT("Changing civilization constructs its hero"), EgyptianHero);
				if (EgyptianHero)
				{
					TestTrue(TEXT("Civilization switch reuses the presentation widget"), EgyptianHero == AnimatedHero);
					TestEqual(TEXT("New hero starts source entry"), EgyptianHero->GetRenderOpacity(), 0.0f);
					auto* FlagGroup = ModalWidget->GetWidgetFromName(TEXT("CivilizationDetailRoot"));
					if (TestNotNull(TEXT("Flag content shares source animation group"), FlagGroup))
					{
						TestEqual(TEXT("Switch begins with flag visible"), FlagGroup->GetRenderOpacity(), 1.0f);
						ModalWidget->AdvanceWindowAnimationForTesting(1.0f / 12.0f);
						TestTrue(TEXT("Real flag group fades out"), FMath::IsNearlyEqual(FlagGroup->GetRenderOpacity(), 0.5f));
						TestTrue(TEXT("Real flag group moves upward"), FMath::IsNearlyEqual(FlagGroup->GetRenderTransform().Translation.Y, -35.0));
						TestEqual(TEXT("Source hero hidden during flag out"), EgyptianHero->GetRenderOpacity(), 0.0f);
						ModalWidget->AdvanceWindowAnimationForTesting(1.0f / 12.0f + 0.25f);
						TestTrue(TEXT("Real groups show together"), FMath::IsNearlyEqual(FlagGroup->GetRenderOpacity(), 0.5f) && FMath::IsNearlyEqual(EgyptianHero->GetRenderOpacity(), 0.5f));
						TestEqual(TEXT("Civilization list does not restart on selection"), ModalWidget->GetWidgetFromName(TEXT("CivilizationListRoot"))->GetRenderOpacity(), 1.0f);
					}
				}
				TestNotNull(
					TEXT("Civilization selection updates the preview"),
					Cast<UTextBlock>(ModalWidget->GetWidgetFromName(TEXT("CivilizationSelectedName"))));
			}
			UButton* ContinueButton = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("CivilizationContinueButton")));
			if (ContinueButton)
			{
				ContinueButton->OnClicked.Broadcast();
				UButton* SubmitButton = Cast<UButton>(
					ModalWidget->GetWidgetFromName(TEXT("CivilizationSubmitButton")));
				TestNotNull(TEXT("World-map civilization confirmation exposes submit"), SubmitButton);
				if (SubmitButton)
				{
					TestFalse(
						TEXT("World-map civilization submit remains server-disabled"),
						SubmitButton->GetIsEnabled());
				}
			}
		}

		ProfileEntry->OnClicked.Broadcast();

		UButton* HeadRoute = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ProfilePictureButton")));
		TestNotNull(
			TEXT("World-map profile exposes the player-head route"),
			HeadRoute);
		if (HeadRoute)
		{
			HeadRoute->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map player-head route opens the Unity catalog"),
				Cast<UButton>(ModalWidget->GetWidgetFromName(
					TEXT("PlayerHeadChoiceButton_0"))));
			UButton* FramePage = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("PlayerHeadPageButton_1")));
			TestNotNull(
				TEXT("World-map player-head route exposes the frame tab"),
				FramePage);
			if (FramePage)
			{
				FramePage->OnClicked.Broadcast();
				TestNotNull(
					TEXT("World-map frame tab rebuilds the frame catalog"),
					Cast<UButton>(ModalWidget->GetWidgetFromName(
						TEXT("PlayerHeadChoiceButton_1"))));
			}
			UButton* CloseSubscreen = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("Type1CloseButton")));
			if (CloseSubscreen)
			{
				CloseSubscreen->OnClicked.Broadcast();
			}
		}

		ProfileEntry->OnClicked.Broadcast();
		UButton* PowerRoute = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ProfileMoreButton")));
		TestNotNull(
			TEXT("World-map profile exposes the power-details route"),
			PowerRoute);
		if (PowerRoute)
		{
			PowerRoute->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map power-details page exposes its server boundary"),
				Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
					TEXT("PlayerPowerServerBoundaryState"))));
			UButton* CloseSubscreen = Cast<UButton>(
				ModalWidget->GetWidgetFromName(TEXT("Type3CloseButton")));
			if (CloseSubscreen)
			{
				CloseSubscreen->OnClicked.Broadcast();
			}
		}

		ProfileEntry->OnClicked.Broadcast();
		UButton* RankingRoute = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ProfileRankingButton")));
		TestNotNull(
			TEXT("World-map profile exposes the ranking route"),
			RankingRoute);
		if (RankingRoute)
		{
			RankingRoute->OnClicked.Broadcast();
			UButton* FirstCategory = Cast<UButton>(
				ModalWidget->GetWidgetFromName(
					TEXT("PlayerRankingCategoryButton_0")));
			TestNotNull(
				TEXT("World-map ranking page exposes source categories"),
				FirstCategory);
			if (FirstCategory)
			{
				FirstCategory->OnClicked.Broadcast();
				TestNotNull(
					TEXT("World-map ranking detail keeps an honest empty state"),
					Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
						TEXT("PlayerRankingEmptyState"))));
				UButton* RankingBack = Cast<UButton>(
					ModalWidget->GetWidgetFromName(
						TEXT("PlayerRankingBackButton")));
				TestNotNull(
					TEXT("World-map ranking detail exposes a local back route"),
					RankingBack);
				if (RankingBack)
				{
					RankingBack->OnClicked.Broadcast();
					TestNotNull(
						TEXT("World-map ranking back restores the category grid"),
						Cast<UButton>(ModalWidget->GetWidgetFromName(
							TEXT("PlayerRankingCategoryButton_0"))));
				}
			}
		}

		ProfileEntry->OnClicked.Broadcast();
		UButton* SettingsRoute = Cast<UButton>(
			ModalWidget->GetWidgetFromName(TEXT("ProfileSettingsButton")));
		if (SettingsRoute && FeatureWidget)
		{
			SettingsRoute->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map profile settings opens the shared settings hub"),
				Cast<UButton>(FeatureWidget->GetWidgetFromName(
					TEXT("SettingsHubGeneralButton"))));
			FeatureWidget->CloseScreen();
		}
	}
	if (FeatureWidget)
	{
		UButton* MailEntry = Cast<UButton>(
			HudWidget->GetWidgetFromName(TEXT("WorldMapMailButton")));
		TestNotNull(
			TEXT("World-map HUD exposes the shared mail entry"),
			MailEntry);
		if (MailEntry)
		{
			MailEntry->OnClicked.Broadcast();
			TestTrue(
				TEXT("World-map mail entry opens the feature screen"),
				FeatureWidget->IsScreenOpen());
			TestNotNull(
				TEXT("World-map mail entry opens the Unity empty list"),
				Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
					TEXT("MailListScroll"))));
			TestNotNull(
				TEXT("World-map mail entry exposes the compose control"),
				Cast<UButton>(FeatureWidget->GetWidgetFromName(
					TEXT("MailCompose"))));
			TestNotNull(
				TEXT("World-map mail entry shows the zero-row state"),
				Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
					TEXT("MailEmptyState"))));
			FeatureWidget->CloseScreen();
		}

		UButton* CityManagerEntry = Cast<UButton>(
			HudWidget->GetWidgetFromName(
				TEXT("WorldMapPlayerArrowHitButton")));
		TestNotNull(
			TEXT("World-map player arrow is the city-manager entry"),
			CityManagerEntry);
		if (CityManagerEntry)
		{
			CityManagerEntry->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map player arrow opens city manager"),
				Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
					TEXT("CityManagerListScroll"))));
			if (ModalWidget)
			{
				TestFalse(
					TEXT("City-manager route closes the profile modal"),
					ModalWidget->IsModalOpen());
			}
			FeatureWidget->CloseScreen();
		}

		UButton* ArmyShowEntry = TacticalHudWidget
			? Cast<UButton>(TacticalHudWidget->GetWidgetFromName(TEXT("ArmyQueueButton")))
			: nullptr;
		TestNotNull(
			TEXT("World-map tactical child retains a real ArmyShow entry"),
			ArmyShowEntry);
		if (ArmyShowEntry)
		{
			ArmyShowEntry->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map army entry opens the Unity ArmyShow list"),
				Cast<UScrollBox>(FeatureWidget->GetWidgetFromName(
					TEXT("ArmyShowListScroll"))));
			TestNotNull(
				TEXT("World-map ArmyShow exposes its rules entry"),
				Cast<UButton>(FeatureWidget->GetWidgetFromName(
					TEXT("ArmyShowRulesButton"))));
			FeatureWidget->CloseScreen();
		}

		UButton* ChatEntry = Cast<UButton>(
			HudWidget->GetWidgetFromName(TEXT("WorldMapChatButton")));
		TestNotNull(
			TEXT("World-map exposes a real chat entry"),
			ChatEntry);
		if (ChatEntry)
		{
			ChatEntry->OnClicked.Broadcast();
			TestNotNull(
				TEXT("World-map chat entry opens the real send control"),
				Cast<UButton>(FeatureWidget->GetWidgetFromName(
					TEXT("ChatSendButton"))));
			TestNotNull(
				TEXT("World-map chat entry opens the honest empty state"),
				Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
					TEXT("ChatMessagesEmptyState"))));
			FeatureWidget->CloseScreen();
		}
	}
	UButton* SearchButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapSearchButton")));
	if (FeatureWidget && SearchButton)
	{
		SearchButton->OnClicked.Broadcast();
		TestTrue(
			TEXT("World-map search opens a feature screen"),
			FeatureWidget->IsScreenOpen());
		TestNotNull(
			TEXT("World-map search opens the resource-search target"),
			Cast<UButton>(
				FeatureWidget->GetWidgetFromName(
					TEXT("ResourceSearchSubmit"))));
		TestNotNull(
			TEXT("World-map search opens the Unity bottom resource dock"),
			FeatureWidget->GetWidgetFromName(
				TEXT("ResourceSearchDockBackground")));
		TestNotNull(
			TEXT("World-map search exposes the transparent touch-close mask"),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				TEXT("ResourceSearchTouchCloseMask"))));
		TestNotNull(
			TEXT("World-map search exposes the Unity integer slider"),
			Cast<USlider>(FeatureWidget->GetWidgetFromName(
				TEXT("ResourceSearchSlider"))));
		FeatureWidget->CloseScreen();
	}

	TestEqual(
		TEXT("Unity world filter defaults to Alliance"),
		HudWidget->GetSelectedFilter(),
		ERokWorldMapHudFilter::Alliance);
	UButton* AllianceFilter = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapFilterHit_0")));
	UButton* ResourceFilter = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapFilterHit_2")));
	if (AllianceFilter)
	{
		AllianceFilter->OnClicked.Broadcast();
		TestEqual(
			TEXT("Clicking the selected Alliance filter toggles it off"),
			HudWidget->GetSelectedFilter(),
			ERokWorldMapHudFilter::None);
	}
	if (ResourceFilter)
	{
		ResourceFilter->OnClicked.Broadcast();
		TestEqual(
			TEXT("Resource filter becomes the single selected filter"),
			HudWidget->GetSelectedFilter(),
			ERokWorldMapHudFilter::Resources);
		ResourceFilter->OnClicked.Broadcast();
		TestEqual(
			TEXT("Clicking the selected Resource filter toggles it off"),
			HudWidget->GetSelectedFilter(),
			ERokWorldMapHudFilter::None);
	}

	UButton* GlobeButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapGlobeButton")));
	UButton* ServerSelectButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(
			TEXT("WorldMapServerSelectUnavailableButton")));
	UButton* ServerCloseButton = Cast<UButton>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapServerListCloseButton")));
	if (GlobeButton)
	{
		GlobeButton->OnClicked.Broadcast();
		TestTrue(
			TEXT("Globe opens the independent kingdom list boundary"),
			HudWidget->IsServerListOpen());
	}
	if (ServerSelectButton)
	{
		ServerSelectButton->OnClicked.Broadcast();
		TestTrue(
			TEXT("Unavailable kingdom selection remains in the kingdom UI"),
			HudWidget->IsServerListOpen());
	}
	if (ServerCloseButton)
	{
		ServerCloseButton->OnClicked.Broadcast();
		TestFalse(
			TEXT("Kingdom list close button closes the overlay"),
			HudWidget->IsServerListOpen());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudZoomPresentationTest,
	"Rok.WorldMap.Hud.ZoomPresentation",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudZoomPresentationTest::RunTest(
	const FString& Parameters)
{
	URokWorldMapHudWidget* HudWidget =
		NewObject<URokWorldMapHudWidget>(GetTransientPackage());
	HudWidget->Initialize();
	HudWidget->TakeWidget();
	URokMainHudWidget* TacticalHud = Cast<URokMainHudWidget>(
		HudWidget->GetWidgetFromName(TEXT("WorldMapTacticalHud")));
	if (!TestNotNull(TEXT("World HUD owns the shared tactical controls"), TacticalHud))
	{
		return false;
	}
	TacticalHud->TakeWidget();
	for (const FName Name : {
			FName(TEXT("PlayerResourcesGroup")),
			FName(TEXT("HotTaskGroup")),
			FName(TEXT("CampaignFeature")),
			FName(TEXT("AllianceFeature")),
			FName(TEXT("ItemsFeature")),
			FName(TEXT("CommanderFeature"))})
	{
		TestNotNull(*FString::Printf(TEXT("Tactical world reuses %s"), *Name.ToString()),
			TacticalHud->GetWidgetFromName(Name));
	}
	for (const FName Name : {
			FName(TEXT("PlayerPowerGroup")),
			FName(TEXT("PositionGroup")),
			FName(TEXT("ChatButton")),
			FName(TEXT("CityWorldButton")),
			FName(TEXT("PlayerCityWorldMarker")),
			FName(TEXT("BuildingPanelWidget")),
			FName(TEXT("UnityFeatureScreenWidget"))})
	{
		TestNull(*FString::Printf(TEXT("Tactical overlay does not duplicate %s"), *Name.ToString()),
			TacticalHud->GetWidgetFromName(Name));
	}
	HudWidget->UpdateZoomPresentation(1537.0f);
	TestEqual(TEXT("Detailed world shows tactical controls"),
		TacticalHud->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	for (const FName Name : {
			FName(TEXT("WorldMapMiniMap")),
			FName(TEXT("WorldMapRelationFilters")),
			FName(TEXT("WorldMapGlobeButton")),
			FName(TEXT("WorldMapMailButton"))})
	{
		UWidget* Widget = HudWidget->GetWidgetFromName(Name);
		if (TestNotNull(*Name.ToString(), Widget))
		{
			TestEqual(TEXT("Overview-only control is absent in detailed world"),
				Widget->GetVisibility(), ESlateVisibility::Collapsed);
		}
	}
	URokUnityFeatureScreenWidget* SharedFeatureScreen =
		Cast<URokUnityFeatureScreenWidget>(HudWidget->GetWidgetFromName(
			TEXT("WorldMapUnityFeatureScreenWidget")));
	UButton* TacticalMail = Cast<UButton>(
		TacticalHud->GetWidgetFromName(TEXT("MailFeatureHitButton")));
	if (TestNotNull(TEXT("Tactical mail is clickable"), TacticalMail)
		&& TestNotNull(TEXT("Both zoom bands keep one feature host"), SharedFeatureScreen))
	{
		TacticalMail->OnClicked.Broadcast();
		TestTrue(TEXT("Tactical mail opens the existing world feature host"),
			SharedFeatureScreen->IsScreenOpen());
		TestNotNull(TEXT("Shared mail route builds its normal list"),
			SharedFeatureScreen->GetWidgetFromName(TEXT("MailListScroll")));
	}
	HudWidget->UpdateZoomPresentation(3000.0f);
	TestEqual(TEXT("Exit starts with raycasts disabled"),
		TacticalHud->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TacticalHud->AdvanceWorldMapModuleAnimation(0.25f);
	TestEqual(TEXT("Completed module exits hide tactical controls"),
		TacticalHud->GetVisibility(), ESlateVisibility::Collapsed);
	UWidget* MiniMap = HudWidget->GetWidgetFromName(TEXT("WorldMapMiniMap"));
	if (MiniMap)
	{
		TestEqual(TEXT("Overview restores the minimap"),
			MiniMap->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	UWidget* Globe = HudWidget->GetWidgetFromName(TEXT("WorldMapGlobeButton"));
	if (Globe)
	{
		TestEqual(TEXT("Globe remains hidden in transition band"), Globe->GetVisibility(), ESlateVisibility::Collapsed);
		HudWidget->UpdateZoomPresentation(5401);
		TestEqual(TEXT("Strategic globe retains self hit testing"),
			Globe->GetVisibility(), ESlateVisibility::Visible);
	}
	if (SharedFeatureScreen)
	{
		TestTrue(TEXT("Changing zoom does not close or replace the open feature"),
			SharedFeatureScreen->IsScreenOpen());
		SharedFeatureScreen->CloseScreen();
	}
	HudWidget->UpdateZoomPresentation(2997.0f);
	TestEqual(TEXT("Zooming back restores tactical controls"),
		TacticalHud->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	URokMainHudWidget* CityHud = NewObject<URokMainHudWidget>(GetTransientPackage());
	CityHud->Initialize();
	CityHud->TakeWidget();
	TestNotNull(TEXT("Default city HUD retains its identity group"),
		CityHud->GetWidgetFromName(TEXT("PlayerPowerGroup")));
	TestNotNull(TEXT("Default city HUD retains its city/world action"),
		CityHud->GetWidgetFromName(TEXT("CityWorldButton")));
	TestNotNull(TEXT("Default city HUD retains its own feature host"),
		CityHud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudNameplateCurvesTest,
	"Rok.WorldMap.Hud.NameplateCurves",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudNameplateCurvesTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("RssTitleCurve starts at source -0.7"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateResourceBadgeOffset(0), -0.7f));
	TestTrue(TEXT("Resource badge uses zero-tangent interpolation"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateResourceBadgeOffset(1500), -0.75f));
	TestTrue(TEXT("Resource badge clamps beyond overview key"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateResourceBadgeOffset(3500), -0.8f));
	TestTrue(TEXT("Resource badge clamps negative distances"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateResourceBadgeOffset(-100), -0.7f));
	TestTrue(TEXT("NpcTroopCurve clamps below its first source key"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateBarbarianBadgeOffset(-1.0f), 0.0f));
	TestTrue(TEXT("NpcTroopCurve keeps its middle source key"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateBarbarianBadgeOffset(267.6259f), -0.318347f));
	TestTrue(TEXT("Tactical troop offset uses source smoothstep tangents"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateBarbarianBadgeOffset(1537.0f), -0.623005846f, 0.00001f));
	TestTrue(TEXT("Overview troop offset clamps after the final key"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateBarbarianBadgeOffset(3500.0f), -1.0f));
	TestTrue(TEXT("Tactical level plaque uses the shared source UI curve"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateWorldObjectBadgeScale(1537.0f, 0.5f, 30.0f), 1.0190513f, 0.00001f));
	TestTrue(TEXT("Overview level plaque retains the final UI curve scale"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateWorldObjectBadgeScale(3500.0f, 0.5f, 30.0f), 0.66f));
	TestTrue(TEXT("Lower viewport plaques use Unity's depth correction"),
		FMath::IsNearlyEqual(URokWorldMapHudWidget::EvaluateWorldObjectBadgeScale(1537.0f, 0.75f, 30.0f), 1.1190513f, 0.00001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudFilterEntityVisibilityTest,
	"Rok.WorldMap.Hud.FilterEntityVisibility",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudFilterEntityVisibilityTest::RunTest(
	const FString& Parameters)
{
	TestTrue(
		TEXT("Explore filter shows village records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("village"),
			ERokWorldMapHudFilter::Explore));
	TestFalse(
		TEXT("Resource filter hides village records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("village"),
			ERokWorldMapHudFilter::Resources));
	TestTrue(
		TEXT("Resource filter shows food records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("food"),
			ERokWorldMapHudFilter::Resources));
	TestFalse(
		TEXT("No filter hides overlay resource records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("stone"),
			ERokWorldMapHudFilter::None));
	TestTrue(
		TEXT("Normal barbarian gameplay actors are not an overlay category"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("barbarian"),
			ERokWorldMapHudFilter::None));
	TestTrue(
		TEXT("Alliance filter recognizes future alliance records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("alliance_building"),
			ERokWorldMapHudFilter::Alliance));
	TestTrue(
		TEXT("Marker filter recognizes future personal marker records"),
		URokWorldMapHudWidget::ShouldShowEntityForFilter(
			TEXT("personal_marker"),
			ERokWorldMapHudFilter::Markers));
	TestTrue(
		TEXT("Food routes to the Unity resource-object popup"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT("food")));
	TestTrue(
		TEXT("Wood routes to the Unity resource-object popup"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT("WOOD")));
	TestTrue(
		TEXT("Stone routes to the Unity resource-object popup"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT(" stone ")));
	TestFalse(
		TEXT("Barbarians keep their dedicated future popup route"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT("barbarian")));
	TestFalse(
		TEXT("Villages keep their dedicated future popup route"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT("village")));
	TestFalse(
		TEXT("Alliance resources keep their dedicated future popup route"),
		URokWorldMapHudWidget::IsResourceEntityKind(
			TEXT("alliance_resource")));
	TestFalse(
		TEXT("Generic resource rows require an explicit visual route"),
		URokWorldMapHudWidget::IsResourceEntityKind(TEXT("resource")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapFeatureScreenEntryTest,
	"Rok.WorldMap.Hud.FeatureScreenEntries",
	EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::EngineFilter)

bool FRokWorldMapFeatureScreenEntryTest::RunTest(
	const FString& Parameters)
{
	URokUnityFeatureScreenWidget* FeatureWidget =
		NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	TestNotNull(
		TEXT("Feature screen can be constructed"),
		FeatureWidget);
	if (!FeatureWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("Feature screen initializes its runtime WidgetTree"),
		FeatureWidget->Initialize());
	FeatureWidget->TakeWidget();
	URokUnityModalWidget* ModalWidget =
		NewObject<URokUnityModalWidget>(GetTransientPackage());
	TestNotNull(TEXT("Modal screen can be constructed"), ModalWidget);
	if (!ModalWidget)
	{
		return false;
	}
	TestTrue(
		TEXT("Modal screen initializes its runtime WidgetTree"),
		ModalWidget->Initialize());
	ModalWidget->TakeWidget();

	FeatureWidget->OpenChat();
	TestTrue(TEXT("Chat screen opens"), FeatureWidget->IsScreenOpen());
	TestNotNull(
		TEXT("Chat screen has a real send button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatSendButton"))));
	TestNotNull(
		TEXT("World-map chat screen has a real emoji toggle"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("ChatEmojiButton"))));
	TestNotNull(
		TEXT("World-map chat screen has an editable message field"),
		Cast<UEditableTextBox>(FeatureWidget->GetWidgetFromName(
			TEXT("ChatInputTextBox"))));
	TestNotNull(
		TEXT("World-map chat screen uses the honest empty message state"),
		Cast<UTextBlock>(FeatureWidget->GetWidgetFromName(
			TEXT("ChatMessagesEmptyState"))));
	TestNull(
		TEXT("World-map chat default does not fabricate messages"),
		FeatureWidget->GetWidgetFromName(TEXT("ChatMessage_0")));

	FeatureWidget->OpenVip();
	UTextBlock* VipProgress = Cast<UTextBlock>(
		FeatureWidget->GetWidgetFromName(TEXT("VipProgressText")));
	TestTrue(
		TEXT("World-map VIP screen keeps missing account progress honest"),
		VipProgress
			&& VipProgress->GetText().ToString().Contains(TEXT("未同步")));
	TestNull(
		TEXT("World-map VIP screen does not fabricate a reward red point"),
		FeatureWidget->GetWidgetFromName(TEXT("VipDailyPointRedPoint")));
	UButton* VipPurchase = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipBuySpecialChest")));
	TestTrue(
		TEXT("World-map VIP purchase waits for a role snapshot"),
		VipPurchase && !VipPurchase->GetIsEnabled());
	UButton* VipFreeChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipFreeChestHit")));
	UButton* VipSpecialChestHit = Cast<UButton>(
		FeatureWidget->GetWidgetFromName(TEXT("VipSpecialChestHit")));
	TestTrue(
		TEXT("World-map VIP free-chest image waits for a role snapshot"),
		VipFreeChestHit && !VipFreeChestHit->GetIsEnabled());
	TestTrue(
		TEXT("World-map VIP special-chest image waits for a role snapshot"),
		VipSpecialChestHit && !VipSpecialChestHit->GetIsEnabled());

	FeatureWidget->OpenWorldPositionSearch();
	TestNotNull(
		TEXT("Coordinate search has a real go button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("WorldPositionGo"))));

	FeatureWidget->OpenMapMarkers();
	TestNotNull(
		TEXT("Map marker list has a real go button"),
		Cast<UButton>(
			FeatureWidget->GetWidgetFromName(TEXT("MapMarkerGo"))));

	FeatureWidget->OpenCharge(107);
	static const int32 ChargePageTypes[] = {
		101,
		102,
		103,
		104,
		107,
		105,
		106,
	};
	for (const int32 PageType : ChargePageTypes)
	{
		TestNotNull(
			*FString::Printf(
				TEXT("World-map charge API exposes page %d"),
				PageType),
			Cast<UButton>(FeatureWidget->GetWidgetFromName(
				*FString::Printf(TEXT("ChargePage_%d"), PageType))));
	}

	ModalWidget->OpenWarWarning(false);
	TestNotNull(
		TEXT("World-map war-warning API exposes the server boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("WarWarningServiceBoundaryState"))));
	TestNull(
		TEXT("World-map production warning API does not fabricate a row"),
		ModalWidget->GetWidgetFromName(TEXT("WarWarningRow_0")));

	ModalWidget->OpenBuffList(false);
	TestNotNull(
		TEXT("World-map buff-list API exposes the server boundary"),
		Cast<UTextBlock>(ModalWidget->GetWidgetFromName(
			TEXT("BuffListServiceBoundaryState"))));
	TestNull(
		TEXT("World-map production buff API does not fabricate a row"),
		ModalWidget->GetWidgetFromName(TEXT("BuffListRow_0")));
	ModalWidget->CloseModal();
	FeatureWidget->CloseScreen();
	TestFalse(TEXT("Feature screen closes"), FeatureWidget->IsScreenOpen());
	return true;
}

#endif
