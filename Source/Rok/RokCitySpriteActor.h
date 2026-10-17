#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokCityViewTypes.h"
#include "RokCitySpriteActor.generated.h"

class UMaterialInterface;
class UMaterialBillboardComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class ROK_API ARokCitySpriteActor : public AActor
{
	GENERATED_BODY()

public:
	ARokCitySpriteActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigureSprite(UMaterialInterface* Material, float Width, float Height, int32 SortPriority);

	// Source tight SpriteMesh geometry and its pivot-preserving child transform.
	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigureSourceMesh(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& RelativeTransform, int32 SortPriority);

	UStaticMeshComponent* GetSpriteMeshComponent() const { return SpriteMeshComponent.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigurePlacement(FIntPoint InFootprintSize, FIntPoint InGameplayTile, FVector InVisualAnchorOffset);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigureUnityCameraLod(FVector InBaseVisualCenterOffset, float InitialDxf);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigureViewLayer(ERokCityViewLayer InViewLayer, int32 InSortPriority);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void SetSelectionCollisionEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void SetSelected(bool bSelected);

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FVector GetVisualAnchorWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FVector GetVisualCenterWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FVector GetVisualBoundsExtent() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FIntPoint GetFootprintSize() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FIntPoint GetGameplayTile() const { return GameplayTile; }

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	FVector GetUnityBaseVisualCenterOffset() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	float GetUnityLodScale() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	int32 GetUnityBuildingType() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	bool CanShowMoveArrows() const;

	UFUNCTION(BlueprintPure, Category = "Rok|City")
	bool IsDecoration() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<UStaticMeshComponent> SpriteMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> LegacyPlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<UMaterialBillboardComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FIntPoint FootprintSize = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FIntPoint GameplayTile = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FVector VisualAnchorOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	ERokCityViewLayer ViewLayer = ERokCityViewLayer::BuildingBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	int32 ViewSortPriority = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	FVector UnityBaseVisualCenterOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	float CurrentUnityLodScale = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	FVector UnityBaseSpriteScale = FVector::OneVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	FVector UnityBaseSourceMeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	bool bUseSourceMesh = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|View")
	bool bUseUnityCameraLod = false;

	FVector SelectionOriginalSpriteScale = FVector::OneVector;
	bool bSelectionHighlighted = false;

	static float EvaluateUnityCityLodScale(float Dxf);
	void ApplyUnityLodScale(float LodScale);
};
