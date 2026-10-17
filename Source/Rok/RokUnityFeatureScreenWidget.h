#pragma once

#include "CoreMinimal.h"
#include "RokTaskRewardPreviewData.h"
#include "RokTaskChapterConfigPreviewData.h"
#include "RokTaskChapterRewardPreviewData.h"
#include "RokExpeditionStagePreviewData.h"
#include "Blueprint/UserWidget.h"
#include "RokUiAnimatedButton.h"
#include "RokUiWindowAnimation.h"
#include "RokChatAnimation.h"
#include "RokResourceSearchAnimation.h"
#include "RokMailAttachmentPulse.h"
#include "RokMailRewardDetails.h"
#include "RokMailLocalUnreadState.h"
#include "RokSpeedUpQueueLifecycle.h"
#include "RokAllianceFlagLocalPreviewData.h"
#include "Styling/SlateBrush.h"
#include "Types/SlateEnums.h"
#include "Templates/Function.h"
#include "RokUnityFeatureScreenWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class UProgressBar;
class USlider;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class AActor;
class URokClientSettingsSubsystem;
class URokCommanderMeshWidget;
class URokTrainArmyMeshWidget;
class URokHelpWidget;
class URokHospitalExplanationWidget;
class URokResearchScreenWidget;
class URokMailAttachmentMeshWidget;
class URokUnityEconomyCatalogSubsystem;
class URokUnityGameplayCatalogSubsystem;
class URokUnityRuntimeStateSubsystem;
class UWorld;
struct FRokUnityRuntimeTaskState;
struct FRokUnityRuntimePlayerProfileState;

DECLARE_MULTICAST_DELEGATE_OneParam(FRokUnityIndexedButtonClicked, int32);
DECLARE_MULTICAST_DELEGATE_OneParam(FRokBuildCityPlacementRequested, int32);
DECLARE_DELEGATE_RetVal_OneParam(bool, FRokTaskGoTownHallRequested, AActor*);

/** Absorbs pointer input only inside the drawer; never takes keyboard focus. */
UCLASS()
class URokChatInputShield : public UButton
{
	GENERATED_BODY()
public:
	URokChatInputShield() { InitIsFocusable(false); }
};

/** Small runtime-only button that routes one integer action without a forest
 * of one-off UFUNCTION handlers on the owning screen. */
UCLASS()
class ROK_API URokUnityIndexedButton : public URokUiAnimatedButton
{
	GENERATED_BODY()

public:
	void ConfigureAction(int32 InActionId);
	FRokUnityIndexedButtonClicked OnIndexedClicked;

private:
	UFUNCTION()
	void HandleButtonClicked();

	friend class URokCommanderSkillReadonlyDetailAuditSubsystem;
	friend class URokDailyTaskReadonlyCatalogAuditSubsystem;
	friend class URokVipDailyPointHelpTipAuditSubsystem;
	friend class URokActivityCalendarReadonlyAuditSubsystem;
	friend class URokAllianceOfficeReadonlyAuditSubsystem;
	friend class URokAllianceTechnologyReadonlyAuditSubsystem;
	friend class URokAllianceFixtureSearchAuditSubsystem;
	friend class URokAllianceFlagLocalPreviewAuditSubsystem;
	friend class URokAllianceGiftReadonlyAuditSubsystem;
	friend class URokAllianceShopReadonlyAuditSubsystem;
	friend class URokAllianceTerritoryReadonlyAuditSubsystem;
	friend class URokAllianceRankingReadonlyAuditSubsystem;
	friend class URokAllianceUiWindowAnimationAuditSubsystem;
	friend class URokTaskCompletedRowAnimationAuditSubsystem;
	int32 ActionId = 0;
	bool bClickHandlerBound = false;
};

UENUM()
enum class ERokUnityFeatureScreenMode : uint8
{
	None,
	ActivityCenter,
	Mail,
	Tasks,
	Commander,
	Items,
	Alliance,
	Vip,
	Chat,
	WorldPositionSearch,
	MapMarkers,
	ResourceSearch,
	Expedition,
	GemShop,
	BuildCity,
	WorkerQueue,
	SettingsHub,
	GeneralSettings,
	NotificationSettings,
	LanguageSettings,
	AccountSettings,
	ArmyStatistics,
	Store,
	TrainArmy,
	SpeedUp,
	Hospital,
	CityManager,
	ArmyShow,
	Charge,
	Help,
	Research,
};

/** Per-row quest screen state. Explicit state replaces the old "row 0 is the
 * main task" claim inference; rows without authoritative server task state
 * stay NotSynced instead of fabricating account progress. */
struct FRokTaskRowState
{
	enum class EPage : uint8 { MainTask, SideTask };
	enum class EState : uint8 { Unfinished, Finished, Received, NotSynced };
	enum class EIntent : uint8 { Go, Claim, Inspect };

	int32 TaskId = 0;
	EPage Page = EPage::MainTask;
	FName SourceAuthority; // runtime_bootstrap | local_fixture | server
	EState State = EState::NotSynced;
	EIntent Intent = EIntent::Inspect;
	int32 Type = 0;
	int32 Param1 = 0;
	int32 Param2 = 0;
	int32 Require = 0;
};

/**
 * Runtime C++ reconstruction of the Unity full-screen activity calendar,
 * mail, and quest windows. Presentation-state previews remain local while
 * authoritative commands stay behind explicit future subsystem boundaries.
 */
UCLASS()
class ROK_API URokUnityFeatureScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenActivityCenter();
	void OpenMail(bool bUseReferenceCaptureState = false);
	/** Explicit synthetic mail interaction fixture; state survives closing this widget instance. */
	void OpenMailLocalUnreadFixture();
	/** Explicit local, read-only selected-mail reward preview; never account mail. */
	void OpenMailReadOnlyRewardFixture(const TMap<int32, FRokMailRewardPayload>& PayloadByMailIndex, int32 InitialMailIndex = 0);
	void OpenTasks(bool bUseTestFixtureState = false);
#if !UE_BUILD_SHIPPING
	int32 GetTaskCompletedAnimationFixtureCallbacksForAudit() const { return TaskCompletedAnimationFixtureCallbacks; }
	float GetTaskCompletedAnimationElapsedForAudit() const { return TaskCompletedAnimationElapsed; }
#endif
	/** Task 200900 only: resolve the existing local Hall, then ask the owning HUD to open its info panel. */
	FRokTaskGoTownHallRequested OnTaskGoTownHallRequested;
	static bool TryResolveLocalTask200900GoTarget(
		const FRokTaskRowState& Row, const FRokUnityRuntimeTaskState& Task,
		const FRokUnityRuntimePlayerProfileState& Profile, bool bRuntimeLoaded,
		UWorld* World, AActor*& OutActor, FName& OutFailure);
	void OpenCommander();
	void OpenItems(bool bUseTestFixtureState = false);
	void OpenAlliance(bool bOpenJoinedPreview = false);
	void OpenVip(bool bUseTestFixtureState = false);
	void OpenChat(bool bUseTestFixtureState = false);
	void OpenWorldPositionSearch();
	void OpenMapMarkers();
	void OpenResourceSearch();
	void OpenExpedition();
#if !UE_BUILD_SHIPPING
	bool IsExpeditionModeForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Expedition; }
	int32 GetSelectedExpeditionStageIdForAudit() const { return SelectedExpeditionStageId; }
	bool IsExpeditionPreviewForAudit() const { return bExpeditionPreview; }
	bool IsExpeditionTaskConfigVisibleForAudit() const { return bExpeditionTaskConfigVisible; }
	FName GetExpeditionAnimationPhaseForAudit() const { return ExpeditionAnimationPhase; }
	float GetExpeditionAnimationElapsedForAudit() const { return ExpeditionAnimationElapsed; }
	int32 GetExpeditionLastBoundaryActionIdForAudit() const { return ExpeditionLastBoundaryActionId; }
	int32 GetExpeditionRenderedStageCountForAudit() const { return ExpeditionRows.Num(); }
	bool IsExpeditionAccountStateKnownForAudit() const { return false; }
#endif
	void OpenGemShop();
	void OpenCharge(int32 InitialPageType = 107);
	void OpenBuildCity();
	/** Card-to-city handoff; a listener owns transient placement and final validation. */
	FRokBuildCityPlacementRequested OnBuildCityPlacementRequested;
	void OpenWorkerQueue(AActor* BuildingActor = nullptr);
	void OpenSettingsHub();
	void OpenGeneralSettings();
	void OpenNotificationSettings();
	void OpenLanguageSettings();
	void OpenHelp();
	int32 GetLanguageDraftIdForAudit() const { return LanguageDraftId; }
	void OpenAccountSettings();
	void OpenArmyStatistics();
	void OpenStore();
	void OpenTrainArmy(AActor* BuildingActor = nullptr, int32 ArmyType = 1);
	void OpenSpeedUp(AActor* QueueActor = nullptr, int32 QueueType = 20102);
	void OpenHospital(AActor* BuildingActor = nullptr);
	/** Read-only source technology tree; null actor is an explicit reference preview. */
	void OpenResearch(AActor* AcademyActor = nullptr);
	bool IsHospitalExplanationVisibleForAudit() const { return bHospitalExplanationVisible; }
	URokHospitalExplanationWidget* GetHospitalExplanationForAudit() const { return HospitalExplanationWidget; }
	void OpenCityManager();
	void OpenArmyShow();
	void CloseScreen();
	/** User dismissal; CloseScreen remains the synchronous routing API. */
	void RequestCloseScreen();
	bool IsScreenOpen() const { return ActiveMode != ERokUnityFeatureScreenMode::None; }
#if !UE_BUILD_SHIPPING
	void SampleBuildCityAnimationForCapture(float ElapsedSeconds);
	bool IsBuildCityModeForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::BuildCity; }
	int32 GetSelectedBuildCityTypeIdForAudit() const { return SelectedBuildCityTypeId; }
	bool IsMailModeForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Mail; }
	bool IsResearchModeForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Research; }
	/** Read-only clock advanced by this widget's NativeTick; -1 once MiddleTip naturally collapses. */
	float GetWorldPositionMiddleTipElapsedSecondsForAudit() const { return WorldPositionMiddleTipElapsedSeconds; }
	bool IsResearchMainMaskClosingForAudit() const { return bResearchMainMaskClosing; }
	float GetResearchMainMaskCloseElapsedSecondsForAudit() const { return bResearchMainMaskClosing ? WindowAnimation.GetElapsedSeconds() : 0.0f; }
	FName GetResearchMainMaskPhaseForAudit() const
	{ return bResearchMainMaskClosing ? FName(TEXT("Closing")) :
		(ActiveMode == ERokUnityFeatureScreenMode::Research
			? FName(TEXT("Open")) : FName(TEXT("Closed"))); }
	FRokUiWindowAnimationSample GetResearchMainMaskSampleForAudit() const
	{ return bResearchMainMaskClosing ? FRokUiWindowAnimation::Evaluate(
		ERokUiWindowAnimationStyle::Type1, true, WindowAnimation.GetElapsedSeconds())
		: WindowAnimation.GetSample(); }
	int32 GetPendingActionIdForAudit() const { return PendingActionId; }
	URokResearchScreenWidget* GetResearchScreenForAudit() const { return ResearchScreenWidget; }
	bool IsMailReadOnlyRewardFixtureForAudit() const { return bReadOnlyMailRewardFixtureState; }
	bool IsMailReferenceCaptureForAudit() const { return bReferenceMailCaptureState && !bReadOnlyMailRewardFixtureState; }
	int32 GetSelectedMailTabForAudit() const { return SelectedMailTab; }
	int32 GetSelectedMailIndexForAudit() const { return SelectedMailIndex; }
	bool IsMailLocalUnreadFixtureForAudit() const { return bLocalMailUnreadFixtureState; }
	int32 GetMailLocalUnreadCountForAudit(int32 Tab) const { return MailLocalUnreadState.UnreadCount(Tab); }
	bool IsMailLocalFixtureRowReadForAudit(int32 Index) const { const auto* Row = MailLocalUnreadState.Find(Index); return Row && Row->bRead; }
	const TArray<FRokMailRewardRow>& GetRenderedMailRewardRowsForAudit() const { return RenderedMailRewardRows; }
	int32 GetActiveMailRewardTooltipIndexForAudit() const { return ActiveMailRewardTooltipIndex; }
	FVector2D GetMailRewardTooltipAnchorForAudit() const { return MailRewardTooltipAnchor; }
	int32 GetLastMailBoundaryActionIdForAudit() const { return LastMailBoundaryActionId; }
	FString GetVisibleItemsInventoryFingerprintForAudit() const;
	FString GetMailRewardFixtureFingerprintForAudit() const;
	int32 GetSelectedTrainTierForAudit() const { return SelectedTrainTier; }
	int32 GetSelectedTrainArmyTypeForAudit() const { return SelectedTrainArmyType; }
	int32 GetSelectedTrainCountForAudit() const { return SelectedTrainCount; }
	AActor* GetFeatureContextActorForAudit() const;
	bool IsTaskDetailLocalProjectionForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 1 && bTaskMainLocalProjectionRendered; }
	bool IsTaskDetailLocalConditionMetForAudit() const { return IsTaskDetailLocalProjectionForAudit() && bTaskMainLocalConditionMet; }
	int32 GetTaskDetailDisplayedCurrentForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 1 ? TaskMainDisplayedCurrent : 0; }
	int32 GetSelectedTaskPageForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Tasks ? SelectedTaskPage : INDEX_NONE; }
	int32 GetSelectedTaskChapterGroupIdForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0 ? SelectedTaskChapterGroupId : INDEX_NONE; }
	bool IsSelectedTaskChapterMetadataKnownForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0 && bSelectedTaskChapterMetadataKnown; }
	const TArray<int32>& GetRenderedTaskChapterTaskIdsForAudit() const
	{
		static const TArray<int32> Empty;
		return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0
			? RenderedTaskChapterTaskIds : Empty;
	}
	const TArray<int32>& GetRenderedTaskChapterTaskRewardPackageIdsForAudit() const
	{
		static const TArray<int32> Empty;
		return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0
			? RenderedTaskChapterTaskRewardPackageIds : Empty;
	}
	const TArray<int32>& GetRenderedTaskChapterAggregateRewardPackageIdsForAudit() const
	{
		static const TArray<int32> Empty;
		return ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0
			? RenderedTaskChapterAggregateRewardPackageIds : Empty;
	}
	int32 GetTaskChapterSourceTaskCountForAudit() const { return TaskChapterTasks.Num(); }
	int32 GetTaskChapterSourceGroupCountForAudit() const { return TaskChapterGroups.Num(); }
#endif
#if WITH_DEV_AUTOMATION_TESTS
	void FlushPendingActionForTesting();
	int32 GetSelectedCommanderIdForAudit() const { return SelectedCommanderId; }
	int32 GetSelectedCommanderTalentIdForAudit() const { return VisibleCommanderTalentIds.IsValidIndex(SelectedCommanderTalentIndex) ? VisibleCommanderTalentIds[SelectedCommanderTalentIndex] : INDEX_NONE; }
	int32 GetSelectedCommanderSkillDetailIdForAudit() const { return VisibleCommanderSkillIds.IsValidIndex(SelectedCommanderSkillDetailIndex) ? VisibleCommanderSkillIds[SelectedCommanderSkillDetailIndex] : INDEX_NONE; }
	void AdvanceWindowAnimationForTesting(float DeltaSeconds) { AdvanceWindowAnimation(DeltaSeconds); }
	void AdvanceArmyShowPanelAnimationForTesting(float DeltaSeconds) { AdvanceArmyShowPanelAnimation(DeltaSeconds); }
	void AdvanceTrainArmyDetailAnimationForTesting(float DeltaSeconds) { AdvanceTrainArmyDetailAnimation(DeltaSeconds); }
	void RebuildArmyShowPresentationForTesting() { if (ActiveMode == ERokUnityFeatureScreenMode::ArmyShow) RebuildModeContent(); }
	void AdvanceCommanderRosterAnimationForTesting(float DeltaSeconds) { AdvanceCommanderRosterAnimation(DeltaSeconds); }
	void RefreshTaskDetailProjectionForTesting() { RefreshTaskMainProjection(); }
	const FRokUiWindowAnimation& GetWindowAnimationForTesting() const { return WindowAnimation; }
	const FRokChatDrawerAnimation& GetChatAnimationForTesting() const { return ChatDrawerAnimation; }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	friend class URokVipDailyPointHelpTipAuditSubsystem;
	friend class URokActivityCalendarReadonlyAuditSubsystem;
	friend class URokAllianceOfficeReadonlyAuditSubsystem;
	friend class URokAllianceTechnologyReadonlyAuditSubsystem;
	friend class URokAllianceFixtureSearchAuditSubsystem;
	friend class URokAllianceFlagLocalPreviewAuditSubsystem;
	friend class URokAllianceGiftReadonlyAuditSubsystem;
	friend class URokAllianceShopReadonlyAuditSubsystem;
	friend class URokAllianceTerritoryReadonlyAuditSubsystem;
	friend class URokAllianceRankingReadonlyAuditSubsystem;
	friend class URokAllianceUiWindowAnimationAuditSubsystem;
	friend class URokTaskCompletedRowAnimationAuditSubsystem;
	UPROPERTY(Transient) TObjectPtr<URokHelpWidget> HelpWidget;
	FRokMailAttachmentPulse MailAttachmentPulse;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<URokMailAttachmentMeshWidget>> MailAttachmentMeshes;
	FRokSpeedUpQueueLifecycle SpeedUpQueueLifecycle;
	struct FItemsInventoryStackState
	{
		int64 ItemIndex = 0;
		int32 ItemId = 0;
		int64 Overlay = 0;
	};

	UPROPERTY()
	TObjectPtr<USizeBox> ExpandedDesignSizeBox;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ExpandedDesignCanvas;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ModeCanvas;

	/** Unity pl_content; contact folding is independent of root entry/exit. */
	UPROPERTY()
	TObjectPtr<UCanvasPanel> ChatContentCanvas;

	UPROPERTY()
	TObjectPtr<UTextBlock> FeedbackText;

	// Only empty WorldPositionSearch coordinates use this source Middle tip.
	UPROPERTY()
	TObjectPtr<UCanvasPanel> WorldPositionMiddleTipRoot;
	UPROPERTY()
	TObjectPtr<UImage> WorldPositionMiddleTipBackground;
	UPROPERTY()
	TObjectPtr<UTextBlock> WorldPositionMiddleTipText;
	float WorldPositionMiddleTipElapsedSeconds = -1.0f;

	UPROPERTY()
	TObjectPtr<UTextBlock> ActivityCountdownText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeedUpQueueDescriptionText;

	UPROPERTY()
	TObjectPtr<UProgressBar> SpeedUpQueueProgress;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeedUpQueueProgressText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueuePrimaryTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueueRuntimeStateText;

	UPROPERTY()
	TObjectPtr<UProgressBar> WorkerQueuePrimaryProgress;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueuePrimaryProgressText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GeneralSettingsGraphicsValueText;

	UPROPERTY()
	TObjectPtr<UTextBlock> GeneralSettingsFrameRateValueText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> ChatInputTextBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> WorldPositionXInput;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> WorldPositionYInput;

	UPROPERTY()
	TObjectPtr<USlider> ResourceSearchSlider;

	UPROPERTY()
	TObjectPtr<UProgressBar> ResourceSearchSliderFill;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResourceSearchLevelValueText;

	ERokUnityFeatureScreenMode ActiveMode = ERokUnityFeatureScreenMode::None;
	ERokUnityFeatureScreenMode AnimatedMode = ERokUnityFeatureScreenMode::None;
	FRokUiWindowAnimation WindowAnimation;
	// The five source-backed alliance prefab roots share UA_WinAnime_T1.
	enum class EAllianceAnimatedRoot : uint8 { None, Main, Join, Flag, Gift, Shop };
	EAllianceAnimatedRoot AllianceAnimatedRoot = EAllianceAnimatedRoot::None;
	int32 PendingAllianceRootCloseActionId = INDEX_NONE;
	bool bDispatchingAllianceRootCloseAction = false;
#if WITH_DEV_AUTOMATION_TESTS
	uint64 AllianceRootCloseDispatchCountForAudit = 0;
	int32 AllianceRootCloseLastActionForAudit = INDEX_NONE;
#endif
	bool bResearchMainMaskClosing = false;
	FRokResourceSearchAnimation ResourceSearchAnimation;
	float BuildCityEntranceTime = 1.0f / 3.0f;
	float BuildCityCardAnimationTime = 10;
	bool bBuildCityInitialScrollPending = false;
#if !UE_BUILD_SHIPPING
	bool bFreezeBuildCityAnimationForCapture = false;
#endif
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanel>> BuildCityAnimatedCards;
	TArray<int32> BuildCityCardSourceIndices;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BuildCityAnimatedRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ResourceSearchDockRoot;
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> ResourceSearchPopupRoot;
	FRokChatDrawerAnimation ChatDrawerAnimation;
	FRokChatContactAnimation ChatContactAnimation;
	float CommanderRosterAnimationTime = 0.0f;
	bool bCommanderRosterFirstTick = false;
	TArray<TWeakObjectPtr<UWidget>> CommanderRosterAnimationWidgets;
	TArray<TWeakObjectPtr<UWidget>> CommanderDataAnimationWidgets;
	void AdvanceCommanderRosterAnimation(float DeltaSeconds);
	FVector2D CurrentExpandedDesignSize = FVector2D(1440.0f, 900.0f);
	int32 SelectedActivityIndex = 1;
	int32 SelectedActivityCalendarId = 0;
	float ActivityCalendarScrollOffset = 0.0f;
	int32 SelectedMailTab = 0;
	int32 SelectedMailIndex = 0;
	bool bReferenceMailCaptureState = false;
	bool bReadOnlyMailRewardFixtureState = false;
	bool bLocalMailUnreadFixtureState = false;
	FRokMailLocalUnreadState MailLocalUnreadState;
	TMap<int32, FRokMailRewardPayload> MailRewardFixturePayloadByIndex;
	TArray<FRokMailRewardRow> RenderedMailRewardRows;
	int32 ActiveMailRewardTooltipIndex = INDEX_NONE;
	FVector2D MailRewardTooltipAnchor = FVector2D::ZeroVector;
	int32 LastMailBoundaryActionId = INDEX_NONE;
	FString MailRewardFixtureError;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> MailRewardTooltipRoot;
	int32 SelectedTaskPage = 1;
	int32 SelectedTaskChapterGroupId = 1;
	bool bSelectedTaskChapterMetadataKnown = false;
	TArray<FRokTaskChapterPreviewRow> TaskChapterGroups;
	TArray<FRokTaskChapterTaskPreviewRow> TaskChapterTasks;
	TArray<int32> RenderedTaskChapterTaskIds;
	TArray<FRokTaskChapterRewardGroup> TaskChapterRewardGroups;
	TArray<int32> RenderedTaskChapterTaskRewardPackageIds;
	TArray<int32> RenderedTaskChapterAggregateRewardPackageIds;
	FString TaskChapterRewardLoadError;
	FString TaskChapterLoadError;
	float TaskProjectionRefreshAccumulator = 0.0f;
	bool bTaskMainLocalProjectionRendered = false;
	bool bTaskMainLocalConditionMet = false;
	int32 TaskMainDisplayedCurrent = 0;
	int32 SelectedCommanderId = 2007;
	UPROPERTY(Transient)
	TObjectPtr<URokCommanderMeshWidget> CommanderAnimatedPortrait;
	int32 CommanderAnimatedPortraitHeroId = INDEX_NONE;
	int32 SelectedCommanderSkillIndex = 0;
	int32 SelectedCommanderSkillDetailIndex = INDEX_NONE;
	int32 SelectedCommanderTalentIndex = INDEX_NONE;
	TArray<int32> VisibleCommanderTalentIds;
	int32 SelectedCommanderSortType = 0;
	bool bCommanderSortMenuOpen = false;
	int32 SelectedItemsCategory = 0;
	int32 SelectedItemsIndex = 0;
	int32 SelectedItemsUseCount = 1;
	int32 AlliancePreviewState = 0;
	RokAllianceFlagLocalPreview::FDraft AllianceCreateFlagDraft;
	RokAllianceFlagLocalPreview::FDraft AllianceFlagEditDraft;
	bool bAllianceFlagEditorOpen = false;
	int32 SelectedAllianceFlagType = 1;
	FString AllianceCreateNameDraft;
	FString AllianceCreateTagDraft;
	FString AllianceCreateNoticeDraft;
	bool bAllianceTestFixtureState = false;
	int32 SelectedAllianceListIndex = 0;
	FString AllianceFixtureSearchInput;
	FString AllianceFixtureSearchQuery;
	TArray<int32> VisibleAllianceFixtureIndices;
	int32 SelectedAlliancePage = 0;
	int32 SelectedAllianceOfficeIndex = 0;
	bool bAllianceTerritoryReferenceOpen = false;
	int32 ExpandedAllianceTerritoryGroup = INDEX_NONE;
	int32 SelectedAllianceTerritoryType = 0;
	bool bAllianceRankingReferenceOpen = false;
	int32 SelectedAllianceRankingCategoryId = INDEX_NONE;
	bool bAllianceShopReferenceOpen = false;
	int32 SelectedAllianceShopGroup = 1;
	int32 SelectedAllianceShopItemType = 0;
	bool bAllianceGiftReferenceOpen = false;
	int32 SelectedAllianceGiftConfiguredLevel = 1;
	bool bAllianceGiftSpecialDefinitions = false;
	bool bAllianceOfficeReferenceOpen = false;
	bool bAllianceTechnologyReferenceOpen = false;
	int32 SelectedAllianceTechnologyCategory = 1;
	int32 SelectedAllianceTechnologyStudyType = 0;
	int32 SelectedAllianceTechnologyLevelKey = 1;
	int32 SelectedVipPreviewLevel = 0;
	bool bVipTestFixtureState = false;
	bool bVipDailyPointHelpVisible = false;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> VipDailyPointHelpTipRoot;
	int32 SelectedChatChannel = 0;
	bool bChatTestFixtureState = false;
	bool bChatEmojiPanelOpen = false;
	int32 SelectedMapMarkerPage = 0;
	int32 SelectedMapMarkerIndex = 0;
	int32 SearchCoordinateX = 1146;
	int32 SearchCoordinateY = 157;
	FString ChatDraft;
	float ChatMessageScrollOffset = 0.0f;
	int32 SelectedResourceSearchType = 0;
	int32 ResourceSearchBarbarianLevel = 1;
	int32 ResourceSearchResourceLevel = 1;
	int32 SelectedExpeditionStageId = 0;
	TArray<FRokExpeditionStagePreviewRow> ExpeditionRows;
	FString ExpeditionLoadError;
	bool bExpeditionPreview = false;
	bool bExpeditionTaskConfigVisible = false;
	FName ExpeditionAnimationPhase = NAME_None;
	float ExpeditionAnimationElapsed = 0.0f;
	int32 ExpeditionLastBoundaryActionId = INDEX_NONE;
	TWeakObjectPtr<UCanvasPanel> ExpeditionPreviewRoot;
	TWeakObjectPtr<UCanvasPanel> ExpeditionSideRoot;
	TWeakObjectPtr<UCanvasPanel> ExpeditionCenterRoot;
	TWeakObjectPtr<UCanvasPanel> ExpeditionCenterImg;
	TWeakObjectPtr<UCanvasPanel> ExpeditionCenterFlagMes;
	int32 SelectedChargePageType = 107;
	int32 SelectedBuildCityCategory = 0;
	int32 SelectedBuildCityTypeId = 0;
	int32 SelectedArmyStatisticsPage = 0;
	int32 SelectedStoreCategory = 0;
	int32 SelectedStorePage = 0;
	int32 SelectedStoreItemIndex = 0;
	int32 SelectedStoreQuantity = 1;
	int32 SelectedTrainArmyType = 1;
	int32 SelectedTrainTier = 1;
	int32 SelectedTrainCount = 1;
	int32 TrainArmyBaseCapacity = 0;
	bool bTrainArmyQuantitySync = false;
	bool bTrainArmyQuantityEditable = false;
	double TrainArmySliderSoundTime = -1.0;
	bool bTrainArmyDetailVisible = false;
	bool bTrainArmyDetailFirstTick = false;
	float TrainArmyDetailAnimationTime = 0.25f;
	TArray<TWeakObjectPtr<UWidget>> TrainArmyAnimatedChildren;
	UPROPERTY(Transient) TObjectPtr<URokTrainArmyMeshWidget> TrainArmyAnimatedModel;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> TrainArmyTipLayer;
	int32 SelectedSpeedUpPage = 0;
	int32 SelectedSpeedUpItemIndex = 0;
	int32 SelectedCityManagerPage = 0;
	int32 SelectedCityManagerGroupId = 0;
	bool bArmyShowRulesVisible = false;
	int32 SpeedUpQueueType = 20102;
	TWeakObjectPtr<AActor> FeatureContextActor;
	TArray<int32> VisibleBuildCityTypeIds;
	TSet<int32> BuildableBuildCityTypeIds;
	TArray<FName> VisiblePersonalityToggleNames;
	TArray<int32> VisibleCommanderIds;
	TArray<int32> VisibleCommanderSkillIds;
	TArray<FRokTaskRowState> VisibleTaskStates;
	TArray<FItemsInventoryStackState> VisibleItemsStacks;
	TArray<int32> VisibleStoreItemIds;
	TArray<int32> VisibleSpeedUpItemIds;
	bool bTaskTestFixtureState = false;
	/** Only the explicitly tagged local task reference page exposes the preview button. */
	TWeakObjectPtr<UCanvasPanel> TaskCompletedAnimatedRow;
	TArray<TWeakObjectPtr<UWidget>> TaskCompletedAnimatedGraphics;
	TArray<float> TaskCompletedOriginalOpacities;
	TFunction<void()> TaskCompletedAnimationCallback;
	float TaskCompletedAnimationElapsed = -1.0f;
	double TaskCompletedAnimationStartedAtSeconds = 0.0;
	int32 TaskCompletedAnimationFixtureCallbacks = 0;
	bool bItemsTestFixtureState = false;
	int32 PendingActionId = INDEX_NONE;
	double ActivityPreviewSecondsRemaining = 91824.0;
	int32 LastRenderedActivitySecond = INDEX_NONE;
	float SpeedUpRefreshAccumulator = 0.0f;
	float WorkerQueueRefreshAccumulator = 0.0f;
	bool bWorkerQueueRenderedBusy = false;

	void BuildWidgetTree();
	void RebuildModeContent(bool bOpening = false);
	void UpdateWindowAnimationForMode(bool bOpening);
	void ApplyWindowAnimation();
	void AdvanceWindowAnimation(float DeltaSeconds);
	EAllianceAnimatedRoot ResolveAllianceAnimatedRoot() const;
	bool TryStartAllianceRootClose(int32 ActionId);
	UFUNCTION()
	void HandleResearchMainMaskClicked();
	void RequestResearchMainMaskClose();
	void BuildActivityCenter();
	void BuildActivityCalendarReadonly();
	void BuildMail();
	void BuildTasks();
	bool BeginTaskCompletedRowAnimation(UCanvasPanel* Row, TFunction<void()> OnComplete);
	void AdvanceTaskCompletedRowAnimation();
	void ResetTaskCompletedRowAnimation();
	void BuildTaskChapter();
	void RefreshTaskMainProjection();
	void BuildCommander();
	void BuildItems();
	void BuildAlliance();
	void BuildAllianceWelcome();
	void BuildAllianceJoinList();
	void BuildAllianceCreate();
	void BuildAllianceFlagEditor();
	void CaptureAllianceCreateFormDraft();
	void ResetAllianceCreateLocalDraft();
	void BuildAllianceJoined();
	void BuildAllianceTerritoryReference();
	void BuildAllianceRankingReference();
	void BuildAllianceShopReference();
	void BuildAllianceGiftReference();
	void BuildAllianceOfficeReference();
	void BuildAllianceTechnologyReference();
	void BuildVip();
	void BuildVipDailyPointHelpTip();
	void BuildChat();
	void BuildWorldPositionSearch();
	void BuildMapMarkers();
	void BuildResourceSearch();
	void BuildExpedition();
	void AdvanceExpeditionAnimation(float DeltaSeconds);
	void BuildGemShop();
	void BuildCharge();
	void BuildBuildCity();
	void BuildWorkerQueue();
	void BuildSettingsHub();
	void BuildGeneralSettings();
	void BuildGraphicsSettingsCard(UCanvasPanel* Content, float GraphicsValue, float FrameValue, bool bEnabled);
	void UpdateGraphicsSettingsSlider(FName Prefix, int32 Level);
	void BuildNotificationSettings();
	void BuildLanguageSettings();
	void BuildHelp();
	void UpdateLanguageDraftSelection();
	int32 LanguageDraftId = 40;
	void BuildAccountSettings();
	void BuildArmyStatistics();
	void BuildStore();
	void BuildTrainArmy();
	void BuildTrainArmyQuantityControls();
	void BuildTrainResourceCost();
	void UpdateTrainResourceCost();
	void BuildTrainArmySharedDisplay(int32 TroopId);
	void ApplyTrainParameterLayout(int32 TroopId);
	void UpdateTrainArmyQuantity(int32 Count, float SliderPosition = -1.0f);
	UFUNCTION() void HandleTrainArmyQuantityTextChanged(const FText& Text);
	UFUNCTION() void HandleTrainArmyQuantitySliderChanged(float Value);
	void BuildTrainArmyDetail(int32 TroopId);
	void ShowTrainArmyDetailTip(int32 Index, bool bSkill);
	void AdvanceTrainArmyDetailAnimation(float DeltaSeconds);
	void BuildSpeedUp();
	void BuildHospital();
	void BuildResearch();
	void HandleResearchDetailVisibilityChanged(bool bVisible);
	void SetHospitalExplanationVisible(bool bVisible);
	bool bHospitalExplanationVisible = false;
	UPROPERTY(Transient) TObjectPtr<URokHospitalExplanationWidget> HospitalExplanationWidget;
	UPROPERTY(Transient) TObjectPtr<URokResearchScreenWidget> ResearchScreenWidget;
	TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> HospitalHiddenControls;
	void BuildCityManager();
	void BuildArmyShow();
	void AdvanceArmyShowPanelAnimation(float DeltaSeconds);
	float ArmyShowPanelAnimationTime = 0.25f;
	TArray<TWeakObjectPtr<UWidget>> ArmyShowPanelAnimationWidgets;
	void BuildSplitWindowFrame(
		const FVector2D& Position,
		const FVector2D& Size,
		const FText& Title,
		const FVector2D& ClosePosition);
	void BuildActivityFrame();
	void BuildFeedback();
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);
	void UpdateActivityCountdown(float DeltaSeconds);
	void UpdateSpeedUpQueuePresentation();
	void UpdateWorkerQueuePresentation();
	void HandleIndexedAction(int32 ActionId);
	void ExecuteIndexedAction(int32 ActionId);
	void ShowBoundaryFeedback(const FText& Message);
	void ShowWorldPositionMiddleTip(const FText& Message);
	void AdvanceWorldPositionMiddleTip(float DeltaSeconds);
	URokClientSettingsSubsystem* GetClientSettingsSubsystem() const;
	URokUnityEconomyCatalogSubsystem* GetEconomyCatalogSubsystem() const;
	URokUnityGameplayCatalogSubsystem* GetGameplayCatalogSubsystem() const;
	URokUnityRuntimeStateSubsystem* GetRuntimeStateSubsystem() const;

	UFUNCTION()
	void HandleChatInputCommitted(
		const FText& Text,
		ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleWorldPositionXCommitted(
		const FText& Text,
		ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleWorldPositionYCommitted(
		const FText& Text,
		ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleGraphicsQualitySliderChanged(float Value);

	UFUNCTION()
	void HandleFrameRateSliderChanged(float Value);

	UFUNCTION()
	void HandleMusicVolumeSliderChanged(float Value);

	UFUNCTION()
	void HandleSfxVolumeSliderChanged(float Value);

	void UpdateGeneralSettingsAudioFill(const FName& Prefix, float Value);
	void PlaySourceScrollFeedback();

	UFUNCTION()
	void HandleResourceSearchSliderChanged(float Value);

	UCanvasPanel* MakeActivityMenuRow(int32 Index);
	UCanvasPanel* MakeMailRow(int32 Index, const FRokMailRewardTemplate* MailTemplate);
	void BuildMailRewards(UCanvasPanel* DetailContent);
	void BuildMailRewardTooltip();
	void ShowMailRewardTooltip(int32 RewardIndex);
	bool ResolveMailRewardIcon(const FRokMailRewardRow& RewardRow, const TCHAR*& OutAtlasPath, FIntRect& OutRect, FIntPoint& OutAtlasSize) const;
	UCanvasPanel* MakeCommanderHead(
		int32 Index,
		const FIntRect& HeadRect,
		bool bRecruited,
		bool bSelected,
		const FText& DisplayName,
		const FText& StateText,
		int32 Rare);
	UCanvasPanel* MakeTaskRow(
		int32 RowIndex,
		int32 TaskId,
		const FText& Title,
		const FText& Description,
		int32 Current,
		int32 Target,
		bool bProgressKnown,
		bool bComplete,
		const FIntRect& TaskIconRect,
		int32 RewardGroupId,
		const TArray<FRokTaskRewardPreviewRow>& RewardPreviewRows);
	UCanvasPanel* MakeRewardCell(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& IconRect,
		const FIntPoint& AtlasSize,
		const FText& CountText,
		const FText& TopText = FText::GetEmpty(),
		int32 QualityStyle = 1,
		const FVector2D& DesiredSize = FVector2D(82.0f));
	URokUnityIndexedButton* MakeActionButton(
		const FName& Name,
		int32 ActionId,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FText& Label = FText::GetEmpty(),
		int32 FontSize = 20,
		const FLinearColor& TextColor = FLinearColor::White,
		ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& BrushMargin = FMargin(0.0f));
	UImage* MakeAtlasImage(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize);
	UCanvasPanel* MakeAtlasBorder(
		const FName& Name,
		const TCHAR* TexturePath,
		const FIntRect& UnityRect,
		const FIntPoint& AtlasSize,
		const FVector2D& DesiredSize,
		const FMargin& BrushMargin);
	UBorder* MakeSolidBorder(const FName& Name, const FLinearColor& Color);
	UTextBlock* MakeScreenText(
		const FName& Name,
		int32 FontSize,
		const FLinearColor& Color,
		bool bBold = false,
		ETextJustify::Type Justification = ETextJustify::Left);
	USizeBox* WrapFixedSize(UWidget* Content, const FName& Name, const FVector2D& Size);
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
};
