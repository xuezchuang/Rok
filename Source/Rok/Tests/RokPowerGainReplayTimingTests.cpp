#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../RokPowerGainWidget.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainReplayTimingTest,
	"Rok.Ui.Animation.PowerGain.ReplayAnimatorTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPowerGainReplayTimingTest::RunTest(const FString&)
{
	auto* Effect = NewObject<URokPowerGainWidget>();
	if (!TestTrue(TEXT("Actual source effect loads"), Effect->Play())) return false;
	auto MaterialU = [&]() { return Effect->Materials[0]->K2_GetScalarParameterValue(TEXT("UTiling")); };

	TestEqual(TEXT("Fresh play starts at the first key"), MaterialU(), 1.f);
	Effect->Advance(.2f);
	const float PreviousU = MaterialU();
	TestTrue(TEXT("Fresh play advances on its first positive update"),
		FMath::IsNearlyEqual(PreviousU, URokPowerGainWidget::EvaluateUTiling(.2f), .00001f));
	TestEqual(TEXT("Initial render phase tracks request elapsed"), Effect->RenderMeshTime, .2f);

	if (!TestTrue(TEXT("Replay request succeeds"), Effect->Play())) return false;
	TestEqual(TEXT("Public request clock resets immediately"), Effect->GetPlaybackTime(), 0.f);
	TestTrue(TEXT("Replay queues Animator reset"), Effect->bAnimatorRestartPending);
	TestEqual(TEXT("Same-frame renderer keeps the prior UV"), MaterialU(), PreviousU);
	TestEqual(TEXT("Same-frame renderer keeps the prior phase"), Effect->RenderMeshTime, .2f);
	TestTrue(TEXT("Root lifetime extends once"), FMath::IsNearlyEqual(Effect->GetRemainingTime(), 1.8f));

	Effect->Advance(0.f);
	Effect->Advance(-1.f);
	Effect->Advance(std::numeric_limits<float>::quiet_NaN());
	TestTrue(TEXT("Nonpositive and nonfinite updates do not consume queued reset"), Effect->bAnimatorRestartPending);
	TestEqual(TEXT("Queued renderer value is stable before a real tick"), MaterialU(), PreviousU);
	TestEqual(TEXT("Elapsed clock stays at zero"), Effect->GetPlaybackTime(), 0.f);

	if (!TestTrue(TEXT("Second same-frame replay succeeds"), Effect->Play())) return false;
	TestTrue(TEXT("Repeated replay remains one queued visible reset"), Effect->bAnimatorRestartPending);
	TestEqual(TEXT("Every replay still requests a particle burst"), Effect->ParticleTimes.Num(), 3);
	TestTrue(TEXT("Every replay still extends root lifetime"), FMath::IsNearlyEqual(Effect->GetRemainingTime(), 2.8f));
	TestEqual(TEXT("Repeated same-frame Play keeps prior UV"), MaterialU(), PreviousU);

	Effect->Advance(.0083325f);
	TestFalse(TEXT("First positive update consumes the reset"), Effect->bAnimatorRestartPending);
	TestEqual(TEXT("First normal update displays Animator phase zero"), Effect->RenderMeshTime, 0.f);
	TestEqual(TEXT("First normal update displays the first UV key"), MaterialU(), 1.f);
	TestEqual(TEXT("Business elapsed still includes first update"), Effect->GetPlaybackTime(), .0083325f);

	// Unity run 2 replay 1 has a real 64 ms second frame. This distinguishes
	// using the current frame delta from accidentally using the first delta.
	Effect->Advance(.0641429f);
	TestEqual(TEXT("Second normal update advances by its own delta"), Effect->RenderMeshTime, .0641429f);
	TestTrue(TEXT("Material follows the render phase"),
		FMath::IsNearlyEqual(MaterialU(), URokPowerGainWidget::EvaluateUTiling(.0641429f), .00001f));
	TestTrue(TEXT("Public clock remains request-relative"),
		FMath::IsNearlyEqual(Effect->GetPlaybackTime(), .0083325f + .0641429f));
	return true;
}
#endif
