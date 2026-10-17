#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokPowerCounterAnimation.h"
#include "RokUiWindowAnimation.h"
#include "RokMainHudWidget.generated.h"

class AActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UMaterialInstanceDynamic;
class USizeBox;
class USoundBase;
class UTextBlock;
class UTexture2D;
class UWidget;
class URokBuildingPanelWidget;
class URokBuildingUpgradeSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokPowerGainWidget;
class URokScoutScreenWidget;

DECLARE_MULTICAST_DELEGATE(FRokPlacementCancelRequested);
DECLARE_MULTICAST_DELEGATE(FRokPlacementConfirmRequested);

UCLASS()
class ROK_API URokMainHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Nullable until the HUD tree has been built (or when no feature host exists).
	URokUnityFeatureScreenWidget* GetFeatureScreenWidgetForPlacement() const { return FeatureScreenWidget.Get(); }
	bool IsPlacementBlockingModalOpen() const;
	void SetPlacementPresentation(bool bVisible, bool bGridValid, bool bCanCommit, const FText& Reason);
	FRokPlacementCancelRequested OnPlacementCancelRequested;
	FRokPlacementConfirmRequested OnPlacementConfirmRequested;
#if !UE_BUILD_SHIPPING
	bool IsPlacementMenuAnimatingForAudit() const { return PlacementMenuAnimation.IsAnimating(); }
	bool IsPlacementMenuClosingForAudit() const { return PlacementMenuAnimation.IsClosing(); }
	float GetPlacementMenuAnimationElapsedSecondsForAudit() const { return PlacementMenuAnimation.GetElapsedSeconds(); }
#endif
	void SetSelectedBuilding(AActor* InBuildingActor);
	void OpenCommandLineModal();
	// Configure before TakeWidget: world identity/navigation/markers stay in
	// the parent HUD, and both zoom bands share its existing modal hosts.
	void ConfigureWorldMapTacticalOverlay(
		URokUnityModalWidget* InModalWidget,
		URokUnityFeatureScreenWidget* InFeatureScreenWidget);
	// Shared by NativeTick and deterministic animation acceptance tests.
	void AdvanceHudAnimations(float DeltaSeconds);
	void SetPowerPresentation(int32 Value);
	void SetWorldMapTacticalVisible(bool bVisible);
	void AdvanceWorldMapModuleAnimation(float DeltaSeconds);
	// Presentation only; does not grant rewards or change runtime task state.
	void SetTaskRewardPresentation(int32 TaskId, bool bComplete);
	// UI-only adapter. Caller owns membership/channel/unread authority.
	void SetChatChannelPresentation(bool bInAlliance, bool bAllianceChannel, int32 AllianceUnread);
#if WITH_DEV_AUTOMATION_TESTS
	int32 GetFoldSoundRequestsForTesting() const { return FoldSoundRequests; }
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
	TObjectPtr<UTextBlock> RuntimeInfoText;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TaskTitleTexts;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TaskProgressTexts;

	UPROPERTY()
	TObjectPtr<UImage> TaskRewardImage;
	int32 RewardTaskId = 0;
	bool bTaskRewardVisible = false;
	float TaskRewardTime = 0.0f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> HotTaskGroup;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> WorldMapRightModules;
	bool bWorldMapModulesVisible = false;
	float WorldMapModuleSeconds = 0.25f;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> TaskListBackground;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> TaskListClip;

	UPROPERTY()
	TObjectPtr<UButton> TaskFoldButton;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> FeatureBackground;
	UPROPERTY(Transient)
	TObjectPtr<UImage> MoreFeatureOverlay;

	UPROPERTY()
	TObjectPtr<UButton> MoreFeatureButton;

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
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlacementControls;
	UPROPERTY(Transient)
	TObjectPtr<UImage> PlacementConfirmIcon;
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
	FRokUiWindowAnimation PlacementMenuAnimation;
	FText PlacementReason;

	UPROPERTY()
	TObjectPtr<URokScoutScreenWidget> ScoutScreenWidget;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> CityMarkerRoot;

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

	void BuildWidgetTree();
	void BuildPlayerPowerGroup();
	void BuildPositionGroup();
	void BuildResourceGroup();
	void BuildAgeGroup();
	void BuildTaskGroup();
	void BuildChatAndMapGroup();
	void BuildFeatureGroup();
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
		bool bShowRedPoint = false,
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
	void HandleToggleFeatureBar();

	UFUNCTION()
	void HandleToggleTaskPanel();
};
