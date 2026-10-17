#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RokStoneDepositData.h"
#include "RokStoneDepositComponent.generated.h"

class UStaticMeshComponent;
class UTexture2D;

UCLASS()
class ROK_API URokStoneDepositComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	bool InitializeSource();
	void UpdatePresentation(float Dxf, bool bShowDetail);
	float GetSourceScale() const { return State.LastScale; }
	TArray<FString> GetActiveSourceItems() const;
private:
	FRokStoneDepositData Data;
	FRokStoneDepositState State;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
};
