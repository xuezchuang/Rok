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

/** Read-only Unity ResearchMain technology tree. Account research and queue state are deliberately unknown. */
UCLASS()
class ROK_API URokResearchScreenWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool InitializeResearch(AActor* AcademyActor);
    FRokResearchCloseRequested OnCloseRequested;
    FRokResearchDetailVisibilityChanged OnDetailVisibilityChanged;
    bool IsDetailVisible() const;

#if !UE_BUILD_SHIPPING
    int32 GetSelectedTypeForAudit() const { return SelectedType; }
    int32 GetSelectedStudyTypeForAudit() const { return SelectedStudyType; }
    int32 GetViewedConfigLevelForAudit() const { return ViewedConfigLevel; }
    bool IsReferencePreviewForAudit() const { return !AcademyContext.IsValid(); }
    bool IsAccountResearchSyncedForAudit() const { return false; }
    bool IsResearchQueueSyncedForAudit() const { return false; }
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
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    bool LoadSpriteManifest(FString& Error);
    UImage* MakeSprite(const FName& Name, const FString& Reference,
        const FVector2D& Size, bool bUseSourceGray = false);
    bool ApplySprite(UImage* Image, const FString& Reference,
        const FVector2D& Size, bool bUseSourceGray = false);
    UCanvasPanel* MakeNineSlice(const FName& Name, const FString& Reference,
        const FVector2D& Size, const FMargin& SourceMargin);
    UTextBlock* MakeText(const FName& Name, const FString& Value, float PixelSize,
        const FLinearColor& Color, bool bBold = false);
    URokUnityIndexedButton* MakeButton(const FName& Name, int32 ActionId);
    void Place(UCanvasPanel* Parent, UWidget* Child, const FVector2D& Position,
        const FVector2D& Size, int32 ZOrder = 0);
    void BuildTree();
    UScrollBox* BuildPage(int32 Type);
    void BuildNode(UCanvasPanel* Column, const FRokResearchRow& Row,
        int32 Type, int32 ItemIndex);
    void BuildDetail();
    void BuildPowerPage();
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

    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> EconomyScroll;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> MilitaryScroll;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> DetailMask;
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
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailCosts;
    UPROPERTY(Transient) TObjectPtr<UImage> DetailIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailProgress;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrBefore;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailAttrDelta;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailPowerBefore;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DetailPowerDelta;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> DetailCostValues;
    UPROPERTY(Transient) TObjectPtr<UImage> EconomySelected;
    UPROPERTY(Transient) TObjectPtr<UImage> MilitarySelected;
    TWeakObjectPtr<AActor> AcademyContext;
    FRokResearchCatalog Catalog;
    TArray<FRokResearchRow> RenderedNodes;
    TArray<FRokResearchRenderedLine> RenderedLines;
    TMap<FString, FRokResearchSprite> Sprites;
    FString LoadError;
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
