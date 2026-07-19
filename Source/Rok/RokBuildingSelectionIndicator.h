#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokBuildingSelectionIndicator.generated.h"

class ARokCitySpriteActor;
class USceneComponent;
class UStaticMeshComponent;

enum class ERokBuildingSelectionArrowDirection : uint8
{
	Up,
	Right,
	Down,
	Left,
};

/**
 * World-space counterpart of Unity's building_move_arrow prefab.
 *
 * The four planes stay on the city ground, use the selected building's real
 * grid footprint, and therefore scale naturally on screen with camera zoom.
 */
UCLASS()
class ROK_API ARokBuildingSelectionIndicator : public AActor
{
	GENERATED_BODY()

public:
	ARokBuildingSelectionIndicator();

	void ShowForBuilding(const ARokCitySpriteActor* BuildingActor);
	void HideIndicator();

	static FVector CalculateArrowOffset(
		FIntPoint FootprintSize,
		ERokBuildingSelectionArrowDirection Direction);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Rok|City|Selection")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Rok|City|Selection")
	TObjectPtr<UStaticMeshComponent> UpArrowComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|City|Selection")
	TObjectPtr<UStaticMeshComponent> RightArrowComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|City|Selection")
	TObjectPtr<UStaticMeshComponent> DownArrowComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rok|City|Selection")
	TObjectPtr<UStaticMeshComponent> LeftArrowComponent;
};
