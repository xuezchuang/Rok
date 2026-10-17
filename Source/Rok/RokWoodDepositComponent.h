#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RokWoodDepositData.h"
#include "RokWoodDepositComponent.generated.h"

class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class ROK_API URokWoodDepositComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	bool InitializeSource();
	void UpdatePresentation(float Dxf, bool bShowDetail);
	float GetSourceScale() const { return State.LastScale; }
	TArray<FString> GetActiveSourceItems() const;
private:
	FRokWoodDepositData Data;
	FRokWoodDepositState State;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
};
