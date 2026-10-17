#include "RokWorldMapHudWidget.h"
#include "RokPowerGainWidget.h"
#include "RokMainInterfaceModuleAnimation.h"
#include "RokWorldMapHomeIndicator.h"
#include "RokUiAnimatedButton.h"
#include "RokAudioRouterSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokStrategicResourceLayer.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapEntitySubsystem.h"
#include "RokWorldMapPlayerController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
const TCHAR* MainMenuAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
const TCHAR* CommonAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* PlayerHeadAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head.rok_hud_atlas_player_head");
const TCHAR* VipAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip");
const TCHAR* MapAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map.rok_hud_atlas_map");
const TCHAR* FoodIconPath =
	TEXT("/Game/RokPrototype/UI/Processed/rok_food_icon.rok_food_icon");
const TCHAR* WoodIconPath =
	TEXT("/Game/RokPrototype/UI/Processed/rok_wood_icon.rok_wood_icon");
const TCHAR* StoneIconPath =
	TEXT("/Game/RokPrototype/UI/Processed/rok_stone_icon.rok_stone_icon");
const TCHAR* GoldIconPath =
	TEXT("/Game/RokPrototype/UI/Processed/rok_gold_icon.rok_gold_icon");

const FIntPoint MainMenuAtlasSize(1024, 1024);
const FIntPoint CommonAtlasSize(2048, 1024);
const FIntPoint PlayerHeadAtlasSize(512, 2048);
const FIntPoint VipAtlasSize(512, 1024);
const FIntPoint MapAtlasSize(1024, 512);

FIntRect SpriteRect(
	int32 X,
	int32 Y,
	int32 Width,
	int32 Height)
{
	return FIntRect(X, Y, X + Width, Y + Height);
}

const FIntRect PlayerBackgroundRect = SpriteRect(1, 872, 314, 136);
const FIntRect PlayerArrowRect = SpriteRect(647, 516, 65, 42);
const FIntRect PowerIconRect = SpriteRect(396, 587, 17, 15);
const FIntRect PositionSearchRect = SpriteRect(796, 809, 22, 22);
const FIntRect FavoriteRect = SpriteRect(1, 475, 36, 36);
const FIntRect PanelBackgroundRect = SpriteRect(499, 532, 24, 38);
const FIntRect ChatBarRect = SpriteRect(1, 1010, 768, 13);
const FIntRect ChatRect = SpriteRect(976, 983, 43, 40);
const FIntRect SearchRect = SpriteRect(417, 812, 92, 92);
const FIntRect GlobeRect = SpriteRect(570, 908, 100, 100);
const FIntRect CityRect = SpriteRect(1, 667, 127, 148);
const FIntRect MailRect = SpriteRect(864, 833, 88, 88);
const FIntRect RedPointRect = SpriteRect(2018, 994, 27, 29);
const FIntRect WorldObjectPopArrowRect = SpriteRect(191, 270, 12, 15);
const FIntRect WorldObjectPopBackgroundRect = SpriteRect(341, 80, 58, 55);

const FIntRect PlayerFrameRect = SpriteRect(214, 984, 211, 211);
const FIntRect PlayerHead1000Rect = SpriteRect(1, 345, 211, 211);
const FIntRect VipIcon100Rect = SpriteRect(1, 325, 138, 138);

const FIntRect MiniMapBackgroundRect = SpriteRect(431, 351, 230, 160);
const FIntRect RelationLeaderRect = SpriteRect(353, 19, 42, 42);
const FIntRect RelationFriendRect = SpriteRect(491, 41, 42, 42);
const FIntRect RelationFortressRect = SpriteRect(447, 52, 42, 42);

constexpr float UnityReferenceWidth = 1440.0f;
constexpr float UnityReferenceHeight = 810.0f;
const FVector2D MiniMapContentSize(193.88f, 138.16f);
const FVector2D MiniMapContentOffset(18.0f, 11.0f);
const FVector2D SelfMarkerSize(100.0f, 100.0f);
const FVector2D WorldObjectInfoSize(390.0f, 270.0f);
const FVector2D WorldObjectInfoPanelSize(390.0f, 250.0f);

FText FormatCompactNumber(int64 Value)
{
	const double AbsoluteValue = FMath::Abs(static_cast<double>(Value));
	if (AbsoluteValue >= 1000000.0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%.1fM"),
			static_cast<double>(Value) / 1000000.0));
	}
	if (AbsoluteValue >= 10000.0)
	{
		return FText::FromString(FString::Printf(
			TEXT("%.1fK"),
			static_cast<double>(Value) / 1000.0));
	}
	return FText::AsNumber(Value);
}
}

void URokWorldMapSelectionInputWidget::SetOwningHud(
	URokWorldMapHudWidget* InOwningHud)
{
	OwningHud = InOwningHud;
}

TSharedRef<SWidget> URokWorldMapSelectionInputWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* InputSurface =
			WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				TEXT("WorldMapSelectionInputSurface"));
		InputSurface->SetBrushColor(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.001f));
		InputSurface->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = InputSurface;
	}
	return Super::RebuildWidget();
}

FReply URokWorldMapSelectionInputWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(
			InGeometry,
			InMouseEvent);
	}

	if (OwningHud.IsValid())
	{
		if (ARokWorldMapPlayerController* WorldMapController =
				Cast<ARokWorldMapPlayerController>(
					OwningHud->GetOwningPlayer()))
		{
			float MouseX = 0.0f;
			float MouseY = 0.0f;
			if (WorldMapController->GetMousePosition(MouseX, MouseY))
			{
				const bool bSelected =
					WorldMapController->SelectWorldMapActorAtScreenPosition(
					FVector2D(MouseX, MouseY));
				if (FParse::Param(
						FCommandLine::Get(),
						TEXT("RokWorldMapDebugHud")))
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Rok world-map selection surface click: screen=(%.2f,%.2f) selected=%s."),
						MouseX,
						MouseY,
						bSelected ? TEXT("true") : TEXT("false"));
				}
			}
			else
			{
				WorldMapController->ClearWorldMapSelection();
			}
		}
	}
	return FReply::Handled();
}

void URokWorldMapMiniMapInputWidget::SetOwningHud(
	URokWorldMapHudWidget* InOwningHud)
{
	OwningHud = InOwningHud;
}

TSharedRef<SWidget> URokWorldMapMiniMapInputWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* InputSurface =
			WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				TEXT("WorldMapMiniMapInputSurface"));
		InputSurface->SetBrushColor(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.001f));
		InputSurface->SetVisibility(ESlateVisibility::Visible);
		WidgetTree->RootWidget = InputSurface;
	}
	return Super::RebuildWidget();
}

FReply URokWorldMapMiniMapInputWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bDragging = true;
	FocusAtPointer(InGeometry, InMouseEvent);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply URokWorldMapMiniMapInputWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bDragging
		|| !InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	FocusAtPointer(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply URokWorldMapMiniMapInputWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bDragging
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	FocusAtPointer(InGeometry, InMouseEvent);
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void URokWorldMapMiniMapInputWidget::FocusAtPointer(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (OwningHud.IsValid())
	{
		OwningHud->FocusMiniMapLocalPoint(
			InGeometry.AbsoluteToLocal(
				InMouseEvent.GetScreenSpacePosition()),
			InGeometry.GetLocalSize());
	}
}

FVector2D URokWorldMapHudWidget::ComputeMiniMapPoint(
	const FRokWorldMapProjection& Projection,
	const FVector2D& UnityPosition,
	const FVector2D& MiniMapSize)
{
	const FVector2D BoundsSize = Projection.UnityBoundsMax
		- Projection.UnityBoundsMin;
	const double SafeWidth = FMath::Max(
		FMath::Abs(BoundsSize.X),
		UE_DOUBLE_SMALL_NUMBER);
	const double SafeHeight = FMath::Max(
		FMath::Abs(BoundsSize.Y),
		UE_DOUBLE_SMALL_NUMBER);
	const FVector2D ClampedUnity = Projection.ClampUnity(UnityPosition);
	const double NormalizedX = FMath::Clamp(
		(ClampedUnity.X - Projection.UnityBoundsMin.X) / SafeWidth,
		0.0,
		1.0);
	const double NormalizedY = FMath::Clamp(
		(ClampedUnity.Y - Projection.UnityBoundsMin.Y) / SafeHeight,
		0.0,
		1.0);
	return FVector2D(
		NormalizedX * MiniMapSize.X,
		(1.0 - NormalizedY) * MiniMapSize.Y);
}

FVector2D URokWorldMapHudWidget::ClampMarkerTopLeft(
	const FVector2D& DesiredTopLeft,
	const FVector2D& MarkerSize,
	const FVector2D& DesignSize,
	float Margin)
{
	const float SafeMargin = FMath::Max(Margin, 0.0f);
	const FVector2D MaxTopLeft(
		FMath::Max(SafeMargin, DesignSize.X - MarkerSize.X - SafeMargin),
		FMath::Max(SafeMargin, DesignSize.Y - MarkerSize.Y - SafeMargin));
	return FVector2D(
		FMath::Clamp(
			DesiredTopLeft.X,
			SafeMargin,
			MaxTopLeft.X),
		FMath::Clamp(
			DesiredTopLeft.Y,
			SafeMargin,
			MaxTopLeft.Y));
}

FVector2D URokWorldMapHudWidget::ComputeUnityPositionFromMiniMapPoint(
	const FRokWorldMapProjection& Projection,
	const FVector2D& MiniMapPoint,
	const FVector2D& MiniMapSize)
{
	const double SafeWidth = FMath::Max(
		FMath::Abs(MiniMapSize.X),
		UE_DOUBLE_SMALL_NUMBER);
	const double SafeHeight = FMath::Max(
		FMath::Abs(MiniMapSize.Y),
		UE_DOUBLE_SMALL_NUMBER);
	const double NormalizedX = FMath::Clamp(
		MiniMapPoint.X / SafeWidth,
		0.0,
		1.0);
	const double NormalizedY = FMath::Clamp(
		MiniMapPoint.Y / SafeHeight,
		0.0,
		1.0);
	const FVector2D BoundsSize =
		Projection.UnityBoundsMax - Projection.UnityBoundsMin;
	return Projection.ClampUnity(FVector2D(
		Projection.UnityBoundsMin.X + NormalizedX * BoundsSize.X,
		Projection.UnityBoundsMin.Y
			+ (1.0 - NormalizedY) * BoundsSize.Y));
}

bool URokWorldMapHudWidget::ShouldShowEntityForFilter(
	const FString& EntityKind,
	ERokWorldMapHudFilter Filter)
{
	FString NormalizedKind = EntityKind;
	NormalizedKind.TrimStartAndEndInline();
	NormalizedKind.ToLowerInline();

	const bool bExploreEntity =
		NormalizedKind == TEXT("village")
		|| NormalizedKind == TEXT("cave")
		|| NormalizedKind == TEXT("village_cave")
		|| NormalizedKind == TEXT("explore");
	const bool bResourceEntity =
		IsResourceEntityKind(NormalizedKind)
		|| NormalizedKind == TEXT("resource")
		|| NormalizedKind == TEXT("alliance_resource");
	const bool bAllianceEntity =
		NormalizedKind == TEXT("alliance")
		|| NormalizedKind == TEXT("alliance_city")
		|| NormalizedKind == TEXT("alliance_building")
		|| NormalizedKind == TEXT("alliance_fortress")
		|| NormalizedKind == TEXT("alliance_resource")
		|| NormalizedKind == TEXT("ally_city");
	const bool bMarkerEntity =
		NormalizedKind == TEXT("marker")
		|| NormalizedKind == TEXT("bookmark")
		|| NormalizedKind == TEXT("personal_marker");
	const bool bFilterableEntity =
		bExploreEntity
		|| bResourceEntity
		|| bAllianceEntity
		|| bMarkerEntity;

	if (!bFilterableEntity)
	{
		// Cities, barbarian camps, and other normal gameplay actors are not
		// minimap overlay categories and remain visible in every filter mode.
		return true;
	}

	switch (Filter)
	{
	case ERokWorldMapHudFilter::Alliance:
		return bAllianceEntity;
	case ERokWorldMapHudFilter::Explore:
		return bExploreEntity;
	case ERokWorldMapHudFilter::Resources:
		return bResourceEntity;
	case ERokWorldMapHudFilter::Markers:
		return bMarkerEntity;
	case ERokWorldMapHudFilter::None:
	default:
		return false;
	}
}

bool URokWorldMapHudWidget::IsResourceEntityKind(
	const FString& EntityKind)
{
	FString NormalizedKind = EntityKind;
	NormalizedKind.TrimStartAndEndInline();
	NormalizedKind.ToLowerInline();
	return NormalizedKind == TEXT("food")
		|| NormalizedKind == TEXT("wood")
		|| NormalizedKind == TEXT("stone")
		|| NormalizedKind == TEXT("gold");
}

TSharedRef<SWidget> URokWorldMapHudWidget::RebuildWidget()
{
	BuildWidgetTree();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD widget tree rebuilt at 1440x810 reference resolution."));
	return Super::RebuildWidget();
}

void URokWorldMapHudWidget::NativeDestruct()
{
	// The layer belongs to this HUD, not to the persistent world. Reopening the
	// HUD in the same world must not leave a second heatmap or fixed-point set.
	if (StrategicResourceLayer.IsValid()) StrategicResourceLayer->Destroy();
	StrategicResourceLayer.Reset();
	if (StrategicExplorationLayer.IsValid()) StrategicExplorationLayer->Destroy();
	StrategicExplorationLayer.Reset();
	Super::NativeDestruct();
}

void URokWorldMapHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshRuntimeValues();

	FString RequestedUi;
	if (FParse::Value(
			FCommandLine::Get(),
			TEXT("RokWorldMapOpenUi="),
			RequestedUi))
	{
		RequestedUi.TrimStartAndEndInline();
		RequestedUi.ToLowerInline();
		if (RequestedUi == TEXT("profile"))
		{
			HandleOpenPlayerProfileClicked();
		}
		else if (RequestedUi == TEXT("vip"))
		{
			HandleOpenVipClicked();
		}
		else if (
			RequestedUi == TEXT("position")
			|| RequestedUi == TEXT("coordinate"))
		{
			HandleOpenPositionSearchClicked();
		}
		else if (
			RequestedUi == TEXT("markers")
			|| RequestedUi == TEXT("bookmarks"))
		{
			HandleOpenMapMarkersClicked();
		}
		else if (RequestedUi == TEXT("chat"))
		{
			HandleOpenChatClicked();
		}
		else if (
			RequestedUi == TEXT("server")
			|| RequestedUi == TEXT("kingdom")
			|| RequestedUi == TEXT("globe"))
		{
			HandleOpenServerListClicked();
		}
	}
}

void URokWorldMapHudWidget::SetPowerPresentation(int32 Value)
{
	if (PowerGain) PowerGain->ObservePower(PowerCounter.Target, Value, IsVisible() && GetRenderOpacity() > 0);
	PowerCounter.SetTarget(Value);
	if (PowerText) PowerText->SetText(PowerCounter.Target > 0 ? FText::AsNumber(PowerCounter.Value)
		: NSLOCTEXT("RokWorldMapHud", "RuntimePowerUnavailable", "--"));
}

void URokWorldMapHudWidget::AdvancePowerPresentation(float DeltaSeconds)
{
	if (PowerGain) PowerGain->Advance(DeltaSeconds);
	const int32 PreviousPower = PowerCounter.Value;
	PowerCounter.Advance(DeltaSeconds);
	if (PreviousPower != PowerCounter.Value) SetPowerPresentation(PowerCounter.Target);
}

void URokWorldMapHudWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	AdvancePowerPresentation(InDeltaTime);
	CurrentRootSize = MyGeometry.GetLocalSize();
	UpdateExpandedDesignResolution(CurrentRootSize);
	AdvanceMiniMapAnimation(InDeltaTime);
	AdvanceStrategicModuleAnimation(InDeltaTime);
	AdvanceWorldObjectAnimation(InDeltaTime);
	AdvanceWorldObjectPageAnimation(InDeltaTime);
	if (TacticalHudWidget) TacticalHudWidget->AdvanceWorldMapModuleAnimation(InDeltaTime);
	AdvanceFilterSelectionAnimation(InDeltaTime);
	AdvanceFilterInputFeedback(InDeltaTime);
	if (const ARokWorldMapCameraPawn* CameraPawn =
			Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn()))
	{
		UpdateZoomPresentation(CameraPawn->GetCurrentDxf());
	}
	UpdatePlayerMarkerProjection();
	ApplyEntityFilter();
	UpdateEntityBadges();
	UpdateWorldObjectInfoProjection();
	UpdateMiniMapMarkers();

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.0f;
		RefreshRuntimeValues();
	}
}

void URokWorldMapHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || ExpandedDesignCanvas)
	{
		return;
	}

	UCanvasPanel* RootCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("RokWorldMapHudRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;

	UScaleBox* ExpandScaleBox =
		WidgetTree->ConstructWidget<UScaleBox>(
			UScaleBox::StaticClass(),
			TEXT("WorldMapExpandScaleBox"));
	ExpandScaleBox->SetStretch(EStretch::ScaleToFit);
	ExpandScaleBox->SetStretchDirection(EStretchDirection::Both);
	ExpandScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* ScaleSlot =
		RootCanvas->AddChildToCanvas(ExpandScaleBox);
	ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ScaleSlot->SetOffsets(FMargin(0.0f));

	ExpandedDesignSizeBox =
		WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("WorldMapExpandedDesignSize"));
	ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
	ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	ExpandedDesignSizeBox->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
	ExpandScaleBox->SetContent(ExpandedDesignSizeBox);

	ExpandedDesignCanvas =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapExpandedDesignCanvas"));
	ExpandedDesignCanvas->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
	ExpandedDesignSizeBox->SetContent(ExpandedDesignCanvas);
	URokWorldMapSelectionInputWidget* SelectionInput =
		WidgetTree->ConstructWidget<URokWorldMapSelectionInputWidget>(
			URokWorldMapSelectionInputWidget::StaticClass(),
			TEXT("WorldMapSelectionInput"));
	SelectionInput->SetOwningHud(this);
	SelectionInput->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* SelectionInputSlot =
			ExpandedDesignCanvas->AddChildToCanvas(SelectionInput))
	{
		SelectionInputSlot->SetAnchors(
			FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		SelectionInputSlot->SetOffsets(FMargin(0.0f));
		SelectionInputSlot->SetZOrder(-100);
	}

	BuildPlayerIdentity();
	BuildCoordinatePanel();
	BuildMiniMap();
	BuildRelationFilters();
	BuildBottomControls();
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		const URokWorldMapEntitySubsystem* EntitySubsystem =
			GameInstance->GetSubsystem<
				URokWorldMapEntitySubsystem>();
		if (EntitySubsystem)
		{
			LevelBadgeTexture = LoadTransientUiTexture(
				EntitySubsystem->GetLevelBadgeTexturePath());
		}
	}
	BuildSelfMarker();
	BuildEntityBadges();
	BuildWorldObjectInfo();
	BuildServerListOverlay();

	ModalWidget = WidgetTree->ConstructWidget<URokUnityModalWidget>(
		URokUnityModalWidget::StaticClass(),
		TEXT("WorldMapUnityModalWidget"));
	ModalWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ModalSlot = RootCanvas->AddChildToCanvas(ModalWidget);
	ModalSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ModalSlot->SetOffsets(FMargin(0.0f));
	ModalSlot->SetZOrder(100);

	FeatureScreenWidget =
		WidgetTree->ConstructWidget<URokUnityFeatureScreenWidget>(
			URokUnityFeatureScreenWidget::StaticClass(),
			TEXT("WorldMapUnityFeatureScreenWidget"));
	FeatureScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* FeatureScreenSlot =
		RootCanvas->AddChildToCanvas(FeatureScreenWidget);
	FeatureScreenSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	FeatureScreenSlot->SetOffsets(FMargin(0.0f));
	FeatureScreenSlot->SetZOrder(110);
	ModalWidget->SetFeatureScreenWidget(FeatureScreenWidget);

	TacticalHudWidget = WidgetTree->ConstructWidget<URokMainHudWidget>(
		URokMainHudWidget::StaticClass(),
		TEXT("WorldMapTacticalHud"));
	TacticalHudWidget->ConfigureWorldMapTacticalOverlay(
		ModalWidget,
		FeatureScreenWidget);
	TacticalHudWidget->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* TacticalSlot = RootCanvas->AddChildToCanvas(TacticalHudWidget);
	TacticalSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	TacticalSlot->SetOffsets(FMargin(0.0f));
	TacticalSlot->SetZOrder(10);
}

void URokWorldMapHudWidget::UpdateZoomPresentation(float Dxf)
{
	if (!ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(Dxf))
		HideWorldObjectInfo();
	// MainInterfaceMediator.GetOpenLodUILevel uses dispatch=2997 as
	// the last tactical value; OpenLodUi row 9 enables Map after it.
	const bool bShowMiniMap = FMath::IsFinite(Dxf) && Dxf > 2997.0f;
	if (MiniMapRoot && bShowMiniMap != bMiniMapTargetVisible)
	{
		bMiniMapTargetVisible = bShowMiniMap;
		MiniMapAnimationSeconds = 0.0f;
		AdvanceMiniMapAnimation(0.0f);
	}
	// OpenLodUi rows CurAge/Events/HotTask/Features/PlayerResources/Mail:
	// tactical UI ends at dispatch (2997), independently of world icon LOD (3000).
	bTacticalPresentation = FMath::IsFinite(Dxf) && Dxf >= 0.0f && Dxf <= 2997.0f;
	if (TacticalHudWidget)
	{
		TacticalHudWidget->SetWorldMapTacticalVisible(bTacticalPresentation);
	}
	// OpenLodUi LodMenu/Server begin in Strategic, after TacticsToStrategy2.
	const bool bShowStrategic = FMath::IsFinite(Dxf) && Dxf > 5400.0f;
	if (bShowStrategic != bStrategicModulesVisible)
	{
		bStrategicModulesVisible = bShowStrategic;
		StrategicModuleSeconds = 0.0f;
		AdvanceStrategicModuleAnimation(0.0f);
	}
}

void URokWorldMapHudWidget::AdvanceStrategicModuleAnimation(float DeltaSeconds)
{
	if (FMath::IsFinite(DeltaSeconds))
		StrategicModuleSeconds = FMath::Clamp(StrategicModuleSeconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.25f);
	const auto Apply = [&](UWidget* Widget, bool bLeft)
	{
		if (!Widget) return;
		const FVector2D Pose = bLeft
			? RokMainInterfaceModuleAnimation::EvaluateLeft(bStrategicModulesVisible, StrategicModuleSeconds)
			: RokMainInterfaceModuleAnimation::EvaluateRight(bStrategicModulesVisible, StrategicModuleSeconds);
		Widget->SetRenderTranslation(FVector2D(Pose.X, 0));
		Widget->SetRenderOpacity(Pose.Y);
		Widget->SetVisibility(bStrategicModulesVisible
			? (bLeft ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Visible)
			: (StrategicModuleSeconds < 0.25f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed));
	};
	Apply(StrategicFilterRoot, true);
	Apply(StrategicGlobeButton, false);
}

void URokWorldMapHudWidget::AdvanceMiniMapAnimation(float DeltaSeconds)
{
	if (!MiniMapRoot) return;
	if (FMath::IsFinite(DeltaSeconds))
		MiniMapAnimationSeconds = FMath::Clamp(MiniMapAnimationSeconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.25f);
	const FVector2D Pose = RokMainInterfaceModuleAnimation::EvaluateRight(bMiniMapTargetVisible, MiniMapAnimationSeconds);
	MiniMapRoot->SetRenderTranslation(FVector2D(Pose.X, 0));
	MiniMapRoot->SetRenderOpacity(Pose.Y);
	// Off disables raycasts immediately, but finishes the visible exit motion.
	MiniMapRoot->SetVisibility(bMiniMapTargetVisible ? ESlateVisibility::SelfHitTestInvisible
		: (MiniMapAnimationSeconds < 0.25f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed));
}

void URokWorldMapHudWidget::BuildPlayerIdentity()
{
	UCanvasPanel* Group =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapPlayerIdentity"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D::ZeroVector,
		FVector2D(302.0f, 125.0f));

	UImage* PlayerBackground = MakeAtlasImage(
		TEXT("WorldMapPlayerBackground"),
		MainMenuAtlasPath,
		PlayerBackgroundRect,
		MainMenuAtlasSize,
		FVector2D(302.0f, 125.0f));
	PlayerBackground->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlayerBackground->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		Group,
		PlayerBackground,
		FVector2D::ZeroVector,
		FVector2D(302.0f, 125.0f));

	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapPlayerHead"),
			PlayerHeadAtlasPath,
			PlayerHead1000Rect,
			PlayerHeadAtlasSize,
			FVector2D(92.0f, 92.0f)),
		FVector2D(10.0f, 10.0f),
		FVector2D(92.0f, 92.0f));
	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapPlayerHeadFrame"),
			PlayerHeadAtlasPath,
			PlayerFrameRect,
			PlayerHeadAtlasSize,
			FVector2D(104.0f, 104.0f)),
		FVector2D(4.0f, 4.0f),
		FVector2D(104.0f, 104.0f));
	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapPowerIcon"),
			MainMenuAtlasPath,
			PowerIconRect,
			MainMenuAtlasSize,
			FVector2D(28.0f, 25.0f)),
		FVector2D(111.0f, 10.0f),
		FVector2D(28.0f, 25.0f));

	PowerText = MakeHudText(
		TEXT("WorldMapPowerText"),
		25,
		FLinearColor(0.9f, 0.96f, 1.0f, 1.0f),
		true);
	PowerText->SetText(
		NSLOCTEXT("RokWorldMapHud", "InitialPowerUnavailable", "--"));
	PlaceOnCanvas(
		Group,
		PowerText,
		FVector2D(145.0f, 5.0f),
		FVector2D(135.0f, 36.0f));
	PowerGain = WidgetTree->ConstructWidget<URokPowerGainWidget>(URokPowerGainWidget::StaticClass(), TEXT("WorldMapPowerGain"));
	PlaceOnCanvas(Group, PowerGain, FVector2D(199.65989f - 320, 31.47f - 180), FVector2D(640, 360))->SetZOrder(51);

	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapVipIcon"),
			VipAtlasPath,
			VipIcon100Rect,
			VipAtlasSize,
			FVector2D(34.0f, 34.0f)),
		FVector2D(112.0f, 49.0f),
		FVector2D(34.0f, 34.0f));
	VipText = MakeHudText(
		TEXT("WorldMapVipText"),
		20,
		FLinearColor(1.0f, 0.88f, 0.4f, 1.0f),
		true);
	VipText->SetText(
		NSLOCTEXT("RokWorldMapHud", "VipUnavailable", "VIP --"));
	PlaceOnCanvas(
		Group,
		VipText,
		FVector2D(151.0f, 51.0f),
		FVector2D(85.0f, 30.0f));
	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapPlayerArrow"),
			MainMenuAtlasPath,
			PlayerArrowRect,
			MainMenuAtlasSize,
			FVector2D(65.0f, 42.0f)),
		FVector2D(237.0f, 50.0f),
		FVector2D(65.0f, 42.0f));

	UButton* PlayerProfileHitButton =
		MakeTransparentHitButton(TEXT("WorldMapPlayerProfileHitButton"));
	PlayerProfileHitButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenPlayerProfileClicked);
	if (UCanvasPanelSlot* ProfileHitSlot = PlaceOnCanvas(
			Group,
			PlayerProfileHitButton,
			FVector2D::ZeroVector,
			FVector2D(108.0f, 108.0f)))
	{
		ProfileHitSlot->SetZOrder(10);
	}

	UButton* VipHitButton =
		MakeTransparentHitButton(TEXT("WorldMapVipHitButton"));
	VipHitButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenVipClicked);
	if (UCanvasPanelSlot* VipHitSlot = PlaceOnCanvas(
			Group,
			VipHitButton,
			FVector2D(108.0f, 44.0f),
			FVector2D(129.0f, 44.0f)))
	{
		VipHitSlot->SetZOrder(10);
	}

	UButton* PlayerArrowHitButton =
		MakeTransparentHitButton(TEXT("WorldMapPlayerArrowHitButton"));
	PlayerArrowHitButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenCityManagerClicked);
	if (UCanvasPanelSlot* ArrowHitSlot = PlaceOnCanvas(
			Group,
			PlayerArrowHitButton,
			FVector2D(237.0f, 50.0f),
			FVector2D(65.0f, 42.0f)))
	{
		ArrowHitSlot->SetZOrder(10);
	}
}

void URokWorldMapHudWidget::BuildCoordinatePanel()
{
	UCanvasPanel* Group =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapCoordinatePanel"));
	Group->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D(298.0f, 6.0f),
		FVector2D(280.0f, 50.0f));

	// Same UI_Item_Position/img source as the city HUD: one sliced image.
	// Separate patches prevent Slate Box margins from using the whole atlas.
	UCanvasPanel* Background = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapCoordinateBackground"));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Group, Background, FVector2D(0, 5), FVector2D(235, 40));
	const int32 SourceX[] = {499, 508, 515, 523};
	const int32 SourceY[] = {570, 557, 544, 532};
	const float DestX[] = {0, 9, 227, 235};
	const float DestY[] = {0, 13, 28, 40};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Column = 0; Column < 3; ++Column)
		{
			const FVector2D Size(DestX[Column + 1] - DestX[Column], DestY[Row + 1] - DestY[Row]);
			PlaceOnCanvas(Background, MakeAtlasImage(
				FName(*FString::Printf(TEXT("WorldMapCoordinateBackground_%d_%d"), Row, Column)),
				MainMenuAtlasPath, FIntRect(SourceX[Column], SourceY[Row + 1], SourceX[Column + 1], SourceY[Row]),
				MainMenuAtlasSize, Size), FVector2D(DestX[Column], DestY[Row]), Size);
		}
	}

	CoordinateText = MakeHudText(
		TEXT("WorldMapCoordinateText"),
		18,
		FLinearColor(0.95f, 0.95f, 0.92f, 1.0f));
	CoordinateText->SetJustification(ETextJustify::Center);
	CoordinateText->SetText(
		NSLOCTEXT("RokWorldMapHud", "DefaultCoordinate", "#1  X:1146 Y:157"));
	PlaceOnCanvas(
		Group,
		CoordinateText,
		FVector2D(8.0f, 8.0f),
		FVector2D(192.0f, 32.0f));
	UButton* PositionSearchButton = MakeAtlasButton(
		TEXT("WorldMapCoordinateSearchButton"),
		MainMenuAtlasPath,
		PositionSearchRect,
		MainMenuAtlasSize,
		FVector2D(22.0f, 22.0f));
	PositionSearchButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenPositionSearchClicked);
	PlaceOnCanvas(
		Group,
		PositionSearchButton,
		FVector2D(198.0f, 13.0f),
		FVector2D(22.0f, 22.0f));

	UButton* MapMarkersButton = MakeAtlasButton(
		TEXT("WorldMapFavoriteButton"),
		MainMenuAtlasPath,
		FavoriteRect,
		MainMenuAtlasSize,
		FVector2D(44.0f, 44.0f));
	MapMarkersButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenMapMarkersClicked);
	PlaceOnCanvas(
		Group,
		MapMarkersButton,
		FVector2D(232.0f, 2.0f),
		FVector2D(44.0f, 44.0f));
}

void URokWorldMapHudWidget::BuildMiniMap()
{
	UCanvasPanel* Group =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapMiniMap"));
	MiniMapRoot = Group;
	AdvanceMiniMapAnimation(0.0f);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D(-15.0f, 12.0f),
		FVector2D(230.0f, 160.0f),
		FAnchors(1.0f, 0.0f),
		FVector2D(1.0f, 0.0f));

	PlaceOnCanvas(
		Group,
		MakeAtlasImage(
			TEXT("WorldMapMiniMapBackground"),
			MapAtlasPath,
			MiniMapBackgroundRect,
			MapAtlasSize,
			FVector2D(230.0f, 160.0f)),
		FVector2D::ZeroVector,
		FVector2D(230.0f, 160.0f));

	MiniMapViewportRoot =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapMiniMapViewport"));
	MiniMapViewportRoot->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
	MiniMapViewportCanvasSlot = PlaceOnCanvas(
		Group,
		MiniMapViewportRoot,
		MiniMapContentOffset,
		FVector2D(18.0f, 12.0f));
	UBorder* ViewFrame =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapMiniMapViewportOuter"));
	ViewFrame->SetBrushColor(FLinearColor::White);
	ViewFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		MiniMapViewportRoot,
		ViewFrame,
		FVector2D::ZeroVector,
		FVector2D(18.0f, 12.0f));
	UBorder* ViewInner =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapMiniMapViewportInner"));
	ViewInner->SetBrushColor(
		FLinearColor(0.02f, 0.11f, 0.14f, 0.92f));
	ViewInner->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		MiniMapViewportRoot,
		ViewInner,
		FVector2D(2.0f, 2.0f),
		FVector2D(14.0f, 8.0f));

	MiniMapSelfDot =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapMiniMapSelfDot"));
	MiniMapSelfDot->SetBrushColor(
		FLinearColor(0.2f, 1.0f, 0.55f, 1.0f));
	MiniMapSelfDot->SetVisibility(ESlateVisibility::HitTestInvisible);
	MiniMapSelfCanvasSlot = PlaceOnCanvas(
		Group,
		MiniMapSelfDot,
		MiniMapContentOffset,
		FVector2D(8.0f, 8.0f));

	URokWorldMapMiniMapInputWidget* MiniMapInput =
		WidgetTree->ConstructWidget<URokWorldMapMiniMapInputWidget>(
			URokWorldMapMiniMapInputWidget::StaticClass(),
			TEXT("WorldMapMiniMapInput"));
	MiniMapInput->SetOwningHud(this);
	MiniMapInput->SetVisibility(ESlateVisibility::Visible);
	if (UCanvasPanelSlot* MiniMapInputSlot = PlaceOnCanvas(
			Group,
			MiniMapInput,
			MiniMapContentOffset,
			MiniMapContentSize))
	{
		MiniMapInputSlot->SetZOrder(20);
	}
}

void URokWorldMapHudWidget::BuildRelationFilters()
{
	// UI_Model_Ck_MainIFLod / GameToggle.AutoPlaySound, decoded with Unity's mono import settings.
	FilterClickSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonSidePage.Sound_Ui_CommonSidePage"));
	if (!FilterClickSound)
		UE_LOG(LogTemp, Warning, TEXT("Missing Unity filter click sound; run ImportRokUnityFilterSound.py"));
	UCanvasPanel* Group =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapRelationFilters"));
	StrategicFilterRoot = Group;
	AdvanceStrategicModuleAnimation(0.0f);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		Group,
		FVector2D(10.0f, 160.0f),
		FVector2D(184.0f, 292.0f));

	UBorder* MainShade =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapRelationFilterShade"));
	MainShade->SetBrushColor(
		FLinearColor(0.025f, 0.075f, 0.105f, 0.78f));
	MainShade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		Group,
		MainShade,
		FVector2D::ZeroVector,
		FVector2D(184.0f, 174.0f));

	const TArray<FText> FilterLabelTexts = {
		NSLOCTEXT("RokWorldMapHud", "AllianceFilter", "联盟"),
		NSLOCTEXT("RokWorldMapHud", "ExploreFilter", "探索"),
		NSLOCTEXT("RokWorldMapHud", "ResourceFilter", "资源"),
		NSLOCTEXT("RokWorldMapHud", "BookmarkFilter", "书签")};
	this->FilterDots.Reset();
	FilterBackgrounds.Reset();
	FilterButtons.Reset();
	FilterTints.Reset();
	FilterStartAlphas.Reset();
	this->FilterLabels.Reset();
	for (int32 Index = 0; Index < FilterLabelTexts.Num(); ++Index)
	{
		// UI_Model_Ck_MainIFLod: ck_com_1000_1 background and
		// ck_com_1000_2 checkmark (ui_common), separate visual layers.
		UImage* Background = MakeAtlasImage(
			*FString::Printf(TEXT("WorldMapFilterBackground_%d"), Index),
			CommonAtlasPath, SpriteRect(611, 181, 44, 44), CommonAtlasSize, FVector2D(36, 36));
		PlaceOnCanvas(Group, Background, FVector2D(7, 4 + Index * 42), FVector2D(36, 36));
		FilterBackgrounds.Add(Background);
		FilterTints.AddDefaulted();
		UImage* Dot = MakeAtlasImage(
			*FString::Printf(TEXT("WorldMapFilterDot_%d"), Index),
			CommonAtlasPath, SpriteRect(697, 231, 44, 44), CommonAtlasSize, FVector2D(32, 32));
		const float InitialAlpha = SelectedFilter == static_cast<ERokWorldMapHudFilter>(Index + 1) ? 1.0f : 0.0f;
		Dot->SetRenderOpacity(InitialAlpha);
		FilterStartAlphas.Add(InitialAlpha);
		PlaceOnCanvas(
			Group,
			Dot,
			FVector2D(9.0f, 6.0f + Index * 42.0f),
			FVector2D(32.0f, 32.0f));
		this->FilterDots.Add(Dot);

		UTextBlock* Label = MakeHudText(
			*FString::Printf(TEXT("WorldMapFilterLabel_%d"), Index),
			18,
			FLinearColor(0.95686275f, 0.95686275f, 0.95686275f, 1.0f),
			false);
		Label->SetText(FilterLabelTexts[Index]);
		PlaceOnCanvas(
			Group,
			Label,
			FVector2D(44.0f, 8.0f + Index * 42.0f),
			FVector2D(120.0f, 30.0f));
		this->FilterLabels.Add(Label);

		UButton* FilterHitButton = MakeTransparentHitButton(
			*FString::Printf(TEXT("WorldMapFilterHit_%d"), Index));
		FilterButtons.Add(FilterHitButton);
		switch (Index)
		{
		case 0:
			FilterHitButton->OnClicked.AddDynamic(
				this,
				&URokWorldMapHudWidget::HandleAllianceFilterClicked);
			break;
		case 1:
			FilterHitButton->OnClicked.AddDynamic(
				this,
				&URokWorldMapHudWidget::HandleExploreFilterClicked);
			break;
		case 2:
			FilterHitButton->OnClicked.AddDynamic(
				this,
				&URokWorldMapHudWidget::HandleResourceFilterClicked);
			break;
		default:
			FilterHitButton->OnClicked.AddDynamic(
				this,
				&URokWorldMapHudWidget::HandleBookmarkFilterClicked);
			break;
		}
		if (UCanvasPanelSlot* FilterHitSlot = PlaceOnCanvas(
				Group,
				FilterHitButton,
				FVector2D(4.0f, 2.0f + Index * 42.0f),
				FVector2D(174.0f, 40.0f)))
		{
			FilterHitSlot->SetZOrder(10);
		}
	}

	// UI_Item_LodMenu owns separate img_typeGuild/img_typeRes panels.
	// The toggle root stays fixed while only the selected legend is visible.
	UCanvasPanel* AllianceLegend = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapAllianceLegend"));
	PlaceOnCanvas(Group, AllianceLegend, FVector2D(0, 181), FVector2D(184, 111));
	UBorder* AllianceShade = WidgetTree->ConstructWidget<UBorder>();
	AllianceShade->SetBrushColor(MainShade->GetBrushColor());
	PlaceOnCanvas(AllianceLegend, AllianceShade, FVector2D::ZeroVector, FVector2D(184, 111));
	UBorder* Separator =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapFilterSeparator"));
	Separator->SetBrushColor(
		FLinearColor(0.3f, 0.46f, 0.52f, 0.45f));
	Separator->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		AllianceLegend,
		Separator,
		FVector2D(8.0f, 0.0f),
		FVector2D(168.0f, 1.0f));

	const TArray<FText> RelationLabels = {
		NSLOCTEXT("RokWorldMapHud", "AllianceLeader", "盟主"),
		NSLOCTEXT("RokWorldMapHud", "AllianceFriend", "盟友"),
		NSLOCTEXT("RokWorldMapHud", "AllianceFortress", "联盟主要塞")};
	const TArray<FIntRect> RelationRects = {
		RelationLeaderRect,
		RelationFriendRect,
		RelationFortressRect};
	for (int32 Index = 0; Index < RelationLabels.Num(); ++Index)
	{
		UImage* RelationIcon = MakeAtlasImage(
			*FString::Printf(TEXT("WorldMapRelationIcon_%d"), Index),
			MapAtlasPath,
			RelationRects[Index],
			MapAtlasSize,
			FVector2D(30.0f, 30.0f));
		PlaceOnCanvas(
			AllianceLegend,
			RelationIcon,
			FVector2D(12.0f, Index * 35.0f),
			FVector2D(30.0f, 30.0f));

		UTextBlock* Label = MakeHudText(
			*FString::Printf(TEXT("WorldMapRelationLabel_%d"), Index),
			18,
			FLinearColor(0.88f, 0.92f, 0.93f, 1.0f));
		Label->SetText(RelationLabels[Index]);
		PlaceOnCanvas(
			AllianceLegend,
			Label,
			FVector2D(48.0f, 2.0f + Index * 35.0f),
			FVector2D(125.0f, 28.0f));
	}

	UCanvasPanel* ResourceLegend = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapResourceLegend"));
	PlaceOnCanvas(Group, ResourceLegend, FVector2D(0, 181), FVector2D(184, 216));
	UBorder* ResourceShade = WidgetTree->ConstructWidget<UBorder>();
	ResourceShade->SetBrushColor(MainShade->GetBrushColor());
	PlaceOnCanvas(ResourceLegend, ResourceShade, FVector2D::ZeroVector, FVector2D(184, 216));
	// Exact UI_Model_1..6 sprite overrides, not atlas numeric-name order.
	const FIntRect ResourceRects[] = {SpriteRect(112,252,27,27), SpriteRect(151,447,27,27),
		SpriteRect(38,39,27,27), SpriteRect(112,339,27,27), SpriteRect(112,310,27,27), SpriteRect(112,281,27,27)};
	const FText ResourceLabels[] = {
		NSLOCTEXT("RokWorldMapHud", "ResourceVeryPoor", "非常贫瘠"),
		NSLOCTEXT("RokWorldMapHud", "ResourcePoor", "贫瘠"),
		NSLOCTEXT("RokWorldMapHud", "ResourceNormal", "普通"),
		NSLOCTEXT("RokWorldMapHud", "ResourceGood", "良好"),
		NSLOCTEXT("RokWorldMapHud", "ResourceRich", "富饶"),
		NSLOCTEXT("RokWorldMapHud", "ResourceVeryRich", "非常富饶")};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		auto* Icon = MakeAtlasImage(*FString::Printf(TEXT("WorldMapResourceLegendIcon_%d"), Index),
			MainMenuAtlasPath, ResourceRects[Index], MainMenuAtlasSize, FVector2D(27,27));
		PlaceOnCanvas(ResourceLegend, Icon, FVector2D(12, 4 + Index * 35), FVector2D(27,27));
		auto* Label = MakeHudText(*FString::Printf(TEXT("WorldMapResourceLegendLabel_%d"), Index),
			18, FLinearColor(0.88f, 0.92f, 0.93f, 1));
		Label->SetText(ResourceLabels[Index]);
		PlaceOnCanvas(ResourceLegend, Label, FVector2D(48, 2 + Index * 35), FVector2D(130,28));
	}
	// UI_Item_LodMenu.prefab: img_typeExplore and img_typeMark. NoSurvey
	// inherits icon_mmu_1005_1 from UI_Model_LodGizmos; the others override it.
	const auto BuildSourceLegend = [&](const TCHAR* Name, const TArray<FText>& Labels,
		const TArray<FIntRect>& Rects)
	{
		auto* Legend = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
		const FVector2D Size(184, 6 + Labels.Num() * 35);
		PlaceOnCanvas(Group, Legend, FVector2D(0,181), Size);
		auto* Shade = WidgetTree->ConstructWidget<UBorder>();
		Shade->SetBrushColor(MainShade->GetBrushColor());
		PlaceOnCanvas(Legend, Shade, FVector2D::ZeroVector, Size);
		for (int32 Index = 0; Index < Labels.Num(); ++Index)
		{
			auto* Icon = MakeAtlasImage(*FString::Printf(TEXT("%sIcon_%d"), Name, Index),
				MainMenuAtlasPath, Rects[Index], MainMenuAtlasSize, FVector2D(30,30));
			PlaceOnCanvas(Legend, Icon, FVector2D(12, Index * 35), FVector2D(30,30));
			auto* Label = MakeHudText(*FString::Printf(TEXT("%sLabel_%d"), Name, Index),
				18, FLinearColor(0.88f, 0.92f, 0.93f, 1));
			Label->SetText(Labels[Index]);
			PlaceOnCanvas(Legend, Label, FVector2D(48, 2 + Index * 35), FVector2D(130,28));
		}
	};
	BuildSourceLegend(TEXT("WorldMapExploreLegend"), {
		NSLOCTEXT("RokWorldMapHud", "NoSurvey", "待调查"),
		NSLOCTEXT("RokWorldMapHud", "Survey", "已调查"),
		NSLOCTEXT("RokWorldMapHud", "NoVisit", "待拜访"),
		NSLOCTEXT("RokWorldMapHud", "Visit", "已拜访")},
		{SpriteRect(170,513,38,38), SpriteRect(130,513,38,38),
		 SpriteRect(984,753,38,38), SpriteRect(984,793,38,38)});
	BuildSourceLegend(TEXT("WorldMapMarkerLegend"), {
		NSLOCTEXT("RokWorldMapHud", "SpecialMarker", "特殊")}, {SpriteRect(984,714,37,37)});
	UpdateFilterLegend();

	// Unity renders the march queue inside the army command HUD
	// (UI_Pop_WorldArmyCmdView), not as a floating world-map label; the
	// queue UI arrives with the march-command slice.
}

void URokWorldMapHudWidget::UpdateFilterLegend()
{
	if (auto* Legend = GetWidgetFromName(TEXT("WorldMapExploreLegend")))
		Legend->SetVisibility(SelectedFilter == ERokWorldMapHudFilter::Explore
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* Legend = GetWidgetFromName(TEXT("WorldMapMarkerLegend")))
		Legend->SetVisibility(SelectedFilter == ERokWorldMapHudFilter::Markers
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* Legend = GetWidgetFromName(TEXT("WorldMapAllianceLegend")))
		Legend->SetVisibility(SelectedFilter == ERokWorldMapHudFilter::Alliance
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* Legend = GetWidgetFromName(TEXT("WorldMapResourceLegend")))
		Legend->SetVisibility(SelectedFilter == ERokWorldMapHudFilter::Resources
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void URokWorldMapHudWidget::BuildBottomControls()
{
	// pl_mmu_1000 is vertically sliced only. Preserve its 6px top and
	// 3px bottom bands rather than scaling atlas-sized Slate Box borders.
	UCanvasPanel* ChatBackground = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapChatBackground"));
	ChatBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
	const int32 SourceY[] = {ChatBarRect.Max.Y, ChatBarRect.Max.Y - 6, ChatBarRect.Min.Y + 3, ChatBarRect.Min.Y};
	const float DestY[] = {0, 6, 88, 91};
	for (int32 Band = 0; Band < 3; ++Band)
	{
		const FVector2D Size(768, DestY[Band + 1] - DestY[Band]);
		PlaceOnCanvas(ChatBackground, MakeAtlasImage(
			FName(*FString::Printf(TEXT("WorldMapChatBackground_%d"), Band)), MainMenuAtlasPath,
			FIntRect(ChatBarRect.Min.X, SourceY[Band + 1], ChatBarRect.Max.X, SourceY[Band]),
			MainMenuAtlasSize, Size), FVector2D(0, DestY[Band]), Size);
	}
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		ChatBackground,
		FVector2D::ZeroVector,
		FVector2D(768.0f, 91.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	// UI_IF_MainInterface/UI_Item_ChatSearchBuild/img_chat1:
	// the world channel remains selected in the current world-map chat shell.
	// This bar is part of the persistent HUD, not the tactical-only overlay.
	UImage* ChatWorldMarker = MakeAtlasImage(TEXT("WorldMapChatWorldMarker"),
		MainMenuAtlasPath, SpriteRect(275, 817, 30, 4), MainMenuAtlasSize, FVector2D(30, 4));
	ChatWorldMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(ExpandedDesignCanvas, ChatWorldMarker,
		FVector2D(202.6f, -84.3f), FVector2D(30, 4), FAnchors(0, 1), FVector2D(0, 0.5));

	UButton* ChatButton = MakeAtlasButton(
		TEXT("WorldMapChatButton"),
		MainMenuAtlasPath,
		ChatRect,
		MainMenuAtlasSize,
		FVector2D(50.0f, 47.0f));
	ChatButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenChatClicked);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		ChatButton,
		FVector2D(134.0f, -18.0f),
		FVector2D(50.0f, 47.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	UButton* SearchButton = MakeAtlasButton(
		TEXT("WorldMapSearchButton"),
		MainMenuAtlasPath,
		SearchRect,
		MainMenuAtlasSize,
		FVector2D(92.0f, 92.0f));
	SearchButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenResourceSearchClicked);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		SearchButton,
		FVector2D(18.0f, -158.0f),
		FVector2D(92.0f, 92.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	UButton* HomeCityButton = MakeAtlasButton(
		TEXT("WorldMapHomeCityButton"),
		MainMenuAtlasPath,
		CityRect,
		MainMenuAtlasSize,
		FVector2D(127.0f, 148.0f));
	HomeCityButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleEnterPlayerCityClicked);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		HomeCityButton,
		FVector2D::ZeroVector,
		FVector2D(127.0f, 148.0f),
		FAnchors(0.0f, 1.0f),
		FVector2D(0.0f, 1.0f));

	UButton* GlobeButton = MakeAtlasButton(
		TEXT("WorldMapGlobeButton"),
		MainMenuAtlasPath,
		GlobeRect,
		MainMenuAtlasSize,
		FVector2D(100.0f, 100.0f));
	StrategicGlobeButton = GlobeButton;
	AdvanceStrategicModuleAnimation(0.0f);
	GlobeButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenServerListClicked);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		GlobeButton,
		FVector2D(-12.0f, -12.0f),
		FVector2D(100.0f, 100.0f),
		FAnchors(1.0f, 1.0f),
		FVector2D(1.0f, 1.0f));

	UButton* MailButton = MakeAtlasButton(
		TEXT("WorldMapMailButton"),
		MainMenuAtlasPath,
		MailRect,
		MainMenuAtlasSize,
		FVector2D(100.0f, 100.0f));
	// OpenLodUi Mail is tactical-only. The shared tactical HUD owns its
	// animated presentation; keep this legacy route hidden, not duplicated in overview.
	MailButton->SetVisibility(ESlateVisibility::Collapsed);
	MailButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleOpenMailClicked);
	PlaceOnCanvas(
		ExpandedDesignCanvas,
		MailButton,
		FVector2D(-16.0f, -116.0f),
		FVector2D(100.0f, 100.0f),
		FAnchors(1.0f, 1.0f),
		FVector2D(1.0f, 1.0f));
}

void URokWorldMapHudWidget::BuildServerListOverlay()
{
	ServerListOverlay =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapServerListOverlay"));
	ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* OverlaySlot =
		ExpandedDesignCanvas->AddChildToCanvas(ServerListOverlay);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetOffsets(FMargin(0.0f));
	OverlaySlot->SetZOrder(95);

	UButton* InputBlocker =
		MakeTransparentHitButton(TEXT("WorldMapServerListInputBlocker"));
	if (UCanvasPanelSlot* InputBlockerSlot =
			ServerListOverlay->AddChildToCanvas(InputBlocker))
	{
		InputBlockerSlot->SetAnchors(
			FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		InputBlockerSlot->SetOffsets(FMargin(0.0f));
		InputBlockerSlot->SetZOrder(0);
	}

	UBorder* DimBackground =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapServerListDim"));
	DimBackground->SetBrushColor(
		FLinearColor(0.0f, 0.015f, 0.025f, 0.72f));
	DimBackground->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* DimSlot =
			ServerListOverlay->AddChildToCanvas(DimBackground))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
		DimSlot->SetZOrder(1);
	}

	UBorder* Panel =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("WorldMapServerListPanel"));
	Panel->SetBrushColor(
		FLinearColor(0.035f, 0.095f, 0.13f, 0.98f));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* PanelSlot = PlaceOnCanvas(
			ServerListOverlay,
			Panel,
			FVector2D::ZeroVector,
			FVector2D(520.0f, 350.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		PanelSlot->SetZOrder(2);
	}

	UTextBlock* Title = MakeHudText(
		TEXT("WorldMapServerListTitle"),
		30,
		FLinearColor(0.95f, 0.83f, 0.47f, 1.0f),
		true);
	Title->SetText(
		NSLOCTEXT("RokWorldMapHud", "ServerListTitle", "王国列表"));
	Title->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* TitleSlot = PlaceOnCanvas(
			ServerListOverlay,
			Title,
			FVector2D(0.0f, -132.0f),
			FVector2D(440.0f, 48.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		TitleSlot->SetZOrder(3);
	}

	UTextBlock* Description = MakeHudText(
		TEXT("WorldMapServerListDescription"),
		20,
		FLinearColor(0.87f, 0.92f, 0.94f, 1.0f));
	Description->SetText(NSLOCTEXT(
		"RokWorldMapHud",
		"ServerListBoundary",
		"王国与角色列表数据尚未迁移\n当前版本暂不支持切换王国"));
	Description->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* DescriptionSlot = PlaceOnCanvas(
			ServerListOverlay,
			Description,
			FVector2D(0.0f, -48.0f),
			FVector2D(440.0f, 74.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		DescriptionSlot->SetZOrder(3);
	}

	UButton* SelectButton =
		WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("WorldMapServerSelectUnavailableButton"));
	SelectButton->SetBackgroundColor(
		FLinearColor(0.16f, 0.34f, 0.43f, 1.0f));
	SelectButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleServerSelectionUnavailableClicked);
	UTextBlock* SelectText = MakeHudText(
		TEXT("WorldMapServerSelectUnavailableText"),
		20,
		FLinearColor::White,
		true);
	SelectText->SetText(NSLOCTEXT(
		"RokWorldMapHud",
		"ServerListUnavailableAction",
		"选择王国"));
	SelectText->SetJustification(ETextJustify::Center);
	SelectButton->SetContent(SelectText);
	if (UCanvasPanelSlot* SelectSlot = PlaceOnCanvas(
			ServerListOverlay,
			SelectButton,
			FVector2D(0.0f, 54.0f),
			FVector2D(230.0f, 50.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		SelectSlot->SetZOrder(3);
	}

	ServerListStatusText = MakeHudText(
		TEXT("WorldMapServerListStatus"),
		17,
		FLinearColor(1.0f, 0.75f, 0.38f, 1.0f));
	ServerListStatusText->SetText(FText::GetEmpty());
	ServerListStatusText->SetJustification(ETextJustify::Center);
	if (UCanvasPanelSlot* StatusSlot = PlaceOnCanvas(
			ServerListOverlay,
			ServerListStatusText,
			FVector2D(0.0f, 116.0f),
			FVector2D(440.0f, 32.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		StatusSlot->SetZOrder(3);
	}

	UButton* CloseButton =
		WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("WorldMapServerListCloseButton"));
	CloseButton->SetBackgroundColor(
		FLinearColor(0.36f, 0.16f, 0.11f, 1.0f));
	CloseButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleCloseServerListClicked);
	UTextBlock* CloseText = MakeHudText(
		TEXT("WorldMapServerListCloseText"),
		22,
		FLinearColor::White,
		true);
	CloseText->SetText(
		NSLOCTEXT("RokWorldMapHud", "CloseServerList", "关闭"));
	CloseText->SetJustification(ETextJustify::Center);
	CloseButton->SetContent(CloseText);
	if (UCanvasPanelSlot* CloseSlot = PlaceOnCanvas(
			ServerListOverlay,
			CloseButton,
			FVector2D(218.0f, -146.0f),
			FVector2D(72.0f, 42.0f),
			FAnchors(0.5f, 0.5f),
			FVector2D(0.5f, 0.5f)))
	{
		CloseSlot->SetZOrder(4);
	}
}

void URokWorldMapHudWidget::BuildSelfMarker()
{
	// UI_Pop_WorldObjectLod3Player: 200x200 root; PlayerHead 126x126
	// at y=81.6, scale .75. Frame is 211x211 * .7 * .75.
	SelfPortraitRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapSelfPortrait"));
	SelfPortraitRoot->SetVisibility(ESlateVisibility::Collapsed);
	SelfPortraitRoot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	PlaceOnCanvas(ExpandedDesignCanvas, SelfPortraitRoot,
		FVector2D::ZeroVector, FVector2D(200, 200))->SetZOrder(81);
	PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitPointer"),
		MapAtlasPath, SpriteRect(559, 184, 56, 71), MapAtlasSize, FVector2D(42, 53.25f)),
		FVector2D(79, 46.75f), FVector2D(42, 53.25f));
	PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitImage"),
		PlayerHeadAtlasPath, PlayerHead1000Rect, PlayerHeadAtlasSize, FVector2D(94.5f, 94.5f)),
		FVector2D(52.75f, -28.85f), FVector2D(94.5f, 94.5f));
	PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitFrame"),
		PlayerHeadAtlasPath, PlayerFrameRect, PlayerHeadAtlasSize, FVector2D(110.775f, 110.775f)),
		FVector2D(44.6125f, -36.9875f), FVector2D(110.775f, 110.775f));
	// UI_Tip_Screen: exact ui_map atlas sprites, not the globe/menu icon.
	HomeIndicatorRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapHomeIndicator"));
	HomeIndicatorRoot->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ExpandedDesignCanvas, HomeIndicatorRoot,
		FVector2D::ZeroVector, FVector2D(100, 120))->SetZOrder(80);
	HomeIndicatorArrow = MakeAtlasImage(TEXT("WorldMapHomeDirection"), MapAtlasPath,
		SpriteRect(254, 203, 73, 88), MapAtlasSize, FVector2D(73, 88));
	PlaceOnCanvas(HomeIndicatorRoot, HomeIndicatorArrow, FVector2D(13.5f, 12), FVector2D(73, 88));
	HomeIndicatorArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.42f));
	UButton* HomeIndicatorButton = MakeAtlasButton(TEXT("WorldMapHomeIndicatorButton"),
		MapAtlasPath, SpriteRect(495, 185, 62, 70), MapAtlasSize, FVector2D(62, 70));
	HomeIndicatorButton->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::FocusPlayerLocation);
	PlaceOnCanvas(HomeIndicatorRoot, HomeIndicatorButton, FVector2D(19, 5), FVector2D(62, 70));
	HomeIndicatorDistance = MakeHudText(TEXT("WorldMapHomeDistance"), 18, FLinearColor::White, true);
	HomeIndicatorDistance->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(HomeIndicatorRoot, HomeIndicatorDistance, FVector2D(0, 89), FVector2D(100, 26));
	SelfMarkerRoot =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldMapSelfMarker"));
	SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
	SelfMarkerRoot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SelfMarkerCanvasSlot = PlaceOnCanvas(
		ExpandedDesignCanvas,
		SelfMarkerRoot,
		FVector2D(665.0f, 330.0f),
		SelfMarkerSize);
	if (SelfMarkerCanvasSlot.IsValid())
	{
		SelfMarkerCanvasSlot->SetZOrder(80);
	}

	// Unity UI_Tip_WorldObjectPlayerLevel: a name plate below the own city
	// plus a pl_map_levelBg level badge. Own-city name renders green
	// (GlobalViewLevelMediator.GetCityColor); the real city name arrives
	// with login/server state and stays a localized placeholder for now.
	SelfCityNameText = MakeHudText(
		TEXT("WorldMapSelfCityName"),
		20,
		FLinearColor(0.266666f, 0.831372f, 0.509803f, 1.0f),
		true);
	SelfCityNameText->SetText(NSLOCTEXT(
		"RokWorldMapHud",
		"SelfCityName",
		"主城"));
	SelfCityNameText->SetJustification(ETextJustify::Center);
	// bg0 has a TOP pivot, not a centered pivot. Its content fitter adds
	// 20 units either side of the name; the horizontal sprite borders are 30.
	UBorder* NameBackground = MakeAtlasBorder(TEXT("WorldMapSelfCityNameBackground"),
		MapAtlasPath, SpriteRect(431, 144, 69, 36), MapAtlasSize,
		FVector2D(69, 36), FMargin(30.0f/69.0f, 0, 30.0f/69.0f, 0));
	NameBackground->SetPadding(FMargin(20, 0));
	NameBackground->SetHorizontalAlignment(HAlign_Center);
	NameBackground->SetVerticalAlignment(VAlign_Center);
	NameBackground->SetContent(SelfCityNameText);
	USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("WorldMapSelfCityNameSize"));
	NameSize->SetHeightOverride(41.28f);
	NameSize->SetContent(NameBackground);
	PlaceOnCanvas(SelfMarkerRoot, NameSize, FVector2D(50, 28.1f),
		FVector2D::ZeroVector, FAnchors(0, 0), FVector2D(0.5f, 0))->SetAutoSize(true);

	const int32 CityHallLevel = ReadInitialCityHallLevel();
	if (UTexture2D* BadgeTexture = LevelBadgeTexture)
	{
		UImage* BadgeImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			TEXT("WorldMapSelfCityLevelBadge"));
		FSlateBrush BadgeBrush;
		BadgeBrush.SetResourceObject(BadgeTexture);
		BadgeBrush.ImageSize = FVector2D(60.0f, 33.0f);
		BadgeImage->SetBrush(BadgeBrush);
		BadgeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(
			SelfMarkerRoot,
			BadgeImage,
			FVector2D(21.51f, 61.7f),
			FVector2D(60.0f, 33.0f));

		SelfCityLevelText = MakeHudText(
			TEXT("WorldMapSelfCityLevel"),
			18,
			FLinearColor(0.184f, 0.176f, 0.153f, 1.0f),
			true);
		SelfCityLevelText->SetText(FText::AsNumber(CityHallLevel));
		SelfCityLevelText->SetJustification(ETextJustify::Center);
		PlaceOnCanvas(
			SelfMarkerRoot,
			SelfCityLevelText,
			FVector2D(21.51f, 66.2f),
			FVector2D(60.0f, 24.0f));
	}
}

void URokWorldMapHudWidget::BuildEntityBadges()
{
	EntityBadges.Reset();
	const UWorld* World = GetWorld();
	if (!World || !LevelBadgeTexture)
	{
		return;
	}
	TArray<FString> BadgeLevelSummary;
	for (TActorIterator<ARokWorldMapEntityActor> EntityIterator(World);
		EntityIterator;
		++EntityIterator)
	{
		ARokWorldMapEntityActor* EntityActor = *EntityIterator;
		if (!EntityActor || EntityActor->GetEntityLevel() <= 0)
		{
			continue;
		}
		BadgeLevelSummary.Add(FString::Printf(
			TEXT("%d:%d"),
			EntityActor->GetSourceId(),
			EntityActor->GetEntityLevel()));

		UCanvasPanel* BadgeRoot =
			WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				FName(*FString::Printf(TEXT("WorldMapEntityBadge_%d"),
					EntityActor->GetSourceId())));
		BadgeRoot->SetVisibility(ESlateVisibility::Collapsed);
		BadgeRoot->SetRenderTransformPivot(FVector2D::ZeroVector);

		UImage* BadgeImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("WorldMapEntityBadgeBg_%d"),
				EntityActor->GetSourceId())));
		FSlateBrush BadgeBrush;
		BadgeBrush.SetResourceObject(LevelBadgeTexture);
		BadgeBrush.ImageSize = FVector2D(60.0f, 33.0f);
		BadgeImage->SetBrush(BadgeBrush);
		BadgeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(
			BadgeRoot,
			BadgeImage,
			FVector2D(2.0f, 2.0f),
			FVector2D(60.0f, 33.0f));

		UTextBlock* LevelText = MakeHudText(
			FName(*FString::Printf(TEXT("WorldMapEntityBadgeLevel_%d"),
				EntityActor->GetSourceId())),
			18,
			FLinearColor(0.184f, 0.176f, 0.153f, 1.0f),
			true);
		LevelText->SetText(FText::AsNumber(
			EntityActor->GetEntityLevel()));
		LevelText->SetJustification(ETextJustify::Center);
		PlaceOnCanvas(
			BadgeRoot,
			LevelText,
			FVector2D(2.0f, 7.0f),
			FVector2D(60.0f, 24.0f));

		FEntityBadgeEntry Entry;
		Entry.Entity = EntityActor;
		Entry.Root = BadgeRoot;
		UCanvasPanelSlot* BadgeSlot = PlaceOnCanvas(
			ExpandedDesignCanvas,
			BadgeRoot,
			FVector2D::ZeroVector,
			FVector2D(64.0f, 37.0f));
		if (BadgeSlot)
		{
			BadgeSlot->SetZOrder(70);
			Entry.CanvasSlot = BadgeSlot;
		}
		EntityBadges.Add(MoveTemp(Entry));
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world entity badges built: count=%d levels=%s"),
		EntityBadges.Num(),
		*FString::Join(BadgeLevelSummary, TEXT(",")));
}

float URokWorldMapHudWidget::EvaluateCityTitleOffset(float Dxf)
{
	// UIRoot.CastleTitleOffsetCurve: unweighted Hermite to 2999;
	// infinite tangents then hold -2.3 until the 3000 key jumps to -1.55.
	if (!FMath::IsFinite(Dxf) || Dxf <= 0.0f) return -3.5f;
	if (Dxf >= 3000.0f) return -1.55f;
	if (Dxf >= 2999.0f) return -2.3f;
	const float T = Dxf / 2999.0f;
	return (2*T*T*T - 3*T*T + 1)*-3.5f
		+ (-2*T*T*T + 3*T*T)*-2.3f
		+ (T*T*T - T*T)*2999.0f*0.0012443735f;
}

bool URokWorldMapHudWidget::ShouldShowCityPortrait(float Dxf)
{
	// GlobalViewLevelMediator CityHud_2 starts at TacticsToStrategy_1.
	// LevelDetailCamera keeps the exact upper bound 3000 in Tactical.
	return FMath::IsFinite(Dxf) && Dxf > 3000.0f;
}

bool URokWorldMapHudWidget::ShouldShowCityTitle(float Dxf)
{
	// GlobalViewLevelMediator.RefreshHUDElementView uses inclusive bounds:
	// CityHud_1 and CityHud_2 overlap in TacticsToStrategy_1. CameraParamLod
	// sets that band's upper limit to 4500; portrait visibility is not its inverse.
	return FMath::IsFinite(Dxf) && Dxf >= 0.0f && Dxf <= 4500.0f;
}

float URokWorldMapHudWidget::EvaluateCityPortraitOffset(float Dxf)
{
	// UIRoot TroopCurve's final segment (the entire portrait-visible band).
	const float T = FMath::Clamp((Dxf - 2997.0f) / 67003.0f, 0.0f, 1.0f);
	return (2*T*T*T - 3*T*T + 1)*1.8f
		+ (T*T*T - 2*T*T + T)*67003.0f*0.0001504026f
		+ (-2*T*T*T + 3*T*T)*11.0f;
}

float URokWorldMapHudWidget::EvaluateBarbarianBadgeOffset(float Dxf)
{
	// UIRoot.prefab NpcTroopCurve: zero endpoint slopes, ClampForever.
	const float Start = Dxf <= 267.6259f ? 0.0f : 267.6259f;
	const float End = Dxf <= 267.6259f ? 267.6259f : 3000.0f;
	const float StartValue = Dxf <= 267.6259f ? 0.0f : -0.318347f;
	const float EndValue = Dxf <= 267.6259f ? -0.318347f : -1.0f;
	const float Alpha = FMath::Clamp((Dxf - Start) / (End - Start), 0.0f, 1.0f);
	return FMath::Lerp(StartValue, EndValue, Alpha * Alpha * (3.0f - 2.0f * Alpha));
}

float URokWorldMapHudWidget::EvaluateResourceBadgeOffset(float Dxf)
{
	// UIRoot RssTitleCurve: (0,-0.7) -> (3000,-0.8), zero tangents, ClampForever.
	const float T = FMath::IsFinite(Dxf) ? FMath::Clamp(Dxf / 3000.0f, 0.0f, 1.0f) : 0.0f;
	return FMath::CubicInterp(-0.7f, 0.0f, -0.8f, 0.0f, T);
}

float URokWorldMapHudWidget::EvaluateWorldObjectBadgeScale(
	float Dxf,
	float ViewportTopFraction,
	float VerticalFov)
{
	// UIRoot AllUIScaleCurve plus MapElementUI's screen-depth correction;
	// Unity viewport Y points up, whereas widget-local Y points down.
	const float Alpha = FMath::Clamp((Dxf - 270.0f) / 2730.0f, 0.0f, 1.0f);
	return FMath::Lerp(1.33f, 0.66f, Alpha)
		+ 0.4f * (VerticalFov / 30.0f) * (ViewportTopFraction - 0.5f);
}

void URokWorldMapHudWidget::UpdateEntityBadges()
{
	if (CurrentRootSize.X <= 1.0f || CurrentRootSize.Y <= 1.0f)
	{
		return;
	}
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		return;
	}
	const ARokWorldMapCameraPawn* CameraPawn =
		Cast<ARokWorldMapCameraPawn>(PlayerController->GetPawn());
	const UCameraComponent* Camera = CameraPawn ? CameraPawn->GetCameraComponent() : nullptr;
	float VerticalFov = 30.0f;
	if (Camera)
	{
		VerticalFov = FMath::RadiansToDegrees(2.0f * FMath::Atan(
			FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView) * 0.5f)
			/ FMath::Max(Camera->AspectRatio, UE_SMALL_NUMBER)));
	}
	FRokWorldMapProjection Projection;
	ResolveProjection(Projection);
	for (FEntityBadgeEntry& Entry : EntityBadges)
	{
		ARokWorldMapEntityActor* EntityActor = Entry.Entity.Get();
		if (!Entry.Root
			|| !EntityActor
			|| EntityActor->IsHidden()
			|| EntityActor->GetCurrentLodBand()
				== ERokWorldMapEntityLodBand::Hidden
			|| (CameraPawn && CameraPawn->GetCurrentDxf() > 4500.0f
				&& (EntityActor->GetEntityKind() == TEXT("barbarian")
					|| IsResourceEntityKind(EntityActor->GetEntityKind()))))
		{
			if (Entry.Root)
			{
				Entry.Root->SetVisibility(
					ESlateVisibility::Collapsed);
			}
			continue;
		}
		FVector2D ScreenPosition = FVector2D::ZeroVector;
		FVector BadgeAnchor = EntityActor->GetActorLocation();
		const bool bBarbarianBadge = CameraPawn && EntityActor->GetEntityKind() == TEXT("barbarian");
		const bool bResourceBadge = CameraPawn && IsResourceEntityKind(EntityActor->GetEntityKind());
		const bool bSourceScaledBadge = bBarbarianBadge || bResourceBadge;
		if (bSourceScaledBadge)
		{
			// MapElementUI applies NpcTroopCurve / RssTitleCurve to Unity y and z.
			const float Offset = (bResourceBadge
				? EvaluateResourceBadgeOffset(CameraPawn->GetCurrentDxf())
				: EvaluateBarbarianBadgeOffset(CameraPawn->GetCurrentDxf()))
				* Projection.GetSafeUeScale();
			BadgeAnchor += FVector(0.0f, Offset, Offset);
		}
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				PlayerController,
				BadgeAnchor,
				ScreenPosition,
				true)
			|| ScreenPosition.X < 0.0f || ScreenPosition.X > CurrentRootSize.X
			|| ScreenPosition.Y < 0.0f || ScreenPosition.Y > CurrentRootSize.Y)
		{
			Entry.Root->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FVector2D DesignPosition(
			ScreenPosition.X * CurrentExpandedDesignSize.X
				/ CurrentRootSize.X,
			ScreenPosition.Y * CurrentExpandedDesignSize.Y
				/ CurrentRootSize.Y);
		const float BadgeScale = bSourceScaledBadge
			? EvaluateWorldObjectBadgeScale(CameraPawn->GetCurrentDxf(),
				ScreenPosition.Y / CurrentRootSize.Y, VerticalFov)
			: 1.0f;
		Entry.Root->SetRenderScale(FVector2D(BadgeScale, BadgeScale));
		if (UCanvasPanelSlot* BadgeSlot = Entry.CanvasSlot.Get())
		{
			// UI_Tip_WorldObjectRssLevel img_bg is anchored 30 UI units
			// below the scaled MapElementUI root, centered on its level text.
			BadgeSlot->SetPosition(bSourceScaledBadge
				? DesignPosition + FVector2D(-32.0f, 30.0f - 18.5f) * BadgeScale
				: DesignPosition - FVector2D(32.0f, 18.0f));
		}
		Entry.Root->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
}

void URokWorldMapHudWidget::BuildWorldObjectInfo()
{
	// GameButton.OnPointerClick / UI_Pop_WorldObjectInfo buttons.
	// This is not the CommonSidePage sound used by the world-map filter toggles.
	ResourceClickSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonClickButton.Sound_Ui_CommonClickButton"));
	if (!ResourceClickSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("Missing Unity resource popup click sound; run ImportRokUnityButtonSound.py"));
	}
	WorldObjectInfoRoot =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldObjectInfoRoot"));
	WorldObjectInfoRoot->SetVisibility(ESlateVisibility::Collapsed);
	WorldObjectInfoRoot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	WorldObjectInfoCanvasSlot = PlaceOnCanvas(
		ExpandedDesignCanvas,
		WorldObjectInfoRoot,
		FVector2D(525.0f, 250.0f),
		WorldObjectInfoSize);
	if (WorldObjectInfoCanvasSlot.IsValid())
	{
		WorldObjectInfoCanvasSlot->SetZOrder(90);
	}

	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldObjectInfoPanel"));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// UI_Pop_WorldObjectInfo/bg -> ui_common/img_com_PopBg (58x55),
	// Unity border L/B/R/T = 25/25/18/21. Slate Box uses the entire
	// texture's dimensions even with a UV region, so use atlas image patches
	// as in RokUnityFeatureScreenWidget rather than stretching atlas-sized corners.
	const int32 SourceX[] = {0, 25, 40, 58};
	const int32 SourceY[] = {55, 34, 25, 0}; // top down in Unity's bottom-origin atlas
	const float DestX[] = {0, 25, static_cast<float>(WorldObjectInfoPanelSize.X) - 18, static_cast<float>(WorldObjectInfoPanelSize.X)};
	const float DestY[] = {0, 21, static_cast<float>(WorldObjectInfoPanelSize.Y) - 25, static_cast<float>(WorldObjectInfoPanelSize.Y)};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			const FVector2D Size(DestX[Col + 1] - DestX[Col], DestY[Row + 1] - DestY[Row]);
			PlaceOnCanvas(Panel, MakeAtlasImage(
				*FString::Printf(TEXT("WorldObjectInfoPatch_%d_%d"), Row, Col), CommonAtlasPath,
				SpriteRect(WorldObjectPopBackgroundRect.Min.X + SourceX[Col],
					WorldObjectPopBackgroundRect.Min.Y + SourceY[Row + 1],
					SourceX[Col + 1] - SourceX[Col], SourceY[Row] - SourceY[Row + 1]),
				CommonAtlasSize, Size), FVector2D(DestX[Col], DestY[Row]), Size);
		}
	}
	WorldObjectInfoPanelCanvasSlot = PlaceOnCanvas(
		WorldObjectInfoRoot,
		Panel,
		FVector2D::ZeroVector,
		WorldObjectInfoPanelSize);
	if (WorldObjectInfoPanelCanvasSlot.IsValid())
	{
		WorldObjectInfoPanelCanvasSlot->SetZOrder(0);
	}

	UCanvasPanel* PanelContent =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldObjectInfoPanelContent"));
	PanelContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WorldObjectNormalRoot = PanelContent;
	PlaceOnCanvas(Panel, PanelContent, FVector2D::ZeroVector, WorldObjectInfoPanelSize)->SetZOrder(1);
	UButton* InputBlocker = MakeTransparentHitButton(
		TEXT("WorldObjectInfoInputBlocker"));
	if (UCanvasPanelSlot* InputBlockerSlot = PlaceOnCanvas(
			PanelContent,
			InputBlocker,
			FVector2D::ZeroVector,
			WorldObjectInfoPanelSize))
	{
		InputBlockerSlot->SetZOrder(-1);
	}

	WorldObjectInfoIcon =
		WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			TEXT("WorldObjectInfoIcon"));
	WorldObjectInfoIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		PanelContent,
		WorldObjectInfoIcon,
		FVector2D(25.0f, 31.0f),
		FVector2D(78.0f, 78.0f));

	WorldObjectInfoNameText = MakeHudText(
		TEXT("WorldObjectInfoName"),
		24,
		FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f),
		true);
	WorldObjectInfoNameText->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(
		PanelContent,
		WorldObjectInfoNameText,
		FVector2D(116.0f, 24.0f),
		FVector2D(190.0f, 36.0f));

	WorldObjectInfoLevelText = MakeHudText(
		TEXT("WorldObjectInfoLevel"),
		19,
		FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f));
	WorldObjectInfoLevelText->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(
		PanelContent,
		WorldObjectInfoLevelText,
		FVector2D(116.0f, 62.0f),
		FVector2D(244.0f, 28.0f));

	WorldObjectInfoCoordinateText = MakeHudText(
		TEXT("WorldObjectInfoCoordinate"),
		17,
		FLinearColor(0.22352941f, 0.21960784f, 0.20784314f, 1.0f));
	WorldObjectInfoCoordinateText->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(
		PanelContent,
		WorldObjectInfoCoordinateText,
		FVector2D(116.0f, 91.0f),
		FVector2D(244.0f, 26.0f));

	// UI_Pop_WorldObjectInfo: 60x60 transparent hit rect, centered 38x38
	// img_descinfo / img_descBack. No BtnAnimation on these two source buttons.
	auto MakeInfoIconButton = [&](const FName& Name, const FIntRect& Rect)
	{
		auto* Button = MakeTransparentHitButton(Name);
		auto* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		Button->SetContent(Content);
		PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()),
			CommonAtlasPath, Rect, CommonAtlasSize, FVector2D(38)), FVector2D(11), FVector2D(38));
		return Button;
	};
	UButton* DetailsButton = MakeInfoIconButton(TEXT("WorldObjectInfoDetailsButton"), SpriteRect(782, 237, 37, 37));
	DetailsButton->SetToolTipText(NSLOCTEXT("RokWorldMapHud", "WorldObjectDetails", "详情"));
	DetailsButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleWorldObjectDetailsClicked);
	PlaceOnCanvas(
		PanelContent,
		DetailsButton,
		FVector2D(316.5f, 18.5f),
		FVector2D(60));

	UButton* GatherButton =
		WidgetTree->ConstructWidget<URokUiAnimatedButton>(
			URokUiAnimatedButton::StaticClass(),
			TEXT("WorldObjectInfoGatherButton"));
	// UI_Item_WorldObjectPopBtns/UI_Model_1gbtn1 -> DoubleLineButton_Blue2.
	// SpriteSwap keeps the same blue art for normal/hover/press, grey when disabled.
	if (UTexture2D* Atlas = LoadUiTexture(CommonAtlasPath))
	{
		const FSlateBrush Blue = MakeAtlasBrush(Atlas, SpriteRect(1098, 571, 202, 81), CommonAtlasSize, FVector2D(202, 84));
		const FSlateBrush Disabled = MakeAtlasBrush(Atlas, SpriteRect(473, 528, 202, 81), CommonAtlasSize, FVector2D(202, 84));
		FButtonStyle Style;
		Style.SetNormal(Blue).SetHovered(Blue).SetPressed(Blue).SetDisabled(Disabled);
		Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		GatherButton->SetStyle(Style);
	}
	GatherButton->OnClicked.AddDynamic(
		this,
		&URokWorldMapHudWidget::HandleWorldObjectGatherClicked);
	UTextBlock* GatherText = MakeHudText(
		TEXT("WorldObjectInfoGatherButtonText"),
		32,
		FLinearColor(0.039215688f, 0.105882354f, 0.18039216f, 1),
		true);
	GatherText->SetShadowOffset(FVector2D::ZeroVector);
	GatherText->SetText(
		NSLOCTEXT("RokWorldMapHud", "WorldObjectGather", "采集"));
	GatherText->SetJustification(ETextJustify::Center);
	GatherButton->SetContent(GatherText);
	PlaceOnCanvas(
		PanelContent,
		GatherButton,
		FVector2D(94.0f, 128.0f),
		FVector2D(202.0f, 84.0f));

	WorldObjectInfoDetailsRoot =
		WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("WorldObjectInfoDetailsRoot"));
	WorldObjectInfoDetailsRoot->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(
		Panel,
		WorldObjectInfoDetailsRoot,
		FVector2D::ZeroVector,
		WorldObjectInfoPanelSize)->SetZOrder(2);
	PlaceOnCanvas(WorldObjectInfoDetailsRoot, MakeTransparentHitButton(TEXT("WorldObjectDescriptionInputBlocker")),
		FVector2D::ZeroVector, WorldObjectInfoPanelSize)->SetZOrder(-1);
	auto* Back = MakeInfoIconButton(TEXT("WorldObjectDescriptionBack"), SpriteRect(1171, 489, 38, 38));
	Back->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectDetailsClicked);
	Back->SetToolTipText(NSLOCTEXT("RokWorldMapHud", "ResourceDescriptionBack", "返回"));
	PlaceOnCanvas(WorldObjectInfoDetailsRoot, Back, FVector2D(315.5f, 5), FVector2D(60));
	auto* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldObjectDescriptionScroll"));
	PlaceOnCanvas(WorldObjectInfoDetailsRoot, DescriptionScroll, FVector2D(22, 58), FVector2D(346, 174));
	// UI_Pop_WorldObjectInfo/lbl_desc: 20pt regular, rich text enabled.
	// Preserve Language.cn <b> spans rather than stripping emphasis.
	auto* DetailsText = WidgetTree->ConstructWidget<URichTextBlock>(
		URichTextBlock::StaticClass(), TEXT("WorldObjectInfoDetailsText"));
	DetailsText->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* Styles = NewObject<UDataTable>(this);
	Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow Regular;
	Regular.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));
	Regular.TextStyle.SetColorAndOpacity(FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1));
	Regular.TextStyle.SetShadowOffset(FVector2D::ZeroVector);
	Styles->AddRow(TEXT("Default"), Regular);
	FRichTextStyleRow Bold = Regular;
	Bold.TextStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20));
	Styles->AddRow(TEXT("b"), Bold);
	DetailsText->SetTextStyleSet(Styles);
	WorldObjectDescriptionText = DetailsText;
	DetailsText->SetAutoWrapText(true);
	DetailsText->SetText(NSLOCTEXT(
		"RokWorldMapHud",
		"WorldObjectDetailsBoundary",
		"资源数量与采集状态需由世界服务器同步"));
	DetailsText->SetJustification(ETextJustify::Left);
	DescriptionScroll->AddChild(DetailsText);

	WorldObjectInfoFeedbackText = MakeHudText(
		TEXT("WorldObjectInfoFeedback"),
		14,
		FLinearColor(1.0f, 0.72f, 0.34f, 1.0f));
	WorldObjectInfoFeedbackText->SetText(FText::GetEmpty());
	WorldObjectInfoFeedbackText->SetJustification(ETextJustify::Center);
	WorldObjectInfoFeedbackText->SetAutoWrapText(true);
	PlaceOnCanvas(
		PanelContent,
		WorldObjectInfoFeedbackText,
		FVector2D(18.0f, 216.0f),
		FVector2D(354.0f, 28.0f));

	WorldObjectInfoArrow = MakeAtlasImage(
		TEXT("WorldObjectInfoArrow"),
		CommonAtlasPath,
		WorldObjectPopArrowRect,
		CommonAtlasSize,
		FVector2D(24.0f, 30.0f));
	WorldObjectInfoArrowCanvasSlot = PlaceOnCanvas(
		WorldObjectInfoRoot,
		WorldObjectInfoArrow,
		FVector2D(183.0f, 240.0f),
		FVector2D(24.0f, 30.0f));
	if (WorldObjectInfoArrowCanvasSlot.IsValid())
	{
		WorldObjectInfoArrowCanvasSlot->SetZOrder(1);
	}
}

void URokWorldMapHudWidget::UpdateExpandedDesignResolution(
	const FVector2D& RootSize)
{
	if (!ExpandedDesignSizeBox
		|| RootSize.X <= 1.0f
		|| RootSize.Y <= 1.0f)
	{
		return;
	}

	const float ViewAspect = RootSize.X / RootSize.Y;
	const float ReferenceAspect =
		UnityReferenceWidth / UnityReferenceHeight;
	FVector2D DesiredDesignSize;
	if (ViewAspect >= ReferenceAspect)
	{
		DesiredDesignSize = FVector2D(
			UnityReferenceHeight * ViewAspect,
			UnityReferenceHeight);
	}
	else
	{
		DesiredDesignSize = FVector2D(
			UnityReferenceWidth,
			UnityReferenceWidth / ViewAspect);
	}

	if (!DesiredDesignSize.Equals(CurrentExpandedDesignSize, 0.5f))
	{
		CurrentExpandedDesignSize = DesiredDesignSize;
		ExpandedDesignSizeBox->SetWidthOverride(
			CurrentExpandedDesignSize.X);
		ExpandedDesignSizeBox->SetHeightOverride(
			CurrentExpandedDesignSize.Y);
	}
}

void URokWorldMapHudWidget::UpdatePlayerMarkerProjection()
{
	if (!SelfMarkerRoot
		|| !SelfMarkerCanvasSlot.IsValid()
		|| CurrentRootSize.X <= 1.0f
		|| CurrentRootSize.Y <= 1.0f)
	{
		return;
	}

	ARokWorldMapCityActor* PlayerCity = ResolvePlayerCity();
	APlayerController* PlayerController = GetOwningPlayer();
	if (HomeIndicatorRoot) HomeIndicatorRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (SelfPortraitRoot) SelfPortraitRoot->SetVisibility(ESlateVisibility::Collapsed);
	if (!PlayerCity || !PlayerController)
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController,
			PlayerCity->GetVisualCenterWorldPosition(),
			ScreenPosition,
			true);
	if (!bProjected)
	{
		// Perspective projection rejects a city behind the camera. Retain
		// its direction instead of losing the home cursor when panning away.
		const ARokWorldMapCameraPawn* CameraPawn = Cast<ARokWorldMapCameraPawn>(PlayerController->GetPawn());
		if (!CameraPawn) { SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed); return; }
		const UCameraComponent* Camera = CameraPawn->GetCameraComponent();
		const FVector Delta = PlayerCity->GetActorLocation() - Camera->GetComponentLocation();
		FVector2D Direction(FVector::DotProduct(Delta, Camera->GetRightVector()),
			-FVector::DotProduct(Delta, Camera->GetUpVector()));
		if (Direction.IsNearlyZero()) Direction = FVector2D(0, 1);
		ScreenPosition = CurrentRootSize * 0.5f + Direction.GetSafeNormal() * CurrentRootSize.Size() * 2;
	}

	const FVector2D DesignPosition(
		ScreenPosition.X * CurrentExpandedDesignSize.X
			/ CurrentRootSize.X,
		ScreenPosition.Y * CurrentExpandedDesignSize.Y
			/ CurrentRootSize.Y);
	const bool bOutside = !bProjected || DesignPosition.X <= 0 || DesignPosition.Y <= 0
		|| DesignPosition.X >= CurrentExpandedDesignSize.X || DesignPosition.Y >= CurrentExpandedDesignSize.Y;
	if (bOutside)
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		const ARokWorldMapCameraPawn* CameraPawn = Cast<ARokWorldMapCameraPawn>(PlayerController->GetPawn());
		FRokWorldMapProjection Projection;
		if (CameraPawn && HomeIndicatorRoot && ResolveProjection(Projection))
		{
			const float DistanceKm = FVector2D::Distance(Projection.UeToUnity(PlayerCity->GetActorLocation()),
				CameraPawn->GetViewCenterUnity()) / 10.0f;
			if (RokWorldMapHomeIndicator::ShouldShow(DesignPosition, CurrentExpandedDesignSize, DistanceKm))
			{
				const FVector2D Center = RokWorldMapHomeIndicator::Position(
					DesignPosition, CurrentExpandedDesignSize, bTacticalPresentation);
				CastChecked<UCanvasPanelSlot>(HomeIndicatorRoot->Slot)->SetPosition(Center - FVector2D(50, 60));
				const FVector2D Direction = DesignPosition - CurrentExpandedDesignSize * 0.5f;
				HomeIndicatorArrow->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)) - 90.0f);
				HomeIndicatorDistance->SetText(FText::Format(NSLOCTEXT("RokWorldMapHud", "HomeDistance", "{0}公里"), FText::AsNumber(FMath::FloorToInt(DistanceKm))));
				HomeIndicatorRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			}
		}
		return;
	}
	const ARokWorldMapCameraPawn* MarkerCamera = Cast<ARokWorldMapCameraPawn>(PlayerController->GetPawn());
	const bool bPortraitBand = MarkerCamera && ShouldShowCityPortrait(MarkerCamera->GetCurrentDxf());
	if (SelfPortraitRoot && bPortraitBand)
	{
		// UIRoot TroopCurve, last unweighted Hermite segment. City has no
		// troop radius, so the extra TroopRadiusCure offset is zero.
		const float Offset = EvaluateCityPortraitOffset(MarkerCamera->GetCurrentDxf());
		FRokWorldMapProjection Projection;
		if (ResolveProjection(Projection))
		{
			FVector2D PortraitPoint;
			const FVector Anchor = PlayerCity->GetActorLocation()
				+ FVector(0, Offset, Offset)*Projection.GetSafeUeScale();
			if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, Anchor, PortraitPoint, true))
			{
				const UCameraComponent* Camera = MarkerCamera->GetCameraComponent();
				const float VerticalFov = FMath::RadiansToDegrees(2*FMath::Atan(
					FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView)*0.5f) / Camera->AspectRatio));
				const float Scale = EvaluateWorldObjectBadgeScale(MarkerCamera->GetCurrentDxf(),
					PortraitPoint.Y / CurrentRootSize.Y, VerticalFov);
				SelfPortraitRoot->SetRenderScale(FVector2D(Scale, Scale));
				CastChecked<UCanvasPanelSlot>(SelfPortraitRoot->Slot)->SetPosition(
					PortraitPoint * CurrentExpandedDesignSize / CurrentRootSize - FVector2D(100, 100));
				SelfPortraitRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
	if (!MarkerCamera || !ShouldShowCityTitle(MarkerCamera->GetCurrentDxf()))
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	FRokWorldMapProjection TitleProjection;
	FVector2D TitlePosition;
	if (!MarkerCamera || !ResolveProjection(TitleProjection)
		|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController,
			PlayerCity->GetActorLocation() + FVector(0,
				EvaluateCityTitleOffset(MarkerCamera->GetCurrentDxf()) * TitleProjection.GetSafeUeScale(),
				bPortraitBand ? EvaluateCityTitleOffset(MarkerCamera->GetCurrentDxf()) * TitleProjection.GetSafeUeScale() : 0),
			TitlePosition, true))
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// MapElementUI.WorldToUIPos: below 3000 CastleTitle offsets Unity z
	// only (UE Y). At/above 3000 it offsets both Unity z and y (UE Y/Z).
	// Neither band uses the imported mesh bounds center.
	const UCameraComponent* TitleCamera = MarkerCamera->GetCameraComponent();
	const float TitleVerticalFov = FMath::RadiansToDegrees(2*FMath::Atan(
		FMath::Tan(FMath::DegreesToRadians(TitleCamera->FieldOfView)*0.5f) / TitleCamera->AspectRatio));
	const float TitleScale = EvaluateWorldObjectBadgeScale(MarkerCamera->GetCurrentDxf(),
		TitlePosition.Y / CurrentRootSize.Y, TitleVerticalFov);
	SelfMarkerRoot->SetRenderScale(FVector2D(TitleScale, TitleScale));
	SelfMarkerCanvasSlot->SetPosition(TitlePosition * CurrentExpandedDesignSize / CurrentRootSize
		- SelfMarkerSize * 0.5f);
	SelfMarkerRoot->SetVisibility(
		ESlateVisibility::SelfHitTestInvisible);
}

void URokWorldMapHudWidget::UpdateWorldObjectInfoProjection()
{
	// The outgoing visual finishes at its last valid anchor after deselection.
	if (WorldObjectAnimation.IsClosing()) return;
	if (!WorldObjectInfoRoot
		|| !WorldObjectInfoCanvasSlot.IsValid()
		|| WorldObjectInfoRoot->GetVisibility()
			== ESlateVisibility::Collapsed)
	{
		return;
	}

	ARokWorldMapEntityActor* EntityActor =
		SelectedWorldObjectActor.Get();
	APlayerController* PlayerController = GetOwningPlayer();
	if (!EntityActor
		|| EntityActor->IsHidden()
		|| EntityActor->GetCurrentLodBand()
			== ERokWorldMapEntityLodBand::Hidden
		|| !PlayerController
		|| CurrentRootSize.X <= 1.0f
		|| CurrentRootSize.Y <= 1.0f)
	{
		if (bWorldObjectProjectionDebugPending
			&& FParse::Param(
				FCommandLine::Get(),
				TEXT("RokWorldMapDebugHud")))
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map resource info projection rejected: actor=%s hidden=%s lod=%d controller=%s root=(%.2f,%.2f)."),
				EntityActor ? *EntityActor->GetName() : TEXT("<none>"),
				EntityActor && EntityActor->IsHidden()
					? TEXT("true")
					: TEXT("false"),
				EntityActor
					? static_cast<int32>(EntityActor->GetCurrentLodBand())
					: -1,
				PlayerController ? TEXT("true") : TEXT("false"),
				CurrentRootSize.X,
				CurrentRootSize.Y);
		}
		bWorldObjectProjectionDebugPending = false;
		HideWorldObjectInfo();
		return;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!PlayerController->ProjectWorldLocationToScreen(
			EntityActor->GetSelectionAnchorWorldPosition(),
			ScreenPosition,
			true))
	{
		if (bWorldObjectProjectionDebugPending
			&& FParse::Param(
				FCommandLine::Get(),
				TEXT("RokWorldMapDebugHud")))
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map resource info projection failed: actor=%s."),
				*EntityActor->GetName());
		}
		bWorldObjectProjectionDebugPending = false;
		HideWorldObjectInfo();
		return;
	}

	// ProjectWorldLocationToScreen returns viewport pixels, while the widget
	// geometry below is expressed in DPI-scaled Slate units. Remove the viewport
	// scale before converting into the 1440x810 design space; otherwise the
	// popup is pushed off-screen on Windows display scaling above 100%.
	const float ViewportScale = FMath::Max(
		UWidgetLayoutLibrary::GetViewportScale(this),
		KINDA_SMALL_NUMBER);
	const FVector2D WidgetPosition = ScreenPosition / ViewportScale;
	const FVector2D DesignPosition(
		WidgetPosition.X * CurrentExpandedDesignSize.X
			/ CurrentRootSize.X,
		WidgetPosition.Y * CurrentExpandedDesignSize.Y
			/ CurrentRootSize.Y);
	const bool bPlaceAbove =
		DesignPosition.Y >= WorldObjectInfoSize.Y + 24.0f;
	const FVector2D DesiredTopLeft(
		DesignPosition.X - WorldObjectInfoSize.X * 0.5f,
		bPlaceAbove
			? DesignPosition.Y - WorldObjectInfoSize.Y - 8.0f
			: DesignPosition.Y + 8.0f);
	const FVector2D ClampedTopLeft = ClampMarkerTopLeft(
		DesiredTopLeft,
		WorldObjectInfoSize,
		CurrentExpandedDesignSize,
		12.0f);
	WorldObjectInfoCanvasSlot->SetPosition(ClampedTopLeft);

	const float ArrowX = FMath::Clamp(
		DesignPosition.X - ClampedTopLeft.X - 12.0f,
		18.0f,
		WorldObjectInfoSize.X - 42.0f);
	if (WorldObjectInfoPanelCanvasSlot.IsValid())
	{
		WorldObjectInfoPanelCanvasSlot->SetPosition(
			bPlaceAbove
				? FVector2D::ZeroVector
				: FVector2D(0.0f, 20.0f));
	}
	if (WorldObjectInfoArrowCanvasSlot.IsValid())
	{
		WorldObjectInfoArrowCanvasSlot->SetPosition(FVector2D(
			ArrowX,
			bPlaceAbove ? 240.0f : 0.0f));
	}
	if (WorldObjectInfoArrow)
	{
		WorldObjectInfoArrow->SetRenderTransformAngle(
			bPlaceAbove ? 0.0f : 180.0f);
	}
	if (bWorldObjectProjectionDebugPending
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("RokWorldMapDebugHud")))
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map resource info projection: screen=(%.2f,%.2f) viewport_scale=%.3f widget=(%.2f,%.2f) root=(%.2f,%.2f) design_size=(%.2f,%.2f) design=(%.2f,%.2f) popup=(%.2f,%.2f) above=%s."),
			ScreenPosition.X,
			ScreenPosition.Y,
			ViewportScale,
			WidgetPosition.X,
			WidgetPosition.Y,
			CurrentRootSize.X,
			CurrentRootSize.Y,
			CurrentExpandedDesignSize.X,
			CurrentExpandedDesignSize.Y,
			DesignPosition.X,
			DesignPosition.Y,
			ClampedTopLeft.X,
			ClampedTopLeft.Y,
			bPlaceAbove ? TEXT("true") : TEXT("false"));
	}
	bWorldObjectProjectionDebugPending = false;
}

void URokWorldMapHudWidget::UpdateMiniMapMarkers()
{
	if (!MiniMapViewportCanvasSlot.IsValid()
		|| !MiniMapSelfCanvasSlot.IsValid())
	{
		return;
	}

	FRokWorldMapProjection Projection;
	if (!ResolveProjection(Projection))
	{
		return;
	}

	if (const ARokWorldMapCameraPawn* CameraPawn =
			Cast<ARokWorldMapCameraPawn>(
				GetOwningPlayerPawn()))
	{
		const FVector2D ViewPoint = ComputeMiniMapPoint(
			Projection,
			CameraPawn->GetViewCenterUnity(),
			MiniMapContentSize);
		MiniMapViewportCanvasSlot->SetPosition(
			MiniMapContentOffset
			+ ViewPoint
			- FVector2D(9.0f, 6.0f));
	}

	if (ARokWorldMapCityActor* PlayerCity = ResolvePlayerCity())
	{
		const FVector2D PlayerUnity =
			Projection.UeToUnity(PlayerCity->GetActorLocation());
		const FVector2D SelfPoint = ComputeMiniMapPoint(
			Projection,
			PlayerUnity,
			MiniMapContentSize);
		MiniMapSelfCanvasSlot->SetPosition(
			MiniMapContentOffset
			+ SelfPoint
			- FVector2D(4.0f, 4.0f));
		MiniMapSelfDot->SetVisibility(
			ESlateVisibility::HitTestInvisible);
	}
	else
	{
		MiniMapSelfDot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokWorldMapHudWidget::RefreshRuntimeValues()
{
	const ARokWorldMapCameraPawn* CameraPawn =
		Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn());
	if (CoordinateText && CameraPawn)
	{
		const FVector2D DisplayCoordinate =
			CameraPawn->GetViewCenterDisplay();
		CoordinateText->SetText(FText::FromString(FString::Printf(
			TEXT("#1   X:%d Y:%d"),
			FMath::FloorToInt(DisplayCoordinate.X),
			FMath::FloorToInt(DisplayCoordinate.Y))));
	}

	if (PowerText || VipText)
	{
		FRokUnityRuntimePlayerProfileState ProfileState;
		bool bHasProfileState = false;
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<
					URokUnityRuntimeStateSubsystem>();
			if (RuntimeState
				&& RuntimeState->IsRuntimeStateLoaded())
			{
				ProfileState = RuntimeState->GetPlayerProfileState();
				bHasProfileState = true;
			}
		}
		if (PowerText)
		{
			SetPowerPresentation(bHasProfileState ? ProfileState.LocalPreviewScore : -1);
			PowerText->SetToolTipText(NSLOCTEXT(
				"RokWorldMapHud",
				"RuntimePowerTooltip",
				"基础预览评分＋当前城市升级增量，不是服务器正式战力"));
		}
		if (VipText)
		{
			VipText->SetText(ProfileState.bHasVipLevel
				? FText::Format(
					NSLOCTEXT("RokWorldMapHud", "RuntimeVipValue", "VIP {0}"),
					FText::AsNumber(ProfileState.VipLevel))
				: NSLOCTEXT("RokWorldMapHud", "RuntimeVipUnavailable", "VIP --"));
		}
	}

}

void URokWorldMapHudWidget::ApplyEntityFilter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn()))
	{
		const bool bResources = bStrategicModulesVisible && SelectedFilter == ERokWorldMapHudFilter::Resources;
		FRokWorldMapProjection Projection;
		if (ResolveProjection(Projection))
		{
			if (bResources && !StrategicResourceLayer.IsValid())
			{
				FActorSpawnParameters Params;
				Params.ObjectFlags |= RF_Transient;
				StrategicResourceLayer = World->SpawnActor<ARokStrategicResourceLayer>(Params);
			}
			if (StrategicResourceLayer.IsValid())
				StrategicResourceLayer->UpdateView(bResources, Projection, Camera->GetViewCenterUnity(), Camera->GetCurrentDxf());
			const bool bExplore = bStrategicModulesVisible && SelectedFilter == ERokWorldMapHudFilter::Explore;
			if (bExplore && !StrategicExplorationLayer.IsValid())
			{
				FActorSpawnParameters Params;
				Params.ObjectFlags |= RF_Transient;
				StrategicExplorationLayer = World->SpawnActor<ARokStrategicResourceLayer>(Params);
				if (StrategicExplorationLayer.IsValid()) StrategicExplorationLayer->ConfigureExploration();
			}
			if (StrategicExplorationLayer.IsValid())
				StrategicExplorationLayer->UpdateView(bExplore, Projection, Camera->GetViewCenterUnity(), Camera->GetCurrentDxf());
		}
	}

	for (TActorIterator<ARokWorldMapEntityActor> EntityIterator(World);
		EntityIterator;
		++EntityIterator)
	{
		ARokWorldMapEntityActor* EntityActor = *EntityIterator;
		const bool bFilterVisible = !bStrategicModulesVisible || ShouldShowEntityForFilter(
			EntityActor->GetEntityKind(),
			SelectedFilter);
		const bool bLodVisible =
			EntityActor->GetCurrentLodBand()
			!= ERokWorldMapEntityLodBand::Hidden;
		const bool bVisible = bFilterVisible && bLodVisible;
		EntityActor->SetActorHiddenInGame(!bVisible);
		EntityActor->SetActorEnableCollision(bVisible);
		if (!bVisible
			&& SelectedWorldObjectActor.Get() == EntityActor)
		{
			if (ARokWorldMapPlayerController* WorldMapController =
					Cast<ARokWorldMapPlayerController>(
						GetOwningPlayer()))
			{
				WorldMapController->ClearWorldMapSelection();
			}
			else
			{
				HideWorldObjectInfo();
			}
		}
	}
}

ARokWorldMapCityActor* URokWorldMapHudWidget::ResolvePlayerCity()
{
	if (PlayerCityActor.IsValid())
	{
		return PlayerCityActor.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ARokWorldMapCityActor> CityIterator(World);
		CityIterator;
		++CityIterator)
	{
		if (CityIterator->Tags.Contains(
				FRokWorldMapMath::WorldMapPlayerCityTag()))
		{
			PlayerCityActor = *CityIterator;
			return PlayerCityActor.Get();
		}
	}
	return nullptr;
}

bool URokWorldMapHudWidget::ResolveProjection(
	FRokWorldMapProjection& OutProjection) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	TActorIterator<ARokWorldMapConfigActor> ConfigIterator(World);
	if (ConfigIterator)
	{
		OutProjection = ConfigIterator->Projection;
		return OutProjection.IsValid();
	}
	OutProjection = FRokWorldMapProjection();
	return OutProjection.IsValid();
}

void URokWorldMapHudWidget::FocusPlayerLocation()
{
	ARokWorldMapPlayerController* WorldMapController =
		Cast<ARokWorldMapPlayerController>(GetOwningPlayer());
	if (WorldMapController)
	{
		WorldMapController->FocusPlayerLocation();
	}
}

void URokWorldMapHudWidget::FocusMiniMapLocalPoint(
	const FVector2D& LocalPoint,
	const FVector2D& LocalSize)
{
	ClearWorldObjectSelection();
	ARokWorldMapCameraPawn* CameraPawn =
		Cast<ARokWorldMapCameraPawn>(GetOwningPlayerPawn());
	FRokWorldMapProjection Projection;
	if (!CameraPawn || !ResolveProjection(Projection))
	{
		return;
	}

	CameraPawn->SetViewCenterUnity(
		ComputeUnityPositionFromMiniMapPoint(
			Projection,
			LocalPoint,
			LocalSize));
}

bool URokWorldMapHudWidget::IsServerListOpen() const
{
	return ServerListOverlay
		&& ServerListOverlay->GetVisibility()
			!= ESlateVisibility::Collapsed;
}

bool URokWorldMapHudWidget::IsWorldObjectInfoOpen() const
{
	return WorldObjectInfoRoot
		&& !WorldObjectAnimation.IsClosing()
		&& WorldObjectInfoRoot->GetVisibility()
			!= ESlateVisibility::Collapsed;
}

const TCHAR* URokWorldMapHudWidget::ResolveWorldObjectIconPath(
	const FString& EntityKind) const
{
	FString NormalizedKind = EntityKind;
	NormalizedKind.TrimStartAndEndInline();
	NormalizedKind.ToLowerInline();
	if (NormalizedKind == TEXT("food"))
	{
		return FoodIconPath;
	}
	if (NormalizedKind == TEXT("wood"))
	{
		return WoodIconPath;
	}
	if (NormalizedKind == TEXT("stone"))
	{
		return StoneIconPath;
	}
	if (NormalizedKind == TEXT("gold"))
	{
		return GoldIconPath;
	}
	return nullptr;
}

void URokWorldMapHudWidget::ShowWorldObjectInfo(AActor* Actor)
{
	ARokWorldMapEntityActor* EntityActor =
		Cast<ARokWorldMapEntityActor>(Actor);
	if (!EntityActor
		|| !IsResourceEntityKind(EntityActor->GetEntityKind()))
	{
		HideWorldObjectInfo();
		return;
	}

	SelectedWorldObjectActor = EntityActor;
	if (WorldObjectDescriptionText)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Descriptions = nullptr;
		const TSharedPtr<FJsonObject>* Description = nullptr;
		FString Text;
		if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/world_resource_descriptions.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()
			&& Root->TryGetObjectField(TEXT("descriptions"), Descriptions)
			&& (*Descriptions)->TryGetObjectField(EntityActor->GetEntityKind(), Description)
			&& (*Description)->TryGetStringField(TEXT("slate_text"), Text))
		{
			WorldObjectDescriptionText->SetText(FText::FromString(Text));
		}
		else
		{
			WorldObjectDescriptionText->SetText(NSLOCTEXT("RokWorldMapHud", "DescriptionMissing", "资源说明尚未导入"));
			UE_LOG(LogTemp, Warning, TEXT("Missing source resource description: %s"), *EntityActor->GetEntityKind());
		}
	}
	bWorldObjectProjectionDebugPending = true;
	SetWorldObjectDetailsExpanded(false);
	WorldObjectPageSeconds = 0.33333334f; // initial ordinary page has no Show clip in Unity
	AdvanceWorldObjectPageAnimation(0);
	if (WorldObjectInfoFeedbackText)
	{
		WorldObjectInfoFeedbackText->SetText(FText::GetEmpty());
	}
	if (WorldObjectInfoNameText)
	{
		WorldObjectInfoNameText->SetText(
			FText::FromString(EntityActor->GetDisplayName()));
	}
	if (WorldObjectInfoLevelText)
	{
		WorldObjectInfoLevelText->SetText(FText::Format(
			NSLOCTEXT(
				"RokWorldMapHud",
				"WorldObjectLevel",
				"等级 {0}"),
			FText::AsNumber(EntityActor->GetEntityLevel())));
	}
	if (WorldObjectInfoCoordinateText)
	{
		FVector2D DisplayPosition = EntityActor->GetUnityPosition();
		FRokWorldMapProjection Projection;
		if (ResolveProjection(Projection))
		{
			DisplayPosition = Projection.UnityToDisplay(
				EntityActor->GetUnityPosition());
		}
		WorldObjectInfoCoordinateText->SetText(FText::FromString(
			FString::Printf(
				TEXT("#1  X:%d Y:%d"),
				FMath::FloorToInt(DisplayPosition.X),
				FMath::FloorToInt(DisplayPosition.Y))));
	}
	if (WorldObjectInfoIcon)
	{
		const TCHAR* IconPath = ResolveWorldObjectIconPath(
			EntityActor->GetEntityKind());
		if (IconPath)
		{
			if (UTexture2D* IconTexture = LoadUiTexture(IconPath))
			{
				WorldObjectInfoIcon->SetBrushFromTexture(
					IconTexture,
					true);
				WorldObjectInfoIcon->SetVisibility(
					ESlateVisibility::HitTestInvisible);
			}
			else
			{
				WorldObjectInfoIcon->SetVisibility(
					ESlateVisibility::Collapsed);
			}
		}
		else
		{
			WorldObjectInfoIcon->SetVisibility(
				ESlateVisibility::Collapsed);
		}
	}
	if (WorldObjectInfoRoot)
	{
		WorldObjectAnimation.Open(ERokUiWindowAnimationStyle::WorldObject);
		AdvanceWorldObjectAnimation(0.0f);
		WorldObjectInfoRoot->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}
	UpdateWorldObjectInfoProjection();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map resource info opened: source_id=%d kind=%s name=%s level=%d."),
		EntityActor->GetSourceId(),
		*EntityActor->GetEntityKind(),
		*EntityActor->GetDisplayName(),
		EntityActor->GetEntityLevel());
}

void URokWorldMapHudWidget::HideWorldObjectInfo()
{
	SelectedWorldObjectActor.Reset();
	bWorldObjectProjectionDebugPending = false;
	if (WorldObjectAnimation.IsClosing()) return;
	if (IsWorldObjectInfoOpen() && WorldObjectAnimation.GetStyle() == ERokUiWindowAnimationStyle::WorldObject)
	{
		URokUiAnimatedButton::ResetAnimationsInHierarchy(WorldObjectInfoRoot);
		WorldObjectAnimation.Close();
		WorldObjectInfoRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	SetWorldObjectDetailsExpanded(false);
	if (WorldObjectInfoFeedbackText)
	{
		WorldObjectInfoFeedbackText->SetText(FText::GetEmpty());
	}
	if (WorldObjectInfoRoot)
	{
		WorldObjectInfoRoot->SetVisibility(
			ESlateVisibility::Collapsed);
	}
}

void URokWorldMapHudWidget::AdvanceWorldObjectAnimation(float DeltaSeconds)
{
	if (!WorldObjectInfoRoot) return;
	if (WorldObjectAnimation.Advance(DeltaSeconds))
	{
		WorldObjectInfoRoot->SetVisibility(ESlateVisibility::Collapsed);
		WorldObjectAnimation.Reset();
		HideWorldObjectInfo();
	}
	WorldObjectInfoRoot->SetRenderScale(FVector2D(WorldObjectAnimation.GetSample().Scale));
}

void URokWorldMapHudWidget::ClearWorldObjectSelection()
{
	if (ARokWorldMapPlayerController* WorldMapController =
			Cast<ARokWorldMapPlayerController>(GetOwningPlayer()))
	{
		WorldMapController->ClearWorldMapSelection();
	}
	else
	{
		HideWorldObjectInfo();
	}
}

void URokWorldMapHudWidget::SetWorldObjectDetailsExpanded(
	bool bExpanded)
{
	URokUiAnimatedButton::ResetAnimationsInHierarchy(WorldObjectNormalRoot);
	bWorldObjectDetailsExpanded = bExpanded;
	if (WorldObjectInfoDetailsRoot)
	{
		WorldObjectInfoDetailsRoot->SetVisibility(
			bExpanded
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (WorldObjectNormalRoot) WorldObjectNormalRoot->SetVisibility(bExpanded
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	WorldObjectPageSeconds = 0;
	if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("WorldObjectDescriptionScroll")))) Scroll->ScrollToStart();
	AdvanceWorldObjectPageAnimation(0);
}

void URokWorldMapHudWidget::AdvanceWorldObjectPageAnimation(float DeltaSeconds)
{
	// UA_WorldObject_NormalShow / UA_WorldObject_DescShow: 20 frames,
	// X -100/+100 -> 0 and CanvasGroup alpha 0 -> 1, zero-tangent cubic.
	WorldObjectPageSeconds = FMath::Clamp(WorldObjectPageSeconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.33333334f);
	const float T = WorldObjectPageSeconds / 0.33333334f;
	const float Alpha = T * T * (3.0f - 2.0f * T);
	UCanvasPanel* Page = bWorldObjectDetailsExpanded ? WorldObjectInfoDetailsRoot.Get() : WorldObjectNormalRoot.Get();
	if (Page)
	{
		Page->SetRenderOpacity(Alpha);
		Page->SetRenderTranslation(FVector2D((bWorldObjectDetailsExpanded ? 100 : -100) * (1 - Alpha), 0));
	}
}

void URokWorldMapHudWidget::HandleWorldObjectDetailsClicked()
{
	SetWorldObjectDetailsExpanded(!bWorldObjectDetailsExpanded);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map resource info details toggled: expanded=%s."),
		bWorldObjectDetailsExpanded ? TEXT("true") : TEXT("false"));
	PlayResourceClickSound();
}

void URokWorldMapHudWidget::HandleWorldObjectGatherClicked()
{
	ARokWorldMapEntityActor* EntityActor =
		SelectedWorldObjectActor.Get();
	if (WorldObjectInfoFeedbackText)
	{
		WorldObjectInfoFeedbackText->SetText(NSLOCTEXT(
			"RokWorldMapHud",
			"WorldObjectGatherBoundary",
			"世界资源状态未同步，暂不能发起采集"));
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map gather boundary: source_id=%d kind=%s; no march command or resource state was mutated."),
		EntityActor ? EntityActor->GetSourceId() : 0,
		EntityActor ? *EntityActor->GetEntityKind() : TEXT("<none>"));
	PlayResourceClickSound();
}

void URokWorldMapHudWidget::PlayResourceClickSound()
{
	if (!ResourceClickSound) return;
#if WITH_DEV_AUTOMATION_TESTS
	++ResourceClickSoundRequests;
#endif
	// Source calls audio after the click action, not on press/release or Tick.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URokAudioRouterSubsystem* Router = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
		{
			Router->PlaySfx(ResourceClickSound);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("RokSelectionLodAudit")))
	{
		UE_LOG(LogTemp, Display, TEXT("Rok resource popup click audio requested: %s"), *ResourceClickSound->GetPathName());
	}
}

void URokWorldMapHudWidget::OpenCommandLineModal()
{
	FString ModalName;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("RokOpenModal="),
			ModalName)
		|| ModalName.IsEmpty())
	{
		return;
	}

	ModalName.TrimStartAndEndInline();
	ModalName.ToLowerInline();
	ClearWorldObjectSelection();
	if (ModalName == TEXT("profile")
		|| ModalName == TEXT("player_profile"))
	{
		HandleOpenPlayerProfileClicked();
	}
	else if (ModalName == TEXT("player_head")
		|| ModalName == TEXT("avatar")
		|| ModalName == TEXT("head_selection"))
	{
		HandleOpenPlayerProfileClicked();
		if (ModalWidget) ModalWidget->OpenPlayerHeadSelection();
	}
	else if (ModalName == TEXT("player_power")
		|| ModalName == TEXT("power_details")
		|| ModalName == TEXT("player_details"))
	{
		HandleOpenPlayerProfileClicked();
		if (ModalWidget) ModalWidget->OpenPlayerPowerDetails();
	}
	else if (ModalName == TEXT("ranking")
		|| ModalName == TEXT("rank")
		|| ModalName == TEXT("player_ranking"))
	{
		HandleOpenPlayerProfileClicked();
		if (ModalWidget) ModalWidget->OpenPlayerRanking();
	}
	else if (ModalName == TEXT("civilization")
		|| ModalName == TEXT("civilization_selection")
		|| ModalName == TEXT("change_civilization"))
	{
		HandleOpenPlayerProfileClicked();
		if (ModalWidget) ModalWidget->OpenCivilizationSelection();
	}
	else if (ModalName == TEXT("civilization_confirmation"))
	{
		HandleOpenPlayerProfileClicked();
		if (ModalWidget)
		{
			ModalWidget->OpenCivilizationSelection();
			ModalWidget->OpenCivilizationChangeConfirmation();
		}
	}
	else if (ModalName == TEXT("city_manager")
		|| ModalName == TEXT("citymanager")
		|| ModalName == TEXT("city_buffs"))
	{
		HandleOpenCityManagerClicked();
	}
	else if (ModalName == TEXT("army_show")
		|| ModalName == TEXT("armyshow")
		|| ModalName == TEXT("marches")
		|| ModalName == TEXT("troop_queue"))
	{
		HandleOpenArmyShowClicked();
	}
	else if (ModalName == TEXT("vip")
		|| ModalName == TEXT("vip0"))
	{
		HandleOpenVipClicked();
	}
	else if (ModalName == TEXT("chat"))
	{
		HandleOpenChatClicked();
	}
	else if (ModalName == TEXT("position")
		|| ModalName == TEXT("coordinate")
		|| ModalName == TEXT("world_position"))
	{
		HandleOpenPositionSearchClicked();
	}
	else if (ModalName == TEXT("markers")
		|| ModalName == TEXT("bookmarks")
		|| ModalName == TEXT("favorite"))
	{
		HandleOpenMapMarkersClicked();
	}
	else if (ModalName == TEXT("resource_search")
		|| ModalName == TEXT("search_resource")
		|| ModalName == TEXT("search_res"))
	{
		HandleOpenResourceSearchClicked();
	}
	else if (ModalName == TEXT("mail")
		|| ModalName == TEXT("email"))
	{
		HandleOpenMailClicked();
	}
	else if (ModalName == TEXT("mail_reference")
		|| ModalName == TEXT("mail_preview"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenMail(true);
		}
	}
	else if (ModalName == TEXT("settings"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenSettingsHub();
		}
	}
	else if (ModalName == TEXT("general_settings")
		|| ModalName == TEXT("generalsetting"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenGeneralSettings();
		}
	}
	else if (ModalName == TEXT("notification_settings")
		|| ModalName == TEXT("notifications"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenNotificationSettings();
		}
	}
	else if (ModalName == TEXT("language_settings")
		|| ModalName == TEXT("language"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenLanguageSettings();
		}
	}
	else if (ModalName == TEXT("account_settings")
		|| ModalName == TEXT("account"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenAccountSettings();
		}
	}
	else if (ModalName == TEXT("army")
		|| ModalName == TEXT("army_statistics")
		|| ModalName == TEXT("armyconst"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenArmyStatistics();
		}
	}
	else if (ModalName == TEXT("store")
		|| ModalName == TEXT("normal_store")
		|| ModalName == TEXT("shop"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenStore();
		}
	}
	else if (ModalName == TEXT("charge")
		|| ModalName == TEXT("recharge")
		|| ModalName == TEXT("ui_charge"))
	{
		OpenFeatureScreen();
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->OpenCharge(107);
		}
	}
	else if (ModalName == TEXT("warn")
		|| ModalName == TEXT("war_warning"))
	{
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->CloseScreen();
		}
		if (ServerListOverlay)
		{
			ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ModalWidget)
		{
			ModalWidget->OpenWarWarning(false);
		}
	}
	else if (ModalName == TEXT("warn_reference")
		|| ModalName == TEXT("war_warning_reference"))
	{
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->CloseScreen();
		}
		if (ServerListOverlay)
		{
			ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ModalWidget)
		{
			ModalWidget->OpenWarWarning(true);
		}
	}
	else if (ModalName == TEXT("buff")
		|| ModalName == TEXT("buff_list")
		|| ModalName == TEXT("buffs"))
	{
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->CloseScreen();
		}
		if (ServerListOverlay)
		{
			ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ModalWidget)
		{
			ModalWidget->OpenBuffList(false);
		}
	}
	else if (ModalName == TEXT("buff_reference")
		|| ModalName == TEXT("buff_list_reference"))
	{
		if (FeatureScreenWidget)
		{
			FeatureScreenWidget->CloseScreen();
		}
		if (ServerListOverlay)
		{
			ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ModalWidget)
		{
			ModalWidget->OpenBuffList(true);
		}
	}
	else if (ModalName == TEXT("server_list")
		|| ModalName == TEXT("kingdom_list")
		|| ModalName == TEXT("kingdoms"))
	{
		HandleOpenServerListClicked();
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world-map modal UI: unknown -RokOpenModal value '%s'."),
			*ModalName);
		return;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map modal UI: opened command-line mode '%s'."),
		*ModalName);
}

void URokWorldMapHudWidget::HandleOpenResourceSearchClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: resource_search."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenResourceSearch();
	}
}

void URokWorldMapHudWidget::HandleOpenMailClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: mail."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenMail(false);
	}
}

void URokWorldMapHudWidget::OpenFeatureScreen()
{
	ClearWorldObjectSelection();
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (ServerListOverlay)
	{
		ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokWorldMapHudWidget::HandleOpenPlayerProfileClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: player_profile."));
	ClearWorldObjectSelection();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->OpenPlayerProfile();
	}
	if (ServerListOverlay)
	{
		ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokWorldMapHudWidget::HandleOpenCityManagerClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: city_manager."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenCityManager();
	}
}

void URokWorldMapHudWidget::HandleOpenArmyShowClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: army_show."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenArmyShow();
	}
}

void URokWorldMapHudWidget::HandleOpenVipClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: vip."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenVip();
	}
}

void URokWorldMapHudWidget::HandleOpenPositionSearchClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: position_search."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenWorldPositionSearch();
	}
}

void URokWorldMapHudWidget::HandleOpenMapMarkersClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: map_markers."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenMapMarkers();
	}
}

void URokWorldMapHudWidget::HandleOpenChatClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: chat."));
	OpenFeatureScreen();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenChat();
	}
}

void URokWorldMapHudWidget::HandleEnterPlayerCityClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: enter_player_city."));
	ClearWorldObjectSelection();
	ARokWorldMapPlayerController* WorldMapController =
		Cast<ARokWorldMapPlayerController>(GetOwningPlayer());
	UWorld* World = GetWorld();
	if (!WorldMapController || !World)
	{
		return;
	}

	TActorIterator<ARokWorldMapConfigActor> ConfigIterator(World);
	if (!ConfigIterator)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world-map city button failed: config_missing."));
		return;
	}

	WorldMapController->RequestEnterPlayerCity(
		ConfigIterator->PlayerCityLevel,
		ConfigIterator->GetCityEntryTargetDxf());
}

void URokWorldMapHudWidget::HandleOpenServerListClicked()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map HUD click: kingdom_list."));
	ClearWorldObjectSelection();
	if (FeatureScreenWidget)
	{
		FeatureScreenWidget->CloseScreen();
	}
	if (ModalWidget)
	{
		ModalWidget->CloseModal();
	}
	if (ServerListStatusText)
	{
		ServerListStatusText->SetText(FText::GetEmpty());
	}
	if (ServerListOverlay)
	{
		ServerListOverlay->SetVisibility(
			ESlateVisibility::SelfHitTestInvisible);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map kingdom list opened; runtime server-role list boundary is not migrated."));
}

void URokWorldMapHudWidget::HandleCloseServerListClicked()
{
	if (ServerListOverlay)
	{
		ServerListOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokWorldMapHudWidget::HandleServerSelectionUnavailableClicked()
{
	if (ServerListStatusText)
	{
		ServerListStatusText->SetText(NSLOCTEXT(
			"RokWorldMapHud",
			"ServerSelectionUnavailable",
			"选择王国功能暂未开放"));
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map kingdom selection rejected: server-role list service is not migrated."));
}

void URokWorldMapHudWidget::AdvanceFilterInputFeedback(float DeltaSeconds)
{
	for (int32 Index = 0; Index < FilterButtons.Num(); ++Index)
	{
		UButton* Button = FilterButtons[Index];
		if (!Button || !FilterBackgrounds.IsValidIndex(Index) || !FilterTints.IsValidIndex(Index)) continue;
		bool bVisible = IsVisible();
		for (const UWidget* Parent = Button; Parent; Parent = Parent->GetParent())
			bVisible = bVisible && Parent->IsVisible();
		if (!bVisible)
			FilterTints[Index] = FRokWorldMapFilterTint();
		else
			FilterTints[Index].Advance(FRokWorldMapFilterTint::Resolve(Button->GetIsEnabled(),
				Button->IsPressed(), Button->HasAnyUserFocus(), Button->IsHovered()), DeltaSeconds);
		FilterBackgrounds[Index]->SetColorAndOpacity(FilterTints[Index].Current);
	}
}

void URokWorldMapHudWidget::AdvanceFilterSelectionAnimation(float DeltaSeconds)
{
	if (!FMath::IsFinite(DeltaSeconds)) return;
	FilterAnimationSeconds = FMath::Clamp(FilterAnimationSeconds + FMath::Max(DeltaSeconds, 0.0f), 0.0f, 0.1f);
	// Unity 2019 Toggle.PlayEffect -> Graphic.CrossFadeAlpha, linear 0.1s.
	// Restart from current alpha when another click interrupts the fade.
	for (int32 Index = 0; Index < FilterDots.Num(); ++Index)
	{
		if (!FilterDots[Index] || !FilterStartAlphas.IsValidIndex(Index)) continue;
		const float Target = SelectedFilter == static_cast<ERokWorldMapHudFilter>(Index + 1) ? 1.0f : 0.0f;
		FilterDots[Index]->SetRenderOpacity(FMath::Lerp(FilterStartAlphas[Index], Target, FilterAnimationSeconds / 0.1f));
	}
}

void URokWorldMapHudWidget::SelectRelationFilter(
	ERokWorldMapHudFilter Filter)
{
	// Only the four click handlers call this: toggling off also plays once, never on Tick/refresh.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URokAudioRouterSubsystem* Router = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
		{
			Router->PlaySfx(FilterClickSound);
		}
	}
	SelectedFilter =
		SelectedFilter == Filter
			? ERokWorldMapHudFilter::None
			: Filter;
	FilterStartAlphas.Reset();
	for (const auto& Dot : FilterDots)
		FilterStartAlphas.Add(Dot ? Dot->GetRenderOpacity() : 0.0f);
	FilterAnimationSeconds = 0.0f;
	UpdateFilterLegend();

	ApplyEntityFilter();
	if (SelectedFilter == ERokWorldMapHudFilter::Resources
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("RokWorldMapDebugHud")))
	{
		APlayerController* PlayerController = GetOwningPlayer();
		UWorld* World = GetWorld();
		if (PlayerController && World)
		{
			for (TActorIterator<ARokWorldMapEntityActor> EntityIterator(World);
				EntityIterator;
				++EntityIterator)
			{
				ARokWorldMapEntityActor* EntityActor = *EntityIterator;
				if (!IsResourceEntityKind(EntityActor->GetEntityKind())
					|| EntityActor->IsHidden())
				{
					continue;
				}
				FVector2D ScreenPosition = FVector2D::ZeroVector;
				const bool bProjected =
					PlayerController->ProjectWorldLocationToScreen(
						EntityActor->GetSelectionAnchorWorldPosition(),
						ScreenPosition,
						true);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Rok world-map resource pointer target: source_id=%d kind=%s projected=%s screen=(%.2f,%.2f)."),
					EntityActor->GetSourceId(),
					*EntityActor->GetEntityKind(),
					bProjected ? TEXT("true") : TEXT("false"),
					ScreenPosition.X,
					ScreenPosition.Y);
			}
		}
	}

	const TCHAR* FilterName = TEXT("None");
	switch (SelectedFilter)
	{
	case ERokWorldMapHudFilter::Alliance:
		FilterName = TEXT("Alliance");
		break;
	case ERokWorldMapHudFilter::Explore:
		FilterName = TEXT("Explore");
		break;
	case ERokWorldMapHudFilter::Resources:
		FilterName = TEXT("Resources");
		break;
	case ERokWorldMapHudFilter::Markers:
		FilterName = TEXT("Markers");
		break;
	case ERokWorldMapHudFilter::None:
	default:
		break;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map relation filter selected: %s. Alliance/marker records remain external data boundaries when absent from the runtime population."),
		FilterName);
}

void URokWorldMapHudWidget::HandleAllianceFilterClicked()
{
	SelectRelationFilter(ERokWorldMapHudFilter::Alliance);
}

void URokWorldMapHudWidget::HandleExploreFilterClicked()
{
	SelectRelationFilter(ERokWorldMapHudFilter::Explore);
}

void URokWorldMapHudWidget::HandleResourceFilterClicked()
{
	SelectRelationFilter(ERokWorldMapHudFilter::Resources);
}

void URokWorldMapHudWidget::HandleBookmarkFilterClicked()
{
	SelectRelationFilter(ERokWorldMapHudFilter::Markers);
}

UCanvasPanelSlot* URokWorldMapHudWidget::PlaceOnCanvas(
	UCanvasPanel* Parent,
	UWidget* Child,
	const FVector2D& Position,
	const FVector2D& Size,
	const FAnchors& Anchors,
	const FVector2D& Alignment)
{
	if (!Parent || !Child)
	{
		return nullptr;
	}
	UCanvasPanelSlot* CanvasSlot =
		Parent->AddChildToCanvas(Child);
	CanvasSlot->SetAnchors(Anchors);
	CanvasSlot->SetAlignment(Alignment);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
	return CanvasSlot;
}

UTextBlock* URokWorldMapHudWidget::MakeHudText(
	const FName& Name,
	int32 FontSize,
	const FLinearColor& Color,
	bool bBold)
{
	UTextBlock* TextBlock =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			Name);
	TextBlock->SetColorAndOpacity(FSlateColor(Color));
	TextBlock->SetFont(FCoreStyle::GetDefaultFontStyle(
		bBold ? TEXT("Bold") : TEXT("Regular"),
		FontSize));
	TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
	TextBlock->SetShadowColorAndOpacity(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));
	TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	return TextBlock;
}

UImage* URokWorldMapHudWidget::MakeAtlasImage(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize)
{
	UImage* Image =
		WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			Name);
	if (UTexture2D* Texture = LoadUiTexture(TexturePath))
	{
		Image->SetBrush(MakeAtlasBrush(
			Texture,
			UnityRect,
			AtlasSize,
			DesiredSize));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
	return Image;
}

UBorder* URokWorldMapHudWidget::MakeAtlasBorder(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FMargin& BrushMargin)
{
	UBorder* Border =
		WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			Name);
	Border->SetPadding(FMargin(0.0f));
	Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UTexture2D* Texture = LoadUiTexture(TexturePath))
	{
		Border->SetBrush(MakeAtlasBrush(
			Texture,
			UnityRect,
			AtlasSize,
			DesiredSize,
			ESlateBrushDrawType::Box,
			BrushMargin));
	}
	else
	{
		Border->SetBrushColor(
			FLinearColor(0.03f, 0.07f, 0.09f, 0.78f));
	}
	return Border;
}

UButton* URokWorldMapHudWidget::MakeAtlasButton(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize)
{
	UButton* Button =
		WidgetTree->ConstructWidget<URokUiAnimatedButton>(
			URokUiAnimatedButton::StaticClass(),
			Name);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(NoBrush);
	ButtonStyle.SetHovered(NoBrush);
	ButtonStyle.SetPressed(NoBrush);
	ButtonStyle.SetDisabled(NoBrush);
	ButtonStyle.SetNormalPadding(FMargin(0.0f));
	ButtonStyle.SetPressedPadding(FMargin(0.0f));
	Button->SetStyle(ButtonStyle);
	Button->SetBackgroundColor(FLinearColor::Transparent);
	Button->SetColorAndOpacity(FLinearColor::White);
	Button->SetContent(MakeAtlasImage(
		*FString::Printf(TEXT("%sImage"), *Name.ToString()),
		TexturePath,
		UnityRect,
		AtlasSize,
		DesiredSize));
	return Button;
}

UButton* URokWorldMapHudWidget::MakeTransparentHitButton(
	const FName& Name)
{
	UButton* Button =
		WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			Name);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle ButtonStyle;
	ButtonStyle.SetNormal(NoBrush);
	ButtonStyle.SetHovered(NoBrush);
	ButtonStyle.SetPressed(NoBrush);
	ButtonStyle.SetDisabled(NoBrush);
	ButtonStyle.SetNormalPadding(FMargin(0.0f));
	ButtonStyle.SetPressedPadding(FMargin(0.0f));
	Button->SetStyle(ButtonStyle);
	Button->SetBackgroundColor(FLinearColor::Transparent);
	return Button;
}

UTexture2D* URokWorldMapHudWidget::LoadUiTexture(
	const TCHAR* TexturePath) const
{
	UTexture2D* Texture =
		LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world-map HUD missing Unity atlas %s."),
			TexturePath);
	}
	return Texture;
}

UTexture2D* URokWorldMapHudWidget::LoadTransientUiTexture(
	const FString& FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		return nullptr;
	}
	UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FilePath);
	if (Texture)
	{
		Texture->NeverStream = true;
		Texture->Filter = TF_Bilinear;
		Texture->UpdateResource();
	}
	return Texture;
}

int32 URokWorldMapHudWidget::ReadInitialCityHallLevel()
{
	// The world-map city nameplate shows the city-hall level from the
	// initial city state manifest (building type 1 / 市政厅).
	const FString ManifestPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir(),
		TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
	{
		return 1;
	}
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject)
		|| !RootObject.IsValid())
	{
		return 1;
	}
	const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("buildings"), Buildings))
	{
		return 1;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Buildings)
	{
		const TSharedPtr<FJsonObject> Building =
			Value.IsValid() ? Value->AsObject() : nullptr;
		if (Building.IsValid()
			&& Building->GetNumberField(TEXT("type_id")) == 1.0)
		{
			double Level = 1.0;
			Building->TryGetNumberField(TEXT("level"), Level);
			return FMath::Max(1, FMath::RoundToInt(Level));
		}
	}
	return 1;
}

FSlateBrush URokWorldMapHudWidget::MakeAtlasBrush(
	UTexture2D* Texture,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	ESlateBrushDrawType::Type DrawAs,
	const FMargin& BrushMargin) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = DesiredSize;
	Brush.DrawAs = DrawAs;
	Brush.Margin = BrushMargin;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	Brush.ImageType = ESlateBrushImageType::FullColor;

	const float Top =
		static_cast<float>(AtlasSize.Y - UnityRect.Max.Y);
	const FVector2f MinUv(
		static_cast<float>(UnityRect.Min.X)
			/ static_cast<float>(AtlasSize.X),
		Top / static_cast<float>(AtlasSize.Y));
	const FVector2f MaxUv(
		static_cast<float>(UnityRect.Max.X)
			/ static_cast<float>(AtlasSize.X),
		(Top + static_cast<float>(UnityRect.Height()))
			/ static_cast<float>(AtlasSize.Y));
	Brush.SetUVRegion(FBox2f(MinUv, MaxUv));
	return Brush;
}
