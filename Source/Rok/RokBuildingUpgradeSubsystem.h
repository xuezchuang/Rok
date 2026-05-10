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
	AllianceCenter
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
	int32 Level = 1;

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
	FText GetBuildingTypeText(ERokBuildingType Type) const;

private:
	UPROPERTY()
	TMap<FName, FRokBuildingState> BuildingStates;

	UPROPERTY()
	FRokResourceStock CurrentResources;

	FRokBuildingState& GetOrCreateBuildingState(AActor* Actor);
	bool RefreshUpgradeCompletion(FRokBuildingState& State);
	FName MakeBuildingKey(AActor* Actor) const;
	FString GetActorDisplayString(AActor* Actor) const;
	ERokBuildingType InferBuildingType(const FString& Label) const;
	TSoftObjectPtr<UTexture2D> ResolveIcon(ERokBuildingType Type) const;
	float GetUpgradeDurationSeconds(const FRokBuildingState& State) const;
	FRokResourceCost GetNextUpgradeCost(const FRokBuildingState& State) const;
	bool CanAfford(const FRokResourceCost& Cost) const;
	void SpendResources(const FRokResourceCost& Cost);
};
