#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokCityCitizenActor.h"

// City-life preview walkers (Unity citizen.prefab / People.cs). The wander has
// to be deterministic so a screenshot or a test can reproduce a frame.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityCitizenWanderTest,
	"Rok.City.Citizen.Wander",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCitizenWanderTest::RunTest(const FString& Parameters)
{
	const FVector Home(100.0f, -50.0f, 90.0f);
	constexpr float Radius = 700.0f;

	const FVector First = ARokCityCitizenActor::EvaluateWanderTarget(Home, Radius, 7, 1);
	const FVector Again = ARokCityCitizenActor::EvaluateWanderTarget(Home, Radius, 7, 1);
	TestTrue(TEXT("The same seed and step reproduce the target"), First.Equals(Again, 0.001f));

	const FVector Next = ARokCityCitizenActor::EvaluateWanderTarget(Home, Radius, 7, 2);
	TestFalse(TEXT("A new step picks a new target"), First.Equals(Next, 1.0f));

	const FVector OtherSeed = ARokCityCitizenActor::EvaluateWanderTarget(Home, Radius, 8, 1);
	TestFalse(TEXT("Another walker does not share the same path"), First.Equals(OtherSeed, 1.0f));

	bool bInsideRadius = true;
	bool bKeepsDistance = true;
	for (int32 Step = 0; Step < 64; ++Step)
	{
		const FVector Target = ARokCityCitizenActor::EvaluateWanderTarget(Home, Radius, 3, Step);
		const float Distance = FVector::Dist2D(Target, Home);
		bInsideRadius &= Distance <= Radius + 0.5f;
		bKeepsDistance &= Distance >= Radius * 0.35f - 0.5f;
		TestTrue(TEXT("Targets stay on the ground plane"), FMath::IsNearlyEqual(Target.Z, Home.Z, 0.001f));
	}
	TestTrue(TEXT("Every target stays inside the wander radius"), bInsideRadius);
	TestTrue(TEXT("Every target clears the 35% dead zone"), bKeepsDistance);

	TestTrue(TEXT("A zero radius keeps the walker home"),
		ARokCityCitizenActor::EvaluateWanderTarget(Home, 0.0f, 5, 9).Equals(Home, 0.001f));
	return true;
}

#endif
