#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokWorldMapTypes.h"
#include "RokWorldMapConfigActor.generated.h"

class USceneComponent;
class UWorld;

UCLASS(Blueprintable)
class ROK_API ARokWorldMapConfigActor : public AActor
{
	GENERATED_BODY()

public:
	ARokWorldMapConfigActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	FRokWorldMapProjection Projection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	FVector2D DefaultFocusUnity = FVector2D(6876.0f, 942.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	FVector UnityCameraForward = FVector(0.0f, -0.7f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Camera")
	TArray<FRokCityViewCameraKey> CameraKeys;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Camera")
	FName MinimumCameraKeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Camera")
	FName MaximumCameraKeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Camera")
	FName DefaultCameraKeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|City Entry")
	bool bEnterPlayerCityOnZoom = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|City Entry")
	FName CityEntryThresholdCameraKeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|City Entry")
	FName CityEntryTargetCameraKeyId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|City Entry", meta = (ClampMin = "0.05"))
	float CityEntryTransitionSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|City Entry")
	TSoftObjectPtr<UWorld> PlayerCityLevel;

	float ClampDxf(float Dxf) const;
	float GetDefaultDxf() const;
	float GetCityEntryThresholdDxf() const;
	float GetCityEntryTargetDxf() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map")
	TObjectPtr<USceneComponent> SceneRoot;

	void NormalizeConfiguration();
};
