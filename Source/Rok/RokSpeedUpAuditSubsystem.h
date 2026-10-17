#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokSpeedUpAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class URokUnityFeatureScreenWidget;
class URokBuildingPanelWidget;
class UUserWidget;
class APlayerController;
class URokUnityRuntimeStateSubsystem;
class URokPowerGainWidget;
class FJsonObject;
class FJsonValue;
class IToolTip;
class SWidget;

/** Explicit Game-only audit of a real, naturally expiring local building queue. */
UCLASS()
class ROK_API URokSpeedUpAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
	enum class EView : uint8 { City, BlockedUpgrade, HallUpgrade, HallActive, HallComplete, Upgrade, Opening, SpeedUp, Selected, ManualClosing, NaturalClosing, PowerEarly, PowerLater, CompletedCity, Worker, WorkerClosing };
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
	TWeakObjectPtr<URokBuildingPanelWidget> Panel;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Upgrades;
	TWeakObjectPtr<AActor> TargetActor, HallActor, WorkerActor, InitialPawn, InitialViewTarget;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> RuntimeState;
	TWeakObjectPtr<URokPowerGainWidget> PowerGain;
	FRokBuildingState InitialState, BoundState, HallInitialState, HallBoundState;
	FRokResourceStock ResourcesBefore, ResourcesAfter, ResourcesInitial, ResourcesAfterHall;
	TArray<TSharedPtr<FJsonValue>> Actions, Captures, QueueSamples, PowerSamples;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture, LastHallObservation, LastQueueObservation;
	TSharedPtr<FJsonObject> TaskHoverEvidence;
	TWeakPtr<IToolTip> TaskHoverInitialTooltip, TaskHoverLastTooltip;
	TWeakPtr<SWidget> TaskHoverInitialTooltipWidget, TaskHoverLastTooltipWidget;
	FDelegateHandle ScreenshotHandle, CompletionHandle;
	FString Directory, PendingPath;
	double Elapsed = 0, StepStarted = 0, ScreenshotStarted = 0, StartedWall = 0, UpgradeClickedWall = 0, CompletionWall = 0, LastSampleTime = -1, HallClickedWall = 0, HallCompletionWall = 0;
	double TaskHoverStartedWall = 0;
	FVector2D TaskHoverOriginalPointer = FVector2D::ZeroVector;
	uint64 Frame = 0;
	int32 Step = 0, CompletionTransitions = 0, CompletionEvents = 0, BaselineScore = 0, InitialScore = 0, PowerPlayCount = 0;
	int32 TaskHoverTooltipChanges = 0, TaskHoverWidgetChanges = 0, TaskHoverPolls = 0;
	float LastPowerPlayback = 0;
	EView View = EView::City;
	bool bFinished = false, bBegun = false, bBound = false, bCompleted = false, bDismissed = false, bWorkerSelected = false, bPowerWasPlaying = false;
	bool bHallSelected = false, bHallStartRequested = false, bHallBound = false, bHallCompleted = false, bTargetBaselineRecorded = false, bTargetStartRequested = false;
	bool bTaskHoverAuditRequested = false, bTaskHoverVerified = false, bTaskHoverPointerSaved = false;
	bool Begin();
	bool CheckIdentity();
	bool BindHallQueue();
	bool CheckHallQueue();
	bool SelectExistingBuilding(AActor* Actor);
	bool BindActualQueue();
	bool CheckQueue();
	bool CheckPower();
	void UpgradeCompleted(const FRokLocalBuildingUpgradeCompleted& Result);
	bool Validate();
	bool CheckSelectedActor(UUserWidget* Widget, AActor* Expected);
	bool Click(UUserWidget* Owner, FName Name);
	bool IsMidAnimation(bool bOpening = false) const;
	void Enter(int32 NextStep);
	TSharedRef<FJsonObject> State() const;
	void Capture(const TCHAR* Name);
	void ScreenshotProcessed();
	bool BeginTaskHoverAudit();
	bool PollTaskHoverAudit();
	bool CheckTaskHoverInvariant() const;
	void RestoreTaskHoverPointer();
	bool Save(bool bComplete);
	void Finish(const FString& Failure = FString());
};
