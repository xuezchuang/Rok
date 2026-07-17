#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
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

UCLASS()
class ROK_API URokBuildingPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSelectedBuilding(AActor* InBuildingActor);

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
	TObjectPtr<UTextBlock> UpgradeButtonText;

	FString LastActionMessage;
	bool bMenuLayoutInitialized = false;
	bool bShowingDecorationLayout = false;

	UFUNCTION()
	void HandleUpgradeClicked();

	void BuildWidgetTree();
	void Refresh();
	void ApplyMenuLayout(bool bIsDecoration);
	void UpdateMenuPosition();
	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;
	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& DesiredSize) const;
	URokBuildingUpgradeSubsystem* GetUpgradeSubsystem() const;
};
