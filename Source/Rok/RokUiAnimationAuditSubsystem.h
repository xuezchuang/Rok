#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokUiAnimationAuditSubsystem.generated.h"

class FJsonObject;
class FJsonValue;
class UButton;
class UUserWidget;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokScoutScreenWidget;
class URokTrainArmyMeshWidget;

/** Opt-in standalone visual audit. Never created in editor/PIE or shipping.
 * Uses real UMG delegates and real runtime ticks, not physical mouse input.
 */
UCLASS()
class ROK_API URokUiAnimationAuditSubsystem : public UTickableWorldSubsystem
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
	enum class EPhase : uint8
	{
		Waiting, TaskClose, TaskOpen, MoreClose, MoreOpen,
		ButtonPress, ButtonRelease, ItemsOpen, ItemsClose,
		ChatOpen, ContactsHide, ContactsShow, ChatChannel, ChatEmoji, ChatClose,
		CommanderOpen, CommanderSort, CommanderClose,
		ArmyOpen, ArmyRules, ArmyList, ArmyClose,
		TrainOpen, TrainDetail, TrainTip, TrainReturn, TrainClose,
		BuildingDescription, BuildingLevels, BuildingScroll, BuildingReturn, BuildingReopen, BuildingClose,
		ScoutInitial, ScoutDescription, ScoutReturn, ScoutClose, Finish
	};
	EPhase Phase = EPhase::Waiting;
	EPhase PhaseAfterCapture = EPhase::Waiting;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokScoutScreenWidget> Scout;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotProcessedHandle;
	FString OutputDirectory;
	FString PendingScreenshotPath;
	double ElapsedSeconds = 0.0;
	double PhaseStartedAt = 0.0;
	double PendingScreenshotStartedAt = 0.0;
	float LastDeltaSeconds = 0.0f;
	float BuildingScrollOffsetBeforeReturn = -1.0f;
	TWeakObjectPtr<URokTrainArmyMeshWidget> TrainMesh;
	uint64 TrainMeshGeneration = 0;
	int32 TrainMeshArmsId = 0;
	TMap<FName, FVector2D> TrainFixedPositions;
	TMap<FName, FVector2D> TrainFixedSizes;
	uint64 AuditFrame = 0;
	int32 PhaseCaptureIndex = 0;
	int32 FinishFrames = 0;
	bool bScreenshotPending = false;
	bool bAdvancePhaseAfterCapture = false;
	bool bFinished = false;

	bool FindRuntimeWidgets();
	bool ValidateTrainFixedPresentation(bool bDetail);
	void EnterPhase(EPhase NextPhase);
	bool Dispatch(UUserWidget* Owner, FName ButtonName, FName EventName);
	bool Capture(const TCHAR* Name, float TargetSeconds, EPhase NextPhase, bool bEndPhase);
	void OnScreenshotProcessed();
	TSharedRef<FJsonObject> ReadRenderState() const;
	bool WriteManifest(bool bComplete);
	void Fail(const FString& Reason);
	void Finish();
};
