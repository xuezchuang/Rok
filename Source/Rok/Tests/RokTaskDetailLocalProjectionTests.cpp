#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTaskDetailLocalProjectionTest,
	"Rok.Ui.TaskDetail.LocalHallProjectionInPlace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokTaskDetailLocalProjectionTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTaskDetailProjectionFixture"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated task detail world"), World)) return false;
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	URokUnityRuntimeStateSubsystem* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!TestNotNull(TEXT("Building subsystem"), Buildings)
		|| !TestNotNull(TEXT("Runtime task source"), Runtime)
		|| !TestTrue(TEXT("Task bootstrap loaded"), Runtime->IsRuntimeStateLoaded())) return false;
	const FRokUnityRuntimeTaskState Snapshot = Runtime->GetActiveTask();
	if (!TestEqual(TEXT("Known source Hall task"), Snapshot.TaskId, 200900)) return false;

	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("TaskDetailSourceHall");
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
	if (!TestNotNull(TEXT("Source Hall fixture"), Hall)) return false;
	Hall->Tags.Add(TEXT("UnityInitId_5"));
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
	Hall->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));

	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestNotNull(TEXT("Task detail widget"), Screen)) return false;
	Screen->Initialize();
	Screen->TakeWidget();
	Screen->OpenTasks();
	auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskMainScroll")));
	auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
	auto* Description = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskDescription_0")));
	auto* Action = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
	if (!TestNotNull(TEXT("Real task list"), Scroll)
		|| !TestNotNull(TEXT("Real task title"), Title)
		|| !TestNotNull(TEXT("Real task description"), Description)
		|| !TestNotNull(TEXT("Existing task action"), Action)) return false;
	TestTrue(TEXT("Level one title marks local projection"), Title->GetText().ToString().Contains(TEXT("·本地")));
	TestTrue(TEXT("Level one detail is 1/2"), Description->GetText().ToString().Contains(TEXT("(1/2)")));
	TestTrue(TEXT("Level one tooltip marks server boundary"),
		Description->GetToolTipText().ToString().Contains(TEXT("服务器任务状态待同步")));
	TestTrue(TEXT("Projection visible in detail"), Screen->IsTaskDetailLocalProjectionForAudit());
	TestFalse(TEXT("Level one condition pending"), Screen->IsTaskDetailLocalConditionMetForAudit());
	TestEqual(TEXT("Displayed current at level one"), Screen->GetTaskDetailDisplayedCurrentForAudit(), 1);

	FString Failure;
	if (!TestTrue(TEXT("Start real local Hall upgrade"), Buildings->StartUpgrade(Hall, Failure)))
	{
		AddError(Failure);
		return false;
	}
	Screen->RefreshTaskDetailProjectionForTesting();
	TestTrue(TEXT("Pending queue retains completed level one"),
		Description->GetText().ToString().Contains(TEXT("(1/2)")));
	FRokBuildingState HallState = Buildings->GetBuildingStateForActor(Hall);
	World->TimeSeconds = HallState.UpgradeFinishesAt;
	Buildings->Tick(0.0f);
	Screen->RefreshTaskDetailProjectionForTesting();
	TestTrue(TEXT("Same title widget survives task progress refresh"),
		Screen->GetWidgetFromName(TEXT("TaskTitle_0")) == Title);
	TestTrue(TEXT("Same scroll survives task progress refresh"),
		Screen->GetWidgetFromName(TEXT("TaskMainScroll")) == Scroll);
	TestTrue(TEXT("Same task action survives local condition"),
		Screen->GetWidgetFromName(TEXT("TaskAction_0")) == Action);
	TestTrue(TEXT("Completed local Hall displays 2/2"),
		Description->GetText().ToString().Contains(TEXT("(2/2)")));
	TestTrue(TEXT("Condition tooltip distinguishes server completion"),
		Description->GetToolTipText().ToString().Contains(TEXT("本地条件已满足，服务器任务状态待同步")));
	TestTrue(TEXT("Local condition met"), Screen->IsTaskDetailLocalConditionMetForAudit());
	TestTrue(TEXT("Task action remains Go rather than claim UI"), Action->GetIsEnabled());
	TestEqual(TEXT("Immutable source task remains 1/2"), Runtime->GetActiveTask().Current, 1);
	TestFalse(TEXT("Source reward remains incomplete"), Runtime->GetActiveTask().bComplete);
	Action->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Task action does not advance bootstrap task"), Runtime->GetActiveTask().TaskId, 200900);
	TestFalse(TEXT("Task action cannot claim reward"), Runtime->GetActiveTask().bComplete);
	TestTrue(TEXT("Task action does not rebuild the task scroll"),
		Screen->GetWidgetFromName(TEXT("TaskMainScroll")) == Scroll);

	Hall->Tags.Remove(TEXT("UnityInitId_5"));
	Screen->RefreshTaskDetailProjectionForTesting();
	TestFalse(TEXT("Missing source Hall removes local mark"),
		Title->GetText().ToString().Contains(TEXT("·本地")));
	TestTrue(TEXT("Missing source Hall restores snapshot progress"),
		Description->GetText().ToString().Contains(TEXT("(1/2)")));
	TestTrue(TEXT("Missing source Hall clears local tooltip"),
		Description->GetToolTipText().IsEmpty());
	TestFalse(TEXT("No Hall is not projected"), Screen->IsTaskDetailLocalProjectionForAudit());
	TestTrue(TEXT("Fallback also leaves scroll in place"),
		Screen->GetWidgetFromName(TEXT("TaskMainScroll")) == Scroll);

	Hall->Tags.Add(TEXT("UnityInitId_5"));
	Screen->OpenTasks();
	auto* Reopened = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TaskDescription_0")));
	if (!TestNotNull(TEXT("Reopened task description"), Reopened)) return false;
	TestTrue(TEXT("Reopening recomputes completed Hall condition"),
		Reopened->GetText().ToString().Contains(TEXT("(2/2)")));
	TestFalse(TEXT("Reopen still cannot claim bootstrap reward"), Runtime->GetActiveTask().bComplete);
	return true;
}
#endif
