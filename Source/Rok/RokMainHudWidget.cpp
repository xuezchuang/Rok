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
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "RokBuildingPanelWidget.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokUnityUiCatalogSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
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
const TCHAR* UiIconCityPath = TEXT("/Game/RokPrototype/UI/UnityMainInterface/rok_umh_ui_mainmenu_icon_mmu_1000_sprite.rok_umh_ui_mainmenu_icon_mmu_1000_sprite");
const TCHAR* UiIconBuildPath = TEXT("/Game/RokPrototype/UI/UnityMainInterface/rok_umh_ui_mainmenu_img_mmu_building_sprite.rok_umh_ui_mainmenu_img_mmu_building_sprite");
const TCHAR* UiIconMarchPath = TEXT("/Game/RokPrototype/UI/UnityMainInterface/rok_umh_ui_hud_btn_hud_1023_sprite.rok_umh_ui_hud_btn_hud_1023_sprite");
const TCHAR* UiIconAlliancePath = TEXT("/Game/RokPrototype/UI/UnityMainInterface/rok_umh_ui_alliance_icon_ali_1002_sprite.rok_umh_ui_alliance_icon_ali_1002_sprite");
const TCHAR* UiIconMailPath = TEXT("/Game/RokPrototype/UI/UnityMainInterface/rok_umh_ui_mail_icon_mail_1004_sprite.rok_umh_ui_mail_icon_mail_1004_sprite");
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
	FoodText->SetText(FText::AsNumber(Resources.Food));
	WoodText->SetText(FText::AsNumber(Resources.Wood));
	StoneText->SetText(FText::AsNumber(Resources.Stone));
	GoldText->SetText(FText::AsNumber(Resources.Gold));

	BuilderQueueText->SetText(FText::Format(
		NSLOCTEXT("RokHud", "BuilderQueueShort", "{0}/{1}"),
		FText::AsNumber(UpgradeSubsystem->GetActiveUpgradeCount()),
		FText::AsNumber(UpgradeSubsystem->GetMaxConcurrentUpgrades())));

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		FString CatalogStatus = TEXT("数据加载中");
		const URokUnityGameplayCatalogSubsystem* CatalogSubsystem = GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
		if (CatalogStatusText && CatalogSubsystem && CatalogSubsystem->IsCatalogLoaded())
		{
			const FRokUnityGameplayCatalogSummary CatalogSummary = CatalogSubsystem->GetSummary();
			CatalogStatus = FText::Format(
				NSLOCTEXT("RokHud", "CatalogStatus", "数据 建筑{0} 兵种{1} 英雄{2} 任务{3}"),
				FText::AsNumber(CatalogSummary.BuildingTypeCount),
				FText::AsNumber(CatalogSummary.TroopCount),
				FText::AsNumber(CatalogSummary.HeroCount),
				FText::AsNumber(CatalogSummary.MainTaskCount)).ToString();
		}

		const URokUnityUiCatalogSubsystem* UiCatalogSubsystem = GameInstance->GetSubsystem<URokUnityUiCatalogSubsystem>();
		if (UiCatalogSubsystem && UiCatalogSubsystem->IsUiCatalogLoaded())
		{
			CatalogStatus += TEXT(" | ");
			CatalogStatus += UiCatalogSubsystem->GetUiCatalogStatusText().ToString();
		}

		if (CatalogStatusText)
		{
			CatalogStatusText->SetText(FText::FromString(CatalogStatus));
		}

		const URokUnityRuntimeStateSubsystem* RuntimeStateSubsystem = GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
		if (RuntimeStateSubsystem && RuntimeStateSubsystem->IsRuntimeStateLoaded())
		{
			const FRokUnityRuntimeTaskState ActiveTask = RuntimeStateSubsystem->GetActiveTask();
			const FRokUnityRuntimeSummary RuntimeSummary = RuntimeStateSubsystem->GetSummary();
			if (TaskTitleText)
			{
				const FString TaskLabel = ActiveTask.Name.IsEmpty()
					? FString::Printf(TEXT("Task %d"), ActiveTask.TaskId)
					: ActiveTask.Name;
				TaskTitleText->SetText(FText::FromString(TaskLabel));
			}
			if (TaskProgressText)
			{
				const FString TaskDesc = ActiveTask.Desc.IsEmpty() ? ActiveTask.ProgressRule : ActiveTask.Desc;
				TaskProgressText->SetText(FText::Format(
					NSLOCTEXT("RokHud", "TaskProgress", "{0}  {1}/{2}"),
					FText::FromString(TaskDesc),
					FText::AsNumber(ActiveTask.Current),
					FText::AsNumber(ActiveTask.Target)));
			}
			if (ArmySummaryText)
			{
				ArmySummaryText->SetText(FText::Format(
					NSLOCTEXT("RokHud", "ArmySummary", "Heroes {0} | Troops {1} | Train {2}"),
					FText::AsNumber(RuntimeSummary.StarterHeroCount),
					FText::AsNumber(RuntimeSummary.TotalTroopCount),
					FText::AsNumber(RuntimeSummary.TrainableTroopCount)));
			}
		}
	}

	if (SelectedBuildingActor.IsValid())
	{
		const FRokBuildingState State = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
		const bool bIsDecoration = State.UnityBuildingTypeId >= 51 && State.UnityBuildingTypeId <= 54;
		StatusText->SetText(bIsDecoration
			? State.DisplayName
			: FText::Format(
				NSLOCTEXT("RokHud", "SelectedBuilding", "{0}  Lv.{1}"),
				State.DisplayName,
				FText::AsNumber(State.Level)));
	}
	else
	{
		StatusText->SetText(NSLOCTEXT("RokHud", "NoSelectionShort", "选择城内建筑"));
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

	UBorder* ResourceBar = MakePanelFrame(TEXT("TopResourceBar"), FLinearColor(0.035f, 0.04f, 0.035f, 0.86f), FMargin(14.0f, 7.0f));
	ApplyBorderTexture(ResourceBar, TopBarBrush, UiMainTopPath, FVector2D(760.0f, 50.0f), TEXT("rok_main_top"));
	UCanvasPanelSlot* ResourceSlot = RootCanvas->AddChildToCanvas(ResourceBar);
	ResourceSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	ResourceSlot->SetPosition(FVector2D(18.0f, 14.0f));
	ResourceSlot->SetSize(FVector2D(760.0f, 50.0f));

	UHorizontalBox* TopLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopResourceLayout"));
	ResourceBar->SetContent(TopLayout);

	UTextBlock* FoodTextRaw = nullptr;
	UTextBlock* WoodTextRaw = nullptr;
	UTextBlock* StoneTextRaw = nullptr;
	UTextBlock* GoldTextRaw = nullptr;
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
		ChipSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		ChipSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* BuilderBar = MakePanelFrame(TEXT("BuilderQueueBar"), FLinearColor(0.028f, 0.036f, 0.033f, 0.88f), FMargin(12.0f, 9.0f));
	UCanvasPanelSlot* BuilderCanvasSlot = RootCanvas->AddChildToCanvas(BuilderBar);
	BuilderCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	BuilderCanvasSlot->SetPosition(FVector2D(24.0f, 76.0f));
	BuilderCanvasSlot->SetSize(FVector2D(132.0f, 70.0f));

	UVerticalBox* BuilderLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuilderQueueLayout"));
	BuilderBar->SetContent(BuilderLayout);
	UTextBlock* BuilderLabelText = MakeHudText(TEXT("BuilderLabelText"), 12, FLinearColor(0.72f, 0.78f, 0.68f, 1.0f));
	BuilderLabelText->SetText(NSLOCTEXT("RokHud", "BuilderLabel", "建造队列"));
	BuilderLabelText->SetJustification(ETextJustify::Center);
	BuilderLayout->AddChildToVerticalBox(BuilderLabelText);
	BuilderQueueText = MakeHudText(TEXT("BuilderQueueText"), 24, FLinearColor(0.82f, 0.94f, 1.0f, 1.0f));
	BuilderQueueText->SetJustification(ETextJustify::Center);
	BuilderLayout->AddChildToVerticalBox(BuilderQueueText);

	UBorder* TaskPanel = MakePanelFrame(TEXT("MainTaskPanel"), FLinearColor(0.035f, 0.042f, 0.039f, 0.86f), FMargin(12.0f, 10.0f));
	UCanvasPanelSlot* TaskSlot = RootCanvas->AddChildToCanvas(TaskPanel);
	TaskSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	TaskSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	TaskSlot->SetPosition(FVector2D(-22.0f, 72.0f));
	TaskSlot->SetSize(FVector2D(332.0f, 118.0f));

	UVerticalBox* TaskLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainTaskLayout"));
	TaskPanel->SetContent(TaskLayout);
	TaskTitleText = MakeHudText(TEXT("TaskTitleText"), 17, FLinearColor(0.97f, 0.87f, 0.58f, 1.0f));
	TaskTitleText->SetText(NSLOCTEXT("RokHud", "TaskPlaceholder", "Main Task"));
	TaskLayout->AddChildToVerticalBox(TaskTitleText);
	TaskProgressText = MakeHudText(TEXT("TaskProgressText"), 13, FLinearColor(0.86f, 0.9f, 0.82f, 1.0f));
	TaskProgressText->SetAutoWrapText(true);
	TaskProgressText->SetText(NSLOCTEXT("RokHud", "TaskProgressPlaceholder", "Runtime task state pending"));
	TaskLayout->AddChildToVerticalBox(TaskProgressText);
	ArmySummaryText = MakeHudText(TEXT("ArmySummaryText"), 12, FLinearColor(0.68f, 0.82f, 0.95f, 1.0f));
	ArmySummaryText->SetText(NSLOCTEXT("RokHud", "ArmySummaryPlaceholder", "Heroes 0 | Troops 0"));
	TaskLayout->AddChildToVerticalBox(ArmySummaryText);

	UBorder* CatalogPanel = MakePanelFrame(TEXT("CatalogStatusPanel"), FLinearColor(0.018f, 0.024f, 0.026f, 0.72f), FMargin(10.0f, 6.0f));
	UCanvasPanelSlot* CatalogSlot = RootCanvas->AddChildToCanvas(CatalogPanel);
	CatalogSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	CatalogSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	CatalogSlot->SetPosition(FVector2D(-22.0f, 20.0f));
	CatalogSlot->SetSize(FVector2D(410.0f, 36.0f));
	CatalogStatusText = MakeHudText(TEXT("CatalogStatusText"), 13, FLinearColor(0.7f, 0.88f, 0.95f, 1.0f));
	CatalogStatusText->SetJustification(ETextJustify::Center);
	CatalogStatusText->SetText(NSLOCTEXT("RokHud", "CatalogStatusPending", "Catalog pending"));
	CatalogPanel->SetContent(CatalogStatusText);

	UBorder* BottomBar = MakePanelFrame(TEXT("BottomActionBar"), FLinearColor(0.035f, 0.037f, 0.034f, 0.9f), FMargin(14.0f, 9.0f));
	ApplyBorderTexture(BottomBar, BottomBarBrush, UiMainBottomPath, FVector2D(620.0f, 66.0f), TEXT("rok_main_bottom"));
	UCanvasPanelSlot* BottomSlot = RootCanvas->AddChildToCanvas(BottomBar);
	BottomSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
	BottomSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	BottomSlot->SetPosition(FVector2D(0.0f, -18.0f));
	BottomSlot->SetSize(FVector2D(620.0f, 66.0f));

	UHorizontalBox* BottomLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomActionLayout"));
	BottomBar->SetContent(BottomLayout);

	const FText ToolLabels[] = {
		NSLOCTEXT("RokHud", "CityTool", "主城"),
		NSLOCTEXT("RokHud", "BuildTool", "建造"),
		NSLOCTEXT("RokHud", "MarchTool", "行军"),
		NSLOCTEXT("RokHud", "AllianceTool", "联盟"),
		NSLOCTEXT("RokHud", "MailTool", "邮件")
	};
	const TCHAR* ToolIcons[] = {
		UiIconCityPath,
		UiIconBuildPath,
		UiIconMarchPath,
		UiIconAlliancePath,
		UiIconMailPath
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ToolLabels); ++Index)
	{
		UBorder* ToolButton = MakeToolButton(*FString::Printf(TEXT("ToolButton_%d"), Index), ToolLabels[Index], ToolIcons[Index]);
		UHorizontalBoxSlot* ToolSlot = BottomLayout->AddChildToHorizontalBox(ToolButton);
		ToolSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		ToolSlot->SetVerticalAlignment(VAlign_Center);
	}

	StatusText = MakeHudText(TEXT("SelectionStatusText"), 13, FLinearColor(0.82f, 0.86f, 0.78f, 1.0f));
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
	TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
	TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
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
	ChipFrame->SetPadding(FMargin(7.0f, 4.0f));
	ChipFrame->SetBrushColor(FLinearColor(0.07f, 0.072f, 0.05f, 0.68f));

	UHorizontalBox* ChipLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%sLayout"), *Name.ToString()));
	ChipFrame->SetContent(ChipLayout);

	UImage* ResourceIcon = MakeUiImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), TexturePath, FVector2D(28.0f, 28.0f));
	UHorizontalBoxSlot* IconSlot = ChipLayout->AddChildToHorizontalBox(ResourceIcon);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
	IconSlot->SetVerticalAlignment(VAlign_Center);

	OutTextBlock = MakeHudText(*FString::Printf(TEXT("%sText"), *Name.ToString()), 17, FLinearColor(0.96f, 0.88f, 0.62f, 1.0f));
	OutTextBlock->SetMinDesiredWidth(72.0f);
	UHorizontalBoxSlot* TextSlot = ChipLayout->AddChildToHorizontalBox(OutTextBlock);
	TextSlot->SetVerticalAlignment(VAlign_Center);
	return ChipFrame;
}

UBorder* URokMainHudWidget::MakeToolButton(const FName& Name, const FText& Label, const TCHAR* TexturePath)
{
	UBorder* ButtonFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	ButtonFrame->SetPadding(FMargin(9.0f, 6.0f));
	ButtonFrame->SetBrushColor(FLinearColor(0.12f, 0.14f, 0.105f, 0.86f));

	UHorizontalBox* ButtonLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%sLayout"), *Name.ToString()));
	ButtonFrame->SetContent(ButtonLayout);

	UImage* Icon = MakeUiImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), TexturePath, FVector2D(22.0f, 22.0f));
	UHorizontalBoxSlot* IconSlot = ButtonLayout->AddChildToHorizontalBox(Icon);
	IconSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
	IconSlot->SetVerticalAlignment(VAlign_Center);

	UTextBlock* LabelText = MakeHudText(*FString::Printf(TEXT("%sText"), *Name.ToString()), 14, FLinearColor(0.92f, 0.86f, 0.7f, 1.0f));
	LabelText->SetText(Label);
	LabelText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* LabelSlot = ButtonLayout->AddChildToHorizontalBox(LabelText);
	LabelSlot->SetVerticalAlignment(VAlign_Center);
	return ButtonFrame;
}

UBorder* URokMainHudWidget::MakePanelFrame(const FName& Name, const FLinearColor& Color, const FMargin& InPadding)
{
	UBorder* PanelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	PanelFrame->SetPadding(InPadding);
	PanelFrame->SetBrushColor(Color);
	return PanelFrame;
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
