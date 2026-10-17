#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/Button.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokServerSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "../RokUnityRuntimeStateSubsystem.h"

namespace
{
struct FTaskGoSnapshot
{
	FRokBuildingState Hall;
	FRokResourceStock Wallet;
	FRokResourceRates Rates;
	FRokUnityRuntimeTaskState Task;
	int32 ActiveUpgrades = 0;
	int32 ActiveConstruction = 0;
	int32 OwnedQueues = 0;
	int32 PendingCompletionEvents = 0;
	int32 BroadcastEvents = 0;
	uint64 HttpDispatches = 0;
	uint64 HttpSuppressions = 0;
};

AActor* SpawnHall(UWorld* World, const TCHAR* Name)
{
	FActorSpawnParameters Parameters;
	Parameters.Name = FName(Name);
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
	if (Hall)
	{
		Hall->Tags.Add(TEXT("UnityInitId_5"));
		Hall->Tags.Add(TEXT("UnityBuildingType_1"));
		Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
		Hall->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));
	}
	return Hall;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMainTaskGoCachedIdleHallGuardTest,
	"Rok.Ui.TaskMain200900.CachedIdleHallPresentationGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMainTaskGoCachedIdleHallGuardTest::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("Rok local task Go blocked: task=200900 reason=hud_unavailable."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName CityPackageName(*FString::Printf(
		TEXT("/Temp/RokTaskGoFixture_%s/L_RokUnityGameAppCity"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UPackage* CityPackage = NewObject<UPackage>(nullptr, CityPackageName, RF_Transient);
	Instance->InitializeStandalone(TEXT("L_RokUnityGameAppCity"), CityPackage);
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Transient city-named world"), World)) return false;
	if (!TestTrue(TEXT("Fixture owns its explicit transient package"),
		World->GetOutermost() == CityPackage && CityPackage->HasAnyFlags(RF_Transient))) return false;
	if (!TestEqual(TEXT("Fixture uses the exact supported map name"),
		World->GetMapName(), FString(TEXT("L_RokUnityGameAppCity")))) return false;
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	auto* Server = Instance->GetSubsystem<URokServerSubsystem>();
	AActor* Hall = SpawnHall(World, TEXT("TaskGoGuardHall"));
	if (!TestNotNull(TEXT("Building subsystem"), Buildings)
		|| !TestNotNull(TEXT("Runtime subsystem"), Runtime)
		|| !TestNotNull(TEXT("Server subsystem"), Server)
		|| !TestNotNull(TEXT("Transient Hall"), Hall)) return false;
	FRokBuildingState Uncached;
	TestFalse(TEXT("Peek cannot create the Hall cache"),
		Buildings->TryPeekExistingBuildingStateForActor(Hall, Uncached));
	TestTrue(TEXT("Missing cache output has no Hall identity"), Uncached.BuildingKey.IsNone());
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(World);
	Hud->Initialize();
	Hud->TakeWidget();
	auto* Feature = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	auto* TaskEntry = Cast<UButton>(Hud->GetWidgetFromName(TEXT("TaskEntryButton")));
	if (!TestNotNull(TEXT("Feature host"), Feature) || !TestNotNull(TEXT("Modal host"), Modal)
		|| !TestNotNull(TEXT("Actual HUD entry"), TaskEntry)) return false;
	// This is a widget callback test, not a physical Slate pointer audit. The
	// existing Tasks opening may populate/refresh Hall state; baseline starts now.
	TaskEntry->OnClicked.Broadcast();
	FRokBuildingState Cached;
	if (!TestTrue(TEXT("Tasks projection has cached the exact Hall"),
		Buildings->TryPeekExistingBuildingStateForActor(Hall, Cached))) return false;
	TestEqual(TEXT("Cached source Hall key"), Cached.BuildingKey, FName(TEXT("UnityInitId_5")));
	TestEqual(TEXT("Cached Hall level one"), Cached.Level, 1);
	TestFalse(TEXT("Cached Hall initially idle"), Cached.bUpgradeInProgress);
	int32 BroadcastEvents = 0;
	const FDelegateHandle Handle = Buildings->OnLocalUpgradeCompleted.AddLambda(
		[&BroadcastEvents](const FRokLocalBuildingUpgradeCompleted&) { ++BroadcastEvents; });
	ON_SCOPE_EXIT { Buildings->OnLocalUpgradeCompleted.Remove(Handle); };
	const auto Capture = [&]()
	{
		FTaskGoSnapshot Result;
		Buildings->TryPeekExistingBuildingStateForActor(Hall, Result.Hall);
		Result.Wallet = Buildings->GetCurrentResources();
		Result.Rates = Buildings->GetProductionRates();
		Result.Task = Runtime->GetActiveTask();
		Result.ActiveUpgrades = Buildings->GetActiveUpgradeCount();
		Result.ActiveConstruction = Buildings->GetActiveLocalConstructionCount();
		Result.OwnedQueues = Buildings->GetOwnedBuildQueueCount();
		Result.PendingCompletionEvents = Buildings->GetPendingCompletedUpgradeCountForTesting();
		Result.BroadcastEvents = BroadcastEvents;
		Result.HttpDispatches = Server->GetHttpDispatchAttemptCount();
		Result.HttpSuppressions = Server->GetSuppressedLocalRequestCount();
		return Result;
	};
	const auto AssertUnchanged = [&](const TCHAR* Phase, const FTaskGoSnapshot& Before)
	{
		const FTaskGoSnapshot After = Capture();
		const FString Prefix(Phase);
		TestEqual(*(Prefix + TEXT(" Hall key")), After.Hall.BuildingKey, Before.Hall.BuildingKey);
		TestEqual(*(Prefix + TEXT(" Hall level")), After.Hall.Level, Before.Hall.Level);
		TestEqual(*(Prefix + TEXT(" Hall power")), After.Hall.Power, Before.Hall.Power);
		TestEqual(*(Prefix + TEXT(" Hall production")), After.Hall.ProducePerHour, Before.Hall.ProducePerHour);
		TestEqual(*(Prefix + TEXT(" Hall gather min")), After.Hall.GatherMin, Before.Hall.GatherMin);
		TestEqual(*(Prefix + TEXT(" Hall gather max")), After.Hall.GatherMax, Before.Hall.GatherMax);
		TestEqual(*(Prefix + TEXT(" Hall upgrade flag")), After.Hall.bUpgradeInProgress, Before.Hall.bUpgradeInProgress);
		TestEqual(*(Prefix + TEXT(" Hall start")), After.Hall.UpgradeStartedAt, Before.Hall.UpgradeStartedAt);
		TestEqual(*(Prefix + TEXT(" Hall finish")), After.Hall.UpgradeFinishesAt, Before.Hall.UpgradeFinishesAt);
		TestEqual(*(Prefix + TEXT(" food")), After.Wallet.Food, Before.Wallet.Food);
		TestEqual(*(Prefix + TEXT(" wood")), After.Wallet.Wood, Before.Wallet.Wood);
		TestEqual(*(Prefix + TEXT(" stone")), After.Wallet.Stone, Before.Wallet.Stone);
		TestEqual(*(Prefix + TEXT(" gold")), After.Wallet.Gold, Before.Wallet.Gold);
		TestEqual(*(Prefix + TEXT(" food production")), After.Rates.FoodPerHour, Before.Rates.FoodPerHour);
		TestEqual(*(Prefix + TEXT(" wood production")), After.Rates.WoodPerHour, Before.Rates.WoodPerHour);
		TestEqual(*(Prefix + TEXT(" stone production")), After.Rates.StonePerHour, Before.Rates.StonePerHour);
		TestEqual(*(Prefix + TEXT(" gold production")), After.Rates.GoldPerHour, Before.Rates.GoldPerHour);
		TestEqual(*(Prefix + TEXT(" task ID")), After.Task.TaskId, Before.Task.TaskId);
		TestEqual(*(Prefix + TEXT(" task current")), After.Task.Current, Before.Task.Current);
		TestEqual(*(Prefix + TEXT(" task complete")), After.Task.bComplete, Before.Task.bComplete);
		TestEqual(*(Prefix + TEXT(" task reward")), After.Task.Reward, Before.Task.Reward);
		TestEqual(*(Prefix + TEXT(" task next")), After.Task.NextId, Before.Task.NextId);
		TestEqual(*(Prefix + TEXT(" upgrades")), After.ActiveUpgrades, Before.ActiveUpgrades);
		TestEqual(*(Prefix + TEXT(" construction")), After.ActiveConstruction, Before.ActiveConstruction);
		TestEqual(*(Prefix + TEXT(" owned queues")), After.OwnedQueues, Before.OwnedQueues);
		TestEqual(*(Prefix + TEXT(" pending completion")), After.PendingCompletionEvents, Before.PendingCompletionEvents);
		TestEqual(*(Prefix + TEXT(" completion delegates")), After.BroadcastEvents, Before.BroadcastEvents);
		TestEqual(*(Prefix + TEXT(" HTTP dispatch delta")), After.HttpDispatches, Before.HttpDispatches);
		TestEqual(*(Prefix + TEXT(" HTTP suppression delta")), After.HttpSuppressions, Before.HttpSuppressions);
	};
	const auto ClickGo = [&]()
	{
		auto* Go = Cast<UButton>(Feature->GetWidgetFromName(TEXT("TaskAction_0")));
		if (!TestNotNull(TEXT("Actual task Go widget"), Go)) return false;
		Go->OnClicked.Broadcast();
		Feature->FlushPendingActionForTesting();
		return true;
	};
	const FTaskGoSnapshot IdleBaseline = Capture();
	if (!ClickGo()) return false;
	TestFalse(TEXT("Idle Hall success closes Tasks"), Feature->IsScreenOpen());
	TestTrue(TEXT("Idle Hall opens built info for exact actor"), Modal->IsBuildingInfoOpenForActor(Hall));
	AssertUnchanged(TEXT("Idle handoff"), IdleBaseline);
	Modal->CloseModal();
	TaskEntry->OnClicked.Broadcast();
	FString Failure;
	if (!TestTrue(TEXT("Fixture starts a real local Hall upgrade"), Buildings->StartUpgrade(Hall, Failure)))
	{
		AddError(Failure);
		return false;
	}
	const FTaskGoSnapshot ActiveBaseline = Capture();
	TestTrue(TEXT("Hall is upgrading"), ActiveBaseline.Hall.bUpgradeInProgress);
	if (!ClickGo()) return false;
	TestTrue(TEXT("Active Hall retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Active Hall opens no modal"), Modal->IsModalOpen());
	AssertUnchanged(TEXT("Active rejection"), ActiveBaseline);
	// No world Tick: an expired but unsettled source timer must not be
	// completed by this navigation's existing modal/panel getters.
	World->TimeSeconds = ActiveBaseline.Hall.UpgradeFinishesAt + 0.01f;
	const FTaskGoSnapshot ExpiredBaseline = Capture();
	if (!ClickGo()) return false;
	TestTrue(TEXT("Expired unsettled Hall retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Expired unsettled Hall opens no modal"), Modal->IsModalOpen());
	AssertUnchanged(TEXT("Expired rejection"), ExpiredBaseline);
	// Deliberate fixture settlement occurs outside Go. The first slice also
	// rejects an already completed level-two Hall instead of reusing this route.
	Buildings->Tick(0);
	const FTaskGoSnapshot CompletedBaseline = Capture();
	TestEqual(TEXT("Fixture settlement reached Hall level two"), CompletedBaseline.Hall.Level, 2);
	if (!ClickGo()) return false;
	TestTrue(TEXT("Level-two Hall retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Level-two Hall opens no modal"), Modal->IsModalOpen());
	AssertUnchanged(TEXT("Completed Hall rejection"), CompletedBaseline);
	if (Server->IsLocalOnlyRun())
	{
		TestEqual(TEXT("Local-only fixture has zero HTTP attempts"), Server->GetHttpDispatchAttemptCount(), uint64(0));
		TestEqual(TEXT("Navigation suppresses no request"), Server->GetSuppressedLocalRequestCount(), uint64(0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMainTaskGoPresentationFailureTest,
	"Rok.Ui.TaskMain200900.UnavailablePresentationKeepsTasks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMainTaskGoPresentationFailureTest::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("Rok local task Go blocked: task=200900 reason=hud_unavailable."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 3);
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName CityPackageName(*FString::Printf(
		TEXT("/Temp/RokTaskGoFixture_%s/L_RokUnityGameAppCity"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UPackage* CityPackage = NewObject<UPackage>(nullptr, CityPackageName, RF_Transient);
	Instance->InitializeStandalone(TEXT("L_RokUnityGameAppCity"), CityPackage);
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Transient city world"), World)) return false;
	if (!TestTrue(TEXT("Fixture owns its explicit transient package"),
		World->GetOutermost() == CityPackage && CityPackage->HasAnyFlags(RF_Transient))) return false;
	if (!TestEqual(TEXT("Fixture uses the exact supported map name"),
		World->GetMapName(), FString(TEXT("L_RokUnityGameAppCity")))) return false;
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Server = Instance->GetSubsystem<URokServerSubsystem>();
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(World);
	Hud->Initialize();
	Hud->TakeWidget();
	auto* Feature = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	auto* TaskEntry = Cast<UButton>(Hud->GetWidgetFromName(TEXT("TaskEntryButton")));
	if (!TestNotNull(TEXT("Buildings"), Buildings)
		|| !TestNotNull(TEXT("Server"), Server) || !TestNotNull(TEXT("Feature"), Feature)
		|| !TestNotNull(TEXT("Modal"), Modal) || !TestNotNull(TEXT("Task entry"), TaskEntry)) return false;
	TaskEntry->OnClicked.Broadcast();
	AActor* Hall = SpawnHall(World, TEXT("TaskGoUnavailableModalHall"));
	if (!TestNotNull(TEXT("Hall created only after Tasks projection"), Hall)) return false;
	auto* Go = Cast<UButton>(Feature->GetWidgetFromName(TEXT("TaskAction_0")));
	if (!TestNotNull(TEXT("Go widget"), Go)) return false;
	const auto ClickGo = [&]() { Go->OnClicked.Broadcast(); Feature->FlushPendingActionForTesting(); };
	const FRokResourceStock WalletBeforeCacheMiss = Buildings->GetCurrentResources();
	const uint64 HttpBeforeCacheMiss = Server->GetHttpDispatchAttemptCount();
	ClickGo();
	TestTrue(TEXT("Uncached Hall retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Uncached Hall cannot open info"), Modal->IsModalOpen());
	FRokBuildingState BeforeHall;
	TestFalse(TEXT("Cache miss remains a cache miss after Go"),
		Buildings->TryPeekExistingBuildingStateForActor(Hall, BeforeHall));
	TestEqual(TEXT("Cache miss does not spend food"), Buildings->GetCurrentResources().Food, WalletBeforeCacheMiss.Food);
	TestEqual(TEXT("Cache miss sends no HTTP"), Server->GetHttpDispatchAttemptCount(), HttpBeforeCacheMiss);
	// Test-only fixture setup through the existing presentation getter; the Go
	// route itself must never repair a cache miss or a mismatched actor binding.
	Buildings->GetBuildingStateForActor(Hall);
	AActor* Alias = SpawnHall(World, TEXT("TaskGoAliasBinding"));
	if (!TestNotNull(TEXT("Alias fixture"), Alias)) return false;
	Buildings->GetBuildingStateForActor(Alias);
	Alias->Tags.Remove(TEXT("UnityInitId_5"));
	TestFalse(TEXT("Exact Hall actor binding mismatch rejects pure peek"),
		Buildings->TryPeekExistingBuildingStateForActor(Hall, BeforeHall));
	ClickGo();
	TestTrue(TEXT("Mismatched cache binding retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Mismatched binding opens no modal"), Modal->IsModalOpen());
	Buildings->GetBuildingStateForActor(Hall);
	if (!TestTrue(TEXT("Explicit fixture setup restored exact cache"),
		Buildings->TryPeekExistingBuildingStateForActor(Hall, BeforeHall))) return false;
	const FRokResourceStock Wallet = Buildings->GetCurrentResources();
	const uint64 Http = Server->GetHttpDispatchAttemptCount();
	const int32 Pending = Buildings->GetPendingCompletedUpgradeCountForTesting();
	Modal->RemoveFromParent(); // Explicit transient fixture: no mounted presentation host.
	ClickGo();
	FRokBuildingState AfterHall;
	TestTrue(TEXT("Unavailable host retains Tasks"), Feature->IsScreenOpen());
	TestFalse(TEXT("Unavailable host cannot report built content"), Modal->IsBuildingInfoOpenForActor(Hall));
	TestFalse(TEXT("Unavailable host did not open a modal"), Modal->IsModalOpen());
	TestTrue(TEXT("Hall remains cached"), Buildings->TryPeekExistingBuildingStateForActor(Hall, AfterHall));
	TestEqual(TEXT("Hall level unchanged"), AfterHall.Level, BeforeHall.Level);
	TestEqual(TEXT("Hall power unchanged"), AfterHall.Power, BeforeHall.Power);
	TestEqual(TEXT("Food unchanged"), Buildings->GetCurrentResources().Food, Wallet.Food);
	TestEqual(TEXT("Wood unchanged"), Buildings->GetCurrentResources().Wood, Wallet.Wood);
	TestEqual(TEXT("No completion event queued"), Buildings->GetPendingCompletedUpgradeCountForTesting(), Pending);
	TestEqual(TEXT("No new HTTP dispatch"), Server->GetHttpDispatchAttemptCount(), Http);
	return true;
}
#endif
