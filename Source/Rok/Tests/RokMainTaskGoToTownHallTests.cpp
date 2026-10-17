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
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMainTaskGoTownHallTest,
	"Rok.Ui.TaskMain200900.LocalTownHallGoReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokMainTaskGoTownHallTest::RunTest(const FString&)
{
	AddExpectedMessagePlain(TEXT("Rok local task Go blocked: task=200900 reason=missing_target."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Rok local task Go blocked: task=200900 reason=ambiguous_target."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("Rok local task Go blocked: task=200900 reason=hud_unavailable."),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName CityPackageName(*FString::Printf(
		TEXT("/Temp/RokTaskGoFixture_%s/L_RokUnityGameAppCity"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UPackage* CityPackage = NewObject<UPackage>(nullptr, CityPackageName, RF_Transient);
	Instance->InitializeStandalone(TEXT("L_RokUnityGameAppCity"), CityPackage);
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated city-named test world"), World)) return false;
	if (!TestTrue(TEXT("Fixture owns its explicit transient package"),
		World->GetOutermost() == CityPackage && CityPackage->HasAnyFlags(RF_Transient))) return false;
	if (!TestEqual(TEXT("Fixture uses the exact supported map name"),
		World->GetMapName(), FString(TEXT("L_RokUnityGameAppCity")))) return false;
	URokUnityRuntimeStateSubsystem* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Actual runtime bootstrap"), Runtime)
		|| !TestNotNull(TEXT("Actual building subsystem"), Buildings)
		|| !TestTrue(TEXT("Source bootstrap loaded"), Runtime->IsRuntimeStateLoaded())) return false;
	const FRokUnityRuntimeTaskState TaskBefore = Runtime->GetActiveTask();
	const FRokResourceStock WalletBefore = Buildings->GetCurrentResources();
	const int32 UpgradesBefore = Buildings->GetActiveUpgradeCount();
	const int32 ConstructionBefore = Buildings->GetActiveLocalConstructionCount();
	const int32 OwnedQueuesBefore = Buildings->GetOwnedBuildQueueCount();
	TestEqual(TEXT("Source task ID"), TaskBefore.TaskId, 200900);
	TestEqual(TEXT("Source task type UpgradeBuilding"), TaskBefore.Type, 8);
	TestEqual(TEXT("Source target building type"), TaskBefore.Param1, 1);
	TestEqual(TEXT("Source target level"), TaskBefore.Param2, 2);

	FActorSpawnParameters Params;
	Params.Name = TEXT("TaskGoHallExplicitFixture");
	Params.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
	if (!TestNotNull(TEXT("Tagged Hall fixture"), Hall)) return false;
	Hall->Tags.Add(TEXT("UnityInitId_5"));
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
	Hall->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));

	FRokTaskRowState Row;
	Row.TaskId = TaskBefore.TaskId;
	Row.Page = FRokTaskRowState::EPage::MainTask;
	Row.SourceAuthority = TEXT("runtime_bootstrap");
	Row.State = FRokTaskRowState::EState::Unfinished;
	Row.Intent = FRokTaskRowState::EIntent::Go;
	Row.Type = TaskBefore.Type;
	Row.Param1 = TaskBefore.Param1;
	Row.Param2 = TaskBefore.Param2;
	AActor* Resolved = nullptr;
	FName Failure;
	auto Resolve = [&](const FRokTaskRowState& Candidate, UWorld* CandidateWorld)
	{
		return URokUnityFeatureScreenWidget::TryResolveLocalTask200900GoTarget(
			Candidate, TaskBefore, Runtime->GetPlayerProfileState(), true,
			CandidateWorld, Resolved, Failure);
	};
	TestTrue(TEXT("Unique source Hall resolves"), Resolve(Row, World));
	TestTrue(TEXT("Resolved exact tagged actor"), Resolved == Hall);
	FRokTaskRowState Unsupported = Row;
	Unsupported.TaskId = 200901;
	TestFalse(TEXT("Other task cannot navigate"), Resolve(Unsupported, World));
	TestEqual(TEXT("Unsupported task reason"), Failure, FName(TEXT("unsupported_task")));
	Unsupported = Row;
	Unsupported.Type = 9;
	TestFalse(TEXT("Other task type cannot navigate"), Resolve(Unsupported, World));
	TestFalse(TEXT("Missing world cannot navigate"), Resolve(Row, nullptr));
	TestEqual(TEXT("Unsupported world reason"), Failure, FName(TEXT("unsupported_world")));
	UGameInstance* OtherInstance = NewObject<UGameInstance>(GEngine);
	const FName OtherPackageName(*FString::Printf(
		TEXT("/Temp/RokTaskGoFixture_%s/L_RokUnityWorldMapPrototype"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UPackage* OtherPackage = NewObject<UPackage>(nullptr, OtherPackageName, RF_Transient);
	OtherInstance->InitializeStandalone(TEXT("L_RokUnityWorldMapPrototype"), OtherPackage);
	UWorld* OtherWorld = OtherInstance->GetWorld();
	ON_SCOPE_EXIT { OtherInstance->Shutdown(); GEngine->DestroyWorldContext(OtherWorld); };
	if (!TestNotNull(TEXT("Isolated unsupported map"), OtherWorld)) return false;
	if (!TestTrue(TEXT("Unsupported world owns its explicit transient package"),
		OtherWorld->GetOutermost() == OtherPackage && OtherPackage->HasAnyFlags(RF_Transient))) return false;
	if (!TestEqual(TEXT("Fixture uses the exact unsupported map name"),
		OtherWorld->GetMapName(), FString(TEXT("L_RokUnityWorldMapPrototype")))) return false;
	TestFalse(TEXT("A different world map cannot navigate"), Resolve(Row, OtherWorld));
	TestEqual(TEXT("Different map reason"), Failure, FName(TEXT("unsupported_world")));

	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestNotNull(TEXT("Actual city HUD fixture"), Hud)) return false;
	Hud->Initialize();
	Hud->TakeWidget();
	auto* Feature = Cast<URokUnityFeatureScreenWidget>(
		Hud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	auto* Modal = Cast<URokUnityModalWidget>(
		Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	auto* TaskEntry = Cast<UButton>(Hud->GetWidgetFromName(TEXT("TaskEntryButton")));
	if (!TestNotNull(TEXT("Real task feature"), Feature)
		|| !TestNotNull(TEXT("Real info modal"), Modal)
		|| !TestNotNull(TEXT("Real HUD task entry"), TaskEntry)) return false;
	TaskEntry->OnClicked.Broadcast();
	auto ClickGo = [&]()
	{
		auto* Go = Cast<UButton>(Feature->GetWidgetFromName(TEXT("TaskAction_0")));
		if (!TestNotNull(TEXT("Task row Go button"), Go)) return false;
		Go->OnClicked.Broadcast();
		Feature->FlushPendingActionForTesting();
		return true;
	};
	if (!ClickGo()) return false;
	TestFalse(TEXT("Successful HUD handoff closes task overlay"), Feature->IsScreenOpen());
	TestTrue(TEXT("Successful HUD handoff opens read-only info"), Modal->IsModalOpen());
	TestTrue(TEXT("Info modal owns the exact Hall actor"), Modal->GetSelectedBuildingActorForAudit() == Hall);
	Modal->CloseModal();

	Hall->Tags.Remove(TEXT("UnityInitId_5"));
	TaskEntry->OnClicked.Broadcast();
	if (!ClickGo()) return false;
	TestTrue(TEXT("Missing Hall retains task screen"), Feature->IsScreenOpen());
	TestFalse(TEXT("Missing Hall opens no modal"), Modal->IsModalOpen());
	TestFalse(TEXT("Missing Hall resolver rejects"), Resolve(Row, World));
	TestEqual(TEXT("Missing Hall reason"), Failure, FName(TEXT("missing_target")));
	auto* Feedback = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
	TestTrue(TEXT("Missing Hall shows safe feedback"),
		Feedback && Feedback->GetVisibility() == ESlateVisibility::HitTestInvisible);
	Hall->Tags.Add(TEXT("UnityInitId_5"));
	FActorSpawnParameters DuplicateParams;
	DuplicateParams.Name = TEXT("TaskGoDuplicateHallFixture");
	DuplicateParams.ObjectFlags |= RF_Transient;
	AActor* Duplicate = World->SpawnActor<AActor>(AActor::StaticClass(), DuplicateParams);
	if (!TestNotNull(TEXT("Duplicate Hall fixture"), Duplicate)) return false;
	Duplicate->Tags.Add(TEXT("UnityInitId_5"));
	Duplicate->Tags.Add(TEXT("UnityBuildingType_1"));
	if (!ClickGo()) return false;
	TestTrue(TEXT("Ambiguous Hall retains task screen"), Feature->IsScreenOpen());
	TestFalse(TEXT("Ambiguous Hall opens no modal"), Modal->IsModalOpen());
	TestFalse(TEXT("Ambiguous Hall resolver rejects"), Resolve(Row, World));
	TestEqual(TEXT("Ambiguous Hall reason"), Failure, FName(TEXT("ambiguous_target")));
	Duplicate->Destroy();
	Hall->Tags.Remove(TEXT("UnityBuildingType_1"));
	TestFalse(TEXT("Wrong building type rejects"), Resolve(Row, World));
	TestEqual(TEXT("Wrong type reason"), Failure, FName(TEXT("identity_mismatch")));
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	URokUnityFeatureScreenWidget* OrphanFeature = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestNotNull(TEXT("Standalone task feature without HUD listener"), OrphanFeature)) return false;
	OrphanFeature->Initialize();
	OrphanFeature->TakeWidget();
	OrphanFeature->OpenTasks();
	auto* OrphanGo = Cast<UButton>(OrphanFeature->GetWidgetFromName(TEXT("TaskAction_0")));
	if (!TestNotNull(TEXT("Standalone Go button"), OrphanGo)) return false;
	OrphanGo->OnClicked.Broadcast();
	OrphanFeature->FlushPendingActionForTesting();
	TestTrue(TEXT("Unbound HUD handoff retains task screen"), OrphanFeature->IsScreenOpen());

	const FRokUnityRuntimeTaskState TaskAfter = Runtime->GetActiveTask();
	const FRokResourceStock WalletAfter = Buildings->GetCurrentResources();
	TestEqual(TEXT("Task identity unchanged"), TaskAfter.TaskId, TaskBefore.TaskId);
	TestEqual(TEXT("Task progress unchanged"), TaskAfter.Current, TaskBefore.Current);
	TestEqual(TEXT("Task completion unchanged"), TaskAfter.bComplete, TaskBefore.bComplete);
	TestEqual(TEXT("Task reward group unchanged"), TaskAfter.Reward, TaskBefore.Reward);
	TestEqual(TEXT("Task next ID unchanged"), TaskAfter.NextId, TaskBefore.NextId);
	TestEqual(TEXT("Food unchanged"), WalletAfter.Food, WalletBefore.Food);
	TestEqual(TEXT("Wood unchanged"), WalletAfter.Wood, WalletBefore.Wood);
	TestEqual(TEXT("Stone unchanged"), WalletAfter.Stone, WalletBefore.Stone);
	TestEqual(TEXT("Gold unchanged"), WalletAfter.Gold, WalletBefore.Gold);
	TestEqual(TEXT("Upgrade queue unchanged"), Buildings->GetActiveUpgradeCount(), UpgradesBefore);
	TestEqual(TEXT("Construction queue unchanged"), Buildings->GetActiveLocalConstructionCount(), ConstructionBefore);
	TestEqual(TEXT("Owned builder slots unchanged"), Buildings->GetOwnedBuildQueueCount(), OwnedQueuesBefore);
	return true;
}
#endif
