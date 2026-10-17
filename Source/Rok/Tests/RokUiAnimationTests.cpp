#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Widgets/SWidget.h"
#include "../RokUiAnimatedButton.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokUiButtonSourceCurvesTest,
	"Rok.Ui.Animation.Button.SourceCurves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiButtonSourceCurvesTest::RunTest(const FString& Parameters)
{
	const auto CheckScale = [this](
		const TCHAR* Label, const FVector2D& Actual, const FVector2D& Expected)
	{
		TestTrue(Label, Actual.Equals(Expected, 0.00001f));
	};
	CheckScale(TEXT("Down starts at one"),
		URokUiAnimatedButton::EvaluatePressScale(0.0f), FVector2D(1.0f));
	CheckScale(TEXT("Down uses the zero-tangent Hermite midpoint"),
		URokUiAnimatedButton::EvaluatePressScale(0.04166667f), FVector2D(0.95f));
	CheckScale(TEXT("Down quarter point is smoothstep, not linear"),
		URokUiAnimatedButton::EvaluatePressScale(0.020833335f), FVector2D(0.984375f));
	CheckScale(TEXT("Down reaches .9 at the source key"),
		URokUiAnimatedButton::EvaluatePressScale(0.08333334f), FVector2D(0.9f));
	CheckScale(TEXT("Down clamps after the source key"),
		URokUiAnimatedButton::EvaluatePressScale(4.0f), FVector2D(0.9f));
	CheckScale(TEXT("Up begins at .9"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.0f), FVector2D(0.9f));
	CheckScale(TEXT("Up first midpoint preserves asymmetric XY"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.04166667f), FVector2D(0.98f, 0.965f));
	CheckScale(TEXT("Up reaches source overshoot"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.08333334f), FVector2D(1.06f, 1.03f));
	CheckScale(TEXT("Up reaches source undershoot"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.1833333f), FVector2D(0.97f, 0.95f));
	CheckScale(TEXT("Up returns to one"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.2833333f), FVector2D(1.0f));
	CheckScale(TEXT("Up clamps at rest"),
		URokUiAnimatedButton::EvaluateReleaseScale(3.0f), FVector2D(1.0f));
	CheckScale(TEXT("Interrupted release begins at the rendered scale"),
		URokUiAnimatedButton::EvaluateReleaseScale(0.0f, FVector2D(0.95f)), FVector2D(0.95f));
	CheckScale(TEXT("Interrupted press begins at the rendered scale"),
		URokUiAnimatedButton::EvaluatePressScale(0.0f, FVector2D(1.02f, 1.01f)), FVector2D(1.02f, 1.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokUiButtonLifecycleTest,
	"Rok.Ui.Animation.Button.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiButtonLifecycleTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Parent = NewObject<UCanvasPanel>();
	URokUiAnimatedButton* Button = NewObject<URokUiAnimatedButton>();
	UImage* Visual = NewObject<UImage>();
	Visual->SetRenderScale(FVector2D(-2.0f, 3.0f));
	Visual->SetRenderTransformPivot(FVector2D(0.25f, 0.75f));
	Visual->SetRenderTranslation(FVector2D(17.0f, 23.0f));
	Button->SetAnimationTarget(Visual);
	// Model the real transparent-button route: both the visual and hit target
	// must exist in one retained Slate tree. NewObject alone is not visible.
	Parent->AddChildToCanvas(Visual);
	Parent->AddChildToCanvas(Button);
	const TSharedRef<SWidget> ParentSlate = Parent->TakeWidget();
	const TSharedRef<SWidget> ButtonSlate = Button->TakeWidget();
	TestTrue(TEXT("Lifecycle owns a live parent Slate tree"), Parent->GetCachedWidget().IsValid());
	TestTrue(TEXT("External visual has a live Slate widget"), Visual->GetCachedWidget().IsValid());
	TestTrue(TEXT("External visual is actually visible"), Visual->IsVisible());
	TestTrue(TEXT("Hit target is actually visible"), Button->IsVisible());
	TestFalse(TEXT("No timer runs at rest"), Button->HasActiveAnimationTimerForTesting());
	TestFalse(TEXT("Animation does not add a click route"), Button->OnClicked.IsBound());

	Button->OnPressed.Broadcast();
	TestTrue(TEXT("Press starts an active timer"), Button->HasActiveAnimationTimerForTesting());
	Button->AdvancePressAnimationForTesting(0.04166667f);
	TestTrue(TEXT("External visual retains signed baseline scale"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-1.9f, 2.85f), 0.00001f));
	TestTrue(TEXT("Transparent hit target is not scaled"),
		Button->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	TestTrue(TEXT("Animated pivot is centered"),
		Visual->GetRenderTransformPivot().Equals(FVector2D(0.5f)));

	Button->OnReleased.Broadcast();
	Button->AdvancePressAnimationForTesting(0.0f);
	TestTrue(TEXT("Early release has no .9 discontinuity"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-1.9f, 2.85f), 0.00001f));
	Button->AdvancePressAnimationForTesting(0.08333334f);
	TestTrue(TEXT("Release overshoot uses original baseline"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-2.12f, 3.09f), 0.00001f));
	Button->OnPressed.Broadcast();
	Button->AdvancePressAnimationForTesting(0.0f);
	TestTrue(TEXT("Repress preserves current overshoot"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-2.12f, 3.09f), 0.00001f));
	Button->AdvancePressAnimationForTesting(0.08333334f);
	TestFalse(TEXT("Completed hold has no active timer"), Button->HasActiveAnimationTimerForTesting());
	TestTrue(TEXT("Held press settles at .9 of signed baseline"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-1.8f, 2.7f), 0.00001f));
	Button->OnReleased.Broadcast();
	Button->AdvancePressAnimationForTesting(0.2833333f);
	TestFalse(TEXT("Completed release stops the timer"), Button->HasActiveAnimationTimerForTesting());
	TestTrue(TEXT("Release restores exact authored scale"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-2.0f, 3.0f)));
	TestTrue(TEXT("Release restores authored pivot"),
		Visual->GetRenderTransformPivot().Equals(FVector2D(0.25f, 0.75f)));
	TestTrue(TEXT("Translation is not replaced by animation"),
		Visual->GetRenderTransform().Translation.Equals(FVector2D(17.0f, 23.0f)));

	Button->OnPressed.Broadcast();
	Button->AdvancePressAnimationForTesting(0.04f);
	Button->ReleaseSlateResources(true);
	TestFalse(TEXT("Slate release cancels the timer"), Button->HasActiveAnimationTimerForTesting());
	TestFalse(TEXT("Old Slate button has no lingering timer"), ButtonSlate->HasActiveTimers());
	TestTrue(TEXT("Slate release restores the external visual"),
		Visual->GetRenderTransform().Scale.Equals(FVector2D(-2.0f, 3.0f)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokUiButtonVisibilityTest,
	"Rok.Ui.Animation.Button.Visibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiButtonVisibilityTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Parent = NewObject<UCanvasPanel>();
	URokUiAnimatedButton* Button = NewObject<URokUiAnimatedButton>();
	Parent->AddChildToCanvas(Button);
	const TSharedRef<SWidget> ParentSlate = Parent->TakeWidget();
	TestTrue(TEXT("Visibility test owns a live Slate tree"), Parent->GetCachedWidget().IsValid());
	TestTrue(TEXT("Visibility test button is in the live tree"), Button->IsVisible());
	Button->OnPressed.Broadcast();
	TestTrue(TEXT("Visible child starts animation before hiding"), Button->HasActiveAnimationTimerForTesting());
	Button->AdvancePressAnimationForTesting(0.04f);
	TestTrue(TEXT("Visible child scale moves before hiding"),
		Button->GetRenderTransform().Scale.X < 1.0f);
	Parent->SetVisibility(ESlateVisibility::Collapsed);
	Button->AdvancePressAnimationForTesting(0.01f);
	TestFalse(TEXT("Collapsed parent cancels its child motion"), Button->HasActiveAnimationTimerForTesting());
	TestTrue(TEXT("Collapsed parent does not retain a pressed child scale"),
		Button->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	Parent->SetVisibility(ESlateVisibility::Visible);
	Button->SetIsEnabled(false);
	Button->OnPressed.Broadcast();
	TestFalse(TEXT("Disabled buttons do not animate"), Button->HasActiveAnimationTimerForTesting());
	Button->SetIsEnabled(true);
	Button->OnPressed.Broadcast();
	Button->AdvancePressAnimationForTesting(0.08333334f);
	URokUiAnimatedButton::ResetAnimationsInHierarchy(Parent);
	TestTrue(TEXT("Owner can reset a completed hold before hiding"),
		Button->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	TestFalse(TEXT("Owner reset does not keep Slate awake"), Button->HasActiveAnimationTimerForTesting());
	Button->OnPressed.Broadcast();
	Button->AdvancePressAnimationForTesting(0.04f);
	URokUiAnimatedButton::ResetAnimationsInHierarchy(Parent);
	TestFalse(TEXT("Hierarchy reset cancels a timer without a future paint"),
		Button->HasActiveAnimationTimerForTesting());
	TestTrue(TEXT("Hierarchy reset restores an in-flight press"),
		Button->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	Button->OnPressed.Broadcast();
	Button->AdvancePressAnimationForTesting(0.04f);
	Button->SetPressAnimationEnabled(false);
	TestTrue(TEXT("Opt-out restores an in-flight press"), Button->GetRenderTransform().Scale.Equals(FVector2D(1)));
	TestFalse(TEXT("Opt-out cancels active timer"), Button->HasActiveAnimationTimerForTesting());
	Button->OnPressed.Broadcast(); Button->OnReleased.Broadcast();
	Button->AdvancePressAnimationForTesting(0.1f);
	TestFalse(TEXT("Source static buttons never start a press/release timer"), Button->HasActiveAnimationTimerForTesting());
	TestTrue(TEXT("Static buttons retain identity scale"), Button->GetRenderTransform().Scale.Equals(FVector2D(1)));
	Button->SetPressAnimationEnabled(true);
	Button->OnPressed.Broadcast();
	TestTrue(TEXT("Re-enabling restores animated button behavior"), Button->HasActiveAnimationTimerForTesting());
	Button->SetAnimationTarget(nullptr);
	Button->ReleaseSlateResources(true);
	return true;
}

#endif
