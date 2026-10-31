#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RokStrategyCameraPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USceneComponent;
class UWorld;

UCLASS()
class ROK_API ARokStrategyCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARokStrategyCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Rok Camera")
	float GetCurrentCityDxf() const;

	static float CalculateCameraDistanceForDxf(
		float Dxf,
		float VerticalFov,
		float UnitsPerUnityUnit);

	static bool ShouldReturnToWorldMap(
		float CurrentDxf,
		float RequestedDxf,
		float ReturnThresholdDxf);

	// City framing mirrors Unity CameraParamLod.csv (see
	// FRokCityViewMath::MakeDefaultCameraKeys): city_default dxf 150 is the
	// opening zoom, min dxf 82.4551 is the closest wheel zoom, and city_bound
	// dxf 250 caps the city before the world-map return takes over.
	static float GetDefaultCityDxf();
	static float GetCityMinDxf();
	static float GetCityMaxDxf();
	static float ClampCityDxf(float Dxf);

	// One mouse-wheel notch multiplies the city dxf by the Unity wheel factor
	// (Skyunion InputManager scrollRate = 1/(mouseWheel + 1), sensitivity 0.1,
	// i.e. 10/9 zooming out and 10/11 zooming in). The step is a pure dxf
	// multiplication, so the target is frame-rate independent.
	static float CalculateWheelZoomDxf(float CurrentDxf, float ZoomDirection);

	// Unity fills the window at a constant vertical FOV. The camera never
	// constrains the aspect ratio and pins the projection axis to the vertical
	// FOV; its FieldOfView stays horizontal, converted at ReferenceAspectRatio.
	// The engine converts it back to the same vertical FOV at any window aspect
	// (FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle).
	static void ApplyCityCameraProjection(
		UCameraComponent* Camera,
		float VerticalFovDegrees,
		float ReferenceAspectRatio);

	UFUNCTION(BlueprintCallable, Category="Rok Camera")
	bool RequestReturnToWorldMap();

	// Unity btn_city: snap back to the city's default framing (city_default dxf
	// and the home ground target) once the camera is outside the city band.
	UFUNCTION(BlueprintCallable, Category="Rok Camera")
	void FrameCityCenter();

	UFUNCTION(BlueprintCallable, Category="Rok Camera")
	bool RequestReturnToWorldMapAtDisplayCoordinate(
		FVector2D TargetDisplayCoordinate);

	// Negative values zoom in and positive values zoom out. The physical
	// mouse-wheel path and unattended runtime verification share this method.
	bool ApplyZoomInput(float ZoomDirection, float DeltaSeconds);
	void SetCityPlacementDragInputSuspended(bool bSuspended);
	bool IsCityPlacementDragInputSuspended() const { return bCityPlacementDragInputSuspended; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Rok Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Rok Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	FVector DefaultCameraTarget = FVector::ZeroVector;

	// Ground point the camera currently looks at. Panning updates it so a zoom
	// keeps the framing instead of snapping back to the home target.
	FVector CurrentCameraTarget = FVector::ZeroVector;

	// CameraParamLod city_default: dxf 150 at fov 7.5 -> 150 / 7.5 * 500. The
	// interactive zoom range is derived from the camera keys via ClampCityDxf,
	// not from a second pair of distance fields, so there is one source of truth.
	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultCameraDistance = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultCameraYaw = 112.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultCameraPitch = -45.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultVerticalFov = 7.5f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultAspectRatio = 16.0f / 10.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultOrthoWidth = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MinOrthoWidth = 1500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MaxOrthoWidth = 5200.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MoveSpeed = 1450.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float FastMoveMultiplier = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float EdgeScrollSpeed = 1250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float EdgeScrollMargin = 28.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	bool bEnableEdgeScroll = false;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	bool bContinueEdgeScrollOutsideViewport = true;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MinCameraHeight = 2600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MaxCameraHeight = 4200.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MapExtent = 3600.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float UeUnitsPerUnityUnit = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera|Map Flow")
	bool bReturnToWorldMapOnZoomOut = true;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera|Map Flow", meta=(ClampMin="1.0"))
	float WorldMapReturnThresholdDxf = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera|Map Flow", meta=(ClampMin="1.0"))
	float WorldMapEntryDxf = 288.316f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera|Map Flow")
	TSoftObjectPtr<UWorld> WorldMapLevel;

	float CurrentCameraDistance = 10000.0f;
	float CurrentVerticalFov = 7.5f;
	bool bWorldMapTravelRequested = false;
	double AutomationWorldReturnNextStepSeconds = 0.0;
	int32 AutomationWorldReturnStepCount = 0;

	FVector2D LastEdgeScrollDirection = FVector2D::ZeroVector;
	bool bCityPlacementDragInputSuspended = false;

	void UpdateKeyboardPan(float DeltaSeconds);
	void UpdateEdgePan(float DeltaSeconds);
	void UpdateMouseWheelZoom(float DeltaSeconds);
	void DriveAutomationWorldMapReturn();
	void ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds);
	void ApplyCityEntryOptions();
	void ApplyCommandLineCameraDistanceOverride();
	void ApplyCameraDefaults();
	bool RequestReturnToWorldMapInternal(
		const TOptional<FVector2D>& TargetDisplayCoordinate);
};
