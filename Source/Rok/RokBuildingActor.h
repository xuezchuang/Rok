#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokBuildingActor.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ROK_API ARokBuildingActor : public AActor
{
	GENERATED_BODY()

public:
	ARokBuildingActor();

	UFUNCTION(BlueprintCallable, Category = "Rok|Building")
	void ConfigureCard(UMaterialInterface* Material, float Width, float Height, FVector InPivotOffset, FRotator InCardRotation);

	UFUNCTION(BlueprintCallable, Category = "Rok|Building")
	void ConfigureFootprint(float Width, float Length, UMaterialInterface* HighlightMaterial, float YawDegrees = 45.0f);

	UFUNCTION(BlueprintCallable, Category = "Rok|Building")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Rok|Building")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintPure, Category = "Rok|Building")
	FVector GetPivotOffset() const { return BuildingPivotOffset; }

	UFUNCTION(BlueprintPure, Category = "Rok|Building")
	UStaticMeshComponent* GetCardMeshComponent() const { return CardMeshComponent; }

	UFUNCTION(BlueprintPure, Category = "Rok|Building")
	UBoxComponent* GetFootprintCollision() const { return FootprintCollision; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	TObjectPtr<UStaticMeshComponent> CardMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	TObjectPtr<UBoxComponent> FootprintCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	TObjectPtr<UStaticMeshComponent> FootprintHighlightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	FVector BuildingPivotOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	FVector2D CardSize = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	FVector2D FootprintSize = FVector2D(220.0f, 220.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rok|Building")
	bool bSelected = false;
};
