#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokCitySpriteActor.generated.h"

class UMaterialInterface;
class UMaterialBillboardComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROK_API ARokCitySpriteActor : public AActor
{
	GENERATED_BODY()

public:
	ARokCitySpriteActor();

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigureSprite(UMaterialInterface* Material, float Width, float Height, int32 SortPriority);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void ConfigurePlacement(FIntPoint InFootprintSize, FIntPoint InGameplayTile, FVector InVisualAnchorOffset);

	UFUNCTION(BlueprintCallable, Category = "Rok|City")
	void SetSelectionCollisionEnabled(bool bEnabled);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<UStaticMeshComponent> SpriteMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	TObjectPtr<UMaterialBillboardComponent> SpriteComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FIntPoint FootprintSize = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FIntPoint GameplayTile = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|City")
	FVector VisualAnchorOffset = FVector::ZeroVector;
};
