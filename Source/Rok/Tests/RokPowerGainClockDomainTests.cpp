#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../RokPowerGainWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainClockDomainTest,
	"Rok.Ui.Animation.PowerGain.AbsoluteClockAndParticleDelta",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokPowerGainClockDomainTest::RunTest(const FString&)
{
	auto MaterialU = [](URokPowerGainWidget* Effect)
	{
		return Effect->Materials[0]->K2_GetScalarParameterValue(TEXT("UTiling"));
	};
	auto* Startup = NewObject<URokPowerGainWidget>();
	if (!TestTrue(TEXT("Pinned source widget loads for startup clock"), Startup->PlayAtClock(0.0))) return false;
	// Unity V6 actual run 0: first natural LateUpdate is frame 1. Unity Time.time is
	// still zero, while the child ParticleSystem has simulated 20 ms.
	TestTrue(TEXT("First source frame accepts independent clocks"), Startup->AdvanceAtClock(.02f, 0.0));
	TestTrue(TEXT("Root lifetime has not advanced"), FMath::IsNearlyEqual(Startup->GetRemainingTime(), 1.f, .000001f));
	TestEqual(TEXT("Public mesh request clock stays at zero"), Startup->GetPlaybackTime(), 0.f);
	TestEqual(TEXT("Actual material stays at the initial key"), MaterialU(Startup), 1.f);
	TestEqual(TEXT("One first-step burst is live"), Startup->ParticleTimes.Num(), 1);
	TestFalse(TEXT("Particle birth is no longer pending"), Startup->ParticleTimes[0].bPendingBirth);
	TestEqual(TEXT("One 20 ms substep leaves birth age zero"), Startup->ParticleTimes[0].Age, 0.f);
	// Unity V6 actual run 0 frame 2 advances both domains by 20 ms.
	TestTrue(TEXT("Second source frame advances"), Startup->AdvanceAtClock(.02f, .02));
	TestTrue(TEXT("Root follows Time.time"), FMath::IsNearlyEqual(Startup->GetRemainingTime(), .98f, .000001f));
	TestTrue(TEXT("Mesh follows Time.time"), FMath::IsNearlyEqual(Startup->GetPlaybackTime(), .02f, .000001f));
	TestTrue(TEXT("Actual material follows V6 frame 2"), FMath::IsNearlyEqual(MaterialU(Startup), .83935833f, .0001f));
	TestTrue(TEXT("Older particle spans the whole second frame"),
		FMath::IsNearlyEqual(Startup->ParticleTimes[0].Age, .02f, .000001f));

	const float Before = Startup->GetRemainingTime();
	TestFalse(TEXT("Backward absolute time is rejected"), Startup->AdvanceAtClock(.02f, .019));
	TestTrue(TEXT("Rejected clock leaves lifetime unchanged"), FMath::IsNearlyEqual(Startup->GetRemainingTime(), Before, .000001f));
	const float BeforeMesh = Startup->MeshTime;
	const float BeforeRenderedMesh = Startup->RenderMeshTime;
	const float BeforeMaterialU = MaterialU(Startup);
	const double BeforeAbsolute = Startup->LastAbsoluteClockSeconds;
	const bool BeforeAbsoluteMode = Startup->bHasAbsoluteClock;
	const bool BeforePlaying = Startup->bPlaying;
	const bool BeforeRestart = Startup->bAnimatorRestartPending;
	const int32 BeforeParticleCount = Startup->ParticleTimes.Num();
	const float BeforeParticleAge = Startup->ParticleTimes[0].Age;
	const bool BeforePendingBirth = Startup->ParticleTimes[0].bPendingBirth;
	if (!TestFalse(TEXT("Negative particle delta is rejected before either clock mutates"),
		Startup->AdvanceAtClock(-.01f, .04))) return false;
	TestEqual(TEXT("Negative delta keeps root lifetime"), Startup->Remaining, Before);
	TestEqual(TEXT("Negative delta keeps public mesh clock"), Startup->MeshTime, BeforeMesh);
	TestEqual(TEXT("Negative delta keeps rendered mesh clock"), Startup->RenderMeshTime, BeforeRenderedMesh);
	TestEqual(TEXT("Negative delta keeps material U"), MaterialU(Startup), BeforeMaterialU);
	TestEqual(TEXT("Negative delta keeps absolute clock"), Startup->LastAbsoluteClockSeconds, BeforeAbsolute);
	TestEqual(TEXT("Negative delta keeps absolute mode"), Startup->bHasAbsoluteClock, BeforeAbsoluteMode);
	TestEqual(TEXT("Negative delta keeps play state"), Startup->bPlaying, BeforePlaying);
	TestEqual(TEXT("Negative delta keeps restart state"), Startup->bAnimatorRestartPending, BeforeRestart);
	TestEqual(TEXT("Negative delta keeps particle count"), Startup->ParticleTimes.Num(), BeforeParticleCount);
	TestEqual(TEXT("Negative delta keeps particle age"), Startup->ParticleTimes[0].Age, BeforeParticleAge);
	TestEqual(TEXT("Negative delta keeps pending birth"), Startup->ParticleTimes[0].bPendingBirth, BeforePendingBirth);
	TestTrue(TEXT("Following valid delta still advances from the old absolute clock"),
		Startup->AdvanceAtClock(.02f, .04));
	TestTrue(TEXT("Valid continuation advances full root delta"),
		FMath::IsNearlyEqual(Startup->Remaining, Before - .02f, .000001f));
	TestTrue(TEXT("Valid continuation advances full particle delta"),
		FMath::IsNearlyEqual(Startup->ParticleTimes[0].Age, BeforeParticleAge + .02f, .000001f));

	auto* Later = NewObject<URokPowerGainWidget>();
	// Unity V6 actual run 2 is instantiated at 6.1707630157 s; its first
	// normal frame reaches 6.2462697029 s with a distinct particle delta.
	if (!TestTrue(TEXT("Later source origin loads"), Later->PlayAtClock(6.17076301574707))) return false;
	TestTrue(TEXT("Later source first frame advances"),
		Later->AdvanceAtClock(.0755065009f, 6.246269702911377));
	TestTrue(TEXT("Later root elapsed follows actual absolute clock"),
		FMath::IsNearlyEqual(Later->GetPlaybackTime(), .0755066872f, .000001f));
	TestTrue(TEXT("Later actual material matches V6 first frame"),
		FMath::IsNearlyEqual(MaterialU(Later), .50168860f, .0001f));
	TestEqual(TEXT("Later first burst exists"), Later->ParticleTimes.Num(), 1);
	TestTrue(TEXT("Later first burst ages across post-emission substep"),
		FMath::IsNearlyEqual(Later->ParticleTimes[0].Age, .0503376722f, .000001f));
	return true;
}
#endif
