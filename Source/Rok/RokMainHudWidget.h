#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokPowerCounterAnimation.h"
#include "RokUiWindowAnimation.h"
#include "RokMainHudWidget.generated.h"

class AActor;
class ARokCitySpriteActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UMaterialInstanceDynamic;
class USizeBox;
class UScaleBox;
class UScrollBox;
class UVerticalBox;
class USoundBase;
class UTextBlock;
class UTexture2D;
class UWidget;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokPowerGainWidget;
class URokUiPageWidget;
class URokScoutScreenWidget;
struct FRokPlayerTopBar;

DECLARE_MULTICAST_DELEGATE(FRokPlacementCancelRequested);
DECLARE_MULTICAST_DELEGATE(FRokPlacementConfirmRequested);

UCLASS()
class ROK_API URokMainHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static TSubclassOf<URokMainHudWidget> ResolveHudClass(bool bTactical = false);

	// Nullable until the HUD tree has been built (or when no feature host exists).
	URokUnityFeatureScreenWidget* GetFeatureScreenWidgetForPlacement() const { return FeatureScreenWidget.Get(); }
	bool IsPlacementBlockingModalOpen() const;
	void SetPlacementPresentation(bool bVisible, bool bGridValid, bool bCanCommit,
		const FText& Reason, bool bCanOpenWorkerQueue = false);
	FRokPlacementCancelRequested OnPlacementCancelRequested;
	FRokPlacementConfirmRequested OnPlacementConfirmRequested;
#if !UE_BUILD_SHIPPING
	bool IsPlacementMenuAnimatingForAudit() const { return PlacementMenuAnimation.IsAnimating(); }
	bool IsPlacementMenuClosingForAudit() const { return PlacementMenuAnimation.IsClosing(); }
	float GetPlacementMenuAnimationElapsedSecondsForAudit() const { return PlacementMenuAnimation.GetElapsedSeconds(); }
#endif
	void SetSelectedBuilding(AActor* InBuildingActor);
	// Capture-only helper: freeze the open modal's window animation.
	void SampleModalAnimationForCapture(float Seconds);
	void OpenCommandLineModal();
	// Opens the developer gallery (F10) without going through the command line.
	void OpenUiGalleryHotkey();
	// Configure before TakeWidget: world identity/navigation/markers stay in
	// the parent HUD, and both zoom bands share its existing modal hosts.
	void ConfigureWorldMapTacticalOverlay(
		URokUnityModalWidget* InModalWidget,
		URokUnityFeatureScreenWidget* InFeatureScreenWidget);
	// Shared by NativeTick and deterministic animation acceptance tests.
	void AdvanceHudAnimations(float DeltaSeconds);
	void SetPowerPresentation(int32 Value);
	void SetWorldMapTacticalVisible(bool bVisible);
	void SetWorldMapSearchVisible(bool bVisible);
	void AdvanceWorldMapModuleAnimation(float DeltaSeconds);
	// Presentation only; does not grant rewards or change runtime task state.
	void SetTaskRewardPresentation(int32 TaskId, bool bComplete);
	// UI-only adapter. Caller owns membership/channel/unread authority.
	void SetChatChannelPresentation(bool bInAlliance, bool bAllianceChannel, int32 AllianceUnread);

	// Unity MainInterfaceMediator.SetCityButton()/OnCityButtonChange(): the split
	// is city_bound (250). Inside the city band the coordinate box is hidden and
	// the hammer + world-globe pair is shown; past it the box appears and the
	// pair flips to the search magnifier + return-to-city castle.
	static bool IsCityHudInsideBand(float CityDxf);
	// MainInterfaceMediator.OnCityButtonChange: outSide = dxf > city_bound ||
	// !IsInViewPort2D(city). The button pair flips on either condition; the
	// coordinate box follows the camera band only.
	static bool IsCityButtonOutsideBand(float CityDxf, bool bCityInViewport);
	void ApplyCityHudBand(bool bInsideCity);
	void ApplyCityHudButtons(bool bOutside);
	bool IsCityInViewport();
	// Unity GlobalCityBuidlingMediator.OnBuildBarHud -> UI_Tip_BuildingBar: one bar per
	// build-queue entry (new builds and upgrades) at the building's Collider root.
	// ClientUtils.FormatCountDown: HH:MM:SS, prefixed by Language 128004 past a day.
	static FString FormatRemainingClock(float Seconds);
	static FSlateFontInfo MakeUnitySourceFont(int32 Pixels, bool bBold);
	// RefreshhudView slider value (costTime - leftTime) / costTime; 1 once overdue.
	static float GetBuildBarProgress(float TotalSeconds, float RemainingSeconds);
	void BuildConstructionBarPool();
	void UpdateConstructionBars();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanel>> ConstructionBars;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ConstructionBarLabels;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanel>> ConstructionBarFills;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ConstructionBarIcons;
	TArray<TWeakObjectPtr<AActor>> ConstructionBarActors;
	UPROPERTY(Transient)
	FSlateBrush ConstructionBarBuildingBrush;
	UPROPERTY(Transient)
	FSlateBrush ConstructionBarLevelUpBrush;
	// Current city camera dxf, or -1 when no city camera pawn owns this HUD.
	float GetCityCameraDxf() const;
#if WITH_DEV_AUTOMATION_TESTS
	bool IsCityHudInsideBandAppliedForTesting() const { return bCityHudInsideBand; }
#endif
#if WITH_DEV_AUTOMATION_TESTS
	int32 GetFoldSoundRequestsForTesting() const { return FoldSoundRequests; }
#endif

	// Live Role_RoleInfo values for the top bar (power, resources, VIP, head, coordinates,
	// age, city name and level). RefreshFromSubsystem calls it while the game net subsystem
	// is RoleReady; otherwise the local preview values stay.
	void ApplyServerTopBar(const FRokPlayerTopBar& TopBar);
#if WITH_DEV_AUTOMATION_TESTS
	int32 GetShownHeadIdForTesting() const { return ShownHeadId; }
	int32 GetShownHeadFrameIdForTesting() const { return ShownHeadFrameId; }
#endif

	UFUNCTION(BlueprintCallable, Category="Rok HUD")
	void RefreshFromSubsystem();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	FRokPowerCounterAnimation PowerCounter;
	UPROPERTY(Transient)
	TObjectPtr<URokPowerGainWidget> PowerGain;
	void ApplyChatChannelPresentation();
	bool bChatAllianceVisible = false;
	bool bChatAllianceSelected = false;
	bool bChatUnreadFlash = false;
	float ChatUnreadTime = 0;
	UPROPERTY(Transient)
	TObjectPtr<UImage> ChatWorldMarker;
	UPROPERTY(Transient)
	TObjectPtr<UImage> ChatAllianceMarker;
	UPROPERTY(Transient)
	TObjectPtr<UImage> ChatAllianceMarkerBase;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> TaskFoldSound;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> FeatureFoldSound;
	void PlayFoldSound(USoundBase* Sound);
#if WITH_DEV_AUTOMATION_TESTS
	int32 FoldSoundRequests = 0;
#endif
	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedDesignSizeBox;

	UPROPERTY(Transient) TObjectPtr<UScaleBox> CityTopBarLayer;
	UPROPERTY(Transient) TObjectPtr<USizeBox> CityTopBarSizeBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ExpandedDesignCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> FoodText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WoodText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GemText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PowerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> VipText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CoordinateText;

	UPROPERTY()
	TObjectPtr<UTextBlock> AgeText;

	UPROPERTY()
	TObjectPtr<UImage> PlayerHeadImage;

	UPROPERTY()
	TObjectPtr<UImage> VipRedPointImage;

	UPROPERTY()
	TObjectPtr<UImage> PlayerHeadFrameImage;

	// PlayerHead ids drawn by PlayerHeadImage/PlayerHeadFrameImage (BuildPlayerPowerGroup draws 1000/2000).
	int32 ShownHeadId = 1000;
	int32 ShownHeadFrameId = 2000;
	void ApplyPlayerHeadSprite(UImage* Image, int32 HeadId, const FVector2D& DesiredSize);

	UPROPERTY()
	TObjectPtr<UTextBlock> RuntimeInfoText;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TaskTitleTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TaskProgressTexts;

	UPROPERTY()
	TObjectPtr<UImage> TaskRewardImage;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> TaskScroll;
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TaskListBox;
	FVector2D TaskRowLayoutSize{196.0, 80.0};
	double TaskRowBottomPadding = 4.0;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ServerTaskRewardImages;
	TArray<int32> ServerHudTaskIds;
	TArray<int32> ServerHudTaskPages;
	bool bServerTaskRows = false;
	FString LastServerHudTaskSignature;
	void RebuildTaskRows(int32 Count, bool bServer);
	bool ApplyServerTasks();
	void RefreshNotificationBadges();
	UCanvasPanel* MakeNotificationBadge(const FName& Name);
	void SetNotificationCount(const FName& Name, int32 Count);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextBlock>> NotificationTexts;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ChargeEventsButton;
	int32 ChargeEventPage = 101;
	void HandleServerTaskClicked(int32 RowIndex);
	void UpdateServerArmyHud();
	void BuildBarracksIdleMarker();
	void UpdateBarracksIdleMarker(float DeltaTime);
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> BarracksIdleGlyphs;
	float BarracksIdleMarkerElapsed = 0.0f;

	void BuildCityStatusIndicators();
	void UpdateCityStatusIndicators(float DeltaTime);
	void HandleCityStatusClicked(int32 BuildingIndex);
	struct FCityStatusIndicator
	{
		TWeakObjectPtr<UCanvasPanel> Root;
		TWeakObjectPtr<UCanvasPanel> Pop;
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UImage> Icon;
		TWeakObjectPtr<UImage> Flag;
		TArray<TWeakObjectPtr<UImage>> Idle;
		FVector2D PopBaseOffset = FVector2D::ZeroVector;
		TArray<FVector2D> IdleBaseOffsets;
		bool bCanUpgrade = false;
		bool bIdle = false;
		bool bHospitalWound = false;
		FText Caption;
	};
	TMap<int32, FCityStatusIndicator> CityStatusIndicators;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> CityStatusIndicatorRoots;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> CityUpgradeFlagTexture;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> CityIdleTexture;
	float CityStatusElapsed = 0.0f;
	float CityStatusRefreshDelay = 0.0f;

	void BuildCityHarvestPops();
	void UpdateCityHarvestPops(float DeltaTime);
	void HandleCityHarvestClicked(int32 Index);
	void RequestSelectedBuildingHarvest(AActor* BuildingActor);
	void SubmitCityHarvest(const TMap<int32, int64>& Buildings, AActor* FeedbackAnchor = nullptr);
	void ShowCityHarvestFeedback(const FText& Message, AActor* SuccessAnchor = nullptr);

	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> CityHarvestRoots;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> CityHarvestNormalBackgrounds;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> CityHarvestFullBackgrounds;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> CityHarvestIcons;
	struct FCityHarvestEntry
	{
		int32 Index = 0;
		int32 Type = 0;
		int64 GainTime = 0;
		bool bFull = false;
		TWeakObjectPtr<ARokCitySpriteActor> Anchor;
	};
	TArray<FCityHarvestEntry> CityHarvestEntries;
	TArray<int32> CityHarvestPopTypes;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CityHarvestFeedback;
	TWeakObjectPtr<ARokCitySpriteActor> CityHarvestFeedbackAnchor;
	float CityHarvestFeedbackSeconds = 0.0f;
	float CityHarvestRefreshDelay = 0.0f;

	void BuildServerTrainingPopPool();
	void UpdateServerTrainingPops();
	void HandleTrainingPopClicked(int32 Index);
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ArmyQueueGroup;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ArmyQueueCountText;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> TrainingPopRoots;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> TrainingPopFills;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> TrainingPopProgressAreas;
	UPROPERTY(Transient) TArray<TObjectPtr<UCanvasPanel>> TrainingPopReadyAreas;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TrainingPopLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> TrainingPopIcons;
	UPROPERTY(Transient) TArray<TObjectPtr<UImage>> TrainingPopReadyIcons;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> TrainingTroopAtlas;
	TArray<int32> TrainingPopFamilies;
	TArray<TWeakObjectPtr<AActor>> TrainingPopAnchors;
	FString LastServerArmySignature;
	int32 RewardTaskId = 0;
	bool bTaskRewardVisible = false;
	float TaskRewardTime = 0.0f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> HotTaskGroup;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> WorldMapRightModules;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> WorldMapNonResourceModules;
	bool bWorldMapModulesVisible = false;
	float WorldMapModuleSeconds = 0.25f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> TaskListBackground;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> TaskListClip;

	UPROPERTY()
	TObjectPtr<UButton> TaskFoldButton;
	FVector2D TaskBackgroundOpenSize = FVector2D::ZeroVector;
	FVector2D TaskClipOpenSize = FVector2D::ZeroVector;
	FVector2D TaskClipOpenPosition = FVector2D::ZeroVector;
	FVector2D TaskFoldOpenPosition = FVector2D::ZeroVector;
	float TaskFoldOpenAngle = 0.0f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> FeatureBackground;
	UPROPERTY(Transient)
	TObjectPtr<UImage> MoreFeatureOverlay;

	UPROPERTY()
	TObjectPtr<UButton> MoreFeatureButton;
	FVector2D FeatureBackgroundOpenSize = FVector2D::ZeroVector;
	FVector2D FeatureBackgroundBorders = FVector2D::ZeroVector;
	float MoreFeatureOpenAngle = 0.0f;
	TArray<FVector2D> CollapsibleFeatureOpenTranslations;

	UPROPERTY()
	TObjectPtr<UTextBlock> CityNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CityLevelText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<URokBuildingPanelWidget> BuildingPanelWidget;

	UPROPERTY()
	TObjectPtr<URokUnityModalWidget> ModalWidget;

	UPROPERTY()
	TObjectPtr<URokUnityFeatureScreenWidget> FeatureScreenWidget;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ExpeditionUnlockTip;
	float ExpeditionUnlockTipSeconds = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlacementControls;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PlacementConfirmIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PlacementButtonShadow;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PlacementCancelIcon;
	// Unity GameButton ColorTint is applied to the same Image as GrayChildrens.
	// Keep its source Gamma-space color tween separate from Slate's disabled effect.
	FLinearColor PlacementConfirmSourceTint = FLinearColor::White;
	FLinearColor PlacementConfirmTintStart = FLinearColor::White;
	FLinearColor PlacementConfirmTintTarget = FLinearColor::White;
	float PlacementConfirmTintElapsed = 0.0f;
	FLinearColor PlacementCancelSourceTint = FLinearColor::White;
	FLinearColor PlacementCancelTintStart = FLinearColor::White;
	FLinearColor PlacementCancelTintTarget = FLinearColor::White;
	float PlacementCancelTintElapsed = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PlacementMenuAtlas;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PlacementConfirmGrayMaterial;
	TWeakObjectPtr<UCanvasPanelSlot> PlacementControlsSlot;
	TWeakObjectPtr<UCanvasPanelSlot> PlacementControlsDesignSlot;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementStatusText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementReasonText;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlacementCancelButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> PlacementConfirmButton;
	bool bPlacementControlsVisible = false;
	bool bPlacementGridValid = false;
	bool bPlacementCanCommit = false;
	bool bPlacementCanOpenWorkerQueue = false;
	FRokUiWindowAnimation PlacementMenuAnimation;
	FText PlacementReason;

	UPROPERTY()
	TObjectPtr<URokScoutScreenWidget> ScoutScreenWidget;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> CityMarkerRoot;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> PositionGroup;

	UPROPERTY()
	TObjectPtr<UButton> BuildCityButton;

	UPROPERTY()
	TObjectPtr<UButton> CityWorldButton;

	UPROPERTY()
	TObjectPtr<UButton> WorldGlobeButton;

	UPROPERTY()
	TObjectPtr<UButton> WorldSearchButton;

	bool bCityHudInsideBand = true;
	bool bCityHudButtonsOutside = false;

	UPROPERTY()
	TArray<TObjectPtr<UWidget>> CollapsibleFeatureWidgets;

	TWeakObjectPtr<AActor> SelectedBuildingActor;
	TWeakObjectPtr<AActor> CityMarkerAnchorActor;
	TWeakObjectPtr<UCanvasPanelSlot> CityMarkerCanvasSlot;
	FVector2D CurrentRootSize = FVector2D::ZeroVector;
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 900.0f);
	float RefreshAccumulator = 0.0f;
	bool bFeatureBarExpanded = true;
	bool bWorldMapTacticalOverlay = false;
	bool bTaskCollapsed = false;
	float TaskFold = 0.0f;
	float TaskFoldStart = 0.0f;
	float TaskAnimationTime = 1.0f;
	float FeatureAnimationTime = 1.0f;
	float FeatureFold = 0.0f;
	float FeatureFoldStart = 0.0f;
	float FeatureButtonsOffset = 0.0f;
	float FeatureButtonsStart = 0.0f;
	float FeatureAlpha = 1.0f;
	float FeatureAlphaStart = 1.0f;
	struct FResourceCellLayout
	{
		double Width = 0.0;
		double Gap = 0.0;
		double Y = 0.0;
	};
	TArray<FResourceCellLayout> ResourceCellLayouts;
	double ResourceGroupLeftPadding = 0.0;
	double ResourceGroupRightPadding = 4.0;

	void BuildWidgetTree();
	void BuildHudLayout(UCanvasPanel* RootCanvas);
	void BindHudLayout();
	void BuildPlacementControls(UCanvasPanel* RootCanvas);
	void ConfigurePlacementMaterial();
	void AddPowerGain(UCanvasPanel* Group);
	void BuildPlayerPowerGroup(UCanvasPanel* Parent);
	void BuildPositionGroup();
	void BuildCityTopBar(UCanvasPanel* RootCanvas);
	void BuildResourceGroup(UCanvasPanel* Parent);
	void CacheResourceGroupLayout();
	void BuildAgeGroup();
	void BuildTaskGroup();
	void BindTaskGroup(URokUiPageWidget& Page);
	void CacheTaskFoldLayout();
	void BuildChatAndMapGroup();
	void BuildFeatureGroup();
	void BuildExpeditionUnlockTip();
	void BindFeatureGroup(URokUiPageWidget& Page);
	void CacheFeatureFoldLayout();
	URokUiPageWidget* CreateHudPage(const TSoftClassPtr<URokUiPageWidget>& Class, FName Name);
	URokUiPageWidget* AddHudPage(UCanvasPanel* Parent, const TSoftClassPtr<URokUiPageWidget>& Class,
		FName Name, int32 ZOrder = 0);
	bool BindTaskRow(URokUiPageWidget& Page, int32 Index);
	void BindHudButton(const UUserWidget& Owner, FName Name, FName Function);
	void BuildCityMarker();
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);
	void UpdateCityMarkerProjection();
	void UpdatePlacementMenuProjection();
	void AdvancePlacementButtonTints(float DeltaSeconds);
	void AdvancePlacementMenuAnimation(float DeltaSeconds);
	void ApplyFeatureBarVisibility();
	void ApplyTaskFoldPresentation();
	void HandleBuildingInfoRequested(AActor* BuildingActor);
	void HandleBuildingUpgradeRequested(AActor* BuildingActor);
	void HandleBuildingFeatureRequested(AActor* BuildingActor);

	UFUNCTION()
	void HandleReturnToWorldMap();

	UFUNCTION()
	void HandleFocusCityCenter();
	UFUNCTION()
	void HandlePlacementCancelClicked();
	UFUNCTION()
	void HandlePlacementConfirmClicked();

	UCanvasPanelSlot* PlaceOnCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f),
		const FVector2D& Alignment = FVector2D::ZeroVector);
	UTextBlock* MakeHudText(const FName& Name, int32 FontSize, const FLinearColor& Color, bool bBold = false);
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
	UCanvasPanel* MakeHudSlicedAtlasPanel(
		const FName& Name, const TCHAR* TexturePath, const FIntRect& UnityRect,
		const FIntPoint& AtlasSize, const FMargin& BorderPixels);
	UButton* MakeAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);
	UCanvasPanel* MakeTaskRow(int32 Index);
	UCanvasPanel* MakeResourceCell(
		const FName& Name,
		const FIntRect& IconRect,
		UTextBlock*& OutTextBlock,
		bool bShowAddButton);
	UCanvasPanel* MakeFeatureButton(
		const FName& Name,
		const FText& Label,
		const FIntRect& IconRect,
		bool bHasNotificationCount = false,
		UButton** OutButton = nullptr);
	UButton* MakeTransparentHitButton(const FName& Name);
	UTexture2D* LoadUiTexture(const TCHAR* TexturePath) const;
	FSlateBrush MakeAtlasBrush(
		UTexture2D* Texture,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& BrushMargin = FMargin(0.0f)) const;
	URokBuildingUpgradeSubsystem* GetUpgradeSubsystem() const;

	UFUNCTION()
	void HandleOpenPlayerProfile();

	UFUNCTION()
	void HandleOpenCityManager();

	UFUNCTION()
	void HandleOpenArmyShow();

	UFUNCTION()
	void HandleOpenFoodResources();

	UFUNCTION()
	void HandleOpenWoodResources();

	UFUNCTION()
	void HandleOpenActivityCenter();

	UFUNCTION()
	void HandleOpenMail();

	UFUNCTION()
	void HandleOpenTasks();
	int32 PendingTaskBuildingType = 0;
	bool bPendingTaskBuildCity = false;
	bool bPendingTaskUpgradeBuilding = false;
	bool HandleTaskGoTownHallRequested(AActor* BuildingActor);

	UFUNCTION()
	void HandleOpenCommander();

	UFUNCTION()
	void HandleOpenItems();

	UFUNCTION()
	void HandleOpenAlliance();

	UFUNCTION()
	void HandleOpenVip();

	UFUNCTION()
	void HandleOpenChat();

	UFUNCTION()
	void HandleOpenWorldPositionSearch();

	UFUNCTION()
	void HandleOpenMapMarkers();

	UFUNCTION()
	void HandleOpenBuildCity();

	UFUNCTION()
	void HandleOpenExpedition();

	UFUNCTION()
	void HandleOpenGemShop();

	UFUNCTION()
	void HandleOpenCharge();

	UFUNCTION()
	void HandleOpenChargeEvents();

	UFUNCTION()
	void HandleToggleFeatureBar();

	UFUNCTION()
	void HandleToggleTaskPanel();
};
