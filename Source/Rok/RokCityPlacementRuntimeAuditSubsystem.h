#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokCityPlacementRuntimeAuditSubsystem.generated.h"

class AActor;
class ARokCitySpriteActor;
class ARokCityPlacementPreviewActor;
class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;
class UWidget;

/** Opt-in, fail-closed GameApp pointer-path audit of temporary city placement. */
UCLASS()
class ROK_API URokCityPlacementRuntimeAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<ARokPlayerController> Controller;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokBuildingPanelWidget> Panel;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
	TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
	TWeakObjectPtr<AActor> Hall;
	TMap<int32, TWeakObjectPtr<AActor>> InitialActors;
	TSet<TWeakObjectPtr<ARokCitySpriteActor>> BaselineSourceActors;
	TSet<TWeakObjectPtr<AActor>> BaselineUnityInitActors;
	TMap<int32, TArray<uint8>> InitialStates;
	TArray<uint8> ProfileBaseline;
	TArray<uint8> ResourceBaseline;
	TArray<uint8> TaskBaseline;
	FRokCityPlacementWorldFrame WorldFrame;
	FRokCityPlacementDragState PredictedDrag;
	FIntPoint InitialGrid = FIntPoint::ZeroValue;
	FIntPoint ValidGrid = FIntPoint::ZeroValue;
	FIntPoint OccupiedGrid = FIntPoint::ZeroValue;
	FIntPoint OutboundGrid = FIntPoint::ZeroValue;
	FVector2D LastAbsolutePointer = FVector2D::ZeroVector;
	FVector2D OriginalAbsolutePointer = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableClickTarget;
	FVector2D StableClickCenter = FVector2D::ZeroVector;
	FVector2D StableClickSize = FVector2D::ZeroVector;
	int32 StableClickFrames = 0;
	FString Directory;
	FString PendingPath;
	TSharedPtr<FJsonObject> Pending;
	TWeakObjectPtr<ARokCityPlacementPreviewActor> PreparedGhost;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Baseline;
	TSharedPtr<FJsonObject> WindowPreparation;
	TArray<TSharedPtr<FJsonValue>> Actions;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	FDelegateHandle ScreenshotHandle;
	FDelegateHandle CompletionHandle;
	double Elapsed = 0.0;
	double StepStarted = 0.0;
	double HallClickedWall = 0.0;
	double ShotStarted = 0.0;
	double GhostReadyObservedWorldSeconds = 0.0;
	double LastObservedBuildingRenderTime = 0.0;
	double LastObservedBottomRenderTime = 0.0;
	int32 Step = 0;
	int32 StepAfterCapture = 0;
	int32 CompletionCount = 0;
	int32 BaselineUpgradeQueues = 0;
	int32 BaselineLocalQueues = 0;
	int32 BaselineOwnedQueues = 0;
	int32 RenderReadyFrames = 0;
	int32 PostScreenshotReadyFrames = 0;
	uint64 LastRenderReadyFrame = 0;
	uint64 ScreenshotProcessedFrame = 0;
	uint64 WorldPressPrimaryCountBefore = 0;
	uint64 WorldPressDragBeginCountBefore = 0;
	uint64 WorldReleaseCountBefore = 0;
	uint64 EscapeSentFrame = 0;
	uint64 EscapeCancelCountBefore = 0;
	bool bBaselineReady = false;
	bool bPointerDown = false;
	bool bScreenshotRequested = false;
	bool bScreenshotProcessed = false;
	bool bFinished = false;

	bool Begin();
	bool PrepareOwnGameWindow();
	bool Identity();
	bool Expect(bool Condition, const TCHAR* Message);
	bool FreezePlacementBaseline();
	bool Unchanged();
	bool CheckPreview(int32 TypeId, bool bExpectedActive, bool bExpectedDragging,
		bool bExpectedValid);
	bool ChooseTargets();
	bool ProjectGrid(const FIntPoint& Grid, FVector2D& OutViewport, FVector2D& OutAbsolute) const;
	bool ProjectHeldGridInterior(const FIntPoint& Grid, FVector2D& OutViewport,
		FVector2D& OutAbsolute, TSharedPtr<FJsonObject>& OutEvidence) const;
	bool ProjectSubgrid(FVector2D& OutViewport, FVector2D& OutAbsolute) const;
	bool MovePointer(const FVector2D& Viewport, const FVector2D& Absolute, bool bHeld,
		const TCHAR* Label);
	bool WorldButton(bool bDown, const TCHAR* Label);
	bool SlateClick(UWidget* Target, const TCHAR* Label);
	bool ReadyForClick(UWidget* Target, bool bHostAnimating);
	bool SlateUiOriginNoDrag(UWidget* Target);
	bool PressEscape();
	void OnUpgradeCompleted(const struct FRokLocalBuildingUpgradeCompleted& Completed);
	void Enter(int32 NextStep);
	void Capture(const TCHAR* Label, int32 NextStep);
	bool PrepareReferencedPreviewAssets(ARokCityPlacementPreviewActor* Ghost, FString& Error);
	bool IsRenderReadyForCapture(TSharedRef<FJsonObject> Evidence) const;
	void AdvancePendingCapture();
	void ScreenshotProcessed();
	TSharedRef<FJsonObject> State() const;
	bool Save(bool Complete);
	void Finish(const FString& Error = FString());
};
