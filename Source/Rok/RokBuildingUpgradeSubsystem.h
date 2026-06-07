#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokBuildingUpgradeSubsystem.generated.h"

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
	Decoration
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

UCLASS()
class ROK_API URokBuildingUpgradeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokBuildingState GetBuildingStateForActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	bool StartUpgrade(AActor* Actor, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	float GetUpgradeProgress(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	float GetRemainingUpgradeSeconds(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FRokResourceCost GetNextUpgradeCost(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	bool CanStartUpgrade(AActor* Actor, FString& OutFailureReason);

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

	UFUNCTION(BlueprintCallable, Category="Rok Building")
	FText GetBuildingTypeText(ERokBuildingType Type) const;

private:
	UPROPERTY()
	TMap<FName, FRokBuildingState> BuildingStates;

	UPROPERTY()
	FRokResourceStock CurrentResources;

	UPROPERTY()
	FRokResourceRates ProductionRates;

	TMap<int32, TMap<int32, FRokUnityBuildingLevelConfig>> UnityBuildingLevelConfigs;
	bool bUnityBuildingConfigLoaded = false;
	TMap<int32, FRokUnityInitialBuildingState> UnityInitialBuildings;
	bool bUnityInitialCityStateLoaded = false;

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
	bool LoadUnityBuildingLevelConfig();
	bool LoadUnityInitialCityState();
	const FRokUnityInitialBuildingState* FindUnityInitialBuilding(AActor* Actor) const;
	ERokBuildingType MapUnityBuildingTypeId(int32 TypeId) const;
	int32 ExtractTaggedInt(AActor* Actor, const TCHAR* Prefix) const;
	bool CanAfford(const FRokResourceCost& Cost) const;
	void SpendResources(const FRokResourceCost& Cost);
};
