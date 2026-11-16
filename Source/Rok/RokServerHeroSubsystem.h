#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokServerHeroSubsystem.generated.h"

class URokUnityGameplayCatalogSubsystem;
class URokUnityConfigSubsystem;

enum class ERokHeroCommand : uint8 { Summon, UpgradeSkill, LearnTalent, SwitchTalent, ResetTalent, RenameTalent };

struct FRokHeroCommand
{
	ERokHeroCommand Type = ERokHeroCommand::Summon;
	int32 HeroId = 0;
	int32 Page = 0;
	int32 TalentId = 0;
	bool bUseGems = false;
	FString Name;
};

struct FRokHeroCommandCost
{
	int32 ItemId = 0;
	int32 ItemCount = 0;
	int64 AvailableItems = 0;
	int32 Gems = 0;
};

struct FRokHeroTalentNode
{
	int32 Id = 0;
	int32 Group = 0;
	int32 Level = 0;
	int32 NameId = 0;
	int32 DescriptionId = 0;
};

USTRUCT(BlueprintType)
struct FRokServerHeroSkill
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 SkillId = 0;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;
	UPROPERTY(BlueprintReadOnly) bool bLevelKnown = false;
};

USTRUCT(BlueprintType)
struct FRokServerHeroTalentTree
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Index = 0;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) bool bNameKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<int32> TalentIds;
	UPROPERTY(BlueprintReadOnly) bool bTalentIdsKnown = false;
};

USTRUCT(BlueprintType)
struct FRokServerHeroGear
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int64 Head = 0;
	UPROPERTY(BlueprintReadOnly) int64 BreastPlate = 0;
	UPROPERTY(BlueprintReadOnly) int64 Weapon = 0;
	UPROPERTY(BlueprintReadOnly) int64 Gloves = 0;
	UPROPERTY(BlueprintReadOnly) int64 Pants = 0;
	UPROPERTY(BlueprintReadOnly) int64 Accessories1 = 0;
	UPROPERTY(BlueprintReadOnly) int64 Accessories2 = 0;
	UPROPERTY(BlueprintReadOnly) int64 Shoes = 0;
	UPROPERTY(BlueprintReadOnly) bool bHeadKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bBreastPlateKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bWeaponKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bGlovesKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bPantsKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bAccessories1Known = false;
	UPROPERTY(BlueprintReadOnly) bool bAccessories2Known = false;
	UPROPERTY(BlueprintReadOnly) bool bShoesKnown = false;
};

USTRUCT(BlueprintType)
struct FRokServerHeroEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 HeroId = 0;
	UPROPERTY(BlueprintReadOnly) bool bCatalogKnown = false;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Title;
	UPROPERTY(BlueprintReadOnly) FString Icon;
	UPROPERTY(BlueprintReadOnly) FString IconAtlas;
	UPROPERTY(BlueprintReadOnly) FString IconSprite;
	UPROPERTY(BlueprintReadOnly) int32 Rare = 0;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;
	UPROPERTY(BlueprintReadOnly) int32 Star = 0;
	UPROPERTY(BlueprintReadOnly) int64 Exp = 0;
	UPROPERTY(BlueprintReadOnly) int64 StarExp = 0;
	UPROPERTY(BlueprintReadOnly) bool bLevelKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bStarKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bExpKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bStarExpKnown = false;
	UPROPERTY(BlueprintReadOnly) int32 Power = 0;
	UPROPERTY(BlueprintReadOnly) bool bPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bSkillsKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerHeroSkill> Skills;
	UPROPERTY(BlueprintReadOnly) bool bTalentTreesKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerHeroTalentTree> TalentTrees;
	UPROPERTY(BlueprintReadOnly) int32 TalentIndex = 0;
	UPROPERTY(BlueprintReadOnly) bool bTalentIndexKnown = false;
	UPROPERTY(BlueprintReadOnly) FRokServerHeroGear Gear;
};

USTRUCT(BlueprintType)
struct FRokServerHeroSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bLoaded = false;
	UPROPERTY(BlueprintReadOnly) bool bHeroesKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bRowsComplete = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerHeroEntry> Heroes;
	UPROPERTY(BlueprintReadOnly) FString Signature;
};

UCLASS()
class ROK_API URokServerHeroSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Rok Server Hero")
	FRokServerHeroSnapshot GetSnapshot() const;
	int32 GetNotificationCount() const;
	static FRokServerHeroSnapshot ProjectSnapshot(
		const TSharedPtr<FJsonObject>& HeroInfo,
		const URokUnityGameplayCatalogSubsystem* Catalog, URokUnityConfigSubsystem* Config = nullptr);
	bool GetItemCount(int32 ItemId, int64& Count) const;
	mutable FString LastNotificationSignature;
	mutable int32 CachedNotificationCount = 0;
	bool GetCommandCost(const FRokHeroCommand& Request, FRokHeroCommandCost& Cost, FString& Error) const;
	bool SubmitCommand(const FRokHeroCommand& Request, TFunction<void(const FString&)> OnResult, FString& Error);
	bool GetTalentPage(const FRokServerHeroEntry& Hero, int32 Page, TArray<FRokHeroTalentNode>& Nodes, int32& Points) const;
	static bool IsCommandApplied(const FRokHeroCommand& Request, const TSharedPtr<FJsonObject>& HeroInfo,
		int32 SkillId = 0, int32 SkillLevel = 0);
#if WITH_DEV_AUTOMATION_TESTS
	void SetSnapshotForTesting(const FRokServerHeroSnapshot& InSnapshot) { TestSnapshot = InSnapshot; }
	void ClearSnapshotForTesting() { TestSnapshot.Reset(); }
#endif
private:
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokServerHeroSnapshot> TestSnapshot;
#endif
};
