#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokMapPointerGesture.h"
#include "RokWorldMapTypes.h"
#include "RokServerArmySubsystem.h"
#include "RokWorldMapFilterTint.h"
#include "RokButtonFeedbackStyle.h"
#include "RokUiWindowAnimation.h"
#include "RokPowerCounterAnimation.h"
#include "Styling/SlateBrush.h"
#include "RokWorldMapHudWidget.generated.h"

class ARokWorldMapCityActor;
class ARokWorldMapPlayerController;
class ARokStrategicResourceLayer;
class ARokWorldMapEntityActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCheckBox;
class UCanvasPanelSlot;
class UImage;
class USizeBox;
class UScrollBox;
class UProgressBar;
class UTextBlock;
class URichTextBlock;
class UTexture2D;
class USoundBase;
class UWidget;
class URokMainHudWidget;
class URokUiPageWidget;
class URokUnityIndexedButton;
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
 * Full-viewport map click and drag surface below every HUD control.
 * Standalone GameAndUI focus may not forward empty-world clicks to the controller.
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

	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

private:
	ARokWorldMapPlayerController* GetMapController() const;
	FVector2D GetViewportPosition(const FPointerEvent& Event) const;
	void UpdateMapPointer(ARokWorldMapPlayerController& Controller, FVector2D Position);

	FRokMapPointerGesture MapPointer;
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
	static TSubclassOf<URokWorldMapHudWidget> ResolveHudClass();

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
	bool FocusWorldSearchResult(int64 ObjectId, FVector2D UnityPosition);
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
	void BuildGatherPanel();
	struct FWorldMarchRowLayout
	{
		UImage* RarityFrame = nullptr;
		UImage* Portrait = nullptr;
		UImage* SelectionFrame = nullptr;
		UImage* Status = nullptr;
	};

	void BuildWorldMarchQueue();
	FWorldMarchRowLayout BuildWorldMarchRowLayout(UCanvasPanel* Row, const FString& Suffix);
	void BindWorldMarchQueue(URokUiPageWidget& Page);
	void BindGatherPanel(URokUiPageWidget& Page);
	void RefreshWorldMarchQueue();
	void UpdateAttackMarchMarkers(const FRokServerArmySnapshot& Snapshot);
	void UpdateAttackMarchRoutes();
	void BuildWorldOverlayArt();
	void BuildWorldDamageArt();
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> WorldDamageArtPage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldDamageArtRoot;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> WorldDamageArt;
	const UImage& GetWorldOverlayArt(FName Name) const;
	void ApplyWorldOverlayArt(UImage& Image, const UImage& Art) const;
	UImage* AddWorldOverlayImage(UCanvasPanel* Parent, FName Name, const UImage& Art);
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> WorldOverlayArtPage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldOverlayArtRoot;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UImage>> WorldOverlayImages;

	TArray<FRokServerMarch> AttackMarchRoutes;
	double AttackMarchSampleTime = 0.0;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> AttackRouteDots;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> AttackRouteDestinations;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> AttackRouteDestinationTexture;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> AttackMarchMarkers;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AttackDamageLabels;
	void HandleWorldMarchClicked(int32 ArmyIndex);
	void TryOpenFocusedWorldTarget();
	FString PendingWorldTargetId;
	FVector2D PendingWorldTargetCenter = FVector2D::ZeroVector;
	double PendingWorldTargetDeadline = 0;
	bool bPendingWorldTargetFromSearch = false;
	UPROPERTY(Transient) TObjectPtr<UImage> WorldSearchResultGuide;
	UFUNCTION()
	void HandleWorldMarchQueueToggle();
	void BuildWorldGatherProgress(UCanvasPanel* Panel);
	int64 SelectedRecallTargetId = 0;
	int32 SelectedMarchArmyIndex = 0;
	bool bWorldMarchQueueExpanded = true;
	FString WorldMarchQueueSignature;
	double WorldMarchRowHeight = 86.0;
	double WorldMarchViewportHeight = 258.0;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> WorldMarchQueueRoot;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WorldMarchQueueCount;
	UPROPERTY(Transient)
	TObjectPtr<UImage> WorldMarchQueueArrow;
	float WorldMarchQueueArrowClosedAngle = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> WorldMarchQueueList;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> WorldGatherProgressRoot;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> WorldGatherProgressBar;
	UPROPERTY(Transient)
	TObjectPtr<UImage> WorldGatherProgressIcon;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WorldGatherProgressText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> WorldGatherActionButton;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WorldGatherActionText;
	void AddWorldPopupFrame(UCanvasPanel* Panel, const FVector2D& PanelSize, const TCHAR* Prefix);
	void OpenGatherPanel(ARokWorldMapEntityActor* Target, int64 TargetId, TOptional<bool> AttackStationAfterBattle = {});
	void BuildBarbarianInfo();
	void BindBarbarianInfo(URokUiPageWidget& Page);
	void RefreshBarbarianInfo(const ARokWorldMapEntityActor& Target);
	void SetBarbarianDetailsExpanded(bool bExpanded);
	UFUNCTION() void HandleBarbarianDetailsClicked();
	UFUNCTION() void HandleBarbarianDetailsBackClicked();
	struct FBarbarianRewardLayout
	{
		UImage* Frame = nullptr;
		UImage* Icon = nullptr;
		UTextBlock* Value = nullptr;
		UTextBlock* Count = nullptr;
	};

	struct FAttackArmyChoiceLayout
	{
		UWidget* Root = nullptr;
		URokUnityIndexedButton* Button = nullptr;
		UTextBlock* Label = nullptr;
		bool bAuthoredSelectionStyle = false;
	};

	FBarbarianRewardLayout BuildBarbarianRewardLayout(UCanvasPanel* Cell, int32 Index);
	FAttackArmyChoiceLayout BuildAttackArmyChoiceLayout(int32 ArmyIndex, bool bSelected);
	void RefreshAttackArmyChoices();
	void SelectAttackArmy(int32 ArmyIndex);
	UFUNCTION() void HandleBarbarianAttackClicked();
	UFUNCTION() void HandleSelectNewAttackArmy();
	void UpdateGatherRoute();
	void SetGatherQueueVisible(bool bVisible);
	UFUNCTION()
	void HandleGatherCreateClicked();
	UFUNCTION()
	void HandleGatherCloseClicked();
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GatherRoot;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GatherQueueCount;
	UPROPERTY(Transient)
	TObjectPtr<UImage> GatherQueueShade;
	double GatherQueueShadeBottomInset = 0.0;
	double GatherQueueShadeMinimumHeight = 0.0;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GatherFeedback;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> GatherRouteDots;
	TMap<TWeakObjectPtr<UWidget>, float> GatherHiddenChrome;
	TWeakObjectPtr<ARokWorldMapEntityActor> GatherTarget;
	int64 GatherTargetId = 0;
	TOptional<bool> GatherAttackStation;
	int32 GatherExistingArmyIndex = 0;
	bool bExistingAttackPending = false;
	uint32 GatherQueueSerial = 0;
	uint64 GatherQueueAoiGeneration = 0;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> AttackArmyChoices;
	UPROPERTY(Transient) TObjectPtr<UButton> GatherCreateButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GatherCreateLabel;
	UPROPERTY(Transient) TObjectPtr<UWidget> GatherNewSelected;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldResourceInfoPanel;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldBarbarianInfoPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianCoordinate;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BarbarianNormalPage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BarbarianDetailsPage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BarbarianRewards;
	TArray<FBox2D> BarbarianRewardCells;
	UPROPERTY(Transient) TObjectPtr<UImage> BarbarianPortrait;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> BarbarianTimer;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianTimeRemaining;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianDescription;
	int32 BarbarianDisplayedRewardGroup = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianCost;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianFeedback;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> BarbarianStay;
	UPROPERTY(Transient) TObjectPtr<UButton> BarbarianAttackButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BarbarianAttackLabel;
	float GatherPreviousDxf = 0;
	FVector2D GatherPreviousCenter = FVector2D::ZeroVector;
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

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldMapChrome;
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
		FString ServerObjectId;
		TObjectPtr<UCanvasPanel> Root;
		TObjectPtr<UTextBlock> LevelText;
		TObjectPtr<UImage> GatheringIcon;
		TWeakObjectPtr<UCanvasPanelSlot> CanvasSlot;
	};

	TArray<FEntityBadgeEntry> EntityBadges;
	uint64 NextEntityBadgeId = 0;

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
	TObjectPtr<UTextBlock> WorldObjectInfoAmountText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoCollectorText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorldObjectInfoAllianceText;

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
	FRokButtonFeedbackStyle FilterFeedbackStyle;
	TArray<FRokWorldMapFilterTint> FilterTints;
	TArray<float> FilterStartAlphas;
	float FilterAnimationSeconds = 0.1f;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> FilterLabels;

	TWeakObjectPtr<UCanvasPanelSlot> SelfMarkerCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> MiniMapViewportCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> MiniMapSelfCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> WorldObjectInfoCanvasSlot;
	TWeakObjectPtr<UCanvasPanelSlot> WorldObjectInfoArrowCanvasSlot;
	float WorldObjectInfoArrowBaseAngle = 0.0f;
	struct FWorldObjectInfoPlacement
	{
		float ObjectGap = 50.0f;
		float ViewportInset = 12.0f;
		float ArrowCornerInset = 18.0f;
		float ArrowEdgeOffset = 5.0f;
	};
	FWorldObjectInfoPlacement WorldObjectInfoPlacement;
	TWeakObjectPtr<ARokWorldMapCityActor> PlayerCityActor;
	TWeakObjectPtr<ARokStrategicResourceLayer> StrategicResourceLayer;
	TWeakObjectPtr<ARokStrategicResourceLayer> StrategicExplorationLayer;
	TWeakObjectPtr<ARokWorldMapEntityActor> SelectedWorldObjectActor;
	FString SelectedWorldObjectServerId;

	FVector2D CurrentRootSize = FVector2D::ZeroVector;
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 810.0f);
	float RefreshAccumulator = 0.0f;
	bool bDisplayingServerTopBar = false;
	bool bWorldObjectDetailsExpanded = false;
	bool bWorldObjectProjectionDebugPending = false;
	bool bTacticalPresentation = false;
	ERokWorldMapHudFilter SelectedFilter =
		ERokWorldMapHudFilter::Alliance;

	void BuildWidgetTree();
	void BuildHudLayout(UCanvasPanel* RootCanvas);
	void BindHudLayout();
	void AddPowerGain(UCanvasPanel* Group);
	void AddMiniMapInput(UCanvasPanel* Group);
	void BuildPlayerIdentity();
	void BuildCoordinatePanel();
	void BuildMiniMap();
	void BuildRelationFilters();
	void UpdateFilterLegend();
	void BuildBottomControls();
	void BuildSelfMarker();

	void BuildEntityBadges();
	void BuildEntityBadgeLayout(FEntityBadgeEntry& Entry, uint64 BadgeId, bool bGatherable);
	void ReconcileEntityBadges();
	void UpdateEntityBadges();
	void RefreshSelectedWorldObjectValues();
	void BuildWorldObjectInfo();
	void BuildResourceInfoLayout();
	void BindResourceInfoLayout(URokUiPageWidget& Page);
	URokUiPageWidget* CreateWorldHudPage(const TSoftClassPtr<URokUiPageWidget>& Class, FName Name);
	URokUiPageWidget* AddWorldHudPage(UCanvasPanel* Parent, const TSoftClassPtr<URokUiPageWidget>& Class,
		FName Name, int32 ZOrder = 0);
	void BindWorldHudButton(const UUserWidget& Owner, FName Name, FName Function);
	void ApplyResourceDescriptionStyles();
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


	UFUNCTION()
	void HandleWorldObjectDetailsClicked();

	UFUNCTION()
	void HandleWorldObjectGatherClicked();

	UFUNCTION()
	void HandleWorldObjectBookmarkClicked();

	UFUNCTION()
	void HandleWorldObjectShareClicked();

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
