#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "../RokHudAnimation.h"
#include "../RokMainHudWidget.h"
#include "../RokWorldMapHudWidget.h"
#include "Sound/SoundWave.h"
#include "../RokBuildingPanelWidget.h"
#include "GameFramework/Actor.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudPowerCounterTest,
	"Rok.Ui.Animation.Hud.PowerCounter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokHudPowerCounterTest::RunTest(const FString&)
{
	auto* PowerMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokPrototype/UI/Materials/M_RokPowerGainUI.M_RokPowerGainUI"));
	if (!TestNotNull(TEXT("Source additive power material saved and reloadable"), PowerMaterial)) return false;
	TestEqual(TEXT("Mobile particle shader uses additive blending"), PowerMaterial->GetBlendMode(), BLEND_Additive);
	float UTiling = 0;
	TestTrue(TEXT("UV animation parameter is bindable"), PowerMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("UTiling")), UTiling));
	TestEqual(TEXT("UV animation starts at source tiling one"), UTiling, 1.0f);
	FRokPowerCounterAnimation Counter;
	Counter.SetTarget(1000);
	TestEqual(TEXT("Initial load does not count up from zero"), Counter.Value, 1000);
	Counter.SetTarget(1200);
	Counter.Advance(0.6f);
	TestEqual(TEXT("OutCirc timeline reaches callback 17 at half duration"), Counter.Value, 1160);
	Counter.SetTarget(1200);
	Counter.Advance(0.6f);
	TestEqual(TEXT("Repeated refresh does not restart; final target exact"), Counter.Value, 1200);
	Counter.SetTarget(1000);
	Counter.Advance(0.6f);
	TestEqual(TEXT("Decreases use the same sequence"), Counter.Value, 1040);
	Counter.SetTarget(1500);
	TestEqual(TEXT("Retarget preserves current visible value"), Counter.Value, 1040);
	Counter.Advance(2);
	TestEqual(TEXT("Previous completion cannot overwrite new target"), Counter.Value, 1500);
	Counter.SetTarget(0);
	TestEqual(TEXT("Unavailable state cancels counter"), Counter.Value, 0);
	auto* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize(); Hud->TakeWidget();
	Hud->SetPowerPresentation(1000); Hud->SetPowerPresentation(1200);
	Hud->AdvanceHudAnimations(0.6f);
	auto* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("PlayerPowerText")));
	if (!TestNotNull(TEXT("Main HUD live power label"), Text)) return false;
	TestEqual(TEXT("Main HUD binds animated value"), Text->GetText().ToString(), FText::AsNumber(1160).ToString());
	auto* WorldHud = NewObject<URokWorldMapHudWidget>();
	WorldHud->Initialize(); WorldHud->TakeWidget();
	WorldHud->SetPowerPresentation(1000); WorldHud->SetPowerPresentation(1200);
	WorldHud->AdvancePowerPresentation(0.6f);
	auto* WorldText = Cast<UTextBlock>(WorldHud->GetWidgetFromName(TEXT("WorldMapPowerText")));
	if (!TestNotNull(TEXT("World HUD live power label"), WorldText)) return false;
	TestEqual(TEXT("World HUD shares counter behavior"), WorldText->GetText().ToString(), FText::AsNumber(1160).ToString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudChatUnreadTest,
	"Rok.Ui.Animation.Hud.ChatUnread", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokHudChatUnreadTest::RunTest(const FString&)
{
	auto* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize(); Hud->TakeWidget();
	UWidget* Alliance = Hud->GetWidgetFromName(TEXT("ChatAllianceMarker"));
	UWidget* World = Hud->GetWidgetFromName(TEXT("ChatWorldMarker"));
	if (!TestNotNull(TEXT("Source alliance marker"), Alliance) || !TestNotNull(TEXT("Source world marker"), World)) return false;
	TestEqual(TEXT("No fabricated alliance on startup"), Alliance->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->SetChatChannelPresentation(true, false, 2);
	TestEqual(TEXT("Unread starts transparent"), Alliance->GetRenderOpacity(), 0.0f);
	Hud->AdvanceHudAnimations(.25f);
	TestEqual(TEXT("Source flash midpoint"), Alliance->GetRenderOpacity(), .5f);
	Hud->SetChatChannelPresentation(true, false, 3);
	TestEqual(TEXT("Unread count refresh does not restart loop"), Alliance->GetRenderOpacity(), .5f);
	Hud->AdvanceHudAnimations(.25f);
	TestEqual(TEXT("Source flash peak"), Alliance->GetRenderOpacity(), 1.0f);
	Hud->AdvanceHudAnimations(.5f);
	TestEqual(TEXT("Source one-second loop"), Alliance->GetRenderOpacity(), 0.0f);
	Hud->SetChatChannelPresentation(true, true, 3);
	Hud->AdvanceHudAnimations(.75f);
	TestEqual(TEXT("Selected alliance does not blink"), Alliance->GetRenderOpacity(), 1.0f);
	TestEqual(TEXT("Selected alliance clears world highlight"), World->GetRenderOpacity(), 0.0f);
	Hud->SetChatChannelPresentation(true, false, 0);
	Hud->AdvanceHudAnimations(.5f);
	TestEqual(TEXT("No unread stops flash"), Alliance->GetRenderOpacity(), 0.0f);
	Hud->SetChatChannelPresentation(false, true, 5);
	TestEqual(TEXT("Leaving alliance hides marker"), Alliance->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Leaving alliance restores world"), World->GetRenderOpacity(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResourceSearchMotionTest,
	"Rok.Ui.Animation.ResourceSearch.SourceMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokResourceSearchMotionTest::RunTest(const FString& Parameters)
{
	FRokResourceSearchAnimation Motion;
	Motion.Open();
	TestEqual(TEXT("Dock starts below settled position"), Motion.DockTranslationY, 145.0f);
	TestEqual(TEXT("Popup starts scaled out, not faded"), Motion.PopupScale, 0.0f);
	Motion.Advance(8.0f / 60.0f);
	TestEqual(TEXT("Popup waits eight frames"), Motion.PopupScale, 0.0f);
	Motion.Advance(2.0f / 60.0f);
	TestTrue(TEXT("Dock overshoots upward by 35"), FMath::IsNearlyEqual(Motion.DockTranslationY, -35.0f));
	Motion.Advance(7.0f / 60.0f);
	TestTrue(TEXT("Dock settles"), FMath::IsNearlyZero(Motion.DockTranslationY));
	TestEqual(TEXT("Popup finishes at native scale"), Motion.PopupScale, 1.0f);
	Motion.Close();
	TestFalse(TEXT("Close callback is not immediate"), Motion.Advance(0));
	TestTrue(TEXT("Source close callback occurs at frame one"), Motion.Advance(1.0f / 60.0f));
	TestFalse(TEXT("Close callback only once"), Motion.Advance(1));

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize();
	Screen->TakeWidget();
	Screen->OpenResourceSearch();
	auto* Dock = Screen->GetWidgetFromName(TEXT("ResourceSearchDockRoot"));
	auto* Popup = Screen->GetWidgetFromName(TEXT("ResourceSearchPopupRoot"));
	auto* Mask = Screen->GetWidgetFromName(TEXT("ResourceSearchTouchCloseMask"));
	if (!TestNotNull(TEXT("Dock group exists"), Dock) || !TestNotNull(TEXT("Popup group exists"), Popup)
		|| !TestNotNull(TEXT("Stationary close mask exists"), Mask)) return false;
	TestTrue(TEXT("Full screen mask never moves with dock"), Mask->GetParent() != Dock);
	TestEqual(TEXT("No invented popup fade"), Popup->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("Source popup pivot remains centered"), Popup->GetRenderTransformPivot().Equals(FVector2D(0.5f)));
	Screen->AdvanceWindowAnimationForTesting(1);
	auto* Food = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ResourceSearchType_1")));
	if (TestNotNull(TEXT("Food type remains clickable"), Food))
	{
		Food->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		Popup = Screen->GetWidgetFromName(TEXT("ResourceSearchPopupRoot"));
		TestTrue(TEXT("Type change does not replay entrance"), Popup->GetRenderTransform().Scale.Equals(FVector2D(1)));
	}
	Screen->RequestCloseScreen();
	Screen->AdvanceWindowAnimationForTesting(1.0f / 60.0f);
	TestFalse(TEXT("Real screen closes at source callback"), Screen->IsScreenOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingMenuAnimationTest,
	"Rok.Ui.Animation.BuildingMenu.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBuildingMenuAnimationTest::RunTest(const FString& Parameters)
{
	auto* Menu = NewObject<URokBuildingPanelWidget>();
	Menu->Initialize();
	Menu->TakeWidget();
	auto* Title = Menu->GetWidgetFromName(TEXT("BuildingTitleRoot"));
	auto* Root = Menu->GetWidgetFromName(TEXT("BuildingMenuRoot"));
	auto* Info = Cast<UButton>(Menu->GetWidgetFromName(TEXT("BuildingInfoButton")));
	if (!TestNotNull(TEXT("Animated title group"), Title) || !TestNotNull(TEXT("Menu root"), Root)
		|| !TestNotNull(TEXT("Unscaled command"), Info)) return false;
	// Identity-only fixture: no actor mutation or gameplay subsystem is needed.
	AActor* Identity = GetMutableDefault<AActor>();
	Menu->SetSelectedBuilding(Identity);
	TestEqual(TEXT("Open begins transparent"), Root->GetRenderOpacity(), 0.0f);
	Menu->AdvanceMenuAnimation(7.0f / 60.0f);
	TestTrue(TEXT("Title follows source 1.2 peak"), Title->GetRenderTransform().Scale.Equals(FVector2D(1.2f), 0.0001f));
	TestTrue(TEXT("Buttons do not inherit title scaling"), Info->GetParent() == Root);
	TestTrue(TEXT("Title uses source top pivot"), Title->GetRenderTransformPivot().Equals(FVector2D(0.5f, 0)));
	Menu->SetSelectedBuilding(Identity);
	TestTrue(TEXT("Same selection does not restart"), Title->GetRenderTransform().Scale.Equals(FVector2D(1.2f), 0.0001f));
	Menu->AdvanceMenuAnimation(0.25f);
	Menu->SetSelectedBuilding(nullptr);
	TestEqual(TEXT("Closing blocks outgoing commands"), Menu->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Menu->AdvanceMenuAnimation(5.0f / 60.0f);
	TestTrue(TEXT("Close has source title peak"), Title->GetRenderTransform().Scale.Equals(FVector2D(1.2f), 0.0001f));
	Menu->SetSelectedBuilding(nullptr);
	Menu->AdvanceMenuAnimation(0.17f);
	TestEqual(TEXT("Repeated deselection cannot prolong close"), Menu->GetVisibility(), ESlateVisibility::Collapsed);
	Menu->SetSelectedBuilding(Identity);
	Menu->AdvanceMenuAnimation(0.06f);
	const float BeforeClose = Root->GetRenderOpacity();
	Menu->SetSelectedBuilding(nullptr);
	TestEqual(TEXT("Interrupted close preserves alpha"), Root->GetRenderOpacity(), BeforeClose);
	Menu->AdvanceMenuAnimation(0.04f);
	const float BeforeReopen = Root->GetRenderOpacity();
	Menu->SetSelectedBuilding(Identity);
	TestEqual(TEXT("Reopen preserves alpha"), Root->GetRenderOpacity(), BeforeReopen);
	Menu->AdvanceMenuAnimation(0.25f);
	TestEqual(TEXT("Reopened menu is visible"), Menu->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Reopened menu reaches full alpha"), Root->GetRenderOpacity(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudRewardAnimationTest,
	"Rok.Ui.Animation.Hud.TaskReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudRewardAnimationTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> SlateWidget = Hud->TakeWidget();
	UImage* Reward = Cast<UImage>(Hud->GetWidgetFromName(TEXT("TaskReward_0")));
	if (!TestNotNull(TEXT("Source reward sprite exists"), Reward)) return false;
	TestNull(TEXT("No fabricated side-task rows"), Hud->GetWidgetFromName(TEXT("TaskRow_1")));
	TestEqual(TEXT("Unloaded gift is hidden"), Reward->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->SetTaskRewardPresentation(200900, false);
	TestEqual(TEXT("Unfinished gift stays hidden"), Reward->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->SetTaskRewardPresentation(200900, true);
	Hud->AdvanceHudAnimations(1.0f / 6.0f);
	TestTrue(TEXT("First scale peak is 1.2"), Reward->GetRenderTransform().Scale.Equals(FVector2D(1.2f), 0.0001f));
	TestTrue(TEXT("Unity positive Z maps to UMG -2 degrees"), FMath::IsNearlyEqual(Reward->GetRenderTransform().Angle, -2.0f));
	Hud->SetTaskRewardPresentation(200900, true);
	Hud->AdvanceHudAnimations(1.0f / 6.0f);
	TestTrue(TEXT("Refreshing same task does not restart loop"), Reward->GetRenderTransform().Scale.Equals(FVector2D(1.0f), 0.0001f));
	Hud->AdvanceHudAnimations(1.0f / 6.0f);
	TestTrue(TEXT("Second scale peak"), Reward->GetRenderTransform().Scale.Equals(FVector2D(1.2f), 0.0001f));
	Hud->AdvanceHudAnimations(0.5f);
	TestTrue(TEXT("Rest after two hops"), Reward->GetRenderTransform().Scale.Equals(FVector2D(1.0f), 0.0001f));
	Hud->SetTaskRewardPresentation(200901, true);
	TestTrue(TEXT("New identity resets pose"), Reward->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	Hud->SetTaskRewardPresentation(0, true);
	TestEqual(TEXT("No task cannot show reward"), Reward->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Loop starts cleanly"), FMath::IsNearlyZero(RokHudAnimation::RewardPulse(1.5f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudMotionCurvesTest,
	"Rok.Ui.Animation.Hud.SourceCurves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudMotionCurvesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Task clip midpoint has zero-tangent interpolation"),
		RokHudAnimation::Smooth(1.0f / 6.0f, RokHudAnimation::TaskDuration), 0.5f);
	TestTrue(TEXT("Feature open starts at 440.5"),
		FMath::IsNearlyEqual(RokHudAnimation::FeatureOpenOffset(0.0f), 440.5f));
	TestTrue(TEXT("Feature open preserves the second-frame key"),
		FMath::IsNearlyEqual(RokHudAnimation::FeatureOpenOffset(0.033333335f), 425.9f));
	TestTrue(TEXT("Feature open reaches zero"),
		FMath::IsNearlyZero(RokHudAnimation::FeatureOpenOffset(0.25f)));
	TestTrue(TEXT("Feature authored middle tangent is not discarded"),
		FMath::IsNearlyEqual((RokHudAnimation::FeatureOpenOffset(0.033343335f)
			- RokHudAnimation::FeatureOpenOffset(0.033323335f)) / 0.00002f,
			-233.6001f, 3.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudTaskAnimationTest,
	"Rok.Ui.Animation.Hud.TaskFold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudTaskAnimationTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> SlateWidget = Hud->TakeWidget();
	UButton* Fold = Cast<UButton>(Hud->GetWidgetFromName(TEXT("TaskFoldButton")));
	UCanvasPanel* Background = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("TaskListBackground")));
	UCanvasPanel* Clip = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("TaskListClip")));
	TestNotNull(TEXT("Fold arrow is a real button"), Fold);
	TestNotNull(TEXT("Source task background"), Background);
	TestNotNull(TEXT("Source list has a clipping viewport"), Clip);
	if (!Fold || !Background || !Clip) return false;
	TestEqual(TEXT("Task panel uses nine source atlas patches"), Background->GetChildrenCount(), 9);
	UImage* Corner = Cast<UImage>(Hud->GetWidgetFromName(TEXT("TaskListBackground_0_0")));
	UImage* Center = Cast<UImage>(Hud->GetWidgetFromName(TEXT("TaskListBackground_1_1")));
	UCanvasPanel* RowBackground = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("TaskRowBackground_0")));
	if (!TestNotNull(TEXT("Panel corner"), Corner) || !TestNotNull(TEXT("Panel center"), Center)
		|| !TestNotNull(TEXT("Row uses source slices too"), RowBackground)) return false;
	TestEqual(TEXT("Image patches avoid whole-atlas Box margin scaling"), Corner->GetBrush().DrawAs, ESlateBrushDrawType::Image);
	UCanvasPanelSlot* CornerSlot = CastChecked<UCanvasPanelSlot>(Corner->Slot);
	TestTrue(TEXT("Source corner is 9 by 13 pixels"), CornerSlot->GetSize().Equals(FVector2D(9, 13)));
	UCanvasPanelSlot* CenterSlot = CastChecked<UCanvasPanelSlot>(Center->Slot);
	TestTrue(TEXT("Center stretches with parent"), CenterSlot->GetAnchors().Maximum.Equals(FVector2D(1)));
	TestTrue(TEXT("Center keeps exact sprite borders"), CenterSlot->GetOffsets() == FMargin(9, 13, 8, 12));
	const FBox2f CornerUV = Corner->GetBrush().GetUVRegion();
	TestTrue(TEXT("Atlas top uses flipped Unity Y"), CornerUV.Min.Equals(FVector2f(499.0f / 1024, 454.0f / 1024), 0.00001f));
	TestTrue(TEXT("Corner UV includes 9 by 13 source pixels"), CornerUV.GetSize().Equals(FVector2f(9.0f / 1024, 13.0f / 1024), 0.00001f));
	TestEqual(TEXT("Row has all nine patches"), RowBackground->GetChildrenCount(), 9);
	UCanvasPanelSlot* BgSlot = CastChecked<UCanvasPanelSlot>(Background->Slot);
	UCanvasPanelSlot* ClipSlot = CastChecked<UCanvasPanelSlot>(Clip->Slot);
	TestTrue(TEXT("Expanded task background uses source size"),
		BgSlot->GetSize().Equals(FVector2D(230.0f, 322.0f)));
	TestTrue(TEXT("Chinese runtime background uses mirrored left/top anchor"),
		BgSlot->GetPosition().Equals(FVector2D(5.63f, 44.52f), 0.001f));
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(RokHudAnimation::TaskDuration / 2.0f);
	TestTrue(TEXT("Background resizes through the source midpoint"),
		BgSlot->GetSize().Equals(FVector2D(198.0f, 182.5f), 0.01f));
	TestTrue(TEXT("List clips instead of vertically compressing its rows"),
		ClipSlot->GetSize().Equals(FVector2D(178.0f, 140.0f), 0.01f));
	TestTrue(TEXT("Centered source list tracks the resizing background"),
		ClipSlot->GetPosition().Equals(FVector2D(15.63f, 82.05f), 0.01f));
	const FVector2D MidSize = BgSlot->GetSize();
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(0.0f);
	TestTrue(TEXT("Rapid reversal does not jump to an endpoint"), BgSlot->GetSize().Equals(MidSize));
	Hud->AdvanceHudAnimations(RokHudAnimation::TaskDuration);
	TestTrue(TEXT("Reversal finishes fully open"), BgSlot->GetSize().Equals(FVector2D(230.0f, 322.0f)));
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(1.0f);
	TestTrue(TEXT("Collapsed source background remains a header"), BgSlot->GetSize().Equals(FVector2D(166.0f, 43.0f)));
	TestTrue(TEXT("Fold does not stretch source corner"), CornerSlot->GetSize().Equals(FVector2D(9, 13)));
	TestTrue(TEXT("Fold preserves center insets"), CenterSlot->GetOffsets() == FMargin(9, 13, 8, 12));
	TestEqual(TEXT("Zero-height list does not receive input"), Clip->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("Toggle itself stays visible"), Fold->IsVisible());
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(1.0f);
	TestEqual(TEXT("Opening restores list visibility"), Clip->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudFeatureSlicesTest,
	"Rok.Ui.Animation.Hud.FeatureSlices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudFeatureSlicesTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> SlateWidget = Hud->TakeWidget();
	auto* Background = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("FeatureBackground")));
	auto* Fold = Cast<UButton>(Hud->GetWidgetFromName(TEXT("MoreFeatureButton")));
	if (!TestNotNull(TEXT("Feature sliced background"), Background) || !TestNotNull(TEXT("More toggle"), Fold)) return false;
	TestEqual(TEXT("Nine source patches"), Background->GetChildrenCount(), 9);
	auto* Corner = Cast<UImage>(Background->GetChildAt(0));
	if (!TestNotNull(TEXT("Corner is an atlas image"), Corner)) return false;
	TestEqual(TEXT("No full-atlas Box scaling"), Corner->GetBrush().DrawAs, ESlateBrushDrawType::Image);
	auto* CornerSlot = CastChecked<UCanvasPanelSlot>(Corner->Slot);
	auto* CenterSlot = CastChecked<UCanvasPanelSlot>(Background->GetChildAt(4)->Slot);
	auto* BgSlot = CastChecked<UCanvasPanelSlot>(Background->Slot);
	TestTrue(TEXT("Source corner dimensions"), CornerSlot->GetSize().Equals(FVector2D(59, 50)));
	TestTrue(TEXT("Source borders leave only 2px vertical center at target height"), CenterSlot->GetOffsets() == FMargin(59, 50, 53, 46));
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(0.125f);
	TestTrue(TEXT("Source width midpoint"), BgSlot->GetSize().Equals(FVector2D(320.08f, 98), 0.01f));
	TestTrue(TEXT("Borders do not stretch at midpoint"), CornerSlot->GetSize().Equals(FVector2D(59, 50)));
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(0.0f);
	TestTrue(TEXT("Reversal preserves background width"), BgSlot->GetSize().Equals(FVector2D(320.08f, 98), 0.01f));
	Hud->AdvanceHudAnimations(1.0f);
	TestTrue(TEXT("Reversal opens fully"), BgSlot->GetSize().Equals(FVector2D(539.06f, 98), 0.01f));
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(1.0f);
	TestTrue(TEXT("Collapsed width follows source"), BgSlot->GetSize().Equals(FVector2D(101.1f, 98), 0.01f));
	TestTrue(TEXT("Small rect compresses corners proportionally"),
		FMath::IsNearlyEqual(CornerSlot->GetSize().X, 59.0 * 101.1 / 112.0, 0.001));
	const FMargin Insets = CenterSlot->GetOffsets();
	TestTrue(TEXT("Collapsed borders meet without overlap"),
		FMath::IsNearlyEqual(static_cast<double>(Insets.Left + Insets.Right), BgSlot->GetSize().X, 0.001));
	TestEqual(TEXT("Finished close hides background"), Background->GetVisibility(), ESlateVisibility::Collapsed);
	Fold->OnClicked.Broadcast();
	Hud->AdvanceHudAnimations(1.0f);
	TestTrue(TEXT("Reopen restores source corner"), CornerSlot->GetSize().Equals(FVector2D(59, 50)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudTopBarSlicesTest,
	"Rok.Ui.Animation.Hud.TopBarSlices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudTopBarSlicesTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> SlateWidget = Hud->TakeWidget();
	for (const TCHAR* Prefix : {TEXT("Position"), TEXT("FoodResource"), TEXT("WoodResource"), TEXT("GemResource")})
	{
		TestNull(TEXT("No invented opaque layer behind source sprite"),
			Hud->GetWidgetFromName(FName(*FString::Printf(TEXT("%sShade"), Prefix))));
		auto* Background = Cast<UCanvasPanel>(Hud->GetWidgetFromName(FName(*FString::Printf(TEXT("%sBackground"), Prefix))));
		if (!TestNotNull(Prefix, Background)) return false;
		TestEqual(TEXT("Nine source patches"), Background->GetChildrenCount(), 9);
		TestEqual(TEXT("Decorative background cannot intercept input"), Background->GetVisibility(), ESlateVisibility::HitTestInvisible);
		auto* Corner = Cast<UImage>(Background->GetChildAt(0));
		if (!TestNotNull(TEXT("Source corner image"), Corner)) return false;
		TestEqual(TEXT("No full-atlas Box margins"), Corner->GetBrush().DrawAs, ESlateBrushDrawType::Image);
		TestTrue(TEXT("Source corner remains 9 by 13"), CastChecked<UCanvasPanelSlot>(Corner->Slot)->GetSize().Equals(FVector2D(9, 13)));
		TestTrue(TEXT("Original header height retained"), FMath::IsNearlyEqual(CastChecked<UCanvasPanelSlot>(Background->Slot)->GetSize().Y, 40.0));
	}
	TestNotNull(TEXT("Coordinate search is still a button"), Cast<UButton>(Hud->GetWidgetFromName(TEXT("PositionSearchButton"))));
	TestNotNull(TEXT("Gem plus is still a button"), Cast<UButton>(Hud->GetWidgetFromName(TEXT("GemResourceAddButton"))));
	URokWorldMapHudWidget* WorldHud = NewObject<URokWorldMapHudWidget>();
	WorldHud->Initialize();
	const TSharedRef<SWidget> WorldSlateWidget = WorldHud->TakeWidget();
	TestNull(TEXT("World coordinate path also has no extra shade"), WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateShade")));
	auto* WorldBackground = Cast<UCanvasPanel>(WorldHud->GetWidgetFromName(TEXT("WorldMapCoordinateBackground")));
	if (!TestNotNull(TEXT("Actual world coordinate background is sliced"), WorldBackground)) return false;
	TestEqual(TEXT("World coordinate nine patches"), WorldBackground->GetChildrenCount(), 9);
	TestTrue(TEXT("World coordinate corner matches city"),
		CastChecked<UCanvasPanelSlot>(WorldBackground->GetChildAt(0)->Slot)->GetSize().Equals(FVector2D(9, 13)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudChatSlicesTest,
	"Rok.Ui.Animation.Hud.ChatSlices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudChatSlicesTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* City = NewObject<URokMainHudWidget>();
	URokWorldMapHudWidget* World = NewObject<URokWorldMapHudWidget>();
	City->Initialize();
	World->Initialize();
	const TSharedRef<SWidget> CitySlate = City->TakeWidget();
	const TSharedRef<SWidget> WorldSlate = World->TakeWidget();
	UCanvasPanel* Backgrounds[] = {
		Cast<UCanvasPanel>(City->GetWidgetFromName(TEXT("ChatBackground"))),
		Cast<UCanvasPanel>(World->GetWidgetFromName(TEXT("WorldMapChatBackground")))};
	for (UCanvasPanel* Background : Backgrounds)
	{
		if (!TestNotNull(TEXT("Chat background uses source bands"), Background)) return false;
		TestEqual(TEXT("Zero-width side borders emit no patches"), Background->GetChildrenCount(), 3);
		TestEqual(TEXT("Chat decoration never consumes clicks"), Background->GetVisibility(), ESlateVisibility::HitTestInvisible);
		auto* Top = Cast<UImage>(Background->GetChildAt(0));
		auto* Bottom = Cast<UImage>(Background->GetChildAt(2));
		if (!Top || !Bottom) return false;
		TestEqual(TEXT("No Box atlas scaling"), Top->GetBrush().DrawAs, ESlateBrushDrawType::Image);
		TestTrue(TEXT("Top band remains six pixels"), FMath::IsNearlyEqual(CastChecked<UCanvasPanelSlot>(Top->Slot)->GetSize().Y, 6.0));
		TestTrue(TEXT("Bottom band remains three pixels"), FMath::IsNearlyEqual(CastChecked<UCanvasPanelSlot>(Bottom->Slot)->GetSize().Y, 3.0));
		TestTrue(TEXT("Source tint is white without added blue/alpha"), Top->GetColorAndOpacity().Equals(FLinearColor::White));
		const FBox2f UV = Top->GetBrush().GetUVRegion();
		TestTrue(TEXT("Source top band samples full width"), UV.GetSize().Equals(FVector2f(768.0f / 1024, 6.0f / 1024), 0.00001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudFoldSoundTest,
	"Rok.Ui.Animation.Hud.FoldSound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudFoldSoundTest::RunTest(const FString& Parameters)
{
	const TCHAR* Names[] = {TEXT("Sound_Ui_CommonClickButton1"), TEXT("Sound_Ui_PickUp")};
	const int32 Samples[] = {6912, 25344};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto* Sound = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/RokPrototype/UI/Audio/%s.%s"), Names[Index], Names[Index]));
		if (!TestNotNull(Names[Index], Sound)) return false;
		TestEqual(TEXT("Unity mono import preserved"), Sound->NumChannels, 1);
		TestFalse(TEXT("Click audio does not loop"), Sound->bLooping);
		TestTrue(TEXT("Original decoded duration"), FMath::IsNearlyEqual(Sound->Duration, Samples[Index] / 44100.0f, 0.001f));
	}
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> SlateWidget = Hud->TakeWidget();
	TestEqual(TEXT("Construction is silent"), Hud->GetFoldSoundRequestsForTesting(), 0);
	int32 ExpectedRequests = 0;
	for (const TCHAR* Name : {TEXT("TaskFoldButton"), TEXT("MoreFeatureButton")})
	{
		auto* Button = Cast<UButton>(Hud->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		for (int32 Click = 0; Click < 2; ++Click)
		{
			Button->OnPressed.Broadcast();
			Button->OnReleased.Broadcast();
			TestEqual(TEXT("Press/release does not play"), Hud->GetFoldSoundRequestsForTesting(), ExpectedRequests);
			Button->OnClicked.Broadcast();
			TestEqual(TEXT("Open and close each play exactly once"), Hud->GetFoldSoundRequestsForTesting(), ++ExpectedRequests);
			Hud->AdvanceHudAnimations(1.0f);
			TestEqual(TEXT("Animation cannot repeat the sound"), Hud->GetFoldSoundRequestsForTesting(), ExpectedRequests);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHudMoreOverlayTest,
	"Rok.Ui.Animation.Hud.MoreSourceOverlay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokHudMoreOverlayTest::RunTest(const FString& Parameters)
{
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>();
	Hud->Initialize();
	const TSharedRef<SWidget> Slate = Hud->TakeWidget();
	auto* Button = Cast<UButton>(Hud->GetWidgetFromName(TEXT("MoreFeatureButton")));
	auto* Base = Cast<UImage>(Hud->GetWidgetFromName(TEXT("MoreFeatureButtonImage")));
	auto* Overlay = Cast<UImage>(Hud->GetWidgetFromName(TEXT("MoreFeatureSourceHideImage")));
	if (!TestNotNull(TEXT("More button"), Button) || !TestNotNull(TEXT("Base sprite"), Base)
		|| !TestNotNull(TEXT("Source img_hide"), Overlay)) return false;
	const FBox2f BaseUV = Base->GetBrush().GetUVRegion();
	const FBox2f OverlayUV = Overlay->GetBrush().GetUVRegion();
	TestTrue(TEXT("Both source images use the instance-overridden sprite"), BaseUV == OverlayUV);
	TestEqual(TEXT("Expanded overlay is visible but cannot intercept clicks"), Overlay->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Source hides overlay immediately, before rotation"), Overlay->GetVisibility(), ESlateVisibility::Collapsed);
	Hud->AdvanceHudAnimations(0.125f);
	TestEqual(TEXT("Overlay stays hidden mid-close"), Overlay->GetVisibility(), ESlateVisibility::Collapsed);
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Opening immediately restores source overlay"), Overlay->GetVisibility(), ESlateVisibility::HitTestInvisible);
	Hud->AdvanceHudAnimations(1.0f);
	TestTrue(TEXT("Both images rotate with their button parent"), Base->GetParent() == Overlay->GetParent()
		&& Base->GetParent() == Button->GetContent());
	return true;
}

#endif
