#include "RokBuildingPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokCitySpriteActor.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* UiBuildingMenuTitlePath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_title.rok_building_menu_title");
const TCHAR* UiBuildingMenuLevelPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_level_bg.rok_building_menu_level_bg");
const TCHAR* UiBuildingMenuShadowPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_button_shadow.rok_building_menu_button_shadow");
const TCHAR* UiBuildingMenuInfoPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_info.rok_building_menu_info");
const TCHAR* UiBuildingMenuUpgradePath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_upgrade.rok_building_menu_upgrade");
const TCHAR* UiBuildingMenuFeaturePath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_feature.rok_building_menu_feature");
const TCHAR* UiBuildingMenuRemovePath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_remove.rok_building_menu_remove");
const TCHAR* UiBuildingMenuArrowUpPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_select_arrow_up.rok_building_select_arrow_up");
const TCHAR* UiBuildingMenuArrowLeftPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_select_arrow_left.rok_building_select_arrow_left");
const TCHAR* UiBuildingMenuArrowRightPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_select_arrow_right.rok_building_select_arrow_right");
const TCHAR* UiBuildingMenuArrowDownPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_select_arrow_down.rok_building_select_arrow_down");

const FVector2D BuildingMenuSize(472.7f, 340.0f);
}

void URokBuildingPanelWidget::SetSelectedBuilding(AActor* InBuildingActor)
{
	SelectedBuildingActor = InBuildingActor;
	LastActionMessage.Reset();
	SetVisibility(InBuildingActor ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Refresh();
}

TSharedRef<SWidget> URokBuildingPanelWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void URokBuildingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void URokBuildingPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (SelectedBuildingActor.IsValid())
	{
		UpdateMenuPosition();
		Refresh();
	}
}

void URokBuildingPanelWidget::HandleUpgradeClicked()
{
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem || !SelectedBuildingActor.IsValid())
	{
		return;
	}

	FString FailureReason;
	if (UpgradeSubsystem->StartUpgrade(SelectedBuildingActor.Get(), FailureReason))
	{
		LastActionMessage = TEXT("开始升级。");
	}
	else
	{
		LastActionMessage = FailureReason;
	}
	Refresh();
}

void URokBuildingPanelWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingPanelCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	BuildingMenuRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingMenuRoot"));
	BuildingMenuSlot = RootCanvas->AddChildToCanvas(BuildingMenuRoot);
	BuildingMenuSlot->SetAnchors(FAnchors(0.0f));
	BuildingMenuSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	BuildingMenuSlot->SetSize(BuildingMenuSize);

	auto AddImage = [this](const FName Name, const TCHAR* TexturePath, const FVector2D Position, const FVector2D Size, int32 ZOrder = 0)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTexture2D* Texture = LoadUiTexture(TexturePath))
		{
			Image->SetBrush(MakeImageBrush(Texture, Size));
		}
		UCanvasPanelSlot* Slot = BuildingMenuRoot->AddChildToCanvas(Image);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetZOrder(ZOrder);
		return Image;
	};

	AddImage(TEXT("ButtonShadow"), UiBuildingMenuShadowPath, FVector2D(50.85f, 190.0f), FVector2D(371.0f, 148.0f));
	AddImage(TEXT("TitleFrame"), UiBuildingMenuTitlePath, FVector2D(100.35f, 0.0f), FVector2D(272.0f, 53.0f), 2);
	LevelFrameImage = AddImage(TEXT("LevelFrame"), UiBuildingMenuLevelPath, FVector2D(169.85f, 48.0f), FVector2D(133.0f, 26.0f), 2);
	AddImage(TEXT("SelectArrowUp"), UiBuildingMenuArrowUpPath, FVector2D(220.35f, 82.0f), FVector2D(32.0f, 32.0f), 1);
	AddImage(TEXT("SelectArrowLeft"), UiBuildingMenuArrowLeftPath, FVector2D(152.35f, 132.0f), FVector2D(32.0f, 32.0f), 1);
	AddImage(TEXT("SelectArrowRight"), UiBuildingMenuArrowRightPath, FVector2D(288.35f, 132.0f), FVector2D(32.0f, 32.0f), 1);
	AddImage(TEXT("SelectArrowDown"), UiBuildingMenuArrowDownPath, FVector2D(220.35f, 178.0f), FVector2D(32.0f, 32.0f), 1);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingTitleText"));
	TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.96f, 0.88f, 1.0f)));
	TitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 22));
	TitleText->SetJustification(ETextJustify::Center);
	TitleText->SetShadowOffset(FVector2D(1.0f));
	TitleText->SetShadowColorAndOpacity(FLinearColor(0.2f, 0.05f, 0.01f, 0.8f));
	UCanvasPanelSlot* TitleSlot = BuildingMenuRoot->AddChildToCanvas(TitleText);
	TitleSlot->SetPosition(FVector2D(100.35f, 7.0f));
	TitleSlot->SetSize(FVector2D(272.0f, 36.0f));
	TitleSlot->SetZOrder(3);

	LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingLevelText"));
	LevelText->SetVisibility(ESlateVisibility::HitTestInvisible);
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LevelText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 18));
	LevelText->SetJustification(ETextJustify::Center);
	LevelText->SetShadowOffset(FVector2D(1.0f));
	LevelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	UCanvasPanelSlot* LevelSlot = BuildingMenuRoot->AddChildToCanvas(LevelText);
	LevelSlot->SetPosition(FVector2D(169.85f, 48.0f));
	LevelSlot->SetSize(FVector2D(133.0f, 26.0f));
	LevelSlot->SetZOrder(3);

	auto AddCommandButton = [this](
		const FName ButtonName,
		const FName ImageName,
		const TCHAR* TexturePath,
		const FVector2D Position,
		TObjectPtr<UImage>& OutImage,
		TObjectPtr<UCanvasPanelSlot>& OutSlot)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		FButtonStyle ButtonStyle;
		ButtonStyle.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		ButtonStyle.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		ButtonStyle.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		ButtonStyle.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		ButtonStyle.NormalPadding = FMargin(0.0f);
		ButtonStyle.PressedPadding = FMargin(1.0f);
		Button->SetStyle(ButtonStyle);
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), ImageName);
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UTexture2D* Texture = LoadUiTexture(TexturePath))
		{
			Image->SetBrush(MakeImageBrush(Texture, FVector2D(88.0f)));
		}
		Button->AddChild(Image);
		UCanvasPanelSlot* Slot = BuildingMenuRoot->AddChildToCanvas(Button);
		Slot->SetPosition(Position);
		Slot->SetSize(FVector2D(88.0f));
		Slot->SetZOrder(4);
		OutImage = Image;
		OutSlot = Slot;
		return Button;
	};

	TObjectPtr<UImage> InfoButtonImage;
	TObjectPtr<UImage> UpgradeButtonImage;
	InfoButton = AddCommandButton(TEXT("BuildingInfoButton"), TEXT("BuildingInfoButtonImage"), UiBuildingMenuInfoPath, FVector2D(92.35f, 224.0f), InfoButtonImage, InfoButtonSlot);
	UpgradeButton = AddCommandButton(TEXT("BuildingUpgradeButton"), TEXT("BuildingUpgradeButtonImage"), UiBuildingMenuUpgradePath, FVector2D(192.35f, 236.0f), UpgradeButtonImage, UpgradeButtonSlot);
	FeatureButton = AddCommandButton(TEXT("BuildingFeatureButton"), TEXT("BuildingFeatureButtonImage"), UiBuildingMenuFeaturePath, FVector2D(292.35f, 224.0f), FeatureButtonImage, FeatureButtonSlot);
	FeatureButtonTexture = LoadUiTexture(UiBuildingMenuFeaturePath);
	RemoveButtonTexture = LoadUiTexture(UiBuildingMenuRemovePath);
	UpgradeButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleUpgradeClicked);
}

void URokBuildingPanelWidget::Refresh()
{
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem || !SelectedBuildingActor.IsValid() || !TitleText || !LevelText || !UpgradeButton)
	{
		return;
	}

	const FRokBuildingState State = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
	const bool bIsDecoration = State.UnityBuildingTypeId >= 51 && State.UnityBuildingTypeId <= 54;
	ApplyMenuLayout(bIsDecoration);
	FString CannotUpgradeReason;
	const bool bCanStartUpgrade = UpgradeSubsystem->CanStartUpgrade(SelectedBuildingActor.Get(), CannotUpgradeReason);
	if (LastActionMessage.Equals(TEXT("开始升级。")) && !State.bUpgradeInProgress)
	{
		LastActionMessage = TEXT("升级完成。");
	}

	TitleText->SetText(State.DisplayName);
	if (!bIsDecoration)
	{
		LevelText->SetText(FText::Format(
			NSLOCTEXT("RokBuildings", "UnityBuildingMenuLevel", "等级{0}"),
			FText::AsNumber(State.Level)));
		UpgradeButton->SetIsEnabled(bCanStartUpgrade && !State.bUpgradeInProgress);
	}
}

void URokBuildingPanelWidget::ApplyMenuLayout(bool bIsDecoration)
{
	if (bMenuLayoutInitialized && bShowingDecorationLayout == bIsDecoration)
	{
		return;
	}

	bMenuLayoutInitialized = true;
	bShowingDecorationLayout = bIsDecoration;
	const ESlateVisibility LevelVisibility = bIsDecoration
		? ESlateVisibility::Collapsed
		: ESlateVisibility::HitTestInvisible;
	if (LevelFrameImage)
	{
		LevelFrameImage->SetVisibility(LevelVisibility);
	}
	if (LevelText)
	{
		LevelText->SetVisibility(LevelVisibility);
	}
	if (UpgradeButton)
	{
		UpgradeButton->SetVisibility(bIsDecoration ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (InfoButtonSlot)
	{
		InfoButtonSlot->SetPosition(bIsDecoration ? FVector2D(142.35f, 224.0f) : FVector2D(92.35f, 224.0f));
	}
	if (UpgradeButtonSlot)
	{
		UpgradeButtonSlot->SetPosition(FVector2D(192.35f, 236.0f));
	}
	if (FeatureButtonSlot)
	{
		FeatureButtonSlot->SetPosition(bIsDecoration ? FVector2D(242.35f, 224.0f) : FVector2D(292.35f, 224.0f));
	}
	if (FeatureButtonImage)
	{
		UTexture2D* Texture = bIsDecoration ? RemoveButtonTexture.Get() : FeatureButtonTexture.Get();
		if (Texture)
		{
			FeatureButtonImage->SetBrush(MakeImageBrush(Texture, FVector2D(88.0f)));
		}
	}
}

void URokBuildingPanelWidget::UpdateMenuPosition()
{
	if (!BuildingMenuSlot || !SelectedBuildingActor.IsValid())
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}

	FVector AnchorWorldPosition = SelectedBuildingActor->GetActorLocation();
	if (const ARokCitySpriteActor* CitySpriteActor = Cast<ARokCitySpriteActor>(SelectedBuildingActor.Get()))
	{
		// Unity decorations (Road and later enum values) use the prefab's first
		// BoxCollider transform for the menu, title, and move-arrow root. The UE
		// placement anchor is the corresponding ground/grid position. Ordinary
		// buildings keep following their rendered center until their dedicated
		// Unity title/button child transforms are migrated.
		AnchorWorldPosition = CitySpriteActor->IsDecoration()
			? CitySpriteActor->GetVisualAnchorWorldPosition()
			: CitySpriteActor->GetVisualCenterWorldPosition();
	}
	else
	{
		FVector BoundsExtent = FVector::ZeroVector;
		SelectedBuildingActor->GetActorBounds(true, AnchorWorldPosition, BoundsExtent);
	}
	FVector2D WidgetPosition;
	if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, AnchorWorldPosition, WidgetPosition, true))
	{
		BuildingMenuSlot->SetPosition(WidgetPosition);
		BuildingMenuRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		BuildingMenuRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

URokBuildingUpgradeSubsystem* URokBuildingPanelWidget::GetUpgradeSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
}

UTexture2D* URokBuildingPanelWidget::LoadUiTexture(const TCHAR* TexturePath) const
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: Missing processed texture %s"), TexturePath);
	}
	return Texture;
}

FSlateBrush URokBuildingPanelWidget::MakeImageBrush(UTexture2D* Texture, const FVector2D& DesiredSize) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = DesiredSize;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	return Brush;
}
