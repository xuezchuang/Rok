#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCommanderDataAnimation.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Button.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderDataAnimationTest,
	"Rok.Ui.Animation.Commander.DataPanel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderDataAnimationTest::RunTest(const FString&)
{
	const auto Held = RokCommanderDataAnimation::Evaluate(5.0f / 60.0f);
	TestEqual(TEXT("Source holds data panel to the right for five frames"), Held.X, 200.0f);
	TestEqual(TEXT("Source holds data panel invisible for five frames"), Held.Opacity, 0.0f);
	const auto Mid = RokCommanderDataAnimation::Evaluate(0.2f);
	TestTrue(TEXT("Source zero-tangent midpoint"), FMath::IsNearlyEqual(Mid.X, 100.0f, 0.001f));
	TestTrue(TEXT("Source alpha midpoint"), FMath::IsNearlyEqual(Mid.Opacity, 0.55f, 0.001f));
	TestEqual(TEXT("CanvasGroup clamps overshoot"), RokCommanderDataAnimation::Evaluate(19.0f / 60.0f).Opacity, 1.0f);
	TestEqual(TEXT("Long frames settle at source rest position"), RokCommanderDataAnimation::Evaluate(10.0f).X, 0.0f);

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenCommander();
	auto* Name = Screen->GetWidgetFromName(TEXT("CommanderName"));
	auto* Power = Screen->GetWidgetFromName(TEXT("CommanderPower"));
	if (!TestNotNull(TEXT("Real commander data panel child"), Name)
		|| !TestNotNull(TEXT("Independent portrait power label"), Power)) return false;
	TestEqual(TEXT("Runtime data panel starts invisible"), Name->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Runtime data panel starts at source X"), FMath::IsNearlyEqual(Name->GetRenderTransform().Translation.X, 200.0));
	TestEqual(TEXT("Portrait label is outside data animation subtree"), Power->GetRenderOpacity(), 1.0f);
	Screen->AdvanceCommanderRosterAnimationForTesting(0.2f);
	TestTrue(TEXT("Runtime data panel reaches source midpoint"), FMath::IsNearlyEqual(Name->GetRenderOpacity(), 0.55f, 0.001f));
	auto* Sort = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSortAction")));
	if (!TestNotNull(TEXT("Runtime sort action"), Sort)) return false;
	Sort->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	Name = Screen->GetWidgetFromName(TEXT("CommanderName"));
	if (!TestNotNull(TEXT("Rebuilt data panel"), Name)) return false;
	TestTrue(TEXT("Sort rebuild preserves animation progress"), FMath::IsNearlyEqual(Name->GetRenderOpacity(), 0.55f, 0.001f));
	Screen->AdvanceCommanderRosterAnimationForTesting(1.0f);
	TestEqual(TEXT("Runtime data panel settles"), Name->GetRenderOpacity(), 1.0f);
	Screen->OpenItems(); Screen->OpenCommander();
	Name = Screen->GetWidgetFromName(TEXT("CommanderName"));
	if (!TestNotNull(TEXT("Reopened data panel"), Name)) return false;
	TestEqual(TEXT("Reopening starts a fresh source entry"), Name->GetRenderOpacity(), 0.0f);
	Screen->CloseScreen();
	return true;
}
#endif
