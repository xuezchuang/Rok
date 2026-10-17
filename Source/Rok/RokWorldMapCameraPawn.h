#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RokWorldMapTypes.h"
#include "RokWorldMapCameraPawn.generated.h"

class ARokWorldMapConfigActor;
class ARokWorldMapCityActor;
class AActor;
class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROK_API ARokWorldMapCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARokWorldMapCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	FVector2D GetViewCenterUnity() const { return CameraState.ViewCenterUnity; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	FVector2D GetViewCenterDisplay() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	float GetCurrentDxf() const { return CameraState.CurrentDxf; }
	float GetWorldUnitsPerUnityUnit() const { return GetProjection().GetSafeUeScale(); }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map")
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map")
	void SetViewCenterUnity(FVector2D NewViewCenterUnity);

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map")
	void SetCurrentDxf(float NewDxf);

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map")
	void FocusDisplayCoordinate(FVector2D DisplayCoordinate);

	// Negative values zoom in and positive values zoom out. The physical
	// mouse-wheel path and unattended runtime verification share this method.
	bool ApplyZoomInput(float ZoomDirection, bool bPreserveCursor);

	static float EvaluateUnityTreeScale(float Dxf);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(Transient)
	TObjectPtr<ARokWorldMapConfigActor> RuntimeConfig;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map")
	FRokWorldMapCameraState CameraState;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Fallback")
	FRokWorldMapProjection FallbackProjection;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Fallback")
	FVector2D FallbackFocusUnity = FVector2D(6876.0f, 942.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Fallback")
	FVector FallbackUnityCameraForward = FVector(0.0f, -0.7f, 0.7f);

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Input")
	float PanSpeedUnityUnitsPerSecond = 180.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Input")
	float FastPanMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Input")
	float ZoomFactorPerNotch = 1.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Input")
	bool bEnableDragPan = true;

	UPROPERTY(EditDefaultsOnly, Category = "Rok|World Map|Camera")
	float DefaultAspectRatio = 16.0f / 10.0f;

	TArray<FRokCityViewCameraKey> ActiveCameraKeys;
	struct FTreeVisual
	{
		TWeakObjectPtr<AActor> Actor;
		FVector ActorScaleAtTactical = FVector::OneVector;
		// Exact source-center mesh route; never scale its placement transform.
		TWeakObjectPtr<UStaticMeshComponent> CenterScaleComponent;
	};
	TArray<FTreeVisual> TreeVisuals;
	TArray<TWeakObjectPtr<ARokWorldMapCityActor>> CityVisuals;
	float LastAppliedTreeScaleRatio = -1.0f;
	float LastAppliedCityDxf = -1.0f;
	FName MinimumCameraKeyId = FRokWorldMapMath::MinimumCameraKey();
	FName MaximumCameraKeyId = FRokWorldMapMath::MaximumCameraKey();
	// Wheel-zoom floor for the world view. Unity's CameraParamLod tactical
	// states start at "FTE_Tactics" (dxf 1110); dxf 500-1100 is the city-exit
	// transit band WorldCameraImpl's dynamic min-dxf only passes through.
	// Inspecting a formation at 1110+ evaluates m_unit_scale_curve at
	// ~2.1+, which renders the dense 15-unit mass the Unity reference
	// screenshots show; allowing the wheel to settle at 500-900 instead
	// surfaces the sparse transient state (curve ~1.1 with clone expansion)
	// that the real game never presents as a resting view.
	FName UserZoomMinimumCameraKeyId = FName(TEXT("FTE_Tactics"));
	FVector2D LastDragScreenPosition = FVector2D::ZeroVector;
	bool bDragging = false;
	bool bEnteringPlayerCity = false;
	float PlayerCityEntryElapsedSeconds = 0.0f;
	float PlayerCityEntryDurationSeconds = 0.5f;
	float PlayerCityEntryStartDxf = 1509.0f;
	float PlayerCityEntryTargetDxf = 150.0f;
	FVector2D PlayerCityEntryStartCenterUnity = FVector2D::ZeroVector;
	FVector2D PlayerCityEntryTargetCenterUnity = FVector2D::ZeroVector;

	const FRokWorldMapProjection& GetProjection() const;
	FVector GetUnityCameraForward() const;
	void ResolveConfiguration();
	void ApplyWorldEntryOptions();
	void ResolveTreeVisuals();
	void ResolveCityVisuals();
	void ApplyCameraState();
	void ApplyUnityTreeScale();
	void ApplyUnityCityLod();
	void UpdateKeyboardPan(float DeltaSeconds);
	void UpdateDragPan();
	void UpdateMouseWheelZoom();
	bool TryBeginPlayerCityEntry(float RequestedDxf);
	bool UpdatePlayerCityEntry(float DeltaSeconds);
	ARokWorldMapCityActor* FindPlayerCity() const;
	bool IsPlayerCityInViewport(
		const ARokWorldMapCityActor* PlayerCity) const;
	void PanByScreenDirection(const FVector2D& ScreenDirection, float DeltaSeconds, float SpeedScale);
	void PanByUeWorldDelta(const FVector2D& UeWorldDelta);
	bool TryIntersectGroundPlaneAtScreen(const FVector2D& ScreenPosition, FVector& OutGroundPoint) const;
};
