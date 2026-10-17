#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokLocalCitySimulationContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalResourceLevelTransitionTest,
	"Rok.Runtime.LocalCity.ResourceLevelTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLocalResourceLevelTransitionTest::RunTest(const FString&)
{
	FRokLocalProductionLedger Ledger;
	FRokLocalConstructionTicket Farm;
	Farm.Id = 41;
	Farm.TypeId = 2;
	Farm.FinishesAt = 1000.0;
	Farm.bCompleted = true;
	const FRokLocalProductionSource LevelOne{2, 1, 400, 66, 4000};
	const FRokLocalProductionSource LevelTwo{2, 2, 430, 71, 4300};
	FString Error;
	if (!TestTrue(TEXT("Completed farm starts its source clock"),
		Ledger.RegisterCompleted(Farm, LevelOne, 1000, Error)))
	{
		AddError(Error);
		return false;
	}

	FRokCityResourceProductionResult Preview;
	TestTrue(TEXT("L1 preview after one hour"), Ledger.Preview(Farm.Id, 4600, Preview, Error));
	TestEqual(TEXT("L1 uses 400 per hour"), Preview.Accrued, int64(400));
	FRokLocalProductionSource Actual;
	TestTrue(TEXT("Clock exposes current source row"), Ledger.GetSource(Farm.Id, Actual, Error));
	TestEqual(TEXT("Clock starts at L1"), Actual.Level, 1);
	TestFalse(TEXT("Unknown construction cannot transition"),
		Ledger.TransitionLevel(999, LevelOne, LevelTwo, Error));
	TestFalse(TEXT("Wrong old rate cannot transition"),
		Ledger.TransitionLevel(Farm.Id, {2, 1, 401, 66, 4000}, LevelTwo, Error));
	TestFalse(TEXT("Wrong type cannot transition"),
		Ledger.TransitionLevel(Farm.Id, LevelOne, {3, 2, 430, 71, 4300}, Error));
	TestFalse(TEXT("Wrong target level cannot transition"),
		Ledger.TransitionLevel(Farm.Id, LevelOne, {2, 3, 430, 71, 4300}, Error));
	TestFalse(TEXT("Invalid next cap cannot transition"),
		Ledger.TransitionLevel(Farm.Id, LevelOne, {2, 2, 430, 71, 70}, Error));
	TestTrue(TEXT("Rejected changes preserve original preview"), Ledger.Preview(Farm.Id, 4600, Preview, Error));
	TestEqual(TEXT("Rejected changes retain L1 rate"), Preview.Accrued, int64(400));

	if (!TestTrue(TEXT("L1 to L2 transition commits new source row"),
		Ledger.TransitionLevel(Farm.Id, LevelOne, LevelTwo, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Completion instant retains old changeTime"), Ledger.Preview(Farm.Id, 4600, Preview, Error));
	TestEqual(TEXT("Retained hour is evaluated at L2 current rate"), Preview.Accrued, int64(430));
	TestFalse(TEXT("Duplicate transition cannot apply twice"),
		Ledger.TransitionLevel(Farm.Id, LevelOne, LevelTwo, Error));
	TestTrue(TEXT("L2 cap preview"), Ledger.Preview(Farm.Id, 100000, Preview, Error));
	TestEqual(TEXT("L2 cap comes from new source row"), Preview.Accrued, int64(4300));

	FRokLocalConstructionResources Wallet{5000, 5000, 1500, 600};
	int64 Collected = 0;
	Wallet.Food = MAX_int32 - 400;
	TestFalse(TEXT("Overflow cannot credit L2 production"),
		Ledger.TryCollect(Farm.Id, 4600, Wallet, Collected, Error));
	TestEqual(TEXT("Overflow preserves wallet"), Wallet.Food, MAX_int32 - 400);
	TestTrue(TEXT("Overflow preserves production clock"), Ledger.Preview(Farm.Id, 4600, Preview, Error));
	TestEqual(TEXT("Overflow retains L2 accrual"), Preview.Accrued, int64(430));
	Wallet.Food = 5000;
	TestTrue(TEXT("Only explicit collect credits the L2 amount"),
		Ledger.TryCollect(Farm.Id, 4600, Wallet, Collected, Error));
	TestEqual(TEXT("L2 collect credits food"), Wallet.Food, 5430);
	TestEqual(TEXT("L2 collect leaves wood unchanged"), Wallet.Wood, 5000);
	TestEqual(TEXT("Credit is source-floor amount"), Collected, int64(430));
	TestFalse(TEXT("Same-second repeat collect is rejected"),
		Ledger.TryCollect(Farm.Id, 4600, Wallet, Collected, Error));
	TestEqual(TEXT("Repeat collect cannot credit again"), Wallet.Food, 5430);
	TestTrue(TEXT("Collect reset starts zero at same tick"), Ledger.Preview(Farm.Id, 4600, Preview, Error));
	TestEqual(TEXT("Clock reset only on collect"), Preview.Accrued, int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalResourceRetainedClockTest,
	"Rok.Runtime.LocalCity.ResourceLevelTransitionRetainsCollectedClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLocalResourceRetainedClockTest::RunTest(const FString&)
{
	FRokLocalProductionLedger Ledger;
	FRokLocalConstructionTicket Sawmill;
	Sawmill.Id = 42;
	Sawmill.TypeId = 3;
	Sawmill.FinishesAt = 1000.0;
	Sawmill.bCompleted = true;
	const FRokLocalProductionSource LevelOne{3, 1, 400, 66, 4000};
	const FRokLocalProductionSource LevelTwo{3, 2, 430, 71, 4300};
	FString Error;
	if (!TestTrue(TEXT("Sawmill starts L1 production"),
		Ledger.RegisterCompleted(Sawmill, LevelOne, 1000, Error)))
	{
		AddError(Error); return false;
	}
	FRokLocalConstructionResources Wallet{5000, 5000, 1500, 600};
	int64 Collected = 0;
	TestTrue(TEXT("L1 collection at 594 elapsed seconds"),
		Ledger.TryCollect(Sawmill.Id, 1594, Wallet, Collected, Error));
	TestEqual(TEXT("L1 wood credit"), Wallet.Wood, 5066);
	TestEqual(TEXT("L1 food unchanged"), Wallet.Food, 5000);
	TestTrue(TEXT("Upgrade retains the prior collection timestamp"),
		Ledger.TransitionLevel(Sawmill.Id, LevelOne, LevelTwo, Error));
	FRokCityResourceProductionResult Preview;
	TestTrue(TEXT("Six elapsed seconds remain after transition"),
		Ledger.Preview(Sawmill.Id, 1600, Preview, Error));
	TestEqual(TEXT("No phantom credit at L2 completion"), Preview.Accrued, int64(0));
	TestEqual(TEXT("Transition alone did not credit wood"), Wallet.Wood, 5066);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalResourceTargetEconomyParseTest,
	"Rok.Runtime.LocalCity.ResourceUpgradeTargetEconomyStrictParse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLocalResourceTargetEconomyParseTest::RunTest(const FString&)
{
	const auto MakeTargetRow = []()
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("type"), 2);
		Row->SetNumberField(TEXT("level"), 2);
		Row->SetNumberField(TEXT("food"), 0);
		Row->SetNumberField(TEXT("wood"), 200);
		Row->SetNumberField(TEXT("stone"), 0);
		Row->SetNumberField(TEXT("coin"), 0);
		Row->SetNumberField(TEXT("buildingTime"), 15);
		Row->SetNumberField(TEXT("power"), 11);
		return Row;
	};
	FRokUnityBuildingLevelConfig Config;
	Config.TypeId = 2;
	Config.Level = 2;
	TSharedPtr<FJsonObject> Row = MakeTargetRow();
	TestTrue(TEXT("Source L2 row accepts legitimate zero components"), Config.ParseSourceEconomy(Row));
	TestEqual(TEXT("Source wood cost parsed exactly"), Config.Cost.Wood, 200);
	TestEqual(TEXT("Source duration parsed exactly"), Config.BuildSeconds, 15);
	Row->SetNumberField(TEXT("wood"), -200);
	TestFalse(TEXT("Negative cost cannot become a wallet credit"), Config.ParseSourceEconomy(Row));
	TestFalse(TEXT("Invalid row clears eligibility"), Config.bSourceEconomyValid);
	TestEqual(TEXT("Rejected parse preserves last valid cost"), Config.Cost.Wood, 200);
	Row = MakeTargetRow();
	Row->SetNumberField(TEXT("buildingTime"), 0);
	TestFalse(TEXT("Zero duration cannot become a one-second upgrade"), Config.ParseSourceEconomy(Row));
	Row = MakeTargetRow();
	Row->RemoveField(TEXT("buildingTime"));
	TestFalse(TEXT("Missing duration cannot be defaulted"), Config.ParseSourceEconomy(Row));
	Row = MakeTargetRow();
	Row->SetNumberField(TEXT("wood"), 200.5);
	TestFalse(TEXT("Fractional cost cannot round into a debit"), Config.ParseSourceEconomy(Row));
	Row = MakeTargetRow();
	Row->SetStringField(TEXT("wood"), TEXT("2147483648"));
	TestFalse(TEXT("Out-of-range cost cannot narrow to int32"), Config.ParseSourceEconomy(Row));
	Row = MakeTargetRow();
	Row->SetNumberField(TEXT("type"), 2.5);
	TestFalse(TEXT("Fractional type cannot round to resource type"), Config.ParseSourceEconomy(Row));
	TestEqual(TEXT("Rejected rows do not replace valid duration"), Config.BuildSeconds, 15);
	return true;
}
#endif
