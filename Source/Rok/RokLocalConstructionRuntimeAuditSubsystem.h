#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokLocalConstructionRuntimeAuditSubsystem.generated.h"

class AActor;
class ARokCitySpriteActor;
class ARokCityPlacementPreviewActor;
class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;
class UWidget;

/** Opt-in, fail-closed GameApp ordinary-UI audit of local Farm/Wood construction. */
UCLASS()
class ROK_API URokLocalConstructionRuntimeAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokServerSubsystem> Server;
	TWeakObjectPtr<AActor> Hall;
	TMap<int32, TWeakObjectPtr<AActor>> InitialActors;
	TMap<uint64, TWeakObjectPtr<ARokCitySpriteActor>> ConstructedActors;
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
	FIntPoint FarmGrid = FIntPoint::ZeroValue;
	FIntPoint WoodGrid = FIntPoint::ZeroValue;
	uint64 FarmId = 0;
	uint64 WoodId = 0;
	uint64 ConfirmCountBefore = 0;
	uint64 SelectionPressBefore = 0;
	double ZoomBeforeDxf = 0.0;
	double ZoomCompletedDxf = 0.0;
	int32 ZoomWheelEventCount = 0;
	uint64 ZoomWheelSentFrame = 0;
	TSharedPtr<FJsonObject> PendingZoomAction;
	TSharedPtr<FJsonObject> InvalidDisabledConfirmAction;
	TSharedPtr<FJsonObject> FirstConfirmAction;
	TSharedPtr<FJsonObject> RepeatConfirmAction;
	double FarmClickedWall = 0.0;
	double WoodClickedWall = 0.0;
	double FarmCompletionWall = 0.0;
	double WoodCompletionWall = 0.0;
	int32 LocalCompletionCount = 0;
	FVector2D LastAbsolutePointer = FVector2D::ZeroVector;
	FVector2D LastConfirmAbsolutePointer = FVector2D::ZeroVector;
	FVector2D OriginalAbsolutePointer = FVector2D::ZeroVector;
	TWeakObjectPtr<UWidget> StableClickTarget;
	FVector2D StableClickCenter = FVector2D::ZeroVector;
	FVector2D StableClickSize = FVector2D::ZeroVector;
	int32 StableClickFrames = 0;
	FString Directory;
	FString PendingPath;
	FString GammaCensusPath;
	TSharedPtr<FJsonObject> Pending;
	TWeakObjectPtr<ARokCityPlacementPreviewActor> PreparedGhost;
	TWeakObjectPtr<ARokCitySpriteActor> PreparedConstructionActor;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Baseline;
	TSharedPtr<FJsonObject> FarmCompletionProduction;
	TSharedPtr<FJsonObject> WoodCompletionProduction;
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
	double ConstructionReadyObservedWorldSeconds = 0.0;
	double LastObservedConstructionRenderTime = 0.0;
	double SelectionArrowReadyObservedWorldSeconds = 0.0;
	double LastObservedSelectionArrowRenderTime = 0.0;
	int32 Step = 0;
	int32 StepAfterCapture = 0;
	int32 CompletionCount = 0;
	int32 BaselineUpgradeQueues = 0;
	int32 BaselineLocalQueues = 0;
	int32 BaselineOwnedQueues = 0;
	int32 RenderReadyFrames = 0;
	int32 PostScreenshotReadyFrames = 0;
	uint64 LastRenderReadyFrame = 0;
	uint64 SelectionPanelLastObservedFrame = 0;
	uint64 SelectionPanelFirstStableFrame = 0;
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
	bool bSelectionArrowAssetsPrepared = false;
	int32 SelectionPanelStableFrames = 0;
	FVector2D SelectionMenuCenter = FVector2D::ZeroVector;
	FVector2D SelectionMenuSize = FVector2D::ZeroVector;
	FVector2D SelectionTitleCenter = FVector2D::ZeroVector;
	FVector2D SelectionTitleSize = FVector2D::ZeroVector;
	FVector2D PlacementMenuLastCenter = FVector2D::ZeroVector;
	FVector2D PlacementMenuLastSize = FVector2D::ZeroVector;
	int32 PlacementMenuStableFrames = 0;
	bool bPlacementMenuPointerCleared = false;
	double PlacementMenuPointerClearWall = 0.0;
	bool bFinished = false;
	bool bGammaCensusInvoked = false;
	bool bGammaCensusReturned = false;

	bool Begin();
	bool PrepareOwnGameWindow();
	bool Identity();
	bool Expect(bool Condition, const TCHAR* Message);
	bool FreezePlacementBaseline();
	bool Unchanged();
	bool CheckPreview(int32 TypeId, bool bExpectedActive, bool bExpectedDragging,
		bool bExpectedValid);
	bool CheckConstruction(uint64 Id, int32 TypeId, bool bCompleted, int32 Food, int32 Wood,
		int32 ActiveCount);
	bool CheckProduction(uint64 Id, const TCHAR* Label, TSharedPtr<FJsonObject>& OutSample);
	bool CheckSelectionFeedback(ARokCitySpriteActor* Actor, const TCHAR* Label,
		bool bExpectArrows);
	bool SlateZoomInAtWorldPointer();
	bool ProjectConstructedActor(ARokCitySpriteActor* Actor, FVector2D& OutViewport,
		FVector2D& OutAbsolute) const;
	bool CheckConstructionBoundary(int32 ExpectedNewActors, int32 ExpectedActiveCount);
	bool ChooseTargets();
	bool ProjectGrid(const FIntPoint& Grid, FVector2D& OutViewport, FVector2D& OutAbsolute) const;
	bool ProjectHeldGridInterior(const FIntPoint& Grid, FVector2D& OutViewport,
		FVector2D& OutAbsolute, TSharedPtr<FJsonObject>& OutEvidence) const;
	bool ProjectSubgrid(FVector2D& OutViewport, FVector2D& OutAbsolute) const;
	bool MovePointer(const FVector2D& Viewport, const FVector2D& Absolute, bool bHeld,
		const TCHAR* Label);
	bool WorldButton(bool bDown, const TCHAR* Label);
	bool SlateClick(UWidget* Target, const TCHAR* Label);
	TSharedRef<FJsonObject> PlacementMenuEvidence() const;
	bool ObservePlacementMenuReadiness(TSharedRef<FJsonObject> Evidence);
	bool DisabledConfirmClick();
	bool RepeatConfirmPointer(const TCHAR* Label);
	bool ObserveFirstConfirmNextFrame(const TCHAR* ExpectedAction);
	bool ObserveRepeatConfirmNextFrame(const TCHAR* ExpectedAction);
	bool ObserveSelectedPanelCloseEnd(double ConfirmClickedWall);
	bool ReadyForClick(UWidget* Target, bool bHostAnimating);
	bool SlateUiOriginNoDrag(UWidget* Target);
	bool PressEscape();
	void OnUpgradeCompleted(const struct FRokLocalBuildingUpgradeCompleted& Completed);
	void Enter(int32 NextStep);
	void Capture(const TCHAR* Label, int32 NextStep);
	bool PrepareReferencedPreviewAssets(ARokCityPlacementPreviewActor* Ghost, FString& Error);
	ARokCitySpriteActor* GetCaptureConstructionActor() const;
	bool PrepareReferencedConstructionAssets(ARokCitySpriteActor* Actor, FString& Error);
	bool PrepareReferencedSelectionArrowAssets(FString& Error);
	bool IsRenderReadyForCapture(TSharedRef<FJsonObject> Evidence) const;
	bool ObserveSelectedPanelReadiness(TSharedRef<FJsonObject> Evidence);
	void AdvancePendingCapture();
	void ScreenshotProcessed();
	TSharedRef<FJsonObject> State() const;
	bool Save(bool Complete);
	void Finish(const FString& Error = FString());
};
