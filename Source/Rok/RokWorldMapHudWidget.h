#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokWorldMapTypes.h"
#include "RokWorldMapFilterTint.h"
#include "RokUiWindowAnimation.h"
#include "RokPowerCounterAnimation.h"
#include "Styling/SlateBrush.h"
#include "RokWorldMapHudWidget.generated.h"

class ARokWorldMapCityActor;
class ARokStrategicResourceLayer;
class ARokWorldMapEntityActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class USizeBox;
class UTextBlock;
class URichTextBlock;
class UTexture2D;
class USoundBase;
class UWidget;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;

UENUM(BlueprintType)
enum class ERokWorldMapHudFilter : uint8
{
	None,
	Alliance,
	Explore,
	Resources,
	Markers,
};

class URokWorldMapHudWidget;

/**
 * Full-viewport pointer surface below every HUD control.  Standalone
 * GameAndUI focus can otherwise deliver Slate button clicks while never
 * forwarding an empty-world left click to the controller InputComponent.
 */
UCLASS(NotBlueprintable)
class ROK_API URokWorldMapSelectionInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningHud(URokWorldMapHudWidget* InOwningHud);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	TWeakObjectPtr<URokWorldMapHudWidget> OwningHud;
};

/**
 * Transparent input surface over the Unity-derived minimap.  Unity moves the
 * world camera on pointer down, drag, and pointer up, so a plain UButton is not
 * sufficient here.
 */
UCLASS(NotBlueprintable)
class ROK_API URokWorldMapMiniMapInputWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningHud(URokWorldMapHudWidget* InOwningHud);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	TWeakObjectPtr<URokWorldMapHudWidget> OwningHud;
	bool bDragging = false;

	void FocusAtPointer(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent);
};

UCLASS()
class ROK_API URokWorldMapHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static FVector2D ComputeMiniMapPoint(
		const FRokWorldMapProjection& Projection,
		const FVector2D& UnityPosition,
		const FVector2D& MiniMapSize);

	static FVector2D ClampMarkerTopLeft(
		const FVector2D& DesiredTopLeft,
		const FVector2D& MarkerSize,
		const FVector2D& DesignSize,
		float Margin);

	static FVector2D ComputeUnityPositionFromMiniMapPoint(
		const FRokWorldMapProjection& Projection,
		const FVector2D& MiniMapPoint,
		const FVector2D& MiniMapSize);

	static bool ShouldShowEntityForFilter(
		const FString& EntityKind,
		ERokWorldMapHudFilter Filter);

	static bool IsResourceEntityKind(const FString& EntityKind);
	static float EvaluateBarbarianBadgeOffset(float Dxf);
	static float EvaluateResourceBadgeOffset(float Dxf);
	static float EvaluateCityPortraitOffset(float Dxf);
	static float EvaluateCityTitleOffset(float Dxf);
	static bool ShouldShowCityPortrait(float Dxf);
	static bool ShouldShowCityTitle(float Dxf);
	static float EvaluateWorldObjectBadgeScale(
		float Dxf,
		float ViewportTopFraction,
		float VerticalFov);

	UFUNCTION(BlueprintCallable, Category = "Rok|World Map|HUD")
	void FocusPlayerLocation();

	void OpenCommandLineModal();
	void UpdateZoomPresentation(float Dxf);
	void AdvanceMiniMapAnimation(float DeltaSeconds);
	void AdvanceStrategicModuleAnimation(float DeltaSeconds);
	void SetPowerPresentation(int32 Value);
	void AdvancePowerPresentation(float DeltaSeconds);
	void AdvanceFilterSelectionAnimation(float DeltaSeconds);
	void AdvanceFilterInputFeedback(float DeltaSeconds);

	void FocusMiniMapLocalPoint(
		const FVector2D& LocalPoint,
		const FVector2D& LocalSize);

	ERokWorldMapHudFilter GetSelectedFilter() const
	{
		return SelectedFilter;
	}

	bool IsServerListOpen() const;

	void ShowWorldObjectInfo(AActor* Actor);
	void HideWorldObjectInfo();
	bool IsWorldObjectInfoOpen() const;
	void AdvanceWorldObjectAnimation(float DeltaSeconds);
	void AdvanceWorldObjectPageAnimation(float DeltaSeconds);
#if WITH_DEV_AUTOMATION_TESTS
	int32 GetResourceClickSoundRequestsForTesting() const { return ResourceClickSoundRequests; }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime) override;

private:
	void PlayResourceClickSound();
	FRokPowerCounterAnimation PowerCounter;
	UPROPERTY(Transient)
	TObjectPtr<class URokPowerGainWidget> PowerGain;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> ResourceClickSound;
#if WITH_DEV_AUTOMATION_TESTS
	int32 ResourceClickSoundRequests = 0;
#endif
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MiniMapRoot;
	bool bMiniMapTargetVisible = false;
	float MiniMapAnimationSeconds = 0.25f;
	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedDesignSizeBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ExpandedDesignCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> PowerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> VipText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CoordinateText;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> SelfMarkerRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> SelfPortraitRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> HomeIndicatorRoot;

	UPROPERTY()
	TObjectPtr<UImage> HomeIndicatorArrow;

	UPROPERTY()
	TObjectPtr<UTextBlock> HomeIndicatorDistance;

	UPROPERTY()
	TObjectPtr<UTextBlock> SelfCityNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SelfCityLevelText;

	// Unity UI_Tip_WorldObjectRssLevel badges that float above world
	// entities (barbarian/resource level plates).
	struct FEntityBadgeEntry
	{
		TWeakObjectPtr<ARokWorldMapEntityActor> Entity;
		TObjectPtr<UCanvasPanel> Root;
		TWeakObjectPtr<UCanvasPanelSlot> CanvasSlot;
	};

	TArray<FEntityBadgeEntry> EntityBadges;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LevelBadgeTexture;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> MiniMapViewportRoot;

	UPROPERTY()
	TObjectPtr<UBorder> MiniMapSelfDot;

	UPROPERTY()
	TObjectPtr<URokUnityModalWidget> ModalWidget;

	UPROPERTY()
	TObjectPtr<URokUnityFeatureScreenWidget> FeatureScreenWidget;

	UPROPERTY()
	TObjectPtr<URokMainHudWidget> TacticalHudWidget;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> StrategicFilterRoot;
	UPROPERTY(Transient)
	TObjectPtr<UButton> StrategicGlobeButton;
	bool bStrategicModulesVisible = false;
	float StrategicModuleSeconds = 0.25f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ServerListOverlay;

	UPROPERTY()
	TObjectPtr<UTextBlock> ServerListStatusText;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> WorldObjectInfoRoot;
	FRokUiWindowAnimation WorldObjectAnimation;
	UPROPERTY()
	TObjectPtr<UCanvasPanel> WorldObjectNormalRoot;
	float WorldObjectPageSeconds = 0.33333334f;
	UPROPERTY()
	TObjectPtr<URichTextBlock> WorldObjectDescriptionText;

	UPROPERTY()
	TObjectPtr<UImage> WorldObjectInfoArrow;

	UPROPERTY()
	TObjectPtr<UImage> WorldObjectInfoIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoLevelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoCoordinateText;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> WorldObjectInfoDetailsRoot;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoFeedbackText;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> FilterDots;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> FilterBackgrounds;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> FilterButtons;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> FilterClickSound;
	TArray<FRokWorldMapFilterTint> FilterTints;
	TArray<float> FilterStartAlphas;
	float FilterAnimationSeconds = 0.1f;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FilterLabels;

	TWeakObjectPtr<UCanvasPanelSlot> SelfMarkerCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> MiniMapViewportCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> MiniMapSelfCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> WorldObjectInfoCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> WorldObjectInfoPanelCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> WorldObjectInfoArrowCanvasSlot;
	TWeakObjectPtr<ARokWorldMapCityActor> PlayerCityActor;
	TWeakObjectPtr<ARokStrategicResourceLayer> StrategicResourceLayer;
	TWeakObjectPtr<ARokStrategicResourceLayer> StrategicExplorationLayer;
	TWeakObjectPtr<ARokWorldMapEntityActor> SelectedWorldObjectActor;

	FVector2D CurrentRootSize = FVector2D::ZeroVector;
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 810.0f);
	float RefreshAccumulator = 0.0f;
	bool bWorldObjectDetailsExpanded = false;
	bool bWorldObjectProjectionDebugPending = false;
	bool bTacticalPresentation = false;
	ERokWorldMapHudFilter SelectedFilter =
		ERokWorldMapHudFilter::Alliance;

	void BuildWidgetTree();
	void BuildPlayerIdentity();
	void BuildCoordinatePanel();
	void BuildMiniMap();
	void BuildRelationFilters();
	void UpdateFilterLegend();
	void BuildBottomControls();
	void BuildSelfMarker();

	void BuildEntityBadges();
	void UpdateEntityBadges();
	void BuildWorldObjectInfo();
	void BuildServerListOverlay();
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);
	void UpdatePlayerMarkerProjection();
	void UpdateWorldObjectInfoProjection();
	void UpdateMiniMapMarkers();
	void RefreshRuntimeValues();
	void ApplyEntityFilter();
	ARokWorldMapCityActor* ResolvePlayerCity();
	bool ResolveProjection(FRokWorldMapProjection& OutProjection) const;
	void SelectRelationFilter(ERokWorldMapHudFilter Filter);
	void OpenFeatureScreen();
	void ClearWorldObjectSelection();
	void SetWorldObjectDetailsExpanded(bool bExpanded);
	const TCHAR* ResolveWorldObjectIconPath(
		const FString& EntityKind) const;

	UFUNCTION()
	void HandleWorldObjectDetailsClicked();

	UFUNCTION()
	void HandleWorldObjectGatherClicked();

	UFUNCTION()
	void HandleOpenResourceSearchClicked();

	UFUNCTION()
	void HandleOpenMailClicked();

	UFUNCTION()
	void HandleOpenPlayerProfileClicked();

	UFUNCTION()
	void HandleOpenCityManagerClicked();

	UFUNCTION()
	void HandleOpenArmyShowClicked();

	UFUNCTION()
	void HandleOpenVipClicked();

	UFUNCTION()
	void HandleOpenPositionSearchClicked();

	UFUNCTION()
	void HandleOpenMapMarkersClicked();

	UFUNCTION()
	void HandleOpenChatClicked();

	UFUNCTION()
	void HandleEnterPlayerCityClicked();

	UFUNCTION()
	void HandleOpenServerListClicked();

	UFUNCTION()
	void HandleCloseServerListClicked();

	UFUNCTION()
	void HandleServerSelectionUnavailableClicked();

	UFUNCTION()
	void HandleAllianceFilterClicked();

	UFUNCTION()
	void HandleExploreFilterClicked();

	UFUNCTION()
	void HandleResourceFilterClicked();

	UFUNCTION()
	void HandleBookmarkFilterClicked();

	UCanvasPanelSlot* PlaceOnCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f),
		const FVector2D& Alignment = FVector2D::ZeroVector);

	UTextBlock* MakeHudText(
		const FName& Name,
		int32 FontSize,
		const FLinearColor& Color,
		bool bBold = false);

	UImage* MakeAtlasImage(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);

	UBorder* MakeAtlasBorder(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FMargin& BrushMargin);

	UButton* MakeAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);

	UButton* MakeTransparentHitButton(const FName& Name);

	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;

	static UTexture2D* LoadTransientUiTexture(const FString& FilePath);
	static int32 ReadInitialCityHallLevel();

	FSlateBrush MakeAtlasBrush(
		UTexture2D* Texture,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& BrushMargin = FMargin(0.0f)) const;
};
