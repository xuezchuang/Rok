#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCommanderMeshWidget.h"
#include "../RokCommanderCamera.h"
#include "../RokCommanderRosterAnimation.h"
#include "Widgets/SWidget.h"
#include "Components/Button.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCivilizationHeroMeshTest,
	"Rok.Ui.Animation.Civilization.HeroMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCivilizationHeroMeshTest::RunTest(const FString&)
{
	const FVector2f Size(1440, 810);
	const auto Origin = RokCommanderCamera::ProjectCivilization(FVector2f::ZeroVector, Size, 50);
	TestEqual(TEXT("Source settled hero anchor including civilization offset"), Origin.X, 310.0f);
	TestEqual(TEXT("Source hero feet anchor"), Origin.Y, 852.0f);
	const auto Point = RokCommanderCamera::ProjectCivilization(FVector2f(1, 1), Size, 50);
	TestEqual(TEXT("SkeletonGraphic horizontal scale"), Point.X - Origin.X, 150.0f);
	TestEqual(TEXT("SkeletonGraphic vertical inversion"), Point.Y - Origin.Y, -150.0f);
	TestEqual(TEXT("Zero-offset civilization keeps source anchor"),
		RokCommanderCamera::ProjectCivilization(FVector2f::ZeroVector, Size, 0).X, 235.0f);
	for (const int32 HeroId : {2007, 2008, 2004})
	{
		auto* Hero = NewObject<URokCommanderMeshWidget>();
		if (!TestTrue(FString::Printf(TEXT("Civilization hero %d loads source samples"), HeroId), Hero->LoadHero(HeroId))) return false;
		Hero->SetCivilizationProjection(HeroId == 2004 ? 0 : 50);
		TestEqual(TEXT("Complete idle clip"), Hero->GetAnimationFrameCount(), 200);
		TestTrue(TEXT("Civilization hero has deforming geometry"), Hero->HasDeformedSamplesForTesting());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderSourceMeshTest,
	"Rok.Ui.Animation.Commander.SourceMesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderSourceMeshTest::RunTest(const FString&)
{
	auto* Widget = NewObject<URokCommanderMeshWidget>();
	if (!TestTrue(TEXT("Generated source mesh loads"), Widget->LoadHero(2007))) return false;
	TestEqual(TEXT("Complete source idle sampled at 60Hz"), Widget->GetAnimationFrameCount(), 200);
	TestTrue(TEXT("Source idle duration, not a generic breathing period"),
		FMath::IsNearlyEqual(Widget->GetAnimationDuration(), 3.3333333f));
	TestTrue(TEXT("Mesh actually deforms rather than translating a still image"), Widget->HasDeformedSamplesForTesting());
	TestTrue(TEXT("Native Slate renderer constructs"), Widget->TakeWidget().GetSharedReferenceCount() > 0);
	Widget->ReleaseSlateResources(true);
	TestTrue(TEXT("Renderer can rebuild without losing source clip"), Widget->TakeWidget().GetSharedReferenceCount() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderRosterMeshTest,
	"Rok.Ui.Animation.Commander.StarterRoster", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderRosterMeshTest::RunTest(const FString&)
{
	// Current bootstrap roster, resolved to matching Hero.model before export.
	for (int32 HeroId : {1006, 1007, 1009, 1010, 1012})
	{
		auto* Widget = NewObject<URokCommanderMeshWidget>();
		if (!TestTrue(FString::Printf(TEXT("Hero %d source mesh loads"), HeroId), Widget->LoadHero(HeroId))) return false;
		TestTrue(TEXT("Roster uses animated geometry"), Widget->HasDeformedSamplesForTesting());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderPortraitLifecycleTest,
	"Rok.Ui.Animation.Commander.PortraitLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderPortraitLifecycleTest::RunTest(const FString&)
{
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenCommander();
	auto* Portrait = Cast<URokCommanderMeshWidget>(Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")));
	if (!TestNotNull(TEXT("Runtime commander owns animated portrait"), Portrait)) return false;
	auto* Sort = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSortAction")));
	if (!TestNotNull(TEXT("Real commander sort action"), Sort)) return false;
	Sort->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestTrue(TEXT("Same-hero rebuild preserves the exact animation owner"),
		Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")) == Portrait);
	Screen->OpenItems();
	TestNull(TEXT("Leaving commander removes portrait from live tree"), Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")));
	Screen->CloseScreen();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderGestureTest,
	"Rok.Ui.Animation.Commander.Gesture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderGestureTest::RunTest(const FString&)
{
	for (int32 HeroId : {1006, 1009})
	{
		auto* Portrait = NewObject<URokCommanderMeshWidget>();
		if (!TestTrue(TEXT("Source idle and gesture load"), Portrait->LoadHero(HeroId))) return false;
		TestTrue(TEXT("First click starts source standby"), Portrait->PlayStandby());
		const float GestureDuration = Portrait->GetAnimationDuration();
		TestTrue(TEXT("Exact source gesture duration"), FMath::IsNearlyEqual(GestureDuration, HeroId == 1006 ? 3.0f : 1.8333334f));
		Portrait->AdvanceAnimation(GestureDuration * 0.5f);
		TestFalse(TEXT("Repeated click does not restart gesture"), Portrait->PlayStandby());
		Portrait->AdvanceAnimation(GestureDuration * 0.5f + 0.01f);
		TestFalse(TEXT("Completion returns to idle"), Portrait->IsPlayingStandby());
		TestEqual(TEXT("Original idle samples restored"), Portrait->GetAnimationFrameCount(), 200);
		TestTrue(TEXT("Can click again after completion"), Portrait->PlayStandby());
		TestTrue(TEXT("Changing hero loads its idle"), Portrait->LoadHero(2007));
		TestFalse(TEXT("Changing hero clears gesture state"), Portrait->IsPlayingStandby());
		TestFalse(TEXT("No invented gesture for source without standby"), Portrait->PlayStandby());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderCameraTest,
	"Rok.Ui.Animation.Commander.Camera", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderCameraTest::RunTest(const FString&)
{
	const FVector2f Size(1440, 900);
	const auto Origin = RokCommanderCamera::Project(FVector2f::ZeroVector, Size, 0);
	TestEqual(TEXT("Source Hero X=0 projects to viewport center"), Origin.X, 720.0f);
	TestTrue(TEXT("Source origin near feet, not mesh bounds center"), Origin.Y > 790 && Origin.Y < 810);
	const auto Right = RokCommanderCamera::Project(FVector2f(1, 0), Size, 0);
	TestTrue(TEXT("Source projection scale from FOV/depth/1.3"), Right.X - Origin.X > 115 && Right.X - Origin.X < 118);
	TestTrue(TEXT("Negative camera offset puts entering hero on right"),
		RokCommanderCamera::Project(FVector2f::ZeroVector, Size, -10).X > Size.X);
	TestTrue(TEXT("Positive camera offset puts entering hero on left"),
		RokCommanderCamera::Project(FVector2f::ZeroVector, Size, 10).X < 0);
	TestTrue(TEXT("First source update uses speed20"), FMath::IsNearlyEqual(RokCommanderCamera::Advance(-10, 0.1f), -8.0f));
	TestTrue(TEXT("Damping increases as camera approaches center"),
		FMath::IsNearlyEqual(RokCommanderCamera::Advance(5, 0.1f), 3.675f));
	for (int32 Fps : {30, 60, 120})
	{
		float X = -10;
		for (int32 Frame = 0; Frame < Fps * 3; ++Frame)
		{
			const float Previous = X;
			X = RokCommanderCamera::Advance(X, 1.0f / Fps);
			TestTrue(TEXT("Camera approaches center without overshoot"), X >= Previous && X <= 0);
		}
		TestEqual(TEXT("Camera settles exactly"), X, 0.0f);
	}
	auto* Portrait = NewObject<URokCommanderMeshWidget>();
	Portrait->LoadHero(1006);
	Portrait->BeginSelectionTransition(true);
	Portrait->AdvanceAnimation(0.1f);
	TestTrue(TEXT("Runtime animation clock also advances selection camera"), FMath::IsNearlyEqual(Portrait->GetSelectionCameraX(), -8.0f));
	Portrait->LoadHero(1009);
	TestEqual(TEXT("Loading a hero clears obsolete camera state"), Portrait->GetSelectionCameraX(), 0.0f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderRosterAnimationTest,
	"Rok.Ui.Animation.Commander.RosterEntrance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokCommanderRosterAnimationTest::RunTest(const FString&)
{
	using namespace RokCommanderRosterAnimation;
	TestEqual(TEXT("Source delayed entry position"), Evaluate(0.08f).X, -200.0f);
	TestEqual(TEXT("Source delayed entry invisible"), Evaluate(0.08f).Opacity, 0.0f);
	TestTrue(FString::Printf(TEXT("Source zero-slope midpoint %.9f"), Evaluate(0.2f).X), FMath::IsNearlyEqual(Evaluate(0.2f).X, -100.0f, 0.001f));
	TestTrue(FString::Printf(TEXT("Source opacity midpoint %.9f"), Evaluate(0.2f).Opacity), FMath::IsNearlyEqual(Evaluate(0.2f).Opacity, 0.55f, 0.00001f));
	TestEqual(TEXT("Final offset"), Evaluate(Duration).X, 0.0f);
	TestEqual(TEXT("Final opacity"), Evaluate(Duration).Opacity, 1.0f);
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenCommander();
	UWidget* List = Screen->GetWidgetFromName(TEXT("CommanderRosterScroll"));
	if (!TestNotNull(TEXT("Real roster"), List)) return false;
	TestEqual(TEXT("Opening applies first pose before paint"), List->GetRenderOpacity(), 0.0f);
	Screen->AdvanceCommanderRosterAnimationForTesting(0.2f);
	TestTrue(TEXT("Runtime entry moves roster"), FMath::IsNearlyEqual(List->GetRenderTransform().Translation.X, -100.0f, 0.001f));
	auto* Sort = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSortAction")));
	if (!TestNotNull(TEXT("Sort route"), Sort)) return false;
	Sort->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	List = Screen->GetWidgetFromName(TEXT("CommanderRosterScroll"));
	TestTrue(TEXT("Rebuild does not reset entrance"), FMath::IsNearlyEqual(List->GetRenderOpacity(), 0.55f, 0.00001f));
	UWidget* Portrait = Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait"));
	TestTrue(TEXT("Roster entry does not move character canvas"), Portrait && Portrait->GetRenderTransform().Translation.IsZero());
	Screen->AdvanceCommanderRosterAnimationForTesting(1.0f);
	TestEqual(TEXT("List settles fully visible"), List->GetRenderOpacity(), 1.0f);
	Screen->CloseScreen(); Screen->OpenCommander();
	TestEqual(TEXT("Reopening starts a fresh entrance"), Screen->GetWidgetFromName(TEXT("CommanderRosterScroll"))->GetRenderOpacity(), 0.0f);
	Screen->CloseScreen();
	return true;
}
#endif
