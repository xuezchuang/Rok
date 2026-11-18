#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RokUiWindowAnimation.h"
#include "RokPlayerHeadCatalog.h"
#include "RokLeaderboardCatalog.h"
#include "RokUnityModalWidget.generated.h"

class URokCommanderMeshWidget;

class AActor;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class USizeBox;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class URokBuildingUpgradeSubsystem;
struct FRokBuildingUpgradePreview;
class URokUnityFeatureScreenWidget;
class URokUiPageWidget;
class URokModalWindowFrame;

UENUM()
enum class ERokUnityModalMode : uint8
{
	None,
	PlayerProfile,
	PlayerHeadSelection,
	PlayerPowerDetails,
	PlayerRanking,
	CivilizationSelection,
	CivilizationChangeConfirmation,
	ResourceInventory,
	BuildingInfo,
	BuildingUpgrade,
	WarWarning,
	BuffList,
	GainEffect,
	// UI_Win_BattlePassRewards: battle-pass level rewards (BattlePassReward.csv).
	BattlePassRewards,
	// UI_Win_EventDate: activity schedule (ActivityCalendar.csv).
	EventDate,
	// UI_Win_GuildPurview: alliance rank permissions (AllianceMemberJurisdiction.csv).
	GuildPurview,
	// UI_Win_ChapterDialog: story chapters (TaskChapter.csv).
	ChapterDialog,
	// UI_Win_GuildFortress: alliance structures and their build cost.
	GuildFortress,
	// UI_Win_ItemCollection: item compendium (Item.csv).
	ItemCollection,
	// UI_Win_VipStore: VIP level shop (VipStore.csv).
	VipStore,
	// UI_Win_TavernBoxDesc: tavern box probabilities (TavernProbility.csv).
	TavernBoxDesc,
	// UI_Win_TheWall: city wall durability by level (BuildingCityWall.csv).
	TheWall,
	// UI_Win_TheTower: guard tower stats and HP-section buffs.
	TheTower,
	// UI_Win_GuildStore: alliance shop (AllianceShopItem.csv).
	GuildStore,
	// UI_Win_GiftLimitedTime: limited-time packs (RechargeLimitTimeBag.csv).
	GiftLimitedTime,
	// UI_Win_ChargePop: paid package list (RechargeSale.csv).
	ChargePop,
	// UI_Win_ExpeditionStore: expedition shop (ExpeditionShop.csv).
	ExpeditionStore,
	// UI_Win_TavernReward: tavern reward pool (TavernRank.csv).
	TavernReward,
	// UI_Win_FuncGuide: tutorial step list (Guide.csv).
	FuncGuide,
	// UI_Win_GuildGift: alliance gift levels (AllianceGiftReward.csv).
	GuildGift,
	// UI_Win_ItemExchange: pick-one reward boxes (ItemRewardChoice.csv).
	ItemExchange,
	// Developer gallery that opens every ported modal in one screen.
	UiGallery,
	// UI_Win_Vip: VIP privileges per level (VipAtt.csv).
	VipPrivilege,
	ChangeName,
};

enum class ERokChangeNameValidationResult : uint8
{
	Valid,
	Empty,
	TooShort,
	TooLong,
	OnlyDigits,
	SameAsCurrent,
	BannedWord,
	BannedWordDataUnavailable,
};

/**
 * Runtime C++ reconstruction of the Unity RoK modal family used by the player
 * profile, quick-use resource list, building information, and upgrade flows.
 * The widget samples original Unity atlas Sprite rects and keeps gameplay state
 * behind the existing C++ subsystems.
 */
/** One TaskDialog.csv line rendered by the chapter dialogue screen. */
struct FRokDialogLine
{
	FString Text;
	FString Model;
	FString Voice;
	bool bRight = false;
};

UCLASS()class ROK_API URokUnityModalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static TSubclassOf<URokUnityModalWidget> ResolveShellClass();
	void OpenPlayerProfile();
	void OpenPlayerHeadSelection();
	void OpenPlayerPowerDetails();
	void OpenPlayerRanking();
	void OpenCivilizationSelection();
	void OpenCivilizationChangeConfirmation();
	void OpenChangeName();
	void OpenResourceInventory(int32 InitialCategory = 0, bool bUseReferenceCaptureState = false);
	void OpenBuildingInfo(AActor* BuildingActor);
	// Opt-in reference audit only; does not create an owned building or current level.
	void OpenResourceBuildingInfoReferencePreview(int32 UnityBuildingTypeId);
	void OpenBuildingUpgrade(AActor* BuildingActor);
	void OpenWarWarning(bool bUseReferenceCaptureState = false);
	void OpenBuffList(bool bUseReferenceCaptureState = false);
	// UI_Win_GainEffect (市政厅增益效果): 0 = economy tab, 1 = military tab.
	void OpenGainEffect(int32 TabIndex = 0);
	// UI_Win_BattlePassRewards: battle-pass level rewards (BattlePassReward.csv).
	void OpenBattlePassRewards();
	// UI_Win_EventDate (活动日程): activity windows from ActivityCalendar.csv.
	void OpenEventDate();
	// UI_Win_GuildPurview (联盟权限): which ranks may use each alliance permission.
	void OpenGuildPurview();
	// UI_Win_ChapterDialog (剧情章节): TaskChapter.csv names and objectives.
	void OpenChapterDialog(int32 GroupId = 0);
	// UI_Win_GuildFortress (联盟堡垒): AllianceBuildingType/Data.csv costs.
	void OpenGuildFortress();
	// UI_Win_ItemCollection (道具图鉴): Item.csv names/type/quality.
	void OpenItemCollection();
	// UI_Win_VipStore (贵宾商店): VipStore.csv rows.
	void OpenVipStore();
	// UI_Win_TavernBoxDesc (酒馆宝箱概率): TavernProbility.csv rows.
	void OpenTavernBoxDesc();
	// UI_Win_TheWall (城墙): BuildingCityWall.csv durability rows.
	void OpenTheWall();
	// UI_Win_TheTower (警戒塔): BuildingGuardTower/GuardTowerAttr rows.
	void OpenTheTower();
	// UI_Win_GuildStore (联盟商店): AllianceShopItem.csv rows.
	void OpenGuildStore();
	// UI_Win_GiftLimitedTime (限时礼包): RechargeLimitTimeBag.csv rows.
	void OpenGiftLimitedTime();
	// UI_Win_ChargePop (充值礼包): RechargeSale.csv rows.
	void OpenChargePop();
	// UI_Win_ExpeditionStore (远征商店): ExpeditionShop.csv rows.
	void OpenExpeditionStore();
	// UI_Win_TavernReward (酒馆奖励池): TavernRank.csv rows.
	void OpenTavernReward();
	// UI_Win_FuncGuide (功能引导): Guide.csv step rows.
	void OpenFuncGuide();
	// UI_Win_GuildGift (联盟礼物): AllianceGiftReward/Type rows.
	void OpenGuildGift();
	// UI_Win_ItemExchange (自选宝箱): ItemRewardChoice.csv rows.
	void OpenItemExchange();
	// Developer-only entry: gallery of every ported modal (see BuildUiGallery).
	void OpenUiGallery();
	// UI_Win_Vip (贵宾特权): VipAtt.csv rows.
	void OpenVipPrivilege();
	// Capture-only: advance the shared window animation to a deterministic time
	// so a screenshot can freeze the source open/close curve (e.g. 0.1167 s).
	void SampleWindowAnimationForCapture(float Seconds);
	bool FreezePreviewAnimationForCapture(float Seconds);
	void SetFeatureScreenWidget(
		URokUnityFeatureScreenWidget* InFeatureScreenWidget);
	void CloseModal();
	// Applies a case-insensitive substring filter to the active definition list.
	void SetDefinitionFilter(const FString& Filter);
	// Rows currently rendered by the active definition list (after filtering).
	int32 GetDefinitionRowCount() const { return ActiveDefinitionRowCount; }
	/** User dismissal; unlike CloseModal, keeps the input mask until exit ends. */
	void RequestCloseModal();
	bool IsModalOpen() const { return ActiveMode != ERokUnityModalMode::None; }
	// WBP conversion: the canvas every mode builds its content into.
	UCanvasPanel* GetModeCanvas() const { return ModeCanvas; }
	// Production handoff proof: the requested actor and BuildingInfo content are mounted.
	bool IsBuildingInfoOpenForActor(const AActor* Actor) const;
#if !UE_BUILD_SHIPPING
	AActor* GetSelectedBuildingActorForAudit() const;
	bool IsBarracksLevelPageOpenForAudit() const { return ActiveMode == ERokUnityModalMode::BuildingUpgrade && bBarracksLevelPage; }
	int32 GetBarracksLevelSelectedLevelForAudit() const { return BarracksLevelSelectedLevel; }
	float GetBarracksLevelScrollOffsetForAudit() const;
	float GetBarracksLevelPageTimeForAudit() const { return BarracksLevelPageTime; }
#endif
#if WITH_DEV_AUTOMATION_TESTS
	void AdvanceWindowAnimationForTesting(float DeltaSeconds) { AdvanceWindowAnimation(DeltaSeconds); }
	const FRokUiWindowAnimation& GetWindowAnimationForTesting() const { return WindowAnimation; }
#endif
	static ERokChangeNameValidationResult ValidateChangeNameCandidate(
		const FString& Candidate,
		const FString& CurrentName,
		bool bHasCurrentName,
		const TArray<FString>& BannedWords,
		bool bBannedWordsLoaded);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	friend class URokCityBuffReferenceAuditSubsystem;
	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedDesignSizeBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ExpandedDesignCanvas;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ModeCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuffListAnimationRoot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GainEffectListRoot;
	int32 GainEffectTabIndex = 0;

	UPROPERTY()
	TObjectPtr<UButton> AnimationInputBlocker;

	UPROPERTY()
	TObjectPtr<UTextBlock> FeedbackText;

	int32 DisplayedUpgradeLevel = 0;
	uint32 UpgradeViewSerial = 0;
	bool bUpgradeUsesServer = false;
	bool bServerUpgradePending = false;
	TOptional<int64> ConfirmedInstantQuote;
	float UpgradeButtonRefreshRemaining = 0.0f;
	bool bUpgradeFeedbackFromAction = false;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> ChangeNameInput;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameCharacterCount;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameValidationFeedback;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChangeNameServiceBoundaryState;

	UPROPERTY()
	TObjectPtr<UButton> ChangeNameConfirmButton;

	UPROPERTY()
	TObjectPtr<URokUnityFeatureScreenWidget> FeatureScreenWidget;

	TWeakObjectPtr<AActor> SelectedBuildingActor;
	ERokUnityModalMode ActiveMode = ERokUnityModalMode::None;
	ERokUnityModalMode AnimatedMode = ERokUnityModalMode::None;
	ERokUnityModalMode CloseReturnMode = ERokUnityModalMode::None;
	FRokUiWindowAnimation WindowAnimation;
	bool bPreviewAnimationFrozenForCapture = false;
	float CivilizationListAnimationTime = 0.5f;
	float CivilizationHeroAnimationTime = 0.5f;
	float CivilizationSelectionAnimationTime = 0;
	UPROPERTY(Transient)
	TObjectPtr<UImage> CivilizationSelectionPulse;
	bool bCivilizationSwitchAnimation = false;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CivilizationDetailRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CivilizationListRoot;
	UPROPERTY(Transient)
	TObjectPtr<URokCommanderMeshWidget> CivilizationHero;
	int32 CivilizationHeroId = INDEX_NONE;
	int32 SelectedResourceCategory = 0;
	int32 SelectedPlayerHeadPage = 0;
	int32 SelectedPlayerHeadIndex = 0;
	TArray<FRokPlayerHeadRow> PlayerHeadPageRows;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> PlayerHeadGridScroll;
	float PlayerHeadScrollOffset = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PlayerKillTipLayer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerKillTipRoot;
	float PlayerKillTipTime = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> PlayerPowerActionTipLayer;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PlayerPowerActionTipRoot;
	float PlayerPowerActionTipTime = 0.f;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuildingDescriptionRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuildingLevelDetailsRoot;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BuildingLevelDetailsScroll;
	UPROPERTY(Transient)
	TObjectPtr<UButton> BuildingLevelBackButton;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BarracksLevelDetailsRoot;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BarracksLevelDetailsScroll;
	UPROPERTY(Transient)
	TObjectPtr<UButton> BarracksLevelBackButton;
	int32 ResourceBuildingReferenceType = 0;
	bool bBuildingLevelPage = false;
	float BuildingPageTime = 0.25f;
	bool bBarracksLevelPage = false;
	int32 BarracksLevelSelectedLevel = 0;
	float BarracksLevelPageTime = 0.25f;
	int32 SelectedRankingCategory = INDEX_NONE;
	TArray<FRokLeaderboardRow> PlayerRankingRows;
	int32 SelectedCivilizationId = 0;
	bool bReferenceWarWarningCaptureState = false;
	bool bReferenceBuffListCaptureState = false;
	bool bReferenceResourceCaptureState = false;
	bool bChangeNameHasCurrentName = false;
	bool bChangeNameBannedWordsLoadAttempted = false;
	bool bChangeNameBannedWordsLoaded = false;
	bool bChangeNameSubmitServiceAvailable = false;
	bool bUpdatingChangeNameInput = false;
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 900.0f);
	FString ChangeNameDraft;
	FString ChangeNameCurrentName;
	TArray<FString> ChangeNameBannedWords;

	void BuildWidgetTree();
	void RebuildModeContent(bool bOpening = false);
	// WBP spike: -RokUiModalPage=<WBP class path> mounts that page instead of the C++ build.
	bool TryMountPageOverride();
	// Adds a WBP piece laid out on the full 1456x910 mode canvas (frame, list).
	void MountFullPiece(UUserWidget* Piece, int32 ZOrder);
	// Mounts a window frame WBP at z 0 with its title set; null when the setting is
	// empty or -RokUiLegacy is on (the caller then builds the C++ frame).
	URokModalWindowFrame* MountWindowFrame(const TSoftClassPtr<URokModalWindowFrame>& FrameSetting, const FText& Title);
	// Mounts the mode's page WBP (URokUiSettings::ModalPages) over the mode canvas;
	// null, logged, when the mode has none. The page owns layout and static style.
	URokUiPageWidget* MountModePage(FName Page);
	// Shared editable text-only error states; false keeps the caller's native fallback.
	bool TryMountStatusMessage(UCanvasPanel* Parent, FName MessageName,
		FName LayoutName, const FText& Message);
	URokUiPageWidget* CreateStatusMessage(FName MessageName, FName LayoutName, const FText& Message);
	void AddBarracksAttributeUnavailable(UCanvasPanel* Parent, const FVector2D& Position,
		const FVector2D& Size, int32 ZOrder);
	// Places one item WBP (a row, tile or cell listed in ModalPages) in a page canvas
	// as InstanceName; its children answer to their old flat names through Prefix,
	// Suffix (the data id) and Index (the list index). Null, logged, when unmapped.
	URokUiPageWidget* MountItem(FName Item, UCanvasPanel* Parent, FName InstanceName,
		const FVector2D& Position, const FVector2D& Size, int32 ZOrder,
		const FString& Prefix, const FString& Suffix, const FString& Index = FString());
	// Gives a page's hit button its action id (HandleProfileSubscreenAction), which
	// MakeIndexedHitButton set for C++-built buttons.
	void BindIndexedButton(UWidget* Button, int32 ActionId);
	/** Mounts a tooltip item WBP over the mode page, filling it (its click-away layer). */
	URokUiPageWidget* MountTipLayer(FName Item, FName InstanceName);
	// Points a page image at one Unity atlas sprite (icons that follow state or data).
	void SetAtlasSprite(UImage* Image, const TCHAR* TexturePath, const FIntRect& UnityRect,
		const FIntPoint& AtlasSize, const FVector2D& DesiredSize) const;
	// Detached, designer-authored context markers; never enters the visible hierarchy.
	URokUiPageWidget* GetModalContextStates();
	UPROPERTY(Transient)
	TObjectPtr<URokUiPageWidget> ModalContextStateTemplate;
	UPROPERTY(Transient)
	TObjectPtr<URokUiPageWidget> ActivePage;
	FName ActivePageName;
	void UpdateWindowAnimationForMode(bool bOpening);
	void ApplyWindowAnimation();
	void AdvanceWindowAnimation(float DeltaSeconds);
	void BuildType1Window(const FText& Title);
	void BuildType2Window(const FText& Title, bool bSourceBarracksUpgradeParent = false);
	void BuildType3Window(const FText& Title);
	void BuildTypeMidWindow(const FText& Title);
	void BuildPlayerProfile();
	void BuildPlayerHeadSelection();
	void BuildPlayerPowerDetails();
	void BuildPlayerRanking();
	void BuildCivilizationSelection();
	void BuildCivilizationChangeWindow(const FText& Title);
	void BuildCivilizationChangeConfirmation();
	void BuildChangeName();
	void BuildResourceInventory();
	void BuildBuildingInfo();
	void AddBuildingPreviewGlow(UCanvasPanel* Canvas, TOptional<FVector2D> PreviewCenter = {});
	void AddBuildingPreview(UCanvasPanel* Canvas, FName ImageName, int32 Type,
		const FVector2D& PanePosition, const FVector2D& PaneSize, const FVector2D& PreviewCenter, FName LayoutSlot);
	void BuildResourceBuildingInfo(int32 Type, int32 KnownLevel);
	friend class FRokBuildingLevelDetailsTest;
	UFUNCTION()
	void HandleBuildingLevelInfoClicked();
	UFUNCTION()
	void HandleBuildingLevelBackClicked();
	void ApplyBuildingPageAnimation();
	void BuildBuildingUpgrade();
	void BindBuildingUpgradePage(URokUiPageWidget* Page, const FRokBuildingUpgradePreview& Preview,
		bool bIsSourcePreview, bool bHasRuntimePreview, bool bBarracks, bool bBarracksAttributes);
	void RefreshUpgradeButtons();
	void SubmitServerUpgrade(bool bImmediately);
	void ShowUpgradeFeedback(const FText& Text);
	void BuildBarracksLevelDetails(int32 KnownLevel, URokUiPageWidget* Page = nullptr);
	void ApplyBarracksLevelPageAnimation();
	UFUNCTION()
	void HandleBarracksLevelInfoClicked();
	UFUNCTION()
	void HandleBarracksLevelBackClicked();
	void BuildWarWarning();
	void BuildBuffList();
	void BuildGainEffect();
	void RebuildGainEffectContent();
	void BuildBattlePassRewards();
	// Shared list layout for the definition-preview windows: a header line, up to
	// six rows (title + detail + tag) and a footer note.
	struct FRokDefinitionRow
	{
		FString Title;
		FString Detail;
		FString Tag;
		// tlas[sprite] reference; drawn when the sprite manifest has it.
		FString Icon;
	};
	void BuildDefinitionList(
		const FText& Header,
		const TArray<FRokDefinitionRow>& Rows,
		const FText& Footer,
		const FString& WidgetPrefix,
		float RowWidth = 700.0f,
		float RowHeight = 76.0f,
		bool bWrapDetail = false,
		bool bWithSearch = false,
		int32 Columns = 1);
	// Detached Designer geometry for the expedition store search WBP only.
	void ReadExpeditionStoreListLayout();
	// Rebuilds the scroll rows from the stored row data, applying the filter.
	void PopulateDefinitionRows();
	UFUNCTION()
	void HandleDefinitionFilterChanged(const FText& Text);
	// Applies a case-insensitive substring filter to the active list (public so
	// the command line and tests can drive it without synthesising input).
	TArray<FRokDefinitionRow> ActiveListRows;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ActiveListCanvas;
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ActiveListSize;
	FString ActiveListFilter;
	FString ActiveListPrefix;
	float ActiveRowWidth = 700.0f;
	float ActiveRowHeight = 76.0f;
	FVector2D ActiveListRowOrigin = FVector2D::ZeroVector;
	FVector2D ActiveListScrollPadding = FVector2D(12.0, 8.0);
	double ActiveListRowStep = 84.0;
	bool bActiveStoreListLayout = false;
	bool bActiveRowsWrap = false;
	int32 ActiveListColumns = 1;
	int32 ItemCollectionTotal = 0;
	int32 ActiveDefinitionRowCount = 0;
	void BuildEventDate();
	void BuildGuildPurview();
	void BuildChapterDialog();
	void BuildChapterDialogScreen();
	void ShowDialogLine();
	UFUNCTION()
	void AdvanceDialogLine();
	TArray<FRokDialogLine> ActiveDialogLines;
	int32 ActiveDialogIndex = 0;
	void BuildGuildFortress();
	void BuildItemCollection();
	void BuildItemTypeFilterChips();
	UFUNCTION()
	void HandleItemTypeFilterChip(int32 ChipIndex);
	UFUNCTION()
	void HandleItemQualityFilterChip(int32 ChipIndex);
	void ApplyItemFilter();
	int32 ItemCollectionType = 0;
	int32 ItemCollectionQuality = 0;
	void BuildVipStore();
	void BuildTavernBoxDesc();
	void BuildTheWall();
	void BuildTheTower();
	void BuildGuildStore();
	void BuildGiftLimitedTime();
	void BuildChargePop();
	void BuildExpeditionStore();
	void BuildTavernReward();
	void BuildFuncGuide();
	void BuildGuildGift();
	void BuildItemExchange();
	void RefreshExchangeSummary();
	UFUNCTION()
	void HandleExchangePlus();
	UFUNCTION()
	void HandleExchangeMinus();
	TArray<FString> ActiveExchangeEntries;
	int32 ItemExchangeIndex = 0;
	int32 ItemExchangeCount = 1;
	void BuildUiGallery();
	void BuildVipPrivilege();
	UFUNCTION()
	void HandleUiGalleryEntry(int32 EntryIndex);
	int32 ChapterDialogGroup = 0;
	UFUNCTION()
	void HandleGainEffectEconomyTab();
	UFUNCTION()
	void HandleGainEffectMilitaryTab();
	void ResetChangeNameWidgetPointers();
	bool EnsureChangeNameBannedWordsLoaded();
	void UpdateChangeNameValidationState();
	FText GetChangeNameValidationMessage(
		ERokChangeNameValidationResult Result) const;
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);

	UButton* MakePlayerDataButton(const FName& Name, const FIntRect& IconRect, const FText& Label);
	UButton* MakeTextAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FText& Label,
		int32 FontSize,
		const FLinearColor& TextColor,
		const FText& SecondaryLabel = FText::GetEmpty());
	UButton* MakeAtlasButton(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);
	UButton* MakeIndexedHitButton(
		const FName& Name,
		int32 ActionId);
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
	UTextBlock* MakeModalText(
		const FName& Name,
		int32 FontSize,
		const FLinearColor& Color,
		bool bBold = false,
		ETextJustify::Type Justification = ETextJustify::Left);
	void PlaceOnCanvas(
		UCanvasPanel* Parent,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 ZOrder = 0);
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
	void HandleCloseClicked();

	UFUNCTION()
	void HandleResourceTabFood();

	UFUNCTION()
	void HandleResourceTabWood();

	UFUNCTION()
	void HandleResourceTabStone();

	UFUNCTION()
	void HandleResourceTabGold();

	UFUNCTION()
	void HandlePreviewItemAction();

	UFUNCTION()
	void HandleProfileArmyClicked();

	UFUNCTION()
	void HandleProfileAvatarClicked();

	UFUNCTION()
	void HandleProfileRenameClicked();

	UFUNCTION()
	void HandleChangeNameTextChanged(const FText& Text);

	UFUNCTION()
	void HandleChangeNameConfirmClicked();

	UFUNCTION()
	void HandleChangeNameCancelClicked();

	UFUNCTION()
	void HandleProfileCivilizationClicked();

	UFUNCTION()
	void HandleCivilizationContinueClicked();

	UFUNCTION()
	void HandleProfileKillsHelpClicked();

	UFUNCTION()
	void ClosePlayerKillTip();
	UFUNCTION()
	void ClosePlayerPowerActionTip();

	UFUNCTION()
	void HandleProfileActionHelpClicked();

	UFUNCTION()
	void HandleProfileActionAddClicked();

	UFUNCTION()
	void HandleProfileMoreClicked();

	UFUNCTION()
	void HandleProfileRankingClicked();

	UFUNCTION()
	void HandleProfileSettingsClicked();

	void HandleProfileSubscreenAction(int32 ActionId);

	UFUNCTION()
	void HandleStartUpgrade();

	UFUNCTION()
	void HandleInstantUpgrade();

	UFUNCTION()
	void HandleBuffListMoreClicked();

	void ShowProfileBoundaryFeedback(
		const FText& Message,
		const TCHAR* LogAction);
};
