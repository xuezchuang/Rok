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
	int32 TroopCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroSkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 HeroTalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MainTaskCount = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogBuildingType
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TypeId = 0;

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
struct FRokUnityCatalogHero
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Civilization = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 InitStar = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MaxStar = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Rare = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 GetItem = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 GetItemNum = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 SkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 TalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Model;
};

USTRUCT(BlueprintType)
struct FRokUnityCatalogHeroSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 BattleSkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Anger = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Open = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 AwakenEnhance = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 LevelUpCostItem = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString ArmyEffect;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;

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
	int32 GainTree = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 MasteryGroupId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	int32 Type = 0;

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

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Catalog")
	FString Icon;
};

UCLASS()
class ROK_API URokUnityGameplayCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Catalog")
	bool LoadCatalog();

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool IsCatalogLoaded() const { return bCatalogLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	FRokUnityGameplayCatalogSummary GetSummary() const { return Summary; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetBuildingTypeCount() const { return BuildingTypes.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetTroopCount() const { return Troops.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroCount() const { return Heroes.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroSkillCount() const { return HeroSkills.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetHeroTalentCount() const { return HeroTalents.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	int32 GetMainTaskCount() const { return MainTasks.Num(); }

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetBuildingType(int32 TypeId, FRokUnityCatalogBuildingType& OutBuildingType) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetTroop(int32 Id, FRokUnityCatalogTroop& OutTroop) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHero(int32 Id, FRokUnityCatalogHero& OutHero) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHeroSkill(int32 Id, FRokUnityCatalogHeroSkill& OutSkill) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetHeroTalent(int32 Id, FRokUnityCatalogHeroTalent& OutTalent) const;

	UFUNCTION(BlueprintPure, Category="Rok Unity Catalog")
	bool GetMainTask(int32 Id, FRokUnityCatalogTask& OutTask) const;

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

private:
	FRokUnityGameplayCatalogSummary Summary;
	TMap<int32, FRokUnityCatalogBuildingType> BuildingTypes;
	TMap<int32, FRokUnityCatalogTroop> Troops;
	TMap<int32, FRokUnityCatalogHero> Heroes;
	TMap<int32, FRokUnityCatalogHeroSkill> HeroSkills;
	TMap<int32, FRokUnityCatalogHeroTalent> HeroTalents;
	TMap<int32, FRokUnityCatalogTask> MainTasks;
	bool bCatalogLoaded = false;

	static FRokUnityGameplayCatalogSummary ParseSummary(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogBuildingType ParseBuildingType(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogTroop ParseTroop(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHero ParseHero(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHeroSkill ParseHeroSkill(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogHeroTalent ParseHeroTalent(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityCatalogTask ParseTask(const TSharedPtr<FJsonObject>& Object);
};
