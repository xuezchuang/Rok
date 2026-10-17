#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokSpeedUpQueueLifecycle.h"
#include <limits>
#include <cmath>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokSpeedUpQueueLifecycleTest, "Rok.Ui.SpeedUp.QueueLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokSpeedUpQueueLifecycleTest::RunTest(const FString&)
{
	FRokBuildingState Initial; Initial.BuildingKey = TEXT("ActualCityActor"); Initial.UnityInitId = 6;
	Initial.UnityBuildingTypeId = 9; Initial.Level = 1; Initial.bUpgradeInProgress = true;
	Initial.UpgradeStartedAt = 10; Initial.UpgradeFinishesAt = 30;
	FRokSpeedUpQueueLifecycle Lifecycle;
	Lifecycle.Bind(20102, nullptr);
	TestTrue(TEXT("Unknown catalog preview never acquires a later queue"), Lifecycle.Observe(&Initial, 15) == ERokSpeedUpQueueStatus::Preview);
	Lifecycle.Bind(20103, &Initial);
	TestTrue(TEXT("Training has no fabricated local queue"), Lifecycle.Observe(&Initial, 15) == ERokSpeedUpQueueStatus::Preview);
	Lifecycle.Bind(20102, &Initial);
	TestTrue(TEXT("Actual queue remains active without mutation"), Lifecycle.Observe(&Initial, 20) == ERokSpeedUpQueueStatus::Active);
	TestFalse(TEXT("Active queue cannot close as complete"), Lifecycle.ConsumeNaturalCompletion());
	FRokBuildingState Completed = Initial; Completed.bUpgradeInProgress = false; Completed.Level = 2;
	Completed.UpgradeStartedAt = 0; Completed.UpgradeFinishesAt = 0;
	TestTrue(TEXT("Only matching finished queue is completion"), Lifecycle.Observe(&Completed, 30) == ERokSpeedUpQueueStatus::Completed);
	TestTrue(TEXT("Completion is consumed once"), Lifecycle.ConsumeNaturalCompletion());
	TestFalse(TEXT("Repeated refresh cannot replay completion"), Lifecycle.ConsumeNaturalCompletion());
	Lifecycle.Observe(nullptr, 31);
	TestFalse(TEXT("Consumed completion is stable across later refresh"), Lifecycle.ConsumeNaturalCompletion());
	Lifecycle.Bind(20102, &Initial);
	TestTrue(TEXT("Closing and reopening observes original clock"), Lifecycle.Observe(&Initial, 29) == ERokSpeedUpQueueStatus::Active);
	TestTrue(TEXT("Reopened queue can naturally complete"), Lifecycle.Observe(&Completed, 30) == ERokSpeedUpQueueStatus::Completed);
	Lifecycle.Bind(20102, &Initial);
	TestTrue(TEXT("Missing actor is lost, never completed"), Lifecycle.Observe(nullptr, 30) == ERokSpeedUpQueueStatus::Lost);
	TestTrue(TEXT("Lost binding cannot silently attach later"), Lifecycle.Observe(&Completed, 31) == ERokSpeedUpQueueStatus::Lost);
	TestFalse(TEXT("Lost actor does not auto-close as success"), Lifecycle.ConsumeNaturalCompletion());
	for (int32 Case = 0; Case < 6; ++Case)
	{
		Lifecycle.Bind(20102, &Initial); FRokBuildingState Changed = Initial;
		switch (Case)
		{
		case 0: Changed.BuildingKey = TEXT("OtherActor"); break;
		case 1: ++Changed.UnityInitId; break;
		case 2: ++Changed.UnityBuildingTypeId; break;
		case 3: ++Changed.Level; break;
		case 4: Changed.UpgradeStartedAt += 1; break;
		case 5: Changed.UpgradeFinishesAt -= 1; break;
		}
		TestTrue(TEXT("Replacement cannot be mistaken for bound work"), Lifecycle.Observe(&Changed, 20) == ERokSpeedUpQueueStatus::Replaced);
		TestFalse(TEXT("Replacement cannot consume completion"), Lifecycle.ConsumeNaturalCompletion());
	}
	Lifecycle.Bind(20102, &Initial);
	TestTrue(TEXT("Early disappearance is not natural completion"), Lifecycle.Observe(&Completed, 29) == ERokSpeedUpQueueStatus::Lost);
	Lifecycle.Bind(20102, &Initial); Completed.Level = 3;
	TestTrue(TEXT("Two gained levels cannot satisfy one completed queue"), Lifecycle.Observe(&Completed, 31) == ERokSpeedUpQueueStatus::Lost);
	Lifecycle.Bind(20102, &Initial);
	TestTrue(TEXT("Nonfinite observation rejected"), Lifecycle.Observe(&Initial, std::numeric_limits<double>::quiet_NaN()) == ERokSpeedUpQueueStatus::Lost);
	// Actual runtime failure: SpeedUpAudit/20260912_115319_68068, not a fabricated timer jump.
	FRokBuildingState Fractional = Initial;
	Fractional.UpgradeStartedAt = 10.183332443237305f;
	Fractional.UpgradeFinishesAt = 30.183332443237305f;
	FRokBuildingState FractionalDone = Fractional; FractionalDone.bUpgradeInProgress = false;
	FractionalDone.Level = 2; FractionalDone.UpgradeStartedAt = 0; FractionalDone.UpgradeFinishesAt = 0;
	const double ActualWorldSeconds = 30.183331534266472;
	TestTrue(TEXT("Recorded double time is below stored float deadline"), ActualWorldSeconds < Fractional.UpgradeFinishesAt);
	TestEqual(TEXT("Product queue clock exactly reaches that deadline"), static_cast<float>(ActualWorldSeconds), Fractional.UpgradeFinishesAt);
	Lifecycle.Bind(20102, &Fractional);
	TestTrue(TEXT("Observed committed float-clock completion is accepted without epsilon"), Lifecycle.Observe(&FractionalDone, ActualWorldSeconds) == ERokSpeedUpQueueStatus::Completed);
	TestTrue(TEXT("Fractional completion consumes once"), Lifecycle.ConsumeNaturalCompletion());
	TestFalse(TEXT("Fractional completion cannot replay"), Lifecycle.ConsumeNaturalCompletion());
	Lifecycle.Bind(20102, &Fractional);
	const float PreviousFloat = std::nextafter(Fractional.UpgradeFinishesAt, 0.f);
	TestTrue(TEXT("Previous representable queue tick remains genuinely early"), PreviousFloat < Fractional.UpgradeFinishesAt);
	TestTrue(TEXT("Even one previous float cannot excuse premature queue removal"), Lifecycle.Observe(&FractionalDone, double(PreviousFloat)) == ERokSpeedUpQueueStatus::Lost);
	TestFalse(TEXT("Early fractional loss cannot consume completion"), Lifecycle.ConsumeNaturalCompletion());
	Lifecycle.Bind(20102, &Fractional);
	TestTrue(TEXT("Finite double overflowing queue float is rejected"), Lifecycle.Observe(&FractionalDone, std::numeric_limits<double>::max()) == ERokSpeedUpQueueStatus::Lost);
	Lifecycle.Bind(20102, &Fractional);
	TestTrue(TEXT("Negative raw world time stays invalid"), Lifecycle.Observe(&FractionalDone, -std::numeric_limits<double>::min()) == ERokSpeedUpQueueStatus::Lost);
	Lifecycle.Reset();
	TestTrue(TEXT("Close resets old binding"), Lifecycle.GetStatus() == ERokSpeedUpQueueStatus::Preview);
	return true;
}
#endif
