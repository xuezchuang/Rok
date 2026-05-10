#include "RokBuildingPanelWidget.h"

#include "Blueprint/WidgetTree.h"
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
#include "RokBuildingUpgradeSubsystem.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* UiUpgradePath = TEXT("/Game/RokPrototype/UI/Processed/rok_upgrade_icon.rok_upgrade_icon");
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
		LastActionMessage = TEXT("Upgrade started.");
	}
	else
	{
		LastActionMessage = FailureReason;
	}
	Refresh();
}

void URokBuildingPanelWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootBorder)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingPanelCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BuildingPanelRoot"));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(RootBorder);
	PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
	PanelSlot->SetPosition(FVector2D(-24.0f, 0.0f));
	PanelSlot->SetSize(FVector2D(330.0f, 390.0f));
	RootBorder->SetPadding(FMargin(16.0f));
	RootBorder->SetBrushColor(FLinearColor(0.045f, 0.05f, 0.045f, 0.92f));

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingPanelLayout"));
	RootBorder->SetContent(Layout);

	BuildingIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingIcon"));
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildingIconBox"));
	IconBox->SetWidthOverride(148.0f);
	IconBox->SetHeightOverride(104.0f);
	IconBox->AddChild(BuildingIcon);
	Layout->AddChildToVerticalBox(IconBox);

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.72f, 1.0f)));
	TitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 22));
	Layout->AddChildToVerticalBox(TitleText);

	TypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TypeText"));
	TypeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.84f, 0.76f, 1.0f)));
	Layout->AddChildToVerticalBox(TypeText);

	LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelText"));
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.88f, 0.82f, 1.0f)));
	Layout->AddChildToVerticalBox(LevelText);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
	CostText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.78f, 0.66f, 1.0f)));
	CostText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(CostText);

	ResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResourceText"));
	ResourceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.88f, 0.72f, 1.0f)));
	ResourceText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(ResourceText);

	QueueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QueueText"));
	QueueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.84f, 1.0f)));
	Layout->AddChildToVerticalBox(QueueText);

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.86f, 1.0f, 1.0f)));
	Layout->AddChildToVerticalBox(TimerText);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.74f, 0.48f, 1.0f)));
	StatusText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(StatusText);

	UpgradeProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("UpgradeProgress"));
	UpgradeProgress->SetFillColorAndOpacity(FLinearColor(0.87f, 0.63f, 0.22f, 1.0f));
	Layout->AddChildToVerticalBox(UpgradeProgress);

	UpgradeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("UpgradeButton"));
	UHorizontalBox* UpgradeButtonLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("UpgradeButtonLayout"));

	UImage* UpgradeIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("UpgradeIcon"));
	if (UTexture2D* UpgradeTexture = LoadUiTexture(UiUpgradePath))
	{
		UpgradeIcon->SetBrush(MakeImageBrush(UpgradeTexture, FVector2D(28.0f, 28.0f)));
	}
	else
	{
		UpgradeIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
	UHorizontalBoxSlot* UpgradeIconSlot = UpgradeButtonLayout->AddChildToHorizontalBox(UpgradeIcon);
	UpgradeIconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	UpgradeIconSlot->SetVerticalAlignment(VAlign_Center);

	UpgradeButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UpgradeButtonText"));
	UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradeButton", "Upgrade"));
	UpgradeButtonText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* UpgradeTextSlot = UpgradeButtonLayout->AddChildToHorizontalBox(UpgradeButtonText);
	UpgradeTextSlot->SetVerticalAlignment(VAlign_Center);
	UpgradeButton->AddChild(UpgradeButtonLayout);
	UpgradeButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleUpgradeClicked);
	Layout->AddChildToVerticalBox(UpgradeButton);
}

void URokBuildingPanelWidget::Refresh()
{
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem || !SelectedBuildingActor.IsValid() || !TitleText)
	{
		return;
	}

	const FRokBuildingState State = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
	const FRokResourceCost Cost = UpgradeSubsystem->GetNextUpgradeCost(SelectedBuildingActor.Get());
	const FRokResourceStock Resources = UpgradeSubsystem->GetCurrentResources();
	const float RemainingSeconds = UpgradeSubsystem->GetRemainingUpgradeSeconds(SelectedBuildingActor.Get());
	const float Progress = UpgradeSubsystem->GetUpgradeProgress(SelectedBuildingActor.Get());
	FString CannotUpgradeReason;
	const bool bCanStartUpgrade = UpgradeSubsystem->CanStartUpgrade(SelectedBuildingActor.Get(), CannotUpgradeReason);
	if (LastActionMessage.Equals(TEXT("Upgrade started.")) && !State.bUpgradeInProgress)
	{
		LastActionMessage = TEXT("Upgrade complete.");
	}

	TitleText->SetText(State.DisplayName);
	TypeText->SetText(UpgradeSubsystem->GetBuildingTypeText(State.Type));
	LevelText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "LevelFormat", "Level {0} / {1}"),
		FText::AsNumber(State.Level),
		FText::AsNumber(UpgradeSubsystem->GetMaxBuildingLevel())));
	if (State.Level >= UpgradeSubsystem->GetMaxBuildingLevel())
	{
		CostText->SetText(NSLOCTEXT("RokBuildings", "MaxLevelCost", "Max level reached"));
	}
	else
	{
		CostText->SetText(FText::Format(
			NSLOCTEXT("RokBuildings", "CostFormat", "Next: Food {0}  Wood {1}  Stone {2}  Gold {3}"),
			FText::AsNumber(Cost.Food),
			FText::AsNumber(Cost.Wood),
			FText::AsNumber(Cost.Stone),
			FText::AsNumber(Cost.Gold)));
	}
	ResourceText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "ResourceFormat", "Owned: Food {0}  Wood {1}  Stone {2}  Gold {3}"),
		FText::AsNumber(Resources.Food),
		FText::AsNumber(Resources.Wood),
		FText::AsNumber(Resources.Stone),
		FText::AsNumber(Resources.Gold)));
	QueueText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "QueueFormat", "Builders: {0} / {1}"),
		FText::AsNumber(UpgradeSubsystem->GetActiveUpgradeCount()),
		FText::AsNumber(UpgradeSubsystem->GetMaxConcurrentUpgrades())));

	if (State.bUpgradeInProgress)
	{
		TimerText->SetText(FText::Format(NSLOCTEXT("RokBuildings", "TimerFormat", "Upgrading: {0}s remaining"), FText::AsNumber(FMath::CeilToInt(RemainingSeconds))));
		UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradingButton", "Upgrading"));
		UpgradeButton->SetIsEnabled(false);
	}
	else
	{
		TimerText->SetText(NSLOCTEXT("RokBuildings", "ReadyToUpgrade", "Ready"));
		UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradeButton", "Upgrade"));
		UpgradeButton->SetIsEnabled(bCanStartUpgrade);
	}
	if (!LastActionMessage.IsEmpty())
	{
		StatusText->SetText(FText::FromString(LastActionMessage));
	}
	else if (!bCanStartUpgrade && !State.bUpgradeInProgress)
	{
		StatusText->SetText(FText::FromString(CannotUpgradeReason));
	}
	else
	{
		StatusText->SetText(FText::GetEmpty());
	}
	UpgradeProgress->SetPercent(Progress);

	UTexture2D* IconTexture = State.Icon.LoadSynchronous();
	if (IconTexture)
	{
		BuildingIcon->SetBrushFromTexture(IconTexture, true);
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
