#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokMainHudWidget.generated.h"

class AActor;
class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;

UCLASS()
class ROK_API URokMainHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSelectedBuilding(AActor* InBuildingActor);

	UFUNCTION(BlueprintCallable, Category="Rok HUD")
	void RefreshFromSubsystem();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UTextBlock> FoodText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WoodText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StoneText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY()
	TObjectPtr<UTextBlock> BuilderQueueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<URokBuildingPanelWidget> BuildingPanelWidget;

	UPROPERTY()
	TWeakObjectPtr<AActor> SelectedBuildingActor;

	FSlateBrush TopBarBrush;
	FSlateBrush BottomBarBrush;

	void BuildWidgetTree();
	UTextBlock* MakeHudText(const FName& Name, int32 FontSize, const FLinearColor& Color);
	UImage* MakeUiImage(const FName& Name, const TCHAR* TexturePath, const FVector2D& DesiredSize);
	UBorder* MakeResourceChip(const FName& Name, const TCHAR* TexturePath, UTextBlock*& OutTextBlock);
	UBorder* MakeToolButton(const FName& Name, const FText& Label);
	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;
	FSlateBrush MakeImageBrush(UTexture2D* Texture, const FVector2D& DesiredSize) const;
	bool ApplyBorderTexture(UBorder* Border, FSlateBrush& BrushStorage, const TCHAR* TexturePath, const FVector2D& DesiredSize, const TCHAR* DebugName);
	URokBuildingUpgradeSubsystem* GetUpgradeSubsystem() const;
};
