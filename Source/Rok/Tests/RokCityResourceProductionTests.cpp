#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokCityResourceProduction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityResourceProductionTest,
	"Rok.Gameplay.CityResourceProduction.SourceClockAndLocalCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityResourceProductionTest::RunTest(const FString&)
{
	// BuildingResourcesProduce.csv type 2 / level 1: 400 per hour, min 66, max 4000.
	FRokCityResourceProductionInput Input;
	Input.ProducePerHour = 400;
	Input.GatherMin = 66;
	Input.GatherMax = 4000;
	Input.ChangeTimeSeconds = 1000;
	FRokCityResourceProductionResult Result;
	FString Error;

	Input.NowSeconds = 1594;
	TestTrue(TEXT("Incomplete building evaluates"), FRokCityResourceProduction::Evaluate(Input, Result, Error));
	TestEqual(TEXT("Incomplete building produces nothing"), Result.Accrued, int64(0));
	TestFalse(TEXT("Incomplete building cannot collect"), Result.bCanCollect);
	FRokCityResourceProductionInput Next;
	TestFalse(TEXT("Incomplete building cannot collect local credit"),
		FRokCityResourceProduction::Collect(Input, Result, Next, Error));
	TestEqual(TEXT("Failed collection leaves clock unchanged"), Next.ChangeTimeSeconds, int64(1000));

	Input.bCompleted = true;
	Input.NowSeconds = 1000;
	TestTrue(TEXT("Just-completed evaluation"), FRokCityResourceProduction::Evaluate(Input, Result, Error));
	TestEqual(TEXT("No immediate yield"), Result.Accrued, int64(0));
	Input.NowSeconds = 1593;
	TestTrue(TEXT("Before threshold evaluation"), FRokCityResourceProduction::Evaluate(Input, Result, Error));
	TestEqual(TEXT("Credit floors fractional amount"), Result.Accrued, int64(65));
	TestTrue(TEXT("Unrounded preview retains a fraction"), Result.AccruedAmount > 65.0);
	TestFalse(TEXT("Before 66 threshold cannot collect"), Result.bCanCollect);
	Input.NowSeconds = 1594;
	TestTrue(TEXT("Exact threshold evaluation"), FRokCityResourceProduction::Evaluate(Input, Result, Error));
	TestEqual(TEXT("594 seconds gives exactly 66"), Result.Accrued, int64(66));
	TestTrue(TEXT("At threshold may collect"), Result.bCanCollect);
	TestTrue(TEXT("Local collection succeeds"), FRokCityResourceProduction::Collect(Input, Result, Next, Error));
	TestEqual(TEXT("Local credit is 66"), Result.Accrued, int64(66));
	TestEqual(TEXT("Collection clears source-shaped num"), Next.StoredNum, int64(0));
	TestEqual(TEXT("Collection starts new local clock"), Next.ChangeTimeSeconds, int64(1594));
	TestFalse(TEXT("Same tick cannot collect twice"),
		FRokCityResourceProduction::Collect(Next, Result, Input, Error));

	Next.NowSeconds = Next.ChangeTimeSeconds + 36000;
	TestTrue(TEXT("Ten hours evaluates"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	TestEqual(TEXT("Base cap is 4000"), Result.Accrued, int64(4000));
	TestTrue(TEXT("At capacity"), Result.bAtCapacity);
	Next.NowSeconds = TNumericLimits<int64>::Max();
	TestTrue(TEXT("Huge time span caps without overflow"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	TestEqual(TEXT("Huge time still caps"), Result.Accrued, int64(4000));
	Next.StoredNum = TNumericLimits<int64>::Max();
	TestTrue(TEXT("Huge persisted num caps without overflow"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	TestEqual(TEXT("Persisted num is capped"), Result.Accrued, int64(4000));

	Next.NowSeconds = Next.ChangeTimeSeconds - 1;
	TestFalse(TEXT("Clock rollback rejected"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	Next.NowSeconds = 0;
	Next.ChangeTimeSeconds = 1;
	TestFalse(TEXT("Future changeTime rejected"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	Next.ChangeTimeSeconds = 0;
	Next.StoredNum = -1;
	TestFalse(TEXT("Negative server num rejected"), FRokCityResourceProduction::Evaluate(Next, Result, Error));
	return true;
}
#endif
