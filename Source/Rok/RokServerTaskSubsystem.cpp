#include "RokServerTaskSubsystem.h"
#include "RokUnityConfigSubsystem.h"

#include "RokGameNetSubsystem.h"
#include "RokResearchCatalog.h"
#include "RokSproto.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Misc/Crc.h"

namespace
{
bool ReadInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int64& Out)
{
	return Object.IsValid() && RokSprotoJson::TryGetInt64(Object, Field, Out);
}

TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(Field, Value) ? *Value : nullptr;
}

TArray<TSharedPtr<FJsonObject>> Rows(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field)
{
	TArray<TSharedPtr<FJsonObject>> Result;
	if (!Parent.IsValid()) return Result;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Parent->TryGetArrayField(Field, Array))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Row) && Row) Result.Add(*Row);
		}
	}
	else if (TSharedPtr<FJsonObject> Object = Child(Parent, Field))
	{
		for (const auto& Pair : Object->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Row) && Row) Result.Add(*Row);
		}
	}
	return Result;
}

bool HasTaskId(const TArray<TSharedPtr<FJsonObject>>& RowsIn, int32 Id, int32* Status = nullptr)
{
	for (const TSharedPtr<FJsonObject>& Row : RowsIn)
	{
		int64 RowId = 0;
		if (ReadInt(Row, TEXT("taskId"), RowId) && RowId == Id)
		{
			if (Status) { int64 Value = 0; *Status = ReadInt(Row, TEXT("status"), Value) ? int32(Value) : 0; }
			return true;
		}
	}
	return false;
}

bool BuildingProgress(const TSharedPtr<FJsonObject>& Buildings, int32 Type, bool bCount, int64& Out)
{
	if (!Buildings.IsValid()) return false;
	Out = 0;
	for (const auto& Pair : Buildings->Values)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		int64 RowType = 0, Level = 0, FinishTime = -1;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row
			|| !ReadInt(*Row, TEXT("type"), RowType)) continue;
		if (RowType != Type) continue;
		ReadInt(*Row, TEXT("finishTime"), FinishTime);
		const bool bHasLevel = ReadInt(*Row, TEXT("level"), Level);
		if (bHasLevel && Level < 0) continue; // Merged server deletion tombstone.
		if (bCount)
		{
			// CityBuildingProxy.GetBuildCount ignores unplaced/cancelled rows (finishTime == 0).
			if (FinishTime != 0) ++Out;
		}
		else if (bHasLevel && Level > 0 && FinishTime != 0)
			Out = FMath::Max(Out, Level);
	}
	return true;
}

bool TechnologyProgress(const TSharedPtr<FJsonObject>& Role, int32 Type, int64& Out)
{
	if (!Role.IsValid() || !Role->HasField(TEXT("technologies"))) return false;
	Out = 0;
	for (const TSharedPtr<FJsonObject>& Row : Rows(Role, TEXT("technologies")))
	{
		int64 RowType = 0, Level = 0;
		if ((!ReadInt(Row, TEXT("technologyType"), RowType)
			&& !ReadInt(Row, TEXT("type"), RowType) && !ReadInt(Row, TEXT("id"), RowType))
			|| RowType != Type) continue;
		if (ReadInt(Row, TEXT("level"), Level)) Out = FMath::Max(Out, Level);
	}
	return true;
}

FString CleanMarkup(const FString& Source)
{
	FString Result;
	bool bInTag = false;
	for (TCHAR Char : Source)
	{
		if (Char == TEXT('<')) { bInTag = true; continue; }
		if (Char == TEXT('>') && bInTag) { bInTag = false; continue; }
		if (!bInTag) Result.AppendChar(Char);
	}
	return Result.TrimStartAndEnd();
}

FString FormatDescription(const FRokUnityCatalogTask& Task)
{
	FString Result = Task.Desc;
	Result.ReplaceInline(TEXT("{p1}"), *FText::AsNumber(Task.Param1).ToString(), ESearchCase::CaseSensitive);
	Result.ReplaceInline(TEXT("{p2}"), *FText::AsNumber(Task.Param2).ToString(), ESearchCase::CaseSensitive);
	Result.ReplaceInline(TEXT("{p3}"), *FText::AsNumber(Task.Require).ToString(), ESearchCase::CaseSensitive);
	FString SourceName = FText::AsNumber(Task.Param1).ToString();
	if (Task.Type == 6 && Task.Param1 > 0)
	{
		static FRokResearchCatalog Research;
		static const bool bResearchLoaded = []()
		{
			FString Error;
			return Research.Load(Error);
		}();
		FRokResearchRow Study;
		if (bResearchLoaded && Research.GetByStudyLevel(Task.Param1, 1, Study) && !Study.Name.IsEmpty())
			SourceName = Study.Name;
	}
	// TaskProxy.GetSn1 uses numeric param1 for all non-research task types.
	Result.ReplaceInline(TEXT("{sn1}"), *SourceName, ESearchCase::CaseSensitive);
	return CleanMarkup(Result);
}

bool StatisticProgress(const TSharedPtr<FJsonObject>& Role, int32 Type, int32 Arg, int64& Out)
{
	if (!Role.IsValid() || !Role->HasField(TEXT("taskStatisticsSum"))) return false;
	Out = 0;
	for (const TSharedPtr<FJsonObject>& TypeRow : Rows(Role, TEXT("taskStatisticsSum")))
	{
		int64 RowType = 0;
		if (!ReadInt(TypeRow, TEXT("type"), RowType) || RowType != Type) continue;
		for (const TSharedPtr<FJsonObject>& Stat : Rows(TypeRow, TEXT("statistics")))
		{
			int64 RowArg = 0, Num = 0;
			if (ReadInt(Stat, TEXT("arg"), RowArg) && RowArg == (Arg > 0 ? Arg : -1)
				&& ReadInt(Stat, TEXT("num"), Num)) { Out = Num; return true; }
		}
	}
	return true; // The server treats a missing type/argument in a present statistics set as zero.
}

bool AnyResourceProgress(const TSharedPtr<FJsonObject>& Role, int32 Type, int64& Out)
{
	if (!Role.IsValid() || !Role->HasField(TEXT("taskStatisticsSum"))) return false;
	Out = 0;
	for (const TSharedPtr<FJsonObject>& TypeRow : Rows(Role, TEXT("taskStatisticsSum")))
	{
		int64 RowType = 0;
		if (!ReadInt(TypeRow, TEXT("type"), RowType) || RowType != Type) continue;
		for (const TSharedPtr<FJsonObject>& Stat : Rows(TypeRow, TEXT("statistics")))
		{
			int64 Num = 0;
			if (ReadInt(Stat, TEXT("num"), Num)) Out = FMath::Max(Out, Num);
		}
	}
	return true;
}

void Progress(FRokServerTaskView& View, const TSharedPtr<FJsonObject>& Role,
	const TSharedPtr<FJsonObject>& Buildings)
{
	View.Target = View.Require;
	if ((View.Type == 8 || View.Type == 6) && View.Param1 > 0) View.Target = View.Param2;
	if (View.Type == 40) View.Target = View.Param1;
	int64 Value = 0;
	switch (View.Type)
	{
	case 8: // BUILDING_UPGRADE: a named building's maximum level, else since-start statistic.
		View.bProgressKnown = View.Param1 > 0 ? BuildingProgress(Buildings, View.Param1, false, Value)
			: StatisticProgress(Role, View.Type, 0, Value); break;
	case 13: // BUILDING_NUM
		View.bProgressKnown = BuildingProgress(Buildings, View.Param1, true, Value); break;
	case 14: // FOG_EXPLORE: server's open-fog override.
		if (Role.IsValid() && Role->HasTypedField<EJson::Boolean>(TEXT("denseFogOpenFlag"))
			&& Role->GetBoolField(TEXT("denseFogOpenFlag"))) { Value = View.Target; View.bProgressKnown = true; }
		else View.bProgressKnown = StatisticProgress(Role, View.Type, 0, Value);
		break;
	case 15: View.bProgressKnown = ReadInt(Role, TEXT("historyPower"), Value); break;
	case 6: // Named technology levels live in RoleInfo.technologies.
		View.bProgressKnown = View.Param1 > 0 ? TechnologyProgress(Role, View.Param1, Value)
			: StatisticProgress(Role, View.Type, 0, Value);
		break;
	case 9: case 11: // Any resource: server accepts any individual matching statistic.
		View.bProgressKnown = View.Param1 > 0 ? StatisticProgress(Role, View.Type, View.Param1, Value)
			: AnyResourceProgress(Role, View.Type, Value); break;
	case 7: // Server currently accepts SOLDIER_NUM unconditionally; roster is not task progress.
	case 17: // Hero collection and rules are not part of this projection.
	case 19:
	case 28: // Resource production needs per-level output and capacity modifiers.
	case 40: // Expedition reaches a checkpoint, not an event counter.
		break;
	case 1: case 2: case 3: case 4: case 5: case 10: case 16:
	case 18: case 20: case 21: case 22: case 23: case 24: case 25:
	case 26: case 27: case 29: case 30: case 31: case 32: case 33:
	case 34: case 37: case 38: case 42: case 43: case 44: case 45:
	case 46: case 47:
		// TaskLogic.lua checks these through taskStatisticsSum, with param1 or -1.
		View.bProgressKnown = StatisticProgress(Role, View.Type, View.Param1, Value);
		break;
	default: break;
	}
	if (View.bProgressKnown) View.Current = FMath::Clamp<int64>(Value, 0, FMath::Max<int64>(0, View.Target));
}

FRokServerTaskView MakeView(const FRokUnityCatalogTask& Task, ERokServerTaskPage Page,
	const TSharedPtr<FJsonObject>& Role, const TSharedPtr<FJsonObject>& Buildings)
{
	FRokServerTaskView View;
	View.TaskId = Task.Id; View.Type = Task.Type; View.Param1 = Task.Param1;
	View.Param2 = Task.Param2; View.Require = Task.Require; View.RewardGroupId = Task.Reward;
	View.Page = Page; View.Name = CleanMarkup(Task.Name);
	View.Description = FormatDescription(Task); View.Icon = Task.Icon;
	Progress(View, Role, Buildings);
	View.State = View.bProgressKnown
		? (View.Current >= View.Target ? ERokServerTaskState::Finished : ERokServerTaskState::Unfinished)
		: ERokServerTaskState::Unknown;
	return View;
}

void AppendSignature(FString& Text, const FRokServerTaskView& View)
{
	Text += FString::Printf(TEXT("|%d,%d,%d,%d,%d,%d,%d,%d,%d,%lld,%lld,%d;%s;%s;%s"),
		int32(View.Page), View.TaskId, View.Type, View.Param1, View.Param2, View.Require,
		View.RewardGroupId, int32(View.State), int32(View.bProgressKnown), View.Current, View.Target,
		View.Name.Len(), *View.Name, *View.Description, *View.Icon);
}
}

void URokServerTaskSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Error;
	RokTaskChapterConfigPreviewData::LoadAll(Chapters, ChapterRows, Error);
}

bool URokServerTaskSubsystem::IsClaimApplied(ERokTaskClaim Kind, int32 Id,
	const TSharedPtr<FJsonObject>& Role, const TSharedPtr<FJsonObject>& Daily)
{
	int64 Value = 0;
	switch (Kind)
	{
	case ERokTaskClaim::Main:
		return ReadInt(Role, TEXT("mainLineTaskId"), Value) && Value != Id;
	case ERokTaskClaim::Side:
		return HasTaskId(Rows(Role, TEXT("finishSideTasks")), Id);
	case ERokTaskClaim::ChapterTask:
	{
		int32 Status = 0;
		return HasTaskId(Rows(Role, TEXT("chapterTasks")), Id, &Status) && Status == 1;
	}
	case ERokTaskClaim::Daily:
		return ReadInt(Child(Daily, *FString::FromInt(Id)), TEXT("taskSchedule"), Value) && Value < 0;
	case ERokTaskClaim::Chapter:
		return ReadInt(Role, TEXT("chapterId"), Value) && Value != Id;
	case ERokTaskClaim::ActivePoints:
	{
		const TArray<TSharedPtr<FJsonValue>>* Received = nullptr;
		return Role && Role->TryGetArrayField(TEXT("activePointRewards"), Received)
			&& Received->ContainsByPredicate([Id](const TSharedPtr<FJsonValue>& Entry)
			{
				int64 Points = 0;
				return Entry && Entry->TryGetNumber(Points) && Points == Id;
			});
	}
	}
	return false;
}

bool URokServerTaskSubsystem::Claim(ERokTaskClaim Kind, int32 Id,
	TFunction<void(const FString&)> OnResult, FString& OutError)
{
	URokGameNetSubsystem* Net = GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
	const FRokServerTaskSnapshot Snapshot = GetSnapshot();
	const TArray<FRokServerTaskView>* Tasks = nullptr;
	switch (Kind)
	{
	case ERokTaskClaim::Main: Tasks = &Snapshot.MainTasks; break;
	case ERokTaskClaim::Side: Tasks = &Snapshot.SideTasks; break;
	case ERokTaskClaim::ChapterTask: Tasks = &Snapshot.ChapterTasks; break;
	case ERokTaskClaim::Daily: Tasks = &Snapshot.DailyTasks; break;
	default: break;
	}
	const bool bClaimable = Tasks ? Tasks->ContainsByPredicate([Id](const FRokServerTaskView& Row)
	{
		return Row.TaskId == Id && Row.State == ERokServerTaskState::Finished;
	}) : Kind == ERokTaskClaim::Chapter ? Snapshot.ChapterId == Id && Snapshot.bChapterClaimable
		: Snapshot.DailyMilestones.ContainsByPredicate([Id](const FRokServerTaskMilestone& Row)
		{
			return Row.Points == Id && Row.State == ERokServerTaskState::Finished;
		});
	if (!Net || !Snapshot.bLoaded || !bClaimable || Id <= 0)
	{
		OutError = TEXT("奖励尚不可领取，请查看当前任务进度。");
		return false;
	}
	return Net->RequestTaskClaim(Kind, Id, MoveTemp(OnResult), OutError);
}

FRokServerTaskSnapshot URokServerTaskSubsystem::GetSnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (TestSnapshot.IsSet()) return TestSnapshot.GetValue();
#endif
	FRokServerTaskSnapshot Empty;
	const UGameInstance* Instance = GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady) return Empty;
	const auto Role = Net->GetRoleInfo();
	FRokServerTaskSnapshot Snapshot = ProjectSnapshot(Role, Net->GetCollection(TEXT("Task_TaskInfo"), TEXT("taskInfo")),
		Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo")),
		Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>(), Chapters, ChapterRows);
	int64 Level = 0;
	if (!ReadInt(Role, TEXT("activePoint"), Snapshot.DailyActivePoints)
		|| !ReadInt(Role, TEXT("level"), Level)) return Snapshot;
	if (auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>())
	{
		Snapshot.Signature += FString::Printf(TEXT("|active=%lld"), Snapshot.DailyActivePoints);
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			FRokServerTaskMilestone Row;
			if (!Config->GetIntFieldByKey(TEXT("TaskActivityReward"), TEXT("age"), Level,
				*FString::Printf(TEXT("activePoints%d"), Index), Row.Points)
				|| !Config->GetIntFieldByKey(TEXT("TaskActivityReward"), TEXT("age"), Level,
					*FString::Printf(TEXT("reward%d"), Index), Row.RewardGroupId)) continue;
			Row.State = IsClaimApplied(ERokTaskClaim::ActivePoints, Row.Points, Role, nullptr)
				? ERokServerTaskState::Received : Snapshot.DailyActivePoints >= Row.Points
					? ERokServerTaskState::Finished : ERokServerTaskState::Unfinished;
			Snapshot.Signature += FString::Printf(TEXT("|%d:%d"), Row.Points, int32(Row.State));
			Snapshot.DailyMilestones.Add(Row);
		}
	}
	return Snapshot;
}

FRokServerTaskSnapshot URokServerTaskSubsystem::ProjectSnapshot(
	const TSharedPtr<FJsonObject>& Role, const TSharedPtr<FJsonObject>& DailyTaskInfo,
	const TSharedPtr<FJsonObject>& Buildings, const URokUnityGameplayCatalogSubsystem* Catalog,
	const TArray<FRokTaskChapterPreviewRow>& ChapterDefinitions,
	const TArray<FRokTaskChapterTaskPreviewRow>& ChapterDefinitionsTasks)
{
	FRokServerTaskSnapshot Result;
	Result.bAvailable = true;
	int64 MainId = 0, ChapterId = 0;
	Result.bLoaded = Role.IsValid() && Catalog && Catalog->IsCatalogLoaded()
		&& ReadInt(Role, TEXT("mainLineTaskId"), MainId)
		&& ReadInt(Role, TEXT("chapterId"), ChapterId);
	Result.bDailyLoaded = DailyTaskInfo.IsValid();
	if (!Result.bLoaded)
	{
		Result.Signature = Result.bDailyLoaded ? TEXT("role-ready:partial:daily") : TEXT("role-ready:partial");
		return Result;
	}
	int64 Id = 0;
	if (ReadInt(Role, TEXT("mainLineTaskId"), Id)) Result.MainTaskId = int32(Id);
	if (ReadInt(Role, TEXT("chapterId"), Id)) Result.ChapterId = int32(Id);
	const auto ReceivedSide = Rows(Role, TEXT("finishSideTasks"));
	const auto ChapterStatuses = Rows(Role, TEXT("chapterTasks"));
	if (Result.MainTaskId > 0)
	{
		FRokUnityCatalogTask Row;
		if (Catalog->GetMainTask(Result.MainTaskId, Row))
			Result.MainTasks.Add(MakeView(Row, ERokServerTaskPage::Main, Role, Buildings));
	}
	TSet<int32> SeenGroups;
	int32 UnfinishedCount = 0;
	TArray<FRokServerTaskView> UnfinishedSides;
	if (Role->HasField(TEXT("finishSideTasks")))
	for (const FRokUnityCatalogTask& Row : Catalog->GetSideTasksBySortOrder())
	{
		if (HasTaskId(ReceivedSide, Row.Id)) continue;
		FRokServerTaskView View = MakeView(Row, ERokServerTaskPage::Side, Role, Buildings);
		if (View.State == ERokServerTaskState::Finished)
		{
			Result.SideTasks.Add(MoveTemp(View));
			continue;
		}
		if (UnfinishedCount >= 5 || SeenGroups.Contains(Row.Group1)) continue;
		int64 RoleLevel = 0, BuildingLevel = 0;
		if (!ReadInt(Role, TEXT("level"), RoleLevel) || RoleLevel < Row.ShowLevelReq) continue;
		if (Row.ShowBuildingReq > 0 && (!BuildingProgress(Buildings, Row.ShowBuildingReq, false, BuildingLevel)
			|| BuildingLevel <= 0)) continue;
		SeenGroups.Add(Row.Group1);
		UnfinishedSides.Add(MoveTemp(View));
		++UnfinishedCount;
	}
	Result.SideTasks.Append(UnfinishedSides);
	if (Result.ChapterId > 0)
	{
		for (const FRokTaskChapterPreviewRow& Chapter : ChapterDefinitions)
		{
			if (Chapter.Id != Result.ChapterId) continue;
			Result.ChapterName = CleanMarkup(Chapter.DisplayTitle());
			Result.ChapterDescription = CleanMarkup(Chapter.Description1.ZhCn);
			Result.ChapterRewardGroupId = Chapter.RewardGroupId;
			break;
		}
		for (const FRokTaskChapterTaskPreviewRow& Row : ChapterDefinitionsTasks)
		{
			if (Row.ChapterId != Result.ChapterId) continue;
			FRokUnityCatalogTask Task;
			if (!Catalog->GetChapterTask(Row.Id, Task)) continue;
			FRokServerTaskView View = MakeView(Task, ERokServerTaskPage::Chapter, Role, Buildings);
			int32 Status = 0;
			if (HasTaskId(ChapterStatuses, Row.Id, &Status))
			{
				if (Status == 1) { View.State = ERokServerTaskState::Received; ++Result.ChapterCurrent; }
				else if (Status == -1) View.State = ERokServerTaskState::Unknown;
			}
			Result.ChapterTasks.Add(MoveTemp(View));
		}
		Result.ChapterTasks.Sort([&](const FRokServerTaskView& A, const FRokServerTaskView& B)
		{
			const auto Rank = [](ERokServerTaskState State)
			{
				switch (State)
				{
				case ERokServerTaskState::Finished: return 0;
				case ERokServerTaskState::Unfinished: return 1;
				case ERokServerTaskState::Received: return 2;
				default: return 3;
				}
			};
			if (Result.ChapterId != 1 && A.State != B.State) return Rank(A.State) < Rank(B.State);
			int32 OrderA = 0, OrderB = 0;
			for (const auto& Row : ChapterDefinitionsTasks)
			{
				if (Row.Id == A.TaskId) OrderA = Row.Order;
				if (Row.Id == B.TaskId) OrderB = Row.Order;
			}
			return OrderA == OrderB ? A.TaskId < B.TaskId : OrderA < OrderB;
		});
		Result.ChapterTarget = Result.ChapterTasks.Num();
		Result.bChapterProgressKnown = Role->HasField(TEXT("chapterTasks")) && Result.ChapterTarget > 0;
		Result.bChapterClaimable = Result.bChapterProgressKnown && Result.ChapterCurrent == Result.ChapterTarget;
	}
	if (DailyTaskInfo.IsValid())
	{
		for (const auto& Pair : DailyTaskInfo->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			int64 TaskId = 0, Schedule = 0;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row
				|| !ReadInt(*Row, TEXT("taskId"), TaskId)
				|| !ReadInt(*Row, TEXT("taskSchedule"), Schedule)) continue;
			FRokUnityCatalogTask Task;
			if (!Catalog->GetDailyTask(int32(TaskId), Task)) continue;
			FRokServerTaskView View = MakeView(Task, ERokServerTaskPage::Daily, Role, Buildings);
			View.Target = Task.Require; View.bProgressKnown = true;
			View.Current = FMath::Clamp<int64>(Schedule, 0, View.Target);
			View.State = Schedule < 0 ? ERokServerTaskState::Received
				: (Schedule >= Task.Require ? ERokServerTaskState::Finished : ERokServerTaskState::Unfinished);
			Result.DailyTasks.Add(MoveTemp(View));
		}
		Result.DailyTasks.Sort([&](const FRokServerTaskView& A, const FRokServerTaskView& B)
		{
			const auto Rank = [](ERokServerTaskState State)
			{
				return State == ERokServerTaskState::Finished ? 0
					: (State == ERokServerTaskState::Unfinished ? 1
						: (State == ERokServerTaskState::Received ? 2 : 3));
			};
			if (A.State != B.State) return Rank(A.State) < Rank(B.State);
			FRokUnityCatalogTask RowA, RowB;
			Catalog->GetDailyTask(A.TaskId, RowA); Catalog->GetDailyTask(B.TaskId, RowB);
			return RowA.Order == RowB.Order ? A.TaskId < B.TaskId : RowA.Order < RowB.Order;
		});
	}
	Result.HudTasks.Append(Result.MainTasks);
	Result.HudTasks.Append(Result.SideTasks);
	if (Result.ChapterId > 0)
	{
		FRokServerTaskView Card;
		Card.Page = ERokServerTaskPage::Chapter; Card.TaskId = Result.ChapterId;
		Card.Name = Result.ChapterName; Card.Description = Result.ChapterDescription;
		Card.RewardGroupId = Result.ChapterRewardGroupId;
		Card.bProgressKnown = Result.bChapterProgressKnown;
		Card.Current = Result.ChapterCurrent; Card.Target = Result.ChapterTarget;
		Card.State = Result.bChapterClaimable ? ERokServerTaskState::Finished
			: (Result.bChapterProgressKnown ? ERokServerTaskState::Unfinished : ERokServerTaskState::Unknown);
		Result.HudTasks.Add(MoveTemp(Card));
	}
	FString SignatureText = FString::Printf(TEXT("%d,%d,%d,%d,%d,%lld,%lld,%d,%d,%d;%s;%s"),
		Result.bAvailable, Result.bLoaded, Result.bDailyLoaded, Result.MainTaskId, Result.ChapterId,
		Result.ChapterCurrent, Result.ChapterTarget, Result.bChapterProgressKnown,
		Result.bChapterClaimable, Result.ChapterRewardGroupId,
		*Result.ChapterName, *Result.ChapterDescription);
	for (const auto& Row : Result.MainTasks) AppendSignature(SignatureText, Row);
	for (const auto& Row : Result.SideTasks) AppendSignature(SignatureText, Row);
	for (const auto& Row : Result.ChapterTasks) AppendSignature(SignatureText, Row);
	for (const auto& Row : Result.DailyTasks) AppendSignature(SignatureText, Row);
	Result.Signature = FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*SignatureText));
	return Result;
}
