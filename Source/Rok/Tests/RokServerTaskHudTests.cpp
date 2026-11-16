#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokServerTaskSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTaskHudTest, "Rok.Net.ServerTasks.Hud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerTaskHudTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerTaskHudTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	URokServerTaskSubsystem* Tasks = Instance->GetSubsystem<URokServerTaskSubsystem>();
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Task service"), Tasks) || !TestNotNull(TEXT("Building service"), Buildings)) return false;
	FRokServerTaskSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.bLoaded = false;
	Tasks->SetSnapshotForTesting(Snapshot);
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestTrue(TEXT("HUD initializes"), Hud->Initialize())) return false;
	Hud->TakeWidget();
	Hud->RefreshFromSubsystem();
	const auto Text = [Hud](const TCHAR* Name)
	{
		const UTextBlock* Value = Cast<UTextBlock>(Hud->GetWidgetFromName(Name));
		return Value ? Value->GetText().ToString() : FString();
	};
	TestTrue(TEXT("Partial server state does not display bootstrap quest"), Text(TEXT("TaskTitle_0")).Contains(TEXT("同步中")));
	Snapshot.bLoaded = true;
	FRokServerTaskView Main;
	Main.TaskId = 200903; Main.Page = ERokServerTaskPage::Main;
	Main.Name = TEXT("弓兵训练"); Main.Description = TEXT("训练20个弓兵单位");
	Main.Current = 3; Main.Target = 20; Main.bProgressKnown = true;
	Main.State = ERokServerTaskState::Unfinished;
	FRokServerTaskView Side = Main;
	Side.TaskId = 301001; Side.Page = ERokServerTaskPage::Side;
	Side.Name = TEXT("支线完成"); Side.Current = Side.Target = 5;
	Side.State = ERokServerTaskState::Finished;
	FRokServerTaskView Chapter = Main;
	Chapter.TaskId = 2; Chapter.Page = ERokServerTaskPage::Chapter;
	Chapter.Name = TEXT("第二章"); Chapter.Description = TEXT("发展城市");
	Chapter.Current = 1; Chapter.Target = 6;
	Snapshot.MainTasks = {Main}; Snapshot.SideTasks = {Side};
	Snapshot.HudTasks = {Main, Side, Chapter};
	Snapshot.ChapterId = 2; Snapshot.ChapterName = Chapter.Name;
	Snapshot.ChapterCurrent = 1; Snapshot.ChapterTarget = 6;
	Snapshot.bChapterProgressKnown = true;
	Tasks->SetSnapshotForTesting(Snapshot);
	Hud->RefreshFromSubsystem();
	TestNotNull(TEXT("Real task list scrolls"), Cast<UScrollBox>(Hud->GetWidgetFromName(TEXT("HudTaskScroll"))));
	TestEqual(TEXT("Main title uses account task"), Text(TEXT("TaskTitle_0")), Main.Name);
	TestTrue(TEXT("Main progress uses server value"), Text(TEXT("TaskProgress_0")).Contains(TEXT("3/20")));
	UTextBlock* Progress = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskProgress_0")));
	if (!TestNotNull(TEXT("Task description label"), Progress)) return false;
	const float ShortDescriptionSize = Progress->GetFont().Size;
	const FString LongDescription = TEXT("在城市内升级兵营并训练弓兵，随后前往世界地图收集资源，回城领取训练部队并完成本次任务");
	Snapshot.HudTasks[0].Description = LongDescription;
	Tasks->SetSnapshotForTesting(Snapshot);
	Hud->RefreshFromSubsystem();
	TestTrue(TEXT("Long descriptions shrink to fit"), Progress->GetFont().Size < ShortDescriptionSize);
	TestTrue(TEXT("Wrapped description fits the source height"), Progress->GetDesiredSize().Y > 0.0
		&& Progress->GetDesiredSize().Y <= 44.0);
	TestTrue(TEXT("Fit retains the complete description and progress"),
		Text(TEXT("TaskProgress_0")).Contains(LongDescription) && Text(TEXT("TaskProgress_0")).Contains(TEXT("3/20")));
	Snapshot.HudTasks[0].Description = Main.Description;
	Tasks->SetSnapshotForTesting(Snapshot);
	Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Shorter description restores its original size"), Progress->GetFont().Size, ShortDescriptionSize);

	TestEqual(TEXT("Completed tasks populate the HUD badge"), Text(TEXT("TaskEntryBadgeCount")), FString(TEXT("1")));
	TestEqual(TEXT("Side follows main"), Text(TEXT("TaskTitle_1")), Side.Name);
	TestEqual(TEXT("Chapter follows side"), Text(TEXT("TaskTitle_2")), Chapter.Name);
	UImage* MainReward = Cast<UImage>(Hud->GetWidgetFromName(TEXT("TaskReward_0")));
	UImage* SideReward = Cast<UImage>(Hud->GetWidgetFromName(TEXT("TaskReward_1")));
	if (!TestNotNull(TEXT("Main marker"), MainReward) || !TestNotNull(TEXT("Side marker"), SideReward)) return false;
	TestEqual(TEXT("Unfinished has no claim marker"), MainReward->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Only finished side has a claim marker"), SideReward->GetVisibility(), ESlateVisibility::HitTestInvisible);
	const auto Before = Buildings->GetCurrentResources();
	UButton* ChapterAction = Cast<UButton>(Hud->GetWidgetFromName(TEXT("HudTaskAction_2")));
	if (!TestNotNull(TEXT("Chapter is clickable"), ChapterAction)) return false;
	ChapterAction->OnClicked.Broadcast();
	URokUnityFeatureScreenWidget* Screen = Hud->GetFeatureScreenWidgetForPlacement();
	TestTrue(TEXT("Chapter card opens task screen"), Screen && Screen->IsScreenOpen()
		&& Screen->GetWidgetFromName(TEXT("TaskChapterRoot")));
	TestEqual(TEXT("Card navigation does not grant food"), Buildings->GetCurrentResources().Food, Before.Food);
	TestEqual(TEXT("Card navigation does not grant wood"), Buildings->GetCurrentResources().Wood, Before.Wood);
	TestEqual(TEXT("Card navigation does not consume completed task"), Tasks->GetSnapshot().HudTasks[1].State,
		ERokServerTaskState::Finished);
	Snapshot.HudTasks.RemoveAt(1);
	Snapshot.HudTasks[0].Current = 7;
	Tasks->SetSnapshotForTesting(Snapshot);
	Hud->RefreshFromSubsystem();
	TestTrue(TEXT("Progress update is live"), Text(TEXT("TaskProgress_0")).Contains(TEXT("7/20")));
	TestEqual(TEXT("Removed side shifts chapter up"), Text(TEXT("TaskTitle_1")), Chapter.Name);
	TestNull(TEXT("No stale third card"), Hud->GetWidgetFromName(TEXT("TaskTitle_2")));
	Snapshot.SideTasks.Init(Side, 100);
	for (int32 Index = 0; Index < Snapshot.SideTasks.Num(); ++Index) Snapshot.SideTasks[Index].TaskId += Index;
	Tasks->SetSnapshotForTesting(Snapshot);
	Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Source limit renders 99+"), Text(TEXT("TaskEntryBadgeCount")), FString(TEXT("99+")));
	Tasks->ClearSnapshotForTesting();
	Hud->RefreshFromSubsystem();
	TestNull(TEXT("Offline has only the original task row"), Hud->GetWidgetFromName(TEXT("TaskRow_1")));
	if (const auto* Badge = Hud->GetWidgetFromName(TEXT("TaskEntryBadge")))
		TestEqual(TEXT("Zero hides both the circle and its label"), Badge->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

#endif
