#include "RokMainHudWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* UiMainTopPath = TEXT("/Game/RokPrototype/UI/Processed/rok_main_top.rok_main_top");
const TCHAR* UiMainBottomPath = TEXT("/Game/RokPrototype/UI/Processed/rok_main_bottom.rok_main_bottom");
const TCHAR* UiFoodPath = TEXT("/Game/RokPrototype/UI/Processed/rok_food_icon.rok_food_icon");
const TCHAR* UiWoodPath = TEXT("/Game/RokPrototype/UI/Processed/rok_wood_icon.rok_wood_icon");
const TCHAR* UiStonePath = TEXT("/Game/RokPrototype/UI/Processed/rok_stone_icon.rok_stone_icon");
const TCHAR* UiGoldPath = TEXT("/Game/RokPrototype/UI/Processed/rok_gold_icon.rok_gold_icon");
}

void URokMainHudWidget::SetSelectedBuilding(AActor* InBuildingActor)
{
	SelectedBuildingActor = InBuildingActor;
	if (BuildingPanelWidget)
	{
		BuildingPanelWidget->SetSelectedBuilding(InBuildingActor);
	}
	RefreshFromSubsystem();
}

void URokMainHudWidget::RefreshFromSubsystem()
{
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	if (!UpgradeSubsystem || !FoodText || !WoodText || !StoneText || !GoldText || !BuilderQueueText || !StatusText)
	{
		return;
	}

	const FRokResourceStock Resources = UpgradeSubsystem->GetCurrentResources();
	FoodText->SetText(FText::Format(NSLOCTEXT("RokHud", "FoodChip", "Food {0}"), FText::AsNumber(Resources.Food)));
	WoodText->SetText(FText::Format(NSLOCTEXT("RokHud", "WoodChip", "Wood {0}"), FText::AsNumber(Resources.Wood)));
	StoneText->SetText(FText::Format(NSLOCTEXT("RokHud", "StoneChip", "Stone {0}"), FText::AsNumber(Resources.Stone)));
	GoldText->SetText(FText::Format(NSLOCTEXT("RokHud", "GoldChip", "Gold {0}"), FText::AsNumber(Resources.Gold)));

	BuilderQueueText->SetText(FText::Format(
		NSLOCTEXT("RokHud", "BuilderQueue", "Builders {0}/{1}"),
		FText::AsNumber(UpgradeSubsystem->GetActiveUpgradeCount()),
		FText::AsNumber(UpgradeSubsystem->GetMaxConcurrentUpgrades())));

	if (SelectedBuildingActor.IsValid())
	{
		const FRokBuildingState State = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
		StatusText->SetText(FText::Format(
			NSLOCTEXT("RokHud", "SelectedBuilding", "Selected: {0}  Lv.{1}"),
			State.DisplayName,
			FText::AsNumber(State.Level)));
	}
	else
	{
		StatusText->SetText(NSLOCTEXT("RokHud", "NoSelection", "Click a city building to inspect or upgrade."));
	}
}

TSharedRef<SWidget> URokMainHudWidget::RebuildWidget()
{
	BuildWidgetTree();
	UE_LOG(LogTemp, Log, TEXT("Rok UI: Main HUD widget tree rebuilt."));
	return Super::RebuildWidget();
}

void URokMainHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
	if (BuildingPanelWidget)
	{
		BuildingPanelWidget->SetSelectedBuilding(nullptr);
	}
	RefreshFromSubsystem();
}

void URokMainHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromSubsystem();
}

void URokMainHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || FoodText)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokMainHudRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UBorder* ResourceBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopResourceBar"));
	ResourceBar->SetPadding(FMargin(12.0f, 7.0f));
	ResourceBar->SetBrushColor(FLinearColor(0.035f, 0.04f, 0.035f, 0.82f));
	ApplyBorderTexture(ResourceBar, TopBarBrush, UiMainTopPath, FVector2D(520.0f, 48.0f), TEXT("rok_main_top"));
	UCanvasPanelSlot* ResourceSlot = RootCanvas->AddChildToCanvas(ResourceBar);
	ResourceSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ResourceSlot->SetPosition(FVector2D(18.0f, 14.0f));
	ResourceSlot->SetSize(FVector2D(520.0f, 48.0f));

	UHorizontalBox* TopLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopResourceLayout"));
	ResourceBar->SetContent(TopLayout);

	UTextBlock* FoodTextRaw = nullptr;
	UTextBlock* WoodTextRaw = nullptr;
	UTextBlock* StoneTextRaw = nullptr;
	UTextBlock* GoldTextRaw = nullptr;
	FoodText = FoodTextRaw;
	UBorder* FoodChip = MakeResourceChip(TEXT("FoodResourceChip"), UiFoodPath, FoodTextRaw);
	FoodText = FoodTextRaw;
	UBorder* WoodChip = MakeResourceChip(TEXT("WoodResourceChip"), UiWoodPath, WoodTextRaw);
	WoodText = WoodTextRaw;
	UBorder* StoneChip = MakeResourceChip(TEXT("StoneResourceChip"), UiStonePath, StoneTextRaw);
	StoneText = StoneTextRaw;
	UBorder* GoldChip = MakeResourceChip(TEXT("GoldResourceChip"), UiGoldPath, GoldTextRaw);
	GoldText = GoldTextRaw;

	UBorder* ResourceChips[] = { FoodChip, WoodChip, StoneChip, GoldChip };
	for (UBorder* Chip : ResourceChips)
	{
		UHorizontalBoxSlot* ChipSlot = TopLayout->AddChildToHorizontalBox(Chip);
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		ChipSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* BuilderBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BuilderQueueBar"));
	BuilderBar->SetPadding(FMargin(12.0f, 8.0f));
	BuilderBar->SetBrushColor(FLinearColor(0.025f, 0.032f, 0.032f, 0.84f));
	UCanvasPanelSlot* BuilderCanvasSlot = RootCanvas->AddChildToCanvas(BuilderBar);
	BuilderCanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	BuilderCanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	BuilderCanvasSlot->SetPosition(FVector2D(-18.0f, 14.0f));
	BuilderCanvasSlot->SetSize(FVector2D(160.0f, 40.0f));

	BuilderQueueText = MakeHudText(TEXT("BuilderQueueText"), 18, FLinearColor(0.74f, 0.9f, 1.0f, 1.0f));
	BuilderQueueText->SetJustification(ETextJustify::Center);
	BuilderBar->SetContent(BuilderQueueText);

	UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomActionBar"));
	BottomBar->SetPadding(FMargin(14.0f, 10.0f));
	BottomBar->SetBrushColor(FLinearColor(0.035f, 0.037f, 0.034f, 0.88f));
	ApplyBorderTexture(BottomBar, BottomBarBrush, UiMainBottomPath, FVector2D(560.0f, 64.0f), TEXT("rok_main_bottom"));
	UCanvasPanelSlot* BottomSlot = RootCanvas->AddChildToCanvas(BottomBar);
	BottomSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	BottomSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	BottomSlot->SetPosition(FVector2D(0.0f, -18.0f));
	BottomSlot->SetSize(FVector2D(560.0f, 64.0f));

	UHorizontalBox* BottomLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomActionLayout"));
	BottomBar->SetContent(BottomLayout);

	const FText ToolLabels[] = {
		NSLOCTEXT("RokHud", "CityTool", "City"),
		NSLOCTEXT("RokHud", "BuildTool", "Build"),
		NSLOCTEXT("RokHud", "MarchTool", "March"),
		NSLOCTEXT("RokHud", "AllianceTool", "Alliance")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ToolLabels); ++Index)
	{
		UBorder* ToolButton = MakeToolButton(*FString::Printf(TEXT("ToolButton_%d"), Index), ToolLabels[Index]);
		UHorizontalBoxSlot* ToolSlot = BottomLayout->AddChildToHorizontalBox(ToolButton);
		ToolSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		ToolSlot->SetVerticalAlignment(VAlign_Center);
	}

	StatusText = MakeHudText(TEXT("SelectionStatusText"), 14, FLinearColor(0.82f, 0.86f, 0.78f, 1.0f));
	StatusText->SetAutoWrapText(true);
	UHorizontalBoxSlot* StatusSlot = BottomLayout->AddChildToHorizontalBox(StatusText);
	StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	StatusSlot->SetVerticalAlignment(VAlign_Center);

	BuildingPanelWidget = WidgetTree->ConstructWidget<URokBuildingPanelWidget>(URokBuildingPanelWidget::StaticClass(), TEXT("BuildingPanelWidget"));
	UCanvasPanelSlot* BuildingPanelSlot = RootCanvas->AddChildToCanvas(BuildingPanelWidget);
	BuildingPanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BuildingPanelSlot->SetOffsets(FMargin(0.0f));
}

UTextBlock* URokMainHudWidget::MakeHudText(const FName& Name, int32 FontSize, const FLinearColor& Color)
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetFont(FSlateFontInfo(FCoreStyle::GetDefaultFont(), FontSize));
	return TextBlock;
}

UImage* URokMainHudWidget::MakeUiImage(const FName& Name, const TCHAR* TexturePath, const FVector2D& DesiredSize)
{
	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	UTexture2D* Texture = LoadUiTexture(TexturePath);
	if (Texture)
	{
		Image->SetBrush(MakeImageBrush(Texture, DesiredSize));
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Image;
}

UBorder* URokMainHudWidget::MakeResourceChip(const FName& Name, const TCHAR* TexturePath, UTextBlock*& OutTextBlock)
{
	UBorder* ChipFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	ChipFrame->SetPadding(FMargin(5.0f, 3.0f));
	ChipFrame->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.055f, 0.62f));

	UHorizontalBox* ChipLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%sLayout"), *Name.ToString()));
	ChipFrame->SetContent(ChipLayout);

	UImage* ResourceIcon = MakeUiImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), TexturePath, FVector2D(24.0f, 24.0f));
	UHorizontalBoxSlot* IconSlot = ChipLayout->AddChildToHorizontalBox(ResourceIcon);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
	IconSlot->SetVerticalAlignment(VAlign_Center);

	OutTextBlock = MakeHudText(*FString::Printf(TEXT("%sText"), *Name.ToString()), 15, FLinearColor(0.96f, 0.88f, 0.62f, 1.0f));
	UHorizontalBoxSlot* TextSlot = ChipLayout->AddChildToHorizontalBox(OutTextBlock);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	return ChipFrame;
}

UBorder* URokMainHudWidget::MakeToolButton(const FName& Name, const FText& Label)
{
	UBorder* ButtonFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	ButtonFrame->SetPadding(FMargin(12.0f, 7.0f));
	ButtonFrame->SetBrushColor(FLinearColor(0.13f, 0.15f, 0.115f, 0.86f));

	UTextBlock* LabelText = MakeHudText(*FString::Printf(TEXT("%sText"), *Name.ToString()), 15, FLinearColor(0.92f, 0.86f, 0.7f, 1.0f));
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Center);
	ButtonFrame->SetContent(LabelText);
	return ButtonFrame;
}

UTexture2D* URokMainHudWidget::LoadUiTexture(const TCHAR* TexturePath) const
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: Missing reference texture %s"), TexturePath);
	}
	return Texture;
}

FSlateBrush URokMainHudWidget::MakeImageBrush(UTexture2D* Texture, const FVector2D& DesiredSize) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = DesiredSize;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	return Brush;
}

bool URokMainHudWidget::ApplyBorderTexture(UBorder* Border, FSlateBrush& BrushStorage, const TCHAR* TexturePath, const FVector2D& DesiredSize, const TCHAR* DebugName)
{
	UTexture2D* Texture = LoadUiTexture(TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: Falling back to solid border for %s"), DebugName);
		return false;
	}

	BrushStorage = MakeImageBrush(Texture, DesiredSize);
	Border->SetBrush(BrushStorage);
	return true;
}

URokBuildingUpgradeSubsystem* URokMainHudWidget::GetUpgradeSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
}
