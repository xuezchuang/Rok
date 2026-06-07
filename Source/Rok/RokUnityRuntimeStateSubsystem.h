#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.generated.h"

class FJsonObject;

USTRUCT(BlueprintType)
struct FRokUnityRuntimeSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ActiveTaskId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ActiveTaskCurrent = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ActiveTaskTarget = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	bool bActiveTaskComplete = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ActiveTaskChainCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ReferenceTaskChainId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ReferenceTaskChainCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 StarterHeroCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 StarterTroopStackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TotalTroopCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TotalTroopPower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TotalHeroScore = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TrainableTroopCount = 0;
};

USTRUCT(BlueprintType)
struct FRokUnityRuntimeTaskState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TaskId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Desc;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Type = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Param1 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Param2 = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Require = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Reward = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 NextId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString ProgressRule;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Current = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Target = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	bool bComplete = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Icon;
};

USTRUCT(BlueprintType)
struct FRokUnityRuntimeHeroState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 HeroId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Star = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 MaxStar = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Rare = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 SkillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TalentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Model;
};

USTRUCT(BlueprintType)
struct FRokUnityRuntimeTroopStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TroopId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 ArmsType = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 PowerPerUnit = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TotalPower = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	int32 TrainSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Icon;

	UPROPERTY(BlueprintReadOnly, Category="Rok Unity Runtime")
	FString Model;
};

UCLASS()
class ROK_API URokUnityRuntimeStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Rok Unity Runtime")
	bool LoadRuntimeState();

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	bool IsRuntimeStateLoaded() const { return bRuntimeStateLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	FRokUnityRuntimeSummary GetSummary() const { return Summary; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	FRokUnityRuntimeTaskState GetActiveTask() const { return ActiveTask; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	TArray<FRokUnityRuntimeTaskState> GetActiveTaskChain() const { return ActiveTaskChain; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	TArray<FRokUnityRuntimeHeroState> GetStarterHeroes() const { return StarterHeroes; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	TArray<FRokUnityRuntimeTroopStack> GetStarterTroopStacks() const { return StarterTroopStacks; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	TArray<int32> GetTrainableTroopIds() const { return TrainableTroopIds; }

	UFUNCTION(BlueprintPure, Category="Rok Unity Runtime")
	FText GetRuntimeStatusText() const;

private:
	FRokUnityRuntimeSummary Summary;
	FRokUnityRuntimeTaskState ActiveTask;
	TArray<FRokUnityRuntimeTaskState> ActiveTaskChain;
	TArray<FRokUnityRuntimeHeroState> StarterHeroes;
	TArray<FRokUnityRuntimeTroopStack> StarterTroopStacks;
	TArray<int32> TrainableTroopIds;
	bool bRuntimeStateLoaded = false;

	static FRokUnityRuntimeSummary ParseSummary(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityRuntimeTaskState ParseTaskState(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityRuntimeHeroState ParseHeroState(const TSharedPtr<FJsonObject>& Object);
	static FRokUnityRuntimeTroopStack ParseTroopStack(const TSharedPtr<FJsonObject>& Object);
};
