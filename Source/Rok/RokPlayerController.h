#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokPlayerController.generated.h"

class URokMainHudWidget;
class ARokBuildingSelectionIndicator;
class ARokCitySpriteActor;
class ARokCityPlacementPreviewActor;
class ARokStrategyCameraPawn;

/** Read-only, actual controller state for real pointer-path runtime audits. */
struct ROK_API FRokCityPlacementRuntimeSnapshot
{
	bool bActive = false;
	bool bDragging = false;
	bool bGridValid = false;
	bool bCanCommit = false;
	bool bCameraInputSuspended = false;
	bool bAwaitPointerRelease = false;
	uint64 SuppressPrimaryClickThroughFrame = 0;
	uint64 PrimaryPressEventCount = 0;
	uint64 LastPrimaryPressFrame = 0;
	FName LastPrimaryPressOutcome = NAME_None;
	uint64 PlacementDragBeginCount = 0;
	uint64 LastPlacementDragBeginFrame = 0;
	uint64 PlacementPointerReleaseCount = 0;
	uint64 LastPlacementPointerReleaseFrame = 0;
	uint64 PlacementEscapeCancelCount = 0;
	uint64 LastPlacementEscapeCancelFrame = 0;
	uint64 ConfirmClickCount = 0;
	uint64 LastConfirmClickFrame = 0;
	FName LastConfirmOutcome = NAME_None;
	FString LastConfirmFailure;
	uint64 LastConfirmedLocalConstructionId = 0;
	TWeakObjectPtr<AActor> LastConfirmedActor;
	uint64 LastPlacementPressFrame = 0;
	FName LastPlacementPressFailure = NAME_None;
	FString LastPlacementPressError;
	FVector2D LastPlacementPressMouse = FVector2D::ZeroVector;
	FVector LastPlacementPressGround = FVector::ZeroVector;
	FVector2D LastPlacementPressLocalHit = FVector2D::ZeroVector;
	FVector2D LastPlacementPressLocalCenter = FVector2D::ZeroVector;
	int32 TypeId = 0;
	FIntPoint Grid = FIntPoint::ZeroValue;
	FIntPoint Footprint = FIntPoint::ZeroValue;
	TWeakObjectPtr<AActor> PreviewActor;
	TWeakObjectPtr<AActor> PriorSelection;
	TWeakObjectPtr<AActor> CurrentSelection;
};

UCLASS()
class ROK_API ARokPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARokPlayerController();
	FRokCityPlacementRuntimeSnapshot GetCityPlacementPreviewSnapshot() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY()
	TObjectPtr<URokMainHudWidget> MainHudWidget;

	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedBuildingActor;

	UPROPERTY()
	TObjectPtr<ARokBuildingSelectionIndicator> BuildingSelectionIndicator;

	FVector SelectedBuildingOriginalScale = FVector::OneVector;
	bool bHasSelectedBuildingOriginalScale = false;
	bool bStartedCityBgmForMap = false;
	FTimerHandle AutoScreenshotTimerHandle;
	FTimerHandle AutoScreenshotQuitTimerHandle;
	FDelegateHandle BuildCityPlacementDelegateHandle;
	FDelegateHandle PlacementCancelDelegateHandle;
	FDelegateHandle PlacementConfirmDelegateHandle;
	TWeakObjectPtr<ARokCityPlacementPreviewActor> PlacementPreviewActor;
	TWeakObjectPtr<AActor> SelectionBeforePlacement;
	TWeakObjectPtr<ARokStrategyCameraPawn> PlacementLockedCamera;
	FRokCityPlacementWorldFrame PlacementWorldFrame;
	FRokCityPlacementDragState PlacementDragState;
	FIntPoint PlacementGrid = FIntPoint::ZeroValue;
	FIntPoint PlacementFootprint = FIntPoint::ZeroValue;
	int32 PlacementTypeId = 0;
	int32 PlacementVisualCivilization = 0;
	int32 PlacementAge = 0;
	bool bPlacementActive = false;
	bool bPlacementGridValid = false;
	bool bPlacementCanCommit = false;
	bool bPlacementConfirmInProgress = false;
	double LastPlacementReadinessCheckSeconds = -1.0;
	bool bPlacementPriorCameraSuspended = false;
	bool bPlacementAwaitPointerRelease = false;
	bool bPlacementFeedbackOnly = false;
	FString PlacementSubmitFailure;
	FString LastPlacementPreviewReason;
	uint64 SuppressPrimaryClickThroughFrame = 0;
	uint64 PrimaryPressEventCount = 0;
	uint64 LastPrimaryPressFrame = 0;
	FName LastPrimaryPressOutcome = NAME_None;
	uint64 PlacementDragBeginCount = 0;
	uint64 LastPlacementDragBeginFrame = 0;
	uint64 PlacementPointerReleaseCount = 0;
	uint64 LastPlacementPointerReleaseFrame = 0;
	uint64 PlacementEscapeCancelCount = 0;
	uint64 LastPlacementEscapeCancelFrame = 0;
	uint64 ConfirmClickCount = 0;
	uint64 LastConfirmClickFrame = 0;
	FName LastConfirmOutcome = NAME_None;
	FString LastConfirmFailure;
	uint64 LastConfirmedLocalConstructionId = 0;
	TWeakObjectPtr<AActor> LastConfirmedActor;
	uint64 LastPlacementPressFrame = 0;
	FName LastPlacementPressFailure = NAME_None;
	FString LastPlacementPressError;
	FVector2D LastPlacementPressMouse = FVector2D::ZeroVector;
	FVector LastPlacementPressGround = FVector::ZeroVector;
	FVector2D LastPlacementPressLocalHit = FVector2D::ZeroVector;
	FVector2D LastPlacementPressLocalCenter = FVector2D::ZeroVector;

	void ScheduleAutoScreenshot();
	void AutoSelectBuildingFromCommandLine();
	void StartSelectedBuildingUpgradeFromCommandLine();
	void TakeAutoScreenshot();
	void QuitAfterAutoScreenshot();
	void HandlePrimaryClick();
	void HandleBuildCityPlacementRequested(int32 UnityBuildingTypeId);
	void HandlePlacementPointerPressed();
	void HandlePlacementPointerReleased();
	void HandlePlacementEscape();
	void HandlePlacementCancelRequested();
	void HandlePlacementConfirmRequested();
	void UpdatePlacementDrag();
	void RefreshPlacementCommitReadiness();
	void CancelPlacementPreview(bool bRestoreSelection = true);
	void SetPlacementCameraInputSuspended(bool bSuspended);
	void PresentPlacementFailure(const FString& Reason);
	bool IsPointerOverRokUi() const;
	AActor* FindSelectableBuildingUnderCursor() const;
	ARokCitySpriteActor* FindCityBuildingByUnityFootprint(const FVector2D& ScreenPosition) const;
	void DrawBuildingGridDebug() const;
	void DrawBuildingGridDebugForActor(
		const ARokCitySpriteActor& BuildingActor,
		const FColor& Color,
		float Thickness) const;
	void SelectBuilding(AActor* Actor);
	void ClearSelection();
	void ApplySelectionHighlight(AActor* Actor, bool bHighlighted);
	bool IsSelectableBuilding(AActor* Actor) const;
	FString GetActorDisplayString(AActor* Actor) const;
};
