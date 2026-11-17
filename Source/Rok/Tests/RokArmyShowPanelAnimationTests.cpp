#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokArmyShowPanelAnimation.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Button.h"
#include "../RokServerArmySubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"

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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokArmyShowSnapshotPresentationTest,
	"Rok.Ui.ArmyShow.SnapshotPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokArmyShowSnapshotPresentationTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokArmyShowSnapshotPresentationTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Army = Instance->GetSubsystem<URokServerArmySubsystem>();
	if (!TestNotNull(TEXT("Army subsystem is available to the screen"), Army))
	{
		return false;
	}

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize();
	Screen->TakeWidget();
	ON_SCOPE_EXIT { Screen->CloseScreen(); };

	FRokServerArmySnapshot Snapshot;
	Army->SetSnapshotForTesting(Snapshot);
	Screen->OpenArmyShow();
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	const auto ReadScroll = [Screen] { return Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("ArmyShowListScroll"))); };
	const auto ReadText = [Screen](const TCHAR* Name)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Text ? Text->GetText().ToString() : FString();
	};
	const auto* InitialList = ReadScroll();
	if (!TestNotNull(TEXT("Normal route has a list"), InitialList))
	{
		return false;
	}
	TestEqual(TEXT("Unavailable data does not fabricate rows"), InitialList->GetChildrenCount(), 0);

	Snapshot.bAvailable = true;
	Snapshot.Signature = TEXT("waiting");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestEqual(TEXT("Loading data shows the synchronization notice"), ReadText(TEXT("ArmyShowServerEmpty")),
		FString(TEXT("等待服务器同步行军部队")));

	Snapshot.bLoaded = Snapshot.bArmiesKnown = true;
	Snapshot.Signature = TEXT("empty");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestEqual(TEXT("A known empty list gets the empty-state notice"), ReadText(TEXT("ArmyShowServerEmpty")),
		FString(TEXT("暂无行军部队")));

	Snapshot.Signature = TEXT("marches");
	FRokServerMarch& March = Snapshot.Marches.AddDefaulted_GetRef();
	March.ArmyIndex = 91;
	March.Phase = ERokServerMarchPhase::Marching;
	March.bSoldiersKnown = March.bTargetKnown = March.bTimeKnown = true;
	March.Count = 640;
	March.TargetName = TEXT("本地行军目的地");
	March.RemainingSeconds = 78.0;
	FRokServerMarch& Returning = Snapshot.Marches.AddDefaulted_GetRef();
	Returning.ArmyIndex = 93;
	Returning.Phase = ERokServerMarchPhase::Returning;
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	const auto* ActiveList = ReadScroll();
	if (!TestNotNull(TEXT("Marches appear in the real scroll box"), ActiveList))
	{
		return false;
	}
	TestEqual(TEXT("Rendered rows track server snapshot cardinality"), ActiveList->GetChildrenCount(), 2);
	TestEqual(TEXT("March phase and count are formatted from data"), ReadText(TEXT("ArmyShowMarchStatus_0")),
		FString(TEXT("行军中 · 兵力 640")));
	TestEqual(TEXT("Known target is shown verbatim"), ReadText(TEXT("ArmyShowMarchTarget_0")),
		FString(TEXT("本地行军目的地")));
	TestEqual(TEXT("Unknown target stays distinguishable"), ReadText(TEXT("ArmyShowMarchTarget_1")),
		FString(TEXT("目标待同步")));
	TestEqual(TEXT("Unknown time stays distinguishable"), ReadText(TEXT("ArmyShowMarchRemaining_1")),
		FString(TEXT("时间待同步")));
	const auto* Row = Screen->GetWidgetFromName(TEXT("ArmyShowMarchRow_0"));
	TestTrue(TEXT("Tooltip displays the server army identity"), Row && Row->GetToolTipText().ToString() == TEXT("91"));

	auto* Recall = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ArmyShowMarchRecall_0")));
	if (!TestNotNull(TEXT("The row retains its original recall action"), Recall))
	{
		return false;
	}
	Recall->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestTrue(TEXT("Recall reports its existing boundary"), ReadText(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("召回请求尚未接入")));

	Snapshot.Marches[0].RemainingSeconds = 0;
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Countdown updates without rebuilding the list"), ReadScroll() == ActiveList);
	TestEqual(TEXT("Completed timers wait for server authority"), ReadText(TEXT("ArmyShowMarchRemaining_0")),
		FString(TEXT("等待服务器确认")));
	return true;
}

#endif
