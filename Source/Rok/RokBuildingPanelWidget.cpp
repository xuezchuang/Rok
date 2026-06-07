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
#include "Styling/CoreStyle.h"
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
	if (!WidgetTree || RootBorder)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingPanelCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BuildingPanelRoot"));
	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(RootBorder);
	PanelSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
	PanelSlot->SetPosition(FVector2D(-24.0f, -104.0f));
	PanelSlot->SetSize(FVector2D(440.0f, 238.0f));
	RootBorder->SetPadding(FMargin(14.0f));
	RootBorder->SetBrushColor(FLinearColor(0.035f, 0.041f, 0.036f, 0.93f));

	UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingPanelLayout"));
	RootBorder->SetContent(Layout);

	UHorizontalBox* HeaderLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildingPanelHeader"));
	Layout->AddChildToVerticalBox(HeaderLayout);

	BuildingIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingIcon"));
	USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildingIconBox"));
	IconBox->SetWidthOverride(132.0f);
	IconBox->SetHeightOverride(88.0f);
	IconBox->AddChild(BuildingIcon);
	UHorizontalBoxSlot* IconSlot = HeaderLayout->AddChildToHorizontalBox(IconBox);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	IconSlot->SetVerticalAlignment(VAlign_Top);

	UVerticalBox* SummaryLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingPanelSummary"));
	UHorizontalBoxSlot* SummarySlot = HeaderLayout->AddChildToHorizontalBox(SummaryLayout);
	SummarySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.72f, 1.0f)));
	TitleText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 19));
	TitleText->SetAutoWrapText(true);
	SummaryLayout->AddChildToVerticalBox(TitleText);

	TypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TypeText"));
	TypeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.8f, 0.72f, 1.0f)));
	TypeText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	TypeText->SetAutoWrapText(true);
	SummaryLayout->AddChildToVerticalBox(TypeText);

	LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelText"));
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.88f, 0.82f, 1.0f)));
	LevelText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	SummaryLayout->AddChildToVerticalBox(LevelText);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CostText"));
	CostText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.78f, 0.52f, 1.0f)));
	CostText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 13));
	CostText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(CostText);

	ResourceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResourceText"));
	ResourceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.88f, 0.72f, 1.0f)));
	ResourceText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	ResourceText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(ResourceText);

	QueueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QueueText"));
	QueueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.84f, 1.0f)));
	QueueText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	Layout->AddChildToVerticalBox(QueueText);

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.86f, 1.0f, 1.0f)));
	TimerText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	Layout->AddChildToVerticalBox(TimerText);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.74f, 0.48f, 1.0f)));
	StatusText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 12));
	StatusText->SetAutoWrapText(true);
	Layout->AddChildToVerticalBox(StatusText);

	UpgradeProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("UpgradeProgress"));
	UpgradeProgress->SetFillColorAndOpacity(FLinearColor(0.87f, 0.63f, 0.22f, 1.0f));
	USizeBox* ProgressBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("UpgradeProgressBox"));
	ProgressBox->SetHeightOverride(8.0f);
	ProgressBox->AddChild(UpgradeProgress);
	Layout->AddChildToVerticalBox(ProgressBox);

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
	UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradeButton", "升级"));
	UpgradeButtonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.72f, 1.0f)));
	UpgradeButtonText->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), 14));
	UpgradeButtonText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* UpgradeTextSlot = UpgradeButtonLayout->AddChildToHorizontalBox(UpgradeButtonText);
	UpgradeTextSlot->SetVerticalAlignment(VAlign_Center);
	UpgradeButton->AddChild(UpgradeButtonLayout);
	UpgradeButton->OnClicked.AddDynamic(this, &URokBuildingPanelWidget::HandleUpgradeClicked);

	UHorizontalBox* ActionLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuildingActionLayout"));
	Layout->AddChildToVerticalBox(ActionLayout);
	USizeBox* UpgradeButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("UpgradeButtonBox"));
	UpgradeButtonBox->SetWidthOverride(150.0f);
	UpgradeButtonBox->SetHeightOverride(34.0f);
	UpgradeButtonBox->AddChild(UpgradeButton);
	UHorizontalBoxSlot* UpgradeButtonSlot = ActionLayout->AddChildToHorizontalBox(UpgradeButtonBox);
	UpgradeButtonSlot->SetPadding(FMargin(0.0f, 7.0f, 10.0f, 0.0f));
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
	const int32 MaxBuildingLevel = UpgradeSubsystem->GetMaxBuildingLevelForActor(SelectedBuildingActor.Get());
	FString CannotUpgradeReason;
	const bool bCanStartUpgrade = UpgradeSubsystem->CanStartUpgrade(SelectedBuildingActor.Get(), CannotUpgradeReason);
	if (LastActionMessage.Equals(TEXT("开始升级。")) && !State.bUpgradeInProgress)
	{
		LastActionMessage = TEXT("升级完成。");
	}

	TitleText->SetText(State.DisplayName);
	if (State.UnityInitId > 0)
	{
		TypeText->SetText(FText::Format(
			NSLOCTEXT("RokBuildings", "UnityTypeFormatCompact", "{0} | 城内坐标 {3},{4}"),
			UpgradeSubsystem->GetBuildingTypeText(State.Type),
			FText::AsNumber(State.UnityInitId),
			FText::AsNumber(State.UnityBuildingTypeId),
			FText::AsNumber(State.UnityGridX),
			FText::AsNumber(State.UnityGridY)));
	}
	else
	{
		TypeText->SetText(UpgradeSubsystem->GetBuildingTypeText(State.Type));
	}
	LevelText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "LevelFormatCompact", "等级 {0}/{1} | 战力 {2}"),
		FText::AsNumber(State.Level),
		FText::AsNumber(MaxBuildingLevel),
		FText::AsNumber(State.Power)));
	if (State.Level >= MaxBuildingLevel)
	{
		CostText->SetText(NSLOCTEXT("RokBuildings", "MaxLevelCostCompact", "已达最高等级"));
	}
	else
	{
		CostText->SetText(FText::Format(
			NSLOCTEXT("RokBuildings", "CostFormatCompact", "升级消耗 粮{0} 木{1} 石{2} 金{3}"),
			FText::AsNumber(Cost.Food),
			FText::AsNumber(Cost.Wood),
			FText::AsNumber(Cost.Stone),
			FText::AsNumber(Cost.Gold)));
	}
	ResourceText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "ResourceFormatCompact", "库存 粮{0} 木{1} 石{2} 金{3}"),
		FText::AsNumber(Resources.Food),
		FText::AsNumber(Resources.Wood),
		FText::AsNumber(Resources.Stone),
		FText::AsNumber(Resources.Gold)));
	QueueText->SetText(FText::Format(
		NSLOCTEXT("RokBuildings", "QueueFormatCompact", "建造队列 {0}/{1}"),
		FText::AsNumber(UpgradeSubsystem->GetActiveUpgradeCount()),
		FText::AsNumber(UpgradeSubsystem->GetMaxConcurrentUpgrades())));

	if (State.bUpgradeInProgress)
	{
		TimerText->SetText(FText::Format(NSLOCTEXT("RokBuildings", "TimerFormatCompact", "升级中 {0}秒"), FText::AsNumber(FMath::CeilToInt(RemainingSeconds))));
		UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradingButton", "升级中"));
		UpgradeButton->SetIsEnabled(false);
	}
	else
	{
		TimerText->SetText(NSLOCTEXT("RokBuildings", "ReadyToUpgrade", "可升级"));
		UpgradeButtonText->SetText(NSLOCTEXT("RokBuildings", "UpgradeButton", "升级"));
		UpgradeButton->SetIsEnabled(bCanStartUpgrade);
	}
	if (!LastActionMessage.IsEmpty())
	{
		StatusText->SetText(FText::FromString(LastActionMessage));
	}
	else if (!bCanStartUpgrade && !State.bUpgradeInProgress)
	{
		if (State.Level >= MaxBuildingLevel)
		{
			StatusText->SetText(NSLOCTEXT("RokBuildings", "MaxReachedStatusCompact", "已经达到最高等级。"));
		}
		else
		{
			StatusText->SetText(FText::FromString(CannotUpgradeReason));
		}
	}
	else
	{
		StatusText->SetText(FText::GetEmpty());
	}
	UpgradeProgress->SetPercent(Progress);

	UTexture2D* IconTexture = State.Icon.LoadSynchronous();
	if (IconTexture)
	{
		BuildingIcon->SetBrush(MakeImageBrush(IconTexture, FVector2D(132.0f, 88.0f)));
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
