#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Curves/RichCurve.h"
#include "RokWorldMapTypes.h"
#include "RokStrategicResourceLayer.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/** Presentation of UI_Item_LodMenu.OnRssMiniMap; never creates harvestable game entities. */
UCLASS()
class ROK_API ARokStrategicResourceLayer : public AActor
{
	GENERATED_BODY()
public:
	ARokStrategicResourceLayer();
	// Configure before first UpdateView. Separate HUD-owned instance for exploration.
	void ConfigureExploration();
	void SetExplorationProgress(const TMap<int32, uint64>& Words) { ExplorationWords = Words; }
	void UpdateView(bool bEnabled, const FRokWorldMapProjection& Projection, FVector2D Center, float Dxf);
	static FIntPoint SourceCell(FVector2D Center);
	int32 GetVisiblePointCount() const { return PointMeshes.Num(); }
private:
	struct FPoint { int32 Id; int32 Type; FVector2D Position; FIntPoint Cell; };
	TArray<FPoint> Points;
	TMap<int32, FRichCurve> Curves;
	TMap<int32, FVector2D> UnitSizes;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> Materials;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> OverlayMaterial;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Overlay;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<UStaticMeshComponent>> PointMeshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> BaseMaterial;
	FIntPoint CurrentCell = FIntPoint(-1, -1);
	bool bAttemptedLoad = false;
	bool bLoaded = false;
	bool bExploration = false;
	TMap<int32, uint64> ExplorationWords;
	bool LoadSource();
	UStaticMeshComponent* MakePlane(UMaterialInstanceDynamic* Material, bool bHorizontal);
};
