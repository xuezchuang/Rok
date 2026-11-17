#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Misc/ScopeExit.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "../RokBuildingSelectionIndicator.h"
#include "../RokBuildingPanelWidget.h"
#include "UObject/UObjectIterator.h"
#include "../RokCityPlacementPreviewActor.h"
#include "../RokCitySpriteActor.h"
#include "../RokGameNetSubsystem.h"
#include "../RokPlayerController.h"
#include "../RokMainHudWidget.h"
#include "../RokServerCitySubsystem.h"
#include "../RokWorldMapHudWidget.h"
#include "../RokWorldDayNightSubsystem.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokServerTaskSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTaskWidgetTest,
	"Rok.Net.ServerTask.Widget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerTaskWidgetTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerTaskWidgetTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	URokServerTaskSubsystem* Tasks = Instance->GetSubsystem<URokServerTaskSubsystem>();
	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Server task subsystem"), Tasks)
		|| !TestNotNull(TEXT("Local economy"), Buildings)) return false;
	FRokServerTaskSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.Signature = TEXT("loading");
	Tasks->SetSnapshotForTesting(Snapshot);
	ON_SCOPE_EXIT { Tasks->ClearSnapshotForTesting(); };
	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Widget initializes"), Screen->Initialize())) return false;
	const TSharedRef<SWidget> Slate = Screen->TakeWidget();
	auto Text = [Screen](const TCHAR* Name) -> FString
	{
		const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};
	Screen->OpenServerTasksPage(1);
	const auto* ModeCanvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const auto* MainScroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskMainScroll")));
	if (!TestTrue(TEXT("The server main presentation is mounted"), RokUiFind::IsInside(MainScroll, ModeCanvas))) return false;
	TestEqual(TEXT("The main presentation uses the expected UI path"), MainScroll->GetParent() == ModeCanvas,
		URokUiSettings::IsLegacyForced());
	TestTrue(TEXT("Main waits for server"), Text(TEXT("TaskMainEmpty")).Contains(TEXT("等待服务器")));
	TestTrue(TEXT("Side waits for server"), Text(TEXT("TaskSideNotSynced")).Contains(TEXT("等待服务器")));
	TestNull(TEXT("Bootstrap task absent while connected"), Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
	Screen->OpenServerTasksPage(0);
	const auto* PendingChapter = Screen->GetWidgetFromName(TEXT("TaskChapterMetaNotice"));
	const auto* PendingPage = PendingChapter ? PendingChapter->GetTypedOuter<URokUiPageWidget>() : nullptr;
	TestTrue(TEXT("The waiting chapter uses the expected layout"), URokUiSettings::IsLegacyForced() ? !PendingPage
		: PendingPage && PendingPage->GetClass()->GetName() == TEXT("WBP_RokFeature_TaskChapterServer_C"));
	TestEqual(TEXT("Chapter waits for server status"), Text(TEXT("TaskChapterMetaNotice")),
		FString(TEXT("等待服务器同步章节进度")));
	TestNull(TEXT("Pending chapter has no task scroll"), Screen->GetWidgetFromName(TEXT("TaskChapterScroll")));
	Screen->OpenServerTasksPage(2);
	TestTrue(TEXT("Daily waits for server collection"), Text(TEXT("TaskDailyCatalogHeader")).Contains(TEXT("等待服务器")));
	Screen->OpenServerTasksPage(1);

	Snapshot.bLoaded = true;
	Snapshot.bDailyLoaded = true;
	Snapshot.MainTaskId = 200901;
	Snapshot.ChapterId = 2;
	Snapshot.ChapterName = TEXT("成长之路");
	Snapshot.ChapterDescription = TEXT("发展城市");
	Snapshot.ChapterCurrent = 1;
	Snapshot.ChapterTarget = 3;
	Snapshot.bChapterProgressKnown = true;
	Snapshot.bChapterClaimable = true;
	FRokServerTaskView& Main = Snapshot.MainTasks.AddDefaulted_GetRef();
	Main.TaskId = 200901;
	Main.RewardGroupId = 200903;
	Main.Page = ERokServerTaskPage::Main;
	Main.Name = TEXT("升级城墙");
	Main.Description = TEXT("将城墙升至三级");
	Main.State = ERokServerTaskState::Unfinished;
	Main.bProgressKnown = true;
	Main.Current = 1;
	Main.Target = 3;
	FRokServerTaskView& Side = Snapshot.SideTasks.AddDefaulted_GetRef();
	Side.TaskId = 310201;
	Side.RewardGroupId = 309001;
	Side.Page = ERokServerTaskPage::Side;
	Side.Name = TEXT("收集木材");
	Side.Description = TEXT("收集资源");
	Side.State = ERokServerTaskState::Received;
	Side.bProgressKnown = true;
	Side.Current = 10;
	Side.Target = 10;
	FRokServerTaskView& Chapter = Snapshot.ChapterTasks.AddDefaulted_GetRef();
	Chapter.TaskId = 410201;
	Chapter.Page = ERokServerTaskPage::Chapter;
	Chapter.Name = TEXT("章节建设");
	Chapter.Description = TEXT("完成城市建设");
	Chapter.State = ERokServerTaskState::Finished;
	Chapter.Current = 2;
	Chapter.Target = 2;
	Chapter.bProgressKnown = true;
	FRokServerTaskView& Daily = Snapshot.DailyTasks.AddDefaulted_GetRef();
	Daily.TaskId = 510201;
	Daily.Page = ERokServerTaskPage::Daily;
	Daily.Name = TEXT("每日探索");
	Daily.Description = TEXT("探索世界");
	Daily.State = ERokServerTaskState::Unknown;
	FRokServerTaskView& ClaimedDaily = Snapshot.DailyTasks.AddDefaulted_GetRef();
	ClaimedDaily.TaskId = 510202;
	ClaimedDaily.Page = ERokServerTaskPage::Daily;
	ClaimedDaily.Name = TEXT("已领取每日任务");
	ClaimedDaily.State = ERokServerTaskState::Received;
	Snapshot.Signature = TEXT("loaded-a");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestEqual(TEXT("Server main id"), Screen->GetVisibleTaskIdForAudit(0), 200901);
	TestEqual(TEXT("Server main title"), Text(TEXT("TaskTitle_0")), FString(TEXT("升级城墙")));
	TestTrue(TEXT("Server main progress"), Text(TEXT("TaskDescription_0")).Contains(TEXT("1/3")));
	TestNull(TEXT("A populated main list removes its empty label"), Screen->GetWidgetFromName(TEXT("TaskMainEmpty")));
	TestNull(TEXT("A populated side list removes its empty label"), Screen->GetWidgetFromName(TEXT("TaskSideNotSynced")));
	TestEqual(TEXT("Chapter badge displays on the inactive tab"), Text(TEXT("TaskPageBadgeCount_0")), FString(TEXT("1")));
	TestEqual(TEXT("Live item package quantity"), Text(TEXT("TaskRewardPreview_0_0Count")), FString(TEXT("2")));
	TestNotNull(TEXT("Live item package icon"), Screen->GetWidgetFromName(TEXT("TaskRewardPreview_0_0Icon")));
	TestEqual(TEXT("Live currency package quantity"), Text(TEXT("TaskRewardPreview_1_0Count")), FText::AsNumber(2000).ToString());
	TestEqual(TEXT("Server side id"), Screen->GetVisibleTaskIdForAudit(1), 310201);
	TestEqual(TEXT("Server side title"), Text(TEXT("TaskTitle_1")), FString(TEXT("收集木材")));
	TestTrue(TEXT("Server side received state"), Text(TEXT("TaskDescription_1")).Contains(TEXT("已领取")));
	TestNull(TEXT("Only server main and side tasks"), Screen->GetWidgetFromName(TEXT("TaskTitle_2")));
	const auto ResourcesBefore = Buildings->GetCurrentResources();
	const FString SignatureBefore = Tasks->GetSnapshot().Signature;
	UButton* SideAction = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_1")));
	if (!TestNotNull(TEXT("Received side action"), SideAction)) return false;
	SideAction->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Read-only action preserves task snapshot"), Tasks->GetSnapshot().Signature, SignatureBefore);
	TestTrue(TEXT("Received action explains the current state"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("已领取")));

	Snapshot.MainTasks[0].State = ERokServerTaskState::Finished;
	Snapshot.MainTasks[0].Current = 3;
	Snapshot.Signature = TEXT("loaded-b");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestTrue(TEXT("Changed snapshot refreshes progress"), Text(TEXT("TaskDescription_0")).Contains(TEXT("3/3")));
	TestTrue(TEXT("Finished state visible"), Text(TEXT("TaskDescription_0")).Contains(TEXT("待领取")));
	TestEqual(TEXT("Main tab counts the finished task"), Text(TEXT("TaskPageBadgeCount_1")), FString(TEXT("1")));
	UButton* MainAction = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
	if (!TestNotNull(TEXT("Finished action"), MainAction)) return false;
	MainAction->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Disconnected claim preserves server projection"), Tasks->GetSnapshot().Signature, Snapshot.Signature);

	Screen->OpenServerTasksPage(0);
	TestEqual(TEXT("Current chapter selected"), Screen->GetSelectedTaskChapterGroupIdForAudit(), 2);
	TestTrue(TEXT("Chapter title and progress"), Text(TEXT("FeatureFrameTitle")).Contains(TEXT("成长之路"))
		&& Text(TEXT("TaskChapterMetaNotice")).Contains(TEXT("1/3")));
	UButton* ChapterAction = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskChapterClaimAction")));
	if (!TestNotNull(TEXT("Claimable chapter has a reward action"), ChapterAction)) return false;
	ChapterAction->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Chapter action preserves task snapshot"), Tasks->GetSnapshot().Signature, Snapshot.Signature);
	TestTrue(TEXT("Only owned chapter task"), Screen->GetRenderedTaskChapterTaskIdsForAudit() == TArray<int32>({410201}));
	TestNotNull(TEXT("Completed chapter task has its own claim button"), Screen->GetWidgetFromName(TEXT("TaskChapterTask_410201Claim")));
	TestTrue(TEXT("Chapter objective shows its server progress"),
		Text(TEXT("TaskChapterTaskDescription_410201")).Contains(TEXT("2/2")));
	Snapshot.bChapterClaimable = false;
	Snapshot.bChapterProgressKnown = false;
	Snapshot.ChapterTasks[0].State = ERokServerTaskState::Unknown;
	Snapshot.ChapterTasks[0].bProgressKnown = false;
	Snapshot.Signature = TEXT("loaded-chapter-unknown");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestEqual(TEXT("Unknown aggregate progress stays unknown"), Text(TEXT("TaskChapterMetaNotice")), FString(TEXT("进度待同步")));
	TestEqual(TEXT("Unknown row is not a go or claim action"), Text(TEXT("TaskChapterTask_410201State")), FString(TEXT("状态待同步")));
	TestNull(TEXT("Unknown chapter has no claim action"), Screen->GetWidgetFromName(TEXT("TaskChapterClaimAction")));
	TestNull(TEXT("Unknown row cannot be claimed"), Screen->GetWidgetFromName(TEXT("TaskChapterTask_410201Claim")));
	Snapshot.ChapterTasks.Reset();
	Snapshot.ChapterId = 9999;
	Snapshot.Signature = TEXT("loaded-chapter-empty");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestEqual(TEXT("Known chapter id with no tasks has an empty list"), Text(TEXT("TaskChapterServerEmpty")), FString(TEXT("当前章节暂无任务")));
	TestNull(TEXT("Unknown chapter metadata has no guide"), Screen->GetWidgetFromName(TEXT("TaskChapterDialogue")));
	Snapshot.ChapterId = 0;
	Snapshot.Signature = TEXT("loaded-no-chapter");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestEqual(TEXT("Synchronized role may have no current chapter"), Text(TEXT("TaskChapterMetaNotice")), FString(TEXT("暂无当前章节")));
	TestNull(TEXT("No chapter does not expose a reward action"), Screen->GetWidgetFromName(TEXT("TaskChapterClaimAction")));
	Screen->OpenServerTasksPage(2);
	TestEqual(TEXT("Only owned daily id"), Screen->GetVisibleTaskIdForAudit(0), 510201);
	TestEqual(TEXT("Daily title"), Text(TEXT("TaskTitle_0")), FString(TEXT("每日探索")));
	TestTrue(TEXT("Unknown daily status stays unknown"), Text(TEXT("TaskDescription_0")).Contains(TEXT("状态待同步"))
		&& Text(TEXT("TaskDescription_0")).Contains(TEXT("进度待同步")));
	TestNull(TEXT("Received daily task is not rendered"), Screen->GetWidgetFromName(TEXT("TaskTitle_1")));
	Snapshot.DailyTasks[0].State = ERokServerTaskState::Received;
	Snapshot.Signature = TEXT("loaded-daily-claimed");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	TestNull(TEXT("Claimed visible daily task disappears on refresh"), Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
	TestTrue(TEXT("Claimed daily list has an empty state"), Text(TEXT("TaskDailyServerEmpty")).Contains(TEXT("暂无每日任务")));

	Snapshot.DailyTasks[0].State = ERokServerTaskState::Finished;
	Snapshot.DailyTasks[0].RewardGroupId = 90001;
	Snapshot.DailyTasks[0].bProgressKnown = true;
	Snapshot.DailyTasks[0].Current = 1;
	Snapshot.DailyTasks[0].Target = 1;
	Snapshot.DailyTasks[1].State = ERokServerTaskState::Unfinished;
	FRokServerTaskView Extra = Snapshot.DailyTasks[1];
	for (const int32 TaskId : {510203, 510204, 510205, 510206})
	{
		Extra.TaskId = TaskId;
		Snapshot.DailyTasks.Add(Extra);
	}
	Snapshot.DailyActivePoints = 60;
	for (const int32 Points : {20, 40, 80})
	{
		auto& Milestone = Snapshot.DailyMilestones.AddDefaulted_GetRef();
		Milestone.Points = Points;
		Milestone.RewardGroupId = 91051;
		Milestone.State = Points == 20 ? ERokServerTaskState::Received
			: Points == 40 ? ERokServerTaskState::Finished : ERokServerTaskState::Unfinished;
	}
	Snapshot.Signature = TEXT("loaded-daily-mixed");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	auto* DailyScroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskDailyCatalogScroll")));
	if (!TestNotNull(TEXT("The mixed daily list is scrollable"), DailyScroll)) return false;
	const auto* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	TestTrue(TEXT("The daily scroll is attached to the active mode"), RokUiFind::IsInside(DailyScroll, Mode));
	TestEqual(TEXT("The expected daily presentation is mounted"), DailyScroll->GetParent() == Mode,
		URokUiSettings::IsLegacyForced());
	TestNull(TEXT("The empty state leaves the mixed list"), Screen->GetWidgetFromName(TEXT("TaskDailyServerEmpty")));
	TestEqual(TEXT("Active points are projected"), Text(TEXT("TaskDailyPoints")), FString(TEXT("今日活跃度 60")));
	TestEqual(TEXT("Completed daily rows retain the reward action"), Text(TEXT("TaskAction_0Label")), FString(TEXT("领取奖励")));
	TestEqual(TEXT("Unfinished daily rows retain the go action"), Text(TEXT("TaskAction_1Label")), FString(TEXT("前往")));
	TestEqual(TEXT("The daily tab counts task and milestone rewards"), Text(TEXT("TaskPageBadgeCount_2")), FString(TEXT("2")));
	TestEqual(TEXT("A reward cell retains the source quantity"), Text(TEXT("TaskRewardPreview_0_0Count")), FText::AsNumber(1000).ToString());
	for (const int32 Points : {20, 40, 80})
	{
		const auto* Button = Cast<UButton>(Screen->GetWidgetFromName(*FString::Printf(TEXT("TaskDailyMilestone_%d"), Points)));
		TestTrue(TEXT("Only the eligible milestone is clickable"), Button && Button->GetIsEnabled() == (Points == 40));
	}
	for (const FName Name : {FName(TEXT("TaskAction_0")), FName(TEXT("TaskDailyMilestone_40"))})
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(TEXT("Offline test opens the existing claim action"), Button)) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		TestTrue(TEXT("Disconnected claim gives the original connection feedback"),
			Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("服务器连接尚未就绪")));
		TestEqual(TEXT("Offline clicks leave the task state untouched"), Tasks->GetSnapshot().Signature, Snapshot.Signature);
	}
	Screen->ForceLayoutPrepass();
	DailyScroll->SetScrollOffset(24.0f);
	TestTrue(TEXT("The populated list scrolls before the refresh"), FMath::IsNearlyEqual(DailyScroll->GetScrollOffset(), 24.0f));
	Snapshot.Signature = TEXT("loaded-daily-refresh");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->RefreshServerTasksForTesting();
	const auto* RefreshedScroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("TaskDailyCatalogScroll")));
	TestTrue(TEXT("Daily refresh retains scroll position"), RefreshedScroll && FMath::IsNearlyEqual(RefreshedScroll->GetScrollOffset(), 24.0f));

	Snapshot.MainTasks.Reset();
	Snapshot.SideTasks.Reset();
	Snapshot.Signature = TEXT("loaded-main-and-side-empty");
	Tasks->SetSnapshotForTesting(Snapshot);
	Screen->OpenServerTasksPage(1);
	TestEqual(TEXT("A synchronized empty main list is visible"), Text(TEXT("TaskMainEmpty")), FString(TEXT("暂无主线任务")));
	TestEqual(TEXT("A synchronized empty side list is visible"), Text(TEXT("TaskSideNotSynced")), FString(TEXT("暂无支线任务")));
	TestNull(TEXT("No local progress appears in the server empty state"), Screen->GetWidgetFromName(TEXT("TaskTitle_0")));
	const auto ResourcesAfter = Buildings->GetCurrentResources();
	TestEqual(TEXT("No food spent"), ResourcesAfter.Food, ResourcesBefore.Food);
	TestEqual(TEXT("No wood spent"), ResourcesAfter.Wood, ResourcesBefore.Wood);
	TestEqual(TEXT("No stone spent"), ResourcesAfter.Stone, ResourcesBefore.Stone);
	TestEqual(TEXT("No gold spent"), ResourcesAfter.Gold, ResourcesBefore.Gold);
	TestNull(TEXT("No local upgrade started"), Buildings->GetFirstActiveUpgradeActor());
	return true;
}

namespace
{
class FCheckChapterNavigation final : public IAutomationLatentCommand
{
public:
	explicit FCheckChapterNavigation(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("Chapter navigation timed out. Run in the city with a chapter 2 server replay with a built hospital."));
			return true;
		}
		if (bReturning)
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				if (TryOpenHospital(**It))
				{
					Test.TestTrue(TEXT("The world heal objective returns to the city's hospital"),
						IsHospitalOpen(**It));
					return true;
				}
			}
			return false;
		}
		if (bTravelling)
		{
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
				const auto* Level = Screen ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ResourceSearchLevelValue"))) : nullptr;
				if (!Level) continue;
				Test.TestEqual(TEXT("World search uses the chapter's barbarian level"),
					Level->GetText().ToString(), FString(TEXT("等级:1")));
				Screen->OpenServerTasksPage(1);
				auto* MainGo = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
				if (!Test.TestNotNull(TEXT("Main barbarian objective has its own navigation"), MainGo)) return true;
				MainGo->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				Level = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("ResourceSearchLevelValue")));
				Test.TestTrue(TEXT("The main objective updates world search to level 2"),
					Level && Level->GetText().ToString() == TEXT("等级:2"));
				Screen->OpenServerTasksPage(0);
				auto* Hospital = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskChapterTask_102004Go")));
				if (!Test.TestNotNull(TEXT("World chapter can route to the hospital"), Hospital)) return true;
				Hospital->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				bReturning = true;
				return false;
			}
			return false;
		}

		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			UWorld* World = It->GetWorld();
			if (!It->IsInViewport() || !World || !World->GetGameInstance()) continue;
			const auto* Net = World->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			const auto* City = World->GetSubsystem<URokServerCitySubsystem>();
			if (!Net || !Net->IsSnapshotReplay() || !City || !City->HasAppliedServerCity()) continue;
			auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
			auto* Entry = Cast<UButton>(RokUiFind::Find(*It, TEXT("TaskEntryButton")));
			if (!Screen || !Entry) continue;

			auto Click = [this, Screen](const TCHAR* Name)
			{
				auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
				if (!Test.TestNotNull(Name, Button)) return false;
				Button->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				return true;
			};
			Entry->OnClicked.Broadcast();
			Test.TestNotNull(TEXT("Normal task entry opens the chapter guide"), Screen->GetWidgetFromName(TEXT("TaskChapterGuide")));
			Test.TestNotNull(TEXT("Claimable chapter objective produces a badge"), Screen->GetWidgetFromName(TEXT("TaskPageBadge_0")));
			Test.TestNotNull(TEXT("Chapter aggregate reward has source art"), Screen->GetWidgetFromName(TEXT("TaskChapterAggregateReward_2_0Icon")));
			if (!Click(TEXT("TaskChapterTask_102002Claim"))) return true;
			const auto* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
			Test.TestTrue(TEXT("Claim uses the server request boundary"),
				Feedback && Feedback->GetText().ToString().Contains(TEXT("快照")));
			if (!Click(TEXT("TaskPage_1")) || !Click(TEXT("TaskPage_2")) || !Click(TEXT("TaskPage_0"))) return true;
			if (!Click(TEXT("TaskChapterTask_102004Go"))) return true;
			if (!Test.TestTrue(TEXT("Heal objective selects the hospital command menu"), TryOpenHospital(**It)))
			{
				return true;
			}
			Test.TestTrue(TEXT("The selected hospital's treatment action opens its screen"), IsHospitalOpen(**It));
			Entry->OnClicked.Broadcast();
			bTravelling = Click(TEXT("TaskChapterTask_102003Go"));
			return !bTravelling;
		}
		return false;
	}

private:
	static bool TryOpenHospital(URokMainHudWidget& Hud)
	{
		const auto* Panel = Cast<URokBuildingPanelWidget>(RokUiFind::Find(&Hud, TEXT("BuildingPanelWidget")));
		const auto* Title = Cast<UTextBlock>(RokUiFind::Find(Panel, TEXT("BuildingTitleText")));
		auto* Button = Cast<UButton>(RokUiFind::Find(Panel, TEXT("BuildingFeatureButton")));
		if (!Panel || !Panel->IsVisible() || !Title || !Title->GetText().ToString().Contains(TEXT("医院"))
			|| !Button || !Button->IsVisible() || !Button->GetIsEnabled())
		{
			return false;
		}
		Button->OnClicked.Broadcast();
		return true;
	}

	static bool IsHospitalOpen(const URokMainHudWidget& Hud)
	{
		const auto* Screen = Hud.GetFeatureScreenWidgetForPlacement();
		return Screen && Screen->IsHospitalOpen() && Screen->GetWidgetFromName(TEXT("HospitalMainContent"));
	}

	FAutomationTestBase& Test;
	bool bTravelling = false;
	bool bReturning = false;
	double Deadline = FPlatformTime::Seconds() + 45.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerChapterNavigationTest, "Rok.Ui.QuestChapter.ServerNavigation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokServerChapterNavigationTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckChapterNavigation(*this));
	return true;
}

namespace
{
class FCheckBuildTaskNavigation final : public IAutomationLatentCommand
{
public:
	explicit FCheckBuildTaskNavigation(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("Build-task navigation timed out. Start in the world map with the 200003 server replay."));
			return true;
		}
		if (!bReturning)
		{
			for (TObjectIterator<URokWorldMapHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("WorldMapUnityFeatureScreenWidget")));
				if (!Screen) continue;
				if (!ClickBuildTask(*Screen)) return true;
				bReturning = true;
				return false;
			}
			return false;
		}

		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			if (!It->IsInViewport()) continue;
			if (PlacementGrid.IsSet())
			{
				if (FPlatformTime::Seconds() < CheckAfter) return false;
				if (Capture(TEXT("RokAllianceCenterPlacementScreenshot=")))
				{
					CheckAfter = FPlatformTime::Seconds() + 0.5;
					return false;
				}
				CheckPlacementPresentation(**It);
				CheckConstructionLifecycle(**It, PlacementGrid.GetValue());
				return true;
			}
			auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
			if (!Screen || !Screen->IsBuildCityModeForAudit()) continue;
			const auto* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("BuildCityBuildingScroll")));
			if (!Test.TestNotNull(TEXT("Build menu has a card list"), Scroll)) return true;
			if (Scroll->GetScrollOffset() <= 0.0f) return false;
			const double Now = FPlatformTime::Seconds();
			// The scroll offset is set during tick. Sample the geometry after Slate lays it out.
			if (CheckAfter == 0.0) CheckAfter = Now + 1.0;
			if (Now < CheckAfter) return false;
			const auto* Guide = Screen->GetWidgetFromName(TEXT("BuildCityTaskGuide"));
			Test.TestTrue(TEXT("The guide points at Alliance Center and does not intercept input"), Guide
				&& Guide->GetParent() == Screen->GetWidgetFromName(TEXT("BuildCityCard_16"))
				&& Guide->GetVisibility() == ESlateVisibility::HitTestInvisible);
			const auto* CardSlot = Screen->GetWidgetFromName(TEXT("BuildCityCardSize_16"));
			if (!Test.TestNotNull(TEXT("Alliance Center card slot"), CardSlot)) return true;
			const FGeometry& CardGeometry = CardSlot->GetCachedGeometry();
			const FGeometry& ScrollGeometry = Scroll->GetCachedGeometry();
			const double CardCenter = ScrollGeometry.AbsoluteToLocal(
				CardGeometry.LocalToAbsolute(CardGeometry.GetLocalSize() * 0.5)).X;
			Test.TestTrue(TEXT("Alliance Center is centered in the list"),
				FMath::IsNearlyEqual(CardCenter, ScrollGeometry.GetLocalSize().X * 0.5, 2.0));
			if (!bOpenedInCity)
			{
				Screen->OpenBuildCity();
				Test.TestNull(TEXT("The normal build menu clears the task guide"),
					Screen->GetWidgetFromName(TEXT("BuildCityTaskGuide")));
				if (!ClickBuildTask(*Screen)) return true;
				bOpenedInCity = true;
				CheckAfter = 0.0;
				return false;
			}
			if (Capture(TEXT("RokBuildTaskScreenshot=")))
			{
				CheckAfter = FPlatformTime::Seconds() + 0.5;
				return false;
			}
			return !StartPlacement(*Screen);
		}
		return false;
	}

private:
	bool Capture(const TCHAR* Option)
	{
		FString Path;
		if (!FParse::Value(FCommandLine::Get(), Option, Path) || CapturedPaths.Contains(Path)) return false;
		CapturedPaths.Add(Path);
		FScreenshotRequest::RequestScreenshot(Path, true, false);
		return true;
	}

	bool StartPlacement(URokUnityFeatureScreenWidget& Screen)
	{
		auto* Controller = Cast<ARokPlayerController>(Screen.GetOwningPlayer());
		auto* Card = Cast<UButton>(Screen.GetWidgetFromName(TEXT("BuildCityCardButton_16")));
		if (!Test.TestNotNull(TEXT("City player controller"), Controller)
			|| !Test.TestTrue(TEXT("Alliance Center is buildable"), Card && Card->GetIsEnabled())) return false;

		auto* Buildings = Controller->GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
		FRokLocalConstructionPreview Entry;
		if (!Test.TestTrue(TEXT("Alliance Center resolves a server construction entry"),
			Buildings && Buildings->PreviewConstructionEntry(16, Entry) && Entry.bSourceEligible))
		{
			Test.AddError(Entry.FailureReason);
			return false;
		}
		Test.TestEqual(TEXT("Classical source model"), Entry.SourceModelId, FString(TEXT("build_16_101_2")));
		Test.TestEqual(TEXT("Source food cost"), Entry.Cost.Food, 500);
		Test.TestEqual(TEXT("Source wood cost"), Entry.Cost.Wood, 500);
		Test.TestEqual(TEXT("Source build time"), Entry.BuildSeconds, 7);
		Card->OnClicked.Broadcast();
		Screen.FlushPendingActionForTesting();
		const auto State = Controller->GetCityPlacementPreviewSnapshot();
		Test.TestFalse(TEXT("Card closes the drawer"), Screen.IsScreenOpen());
		if (!Test.TestTrue(TEXT("Card displays a visible preview on a valid tile"),
			State.bActive && State.bGridValid && State.PreviewActor.IsValid() && !State.PreviewActor->IsHidden())) return false;
		Test.TestEqual(TEXT("Alliance Center placement type"), State.TypeId, 16);
		Test.TestEqual(TEXT("Alliance Center placement footprint"), State.Footprint, FIntPoint(6, 6));
		Test.TestFalse(TEXT("A replay cannot spend resources"), State.bCanCommit);
		PlacementGrid = State.Grid;
		CheckAfter = FPlatformTime::Seconds() + 1.0;
		return true;
	}

	void CheckPlacementPresentation(URokMainHudWidget& Hud)
	{
		const auto* Controller = Cast<ARokPlayerController>(Hud.GetOwningPlayer());
		const auto Snapshot = Controller->GetCityPlacementPreviewSnapshot();
		auto* Preview = Cast<ARokCityPlacementPreviewActor>(Snapshot.PreviewActor.Get());
		if (!Test.TestNotNull(TEXT("Placement preview for visual checks"), Preview)) return;
		auto* Bottom = Preview->GetBottomMeshComponent();
		Test.TestFalse(TEXT("Idle valid placement hides the colored grid"), Bottom->IsVisible());
		Preview->UpdateFootprintFeedback(true, true);
		Test.TestTrue(TEXT("Dragging shows the colored grid"), Bottom->IsVisible());
		Preview->UpdateFootprintFeedback(false, false);
		Test.TestTrue(TEXT("Invalid placement keeps the grid visible"), Bottom->IsVisible());
		Preview->UpdateFootprintFeedback(false, true);
		Test.TestFalse(TEXT("Returning to idle valid placement hides the grid"), Bottom->IsVisible());
		const TActorIterator<ARokBuildingSelectionIndicator> Arrows(Hud.GetWorld());
		Test.TestTrue(TEXT("Placement shows the source selection arrows"), Arrows && !Arrows->IsHidden());

		const auto* Shadow = Cast<UImage>(RokUiFind::Find(&Hud, TEXT("CityPlacementButtonShadow")));
		Test.TestTrue(TEXT("Placement uses the source shadow without blocking input"), Shadow
			&& Shadow->GetBrush().GetResourceObject()
			&& Shadow->GetVisibility() == ESlateVisibility::HitTestInvisible);
		const auto* Cancel = RokUiFind::Find(&Hud, TEXT("CityPlacementCancelButton"));
		const auto* Confirm = RokUiFind::Find(&Hud, TEXT("CityPlacementConfirmButton"));
		if (Test.TestNotNull(TEXT("Cancel button"), Cancel) && Test.TestNotNull(TEXT("Confirm button"), Confirm))
		{
			const FGeometry& HudGeometry = Hud.GetCachedGeometry();
			const FGeometry& Left = Cancel->GetCachedGeometry();
			const FGeometry& Right = Confirm->GetCachedGeometry();
			const FVector2D Center = HudGeometry.AbsoluteToLocal((
				Left.LocalToAbsolute(Left.GetLocalSize() * 0.5) +
				Right.LocalToAbsolute(Right.GetLocalSize() * 0.5)) * 0.5);
			const FVector2D Relative = Center / HudGeometry.GetLocalSize();
			Test.TestTrue(TEXT("Placement buttons sit below the centered building"),
				FMath::IsNearlyEqual(Relative.X, 0.5, 0.015) && FMath::IsNearlyEqual(Relative.Y, 0.72, 0.025));
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("RokExpectNightPlacement")))
		{
			const auto* Lighting = Hud.GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>();
			Test.TestTrue(TEXT("City consumes the replay's nighttime server phase"), Lighting && Lighting->GetLocalState().bNight);
			FLinearColor NightLight;
			const auto* Material = Preview->GetBuildingMeshComponent()->GetMaterial(0);
			Test.TestTrue(TEXT("Nighttime preview has active mask-driven window lighting"), Material
				&& Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("NightLight")), NightLight)
				&& NightLight.R > 0.0f);
		}
	}

	void CheckConstructionLifecycle(URokMainHudWidget& Hud, FIntPoint Grid)
	{
		auto* Net = Hud.GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
		auto* City = Hud.GetWorld()->GetSubsystem<URokServerCitySubsystem>();
		if (!Test.TestTrue(TEXT("Construction fixture runs only in replay"),
			Net && Net->IsSnapshotReplay() && City && City->HasAppliedServerCity())) return;
		auto* Cancel = Cast<UButton>(RokUiFind::Find(&Hud, TEXT("CityPlacementCancelButton")));
		if (!Test.TestNotNull(TEXT("Placement cancel"), Cancel)) return;
		Cancel->OnClicked.Broadcast();
		const auto* Controller = Cast<ARokPlayerController>(Hud.GetOwningPlayer());
		Test.TestTrue(TEXT("Cancel removes the preview"), Controller && !Controller->GetCityPlacementPreviewSnapshot().bActive);

		const auto Original = City->GetAppliedBuildings();
		TArray<FRokServerBuilding> Updated = Original;
		const int32 Index = Updated.Last().Index + 1;
		Updated.Add({Index, 16, 0, Grid, 1794222270});
		FString Error;
		if (Test.TestTrue(TEXT("Apply server construction in progress"), City->ApplyBuildings(Updated, Error)))
		{
			Test.TestNotNull(TEXT("Construction countdown anchor"), City->FindBuildingBarAnchor(Index));
			Test.TestNull(TEXT("In-progress construction has no completed actor"), City->FindSpawnedBuilding(Index));
			Updated.Last().Level = 1;
			Updated.Last().FinishTime = -1;
			if (Test.TestTrue(TEXT("Apply server construction completion"), City->ApplyBuildings(Updated, Error)))
			{
				const auto* Body = City->FindSpawnedBuilding(Index);
				Test.TestTrue(TEXT("Completed Alliance Center uses the classical source model"),
					Body && !Body->IsHidden() && Body->ActorHasTag(TEXT("RokLocalSourceModel_build_16_101_2"))
					&& Body->GetGameplayTile() == Grid && Body->GetFootprintSize() == FIntPoint(6, 6));
			}
		}
		Test.TestTrue(TEXT("Restore the original replay state"), City->ApplyBuildings(Original, Error));
	}

	bool ClickBuildTask(URokUnityFeatureScreenWidget& Screen)
	{
		Screen.OpenServerTasksPage(1);
		if (!Test.TestEqual(TEXT("Alliance Center is the current server main task"), Screen.GetVisibleTaskIdForAudit(0), 200003))
			return false;
		const auto* Title = Cast<UTextBlock>(Screen.GetWidgetFromName(TEXT("TaskTitle_0")));
		Test.TestTrue(TEXT("Alliance Center task uses Unity's localized title"),
			Title && Title->GetText().ToString() == TEXT("盟约的开始"));
		auto* Go = Cast<UButton>(Screen.GetWidgetFromName(TEXT("TaskAction_0")));
		if (!Test.TestNotNull(TEXT("Task row has a Go button"), Go)) return false;
		Go->OnClicked.Broadcast();
		Screen.FlushPendingActionForTesting();
		return true;
	}

	FAutomationTestBase& Test;
	bool bReturning = false;
	bool bOpenedInCity = false;
	TOptional<FIntPoint> PlacementGrid;
	TSet<FString> CapturedPaths;
	double CheckAfter = 0.0;
	double Deadline = FPlatformTime::Seconds() + 60.0;
};
}

namespace
{
class FCheckUpgradeTaskNavigation final : public IAutomationLatentCommand
{
public:
	explicit FCheckUpgradeTaskNavigation(FAutomationTestBase& InTest) : Test(InTest) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("Upgrade-task navigation timed out waiting for the server replay city."));
			return true;
		}
		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			if (!It->IsInViewport()) continue;
			auto* Screen = Cast<URokUnityFeatureScreenWidget>(It->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
			auto* Controller = Cast<ARokPlayerController>(It->GetOwningPlayer());
			auto* City = It->GetWorld()->GetSubsystem<URokServerCitySubsystem>();
			auto* Net = It->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			auto* Tasks = It->GetGameInstance()->GetSubsystem<URokServerTaskSubsystem>();
			if (!Screen || !Controller || !City || !City->HasAppliedServerCity() || !Net || !Net->IsSnapshotReplay() || !Tasks) continue;
			const FRokServerBuilding* Farm = nullptr;
			for (const FRokServerBuilding& Building : City->GetAppliedBuildings())
				if (Building.Type == 2 && Building.Level > 0 && (!Farm || Building.Level > Farm->Level)) Farm = &Building;
			if (!Test.TestNotNull(TEXT("Replay contains a farm"), Farm)) return true;
			AActor* Expected = City->FindBuildingBarAnchor(Farm->Index);
			if (!Test.TestNotNull(TEXT("Farm has an actor"), Expected)) return true;
			const TArray<FRokServerBuilding> Before = City->GetAppliedBuildings();
			FRokServerTaskSnapshot Snapshot;
			Snapshot.bAvailable = Snapshot.bLoaded = true;
			Snapshot.MainTaskId = 200008;
			FRokServerTaskView& Task = Snapshot.MainTasks.AddDefaulted_GetRef();
			Task.TaskId = 200008; Task.Type = 8; Task.Param1 = 2; Task.Param2 = 2;
			Task.Page = ERokServerTaskPage::Main; Task.State = ERokServerTaskState::Unfinished;
			Task.Name = TEXT("庄园之主"); Task.Description = TEXT("将农场升至等级2");
			Tasks->SetSnapshotForTesting(Snapshot);
			ON_SCOPE_EXIT { Tasks->ClearSnapshotForTesting(); };
			Screen->OpenServerTasksPage(1);
			auto* Go = Cast<UButton>(Screen->GetWidgetFromName(TEXT("TaskAction_0")));
			if (!Test.TestNotNull(TEXT("Farm task Go button"), Go)) return true;
			Go->OnClicked.Broadcast();
			Screen->FlushPendingActionForTesting();
			Test.TestFalse(TEXT("Go closes the task screen"), Screen->IsScreenOpen());
			Test.TestEqual(TEXT("Go selects the highest-level farm"), Controller->GetCityPlacementPreviewSnapshot().CurrentSelection.Get(), Expected);
			Test.TestTrue(TEXT("Navigation does not mutate the server city"), City->GetAppliedBuildings() == Before);
			auto* Panel = Cast<URokBuildingPanelWidget>(It->GetWidgetFromName(TEXT("BuildingPanelWidget")));
			if (!Test.TestNotNull(TEXT("Building menu"), Panel)) return true;
			auto* Harvest = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingFeatureButton")));
			const auto* Icon = Cast<UImage>(Panel->GetWidgetFromName(TEXT("BuildingFeatureButtonImage")));
			if (Test.TestTrue(TEXT("Farm harvest action is visible and enabled"), Harvest && Harvest->IsVisible() && Harvest->GetIsEnabled())
				&& Test.TestNotNull(TEXT("Farm harvest icon"), Icon))
			{
				const FBox2f UV = Icon->GetBrush().GetUVRegion();
				Test.TestTrue(TEXT("Farm action uses the Unity food harvest atlas sprite"),
					Icon->GetBrush().GetResourceObject()
					&& FMath::IsNearlyEqual(UV.Min.X, 531.0f / 1024.0f)
					&& FMath::IsNearlyEqual(UV.Min.Y, 181.0f / 1024.0f));
				Harvest->OnClicked.Broadcast();
				const auto* Feedback = Cast<UTextBlock>(RokUiFind::Find(*It, TEXT("CityHarvestFeedback")));
				Test.TestTrue(TEXT("Harvest respects the replay write guard"), Feedback && Feedback->IsVisible()
					&& Feedback->GetText().ToString().Contains(TEXT("快照")));
				Test.TestTrue(TEXT("Replay harvest leaves city unchanged"), City->GetAppliedBuildings() == Before);
			}
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase& Test;
	double Deadline = FPlatformTime::Seconds() + 60.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerUpgradeTaskNavigationTest, "Rok.Ui.QuestUpgrade.ServerNavigation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokServerUpgradeTaskNavigationTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckUpgradeTaskNavigation(*this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerBuildTaskNavigationTest, "Rok.Ui.QuestBuild.ServerNavigation",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokServerBuildTaskNavigationTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckBuildTaskNavigation(*this));
	return true;
}

#endif
