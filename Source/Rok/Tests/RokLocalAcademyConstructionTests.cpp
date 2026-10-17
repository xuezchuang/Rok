#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalAcademyGridTest,
	"Rok.Runtime.LocalAcademy.SourceGridAndAtomicLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalAcademyGridTest::RunTest(const FString&)
{
	FRokCityConstructionGrid Grid;
	FString Error;
	if (!TestTrue(TEXT("Five source age maps and 26 initial footprints load"), Grid.Load(Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("Unity index has its unusual +1 x offset"),
		FRokCityConstructionGrid::SourceCellIndex(41, -20, -20), 1);
	TestEqual(TEXT("Hall1 uses age1"), Grid.GetAgeForTownHallLevel(1), 1);
	TestEqual(TEXT("Hall4 uses age2"), Grid.GetAgeForTownHallLevel(4), 2);
	TestEqual(TEXT("Source tracks all InitBuilding rows including two empty footprints"), Grid.GetInitialOccupancy().Num(), 26);
	const FRokCityConstructionFootprint Free{FIntPoint(-20, 7), FIntPoint(6, 6)};
	const TArray<FRokCityConstructionFootprint> None;
	TArray<int32> Indices;
	TestTrue(TEXT("Known six-cell academy rectangle is legal in age2"), Grid.CheckFootprint(2, Free, None, Error, &Indices));
	TestEqual(TEXT("Academy reserves 36 source cells"), Indices.Num(), 36);
	TestFalse(TEXT("Initial builder hut footprint blocks occupancy"), Grid.CheckFootprint(2,
		FRokCityConstructionFootprint{FIntPoint(4, -2), FIntPoint(6, 6)}, None, Error));
	TestFalse(TEXT("Out-of-bounds rectangle is rejected"), Grid.CheckFootprint(2,
		FRokCityConstructionFootprint{FIntPoint(21, 20), FIntPoint(6, 6)}, None, Error));
	const FRokCityConstructionFootprint Extreme{FIntPoint(MAX_int32 - 2, 7), FIntPoint(6, 6)};
	TestTrue(TEXT("Overlap endpoints use wide arithmetic"), Extreme.Overlaps(
		FRokCityConstructionFootprint{FIntPoint(MAX_int32 - 1, 7), FIntPoint(2, 6)}));
	TestFalse(TEXT("Integer-max origin cannot wrap into a legal grid interval"),
		Grid.CheckFootprint(2, Extreme, None, Error));
	TestFalse(TEXT("Integer-max size cannot skip validation through loop overflow"),
		Grid.CheckFootprint(2,
			FRokCityConstructionFootprint{FIntPoint(0, 0), FIntPoint(MAX_int32, 6)}, None, Error));
	FRokLocalConstructionResources Scarce{100, 100, 0, 0};
	FRokLocalConstructionLedger ScarceLedger;
	FRokLocalConstructionTicket ScarceTicket;
	ScarceTicket.TypeId = 13; ScarceTicket.Civilization = 101; ScarceTicket.Age = 2;
	ScarceTicket.Footprint = Free;
	ScarceTicket.Cost = FRokLocalConstructionResources{500, 800, 0, 0};
	ScarceTicket.StartedAt = 10.0; ScarceTicket.FinishesAt = 16.0;
	uint64 ScarceId = 0;
	TestFalse(TEXT("Insufficient fixture stock cannot produce a ticket"),
		ScarceLedger.TryBegin(Grid, ScarceTicket, 0, 1, 0, 1, Scarce, ScarceId, Error));
	TestEqual(TEXT("Failed affordability does not debit food"), Scarce.Food, 100);
	TestEqual(TEXT("Failed affordability leaves ID unassigned"), ScarceId, uint64(0));
	TArray<FRokCityConstructionFootprint> Reserved{Free};
	TestFalse(TEXT("Local pending footprint blocks another preview"), Grid.CheckFootprint(2, Free, Reserved, Error));

	FRokLocalConstructionLedger Ledger;
	FRokLocalConstructionResources Resources{5000, 5000, 1500, 600};
	FRokLocalConstructionTicket Ticket;
	Ticket.TypeId = 13; Ticket.Civilization = 101; Ticket.Age = 2;
	Ticket.Footprint = Free;
	Ticket.Cost = FRokLocalConstructionResources{500, 800, 0, 0};
	Ticket.Power = 5; Ticket.StartedAt = 10.0; Ticket.FinishesAt = 16.0;
	uint64 Id = 0;
	FRokLocalConstructionTicket Invalid = Ticket;
	Invalid.Footprint = Extreme;
	TestFalse(TEXT("Invalid extreme tile cannot debit fixture resources"),
		Ledger.TryBegin(Grid, Invalid, 0, 1, 0, 1, Resources, Id, Error));
	TestEqual(TEXT("Extreme tile leaves food unchanged"), Resources.Food, 5000);
	Invalid = Ticket; Invalid.StartedAt = -1.0;
	TestFalse(TEXT("Negative construction timestamp is invalid"),
		Ledger.TryBegin(Grid, Invalid, 0, 1, 0, 1, Resources, Id, Error));
	Invalid = Ticket; Invalid.Power = -1;
	TestFalse(TEXT("Negative source power is invalid"),
		Ledger.TryBegin(Grid, Invalid, 0, 1, 0, 1, Resources, Id, Error));
	TestFalse(TEXT("Int-max active queue count cannot overflow past capacity"),
		Ledger.TryBegin(Grid, Ticket, MAX_int32, 1, 0, 1, Resources, Id, Error));
	TestFalse(TEXT("One active upgrade shares and occupies the only builder queue"),
		Ledger.TryBegin(Grid, Ticket, 1, 1, 0, 1, Resources, Id, Error));
	TestEqual(TEXT("Rejected queue does not spend food"), Resources.Food, 5000);
	if (!TestTrue(TEXT("Source-valid isolated fixture begins exactly one local construction"),
		Ledger.TryBegin(Grid, Ticket, 0, 1, 0, 1, Resources, Id, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("First local ID is independent of UnityInitId"), Id, uint64(1));
	TestEqual(TEXT("Exact academy food debit"), Resources.Food, 4500);
	TestEqual(TEXT("Exact academy wood debit"), Resources.Wood, 4200);
	TestEqual(TEXT("Other resource unchanged"), Resources.Stone, 1500);
	TestEqual(TEXT("Queued construction uses one worker"), Ledger.GetActiveCount(), 1);
	uint64 DuplicateId = 0;
	TestFalse(TEXT("Repeated confirmation cannot double debit or reserve"),
		Ledger.TryBegin(Grid, Ticket, 0, 1, 0, 1, Resources, DuplicateId, Error));
	TestEqual(TEXT("Repeat emits no ID"), DuplicateId, uint64(0));
	TestEqual(TEXT("Repeat leaves money intact"), Resources.Food, 4500);
	TArray<FRokLocalConstructionTicket> Completed;
	Ledger.CompleteDue(15.99, Completed);
	TestEqual(TEXT("Six-second queue has not completed early"), Completed.Num(), 0);
	Ledger.CompleteDue(16.0, Completed);
	TestEqual(TEXT("Six-second queue completes once"), Completed.Num(), 1);
	TestTrue(TEXT("Completed ticket retains local ID"), Completed.Num() == 1 && Completed[0].Id == Id);
	Ledger.CompleteDue(17.0, Completed);
	TestEqual(TEXT("Completion is idempotent"), Completed.Num(), 0);
	TestEqual(TEXT("Completed work releases builder queue"), Ledger.GetActiveCount(), 0);
	TestFalse(TEXT("Completed academy still consumes Hall4 count limit"),
		Ledger.TryBegin(Grid, Ticket, 0, 1, 0, 1, Resources, DuplicateId, Error));
	TestEqual(TEXT("Count denial does not spend resources"), Resources.Wood, 4200);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalAcademyCurrentAccountTest,
	"Rok.Runtime.LocalAcademy.CurrentAccountRemainsBlocked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalAcademyCurrentAccountTest::RunTest(const FString&)
{
	FRokLocalCitySimulationContext Context;
	FString ContextError;
	if (!TestTrue(TEXT("Source-hashed local city context loads"), Context.Load(ContextError)))
	{
		AddError(ContextError); return false;
	}
	TestEqual(TEXT("Visual variant is explicit local fixture, not a player civilization"),
		Context.GetVisualCivilization(), 101);
	TestEqual(TEXT("Initial source hall remains level one"), Context.GetInitialHallLevel(), 1);
	TestTrue(TEXT("Initial map age is supported"), Context.SupportsAge(1));
	TestFalse(TEXT("Hall4 config age lacks verified local map conversion"), Context.SupportsAge(2));
	FString LevelJson;
	const FString LevelPath = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/ConfigTables/BuildingLevelData.json");
	if (!TestTrue(TEXT("Source building level rows exist"), FFileHelper::LoadFileToString(LevelJson, *LevelPath)))
		return false;
	TSharedPtr<FJsonObject> LevelRoot;
	if (!TestTrue(TEXT("Source building level rows parse"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LevelJson), LevelRoot) && LevelRoot.IsValid()))
		return false;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!TestTrue(TEXT("Source level table has rows"), LevelRoot->TryGetArrayField(TEXT("rows"), Rows) && Rows))
		return false;
	bool bAcademyPrerequisiteFound = false;
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		double Type = 0.0, Level = 0.0;
		if (!Row.IsValid() || !Row->TryGetNumberField(TEXT("type"), Type)
			|| !Row->TryGetNumberField(TEXT("level"), Level) || Type != 13.0 || Level != 1.0) continue;
		FRokUnityBuildingLevelConfig Academy;
		if (!TestTrue(TEXT("Academy L1 source prerequisites parse"), Academy.ParsePrerequisites(Row))) return false;
		TMap<int32, int32> CompletedLevels;
		CompletedLevels.Add(1, Context.GetInitialHallLevel());
		FRokUnityBuildingPrerequisite Missing;
		int32 CurrentLevel = 0;
		TestFalse(TEXT("Source initial Hall1 cannot build Academy"),
			Academy.CheckPrerequisites(CompletedLevels, Missing, CurrentLevel));
		TestEqual(TEXT("Source missing building is Hall type1"), Missing.TypeId, 1);
		TestEqual(TEXT("Academy source requires Hall4"), Missing.Level, 4);
		TestEqual(TEXT("Initial Hall is still level1"), CurrentLevel, 1);
		bAcademyPrerequisiteFound = true;
		break;
	}
	TestTrue(TEXT("Academy first-level source row was found"), bAcademyPrerequisiteFound);
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalAcademyCurrentAccount"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Actual world-scoped building ledger"), Buildings)) return false;
	const auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!TestNotNull(TEXT("Runtime profile source"), Runtime)) return false;
	const FRokUnityRuntimePlayerProfileState Profile = Runtime->GetPlayerProfileState();
	TestEqual(TEXT("Runtime remains an isolated preview"), Profile.StateKind, FString(TEXT("local_preview")));
	TestFalse(TEXT("Local visual variant does not set account civilization"), Profile.bHasCivilization);
	TestEqual(TEXT("Unknown account civilization remains zero"), Profile.CivilizationId, 0);
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = 13;
	Request.CivilizationId = 101; // Caller assertion cannot prove world or account identity.
	Request.Age = 1;
	Request.Grid = FIntPoint(-20, 7);
	const FRokResourceStock Before = Buildings->GetCurrentResources();
	FRokLocalConstructionPreview Preview;
	TestFalse(TEXT("Standalone non-city world cannot claim a verified map context"),
		Buildings->PreviewLocalConstruction(Request, Preview));
	TestFalse(TEXT("Unverified world cannot claim source construction eligibility"), Preview.bSourceEligible);
	TestFalse(TEXT("Unverified world cannot claim context validation"), Preview.bContextVerified);
	TestFalse(TEXT("No presentation-ready commit exists"), Preview.bCanCommit);
	TestTrue(TEXT("Reason identifies map context mismatch"), Preview.FailureReason.Contains(TEXT("map mismatch")));
	FRokLocalConstructionPreview Entry;
	TestFalse(TEXT("Card preflight also refuses a non-city world"),
		Buildings->PreviewLocalConstructionEntry(2, Entry));
	uint64 Id = 99;
	FString Failure;
	TestFalse(TEXT("Direct command cannot bypass blocked preview"), Buildings->StartLocalConstruction(Request, Id, Failure));
	TestEqual(TEXT("Denied command assigns no local ID"), Id, uint64(0));
	TestEqual(TEXT("Denied command creates no local ticket"), Buildings->GetActiveLocalConstructionCount(), 0);
	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("UnownedAcademyTagFixture");
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Fake = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
	if (!TestNotNull(TEXT("Unowned visual fixture"), Fake)) return false;
	Fake->Tags.Add(TEXT("UnityBuildingType_13"));
	Fake->Tags.Add(TEXT("UnityBuildingLevel_25"));
	TestEqual(TEXT("Loose Academy visual tag cannot claim ownership"),
		Buildings->GetBuildingStateForActor(Fake).UnityBuildingTypeId, 0);
	const FRokResourceStock After = Buildings->GetCurrentResources();
	TestTrue(TEXT("Preview and denied command preserve full resource stock"),
		Before.Food == After.Food && Before.Wood == After.Wood && Before.Stone == After.Stone && Before.Gold == After.Gold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalResourceProductionLedgerTest,
	"Rok.Runtime.LocalAcademy.CompletedResourceProductionAtomicCollection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalResourceProductionLedgerTest::RunTest(const FString&)
{
	FRokLocalProductionLedger Production;
	FRokLocalConstructionTicket Farm;
	Farm.Id = 7; Farm.TypeId = 2; Farm.FinishesAt = 10.0; Farm.bCompleted = true;
	FRokLocalProductionSource FarmRate{2, 1, 400, 66, 4000};
	FString Error;
	TestFalse(TEXT("Clock cannot begin before source completion"),
		Production.RegisterCompleted(Farm, FarmRate, 9, Error));
	TestFalse(TEXT("Rejected clock has no entry"), Production.HasClock(Farm.Id));
	if (!TestTrue(TEXT("Completed farm starts base-rate clock"),
		Production.RegisterCompleted(Farm, FarmRate, 10, Error)))
	{
		AddError(Error); return false;
	}
	TestFalse(TEXT("Duplicate registration cannot restart production clock"),
		Production.RegisterCompleted(Farm, FarmRate, 10, Error));
	FRokCityResourceProductionResult Preview;
	TestTrue(TEXT("Read-only preview before source gather threshold"),
		Production.Preview(Farm.Id, 603, Preview, Error));
	TestFalse(TEXT("65 units cannot be collected"), Preview.bCanCollect);
	TestEqual(TEXT("Production floors the displayed amount"), Preview.Accrued, int64(65));
	FRokLocalConstructionResources Wallet{5000, 5000, 1500, 600};
	int64 Collected = -1;
	TestFalse(TEXT("Early collection does not credit wallet"),
		Production.TryCollect(Farm.Id, 603, Wallet, Collected, Error));
	TestEqual(TEXT("Early collection keeps food unchanged"), Wallet.Food, 5000);
	TestEqual(TEXT("Early collection emits zero credit"), Collected, int64(0));
	TestTrue(TEXT("Threshold collection credits exactly 66 food"),
		Production.TryCollect(Farm.Id, 604, Wallet, Collected, Error));
	TestEqual(TEXT("Collected amount is source base rate"), Collected, int64(66));
	TestEqual(TEXT("Collected farm output shares construction wallet"), Wallet.Food, 5066);
	TestFalse(TEXT("Reentrant same-second collection cannot double credit"),
		Production.TryCollect(Farm.Id, 604, Wallet, Collected, Error));
	TestEqual(TEXT("Repeated collection leaves wallet unchanged"), Wallet.Food, 5066);
	TestEqual(TEXT("Repeated collection emits zero"), Collected, int64(0));
	TestTrue(TEXT("Reset clock has no accrued output"), Production.Preview(Farm.Id, 604, Preview, Error));
	TestEqual(TEXT("Reset clock starts at zero"), Preview.Accrued, int64(0));
	FRokLocalConstructionTicket Wood = Farm;
	Wood.Id = 8; Wood.TypeId = 3;
	FRokLocalProductionSource WoodRate{3, 1, 400, 66, 4000};
	TestTrue(TEXT("Completed sawmill starts separate wood clock"),
		Production.RegisterCompleted(Wood, WoodRate, 10, Error));
	Wallet.Wood = MAX_int32 - 50;
	TestFalse(TEXT("Wallet overflow rejects collection atomically"),
		Production.TryCollect(Wood.Id, 604, Wallet, Collected, Error));
	TestEqual(TEXT("Overflow keeps previous wood"), Wallet.Wood, MAX_int32 - 50);
	Wallet.Wood = 5000;
	TestTrue(TEXT("Failed overflow does not reset source clock"),
		Production.TryCollect(Wood.Id, 604, Wallet, Collected, Error));
	TestEqual(TEXT("Sawmill credits the wood field only"), Wallet.Wood, 5066);
	TestEqual(TEXT("Sawmill does not credit food"), Wallet.Food, 5066);
	return true;
}
#endif
