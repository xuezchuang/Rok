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
#include "RokUiSpec.h"
#include "RokUiWidgets.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* UiBuildingMenuAtlasPath = TEXT("/Game/RokPrototype/UI/PlacementAtlases/rok_placement_ui_hud.rok_placement_ui_hud");
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
constexpr float BuildingShadowY = 184.0f;
constexpr float BuildingInfoButtonY = 218.0f;
constexpr float BuildingUpgradeButtonY = 230.0f;
}

TSubclassOf<URokBuildingPanelWidget> URokBuildingPanelWidget::ResolvePanelClass()
{
	static const TSoftClassPtr<URokBuildingPanelWidget> PanelClass(
		FSoftObjectPath(TEXT("/Game/RokUI/Building/WBP_RokBuildingPanel.WBP_RokBuildingPanel_C")));
	UClass* Class = URokUiSettings::Resolve(PanelClass);
	return Class ? Class : StaticClass();
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

void URokBuildingPanelWidget::SampleMenuOpenForCapture(float Seconds)
{
	if (!bMenuOpen)
	{
		bMenuOpen = true;
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	MenuAnimation.Open(ERokUiWindowAnimationStyle::BuildingMenu);
	AdvanceMenuAnimation(FMath::Max(0.0f, Seconds));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok building menu animation sample: %.4fs scale=%.4f opacity=%.4f."),
		Seconds,
		MenuAnimation.GetSample().Scale,
		MenuAnimation.GetSample().Opacity);
}

TSharedRef<SWidget> URokBuildingPanelWidget::RebuildWidget()
{
	BuildWidgetTree();
	if (!bDesignLayoutCached) CacheDesignLayout();
	return Super::RebuildWidget();
}

void URokBuildingPanelWidget::CacheDesignLayout()
{
	DesignMenuSize = BuildingMenuSlot ? BuildingMenuSlot->GetSize() : BuildingMenuSize;
	DesignInfoPosition = InfoButtonSlot ? InfoButtonSlot->GetPosition() : FVector2D(92.35f, BuildingInfoButtonY);
	DesignUpgradePosition = UpgradeButtonSlot ? UpgradeButtonSlot->GetPosition() : FVector2D(192.35f, BuildingUpgradeButtonY);
	DesignFeaturePosition = FeatureButtonSlot ? FeatureButtonSlot->GetPosition() : FVector2D(292.35f, BuildingInfoButtonY);
	const UCanvasPanelSlot* ShadowSlot = WidgetTree
		? UWidgetLayoutLibrary::SlotAsCanvasSlot(WidgetTree->FindWidget(TEXT("ButtonShadow"))) : nullptr;
	DesignShadowPosition = ShadowSlot ? ShadowSlot->GetPosition() : FVector2D(50.85f, BuildingShadowY);
	if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		CacheFeatureButtonStyles();
		static const TSoftClassPtr<URokUiPageWidget> LayoutClass(FSoftObjectPath(TEXT(
			"/Game/RokUI/Building/WBP_RokBuildingMenuLayouts.WBP_RokBuildingMenuLayouts_C")));
		if (UClass* Class = URokUiSettings::Resolve(LayoutClass))
		{
			auto* Layout = NewObject<URokUiPageWidget>(this, Class);
			if (Layout->Initialize())
			{
				const auto FindSlot = [Layout](const FString& Name)
				{
					const UWidget* Marker = Layout->Get<UWidget>(FName(*Name));
					return Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
				};
				for (const TCHAR* VariantName : {TEXT("TwoButtons"), TEXT("BuilderHut"), TEXT("Decoration")})
				{
					const FString Variant(VariantName);
					const auto* Info = FindSlot(Variant + TEXT("Info"));
					const auto* Upgrade = FindSlot(Variant + TEXT("Upgrade"));
					const auto* Feature = FindSlot(Variant + TEXT("Feature"));
					if (Info && Upgrade && Feature)
					{
						MenuVariants.Add(FName(*Variant), FMenuButtonPositions{
							Info->GetPosition(), Upgrade->GetPosition(), Feature->GetPosition()});
					}
				}
			}
		}
	}
	bDesignLayoutCached = true;
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
		Refresh();
		UpdateMenuPosition();
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
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		BuildingMenuRoot = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("BuildingMenuRoot")));
		BuildingTitleRoot = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("BuildingTitleRoot")));
		BuildingMenuSlot = BuildingMenuRoot ? Cast<UCanvasPanelSlot>(BuildingMenuRoot->Slot) : nullptr;
		TitleText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("BuildingTitleText")));
		LevelText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("BuildingLevelText")));
		LevelFrameImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("LevelFrame")));
		InfoButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BuildingInfoButton")));
		UpgradeButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BuildingUpgradeButton")));
		FeatureButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BuildingFeatureButton")));
		FeatureButtonImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("BuildingFeatureButtonImage")));
		InfoButtonSlot = InfoButton ? Cast<UCanvasPanelSlot>(InfoButton->Slot) : nullptr;
		UpgradeButtonSlot = UpgradeButton ? Cast<UCanvasPanelSlot>(UpgradeButton->Slot) : nullptr;
		FeatureButtonSlot = FeatureButton ? Cast<UCanvasPanelSlot>(FeatureButton->Slot) : nullptr;
		if (!BuildingMenuRoot || !BuildingTitleRoot || !BuildingMenuSlot || !TitleText || !LevelText
			|| !LevelFrameImage || !InfoButtonSlot || !UpgradeButtonSlot || !FeatureButtonSlot || !FeatureButtonImage)
		{
			UE_LOG(LogTemp, Error, TEXT("Rok building panel WBP is missing required widgets"));
			return;
		}
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
		InfoButton->OnClicked.AddUniqueDynamic(this, &URokBuildingPanelWidget::HandleInfoClicked);
		UpgradeButton->OnClicked.AddUniqueDynamic(this, &URokBuildingPanelWidget::HandleUpgradeClicked);
		FeatureButton->OnClicked.AddUniqueDynamic(this, &URokBuildingPanelWidget::HandleFeatureClicked);
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
#if WITH_EDITOR
	FString SpecPath;
	if (URokUiSettings::IsLegacyForced() && FParse::Value(FCommandLine::Get(), TEXT("RokUiExportBuildingPanel="), SpecPath))
	{
		FString Error;
		if (!RokUiSpec::ExportChildren(RootCanvas, SpecPath, TEXT("building_panel"), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Rok building panel spec export failed: %s"), *Error);
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("Rok building panel spec saved to %s"), *SpecPath);
		}
	}
#endif
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
	const bool bIsBuilderHut = State.Type == ERokBuildingType::BuilderHut;
	const bool bHasFeatureAction = State.bUpgradeInProgress
		// TownCenter: BuildingMenuData[129] -> openUI 1001 -> UI.s_GainEffect.
		|| State.UnityBuildingTypeId == 1
		// BuildingMenuData 106-109: harvest the selected resource building.
		|| (State.UnityBuildingTypeId >= 2 && State.UnityBuildingTypeId <= 5 && State.Level >= 1 && State.Level <= 25)
		|| bIsBuilderHut
		|| (State.UnityBuildingTypeId >= 9 && State.UnityBuildingTypeId <= 12)
		// BuildingTypeConfig[13] -> BuildingMenuData[110], levels 1..25.
		|| (State.UnityBuildingTypeId == 13 && State.Level >= 1 && State.Level <= 25)
		|| State.UnityBuildingTypeId == 14
		// BuildingTypeConfig[28] -> BuildingMenuData[137], levels 1..25.
		|| (State.UnityBuildingTypeId == 28 && State.Level >= 1 && State.Level <= 25);
	ApplyMenuLayout(bIsDecoration, bIsBuilderHut, bHasFeatureAction);
	ApplyFeatureButtonVisual(State.UnityBuildingTypeId, State.bUpgradeInProgress);
	if (LastActionMessage.Equals(TEXT("开始升级。")) && !State.bUpgradeInProgress)
	{
		LastActionMessage = TEXT("升级完成。");
	}

	TitleText->SetText(State.DisplayName);
	if (!bIsDecoration && !bIsBuilderHut)
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
	bool bIsBuilderHut,
	bool bHasFeatureAction)
{
	if (bMenuLayoutInitialized
		&& bShowingDecorationLayout == bIsDecoration
		&& bShowingBuilderHutLayout == bIsBuilderHut
		&& bShowingFeatureAction == bHasFeatureAction)
	{
		return;
	}

	bMenuLayoutInitialized = true;
	bShowingDecorationLayout = bIsDecoration;
	bShowingBuilderHutLayout = bIsBuilderHut;
	bShowingFeatureAction = bHasFeatureAction;
	// Unity BuildingTypeConfig[7] defines info and openWorkHouse, with speed-up only while building.
	const bool bShowLevelAndUpgrade = !bIsDecoration && !bIsBuilderHut;
	const ESlateVisibility LevelVisibility = !bShowLevelAndUpgrade
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
		UpgradeButton->SetVisibility(bShowLevelAndUpgrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bShowLevelAndUpgrade) UpgradeButton->SetIsEnabled(false);
	}
	if (FeatureButton)
	{
		FeatureButton->SetVisibility(bHasFeatureAction ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		FeatureButton->SetIsEnabled(bHasFeatureAction);
	}
	if (!ApplyMenuVariant(0.0))
	{
		if (InfoButtonSlot)
		{
			const double TwoButtonX = (DesignInfoPosition.X + DesignUpgradePosition.X) * 0.5;
			InfoButtonSlot->SetPosition(
				{bIsDecoration ? DesignUpgradePosition.X
					: (bHasFeatureAction && !bIsBuilderHut ? DesignInfoPosition.X : TwoButtonX), DesignInfoPosition.Y});
		}
		if (UpgradeButtonSlot)
		{
			const double TwoButtonX = (DesignUpgradePosition.X + DesignFeaturePosition.X) * 0.5;
			UpgradeButtonSlot->SetPosition({bHasFeatureAction ? DesignUpgradePosition.X : TwoButtonX, DesignUpgradePosition.Y});
		}
		if (FeatureButtonSlot)
		{
			const double TwoButtonX = (DesignUpgradePosition.X + DesignFeaturePosition.X) * 0.5;
			FeatureButtonSlot->SetPosition({bIsBuilderHut ? TwoButtonX : DesignFeaturePosition.X, DesignFeaturePosition.Y});
		}
	}
	if (UWidget* ButtonShadow = WidgetTree->FindWidget(TEXT("ButtonShadow")))
	{
		if (UCanvasPanelSlot* ButtonShadowSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ButtonShadow))
		{
			ButtonShadowSlot->SetPosition(DesignShadowPosition);
		}
	}
	if (FeatureButtonImage && bIsDecoration && bHasFeatureAction)
	{
		TryApplyFeatureButtonStyle(TEXT("FeatureRemove"), RemoveButtonTexture.Get());
	}
}

bool URokBuildingPanelWidget::ApplyMenuVariant(double RowProjectionOffset)
{
	if (!bMenuLayoutInitialized || (bShowingFeatureAction && !bShowingDecorationLayout && !bShowingBuilderHutLayout))
	{
		return false;
	}
	const FName Variant(bShowingDecorationLayout ? TEXT("Decoration")
		: bShowingBuilderHutLayout ? TEXT("BuilderHut") : TEXT("TwoButtons"));
	const FMenuButtonPositions* Positions = MenuVariants.Find(Variant);
	if (!Positions || !InfoButtonSlot || !UpgradeButtonSlot || !FeatureButtonSlot)
	{
		return false;
	}

	const FVector2D Offset(0.0, RowProjectionOffset);
	InfoButtonSlot->SetPosition(Positions->Info + Offset);
	UpgradeButtonSlot->SetPosition(Positions->Upgrade + Offset);
	FeatureButtonSlot->SetPosition(Positions->Feature + Offset);
	return true;
}

void URokBuildingPanelWidget::CacheFeatureButtonStyles()
{
	static const TSoftClassPtr<URokUiPageWidget> StyleClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Building/WBP_RokBuildingMenuFeatureStyles.WBP_RokBuildingMenuFeatureStyles_C")));
	if (UClass* Class = URokUiSettings::Resolve(StyleClass))
	{
		auto* Styles = NewObject<URokUiPageWidget>(this, Class);
		if (Styles->Initialize() && Styles->WidgetTree)
		{
			Styles->WidgetTree->ForEachWidget([this](UWidget* Widget)
			{
				if (const auto* Icon = Cast<UImage>(Widget); Icon && Icon->GetBrush().GetResourceObject())
				{
					FeatureButtonStyles.Add(Icon->GetFName(), Icon->GetBrush());
				}
			});
		}
	}
}

bool URokBuildingPanelWidget::TryApplyFeatureButtonStyle(FName StyleName, UTexture2D* FallbackTexture)
{
	if (const FSlateBrush* Style = FeatureButtonStyles.Find(StyleName); Style && Style->GetResourceObject())
	{
		SetFeatureButtonArt(Style->GetResourceObject(), static_cast<FBox2f>(Style->GetUVRegion()));
		return true;
	}
	if (FallbackTexture)
	{
		SetFeatureButtonArt(FallbackTexture);
		return true;
	}
	return false;
}

void URokBuildingPanelWidget::SetFeatureButtonArt(UObject* Resource, const FBox2f& UV)
{
	// The live button owns its dimensions, tint and drawing style through every art change.
	FSlateBrush Brush = GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
		? FeatureButtonImage->GetBrush() : MakeImageBrush(nullptr, FVector2D(88.0f));
	Brush.SetResourceObject(Resource);
	Brush.SetUVRegion(UV);
	FeatureButtonImage->SetBrush(Brush);
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
	if (UnityBuildingTypeId >= 2 && UnityBuildingTypeId <= 5 && !bUpgradeInProgress)
	{
		const FName ResourceStyles[] = {TEXT("FeatureFood"), TEXT("FeatureWood"), TEXT("FeatureStone"), TEXT("FeatureGold")};
		const int32 ResourceIndex = UnityBuildingTypeId - 2;
		if (TryApplyFeatureButtonStyle(ResourceStyles[ResourceIndex]))
		{
			return;
		}
		// Unity ui_hud.tpsheet, BuildingMenuData 106-109. Rects use bottom-left coordinates.
		const FIntPoint Origins[] = {{531, 755}, {504, 845}, {774, 935}, {441, 745}};
		const FIntPoint Origin = Origins[ResourceIndex];
		UTexture2D* Atlas = LoadUiTexture(UiBuildingMenuAtlasPath);
		const float Top = 1024.0f - Origin.Y - 88.0f;
		SetFeatureButtonArt(Atlas, FBox2f(FVector2f(Origin.X / 1024.0f, Top / 1024.0f),
			FVector2f((Origin.X + 88.0f) / 1024.0f, (Top + 88.0f) / 1024.0f)));
		if (!Atlas) FeatureButtonImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	if (UnityBuildingTypeId == 13 && !bUpgradeInProgress)
	{
		// Source btn_hud_1018; a missing import must not show another feature icon.
		if (!TryApplyFeatureButtonStyle(TEXT("FeatureResearch"), ResearchButtonTexture.Get()))
		{
			SetFeatureButtonArt(nullptr);
			FeatureButtonImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}
	if (UnityBuildingTypeId == 28 && !bUpgradeInProgress)
	{
		// Source btn_hud_1041; do not display another building's action if its import is missing.
		if (!TryApplyFeatureButtonStyle(TEXT("FeatureScout"), ScoutButtonTexture.Get()))
		{
			SetFeatureButtonArt(nullptr);
			FeatureButtonImage->SetVisibility(ESlateVisibility::Hidden);
		}
		return;
	}

	if (bUpgradeInProgress && TryApplyFeatureButtonStyle(TEXT("FeatureSpeedUp"), SpeedUpButtonTexture.Get()))
	{
		return;
	}
	FName StyleName;
	UTexture2D* Texture = nullptr;
	switch (UnityBuildingTypeId)
	{
	case 7:
		StyleName = TEXT("FeatureWorker");
		Texture = WorkerButtonTexture.Get();
		break;
	case 9:
		StyleName = TEXT("FeatureInfantry");
		Texture = InfantryButtonTexture.Get();
		break;
	case 10:
		StyleName = TEXT("FeatureCavalry");
		Texture = CavalryButtonTexture.Get();
		break;
	case 11:
		StyleName = TEXT("FeatureArcher");
		Texture = ArcherButtonTexture.Get();
		break;
	case 12:
		StyleName = TEXT("FeatureSiege");
		Texture = SiegeButtonTexture.Get();
		break;
	case 14:
		StyleName = TEXT("FeatureHospital");
		Texture = HospitalButtonTexture.Get();
		break;
	default:
		break;
	}
	if (!TryApplyFeatureButtonStyle(StyleName, Texture))
	{
		TryApplyFeatureButtonStyle(TEXT("FeatureDefault"), FeatureButtonTexture.Get());
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
		float VisualTopY = AnchorWidgetPosition.Y - DesignInfoPosition.Y;
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
			? VisualTopY - ProjectedTitleOffset
			: AnchorWidgetPosition.Y - DesignInfoPosition.Y;
		const float ButtonY = bProjectedBounds
			? VisualBottomY + ProjectedButtonGap - MenuTopY
			: DesignInfoPosition.Y;
		const double RowProjectionOffset = ButtonY - DesignInfoPosition.Y;

		BuildingMenuSlot->SetPosition(FVector2D(AnchorWidgetPosition.X, MenuTopY));
		BuildingMenuSlot->SetSize(FVector2D(
			DesignMenuSize.X,
			FMath::Max(DesignMenuSize.Y, ButtonY + ProjectedButtonRowBottomPadding)));
		if (!ApplyMenuVariant(RowProjectionOffset))
		{
			const bool bTwoButtonRow = bIsDecoration || bShowingBuilderHutLayout;
			if (InfoButtonSlot)
			{
				const double TwoButtonX = (DesignInfoPosition.X + DesignUpgradePosition.X) * 0.5;
				InfoButtonSlot->SetPosition({bTwoButtonRow ? TwoButtonX : DesignInfoPosition.X, ButtonY});
			}
			if (UpgradeButtonSlot)
			{
				UpgradeButtonSlot->SetPosition(DesignUpgradePosition + FVector2D(0.0, RowProjectionOffset));
			}
			if (FeatureButtonSlot)
			{
				const double TwoButtonX = (DesignUpgradePosition.X + DesignFeaturePosition.X) * 0.5;
				FeatureButtonSlot->SetPosition({bTwoButtonRow ? TwoButtonX : DesignFeaturePosition.X,
					DesignFeaturePosition.Y + RowProjectionOffset});
			}
		}
		if (UWidget* ButtonShadow = WidgetTree->FindWidget(TEXT("ButtonShadow")))
		{
			if (UCanvasPanelSlot* ButtonShadowSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ButtonShadow))
			{
				ButtonShadowSlot->SetPosition(FVector2D(
					DesignShadowPosition.X,
					FMath::Max(0.0, DesignShadowPosition.Y + RowProjectionOffset)));
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
