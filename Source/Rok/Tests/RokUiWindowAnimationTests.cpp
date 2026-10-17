#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "../RokUiWindowAnimation.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "../RokCivilizationAnimation.h"
#include "../RokBuildCityCardAnimation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildCityCardScaleTest,
	"Rok.Ui.Animation.BuildCity.CardStagger", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokBuildCityCardScaleTest::RunTest(const FString&)
{
	using namespace RokBuildCityCardAnimation;
	TestEqual(TEXT("Card is never hidden before its timer"), Scale(0, 0), 1.0f);
	TestEqual(TEXT("First timer begins at 0.05 seconds"), Scale(0.05f, 0), 1.0f);
	TestTrue(TEXT("Source peak at seven frames after timer"), FMath::IsNearlyEqual(Scale(0.05f + 7.0f / 60.0f, 0), 1.2f));
	TestTrue(TEXT("Second card has exact 0.05 second stagger"), FMath::IsNearlyEqual(Scale(0.15f, 1), Scale(0.1f, 0)));
	TestTrue(TEXT("Later cards have not started"), FMath::IsNearlyEqual(Scale(0.15f, 5), 1.0f));
	TestEqual(TEXT("Source clip settles after twenty frames"), Scale(0.05f + 1.0f / 3.0f, 0), 1.0f);
	TestEqual(TEXT("Tab refresh settled state"), Scale(10, 0), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildCityEntranceTest,
	"Rok.Ui.Animation.BuildCity.Entrance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokBuildCityEntranceTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenBuildCity();
	UWidget* Drawer = Screen->GetWidgetFromName(TEXT("BuildCityAnimatedRoot"));
	UWidget* Mask = Screen->GetWidgetFromName(TEXT("BuildCityMaskClose"));
	if (!TestNotNull(TEXT("Source rect group"), Drawer) || !TestNotNull(TEXT("Independent close mask"), Mask)) return false;
	TestEqual(TEXT("Drawer starts invisible"), Drawer->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Drawer enters from below"), FMath::IsNearlyEqual(Drawer->GetRenderTransform().Translation.Y, 100.0));
	TestTrue(TEXT("Mask is outside animated group"), Mask->GetParent() != Drawer);
	UWidget* Background = Screen->GetWidgetFromName(TEXT("BuildCityDrawerBackground"));
	if (!TestNotNull(TEXT("Source bottom background"), Background)) return false;
	const auto* BackgroundSlot = Cast<UCanvasPanelSlot>(Background->Slot);
	if (!TestNotNull(TEXT("Background slot"), BackgroundSlot)) return false;
	TestEqual(TEXT("Source bg is a 153px bottom strip, not a full drawer"), BackgroundSlot->GetSize(), FVector2D(1462, 153));
	TestTrue(TEXT("Source bg horizontally extends seven pixels to left"), FMath::IsNearlyEqual(BackgroundSlot->GetPosition().X, -7.0));
	const UWidget* TopPatch = Screen->GetWidgetFromName(TEXT("BuildCityDrawerBackground_Top"));
	const UWidget* BottomPatch = Screen->GetWidgetFromName(TEXT("BuildCityDrawerBackground_Bottom"));
	if (!TestNotNull(TEXT("Background top slice"), TopPatch) || !TestNotNull(TEXT("Background bottom slice"), BottomPatch)) return false;
	TestEqual(TEXT("Unity w border maps to Slate top"), CastChecked<UCanvasPanelSlot>(TopPatch->Slot)->GetSize().Y, 41.0);
	TestEqual(TEXT("Unity y border maps to Slate bottom"), CastChecked<UCanvasPanelSlot>(BottomPatch->Slot)->GetSize().Y, 44.0);
	UWidget* ListMask = Screen->GetWidgetFromName(TEXT("BuildCityListMask"));
	UWidget* Scroll = Screen->GetWidgetFromName(TEXT("BuildCityBuildingScroll"));
	if (!TestNotNull(TEXT("Unity n mask"), ListMask) || !TestNotNull(TEXT("Unity v viewport"), Scroll)) return false;
	TestTrue(TEXT("Only outer n clips animation"), ListMask->GetClipping() == EWidgetClipping::ClipToBoundsAlways
		&& Scroll->GetClipping() == EWidgetClipping::Inherit);
	TestTrue(TEXT("Viewport is inside independent mask"), Scroll->GetParent() == ListMask);
	const auto* ListMaskSlot = Cast<UCanvasPanelSlot>(ListMask->Slot);
	const auto* ScrollSlot = Cast<UCanvasPanelSlot>(Scroll->Slot);
	if (!TestNotNull(TEXT("Mask canvas slot"), ListMaskSlot) || !TestNotNull(TEXT("Viewport canvas slot"), ScrollSlot)) return false;
	TestTrue(TEXT("Source mask extends upward by 241.1 without changing bottom"),
		FMath::IsNearlyEqual(ScrollSlot->GetPosition().Y, 241.1, 0.001)
		&& FMath::IsNearlyEqual(ListMaskSlot->GetSize().Y, ScrollSlot->GetSize().Y + 241.1, 0.001));
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestTrue(TEXT("Source midpoint alpha"), FMath::IsNearlyEqual(Drawer->GetRenderOpacity(), 0.5f));
	TestTrue(TEXT("Source midpoint position"), FMath::IsNearlyEqual(Drawer->GetRenderTransform().Translation.Y, 50.0));
	auto* Tab = Cast<UButton>(Screen->GetWidgetFromName(TEXT("BuildCityCategory_1")));
	if (!TestNotNull(TEXT("Real build category button"), Tab)) return false;
	if (auto* AnimatedTab = Cast<URokUiAnimatedButton>(Tab))
	{
		Tab->OnPressed.Broadcast();
		AnimatedTab->AdvancePressAnimationForTesting(0.08f);
		Tab->OnReleased.Broadcast();
		TestFalse(TEXT("BuildCity category has no source press animation"), AnimatedTab->HasActiveAnimationTimerForTesting());
		TestEqual(TEXT("Category stays full size when pressed"), Tab->GetRenderTransform().Scale, FVector2D(1));
	}
	Tab->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	Drawer = Screen->GetWidgetFromName(TEXT("BuildCityAnimatedRoot"));
	TestTrue(TEXT("Category rebuild preserves entry clock"), Drawer && FMath::IsNearlyEqual(Drawer->GetRenderOpacity(), 0.5f));
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestEqual(TEXT("Settled opacity"), Drawer->GetRenderOpacity(), 1.0f);
	Screen->CloseScreen(); Screen->OpenBuildCity();
	TestEqual(TEXT("Reopen restarts source entry"), Screen->GetWidgetFromName(TEXT("BuildCityAnimatedRoot"))->GetRenderOpacity(), 0.0f);
	Screen->CloseScreen();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCivilizationSelectionPulseTest,
	"Rok.Ui.Animation.Civilization.SelectionPulse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCivilizationSelectionPulseTest::RunTest(const FString&)
{
	using namespace RokCivilizationAnimation;
	TestEqual(TEXT("Source glow starts transparent"), EvaluateSelection(0).Alpha, 0.0f);
	TestTrue(TEXT("Source initial scale"), FMath::IsNearlyEqual(EvaluateSelection(0).Scale, 1.0038509f));
	TestTrue(TEXT("Source asymmetric fade-in tangent"), FMath::IsNearlyEqual(EvaluateSelection(0.2f).Alpha, 0.375f));
	TestEqual(TEXT("Source peak at 24 frames"), EvaluateSelection(0.4f).Alpha, 1.0f);
	TestTrue(TEXT("Glow moves up as it grows"), EvaluateSelection(2).Y < EvaluateSelection(1).Y && EvaluateSelection(2).Scale > EvaluateSelection(1).Scale);
	TestTrue(TEXT("Glow fades toward end"), EvaluateSelection(3.1f).Alpha < 0.01f);
	TestEqual(TEXT("Source clip loops"), EvaluateSelection(SelectionDuration).Alpha, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCivilizationSwitchCurvesTest,
	"Rok.Ui.Animation.Civilization.SwitchCurves", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCivilizationSwitchCurvesTest::RunTest(const FString&)
{
	using namespace RokCivilizationAnimation;
	const auto Begin = Evaluate(0, true);
	TestEqual(TEXT("Source out hides hero immediately"), Begin.HeroAlpha, 0.0f);
	TestEqual(TEXT("Flag begins opaque"), Begin.FlagAlpha, 1.0f);
	const auto Mid = Evaluate(OutDuration * 0.5f, true);
	TestEqual(TEXT("Flag out midpoint"), Mid.FlagY, -35.0f);
	TestEqual(TEXT("Flag alpha midpoint"), Mid.FlagAlpha, 0.5f);
	TestEqual(TEXT("Hero stays hidden throughout out"), Mid.HeroAlpha, 0.0f);
	const auto Join = Evaluate(OutDuration, true);
	TestEqual(TEXT("Automatic source transition starts show from invisible"), Join.FlagAlpha, 0.0f);
	TestEqual(TEXT("Hero show starting offset"), Join.HeroX, -200.0f);
	const auto Show = Evaluate(OutDuration + ShowDuration * 0.5f, true);
	TestTrue(TEXT("Both source groups fade in together"), FMath::IsNearlyEqual(Show.FlagAlpha, 0.5f) && FMath::IsNearlyEqual(Show.HeroAlpha, 0.5f));
	const auto End = Evaluate(OutDuration + ShowDuration, true);
	TestEqual(TEXT("Flag settles"), End.FlagY, 0.0f);
	TestEqual(TEXT("Hero settles"), End.HeroAlpha, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCivilizationListAnimationTest,
	"Rok.Ui.Animation.Civilization.ListEntrance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCivilizationListAnimationTest::RunTest(const FString& Parameters)
{
	auto* Modal = NewObject<URokUnityModalWidget>();
	Modal->Initialize();
	Modal->TakeWidget();
	Modal->OpenCivilizationSelection();
	auto* List = Modal->GetWidgetFromName(TEXT("CivilizationListRoot"));
	auto* Back = Modal->GetWidgetFromName(TEXT("CivilizationSelectionBackButton"));
	if (!TestNotNull(TEXT("Source country-list group"), List) || !TestNotNull(TEXT("Independent back control"), Back)) return false;
	const auto* ListSlot = CastChecked<UCanvasPanelSlot>(List->Slot);
	TestTrue(TEXT("Country list follows viewport bottom"), ListSlot->GetAnchors().Minimum.Equals(FVector2D(0.5, 1)));
	TestTrue(TEXT("Country list retains source reference offset"), ListSlot->GetPosition().Equals(FVector2D(-720, -810)));
	const auto* Shade = Modal->GetWidgetFromName(TEXT("CivilizationListShade"));
	if (!TestNotNull(TEXT("Country list backdrop"), Shade)) return false;
	const auto* ShadeSlot = CastChecked<UCanvasPanelSlot>(Shade->Slot);
	TestTrue(TEXT("List backdrop fills expanded width"), ShadeSlot->GetAnchors().Maximum.Equals(FVector2D(1, 1)));
	TestTrue(TEXT("List backdrop stays bottom anchored"), ShadeSlot->GetAnchors().Minimum.Equals(FVector2D(0, 1)));
	TestEqual(TEXT("Country list begins transparent"), List->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Unity -200 -> 1 becomes +201 translation"), FMath::IsNearlyEqual(List->GetRenderTransform().Translation.Y, 201.0));
	TestTrue(TEXT("Back is not part of animated list"), Back->GetParent() != List);
	Modal->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestTrue(TEXT("Zero-tangent midpoint alpha"), FMath::IsNearlyEqual(List->GetRenderOpacity(), 0.5f));
	TestTrue(TEXT("Midpoint translation"), FMath::IsNearlyEqual(List->GetRenderTransform().Translation.Y, 100.5f));
	TestTrue(TEXT("List has no invented scale animation"), List->GetRenderTransform().Scale.Equals(FVector2D(1)));
	Modal->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestEqual(TEXT("List is fully visible at twenty frames"), List->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("List settles at twenty frames"), List->GetRenderTransform().Translation.IsNearlyZero());
	Modal->AdvanceWindowAnimationForTesting(0.5f);
	TestEqual(TEXT("Source hold keeps opacity"), List->GetRenderOpacity(), 1.0f);
	Modal->CloseModal();
	Modal->OpenCivilizationSelection();
	List = Modal->GetWidgetFromName(TEXT("CivilizationListRoot"));
	TestEqual(TEXT("Fresh entry restarts list animation"), List->GetRenderOpacity(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokUiWindowAnimationCurvesTest,
	"Rok.Ui.Animation.WindowSourceCurves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiWindowAnimationCurvesTest::RunTest(const FString& Parameters)
{
	using EStyle = ERokUiWindowAnimationStyle;
	TestEqual(TEXT("Building title opens from zero"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, false, 0).Scale, 0.0f);
	TestEqual(TEXT("Building menu fades in"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, false, 0).Opacity, 0.0f);
	TestEqual(TEXT("Building title opening peak is 1.2 at seven frames"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, false, 0.11666667f).Scale, 1.2f);
	TestEqual(TEXT("Root is opaque by opening peak"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, false, 0.11666667f).Opacity, 1.0f);
	TestEqual(TEXT("Building title closes via 1.2 at five frames"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, true, 0.083333336f).Scale, 1.2f);
	TestEqual(TEXT("Root stays opaque until closing peak"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, true, 0.083333336f).Opacity, 1.0f);
	TestEqual(TEXT("Building close ends after fifteen frames"), FRokUiWindowAnimation::GetDuration(EStyle::BuildingMenu, true), 0.25f);
	TestEqual(TEXT("Building menu fully fades out"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, true, 0.25f).Opacity, 0.0f);
	TestEqual(TEXT("Building title collapses"), FRokUiWindowAnimation::Evaluate(EStyle::BuildingMenu, true, 0.25f).Scale, 0.0f);
	TestEqual(TEXT("T1 starts at source 1.05"),
		FRokUiWindowAnimation::Evaluate(EStyle::Type1, false, 0.0f).Scale, 1.05f);
	TestEqual(TEXT("T1 overshoots at frame 7 / 60"),
		FRokUiWindowAnimation::Evaluate(EStyle::Type1, false, 0.11666667f).Scale, 1.1f);
	TestEqual(TEXT("T1 opens without a fabricated fade"),
		FRokUiWindowAnimation::Evaluate(EStyle::Type1, false, 0.0f).Opacity, 1.0f);
	TestEqual(TEXT("IF begins invisible"),
		FRokUiWindowAnimation::Evaluate(EStyle::Interface, false, 0.0f).Opacity, 0.0f);
	TestEqual(TEXT("IF zero-tangent midpoint alpha"),
		FRokUiWindowAnimation::Evaluate(EStyle::Interface, false, 0.125f).Opacity, 0.5f);
	TestTrue(TEXT("Zero tangents use cubic interpolation, not linear"),
		FMath::IsNearlyEqual(FRokUiWindowAnimation::Evaluate(
			EStyle::Interface, false, 0.0625f).Opacity, 0.15625f));
	TestEqual(TEXT("Pop starts at zero size"),
		FRokUiWindowAnimation::Evaluate(EStyle::Pop, false, 0.0f).Scale, 0.0f);
	TestEqual(TEXT("Pop keeps alpha at one on exit"),
		FRokUiWindowAnimation::Evaluate(EStyle::Pop, true, 0.125f).Opacity, 1.0f);
	TestEqual(TEXT("T1 source close event is 0.15 seconds"),
		FRokUiWindowAnimation::GetDuration(EStyle::Type1, true), 0.15f);
	TestEqual(TEXT("Pop source close duration is 0.25 seconds"),
		FRokUiWindowAnimation::GetDuration(EStyle::Pop, true), 0.25f);
	TestEqual(TEXT("World object has its own ten-frame close"),
		FRokUiWindowAnimation::GetDuration(EStyle::WorldObject, true), 0.16666667f);
	TestEqual(TEXT("World object starts at zero"),
		FRokUiWindowAnimation::Evaluate(EStyle::WorldObject, false, 0).Scale, 0.0f);
	TestEqual(TEXT("World object opening source overshoot"),
		FRokUiWindowAnimation::Evaluate(EStyle::WorldObject, false, 0.11666667f).Scale, 1.1f);
	TestTrue(TEXT("World object closes to half scale at five frames"),
		FMath::IsNearlyEqual(FRokUiWindowAnimation::Evaluate(EStyle::WorldObject, true, 0.083333335f).Scale, 0.5f));
	TestEqual(TEXT("World object closing has no invented fade"),
		FRokUiWindowAnimation::Evaluate(EStyle::WorldObject, true, 0.1f).Opacity, 1.0f);
	FRokUiWindowAnimation ObjectAnimation;
	ObjectAnimation.Open(EStyle::WorldObject);
	ObjectAnimation.Advance(0.06f);
	const float OpeningScale = ObjectAnimation.GetSample().Scale;
	ObjectAnimation.Close();
	ObjectAnimation.Advance(0);
	TestEqual(TEXT("Interrupted opening does not jump on close"), ObjectAnimation.GetSample().Scale, OpeningScale);
	ObjectAnimation.Advance(0.04f);
	const float ClosingScale = ObjectAnimation.GetSample().Scale;
	ObjectAnimation.Open(EStyle::WorldObject);
	TestEqual(TEXT("Rapid reselection preserves current scale"), ObjectAnimation.GetSample().Scale, ClosingScale);
	ObjectAnimation.Advance(0.25f);
	TestEqual(TEXT("Reselection settles at full size"), ObjectAnimation.GetSample().Scale, 1.0f);
	ObjectAnimation.Close();
	TestTrue(TEXT("World object close completes at its source end"), ObjectAnimation.Advance(0.16666667f));

	FRokUiWindowAnimation Animation;
	Animation.Open(EStyle::Type1);
	TestFalse(TEXT("Opening completion is not a close event"), Animation.Advance(1.0f));
	TestFalse(TEXT("Open animation settles"), Animation.IsAnimating());
	TestEqual(TEXT("Final size is exactly one"), Animation.GetSample().Scale, 1.0f);
	Animation.Close();
	TestFalse(TEXT("Close remains pending before source callback"), Animation.Advance(0.075f));
	const FRokUiWindowAnimationSample HalfClosed = Animation.GetSample();
	Animation.Open(EStyle::Type1);
	TestEqual(TEXT("Reopen keeps interrupted size"), Animation.GetSample().Scale, HalfClosed.Scale);
	TestEqual(TEXT("Reopen keeps interrupted alpha"), Animation.GetSample().Opacity, HalfClosed.Opacity);
	TestFalse(TEXT("Interrupted close cannot close a reopened window"), Animation.Advance(0.25f));
	TestEqual(TEXT("Reopened window restores alpha"), Animation.GetSample().Opacity, 1.0f);
	Animation.Close();
	TestTrue(TEXT("Close callback fires at completion"), Animation.Advance(0.15f));
	TestFalse(TEXT("Close callback only fires once"), Animation.Advance(0.15f));
	Animation.Reset();
	TestFalse(TEXT("Reset cancels any transition"), Animation.IsAnimating());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokUiWindowAnimationModalLifecycleTest,
	"Rok.Ui.Animation.ModalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiWindowAnimationModalLifecycleTest::RunTest(const FString& Parameters)
{
	URokUnityModalWidget* Modal = NewObject<URokUnityModalWidget>(GetTransientPackage());
	Modal->Initialize();
	const TSharedRef<SWidget> SlateWidget = Modal->TakeWidget();
	Modal->OpenPlayerProfile();
	UWidget* Content = Modal->GetWidgetFromName(TEXT("ModalModeCanvas"));
	UWidget* Mask = Modal->GetWidgetFromName(TEXT("ModalDismissMask"));
	if (!TestNotNull(TEXT("Modal content exists"), Content)
		|| !TestNotNull(TEXT("Modal input mask exists"), Mask))
	{
		return false;
	}
	TestEqual(TEXT("Opening content blocks descendant hits"),
		Content->GetVisibility(), ESlateVisibility::HitTestInvisible);
	UWidget* AnimationBlocker = Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker"));
	if (!TestNotNull(TEXT("Transition has a non-dismissing click blocker"), AnimationBlocker))
	{
		return false;
	}
	TestEqual(TEXT("Opening clicks cannot reach the dismiss mask"),
		AnimationBlocker->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("Input mask never scales with window"), Mask->GetRenderTransform().Scale, FVector2D(1.0f));
	TestEqual(TEXT("Input mask never fades with window"), Mask->GetRenderOpacity(), 1.0f);
	Modal->AdvanceWindowAnimationForTesting(0.25f);
	TestEqual(TEXT("Settled content accepts button hits"),
		Content->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Transition blocker releases after entry"),
		AnimationBlocker->GetVisibility(), ESlateVisibility::Collapsed);
	Modal->RequestCloseModal();
	TestTrue(TEXT("Closing modal still owns input"), Modal->IsModalOpen());
	TestTrue(TEXT("Root stays visible through exit"), Modal->IsVisible());
	Modal->AdvanceWindowAnimationForTesting(0.075f);
	Modal->OpenPlayerProfile();
	Modal->AdvanceWindowAnimationForTesting(0.25f);
	TestTrue(TEXT("Reopen cancels pending dismissal"), Modal->IsModalOpen());
	Modal->RequestCloseModal();
	Modal->AdvanceWindowAnimationForTesting(0.15f);
	TestFalse(TEXT("Modal closes only after callback time"), Modal->IsModalOpen());
	TestEqual(TEXT("Modal collapses after exit"), Modal->GetVisibility(), ESlateVisibility::Collapsed);

	Modal->OpenResourceInventory();
	Modal->AdvanceWindowAnimationForTesting(0.1f);
	const float Elapsed = Modal->GetWindowAnimationForTesting().GetElapsedSeconds();
	if (UButton* Tab = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ResourceTab_1"))))
	{
		Tab->OnClicked.Broadcast();
		TestEqual(TEXT("Resource tab rebuild does not restart entry"),
			Modal->GetWindowAnimationForTesting().GetElapsedSeconds(), Elapsed);
	}
	else
	{
		AddError(TEXT("Resource tab for lifecycle test is missing"));
	}
	Modal->CloseModal();
	TestFalse(TEXT("Routing CloseModal remains synchronous"), Modal->IsModalOpen());
	Modal->AdvanceWindowAnimationForTesting(1.0f);
	TestFalse(TEXT("Routing close cancels old animation"), Modal->IsModalOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokUiWindowAnimationFeatureLifecycleTest,
	"Rok.Ui.Animation.FeatureLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiWindowAnimationFeatureLifecycleTest::RunTest(const FString& Parameters)
{
	URokUnityFeatureScreenWidget* Feature = NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	Feature->Initialize();
	const TSharedRef<SWidget> SlateWidget = Feature->TakeWidget();
	Feature->OpenItems();
	UWidget* Content = Feature->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	UButton* Mask = Cast<UButton>(Feature->GetWidgetFromName(TEXT("FeatureScreenInputBlocker")));
	if (!TestNotNull(TEXT("Feature content exists"), Content)
		|| !TestNotNull(TEXT("Feature input blocker exists"), Mask))
	{
		return false;
	}
	Feature->AdvanceWindowAnimationForTesting(0.25f);
	UButton* Close = Cast<UButton>(Feature->GetWidgetFromName(TEXT("FeatureFrameClose")));
	if (!TestNotNull(TEXT("Actual feature close button exists"), Close))
	{
		return false;
	}
	Close->OnClicked.Broadcast();
	TestFalse(TEXT("Button dispatch remains deferred until next action flush"),
		Feature->GetWindowAnimationForTesting().IsClosing());
	Feature->FlushPendingActionForTesting();
	TestTrue(TEXT("Deferred user dismissal starts exit"), Feature->GetWindowAnimationForTesting().IsClosing());
	TestTrue(TEXT("Exit does not release feature input owner early"), Feature->IsScreenOpen());
	TestEqual(TEXT("Closing content cannot receive hits"), Content->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Backdrop blocker remains visible"), Mask->GetVisibility(), ESlateVisibility::Visible);
	Feature->AdvanceWindowAnimationForTesting(0.1f);
	TestTrue(TEXT("Feature remains open before callback"), Feature->IsScreenOpen());
	Feature->AdvanceWindowAnimationForTesting(0.05f);
	TestFalse(TEXT("Feature closes at source callback"), Feature->IsScreenOpen());
	Feature->OpenItems();
	Feature->RequestCloseScreen();
	Feature->OpenItems();
	Feature->AdvanceWindowAnimationForTesting(0.25f);
	TestTrue(TEXT("Same-mode reopen cancels closing"), Feature->IsScreenOpen());
	Feature->CloseScreen();
	TestFalse(TEXT("Routing CloseScreen remains synchronous"), Feature->IsScreenOpen());
	return true;
}

#endif
