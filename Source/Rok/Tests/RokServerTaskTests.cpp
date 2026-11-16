#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "../RokServerTaskSubsystem.h"
#include "../RokResearchCatalog.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include <initializer_list>

namespace
{
TSharedPtr<FJsonObject> Row(std::initializer_list<TPair<FString, int64>> Values)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	for (const auto& Pair : Values) Result->SetNumberField(Pair.Key, double(Pair.Value));
	return Result;
}

TArray<TSharedPtr<FJsonValue>> ArrayOf(std::initializer_list<TSharedPtr<FJsonObject>> Rows)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const auto& Item : Rows) Values.Add(MakeShared<FJsonValueObject>(Item));
	return Values;
}

TSharedPtr<FJsonObject> Role(int32 MainSchedule, bool bStatistics = true)
{
	TSharedPtr<FJsonObject> Result = Row({{TEXT("mainLineTaskId"), 200903},
		{TEXT("chapterId"), 2}, {TEXT("level"), 2}, {TEXT("historyPower"), 7357}});
	Result->SetArrayField(TEXT("finishSideTasks"), ArrayOf({Row({{TEXT("taskId"), 304001}})}));
	Result->SetArrayField(TEXT("chapterTasks"), ArrayOf({Row({{TEXT("taskId"), 102001}, {TEXT("status"), 1}})}));
	Result->SetArrayField(TEXT("technologies"), TArray<TSharedPtr<FJsonValue>>());
	if (bStatistics)
	{
		TSharedPtr<FJsonObject> Type = Row({{TEXT("type"), 33}});
		Type->SetArrayField(TEXT("statistics"), ArrayOf({Row({{TEXT("arg"), 3}, {TEXT("num"), MainSchedule}})}));
		TSharedPtr<FJsonObject> CityResource = Row({{TEXT("type"), 9}});
		CityResource->SetArrayField(TEXT("statistics"), ArrayOf({Row({{TEXT("arg"), 1}, {TEXT("num"), 24638}})}));
		Result->SetArrayField(TEXT("taskStatisticsSum"), ArrayOf({Type, CityResource}));
	}
	return Result;
}

TSharedPtr<FJsonObject> Buildings()
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("5"), Row({{TEXT("type"), 1}, {TEXT("level"), 2}, {TEXT("finishTime"), -1}}));
	Result->SetObjectField(TEXT("30"), Row({{TEXT("type"), 11}, {TEXT("level"), 1}, {TEXT("finishTime"), -1}}));
	Result->SetObjectField(TEXT("31"), Row({{TEXT("type"), 11}, {TEXT("level"), 0}, {TEXT("finishTime"), 0}}));
	Result->SetObjectField(TEXT("32"), Row({{TEXT("type"), 11}, {TEXT("level"), -1}, {TEXT("finishTime"), -1}}));
	return Result;
}

TSharedPtr<FJsonObject> Daily(bool bClaimOne = false)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	// Task_TaskInfo.taskInfo from the recorded 2026-11-04 session, task fields only.
	for (int32 Id = 401001; Id <= 401024; ++Id)
	{
		const int32 Schedule = Id == 401001 || Id == 401008 ? 20 : (Id == 401013 ? 2000 : 0);
		Result->SetObjectField(FString::FromInt(Id), Row({{TEXT("taskId"), Id},
			{TEXT("taskSchedule"), bClaimOne && Id == 401002 ? -1 : Schedule}}));
	}
	return Result;
}

TArray<FRokTaskChapterPreviewRow> Chapters()
{
	FRokTaskChapterPreviewRow Chapter;
	Chapter.Id = 2; Chapter.bMetadataKnown = true;
	Chapter.Title.ZhCn = TEXT("Chapter Two");
	Chapter.Description1.ZhCn = TEXT("Chapter description");
	Chapter.RewardGroupId = 100002;
	return {Chapter};
}

TArray<FRokTaskChapterTaskPreviewRow> ChapterTasks()
{
	TArray<FRokTaskChapterTaskPreviewRow> Result;
	for (int32 I = 1; I <= 4; ++I)
	{
		FRokTaskChapterTaskPreviewRow& RowOut = Result.AddDefaulted_GetRef();
		RowOut.Id = 102000 + I; RowOut.ChapterId = 2; RowOut.Order = I;
	}
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTaskProjectionTest,
	"Rok.Net.ServerTasks.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerTaskProjectionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerTasks_%s/L_RokServerTasks"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Instance->InitializeStandalone(TEXT("L_RokServerTasks"), NewObject<UPackage>(nullptr, PackageName, RF_Transient));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Catalog"), Catalog) || !TestTrue(TEXT("Catalog loaded"), Catalog->IsCatalogLoaded())) return false;
	const auto ChapterDefs = Chapters();
	const auto TaskDefs = ChapterTasks();
	const auto Project = [&](const TSharedPtr<FJsonObject>& R, const TSharedPtr<FJsonObject>& D)
	{
		return URokServerTaskSubsystem::ProjectSnapshot(R, D, Buildings(), Catalog, ChapterDefs, TaskDefs);
	};
	FRokServerTaskSnapshot Partial = Project(nullptr, Daily());
	TestTrue(TEXT("RoleReady remains available on partial data"), Partial.bAvailable);
	TestFalse(TEXT("Missing role not loaded"), Partial.bLoaded);
	TestTrue(TEXT("Daily collection independently loaded"), Partial.bDailyLoaded);
	TSharedPtr<FJsonObject> EmptyRole = MakeShared<FJsonObject>();
	TestFalse(TEXT("Empty role is pending"), Project(EmptyRole, Daily()).bLoaded);
	Partial = Project(Role(20, false), Daily());
	TestTrue(TEXT("Role loads without statistics"), Partial.bLoaded);
	TestFalse(TEXT("Missing statistics is unknown, not zero"), Partial.MainTasks[0].bProgressKnown);
	TestEqual(TEXT("Missing statistics state"), Partial.MainTasks[0].State, ERokServerTaskState::Unknown);

	const FRokServerTaskSnapshot Initial = Project(Role(10), Daily());
	TestEqual(TEXT("Current server main ID"), Initial.MainTaskId, 200903);
	TestEqual(TEXT("Current chapter ID"), Initial.ChapterId, 2);
	if (!TestEqual(TEXT("Exactly current main"), Initial.MainTasks.Num(), 1)) return false;
	TestEqual(TEXT("Main type is soldier training"), Initial.MainTasks[0].Type, 33);
	TestEqual(TEXT("Since-start training count"), Initial.MainTasks[0].Current, int64(10));
	TestEqual(TEXT("Main target"), Initial.MainTasks[0].Target, int64(20));
	TestEqual(TEXT("Main incomplete"), Initial.MainTasks[0].State, ERokServerTaskState::Unfinished);
	TestEqual(TEXT("Recorded daily task rows"), Initial.DailyTasks.Num(), 24);
	TestEqual(TEXT("Finished daily task sorts first"), Initial.DailyTasks[0].TaskId, 401013);
	bool bFoundDailyTraining = false;
	for (const FRokServerTaskView& View : Initial.DailyTasks)
	{
		if (View.TaskId == 401001)
		{
			bFoundDailyTraining = true;
			TestEqual(TEXT("Daily progress from server"), View.Current, int64(20));
		}
	}
	TestTrue(TEXT("Daily training row exists"), bFoundDailyTraining);
	TestEqual(TEXT("Three finished side tasks plus five candidates"), Initial.SideTasks.Num(), 8);
	TestEqual(TEXT("Finished side task first"), Initial.SideTasks[0].TaskId, 309001);
	TestEqual(TEXT("Second finished side task"), Initial.SideTasks[1].TaskId, 309002);
	TestEqual(TEXT("Third finished side task"), Initial.SideTasks[2].TaskId, 309003);
	TestEqual(TEXT("First unfinished side task"), Initial.SideTasks[3].TaskId, 301001);
	for (const FRokServerTaskView& Side : Initial.SideTasks)
		TestTrue(TEXT("Recorded visible side progress known"), Side.bProgressKnown);
	// The recorded account trains type 1; the current main task requires type 3.
	// Its denseFogOpenFlag makes 97 fog side rows locally complete. These fields
	// intentionally differ from the focused nonempty-type-3 fixture above.
	TSharedPtr<FJsonObject> RecordedRole = Role(0);
	TArray<TSharedPtr<FJsonValue>> RecordedClaims;
	for (int32 TaskId : {304001, 306163, 306164, 306165, 306166, 301351, 301096, 305001, 311002, 311003, 311001})
		RecordedClaims.Add(MakeShared<FJsonValueObject>(Row({{TEXT("taskId"), TaskId}})));
	RecordedRole->SetArrayField(TEXT("finishSideTasks"), RecordedClaims);
	TSharedPtr<FJsonObject> TrainedType = Row({{TEXT("type"), 33}});
	TrainedType->SetArrayField(TEXT("statistics"), ArrayOf({Row({{TEXT("arg"), 1}, {TEXT("num"), 20}})}));
	TSharedPtr<FJsonObject> GatheredFood = Row({{TEXT("type"), 9}});
	GatheredFood->SetArrayField(TEXT("statistics"), ArrayOf({Row({{TEXT("arg"), 1}, {TEXT("num"), 24638}})}));
	TSharedPtr<FJsonObject> Summoned = Row({{TEXT("type"), 10}});
	Summoned->SetArrayField(TEXT("statistics"), ArrayOf({Row({{TEXT("arg"), 1}, {TEXT("num"), 20}})}));
	RecordedRole->SetArrayField(TEXT("taskStatisticsSum"), ArrayOf({TrainedType, GatheredFood, Summoned}));
	RecordedRole->SetBoolField(TEXT("denseFogOpenFlag"), true);
	const FRokServerTaskSnapshot Recorded = Project(RecordedRole, Daily());
	TestEqual(TEXT("Recorded main trained different troop type"), Recorded.MainTasks[0].Current, int64(0));
	TestEqual(TEXT("Recorded main target"), Recorded.MainTasks[0].Target, int64(20));
	TestEqual(TEXT("Recorded fog-expanded side count"), Recorded.SideTasks.Num(), 105);
	for (const FRokServerTaskView& Side : Recorded.SideTasks)
		TestTrue(TEXT("Recorded side progress is known"), Side.bProgressKnown);
	TestEqual(TEXT("Recorded chapter aggregate"), Recorded.ChapterCurrent, int64(1));
	TestEqual(TEXT("Recorded daily count"), Recorded.DailyTasks.Num(), 24);
	TSharedPtr<FJsonObject> NonemptyTechRole = Role(10);
	NonemptyTechRole->SetArrayField(TEXT("technologies"), ArrayOf({
		Row({{TEXT("technologyType"), 101}, {TEXT("level"), 1}})}));
	const FRokServerTaskSnapshot TechUpdated = Project(NonemptyTechRole, Daily());
	FRokResearchCatalog Research;
	FString ResearchError;
	if (!TestTrue(TEXT("Source research catalog loads"), Research.Load(ResearchError)))
	{
		AddError(ResearchError);
		return false;
	}
	FRokResearchRow ResearchRow;
	if (!TestTrue(TEXT("Study 101 level 1 exists"), Research.GetByStudyLevel(101, 1, ResearchRow))) return false;
	bool bFoundNamedTechnology = false;
	for (const FRokServerTaskView& Side : TechUpdated.SideTasks)
	{
		if (Side.TaskId != 307001) continue;
		bFoundNamedTechnology = true;
		TestEqual(TEXT("Nonempty TechnologyInfo uses technologyType"), Side.Current, int64(1));
		TestEqual(TEXT("Technology task becomes finished"), Side.State, ERokServerTaskState::Finished);
		TestTrue(TEXT("Technology description uses source research name"), Side.Description.Contains(ResearchRow.Name));
		TestFalse(TEXT("Technology placeholder resolved"), Side.Description.Contains(TEXT("{sn1}")));
		TestFalse(TEXT("Technology raw ID absent from product description"), Side.Description.Contains(TEXT("科技101")));
	}
	TestTrue(TEXT("Named technology task remains visible"), bFoundNamedTechnology);
	TSharedPtr<FJsonObject> NoClaimChapterRole = Role(10);
	NoClaimChapterRole->SetArrayField(TEXT("chapterTasks"), TArray<TSharedPtr<FJsonValue>>());
	const FRokServerTaskSnapshot BuildingCount = Project(NoClaimChapterRole, Daily());
	for (const FRokServerTaskView& ChapterTask : BuildingCount.ChapterTasks)
	{
		if (ChapterTask.TaskId == 102001)
		{
			TestEqual(TEXT("Live level-one building counts once"), ChapterTask.Current, int64(1));
			TestEqual(TEXT("Level-zero and negative tombstones excluded"), ChapterTask.Target, int64(1));
		}
	}
	// With the one live archery camp removed, the unplaced and deleted rows
	// must not satisfy the target (clamping a false count to 1 would hide this).
	auto DeletedOnly = Buildings();
	DeletedOnly->RemoveField(TEXT("30"));
	auto DeletedProjection = URokServerTaskSubsystem::ProjectSnapshot(NoClaimChapterRole, Daily(),
		DeletedOnly, Catalog, ChapterDefs, TaskDefs);
	for (const auto& Task : DeletedProjection.ChapterTasks)
		if (Task.TaskId == 102001) TestEqual(TEXT("Deleted/unplaced buildings never complete a task"), Task.Current, int64(0));
	DeletedOnly->GetObjectField(TEXT("31"))->SetNumberField(TEXT("finishTime"), 1800000000.0);
	DeletedProjection = URokServerTaskSubsystem::ProjectSnapshot(NoClaimChapterRole, Daily(),
		DeletedOnly, Catalog, ChapterDefs, TaskDefs);
	for (const auto& Task : DeletedProjection.ChapterTasks)
		if (Task.TaskId == 102001) TestEqual(TEXT("Active level-zero construction counts as Unity does"), Task.Current, int64(1));
	TestEqual(TEXT("Four current chapter tasks"), Initial.ChapterTasks.Num(), 4);
	TestEqual(TEXT("Server-recorded chapter progress"), Initial.ChapterCurrent, int64(1));
	TestEqual(TEXT("Chapter target"), Initial.ChapterTarget, int64(4));
	TestFalse(TEXT("Chapter is not claimable"), Initial.bChapterClaimable);
	TestEqual(TEXT("Chapter received task"), Initial.ChapterTasks.Last().State, ERokServerTaskState::Received);
	TestEqual(TEXT("Chapter reward group"), Initial.ChapterRewardGroupId, 100002);
	if (TestTrue(TEXT("HUD has chapter card"), !Initial.HudTasks.IsEmpty()))
	{
		TestEqual(TEXT("Last HUD row is chapter card"), Initial.HudTasks.Last().Page, ERokServerTaskPage::Chapter);
		TestEqual(TEXT("Chapter card ID"), Initial.HudTasks.Last().TaskId, 2);
	}
	for (const FRokServerTaskView& Side : Initial.SideTasks)
	{
		TestTrue(TEXT("Received side task excluded"), Side.TaskId != 304001);
	}

	const FRokServerTaskSnapshot Finished = Project(Role(20), Daily());
	TestEqual(TEXT("Local eligible main state"), Finished.MainTasks[0].State, ERokServerTaskState::Finished);
	TestNotEqual(TEXT("Progress changes snapshot signature"), Initial.Signature, Finished.Signature);
	const FRokServerTaskSnapshot ClaimedDaily = Project(Role(20), Daily(true));
	bool bFoundClaimedDaily = false;
	for (const FRokServerTaskView& View : ClaimedDaily.DailyTasks)
	{
		if (View.TaskId == 401002)
		{
			bFoundClaimedDaily = true;
			TestEqual(TEXT("Daily -1 is received"), View.State, ERokServerTaskState::Received);
		}
	}
	TestTrue(TEXT("Claimed daily row remains represented"), bFoundClaimedDaily);
	TestNotEqual(TEXT("Daily claim changes signature"), Finished.Signature, ClaimedDaily.Signature);
	TSharedPtr<FJsonObject> CompleteRole = Role(20);
	CompleteRole->SetArrayField(TEXT("chapterTasks"), ArrayOf({
		Row({{TEXT("taskId"), 102001}, {TEXT("status"), 1}}),
		Row({{TEXT("taskId"), 102002}, {TEXT("status"), 1}}),
		Row({{TEXT("taskId"), 102003}, {TEXT("status"), 1}}),
		Row({{TEXT("taskId"), 102004}, {TEXT("status"), 1}})}));
	const FRokServerTaskSnapshot Complete = Project(CompleteRole, Daily());
	TestTrue(TEXT("Only server received status makes chapter claimable"), Complete.bChapterClaimable);
	TestEqual(TEXT("Chapter card finished"), Complete.HudTasks.Last().State, ERokServerTaskState::Finished);
	TestNotEqual(TEXT("Chapter status changes signature"), Finished.Signature, Complete.Signature);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerTaskClaimPushTest, "Rok.Net.ServerTask.ClaimPush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerTaskClaimPushTest::RunTest(const FString&)
{
	auto Player = Role(20);
	auto DailyTasks = Daily();
	const auto Applied = [&Player, &DailyTasks](ERokTaskClaim Kind, int32 Id)
	{
		return URokServerTaskSubsystem::IsClaimApplied(Kind, Id, Player, DailyTasks);
	};
	TestFalse(TEXT("Finished main task still awaits a claim push"), Applied(ERokTaskClaim::Main, 200903));
	Player->SetNumberField(TEXT("mainLineTaskId"), 200904);
	TestTrue(TEXT("New main task confirms previous claim"), Applied(ERokTaskClaim::Main, 200903));
	TestFalse(TEXT("Another side task stays unclaimed"), Applied(ERokTaskClaim::Side, 304002));
	TestTrue(TEXT("Specific side task receipt is confirmed"), Applied(ERokTaskClaim::Side, 304001));
	TestTrue(TEXT("Specific chapter task receipt is confirmed"), Applied(ERokTaskClaim::ChapterTask, 102001));
	TestFalse(TEXT("Chapter reward awaits next chapter"), Applied(ERokTaskClaim::Chapter, 2));
	Player->SetNumberField(TEXT("chapterId"), 3);
	TestTrue(TEXT("Chapter advancement confirms its reward"), Applied(ERokTaskClaim::Chapter, 2));
	TestFalse(TEXT("Positive daily progress is not a receipt"), Applied(ERokTaskClaim::Daily, 401001));
	DailyTasks->SetObjectField(TEXT("401001"), Row({{TEXT("taskId"), 401001}, {TEXT("taskSchedule"), -1}}));
	TestTrue(TEXT("Daily removal confirms claim"), Applied(ERokTaskClaim::Daily, 401001));
	Player->SetNumberField(TEXT("activePoint"), 40);
	TestFalse(TEXT("Enough points do not confirm a chest claim"), Applied(ERokTaskClaim::ActivePoints, 20));
	Player->SetArrayField(TEXT("activePointRewards"), {MakeShared<FJsonValueNumber>(20)});
	TestTrue(TEXT("The received-points array confirms the chest"), Applied(ERokTaskClaim::ActivePoints, 20));
	return true;
}

#endif
