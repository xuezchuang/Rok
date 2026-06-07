#include "RokUnityRuntimeStateSubsystem.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* RuntimeBootstrapRelativePath = TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json");

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

bool URokUnityRuntimeStateSubsystem::LoadRuntimeState()
{
	Summary = FRokUnityRuntimeSummary();
	ActiveTask = FRokUnityRuntimeTaskState();
	ActiveTaskChain.Reset();
	StarterHeroes.Reset();
	StarterTroopStacks.Reset();
	TrainableTroopIds.Reset();
	bRuntimeStateLoaded = false;

	const FString RuntimeStatePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RuntimeBootstrapRelativePath);
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
			TEXT("Loaded Rok Unity runtime bootstrap: active_task=%d progress=%d/%d heroes=%d troop_stacks=%d total_troops=%d trainable=%d"),
			Summary.ActiveTaskId,
			Summary.ActiveTaskCurrent,
			Summary.ActiveTaskTarget,
			StarterHeroes.Num(),
			StarterTroopStacks.Num(),
			Summary.TotalTroopCount,
			TrainableTroopIds.Num());
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
