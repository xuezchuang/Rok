#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokTaskChapterConfigPreviewData.h"
#include "RokServerTaskSubsystem.generated.h"

class URokUnityGameplayCatalogSubsystem;

enum class ERokTaskClaim : uint8 { Main, Side, ChapterTask, Daily, Chapter, ActivePoints };

UENUM(BlueprintType)
enum class ERokServerTaskPage : uint8 { Main, Side, Chapter, Daily };

UENUM(BlueprintType)
enum class ERokServerTaskState : uint8 { Unknown, Unfinished, Finished, Received };

USTRUCT(BlueprintType)
struct FRokServerTaskView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 TaskId = 0;
	UPROPERTY(BlueprintReadOnly) int32 Type = 0;
	UPROPERTY(BlueprintReadOnly) int32 Param1 = 0;
	UPROPERTY(BlueprintReadOnly) int32 Param2 = 0;
	UPROPERTY(BlueprintReadOnly) int32 Require = 0;
	UPROPERTY(BlueprintReadOnly) int32 RewardGroupId = 0;
	UPROPERTY(BlueprintReadOnly) ERokServerTaskPage Page = ERokServerTaskPage::Main;
	UPROPERTY(BlueprintReadOnly) ERokServerTaskState State = ERokServerTaskState::Unknown;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Description;
	UPROPERTY(BlueprintReadOnly) FString Icon;
	UPROPERTY(BlueprintReadOnly) bool bProgressKnown = false;
	UPROPERTY(BlueprintReadOnly) int64 Current = 0;
	UPROPERTY(BlueprintReadOnly) int64 Target = 0;
};

USTRUCT(BlueprintType)
struct FRokServerTaskMilestone
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Points = 0;
	UPROPERTY(BlueprintReadOnly) int32 RewardGroupId = 0;
	UPROPERTY(BlueprintReadOnly) ERokServerTaskState State = ERokServerTaskState::Unknown;
};

USTRUCT(BlueprintType)
struct FRokServerTaskSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bLoaded = false;
	UPROPERTY(BlueprintReadOnly) bool bDailyLoaded = false;
	UPROPERTY(BlueprintReadOnly) int32 MainTaskId = 0;
	UPROPERTY(BlueprintReadOnly) int32 ChapterId = 0;
	UPROPERTY(BlueprintReadOnly) FString ChapterName;
	UPROPERTY(BlueprintReadOnly) FString ChapterDescription;
	UPROPERTY(BlueprintReadOnly) int32 ChapterRewardGroupId = 0;
	UPROPERTY(BlueprintReadOnly) int64 ChapterCurrent = 0;
	UPROPERTY(BlueprintReadOnly) int64 ChapterTarget = 0;
	UPROPERTY(BlueprintReadOnly) bool bChapterProgressKnown = false;
	UPROPERTY(BlueprintReadOnly) bool bChapterClaimable = false;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskView> MainTasks;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskView> SideTasks;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskView> ChapterTasks;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskView> DailyTasks;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskMilestone> DailyMilestones;
	UPROPERTY(BlueprintReadOnly) int64 DailyActivePoints = -1;
	UPROPERTY(BlueprintReadOnly) TArray<FRokServerTaskView> HudTasks;
	UPROPERTY(BlueprintReadOnly) FString Signature;
};

// Projects server task state and validates claims against the latest snapshot.
UCLASS()
class ROK_API URokServerTaskSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UFUNCTION(BlueprintPure, Category="Rok Server Tasks")
	FRokServerTaskSnapshot GetSnapshot() const;
	bool Claim(ERokTaskClaim Kind, int32 Id, TFunction<void(const FString&)> OnResult, FString& OutError);
	static bool IsClaimApplied(ERokTaskClaim Kind, int32 Id, const TSharedPtr<FJsonObject>& Role,
		const TSharedPtr<FJsonObject>& Daily);
	static FRokServerTaskSnapshot ProjectSnapshot(
		const TSharedPtr<FJsonObject>& Role,
		const TSharedPtr<FJsonObject>& DailyTaskInfo,
		const TSharedPtr<FJsonObject>& Buildings,
		const URokUnityGameplayCatalogSubsystem* Catalog,
		const TArray<FRokTaskChapterPreviewRow>& Chapters,
		const TArray<FRokTaskChapterTaskPreviewRow>& ChapterRows);
#if WITH_DEV_AUTOMATION_TESTS
	void SetSnapshotForTesting(const FRokServerTaskSnapshot& InSnapshot) { TestSnapshot = InSnapshot; }
	void ClearSnapshotForTesting() { TestSnapshot.Reset(); }
#endif
private:
	TArray<FRokTaskChapterPreviewRow> Chapters;
	TArray<FRokTaskChapterTaskPreviewRow> ChapterRows;
#if WITH_DEV_AUTOMATION_TESTS
	TOptional<FRokServerTaskSnapshot> TestSnapshot;
#endif
};
