#include "RokWorldMapHudWidget.h"
#include "RokDataPaths.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "RokUiTextTable.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UObject/UnrealType.h"
#include "RokGameNetSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokServerArmySubsystem.h"
#include "RokUiAnimatedButton.h"
#include "RokUiTextBlock.h"
#include "RokUiWidgets.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapEntitySubsystem.h"
#include "RokWorldMapGameMode.h"
#include "RokWorldMapPlayerController.h"

namespace
{
const TCHAR* Common = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* HeroUi = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui.rok_commander_ui");
const TCHAR* MapUi = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map.rok_hud_atlas_map");
const TCHAR* MapIconUi = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map_icon.rok_hud_atlas_map_icon");
const FIntPoint MapIconUiSize(1024, 2048);
const FIntPoint MapUiSize(1024, 512);
const FIntPoint CommonSize(2048, 1024);
const FIntPoint HeroUiSize(2048, 512);
// RS.green / troop_line_mine_end from the Unity map presentation.
const FLinearColor AttackRouteGreen = FLinearColor(FColor(68, 212, 130));

bool ClipAttackRouteAxis(double Origin, double Direction, double Limit, double& First, double& Last)
{
	if (FMath::IsNearlyZero(Direction)) return Origin >= 0 && Origin <= Limit;
	const double A = -Origin / Direction;
	const double B = (Limit - Origin) / Direction;
	First = FMath::Max(First, FMath::Min(A, B));
	Last = FMath::Min(Last, FMath::Max(A, B));
	return First < Last;
}

constexpr float DispatchDxf = 2994.0f; // CameraParamLod.FTE_Scout, UI_Pop_ArmySelectMediator.InitData.

bool GroundAtScreen(const APlayerController& Controller, FVector2D Screen, double GroundZ, FVector& Out)
{
	FVector Origin, Direction;
	if (!Controller.DeprojectScreenPositionToWorld(Screen.X, Screen.Y, Origin, Direction)
		|| FMath::IsNearlyZero(Direction.Z)) return false;
	Out = Origin + Direction * ((GroundZ - Origin.Z) / Direction.Z);
	return true;
}
}


void URokWorldMapHudWidget::BuildWorldGatherProgress(UCanvasPanel* Panel)
{
	// Unity UI_Item_WorldObjInfoTCollect/pb_bar (progress, resource icon, count).
	WorldGatherProgressRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldGatherProgress"));
	WorldGatherProgressRoot->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Panel, WorldGatherProgressRoot, {23, 201}, {425, 32});

	WorldGatherProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("WorldGatherProgressBar"));
	FProgressBarStyle Style;
	Style.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.12f, 0.13f, 0.14f, 0.38f)));
	Style.SetFillImage(FSlateColorBrush(FLinearColor(0.25f, 0.56f, 0.22f, 1.0f)));
	WorldGatherProgressBar->SetWidgetStyle(Style);
	WorldGatherProgressBar->SetBorderPadding(FVector2D::ZeroVector);
	PlaceOnCanvas(WorldGatherProgressRoot, WorldGatherProgressBar, {26, 3}, {401, 23});

	WorldGatherProgressIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldGatherProgressIcon"));
	PlaceOnCanvas(WorldGatherProgressRoot, WorldGatherProgressIcon, {-14, -15}, {60, 60});
	WorldGatherProgressText = MakeHudText(TEXT("WorldGatherProgressAmount"), 16, FLinearColor::White);
	WorldGatherProgressText->SetFont(URokMainHudWidget::MakeUnitySourceFont(16, false));
	WorldGatherProgressText->SetShadowOffset(FVector2D::ZeroVector);
	WorldGatherProgressText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(WorldGatherProgressRoot, WorldGatherProgressText, {26, 4}, {401, 23});
}

void URokWorldMapHudWidget::BuildWorldMarchQueue()
{
	AttackRouteDestinationTexture = LoadTransientUiTexture(RokDataPaths::Resolve(TEXT("WorldMarch/troop_line_mine_end.png")));
	static const TSoftClassPtr<URokUiPageWidget> MarchPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldMarchQueue.WBP_RokWorldMarchQueue_C")));
	if (auto* Page = AddWorldHudPage(ExpandedDesignCanvas, MarchPage, TEXT("WorldMarchQueuePage"), 85))
	{
		BindWorldMarchQueue(*Page);
		return;
	}
	// Unity MainInterfaceMediator.RefreshTroopsDispatchNum and UI_Item_MainIFArm.
	WorldMarchQueueRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldMarchQueue"));
	WorldMarchQueueRoot->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ExpandedDesignCanvas, WorldMarchQueueRoot, {-126, 192}, {102, 120}, FAnchors(1, 0))->SetZOrder(85);

	auto* Shade = WidgetTree->ConstructWidget<UImage>();
	Shade->SetBrush(FSlateColorBrush(FLinearColor(0.012f, 0.049f, 0.078f, 0.74f)));
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(WorldMarchQueueRoot, Shade, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0, 0, 1, 1));

	auto* Toggle = MakeTransparentHitButton(TEXT("WorldMarchQueueToggle"));
	Toggle->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleWorldMarchQueueToggle);
	PlaceOnCanvas(WorldMarchQueueRoot, Toggle, {0, 0}, {102, 31});
	WorldMarchQueueArrow = MakeAtlasImage(TEXT("WorldMarchQueueArrow"),
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
		FIntRect(75, 181, 100, 215), {1024, 1024}, {20, 28});
	PlaceOnCanvas(WorldMarchQueueRoot, WorldMarchQueueArrow, {4, 1}, {20, 28});
	WorldMarchQueueCount = MakeHudText(TEXT("WorldMarchQueueCount"), 18, FLinearColor::White);
	WorldMarchQueueCount->SetFont(URokMainHudWidget::MakeUnitySourceFont(18, false));
	WorldMarchQueueCount->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(WorldMarchQueueRoot, WorldMarchQueueCount, {29, 4}, {45, 24});

	WorldMarchQueueList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldMarchQueueList"));
	WorldMarchQueueList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	WorldMarchQueueList->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	PlaceOnCanvas(WorldMarchQueueRoot, WorldMarchQueueList, {0, 31}, FVector2D::ZeroVector, FAnchors(0, 0, 1, 1));
}

void URokWorldMapHudWidget::BindWorldMarchQueue(URokUiPageWidget& Page)
{
	WorldMarchQueueRoot = Page.Get<UCanvasPanel>(TEXT("WorldMarchQueue"));
	WorldMarchQueueList = Page.Get<UScrollBox>(TEXT("WorldMarchQueueList"));
	WorldMarchQueueArrow = Page.Get<UImage>(TEXT("WorldMarchQueueArrow"));
	WorldMarchQueueCount = Page.Get<UTextBlock>(TEXT("WorldMarchQueueCount"));
	static const TSoftClassPtr<URokUiPageWidget> ListLayoutClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudListLayout.WBP_RokHudListLayout_C")));
	if (auto* Layout = CreateWorldHudPage(ListLayoutClass, NAME_None))
	{
		if (const auto* Row = Layout->Get<UCanvasPanel>(TEXT("WorldMarchRow")))
		{
			if (const auto* RowLayoutSlot = Cast<UCanvasPanelSlot>(Row->Slot))
			{
				WorldMarchRowHeight = RowLayoutSlot->GetSize().Y;
			}
		}
		if (const auto* Viewport = Layout->Get<UCanvasPanel>(TEXT("WorldMarchViewport")))
		{
			if (const auto* ViewportLayoutSlot = Cast<UCanvasPanelSlot>(Viewport->Slot))
			{
				WorldMarchViewportHeight = ViewportLayoutSlot->GetSize().Y;
			}
		}
	}
	if (WorldMarchQueueArrow)
	{
		WorldMarchQueueArrowClosedAngle = WorldMarchQueueArrow->GetRenderTransformAngle();
	}
	BindWorldHudButton(Page, TEXT("WorldMarchQueueToggle"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleWorldMarchQueueToggle));
}

void URokWorldMapHudWidget::HandleWorldMarchQueueToggle()
{
	bWorldMarchQueueExpanded = !bWorldMarchQueueExpanded;
	RefreshWorldMarchQueue();
}

URokWorldMapHudWidget::FWorldMarchRowLayout URokWorldMapHudWidget::BuildWorldMarchRowLayout(
	UCanvasPanel* Row, const FString& Suffix)
{
	static const TSoftClassPtr<URokUiPageWidget> RowClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldMarchRow.WBP_RokWorldMarchRow_C")));
	if (auto* Page = AddWorldHudPage(Row, RowClass, NAME_None))
	{
		const FWorldMarchRowLayout Layout{
			Page->Get<UImage>(TEXT("WorldMarchRarityFrame")),
			Page->Get<UImage>(TEXT("WorldMarchPortrait")),
			Page->Get<UImage>(TEXT("WorldMarchSelectionFrame")),
			Page->Get<UImage>(TEXT("WorldMarchStatusIcon"))
		};
		if (Layout.RarityFrame && Layout.Portrait && Layout.SelectionFrame && Layout.Status)
		{
			// Preserve the desired size of the artwork in the centered button content slot.
			CastChecked<UCanvasPanelSlot>(Page->Slot)->SetAutoSize(true);
			return Layout;
		}
		Page->RemoveFromParent();
	}

	const auto Name = [this, &Suffix](const TCHAR* Stem)
	{
		return Suffix.IsEmpty() ? FName(Stem)
			: MakeUniqueObjectName(WidgetTree, UImage::StaticClass(), FName(*(FString(Stem) + Suffix)));
	};
	FWorldMarchRowLayout Layout;
	Layout.RarityFrame = MakeAtlasImage(Name(TEXT("WorldMarchRarityFrame")), HeroUi,
		FIntRect(1, 53, 135, 205), HeroUiSize, {78, 86});
	PlaceOnCanvas(Row, Layout.RarityFrame, {12, 0}, {78, 86});
	Layout.Portrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("WorldMarchPortrait")));
	FSlateBrush PortraitBrush;
	PortraitBrush.ImageType = ESlateBrushImageType::FullColor;
	PortraitBrush.ImageSize = FVector2D(73, 81);
	Layout.Portrait->SetBrush(PortraitBrush);
	Layout.Portrait->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Row, Layout.Portrait, {14.5, 2}, {73, 81});
	Layout.SelectionFrame = MakeAtlasImage(Name(TEXT("WorldMarchSelectionFrame")), HeroUi,
		FIntRect(1905, 356, 2046, 511), HeroUiSize, {86, 90});
	PlaceOnCanvas(Row, Layout.SelectionFrame, {8, -3}, {86, 90});
	Layout.Status = MakeAtlasImage(Name(TEXT("WorldMarchStatusIcon")), MapUi,
		FIntRect(305, 120, 351, 166), MapUiSize, {29, 29});
	PlaceOnCanvas(Row, Layout.Status, {70, 58}, {29, 29});
	return Layout;
}

void URokWorldMapHudWidget::RefreshWorldMarchQueue()
{
	if (!WorldMarchQueueRoot || !WorldMarchQueueList || !WorldMarchQueueArrow || !WorldMarchQueueCount) return;
	const auto* Instance = GetGameInstance();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Catalog = Instance ? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	const auto Snapshot = Army ? Army->GetSnapshot() : FRokServerArmySnapshot();
	AttackMarchSampleTime = FPlatformTime::Seconds();
	AttackMarchRoutes.Reset();
	for (const auto& March : Snapshot.Marches)
	{
		if (!March.AttackPath.IsEmpty()) AttackMarchRoutes.Add(March);
	}
	UpdateAttackMarchMarkers(Snapshot);
	if (!Snapshot.bAvailable || Snapshot.Marches.IsEmpty())
	{
		WorldMarchQueueRoot->SetVisibility(ESlateVisibility::Collapsed);
		WorldMarchQueueSignature.Reset();
		WorldMarchQueueList->ClearChildren();
		SelectedMarchArmyIndex = 0;
		return;
	}

	WorldMarchQueueRoot->SetVisibility(bTacticalPresentation ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	WorldMarchQueueList->SetVisibility(bWorldMarchQueueExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	WorldMarchQueueArrow->SetRenderTransformAngle(WorldMarchQueueArrowClosedAngle + (bWorldMarchQueueExpanded ? 180.0f : 0.0f));
	if (auto* QueueSlot = Cast<UCanvasPanelSlot>(WorldMarchQueueRoot->Slot))
	{
		const auto* ListSlot = Cast<UCanvasPanelSlot>(WorldMarchQueueList->Slot);
		const double HeaderHeight = ListSlot ? ListSlot->GetOffsets().Top : 31.0;
		QueueSlot->SetSize({QueueSlot->GetSize().X,
			HeaderHeight + (bWorldMarchQueueExpanded
				? FMath::Min(WorldMarchViewportHeight, WorldMarchRowHeight * Snapshot.Marches.Num()) : 0.0)});
	}

	int32 HighlightedArmy = SelectedMarchArmyIndex;
	if (IsWorldObjectInfoOpen())
	{
		int64 TargetId = 0;
		if (LexTryParseString(TargetId, *SelectedWorldObjectServerId) && TargetId > 0)
		{
			const auto* Match = Snapshot.Marches.FindByPredicate([TargetId](const FRokServerMarch& March)
			{
				return March.Phase == ERokServerMarchPhase::Collecting && March.TargetObjectId == TargetId;
			});
			if (Match) HighlightedArmy = Match->ArmyIndex;
		}
	}
	const FString Signature = FString::Printf(TEXT("%s/%d"), *Snapshot.Signature, HighlightedArmy);
	if (WorldMarchQueueSignature == Signature) return;
	WorldMarchQueueSignature = Signature;
	WorldMarchQueueCount->SetText(FText::FromString(FString::Printf(TEXT("%s/%s"),
		Snapshot.bDispatchUsedKnown ? *LexToString(Snapshot.DispatchSlotsUsed) : TEXT("—"),
		Snapshot.bDispatchCapacityKnown ? *LexToString(Snapshot.DispatchSlotsMax) : TEXT("—"))));

	constexpr TCHAR HeadAtlas[] = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head.rok_commander_head");
	const FIntPoint HeadAtlasSize(1024, 2048);
	WorldMarchQueueList->ClearChildren();
	for (const FRokServerMarch& March : Snapshot.Marches)
	{
		const FString Suffix = LexToString(March.ArmyIndex);
		auto* RowSize = WidgetTree->ConstructWidget<USizeBox>();
		RowSize->SetHeightOverride(WorldMarchRowHeight);
		WorldMarchQueueList->AddChild(RowSize);
		URokUnityIndexedButton* Button = nullptr;
		FWorldMarchRowLayout Layout;
		static const TSoftClassPtr<URokUiPageWidget> EntryClass(FSoftObjectPath(
			TEXT("/Game/RokUI/Hud/WBP_RokWorldMarchEntry.WBP_RokWorldMarchEntry_C")));
		auto* Entry = CreateWorldHudPage(EntryClass, NAME_None);
		if (Entry)
		{
			Button = Entry->Get<URokUnityIndexedButton>(TEXT("WorldMarchEntryAction"));
			Layout = {
				Entry->Get<UImage>(TEXT("WorldMarchRarityFrame")),
				Entry->Get<UImage>(TEXT("WorldMarchPortrait")),
				Entry->Get<UImage>(TEXT("WorldMarchSelectionFrame")),
				Entry->Get<UImage>(TEXT("WorldMarchStatusIcon"))
			};
			if (Button && Cast<UCanvasPanelSlot>(Button->Slot)
				&& Layout.RarityFrame && Layout.Portrait && Layout.SelectionFrame && Layout.Status)
			{
				Entry->LegacyNames.Add(TEXT("WorldMarchEntryAction"), TEXT("WorldMarchArmy_") + Suffix);
				RowSize->SetContent(Entry);
			}
			else
			{
				Entry->RemoveFromParent();
				Entry = nullptr;
			}
		}
		if (!Entry)
		{
			Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(),
				MakeUniqueObjectName(WidgetTree, URokUnityIndexedButton::StaticClass(), FName(*(TEXT("WorldMarchArmy_") + Suffix))));
			FSlateBrush NoBrush;
			NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
			FButtonStyle ButtonStyle;
			ButtonStyle.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush)
				.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
			Button->SetStyle(ButtonStyle);
			RowSize->SetContent(Button);

			auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>();
			Row->SetVisibility(ESlateVisibility::HitTestInvisible);
			Button->SetContent(Row);
			Layout = BuildWorldMarchRowLayout(Row, TEXT("_") + Suffix);
		}
		Button->ConfigureAction(March.ArmyIndex);
		Button->OnIndexedClicked.AddUObject(this, &URokWorldMapHudWidget::HandleWorldMarchClicked);
		const auto SetAtlas = [this](UImage* Image, const TCHAR* TexturePath, const FIntRect& Rect, const FIntPoint& AtlasSize)
		{
			UTexture2D* Texture = LoadUiTexture(TexturePath);
			if (Texture)
			{
				FSlateBrush Brush = Image->GetBrush();
				const FSlateBrush Atlas = MakeAtlasBrush(Texture, Rect, AtlasSize, Brush.ImageSize);
				Brush.SetResourceObject(Texture);
				Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
				Image->SetBrush(Brush);
			}
			Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		};

		FRokUnityCatalogHero Hero;
		const bool bHasHero = Catalog && Catalog->GetHero(March.MainHeroId, Hero);
		const bool bHeadAtlasFrame = Hero.Rare == 4 || Hero.Rare == 5;
		const FIntRect Frame = Hero.Rare == 5 ? FIntRect(593, 1895, 727, 2047)
			: Hero.Rare == 4 ? FIntRect(729, 1895, 863, 2047) : FIntRect(1, 53, 135, 205);
		SetAtlas(Layout.RarityFrame, bHeadAtlasFrame ? HeadAtlas : HeroUi, Frame,
			bHeadAtlasFrame ? HeadAtlasSize : HeroUiSize);
		const FLinearColor FrameTint = Layout.RarityFrame->GetColorAndOpacity();
		if (Hero.Rare == 3) Layout.RarityFrame->SetColorAndOpacity(FrameTint * FLinearColor(0.55f, 0.78f, 1.0f));
		if (Hero.Rare == 2) Layout.RarityFrame->SetColorAndOpacity(FrameTint * FLinearColor(0.62f, 1.0f, 0.70f));
		if (bHasHero && Hero.IconAtlas == TEXT("hero_head") && Hero.IconWidth > 0 && Hero.IconHeight > 0)
		{
			SetAtlas(Layout.Portrait, HeadAtlas,
				FIntRect(Hero.IconX, Hero.IconY, Hero.IconX + Hero.IconWidth, Hero.IconY + Hero.IconHeight), HeadAtlasSize);
		}
		else
		{
			Layout.Portrait->SetVisibility(ESlateVisibility::Collapsed);
		}
		Layout.SelectionFrame->SetVisibility(HighlightedArmy == March.ArmyIndex && Layout.SelectionFrame->GetBrush().GetResourceObject()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		FIntPoint StatusOrigin(305, 120); // rest
		FText Status = NSLOCTEXT("RokGather", "ArmyStationed", "驻扎");
		switch (March.Phase)
		{
		case ERokServerMarchPhase::Collecting:
			StatusOrigin = {449, 96};
			Status = NSLOCTEXT("RokGather", "ArmyCollecting", "采集中");
			break;
		case ERokServerMarchPhase::Returning:
			StatusOrigin = {545, 84};
			Status = NSLOCTEXT("RokGather", "ArmyReturning", "返回中");
			break;
		case ERokServerMarchPhase::Marching:
			StatusOrigin = {257, 72};
			Status = March.TargetType == 1 ? NSLOCTEXT("RokGather", "ArmyAttacking", "进攻中")
				: NSLOCTEXT("RokGather", "ArmyMarching", "行军中");
			break;
		case ERokServerMarchPhase::Battling:
			StatusOrigin = {960, 285};
			Status = NSLOCTEXT("RokGather", "ArmyBattling", "战斗中");
			break;
		case ERokServerMarchPhase::Garrisoned:
			StatusOrigin = {305, 24};
			Status = NSLOCTEXT("RokGather", "ArmyGarrisoned", "驻防");
			break;
		case ERokServerMarchPhase::Rallying:
			StatusOrigin = {401, 50};
			Status = NSLOCTEXT("RokGather", "ArmyRallying", "集结中");
			break;
		case ERokServerMarchPhase::Unknown:
			Status = NSLOCTEXT("RokGather", "ArmyUnknown", "状态同步中");
			break;
		default:
			break;
		}
		SetAtlas(Layout.Status, MapUi, FIntRect(StatusOrigin, StatusOrigin + FIntPoint(46, 46)), MapUiSize);
		Button->SetToolTipText(bHasHero ? FText::Format(NSLOCTEXT("RokGather", "ArmyTooltip", "{0} · {1}"),
			FText::FromString(Hero.DisplayNameZhCn.IsEmpty() ? Hero.DisplayNameEn : Hero.DisplayNameZhCn), Status) : Status);
	}
}

void URokWorldMapHudWidget::BuildWorldOverlayArt()
{
	static const TSoftClassPtr<URokUiPageWidget> ArtClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldOverlayArt.WBP_RokWorldOverlayArt_C")));
	const FName Required[] = {TEXT("GatherRouteDot"), TEXT("AttackRouteDot"), TEXT("AttackRouteDestination"),
		TEXT("SearchResultGuide"), TEXT("MarchMarker"), TEXT("BattleMarker")};
	if (auto* Page = CreateWorldHudPage(ArtClass, TEXT("WorldOverlayArt")))
	{
		for (FName Name : Required)
		{
			if (auto* Art = Page->Get<UImage>(Name); Art && Cast<UCanvasPanelSlot>(Art->Slot))
			{
				WorldOverlayImages.Add(Name, Art);
			}
		}
		auto* Root = Cast<UCanvasPanel>(Page->GetRootWidget());
		if (Root && WorldOverlayImages.Num() == UE_ARRAY_COUNT(Required))
		{
			WorldOverlayArtPage = Page;
			WorldOverlayArtRoot = Root;
			return;
		}
		WorldOverlayImages.Reset();
	}

	WorldOverlayArtRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
	const auto Add = [this](UImage* Art, FVector2D Position, FVector2D Size, FVector2D Alignment, int32 ZOrder)
	{
		Art->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(WorldOverlayArtRoot, Art, Position, Size, FAnchors(), Alignment)->SetZOrder(ZOrder);
		WorldOverlayImages.Add(Art->GetFName(), Art);
	};

	auto* GatherDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GatherRouteDot"));
	GatherDot->SetBrush(FSlateColorBrush(FLinearColor(0.94f, 0.96f, 1.0f, 0.96f)));
	GatherDot->SetRenderTransformAngle(45.0f);
	Add(GatherDot, FVector2D::ZeroVector, {5.4f, 5.4f}, FVector2D(0.5f), 0);

	auto* AttackDot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AttackRouteDot"));
	const FVector2D DotSize(8, 5.5);
	AttackDot->SetBrush(FSlateRoundedBoxBrush(AttackRouteGreen,
		FLinearColor(0.027f, 0.16f, 0.08f, 0.92f), 1.f, DotSize));
	Add(AttackDot, FVector2D::ZeroVector, DotSize, FVector2D(0.5f), 5);

	auto* Cross = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("AttackRouteDestination"));
	Cross->SetColorAndOpacity(AttackRouteGreen);
	Add(Cross, FVector2D::ZeroVector, {43, 34}, FVector2D(0.5f), 5);

	auto* SearchGuide = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SearchResultGuide"));
	SearchGuide->SetRenderTransformAngle(180.0f);
	Add(SearchGuide, {7.5, -190}, {49, 149}, FVector2D::ZeroVector, 0);

	Add(MakeAtlasImage(TEXT("MarchMarker"), MapUi, FIntRect(257, 72, 303, 118), MapUiSize, {44, 44}),
		{-22, -32}, {44, 44}, FVector2D::ZeroVector, 71);
	Add(MakeAtlasImage(TEXT("BattleMarker"), MapUi, FIntRect(960, 285, 1006, 331), MapUiSize, {44, 44}),
		{-22, -72}, {44, 44}, FVector2D::ZeroVector, 71);
}

const UImage& URokWorldMapHudWidget::GetWorldOverlayArt(FName Name) const
{
	return *WorldOverlayImages.FindChecked(Name);
}

void URokWorldMapHudWidget::ApplyWorldOverlayArt(UImage& Image, const UImage& Art) const
{
	Image.SetBrush(Art.GetBrush());
	Image.SetColorAndOpacity(Art.GetColorAndOpacity());
	Image.SetRenderTransform(Art.GetRenderTransform());
	Image.SetRenderTransformPivot(Art.GetRenderTransformPivot());

	const auto* ArtSlot = CastChecked<UCanvasPanelSlot>(Art.Slot);
	auto* ImageSlot = CastChecked<UCanvasPanelSlot>(Image.Slot);
	ImageSlot->SetLayout(ArtSlot->GetLayout());
	ImageSlot->SetAutoSize(ArtSlot->GetAutoSize());
	ImageSlot->SetZOrder(ArtSlot->GetZOrder());
}

UImage* URokWorldMapHudWidget::AddWorldOverlayImage(UCanvasPanel* Parent, FName Name, const UImage& Art)
{
	auto* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
	Parent->AddChildToCanvas(Image);
	ApplyWorldOverlayArt(*Image, Art);
	return Image;
}

void URokWorldMapHudWidget::UpdateAttackMarchRoutes()
{
	const auto* Controller = GetOwningPlayer();
	FRokWorldMapProjection Projection;
	int32 DotsShown = 0;
	int32 DestinationsShown = 0;
	if (!AttackMarchRoutes.IsEmpty() && bTacticalPresentation && Controller
		&& CurrentRootSize.X > 0 && CurrentRootSize.Y > 0 && ResolveProjection(Projection))
	{
		const double Elapsed = FPlatformTime::Seconds() - AttackMarchSampleTime;
		const FVector2D Scale = CurrentExpandedDesignSize / CurrentRootSize
			/ FMath::Max(UE_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));
		const FBox2D View(FVector2D::ZeroVector, CurrentExpandedDesignSize);
		const auto& DotArt = GetWorldOverlayArt(TEXT("AttackRouteDot"));
		const auto& CrossArt = GetWorldOverlayArt(TEXT("AttackRouteDestination"));
		const FVector2D DotOffset = CastChecked<UCanvasPanelSlot>(DotArt.Slot)->GetPosition();
		const FVector2D CrossOffset = CastChecked<UCanvasPanelSlot>(CrossArt.Slot)->GetPosition();
		const auto Project = [&](const FVector2D& Map, FVector2D& Screen)
		{
			if (!Controller->ProjectWorldLocationToScreen(Projection.UnityToUe(Map), Screen, true)) return false;
			Screen *= Scale;
			return true;
		};
		for (const FRokServerMarch& March : AttackMarchRoutes)
		{
			FVector2D Position;
			int32 NextPoint = 0;
			if (!March.TryGetAttackMarchPosition(Position, &NextPoint, Elapsed)) continue;

			FVector2D From = FVector2D::ZeroVector;
			bool bFromProjected = Project(Position, From);
			double DistanceToDot = 10.0;
			for (int32 Point = NextPoint; Point < March.AttackPath.Num(); ++Point)
			{
				FVector2D To = FVector2D::ZeroVector;
				const bool bToProjected = Project(March.AttackPath[Point], To);
				if (bFromProjected && bToProjected)
				{
					const FVector2D Delta = To - From;
					const double Length = Delta.Size();
					if (Length > UE_SMALL_NUMBER)
					{
						constexpr double Spacing = 14.0;
						const FVector2D Direction = Delta / Length;
						double FirstVisible = 0.0, LastVisible = Length;
						const bool bVisible = ClipAttackRouteAxis(From.X, Direction.X, View.Max.X, FirstVisible, LastVisible)
							&& ClipAttackRouteAxis(From.Y, Direction.Y, View.Max.Y, FirstVisible, LastVisible);
						if (bVisible)
						{
							const double FirstDot = DistanceToDot + Spacing * FMath::Max(0.0,
								FMath::CeilToDouble((FirstVisible - DistanceToDot) / Spacing));
							// Leave space for the cross at the last waypoint.
							const double End = FMath::Min(LastVisible, Length - (Point == March.AttackPath.Num() - 1 ? 17.0 : 0.0));
							for (double Distance = FirstDot; Distance < End; Distance += Spacing)
							{
								if (!AttackRouteDots.IsValidIndex(DotsShown))
								{
									AttackRouteDots.Add(AddWorldOverlayImage(ExpandedDesignCanvas,
											*FString::Printf(TEXT("WorldAttackRouteDot_%d"), DotsShown), DotArt));
								}
								auto* Dot = AttackRouteDots[DotsShown++].Get();
								Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
								CastChecked<UCanvasPanelSlot>(Dot->Slot)->SetPosition(From + Direction * Distance + DotOffset);
								Dot->SetRenderTransformAngle(DotArt.GetRenderTransformAngle()
									+ FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
							}
						}
						DistanceToDot = Spacing - FMath::Fmod(Length + Spacing - DistanceToDot, Spacing);
					}
				}
				From = To;
				bFromProjected = bToProjected;
			}
			if (!bFromProjected || !View.IsInside(From)) continue;
			if (!AttackRouteDestinations.IsValidIndex(DestinationsShown))
			{
				if (!AttackRouteDestinationTexture) continue;
				auto* Cross = AddWorldOverlayImage(ExpandedDesignCanvas,
					*FString::Printf(TEXT("WorldAttackRouteDestination_%d"), DestinationsShown), CrossArt);
				if (!Cross->GetBrush().GetResourceObject()) Cross->SetBrushFromTexture(AttackRouteDestinationTexture);
				AttackRouteDestinations.Add(Cross);
			}
			auto* Cross = AttackRouteDestinations[DestinationsShown++].Get();
			Cross->SetVisibility(ESlateVisibility::HitTestInvisible);
			CastChecked<UCanvasPanelSlot>(Cross->Slot)->SetPosition(From + CrossOffset);
		}
	}
	for (int32 Index = DotsShown; Index < AttackRouteDots.Num(); ++Index)
		AttackRouteDots[Index]->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Index = DestinationsShown; Index < AttackRouteDestinations.Num(); ++Index)
		AttackRouteDestinations[Index]->SetVisibility(ESlateVisibility::Collapsed);
}


void URokWorldMapHudWidget::BuildWorldDamageArt()
{
	static const TSoftClassPtr<URokUiPageWidget> ArtClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldDamageNumbers.WBP_RokWorldDamageNumbers_C")));
	const FName Names[] = {TEXT("ArmyDamage"), TEXT("TargetDamage")};
	if (auto* Page = CreateWorldHudPage(ArtClass, TEXT("WorldDamageArt")))
	{
		auto* Root = Cast<UCanvasPanel>(Page->GetRootWidget());
		for (FName Name : Names)
		{
			auto* Text = Page->Get<UTextBlock>(Name);
			if (Text && Cast<UCanvasPanelSlot>(Text->Slot))
			{
				// The templates are never mounted, so resolve their runtime fonts here.
				if (auto* RokText = Cast<URokUiTextBlock>(Text)) RokText->ApplyFontRole();
				WorldDamageArt.Add(Text);
			}
		}
		if (Root && WorldDamageArt.Num() == UE_ARRAY_COUNT(Names))
		{
			WorldDamageArtPage = Page;
			WorldDamageArtRoot = Root;
			return;
		}
		WorldDamageArt.Reset();
	}

	WorldDamageArtRoot = WidgetTree->ConstructWidget<UCanvasPanel>();
	const FLinearColor Colors[] = {FLinearColor(1.0f, 0.38f, 0.29f), FLinearColor(1.0f, 0.80f, 0.45f)};
	for (int32 Side = 0; Side < UE_ARRAY_COUNT(Names); ++Side)
	{
		auto* Label = MakeHudText(Names[Side], 26, Colors[Side]);
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(FText::Format(NSLOCTEXT("RokBattle", "DamageNumber", "−{0}"), FText::AsNumber(100)));
		PlaceOnCanvas(WorldDamageArtRoot, Label, {Side == 0 ? -112.0 : 7.0, -60.0}, {105, 35})->SetZOrder(72);
		WorldDamageArt.Add(Label);
	}
}

void URokWorldMapHudWidget::UpdateAttackMarchMarkers(const FRokServerArmySnapshot& Snapshot)
{
	const auto* Controller = GetOwningPlayer();
	const auto* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ARokWorldMapGameMode>() : nullptr;
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FRokWorldMapProjection Projection;
	int32 Shown = 0;
	int32 DamageShown = 0;
	if (bTacticalPresentation && Controller && CurrentRootSize.X > 0 && CurrentRootSize.Y > 0 && ResolveProjection(Projection))
	{
		for (const auto& March : Snapshot.Marches)
		{
			FVector2D MapPosition, ScreenPosition;
			const bool bBattle = March.Phase == ERokServerMarchPhase::Battling && March.TargetType == 1 && March.bTargetKnown;
			if (!bBattle && Mode && Mode->HasVisibleArmyFormation(March.MapObjectId)) continue;
			if (bBattle) MapPosition = FVector2D(March.TargetX, March.TargetY) / 100.0;
			else if (!March.TryGetAttackMarchPosition(MapPosition)) continue;
			FVector Anchor = Projection.UnityToUe(MapPosition);
			if (bBattle)
			{
				const FString Id = LexToString(March.TargetObjectId);
				if (const auto* Badge = EntityBadges.FindByPredicate([&Id](const FEntityBadgeEntry& Entry)
					{ return Entry.ServerObjectId == Id && Entry.Entity.IsValid(); }))
					Anchor = Badge->Entity->GetSelectionAnchorWorldPosition();
			}
			if (!Controller->ProjectWorldLocationToScreen(Anchor, ScreenPosition, true)) continue;
			ScreenPosition /= FMath::Max(UE_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));
			if (ScreenPosition.X < 0 || ScreenPosition.Y < 0
				|| ScreenPosition.X > CurrentRootSize.X || ScreenPosition.Y > CurrentRootSize.Y) continue;

			const auto& Art = GetWorldOverlayArt(bBattle ? TEXT("BattleMarker") : TEXT("MarchMarker"));
			if (!AttackMarchMarkers.IsValidIndex(Shown))
			{
				AttackMarchMarkers.Add(AddWorldOverlayImage(ExpandedDesignCanvas,
					*FString::Printf(TEXT("WorldAttackMarchMarker_%d"), Shown), Art));
			}
			auto* Marker = AttackMarchMarkers[Shown++].Get();
			ApplyWorldOverlayArt(*Marker, Art);
			Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
			const FVector2D DesignPosition = ScreenPosition * CurrentExpandedDesignSize / CurrentRootSize;
			CastChecked<UCanvasPanelSlot>(Marker->Slot)->SetPosition(
				DesignPosition + CastChecked<UCanvasPanelSlot>(Art.Slot)->GetPosition());
			if (!bBattle || !Net) continue;
			const int64 Objects[] = {March.MapObjectId, March.TargetObjectId};
			for (int32 Side = 0; Side < UE_ARRAY_COUNT(Objects); ++Side)
			{
				int64 Damage = 0;
				double Age = 0;
				if (!Net->GetRecentBattleDamage(Objects[Side], Damage, Age)) continue;
				if (!AttackDamageLabels.IsValidIndex(DamageShown))
				{
					auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
						*FString::Printf(TEXT("WorldAttackDamage_%d"), DamageShown));
					ExpandedDesignCanvas->AddChildToCanvas(Label);
					AttackDamageLabels.Add(Label);
				}
				const UTextBlock& DamageArt = *WorldDamageArt[Side];
				auto* Label = AttackDamageLabels[DamageShown++].Get();
				Label->SetFont(DamageArt.GetFont());
				Label->SetColorAndOpacity(DamageArt.GetColorAndOpacity());
				Label->SetShadowOffset(DamageArt.GetShadowOffset());
				Label->SetShadowColorAndOpacity(DamageArt.GetShadowColorAndOpacity());
				static const auto* Justification = FindFProperty<FByteProperty>(UTextBlock::StaticClass(), TEXT("Justification"));
				Label->SetJustification(Justification
					? static_cast<ETextJustify::Type>(Justification->GetPropertyValue_InContainer(&DamageArt)) : ETextJustify::Center);
				Label->SetAutoWrapText(DamageArt.GetAutoWrapText());
				Label->SetWrapTextAt(DamageArt.GetWrapTextAt());
				Label->SetRenderTransform(DamageArt.GetRenderTransform());
				Label->SetRenderTransformPivot(DamageArt.GetRenderTransformPivot());
				Label->SetVisibility(ESlateVisibility::HitTestInvisible);
				Label->SetRenderOpacity(DamageArt.GetRenderOpacity() * FMath::Clamp(1.5 - Age, 0.0, 1.0));
				Label->SetText(FText::Format(NSLOCTEXT("RokBattle", "DamageNumber", "−{0}"), FText::AsNumber(Damage)));
				const auto* ArtSlot = CastChecked<UCanvasPanelSlot>(DamageArt.Slot);
				auto* LabelSlot = CastChecked<UCanvasPanelSlot>(Label->Slot);
				LabelSlot->SetLayout(ArtSlot->GetLayout());
				LabelSlot->SetAutoSize(ArtSlot->GetAutoSize());
				LabelSlot->SetZOrder(ArtSlot->GetZOrder());
				LabelSlot->SetPosition(DesignPosition + ArtSlot->GetPosition() - FVector2D(0, Age * 28));
			}
		}
	}
	for (int32 Index = Shown; Index < AttackMarchMarkers.Num(); ++Index)
		AttackMarchMarkers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Index = DamageShown; Index < AttackDamageLabels.Num(); ++Index)
		AttackDamageLabels[Index]->SetVisibility(ESlateVisibility::Collapsed);
}

void URokWorldMapHudWidget::HandleWorldMarchClicked(int32 ArmyIndex)
{
	auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn());
	const auto* Instance = GetGameInstance();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	if (!Camera || !Army) return;
	const auto Snapshot = Army->GetSnapshot();
	const auto* March = Snapshot.Marches.FindByPredicate([ArmyIndex](const FRokServerMarch& Row)
	{
		return Row.ArmyIndex == ArmyIndex;
	});
	if (!March) return;

	PlayResourceClickSound();
	ClearWorldObjectSelection();
	SelectedMarchArmyIndex = ArmyIndex;
	if (March->bTargetKnown)
		Camera->SetViewCenterUnity(FVector2D(March->TargetX, March->TargetY) / 100.0);
	if (March->Phase == ERokServerMarchPhase::Collecting && March->TargetObjectId > 0)
	{
		PendingWorldTargetId = LexToString(March->TargetObjectId);
		bPendingWorldTargetFromSearch = false;
		PendingWorldTargetCenter = Camera->GetViewCenterUnity();
		PendingWorldTargetDeadline = FPlatformTime::Seconds() + 4.0;
		TryOpenFocusedWorldTarget();
	}
	RefreshWorldMarchQueue();
}

bool URokWorldMapHudWidget::FocusWorldSearchResult(int64 ObjectId, FVector2D UnityPosition)
{
	auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn());
	if (!Camera || ObjectId <= 0) return false;
	ClearWorldObjectSelection();
	Camera->SetCurrentDxf(Camera->GetTacticalDxf());
	Camera->SetViewCenterUnity(UnityPosition);
	PendingWorldTargetId = LexToString(ObjectId);
	bPendingWorldTargetFromSearch = true;
	PendingWorldTargetCenter = Camera->GetViewCenterUnity();
	PendingWorldTargetDeadline = FPlatformTime::Seconds() + 4.0;
	TryOpenFocusedWorldTarget();
	return true;
}

void URokWorldMapHudWidget::TryOpenFocusedWorldTarget()
{
	if (PendingWorldTargetId.IsEmpty()) return;
	auto* Controller = Cast<ARokWorldMapPlayerController>(GetOwningPlayer());
	const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn());
	if (!Controller || !Camera || !Camera->GetViewCenterUnity().Equals(PendingWorldTargetCenter, 1.0)
		|| !ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(Camera->GetCurrentDxf())
		|| FPlatformTime::Seconds() >= PendingWorldTargetDeadline)
	{
		PendingWorldTargetId.Reset();
		return;
	}
	// A distant search or march target arrives with the next AOI update.
	for (const FEntityBadgeEntry& Entry : EntityBadges)
	{
		if (Entry.ServerObjectId == PendingWorldTargetId && Entry.Entity.IsValid())
		{
			PendingWorldTargetId.Reset();
			const bool bWaitForBarbarianClick = bPendingWorldTargetFromSearch
				&& Entry.Entity->GetEntityKind() == TEXT("barbarian");
			Controller->SelectWorldMapActor(Entry.Entity.Get(), !bWaitForBarbarianClick);
			if (bPendingWorldTargetFromSearch && Entry.Root)
			{
				const auto& Art = GetWorldOverlayArt(TEXT("SearchResultGuide"));
				if (!WorldSearchResultGuide)
				{
					UTexture2D* Texture = Cast<UTexture2D>(Art.GetBrush().GetResourceObject());
					if (!Texture) Texture = LoadTransientUiTexture(RokDataPaths::Resolve(TEXT("WorldSearch/finger001.png")));
					if (Texture)
					{
						WorldSearchResultGuide = AddWorldOverlayImage(Entry.Root, TEXT("WorldSearchResultGuide"), Art);
						WorldSearchResultGuide->SetBrushFromTexture(Texture);
						WorldSearchResultGuide->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
				}
				else
				{
					Entry.Root->AddChildToCanvas(WorldSearchResultGuide);
					const auto* ArtSlot = CastChecked<UCanvasPanelSlot>(Art.Slot);
					auto* GuideSlot = CastChecked<UCanvasPanelSlot>(WorldSearchResultGuide->Slot);
					GuideSlot->SetLayout(ArtSlot->GetLayout());
					GuideSlot->SetAutoSize(ArtSlot->GetAutoSize());
					GuideSlot->SetZOrder(ArtSlot->GetZOrder());
				}
			}
			return;
		}
	}
}

void URokWorldMapHudWidget::BuildBarbarianInfo()
{
	static const TSoftClassPtr<URokUiPageWidget> BarbarianPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldBarbarianInfo.WBP_RokWorldBarbarianInfo_C")));
	if (auto* Page = AddWorldHudPage(WorldObjectInfoRoot, BarbarianPage, TEXT("WorldBarbarianInfoPage")))
	{
		BindBarbarianInfo(*Page);
		static const TSoftClassPtr<URokUiPageWidget> RewardGridClass(FSoftObjectPath(
			TEXT("/Game/RokUI/Hud/WBP_RokWorldBarbarianRewardGrid.WBP_RokWorldBarbarianRewardGrid_C")));
		if (auto* Grid = CreateWorldHudPage(RewardGridClass, NAME_None))
		{
			for (int32 Index = 0; Index < 6; ++Index)
			{
				const auto* Cell = Grid->Get<UCanvasPanel>(FName(*FString::Printf(TEXT("RewardCell_%d"), Index)));
				const auto* CellLayoutSlot = Cell ? Cast<UCanvasPanelSlot>(Cell->Slot) : nullptr;
				if (!CellLayoutSlot)
				{
					BarbarianRewardCells.Reset();
					break;
				}
				const FVector2D Position = CellLayoutSlot->GetPosition();
				BarbarianRewardCells.Emplace(Position, Position + CellLayoutSlot->GetSize());
			}
		}
		return;
	}

	// UI_Pop_WorldObjectMonster and UI_Item_WorldObjInfoTDrop, scaled into the 1440-wide HUD.
	const FVector2D Size(486, 434);
	const FLinearColor Ink(0.025f, 0.022f, 0.018f);
	WorldBarbarianInfoPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianInfo"));
	WorldBarbarianInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(WorldObjectInfoRoot, WorldBarbarianInfoPanel, FVector2D::ZeroVector, Size);
	AddWorldPopupFrame(WorldBarbarianInfoPanel, Size, TEXT("WorldBarbarianInfoPatch"));
	PlaceOnCanvas(WorldBarbarianInfoPanel, MakeTransparentHitButton(TEXT("WorldBarbarianInputBlocker")),
		FVector2D::ZeroVector, Size);

	BarbarianNormalPage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianNormal"));
	BarbarianDetailsPage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianDetailPage"));
	PlaceOnCanvas(WorldBarbarianInfoPanel, BarbarianNormalPage, FVector2D::ZeroVector, Size);
	PlaceOnCanvas(WorldBarbarianInfoPanel, BarbarianDetailsPage, FVector2D::ZeroVector, Size);
	SetBarbarianDetailsExpanded(false);

	const auto AddText = [this, Ink](UCanvasPanel* Page, FName Name, int32 Pixels, bool bBold, FVector2D Position, FVector2D TextSize)
	{
		auto* Text = MakeHudText(Name, Pixels, Ink, bBold);
		Text->SetFont(URokMainHudWidget::MakeUnitySourceFont(Pixels, bBold));
		Text->SetJustification(ETextJustify::Center);
		Text->SetShadowOffset(FVector2D::ZeroVector);
		PlaceOnCanvas(Page, Text, Position, TextSize);
		return Text;
	};
	BarbarianCoordinate = AddText(BarbarianNormalPage, TEXT("WorldBarbarianCoordinate"), 20, false, {68, 20}, {350, 28});
	BarbarianTitle = AddText(BarbarianNormalPage, TEXT("WorldBarbarianTitle"), 24, true, {68, 52}, {350, 32});

	auto* Shade = WidgetTree->ConstructWidget<UImage>();
	Shade->SetBrush(FSlateColorBrush(FLinearColor(0.08f, 0.075f, 0.065f, 0.36f)));
	Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(BarbarianNormalPage, Shade, {20, 88}, {446, 196});
	PlaceOnCanvas(BarbarianNormalPage, MakeAtlasImage(TEXT("WorldBarbarianPortraitFrame"), MapIconUi,
		FIntRect(729, 1878, 897, 2047), MapIconUiSize, {168, 169}), {28, 88}, {168, 169});
	BarbarianPortrait = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldBarbarianPortrait"));
	BarbarianPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(BarbarianNormalPage, BarbarianPortrait, {30, 82}, {164, 176});
	AddText(BarbarianNormalPage, TEXT("WorldBarbarianRewardTitle"), 20, true, {214, 96}, {234, 26})
		->SetText(NSLOCTEXT("RokBattle", "BarbarianRewards", "可能掉落"));
	BarbarianRewards = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianRewards"));
	BarbarianRewards->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(BarbarianNormalPage, BarbarianRewards, {214, 122}, {234, 152});

	BarbarianTimer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianTimer"));
	PlaceOnCanvas(BarbarianNormalPage, BarbarianTimer, {53, 250}, {145, 29});
	auto* Hourglass = MakeAtlasImage(TEXT("WorldBarbarianHourglass"), Common,
		FIntRect(340, 32, 382, 78), CommonSize, {27, 30});
	PlaceOnCanvas(BarbarianTimer, Hourglass, {-5, 0}, {27, 30});
	BarbarianTimeRemaining = AddText(BarbarianTimer, TEXT("WorldBarbarianTimeRemaining"), 18, true, {25, 5}, {115, 23});
	BarbarianTimeRemaining->SetJustification(ETextJustify::Left);

	auto* Details = MakeAtlasButton(TEXT("WorldBarbarianDetails"), Common,
		FIntRect(782, 237, 819, 274), CommonSize, {38, 38});
	Details->SetToolTipText(NSLOCTEXT("RokBattle", "BarbarianDetails", "详情"));
	Details->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleBarbarianDetailsClicked);
	PlaceOnCanvas(BarbarianNormalPage, Details, {24, 39}, {38, 38});
	auto* Share = MakeAtlasButton(TEXT("WorldBarbarianShare"), MapUi,
		FIntRect(729, 198, 783, 265), MapUiSize, {48, 60});
	Share->SetToolTipText(NSLOCTEXT("RokBattle", "BarbarianShare", "复制坐标"));
	Share->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectShareClicked);
	PlaceOnCanvas(BarbarianNormalPage, Share, {428, -4}, {48, 60});
	PlaceOnCanvas(BarbarianNormalPage, MakeAtlasImage(TEXT("WorldBarbarianShareIcon"), MapUi,
		FIntRect(481, 7, 513, 39), MapUiSize, {28, 28}), {438, 7}, {28, 28});

	auto* Back = MakeAtlasButton(TEXT("WorldBarbarianDetailsBack"), Common,
		FIntRect(1171, 489, 1209, 527), CommonSize, {38, 38});
	Back->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleBarbarianDetailsBackClicked);
	PlaceOnCanvas(BarbarianDetailsPage, Back, {24, 31}, {38, 38});
	auto* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldBarbarianDescriptionScroll"));
	DescriptionScroll->SetClipping(EWidgetClipping::ClipToBounds);
	PlaceOnCanvas(BarbarianDetailsPage, DescriptionScroll, {28, 88}, {430, 276});
	BarbarianDescription = MakeHudText(TEXT("WorldBarbarianDescription"), 20, Ink);
	BarbarianDescription->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, false));
	BarbarianDescription->SetShadowOffset(FVector2D::ZeroVector);
	BarbarianDescription->SetAutoWrapText(true);
	DescriptionScroll->AddChild(BarbarianDescription);
	BarbarianCost = AddText(BarbarianDetailsPage, TEXT("WorldBarbarianCost"), 20, true, {28, 380}, {430, 28});

	BarbarianAttackButton = MakeAtlasButton(TEXT("WorldBarbarianAttack"), Common,
		FIntRect(677, 517, 879, 598), CommonSize, {202, 81});
	BarbarianAttackButton->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleBarbarianAttackClicked);
	PlaceOnCanvas(BarbarianNormalPage, BarbarianAttackButton, {142, 307}, {202, 81});
	BarbarianAttackLabel = AddText(BarbarianNormalPage, TEXT("WorldBarbarianAttackLabel"), 30, true, {142, 332}, {202, 43});
	BarbarianFeedback = AddText(BarbarianNormalPage, TEXT("WorldBarbarianFeedback"), 19, true, {24, 391}, {438, 31});

	BarbarianStay = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("WorldBarbarianStay"));
	auto* Atlas = LoadUiTexture(Common);
	const FSlateBrush Off = MakeAtlasBrush(Atlas, FIntRect(611, 181, 655, 225), CommonSize, {30, 30});
	const FSlateBrush On = MakeAtlasBrush(Atlas, FIntRect(697, 231, 741, 275), CommonSize, {30, 30});
	FCheckBoxStyle Style = BarbarianStay->GetWidgetStyle();
	Style.SetUncheckedImage(Off).SetUncheckedHoveredImage(Off).SetUncheckedPressedImage(Off)
		.SetCheckedImage(On).SetCheckedHoveredImage(On).SetCheckedPressedImage(On).SetPadding(FMargin(11, 0));
	BarbarianStay->SetWidgetStyle(Style);
	auto* StayLabel = MakeHudText(TEXT("WorldBarbarianStayLabel"), 20, Ink);
	StayLabel->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, false));
	StayLabel->SetShadowOffset(FVector2D::ZeroVector);
	StayLabel->SetText(NSLOCTEXT("RokBattle", "BarbarianStay", "进攻结束后在原地驻扎"));
	BarbarianStay->SetContent(StayLabel);
	PlaceOnCanvas(BarbarianNormalPage, BarbarianStay, {100, 390}, {300, 34});
}

void URokWorldMapHudWidget::BindBarbarianInfo(URokUiPageWidget& Page)
{
	WorldBarbarianInfoPanel = Page.Get<UCanvasPanel>(TEXT("WorldBarbarianInfo"));
	BarbarianNormalPage = Page.Get<UCanvasPanel>(TEXT("WorldBarbarianNormal"));
	BarbarianDetailsPage = Page.Get<UCanvasPanel>(TEXT("WorldBarbarianDetailPage"));
	BarbarianCoordinate = Page.Get<UTextBlock>(TEXT("WorldBarbarianCoordinate"));
	BarbarianTitle = Page.Get<UTextBlock>(TEXT("WorldBarbarianTitle"));
	BarbarianPortrait = Page.Get<UImage>(TEXT("WorldBarbarianPortrait"));
	BarbarianRewards = Page.Get<UCanvasPanel>(TEXT("WorldBarbarianRewards"));
	BarbarianTimer = Page.Get<UCanvasPanel>(TEXT("WorldBarbarianTimer"));
	BarbarianTimeRemaining = Page.Get<UTextBlock>(TEXT("WorldBarbarianTimeRemaining"));
	BarbarianDescription = Page.Get<UTextBlock>(TEXT("WorldBarbarianDescription"));
	BarbarianCost = Page.Get<UTextBlock>(TEXT("WorldBarbarianCost"));
	BarbarianAttackButton = Page.Get<UButton>(TEXT("WorldBarbarianAttack"));
	BarbarianAttackLabel = Page.Get<UTextBlock>(TEXT("WorldBarbarianAttackLabel"));
	BarbarianFeedback = Page.Get<UTextBlock>(TEXT("WorldBarbarianFeedback"));
	BarbarianStay = Page.Get<UCheckBox>(TEXT("WorldBarbarianStay"));
	BindWorldHudButton(Page, TEXT("WorldBarbarianDetails"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleBarbarianDetailsClicked));
	BindWorldHudButton(Page, TEXT("WorldBarbarianDetailsBack"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleBarbarianDetailsBackClicked));
	BindWorldHudButton(Page, TEXT("WorldBarbarianShare"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleWorldObjectShareClicked));
	BindWorldHudButton(Page, TEXT("WorldBarbarianAttack"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleBarbarianAttackClicked));
	SetBarbarianDetailsExpanded(false);
}

void URokWorldMapHudWidget::SetBarbarianDetailsExpanded(bool bExpanded)
{
	BarbarianNormalPage->SetVisibility(bExpanded ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	BarbarianDetailsPage->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bExpanded)
	{
		if (auto* Scroll = Cast<UScrollBox>(RokUiFind::Find(this, TEXT("WorldBarbarianDescriptionScroll"))))
			Scroll->ScrollToStart();
	}
}

void URokWorldMapHudWidget::HandleBarbarianDetailsClicked()
{
	PlayResourceClickSound();
	SetBarbarianDetailsExpanded(true);
}

void URokWorldMapHudWidget::HandleBarbarianDetailsBackClicked()
{
	PlayResourceClickSound();
	SetBarbarianDetailsExpanded(false);
}

URokWorldMapHudWidget::FBarbarianRewardLayout URokWorldMapHudWidget::BuildBarbarianRewardLayout(
	UCanvasPanel* Cell, int32 Index)
{
	static const TSoftClassPtr<URokUiPageWidget> RewardClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldBarbarianReward.WBP_RokWorldBarbarianReward_C")));
	if (auto* Page = AddWorldHudPage(Cell, RewardClass, NAME_None))
	{
		const FBarbarianRewardLayout Layout{
			Page->Get<UImage>(TEXT("WorldBarbarianRewardFrame")),
			Page->Get<UImage>(TEXT("WorldBarbarianRewardIcon")),
			Page->Get<UTextBlock>(TEXT("WorldBarbarianRewardValue")),
			Page->Get<UTextBlock>(TEXT("WorldBarbarianRewardCount"))
		};
		if (Layout.Frame && Layout.Icon && Layout.Value && Layout.Count)
		{
			Page->LegacyIndex = FString::Printf(TEXT("_%d"), Index);
			return Layout;
		}
		Page->RemoveFromParent();
	}

	const FString Prefix = FString::Printf(TEXT("WorldBarbarianReward_%d"), Index);
	FBarbarianRewardLayout Layout;
	Layout.Frame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(Prefix + TEXT("Frame")));
	Layout.Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Cell, Layout.Frame, {0, 0}, {74, 74});
	Layout.Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(Prefix + TEXT("Icon")));
	Layout.Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Cell, Layout.Icon, {5, 5}, {64, 64});
	Layout.Value = MakeHudText(*(Prefix + TEXT("Value")), 12, FLinearColor::White);
	Layout.Value->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(Cell, Layout.Value, {3, 3}, {68, 18});
	Layout.Count = MakeHudText(*(Prefix + TEXT("Count")), 15, FLinearColor::White);
	Layout.Count->SetJustification(ETextJustify::Right);
	PlaceOnCanvas(Cell, Layout.Count, {3, 54}, {65, 18});
	return Layout;
}

void URokWorldMapHudWidget::RefreshBarbarianInfo(const ARokWorldMapEntityActor& Target)
{
	const auto* Instance = GetGameInstance();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FRokBarbarianAttackInfo Info;
	const bool bKnown = Army && Army->GetBarbarianAttackInfo(FCString::Atoi64(*SelectedWorldObjectServerId), Info);
	double ServerTime = 0.0;
	const bool bTimerKnown = Info.ExpiresAt > 0 && Net && Net->TryGetServerTimeSeconds(ServerTime);
	if (bTimerKnown && ServerTime >= Info.ExpiresAt)
	{
		ClearWorldObjectSelection();
		return;
	}
	BarbarianTimer->SetVisibility(bTimerKnown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bTimerKnown)
	{
		const int64 Seconds = FMath::CeilToInt64(Info.ExpiresAt - ServerTime);
		BarbarianTimeRemaining->SetText(FText::FromString(FString::Printf(TEXT("%02lld:%02lld:%02lld"),
			Seconds / 3600, (Seconds / 60) % 60, Seconds % 60)));
	}

	if (BarbarianDisplayedRewardGroup != Info.RewardGroupId && FeatureScreenWidget)
	{
		BarbarianDisplayedRewardGroup = Info.RewardGroupId;
		BarbarianRewards->ClearChildren();
		const auto& Rewards = FeatureScreenWidget->GetConfiguredRewardPreview(Info.RewardGroupId);
		for (int32 Index = 0; Index < FMath::Min(6, Rewards.Num()); ++Index)
		{
			const auto& Reward = Rewards[Index];
			const FString Prefix = FString::Printf(TEXT("WorldBarbarianReward_%d"), Index);
			auto* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *Prefix);
			Cell->SetToolTipText(FText::FromString(Reward.DisplayName));
			const FVector2D RewardDesignSize(74.0, 74.0);
			const FVector2D LegacyPosition(79.0 * (Index % 3), 78.0 * (Index / 3));
			const FBox2D CellBounds = BarbarianRewardCells.IsValidIndex(Index) ? BarbarianRewardCells[Index]
				: FBox2D(LegacyPosition, LegacyPosition + RewardDesignSize);
			PlaceOnCanvas(BarbarianRewards, Cell, CellBounds.Min, RewardDesignSize);
			Cell->SetRenderTransformPivot(FVector2D::ZeroVector);
			Cell->SetRenderScale(CellBounds.GetSize() / RewardDesignSize);
			const FBarbarianRewardLayout Layout = BuildBarbarianRewardLayout(Cell, Index);
			Layout.Frame->SetBrush(FeatureScreenWidget->MakeRewardQualityBrush(Reward.Quality, RewardDesignSize));
			UTexture2D* Atlas = Reward.AtlasPath.IsEmpty() ? nullptr : LoadUiTexture(*Reward.AtlasPath);
			if (Atlas)
			{
				Layout.Icon->SetBrush(MakeAtlasBrush(Atlas, Reward.IconRect, Reward.IconAtlasSize, {64, 64}));
			}
			Layout.Icon->SetVisibility(Atlas ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Layout.Value->SetText(Reward.TopLabel);
			Layout.Count->SetText(FText::AsNumber(Reward.Quantity));
		}
		const bool bCommonPortrait = Info.HeadIcon == TEXT("ui_common[icon_com_monster]");
		BarbarianPortrait->SetBrush(MakeAtlasBrush(LoadUiTexture(bCommonPortrait ? Common : MapIconUi),
			bCommonPortrait ? FIntRect(960, 731, 1094, 883) : FIntRect(547, 1847, 727, 2047),
			bCommonPortrait ? CommonSize : MapIconUiSize, {164, 176}));
	}
	const bool bLocked = bKnown && Info.AttackLevelMax > 0 && Info.Level > Info.AttackLevelMax;
	const FString Title = FRokUiTextTable::Resolve(Info.NameId);
	BarbarianTitle->SetText(FText::FromString(Title.IsEmpty() ? Target.GetDisplayName() : Title));
	FRokWorldMapProjection Projection;
	const FVector2D Position = ResolveProjection(Projection)
		? Projection.UnityToDisplay(Target.GetUnityPosition()) : Target.GetUnityPosition();
	BarbarianCoordinate->SetText(FText::FromString(FString::Printf(TEXT("X:%.0f,Y:%.0f"), Position.X, Position.Y)));
	BarbarianDescription->SetText(FText::FromString(FRokUiTextTable::SanitizeRichText(FRokUiTextTable::Resolve(Info.DescriptionId))));
	BarbarianCost->SetText(!bKnown ? FText::GetEmpty() : FText::FromString(
		Info.ActionForce >= 0 ? FString::Printf(TEXT("行动力消耗 %d   ·   当前 %lld"), Info.ActionCost, Info.ActionForce)
		: FString::Printf(TEXT("行动力消耗 %d"), Info.ActionCost)));
	BarbarianFeedback->SetText(FText::FromString(bKnown ? Info.BlockReason : FString(TEXT("等待服务器同步该目标。"))));
	BarbarianAttackLabel->SetText(FText::FromString(bLocked ? TEXT("搜索乱军") : TEXT("攻击")));
	BarbarianAttackButton->SetIsEnabled(bKnown && (bLocked || Info.BlockReason.IsEmpty()));
	BarbarianStay->SetVisibility(bKnown && Info.BlockReason.IsEmpty() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}


void URokWorldMapHudWidget::HandleBarbarianAttackClicked()
{
	auto* Target = SelectedWorldObjectActor.Get();
	const auto* Army = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	FRokBarbarianAttackInfo Info;
	const int64 TargetId = FCString::Atoi64(*SelectedWorldObjectServerId);
	if (!Target || !Army || !Army->GetBarbarianAttackInfo(TargetId, Info)) return;
	PlayResourceClickSound();
	if (Info.AttackLevelMax > 0 && Info.Level > Info.AttackLevelMax && FeatureScreenWidget)
	{
		OpenFeatureScreen();
		FeatureScreenWidget->OpenResourceSearch(FMath::Clamp(Info.AttackLevelMax, 1, 25));
		return;
	}
	if (Info.BlockReason.IsEmpty())
		OpenGatherPanel(Target, TargetId, BarbarianStay->IsChecked());
}

URokWorldMapHudWidget::FAttackArmyChoiceLayout URokWorldMapHudWidget::BuildAttackArmyChoiceLayout(int32 ArmyIndex)
{
	static const TSoftClassPtr<URokUiPageWidget> ChoiceClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldAttackArmyChoice.WBP_RokWorldAttackArmyChoice_C")));
	if (auto* Page = CreateWorldHudPage(ChoiceClass, NAME_None))
	{
		auto* Button = Page->Get<URokUnityIndexedButton>(TEXT("WorldAttackArmy"));
		auto* Label = Page->Get<UTextBlock>(TEXT("WorldAttackArmyCaption"));
		if (Button && Label)
		{
			Page->LegacySuffix = FString::Printf(TEXT("_%d"), ArmyIndex);
			return {Page, Button, Label};
		}
	}

	const FName Key(*FString::Printf(TEXT("WorldAttackArmy_%d"), ArmyIndex));
	auto* Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Key);
	if (auto* Atlas = LoadUiTexture(Common))
	{
		FButtonStyle Style;
		const FSlateBrush Background = MakeAtlasBrush(Atlas, FIntRect(1098, 571, 1300, 652), CommonSize, {130, 68});
		Style.SetNormal(Background).SetHovered(Background).SetPressed(Background);
		Button->SetStyle(Style);
	}
	const FName LabelName = ArmyIndex == 0 ? FName(TEXT("WorldAttackArmyCaption")) : NAME_None;
	auto* Label = MakeHudText(LabelName, 17, FLinearColor::White, true);
	Label->SetJustification(ETextJustify::Center);
	Button->SetContent(Label);
	return {Button, Button, Label};
}

void URokWorldMapHudWidget::RefreshAttackArmyChoices()
{
	AttackArmyChoices->ClearChildren();
	const auto* Army = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	if (!Army) return;

	for (const FRokServerMarch& March : Army->GetSnapshot().Marches)
	{
		if (!URokServerArmySubsystem::CanRetargetArmy(March, GatherTargetId)) continue;
		FRokUnityCatalogHero Hero;
		const FString Name = Catalog && Catalog->GetHero(March.MainHeroId, Hero) ? Hero.DisplayNameZhCn : TEXT("部队");
		const FString Caption = FString::Printf(TEXT("%s\n%s 名士兵"), *Name, *FText::AsNumber(March.Count).ToString());
		const FAttackArmyChoiceLayout Choice = BuildAttackArmyChoiceLayout(March.ArmyIndex);
		Choice.Button->ConfigureAction(March.ArmyIndex);
		Choice.Button->OnIndexedClicked.AddUObject(this, &URokWorldMapHudWidget::SelectAttackArmy);
		if (auto* Atlas = LoadUiTexture(Common))
		{
			FButtonStyle Style = Choice.Button->GetStyle();
			const FSlateBrush Background = MakeAtlasBrush(Atlas,
				March.ArmyIndex == GatherExistingArmyIndex ? FIntRect(1766, 586, 2040, 670) : FIntRect(1098, 571, 1300, 652),
				CommonSize, {130, 68});
			const auto ApplyState = [&Background, Atlas](FSlateBrush Brush)
			{
				Brush.SetResourceObject(Atlas);
				Brush.SetUVRegion(static_cast<FBox2f>(Background.GetUVRegion()));
				return Brush;
			};
			Style.SetNormal(ApplyState(Style.Normal))
				.SetHovered(ApplyState(Style.Hovered))
				.SetPressed(ApplyState(Style.Pressed));
			Choice.Button->SetStyle(Style);
		}
		Choice.Label->SetText(FText::FromString(Caption));
		AttackArmyChoices->AddChild(Choice.Root);
	}
}

void URokWorldMapHudWidget::SelectAttackArmy(int32 ArmyIndex)
{
	if (!GatherAttackStation.IsSet() || bExistingAttackPending || !GatherRoot->IsVisible()) return;
	GatherExistingArmyIndex = ArmyIndex;
	GatherNewSelected->SetVisibility(ArmyIndex == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	GatherCreateLabel->SetText(FText::FromString(ArmyIndex == 0 ? TEXT("创建部队") : TEXT("出征")));
	GatherFeedback->SetText(FText::FromString(ArmyIndex == 0 ? TEXT("创建一支从你的城市出发的部队") : TEXT("派遣所选部队攻击乱军")));
	RefreshAttackArmyChoices();
}

void URokWorldMapHudWidget::HandleSelectNewAttackArmy()
{
	SelectAttackArmy(0);
}

void URokWorldMapHudWidget::BuildGatherPanel()
{
	GatherRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldGatherDispatch"));
	GatherRoot->SetVisibility(ESlateVisibility::Collapsed);
	auto* Overlay = ExpandedDesignCanvas->AddChildToCanvas(GatherRoot);
	Overlay->SetAnchors(FAnchors(0, 0, 1, 1));
	Overlay->SetOffsets(FMargin(0));
	Overlay->SetZOrder(200);

	static const TSoftClassPtr<URokUiPageWidget> GatherPageClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldGatherDispatch.WBP_RokWorldGatherDispatch_C")));
	auto* GatherPage = AddWorldHudPage(GatherRoot, GatherPageClass, TEXT("WorldGatherDispatchPage"), 1);
	if (!GatherPage)
	{
		auto* Back = MakeTransparentHitButton(TEXT("WorldGatherDispatchDismiss"));
		Back->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleGatherCloseClicked);
		auto* BackSlot = GatherRoot->AddChildToCanvas(Back);
		BackSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		BackSlot->SetOffsets(FMargin(0));
	}

	// The path is a preview drawn between the actual city and resource point.
	const auto& DotArt = GetWorldOverlayArt(TEXT("GatherRouteDot"));
	for (int32 Index = 0; Index < 64; ++Index)
	{
		auto* Dot = AddWorldOverlayImage(GatherRoot, *FString::Printf(TEXT("WorldGatherRouteDot_%d"), Index), DotArt);
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		GatherRouteDots.Add(Dot);
	}

	if (GatherPage)
	{
		BindGatherPanel(*GatherPage);
		return;
	}

	const FAnchors Right(1, 0);
	GatherQueueShade = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldGatherQueueShade"));
	GatherQueueShade->SetBrush(FSlateColorBrush(FLinearColor(0.15f, 0.10f, 0.055f, 0.50f)));
	GatherQueueShade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(GatherRoot, GatherQueueShade, {-146, 66}, {146, 770}, Right);
	GatherQueueCount = MakeHudText(TEXT("WorldGatherQueueCount"), 20, FLinearColor::White, true);
	GatherQueueCount->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, true));
	GatherQueueCount->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(GatherRoot, GatherQueueCount, {-145, 78}, {145, 33}, Right);

	// UI_Item_ArmyQueueNew: img_com_TroopEmpty already contains the plus.
	PlaceOnCanvas(GatherRoot, MakeAtlasImage(TEXT("WorldGatherNewTroop"), Common, FIntRect(1, 28, 107, 149),
		CommonSize, {102, 116}), {-124, 111}, {102, 116}, Right);
	GatherNewSelected = MakeAtlasImage(TEXT("WorldGatherNewTroopSelected"), HeroUi,
		FIntRect(1905, 356, 2046, 511), HeroUiSize, {126, 138});
	PlaceOnCanvas(GatherRoot, GatherNewSelected, {-136, 100}, {126, 138}, Right);
	// The source plus selects the new-army slot. It starts selected for gathering.
	auto* SelectNew = MakeTransparentHitButton(TEXT("WorldGatherSelectNew"));
	SelectNew->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleSelectNewAttackArmy);
	PlaceOnCanvas(GatherRoot, SelectNew, {-125, 110}, {104, 116}, Right);
	AttackArmyChoices = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldAttackArmyChoices"));
	AttackArmyChoices->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(GatherRoot, AttackArmyChoices, {-141, 242}, {138, 474}, Right);

	auto* Card = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldGatherCreateCard"));
	Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(GatherRoot, Card, {-412, 64}, {247, 197}, Right);
	PlaceOnCanvas(Card, MakeTransparentHitButton(TEXT("WorldGatherCardBlocker")), {0, 0}, {247, 197});
	AddWorldPopupFrame(Card, {247, 197}, TEXT("WorldGatherCreatePatch"));
	PlaceOnCanvas(Card, MakeAtlasImage(TEXT("WorldGatherCreateArrow"), Common,
		FIntRect(191, 270, 203, 285), CommonSize, {13, 17}), {244, 63}, {13, 17});
	auto* TipShade = WidgetTree->ConstructWidget<UImage>();
	TipShade->SetBrush(FSlateColorBrush(FLinearColor(0.24f, 0.26f, 0.21f, 0.34f)));
	TipShade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Card, TipShade, {17, 13}, {212, 84});
	GatherFeedback = MakeHudText(TEXT("WorldGatherCreateTip"), 20, FLinearColor(0.1f, 0.10f, 0.075f));
	GatherFeedback->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, false));
	GatherFeedback->SetShadowOffset(FVector2D::ZeroVector);
	GatherFeedback->SetAutoWrapText(true);
	PlaceOnCanvas(Card, GatherFeedback, {26, 29}, {195, 65});

	auto* Create = WidgetTree->ConstructWidget<URokUiAnimatedButton>(
		URokUiAnimatedButton::StaticClass(), TEXT("WorldGatherCreateArmy"));
	if (auto* Atlas = LoadUiTexture(Common))
	{
		const FSlateBrush Blue = MakeAtlasBrush(Atlas, FIntRect(677, 517, 879, 598), CommonSize, {202, 81});
		FButtonStyle Style;
		Style.SetNormal(Blue).SetHovered(Blue).SetPressed(Blue);
		Style.SetDisabled(MakeAtlasBrush(Atlas, FIntRect(473, 528, 675, 609), CommonSize, {202, 81}));
		Create->SetStyle(Style);
	}
	auto* Label = MakeHudText(TEXT("WorldGatherCreateLabel"), 30, FLinearColor(0.02f, 0.09f, 0.15f), true);
	Label->SetFont(URokMainHudWidget::MakeUnitySourceFont(30, true));
	Label->SetShadowOffset(FVector2D::ZeroVector);
	Label->SetJustification(ETextJustify::Center);
	Label->SetText(NSLOCTEXT("RokGather", "Create", "创建部队"));
	Create->SetContent(Label);
	GatherCreateButton = Create;
	GatherCreateLabel = Label;
	Create->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleGatherCreateClicked);
	PlaceOnCanvas(Card, Create, {22, 101}, {202, 81});
}

void URokWorldMapHudWidget::BindGatherPanel(URokUiPageWidget& Page)
{
	GatherQueueShade = Page.Get<UImage>(TEXT("WorldGatherQueueShade"));
	GatherQueueCount = Page.Get<UTextBlock>(TEXT("WorldGatherQueueCount"));
	GatherFeedback = Page.Get<UTextBlock>(TEXT("WorldGatherCreateTip"));
	GatherCreateButton = Page.Get<UButton>(TEXT("WorldGatherCreateArmy"));
	GatherCreateLabel = Page.Get<UTextBlock>(TEXT("WorldGatherCreateLabel"));
	GatherNewSelected = Page.Get<UImage>(TEXT("WorldGatherNewTroopSelected"));
	AttackArmyChoices = Page.Get<UScrollBox>(TEXT("WorldAttackArmyChoices"));
	BindWorldHudButton(Page, TEXT("WorldGatherDispatchDismiss"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleGatherCloseClicked));
	BindWorldHudButton(Page, TEXT("WorldGatherSelectNew"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleSelectNewAttackArmy));
	BindWorldHudButton(Page, TEXT("WorldGatherCreateArmy"),
		GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleGatherCreateClicked));
}

void URokWorldMapHudWidget::SetGatherQueueVisible(bool bVisible)
{
	GatherRoot->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* ScaleSlot = Cast<UCanvasPanelSlot>(ExpandedDesignSizeBox->GetParent()->Slot))
		ScaleSlot->SetZOrder(bVisible ? 90 : 0);
	if (bVisible)
	{
		for (UWidget* Widget : ExpandedDesignCanvas->GetAllChildren())
		{
			if (Widget == GatherRoot || Widget == SelfMarkerRoot || Widget->GetName().StartsWith(TEXT("WorldMapEntityBadge_"))) continue;
			GatherHiddenChrome.Add(Widget, Widget->GetRenderOpacity());
			Widget->SetRenderOpacity(0);
		}
		if (TacticalHudWidget)
		{
			GatherHiddenChrome.Add(TacticalHudWidget.Get(), TacticalHudWidget->GetRenderOpacity());
			TacticalHudWidget->SetRenderOpacity(0);
		}
	}
	else
	{
		for (const auto& Pair : GatherHiddenChrome)
			if (auto* Widget = Pair.Key.Get()) Widget->SetRenderOpacity(Pair.Value);
		GatherHiddenChrome.Reset();
	}
}

void URokWorldMapHudWidget::OpenGatherPanel(ARokWorldMapEntityActor* Target, int64 TargetId, TOptional<bool> AttackStationAfterBattle)
{
	auto* Instance = GetGameInstance();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Army || !Net || Net->GetState() != ERokGameNetState::RoleReady)
	{
		if (WorldObjectInfoFeedbackText)
			WorldObjectInfoFeedbackText->SetText(NSLOCTEXT("RokGather", "Offline", "连接服务器后可派兵采集。"));
		return;
	}
	++GatherQueueSerial;
	GatherQueueAoiGeneration = Net->GetWorldMapAoiGeneration();
	GatherTarget = Target;
	GatherTargetId = TargetId;
	GatherAttackStation = AttackStationAfterBattle;
	GatherExistingArmyIndex = 0;
	bExistingAttackPending = false;
	GatherCreateButton->SetIsEnabled(true);
	GatherCreateLabel->SetText(FText::FromString(TEXT("创建部队")));
	GatherNewSelected->SetVisibility(ESlateVisibility::HitTestInvisible);
	AttackArmyChoices->SetVisibility(GatherAttackStation.IsSet() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (GatherAttackStation.IsSet()) RefreshAttackArmyChoices();
	const auto Snapshot = Army->GetSnapshot();
	GatherQueueCount->SetText(FText::Format(NSLOCTEXT("RokGather", "Queue", "派遣队列:{0}/{1}"),
		Snapshot.bDispatchUsedKnown ? FText::AsNumber(Snapshot.DispatchSlotsUsed) : FText::FromString(TEXT("—")),
		Snapshot.bDispatchCapacityKnown ? FText::AsNumber(Snapshot.DispatchSlotsMax) : FText::FromString(TEXT("—"))));
	GatherFeedback->SetText(NSLOCTEXT("RokGather", "CreateTip", "创建一支从你的城市出发的部队"));
	ClearWorldObjectSelection();
	if (auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn()))
	{
		GatherPreviousDxf = Camera->GetCurrentDxf();
		GatherPreviousCenter = Camera->GetViewCenterUnity();
		Camera->SetCurrentDxf(FMath::Max(GatherPreviousDxf, DispatchDxf));
		const auto* Controller = GetOwningPlayer();
		const auto* City = ResolvePlayerCity();
		FRokWorldMapProjection Projection;
		if (Controller && City && ResolveProjection(Projection))
		{
			const FVector2D Home = Projection.UeToUnity(City->GetActorLocation());
			const FVector2D Destination = Target->GetUnityPosition();
			const FVector2D Center = FVector2D::Distance(Home, Destination) < 50 ? (Home + Destination) * 0.5f : Destination;
			int32 Width = 0, Height = 0;
			Controller->GetViewportSize(Width, Height);
			FVector Middle, Left;
			const double GroundZ = Target->GetActorLocation().Z;
			if (Width > 0 && Height > 0
				&& GroundAtScreen(*Controller, {Width * 0.5, Height * 0.5}, GroundZ, Middle)
				&& GroundAtScreen(*Controller, {Width * 0.33, Height * 0.54}, GroundZ, Left))
				Camera->SetViewCenterUnity(Center + Projection.UeToUnity(Middle) - Projection.UeToUnity(Left));
			else Camera->SetViewCenterUnity(Center);
		}
	}
	SetGatherQueueVisible(true);
}

void URokWorldMapHudWidget::UpdateGatherRoute()
{
	if (!GatherRoot || !GatherRoot->IsVisible()) return;
	const auto* Target = GatherTarget.Get();
	const auto* City = ResolvePlayerCity();
	const auto* Controller = GetOwningPlayer();
	if (!Target || !Controller || Target->GetServerObjectId() != LexToString(GatherTargetId))
	{
		HandleGatherCloseClicked();
		return;
	}
	auto* ShadeSlot = CastChecked<UCanvasPanelSlot>(GatherQueueShade->Slot);
	ShadeSlot->SetSize({ShadeSlot->GetSize().X, FMath::Max(100, CurrentExpandedDesignSize.Y - 128)});
	FVector2D From, To;
	const bool bProjected = GatherExistingArmyIndex == 0 && City && CurrentRootSize.X > 0 && CurrentRootSize.Y > 0
		&& Controller->ProjectWorldLocationToScreen(City->GetActorLocation(), From, true)
		&& Controller->ProjectWorldLocationToScreen(Target->GetActorLocation(), To, true);
	int32 Count = 0;
	if (bProjected)
	{
		const FVector2D Scale = CurrentExpandedDesignSize / CurrentRootSize
			/ FMath::Max(UE_SMALL_NUMBER, UWidgetLayoutLibrary::GetViewportScale(this));
		From *= Scale;
		To *= Scale;
		Count = FMath::Clamp(FMath::FloorToInt((To - From).Size() / 17.0) - 2, 0, GatherRouteDots.Num());
	}
	const FVector2D DotOffset = CastChecked<UCanvasPanelSlot>(GetWorldOverlayArt(TEXT("GatherRouteDot")).Slot)->GetPosition();
	for (int32 Index = 0; Index < GatherRouteDots.Num(); ++Index)
	{
		auto* Dot = GatherRouteDots[Index].Get();
		Dot->SetVisibility(Index < Count ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Index < Count)
			CastChecked<UCanvasPanelSlot>(Dot->Slot)->SetPosition(FMath::Lerp(From, To, (Index + 1.5) / (Count + 2.0)) + DotOffset);
	}
}

void URokWorldMapHudWidget::HandleGatherCloseClicked()
{
	++GatherQueueSerial;
	bExistingAttackPending = false;
	SetGatherQueueVisible(false);
	if (auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn()); Camera && GatherPreviousDxf > 0)
	{
		Camera->SetCurrentDxf(GatherPreviousDxf);
		Camera->SetViewCenterUnity(GatherPreviousCenter);
	}
	GatherPreviousDxf = 0;
	GatherTarget.Reset();
	GatherTargetId = 0;
}

void URokWorldMapHudWidget::HandleGatherCreateClicked()
{
	if (bExistingAttackPending) return;
	PlayResourceClickSound();
	if (GatherAttackStation.IsSet() && GatherExistingArmyIndex > 0)
	{
		auto* Instance = GetGameInstance();
		auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
		const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		const auto OnResult = [WeakHud = TWeakObjectPtr<URokWorldMapHudWidget>(this), Serial = GatherQueueSerial]
			(int32 Index, const FString& Failure)
		{
			auto* Hud = WeakHud.Get();
			if (!Hud || Hud->GatherQueueSerial != Serial) return;
			Hud->bExistingAttackPending = false;
			Hud->GatherCreateButton->SetIsEnabled(true);
			if (Index > 0 && Failure.IsEmpty()) Hud->HandleGatherCloseClicked();
			else Hud->GatherFeedback->SetText(FText::FromString(Failure));
		};
		FString Error(TEXT("地图已重新同步，请重新选择乱军。"));
		if (Army && Net && Net->GetWorldMapAoiGeneration() == GatherQueueAoiGeneration
			&& Army->RequestExistingArmyAttack(GatherTargetId, GatherExistingArmyIndex, GatherAttackStation.GetValue(), OnResult, Error))
		{
			bExistingAttackPending = true;
			GatherCreateButton->SetIsEnabled(false);
			GatherFeedback->SetText(FText::FromString(TEXT("正在等待服务器确认出征…")));
		}
		else GatherFeedback->SetText(FText::FromString(Error));
		return;
	}
	const auto* Target = GatherTarget.Get();
	const auto* City = ResolvePlayerCity();
	FString Error;
	FRokWorldMapProjection Projection;
	const double Distance = Target && City && ResolveProjection(Projection)
		? (Projection.UeToUnity(City->GetActorLocation()) - Target->GetUnityPosition()).Size() : 0;
	const auto* Entities = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokWorldMapEntitySubsystem>() : nullptr;
	const double ResourceRadius = Target && Entities ? Entities->GetResourceRadius(Target->GetSourceId()) : 0;
	if (Target && FeatureScreenWidget && FeatureScreenWidget->OpenGatherArmy(GatherTargetId, Distance, ResourceRadius, Error, GatherAttackStation))
		HandleGatherCloseClicked();
	else if (GatherFeedback && !Error.IsEmpty())
		GatherFeedback->SetText(FText::FromString(Error));
}
