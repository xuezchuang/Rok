#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokUiWindowAnimation.h"
#include "RokBuildingPanelWidget.generated.h"

class AActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class URokBuildingUpgradeSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FRokBuildingPanelActorAction, AActor*);

UCLASS()
class ROK_API URokBuildingPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSelectedBuilding(AActor* InBuildingActor);
	void AdvanceMenuAnimation(float DeltaSeconds);
	FRokBuildingPanelActorAction OnInfoRequested;
	FRokBuildingPanelActorAction OnUpgradeRequested;
	FRokBuildingPanelActorAction OnFeatureRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedBuildingActor;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> BuildingMenuRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> BuildingTitleRoot;

	FRokUiWindowAnimation MenuAnimation;
	bool bMenuOpen = false;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> BuildingMenuSlot;

	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UImage> BuildingIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TypeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY()
	TObjectPtr<UImage> LevelFrameImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResourceText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QueueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UProgressBar> UpgradeProgress;

	UPROPERTY()
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY()
	TObjectPtr<UButton> InfoButton;

	UPROPERTY()
	TObjectPtr<UButton> FeatureButton;

	UPROPERTY()
	TObjectPtr<UImage> FeatureButtonImage;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> InfoButtonSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> UpgradeButtonSlot;

	UPROPERTY()
	TObjectPtr<UCanvasPanelSlot> FeatureButtonSlot;

	UPROPERTY()
	TObjectPtr<UTexture2D> FeatureButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> RemoveButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> SpeedUpButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> WorkerButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> InfantryButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> CavalryButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> ArcherButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> SiegeButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> HospitalButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> ScoutButtonTexture;

	UPROPERTY()
	TObjectPtr<UTexture2D> ResearchButtonTexture;

	UPROPERTY()
	TObjectPtr<UTextBlock> UpgradeButtonText;

	FString LastActionMessage;
	bool bMenuLayoutInitialized = false;
	bool bShowingDecorationLayout = false;
	bool bShowingFeatureAction = false;

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION()
	void HandleInfoClicked();

	UFUNCTION()
	void HandleFeatureClicked();

	void BuildWidgetTree();
	void Refresh();
	void ApplyMenuLayout(bool bIsDecoration, bool bHasFeatureAction);
	void ApplyFeatureButtonVisual(int32 UnityBuildingTypeId, bool bUpgradeInProgress);
	void UpdateMenuPosition();
	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;
	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& DesiredSize) const;
	URokBuildingUpgradeSubsystem* GetUpgradeSubsystem() const;
};
