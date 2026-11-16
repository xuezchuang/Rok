#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokServerCitySubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerQueueWidgetTest,
	"Rok.Net.ServerQueue.Widget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerQueueWidgetTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerQueueWidgetTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	URokServerCitySubsystem* ServerCity = World->GetSubsystem<URokServerCitySubsystem>();
	URokBuildingUpgradeSubsystem* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Server city"), ServerCity) || !TestNotNull(TEXT("Local upgrades"), Upgrades)) return false;

	FRokServerBuildQueueSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.bTimeSynchronized = true;
	FRokServerBuildQueueView& Primary = Snapshot.Queues.AddDefaulted_GetRef();
	Primary.QueueIndex = 1;
	Primary.State = ERokServerBuildQueueState::Unknown;
	FRokServerBuildQueueView& Secondary = Snapshot.Queues.AddDefaulted_GetRef();
	Secondary.QueueIndex = 2;
	Secondary.State = ERokServerBuildQueueState::Unknown;
	ServerCity->SetBuildQueueSnapshotForTesting(Snapshot);
	ON_SCOPE_EXIT { ServerCity->ClearBuildQueueSnapshotForTesting(); };

	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Widget initializes"), Screen->Initialize())) return false;
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	Screen->OpenWorkerQueue();
	auto Text = [Screen](const TCHAR* Name) -> FString
	{
		const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	TestTrue(TEXT("Primary waits for actual server data"), Text(TEXT("WorkerQueuePrimaryTitle")).IsEmpty());
	TestTrue(TEXT("Secondary waits for actual server data"), Text(TEXT("WorkerQueueSecondaryTitle")).IsEmpty());

	Snapshot.Queues[0].State = ERokServerBuildQueueState::Idle;
	Snapshot.Queues[1].State = ERokServerBuildQueueState::Locked;
	Snapshot.Queues[1].LeaseRemainingSeconds = 0.0;
	ServerCity->SetBuildQueueSnapshotForTesting(Snapshot);
	Screen->RefreshWorkerQueueForTesting();
	TestTrue(TEXT("Primary idle comes from server"), Text(TEXT("WorkerQueuePrimaryTitle")).Contains(TEXT("空闲")));
	TestTrue(TEXT("Secondary locked comes from server"), Text(TEXT("WorkerQueueSecondaryTitle")).Contains(TEXT("雇佣额外")));
	TestTrue(TEXT("Hire shows the Unity contract duration"), Text(TEXT("WorkerQueueHireTerm")).Contains(TEXT("2天")));
	TestNull(TEXT("Worker window has no diagnostic footer"), Screen->GetWidgetFromName(TEXT("WorkerQueueContractBoundary")));

	Snapshot.Queues[0].State = ERokServerBuildQueueState::Busy;
	Snapshot.Queues[0].BuildingIndex = 5;
	Snapshot.Queues[0].BuildingName = TEXT("市政厅");
	Snapshot.Queues[0].TargetLevel = 3;
	Snapshot.Queues[0].RemainingSeconds = 90.0;
	Snapshot.Queues[0].bCountdownKnown = true;
	Snapshot.Queues[0].TotalSeconds = 180.0;
	Snapshot.Queues[0].Progress = 0.5f;
	Snapshot.Queues[0].bProgressKnown = true;
	Snapshot.Queues[1].State = ERokServerBuildQueueState::Busy;
	Snapshot.Queues[1].BuildingIndex = 27;
	Snapshot.Queues[1].BuildingName = TEXT("农场");
	Snapshot.Queues[1].TargetLevel = 2;
	Snapshot.Queues[1].RemainingSeconds = 45.0;
	Snapshot.Queues[1].bCountdownKnown = true;
	Snapshot.Queues[1].TotalSeconds = 90.0;
	Snapshot.Queues[1].Progress = 0.5f;
	Snapshot.Queues[1].bProgressKnown = true;
	Snapshot.Queues[1].LeaseRemainingSeconds = 3600.0;
	ServerCity->SetBuildQueueSnapshotForTesting(Snapshot);
	Screen->RefreshWorkerQueueForTesting();
	TestTrue(TEXT("Primary building and level"), Text(TEXT("WorkerQueuePrimaryTitle")).Contains(TEXT("市政厅"))
		&& Text(TEXT("WorkerQueuePrimaryTitle")).Contains(TEXT("3")));
	TestTrue(TEXT("Secondary building and level"), Text(TEXT("WorkerQueueSecondaryTitle")).Contains(TEXT("农场"))
		&& Text(TEXT("WorkerQueueSecondaryTitle")).Contains(TEXT("2")));
	TestTrue(TEXT("Primary countdown"), Text(TEXT("WorkerQueuePrimaryProgressText")).Contains(TEXT(":")));
	TestTrue(TEXT("Secondary countdown"), Text(TEXT("WorkerQueueSecondaryProgressText")).Contains(TEXT(":")));
	TestTrue(TEXT("Busy secondary shows the original working label"), Text(TEXT("WorkerQueueSecondaryNote")).Contains(TEXT("辛勤工作")));
	const UProgressBar* PrimaryBar = Cast<UProgressBar>(Screen->GetWidgetFromName(TEXT("WorkerQueuePrimaryProgress")));
	const UProgressBar* SecondaryBar = Cast<UProgressBar>(Screen->GetWidgetFromName(TEXT("WorkerQueueSecondaryProgress")));
	TestTrue(TEXT("Both server progress bars show half"), PrimaryBar && SecondaryBar
		&& FMath::IsNearlyEqual(PrimaryBar->GetPercent(), 0.5f)
		&& FMath::IsNearlyEqual(SecondaryBar->GetPercent(), 0.5f));

	const auto BeforeResources = Upgrades->GetCurrentResources();
	for (const TCHAR* Name : {TEXT("WorkerQueuePrimaryAction"), TEXT("WorkerQueueSecondQueueUnlockOrExtend")})
	{
		UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		TestNotNull(TEXT("Busy worker opens the server speed-up list"), Screen->GetWidgetFromName(TEXT("BuildingSpeedUpItems")));
		TestTrue(TEXT("Speed-up is bound to the clicked worker's building"), Text(TEXT("SpeedUpQueueDescription")).Contains(
			FCString::Strcmp(Name, TEXT("WorkerQueuePrimaryAction")) == 0 ? TEXT("市政厅") : TEXT("农场")));
		Screen->RequestCloseScreen();
		TestNotNull(TEXT("Closing speed-up returns to workers"), Screen->GetWidgetFromName(TEXT("WorkerQueuePrimaryTitle")));
	}

	Snapshot.Queues[1].State = ERokServerBuildQueueState::Idle;
	Snapshot.Queues[1].LeaseRemainingSeconds = 5;
	ServerCity->SetBuildQueueSnapshotForTesting(Snapshot);
	Screen->OpenWorkerQueue(nullptr, 8);
	TestTrue(TEXT("Short lease offers extension for the waiting build"), Text(TEXT("WorkerQueueSecondaryNote")).Contains(TEXT("剩余时间不足")));
	TestNotNull(TEXT("Extension uses the original two-day contract button"), Screen->GetWidgetFromName(TEXT("WorkerQueueHireCost")));
	Snapshot.Queues[1].LeaseRemainingSeconds = 172805;
	ServerCity->SetBuildQueueSnapshotForTesting(Snapshot);
	Screen->RefreshWorkerQueueForTesting();
	TestTrue(TEXT("Renewed lease returns to the available worker state"), Text(TEXT("WorkerQueueSecondaryNote")).Contains(TEXT("剩余时间:")));
	TestNull(TEXT("Renewed worker no longer offers the contract button"), Screen->GetWidgetFromName(TEXT("WorkerQueueHireCost")));
	const auto AfterResources = Upgrades->GetCurrentResources();
	TestEqual(TEXT("No food spent"), AfterResources.Food, BeforeResources.Food);
	TestEqual(TEXT("No wood spent"), AfterResources.Wood, BeforeResources.Wood);
	TestEqual(TEXT("No stone spent"), AfterResources.Stone, BeforeResources.Stone);
	TestEqual(TEXT("No gold spent"), AfterResources.Gold, BeforeResources.Gold);
	TestNull(TEXT("Server actions never start a local upgrade"), Upgrades->GetFirstActiveUpgradeActor());
	return true;
}

#endif
