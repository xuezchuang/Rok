#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokWorldMapSpriteAnimation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapSpriteAnimationTest,
	"Rok.WorldMap.EntityPopulation.IndependentIdleClocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapSpriteAnimationTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(1729), ReplayRandom(1729);
	TestEqual(TEXT("Below first troop LOD"), FRokWorldMapSpriteAnimation::FormationLod(500.9f), 0);
	TestEqual(TEXT("First troop LOD includes exact upper bound"), FRokWorldMapSpriteAnimation::FormationLod(501.0f), 0);
	TestEqual(TEXT("Crossing first troop LOD rebuilds"), FRokWorldMapSpriteAnimation::FormationLod(501.01f), 1);
	TestEqual(TEXT("Expanded troop LOD"), FRokWorldMapSpriteAnimation::FormationLod(899.9f), 1);
	TestEqual(TEXT("Expanded troop LOD includes exact upper bound"), FRokWorldMapSpriteAnimation::FormationLod(900.0f), 1);
	TestEqual(TEXT("Crossing second troop LOD rebuilds"), FRokWorldMapSpriteAnimation::FormationLod(900.01f), 2);
	FRokWorldMapSpriteAnimation Cards[3], Replay;
	for (auto& Card : Cards) Card.Reset(Random, 8, 0.016f);
	Replay.Reset(ReplayRandom, 8, 0.016f);
	TestEqual(TEXT("Seed repeats initial frame"), Cards[0].Frame, Replay.Frame);
	TestEqual(TEXT("Pooled OnSpawn overwrites random initial frame"), Cards[0].Frame, 0);
	TestEqual(TEXT("Seed repeats timer phase"), Cards[0].Elapsed, Replay.Elapsed);
	TestTrue(TEXT("Clones have independent phase"), Cards[0].Elapsed != Cards[1].Elapsed && Cards[1].Elapsed != Cards[2].Elapsed);
	FRokWorldMapSpriteAnimation Clock;
	TestFalse(TEXT("Exact rate boundary holds source frame"), Clock.Advance(0.15f, 0.15f, 8));
	TestTrue(TEXT("Next tick advances"), Clock.Advance(0.001f, 0.15f, 8));
	TestEqual(TEXT("First advancement"), Clock.Frame, 1);
	Clock.Elapsed = 0.0f;
	Clock.Frame = 6;
	Clock.Advance(0.46f, 0.15f, 8);
	TestEqual(TEXT("Unity hitch overshoot resets to zero instead of modulo"), Clock.Frame, 0);
	TestTrue(TEXT("Hitch retains fractional elapsed time"), FMath::IsNearlyEqual(Clock.Elapsed, 0.01f));
	Clock.Elapsed = 0.0f;
	TestFalse(TEXT("Minimum source interval holds"), Clock.Advance(0.04f, 0.0f, 8));
	TestTrue(TEXT("Minimum source interval advances"), Clock.Advance(0.002f, 0.0f, 8));
	TestFalse(TEXT("No frames cannot advance"), Clock.Advance(1.0f, 0.15f, 0));
	TestTrue(TEXT("Source spawn fade reaches half at one sixth second"), FMath::IsNearlyEqual(FRokWorldMapSpriteAnimation::AdvanceSpawnFade(0.0f, 1.0f / 6.0f), 0.5f));
	TestEqual(TEXT("Source fade clamps on a long frame"), FRokWorldMapSpriteAnimation::AdvanceSpawnFade(0.5f, 1.0f), 1.0f);
	TestEqual(TEXT("Complete fade remains complete"), FRokWorldMapSpriteAnimation::AdvanceSpawnFade(1.0f, 0.01f), 1.0f);
	return true;
}

#endif
