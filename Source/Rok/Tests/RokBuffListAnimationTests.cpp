#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/CanvasPanel.h"
#include "../RokUnityModalWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuffListChildAnimationTest,
	"Rok.Ui.Animation.BuffList.ChildPopLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokBuffListChildAnimationTest::RunTest(const FString&)
{
	auto* Modal = NewObject<URokUnityModalWidget>();
	Modal->Initialize();
	Modal->TakeWidget();
	Modal->OpenBuffList();
	UWidget* Popup = Modal->GetWidgetFromName(TEXT("BuffListAnimationRoot"));
	UWidget* Root = Modal->GetWidgetFromName(TEXT("ModalModeCanvas"));
	UWidget* Blocker = Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker"));
	if (!TestNotNull(TEXT("pl_pos equivalent exists"), Popup)
		|| !TestNotNull(TEXT("Unanimated modal root"), Root)
		|| !TestNotNull(TEXT("Independent input blocker"), Blocker)) return false;
	TestEqual(TEXT("Source popup starts at zero XY scale"), Popup->GetRenderTransform().Scale, FVector2D(0));
	TestEqual(TEXT("Full-screen root stays at unit scale"), Root->GetRenderTransform().Scale, FVector2D(1));
	TestEqual(TEXT("Pop has no alpha track"), Popup->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("Input blocker outside animated parent"), Blocker->GetParent() != Popup);
	TestTrue(TEXT("Source top edge pivot"), FMath::IsNearlyZero(Popup->GetRenderTransformPivot().Y));
	TestTrue(TEXT("Source background pivot compensates 3.3 offset"),
		FMath::IsNearlyEqual(Popup->GetRenderTransformPivot().X, 0.9 - 3.3 / 450.3, 0.00001));
	Modal->AdvanceWindowAnimationForTesting(0.11666667f);
	// Source samples are float; UMG's FVector2D promotes the sample to double.
	// Comparing with a double literal at 1e-8 rejects float32's exact 1.1 value.
	TestEqual(TEXT("Frame seven source sample reaches 1.1"),
		Modal->GetWindowAnimationForTesting().GetSample().Scale, 1.1f);
	TestEqual(TEXT("Frame seven widget receives the exact source sample"),
		Popup->GetRenderTransform().Scale.X, static_cast<double>(1.1f));
	Modal->AdvanceWindowAnimationForTesting(0.25f);
	TestEqual(TEXT("Settled popup scale"), Popup->GetRenderTransform().Scale, FVector2D(1));
	TestEqual(TEXT("Input released after entry"), Blocker->GetVisibility(), ESlateVisibility::Collapsed);
	Modal->RequestCloseModal();
	TestFalse(TEXT("UIManager removes child-Animator popup immediately"), Modal->IsModalOpen());
	TestEqual(TEXT("Closed modal collapsed"), Modal->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Closed popup children removed"), CastChecked<UCanvasPanel>(Root)->GetChildrenCount(), 0);
	Modal->OpenBuffList();
	Popup = Modal->GetWidgetFromName(TEXT("BuffListAnimationRoot"));
	TestEqual(TEXT("Reopen starts fresh default Open"), Popup->GetRenderTransform().Scale, FVector2D(0));
	Modal->AdvanceWindowAnimationForTesting(0.05f);
	Modal->RequestCloseModal();
	TestFalse(TEXT("Dismiss during entry is also immediate"), Modal->IsModalOpen());
	Modal->OpenBuffList();
	Modal->OpenPlayerProfile();
	TestTrue(TEXT("Another screen uses its own Type1 animation"),
		Modal->GetWindowAnimationForTesting().GetStyle() == ERokUiWindowAnimationStyle::Type1);
	TestTrue(TEXT("New screen root does not retain popup's zero scale"), Root->GetRenderTransform().Scale.X > 1.0);
	Modal->CloseModal();
	return true;
}

#endif
