#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokCityConstructionGrid.h"
#include "RokCityPlacementPreviewActor.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

/** Temporary, nonselectable visual for a source-grid placement candidate. */
UCLASS(NotPlaceable)
class ROK_API ARokCityPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ARokCityPlacementPreviewActor();

	bool InitializePreview(int32 TypeId, const FString& ModelId, FString& OutError);
	void SetCandidate(const FTransform& CityContainer, const FRokCityConstructionFootprint& Footprint);
	void UpdateFootprintFeedback(bool bDragging, bool bGridValid);
	void SetFootprintLightingCompensation(const FLinearColor& Compensation);

	UStaticMeshComponent* GetBuildingMeshComponent() const { return BuildingMesh.Get(); }
	UStaticMeshComponent* GetBottomMeshComponent() const { return BottomMesh.Get(); }

private:
	UPROPERTY(VisibleAnywhere, Category="Rok|Placement")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Rok|Placement")
	TObjectPtr<USceneComponent> CandidateRoot;

	UPROPERTY(VisibleAnywhere, Category="Rok|Placement")
	TObjectPtr<UStaticMeshComponent> BuildingMesh;

	UPROPERTY(VisibleAnywhere, Category="Rok|Placement")
	TObjectPtr<UStaticMeshComponent> BottomMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BottomMaterial;

	FLinearColor FootprintTint = FLinearColor::Green;
	FLinearColor FootprintCompensation = FLinearColor::White;
	bool bInitialized = false;
};
