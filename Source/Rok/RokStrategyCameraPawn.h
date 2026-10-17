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

	UFUNCTION(BlueprintCallable, Category="Rok Camera")
	bool RequestReturnToWorldMap();

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

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultCameraDistance = 17000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MinCameraDistance = 3500.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MaxCameraDistance = 32000.0f;

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
	float ZoomSpeed = 1600.0f;

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

	float CurrentCameraDistance = 17000.0f;
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
	void ApplyCameraDefaults();
	bool RequestReturnToWorldMapInternal(
		const TOptional<FVector2D>& TargetDisplayCoordinate);
};
