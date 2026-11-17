#include "RokWorldMapHudWidget.h"

#include "Components/ProgressBar.h"
#include "RokServerArmySubsystem.h"
#include "RokDataPaths.h"
#include "RokPowerGainWidget.h"
#include "RokMainInterfaceModuleAnimation.h"
#include "RokWorldMapHomeIndicator.h"
#include "RokUiAnimatedButton.h"
#include "RokAudioRouterSubsystem.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
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
#include "RokUiWidgets.h"
#include "RokUiSpec.h"
#include "HAL/PlatformApplicationMisc.h"
#include "RokGameNetSubsystem.h"
#include "RokPlayerTopBar.h"
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
const TCHAR* ResourceIconAtlasPath =
	TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map_icon.rok_hud_atlas_map_icon");
const FIntPoint ResourceIconAtlasSize(1024, 2048);

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
const FVector2D WorldObjectInfoPanelSize(479.0f, 357.0f);
const FVector2D WorldBarbarianPanelSize(486.0f, 434.0f);

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
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const TSharedPtr<SWidget> SlateWidget = GetCachedWidget();
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !GetMapController() || !SlateWidget)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	MapPointer.Begin(GetViewportPosition(InMouseEvent));
	return FReply::Handled().CaptureMouse(SlateWidget.ToSharedRef());
}

FReply URokWorldMapSelectionInputWidget::NativeOnMouseButtonDoubleClick(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		? NativeOnMouseButtonDown(InGeometry, InMouseEvent)
		: Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

FReply URokWorldMapSelectionInputWidget::NativeOnMouseMove(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (MapPointer.IsActive() && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		if (auto* Controller = GetMapController())
		{
			UpdateMapPointer(*Controller, GetViewportPosition(InMouseEvent));
			return FReply::Handled();
		}
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply URokWorldMapSelectionInputWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!MapPointer.IsActive() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	if (auto* Controller = GetMapController())
	{
		const FVector2D Position = GetViewportPosition(InMouseEvent);
		UpdateMapPointer(*Controller, Position);
		if (MapPointer.EndAsClick())
		{
			const bool bSelected = Controller->SelectWorldMapActorAtScreenPosition(Position);
			if (FParse::Param(FCommandLine::Get(), TEXT("RokWorldMapDebugHud")))
			{
				UE_LOG(LogTemp, Display, TEXT("Rok world-map selection surface click: screen=(%.2f,%.2f) selected=%s."),
					Position.X, Position.Y, bSelected ? TEXT("true") : TEXT("false"));
			}
		}
	}
	MapPointer.Cancel();
	return FReply::Handled().ReleaseMouseCapture();
}

void URokWorldMapSelectionInputWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	MapPointer.Cancel();
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

ARokWorldMapPlayerController* URokWorldMapSelectionInputWidget::GetMapController() const
{
	return OwningHud.IsValid() ? Cast<ARokWorldMapPlayerController>(OwningHud->GetOwningPlayer()) : nullptr;
}

FVector2D URokWorldMapSelectionInputWidget::GetViewportPosition(const FPointerEvent& Event) const
{
	FVector2D PixelPosition, WidgetPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, Event.GetScreenSpacePosition(), PixelPosition, WidgetPosition);
	return PixelPosition;
}

void URokWorldMapSelectionInputWidget::UpdateMapPointer(ARokWorldMapPlayerController& Controller, FVector2D Position)
{
	MapPointer.Update(Position, [&Controller](FVector2D Previous, FVector2D Current)
	{
		if (auto* Camera = Cast<ARokWorldMapCameraPawn>(Controller.GetPawn()))
		{
			Camera->PanFromScreenDrag(Previous, Current);
		}
	});
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

TSubclassOf<URokWorldMapHudWidget> URokWorldMapHudWidget::ResolveHudClass()
{
	static const TSoftClassPtr<URokWorldMapHudWidget> WorldClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldMapHud.WBP_RokWorldMapHud_C")));
	UClass* HudClass = URokUiSettings::Resolve(WorldClass);
	return HudClass ? HudClass : StaticClass();
}

TSharedRef<SWidget> URokWorldMapHudWidget::RebuildWidget()
{
	if (!IsDesignTime())
	{
		BuildWidgetTree();
	}
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
	const UWorld* World = GetWorld();
	const int32 TaskLevel = World ? FCString::Atoi(World->URL.GetOption(TEXT("RokTaskBarbarianLevel="), TEXT("0"))) : 0;
	if (FeatureScreenWidget && TaskLevel >= 1 && TaskLevel <= 25)
	{
		OpenFeatureScreen();
		FeatureScreenWidget->OpenResourceSearch(TaskLevel);
		return;
	}

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
	UpdateEntityBadges();
	UpdateWorldObjectInfoProjection();
	UpdateGatherRoute();
	UpdateAttackMarchRoutes();
	UpdateMiniMapMarkers();

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator >= 0.2f)
	{
		RefreshAccumulator = 0.0f;
		ReconcileEntityBadges();
		ApplyEntityFilter();
		RefreshRuntimeValues();
		TryOpenFocusedWorldTarget();
		RefreshWorldMarchQueue();
		RefreshSelectedWorldObjectValues();
	}
}

void URokWorldMapHudWidget::BuildWidgetTree()
{
	if (!WidgetTree || ExpandedDesignCanvas)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (RootCanvas)
	{
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		BindHudLayout();
		if (!ExpandedDesignCanvas)
		{
			return;
		}
	}
	else
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokWorldMapHudRoot"));
		RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = RootCanvas;
		BuildHudLayout(RootCanvas);
	}

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

#if WITH_EDITOR
	if (URokUiSettings::IsLegacyForced())
	{
		FString Filename;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldHud="), Filename))
		{
			FString Error;
			if (RokUiSpec::ExportChildren(RootCanvas, Filename, TEXT("world_hud"), Error))
			{
				UE_LOG(LogTemp, Display, TEXT("Rok UI: HUD widget spec saved to %s"), *Filename);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: HUD widget spec export failed: %s"), *Error);
			}
		}
	}
#endif

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
	BuildWorldMarchQueue();
	BuildWorldOverlayArt();
	BuildWorldDamageArt();
	BuildGatherPanel();
#if WITH_EDITOR
	if (URokUiSettings::IsLegacyForced())
	{
		FString Filename;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldOverlayArt="), Filename))
		{
			FString Error;
			if (!RokUiSpec::ExportChildren(WorldOverlayArtRoot, Filename, TEXT("world_overlay_art"), Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: world overlay art export failed: %s"), *Error);
			}
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldDamageArt="), Filename))
		{
			FString Error;
			if (!RokUiSpec::ExportChildren(WorldDamageArtRoot, Filename, TEXT("world_damage_art"), Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: world damage art export failed: %s"), *Error);
			}
		}
		FString BattleRowSpecDir;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldBattleRows="), BattleRowSpecDir))
		{
			const auto Export = [&BattleRowSpecDir](const UPanelWidget* Panel, const FString& SpecName)
			{
				FString Error;
				if (!RokUiSpec::ExportChildren(Panel, BattleRowSpecDir / (SpecName + TEXT(".json")), SpecName, Error))
				{
					UE_LOG(LogTemp, Error, TEXT("Rok UI: %s spec export failed: %s"), *SpecName, *Error);
				}
			};
			auto* SampleReward = NewObject<UCanvasPanel>();
			BuildBarbarianRewardLayout(SampleReward, 0);
			Export(SampleReward, TEXT("world_barbarian_reward"));
			auto* SampleChoice = NewObject<UCanvasPanel>();
			const FAttackArmyChoiceLayout Choice = BuildAttackArmyChoiceLayout(0);
			Choice.Label->SetText(NSLOCTEXT("RokGather", "ArmyChoicePreview", "部队"));
			PlaceOnCanvas(SampleChoice, Choice.Root, FVector2D::ZeroVector, FVector2D::ZeroVector,
				FAnchors(0, 0, 1, 1))->SetAutoSize(true);
			Export(SampleChoice, TEXT("world_attack_army_choice"));
		}
		FString ProjectedSpecDir;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldProjected="), ProjectedSpecDir))
		{
			const auto Export = [&ProjectedSpecDir](const UPanelWidget* Panel, const FString& SpecName)
			{
				FString Error;
				if (!RokUiSpec::ExportChildren(Panel, ProjectedSpecDir / (SpecName + TEXT(".json")), SpecName, Error))
				{
					UE_LOG(LogTemp, Error, TEXT("Rok UI: %s spec export failed: %s"), *SpecName, *Error);
				}
			};
			Export(ExpandedDesignCanvas, TEXT("world_projected_hud"));
			FEntityBadgeEntry SampleBadge;
			SampleBadge.Root = NewObject<UCanvasPanel>();
			BuildEntityBadgeLayout(SampleBadge, 0, true);
			SampleBadge.LevelText->SetText(FText::AsNumber(1));
			Export(SampleBadge.Root, TEXT("world_projected_entity_badge"));
			UCanvasPanel* SampleMarch = NewObject<UCanvasPanel>();
			BuildWorldMarchRowLayout(SampleMarch, FString());
			Export(SampleMarch, TEXT("world_march_row"));
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldPanels="), Filename))
		{
			FString Error;
			if (!RokUiSpec::ExportChildren(ExpandedDesignCanvas, Filename, TEXT("world_panels"), Error))
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: world HUD panel spec export failed: %s"), *Error);
			}
		}
	}
#endif

	ModalWidget = WidgetTree->ConstructWidget<URokUnityModalWidget>(
		URokUnityModalWidget::ResolveShellClass(),
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

	const TSubclassOf<URokMainHudWidget> TacticalClass = GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
		? URokMainHudWidget::ResolveHudClass(true) : TSubclassOf<URokMainHudWidget>(URokMainHudWidget::StaticClass());
	TacticalHudWidget = WidgetTree->ConstructWidget<URokMainHudWidget>(
		TacticalClass,
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

void URokWorldMapHudWidget::BuildHudLayout(UCanvasPanel* RootCanvas)
{
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
	// The search overlay keeps the map and resources visible while hiding HUD controls.
	UCanvasPanel* Design = ExpandedDesignCanvas;
	WorldMapChrome = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WorldMapChrome"));
	auto* ChromeSlot = Design->AddChildToCanvas(WorldMapChrome);
	ChromeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ChromeSlot->SetOffsets(FMargin(0));
	WorldMapChrome->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ExpandedDesignCanvas = WorldMapChrome;
	BuildPlayerIdentity();
	BuildCoordinatePanel();
	BuildMiniMap();
	BuildRelationFilters();
	BuildBottomControls();
	ExpandedDesignCanvas = Design;
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
	const bool bSearching = FeatureScreenWidget && FeatureScreenWidget->IsResourceSearchOpen();
	if (WorldMapChrome)
		WorldMapChrome->SetVisibility(bSearching ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (WorldMarchQueueRoot)
		WorldMarchQueueRoot->SetRenderOpacity(bSearching || (GatherRoot && GatherRoot->IsVisible()) ? 0.0f : 1.0f);
	if (TacticalHudWidget)
	{
		TacticalHudWidget->SetWorldMapTacticalVisible(bTacticalPresentation || bSearching);
		TacticalHudWidget->SetWorldMapSearchVisible(bSearching);
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

void URokWorldMapHudWidget::AddPowerGain(UCanvasPanel* Group)
{
	static const TSoftClassPtr<URokUiPageWidget> Class(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokHudPowerGain.WBP_RokHudPowerGain_C")));
	if (auto* Page = AddWorldHudPage(Group, Class, TEXT("HudPowerGainPage"), 51))
	{
		PowerGain = Page->Get<URokPowerGainWidget>(TEXT("PlayerPowerGain"));
		if (PowerGain)
		{
			Page->LegacyNames.Add(PowerGain->GetFName(), TEXT("WorldMapPowerGain"));
			return;
		}
		Page->RemoveFromParent();
	}
	PowerGain = WidgetTree->ConstructWidget<URokPowerGainWidget>(URokPowerGainWidget::StaticClass(), TEXT("WorldMapPowerGain"));
	PlaceOnCanvas(Group, PowerGain, FVector2D(199.65989f - 320, 31.47f - 180), FVector2D(640, 360))->SetZOrder(51);
}

void URokWorldMapHudWidget::BindHudLayout()
{
	ExpandedDesignCanvas = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapExpandedDesignCanvas"));
	ExpandedDesignSizeBox = WidgetTree->FindWidget<USizeBox>(TEXT("WorldMapExpandedDesignSize"));
	WorldMapChrome = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapChrome"));
	if (!ExpandedDesignCanvas || !ExpandedDesignSizeBox || !WorldMapChrome)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: required HUD layout containers are missing from %s"), *GetClass()->GetName());
		ExpandedDesignCanvas = nullptr;
		return;
	}

	PowerText = WidgetTree->FindWidget<UTextBlock>(TEXT("WorldMapPowerText"));
	VipText = WidgetTree->FindWidget<UTextBlock>(TEXT("WorldMapVipText"));
	CoordinateText = WidgetTree->FindWidget<UTextBlock>(TEXT("WorldMapCoordinateText"));
	if (UCanvasPanel* Identity = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapPlayerIdentity")))
	{
		AddPowerGain(Identity);
	}

	MiniMapRoot = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapMiniMap"));
	MiniMapViewportRoot = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapMiniMapViewport"));
	MiniMapSelfDot = WidgetTree->FindWidget<UBorder>(TEXT("WorldMapMiniMapSelfDot"));
	MiniMapViewportCanvasSlot = MiniMapViewportRoot ? Cast<UCanvasPanelSlot>(MiniMapViewportRoot->Slot) : nullptr;
	MiniMapSelfCanvasSlot = MiniMapSelfDot ? Cast<UCanvasPanelSlot>(MiniMapSelfDot->Slot) : nullptr;
	if (MiniMapRoot)
	{
		AddMiniMapInput(MiniMapRoot);
	}
	AdvanceMiniMapAnimation(0.0f);

	StrategicGlobeButton = WidgetTree->FindWidget<UButton>(TEXT("WorldMapGlobeButton"));
	StrategicFilterRoot = WidgetTree->FindWidget<UCanvasPanel>(TEXT("WorldMapRelationFilters"));
	AdvanceStrategicModuleAnimation(0.0f);
	FilterClickSound = LoadObject<USoundBase>(nullptr,
		TEXT("/Game/RokPrototype/UI/Audio/Sound_Ui_CommonSidePage.Sound_Ui_CommonSidePage"));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const auto Name = [Index](const TCHAR* Stem)
		{
			return FName(*FString::Printf(TEXT("WorldMapFilter%s_%d"), Stem, Index));
		};
		FilterBackgrounds.Add(WidgetTree->FindWidget<UImage>(Name(TEXT("Background"))));
		FilterButtons.Add(WidgetTree->FindWidget<UButton>(Name(TEXT("Hit"))));
		FilterLabels.Add(WidgetTree->FindWidget<UTextBlock>(Name(TEXT("Label"))));
		UImage* Dot = WidgetTree->FindWidget<UImage>(Name(TEXT("Dot")));
		FilterDots.Add(Dot);
		FilterTints.AddDefaulted();
		FilterStartAlphas.Add(Dot ? Dot->GetRenderOpacity() : 0.0f);
	}
	UpdateFilterLegend();

	const auto Bind = [this](const FName Name, const FName Function)
	{
		if (UButton* Button = WidgetTree->FindWidget<UButton>(Name))
		{
			FScriptDelegate Handler;
			Handler.BindUFunction(this, Function);
			Button->OnClicked.AddUnique(Handler);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Rok UI: %s has no HUD button named %s"), *GetClass()->GetName(), *Name.ToString());
		}
	};
	Bind(TEXT("WorldMapPlayerProfileHitButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenPlayerProfileClicked));
	Bind(TEXT("WorldMapVipHitButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenVipClicked));
	Bind(TEXT("WorldMapPlayerArrowHitButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenCityManagerClicked));
	Bind(TEXT("WorldMapCoordinateSearchButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenPositionSearchClicked));
	Bind(TEXT("WorldMapFavoriteButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenMapMarkersClicked));
	Bind(TEXT("WorldMapFilterHit_0"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleAllianceFilterClicked));
	Bind(TEXT("WorldMapFilterHit_1"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleExploreFilterClicked));
	Bind(TEXT("WorldMapFilterHit_2"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleResourceFilterClicked));
	Bind(TEXT("WorldMapFilterHit_3"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleBookmarkFilterClicked));
	Bind(TEXT("WorldMapChatButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenChatClicked));
	Bind(TEXT("WorldMapSearchButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenResourceSearchClicked));
	Bind(TEXT("WorldMapHomeCityButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleEnterPlayerCityClicked));
	Bind(TEXT("WorldMapGlobeButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenServerListClicked));
	Bind(TEXT("WorldMapMailButton"), GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleOpenMailClicked));
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
	AddPowerGain(Group);

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

	AddMiniMapInput(Group);
}

void URokWorldMapHudWidget::AddMiniMapInput(UCanvasPanel* Group)
{
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

URokUiPageWidget* URokWorldMapHudWidget::CreateWorldHudPage(
	const TSoftClassPtr<URokUiPageWidget>& Class, FName Name)
{
	UClass* PageClass = GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint)
		? URokUiSettings::Resolve(Class) : nullptr;
	if (!PageClass)
	{
		return nullptr;
	}
	auto* Page = WidgetTree->ConstructWidget<URokUiPageWidget>(PageClass, Name);
	Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UWidget* Root = Page->GetRootWidget())
	{
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Page;
}

URokUiPageWidget* URokWorldMapHudWidget::AddWorldHudPage(UCanvasPanel* Parent,
	const TSoftClassPtr<URokUiPageWidget>& Class, FName Name, int32 ZOrder)
{
	auto* Page = CreateWorldHudPage(Class, Name);
	if (!Page) return nullptr;
	auto* PageSlot = Parent->AddChildToCanvas(Page);
	PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PageSlot->SetOffsets(FMargin(0));
	PageSlot->SetZOrder(ZOrder);
	return Page;
}

void URokWorldMapHudWidget::BindWorldHudButton(const UUserWidget& Owner, FName Name, FName Function)
{
	if (auto* Button = Cast<UButton>(Owner.GetWidgetFromName(Name)))
	{
		FScriptDelegate Handler;
		Handler.BindUFunction(this, Function);
		Button->OnClicked.AddUnique(Handler);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: %s has no HUD button named %s"), *Owner.GetClass()->GetName(), *Name.ToString());
	}
}

void URokWorldMapHudWidget::BuildServerListOverlay()
{
	static const TSoftClassPtr<URokUiPageWidget> ServerPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldKingdoms.WBP_RokWorldKingdoms_C")));
	if (auto* Page = AddWorldHudPage(ExpandedDesignCanvas, ServerPage, TEXT("WorldMapKingdomPage"), 95))
	{
		ServerListOverlay = Page->Get<UCanvasPanel>(TEXT("WorldMapServerListOverlay"));
		ServerListStatusText = Page->Get<UTextBlock>(TEXT("WorldMapServerListStatus"));
		BindWorldHudButton(*Page, TEXT("WorldMapServerSelectUnavailableButton"),
			GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleServerSelectionUnavailableClicked));
		BindWorldHudButton(*Page, TEXT("WorldMapServerListCloseButton"),
			GET_FUNCTION_NAME_CHECKED(URokWorldMapHudWidget, HandleCloseServerListClicked));
		return;
	}

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
	static const TSoftClassPtr<URokUiPageWidget> PortraitClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldCityPortrait.WBP_RokWorldCityPortrait_C")));
	if (!AddWorldHudPage(SelfPortraitRoot, PortraitClass, TEXT("WorldCityPortraitPage")))
	{
		PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitPointer"),
			MapAtlasPath, SpriteRect(559, 184, 56, 71), MapAtlasSize, FVector2D(42, 53.25f)),
			FVector2D(79, 46.75f), FVector2D(42, 53.25f));
		PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitImage"),
			PlayerHeadAtlasPath, PlayerHead1000Rect, PlayerHeadAtlasSize, FVector2D(94.5f, 94.5f)),
			FVector2D(52.75f, -28.85f), FVector2D(94.5f, 94.5f));
		PlaceOnCanvas(SelfPortraitRoot, MakeAtlasImage(TEXT("WorldMapSelfPortraitFrame"),
			PlayerHeadAtlasPath, PlayerFrameRect, PlayerHeadAtlasSize, FVector2D(110.775f, 110.775f)),
			FVector2D(44.6125f, -36.9875f), FVector2D(110.775f, 110.775f));
	}
	// UI_Tip_Screen: exact ui_map atlas sprites, not the globe/menu icon.
	HomeIndicatorRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldMapHomeIndicator"));
	HomeIndicatorRoot->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ExpandedDesignCanvas, HomeIndicatorRoot,
		FVector2D::ZeroVector, FVector2D(100, 120))->SetZOrder(80);
	static const TSoftClassPtr<URokUiPageWidget> HomeIndicatorClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldHomeIndicator.WBP_RokWorldHomeIndicator_C")));
	if (auto* Page = AddWorldHudPage(HomeIndicatorRoot, HomeIndicatorClass, TEXT("WorldHomeIndicatorPage")))
	{
		auto* Arrow = Page->Get<UImage>(TEXT("WorldMapHomeDirection"));
		auto* Distance = Page->Get<UTextBlock>(TEXT("WorldMapHomeDistance"));
		auto* Button = Page->Get<UButton>(TEXT("WorldMapHomeIndicatorButton"));
		if (Arrow && Distance && Button)
		{
			HomeIndicatorArrow = Arrow;
			HomeIndicatorDistance = Distance;
			Button->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::FocusPlayerLocation);
		}
		else
		{
			Page->RemoveFromParent();
		}
	}
	if (!HomeIndicatorArrow)
	{
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
	}
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

	static const TSoftClassPtr<URokUiPageWidget> SelfMarkerClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldCityNameplate.WBP_RokWorldCityNameplate_C")));
	if (auto* Page = AddWorldHudPage(SelfMarkerRoot, SelfMarkerClass, TEXT("WorldCityNameplatePage")))
	{
		auto* CityName = Page->Get<UTextBlock>(TEXT("WorldMapSelfCityName"));
		auto* CityLevel = Page->Get<UTextBlock>(TEXT("WorldMapSelfCityLevel"));
		auto* Badge = Page->Get<UImage>(TEXT("WorldMapSelfCityLevelBadge"));
		if (CityName && CityLevel && Badge)
		{
			SelfCityNameText = CityName;
			if (LevelBadgeTexture)
			{
				FSlateBrush Brush = Badge->GetBrush();
				Brush.SetResourceObject(LevelBadgeTexture);
				Badge->SetBrush(Brush);
				SelfCityLevelText = CityLevel;
				SelfCityLevelText->SetText(FText::AsNumber(ReadInitialCityHallLevel()));
			}
			else
			{
				Badge->SetVisibility(ESlateVisibility::Collapsed);
				CityLevel->SetVisibility(ESlateVisibility::Collapsed);
			}
			return;
		}
		Page->RemoveFromParent();
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
	ReconcileEntityBadges();
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world entity badges built: count=%d"),
		EntityBadges.Num());
}

void URokWorldMapHudWidget::ReconcileEntityBadges()
{
	UWorld* World = GetWorld();
	if (!World || !ExpandedDesignCanvas || !LevelBadgeTexture)
	{
		return;
	}
	for (int32 Index = EntityBadges.Num() - 1; Index >= 0; --Index)
	{
		const FEntityBadgeEntry& Entry = EntityBadges[Index];
		const ARokWorldMapEntityActor* Entity = Entry.Entity.Get();
		if (!IsValid(Entity) || Entity->GetEntityLevel() <= 0
			|| Entry.ServerObjectId != Entity->GetServerObjectId())
		{
			if (Entry.Root)
			{
				Entry.Root->RemoveFromParent();
			}
			EntityBadges.RemoveAtSwap(Index);
		}
	}
	for (TActorIterator<ARokWorldMapEntityActor> EntityIterator(World);
		EntityIterator;
		++EntityIterator)
	{
		ARokWorldMapEntityActor* EntityActor = *EntityIterator;
		if (!EntityActor || EntityActor->GetEntityLevel() <= 0)
		{
			continue;
		}
		const FString ServerObjectId = EntityActor->GetServerObjectId();
		FEntityBadgeEntry* Existing = EntityBadges.FindByPredicate(
			[EntityActor, &ServerObjectId](const FEntityBadgeEntry& Entry)
			{
				return Entry.Entity.Get() == EntityActor
					&& Entry.ServerObjectId == ServerObjectId;
			});
		if (Existing)
		{
			if (Existing->LevelText)
			{
				Existing->LevelText->SetText(FText::AsNumber(
					EntityActor->GetEntityLevel()));
			}
			continue;
		}
		const uint64 BadgeId = ++NextEntityBadgeId;

		UCanvasPanel* BadgeRoot =
			WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				FName(*FString::Printf(TEXT("WorldMapEntityBadge_%llu"), BadgeId)));
		BadgeRoot->SetVisibility(ESlateVisibility::Collapsed);
		BadgeRoot->SetRenderTransformPivot(FVector2D::ZeroVector);

		FEntityBadgeEntry Entry;
		Entry.Entity = EntityActor;
		Entry.ServerObjectId = ServerObjectId;
		Entry.Root = BadgeRoot;
		BuildEntityBadgeLayout(Entry, BadgeId, IsResourceEntityKind(EntityActor->GetEntityKind()) && !ServerObjectId.IsEmpty());
		Entry.LevelText->SetText(FText::AsNumber(EntityActor->GetEntityLevel()));
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
}

void URokWorldMapHudWidget::BuildEntityBadgeLayout(FEntityBadgeEntry& Entry, uint64 BadgeId, bool bGatherable)
{
	static const TSoftClassPtr<URokUiPageWidget> BadgeClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldEntityBadge.WBP_RokWorldEntityBadge_C")));
	if (auto* Page = AddWorldHudPage(Entry.Root, BadgeClass, NAME_None))
	{
		auto* Badge = Page->Get<UImage>(TEXT("WorldMapEntityBadgeBg"));
		auto* Level = Page->Get<UTextBlock>(TEXT("WorldMapEntityBadgeLevel"));
		auto* Gathering = Page->Get<UImage>(TEXT("WorldMapGatheringIcon"));
		if (Badge && Level && Gathering)
		{
			Page->LegacySuffix = FString::Printf(TEXT("_%llu"), BadgeId);
			FSlateBrush Brush = Badge->GetBrush();
			Brush.SetResourceObject(LevelBadgeTexture);
			Badge->SetBrush(Brush);
			Entry.LevelText = Level;
			Gathering->SetVisibility(ESlateVisibility::Collapsed);
			Entry.GatheringIcon = bGatherable ? Gathering : nullptr;
			return;
		}
		Page->RemoveFromParent();
	}

	const auto Name = [BadgeId](const TCHAR* Stem) { return FName(*FString::Printf(TEXT("%s_%llu"), Stem, BadgeId)); };
	UImage* Badge = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("WorldMapEntityBadgeBg")));
	FSlateBrush Brush;
	Brush.SetResourceObject(LevelBadgeTexture);
	Brush.ImageSize = FVector2D(60, 33);
	Badge->SetBrush(Brush);
	Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Entry.Root, Badge, {2, 2}, {60, 33});

	Entry.LevelText = MakeHudText(Name(TEXT("WorldMapEntityBadgeLevel")), 18,
		FLinearColor(0.184f, 0.176f, 0.153f, 1.0f), true);
	Entry.LevelText->SetJustification(ETextJustify::Center);
	PlaceOnCanvas(Entry.Root, Entry.LevelText, {2, 7}, {60, 24});

	if (bGatherable)
	{
		// Unity MapTouchHandler / ui_map[img_map_state_collect1].
		Entry.GatheringIcon = MakeAtlasImage(Name(TEXT("WorldMapGatheringIcon")),
			MapAtlasPath, SpriteRect(449, 96, 46, 46), MapAtlasSize, FVector2D(54));
		Entry.GatheringIcon->SetVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(Entry.Root, Entry.GatheringIcon, {5, -144}, FVector2D(54));
	}
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
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const int64 RoleId = Net ? Net->GetRoleId() : 0;
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
		if (Entry.GatheringIcon)
			Entry.GatheringIcon->SetVisibility(RoleId > 0 && EntityActor->GetCollectorRoleId() == RoleId
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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

void URokWorldMapHudWidget::AddWorldPopupFrame(UCanvasPanel* Panel, const FVector2D& PanelSize, const TCHAR* Prefix)
{
	// UI_Pop_WorldObjectInfo/bg -> ui_common/img_com_PopBg (58x55),
	// Unity border L/B/R/T = 25/25/18/21. Slate Box uses the entire
	// texture's dimensions even with a UV region, so use atlas image patches
	// as in RokUnityFeatureScreenWidget rather than stretching atlas-sized corners.
	const int32 SourceX[] = {0, 25, 40, 58};
	const int32 SourceY[] = {55, 34, 25, 0}; // top down in Unity's bottom-origin atlas
	const float DestX[] = {0, 25, static_cast<float>(PanelSize.X) - 18, static_cast<float>(PanelSize.X)};
	const float DestY[] = {0, 21, static_cast<float>(PanelSize.Y) - 25, static_cast<float>(PanelSize.Y)};
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			const FVector2D Size(DestX[Col + 1] - DestX[Col], DestY[Row + 1] - DestY[Row]);
			PlaceOnCanvas(Panel, MakeAtlasImage(
				*FString::Printf(TEXT("%s_%d_%d"), Prefix, Row, Col), CommonAtlasPath,
				SpriteRect(WorldObjectPopBackgroundRect.Min.X + SourceX[Col],
					WorldObjectPopBackgroundRect.Min.Y + SourceY[Row + 1],
					SourceX[Col + 1] - SourceX[Col], SourceY[Row] - SourceY[Row + 1]),
				CommonAtlasSize, Size), FVector2D(DestX[Col], DestY[Row]), Size);
		}
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
		WorldObjectInfoPanelSize);
	if (WorldObjectInfoCanvasSlot.IsValid())
	{
		WorldObjectInfoCanvasSlot->SetZOrder(90);
	}

	static const TSoftClassPtr<URokUiPageWidget> ResourceInfoPage(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldObjectInfo.WBP_RokWorldObjectInfo_C")));
	if (auto* Page = AddWorldHudPage(WorldObjectInfoRoot, ResourceInfoPage, TEXT("WorldObjectInfoPage")))
	{
		BindResourceInfoLayout(*Page);
	}
	else
	{
		BuildResourceInfoLayout();
	}

#if WITH_EDITOR
	if (URokUiSettings::IsLegacyForced())
	{
		FString Filename;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldObjectHud="), Filename))
		{
			FString Error;
			if (RokUiSpec::ExportChildren(WorldObjectInfoRoot, Filename, TEXT("world_object_hud"), Error))
			{
				UE_LOG(LogTemp, Display, TEXT("Rok UI: world-object HUD spec saved to %s"), *Filename);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Rok UI: world-object HUD spec export failed: %s"), *Error);
			}
		}
	}
#endif

	BuildBarbarianInfo();
	static const TSoftClassPtr<URokUiPageWidget> ArrowClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Hud/WBP_RokWorldObjectInfoArrow.WBP_RokWorldObjectInfoArrow_C")));
	if (auto* Page = AddWorldHudPage(WorldObjectInfoRoot, ArrowClass, TEXT("WorldObjectInfoArrowPage"), 1))
	{
		auto* Arrow = Page->Get<UImage>(TEXT("WorldObjectInfoArrow"));
		auto* ArrowSlot = Arrow ? Cast<UCanvasPanelSlot>(Arrow->Slot) : nullptr;
		if (Arrow && ArrowSlot)
		{
			WorldObjectInfoArrow = Arrow;
			WorldObjectInfoArrowCanvasSlot = ArrowSlot;
			WorldObjectInfoArrowBaseAngle = Arrow->GetRenderTransformAngle();
		}
		else
		{
			Page->RemoveFromParent();
		}
	}
	if (!WorldObjectInfoArrow)
	{
		WorldObjectInfoArrow = MakeAtlasImage(TEXT("WorldObjectInfoArrow"),
			CommonAtlasPath, WorldObjectPopArrowRect, CommonAtlasSize, {12, 15});
		auto* ArrowSlot = PlaceOnCanvas(WorldObjectInfoRoot, WorldObjectInfoArrow, FVector2D::ZeroVector, {12, 15});
		ArrowSlot->SetZOrder(1);
		WorldObjectInfoArrowCanvasSlot = ArrowSlot;
	}
#if WITH_EDITOR
	FString ArrowSpec;
	if (URokUiSettings::IsLegacyForced()
		&& FParse::Value(FCommandLine::Get(), TEXT("RokUiExportWorldObjectInfoArrow="), ArrowSpec))
	{
		FString Error;
		if (!RokUiSpec::ExportChildren(WorldObjectInfoRoot, ArrowSpec, TEXT("world_object_info_arrow_source"), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Rok UI: world object arrow export failed: %s"), *Error);
		}
	}
#endif
}

void URokWorldMapHudWidget::BuildResourceInfoLayout()
{
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("WorldObjectInfoPanel"));
	Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WorldResourceInfoPanel = Panel;
	AddWorldPopupFrame(Panel, WorldObjectInfoPanelSize, TEXT("WorldObjectInfoPatch"));
	PlaceOnCanvas(WorldObjectInfoRoot, Panel, FVector2D::ZeroVector, WorldObjectInfoPanelSize);

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

	// UI_Pop_WorldObjectInfo and UI_Item_WorldObjInfoTCollect.
	const FLinearColor Ink(FColor(35, 35, 32));
	auto AddText = [&](FName Name, int32 FontSize, const FVector2D& Position, const FVector2D& Size,
		ETextJustify::Type Justification, bool bBold, FLinearColor Color)
	{
		auto* Text = MakeHudText(Name, FontSize, Color, bBold);
		Text->SetFont(URokMainHudWidget::MakeUnitySourceFont(FontSize, bBold));
		Text->SetShadowOffset(FVector2D::ZeroVector);
		Text->SetJustification(Justification);
		PlaceOnCanvas(PanelContent, Text, Position, Size);
		return Text;
	};

	WorldObjectInfoCoordinateText = AddText(TEXT("WorldObjectInfoCoordinate"), 20,
		FVector2D(118, 11), FVector2D(243, 28), ETextJustify::Center, false,
		FLinearColor(FColor(57, 56, 53)));
	WorldObjectInfoNameText = AddText(TEXT("WorldObjectInfoName"), 24,
		FVector2D(88, 44), FVector2D(303, 31), ETextJustify::Center, true, Ink);

	PlaceOnCanvas(PanelContent, MakeAtlasImage(TEXT("WorldObjectResourceStatsBackground"), CommonAtlasPath,
		SpriteRect(436, 165, 63, 55), CommonAtlasSize, FVector2D(383, 100)), FVector2D(73, 80), FVector2D(383, 100));
	PlaceOnCanvas(PanelContent, MakeAtlasImage(TEXT("WorldObjectResourceFrame"), ResourceIconAtlasPath,
		SpriteRect(729, 1878, 168, 169), ResourceIconAtlasSize, FVector2D(120)), FVector2D(5, 68), FVector2D(120));
	WorldObjectInfoIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WorldObjectInfoIcon"));
	WorldObjectInfoIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(PanelContent, WorldObjectInfoIcon, FVector2D(15, 78), FVector2D(100));

	const FText Labels[] = {
		NSLOCTEXT("RokWorldMapHud", "ResourceReserve", "储量"),
		NSLOCTEXT("RokWorldMapHud", "ResourceCollector", "采集者"),
		NSLOCTEXT("RokWorldMapHud", "ResourceAlliance", "所属联盟")};
	const FName ValueNames[] = {TEXT("WorldObjectInfoAmount"), TEXT("WorldObjectInfoCollector"), TEXT("WorldObjectInfoAlliance")};
	TObjectPtr<UTextBlock>* Values[] = {&WorldObjectInfoAmountText, &WorldObjectInfoCollectorText, &WorldObjectInfoAllianceText};
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(Labels); ++Row)
	{
		const float Y = 85.0f + 30.0f * Row;
		AddText(*FString::Printf(TEXT("WorldObjectResourceLabel_%d"), Row), 20,
			FVector2D(130, Y), FVector2D(110, 29), ETextJustify::Left, true, Ink)->SetText(Labels[Row]);
		*Values[Row] = AddText(ValueNames[Row], 20, FVector2D(244, Y), FVector2D(188, 29),
			ETextJustify::Right, true, Ink);
		(*Values[Row])->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	}

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
		FVector2D(12, 17),
		FVector2D(60));

	auto* Bookmark = MakeAtlasButton(TEXT("WorldObjectBookmarkButton"), MapAtlasPath,
		SpriteRect(617, 188, 54, 67), MapAtlasSize, FVector2D(54, 67));
	Bookmark->SetToolTipText(NSLOCTEXT("RokWorldMapHud", "ResourceBookmark", "地图标记"));
	Bookmark->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectBookmarkClicked);
	PlaceOnCanvas(PanelContent, Bookmark, FVector2D(365, -5), FVector2D(54, 67));
	auto* Share = MakeAtlasButton(TEXT("WorldObjectShareButton"), MapAtlasPath,
		SpriteRect(729, 198, 54, 67), MapAtlasSize, FVector2D(54, 67));
	Share->SetToolTipText(NSLOCTEXT("RokWorldMapHud", "ResourceShare", "复制坐标"));
	Share->OnClicked.AddDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectShareClicked);
	PlaceOnCanvas(PanelContent, Share, FVector2D(419, -5), FVector2D(54, 67));
	PlaceOnCanvas(PanelContent, MakeAtlasImage(TEXT("WorldObjectShareIcon"), MapAtlasPath,
		SpriteRect(481, 7, 32, 32), MapAtlasSize, FVector2D(32)), FVector2D(430, 6), FVector2D(32));

	BuildWorldGatherProgress(PanelContent);

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
		FLinearColor(FColor(10, 27, 46)),
		true);
	GatherText->SetFont(URokMainHudWidget::MakeUnitySourceFont(32, true));
	GatherText->SetShadowOffset(FVector2D::ZeroVector);
	GatherText->SetText(
		NSLOCTEXT("RokWorldMapHud", "WorldObjectGather", "采集"));
	GatherText->SetJustification(ETextJustify::Center);
	GatherButton->SetContent(GatherText);
	WorldGatherActionButton = GatherButton;
	WorldGatherActionText = GatherText;
	PlaceOnCanvas(
		PanelContent,
		GatherButton,
		FVector2D(138.5f, 237.0f),
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
	PlaceOnCanvas(WorldObjectInfoDetailsRoot, Back, FVector2D(403.5f, 5), FVector2D(60));
	auto* DescriptionScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("WorldObjectDescriptionScroll"));
	PlaceOnCanvas(WorldObjectInfoDetailsRoot, DescriptionScroll, FVector2D(22, 58), FVector2D(435, 272));
	// UI_Pop_WorldObjectInfo/lbl_desc: 20pt regular, rich text enabled.
	// Preserve Language.cn <b> spans rather than stripping emphasis.
	auto* DetailsText = WidgetTree->ConstructWidget<URichTextBlock>(
		URichTextBlock::StaticClass(), TEXT("WorldObjectInfoDetailsText"));
	DetailsText->SetVisibility(ESlateVisibility::HitTestInvisible);
	WorldObjectDescriptionText = DetailsText;
	ApplyResourceDescriptionStyles();
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
		FVector2D(18.0f, 324.0f),
		FVector2D(443.0f, 28.0f));
}

void URokWorldMapHudWidget::ApplyResourceDescriptionStyles()
{
	if (!WorldObjectDescriptionText || WorldObjectDescriptionText->GetTextStyleSet())
	{
		return;
	}
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
	WorldObjectDescriptionText->SetTextStyleSet(Styles);
}

void URokWorldMapHudWidget::BindResourceInfoLayout(URokUiPageWidget& Page)
{
	WorldResourceInfoPanel = Page.Get<UCanvasPanel>(TEXT("WorldObjectInfoPanel"));
	WorldObjectNormalRoot = Page.Get<UCanvasPanel>(TEXT("WorldObjectInfoPanelContent"));
	WorldObjectInfoDetailsRoot = Page.Get<UCanvasPanel>(TEXT("WorldObjectInfoDetailsRoot"));
	WorldObjectInfoCoordinateText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoCoordinate"));
	WorldObjectInfoNameText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoName"));
	WorldObjectInfoIcon = Page.Get<UImage>(TEXT("WorldObjectInfoIcon"));
	WorldObjectInfoAmountText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoAmount"));
	WorldObjectInfoCollectorText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoCollector"));
	WorldObjectInfoAllianceText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoAlliance"));
	WorldObjectInfoFeedbackText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoFeedback"));
	WorldObjectDescriptionText = Page.Get<URichTextBlock>(TEXT("WorldObjectInfoDetailsText"));
	ApplyResourceDescriptionStyles();
	WorldGatherActionButton = Page.Get<UButton>(TEXT("WorldObjectInfoGatherButton"));
	WorldGatherActionText = Page.Get<UTextBlock>(TEXT("WorldObjectInfoGatherButtonText"));
	WorldGatherProgressRoot = Page.Get<UCanvasPanel>(TEXT("WorldGatherProgress"));
	WorldGatherProgressBar = Page.Get<UProgressBar>(TEXT("WorldGatherProgressBar"));
	WorldGatherProgressIcon = Page.Get<UImage>(TEXT("WorldGatherProgressIcon"));
	WorldGatherProgressText = Page.Get<UTextBlock>(TEXT("WorldGatherProgressAmount"));

	if (auto* Button = Page.Get<UButton>(TEXT("WorldObjectInfoDetailsButton")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectDetailsClicked);
	}
	if (auto* Button = Page.Get<UButton>(TEXT("WorldObjectDescriptionBack")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectDetailsClicked);
	}
	if (auto* Button = Page.Get<UButton>(TEXT("WorldObjectBookmarkButton")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectBookmarkClicked);
	}
	if (auto* Button = Page.Get<UButton>(TEXT("WorldObjectShareButton")))
	{
		Button->OnClicked.AddUniqueDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectShareClicked);
	}
	if (WorldGatherActionButton)
	{
		WorldGatherActionButton->OnClicked.AddUniqueDynamic(this, &URokWorldMapHudWidget::HandleWorldObjectGatherClicked);
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
	if (!ResolveProjection(TitleProjection))
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// GlobalViewLevelMediator positions tactical titles at the map object's
	// ground coordinates. The art's pivot and LOD do not move that anchor.
	const FVector TitleAnchor = bPortraitBand
		? PlayerCity->GetVisualCenterWorldPosition() : PlayerCity->GetActorLocation();
	const float TitleOffsetCm =
		EvaluateCityTitleOffset(MarkerCamera->GetCurrentDxf())
		* TitleProjection.GetSafeUeScale();
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController,
			TitleAnchor + FVector(0,
				TitleOffsetCm,
				bPortraitBand ? TitleOffsetCm : 0),
			TitlePosition, true))
	{
		SelfMarkerRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// MapElementUI.WorldToUIPos: below 3000 CastleTitle offsets Unity z
	// only (UE Y). At/above 3000 it offsets both Unity z and y (UE Y/Z).
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
	// ChangeWinPos places the card toward the viewport centre, beside a point
	// in the middle band and above/below points close to a vertical edge.
	const FVector2D PanelSize = WorldObjectInfoCanvasSlot->GetSize();
	const FVector2D Half = PanelSize * 0.5f;
	const bool bPlaceAbove = DesignPosition.Y > CurrentExpandedDesignSize.Y - Half.Y;
	const bool bPlaceBelow = DesignPosition.Y < Half.Y;
	const bool bOnLeft = DesignPosition.X < CurrentExpandedDesignSize.X * 0.5f - 1.0f;
	FVector2D DesiredTopLeft = DesignPosition - Half;
	if (bPlaceAbove || bPlaceBelow)
	{
		DesiredTopLeft.Y = bPlaceAbove ? DesignPosition.Y - PanelSize.Y - 50.0f : DesignPosition.Y + 50.0f;
	}
	else
	{
		DesiredTopLeft.X = bOnLeft ? DesignPosition.X + 50.0f : DesignPosition.X - PanelSize.X - 50.0f;
	}
	const FVector2D ClampedTopLeft = ClampMarkerTopLeft(DesiredTopLeft, PanelSize,
		CurrentExpandedDesignSize, 12.0f);
	WorldObjectInfoCanvasSlot->SetPosition(ClampedTopLeft);

	if (WorldObjectInfoArrowCanvasSlot.IsValid() && WorldObjectInfoArrow)
	{
		const FVector2D LocalPoint = DesignPosition - ClampedTopLeft;
		const bool bVertical = bPlaceAbove || bPlaceBelow;
		const FVector2D ArrowCenter = bVertical
			? FVector2D(FMath::Clamp(LocalPoint.X, 18.0, PanelSize.X - 18.0), bPlaceAbove ? PanelSize.Y + 5.0 : -5.0)
			: FVector2D(bOnLeft ? -5.0 : PanelSize.X + 5.0, FMath::Clamp(LocalPoint.Y, 18.0, PanelSize.Y - 18.0));
		const FVector2D ArrowSize = WorldObjectInfoArrowCanvasSlot->GetSize();
		WorldObjectInfoArrowCanvasSlot->SetPosition(ArrowCenter - ArrowSize * 0.5);
		const float DirectionAngle = bVertical ? (bPlaceAbove ? 0.0f : 180.0f) : (bOnLeft ? 90.0f : -90.0f);
		WorldObjectInfoArrow->SetRenderTransformAngle(WorldObjectInfoArrowBaseAngle + DirectionAngle);
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
	FRokPlayerTopBar ServerTopBar;
	bool bServerTopBar = false;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		const URokGameNetSubsystem* Net =
			GameInstance->GetSubsystem<URokGameNetSubsystem>();
		if (Net && Net->GetState() == ERokGameNetState::RoleReady)
		{
			FString Error;
			bServerTopBar = RokPlayerTopBar::FromRoleInfo(
				Net->GetRoleInfo(), ServerTopBar, Error);
		}
	}
	if (bServerTopBar)
	{
		bDisplayingServerTopBar = true;
		SetPowerPresentation(static_cast<int32>(FMath::Clamp<int64>(
			ServerTopBar.Power, 0, MAX_int32)));
		if (PowerText)
		{
			PowerText->SetToolTipText(FText::GetEmpty());
		}
		if (VipText)
		{
			VipText->SetText(FText::FromString(ServerTopBar.VipText));
		}
		if (SelfCityNameText)
		{
			SelfCityNameText->SetText(FText::FromString(ServerTopBar.Name));
		}
		if (SelfCityLevelText)
		{
			SelfCityLevelText->SetText(FText::AsNumber(FMath::Max(1, ServerTopBar.Level)));
		}
	}
	else if (bDisplayingServerTopBar)
	{
		bDisplayingServerTopBar = false;
		if (SelfCityNameText)
		{
			SelfCityNameText->SetText(NSLOCTEXT("RokWorldMapHud", "SelfCityName", "主城"));
		}
		if (SelfCityLevelText)
		{
			SelfCityLevelText->SetText(FText::AsNumber(ReadInitialCityHallLevel()));
		}
	}
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

	if (!bServerTopBar && (PowerText || VipText))
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

void URokWorldMapHudWidget::ShowWorldObjectInfo(AActor* Actor)
{
	PendingWorldTargetId.Reset();
	if (WorldSearchResultGuide) WorldSearchResultGuide->RemoveFromParent();
	ARokWorldMapEntityActor* EntityActor =
		Cast<ARokWorldMapEntityActor>(Actor);
	const bool bBarbarian = EntityActor && EntityActor->GetEntityKind() == TEXT("barbarian");
	if (!EntityActor || (!bBarbarian && !IsResourceEntityKind(EntityActor->GetEntityKind())))
	{
		HideWorldObjectInfo();
		return;
	}

	SelectedWorldObjectActor = EntityActor;
	SelectedWorldObjectServerId = EntityActor->GetServerObjectId();
	if (WorldResourceInfoPanel) WorldResourceInfoPanel->SetVisibility(bBarbarian
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (WorldBarbarianInfoPanel) WorldBarbarianInfoPanel->SetVisibility(bBarbarian
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (WorldObjectInfoCanvasSlot.IsValid())
	{
		const UCanvasPanel* Panel = bBarbarian ? WorldBarbarianInfoPanel.Get() : WorldResourceInfoPanel.Get();
		const auto* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
		FVector2D PanelSize = bBarbarian ? WorldBarbarianPanelSize : WorldObjectInfoPanelSize;
		if (PanelSlot && PanelSlot->GetAnchors().Minimum == PanelSlot->GetAnchors().Maximum)
		{
			const FVector2D AuthoredSize = PanelSlot->GetSize();
			if (AuthoredSize.X > 0 && AuthoredSize.Y > 0)
			{
				PanelSize = AuthoredSize;
			}
		}
		WorldObjectInfoCanvasSlot->SetSize(PanelSize);
	}
	if (bBarbarian)
	{
		const auto* Army = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerArmySubsystem>() : nullptr;
		FRokBarbarianAttackInfo Info;
		if (BarbarianStay) BarbarianStay->SetIsChecked(Army
			&& Army->GetBarbarianAttackInfo(FCString::Atoi64(*SelectedWorldObjectServerId), Info) && Info.bSituStation);
		SetBarbarianDetailsExpanded(false);
		BarbarianDisplayedRewardGroup = INDEX_NONE;
		WorldObjectAnimation.Open(ERokUiWindowAnimationStyle::WorldObject);
		AdvanceWorldObjectAnimation(0.0f);
		WorldObjectInfoRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RefreshBarbarianInfo(*EntityActor);
		UpdateWorldObjectInfoProjection();
		return;
	}
	if (WorldObjectDescriptionText)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Descriptions = nullptr;
		const TSharedPtr<FJsonObject>* Description = nullptr;
		FString Text;
		if (FFileHelper::LoadFileToString(Json, *(RokDataPaths::Resolve(TEXT("world_resource_descriptions.json"))))
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
	if (WorldObjectInfoIcon)
	{
		const FString& Kind = EntityActor->GetEntityKind();
		const FIntRect Rect = Kind.Equals(TEXT("food"), ESearchCase::IgnoreCase) ? SpriteRect(865, 349, 80, 80)
			: Kind.Equals(TEXT("wood"), ESearchCase::IgnoreCase) ? SpriteRect(1085, 489, 80, 80)
			: Kind.Equals(TEXT("stone"), ESearchCase::IgnoreCase) ? SpriteRect(947, 431, 80, 80)
			: SpriteRect(865, 431, 80, 80);
		WorldObjectInfoIcon->SetBrush(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), Rect, CommonAtlasSize, FVector2D(100)));
		WorldGatherProgressIcon->SetBrush(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), Rect, CommonAtlasSize, FVector2D(60)));
	}
	RefreshSelectedWorldObjectValues();
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

void URokWorldMapHudWidget::RefreshSelectedWorldObjectValues()
{
	ARokWorldMapEntityActor* EntityActor = SelectedWorldObjectActor.Get();
	if (!IsValid(EntityActor)
		|| (EntityActor && EntityActor->GetServerObjectId() != SelectedWorldObjectServerId))
	{
		if (IsWorldObjectInfoOpen())
		{
			HideWorldObjectInfo();
		}
		return;
	}
	if (EntityActor->GetEntityKind() == TEXT("barbarian"))
	{
		RefreshBarbarianInfo(*EntityActor);
		return;
	}
	const FText None = NSLOCTEXT("RokWorldMapHud", "NoResourceOwner", "无");
	const FText Unknown = NSLOCTEXT("RokWorldMapHud", "UnknownResourceValue", "—");
	const auto* Instance = GetGameInstance();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const FRokGatherPointState Gathering = Army ? Army->GetGatherPointState(SelectedWorldObjectServerId) : FRokGatherPointState();
	if (WorldGatherActionButton)
	{
		const bool bOccupied = EntityActor->GetCollectorRoleId() > 0;
		const bool bRecallPending = SelectedRecallTargetId > 0 && LexToString(SelectedRecallTargetId) == SelectedWorldObjectServerId;
		WorldGatherActionButton->SetVisibility(!bOccupied || Gathering.bOwned
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		WorldGatherActionButton->SetIsEnabled(!bRecallPending && (!Gathering.bOwned || Gathering.bCanRecall));
		WorldGatherActionText->SetText(Gathering.bOwned
			? NSLOCTEXT("RokGather", "Recall", "召回")
			: NSLOCTEXT("RokWorldMapHud", "WorldObjectGather", "采集"));
	}
	if (WorldGatherProgressRoot)
	{
		WorldGatherProgressRoot->SetVisibility(Gathering.bOwned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Gathering.bOwned)
		{
			WorldGatherProgressBar->SetPercent(Gathering.Goal > 0 && Gathering.Gathered >= 0
				? static_cast<float>(static_cast<double>(Gathering.Gathered) / Gathering.Goal) : 0.0f);
			FText Goal = Gathering.Goal >= 0 ? FText::AsNumber(Gathering.Goal) : Unknown;
			if (Gathering.bGoalApproximate)
				Goal = FText::Format(NSLOCTEXT("RokGather", "ApproximateGoal", "约{0}"), Goal);
			WorldGatherProgressText->SetText(FText::Format(NSLOCTEXT("RokGather", "Progress", "采集量:{0}/{1}"),
				Gathering.Gathered >= 0 ? FText::AsNumber(Gathering.Gathered) : Unknown, Goal));
		}
	}
	if (WorldObjectInfoAmountText)
	{
		const int64 Amount = Gathering.Remaining >= 0 ? Gathering.Remaining
			: (EntityActor->GetServerResourceAmount() > 0 ? EntityActor->GetServerResourceAmount() : EntityActor->GetResourceCapacity());
		WorldObjectInfoAmountText->SetText(Amount >= 0 ? FText::AsNumber(Amount) : Unknown);
	}
	if (WorldObjectInfoCollectorText)
	{
		const FString& Collector = EntityActor->GetCollectorName();
		WorldObjectInfoCollectorText->SetText(EntityActor->GetCollectorRoleId() <= 0 ? None
			: (Collector.IsEmpty() ? Unknown : FText::FromString(Collector)));
	}
	if (WorldObjectInfoAllianceText)
	{
		const FString& Alliance = EntityActor->GetResourceAlliance();
		WorldObjectInfoAllianceText->SetText(Alliance.IsEmpty() ? None
			: FText::FromString(FString::Printf(TEXT("[%s]"), *Alliance)));
	}
	if (WorldObjectInfoCoordinateText)
	{
		FVector2D DisplayPosition = EntityActor->GetUnityPosition();
		FRokWorldMapProjection Projection;
		if (ResolveProjection(Projection))
		{
			DisplayPosition = Projection.UnityToDisplay(DisplayPosition);
		}
		WorldObjectInfoCoordinateText->SetText(FText::FromString(FString::Printf(
			TEXT("X:%d,Y:%d"), FMath::FloorToInt(DisplayPosition.X), FMath::FloorToInt(DisplayPosition.Y))));
	}
}

void URokWorldMapHudWidget::HideWorldObjectInfo()
{
	SelectedWorldObjectActor.Reset();
	SelectedWorldObjectServerId.Reset();
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
	if (auto* Scroll = Cast<UScrollBox>(RokUiFind::Find(this, TEXT("WorldObjectDescriptionScroll")))) Scroll->ScrollToStart();
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
	PlayResourceClickSound();
	ARokWorldMapEntityActor* EntityActor = SelectedWorldObjectActor.Get();
	int64 TargetId = 0;
	if (!EntityActor || !LexTryParseString(TargetId, *SelectedWorldObjectServerId) || TargetId <= 0)
	{
		if (WorldObjectInfoFeedbackText)
		{
			WorldObjectInfoFeedbackText->SetText(NSLOCTEXT(
				"RokWorldMapHud", "GatherNeedsLiveResource", "请选择服务器上的资源点"));
		}
		return;
	}
	auto* Instance = GetGameInstance();
	auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	if (Army && Army->GetGatherPointState(SelectedWorldObjectServerId).bOwned)
	{
		if (SelectedRecallTargetId == TargetId) return;
		FString Error;
		const auto OnResult = [WeakThis = TWeakObjectPtr<URokWorldMapHudWidget>(this), TargetId](const FString& Failure)
		{
			auto* Hud = WeakThis.Get();
			if (!Hud) return;
			Hud->SelectedRecallTargetId = 0;
			if (Hud->SelectedWorldObjectServerId != LexToString(TargetId)) return;
			if (Failure.IsEmpty()) Hud->ClearWorldObjectSelection();
			else if (Hud->WorldObjectInfoFeedbackText) Hud->WorldObjectInfoFeedbackText->SetText(FText::FromString(Failure));
			Hud->RefreshSelectedWorldObjectValues();
		};
		if (Army->RequestGatherRecall(SelectedWorldObjectServerId, OnResult, Error))
		{
			SelectedRecallTargetId = TargetId;
			WorldObjectInfoFeedbackText->SetText(NSLOCTEXT("RokGather", "RecallPending", "正在召回…"));
			RefreshSelectedWorldObjectValues();
		}
		else if (WorldObjectInfoFeedbackText)
		{
			WorldObjectInfoFeedbackText->SetText(FText::FromString(Error));
		}
		return;
	}
	if (EntityActor->GetCollectorRoleId() <= 0) OpenGatherPanel(EntityActor, TargetId);
}

void URokWorldMapHudWidget::HandleWorldObjectBookmarkClicked()
{
	PlayResourceClickSound();
	ClearWorldObjectSelection();
	HandleOpenMapMarkersClicked();
}

void URokWorldMapHudWidget::HandleWorldObjectShareClicked()
{
	const auto* Target = SelectedWorldObjectActor.Get();
	if (!Target) return;
	const bool bBarbarian = Target->GetEntityKind() == TEXT("barbarian");
	const auto* Coordinate = bBarbarian ? BarbarianCoordinate.Get() : WorldObjectInfoCoordinateText.Get();
	if (!Coordinate) return;
	FPlatformApplicationMisc::ClipboardCopy(*Coordinate->GetText().ToString());
	if (!bBarbarian && WorldObjectInfoFeedbackText)
	{
		WorldObjectInfoFeedbackText->SetText(NSLOCTEXT("RokWorldMapHud", "ResourceCoordinateCopied", "已复制坐标"));
	}
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
		if (!Button || !FilterBackgrounds.IsValidIndex(Index) || !FilterBackgrounds[Index] || !FilterTints.IsValidIndex(Index)) continue;
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
		RokDataPaths::Resolve(TEXT("unity_initial_city_state.json")));
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
