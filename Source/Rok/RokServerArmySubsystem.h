#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokResearchCatalog.h"
#include "RokServerArmySubsystem.generated.h"

struct FRokServerHeroEntry;
struct FRokUnityCatalogTroop;

struct FRokBarbarianAttackInfo
{
	int32 Level = 0;
	int32 NameId = 0;
	int32 DescriptionId = 0;
	int32 RewardGroupId = 0;
	int64 ExpiresAt = 0;
	FString HeadIcon;
	int32 ActionCost = 0;
	int32 AttackLevelMax = 0;
	int64 ActionForce = -1;
	bool bSituStation = false;
	FString BlockReason;
};

struct FRokGatherPointState
{
	bool bOwned = false;
	bool bCanRecall = false;
	int64 Gathered = -1;
	int64 Remaining = -1;
	int64 Goal = -1;
	bool bGoalApproximate = false;
};

class URokUnityGameplayCatalogSubsystem;

UENUM(BlueprintType)
enum class ERokServerTrainingState : uint8
{
	Unknown, Idle, Training, Upgrading, AwaitingReceive
};

UENUM(BlueprintType)
enum class ERokServerMarchPhase : uint8
{
	Unknown, Marching, Returning, Garrisoned, Collecting, Stationed, Battling, Rallying
};

USTRUCT(BlueprintType)
struct FRokServerSoldierStack
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Id = 0;
	UPROPERTY(BlueprintReadOnly) int32 FamilyType = 0;
	UPROPERTY(BlueprintReadOnly) int32 Tier = 0;
	UPROPERTY(BlueprintReadOnly) int64 Count = 0;
	UPROPERTY(BlueprintReadOnly) int64 Minor = 0;
	UPROPERTY(BlueprintReadOnly) int32 PowerPerUnit = 0;
	// Contribution to source troop power; minor field soldiers do not add power.
	UPROPERTY(BlueprintReadOnly) int64 TotalPower = 0;
	UPROPERTY(BlueprintReadOnly) bool bCatalogKnown = false;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Icon;
};

USTRUCT(BlueprintType)
struct FRokServerTrainingBuildingView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 BuildingIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 BuildingType = 0;
	UPROPERTY(BlueprintReadOnly) int32 Level = 0;
	UPROPERTY(BlueprintReadOnly) FString Name;
};

USTRUCT(BlueprintType)
struct FRokServerTrainingQueue
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 QueueIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 BuildingIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 BuildingType = 0;
	UPROPERTY(BlueprintReadOnly) int32 BuildingLevel = 0;
	UPROPERTY(BlueprintReadOnly) FString BuildingName;
	UPROPERTY(BlueprintReadOnly) bool bBuildingKnown = false;
	UPROPERTY(BlueprintReadOnly) int32 ArmyType = 0;
	UPROPERTY(BlueprintReadOnly) int32 NewTier = 0;
	UPROPERTY(BlueprintReadOnly) int32 OldTier = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetTroopId = 0;
	UPROPERTY(BlueprintReadOnly) int64 Count = 0;
	UPROPERTY(BlueprintReadOnly) int64 BeginTime = 0;
	UPROPERTY(BlueprintReadOnly) int64 FinishTime = 0;
	UPROPERTY(BlueprintReadOnly) int64 FirstFinishTime = 0;
	UPROPERTY(BlueprintReadOnly) bool bTimeKnown = false;
	UPROPERTY(BlueprintReadOnly) double RemainingSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly) double DurationSeconds = 0.0;
	UPROPERTY(BlueprintReadOnly) double Progress01 = 0.0;
	UPROPERTY(BlueprintReadOnly) FString TroopName;
	UPROPERTY(BlueprintReadOnly) ERokServerTrainingState State = ERokServerTrainingState::Unknown;
};

USTRUCT(BlueprintType)
struct FRokServerMarch
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 ArmyIndex = 0;
	UPROPERTY(BlueprintReadOnly) int32 MainHeroId = 0;
	UPROPERTY(BlueprintReadOnly) int32 DeputyHeroId = 0;
	UPROPERTY(BlueprintReadOnly) int64 StatusMask = 0;
	bool bInRally = false;
	UPROPERTY(BlueprintReadOnly) ERokServerMarchPhase Phase = ERokServerMarchPhase::Unknown;
	UPROPERTY(BlueprintReadOnly) int32 TargetType = 0;
	int64 TargetObjectId = 0;
	int64 MapObjectId = 0;
	UPROPERTY(BlueprintReadOnly) FString TargetName;
	UPROPERTY(BlueprintReadOnly) int32 TargetX = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetY = 0;
	UPROPERTY(BlueprintReadOnly) bool bTargetKnown = false;
	UPROPERTY(BlueprintReadOnly) int64 StartTime = 0;
	UPROPERTY(BlueprintReadOnly) int64 ArrivalTime = 0;
	UPROPERTY(BlueprintReadOnly) bool bTimeKnown = false;
	UPROPERTY(BlueprintReadOnly) double RemainingSeconds = 0.0;
	TArray<FVector2D> AttackPath;
	bool TryGetAttackMarchPosition(FVector2D& OutPosition, int32* OutNextPoint = nullptr,
		double ElapsedSinceSnapshot = 0.0) const;
	static ERokServerMarchPhase ClassifyStatus(int64 Status);
	UPROPERTY(BlueprintReadOnly) int64 Count = 0;
	UPROPERTY(BlueprintReadOnly) int64 MinorCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bSoldiersKnown = false;
	UPROPERTY(BlueprintReadOnly) int64 Power = 0;
	UPROPERTY(BlueprintReadOnly) bool bPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerSoldierStack> Soldiers;
};

USTRUCT(BlueprintType)
struct FRokServerArmySnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bLoaded = false;
	UPROPERTY(BlueprintReadOnly) bool bCityKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bArmiesKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bTrainingKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bBuildingsKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<int32> KnownTrainingBuildingIndices;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTrainingBuildingView> TrainingBuildings;
	UPROPERTY(BlueprintReadOnly) bool bDispatchCapacityKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bDispatchUsedKnown = false;
	UPROPERTY(BlueprintReadOnly) int32 TransportCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bHudQueueKnown = false;
	UPROPERTY(BlueprintReadOnly) int32 ActiveScoutCount = 0;
	UPROPERTY(BlueprintReadOnly) int32 HudQueueRowCount = 0;
	// Unity TroopProxy.GetAllTroopCount = live armies + live transports.
	UPROPERTY(BlueprintReadOnly) int32 DispatchSlotsUsed = 0;
	UPROPERTY(BlueprintReadOnly) int32 DispatchSlotsMax = 0;
	UPROPERTY(BlueprintReadOnly) int64 CityCount = 0;
	UPROPERTY(BlueprintReadOnly) int64 FieldCount = 0;
	UPROPERTY(BlueprintReadOnly) int64 FieldMinorCount = 0;
	UPROPERTY(BlueprintReadOnly) int64 UpgradingCount = 0;
	UPROPERTY(BlueprintReadOnly) int64 TotalCount = 0;
	UPROPERTY(BlueprintReadOnly) bool bTotalCountKnown = false;
	UPROPERTY(BlueprintReadOnly) int64 CityPower = 0;
	UPROPERTY(BlueprintReadOnly) int64 FieldPower = 0;
	UPROPERTY(BlueprintReadOnly) int64 UpgradingPower = 0;
	UPROPERTY(BlueprintReadOnly) int64 TotalPower = 0;
	UPROPERTY(BlueprintReadOnly) bool bCityPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bFieldPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bUpgradingPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bTotalPowerKnown = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerSoldierStack> CitySoldiers;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerSoldierStack> FieldSoldiers;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerSoldierStack> AllSoldiers;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTrainingQueue> TrainingQueues;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerMarch> Marches;
	// Structural/display data and phase changes only; clock countdown changes do not rebuild rows.
	UPROPERTY(BlueprintReadOnly) FString Signature;
};

UCLASS()
class ROK_API URokServerArmySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UFUNCTION(BlueprintPure, Category="Rok Server Army")
	FRokServerArmySnapshot GetSnapshot() const;
	bool GetGatherOptions(int64 TargetId, FRokServerArmySnapshot& Army,
		TArray<FRokServerHeroEntry>& Heroes, FString& Error) const;
	bool RequestGather(int64 TargetId, int32 MainHeroId, int32 DeputyHeroId,
		const TMap<int32, int64>& Counts, TFunction<void(int32, const FString&)> OnResult, FString& Error);
	bool GetBarbarianAttackInfo(int64 TargetId, FRokBarbarianAttackInfo& Info) const;
	bool GetBarbarianArmyOptions(int64 TargetId, FRokServerArmySnapshot& Army,
		TArray<FRokServerHeroEntry>& Heroes, FString& Error) const;
	bool RequestBarbarianAttack(int64 TargetId, int32 MainHeroId, int32 DeputyHeroId, const TMap<int32, int64>& Counts,
		bool bSituStation, TFunction<void(int32, const FString&)> OnResult, FString& Error);
	bool RequestExistingArmyAttack(int64 TargetId, int32 ArmyIndex, bool bSituStation,
		TFunction<void(int32, const FString&)> OnResult, FString& Error);
	static bool CanRetargetArmy(const FRokServerMarch& Army, int64 TargetId);
	bool GetTrainingTroop(int32 ArmyType, int32 Tier, FRokUnityCatalogTroop& Out) const;
	bool IsTrainingTierUnlocked(int32 StudyId) const;
	int32 GetTrainingCapacityBonus() const;
	int64 GetTrainingSeconds(const FRokUnityCatalogTroop& Troop, int32 Count) const;
	bool GetInstantTrainingPrice(const FRokUnityCatalogTroop& Troop, int32 Count, int64& Gems) const;
	bool RequestTraining(int32 BuildingIndex, int32 ArmyType, int32 Tier, int32 Count,
		TFunction<void(const FString&)> OnResult, FString& Error, TOptional<int64> ConfirmedGems = {});
	bool RequestTrainingAward(int32 BuildingIndex, int32 ArmyType,
		TFunction<void(const FString&)> OnResult, FString& Error);

	FRokGatherPointState GetGatherPointState(const FString& ObjectId) const;
	bool RequestGatherRecall(const FString& ObjectId, TFunction<void(const FString&)> OnResult, FString& Error);
	static FRokServerArmySnapshot ProjectSnapshot(
		const TSharedPtr<FJsonObject>& Role,
		const TSharedPtr<FJsonObject>& ArmyInfo,
		const TSharedPtr<FJsonObject>& TransportInfo,
		const TSharedPtr<FJsonObject>& ScoutsQueue,
		const TSharedPtr<FJsonObject>& Buildings,
		const URokUnityGameplayCatalogSubsystem* Catalog,
		const TMap<int32, int32>& DispatchSlotsByTownHallLevel,
		const TMap<int32, TArray<int32>>& FeatureArmsByCountry,
		bool bHasServerTime, double ServerTimeSeconds);
#if WITH_DEV_AUTOMATION_TESTS
	void SetSnapshotForTesting(const FRokServerArmySnapshot& InSnapshot) { TestSnapshot = InSnapshot; }
	void ClearSnapshotForTesting() { TestSnapshot.Reset(); }
#endif
private:
	friend class FRokResourceGatherNetTest;
	friend class FRokResourceRecallNetTest;
	friend class FRokBarbarianAttackNetTest;
	static FRokGatherPointState ProjectGatherPoint(const TSharedPtr<FJsonObject>& Point,
		const TSharedPtr<FJsonObject>& Army, int64 RoleId, double ServerSeconds, int64 BaseGoal = -1);
	static TSharedPtr<FJsonObject> MakeGatherRequest(int64 TargetId, const TSharedPtr<FJsonObject>& Target,
		const FRokServerArmySnapshot& Army, const TArray<FRokServerHeroEntry>& Heroes, int32 MainHeroId,
		int32 DeputyHeroId, const TMap<int32, int64>& Counts, bool bSituStation, FString& Error, bool bAttack = false);
	bool GetCityArmyOptions(FRokServerArmySnapshot& Army, TArray<FRokServerHeroEntry>& Heroes, FString& Error) const;
	static TSharedPtr<FJsonObject> MakeExistingAttackRequest(int64 TargetId, const TSharedPtr<FJsonObject>& Target,
		int32 ArmyIndex, bool bSituStation, FString& Error);
	FRokResearchCatalog TrainingResearch;
	TMap<int32, int32> DispatchSlotsByTownHallLevel;
	TMap<int32, TArray<int32>> FeatureArmsByCountry;
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokServerArmySnapshot> TestSnapshot;
#endif
};
