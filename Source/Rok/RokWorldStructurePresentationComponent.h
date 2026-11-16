#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RokWorldStructurePresentationData.h"
#include "RokWorldStructurePresentationComponent.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class UTexture2D;

UCLASS()
class ROK_API URokWorldStructurePresentationComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	bool InitializeSource(const FString& ModelId);
	void UpdatePresentation(float Dxf, bool bShowDetail);
	const FString& GetModelId() const { return Data.ModelId; }
	int32 GetCardCount() const { return Cards.Num(); }
	int32 GetMeshCount() const { return MeshLayers.Num(); }
	int32 GetActiveMeshCount() const { return ActiveMeshCount; }
	int32 GetActiveCardCount() const { return ActiveCardCount; }
private:
	FRokWorldStructurePresentationData Data;
	FRokStoneDepositData CollisionData;
	FRokStoneDepositState CollisionState;
	TArray<bool> GroupVisibility;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Cards;
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> MeshLayers;
	int32 ActiveCardCount = 0;
	int32 ActiveMeshCount = 0;
	void ClearCards();
};
