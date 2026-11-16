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
#include "../RokServerArmySubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerArmyWidgetTest,
	"Rok.Net.ServerArmy.Widget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerArmyWidgetTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerArmyWidgetTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	URokServerArmySubsystem* Army = Instance->GetSubsystem<URokServerArmySubsystem>();
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	URokUnityRuntimeStateSubsystem* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!TestNotNull(TEXT("Server army source"), Army)
		|| !TestNotNull(TEXT("Local economy"), Buildings)
		|| !TestNotNull(TEXT("Bootstrap state"), Runtime)) return false;
	FRokServerArmySnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.Signature = TEXT("loading");
	Army->SetSnapshotForTesting(Snapshot);
	ON_SCOPE_EXIT { Army->ClearSnapshotForTesting(); };
	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Widget initializes"), Screen->Initialize())) return false;
	Screen->TakeWidget();
	auto Text = [Screen](const TCHAR* Name) -> FString
	{
		const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	Screen->OpenArmyStatistics();
	TestTrue(TEXT("Connected missing city and field waits"), Text(TEXT("ArmyStatisticsTotalCount")).Contains(TEXT("等待同步")));
	TestTrue(TEXT("No bootstrap total in connected view"), !Text(TEXT("ArmyStatisticsTotalCount")).Contains(TEXT("本地预览")));
	Screen->OpenArmyStatisticsPage(1);
	TestTrue(TEXT("Missing city collection is waiting"), Text(TEXT("ArmyStatisticsServerEmpty")).Contains(TEXT("等待服务器")));
	Screen->OpenArmyShow();
	TestTrue(TEXT("Missing army collection is waiting"), Text(TEXT("ArmyShowServerEmpty")).Contains(TEXT("等待服务器")));
	Screen->OpenArmyStatisticsPage(1);

	Snapshot.bLoaded = true;
	Snapshot.bCityKnown = true;
	Snapshot.bArmiesKnown = true;
	Snapshot.bTrainingKnown = true;
	Snapshot.bTotalCountKnown = true;
	Snapshot.bCityPowerKnown = true;
	Snapshot.bFieldPowerKnown = false;
	Snapshot.bTotalPowerKnown = false;
	FRokServerSoldierStack& City = Snapshot.CitySoldiers.AddDefaulted_GetRef();
	City.Id = 1001;
	City.FamilyType = 1;
	City.Tier = 1;
	City.Count = 120;
	City.TotalPower = 240;
	City.Name = TEXT("持盾步兵");
	City.bCatalogKnown = true;
	FRokServerSoldierStack& Field = Snapshot.FieldSoldiers.AddDefaulted_GetRef();
	Field.Id = 2001;
	Field.FamilyType = 2;
	Field.Tier = 2;
	Field.Count = 30;
	Field.Name = TEXT("轻骑兵");
	Field.bCatalogKnown = true;
	Snapshot.AllSoldiers.Add(City);
	Snapshot.AllSoldiers.Add(Field);
	Snapshot.CityCount = 120;
	Snapshot.FieldCount = 30;
	Snapshot.TotalCount = 150;
	Snapshot.CityPower = 240;
	Snapshot.Signature = TEXT("army-loaded");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("City total from server"), Text(TEXT("ArmyStatisticsTotalCount")).Contains(TEXT("120")));
	TestTrue(TEXT("City source name"), Text(TEXT("ArmyStatisticsRowName_0")).Contains(TEXT("持盾步兵")));
	TestTrue(TEXT("City power known"), Text(TEXT("ArmyStatisticsTotalPower")).Contains(TEXT("240")));
	Screen->OpenArmyStatisticsPage(2);
	TestTrue(TEXT("Field count from server"), Text(TEXT("ArmyStatisticsTotalCount")).Contains(TEXT("30")));
	TestTrue(TEXT("Field power unknown"), Text(TEXT("ArmyStatisticsTotalPower")).Contains(TEXT("待同步")));
	TestTrue(TEXT("Field source name"), Text(TEXT("ArmyStatisticsRowName_0")).Contains(TEXT("轻骑兵")));
	Screen->OpenArmyStatisticsPage(0);
	TestTrue(TEXT("All total combines city and field"), Text(TEXT("ArmyStatisticsTotalCount")).Contains(TEXT("150")));
	TestTrue(TEXT("All power not inferred"), Text(TEXT("ArmyStatisticsTotalPower")).Contains(TEXT("待同步")));
	Screen->OpenArmyShow();
	TestTrue(TEXT("Known empty march collection is empty"), Text(TEXT("ArmyShowServerEmpty")).Contains(TEXT("暂无行军部队")));

	FRokServerMarch& March = Snapshot.Marches.AddDefaulted_GetRef();
	March.ArmyIndex = 7;
	March.Phase = ERokServerMarchPhase::Marching;
	March.Count = 30;
	March.bSoldiersKnown = true;
	March.TargetName = TEXT("林地营地");
	March.bTargetKnown = true;
	March.bTimeKnown = true;
	March.RemainingSeconds = 90.0;
	FRokServerMarch& Sparse = Snapshot.Marches.AddDefaulted_GetRef();
	Sparse.ArmyIndex = 8;
	Sparse.Phase = ERokServerMarchPhase::Unknown;
	Sparse.bSoldiersKnown = false;
	Snapshot.Signature = TEXT("marches-loaded");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->OpenArmyShow();
	TestTrue(TEXT("March phase and count"), Text(TEXT("ArmyShowMarchStatus_0")).Contains(TEXT("行军中"))
		&& Text(TEXT("ArmyShowMarchStatus_0")).Contains(TEXT("30")));
	TestTrue(TEXT("March target and time"), Text(TEXT("ArmyShowMarchTarget_0")).Contains(TEXT("林地营地"))
		&& Text(TEXT("ArmyShowMarchRemaining_0")).Contains(TEXT("剩余")));
	TestTrue(TEXT("Sparse march does not claim zero troops"), Text(TEXT("ArmyShowMarchStatus_1")).Contains(TEXT("待同步")));
	const UTextBlock* Remaining = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ArmyShowMarchRemaining_0")));
	Snapshot.Marches[0].RemainingSeconds = 30.0;
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Clock update keeps row widget"), Screen->GetWidgetFromName(TEXT("ArmyShowMarchRemaining_0")) == Remaining);
	TestTrue(TEXT("Clock advances in place"), Text(TEXT("ArmyShowMarchRemaining_0")).Contains(TEXT("30秒")));
	const auto BeforeResources = Buildings->GetCurrentResources();
	const auto BeforeStacks = Runtime->GetStarterTroopStacks();
	UButton* Recall = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ArmyShowMarchRecall_0")));
	if (!TestNotNull(TEXT("Recall boundary action"), Recall)) return false;
	Recall->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Recall preserves server state"), Army->GetSnapshot().Signature, Snapshot.Signature);
	TestTrue(TEXT("Recall reports read-only boundary"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未改变")));

	Snapshot.bTrainingKnown = true;
	FRokServerTrainingQueue& Queue = Snapshot.TrainingQueues.AddDefaulted_GetRef();
	Queue.QueueIndex = 1;
	Queue.ArmyType = 1;
	Queue.State = ERokServerTrainingState::Unknown;
	Snapshot.Signature = TEXT("queue-unknown");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->OpenTrainArmyPreview(1);
	TestTrue(TEXT("Unbound training building stays unknown"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("待同步")));
	TestTrue(TEXT("Unknown training building explicit"), Text(TEXT("TrainArmyBuildingContext")).Contains(TEXT("待同步")));
	Screen->OpenTrainArmyPreview(2);
	TestTrue(TEXT("Missing building is not idle"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("待同步")));
	Snapshot.bBuildingsKnown = true;
	Snapshot.KnownTrainingBuildingIndices.Add(42);
	FRokServerTrainingBuildingView& Camp = Snapshot.TrainingBuildings.AddDefaulted_GetRef();
	Camp.BuildingIndex = 42;
	Camp.BuildingType = 9;
	Camp.Level = 2;
	Camp.Name = TEXT("兵营");
	Snapshot.Signature = TEXT("building-known-no-queue");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->OpenTrainArmyServerBuilding(42, 1);
	TestTrue(TEXT("Source complete queue without matching building entry is idle"),
		Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("空闲")));
	TestTrue(TEXT("Idle building identity comes from server"), Text(TEXT("TrainArmyBuildingContext")).Contains(TEXT("兵营")));
	Snapshot.bTrainingKnown = false;
	Snapshot.Signature = TEXT("queue-collection-missing");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Missing queue collection waits"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("等待服务器")));
	Snapshot.bTrainingKnown = true;
	Queue.BuildingIndex = 42;
	Queue.State = ERokServerTrainingState::Unknown;
	Snapshot.Signature = TEXT("building-known-queue-unknown");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Matching sparse queue remains unknown"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("待同步")));
	Queue.bBuildingKnown = true;
	Queue.BuildingLevel = 2;
	Queue.BuildingName = TEXT("兵营");
	Queue.State = ERokServerTrainingState::Training;
	Queue.TroopName = TEXT("持盾步兵");
	Queue.Count = 80;
	Queue.bTimeKnown = true;
	Queue.RemainingSeconds = 60.0;
	Queue.DurationSeconds = 120.0;
	Queue.Progress01 = 0.5;
	Snapshot.Signature = TEXT("queue-busy");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Server building and level"), Text(TEXT("TrainArmyBuildingContext")).Contains(TEXT("兵营"))
		&& Text(TEXT("TrainArmyBuildingContext")).Contains(TEXT("2")));
	TestTrue(TEXT("Busy queue source troop and count"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("持盾步兵"))
		&& Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("80")));
	UProgressBar* Progress = Cast<UProgressBar>(Screen->GetWidgetFromName(TEXT("TrainArmyServerQueueProgress")));
	TestTrue(TEXT("Busy queue progress"), Progress && FMath::IsNearlyEqual(Progress->GetPercent(), 0.5f));
	const auto* BusyButton = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
	TestTrue(TEXT("Busy queue cannot start a second job"), BusyButton && !BusyButton->GetIsEnabled());
	const UTextBlock* QueueStatus = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyServerQueueStatus")));
	Snapshot.TrainingQueues[0].RemainingSeconds = 20.0;
	Snapshot.TrainingQueues[0].Progress01 = 0.85;
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Clock-only queue update preserves widget"), Screen->GetWidgetFromName(TEXT("TrainArmyServerQueueStatus")) == QueueStatus);
	TestTrue(TEXT("Queue countdown advances"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("20秒")));
	Snapshot.TrainingQueues[0].State = ERokServerTrainingState::AwaitingReceive;
	Snapshot.Signature = TEXT("queue-awaiting");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Expired queue is ready to collect"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("待领取")));
	const auto* Award = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
	TestTrue(TEXT("Ready queue exposes the collect action"), Award && Award->GetIsEnabled()
		&& Text(TEXT("TrainArmyStartLabel")) == TEXT("领取"));
	Snapshot.TrainingQueues[0].State = ERokServerTrainingState::Idle;
	Snapshot.Signature = TEXT("queue-idle");
	Army->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerArmyForTesting();
	TestTrue(TEXT("Confirmed idle queue"), Text(TEXT("TrainArmyServerQueueStatus")).Contains(TEXT("空闲")));
	UButton* Train = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
	if (!TestNotNull(TEXT("Train boundary action"), Train)) return false;
	Train->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Training action preserves server state"), Army->GetSnapshot().Signature, Snapshot.Signature);
	TestTrue(TEXT("Offline training reports the missing server connection"),
		Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("连接服务器")));
	const auto AfterResources = Buildings->GetCurrentResources();
	TestEqual(TEXT("No food spent"), AfterResources.Food, BeforeResources.Food);
	TestEqual(TEXT("No wood spent"), AfterResources.Wood, BeforeResources.Wood);
	TestEqual(TEXT("No stone spent"), AfterResources.Stone, BeforeResources.Stone);
	TestEqual(TEXT("No gold spent"), AfterResources.Gold, BeforeResources.Gold);
	const auto AfterStacks = Runtime->GetStarterTroopStacks();
	TestEqual(TEXT("Local troop stack count unchanged"), AfterStacks.Num(), BeforeStacks.Num());
	for (int32 Index = 0; Index < FMath::Min(AfterStacks.Num(), BeforeStacks.Num()); ++Index)
	{
		TestEqual(TEXT("Local troop identity unchanged"), AfterStacks[Index].TroopId, BeforeStacks[Index].TroopId);
		TestEqual(TEXT("Local troop quantity unchanged"), AfterStacks[Index].Count, BeforeStacks[Index].Count);
	}
	TestNull(TEXT("No local upgrade started"), Buildings->GetFirstActiveUpgradeActor());
	return true;
}

#endif
