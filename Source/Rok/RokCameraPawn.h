#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RokCameraPawn.generated.h"

class UCameraComponent;
class USceneComponent;

UCLASS()
class ROK_API ARokCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ARokCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Rok|Camera")
	UCameraComponent* GetCameraComponent() const { return CameraComponent; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rok Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Rok Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	FVector DefaultCameraLocation = FVector(-1650.0f, -1650.0f, 2850.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	FRotator DefaultCameraRotation = FRotator(-55.0f, 45.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float DefaultOrthoWidth = 1800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float MinOrthoWidth = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float MaxOrthoWidth = 3600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float MoveSpeed = 950.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float FastMoveMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float ZoomStep = 130.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Rok Camera")
	float MapExtent = 2400.0f;

	void ApplyFixedCameraSettings();
	void UpdateKeyboardPan(float DeltaSeconds);
	void UpdateMouseWheelZoom();
	void ApplyPlanarMovement(const FVector2D& Movement, float DeltaSeconds);
};
