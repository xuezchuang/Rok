#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokDataPaths.h"
#include "Engine/World.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* RuntimeBootstrapRelativePath = TEXT("unity_runtime_bootstrap_state.json");

TSharedPtr<FJsonObject> JsonObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FieldName);
	return Value && Value->IsValid() ? (*Value)->AsObject() : nullptr;
}

int32 JsonIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return FMath::RoundToInt(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi(*StringValue);
	}
	return 0;
}

int64 JsonInt64Field(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return static_cast<int64>(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atoi64(*StringValue);
	}
	return 0;
}

bool JsonBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return false;
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return !FMath::IsNearlyZero(NumberValue);
	}
	return false;
}

FString JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : FString();
}

void ReadIntArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TArray<int32>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid())
		{
			continue;
		}

		if (Value->Type == EJson::Number)
		{
			OutValues.Add(FMath::RoundToInt(Value->AsNumber()));
		}
		else if (Value->Type == EJson::String)
		{
			OutValues.Add(FCString::Atoi(*Value->AsString()));
		}
	}
}

template <typename ParsedType>
void ReadObjectArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, TFunctionRef<ParsedType(const TSharedPtr<FJsonObject>&)> Parser, TArray<ParsedType>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(FieldName, Values) || !Values)
	{
		return;
	}

	OutValues.Reserve(Values->Num());
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		TSharedPtr<FJsonObject> ValueObject = Value.IsValid() ? Value->AsObject() : nullptr;
		if (ValueObject.IsValid())
		{
			OutValues.Add(Parser(ValueObject));
		}
	}
}
}

void URokUnityRuntimeStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadRuntimeState();
}

FRokUnityRuntimePlayerProfileState URokUnityRuntimeStateSubsystem::GetPlayerProfileState() const
{
	FRokUnityRuntimePlayerProfileState Result = PlayerProfileState;
	if (bRuntimeStateLoaded)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				Result.LocalBuildingUpgradeScore = Buildings->GetLocalCompletedBuildingPowerDelta();
				Result.LocalPreviewScore = int32(FMath::Clamp<int64>(int64(Result.LocalPreviewScore)
					+ Result.LocalBuildingUpgradeScore, 0, MAX_int32));
			}
		}
	}
	// The bootstrap remains immutable in the GameInstance. Leaving this world
	// resets its local building progress and this projection together.
	return Result;
}

FRokUnityRuntimeTaskLocalProjection URokUnityRuntimeStateSubsystem::GetActiveTaskLocalProjection(
	const FRokBuildingState* HallState) const
{
	return ProjectLocalBuildingLevelTask(ActiveTask, PlayerProfileState, bRuntimeStateLoaded, HallState);
}

FRokUnityRuntimeTaskLocalProjection URokUnityRuntimeStateSubsystem::ProjectLocalBuildingLevelTask(
	const FRokUnityRuntimeTaskState& Task, const FRokUnityRuntimePlayerProfileState& Profile,
	bool bRuntimeLoaded, const FRokBuildingState* HallState)
{
	FRokUnityRuntimeTaskLocalProjection Result;
	Result.Task = Task;
	// Unity TaskProxy.GetTaskStatisticsNum evaluates UpgradeBuilding from
	// CityBuildingProxy.GetMaxLevelofType. The current local city has one source-owned
	// Hall, InitBuilding id 5. This is only its completed-level display condition;
	// TaskScriptGlobalMediator/TaskProxy still own account task/reward progression.
	if (!bRuntimeLoaded || Profile.StateKind != TEXT("local_preview")
		|| Profile.bHasRoleId || !Profile.RoleId.IsEmpty()
		|| Profile.bHasCivilization || Profile.CivilizationId != 0 || !Profile.CivilizationName.IsEmpty()
		|| Task.TaskId != 200900 || Task.Type != 8 || Task.Param1 != 1 || Task.Param2 != 2
		|| Task.Target != 2 || Task.ProgressRule != TEXT("building_level") || Task.bComplete
		|| !HallState || HallState->BuildingKey != FName(TEXT("UnityInitId_5"))
		|| HallState->UnityInitId != 5 || HallState->UnityBuildingTypeId != 1
		|| HallState->Type != ERokBuildingType::TownCenter || HallState->Level < 1)
	{
		return Result;
	}
	Result.bProjected = true;
	Result.Task.Current = FMath::Min(HallState->Level, Task.Target);
	Result.bConditionMet = Result.Task.Current >= Task.Target;
	// Never mark Task.bComplete or advance NextId. That is server/account state.
	return Result;
}

bool URokUnityRuntimeStateSubsystem::LoadRuntimeState()
{
	Summary = FRokUnityRuntimeSummary();
	PlayerProfileState = FRokUnityRuntimePlayerProfileState();
	ActiveTask = FRokUnityRuntimeTaskState();
	ActiveTaskChain.Reset();
	StarterHeroes.Reset();
	StarterTroopStacks.Reset();
	TrainableTroopIds.Reset();
	bRuntimeStateLoaded = false;

	const FString RuntimeStatePath = FPaths::ConvertRelativePathToFull(RokDataPaths::Resolve(RuntimeBootstrapRelativePath));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *RuntimeStatePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity runtime bootstrap not found: %s"), *RuntimeStatePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity runtime bootstrap is invalid JSON: %s"), *RuntimeStatePath);
		return false;
	}

	Summary = ParseSummary(JsonObjectField(RootObject, TEXT("summary")));
	PlayerProfileState = ParsePlayerProfileState(JsonObjectField(RootObject, TEXT("player_runtime")));

	const TSharedPtr<FJsonObject> MainTaskRuntimeObject = JsonObjectField(RootObject, TEXT("main_task_runtime"));
	ActiveTask = ParseTaskState(JsonObjectField(MainTaskRuntimeObject, TEXT("active_task")));
	ReadObjectArray<FRokUnityRuntimeTaskState>(
		MainTaskRuntimeObject,
		TEXT("active_chain"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityRuntimeStateSubsystem::ParseTaskState(Object); },
		ActiveTaskChain);

	const TSharedPtr<FJsonObject> HeroRuntimeObject = JsonObjectField(RootObject, TEXT("hero_runtime"));
	ReadObjectArray<FRokUnityRuntimeHeroState>(
		HeroRuntimeObject,
		TEXT("starter_heroes"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityRuntimeStateSubsystem::ParseHeroState(Object); },
		StarterHeroes);

	const TSharedPtr<FJsonObject> TroopRuntimeObject = JsonObjectField(RootObject, TEXT("troop_runtime"));
	ReadObjectArray<FRokUnityRuntimeTroopStack>(
		TroopRuntimeObject,
		TEXT("starter_stacks"),
		[](const TSharedPtr<FJsonObject>& Object) { return URokUnityRuntimeStateSubsystem::ParseTroopStack(Object); },
		StarterTroopStacks);
	ReadIntArray(TroopRuntimeObject, TEXT("trainable_troop_ids"), TrainableTroopIds);

	bRuntimeStateLoaded = ActiveTask.TaskId > 0 && StarterHeroes.Num() > 0 && StarterTroopStacks.Num() > 0;
	if (bRuntimeStateLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Loaded Rok Unity runtime bootstrap: active_task=%d progress=%d/%d heroes=%d troop_stacks=%d total_troops=%d trainable=%d player_state=%s"),
			Summary.ActiveTaskId,
			Summary.ActiveTaskCurrent,
			Summary.ActiveTaskTarget,
			StarterHeroes.Num(),
			StarterTroopStacks.Num(),
			Summary.TotalTroopCount,
			TrainableTroopIds.Num(),
			*PlayerProfileState.StateKind);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity runtime bootstrap loaded with missing sections: %s"), *RuntimeStatePath);
	}
	return bRuntimeStateLoaded;
}

FText URokUnityRuntimeStateSubsystem::GetRuntimeStatusText() const
{
	if (!bRuntimeStateLoaded)
	{
		return NSLOCTEXT("RokUnityRuntime", "RuntimeNotLoaded", "Runtime bootstrap not loaded.");
	}

	return FText::Format(
		NSLOCTEXT("RokUnityRuntime", "RuntimeStatus", "Task {0} {1} {2}/{3} | Heroes {4} | Troops {5}"),
		FText::AsNumber(ActiveTask.TaskId),
		FText::FromString(ActiveTask.Name),
		FText::AsNumber(ActiveTask.Current),
		FText::AsNumber(ActiveTask.Target),
		FText::AsNumber(StarterHeroes.Num()),
		FText::AsNumber(Summary.TotalTroopCount));
}

FRokUnityRuntimeSummary URokUnityRuntimeStateSubsystem::ParseSummary(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityRuntimeSummary Parsed;
	Parsed.ActiveTaskId = JsonIntField(Object, TEXT("active_task_id"));
	Parsed.ActiveTaskCurrent = JsonIntField(Object, TEXT("active_task_current"));
	Parsed.ActiveTaskTarget = JsonIntField(Object, TEXT("active_task_target"));
	Parsed.bActiveTaskComplete = JsonBoolField(Object, TEXT("active_task_complete"));
	Parsed.ActiveTaskChainCount = JsonIntField(Object, TEXT("active_task_chain_count"));
	Parsed.ReferenceTaskChainId = JsonIntField(Object, TEXT("reference_task_chain_id"));
	Parsed.ReferenceTaskChainCount = JsonIntField(Object, TEXT("reference_task_chain_count"));
	Parsed.StarterHeroCount = JsonIntField(Object, TEXT("starter_hero_count"));
	Parsed.StarterTroopStackCount = JsonIntField(Object, TEXT("starter_troop_stack_count"));
	Parsed.TotalTroopCount = JsonIntField(Object, TEXT("total_troop_count"));
	Parsed.TotalTroopPower = JsonIntField(Object, TEXT("total_troop_power"));
	Parsed.TotalHeroScore = JsonIntField(Object, TEXT("total_hero_score"));
	Parsed.TrainableTroopCount = JsonIntField(Object, TEXT("trainable_troop_count"));
	return Parsed;
}

FRokUnityRuntimePlayerProfileState URokUnityRuntimeStateSubsystem::ParsePlayerProfileState(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityRuntimePlayerProfileState Parsed;
	Parsed.StateKind = JsonStringField(Object, TEXT("state_kind"));
	Parsed.DefaultHeadId = JsonIntField(Object, TEXT("default_head_id"));
	Parsed.DefaultFrameId = JsonIntField(Object, TEXT("default_frame_id"));
	Parsed.bHasRoleId = JsonBoolField(Object, TEXT("has_role_id"));
	Parsed.RoleId = JsonStringField(Object, TEXT("role_id"));
	Parsed.bHasDisplayName = JsonBoolField(Object, TEXT("has_display_name"));
	Parsed.DisplayName = JsonStringField(Object, TEXT("display_name"));
	Parsed.bHasCivilization = JsonBoolField(Object, TEXT("has_civilization"));
	Parsed.CivilizationId = JsonIntField(Object, TEXT("civilization_id"));
	Parsed.CivilizationName = JsonStringField(Object, TEXT("civilization_name"));
	Parsed.bHasAlliance = JsonBoolField(Object, TEXT("has_alliance"));
	Parsed.AllianceName = JsonStringField(Object, TEXT("alliance_name"));
	Parsed.bHasKillCount = JsonBoolField(Object, TEXT("has_kill_count"));
	Parsed.KillCount = JsonInt64Field(Object, TEXT("kill_count"));
	Parsed.bHasActionPoints = JsonBoolField(Object, TEXT("has_action_points"));
	Parsed.ActionPointsCurrent = JsonIntField(Object, TEXT("action_points_current"));
	Parsed.ActionPointsMax = JsonIntField(Object, TEXT("action_points_max"));
	Parsed.bHasVipLevel = JsonBoolField(Object, TEXT("has_vip_level"));
	Parsed.VipLevel = JsonIntField(Object, TEXT("vip_level"));
	Parsed.LocalHeroScore = JsonIntField(Object, TEXT("local_hero_score"));
	Parsed.LocalTroopPower = JsonIntField(Object, TEXT("local_troop_power"));
	Parsed.LocalPreviewScore = JsonIntField(Object, TEXT("local_preview_score"));
	return Parsed;
}

FRokUnityRuntimeTaskState URokUnityRuntimeStateSubsystem::ParseTaskState(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityRuntimeTaskState Parsed;
	Parsed.TaskId = JsonIntField(Object, TEXT("task_id"));
	Parsed.Name = JsonStringField(Object, TEXT("name"));
	Parsed.Desc = JsonStringField(Object, TEXT("desc"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.Param1 = JsonIntField(Object, TEXT("param1"));
	Parsed.Param2 = JsonIntField(Object, TEXT("param2"));
	Parsed.Require = JsonIntField(Object, TEXT("require"));
	Parsed.Reward = JsonIntField(Object, TEXT("reward"));
	Parsed.NextId = JsonIntField(Object, TEXT("next_id"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));

	const TSharedPtr<FJsonObject> ProgressObject = JsonObjectField(Object, TEXT("progress"));
	Parsed.ProgressRule = JsonStringField(ProgressObject, TEXT("rule"));
	Parsed.Current = JsonIntField(ProgressObject, TEXT("current"));
	Parsed.Target = JsonIntField(ProgressObject, TEXT("target"));
	Parsed.bComplete = JsonBoolField(ProgressObject, TEXT("complete"));
	return Parsed;
}

FRokUnityRuntimeHeroState URokUnityRuntimeStateSubsystem::ParseHeroState(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityRuntimeHeroState Parsed;
	Parsed.HeroId = JsonIntField(Object, TEXT("hero_id"));
	Parsed.Level = JsonIntField(Object, TEXT("level"));
	Parsed.Star = JsonIntField(Object, TEXT("star"));
	Parsed.MaxStar = JsonIntField(Object, TEXT("max_star"));
	Parsed.Rare = JsonIntField(Object, TEXT("rare"));
	Parsed.Score = JsonIntField(Object, TEXT("score"));
	Parsed.SkillCount = 0;
	Parsed.TalentCount = 0;

	const TArray<TSharedPtr<FJsonValue>>* SkillValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("skill_ids"), SkillValues) && SkillValues)
	{
		Parsed.SkillCount = SkillValues->Num();
	}
	const TArray<TSharedPtr<FJsonValue>>* TalentValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("talent_ids"), TalentValues) && TalentValues)
	{
		Parsed.TalentCount = TalentValues->Num();
	}

	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.Model = JsonStringField(Object, TEXT("model"));
	return Parsed;
}

FRokUnityRuntimeTroopStack URokUnityRuntimeStateSubsystem::ParseTroopStack(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityRuntimeTroopStack Parsed;
	Parsed.TroopId = JsonIntField(Object, TEXT("troop_id"));
	Parsed.ArmsType = JsonIntField(Object, TEXT("arms_type"));
	Parsed.Level = JsonIntField(Object, TEXT("level"));
	Parsed.Count = JsonIntField(Object, TEXT("count"));
	Parsed.PowerPerUnit = JsonIntField(Object, TEXT("power_per_unit"));
	Parsed.TotalPower = JsonIntField(Object, TEXT("total_power"));
	Parsed.TrainSeconds = JsonIntField(Object, TEXT("train_seconds"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.Model = JsonStringField(Object, TEXT("model"));
	return Parsed;
}
