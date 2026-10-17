#pragma once

#include "CoreMinimal.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityGameplayCatalogSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BuildingTypeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BuildingLevelRowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BuildingMenuCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BuildingCountLimitRowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BuildingProduceRowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TrainingCapacityRowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HospitalCapacityRowCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TroopCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroSkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroTalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroReferencedSkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroReferencedTalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MainTaskCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 SideTaskCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 DailyTaskCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ChapterTaskCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 VipLevelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 VipAttributeCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 VipDayPointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 VipStoreCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 CivilizationCount = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogBuildingType
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 NameLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 DescriptionLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayDescriptionZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayDescriptionEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ShortDescriptionLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayShortDescriptionZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayShortDescriptionEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Group = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Length = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MenuButtonCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString MenuFunctions;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LevelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MinLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MaxLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TotalPower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level1BuildSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level1Power = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FRokResourceCost Level1Cost;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level2BuildSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level2Power = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FRokResourceCost Level2Cost;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ProductionLevelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	bool bHasProduction = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TMap<int32, int32> TrainingCapacityByLevel;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TMap<int32, int32> HospitalCapacityByLevel;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TMap<int32, int32> CountLimitByTownHallLevel;

	int32 GetTrainingCapacity(int32 Level) const
	{
		return TrainingCapacityByLevel.FindRef(Level);
	}

	int32 GetHospitalCapacity(int32 Level) const
	{
		return HospitalCapacityByLevel.FindRef(Level);
	}

	int32 GetCountLimit(int32 TownHallLevel) const
	{
		int32 Result = 0;
		for (const TPair<int32, int32>& Pair : CountLimitByTownHallLevel)
		{
			if (Pair.Key <= TownHallLevel)
			{
				Result = FMath::Max(Result, Pair.Value);
			}
		}
		return Result;
	}

	int32 GetMaximumCountLimit() const
	{
		int32 Result = 0;
		for (const TPair<int32, int32>& Pair : CountLimitByTownHallLevel)
		{
			Result = FMath::Max(Result, Pair.Value);
		}
		return Result;
	}
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogTroop
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ArmsType = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 SubType = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 StudyId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Age = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Attack = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Defense = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Hp = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Power = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FRokResourceCost TrainCost;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TrainSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Model;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogCivilization
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString BonusNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString BonusDescriptionZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString AddData;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TArray<int32> BonusValues;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 InitialHeroId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString HeroResourceKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	float HeroAboutOffset = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	bool bLocked = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString FlagKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString MarkKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString CityKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString BackgroundKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString SoldiersKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Rank = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogHero
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 NameLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Civilization = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 AppellationLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTitleZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTitleEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 InitStar = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MaxStar = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Rare = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 DescriptionLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayDescriptionZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayDescriptionEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BasePower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 GetItem = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 GetItemNum = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 SkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TArray<int32> SkillIds;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TArray<int32> TalentIds;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ListDisplay = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString IconAtlas;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString IconSprite;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconX = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconY = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconAtlasWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconAtlasHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Model;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogHeroSkillLevelEffect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Data;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTextZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTextEn;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogHeroSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 NameLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TypeLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTypeZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTypeEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BattleSkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Anger = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Open = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 OpenLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayOpenZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayOpenEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 AwakenEnhance = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LevelUpCostItem = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MessageLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayMessageZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayMessageEn;

	/** Four source lvEffectDate/l_lvEffect groups in Unity order; config, not hero progress. */
	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	TArray<FRokUnityCatalogHeroSkillLevelEffect> LevelEffects;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString ArmyEffect;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString IconAtlas;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString IconSprite;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconX = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconY = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconAtlasWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 IconAtlasHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Sound;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogHeroTalent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TalentLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayNameEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 GainTree = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MasteryGroupId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TipsLocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTipsZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTipsEn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Desc;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Param1 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Param2 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Require = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Reward = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 NextId = 0;

	// TaskSide selection fields mirroring Unity TaskProxy:
	// SortId = Group1 * 10000 + Order; ShowLevelReq/ShowBuildingReq gate
	// whether an unfinished side task is eligible for display.
	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Group1 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Order = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ShowLevelReq = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 ShowBuildingReq = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogVipLevel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Point = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 FreeBox = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 SpecialBox = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	float PriceUsd = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString PriceDisplay;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogVipAttribute
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LevelGroup = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	bool bNew = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Attribute;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Description;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString LocalizedTemplate;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString DisplayTextZhCn;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LocaleId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	float Add = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	bool bInteger = false;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogVipDayPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Point = 0;
};

/** Static Unity AllianceOfficially definitions only; no appointment or applied benefits. */
struct FRokUnityAllianceOfficeReadonlyRow
{
	int32 Id = 0;
	int32 NameLocaleId = 0;
	int32 DescriptionLocaleId = 0;
	int32 AddDescriptionLocaleId = 0;
	FString Name;
	FString Description;
	FString AddDescription;
	FString IconReference;
	FString IconSprite;
};

/** One AllianceStudy prerequisite edge, expressed as static source keys only. */
struct FRokUnityAllianceTechnologyPrerequisite
{
	int32 Slot = 0;
	int32 StudyType = 0;
	int32 RequiredLevel = 0;
	int32 SourceRowId = 0;
};

/** One configured AllianceStudy row; LevelKey is not a player's current level. */
struct FRokUnityAllianceTechnologyReadonlyRow
{
	int32 Id = 0;
	int32 CategoryType = 0;
	int32 StudyType = 0;
	int32 LevelKey = 0;
	int32 TitleLocaleId = 0;
	int32 DescriptionLocaleId = 0;
	FString Title;
	FString Description;
	FString IconAtlas;
	FString IconSprite;
	FIntRect IconRect;
	FIntPoint AtlasSize = FIntPoint::ZeroValue;
	TArray<FRokUnityAllianceTechnologyPrerequisite> Prerequisites;
};

/** Static Unity ActivityCalendar definitions only; no server schedule or account state. */
struct FRokUnityActivityCalendarReadonlyRow
{
	int32 Id = 0;
	FString TitleZhCn;
	FString SketchZhCn;
	FString IconReference;
};

UCLASS()
class ROK_API URokUnityGameplayCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Catalog")
	bool LoadCatalog();

	/** Loads only pinned static AllianceStudy definitions, never account research state. */
	bool LoadAllianceTechnologyReadonly();
	const TArray<FRokUnityAllianceTechnologyReadonlyRow>& GetAllianceTechnologyReadonlyRows() const
	{
		return AllianceTechnologyReadonlyRows;
	}

	/** Loads static office definitions; does not load membership or appointment state. */
	bool LoadAllianceOfficeReadonly();
	const TArray<FRokUnityAllianceOfficeReadonlyRow>& GetAllianceOfficeReadonlyRows() const
	{
		return AllianceOfficeReadonlyRows;
	}

	/** Loads the pinned local config tables without inferring dates, rewards, or eligibility. */
	bool LoadActivityCalendarReadonly();
	const TArray<FRokUnityActivityCalendarReadonlyRow>& GetActivityCalendarReadonlyRows() const
	{
		return ActivityCalendarReadonlyRows;
	}

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool IsCatalogLoaded() const { return bCatalogLoaded; }

	/** Source-backed Chinese VIP daily-point help copy; false means unavailable. */
	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetVipDailyPointHelpText(FText& OutText) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	FRokUnityGameplayCatalogSummary GetSummary() const { return Summary; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetBuildingTypeCount() const { return BuildingTypes.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetTroopCount() const { return Troops.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetCivilizationCount() const { return Civilizations.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroCount() const { return Heroes.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroSkillCount() const { return HeroSkills.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroTalentCount() const { return HeroTalents.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetMainTaskCount() const { return MainTasks.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetSideTaskCount() const { return SideTasks.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetDailyTaskCount() const { return DailyTasks.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetChapterTaskCount() const { return ChapterTasks.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetBuildingType(int32 TypeId, FRokUnityCatalogBuildingType& OutBuildingType) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetTroop(int32 Id, FRokUnityCatalogTroop& OutTroop) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<FRokUnityCatalogTroop> GetTroopsForArmyType(int32 ArmsType) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<FRokUnityCatalogCivilization> GetCivilizations() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetCivilization(int32 Id, FRokUnityCatalogCivilization& OutCivilization) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHero(int32 Id, FRokUnityCatalogHero& OutHero) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHeroSkill(int32 Id, FRokUnityCatalogHeroSkill& OutSkill) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHeroTalent(int32 Id, FRokUnityCatalogHeroTalent& OutTalent) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetMainTask(int32 Id, FRokUnityCatalogTask& OutTask) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetSideTask(int32 Id, FRokUnityCatalogTask& OutTask) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetDailyTask(int32 Id, FRokUnityCatalogTask& OutTask) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetChapterTask(int32 Id, FRokUnityCatalogTask& OutTask) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetVipLevel(int32 Level, FRokUnityCatalogVipLevel& OutVipLevel) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<FRokUnityCatalogVipAttribute> GetVipAttributesForLevel(int32 Level) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetVipDayPoint(int32 Day, FRokUnityCatalogVipDayPoint& OutDayPoint) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetBuildingTypeIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetTroopIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetHeroIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetHeroSkillIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetHeroTalentIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetMainTaskIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetSideTaskIds() const;

	// Side tasks ordered like Unity TaskProxy: SortId = group1 * 10000 + order.
	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<FRokUnityCatalogTask> GetSideTasksBySortOrder() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetDailyTaskIds() const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	TArray<int32> GetChapterTaskIds() const;

private:
	FRokUnityGameplayCatalogSummary Summary;
	TMap<int32, FRokUnityCatalogBuildingType> BuildingTypes;
	TMap<int32, FRokUnityCatalogTroop> Troops;
	TMap<int32, FRokUnityCatalogCivilization> Civilizations;
	TMap<int32, FRokUnityCatalogHero> Heroes;
	TMap<int32, FRokUnityCatalogHeroSkill> HeroSkills;
	TMap<int32, FRokUnityCatalogHeroTalent> HeroTalents;
	TMap<int32, FRokUnityCatalogTask> MainTasks;
	TMap<int32, FRokUnityCatalogTask> SideTasks;
	TMap<int32, FRokUnityCatalogTask> DailyTasks;
	TMap<int32, FRokUnityCatalogTask> ChapterTasks;
	TMap<int32, FRokUnityCatalogVipLevel> VipLevels;
	TArray<FRokUnityCatalogVipAttribute> VipAttributes;
	TMap<int32, FRokUnityCatalogVipDayPoint> VipDayPoints;
	FString VipDailyPointHelpText;
	TArray<FRokUnityActivityCalendarReadonlyRow> ActivityCalendarReadonlyRows;
	bool bActivityCalendarReadonlyLoaded = false;
	TArray<FRokUnityAllianceTechnologyReadonlyRow> AllianceTechnologyReadonlyRows;
	bool bAllianceTechnologyReadonlyLoaded = false;
	bool bAllianceTechnologyReadonlyLoadAttempted = false;
	TArray<FRokUnityAllianceOfficeReadonlyRow> AllianceOfficeReadonlyRows;
	bool bAllianceOfficeReadonlyLoaded = false;
	bool bAllianceOfficeReadonlyLoadAttempted = false;
	bool bCatalogLoaded = false;

	void LoadVipDailyPointHelpText();

	static FRokUnityGameplayCatalogSummary ParseSummary(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogBuildingType ParseBuildingType(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogTroop ParseTroop(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogCivilization ParseCivilization(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHero ParseHero(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHeroSkill ParseHeroSkill(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHeroTalent ParseHeroTalent(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogTask ParseTask(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogVipLevel ParseVipLevel(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogVipAttribute ParseVipAttribute(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogVipDayPoint ParseVipDayPoint(const TSharedPtr<FJsonObject>& Object);
};
