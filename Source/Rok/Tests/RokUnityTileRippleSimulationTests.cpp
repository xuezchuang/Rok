#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "../RokDataPaths.h"
#include "../RokUnityTileRippleSimulation.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokUnityTileRippleSimulationTest,
	"Rok.WorldTile.TileRipple.SourceLifecycleAndClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUnityTileRippleSimulationTest::RunTest(const FString&)
{
	const FString Fixture = RokDataPaths::Resolve(TEXT("TileRippleSimulationV2/tile_ripple_source_config.json"));
	TArray<FRokTileRippleSourceConfig> Configs;
	FString Error;
	if (!TestTrue(TEXT("Verified two-emitter source fixture loads"),
		FRokTileRippleSourceConfig::LoadSourceFixture(Fixture, Configs, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("Exactly two source emitters"), Configs.Num(), 2);
	if (Configs.Num() != 2) return false;
	TestEqual(TEXT("First stable source identity"), Configs[0].SystemName, FString(TEXT("bowen")));
	TestEqual(TEXT("Second stable source identity"), Configs[1].SystemName, FString(TEXT("bowen (1)")));
	TestEqual(TEXT("Source 5-second loop"), Configs[0].DurationSeconds, 5.0);
	TestEqual(TEXT("Source one birth per second"), Configs[0].RatePerSecond, 1.0);
	TestEqual(TEXT("Source 2.5-second minimum life"), Configs[0].LifetimeMinSeconds, 2.5);
	TestEqual(TEXT("Source 3-second maximum life"), Configs[0].LifetimeMaxSeconds, 3.0);
	TestEqual(TEXT("Source serialized but inactive arc retained"), Configs[0].ShapeArcDegrees, 153.4);
	TestEqual(TEXT("Source alpha starts transparent"), Configs[0].AlphaKeys[0].Alpha, 0.0);
	TestEqual(TEXT("Source alpha ends transparent"), Configs[0].AlphaKeys.Last().Alpha, 0.0);

	// Fix only the random life/size range to expose exact birth/death boundaries.
	FRokTileRippleSourceConfig Fixed = Configs[0];
	Fixed.LifetimeMaxSeconds = Fixed.LifetimeMinSeconds;
	Fixed.StartSizeMax = Fixed.StartSizeMin;
	FRokUnityTileRippleSimulation Sim(Fixed);
	TestTrue(TEXT("Reset fixed-seed local simulation"), Sim.Reset(424242, Error));
	TArray<FRokTileRippleParticleSnapshot> Views;
	Sim.Snapshot(Views);
	TestEqual(TEXT("No prewarm birth"), Views.Num(), 0);
	TestTrue(TEXT("Birth boundary at one second"), Sim.Advance(1.0, Error));
	Sim.Snapshot(Views);
	TestEqual(TEXT("First scheduled birth"), Views.Num(), 1);
	if (Views.Num() != 1) return false;
	TestEqual(TEXT("Birth ordinal"), Views[0].BirthOrdinal, uint64(1));
	TestTrue(TEXT("Birth size zero from source Hermite"), FMath::IsNearlyZero(Views[0].CurrentSize));
	TestTrue(TEXT("Birth alpha zero from source gradient"), FMath::IsNearlyZero(Views[0].CurrentColor.A));
	TestTrue(TEXT("Local edge spawn bounded by source radius"),
		FMath::Abs(Views[0].LocalPosition.X) <= Fixed.ShapeEdgeRadius);
	TestTrue(TEXT("Source scalar rotation retained"),
		FMath::IsNearlyEqual(Views[0].InitialRotationRadians, -1.570796, 0.000001));
	FRokUnityTileRippleSimulation Boundary(Fixed);
	TestTrue(TEXT("Boundary reset"), Boundary.Reset(424242, Error));
	TestTrue(TEXT("Just before first birth"), Boundary.Advance(0.999999, Error));
	TestEqual(TEXT("No early analytic birth"), Boundary.GetLiveCount(), 0);
	TestTrue(TEXT("Reach exact first birth"), Boundary.Advance(0.000001, Error));
	TestEqual(TEXT("One birth at boundary"), Boundary.GetLiveCount(), 1);
	TestTrue(TEXT("Just after first birth"), Boundary.Advance(0.000001, Error));
	TestEqual(TEXT("No duplicate boundary birth"), Boundary.GetLiveCount(), 1);
	TestTrue(TEXT("Half-second natural advance"), Sim.Advance(0.5, Error));
	Sim.Snapshot(Views);
	TestEqual(TEXT("No extra birth at 1.5 seconds"), Views.Num(), 1);
	TestTrue(TEXT("Size Hermite grows after birth"), Views[0].CurrentSize > 0.0);
	TestTrue(TEXT("Alpha becomes visible then later fades"), Views[0].CurrentColor.A > 0.0f);
	TestTrue(TEXT("Enabled local velocity moves Z by source .2 per second"),
		FMath::IsNearlyEqual(Views[0].LocalPosition.Z, 0.1, 0.00001));
	TestTrue(TEXT("Advance to fixed-life death boundary"), Sim.Advance(2.0, Error));
	Sim.Snapshot(Views);
	TestEqual(TEXT("Born at one second is dead at age2.5"), Sim.GetTotalBirths(), uint64(3));
	for (const FRokTileRippleParticleSnapshot& View : Views)
	{
		TestTrue(TEXT("No dead particle retained"), View.BirthOrdinal != 1);
	}
	TestTrue(TEXT("Advance past five-second source cycle"), Sim.Advance(1.6, Error));
	TestEqual(TEXT("Loop continues scheduled births"), Sim.GetTotalBirths(), uint64(5));
	TestTrue(TEXT("Loop phase wraps at source duration"), FMath::IsNearlyEqual(Sim.GetCycleSeconds(), 0.1, 0.000001));

	FRokUnityTileRippleSimulation Whole(Configs[1]), Split(Configs[1]);
	TestTrue(TEXT("Whole reset"), Whole.Reset(17, Error));
	TestTrue(TEXT("Split reset"), Split.Reset(17, Error));
	TestTrue(TEXT("Whole 7.5-second advance"), Whole.Advance(7.5, Error));
	for (int32 Step = 0; Step < 75; ++Step)
	{
		if (!TestTrue(TEXT("Split 0.1-second advance"), Split.Advance(0.1, Error))) return false;
	}
	TArray<FRokTileRippleParticleSnapshot> WholeViews, SplitViews;
	Whole.Snapshot(WholeViews); Split.Snapshot(SplitViews);
	auto CompareSnapshots = [this](const TCHAR* Label,
		const TArray<FRokTileRippleParticleSnapshot>& Left,
		const TArray<FRokTileRippleParticleSnapshot>& Right)
	{
		TestEqual(FString::Printf(TEXT("%s living count"), Label), Left.Num(), Right.Num());
		for (int32 Index = 0; Index < FMath::Min(Left.Num(), Right.Num()); ++Index)
		{
			const FRokTileRippleParticleSnapshot& A = Left[Index];
			const FRokTileRippleParticleSnapshot& B = Right[Index];
			TestEqual(FString::Printf(TEXT("%s ordinal %d"), Label, Index), A.BirthOrdinal, B.BirthOrdinal);
			TestTrue(FString::Printf(TEXT("%s birth/age/life %d"), Label, Index),
				FMath::IsNearlyEqual(A.BirthSeconds, B.BirthSeconds, 0.000001)
				&& FMath::IsNearlyEqual(A.AgeSeconds, B.AgeSeconds, 0.000001)
				&& FMath::IsNearlyEqual(A.LifetimeSeconds, B.LifetimeSeconds, 0.000001));
			TestTrue(FString::Printf(TEXT("%s position/size/rotation %d"), Label, Index),
				A.LocalPosition.Equals(B.LocalPosition, 0.000001)
				&& FMath::IsNearlyEqual(A.CurrentSize, B.CurrentSize, 0.000001)
				&& FMath::IsNearlyEqual(A.InitialRotationRadians, B.InitialRotationRadians, 0.000001));
			TestTrue(FString::Printf(TEXT("%s RGBA %d"), Label, Index),
				A.CurrentColor.Equals(B.CurrentColor, 0.000001));
		}
	};
	TestEqual(TEXT("Partition-independent birth count"), Whole.GetTotalBirths(), Split.GetTotalBirths());
	CompareSnapshots(TEXT("Split-delta complete snapshot"), WholeViews, SplitViews);
	TestTrue(TEXT("Repeat reset with same seed"), Whole.Reset(17, Error));
	TestTrue(TEXT("Repeat natural advance"), Whole.Advance(7.5, Error));
	TArray<FRokTileRippleParticleSnapshot> Replayed;
	Whole.Snapshot(Replayed);
	CompareSnapshots(TEXT("Reset replay complete snapshot"), WholeViews, Replayed);
	FRokUnityTileRippleSimulation Catchup(Configs[0]);
	TestTrue(TEXT("Catch-up reset"), Catchup.Reset(29, Error));
	TestTrue(TEXT("One-billion-second legal advance"), Catchup.Advance(1.0e9, Error));
	TestEqual(TEXT("Scheduled births advance without replaying old particles"),
		Catchup.GetTotalBirths(), uint64(1000000000));
	TestTrue(TEXT("Catch-up retains only potential live particles"), Catchup.GetLiveCount() <= 4);
	TArray<FRokTileRippleParticleSnapshot> CatchupBefore;
	Catchup.Snapshot(CatchupBefore);
	TestFalse(TEXT("Beyond one-billion-second clock limit"), Catchup.Advance(0.1, Error));
	TestFalse(TEXT("Positive infinity rejected"), Catchup.Advance(std::numeric_limits<double>::infinity(), Error));
	TestFalse(TEXT("Negative infinity rejected"), Catchup.Advance(-std::numeric_limits<double>::infinity(), Error));
	TArray<FRokTileRippleParticleSnapshot> CatchupAfter;
	Catchup.Snapshot(CatchupAfter);
	TestEqual(TEXT("Rejected large/nonfinite clock unchanged"), Catchup.GetElapsedSeconds(), 1.0e9);
	TestEqual(TEXT("Rejected large/nonfinite births unchanged"), Catchup.GetTotalBirths(), uint64(1000000000));
	CompareSnapshots(TEXT("Rejected large/nonfinite snapshot unchanged"), CatchupBefore, CatchupAfter);
	const double Before = Whole.GetElapsedSeconds();
	const uint64 BirthsBefore = Whole.GetTotalBirths();
	TestFalse(TEXT("Reject clock rollback"), Whole.Advance(-0.1, Error));
	TestFalse(TEXT("Reject non-finite advance"), Whole.Advance(std::numeric_limits<double>::quiet_NaN(), Error));
	TestEqual(TEXT("Rejected advances preserve clock"), Whole.GetElapsedSeconds(), Before);
	TestEqual(TEXT("Rejected advances preserve births"), Whole.GetTotalBirths(), BirthsBefore);
	FRokTileRippleSourceConfig Invalid = Configs[0];
	Invalid.RatePerSecond = -1.0;
	FRokUnityTileRippleSimulation Bad(Invalid);
	TestFalse(TEXT("Reject malformed source rate on reset"), Bad.Reset(0, Error));
	return true;
}
#endif
