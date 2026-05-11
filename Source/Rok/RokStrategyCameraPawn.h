#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RokStrategyCameraPawn.generated.h"

class UCameraComponent;
class UFloatingPawnMovement;
class USceneComponent;

UCLASS()
class ROK_API ARokStrategyCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARokStrategyCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category="Rok Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Rok Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	FVector DefaultCameraLocation = FVector(-2200.0f, -2200.0f, 4450.0f);

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	FRotator DefaultCameraRotation = FRotator(-55.0f, 45.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float DefaultOrthoWidth = 2950.0f;

	UPROPERTY(EditDefaultsOnly, Category="Rok Camera")
	float MinOrthoWidth = 1800.0f;

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

	FVector2D LastEdgeScrollDirection = FVector2D::ZeroVector;

	void UpdateKeyboardPan(float DeltaSeconds);
	void UpdateEdgePan(float DeltaSeconds);
	void UpdateMouseWheelZoom(float DeltaSeconds);
	void ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds);
};
