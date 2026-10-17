#include "RokBuildingPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
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
const TCHAR* UiBuildingMenuSpeedUpPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_speedup.rok_building_menu_speedup");
const TCHAR* UiBuildingMenuWorkerPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_worker.rok_building_menu_worker");
const TCHAR* UiBuildingMenuInfantryPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_train_infantry.rok_building_menu_train_infantry");
const TCHAR* UiBuildingMenuCavalryPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_train_cavalry.rok_building_menu_train_cavalry");
const TCHAR* UiBuildingMenuArcherPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_train_archer.rok_building_menu_train_archer");
const TCHAR* UiBuildingMenuSiegePath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_train_siege.rok_building_menu_train_siege");
const TCHAR* UiBuildingMenuHospitalPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_hospital.rok_building_menu_hospital");
const TCHAR* UiBuildingMenuScoutPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_scout.rok_building_menu_scout");
const TCHAR* UiBuildingMenuResearchPath = TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_research.rok_building_menu_research");
const FVector2D BuildingMenuSize(472.7f, 340.0f);
constexpr float BuildingTitleHeight = 53.0f;
constexpr float BuildingButtonBoundsGap = 6.0f;
constexpr float BuildingShadowButtonOffset = 34.0f;
constexpr float BuildingShadowY = 184.0f;
constexpr float BuildingInfoButtonY = 218.0f;
constexpr float BuildingUpgradeButtonY = 230.0f;
}

void URokBuildingPanelWidget::SetSelectedBuilding(AActor* InBuildingActor)
{
	if (!IsValid(InBuildingActor))
	{
		SelectedBuildingActor.Reset();
		if (!bMenuOpen) return;
		bMenuOpen = false;
		MenuAnimation.Close();
		// Keep the outgoing content at its last anchor, but stop accepting commands.
		SetVisibility(ESlateVisibility::HitTestInvisible);
		AdvanceMenuAnimation(0);
		return;
	}
	const bool bNewSelection = !bMenuOpen || SelectedBuildingActor.Get() != InBuildingActor;
	SelectedBuildingActor = InBuildingActor;
	if (bNewSelection)
	{
		LastActionMessage.Reset();
		MenuAnimation.Open(ERokUiWindowAnimationStyle::BuildingMenu);
	}
	bMenuOpen = true;
	// This user widget also spans the full viewport. Only its child UButtons
	// should consume pointer input; the transparent area must pass world clicks.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Refresh();
	AdvanceMenuAnimation(0);
}

void URokBuildingPanelWidget::AdvanceMenuAnimation(float DeltaSeconds)
{
	const bool bClosed = MenuAnimation.Advance(DeltaSeconds);
	const FRokUiWindowAnimationSample& Sample = MenuAnimation.GetSample();
	if (BuildingTitleRoot) BuildingTitleRoot->SetRenderScale(FVector2D(Sample.Scale));
	if (BuildingMenuRoot) BuildingMenuRoot->SetRenderOpacity(Sample.Opacity);
	if (bClosed) SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> URokBuildingPanelWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void URokBuildingPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(bMenuOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	AdvanceMenuAnimation(0);
}

void URokBuildingPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bMenuOpen && !SelectedBuildingActor.IsValid()) SetSelectedBuilding(nullptr);
	AdvanceMenuAnimation(InDeltaTime);
	if (SelectedBuildingActor.IsValid())
	{
		UpdateMenuPosition();
		Refresh();
	}
}

void URokBuildingPanelWidget::HandleUpgradeClicked()
{
	if (SelectedBuildingActor.IsValid() && OnUpgradeRequested.IsBound())
	{
		OnUpgradeRequested.Broadcast(SelectedBuildingActor.Get());
		return;
	}

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

void URokBuildingPanelWidget::HandleInfoClicked()
{
	if (SelectedBuildingActor.IsValid())
	{
		OnInfoRequested.Broadcast(SelectedBuildingActor.Get());
	}
}

void URokBuildingPanelWidget::HandleFeatureClicked()
{
	if (SelectedBuildingActor.IsValid())
	{
		OnFeatureRequested.Broadcast(SelectedBuildingActor.Get());
	}
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
	BuildingMenuSlot->SetAlignment(FVector2D(0.5f, 0.0f));
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

	AddImage(TEXT("ButtonShadow"), UiBuildingMenuShadowPath, FVector2D(50.85f, BuildingShadowY), FVector2D(371.0f, 148.0f));
	AddImage(TEXT("TitleFrame"), UiBuildingMenuTitlePath, FVector2D(100.35f, 0.0f), FVector2D(272.0f, 53.0f), 2);
	LevelFrameImage = AddImage(TEXT("LevelFrame"), UiBuildingMenuLevelPath, FVector2D(169.85f, 48.0f), FVector2D(133.0f, 26.0f), 2);
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

	// UA_BuildingMenu scales pl_Title only. Keep the action buttons outside
	// this group; Unity's (.5,1) top pivot maps to UMG (.5,0).
	BuildingTitleRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingTitleRoot"));
	BuildingTitleRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	BuildingTitleRoot->SetRenderTransformPivot(FVector2D(0.5f, 0));
	UCanvasPanelSlot* TitleRootSlot = BuildingMenuRoot->AddChildToCanvas(BuildingTitleRoot);
	TitleRootSlot->SetSize(FVector2D(BuildingMenuSize.X, 74));
	TitleRootSlot->SetZOrder(3);
	for (const FName Name : {FName(TEXT("TitleFrame")), FName(TEXT("LevelFrame")),
		FName(TEXT("BuildingTitleText")), FName(TEXT("BuildingLevelText"))})
	{
		UWidget* Child = WidgetTree->FindWidget(Name);
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		auto* NewSlot = BuildingTitleRoot->AddChildToCanvas(Child);
		NewSlot->SetPosition(Position);
		NewSlot->SetSize(Size);
		NewSlot->SetZOrder(ZOrder);
	}

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
	InfoButton = AddCommandButton(TEXT("BuildingInfoButton"), TEXT("BuildingInfoButtonImage"), UiBuildingMenuInfoPath, FVector2D(92.35f, BuildingInfoButtonY), InfoButtonImage, InfoButtonSlot);
	UpgradeButton = AddCommandButton(TEXT("BuildingUpgradeButton"), TEXT("BuildingUpgradeButtonImage"), UiBuildingMenuUpgradePath, FVector2D(192.35f, BuildingUpgradeButtonY), UpgradeButtonImage, UpgradeButtonSlot);
	FeatureButton = AddCommandButton(TEXT("BuildingFeatureButton"), TEXT("BuildingFeatureButtonImage"), UiBuildingMenuFeaturePath, FVector2D(292.35f, BuildingInfoButtonY), FeatureButtonImage, FeatureButtonSlot);
	FeatureButtonTexture = LoadUiTexture(UiBuildingMenuFeaturePath);
	RemoveButtonTexture = LoadUiTexture(UiBuildingMenuRemovePath);
	SpeedUpButtonTexture = LoadUiTexture(UiBuildingMenuSpeedUpPath);
	WorkerButtonTexture = LoadUiTexture(UiBuildingMenuWorkerPath);
	InfantryButtonTexture = LoadUiTexture(UiBuildingMenuInfantryPath);
	CavalryButtonTexture = LoadUiTexture(UiBuildingMenuCavalryPath);
	ArcherButtonTexture = LoadUiTexture(UiBuildingMenuArcherPath);
	SiegeButtonTexture = LoadUiTexture(UiBuildingMenuSiegePath);
	HospitalButtonTexture = LoadUiTexture(UiBuildingMenuHospitalPath);
	ScoutButtonTexture = LoadUiTexture(UiBuildingMenuScoutPath);
	ResearchButtonTexture = LoadUiTexture(UiBuildingMenuResearchPath);
	InfoButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleInfoClicked);
	UpgradeButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleUpgradeClicked);
	FeatureButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleFeatureClicked);
}

void URokBuildingPanelWidget::Refresh()
{
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem || !SelectedBuildingActor.IsValid() || !TitleText || !LevelText || !UpgradeButton)
	{
		return;
	}

	const FRokBuildingState State = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
	const bool bIsDecoration = State.UnityBuildingTypeId >= 51 && State.UnityBuildingTypeId <= 55;
	const bool bHasFeatureAction = State.bUpgradeInProgress
		|| State.UnityBuildingTypeId == 7
		|| (State.UnityBuildingTypeId >= 9 && State.UnityBuildingTypeId <= 12)
		// BuildingTypeConfig[13] -> BuildingMenuData[110], levels 1..25.
		|| (State.UnityBuildingTypeId == 13 && State.Level >= 1 && State.Level <= 25)
		|| State.UnityBuildingTypeId == 14
		// BuildingTypeConfig[28] -> BuildingMenuData[137], levels 1..25.
		|| (State.UnityBuildingTypeId == 28 && State.Level >= 1 && State.Level <= 25);
	ApplyMenuLayout(bIsDecoration, bHasFeatureAction);
	ApplyFeatureButtonVisual(State.UnityBuildingTypeId, State.bUpgradeInProgress);
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
		// Unity openBuildingUpdata opens its requirement details even when the upgrade cannot start.
		// Actual resource/queue/prerequisite validation stays on the dialog's timed command.
		UpgradeButton->SetIsEnabled(State.Type != ERokBuildingType::Unknown && !State.bUpgradeInProgress
			&& State.Level < UpgradeSubsystem->GetMaxBuildingLevelForActor(SelectedBuildingActor.Get()));
	}
}

void URokBuildingPanelWidget::ApplyMenuLayout(
	bool bIsDecoration,
	bool bHasFeatureAction)
{
	if (bMenuLayoutInitialized
		&& bShowingDecorationLayout == bIsDecoration
		&& bShowingFeatureAction == bHasFeatureAction)
	{
		return;
	}

	bMenuLayoutInitialized = true;
	bShowingDecorationLayout = bIsDecoration;
	bShowingFeatureAction = bHasFeatureAction;
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
	if (FeatureButton)
	{
		FeatureButton->SetVisibility(bHasFeatureAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FeatureButton->SetIsEnabled(bHasFeatureAction);
	}
	if (InfoButtonSlot)
	{
		InfoButtonSlot->SetPosition(
			bIsDecoration
				? FVector2D(192.35f, BuildingInfoButtonY)
				: (bHasFeatureAction
					? FVector2D(92.35f, BuildingInfoButtonY)
					: FVector2D(142.35f, BuildingInfoButtonY)));
	}
	if (UpgradeButtonSlot)
	{
		UpgradeButtonSlot->SetPosition(bHasFeatureAction
			? FVector2D(192.35f, BuildingUpgradeButtonY)
			: FVector2D(242.35f, BuildingUpgradeButtonY));
	}
	if (FeatureButtonSlot)
	{
		FeatureButtonSlot->SetPosition(FVector2D(292.35f, BuildingInfoButtonY));
	}
	if (UWidget* ButtonShadow = WidgetTree->FindWidget(TEXT("ButtonShadow")))
	{
		if (UCanvasPanelSlot* ButtonShadowSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ButtonShadow))
		{
			ButtonShadowSlot->SetPosition(FVector2D(50.85f, BuildingShadowY));
		}
	}
	if (FeatureButtonImage && bIsDecoration && bHasFeatureAction)
	{
		UTexture2D* Texture = RemoveButtonTexture.Get();
		if (Texture)
		{
			FeatureButtonImage->SetBrush(MakeImageBrush(Texture, FVector2D(88.0f)));
		}
	}
}

void URokBuildingPanelWidget::ApplyFeatureButtonVisual(
	int32 UnityBuildingTypeId,
	bool bUpgradeInProgress)
{
	if (!FeatureButtonImage)
	{
		return;
	}
	FeatureButtonImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UnityBuildingTypeId == 13 && !bUpgradeInProgress)
	{
		// Source btn_hud_1018; a missing import must not show another feature icon.
		FeatureButtonImage->SetBrush(MakeImageBrush(ResearchButtonTexture.Get(), FVector2D(88.0f)));
		if (!ResearchButtonTexture) FeatureButtonImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	if (UnityBuildingTypeId == 28 && !bUpgradeInProgress)
	{
		// Source btn_hud_1041; do not display another building's action if its import is missing.
		FeatureButtonImage->SetBrush(MakeImageBrush(ScoutButtonTexture.Get(), FVector2D(88.0f)));
		if (!ScoutButtonTexture) FeatureButtonImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	UTexture2D* Texture = bUpgradeInProgress
		? SpeedUpButtonTexture.Get()
		: nullptr;
	if (!Texture)
	{
		switch (UnityBuildingTypeId)
		{
		case 7:
			Texture = WorkerButtonTexture.Get();
			break;
		case 9:
			Texture = InfantryButtonTexture.Get();
			break;
		case 10:
			Texture = CavalryButtonTexture.Get();
			break;
		case 11:
			Texture = ArcherButtonTexture.Get();
			break;
		case 12:
			Texture = SiegeButtonTexture.Get();
			break;
		case 14:
			Texture = HospitalButtonTexture.Get();
			break;
		default:
			break;
		}
	}
	if (!Texture)
	{
		Texture = FeatureButtonTexture.Get();
	}
	if (Texture)
	{
		FeatureButtonImage->SetBrush(MakeImageBrush(Texture, FVector2D(88.0f)));
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
	FVector VisualBoundsOrigin = AnchorWorldPosition;
	FVector VisualBoundsExtent = FVector::ZeroVector;
	bool bIsDecoration = false;
	if (const ARokCitySpriteActor* CitySpriteActor = Cast<ARokCitySpriteActor>(SelectedBuildingActor.Get()))
	{
		// Keep the menu horizontally centered on Unity's building/grid anchor,
		// while deriving the title and command rows from the rendered sprite
		// bounds. The sprite bounds change with Unity camera LOD, so a fixed
		// screen-space offset drifts away from small decorations and large
		// buildings as the camera zoom changes.
		AnchorWorldPosition = CitySpriteActor->GetVisualAnchorWorldPosition();
		VisualBoundsOrigin = CitySpriteActor->GetVisualCenterWorldPosition();
		VisualBoundsExtent = CitySpriteActor->GetVisualBoundsExtent();
		bIsDecoration = CitySpriteActor->IsDecoration();
	}
	else
	{
		SelectedBuildingActor->GetActorBounds(true, VisualBoundsOrigin, VisualBoundsExtent);
		AnchorWorldPosition = VisualBoundsOrigin;
	}

	// With a constrained camera aspect ratio, UE projects world positions inside
	// the constrained view rectangle, but the UMG canvas still fills the full
	// viewport (including letterbox/pillarbox bars). Add that view-rectangle
	// offset before assigning CanvasPanel positions.
	FVector2D ConstrainedViewOffset = FVector2D::ZeroVector;
	if (const AActor* ViewTarget = PlayerController->GetViewTarget())
	{
		if (const UCameraComponent* CameraComponent = ViewTarget->FindComponentByClass<UCameraComponent>();
			CameraComponent && CameraComponent->bConstrainAspectRatio && CameraComponent->AspectRatio > UE_SMALL_NUMBER)
		{
			int32 ViewportWidth = 0;
			int32 ViewportHeight = 0;
			PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
			if (ViewportWidth > 0 && ViewportHeight > 0)
			{
				const float ViewportAspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
				if (ViewportAspectRatio > CameraComponent->AspectRatio)
				{
					const float ConstrainedWidth = ViewportHeight * CameraComponent->AspectRatio;
					ConstrainedViewOffset.X = (ViewportWidth - ConstrainedWidth) * 0.5f;
				}
				else if (ViewportAspectRatio < CameraComponent->AspectRatio)
				{
					const float ConstrainedHeight = ViewportWidth / CameraComponent->AspectRatio;
					ConstrainedViewOffset.Y = (ViewportHeight - ConstrainedHeight) * 0.5f;
				}

				const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
				if (ViewportScale > UE_SMALL_NUMBER)
				{
					ConstrainedViewOffset /= ViewportScale;
				}
			}
		}
	}

	auto ProjectToMenuWidget = [PlayerController, ConstrainedViewOffset](
		const FVector& WorldPosition,
		FVector2D& OutWidgetPosition)
	{
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController,
			WorldPosition,
			OutWidgetPosition,
			true))
		{
			return false;
		}

		OutWidgetPosition += ConstrainedViewOffset;
		return true;
	};

	FVector2D AnchorWidgetPosition;
	if (ProjectToMenuWidget(AnchorWorldPosition, AnchorWidgetPosition))
	{
		float VisualTopY = AnchorWidgetPosition.Y - BuildingInfoButtonY;
		float VisualBottomY = AnchorWidgetPosition.Y;
		bool bProjectedBounds = !VisualBoundsExtent.IsNearlyZero();

		if (bProjectedBounds)
		{
			VisualTopY = TNumericLimits<float>::Max();
			VisualBottomY = TNumericLimits<float>::Lowest();
			for (int32 XSign = -1; XSign <= 1 && bProjectedBounds; XSign += 2)
			{
				for (int32 YSign = -1; YSign <= 1 && bProjectedBounds; YSign += 2)
				{
					for (int32 ZSign = -1; ZSign <= 1; ZSign += 2)
					{
						const FVector BoundsCorner = VisualBoundsOrigin + FVector(
							VisualBoundsExtent.X * XSign,
							VisualBoundsExtent.Y * YSign,
							VisualBoundsExtent.Z * ZSign);
						FVector2D CornerWidgetPosition;
						if (!ProjectToMenuWidget(BoundsCorner, CornerWidgetPosition))
						{
							bProjectedBounds = false;
							break;
						}
						VisualTopY = FMath::Min(VisualTopY, CornerWidgetPosition.Y);
						VisualBottomY = FMath::Max(VisualBottomY, CornerWidgetPosition.Y);
					}
				}
			}
		}

		const float MenuTopY = bProjectedBounds
			? VisualTopY - BuildingTitleHeight
			: AnchorWidgetPosition.Y - BuildingInfoButtonY;
		const float ButtonY = bProjectedBounds
			? VisualBottomY + BuildingButtonBoundsGap - MenuTopY
			: BuildingInfoButtonY;

		BuildingMenuSlot->SetPosition(FVector2D(AnchorWidgetPosition.X, MenuTopY));
		BuildingMenuSlot->SetSize(FVector2D(
			BuildingMenuSize.X,
			FMath::Max(BuildingMenuSize.Y, ButtonY + 148.0f)));
		if (InfoButtonSlot)
		{
			InfoButtonSlot->SetPosition(FVector2D(bIsDecoration ? 142.35f : 92.35f, ButtonY));
		}
		if (UpgradeButtonSlot)
		{
			UpgradeButtonSlot->SetPosition(FVector2D(192.35f, ButtonY + 12.0f));
		}
		if (FeatureButtonSlot)
		{
			FeatureButtonSlot->SetPosition(FVector2D(bIsDecoration ? 242.35f : 292.35f, ButtonY));
		}
		if (UWidget* ButtonShadow = WidgetTree->FindWidget(TEXT("ButtonShadow")))
		{
			if (UCanvasPanelSlot* ButtonShadowSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ButtonShadow))
			{
				ButtonShadowSlot->SetPosition(FVector2D(
					50.85f,
					FMath::Max(0.0f, ButtonY - BuildingShadowButtonOffset)));
			}
		}
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
