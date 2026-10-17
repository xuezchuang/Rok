#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityConstructionGrid.h"
#include "RokCityConstructionSources.h"
#include "RokLocalCitySimulationContext.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.generated.h"

class ARokCitySpriteActor;

// The upgrade wallet is only authoritative for the isolated local preview.
// Keep this separate from the city-map construction context: upgrade fixtures
// and world-map building panels also use the same world-scoped subsystem.
struct FRokLocalEconomyAuthoritySnapshot
{
	bool bRuntimeLoaded = false;
	FRokUnityRuntimePlayerProfileState Profile;
};

UENUM(BlueprintType)
enum class ERokBuildingType : uint8
{
	Unknown,
	TownCenter,
	CityWall,
	GuardTower,
	Barracks,
	Stable,
	ArcheryRange,
	Hospital,
	Farm,
	Lumbermill,
	Quarry,
	Goldmine,
	Storehouse,
	Tavern,
	Monument,
	ScoutCamp,
	AllianceCenter,
	BuilderHut,
	NoticeBoard,
	Decoration,
	SiegeWorkshop,
	Academy
};

USTRUCT(BlueprintType)
struct FRokResourceCost
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Food = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Wood = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Stone = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Gold = 0;
};

USTRUCT(BlueprintType)
struct FRokResourceStock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Food = 2500;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Wood = 2500;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Stone = 1200;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Gold = 600;
};

USTRUCT(BlueprintType)
struct FRokResourceRates
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 FoodPerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 WoodPerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 StonePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GoldPerHour = 0;
};

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityBuildingPrerequisite
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 TypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Level = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityBuildingLevelConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 TypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FRokResourceCost Cost;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 BuildSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Power = 0;

	// Required source economy fields must parse exactly before local resource upgrades.
	bool bSourceEconomyValid = false;
	bool ParseSourceEconomy(const TSharedPtr<FJsonObject>& Row);

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	TArray<FRokUnityBuildingPrerequisite> Prerequisites;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	bool bPrerequisitesValid = false;

	// Shared by registry/fallback ingestion; a missing slot is not an unlocked slot.
	bool ParsePrerequisites(const TSharedPtr<FJsonObject>& Row);
	bool CheckPrerequisites(const TMap<int32, int32>& CompletedLevels, FRokUnityBuildingPrerequisite& OutMissing, int32& OutCurrentLevel) const;
};

USTRUCT(BlueprintType)
struct FRokUnityBuildingProduceConfig
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 TypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 ProducePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMin = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMax = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityInitialBuildingState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 InitId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 TypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GridX = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GridY = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	bool bDecoration = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FString SpriteName;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Power = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 ProducePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMin = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMax = 0;
};

USTRUCT(BlueprintType)
struct FRokBuildingState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FName BuildingKey;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	ERokBuildingType Type = ERokBuildingType::Unknown;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 UnityInitId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 UnityBuildingTypeId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 UnityGridX = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 UnityGridY = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 Power = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 ProducePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMin = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 GatherMax = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	bool bUpgradeInProgress = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	float UpgradeStartedAt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	float UpgradeFinishesAt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FRokBuildingUpgradePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FRokBuildingState CurrentState;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 TargetLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 CurrentPower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 NextPower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 CurrentProducePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 NextProducePerHour = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 CurrentGatherMax = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 NextGatherMax = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FRokResourceCost Cost;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	int32 BuildSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	bool bCanStartTimedUpgrade = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Building")
	FString FailureReason;
};

/** Civilization and age are optional assertions against the verified local visual fixture. */
struct FRokLocalConstructionRequest
{
	int32 UnityBuildingTypeId = 0;
	int32 CivilizationId = 0;
	int32 Age = 0;
	FIntPoint Grid = FIntPoint::ZeroValue;
};

struct FRokLocalConstructionPreview
{
	bool bContextVerified = false;
	bool bSourceEligible = false;
	bool bCanCommit = false;
	bool bPositionEvaluated = false;
	FString FailureReason;
	int32 VisualCivilization = 0;
	int32 ResolvedAge = 0;
	int32 MissingTypeId = 0;
	int32 RequiredLevel = 0;
	int32 CurrentLevel = 0;
	FIntPoint Footprint = FIntPoint::ZeroValue;
	FRokResourceCost Cost;
	int32 BuildSeconds = 0;
	int32 Power = 0;
	int32 CountLimit = 0;
	int32 ExistingCount = 0;
	FString SourceModelId;
	TArray<int32> SourceCellIndices;
};

/** A committed local queue result, never a Role_RoleInfo/server power update. */
struct FRokLocalBuildingUpgradeCompleted
{
	FName BuildingKey;
	int32 UnityInitId = 0;
	int32 UnityBuildingTypeId = 0;
	int32 PreviousLevel = 0;
	int32 CurrentLevel = 0;
	int32 PreviousPower = 0;
	int32 CurrentPower = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FRokLocalBuildingUpgradeEvent, const FRokLocalBuildingUpgradeCompleted&);

UCLASS()
class ROK_API URokBuildingUpgradeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// Broadcast after the subsystem tick has finished iterating its state map.
	FRokLocalBuildingUpgradeEvent OnLocalUpgradeCompleted;
	// Source-derived construction is deliberately distinct from a UnityInitId building.
	bool PreviewLocalConstructionEntry(int32 UnityBuildingTypeId, FRokLocalConstructionPreview& OutPreview) const;
	bool PreviewLocalConstruction(const FRokLocalConstructionRequest& Request, FRokLocalConstructionPreview& OutPreview) const;
	bool StartLocalConstruction(const FRokLocalConstructionRequest& Request, uint64& OutLocalConstructionId, FString& OutFailureReason);
	bool GetLocalConstructionTicket(uint64 LocalConstructionId, FRokLocalConstructionTicket& OutTicket) const;
	int32 GetActiveLocalConstructionCount() const;
	ARokCitySpriteActor* GetLocalConstructionActor(uint64 LocalConstructionId) const;
	bool IsLocalConstructionPending(uint64 LocalConstructionId) const;
	bool GetLocalConstructionCompletionFailure(uint64 LocalConstructionId, FString& OutFailureReason) const;
	bool RegisterCompletedLocalConstructionActor(uint64 LocalConstructionId, ARokCitySpriteActor* Actor, FString& OutFailureReason);
	bool QueryLocalProduction(uint64 LocalConstructionId,
		FRokCityResourceProductionResult& OutResult, FString& OutFailureReason) const;
	bool CollectLocalProduction(uint64 LocalConstructionId,
		int64& OutCollected, FString& OutFailureReason);
	// World-scoped, source-config-derived increment. Read-only: no timer refresh.
	int32 GetLocalCompletedBuildingPowerDelta() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokBuildingState GetBuildingStateForActor(AActor* Actor);
	// Pure cache read: no actor binding insertion or elapsed-upgrade settlement.
	bool TryPeekExistingBuildingStateForActor(const AActor* Actor, FRokBuildingState& OutState) const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	bool StartUpgrade(AActor* Actor, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	float GetUpgradeProgress(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	float GetRemainingUpgradeSeconds(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	AActor* GetFirstActiveUpgradeActor() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokResourceCost GetNextUpgradeCost(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	bool CanStartUpgrade(AActor* Actor, FString& OutFailureReason);

#if WITH_DEV_AUTOMATION_TESTS
	// Per-instance identity seam for authority tests; never changes runtime/profile data.
	void SetLocalEconomyAuthorityForTesting(const FRokLocalEconomyAuthoritySnapshot& Snapshot);
	void ClearLocalEconomyAuthorityForTesting();
	int32 GetPendingCompletedUpgradeCountForTesting() const { return PendingCompletedUpgrades.Num(); }
#endif

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokResourceStock GetCurrentResources() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetActiveUpgradeCount() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetMaxConcurrentUpgrades() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetMaxBuildingLevel() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetMaxBuildingLevelForActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokResourceRates GetProductionRates() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetInitialCityBuildingCount() const;

	// Unity type ids present in the initial city state; mirrors the built-type
	// set Unity TaskProxy checks showBuildingReq against.
	TArray<int32> GetUnityInitialBuildingTypeIds() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FText GetBuildingTypeText(ERokBuildingType Type) const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	bool GetUpgradePreview(AActor* Actor, FRokBuildingUpgradePreview& OutPreview);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	int32 GetOwnedBuildQueueCount() const;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	float GetBuildSpeedBonusPercent() const;

private:
	UPROPERTY()
	TMap<FName, FRokBuildingState> BuildingStates;

	TMap<FName, TWeakObjectPtr<AActor>> BuildingActors;
	TArray<FRokLocalBuildingUpgradeCompleted> PendingCompletedUpgrades;

	UPROPERTY()
	FRokResourceStock CurrentResources;

	UPROPERTY()
	FRokResourceRates ProductionRates;

	TMap<int32, TMap<int32, FRokUnityBuildingLevelConfig>> UnityBuildingLevelConfigs;
	bool bUnityBuildingConfigLoaded = false;
	TMap<int32, TMap<int32, FRokUnityBuildingProduceConfig>> UnityBuildingProduceConfigs;
	bool bUnityBuildingProduceConfigLoaded = false;
	TMap<int32, FRokUnityInitialBuildingState> UnityInitialBuildings;
	bool bUnityInitialCityStateLoaded = false;
	FRokCityConstructionGrid ConstructionGrid;
	FRokCityConstructionSources ConstructionSources;
	bool bConstructionSourcesLoaded = false;
	FRokLocalCitySimulationContext LocalCityContext;
	FRokLocalConstructionLedger LocalConstructionLedger;
	FRokLocalProductionLedger LocalProductionLedger;
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokLocalEconomyAuthoritySnapshot> LocalEconomyAuthorityOverride;
#endif
	// A completed local construction is trusted only after explicit actor binding.
	TMap<uint64, TWeakObjectPtr<AActor>> LocalConstructionActors;
	// Prepared source actors stay unselectable until their ticket is registered at completion.
	TMap<uint64, TWeakObjectPtr<ARokCitySpriteActor>> PendingLocalConstructionActors;
	TMap<uint64, FString> LocalConstructionCompletionFailures;

	FRokBuildingState& GetOrCreateBuildingState(AActor* Actor);
	bool RefreshUpgradeCompletion(FRokBuildingState& State);
	FName MakeBuildingKey(AActor* Actor) const;
	FString GetActorDisplayString(AActor* Actor) const;
	ERokBuildingType InferBuildingType(const FString& Label) const;
	TSoftObjectPtr<UTexture2D> ResolveIcon(ERokBuildingType Type) const;
	float GetUpgradeDurationSeconds(const FRokBuildingState& State) const;
	FRokResourceCost GetNextUpgradeCost(const FRokBuildingState& State) const;
	int32 GetMaxBuildingLevelForState(const FRokBuildingState& State) const;
	int32 GetUnityBuildingTypeId(ERokBuildingType Type) const;
	const FRokUnityBuildingLevelConfig* FindUnityBuildingConfig(const FRokBuildingState& State, int32 TargetLevel) const;
	const FRokUnityBuildingProduceConfig* FindUnityBuildingProduceConfig(const FRokBuildingState& State, int32 TargetLevel) const;
	bool LoadUnityBuildingLevelConfig();
	bool LoadUnityBuildingProduceConfig();
	bool LoadUnityInitialCityState();
	const FRokUnityInitialBuildingState* FindUnityInitialBuilding(AActor* Actor) const;
	ERokBuildingType MapUnityBuildingTypeId(int32 TypeId) const;
	int32 ExtractTaggedInt(AActor* Actor, const TCHAR* Prefix) const;
	bool CheckUpgradePrerequisites(const FRokBuildingState& State, FString& OutFailureReason) const;
	bool GetKnownCompletedBuildingLevels(TMap<int32, int32>& OutLevels) const;
	bool CanAfford(const FRokResourceCost& Cost) const;
	void SpendResources(const FRokResourceCost& Cost);
	bool IsRegisteredLocalConstruction(const FRokBuildingState& State) const;
	bool FindRegisteredLocalConstructionId(const FRokBuildingState& State, uint64& OutId) const;
	bool ValidateLocalResourceLevelTransition(const FRokBuildingState& State, uint64& OutId,
		FRokLocalProductionSource& OutCurrent, FRokLocalProductionSource& OutNext,
		int32& OutNextAggregateRate, FString& OutFailureReason) const;
	bool CanUseLocalCitySimulation(FString& OutFailureReason) const;
	bool CanUseLocalEconomy(FString& OutFailureReason) const;
	FRokLocalEconomyAuthoritySnapshot GetLocalEconomyAuthoritySnapshot() const;
	bool EvaluateLocalConstruction(const FRokLocalConstructionRequest& Request, bool bCheckPosition,
		FRokLocalConstructionPreview& OutPreview) const;
	bool ResolveLocalProductionNowSeconds(uint64 LocalConstructionId,
		int64& OutNowSeconds, FString& OutFailureReason) const;
};
