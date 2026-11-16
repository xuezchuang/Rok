#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokResearchCatalog.h"
#include "RokResearchScreenWidget.generated.h"

class AActor;
class UCanvasPanel;
class UImage;
class URokUnityIndexedButton;
class UScrollBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

struct FRokResearchSprite
{
    FString Reference;
    FString AssetPath;
    FIntRect Rect;
    FIntPoint AtlasSize;
};

struct FRokResearchRenderedLine
{
    int32 SourceStudyType = 0;
    FString PrimarySprite;
    FString SecondarySprite;
    bool bPrimaryMirroredY = false;
    int32 SameColumnChildCount = 0;
    int32 CrossColumnChildCount = 0;
};

DECLARE_MULTICAST_DELEGATE(FRokResearchCloseRequested);
DECLARE_MULTICAST_DELEGATE_OneParam(FRokResearchDetailVisibilityChanged, bool);

/** Binds the research Widget Blueprints to the source catalog and server state. */
UCLASS()
class ROK_API URokResearchScreenWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static TSubclassOf<URokResearchScreenWidget> LoadPresentationClass();
    bool InitializeResearch(AActor* AcademyActor);
    UWidget* GetWidgetFromName(const FName& Name) const;
    FRokResearchCloseRequested OnCloseRequested;
    FRokResearchDetailVisibilityChanged OnDetailVisibilityChanged;
    bool IsDetailVisible() const;

#if !UE_BUILD_SHIPPING
    int32 GetSelectedTypeForAudit() const { return SelectedType; }
    int32 GetSelectedStudyTypeForAudit() const { return SelectedStudyType; }
    int32 GetViewedConfigLevelForAudit() const { return ViewedConfigLevel; }
    bool IsReferencePreviewForAudit() const { return !AcademyContext.IsValid(); }
    bool IsAccountResearchSyncedForAudit() const { return bAccountResearchSynced; }
    bool IsResearchQueueSyncedForAudit() const { return bResearchQueueSynced; }
    float GetEconomyScrollOffsetForAudit() const;
    float GetMilitaryScrollOffsetForAudit() const;
    float GetPowerScrollOffsetForAudit() const;
    bool IsPowerPageForAudit() const;
    FName GetAnimationPhaseForAudit() const;
    float GetAnimationElapsedSecondsForAudit() const;
    float GetAnimationNormalizedTimeForAudit() const;
    float GetRootOpenElapsedSecondsForAudit() const { return RootOpenElapsedSeconds; }
    float GetChildShowElapsedSecondsForAudit() const { return AnimationElapsedSeconds; }
    float GetDetailCloseElapsedSecondsForAudit() const { return DetailCloseElapsedSeconds; }
    bool IsDetailClosingForAudit() const { return bDetailCloseAnimating; }
    bool IsDetailClosedForAudit() const { return bDetailClosed; }
    bool IsRootOpenAnimationActiveForAudit() const { return bRootOpenAnimating; }
    bool IsChildShowAnimationActiveForAudit() const
    { return DetailAnimationPhase == EDetailAnimationPhase::PowerShow ||
        DetailAnimationPhase == EDetailAnimationPhase::ContentShow; }
    int32 GetPendingActionIdForAudit() const { return PendingActionId; }
    int32 GetAnimationInterruptCountForAudit() const { return AnimationInterruptCount; }
    const TArray<FRokResearchRow>& GetRenderedNodesForAudit() const { return RenderedNodes; }
    const TArray<FRokResearchRenderedLine>& GetRenderedLinesForAudit() const { return RenderedLines; }
    int32 GetLastBoundaryActionIdForAudit() const { return LastBoundaryActionId; }
    const FString& GetLoadErrorForAudit() const { return LoadError; }
#endif

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    friend class FRokResearchWidgetTest;
    bool LoadSpriteManifest(FString& Error);
    bool ApplySprite(UImage* Image, const FString& Reference, bool bUseSourceGray = false);
    bool LoadResearchData();
    bool BindPresentation();
    void BindNodePresentation(bool bBindActions);
    bool BindAction(UWidget* Widget, int32 ActionId, bool bAnimatePress = true);
    void RefreshAccountState();
    FString ResearchActionStatus(bool bIgnoreResourceShortage = false) const;
    bool GetResearchQuote(int64& Seconds, int64& Gems) const;
    void ShowSelectedDetail();
    void RefreshPowerRows(const FRokResearchRow& SelectedRow);
    void HandleIndexedAction(int32 ActionId);
    void ExecuteIndexedAction(int32 ActionId);
    void SetSelectedType(int32 Type);
    enum class EDetailAnimationPhase : uint8 { Idle, DetailOpen, PowerShow, ContentShow };
    void StartDetailAnimation(EDetailAnimationPhase Phase);
    void StartDetailClose();
    void FinishDetailClose();
    void SetDetailVisible(bool bVisible);
    void AdvanceDetailAnimation(float DeltaSeconds);
    void ApplyDetailAnimation();
    void ResetDetailAnimation();

    UPROPERTY(Transient) TObjectPtr<UScrollBox> EconomyScroll;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> MilitaryScroll;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> DetailMask;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> TreePages;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> DetailPages;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> DetailView;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> DetailRoot;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> DetailUpdateRoot;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> PowerRoot;
    UPROPERTY(Transient) TObjectPtr<UWidget> PowerBackIcon;
    UPROPERTY(Transient) TObjectPtr<UWidget> PowerBackButton;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> PowerScroll;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PowerHeaderDescription;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> PowerRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PowerLevels;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PowerEffects;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PowerValues;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WindowTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailDescription;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailLevel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailLevelNext;
    UPROPERTY(Transient) TObjectPtr<UImage> DetailLevelArrow;
    UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailProgress;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrBefore;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrDelta;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailPowerBefore;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailPowerDelta;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DetailCostValues;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UTextBlock>> NodeLevels;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> EconomyTabStyle;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> MilitaryTabStyle;
    TWeakObjectPtr<AActor> AcademyContext;
    FRokResearchCatalog Catalog;
    TArray<FRokResearchRow> RenderedNodes;
    TArray<FRokResearchRenderedLine> RenderedLines;
    TMap<FString, FRokResearchSprite> Sprites;
    FString LoadError;
    TMap<int32, int32> AccountLevels;
    bool bAccountResearchSynced = false;
    bool bResearchQueueSynced = false;
    int32 QueueStudyType = 0;
    int64 QueueFinishTime = 0;
    bool bConfirmStop = false;
    bool bConfirmInstant = false;
    int64 ConfirmedInstantPrice = -1;
    FString ResearchResult;
    float AccountRefreshElapsed = 1.0f;
    int32 SelectedType = 1;
    int32 SelectedStudyType = 0;
    int32 ViewedConfigLevel = 1;
    int32 LastBoundaryActionId = INDEX_NONE;
    int32 PendingActionId = INDEX_NONE;
    EDetailAnimationPhase DetailAnimationPhase = EDetailAnimationPhase::Idle;
    float AnimationElapsedSeconds = 0.0f;
    float RootOpenElapsedSeconds = 0.0f;
    float DetailCloseElapsedSeconds = 0.0f;
    bool bRootOpenAnimating = false;
    bool bDetailCloseAnimating = false;
    bool bDetailClosed = false;
    int32 AnimationInterruptCount = 0;
};
