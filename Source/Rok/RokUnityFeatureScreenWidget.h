#pragma once

#include "CoreMinimal.h"
#include "RokTaskRewardPreviewData.h"
#include "RokServerHeroSubsystem.h"
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
#include "RokHospitalTreatment.h"
#include "RokUnityFeatureScreenWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class UScrollBox;
class UProgressBar;
class USlider;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UWidget;
class AActor;
class URokClientSettingsSubsystem;
class URokCommanderMeshWidget;
struct FRokServerTaskSnapshot;
struct FRokServerArmySnapshot;
struct FRokServerTrainingQueue;
struct FRokUnityCatalogTroop;
class URokServerArmySubsystem;
class URokTrainArmyMeshWidget;
class URokHelpWidget;
class URokUiPageWidget;
namespace RokArmyStateStyles
{
struct FBatch;
}
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
	GatherArmy,
};

/** Per-row quest screen state. Explicit state replaces the old "row 0 is the
 * main task" claim inference; rows without authoritative server task state
 * stay NotSynced instead of fabricating account progress. */
enum class ERokTaskClaim : uint8;

struct FRokTaskRowState
{
	enum class EPage : uint8 { MainTask, SideTask, DailyTask, ChapterTask };
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
	void OpenTasks(bool bUseTestFixtureState = false, int32 PageIndex = INDEX_NONE);
	UFUNCTION(BlueprintCallable, Category = "Rok Tasks")
	void OpenServerTasksPage(int32 PageIndex);
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
	UFUNCTION(BlueprintCallable, Category = "Rok Commander")
	void OpenCommander(bool bUseLocalRosterFixture = false);
	UFUNCTION(BlueprintCallable, Category = "Rok Items")
	void OpenItems(bool bUseTestFixtureState = false);
	UFUNCTION(BlueprintCallable, Category = "Rok Commander")
	void SelectCommanderHero(int32 HeroId);
	UFUNCTION(BlueprintCallable, Category = "Rok Items")
	void SelectItemsCategory(int32 Category);
	UFUNCTION(BlueprintCallable, Category = "Rok Items")
	void SelectItemsStack(int64 ItemIndex);
	void OpenAlliance(bool bOpenJoinedPreview = false);
	void OpenVip(bool bUseTestFixtureState = false);
	void OpenChat(bool bUseTestFixtureState = false);
	void OpenWorldPositionSearch();
	void OpenMapMarkers();
	void OpenResourceSearch(int32 BarbarianLevel = 0);
	bool IsResourceSearchOpen() const { return ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch; }
	void OpenExpedition();
	bool OpenGatherArmy(int64 TargetId, double Distance, double ResourceRadius, FString& Error,
		TOptional<bool> AttackStationAfterBattle = {});
	FSimpleDelegate OnExpeditionStoreRequested;
#if !UE_BUILD_SHIPPING
	bool IsExpeditionModeForAudit() const { return ActiveMode == ERokUnityFeatureScreenMode::Expedition; }
	int32 GetSelectedExpeditionStageIdForAudit() const { return SelectedExpeditionStageId; }
	bool IsExpeditionPreviewForAudit() const { return bExpeditionPreview; }
	bool IsExpeditionStaging() const { return IsExpeditionModeForAudit() && bExpeditionTaskConfigVisible; }
	bool IsExpeditionTaskConfigVisibleForAudit() const { return bExpeditionTaskConfigVisible; }
	FName GetExpeditionAnimationPhaseForAudit() const { return ExpeditionAnimationPhase; }
	float GetExpeditionAnimationElapsedForAudit() const { return ExpeditionAnimationElapsed; }
	int32 GetExpeditionLastBoundaryActionIdForAudit() const { return ExpeditionLastBoundaryActionId; }
	int32 GetExpeditionRenderedStageCountForAudit() const { return ExpeditionRows.Num(); }
	bool IsExpeditionAccountStateKnownForAudit() const { return bExpeditionProgressKnown; }
#endif
	void OpenGemShop();
	void OpenCharge(int32 InitialPageType = 107);
	void OpenBuildCity(int32 GuideBuildingType = 0);
	/** Card-to-city handoff; a listener owns transient placement and final validation. */
	FRokBuildCityPlacementRequested OnBuildCityPlacementRequested;
	void OpenWorkerQueue(AActor* BuildingActor = nullptr, int32 RequiredSeconds = 0);
	void OpenSettingsHub();
	void OpenGeneralSettings();
	void OpenNotificationSettings();
	void OpenLanguageSettings();
	void OpenHelp();
	int32 GetLanguageDraftIdForAudit() const { return LanguageDraftId; }
	void OpenAccountSettings();
	UFUNCTION(BlueprintCallable, Category = "Rok Army")
	void OpenArmyStatistics();
	UFUNCTION(BlueprintCallable, Category = "Rok Army")
	void OpenArmyStatisticsPage(int32 PageIndex);
	void OpenStore();
	void OpenTrainArmy(AActor* BuildingActor = nullptr, int32 ArmyType = 1);
	UFUNCTION(BlueprintCallable, Category = "Rok Army")
	void OpenTrainArmyPreview(int32 ArmyType);
	UFUNCTION(BlueprintCallable, Category = "Rok Army")
	void OpenTrainArmyServerBuilding(int32 BuildingIndex, int32 ArmyType);
	void OpenSpeedUp(AActor* QueueActor = nullptr, int32 QueueType = 20102);
	void OpenHospital(AActor* BuildingActor = nullptr);
	void OpenTaskCityTarget(int32 BuildingType, bool bOpenBuildCity = false, bool bUpgradeBuilding = false);
	/** Read-only source technology tree; null actor is an explicit reference preview. */
	void OpenResearch(AActor* AcademyActor = nullptr);
	bool IsHospitalExplanationVisibleForAudit() const { return bHospitalExplanationVisible; }
	URokHospitalExplanationWidget* GetHospitalExplanationForAudit() const { return HospitalExplanationWidget; }
	const TArray<FRokTaskRewardPreviewRow>& GetConfiguredRewardPreview(int32 GroupId);
	FSlateBrush MakeRewardQualityBrush(int32 Quality, FVector2D Size) const;
	void OpenCityManager();
	UFUNCTION(BlueprintCallable, Category = "Rok Army")
	void OpenArmyShow();
	UFUNCTION(BlueprintCallable, Category = "Rok Tasks")
	void CloseScreen();
	/** User dismissal; CloseScreen remains the synchronous routing API. */
	void RequestCloseScreen();
	bool IsScreenOpen() const { return ActiveMode != ERokUnityFeatureScreenMode::None; }
	bool ExportCurrentModeSpec(const FString& Filename, FString& OutError) const;
	/** Resolves both native shell widgets and widgets inside the active designer page. */
	UWidget* GetWidgetFromName(const FName& Name) const;
	bool IsWorkerQueueOpen() const
	{
		return ActiveMode == ERokUnityFeatureScreenMode::WorkerQueue
			|| (ActiveMode == ERokUnityFeatureScreenMode::SpeedUp && bSpeedUpFromWorkerQueue);
	}
	bool IsHospitalOpen() const { return ActiveMode == ERokUnityFeatureScreenMode::Hospital; }
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
	int32 GetVisibleTaskIdForAudit(int32 RowIndex) const
	{ return ActiveMode == ERokUnityFeatureScreenMode::Tasks && VisibleTaskStates.IsValidIndex(RowIndex)
		? VisibleTaskStates[RowIndex].TaskId : INDEX_NONE; }
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
	void RefreshWorkerQueueForTesting() { if (ActiveMode == ERokUnityFeatureScreenMode::WorkerQueue) UpdateWorkerQueuePresentation(); }
	int32 GetSelectedCommanderIdForAudit() const { return SelectedCommanderId; }
	int32 GetSelectedCommanderTalentIdForAudit() const { return VisibleCommanderTalentIds.IsValidIndex(SelectedCommanderTalentIndex) ? VisibleCommanderTalentIds[SelectedCommanderTalentIndex] : INDEX_NONE; }
	int32 GetSelectedCommanderSkillDetailIdForAudit() const { return VisibleCommanderSkillIds.IsValidIndex(SelectedCommanderSkillDetailIndex) ? VisibleCommanderSkillIds[SelectedCommanderSkillDetailIndex] : INDEX_NONE; }
	void AdvanceWindowAnimationForTesting(float DeltaSeconds) { AdvanceWindowAnimation(DeltaSeconds); }
	void AdvanceArmyShowPanelAnimationForTesting(float DeltaSeconds) { AdvanceArmyShowPanelAnimation(DeltaSeconds); }
	void AdvanceActivityCountdownForTesting(float DeltaSeconds) { UpdateActivityCountdown(DeltaSeconds); }
	void AdvanceTrainArmyDetailAnimationForTesting(float DeltaSeconds) { AdvanceTrainArmyDetailAnimation(DeltaSeconds); }
	void RebuildArmyShowPresentationForTesting() { if (ActiveMode == ERokUnityFeatureScreenMode::ArmyShow) RebuildModeContent(); }
	void AdvanceCommanderRosterAnimationForTesting(float DeltaSeconds) { AdvanceCommanderRosterAnimation(DeltaSeconds); }
	void RefreshTaskDetailProjectionForTesting() { RefreshTaskMainProjection(); }
	void RefreshServerTasksForTesting() { RefreshServerTaskPresentation(); }
	void RefreshServerArmyForTesting() { RefreshServerArmyPresentation(); }
	void RefreshServerPlayerAssetsForTesting() { RefreshServerPlayerAssetsPresentation(); }
	int64 GetSelectedItemsStackIndexForAudit() const { return SelectedItemsStackIndex; }
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

	UPROPERTY(Transient)
	TObjectPtr<URokUiPageWidget> FeatureShellPage;

	UPROPERTY(Transient)
	TObjectPtr<URokUiPageWidget> ActiveModePage;

	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> FeatureTextStateTemplate;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> FeatureTextColorTemplate;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> TaskRewardTextStateTemplate;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> TaskChapterRewardLayoutTemplate;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> TaskPresentationTemplate;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> TaskListLayoutTemplate;

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

	struct FLocalSpeedUpQueueColors
	{
		FLinearColor Description = FLinearColor::White;
		FLinearColor Fill = FLinearColor::White;
	};
	FLocalSpeedUpQueueColors LocalSpeedUpActiveColors;
	FLocalSpeedUpQueueColors LocalSpeedUpInactiveColors;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueuePrimaryTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueueRuntimeStateText;

	UPROPERTY()
	TObjectPtr<UProgressBar> WorkerQueuePrimaryProgress;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueuePrimaryProgressText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueueSecondaryTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueueSecondaryNoteText;

	UPROPERTY()
	TObjectPtr<UProgressBar> WorkerQueueSecondaryProgress;

	UPROPERTY()
	TObjectPtr<UTextBlock> WorkerQueueSecondaryProgressText;

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

	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResourceSearchMonsterTip;
	UPROPERTY(Transient) TObjectPtr<UImage> ResourceSearchGuide;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> TaskGuideTexture;
	uint32 ResourceSearchRequestSerial = 0;
	bool bResourceSearchPending = false;
	bool bResourceSearchTaskGuide = false;
	bool bFreezeResourceSearchGuideForCapture = false;

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
	int64 SelectedSessionMail = 0;
	int64 SelectedBattleReport = 0;
	uint64 ShownBattleReportRevision = 0;
	bool bShownMailInboxLoading = false;
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
	FString LastServerTaskSignature;
	bool bLastServerTaskAvailable = false;
	bool bLastServerTaskLoaded = false;
	bool bTaskMainLocalProjectionRendered = false;
	bool bTaskMainLocalConditionMet = false;
	int32 TaskMainDisplayedCurrent = 0;
	int32 SelectedCommanderId = 2007;
	bool bCommanderServerSelectionInitialized = false;
	bool bSelectedCommanderServerOwned = false;
	UPROPERTY(Transient)
	TObjectPtr<URokCommanderMeshWidget> CommanderAnimatedPortrait;
	int32 CommanderAnimatedPortraitHeroId = INDEX_NONE;
	UPROPERTY(Transient) TObjectPtr<UImage> CommanderSceneImage;
	UPROPERTY(Transient) TObjectPtr<UMediaPlayer> CommanderScenePlayer;
	UPROPERTY(Transient) TObjectPtr<UMediaTexture> CommanderSceneVideo;
	FName CommanderSceneName;
	void ShowCommanderScene(const FString& Scene);
	void StopCommanderSceneMotion();
	void UpdateCommanderSceneMotion();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> CommanderChrome;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> CommanderPreviewTextures;
	UTexture2D* LoadCommanderPreviewTexture(const FString& RelativePath);
	bool bCommanderLocalRosterFixture = false;
	bool bCommanderBiographyOpen = false;
	int32 SelectedCommanderSkillIndex = 0;
	int32 SelectedCommanderSkillDetailIndex = INDEX_NONE;
	int32 SelectedCommanderTalentIndex = INDEX_NONE;
	int32 CommanderTalentPage = 1;
	bool bCommanderTalentOpen = false;
	TArray<FRokHeroTalentNode> CommanderTalentNodes;
	TOptional<FRokHeroCommand> CommanderConfirmation;
	FRokHeroCommandCost CommanderConfirmationCost;
	FString LastCommanderInventorySignature;
	TArray<int32> VisibleCommanderTalentIds;
	int32 SelectedCommanderSortType = 0;
	bool bCommanderSortMenuOpen = false;
	int32 SelectedItemsCategory = 0;
	int32 SelectedItemsIndex = 0;
	int64 SelectedItemsStackIndex = 0;
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
	struct FVipPresentation
	{
		int64 Points = INDEX_NONE;
		int64 LoginDays = INDEX_NONE;
		int32 CurrentLevel = INDEX_NONE;
		int32 LevelThreshold = 0;
		int32 TodayPoints = INDEX_NONE;
		TOptional<bool> DailyClaimed;
		TOptional<bool> FreeChestClaimed;
		uint32 PurchasedLevels = 0;
		bool bClaimPending = false;

		bool operator==(const FVipPresentation&) const = default;
	};
	FVipPresentation LastVipPresentation;
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
	struct FExpeditionTeam
	{
		int32 HeroId = 0;
		TMap<int32, int32> Soldiers;
	};

	int32 SelectedExpeditionStageId = 0;
	float ExpeditionMapOffset = 0.0f;
	bool bExpeditionProgressKnown = false;
	TMap<int32, int32> ExpeditionStageStars;
	TMap<int32, FExpeditionTeam> ExpeditionTeams;
	FExpeditionTeam ExpeditionDraft;
	int32 ExpeditionDraftSlot = INDEX_NONE;
	bool bExpeditionChoosingHero = false;
	TArray<int32> ExpeditionVisibleHeroChoices;
	TArray<int32> ExpeditionDraftSoldierIds;
	int32 ExpeditionDraftCapacity = 0;
	int64 GatherArmyTargetId = 0;
	uint64 GatherArmyAoiGeneration = 0;
	double GatherArmyDistance = 0;
	double GatherArmyResourceRadius = 0;
	int32 GatherArmyDeputy = 0;
	bool bGatherPickingDeputy = false;
	bool bGatherSubmitting = false;
	TOptional<bool> GatherArmyAttackStation;
	int32 GatherArmyActionCost = 0;
	uint32 GatherArmyComposerSerial = 0;
	TMap<int32, int32> GatherArmyAvailable;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> ExpeditionTextures;
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
	int32 BuildCityGuideTypeId = 0;
	bool bBuildCityGuideNeedsScroll = false;
	int32 SelectedBuildCityTypeId = 0;
	int32 SelectedArmyStatisticsPage = 0;
	float ServerArmyRefreshAccumulator = 0.0f;
	FString LastServerArmySignature;
	bool bLastServerArmyAvailable = false;
	int32 SelectedStoreCategory = 0;
	int32 SelectedStorePage = 0;
	int32 SelectedStoreItemIndex = 0;
	int32 SelectedStoreQuantity = 1;
	int32 SelectedTrainArmyType = 1;
	int32 SelectedServerTrainBuildingIndex = 0;
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
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> TrainResourceCostLayout;
	UPROPERTY(Transient) TObjectPtr<UWidget> TrainArmyTipLayer;
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
	float ServerPlayerAssetsRefreshAccumulator = 0.0f;
	FString LastServerHeroSignature;
	FString LastServerInventorySignature;
	bool bLastServerHeroAvailable = false;
	bool bLastServerHeroKnown = false;
	bool bLastServerInventoryAvailable = false;
	bool bLastServerInventoryKnown = false;
	int32 PendingActionId = INDEX_NONE;
	double ActivityPreviewSecondsRemaining = 91824.0;
	int32 LastRenderedActivitySecond = INDEX_NONE;
	bool bFreezeActivityCountdownForCapture = false;
	float SpeedUpRefreshAccumulator = 0.0f;
	float WorkerQueueRefreshAccumulator = 0.0f;
	bool bWorkerQueueRenderedBusy = false;
	bool bWorkerQueueRenderedServer = false;
	int32 WorkerQueueRenderedPrimaryState = INDEX_NONE;
	int32 WorkerQueueRenderedSecondaryState = INDEX_NONE;
	int32 WorkerRequiredSeconds = 0;
	int32 WorkerHirePrice = 0;
	bool bWorkerHireUsesItem = false;
	bool bWorkerHireAffordable = false;
	bool bWorkerQueueCanWrite = false;
	bool bWorkerQueueRenderedItemsKnown = false;
	bool bWorkerQueueRenderedRenew = false;
	int32 ServerSpeedUpQueueIndex = 0;
	int32 ServerSpeedUpBuildingIndex = 0;
	bool bSpeedUpFromWorkerQueue = false;
	bool bServerSpeedUpPending = false;
	TArray<bool> ServerSpeedUpItemCostsGems;
	UPROPERTY()
	TObjectPtr<UWidget> ServerSpeedUpConfirmation;
	int32 ServerSpeedUpConfirmIndex = INDEX_NONE;

	void BuildWidgetTree();
	void RebuildModeContent(bool bOpening = false);
	static UClass* ResolveFeaturePageClass(FName Name);
	URokUiPageWidget* MountModePage(FName PageName = NAME_None, bool bBindFrameClose = true, int32 ZOrder = 0);
	URokUiPageWidget* MountPageItem(UCanvasPanel* Parent, UClass* ItemClass, const TCHAR* LegacyPrefix, int32 Index);
	void BindPageAction(const URokUiPageWidget& Page, FName Name, int32 ActionId);
	void BindGeneralSettingsPage(const URokUiPageWidget& Page);
	void UpdateWindowAnimationForMode(bool bOpening);
	void ApplyWindowAnimation();
	void AdvanceWindowAnimation(float DeltaSeconds);
	EAllianceAnimatedRoot ResolveAllianceAnimatedRoot() const;
	bool TryStartAllianceRootClose(int32 ActionId);
	UFUNCTION()
	void HandleResearchMainMaskClicked();
	void RequestResearchMainMaskClose();
	void BuildActivityCenter();
	void BuildActivityCalendarReadonly(const URokUiPageWidget* Page = nullptr, UClass* RowClass = nullptr,
		const URokUiPageWidget* Presentation = nullptr);
	void SetActivityIcon(UImage* Image, UTexture2D* Atlas, const FIntRect& Rect) const;
	void BuildMail();
	void BindMailPageControls(const URokUiPageWidget& Page);
	bool TryBuildMailReferencePage(const FRokMailRewardTemplate& MailTemplate, bool bHasMailTemplate,
		const FString& MailTemplateError);
	void PreviewMailReferenceSelection();
	void AttachMailGiftMesh(int32 Index, UCanvasPanel& Pulse);
	int32 GetSessionMailUnreadCount(int32 Tab) const;
	void BuildSessionMail(UScrollBox* List);
	bool TryBuildSessionMailWbp(UScrollBox* List, const TArray<int64>& Ids);
	void BindLiveMailRewardCard(URokUiPageWidget& Page, const FRokMailRewardRow& Reward, const TCHAR* Prefix);
	bool TryBuildBattleReportOverview(URokUiPageWidget& Page, const TSharedPtr<FJsonObject>& Mail,
		const TSharedPtr<FJsonObject>& Report, int64 RoleId, bool bDownloadFailed);
	void SelectSessionMail(int64 EmailIndex);
	void BuildBattleReportMail(UScrollBox* List);
	void SelectBattleReport(int64 EmailIndex);
	void RefreshBattleReportMail();
	void BuildTasks();
	bool BeginTaskCompletedRowAnimation(UCanvasPanel* Row, TFunction<void()> OnComplete);
	void AdvanceTaskCompletedRowAnimation();
	void ResetTaskCompletedRowAnimation();
	void BuildTaskChapter(const URokUiPageWidget* Page = nullptr, UClass* RowClass = nullptr,
		UClass* AggregateRewardClass = nullptr, UClass* TaskRewardClass = nullptr);
	bool PlaceTaskChapterRewardFromLayout(UCanvasPanel& Host, UWidget& Cell, FName LayoutName, int32 Index);
	void BuildServerTaskChapter(const FRokServerTaskSnapshot& Snapshot);
	bool TryBuildServerTaskChapterPage(const FRokServerTaskSnapshot& Snapshot);
	FText GetServerTaskChapterTitle(const FRokServerTaskSnapshot& Snapshot) const;
	bool TryBuildServerDailyTaskPage(const FRokServerTaskSnapshot& Snapshot);
	FVector2D GetTaskListRowSize(const UWidget* Row, FName LayoutName);
	void BindServerTaskPageActions(const URokUiPageWidget& Page, const FRokServerTaskSnapshot& Snapshot);
	int32 AddServerDailyTaskRows(const FRokServerTaskSnapshot& Snapshot, UScrollBox& Scroll,
		UClass* RowClass = nullptr, UClass* RewardClass = nullptr);
	URokUiPageWidget* BuildTaskRowPage(UClass* RowClass, UClass* RewardClass, int32 RowIndex,
		const FText& Title, const FText& Description, const FIntRect& TaskIconRect, int32 RewardGroupId,
		const TArray<FRokTaskRewardPreviewRow>& Rewards, bool bServerRow, bool bClaimable, const FText& ButtonLabel);
	void BuildServerTaskChapterGuide(const FRokTaskChapterPreviewRow& Metadata);
	void PopulateServerTaskChapterGuide(const FRokTaskChapterPreviewRow& Metadata, class URichTextBlock& Dialogue,
		UCanvasPanel* ModelSlot);
	void RefreshTaskMainProjection();
	void RefreshServerTaskPresentation();
	void SubmitTaskClaim(ERokTaskClaim Kind, int32 Id);
	int64 TaskRewardCivilization = 0;
	TMap<int32, TArray<FRokTaskRewardPreviewRow>> ServerTaskRewardCache;
	TMultiMap<int32, TSharedPtr<FJsonObject>> TaskRewardRowsByGroup;
	void BuildCommander();
	void BuildCommanderProgression(const FRokServerHeroEntry* Hero);
	bool HandleCommanderProgressionAction(int32 ActionId);
	void PrepareCommanderCommand(FRokHeroCommand Request);
	void ConfirmCommanderCommand();
	void BuildItems();
	void RefreshServerPlayerAssetsPresentation();
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
	FVipPresentation ReadVipPresentation() const;
	void RefreshVipPresentation();
	void RequestVipClaim(bool bFreeChest);
	void BuildVipDailyPointHelpTip();
	void BuildChat();
	void BuildWorldPositionSearch();
	void BuildMapMarkers();
	void BuildResourceSearch();
	FText GetResourceSearchLock(int32 Type) const;
	void SubmitResourceSearch();
	void BuildExpedition();
	void BuildExpeditionMap(const FRokExpeditionStagePreviewRow& Stage);
	void BuildExpeditionTask(const FRokExpeditionStagePreviewRow& Stage);
	void BuildArmyComposer();
	void RefillGatherArmy();
	bool HandleGatherArmyAction(int32 ActionId);
	void FinishGatherArmy(int32 ArmyIndex, const FString& Error);
	void RefreshExpeditionAccount();
	bool CanChallengeExpedition(const FRokExpeditionStagePreviewRow& Stage) const;
	bool HandleExpeditionAction(int32 ActionId);
	void OpenExpeditionArmySlot(int32 Slot);
	TMap<int32, int32> GetExpeditionAvailableSoldiers(int32 Slot) const;
	int32 GetExpeditionDraftCapacity() const;
	void UpdateExpeditionArmySummary();
	UFUNCTION() void HandleExpeditionArmySliderChanged(float Value);
	UImage* AddExpeditionArt(UCanvasPanel* Canvas, FName Name, const FString& Asset,
		FVector2D Position, FVector2D Size, int32 Layer = 0);
	void AddExpeditionHeroHead(UCanvasPanel* Canvas, int32 HeroId, FName Name,
		FVector2D Position, FVector2D Size, int32 Layer);
	void AdvanceExpeditionAnimation(float DeltaSeconds);
	void BuildGemShop();
	void BuildCharge();
	void BuildBuildCity();
	bool TryGetFeatureTextStyle(FName StateName, struct FTextBlockStyle& OutStyle);
	bool TryGetFeatureTextColor(FName StateName, struct FSlateColor& OutColor);
	bool ApplyTaskRewardTextOnlyState(UTextBlock& Label);
	void BuildWorkerQueue();
	void HandleWorkerQueueAction(int32 ActionId);
	void BuildServerBuildingSpeedUp();
	void UpdateServerBuildingSpeedUp();
	void HandleServerSpeedUpItem(int32 Index, bool bConfirmed = false);
	void BuildSettingsHub();
	void BuildGeneralSettings();
	void BuildGraphicsSettingsCard(UCanvasPanel* Content, float GraphicsValue, float FrameValue, bool bEnabled);
	void UpdateGraphicsSettingsSlider(FName Prefix, int32 Level);
	struct FGeneralSettingsFillSlot
	{
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		FVector2D Alignment = FVector2D::ZeroVector;
	};
	struct FGeneralSettingsFillLayout
	{
		FGeneralSettingsFillSlot Fill;
		FGeneralSettingsFillSlot Patches[3];
	};
	TMap<FName, FGeneralSettingsFillLayout> GeneralSettingsFillLayouts;
	bool UpdateAuthoredGeneralSettingsFill(FName Prefix, float Fraction, bool bRightAnchored);
	void BuildNotificationSettings();
	void BuildLanguageSettings();
	void BuildHelp();
	void UpdateLanguageDraftSelection();
	int32 LanguageDraftId = 40;
	void BuildAccountSettings();
	void BuildArmyStatistics();
	void RefreshServerArmyPresentation();
	void RefreshTrainArmyAction();
	void SubmitTrainArmy(TOptional<int64> ConfirmedGems = {});
	void OpenTrainArmyInstantConfirmation();
	void CloseTrainArmyInstantConfirmation(bool bConfirm);
	UPROPERTY(Transient)
	TObjectPtr<UWidget> TrainArmyInstantConfirmation;
	int64 TrainArmyInstantPrice = -1;
	int64 TrainArmyConfirmationPrice = -1;
	bool bTrainActionReceives = false;
	bool bTrainCanStart = false;
	void BuildStore();
	void BuildTrainArmy();
	void BuildTrainArmyQuantityControls();
	void BuildTrainResourceCost();
	void UpdateTrainResourceCost();
	void BuildTrainArmySharedDisplay(int32 TroopId);
	void ApplyTrainParameterLayout(int32 TroopId);
	bool BuildTrainArmyParameterPage(const FRokUnityCatalogTroop& SelectedTroop,
		const TArray<FRokUnityCatalogTroop>& Troops, const FRokServerArmySnapshot& ArmySnapshot,
		const URokServerArmySubsystem* ServerArmy, const FRokServerTrainingQueue* ServerQueue,
		const FText& ContextText, bool bTierUnlocked, bool bIdle);
	void UpdateTrainArmyQuantity(int32 Count, float SliderPosition = -1.0f);
	UFUNCTION() void HandleTrainArmyQuantityTextChanged(const FText& Text);
	UFUNCTION() void HandleTrainArmyQuantitySliderChanged(float Value);
	bool TryBuildTrainArmyUnavailableState(FName State, UCanvasPanel* Parent, const FIntRect* PortraitRect = nullptr);
	void BuildTrainArmyDetail(int32 TroopId);
	bool BuildTrainArmyDetailPage(const struct FRokTrainArmyDetail& Detail);
	void ClickTrainArmyPreviewButton(FName Name);
	void ShowTrainArmyDetailTip(int32 Index, bool bSkill);
	void AdvanceTrainArmyDetailAnimation(float DeltaSeconds);
	void BuildSpeedUp();
	void BuildHospital();
	bool BuildHospitalPage(const TArray<FRokHospitalSoldier>& Units, bool bQueue, int32 HospitalMark);
	void BuildLegacyHospitalPage(const TArray<FRokHospitalSoldier>& Units, bool bQueue, int32 HospitalMark);
	void PopulateHospitalTroopRows(UScrollBox& List, UClass* RowClass,
		const TArray<FRokHospitalSoldier>& Units, bool bQueue);
	void BuildResearch();
	void HandleResearchDetailVisibilityChanged(bool bVisible);
	void SetHospitalExplanationVisible(bool bVisible);
	bool bHospitalExplanationVisible = false;
	UPROPERTY(Transient) TObjectPtr<URokHospitalExplanationWidget> HospitalExplanationWidget;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> HospitalResourceCostLayout;
	UPROPERTY(Transient) TObjectPtr<URokResearchScreenWidget> ResearchScreenWidget;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> HospitalMainContent;
	UPROPERTY(Transient) TObjectPtr<UWidget> HospitalConfirmation;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<URokHospitalQuantity>> HospitalQuantities;
	FRokHospitalState HospitalState;
	FRokHospitalQuote HospitalQuote;
	TMap<int32, int32> HospitalCounts;
	TMap<int32, FVector2D> HospitalSliderFillSizes;
	TMap<int32, int32> HospitalSelectedCounts;
	bool bHospitalCanCollect = false;
	float HospitalRefreshAccumulator = 0;
	int64 HospitalConfirmationPrice = 0;
	void RefreshHospital();
	void UpdateHospitalActions();
	void OpenHospitalInstantConfirmation();
	void SubmitHospitalTreatment(bool bImmediately);
	void BuildCityManager();
	void BuildArmyShow();
	void AdvanceArmyShowPanelAnimation(float DeltaSeconds);
	float ArmyShowPanelAnimationTime = 0.25f;
	TArray<TWeakObjectPtr<UWidget>> ArmyShowPanelAnimationWidgets;
	void BuildSplitWindowFrame(
		const FVector2D& Position,
		const FVector2D& Size,
		const FText& Title,
		const FVector2D& ClosePosition,
		UCanvasPanel* Parent = nullptr,
		int32 CloseAction = 1,
		float BodyInset = 0.0f,
		int32 SourceTitlePixels = 0);
	void BuildActivityFrame();
	void BuildFeedback();
	void UpdateExpandedDesignResolution(const FVector2D& RootSize);
	void UpdateActivityCountdown(float DeltaSeconds);
	void UpdateSpeedUpQueuePresentation();
	void ApplyLocalSpeedUpQueueColors(bool bHasActiveQueue);
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
	TOptional<float> TryBuildMailRewardGrid(UCanvasPanel& DetailContent);
	void BindMailRewardCard(URokUiPageWidget& Card, const FRokMailRewardRow& Row, int32 Index, bool bReferenceCard);
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
		int32 Rare,
		const RokArmyStateStyles::FBatch& ArmyStyles);
	UWidget* MakeTaskRow(
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
		const TArray<FRokTaskRewardPreviewRow>& RewardPreviewRows,
		UClass* RowClass = nullptr, UClass* RewardClass = nullptr);
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
