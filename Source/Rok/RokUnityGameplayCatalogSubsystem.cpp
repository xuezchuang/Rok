#include "RokUnityGameplayCatalogSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* CatalogRelativePath = TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json");

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

float JsonFloatField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	if (!Object.IsValid())
	{
		return 0.0f;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return static_cast<float>(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return FCString::Atof(*StringValue);
	}
	return 0.0f;
}

FString JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FString Value;
	return Object.IsValid() && Object->TryGetStringField(FieldName, Value) ? Value : FString();
}

int32 JsonArrayCount(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Array) && Array)
	{
		return Array->Num();
	}
	return 0;
}

FRokResourceCost JsonCostField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	FRokResourceCost Cost;
	const TSharedPtr<FJsonObject> CostObject = JsonObjectField(Object, FieldName);
	Cost.Food = JsonIntField(CostObject, TEXT("food"));
	Cost.Wood = JsonIntField(CostObject, TEXT("wood"));
	Cost.Stone = JsonIntField(CostObject, TEXT("stone"));
	Cost.Gold = JsonIntField(CostObject, TEXT("gold"));
	return Cost;
}

template <typename ValueType>
TArray<int32> SortedKeys(const TMap<int32, ValueType>& Map)
{
	TArray<int32> Keys;
	Map.GetKeys(Keys);
	Keys.Sort();
	return Keys;
}
}

void URokUnityGameplayCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
}

bool URokUnityGameplayCatalogSubsystem::LoadCatalog()
{
	BuildingTypes.Reset();
	Troops.Reset();
	Heroes.Reset();
	HeroSkills.Reset();
	HeroTalents.Reset();
	MainTasks.Reset();
	Summary = FRokUnityGameplayCatalogSummary();
	bCatalogLoaded = false;

	const FString CatalogPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), CatalogRelativePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *CatalogPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity gameplay catalog not found: %s"), *CatalogPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity gameplay catalog is invalid JSON: %s"), *CatalogPath);
		return false;
	}

	Summary = ParseSummary(JsonObjectField(RootObject, TEXT("summary")));

	const TArray<TSharedPtr<FJsonValue>>* BuildingValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("building_types"), BuildingValues) && BuildingValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *BuildingValues)
		{
			FRokUnityCatalogBuildingType BuildingType = ParseBuildingType(Value.IsValid() ? Value->AsObject() : nullptr);
			if (BuildingType.TypeId > 0)
			{
				BuildingTypes.Add(BuildingType.TypeId, MoveTemp(BuildingType));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TroopValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("troops"), TroopValues) && TroopValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *TroopValues)
		{
			FRokUnityCatalogTroop Troop = ParseTroop(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Troop.Id > 0)
			{
				Troops.Add(Troop.Id, MoveTemp(Troop));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* HeroValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("heroes"), HeroValues) && HeroValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *HeroValues)
		{
			FRokUnityCatalogHero Hero = ParseHero(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Hero.Id > 0)
			{
				Heroes.Add(Hero.Id, MoveTemp(Hero));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* HeroSkillValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("hero_skills"), HeroSkillValues) && HeroSkillValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *HeroSkillValues)
		{
			FRokUnityCatalogHeroSkill Skill = ParseHeroSkill(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Skill.Id > 0)
			{
				HeroSkills.Add(Skill.Id, MoveTemp(Skill));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* HeroTalentValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("hero_talents"), HeroTalentValues) && HeroTalentValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *HeroTalentValues)
		{
			FRokUnityCatalogHeroTalent Talent = ParseHeroTalent(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Talent.Id > 0)
			{
				HeroTalents.Add(Talent.Id, MoveTemp(Talent));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TaskValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("main_tasks"), TaskValues) && TaskValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *TaskValues)
		{
			FRokUnityCatalogTask Task = ParseTask(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Task.Id > 0)
			{
				MainTasks.Add(Task.Id, MoveTemp(Task));
			}
		}
	}

	bCatalogLoaded = BuildingTypes.Num() > 0 && Troops.Num() > 0 && Heroes.Num() > 0 && HeroSkills.Num() > 0 && HeroTalents.Num() > 0 && MainTasks.Num() > 0;
	if (bCatalogLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Loaded Rok Unity gameplay catalog: buildings=%d levels=%d troops=%d heroes=%d tasks=%d skills=%d talents=%d"),
			Summary.BuildingTypeCount,
			Summary.BuildingLevelRowCount,
			Summary.TroopCount,
			Summary.HeroCount,
			Summary.MainTaskCount,
			Summary.HeroSkillCount,
			Summary.HeroTalentCount);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok Unity gameplay catalog loaded with missing sections: %s"), *CatalogPath);
	}
	return bCatalogLoaded;
}

bool URokUnityGameplayCatalogSubsystem::GetBuildingType(int32 TypeId, FRokUnityCatalogBuildingType& OutBuildingType) const
{
	if (const FRokUnityCatalogBuildingType* Found = BuildingTypes.Find(TypeId))
	{
		OutBuildingType = *Found;
		return true;
	}
	OutBuildingType = FRokUnityCatalogBuildingType();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetTroop(int32 Id, FRokUnityCatalogTroop& OutTroop) const
{
	if (const FRokUnityCatalogTroop* Found = Troops.Find(Id))
	{
		OutTroop = *Found;
		return true;
	}
	OutTroop = FRokUnityCatalogTroop();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetHero(int32 Id, FRokUnityCatalogHero& OutHero) const
{
	if (const FRokUnityCatalogHero* Found = Heroes.Find(Id))
	{
		OutHero = *Found;
		return true;
	}
	OutHero = FRokUnityCatalogHero();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetHeroSkill(int32 Id, FRokUnityCatalogHeroSkill& OutSkill) const
{
	if (const FRokUnityCatalogHeroSkill* Found = HeroSkills.Find(Id))
	{
		OutSkill = *Found;
		return true;
	}
	OutSkill = FRokUnityCatalogHeroSkill();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetHeroTalent(int32 Id, FRokUnityCatalogHeroTalent& OutTalent) const
{
	if (const FRokUnityCatalogHeroTalent* Found = HeroTalents.Find(Id))
	{
		OutTalent = *Found;
		return true;
	}
	OutTalent = FRokUnityCatalogHeroTalent();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetMainTask(int32 Id, FRokUnityCatalogTask& OutTask) const
{
	if (const FRokUnityCatalogTask* Found = MainTasks.Find(Id))
	{
		OutTask = *Found;
		return true;
	}
	OutTask = FRokUnityCatalogTask();
	return false;
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetBuildingTypeIds() const
{
	return SortedKeys(BuildingTypes);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetTroopIds() const
{
	return SortedKeys(Troops);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetHeroIds() const
{
	return SortedKeys(Heroes);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetHeroSkillIds() const
{
	return SortedKeys(HeroSkills);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetHeroTalentIds() const
{
	return SortedKeys(HeroTalents);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetMainTaskIds() const
{
	return SortedKeys(MainTasks);
}

FRokUnityGameplayCatalogSummary URokUnityGameplayCatalogSubsystem::ParseSummary(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityGameplayCatalogSummary Parsed;
	Parsed.BuildingTypeCount = JsonIntField(Object, TEXT("building_type_count"));
	Parsed.BuildingLevelRowCount = JsonIntField(Object, TEXT("building_level_row_count"));
	Parsed.BuildingMenuCount = JsonIntField(Object, TEXT("building_menu_count"));
	Parsed.BuildingCountLimitRowCount = JsonIntField(Object, TEXT("building_count_limit_row_count"));
	Parsed.BuildingProduceRowCount = JsonIntField(Object, TEXT("building_produce_row_count"));
	Parsed.TroopCount = JsonIntField(Object, TEXT("troop_count"));
	Parsed.HeroCount = JsonIntField(Object, TEXT("hero_count"));
	Parsed.HeroSkillCount = JsonIntField(Object, TEXT("hero_skill_count"));
	Parsed.HeroTalentCount = JsonIntField(Object, TEXT("hero_talent_count"));
	Parsed.MainTaskCount = JsonIntField(Object, TEXT("main_task_count"));
	return Parsed;
}

FRokUnityCatalogBuildingType URokUnityGameplayCatalogSubsystem::ParseBuildingType(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogBuildingType Parsed;
	Parsed.TypeId = JsonIntField(Object, TEXT("type_id"));
	Parsed.Group = JsonIntField(Object, TEXT("group"));

	const TSharedPtr<FJsonObject> SizeObject = JsonObjectField(Object, TEXT("size"));
	Parsed.Width = JsonIntField(SizeObject, TEXT("width"));
	Parsed.Length = JsonIntField(SizeObject, TEXT("length"));
	Parsed.MenuButtonCount = JsonArrayCount(Object, TEXT("menu_buttons"));

	const TArray<TSharedPtr<FJsonValue>>* MenuValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("menu_buttons"), MenuValues) && MenuValues)
	{
		TArray<FString> Functions;
		for (const TSharedPtr<FJsonValue>& Value : *MenuValues)
		{
			const FString FunctionName = JsonStringField(Value.IsValid() ? Value->AsObject() : nullptr, TEXT("func"));
			if (!FunctionName.IsEmpty())
			{
				Functions.AddUnique(FunctionName);
			}
		}
		Parsed.MenuFunctions = FString::Join(Functions, TEXT(","));
	}

	const TSharedPtr<FJsonObject> LevelSummaryObject = JsonObjectField(Object, TEXT("level_summary"));
	Parsed.LevelCount = JsonIntField(LevelSummaryObject, TEXT("count"));
	Parsed.MinLevel = JsonIntField(LevelSummaryObject, TEXT("min_level"));
	Parsed.MaxLevel = JsonIntField(LevelSummaryObject, TEXT("max_level"));
	Parsed.TotalPower = JsonIntField(LevelSummaryObject, TEXT("total_power"));

	const TSharedPtr<FJsonObject> Level1Object = JsonObjectField(LevelSummaryObject, TEXT("level1"));
	Parsed.Level1BuildSeconds = JsonIntField(Level1Object, TEXT("build_seconds"));
	Parsed.Level1Power = JsonIntField(Level1Object, TEXT("power"));
	Parsed.Level1Cost = JsonCostField(Level1Object, TEXT("cost"));

	const TSharedPtr<FJsonObject> Level2Object = JsonObjectField(LevelSummaryObject, TEXT("level2"));
	Parsed.Level2BuildSeconds = JsonIntField(Level2Object, TEXT("build_seconds"));
	Parsed.Level2Power = JsonIntField(Level2Object, TEXT("power"));
	Parsed.Level2Cost = JsonCostField(Level2Object, TEXT("cost"));

	Parsed.ProductionLevelCount = JsonArrayCount(Object, TEXT("production"));
	Parsed.bHasProduction = Parsed.ProductionLevelCount > 0;
	return Parsed;
}

FRokUnityCatalogTroop URokUnityGameplayCatalogSubsystem::ParseTroop(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogTroop Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.ArmsType = JsonIntField(Object, TEXT("arms_type"));
	Parsed.SubType = JsonIntField(Object, TEXT("sub_type"));
	Parsed.Level = JsonIntField(Object, TEXT("level"));
	Parsed.Age = JsonIntField(Object, TEXT("age"));

	const TSharedPtr<FJsonObject> StatsObject = JsonObjectField(Object, TEXT("stats"));
	Parsed.Attack = JsonIntField(StatsObject, TEXT("attack"));
	Parsed.Defense = JsonIntField(StatsObject, TEXT("defense"));
	Parsed.Hp = JsonIntField(StatsObject, TEXT("hp"));
	Parsed.Speed = JsonFloatField(StatsObject, TEXT("speed"));
	Parsed.Capacity = JsonIntField(StatsObject, TEXT("capacity"));
	Parsed.Power = JsonIntField(StatsObject, TEXT("power"));

	Parsed.TrainCost = JsonCostField(Object, TEXT("train_cost"));
	Parsed.TrainSeconds = JsonIntField(Object, TEXT("train_seconds"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.Model = JsonStringField(Object, TEXT("model"));
	return Parsed;
}

FRokUnityCatalogHero URokUnityGameplayCatalogSubsystem::ParseHero(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogHero Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.Civilization = JsonIntField(Object, TEXT("civilization"));
	Parsed.InitStar = JsonIntField(Object, TEXT("init_star"));
	Parsed.MaxStar = JsonIntField(Object, TEXT("max_star"));
	Parsed.Rare = JsonIntField(Object, TEXT("rare"));
	Parsed.Score = JsonIntField(Object, TEXT("score"));
	Parsed.GetItem = JsonIntField(Object, TEXT("get_item"));
	Parsed.GetItemNum = JsonIntField(Object, TEXT("get_item_num"));
	Parsed.SkillCount = JsonArrayCount(Object, TEXT("skill_ids"));
	Parsed.TalentCount = JsonArrayCount(Object, TEXT("talent_ids"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.Model = JsonStringField(Object, TEXT("model"));
	return Parsed;
}

FRokUnityCatalogHeroSkill URokUnityGameplayCatalogSubsystem::ParseHeroSkill(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogHeroSkill Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.BattleSkillId = JsonIntField(Object, TEXT("battle_skill_id"));
	Parsed.Anger = JsonIntField(Object, TEXT("anger"));
	Parsed.Open = JsonIntField(Object, TEXT("open"));
	Parsed.AwakenEnhance = JsonIntField(Object, TEXT("awaken_enhance"));
	Parsed.LevelUpCostItem = JsonIntField(Object, TEXT("level_up_cost_item"));
	Parsed.ArmyEffect = JsonStringField(Object, TEXT("army_effect"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	Parsed.Sound = JsonStringField(Object, TEXT("sound"));
	return Parsed;
}

FRokUnityCatalogHeroTalent URokUnityGameplayCatalogSubsystem::ParseHeroTalent(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogHeroTalent Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.GainTree = JsonIntField(Object, TEXT("gain_tree"));
	Parsed.MasteryGroupId = JsonIntField(Object, TEXT("mastery_group_id"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	return Parsed;
}

FRokUnityCatalogTask URokUnityGameplayCatalogSubsystem::ParseTask(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogTask Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.Name = JsonStringField(Object, TEXT("name"));
	Parsed.Desc = JsonStringField(Object, TEXT("desc"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.Param1 = JsonIntField(Object, TEXT("param1"));
	Parsed.Param2 = JsonIntField(Object, TEXT("param2"));
	Parsed.Require = JsonIntField(Object, TEXT("require"));
	Parsed.Reward = JsonIntField(Object, TEXT("reward"));
	Parsed.NextId = JsonIntField(Object, TEXT("next_id"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	return Parsed;
}
