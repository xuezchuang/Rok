#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "../RokServerArmySubsystem.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include <initializer_list>

namespace
{
TSharedPtr<FJsonObject> Row(std::initializer_list<TPair<FString, int64>> Fields)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	for (const auto& Field : Fields) Result->SetNumberField(Field.Key, double(Field.Value));
	return Result;
}

TArray<TSharedPtr<FJsonValue>> ArrayOf(std::initializer_list<TSharedPtr<FJsonObject>> Items)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const auto& Item : Items) Result.Add(MakeShared<FJsonValueObject>(Item));
	return Result;
}

TSharedPtr<FJsonObject> CityRole()
{
	TSharedPtr<FJsonObject> Role = MakeShared<FJsonObject>();
	Role->SetNumberField(TEXT("country"), 101);
	Role->SetArrayField(TEXT("soldiers"), ArrayOf({
		Row({{TEXT("id"), 10301}, {TEXT("type"), 3}, {TEXT("level"), 1}, {TEXT("num"), 1020}, {TEXT("minor"), 0}}),
		Row({{TEXT("id"), 10101}, {TEXT("type"), 1}, {TEXT("level"), 1}, {TEXT("num"), 3540}, {TEXT("minor"), 0}}),
		Row({{TEXT("id"), 10201}, {TEXT("type"), 2}, {TEXT("level"), 1}, {TEXT("num"), 20}, {TEXT("minor"), 0}})}));
	Role->SetArrayField(TEXT("armyQueue"), ArrayOf({
		Row({{TEXT("queueIndex"), 1}, {TEXT("finishTime"), -1}, {TEXT("buildingIndex"), 0},
			{TEXT("armyType"), 0}, {TEXT("armyNum"), 0}, {TEXT("beginTime"), 0}}),
		Row({{TEXT("queueIndex"), 3}})}));
	return Role;
}

TSharedPtr<FJsonObject> Buildings()
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("5"), Row({{TEXT("buildingIndex"), 5}, {TEXT("type"), 1},
		{TEXT("level"), 2}, {TEXT("finishTime"), -1}}));
	Result->SetObjectField(TEXT("30"), Row({{TEXT("buildingIndex"), 30}, {TEXT("type"), 11},
		{TEXT("level"), 1}, {TEXT("finishTime"), -1}}));
	return Result;
}

TSharedPtr<FJsonObject> Scouts(bool bActive)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("1"), Row({{TEXT("scoutsIndex"), 1},
		{TEXT("scoutsStatus"), bActive ? (1 << 12) : (1 << 14)}}));
	return Result;
}

TSharedPtr<FJsonObject> Army(int32 Index, int32 Hero, int64 Status,
	std::initializer_list<TSharedPtr<FJsonObject>> Soldiers, int64 Arrival = 0)
{
	TSharedPtr<FJsonObject> Result = Row({{TEXT("armyIndex"), Index}, {TEXT("mainHeroId"), Hero},
		{TEXT("status"), Status}, {TEXT("startTime"), 100}, {TEXT("arrivalTime"), Arrival}});
	Result->SetArrayField(TEXT("soldiers"), ArrayOf(Soldiers));
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerArmyProjectionTest,
	"Rok.Net.ServerArmy.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerArmyProjectionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerArmy_%s/L_RokServerArmy"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Instance->InitializeStandalone(TEXT("L_RokServerArmy"), NewObject<UPackage>(nullptr, PackageName, RF_Transient));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Catalog"), Catalog) || !TestTrue(TEXT("Catalog loaded"), Catalog->IsCatalogLoaded())) return false;
	TMap<int32, int32> TownHallSlots;
	TownHallSlots.Add(1, 1); TownHallSlots.Add(2, 1);
	TMap<int32, TArray<int32>> FeatureArms;
	TArray<int32> FeatureIds = {20141, 20231, 20232};
	FeatureArms.Add(101, FeatureIds);
	const auto Project = [&](const TSharedPtr<FJsonObject>& Role, const TSharedPtr<FJsonObject>& Armies,
		const TSharedPtr<FJsonObject>& Transport, const TSharedPtr<FJsonObject>& Scout,
		bool bClock = true, double ServerSeconds = 150.0)
	{
		return URokServerArmySubsystem::ProjectSnapshot(Role, Armies, Transport, Scout,
			Buildings(), Catalog, TownHallSlots, FeatureArms, bClock, ServerSeconds);
	};
	const TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
	const FRokServerArmySnapshot Recorded = Project(CityRole(), Empty, Empty, Scouts(false));
	TestTrue(TEXT("RoleReady available"), Recorded.bAvailable);
	TestTrue(TEXT("Role loaded"), Recorded.bLoaded);
	TestTrue(TEXT("City stock known"), Recorded.bCityKnown);
	TestTrue(TEXT("Empty army push is known"), Recorded.bArmiesKnown);
	TestTrue(TEXT("Training collection known"), Recorded.bTrainingKnown);
	TestTrue(TEXT("Server building collection known"), Recorded.bBuildingsKnown);
	TestEqual(TEXT("Only training facility building view"), Recorded.TrainingBuildings.Num(), 1);
	TestEqual(TEXT("Archery range building index"), Recorded.TrainingBuildings[0].BuildingIndex, 30);
	TestEqual(TEXT("Archery range level"), Recorded.TrainingBuildings[0].Level, 1);
	TestFalse(TEXT("Source training building name"), Recorded.TrainingBuildings[0].Name.IsEmpty());
	TestTrue(TEXT("Archery range membership"), Recorded.KnownTrainingBuildingIndices.Contains(30));
	TestFalse(TEXT("Town hall is not a training building"), Recorded.KnownTrainingBuildingIndices.Contains(5));
	TestEqual(TEXT("Recorded city total"), Recorded.CityCount, int64(4580));
	TestEqual(TEXT("No invented field soldiers"), Recorded.FieldCount, int64(0));
	TestEqual(TEXT("Recorded combined total"), Recorded.TotalCount, int64(4580));
	TestTrue(TEXT("Recorded total count known"), Recorded.bTotalCountKnown);
	TestEqual(TEXT("Recorded troop power"), Recorded.TotalPower, int64(4580));
	TestTrue(TEXT("Troop power config known"), Recorded.bTotalPowerKnown);
	TestEqual(TEXT("Three troop families"), Recorded.CitySoldiers.Num(), 3);
	TestEqual(TEXT("First family ID"), Recorded.CitySoldiers[0].Id, 10101);
	TestEqual(TEXT("Second family count"), Recorded.CitySoldiers[1].Count, int64(20));
	TestEqual(TEXT("Third family count"), Recorded.CitySoldiers[2].Count, int64(1020));
	TestEqual(TEXT("No live army"), Recorded.Marches.Num(), 0);
	TestTrue(TEXT("Dispatch source counts known"), Recorded.bDispatchUsedKnown);
	TestEqual(TEXT("Dispatch used zero"), Recorded.DispatchSlotsUsed, 0);
	TestTrue(TEXT("Town hall dispatch config known"), Recorded.bDispatchCapacityKnown);
	TestEqual(TEXT("Level two town hall max one"), Recorded.DispatchSlotsMax, 1);
	TestTrue(TEXT("Scout collection gives complete HUD queue"), Recorded.bHudQueueKnown);
	TestEqual(TEXT("Idle scout not active"), Recorded.ActiveScoutCount, 0);
	TestEqual(TEXT("HUD queue empty"), Recorded.HudQueueRowCount, 0);
	if (!TestEqual(TEXT("Only server-provided training rows"), Recorded.TrainingQueues.Num(), 2)) return false;
	TestEqual(TEXT("Idle queue"), Recorded.TrainingQueues[0].State, ERokServerTrainingState::Idle);
	TestEqual(TEXT("A queueIndex-only slot from BuildingLogic.initQueue is idle"),
		Recorded.TrainingQueues[1].State, ERokServerTrainingState::Idle);

	const FRokServerArmySnapshot MissingArmy = Project(CityRole(), nullptr, Empty, Scouts(false));
	TestFalse(TEXT("Missing army collection unknown"), MissingArmy.bArmiesKnown);
	TestFalse(TEXT("Missing army collection gives unknown used slots"), MissingArmy.bDispatchUsedKnown);
	TestNotEqual(TEXT("Missing vs empty signature"), MissingArmy.Signature, Recorded.Signature);
	const FRokServerArmySnapshot MissingCity = Project(MakeShared<FJsonObject>(), Empty, Empty, Scouts(false));
	TestFalse(TEXT("Missing soldiers collection unknown"), MissingCity.bCityKnown);
	TestFalse(TEXT("Missing training collection unknown"), MissingCity.bTrainingKnown);
	TestFalse(TEXT("Missing training collection makes total unknown"), MissingCity.bTotalCountKnown);
	const FRokServerArmySnapshot MissingTransport = Project(CityRole(), Empty, nullptr, Scouts(false));
	TestFalse(TEXT("Missing transport makes dispatch used unknown"), MissingTransport.bDispatchUsedKnown);
	const FRokServerArmySnapshot MissingScout = Project(CityRole(), Empty, Empty, nullptr);
	TestFalse(TEXT("Missing scouts collection makes HUD queue pending"), MissingScout.bHudQueueKnown);
	TSharedPtr<FJsonObject> KeyedRole = CityRole();
	TSharedPtr<FJsonObject> KeyedQueues = MakeShared<FJsonObject>();
	KeyedQueues->SetObjectField(TEXT("3"), Row({{TEXT("queueIndex"), 3}, {TEXT("finishTime"), -1}}));
	KeyedQueues->SetObjectField(TEXT("4"), Row({{TEXT("queueIndex"), 4}, {TEXT("buildingIndex"), 31}}));
	KeyedRole->SetObjectField(TEXT("armyQueue"), KeyedQueues);
	const FRokServerArmySnapshot Keyed = Project(KeyedRole, Empty, Empty, Scouts(false));
	if (!TestEqual(TEXT("Sparse keyed queue collection parsed"), Keyed.TrainingQueues.Num(), 2)) return false;
	TestEqual(TEXT("An incomplete bound queue stays unknown"), Keyed.TrainingQueues[1].State, ERokServerTrainingState::Unknown);

	TSharedPtr<FJsonObject> ActiveRole = CityRole();
	ActiveRole->SetArrayField(TEXT("armyQueue"), ArrayOf({
		Row({{TEXT("queueIndex"), 1}, {TEXT("buildingIndex"), 30}, {TEXT("armyType"), 1},
			{TEXT("newArmyLevel"), 2}, {TEXT("oldArmyLevel"), 1}, {TEXT("armyNum"), 20},
			{TEXT("beginTime"), 100}, {TEXT("finishTime"), 300}}),
		Row({{TEXT("queueIndex"), 3}, {TEXT("buildingIndex"), 30}, {TEXT("armyType"), 3},
			{TEXT("newArmyLevel"), 1}, {TEXT("oldArmyLevel"), 0}, {TEXT("armyNum"), 50},
			{TEXT("beginTime"), 100}, {TEXT("finishTime"), 200}})}));
	TSharedPtr<FJsonObject> ActiveArmies = MakeShared<FJsonObject>();
	ActiveArmies->SetObjectField(TEXT("1"), Army(1, 4000, int64(1) << 0,
		{Row({{TEXT("id"), 10101}, {TEXT("type"), 1}, {TEXT("level"), 1}, {TEXT("num"), 100}})}, 200));
	TSharedPtr<FJsonObject> ReturningArmy = Army(2, 4000, int64(1) << 13,
		{Row({{TEXT("id"), 10101}, {TEXT("type"), 1}, {TEXT("level"), 1}, {TEXT("num"), 20}}),
		Row({{TEXT("id"), 10201}, {TEXT("type"), 2}, {TEXT("level"), 1}, {TEXT("num"), 10}})}, 190);
	ReturningArmy->SetArrayField(TEXT("minorSoldiers"), ArrayOf({
		Row({{TEXT("id"), 10301}, {TEXT("type"), 3}, {TEXT("level"), 1}, {TEXT("num"), 5}})}));
	ActiveArmies->SetObjectField(TEXT("2"), ReturningArmy);
	ActiveArmies->SetObjectField(TEXT("3"), Army(3, 4000, int64(1) << 10,
		{Row({{TEXT("id"), 10301}, {TEXT("type"), 3}, {TEXT("level"), 1}, {TEXT("num"), 30}})}));
	ActiveArmies->SetObjectField(TEXT("4"), Row({{TEXT("armyIndex"), 4}, {TEXT("mainHeroId"), 0}}));
	TSharedPtr<FJsonObject> Transports = MakeShared<FJsonObject>();
	Transports->SetObjectField(TEXT("1"), Row({{TEXT("transportIndex"), 1}, {TEXT("targetObjectIndex"), 100}}));
	Transports->SetObjectField(TEXT("2"), Row({{TEXT("transportIndex"), 2}, {TEXT("targetObjectIndex"), -1}}));
	const FRokServerArmySnapshot Active = Project(ActiveRole, ActiveArmies, Transports, Scouts(true));
	TestEqual(TEXT("Three active armies; tombstone removed"), Active.Marches.Num(), 3);
	TestEqual(TEXT("March, return, garrison phases"), Active.Marches[0].Phase, ERokServerMarchPhase::Marching);
	TestEqual(TEXT("Returning phase"), Active.Marches[1].Phase, ERokServerMarchPhase::Returning);
	TestEqual(TEXT("Garrison phase"), Active.Marches[2].Phase, ERokServerMarchPhase::Garrisoned);
	TestEqual(TEXT("Field soldiers only once"), Active.FieldCount, int64(160));
	TestEqual(TEXT("Field minor soldiers separate"), Active.FieldMinorCount, int64(5));
	TestEqual(TEXT("Returning march minor count"), Active.Marches[1].MinorCount, int64(5));
	TestEqual(TEXT("Upgrade old tier soldiers retained"), Active.UpgradingCount, int64(20));
	TestEqual(TEXT("City+field+minor+upgrade total"), Active.TotalCount, int64(4765));
	TestEqual(TEXT("Total roster power includes upgrade, not field minor"), Active.TotalPower, int64(4760));
	TestTrue(TEXT("Total roster count known"), Active.bTotalCountKnown);
	TestEqual(TEXT("Repeated field troop ID aggregated"), Active.FieldSoldiers[0].Count, int64(120));
	int64 AllStackCount = 0;
	for (const FRokServerSoldierStack& Stack : Active.AllSoldiers) AllStackCount += Stack.Count;
	TestEqual(TEXT("All rows sum to inclusive total"), AllStackCount, Active.TotalCount);
	TestEqual(TEXT("One transport, tombstone excluded"), Active.TransportCount, 1);
	TestEqual(TEXT("Dispatch used includes armies+transport"), Active.DispatchSlotsUsed, 4);
	TestEqual(TEXT("Active scout counted for HUD only"), Active.ActiveScoutCount, 1);
	TestEqual(TEXT("HUD queue includes scout"), Active.HudQueueRowCount, 5);
	TestEqual(TEXT("Upgrade queue"), Active.TrainingQueues[0].State, ERokServerTrainingState::Upgrading);
	TestEqual(TEXT("Civilization special infantry tier two"), Active.TrainingQueues[0].TargetTroopId, 20141);
	TestEqual(TEXT("Training queue"), Active.TrainingQueues[1].State, ERokServerTrainingState::Training);
	TestEqual(TEXT("Training remaining"), Active.TrainingQueues[1].RemainingSeconds, 50.0);
	TestEqual(TEXT("Training duration"), Active.TrainingQueues[1].DurationSeconds, 100.0);
	TestEqual(TEXT("Training progress"), Active.TrainingQueues[1].Progress01, 0.5);
	const FRokServerArmySnapshot Later = Project(ActiveRole, ActiveArmies, Transports, Scouts(true), true, 151.0);
	TestEqual(TEXT("Clock-only countdown does not rebuild"), Later.Signature, Active.Signature);
	TestTrue(TEXT("Clock-only countdown still refreshes"), Later.TrainingQueues[1].RemainingSeconds < Active.TrainingQueues[1].RemainingSeconds);
	const FRokServerArmySnapshot Awaiting = Project(ActiveRole, ActiveArmies, Transports, Scouts(true), true, 200.0);
	TestEqual(TEXT("Timer expiry waits for server receipt"), Awaiting.TrainingQueues[1].State, ERokServerTrainingState::AwaitingReceive);
	TestEqual(TEXT("No local soldiers awarded on countdown zero"), Awaiting.TotalCount, Active.TotalCount);
	TestNotEqual(TEXT("Queue phase change rebuilds"), Awaiting.Signature, Active.Signature);
	ActiveRole->RemoveField(TEXT("country"));
	const FRokServerArmySnapshot UnknownCivilization = Project(ActiveRole, ActiveArmies, Transports, Scouts(true));
	TestEqual(TEXT("Unknown civilization cannot assert special target troop"), UnknownCivilization.TrainingQueues[0].TargetTroopId, 0);
	TestFalse(TEXT("Unknown upgrade old-tier power stays explicit"), UnknownCivilization.bTotalPowerKnown);

	URokServerArmySubsystem* Subsystem = Instance->GetSubsystem<URokServerArmySubsystem>();
	if (TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		Subsystem->SetSnapshotForTesting(Active);
		TestEqual(TEXT("Injected snapshot"), Subsystem->GetSnapshot().Signature, Active.Signature);
		Subsystem->ClearSnapshotForTesting();
		TestFalse(TEXT("Cleared test snapshot returns offline"), Subsystem->GetSnapshot().bAvailable);
	}
	return true;
}

#endif
