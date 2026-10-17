#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalTaskProgressProjectionTest,
	"Rok.Runtime.LocalTaskProgress.CompletedHallCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLocalTaskProgressProjectionTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalTaskProgressFixture"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated task world"), World)) return false;
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	URokUnityRuntimeStateSubsystem* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!TestNotNull(TEXT("Real building state"), Buildings)
		|| !TestNotNull(TEXT("Real runtime bootstrap"), Runtime)) return false;
	if (!TestTrue(TEXT("Source bootstrap loads"), Runtime->IsRuntimeStateLoaded())) return false;
	const FRokUnityRuntimeTaskState Snapshot = Runtime->GetActiveTask();
	TestEqual(TEXT("Source active task"), Snapshot.TaskId, 200900);
	TestEqual(TEXT("Source bootstrap current"), Snapshot.Current, 1);
	TestEqual(TEXT("Source bootstrap target"), Snapshot.Target, 2);
	TestFalse(TEXT("Source bootstrap not complete"), Snapshot.bComplete);

	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("LocalTaskSourceHall");
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
	if (!TestNotNull(TEXT("Source-owned Hall actor fixture"), Hall)) return false;
	Hall->Tags.Add(TEXT("UnityInitId_5"));
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
	Hall->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));

	FRokBuildingState HallState = Buildings->GetBuildingStateForActor(Hall);
	TestEqual(TEXT("Source identity resolved"), HallState.BuildingKey, FName(TEXT("UnityInitId_5")));
	const auto Initial = Runtime->GetActiveTaskLocalProjection(&HallState);
	TestTrue(TEXT("Loaded local preview projects Hall progress"), Initial.bProjected);
	TestEqual(TEXT("Completed level one displays 1/2"), Initial.Task.Current, 1);
	TestFalse(TEXT("Level one condition not met"), Initial.bConditionMet);

	FString Failure;
	if (!TestTrue(TEXT("Start real local Hall upgrade queue"), Buildings->StartUpgrade(Hall, Failure)))
	{
		AddError(Failure);
		return false;
	}
	HallState = Buildings->GetBuildingStateForActor(Hall);
	TestTrue(TEXT("Upgrade is pending"), HallState.bUpgradeInProgress);
	const auto Pending = Runtime->GetActiveTaskLocalProjection(&HallState);
	TestEqual(TEXT("Pending target level does not advance task condition"), Pending.Task.Current, 1);
	TestFalse(TEXT("Pending queue cannot satisfy condition"), Pending.bConditionMet);
	World->TimeSeconds = HallState.UpgradeFinishesAt;
	Buildings->Tick(0);
	HallState = Buildings->GetBuildingStateForActor(Hall);
	TestEqual(TEXT("Completed world level is two"), HallState.Level, 2);
	const auto Completed = Runtime->GetActiveTaskLocalProjection(&HallState);
	TestTrue(TEXT("Completed Hall remains an isolated local projection"), Completed.bProjected);
	TestEqual(TEXT("Completed Hall displays 2/2"), Completed.Task.Current, 2);
	TestTrue(TEXT("Local condition met after completion"), Completed.bConditionMet);
	TestFalse(TEXT("Projected view does not make reward claimable"), Completed.Task.bComplete);
	TestEqual(TEXT("Immutable runtime snapshot still 1/2"), Runtime->GetActiveTask().Current, 1);
	TestFalse(TEXT("Immutable runtime snapshot still incomplete"), Runtime->GetActiveTask().bComplete);
	const FRokBuildingState ReopenedHallState = Buildings->GetBuildingStateForActor(Hall);
	TestEqual(TEXT("Reopened projection recomputes 2/2"),
		Runtime->GetActiveTaskLocalProjection(&ReopenedHallState).Task.Current, 2);

	const FRokUnityRuntimePlayerProfileState LocalProfile = Runtime->GetPlayerProfileState();
	FRokBuildingState HigherCompleted = HallState;
	HigherCompleted.Level = 3;
	const auto Higher = URokUnityRuntimeStateSubsystem::ProjectLocalBuildingLevelTask(
		Snapshot, LocalProfile, true, &HigherCompleted);
	TestEqual(TEXT("Completed level above target is displayed at 2/2"), Higher.Task.Current, 2);
	TestTrue(TEXT("Above-target condition met"), Higher.bConditionMet);

	const auto CheckPreserved = [this](
		const TCHAR* Label, const FRokUnityRuntimePlayerProfileState& Profile, bool bLoaded,
		const FRokUnityRuntimeTaskState& Task, const FRokBuildingState* State)
	{
		const auto Projection = URokUnityRuntimeStateSubsystem::ProjectLocalBuildingLevelTask(
			Task, Profile, bLoaded, State);
		TestFalse(*FString::Printf(TEXT("%s is not projected"), Label), Projection.bProjected);
		TestEqual(*FString::Printf(TEXT("%s keeps its current progress"), Label), Projection.Task.Current, Task.Current);
		TestEqual(*FString::Printf(TEXT("%s keeps completion state"), Label), Projection.Task.bComplete, Task.bComplete);
	};
	CheckPreserved(TEXT("Unknown runtime"), LocalProfile, false, Snapshot, &HallState);
	FRokUnityRuntimePlayerProfileState AccountProfile = LocalProfile;
	AccountProfile.StateKind = TEXT("server_account");
	CheckPreserved(TEXT("Account mode"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.StateKind = TEXT("Unknown");
	CheckPreserved(TEXT("Unknown profile mode"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.bHasRoleId = true;
	CheckPreserved(TEXT("Role identity flag"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.RoleId = TEXT("synthetic-role");
	CheckPreserved(TEXT("Role identity string"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.bHasCivilization = true;
	CheckPreserved(TEXT("Civilization identity flag"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.CivilizationId = 101;
	CheckPreserved(TEXT("Civilization identity id"), AccountProfile, true, Snapshot, &HallState);
	AccountProfile = LocalProfile;
	AccountProfile.CivilizationName = TEXT("synthetic civilization");
	CheckPreserved(TEXT("Civilization identity name"), AccountProfile, true, Snapshot, &HallState);
	FRokUnityRuntimeTaskState OtherTask = Snapshot;
	OtherTask.TaskId = 200901;
	CheckPreserved(TEXT("Other task"), LocalProfile, true, OtherTask, &HallState);
	OtherTask = Snapshot;
	OtherTask.Target = 3;
	CheckPreserved(TEXT("Different source target"), LocalProfile, true, OtherTask, &HallState);
	FRokBuildingState OtherBuilding = HallState;
	OtherBuilding.BuildingKey = FName(TEXT("UnityInitId_6"));
	CheckPreserved(TEXT("Other source building"), LocalProfile, true, Snapshot, &OtherBuilding);
	CheckPreserved(TEXT("Missing Hall"), LocalProfile, true, Snapshot, nullptr);
	return true;
}
#endif
