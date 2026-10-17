#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokWorldMapCityActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UBillboardComponent;
class UBoxComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class ROK_API ARokWorldMapCityActor : public AActor
{
	GENERATED_BODY()

public:
	ARokWorldMapCityActor();

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map|City")
	void ConfigureSourceVisual(
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		FRotator VisualRotation,
		FVector InBaseVisualScale,
		FVector InSelectionAnchorOffset,
		FVector InBaseVisualCenterOffset,
		float InitialDxf,
		float InAgeLodScaleCap,
		int32 SortPriority);

	void ApplyCameraDxf(float Dxf);

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	FVector GetVisualCenterWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	FVector GetVisualBoundsExtent() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	FVector GetSelectionAnchorWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	float GetCurrentLodScale() const { return CurrentLodScale; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	float GetAgeLodScaleCap() const { return AgeLodScaleCap; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	UStaticMeshComponent* GetVisualMeshComponent() const
	{
		return VisualMeshComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|City")
	UBoxComponent* GetSelectionCollisionComponent() const
	{
		return SelectionCollisionComponent;
	}

	static float EvaluateUnityCityLodScale(float Dxf, float LodScaleCap);
	static float EvaluateOwnCityOverviewScale(float Dxf);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PulsePlanes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PulseMaterials;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PulseTexture;
	bool bPulseVisible = false;
	double PulseSeconds = 0.0;
	float PulseDiameter = 6000.0f;
	void EnsurePulseLoaded();
	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|City")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|City")
	TObjectPtr<UStaticMeshComponent> VisualMeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|City")
	TObjectPtr<UBoxComponent> SelectionCollisionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|City")
	TObjectPtr<UBillboardComponent> FarVisualComponent;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FarSpriteComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|City")
	TObjectPtr<UBoxComponent> FarSelectionCollisionComponent;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FarTexture;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|City")
	FVector BaseVisualScale = FVector::OneVector;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|City")
	FVector SelectionAnchorOffset = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|City")
	FVector BaseVisualCenterOffset = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|City")
	float CurrentLodScale = 1.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|City")
	float AgeLodScaleCap = 7.8f;

	void EnsureFarVisualLoaded();
	void ApplyUnityCityLodScale(float LodScale);
};
