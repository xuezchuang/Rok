#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokTrainArmyDetail.h"
#include "RokTrainTierAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class UUserWidget;
class FJsonObject;
class FJsonValue;
class URokTrainArmyMeshWidget;

/** Opt-in city training browse audit. Never issues a training command. */
UCLASS()
class ROK_API URokTrainTierAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	enum class EStage : uint8 { Startup, Opened, Selected, QuantityReduced, ParametersReady, Detail, Skill, SkillClosed, Returned, Closed, Reopened, FinalClosed };
	EStage Stage = EStage::Startup;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
	TWeakObjectPtr<URokTrainArmyMeshWidget> AnimatedModel;
	TSet<int32> PaintedFrames;
	TArray<TSharedPtr<FJsonValue>> AnimationChecks;
	float LastAnimationTime = 0;
	double LastAnimationObservedAt = 0;
	uint64 LastPaintCount = 0;
	int32 ObservedArmsId = 0;
	FName PendingAnimationAction;
	TWeakObjectPtr<URokTrainArmyMeshWidget> ActionModel;
	int32 ActionArmsId = 0;
	uint64 ActionGeneration = 0;
	float ActionTime = 0;
	double ActionAt = 0;
	FRokTrainArmyDetail Detail;
	int32 Tier = 1, SkillIndex = 0, StudyId = 0, SelectedCount = 1;
	int32 QuantityBeforeAdjustment = 0;
	FString ExpectedName, OutputDirectory, ScreenshotPath;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures, Checks, QuantityChecks;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotHandle;
	double Elapsed = 0, StageStarted = 0, RequestedAt = 0;
	uint64 Frame = 0;
	bool bFinished = false;
	bool FindRuntime();
	bool LoadTier();
	bool Click(UUserWidget* Owner, FName Name);
	bool EditQuantityThroughSlider(int32 Count, int32 Capacity);
	bool Validate(bool bDetail);
	bool ObserveAnimation(bool bRequirePaint);
	bool ResolveAnimationAction();
	TSharedRef<FJsonObject> AnimationState() const;
	bool CheckText(FName Name, const FString& Expected);
	void Enter(EStage Next);
	void Capture(const FString& Name);
	void ScreenshotProcessed();
	bool Save(bool bComplete);
	void Fail(const FString& Reason);
	void Finish();
};
