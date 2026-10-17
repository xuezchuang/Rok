#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokArmyShowPanelAnimation.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Button.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokArmyShowPanelAnimationTest,
	"Rok.Ui.Animation.ArmyShow.PanelNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokArmyShowPanelAnimationTest::RunTest(const FString&)
{
	const auto DescriptionMid = RokArmyShowPanelAnimation::Evaluate(0.125f, true);
	const auto ListMid = RokArmyShowPanelAnimation::Evaluate(0.125f, false);
	TestEqual(TEXT("Description enters from source right"), DescriptionMid.X, 25.0f);
	TestEqual(TEXT("List enters from source left"), ListMid.X, -25.0f);
	TestEqual(TEXT("Source alpha midpoint"), DescriptionMid.Opacity, 0.5f);
	TestEqual(TEXT("Negative delta cannot reverse a fresh entry"), RokArmyShowPanelAnimation::Evaluate(-1.0f, true).Opacity, 0.0f);

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenArmyShow();
	auto* List = Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"));
	if (!TestNotNull(TEXT("Real army list"), List)) return false;
	TestEqual(TEXT("Source first open skips child Show"), List->GetRenderOpacity(), 1.0f);
	Screen->AdvanceWindowAnimationForTesting(0.3f);
	auto Click = [this, Screen](const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
		return true;
	};
	if (!Click(TEXT("ArmyShowRulesButton"))) return false;
	auto* Rules = Screen->GetWidgetFromName(TEXT("ArmyShowRulesScroll"));
	auto* Back = Screen->GetWidgetFromName(TEXT("ArmyShowRulesBackButton"));
	if (!TestNotNull(TEXT("Real rule description"), Rules) || !TestNotNull(TEXT("Back stays outside animated child"), Back)) return false;
	TestEqual(TEXT("Description starts transparent"), Rules->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Description source start X"), FMath::IsNearlyEqual(Rules->GetRenderTransform().Translation.X, 50.0));
	TestEqual(TEXT("Back is not faded with description"), Back->GetRenderOpacity(), 1.0f);
	TestFalse(TEXT("Navigation does not restart full window entry"), Screen->GetWindowAnimationForTesting().IsAnimating());
	Screen->AdvanceArmyShowPanelAnimationForTesting(0.125f);
	TestEqual(TEXT("Description runtime midpoint"), Rules->GetRenderOpacity(), 0.5f);
	// Army data networking is still absent. Exercise the real shared rebuild
	// path without inventing a server payload or reporting a network refresh.
	Screen->RebuildArmyShowPresentationForTesting();
	Rules = Screen->GetWidgetFromName(TEXT("ArmyShowRulesScroll"));
	if (!TestNotNull(TEXT("Refreshed rule description"), Rules)) return false;
	TestEqual(TEXT("Same-mode rebuild preserves description time"), Rules->GetRenderOpacity(), 0.5f);
	TestTrue(TEXT("Same-mode rebuild preserves description position"), FMath::IsNearlyEqual(Rules->GetRenderTransform().Translation.X, 25.0));
	if (!Click(TEXT("ArmyShowRulesBackButton"))) return false;
	List = Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"));
	auto* RuleBar = Screen->GetWidgetFromName(TEXT("ArmyShowRuleBar"));
	if (!TestNotNull(TEXT("Restored list"), List) || !TestNotNull(TEXT("Restored description link bar"), RuleBar)) return false;
	TestEqual(TEXT("Returning plays ContentShow"), List->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Returning starts left of rest"), FMath::IsNearlyEqual(List->GetRenderTransform().Translation.X, -50.0));
	Screen->AdvanceArmyShowPanelAnimationForTesting(0.125f);
	TestEqual(TEXT("Both source Content animators share progress"), RuleBar->GetRenderOpacity(), List->GetRenderOpacity());
	Screen->RebuildArmyShowPresentationForTesting();
	List = Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"));
	if (!TestNotNull(TEXT("Refreshed list"), List)) return false;
	TestEqual(TEXT("Same-mode rebuild preserves list time"), List->GetRenderOpacity(), 0.5f);
	// Each navigation activates a different source Animator and calls Show.
	// Reverse immediately during entry: restart target clip, do not blend the
	// previous panel's opacity/translation into it.
	if (!Click(TEXT("ArmyShowRulesButton"))) return false;
	Rules = Screen->GetWidgetFromName(TEXT("ArmyShowRulesScroll"));
	if (!TestNotNull(TEXT("Rapid reverse description"), Rules)) return false;
	TestEqual(TEXT("Reverse replays description from frame zero"), Rules->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Reverse uses description source start position"), FMath::IsNearlyEqual(Rules->GetRenderTransform().Translation.X, 50.0));
	if (!Click(TEXT("ArmyShowRulesBackButton"))) return false;
	List = Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"));
	if (!TestNotNull(TEXT("Immediate reverse list"), List)) return false;
	TestEqual(TEXT("Immediate reverse replays list from frame zero"), List->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Immediate reverse uses list source start position"), FMath::IsNearlyEqual(List->GetRenderTransform().Translation.X, -50.0));
	Screen->AdvanceArmyShowPanelAnimationForTesting(1.0f);
	TestEqual(TEXT("Long frame settles list"), List->GetRenderOpacity(), 1.0f);
	Screen->OpenItems(); Screen->OpenArmyShow();
	List = Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"));
	if (!TestNotNull(TEXT("Reopened list"), List)) return false;
	TestEqual(TEXT("New screen first open again skips child Show"), List->GetRenderOpacity(), 1.0f);
	Screen->CloseScreen();
	return true;
}
#endif
