#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokWorldMapEntitySubsystem.h"
#include "RokWorldMapSpriteAnimation.h"
#include "RokWorldMapEntityActor.generated.h"

class ARokWorldMapCameraPawn;
class UBoxComponent;
class UMaterial;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTexture2D;
class URokFoodDepositComponent;
class URokWoodDepositComponent;
class URokStoneDepositComponent;
class URokVillagePresentationComponent;

UENUM(BlueprintType)
enum class ERokWorldMapEntityLodBand : uint8
{
	CloseDetail,
	TacticalDetail,
	FarIcon,
	Hidden
};

UCLASS()
class ROK_API ARokWorldMapEntityActor : public AActor
{
	GENERATED_BODY()

public:
	ARokWorldMapEntityActor();

	virtual void Tick(float DeltaSeconds) override;

	bool Configure(
		const FRokWorldMapEntityRecord& InRecord,
		float InitialDxf);

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	int32 GetSourceId() const { return Record.SourceId; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	FString GetEntityKind() const { return Record.Kind; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	FString GetDisplayName() const { return Record.DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	int32 GetEntityLevel() const { return Record.Level; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	FVector2D GetUnityPosition() const { return Record.UnityPosition; }

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	FText GetSelectionText() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	FVector GetSelectionAnchorWorldPosition() const;

	UFUNCTION(BlueprintPure, Category = "Rok|World Map|Entity")
	ERokWorldMapEntityLodBand GetCurrentLodBand() const
	{
		return CurrentLodBand;
	}

	static ERokWorldMapEntityLodBand ResolveLodBand(
		float Dxf,
		float CloseDetailMaxDxf,
		float TacticalDetailMaxDxf,
		float FarIconMaxDxf);

	static bool ShouldExpandFormation(float Dxf);

private:
	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|Entity")
	TObjectPtr<USceneComponent> SceneRoot;

	// 45-degree tilted camera-facing card planes (Unity world-map sprite
	// quad orientation). UBillboardComponent is gated by the editor
	// BillboardSprites show flag and must not back gameplay entities.
	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|Entity")
	TObjectPtr<UStaticMeshComponent> DetailVisual;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|Entity")
	TObjectPtr<UStaticMeshComponent> FarVisual;

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|Entity")
	TObjectPtr<UBoxComponent> SelectionCollision;

	UPROPERTY(Transient)
	TObjectPtr<URokFoodDepositComponent> FoodDeposit;

	UPROPERTY(Transient)
	TObjectPtr<URokWoodDepositComponent> WoodDeposit;

	UPROPERTY(Transient)
	TObjectPtr<URokStoneDepositComponent> StoneDeposit;

	UPROPERTY(Transient)
	TObjectPtr<URokVillagePresentationComponent> VillagePresentation;

	// Per-unit cards from the Unity BarbarianFormation prefab. Each entry
	// owns the base card plus (for expanding rows) two close-LOD clone
	// cards, and animates the idle frame sequence independently.
	struct FFormationUnitVisual
	{
		TObjectPtr<UStaticMeshComponent> Component;
		TObjectPtr<UStaticMeshComponent> CloneA;
		TObjectPtr<UStaticMeshComponent> CloneB;
		FRokWorldMapFormationUnit Unit;
		TArray<TObjectPtr<UTexture2D>> Frames;
		TArray<TObjectPtr<UTexture2D>> FadeFrames;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		TWeakObjectPtr<UMaterialInstanceDynamic> CloneMaterialA;
		TWeakObjectPtr<UMaterialInstanceDynamic> CloneMaterialB;
		FVector TacticalLocation = FVector::ZeroVector;
		FVector CloseLocation = FVector::ZeroVector;
		FVector CloneALocation = FVector::ZeroVector;
		FVector CloneBLocation = FVector::ZeroVector;
		FRokWorldMapSpriteAnimation Animation[3];
		float FadeAmount[3] = { 1.0f, 1.0f, 1.0f };
	};

	TArray<FFormationUnitVisual> FormationUnitVisuals;

	// The non-reflected per-unit structs and the shared weak cache do not
	// retain textures for GC. Keep every idle frame alive while this actor lives.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> IdleFrameTextures;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DetailTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FarTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterial> CardMaterialParent;

	bool bCardMaterialParentLoadFailed = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|Entity")
	FRokWorldMapEntityRecord Record;

	UPROPERTY(VisibleInstanceOnly, Category = "Rok|World Map|Entity")
	ERokWorldMapEntityLodBand CurrentLodBand =
		ERokWorldMapEntityLodBand::Hidden;

	float DetailBaseWidth = 200.0f;
	float FarBaseWidth = 200.0f;
	float LastAppliedDxf = -1.0f;
	bool bVisibleBandDetailed = false;

	static UTexture2D* LoadTransientTexture(const FString& FilePath);
	static UTexture2D* LoadSharedTexture(const FString& FilePath);
	UMaterialInstanceDynamic* MakeCardMaterial(UTexture2D* Texture);
	void BuildFormationUnits();
	void ApplyUnitScale(float Dxf);
	void ApplyDxf(float Dxf);
	void AdvanceIdleAnimation(float DeltaSeconds);
};
