#include "RokUnityGameplayCatalogSubsystem.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* CatalogRelativePath = TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json");
const TCHAR* LanguageTableRelativePath = TEXT("Saved/RokUnityMigration/ConfigTables/Language.json");
const TCHAR* ActivityCalendarTableRelativePath = TEXT("Saved/RokUnityMigration/ConfigTables/ActivityCalendar.json");
const TCHAR* AllianceOfficiallyTableRelativePath = TEXT("Saved/RokUnityMigration/ConfigTables/AllianceOfficially.json");
const TCHAR* AllianceTechnologyRelativePath = TEXT("Saved/RokUnityMigration/AllianceTechnologyReadonlyCatalogV1.json");

bool LoadReadonlyConfigTable(const TCHAR* RelativePath, const TCHAR* TableName,
	int32 ExpectedRows, TArray<TSharedPtr<FJsonValue>>& OutRows)
{
	FString Text;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RelativePath);
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
	FString Name;
	double RowCount = 0;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetStringField(TEXT("table_name"), Name) || Name != TableName
		|| !Root->TryGetNumberField(TEXT("row_count"), RowCount) || RowCount != ExpectedRows
		|| !Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows || Rows->Num() != ExpectedRows) return false;
	OutRows = *Rows;
	return true;
}

bool ReadStrictNonnegativeInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, int32& Out)
{
	if (!Object.IsValid()) return false;
	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Name);
	if (!Value || !Value->IsValid() || (*Value)->Type != EJson::Number) return false;
	const double Number = (*Value)->AsNumber();
	if (!FMath::IsFinite(Number) || Number < 0 || Number > MAX_int32
		|| static_cast<double>(static_cast<int32>(Number)) != Number) return false;
	Out = static_cast<int32>(Number);
	return true;
}

bool ReadStrictString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, FString& Out)
{
	if (!Object.IsValid()) return false;
	const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Name);
	if (!Value || !Value->IsValid() || (*Value)->Type != EJson::String) return false;
	Out = (*Value)->AsString();
	return true;
}

bool HasBrokenLocale(const FString& Value)
{
	int32 Position = INDEX_NONE;
	return Value.FindChar(static_cast<TCHAR>(0xFFFD), Position);
}

bool ResolveAllianceOfficeLocale(const TMap<int32, FString>& Chinese, int32 LocaleId, FString& Out)
{
	Out.Reset();
	const FString* Found = Chinese.Find(LocaleId);
	if (!Found || Found->IsEmpty() || HasBrokenLocale(*Found)) return false;
	Out = *Found;
	return true;
}

bool FormatAllianceOfficeBonus(const FString& SourceTemplate, const FString& SourceValues, FString& Out)
{
	Out.Reset();
	if (SourceTemplate.IsEmpty() || HasBrokenLocale(SourceTemplate) || SourceValues.IsEmpty()) return false;
	FString Clean = SourceTemplate;
	Clean.ReplaceInline(TEXT("<color=green>"), TEXT(""), ESearchCase::CaseSensitive);
	Clean.ReplaceInline(TEXT("</color>"), TEXT(""), ESearchCase::CaseSensitive);
	TArray<FString> Values;
	SourceValues.ParseIntoArray(Values, TEXT("|"), false);
	if (Values.Num() == 0) return false;
	for (const FString& Value : Values)
	{
		if (Value.IsEmpty() || !Value.IsNumeric()) return false;
	}
	int32 Replacements = 0;
	for (int32 Index = 0; Index < Values.Num(); ++Index)
	{
		const FString Token = FString::Printf(TEXT("{%d}"), Index);
		if (Clean.Contains(Token))
		{
			Clean.ReplaceInline(*Token, *Values[Index], ESearchCase::CaseSensitive);
			++Replacements;
		}
	}
	if (Replacements == 0 || Clean.Contains(TEXT("{")) || Clean.Contains(TEXT("}"))) return false;
	Out = MoveTemp(Clean);
	return true;
}

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

TArray<int32> JsonIntArrayField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	TArray<int32> Result;
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(FieldName, Array) && Array)
	{
		Result.Reserve(Array->Num());
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			if (!Value.IsValid())
			{
				Result.Add(0);
				continue;
			}

			double NumberValue = 0.0;
			if (Value->TryGetNumber(NumberValue))
			{
				Result.Add(FMath::RoundToInt(NumberValue));
				continue;
			}

			FString StringValue;
			Result.Add(Value->TryGetString(StringValue) ? FCString::Atoi(*StringValue) : 0);
		}
	}
	return Result;
}

int32 JsonArrayCount(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
{
	const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
	return Object.IsValid()
		&& Object->TryGetArrayField(FieldName, Array)
		&& Array
		? Array->Num()
		: 0;
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

void ParseIconMetadata(
	const TSharedPtr<FJsonObject>& Object,
	FString& OutAtlas,
	FString& OutSprite,
	int32& OutX,
	int32& OutY,
	int32& OutWidth,
	int32& OutHeight,
	int32& OutAtlasWidth,
	int32& OutAtlasHeight)
{
	OutAtlas = JsonStringField(Object, TEXT("icon_atlas"));
	OutSprite = JsonStringField(Object, TEXT("icon_sprite"));

	const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(TEXT("icon_rect"), Rect)
		&& Rect
		&& Rect->Num() == 4)
	{
		OutX = FMath::RoundToInt((*Rect)[0]->AsNumber());
		OutY = FMath::RoundToInt((*Rect)[1]->AsNumber());
		OutWidth = FMath::RoundToInt((*Rect)[2]->AsNumber());
		OutHeight = FMath::RoundToInt((*Rect)[3]->AsNumber());
	}

	const TArray<TSharedPtr<FJsonValue>>* AtlasSize = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(TEXT("icon_atlas_size"), AtlasSize)
		&& AtlasSize
		&& AtlasSize->Num() == 2)
	{
		OutAtlasWidth = FMath::RoundToInt((*AtlasSize)[0]->AsNumber());
		OutAtlasHeight = FMath::RoundToInt((*AtlasSize)[1]->AsNumber());
	}
}

template <typename ValueType>
TArray<int32> SortedKeys(const TMap<int32, ValueType>& Map)
{
	TArray<int32> Keys;
	Map.GetKeys(Keys);
	Keys.Sort();
	return Keys;
}

bool ValidateHeroCatalogRelationships(
	const TMap<int32, FRokUnityCatalogHero>& Heroes,
	const TMap<int32, FRokUnityCatalogHeroSkill>& HeroSkills,
	const TMap<int32, FRokUnityCatalogHeroTalent>& HeroTalents)
{
	bool bValid = true;
	for (const TPair<int32, FRokUnityCatalogHero>& HeroPair : Heroes)
	{
		const FRokUnityCatalogHero& Hero = HeroPair.Value;
		if (Hero.NameLocaleId <= 0
			|| Hero.AppellationLocaleId <= 0
			|| Hero.DisplayNameZhCn.IsEmpty()
			|| Hero.DisplayNameEn.IsEmpty()
			|| Hero.DisplayTitleZhCn.IsEmpty()
			|| Hero.DisplayTitleEn.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Hero %d catalog display localization is incomplete"), Hero.Id);
			bValid = false;
		}

		if (Hero.InitStar <= 0 || Hero.MaxStar < Hero.InitStar || Hero.Rare <= 0 || Hero.BasePower <= 0 || Hero.BasePower != Hero.Score)
		{
			UE_LOG(LogTemp, Warning, TEXT("Hero %d catalog base presentation fields are invalid"), Hero.Id);
			bValid = false;
		}

		TSet<int32> SeenSkillIds;
		for (const int32 SkillId : Hero.SkillIds)
		{
			if (SkillId <= 0 || SeenSkillIds.Contains(SkillId))
			{
				UE_LOG(LogTemp, Warning, TEXT("Hero %d has an invalid or duplicate skill ID %d"), Hero.Id, SkillId);
				bValid = false;
				continue;
			}
			SeenSkillIds.Add(SkillId);

			const FRokUnityCatalogHeroSkill* Skill = HeroSkills.Find(SkillId);
			if (!Skill || Skill->DisplayNameZhCn.IsEmpty() || Skill->DisplayNameEn.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Hero %d references missing or unlocalized skill %d"), Hero.Id, SkillId);
				bValid = false;
			}
		}

		TSet<int32> SeenTalentIds;
		for (const int32 TalentId : Hero.TalentIds)
		{
			if (TalentId <= 0 || SeenTalentIds.Contains(TalentId))
			{
				UE_LOG(LogTemp, Warning, TEXT("Hero %d has an invalid or duplicate talent ID %d"), Hero.Id, TalentId);
				bValid = false;
				continue;
			}
			SeenTalentIds.Add(TalentId);

			const FRokUnityCatalogHeroTalent* Talent = HeroTalents.Find(TalentId);
			if (!Talent || Talent->DisplayNameZhCn.IsEmpty() || Talent->DisplayNameEn.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("Hero %d references missing or unlocalized talent %d"), Hero.Id, TalentId);
				bValid = false;
			}
		}
	}
	return bValid;
}
}

void URokUnityGameplayCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadCatalog();
	LoadVipDailyPointHelpText();
}

void URokUnityGameplayCatalogSubsystem::LoadVipDailyPointHelpText()
{
	VipDailyPointHelpText.Reset();
	const FString LanguagePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), LanguageTableRelativePath);
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *LanguagePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok VIP help locale table not found: %s"), *LanguagePath);
		return;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok VIP help locale table is invalid JSON: %s"), *LanguagePath);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("rows"), Rows) || !Rows)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok VIP help locale table has no rows: %s"), *LanguagePath);
		return;
	}

	bool bFoundTarget = false;
	FString CandidateText;
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
		double LocaleId = 0.0;
		if (!Row.IsValid() || !Row->TryGetNumberField(TEXT("id"), LocaleId) || LocaleId != 800032.0)
		{
			continue;
		}
		if (bFoundTarget || !Row->TryGetStringField(TEXT("cn"), CandidateText)
			|| CandidateText.IsEmpty() || CandidateText.Contains(TEXT("\uFFFD")))
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok VIP help locale 800032 is duplicate or invalid: %s"), *LanguagePath);
			return;
		}
		bFoundTarget = true;
	}

	if (!bFoundTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok VIP help locale 800032 is missing: %s"), *LanguagePath);
		return;
	}
	VipDailyPointHelpText = MoveTemp(CandidateText);
}

bool URokUnityGameplayCatalogSubsystem::GetVipDailyPointHelpText(FText& OutText) const
{
	OutText = FText::GetEmpty();
	if (VipDailyPointHelpText.IsEmpty() || VipDailyPointHelpText.Contains(TEXT("\uFFFD")))
	{
		return false;
	}
	OutText = FText::FromString(VipDailyPointHelpText);
	return true;
}

bool URokUnityGameplayCatalogSubsystem::LoadCatalog()
{
	BuildingTypes.Reset();
	Troops.Reset();
	Civilizations.Reset();
	Heroes.Reset();
	HeroSkills.Reset();
	HeroTalents.Reset();
	MainTasks.Reset();
	SideTasks.Reset();
	DailyTasks.Reset();
	ChapterTasks.Reset();
	VipLevels.Reset();
	VipAttributes.Reset();
	VipDayPoints.Reset();
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

	const TArray<TSharedPtr<FJsonValue>>* CivilizationValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("civilizations"), CivilizationValues) && CivilizationValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *CivilizationValues)
		{
			FRokUnityCatalogCivilization Civilization = ParseCivilization(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Civilization.Id > 0)
			{
				Civilizations.Add(Civilization.Id, MoveTemp(Civilization));
			}
		}
	}

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

	const TArray<TSharedPtr<FJsonValue>>* SideTaskValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("side_tasks"), SideTaskValues) && SideTaskValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *SideTaskValues)
		{
			FRokUnityCatalogTask Task = ParseTask(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Task.Id > 0)
			{
				SideTasks.Add(Task.Id, MoveTemp(Task));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* DailyTaskValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("daily_tasks"), DailyTaskValues) && DailyTaskValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *DailyTaskValues)
		{
			FRokUnityCatalogTask Task = ParseTask(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Task.Id > 0)
			{
				DailyTasks.Add(Task.Id, MoveTemp(Task));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ChapterTaskValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("chapter_tasks"), ChapterTaskValues) && ChapterTaskValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *ChapterTaskValues)
		{
			FRokUnityCatalogTask Task = ParseTask(Value.IsValid() ? Value->AsObject() : nullptr);
			if (Task.Id > 0)
			{
				ChapterTasks.Add(Task.Id, MoveTemp(Task));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* VipLevelValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("vip_levels"), VipLevelValues) && VipLevelValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *VipLevelValues)
		{
			FRokUnityCatalogVipLevel VipLevel = ParseVipLevel(Value.IsValid() ? Value->AsObject() : nullptr);
			if (VipLevel.Id > 0)
			{
				VipLevels.Add(VipLevel.Level, MoveTemp(VipLevel));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* VipAttributeValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("vip_attributes"), VipAttributeValues) && VipAttributeValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *VipAttributeValues)
		{
			FRokUnityCatalogVipAttribute VipAttribute = ParseVipAttribute(Value.IsValid() ? Value->AsObject() : nullptr);
			if (VipAttribute.Id > 0 && VipAttribute.LocaleId > 0)
			{
				VipAttributes.Add(MoveTemp(VipAttribute));
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* VipDayPointValues = nullptr;
	if (RootObject->TryGetArrayField(TEXT("vip_day_points"), VipDayPointValues) && VipDayPointValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *VipDayPointValues)
		{
			FRokUnityCatalogVipDayPoint VipDayPoint = ParseVipDayPoint(Value.IsValid() ? Value->AsObject() : nullptr);
			if (VipDayPoint.Id > 0)
			{
				VipDayPoints.Add(VipDayPoint.Day, MoveTemp(VipDayPoint));
			}
		}
	}

	const bool bHeroSectionCountsValid = Heroes.Num() == Summary.HeroCount
		&& HeroSkills.Num() == Summary.HeroSkillCount
		&& HeroTalents.Num() == Summary.HeroTalentCount;
	const bool bHeroRelationshipsValid = ValidateHeroCatalogRelationships(Heroes, HeroSkills, HeroTalents);
	TSet<int32> ReferencedHeroSkillIds;
	TSet<int32> ReferencedHeroTalentIds;
	for (const TPair<int32, FRokUnityCatalogHero>& HeroPair : Heroes)
	{
		for (const int32 SkillId : HeroPair.Value.SkillIds)
		{
			ReferencedHeroSkillIds.Add(SkillId);
		}
		for (const int32 TalentId : HeroPair.Value.TalentIds)
		{
			ReferencedHeroTalentIds.Add(TalentId);
		}
	}
	const bool bHeroRelationshipCountsValid =
		ReferencedHeroSkillIds.Num() == Summary.HeroReferencedSkillCount
		&& ReferencedHeroTalentIds.Num() == Summary.HeroReferencedTalentCount;
	const bool bAdditionalTaskCountsValid =
		(Summary.SideTaskCount <= 0 || SideTasks.Num() == Summary.SideTaskCount)
		&& (Summary.DailyTaskCount <= 0 || DailyTasks.Num() == Summary.DailyTaskCount)
		&& (Summary.ChapterTaskCount <= 0 || ChapterTasks.Num() == Summary.ChapterTaskCount);
	const int32 CivilizationRowCount = CivilizationValues ? CivilizationValues->Num() : 0;
	const bool bCivilizationSectionCountValid = CivilizationRowCount == 7
		&& Civilizations.Num() == CivilizationRowCount
		&& Summary.CivilizationCount == CivilizationRowCount;
	if (!bHeroSectionCountsValid || !bHeroRelationshipCountsValid)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Hero catalog counts do not match summary: heroes=%d/%d skills=%d/%d talents=%d/%d referencedSkills=%d/%d referencedTalents=%d/%d"),
			Heroes.Num(),
			Summary.HeroCount,
			HeroSkills.Num(),
			Summary.HeroSkillCount,
			HeroTalents.Num(),
			Summary.HeroTalentCount,
			ReferencedHeroSkillIds.Num(),
			Summary.HeroReferencedSkillCount,
			ReferencedHeroTalentIds.Num(),
			Summary.HeroReferencedTalentCount);
	}
	if (!bCivilizationSectionCountValid)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Civilization catalog count does not match required visible rows: array=%d unique=%d summary=%d expected=7"),
			CivilizationRowCount,
			Civilizations.Num(),
			Summary.CivilizationCount);
	}

	bCatalogLoaded = BuildingTypes.Num() > 0
		&& Troops.Num() > 0
		&& bCivilizationSectionCountValid
		&& Heroes.Num() > 0
		&& HeroSkills.Num() > 0
		&& HeroTalents.Num() > 0
		&& bHeroSectionCountsValid
		&& bHeroRelationshipsValid
		&& bHeroRelationshipCountsValid
		&& MainTasks.Num() > 0
		&& bAdditionalTaskCountsValid
		&& VipLevels.Num() == 18
		&& VipAttributes.Num() > 0
		&& VipDayPoints.Num() == 8;
	if (bCatalogLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Loaded Rok Unity gameplay catalog: buildings=%d levels=%d troops=%d civilizations=%d heroes=%d tasks=%d side=%d daily=%d chapter=%d skills=%d talents=%d vip=%d vipAtt=%d"),
			Summary.BuildingTypeCount,
			Summary.BuildingLevelRowCount,
			Summary.TroopCount,
			Summary.CivilizationCount,
			Summary.HeroCount,
			Summary.MainTaskCount,
			Summary.SideTaskCount,
			Summary.DailyTaskCount,
			Summary.ChapterTaskCount,
			Summary.HeroSkillCount,
			Summary.HeroTalentCount,
			Summary.VipLevelCount,
			Summary.VipAttributeCount);
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

TArray<FRokUnityCatalogTroop>
URokUnityGameplayCatalogSubsystem::GetTroopsForArmyType(int32 ArmsType) const
{
	TArray<FRokUnityCatalogTroop> Result;
	for (const TPair<int32, FRokUnityCatalogTroop>& Pair : Troops)
	{
		const FRokUnityCatalogTroop& Troop = Pair.Value;
		if (Troop.ArmsType == ArmsType
			&& Troop.SubType == 1
			&& Troop.Level >= 1
			&& Troop.Level <= 5)
		{
			Result.Add(Troop);
		}
	}
	Result.Sort(
		[](const FRokUnityCatalogTroop& Left,
			const FRokUnityCatalogTroop& Right)
		{
			return Left.Level == Right.Level
				? Left.Id < Right.Id
				: Left.Level < Right.Level;
		});
	return Result;
}

TArray<FRokUnityCatalogCivilization> URokUnityGameplayCatalogSubsystem::GetCivilizations() const
{
	TArray<FRokUnityCatalogCivilization> Result;
	Civilizations.GenerateValueArray(Result);
	Result.Sort([](const FRokUnityCatalogCivilization& Left, const FRokUnityCatalogCivilization& Right)
	{
		return Left.Rank == Right.Rank
			? Left.Id < Right.Id
			: Left.Rank < Right.Rank;
	});
	return Result;
}

bool URokUnityGameplayCatalogSubsystem::GetCivilization(int32 Id, FRokUnityCatalogCivilization& OutCivilization) const
{
	if (const FRokUnityCatalogCivilization* Found = Civilizations.Find(Id))
	{
		OutCivilization = *Found;
		return true;
	}
	OutCivilization = FRokUnityCatalogCivilization();
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

bool URokUnityGameplayCatalogSubsystem::GetSideTask(int32 Id, FRokUnityCatalogTask& OutTask) const
{
	if (const FRokUnityCatalogTask* Found = SideTasks.Find(Id))
	{
		OutTask = *Found;
		return true;
	}
	OutTask = FRokUnityCatalogTask();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetDailyTask(int32 Id, FRokUnityCatalogTask& OutTask) const
{
	if (const FRokUnityCatalogTask* Found = DailyTasks.Find(Id))
	{
		OutTask = *Found;
		return true;
	}
	OutTask = FRokUnityCatalogTask();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetChapterTask(int32 Id, FRokUnityCatalogTask& OutTask) const
{
	if (const FRokUnityCatalogTask* Found = ChapterTasks.Find(Id))
	{
		OutTask = *Found;
		return true;
	}
	OutTask = FRokUnityCatalogTask();
	return false;
}

bool URokUnityGameplayCatalogSubsystem::GetVipLevel(int32 Level, FRokUnityCatalogVipLevel& OutVipLevel) const
{
	if (const FRokUnityCatalogVipLevel* Found = VipLevels.Find(Level))
	{
		OutVipLevel = *Found;
		return true;
	}
	OutVipLevel = FRokUnityCatalogVipLevel();
	return false;
}

TArray<FRokUnityCatalogVipAttribute> URokUnityGameplayCatalogSubsystem::GetVipAttributesForLevel(int32 Level) const
{
	TArray<FRokUnityCatalogVipAttribute> Result;
	for (const FRokUnityCatalogVipAttribute& Attribute : VipAttributes)
	{
		if (Attribute.LevelGroup == Level)
		{
			Result.Add(Attribute);
		}
	}
	Result.Sort([](const FRokUnityCatalogVipAttribute& Left, const FRokUnityCatalogVipAttribute& Right)
	{
		return Left.Id < Right.Id;
	});
	return Result;
}

bool URokUnityGameplayCatalogSubsystem::GetVipDayPoint(int32 Day, FRokUnityCatalogVipDayPoint& OutDayPoint) const
{
	const int32 ClampedDay = FMath::Clamp(Day, 1, 8);
	if (const FRokUnityCatalogVipDayPoint* Found = VipDayPoints.Find(ClampedDay))
	{
		OutDayPoint = *Found;
		return true;
	}
	OutDayPoint = FRokUnityCatalogVipDayPoint();
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

TArray<int32> URokUnityGameplayCatalogSubsystem::GetSideTaskIds() const
{
	return SortedKeys(SideTasks);
}

TArray<FRokUnityCatalogTask> URokUnityGameplayCatalogSubsystem::GetSideTasksBySortOrder() const
{
	TArray<FRokUnityCatalogTask> Tasks;
	SideTasks.GenerateValueArray(Tasks);
	Tasks.Sort([](const FRokUnityCatalogTask& A, const FRokUnityCatalogTask& B)
	{
		const int32 SortIdA = A.Group1 * 10000 + A.Order;
		const int32 SortIdB = B.Group1 * 10000 + B.Order;
		if (SortIdA != SortIdB)
		{
			return SortIdA < SortIdB;
		}
		return A.Id < B.Id;
	});
	return Tasks;
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetDailyTaskIds() const
{
	return SortedKeys(DailyTasks);
}

TArray<int32> URokUnityGameplayCatalogSubsystem::GetChapterTaskIds() const
{
	return SortedKeys(ChapterTasks);
}

FRokUnityGameplayCatalogSummary URokUnityGameplayCatalogSubsystem::ParseSummary(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityGameplayCatalogSummary Parsed;
	Parsed.BuildingTypeCount = JsonIntField(Object, TEXT("building_type_count"));
	Parsed.BuildingLevelRowCount = JsonIntField(Object, TEXT("building_level_row_count"));
	Parsed.BuildingMenuCount = JsonIntField(Object, TEXT("building_menu_count"));
	Parsed.BuildingCountLimitRowCount = JsonIntField(Object, TEXT("building_count_limit_row_count"));
	Parsed.BuildingProduceRowCount = JsonIntField(Object, TEXT("building_produce_row_count"));
	Parsed.TrainingCapacityRowCount = JsonIntField(Object, TEXT("training_capacity_row_count"));
	Parsed.HospitalCapacityRowCount = JsonIntField(Object, TEXT("hospital_capacity_row_count"));
	Parsed.TroopCount = JsonIntField(Object, TEXT("troop_count"));
	Parsed.HeroCount = JsonIntField(Object, TEXT("hero_count"));
	Parsed.HeroSkillCount = JsonIntField(Object, TEXT("hero_skill_count"));
	Parsed.HeroTalentCount = JsonIntField(Object, TEXT("hero_talent_count"));
	Parsed.HeroReferencedSkillCount = JsonIntField(Object, TEXT("hero_referenced_skill_count"));
	Parsed.HeroReferencedTalentCount = JsonIntField(Object, TEXT("hero_referenced_talent_count"));
	Parsed.MainTaskCount = JsonIntField(Object, TEXT("main_task_count"));
	Parsed.SideTaskCount = JsonIntField(Object, TEXT("side_task_count"));
	Parsed.DailyTaskCount = JsonIntField(Object, TEXT("daily_task_count"));
	Parsed.ChapterTaskCount = JsonIntField(Object, TEXT("chapter_task_count"));
	Parsed.VipLevelCount = JsonIntField(Object, TEXT("vip_level_count"));
	Parsed.VipAttributeCount = JsonIntField(Object, TEXT("vip_attribute_count"));
	Parsed.VipDayPointCount = JsonIntField(Object, TEXT("vip_day_point_count"));
	Parsed.VipStoreCount = JsonIntField(Object, TEXT("vip_store_count"));
	Parsed.CivilizationCount = JsonIntField(Object, TEXT("civilization_count"));
	return Parsed;
}

FRokUnityCatalogCivilization URokUnityGameplayCatalogSubsystem::ParseCivilization(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogCivilization Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.BonusNameZhCn = JsonStringField(Object, TEXT("bonus_name_zh_cn"));
	Parsed.BonusDescriptionZhCn = JsonStringField(Object, TEXT("bonus_description_zh_cn"));
	Parsed.AddData = JsonStringField(Object, TEXT("add_data"));
	Parsed.BonusValues = JsonIntArrayField(Object, TEXT("bonus_values"));
	Parsed.InitialHeroId = JsonIntField(Object, TEXT("initial_hero_id"));
	Parsed.HeroResourceKey = JsonStringField(Object, TEXT("hero_resource_key"));
	Parsed.HeroAboutOffset = JsonFloatField(Object, TEXT("hero_about_offset"));
	Parsed.bLocked = JsonIntField(Object, TEXT("if_lock")) == 1;
	Parsed.FlagKey = JsonStringField(Object, TEXT("flag_key"));
	Parsed.MarkKey = JsonStringField(Object, TEXT("mark_key"));
	Parsed.CityKey = JsonStringField(Object, TEXT("city_key"));
	Parsed.BackgroundKey = JsonStringField(Object, TEXT("background_key"));
	Parsed.SoldiersKey = JsonStringField(Object, TEXT("soldiers_key"));
	Parsed.Rank = JsonIntField(Object, TEXT("rank"));
	return Parsed;
}

FRokUnityCatalogBuildingType URokUnityGameplayCatalogSubsystem::ParseBuildingType(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogBuildingType Parsed;
	Parsed.TypeId = JsonIntField(Object, TEXT("type_id"));
	Parsed.NameLocaleId = JsonIntField(Object, TEXT("name_locale_id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.DisplayNameEn = JsonStringField(Object, TEXT("display_name_en"));
	Parsed.DescriptionLocaleId = JsonIntField(Object, TEXT("desc_locale_id"));
	Parsed.DisplayDescriptionZhCn = JsonStringField(Object, TEXT("display_description_zh_cn"));
	Parsed.DisplayDescriptionEn = JsonStringField(Object, TEXT("display_description_en"));
	Parsed.ShortDescriptionLocaleId = JsonIntField(Object, TEXT("short_desc_locale_id"));
	Parsed.DisplayShortDescriptionZhCn = JsonStringField(Object, TEXT("display_short_description_zh_cn"));
	Parsed.DisplayShortDescriptionEn = JsonStringField(Object, TEXT("display_short_description_en"));
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
	const TArray<TSharedPtr<FJsonValue>>* CountLimitValues = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(
			TEXT("count_limits"),
			CountLimitValues)
		&& CountLimitValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *CountLimitValues)
		{
			const TSharedPtr<FJsonObject> CountLimitObject =
				Value.IsValid() ? Value->AsObject() : nullptr;
			const int32 TownHallLevel =
				JsonIntField(CountLimitObject, TEXT("town_level"));
			const int32 CountLimit =
				JsonIntField(CountLimitObject, TEXT("limit"));
			if (TownHallLevel > 0 && CountLimit >= 0)
			{
				Parsed.CountLimitByTownHallLevel.Add(
					TownHallLevel,
					CountLimit);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* CapacityValues = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(TEXT("training_capacity"), CapacityValues)
		&& CapacityValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *CapacityValues)
		{
			const TSharedPtr<FJsonObject> CapacityObject =
				Value.IsValid() ? Value->AsObject() : nullptr;
			const int32 Level = JsonIntField(CapacityObject, TEXT("level"));
			const int32 Capacity = JsonIntField(CapacityObject, TEXT("capacity"));
			if (Level > 0 && Capacity > 0)
			{
				Parsed.TrainingCapacityByLevel.Add(Level, Capacity);
			}
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* HospitalCapacityValues = nullptr;
	if (Object.IsValid()
		&& Object->TryGetArrayField(
			TEXT("hospital_capacity"),
			HospitalCapacityValues)
		&& HospitalCapacityValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *HospitalCapacityValues)
		{
			const TSharedPtr<FJsonObject> CapacityObject =
				Value.IsValid() ? Value->AsObject() : nullptr;
			const int32 Level = JsonIntField(CapacityObject, TEXT("level"));
			const int32 Capacity = JsonIntField(CapacityObject, TEXT("capacity"));
			if (Level > 0 && Capacity > 0)
			{
				Parsed.HospitalCapacityByLevel.Add(Level, Capacity);
			}
		}
	}
	return Parsed;
}

FRokUnityCatalogTroop URokUnityGameplayCatalogSubsystem::ParseTroop(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogTroop Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.ArmsType = JsonIntField(Object, TEXT("arms_type"));
	Parsed.SubType = JsonIntField(Object, TEXT("sub_type"));
	Parsed.Level = JsonIntField(Object, TEXT("level"));
	Parsed.StudyId = JsonIntField(Object, TEXT("study_id"));
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
	Parsed.NameLocaleId = JsonIntField(Object, TEXT("name_locale_id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.DisplayNameEn = JsonStringField(Object, TEXT("display_name_en"));
	Parsed.Civilization = JsonIntField(Object, TEXT("civilization"));
	Parsed.AppellationLocaleId = JsonIntField(Object, TEXT("appellation_locale_id"));
	Parsed.DisplayTitleZhCn = JsonStringField(Object, TEXT("display_title_zh_cn"));
	Parsed.DisplayTitleEn = JsonStringField(Object, TEXT("display_title_en"));
	Parsed.InitStar = JsonIntField(Object, TEXT("init_star"));
	Parsed.MaxStar = JsonIntField(Object, TEXT("max_star"));
	Parsed.Rare = JsonIntField(Object, TEXT("rare"));
	Parsed.DescriptionLocaleId = JsonIntField(Object, TEXT("desc_locale_id"));
	Parsed.DisplayDescriptionZhCn = JsonStringField(Object, TEXT("display_description_zh_cn"));
	Parsed.DisplayDescriptionEn = JsonStringField(Object, TEXT("display_description_en"));
	Parsed.Score = JsonIntField(Object, TEXT("score"));
	Parsed.BasePower = JsonIntField(Object, TEXT("base_power"));
	if (Parsed.BasePower <= 0)
	{
		Parsed.BasePower = Parsed.Score;
	}
	Parsed.GetItem = JsonIntField(Object, TEXT("get_item"));
	Parsed.GetItemNum = JsonIntField(Object, TEXT("get_item_num"));
	Parsed.SkillIds = JsonIntArrayField(Object, TEXT("skill_ids"));
	Parsed.TalentIds = JsonIntArrayField(Object, TEXT("talent_ids"));
	Parsed.SkillCount = Parsed.SkillIds.Num();
	Parsed.TalentCount = Parsed.TalentIds.Num();
	Parsed.ListDisplay = JsonIntField(Object, TEXT("list_display"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	ParseIconMetadata(
		Object,
		Parsed.IconAtlas,
		Parsed.IconSprite,
		Parsed.IconX,
		Parsed.IconY,
		Parsed.IconWidth,
		Parsed.IconHeight,
		Parsed.IconAtlasWidth,
		Parsed.IconAtlasHeight);
	Parsed.Model = JsonStringField(Object, TEXT("model"));
	return Parsed;
}

FRokUnityCatalogHeroSkill URokUnityGameplayCatalogSubsystem::ParseHeroSkill(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogHeroSkill Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.NameLocaleId = JsonIntField(Object, TEXT("name_locale_id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.DisplayNameEn = JsonStringField(Object, TEXT("display_name_en"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.TypeLocaleId = JsonIntField(Object, TEXT("type_locale_id"));
	Parsed.DisplayTypeZhCn = JsonStringField(Object, TEXT("display_type_zh_cn"));
	Parsed.DisplayTypeEn = JsonStringField(Object, TEXT("display_type_en"));
	Parsed.BattleSkillId = JsonIntField(Object, TEXT("battle_skill_id"));
	Parsed.Anger = JsonIntField(Object, TEXT("anger"));
	Parsed.Open = JsonIntField(Object, TEXT("open"));
	Parsed.OpenLocaleId = JsonIntField(Object, TEXT("open_locale_id"));
	Parsed.DisplayOpenZhCn = JsonStringField(Object, TEXT("display_open_zh_cn"));
	Parsed.DisplayOpenEn = JsonStringField(Object, TEXT("display_open_en"));
	Parsed.AwakenEnhance = JsonIntField(Object, TEXT("awaken_enhance"));
	Parsed.LevelUpCostItem = JsonIntField(Object, TEXT("level_up_cost_item"));
	Parsed.MessageLocaleId = JsonIntField(Object, TEXT("message_locale_id"));
	Parsed.DisplayMessageZhCn = JsonStringField(Object, TEXT("display_message_zh_cn"));
	Parsed.DisplayMessageEn = JsonStringField(Object, TEXT("display_message_en"));
	const TArray<TSharedPtr<FJsonValue>>* LevelEffectValues = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(TEXT("level_effects"), LevelEffectValues)
		&& LevelEffectValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *LevelEffectValues)
		{
			const TSharedPtr<FJsonObject> EffectObject = Value.IsValid() ? Value->AsObject() : nullptr;
			FRokUnityCatalogHeroSkillLevelEffect Effect;
			Effect.Data = JsonStringField(EffectObject, TEXT("data"));
			Effect.LocaleId = JsonIntField(EffectObject, TEXT("locale_id"));
			Effect.DisplayTextZhCn = JsonStringField(EffectObject, TEXT("display_text_zh_cn"));
			Effect.DisplayTextEn = JsonStringField(EffectObject, TEXT("display_text_en"));
			Parsed.LevelEffects.Add(MoveTemp(Effect));
		}
	}
	Parsed.ArmyEffect = JsonStringField(Object, TEXT("army_effect"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	ParseIconMetadata(
		Object,
		Parsed.IconAtlas,
		Parsed.IconSprite,
		Parsed.IconX,
		Parsed.IconY,
		Parsed.IconWidth,
		Parsed.IconHeight,
		Parsed.IconAtlasWidth,
		Parsed.IconAtlasHeight);
	Parsed.Sound = JsonStringField(Object, TEXT("sound"));
	return Parsed;
}

FRokUnityCatalogHeroTalent URokUnityGameplayCatalogSubsystem::ParseHeroTalent(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogHeroTalent Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.TalentLocaleId = JsonIntField(Object, TEXT("talent_locale_id"));
	Parsed.DisplayNameZhCn = JsonStringField(Object, TEXT("display_name_zh_cn"));
	Parsed.DisplayNameEn = JsonStringField(Object, TEXT("display_name_en"));
	Parsed.GainTree = JsonIntField(Object, TEXT("gain_tree"));
	Parsed.MasteryGroupId = JsonIntField(Object, TEXT("mastery_group_id"));
	Parsed.Type = JsonIntField(Object, TEXT("type"));
	Parsed.TipsLocaleId = JsonIntField(Object, TEXT("tips_locale_id"));
	Parsed.DisplayTipsZhCn = JsonStringField(Object, TEXT("display_tips_zh_cn"));
	Parsed.DisplayTipsEn = JsonStringField(Object, TEXT("display_tips_en"));
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
	Parsed.Group1 = JsonIntField(Object, TEXT("group1"));
	Parsed.Order = JsonIntField(Object, TEXT("order"));
	Parsed.ShowLevelReq = JsonIntField(Object, TEXT("show_level_req"));
	Parsed.ShowBuildingReq = JsonIntField(Object, TEXT("show_building_req"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	return Parsed;
}

FRokUnityCatalogVipLevel URokUnityGameplayCatalogSubsystem::ParseVipLevel(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogVipLevel Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.Level = JsonIntField(Object, TEXT("level"));
	Parsed.Point = JsonIntField(Object, TEXT("point"));
	Parsed.FreeBox = JsonIntField(Object, TEXT("free_box"));
	Parsed.SpecialBox = JsonIntField(Object, TEXT("special_box"));
	Parsed.Price = JsonIntField(Object, TEXT("price"));
	Parsed.PriceUsd = JsonFloatField(Object, TEXT("price_usd"));
	Parsed.PriceDisplay = JsonStringField(Object, TEXT("price_display"));
	Parsed.Icon = JsonStringField(Object, TEXT("icon"));
	return Parsed;
}

FRokUnityCatalogVipAttribute URokUnityGameplayCatalogSubsystem::ParseVipAttribute(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogVipAttribute Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.LevelGroup = JsonIntField(Object, TEXT("level_group"));
	Parsed.bNew = JsonIntField(Object, TEXT("new_sign")) == 1;
	Parsed.Attribute = JsonStringField(Object, TEXT("attribute"));
	Parsed.Description = JsonStringField(Object, TEXT("description"));
	Parsed.LocalizedTemplate = JsonStringField(Object, TEXT("localized_template"));
	Parsed.DisplayTextZhCn = JsonStringField(Object, TEXT("display_text_zh_cn"));
	Parsed.LocaleId = JsonIntField(Object, TEXT("locale_id"));
	Parsed.Add = JsonFloatField(Object, TEXT("add"));
	Parsed.bInteger = JsonIntField(Object, TEXT("integer")) == 1;
	return Parsed;
}

FRokUnityCatalogVipDayPoint URokUnityGameplayCatalogSubsystem::ParseVipDayPoint(const TSharedPtr<FJsonObject>& Object)
{
	FRokUnityCatalogVipDayPoint Parsed;
	Parsed.Id = JsonIntField(Object, TEXT("id"));
	Parsed.Day = JsonIntField(Object, TEXT("day"));
	Parsed.Point = JsonIntField(Object, TEXT("point"));
	return Parsed;
}

bool URokUnityGameplayCatalogSubsystem::LoadActivityCalendarReadonly()
{
	if (bActivityCalendarReadonlyLoaded) return true;
	ActivityCalendarReadonlyRows.Reset();
	TArray<TSharedPtr<FJsonValue>> LanguageRows;
	TArray<TSharedPtr<FJsonValue>> CalendarRows;
	if (!LoadReadonlyConfigTable(LanguageTableRelativePath, TEXT("Language"), 8067, LanguageRows)
		|| !LoadReadonlyConfigTable(ActivityCalendarTableRelativePath, TEXT("ActivityCalendar"), 123, CalendarRows))
	{
		UE_LOG(LogTemp, Warning, TEXT("Activity Calendar static config unavailable; no local reference rows shown."));
		return false;
	}
	TMap<int32, FString> Chinese;
	for (const TSharedPtr<FJsonValue>& Value : LanguageRows)
	{
		TSharedPtr<FJsonObject> Row;
		if (Value.IsValid() && Value->Type == EJson::Object) Row = Value->AsObject();
		int32 Id = 0;
		FString Text;
		if (!ReadStrictNonnegativeInt(Row, TEXT("id"), Id) || Id <= 0
			|| !ReadStrictString(Row, TEXT("cn"), Text) || Chinese.Contains(Id)) return false;
		Chinese.Add(Id, MoveTemp(Text));
	}
	TSet<int32> SeenIds;
	TArray<FRokUnityActivityCalendarReadonlyRow> Resolved;
	int32 WithSketch = 0;
	int32 WithIcon = 0;
	for (int32 Index = 0; Index < CalendarRows.Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& Value = CalendarRows[Index];
		TSharedPtr<FJsonObject> Row;
		if (Value.IsValid() && Value->Type == EJson::Object) Row = Value->AsObject();
		if (!Row.IsValid()) return false;
		// The generated table has one final, fully empty sentinel. No other missing ID is allowed.
		if (Index == 122)
		{
			FString BlankId;
			if (!ReadStrictString(Row, TEXT("ID"), BlankId) || !BlankId.IsEmpty()) return false;
			for (const auto& Field : Row->Values)
			{
				if (!Field.Value.IsValid() ||
					(Field.Value->Type == EJson::String && !Field.Value->AsString().IsEmpty()) ||
					(Field.Value->Type == EJson::Number && Field.Value->AsNumber() != 0) ||
					(Field.Value->Type != EJson::String && Field.Value->Type != EJson::Number)) return false;
			}
			continue;
		}
		int32 Id = 0, NameId = 0, SketchId = 0;
		FString Icon;
		if (!ReadStrictNonnegativeInt(Row, TEXT("ID"), Id) || Id <= 0 || SeenIds.Contains(Id)
			|| !ReadStrictNonnegativeInt(Row, TEXT("l_nameID"), NameId)
			|| !ReadStrictNonnegativeInt(Row, TEXT("l_sketchID"), SketchId)
			|| !ReadStrictString(Row, TEXT("icon"), Icon)) return false;
		SeenIds.Add(Id);
		const FString* Title = Chinese.Find(NameId);
		if (!Title || Title->IsEmpty() || HasBrokenLocale(*Title)) continue;
		FRokUnityActivityCalendarReadonlyRow Entry;
		Entry.Id = Id;
		Entry.TitleZhCn = *Title;
		Entry.IconReference = MoveTemp(Icon);
		if (const FString* Sketch = Chinese.Find(SketchId))
		{
			if (!Sketch->IsEmpty() && !HasBrokenLocale(*Sketch)) Entry.SketchZhCn = *Sketch;
		}
		if (!Entry.SketchZhCn.IsEmpty()) ++WithSketch;
		if (!Entry.IconReference.IsEmpty()) ++WithIcon;
		Resolved.Add(MoveTemp(Entry));
	}
	if (SeenIds.Num() != 122 || Resolved.Num() != 89 || WithSketch != 64 || WithIcon != 88)
	{
		UE_LOG(LogTemp, Warning, TEXT("Activity Calendar static source counts differ; no local reference rows shown."));
		return false;
	}
	ActivityCalendarReadonlyRows = MoveTemp(Resolved);
	bActivityCalendarReadonlyLoaded = true;
	return true;
}

bool URokUnityGameplayCatalogSubsystem::LoadAllianceTechnologyReadonly()
{
	if (bAllianceTechnologyReadonlyLoaded) return true;
	if (bAllianceTechnologyReadonlyLoadAttempted) return false;
	bAllianceTechnologyReadonlyLoadAttempted = true;
	AllianceTechnologyReadonlyRows.Reset();
	FString Text;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), AllianceTechnologyRelativePath);
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("AllianceStudy source manifest unavailable; no static technology definitions shown."));
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	FString Schema;
	int32 SourceRows = 0, StudyTypes = 0, EdgeCount = 0;
	bool bAccount = true, bProgress = true, bCosts = true, bServer = true;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!ReadStrictString(Root, TEXT("schema"), Schema)
		|| Schema != TEXT("rok.alliance_technology_readonly_catalog.v1")
		|| !ReadStrictNonnegativeInt(Root, TEXT("source_rows"), SourceRows) || SourceRows != 460
		|| !ReadStrictNonnegativeInt(Root, TEXT("study_types"), StudyTypes) || StudyTypes != 60
		|| !ReadStrictNonnegativeInt(Root, TEXT("prerequisite_edges"), EdgeCount) || EdgeCount != 202
		|| !Root->TryGetBoolField(TEXT("account_state_present"), bAccount) || bAccount
		|| !Root->TryGetBoolField(TEXT("research_progress_present"), bProgress) || bProgress
		|| !Root->TryGetBoolField(TEXT("costs_or_timers_present"), bCosts) || bCosts
		|| !Root->TryGetBoolField(TEXT("server_state_present"), bServer) || bServer
		|| !Root->TryGetArrayField(TEXT("rows"), Rows) || !Rows || Rows->Num() != 460)
	{
		UE_LOG(LogTemp, Warning, TEXT("AllianceStudy source manifest contract failed; no technology definitions shown."));
		return false;
	}
	TArray<FRokUnityAllianceTechnologyReadonlyRow> Candidate;
	Candidate.Reserve(460);
	TSet<int32> Ids;
	TMap<int64, int32> LevelKeys;
	TMap<int32, int32> TypeCategory;
	TMap<int32, int32> TypeLevelCounts;
	TMap<int32, int32> TypeMaxLevels;
	int32 Categories[3] = {0, 0, 0};
	int32 CategoryDefinitions[3] = {0, 0, 0};
	int32 Atlas1Count = 0, Atlas2Count = 0, Edges = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Rows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FRokUnityAllianceTechnologyReadonlyRow Entry;
		FString Category, IconSource;
		const TSharedPtr<FJsonObject> Icon = JsonObjectField(Row, TEXT("icon"));
		const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Extent = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Prerequisites = nullptr;
		if (!ReadStrictNonnegativeInt(Row, TEXT("id"), Entry.Id) || Entry.Id <= 0 || Ids.Contains(Entry.Id)
			|| !ReadStrictNonnegativeInt(Row, TEXT("category_type"), Entry.CategoryType)
			|| Entry.CategoryType < 1 || Entry.CategoryType > 3
			|| !ReadStrictString(Row, TEXT("category"), Category)
			|| Category != (Entry.CategoryType == 1 ? TEXT("develop")
				: Entry.CategoryType == 2 ? TEXT("territory") : TEXT("war"))
			|| !ReadStrictNonnegativeInt(Row, TEXT("study_type"), Entry.StudyType) || Entry.StudyType <= 0
			|| !ReadStrictNonnegativeInt(Row, TEXT("study_level_key"), Entry.LevelKey) || Entry.LevelKey <= 0
			|| !ReadStrictNonnegativeInt(Row, TEXT("title_locale_id"), Entry.TitleLocaleId) || Entry.TitleLocaleId <= 0
			|| !ReadStrictNonnegativeInt(Row, TEXT("description_locale_id"), Entry.DescriptionLocaleId) || Entry.DescriptionLocaleId <= 0
			|| !ReadStrictString(Row, TEXT("title_cn"), Entry.Title) || Entry.Title.TrimStartAndEnd().IsEmpty() || HasBrokenLocale(Entry.Title)
			|| !ReadStrictString(Row, TEXT("description_cn"), Entry.Description) || Entry.Description.TrimStartAndEnd().IsEmpty() || HasBrokenLocale(Entry.Description)
			|| !ReadStrictString(Icon, TEXT("source"), IconSource)
			|| !ReadStrictString(Icon, TEXT("atlas"), Entry.IconAtlas)
			|| !ReadStrictString(Icon, TEXT("sprite"), Entry.IconSprite)
			|| !Icon.IsValid() || !Icon->TryGetArrayField(TEXT("rect_xywh"), Rect) || !Rect || Rect->Num() != 4
			|| !Icon->TryGetArrayField(TEXT("atlas_size"), Extent) || !Extent || Extent->Num() != 2
			|| !Row->TryGetArrayField(TEXT("prerequisites"), Prerequisites) || !Prerequisites || Prerequisites->Num() > 4)
		{
			UE_LOG(LogTemp, Warning, TEXT("AllianceStudy row identity/locale/icon contract failed."));
			return false;
		}
		const bool bAtlas1 = Entry.IconAtlas == TEXT("research_icon1");
		const bool bAtlas2 = Entry.IconAtlas == TEXT("research_icon2");
		if ((!bAtlas1 && !bAtlas2) || Entry.IconSprite.IsEmpty()
			|| IconSource != FString::Printf(TEXT("%s[%s]"), *Entry.IconAtlas, *Entry.IconSprite)) return false;
		int32 RectValues[4] = {0, 0, 0, 0};
		int32 ExtentValues[2] = {0, 0};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const TSharedPtr<FJsonValue>& Number = (*Rect)[Index];
			if (!Number.IsValid() || Number->Type != EJson::Number) return false;
			const double Source = Number->AsNumber();
			if (!FMath::IsFinite(Source) || Source < 0 || Source > MAX_int32
				|| static_cast<double>(static_cast<int32>(Source)) != Source) return false;
			RectValues[Index] = static_cast<int32>(Source);
		}
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const TSharedPtr<FJsonValue>& Number = (*Extent)[Index];
			if (!Number.IsValid() || Number->Type != EJson::Number) return false;
			const double Source = Number->AsNumber();
			if (!FMath::IsFinite(Source) || Source < 0 || Source > MAX_int32
				|| static_cast<double>(static_cast<int32>(Source)) != Source) return false;
			ExtentValues[Index] = static_cast<int32>(Source);
		}
		if (RectValues[2] <= 0 || RectValues[3] <= 0
			|| ExtentValues[0] != (bAtlas1 ? 512 : 1024) || ExtentValues[1] != 2048
			|| RectValues[0] > ExtentValues[0] - RectValues[2]
			|| RectValues[1] > ExtentValues[1] - RectValues[3]) return false;
		Entry.IconRect = FIntRect(RectValues[0], RectValues[1],
			RectValues[0] + RectValues[2], RectValues[1] + RectValues[3]);
		Entry.AtlasSize = FIntPoint(ExtentValues[0], ExtentValues[1]);
		TSet<int32> Slots;
		for (const TSharedPtr<FJsonValue>& PrerequisiteValue : *Prerequisites)
		{
			const TSharedPtr<FJsonObject> Prerequisite = PrerequisiteValue.IsValid()
				&& PrerequisiteValue->Type == EJson::Object ? PrerequisiteValue->AsObject() : nullptr;
			FRokUnityAllianceTechnologyPrerequisite Edge;
			if (!ReadStrictNonnegativeInt(Prerequisite, TEXT("slot"), Edge.Slot)
				|| Edge.Slot < 1 || Edge.Slot > 4 || Slots.Contains(Edge.Slot)
				|| !ReadStrictNonnegativeInt(Prerequisite, TEXT("study_type"), Edge.StudyType) || Edge.StudyType <= 0
				|| !ReadStrictNonnegativeInt(Prerequisite, TEXT("required_level"), Edge.RequiredLevel) || Edge.RequiredLevel <= 0
				|| !ReadStrictNonnegativeInt(Prerequisite, TEXT("source_row_id"), Edge.SourceRowId) || Edge.SourceRowId <= 0) return false;
			Slots.Add(Edge.Slot);
			Entry.Prerequisites.Add(Edge);
			++Edges;
		}
		const int64 Key = (static_cast<int64>(Entry.StudyType) << 32) | static_cast<uint32>(Entry.LevelKey);
		if (LevelKeys.Contains(Key)) return false;
		const int32* ExistingCategory = TypeCategory.Find(Entry.StudyType);
		if (ExistingCategory && *ExistingCategory != Entry.CategoryType) return false;
		TypeCategory.Add(Entry.StudyType, Entry.CategoryType);
		TypeLevelCounts.FindOrAdd(Entry.StudyType)++;
		TypeMaxLevels.FindOrAdd(Entry.StudyType) = FMath::Max(TypeMaxLevels.FindRef(Entry.StudyType), Entry.LevelKey);
		LevelKeys.Add(Key, Entry.Id);
		Ids.Add(Entry.Id);
		++Categories[Entry.CategoryType - 1];
		if (Entry.LevelKey == 1) ++CategoryDefinitions[Entry.CategoryType - 1];
		if (bAtlas1) ++Atlas1Count; else ++Atlas2Count;
		Candidate.Add(MoveTemp(Entry));
	}
	if (Ids.Num() != 460 || TypeCategory.Num() != 60 || Edges != 202
		|| Categories[0] != 165 || Categories[1] != 125 || Categories[2] != 170
		|| CategoryDefinitions[0] != 22 || CategoryDefinitions[1] != 16 || CategoryDefinitions[2] != 22
		|| Atlas1Count != 170 || Atlas2Count != 290) return false;
	for (const auto& Pair : TypeLevelCounts)
	{
		if (Pair.Value != TypeMaxLevels.FindRef(Pair.Key)) return false;
	}
	for (const FRokUnityAllianceTechnologyReadonlyRow& Entry : Candidate)
	{
		for (const FRokUnityAllianceTechnologyPrerequisite& Edge : Entry.Prerequisites)
		{
			const int64 Key = (static_cast<int64>(Edge.StudyType) << 32) | static_cast<uint32>(Edge.RequiredLevel);
			const int32* ResolvedId = LevelKeys.Find(Key);
			if (!ResolvedId || *ResolvedId != Edge.SourceRowId) return false;
		}
	}
	AllianceTechnologyReadonlyRows = MoveTemp(Candidate);
	bAllianceTechnologyReadonlyLoaded = true;
	return true;
}

bool URokUnityGameplayCatalogSubsystem::LoadAllianceOfficeReadonly()
{
	if (bAllianceOfficeReadonlyLoaded) return true;
	if (bAllianceOfficeReadonlyLoadAttempted) return false;
	bAllianceOfficeReadonlyLoadAttempted = true;
	AllianceOfficeReadonlyRows.Reset();
	TArray<TSharedPtr<FJsonValue>> LanguageRows;
	TArray<TSharedPtr<FJsonValue>> OfficeRows;
	if (!LoadReadonlyConfigTable(LanguageTableRelativePath, TEXT("Language"), 8067, LanguageRows)
		|| !LoadReadonlyConfigTable(AllianceOfficiallyTableRelativePath, TEXT("AllianceOfficially"), 4, OfficeRows))
	{
		UE_LOG(LogTemp, Warning, TEXT("Alliance office static config unavailable; no office definitions shown."));
		return false;
	}

	TMap<int32, FString> Chinese;
	for (const TSharedPtr<FJsonValue>& Value : LanguageRows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() && Value->Type == EJson::Object
			? Value->AsObject() : nullptr;
		int32 Id = 0;
		FString Text;
		if (!ReadStrictNonnegativeInt(Row, TEXT("id"), Id) || Id <= 0
			|| !ReadStrictString(Row, TEXT("cn"), Text) || Chinese.Contains(Id))
		{
			UE_LOG(LogTemp, Warning, TEXT("Alliance office locale table has an invalid or duplicate id."));
			return false;
		}
		Chinese.Add(Id, MoveTemp(Text));
	}

	TSet<int32> SeenIds;
	TArray<FRokUnityAllianceOfficeReadonlyRow> Resolved;
	for (const TSharedPtr<FJsonValue>& Value : OfficeRows)
	{
		const TSharedPtr<FJsonObject> Row = Value.IsValid() && Value->Type == EJson::Object
			? Value->AsObject() : nullptr;
		FRokUnityAllianceOfficeReadonlyRow Entry;
		FString BonusTemplate, BonusValues;
		if (!ReadStrictNonnegativeInt(Row, TEXT("ID"), Entry.Id) || Entry.Id <= 0
			|| SeenIds.Contains(Entry.Id)
			|| !ReadStrictNonnegativeInt(Row, TEXT("l_officiallyID"), Entry.NameLocaleId)
			|| !ReadStrictNonnegativeInt(Row, TEXT("l_desID"), Entry.DescriptionLocaleId)
			|| !ReadStrictNonnegativeInt(Row, TEXT("l_addDesID"), Entry.AddDescriptionLocaleId)
			|| !ReadStrictString(Row, TEXT("addDesData"), BonusValues)
			|| !ReadStrictString(Row, TEXT("icon"), Entry.IconReference))
		{
			UE_LOG(LogTemp, Warning, TEXT("Alliance office row has invalid required fields."));
			return false;
		}
		SeenIds.Add(Entry.Id);
		if (Entry.Id == 1001) Entry.IconSprite = TEXT("icon_ali_achi1");
		else if (Entry.Id == 2001) Entry.IconSprite = TEXT("icon_ali_achi2");
		else if (Entry.Id == 3001) Entry.IconSprite = TEXT("icon_ali_achi3");
		else if (Entry.Id == 4001) Entry.IconSprite = TEXT("icon_ali_achi4");
		else return false;
		if (Entry.IconReference != FString::Printf(TEXT("ui_alliance[%s]"), *Entry.IconSprite)) return false;

		ResolveAllianceOfficeLocale(Chinese, Entry.NameLocaleId, Entry.Name);
		ResolveAllianceOfficeLocale(Chinese, Entry.DescriptionLocaleId, Entry.Description);
		if (ResolveAllianceOfficeLocale(Chinese, Entry.AddDescriptionLocaleId, BonusTemplate))
		{
			FormatAllianceOfficeBonus(BonusTemplate, BonusValues, Entry.AddDescription);
		}
		Resolved.Add(MoveTemp(Entry));
	}
	static const TSet<int32> RequiredIds = {1001, 2001, 3001, 4001};
	if (SeenIds.Num() != RequiredIds.Num() || !SeenIds.Contains(1001) || !SeenIds.Contains(2001)
		|| !SeenIds.Contains(3001) || !SeenIds.Contains(4001))
	{
		UE_LOG(LogTemp, Warning, TEXT("Alliance office definitions do not match the four expected source IDs."));
		return false;
	}
	Resolved.Sort([](const FRokUnityAllianceOfficeReadonlyRow& A, const FRokUnityAllianceOfficeReadonlyRow& B)
	{
		return A.Id < B.Id;
	});
	AllianceOfficeReadonlyRows = MoveTemp(Resolved);
	bAllianceOfficeReadonlyLoaded = true;
	return true;
}
