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
#include "RokGameNetSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokServerArmySubsystem.h"
#include "RokUiAnimatedButton.h"
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

void URokWorldMapHudWidget::HandleWorldMarchQueueToggle()
{
	bWorldMarchQueueExpanded = !bWorldMarchQueueExpanded;
	RefreshWorldMarchQueue();
}

void URokWorldMapHudWidget::RefreshWorldMarchQueue()
{
	if (!WorldMarchQueueRoot) return;
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
	WorldMarchQueueArrow->SetRenderTransformAngle(bWorldMarchQueueExpanded ? 180.0f : 0.0f);
	CastChecked<UCanvasPanelSlot>(WorldMarchQueueRoot->Slot)->SetSize(
		{102, 31.0 + (bWorldMarchQueueExpanded ? 86.0 * FMath::Min(3, Snapshot.Marches.Num()) : 0.0)});

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
		RowSize->SetHeightOverride(86.0f);
		WorldMarchQueueList->AddChild(RowSize);
		auto* Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(),
			MakeUniqueObjectName(WidgetTree, URokUnityIndexedButton::StaticClass(), FName(*(TEXT("WorldMarchArmy_") + Suffix))));
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle ButtonStyle;
		ButtonStyle.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush)
			.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Button->SetStyle(ButtonStyle);
		Button->ConfigureAction(March.ArmyIndex);
		Button->OnIndexedClicked.AddUObject(this, &URokWorldMapHudWidget::HandleWorldMarchClicked);
		RowSize->SetContent(Button);

		auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>();
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		Button->SetContent(Row);
		FRokUnityCatalogHero Hero;
		const bool bHasHero = Catalog && Catalog->GetHero(March.MainHeroId, Hero);
		const bool bHeadAtlasFrame = Hero.Rare == 4 || Hero.Rare == 5;
		const FIntRect Frame = Hero.Rare == 5 ? FIntRect(593, 1895, 727, 2047)
			: Hero.Rare == 4 ? FIntRect(729, 1895, 863, 2047) : FIntRect(1, 53, 135, 205);
		auto* RarityFrame = MakeAtlasImage(NAME_None, bHeadAtlasFrame ? HeadAtlas : HeroUi, Frame,
			bHeadAtlasFrame ? HeadAtlasSize : HeroUiSize, {78, 86});
		if (Hero.Rare == 3) RarityFrame->SetColorAndOpacity(FLinearColor(0.55f, 0.78f, 1.0f));
		if (Hero.Rare == 2) RarityFrame->SetColorAndOpacity(FLinearColor(0.62f, 1.0f, 0.70f));
		PlaceOnCanvas(Row, RarityFrame, {12, 0}, {78, 86});
		if (bHasHero && Hero.IconAtlas == TEXT("hero_head") && Hero.IconWidth > 0 && Hero.IconHeight > 0)
		{
			PlaceOnCanvas(Row, MakeAtlasImage(NAME_None, HeadAtlas,
				FIntRect(Hero.IconX, Hero.IconY, Hero.IconX + Hero.IconWidth, Hero.IconY + Hero.IconHeight),
				HeadAtlasSize, {73, 81}), {14.5, 2}, {73, 81});
		}
		if (HighlightedArmy == March.ArmyIndex)
		{
			PlaceOnCanvas(Row, MakeAtlasImage(NAME_None, HeroUi, FIntRect(1905, 356, 2046, 511), HeroUiSize, {86, 90}),
				{8, -3}, {86, 90});
		}
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
		PlaceOnCanvas(Row, MakeAtlasImage(NAME_None, MapUi, FIntRect(StatusOrigin, StatusOrigin + FIntPoint(46, 46)),
			MapUiSize, {29, 29}), {70, 58}, {29, 29});
		Button->SetToolTipText(bHasHero ? FText::Format(NSLOCTEXT("RokGather", "ArmyTooltip", "{0} · {1}"),
			FText::FromString(Hero.DisplayNameZhCn.IsEmpty() ? Hero.DisplayNameEn : Hero.DisplayNameZhCn), Status) : Status);
	}
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
									const FVector2D DotSize(8, 5.5);
									auto* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
										*FString::Printf(TEXT("WorldAttackRouteDot_%d"), DotsShown));
									Dot->SetBrush(FSlateRoundedBoxBrush(AttackRouteGreen,
										FLinearColor(0.027f, 0.16f, 0.08f, 0.92f), 1.f, DotSize));
									PlaceOnCanvas(ExpandedDesignCanvas, Dot, FVector2D::ZeroVector,
										DotSize, FAnchors(), FVector2D(0.5f))->SetZOrder(5);
									AttackRouteDots.Add(Dot);
								}
								auto* Dot = AttackRouteDots[DotsShown++].Get();
								Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
								CastChecked<UCanvasPanelSlot>(Dot->Slot)->SetPosition(From + Direction * Distance);
								Dot->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
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
				auto* Cross = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
					*FString::Printf(TEXT("WorldAttackRouteDestination_%d"), DestinationsShown));
				Cross->SetBrushFromTexture(AttackRouteDestinationTexture);
				Cross->SetColorAndOpacity(AttackRouteGreen);
				PlaceOnCanvas(ExpandedDesignCanvas, Cross, FVector2D::ZeroVector,
					{43, 34}, FAnchors(), FVector2D(0.5f))->SetZOrder(5);
				AttackRouteDestinations.Add(Cross);
			}
			auto* Cross = AttackRouteDestinations[DestinationsShown++].Get();
			Cross->SetVisibility(ESlateVisibility::HitTestInvisible);
			CastChecked<UCanvasPanelSlot>(Cross->Slot)->SetPosition(From);
		}
	}
	for (int32 Index = DotsShown; Index < AttackRouteDots.Num(); ++Index)
		AttackRouteDots[Index]->SetVisibility(ESlateVisibility::Collapsed);
	for (int32 Index = DestinationsShown; Index < AttackRouteDestinations.Num(); ++Index)
		AttackRouteDestinations[Index]->SetVisibility(ESlateVisibility::Collapsed);
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

			if (!AttackMarchMarkers.IsValidIndex(Shown))
			{
				auto* Marker = MakeAtlasImage(*FString::Printf(TEXT("WorldAttackMarchMarker_%d"), Shown),
					MapUi, FIntRect(257, 72, 303, 118), MapUiSize, {44, 44});
				PlaceOnCanvas(ExpandedDesignCanvas, Marker, FVector2D::ZeroVector, {44, 44})->SetZOrder(71);
				AttackMarchMarkers.Add(Marker);
			}
			auto* Marker = AttackMarchMarkers[Shown++].Get();
			const FIntPoint Origin = bBattle ? FIntPoint(960, 285) : FIntPoint(257, 72);
			Marker->SetBrush(MakeAtlasBrush(LoadUiTexture(MapUi),
				FIntRect(Origin, Origin + FIntPoint(46, 46)), MapUiSize, {44, 44}));
			Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
			const FVector2D DesignPosition = ScreenPosition * CurrentExpandedDesignSize / CurrentRootSize;
			CastChecked<UCanvasPanelSlot>(Marker->Slot)->SetPosition(DesignPosition - FVector2D(22, bBattle ? 72 : 32));
			if (!bBattle || !Net) continue;
			const int64 Objects[] = {March.MapObjectId, March.TargetObjectId};
			for (int32 Side = 0; Side < UE_ARRAY_COUNT(Objects); ++Side)
			{
				int64 Damage = 0;
				double Age = 0;
				if (!Net->GetRecentBattleDamage(Objects[Side], Damage, Age)) continue;
				if (!AttackDamageLabels.IsValidIndex(DamageShown))
				{
					auto* Label = MakeHudText(*FString::Printf(TEXT("WorldAttackDamage_%d"), DamageShown), 26, FLinearColor::White);
					Label->SetJustification(ETextJustify::Center);
					PlaceOnCanvas(ExpandedDesignCanvas, Label, FVector2D::ZeroVector, {105, 35})->SetZOrder(72);
					AttackDamageLabels.Add(Label);
				}
				auto* Label = AttackDamageLabels[DamageShown++].Get();
				Label->SetVisibility(ESlateVisibility::HitTestInvisible);
				Label->SetColorAndOpacity(Side == 0 ? FLinearColor(1.0f, 0.38f, 0.29f) : FLinearColor(1.0f, 0.80f, 0.45f));
				Label->SetRenderOpacity(FMath::Clamp(1.5 - Age, 0.0, 1.0));
				Label->SetText(FText::Format(NSLOCTEXT("RokBattle", "DamageNumber", "−{0}"), FText::AsNumber(Damage)));
				CastChecked<UCanvasPanelSlot>(Label->Slot)->SetPosition(DesignPosition + FVector2D(Side == 0 ? -112 : 7, -60 - Age * 28));
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
				if (!WorldSearchResultGuide)
				{
					if (auto* Texture = LoadTransientUiTexture(RokDataPaths::Resolve(TEXT("WorldSearch/finger001.png"))))
					{
						WorldSearchResultGuide = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldSearchResultGuide"));
						WorldSearchResultGuide->SetBrushFromTexture(Texture);
						WorldSearchResultGuide->SetRenderTransformAngle(180.0f);
						WorldSearchResultGuide->SetVisibility(ESlateVisibility::HitTestInvisible);
					}
				}
				if (WorldSearchResultGuide)
					PlaceOnCanvas(Entry.Root, WorldSearchResultGuide, FVector2D(7.5, -190), FVector2D(49, 149));
			}
			return;
		}
	}
}

void URokWorldMapHudWidget::BuildBarbarianInfo()
{
	// UI_Pop_WorldObjectMonster and UI_Item_WorldObjInfoTDrop, scaled into the 1440-wide HUD.
	const FVector2D Size(486, 434);
	const FLinearColor Ink(0.025f, 0.022f, 0.018f);
	WorldBarbarianInfoPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldBarbarianInfo"));
	WorldBarbarianInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(WorldObjectInfoRoot, WorldBarbarianInfoPanel, FVector2D::ZeroVector, Size);
	AddWorldPopupFrame(WorldBarbarianInfoPanel, Size, TEXT("WorldBarbarianInfoPatch"));
	PlaceOnCanvas(WorldBarbarianInfoPanel, MakeTransparentHitButton(TEXT("WorldBarbarianInputBlocker")),
		FVector2D::ZeroVector, Size);

	BarbarianNormalPage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BarbarianDetailsPage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
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
	BarbarianRewards = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BarbarianRewards->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(BarbarianNormalPage, BarbarianRewards, {214, 122}, {234, 152});

	BarbarianTimer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
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

void URokWorldMapHudWidget::SetBarbarianDetailsExpanded(bool bExpanded)
{
	BarbarianNormalPage->SetVisibility(bExpanded ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	BarbarianDetailsPage->SetVisibility(bExpanded ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (bExpanded)
	{
		if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("WorldBarbarianDescriptionScroll"))))
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
			PlaceOnCanvas(BarbarianRewards, Cell, {79.0 * (Index % 3), 78.0 * (Index / 3)}, {74, 74});
			auto* Frame = WidgetTree->ConstructWidget<UImage>();
			Frame->SetBrush(FeatureScreenWidget->MakeRewardQualityBrush(Reward.Quality, {74, 74}));
			Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaceOnCanvas(Cell, Frame, {0, 0}, {74, 74});
			if (!Reward.AtlasPath.IsEmpty())
				PlaceOnCanvas(Cell, MakeAtlasImage(*(Prefix + TEXT("Icon")), *Reward.AtlasPath,
					Reward.IconRect, Reward.IconAtlasSize, {64, 64}), {5, 5}, {64, 64});
			auto* Top = MakeHudText(*(Prefix + TEXT("Value")), 12, FLinearColor::White);
			Top->SetJustification(ETextJustify::Center);
			Top->SetText(Reward.TopLabel);
			PlaceOnCanvas(Cell, Top, {3, 3}, {68, 18});
			auto* Count = MakeHudText(*(Prefix + TEXT("Count")), 15, FLinearColor::White);
			Count->SetJustification(ETextJustify::Right);
			Count->SetText(FText::AsNumber(Reward.Quantity));
			PlaceOnCanvas(Cell, Count, {3, 54}, {65, 18});
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
		const FName Key(*FString::Printf(TEXT("WorldAttackArmy_%d"), March.ArmyIndex));
		auto* Row = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Key);
		Row->ConfigureAction(March.ArmyIndex);
		Row->OnIndexedClicked.AddUObject(this, &URokWorldMapHudWidget::SelectAttackArmy);
		if (auto* Atlas = LoadUiTexture(Common))
		{
			FButtonStyle Style;
			const FSlateBrush Background = MakeAtlasBrush(Atlas,
				March.ArmyIndex == GatherExistingArmyIndex ? FIntRect(1766, 586, 2040, 670) : FIntRect(1098, 571, 1300, 652),
				CommonSize, {130, 68});
			Style.SetNormal(Background).SetHovered(Background).SetPressed(Background);
			Row->SetStyle(Style);
		}
		auto* Label = MakeHudText(NAME_None, 17, FLinearColor::White, true);
		Label->SetJustification(ETextJustify::Center);
		Label->SetText(FText::FromString(Caption));
		Row->SetContent(Label);
		AttackArmyChoices->AddChild(Row);
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

	auto* Back = MakeTransparentHitButton(TEXT("WorldGatherDispatchDismiss"));
	Back->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleGatherCloseClicked);
	auto* BackSlot = GatherRoot->AddChildToCanvas(Back);
	BackSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	BackSlot->SetOffsets(FMargin(0));

	// The path is a preview drawn between the actual city and resource point.
	for (int32 Index = 0; Index < 64; ++Index)
	{
		auto* Dot = WidgetTree->ConstructWidget<UImage>();
		Dot->SetBrush(FSlateColorBrush(FLinearColor(0.94f, 0.96f, 1.0f, 0.96f)));
		Dot->SetRenderTransformAngle(45.0f);
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(GatherRoot, Dot, FVector2D::ZeroVector, {5.4f, 5.4f}, FAnchors(), FVector2D(0.5f));
		GatherRouteDots.Add(Dot);
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
	CastChecked<UCanvasPanelSlot>(GatherQueueShade->Slot)->SetSize({146, FMath::Max(100, CurrentExpandedDesignSize.Y - 128)});
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
	for (int32 Index = 0; Index < GatherRouteDots.Num(); ++Index)
	{
		auto* Dot = GatherRouteDots[Index].Get();
		Dot->SetVisibility(Index < Count ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Index < Count)
			CastChecked<UCanvasPanelSlot>(Dot->Slot)->SetPosition(FMath::Lerp(From, To, (Index + 1.5) / (Count + 2.0)));
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
