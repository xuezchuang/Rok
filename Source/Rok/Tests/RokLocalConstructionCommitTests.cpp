#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../RokBuildingUpgradeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalConstructionTransactionTest,
	"Rok.Runtime.LocalConstruction.FarmWoodAtomicReservationAndOnceCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalConstructionTransactionTest::RunTest(const FString&)
{
	FRokCityConstructionGrid Grid;
	FString Error;
	if (!TestTrue(TEXT("Verified source grid loads"), Grid.Load(Error)))
	{
		AddError(Error); return false;
	}
	TArray<FRokCityConstructionFootprint> Empty;
	FRokCityConstructionFootprint FarmFootprint;
	FRokCityConstructionFootprint WoodFootprint;
	for (int32 X = -20; X <= 15; ++X)
	{
		for (int32 Y = -20; Y <= 15; ++Y)
		{
			const FRokCityConstructionFootprint Candidate{FIntPoint(X, Y), FIntPoint(5, 5)};
			if (!Grid.CheckFootprint(1, Candidate, Empty, Error)) continue;
			if (!FarmFootprint.IsValid()) FarmFootprint = Candidate;
			else if (!FarmFootprint.Overlaps(Candidate))
			{
				WoodFootprint = Candidate;
				break;
			}
		}
		if (WoodFootprint.IsValid()) break;
	}
	if (!TestTrue(TEXT("Two independent age-one 5x5 source rectangles exist"),
		FarmFootprint.IsValid() && WoodFootprint.IsValid())) return false;

	FRokLocalConstructionLedger Ledger;
	FRokLocalConstructionResources Wallet{500, 500, 0, 0};
	FRokLocalConstructionTicket Farm;
	Farm.TypeId = 2; Farm.Civilization = 101; Farm.Age = 1;
	Farm.Footprint = FarmFootprint; Farm.Cost = {100, 120, 0, 0};
	Farm.Power = 5; Farm.StartedAt = 10.0; Farm.FinishesAt = 16.0;
	uint64 FarmId = 0;
	if (!TestTrue(TEXT("Farm reserves one local queue slot and debits once"),
		Ledger.TryBegin(Grid, Farm, 0, 1, 0, 2, Wallet, FarmId, Error)))
	{
		AddError(Error); return false;
	}
	TestTrue(TEXT("Local ticket ID is nonzero"), FarmId != 0);
	TestEqual(TEXT("Exact food debit"), Wallet.Food, 400);
	TestEqual(TEXT("Exact wood debit"), Wallet.Wood, 380);
	FRokLocalConstructionTicket Wood = Farm;
	Wood.TypeId = 3; Wood.Footprint = WoodFootprint;
	Wood.Cost = {80, 90, 0, 0};
	Wood.StartedAt = 17.0; Wood.FinishesAt = 19.0;
	uint64 WoodId = 0;
	TestFalse(TEXT("Farm shares the only builder queue with Wood"),
		Ledger.TryBegin(Grid, Wood, 0, 1, 0, 2, Wallet, WoodId, Error));
	TestEqual(TEXT("Rejected command cannot debit food"), Wallet.Food, 400);
	TestEqual(TEXT("Rejected command cannot reserve a second ID"), WoodId, uint64(0));
	TArray<FRokLocalConstructionTicket> Completed;
	Ledger.CompleteDue(15.99, Completed);
	TestEqual(TEXT("Construction is not completed early"), Completed.Num(), 0);
	Ledger.CompleteDue(16.0, Completed);
	TestEqual(TEXT("Due Farm completes exactly once"), Completed.Num(), 1);
	Ledger.CompleteDue(17.0, Completed);
	TestEqual(TEXT("Repeated clock tick cannot repeat completion"), Completed.Num(), 0);
	TArray<FRokCityConstructionFootprint> Reserved;
	Reserved.Add(FarmFootprint);
	TestFalse(TEXT("Completed Farm footprint remains reserved"),
		Grid.CheckFootprint(1, FarmFootprint, Reserved, Error));
	TestTrue(TEXT("Queue is free after completion"), Ledger.GetActiveCount() == 0);
	TestTrue(TEXT("Wood can now reserve its own source rectangle"),
		Ledger.TryBegin(Grid, Wood, 0, 1, 0, 2, Wallet, WoodId, Error));
	TestTrue(TEXT("Wood receives a different local ID"), WoodId != 0 && WoodId != FarmId);
	TestEqual(TEXT("One debit per accepted command"), Wallet.Food, 320);
	TestEqual(TEXT("One debit per accepted command"), Wallet.Wood, 290);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalConstructionWorldGateTest,
	"Rok.Runtime.LocalConstruction.UnverifiedWorldCannotCreateOrSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalConstructionWorldGateTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalConstructionWorldGate"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Standalone fixture world"), World)) return false;
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("World-scoped building subsystem"), Buildings)) return false;
	const FRokResourceStock Before = Buildings->GetCurrentResources();
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = 2;
	Request.CivilizationId = 101;
	Request.Age = 1;
	Request.Grid = FIntPoint(-15, 10);
	uint64 Id = 123;
	FString Failure;
	TestFalse(TEXT("An unverified world cannot create a Farm source actor or ticket"),
		Buildings->StartLocalConstruction(Request, Id, Failure));
	TestEqual(TEXT("Denied world issues no local ID"), Id, uint64(0));
	TestEqual(TEXT("Denied world reserves no worker"), Buildings->GetActiveLocalConstructionCount(), 0);
	TestNull(TEXT("Denied world has no constructed actor"), Buildings->GetLocalConstructionActor(1));
	TestFalse(TEXT("Denied world has no pending construction"), Buildings->IsLocalConstructionPending(1));
	TestFalse(TEXT("Denied world has no completion error for a nonexistent ticket"),
		Buildings->GetLocalConstructionCompletionFailure(1, Failure));
	const FRokResourceStock After = Buildings->GetCurrentResources();
	TestTrue(TEXT("Denied world leaves all wallet resources unchanged"),
		Before.Food == After.Food && Before.Wood == After.Wood
		&& Before.Stone == After.Stone && Before.Gold == After.Gold);
	return true;
}
#endif
