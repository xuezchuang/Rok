#include "RokResearchScreenWidget.h"

#include "RokArmyShowPanelAnimation.h"
#include "RokUiWindowAnimation.h"
#include "RokUnityFeatureScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"

namespace
{
constexpr int32 EconomyType = 1;
constexpr int32 MilitaryType = 2;
constexpr int32 CloseAction = 1;
constexpr int32 EconomyAction = 10;
constexpr int32 MilitaryAction = 11;
constexpr int32 PreviousLevelAction = 20;
constexpr int32 NextLevelAction = 21;
constexpr int32 BackAction = 22;
constexpr int32 MoreAction = 23;
constexpr int32 PowerBackAction = 24;
constexpr int32 BoundaryAction = 30;
constexpr int32 NodeActionBase = 100000;
constexpr float DetailShowDuration = 0.25f;
constexpr float DetailCloseDuration = 0.15f;

bool ReadIntPair(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32& X, int32& Y)
{
    const TSharedPtr<FJsonObject>* Size = nullptr;
    if (!Object->TryGetObjectField(Key, Size) || !Size->IsValid() ||
        !(*Size)->TryGetNumberField(TEXT("width"), X) ||
        !(*Size)->TryGetNumberField(TEXT("height"), Y)) return false;
    return X > 0 && Y > 0;
}

bool ReadSpriteRect(const TSharedPtr<FJsonObject>& Object, FIntRect& Out)
{
    const TSharedPtr<FJsonObject>* Rect = nullptr;
    int32 X = 0, Y = 0, Width = 0, Height = 0;
    if (!Object->TryGetObjectField(TEXT("rect"), Rect) || !Rect->IsValid() ||
        !(*Rect)->TryGetNumberField(TEXT("x"), X) ||
        !(*Rect)->TryGetNumberField(TEXT("y"), Y) ||
        !(*Rect)->TryGetNumberField(TEXT("width"), Width) ||
        !(*Rect)->TryGetNumberField(TEXT("height"), Height) ||
        X < 0 || Y < 0 || Width <= 0 || Height <= 0) return false;
    Out = FIntRect(X, Y, X + Width, Y + Height);
    return true;
}

FString ResearchLineSprite(int32 Number)
{
    return FString::Printf(TEXT("ui_research[line_research_%03d]"), Number);
}

FRokResearchRenderedLine DescribeSourceLine(const FRokResearchRow& Parent,
    const TArray<FRokResearchRow>& AllNodes)
{
    // ResearchProxy.GetTechnologySub groups children from all four source
    // prerequisite slots. UI_ResearchSubItem_SubView then divides that group
    // into next-column and two-columns-ahead branches.
    TArray<const FRokResearchRow*> SameColumnChildren, CrossColumnChildren;
    for (const FRokResearchRow& Child : AllNodes)
    {
        if (Child.Type != Parent.Type) continue;
        const bool bRequiresParent = Child.Preconditions.ContainsByPredicate(
            [&Parent](const FRokResearchPrerequisite& Prerequisite)
            { return Prerequisite.StudyType == Parent.StudyType; });
        if (!bRequiresParent) continue;
        if (Child.Columns == Parent.Columns + 1) SameColumnChildren.Add(&Child);
        else if (Child.Columns == Parent.Columns + 2) CrossColumnChildren.Add(&Child);
    }
    FRokResearchRenderedLine Result;
    Result.SourceStudyType = Parent.StudyType;
    Result.SameColumnChildCount = SameColumnChildren.Num();
    Result.CrossColumnChildCount = CrossColumnChildren.Num();
    if (SameColumnChildren.IsEmpty() && CrossColumnChildren.IsEmpty()) return Result;

    // RS.cs ResearchLineJJ/JX, ResearchLineUpToDown,
    // ResearchSubAndCrossLine, ResearchLineSp1TO2. Runtime research level is
    // unavailable, so the visual uses the explicit level-zero *reference*
    // fixture style; no account level is inferred from this choice.
    if (CrossColumnChildren.IsEmpty())
    {
        if (SameColumnChildren.Num() == 1 && Parent.Location != SameColumnChildren[0]->Location)
        {
            const int32 Distance = FMath::Abs(Parent.Location - SameColumnChildren[0]->Location);
            const int32 Sprite = Distance >= 4 ? 113 : Distance == 3 ? 112 : 114;
            Result.PrimarySprite = ResearchLineSprite(Sprite);
            Result.bPrimaryMirroredY = Parent.Location > SameColumnChildren[0]->Location;
        }
        else
        {
            const int32 Count = SameColumnChildren.Num();
            const int32 Sprite = Count == 1 ? 101 : Count == 2
                ? (Parent.Type == EconomyType ? 110 : 102) : Count == 3
                ? (Parent.Type == EconomyType ? 109 : 103) : Count == 4 ? 104 : 0;
            if (Sprite != 0) Result.PrimarySprite = ResearchLineSprite(Sprite);
        }
    }
    else
    {
        if (SameColumnChildren.Num() > 0)
        {
            const int32 Sprite = Parent.StudyType == 121 ? 105 :
                SameColumnChildren.Num() == 1 ? 108 : SameColumnChildren.Num() == 2 ? 115 : 0;
            if (Sprite != 0) Result.PrimarySprite = ResearchLineSprite(Sprite);
        }
        if (CrossColumnChildren.Num() == 1 && Parent.Location == CrossColumnChildren[0]->Location)
            Result.SecondarySprite = ResearchLineSprite(101);
        else if (CrossColumnChildren.Num() == 4)
            Result.SecondarySprite = ResearchLineSprite(104);
    }
    return Result;
}
}

bool URokResearchScreenWidget::LoadSpriteManifest(FString& Error)
{
    Sprites.Reset();
    FString Json;
    const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/ResearchNative/atlas_manifest.json");
    if (!FFileHelper::LoadFileToString(Json, *Path))
    {
        Error = TEXT("Research atlas manifest is missing");
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    {
        Error = TEXT("Research atlas manifest is invalid JSON");
        return false;
    }
    FString Schema;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.research.atlas.v1") ||
        !Root->TryGetArrayField(TEXT("sprites"), Values))
    {
        Error = TEXT("Research atlas manifest schema differs from source contract");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            Error = TEXT("Research atlas sprite entry is invalid");
            return false;
        }
        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FRokResearchSprite Sprite;
        if (!Object->TryGetStringField(TEXT("reference"), Sprite.Reference) ||
            !Object->TryGetStringField(TEXT("ueAssetPath"), Sprite.AssetPath) ||
            !ReadSpriteRect(Object, Sprite.Rect) ||
            !ReadIntPair(Object, TEXT("atlasSize"), Sprite.AtlasSize.X, Sprite.AtlasSize.Y) ||
            Sprite.Reference.IsEmpty() || Sprite.AssetPath.IsEmpty() || Sprites.Contains(Sprite.Reference) ||
            Sprite.Rect.Max.X > Sprite.AtlasSize.X || Sprite.Rect.Max.Y > Sprite.AtlasSize.Y)
        {
            Error = TEXT("Research atlas sprite identity or bounds are invalid");
            return false;
        }
        Sprites.Add(Sprite.Reference, MoveTemp(Sprite));
    }
    if (Sprites.Num() < 20)
    {
        Error = TEXT("Research atlas is missing required source sprites");
        return false;
    }
    // These common/page sprites are already imported by the modal atlas
    // pipeline. Coordinates come from ui_common.tpsheet and ui_page.tpsheet.
    auto AddExisting = [this](const TCHAR* Reference, const TCHAR* Path,
        int32 X, int32 Y, int32 Width, int32 Height, const FIntPoint& AtlasSize)
    {
        FRokResearchSprite Sprite;
        Sprite.Reference = Reference;
        Sprite.AssetPath = Path;
        Sprite.Rect = FIntRect(X, Y, X + Width, Y + Height);
        Sprite.AtlasSize = AtlasSize;
        Sprites.Add(Sprite.Reference, MoveTemp(Sprite));
    };
    const TCHAR* CommonPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common");
    const TCHAR* PagePath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page");
    const TCHAR* FramePath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame");
    AddExisting(TEXT("ui_common[btn_com_1008_1]"), CommonPath, 205, 370, 145, 103, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[btn_com_1008_2]"), CommonPath, 205, 266, 121, 102, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pb_com_1000_0]"), CommonPath, 427, 477, 45, 40, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pb_com_1000_1]"), CommonPath, 401, 112, 20, 23, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pl_com_1001]"), CommonPath, 436, 165, 63, 55, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pl_com_1008]"), CommonPath, 1003, 932, 400, 42, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[btn_com_1000_1]"), CommonPath, 883, 589, 69, 69, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[btn_com_1000]"), CommonPath, 703, 282, 69, 69, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[btn_com_1007]"), CommonPath, 782, 237, 37, 37, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pl_com_1012_2]"), CommonPath, 1003, 976, 964, 47, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_common[pl_com_1005]"), CommonPath, 1, 611, 570, 2, FIntPoint(2048, 1024));
    AddExisting(TEXT("ui_frame[pl_fra_1000]"), FramePath, 1, 822, 524, 659, FIntPoint(1024, 2048));
    AddExisting(TEXT("ui_frame[pl_fra_1002]"), FramePath, 1, 329, 560, 60, FIntPoint(1024, 2048));
    AddExisting(TEXT("ui_page[paging_401]"), PagePath, 1, 561, 60, 60, FIntPoint(128, 2048));
    AddExisting(TEXT("ui_page[paging_401_1]"), PagePath, 1, 623, 60, 60, FIntPoint(128, 2048));
    AddExisting(TEXT("ui_page[paging_402]"), PagePath, 1, 685, 60, 60, FIntPoint(128, 2048));
    AddExisting(TEXT("ui_page[paging_402_1]"), PagePath, 1, 747, 60, 60, FIntPoint(128, 2048));
    return true;
}

bool URokResearchScreenWidget::InitializeResearch(AActor* AcademyActor)
{
    AcademyContext = AcademyActor;
    SelectedType = EconomyType;
    SelectedStudyType = 0;
    ViewedConfigLevel = 1;
    LastBoundaryActionId = INDEX_NONE;
    PendingActionId = INDEX_NONE;
    DetailAnimationPhase = EDetailAnimationPhase::Idle;
    AnimationElapsedSeconds = 0.0f;
    RootOpenElapsedSeconds = 0.0f;
    DetailCloseElapsedSeconds = 0.0f;
    bRootOpenAnimating = false;
    bDetailCloseAnimating = false;
    bDetailClosed = false;
    AnimationInterruptCount = 0;
    RenderedNodes.Reset();
    RenderedLines.Reset();
    LoadError.Empty();
    bool bLoaded = Catalog.Load(LoadError);
    if (bLoaded)
    {
        for (int32 Type : { EconomyType, MilitaryType })
        {
            TArray<FRokResearchRow> Rows;
            Catalog.GetRowsByTypeAndLevel(Type, 1, Rows);
            bLoaded = Rows.Num() == (Type == EconomyType ? 28 : 43);
            if (!bLoaded) break;
            for (const FRokResearchRow& Row : Rows)
            {
                if (Row.Columns < 1 || Row.Columns > 15 || !Row.bHasLocation || Row.Location < 1 ||
                    Row.Location >= (Type == EconomyType ? 7 : 10))
                {
                    bLoaded = false;
                    break;
                }
                RenderedNodes.Add(Row);
            }
            if (!bLoaded) break;
        }
        if (!bLoaded) LoadError = TEXT("Research source columns, locations or node count differ");
    }
    if (bLoaded) bLoaded = LoadSpriteManifest(LoadError);
    BuildTree();
    return bLoaded;
}

bool URokResearchScreenWidget::IsDetailVisible() const
{
    return DetailRoot && DetailRoot->GetVisibility() != ESlateVisibility::Collapsed;
}

#if !UE_BUILD_SHIPPING
float URokResearchScreenWidget::GetEconomyScrollOffsetForAudit() const
{
    return EconomyScroll ? EconomyScroll->GetScrollOffset() : 0.0f;
}

float URokResearchScreenWidget::GetMilitaryScrollOffsetForAudit() const
{
    return MilitaryScroll ? MilitaryScroll->GetScrollOffset() : 0.0f;
}

float URokResearchScreenWidget::GetPowerScrollOffsetForAudit() const
{
    return PowerScroll ? PowerScroll->GetScrollOffset() : 0.0f;
}

bool URokResearchScreenWidget::IsPowerPageForAudit() const
{
    return PowerRoot && PowerRoot->GetVisibility() != ESlateVisibility::Collapsed;
}

FName URokResearchScreenWidget::GetAnimationPhaseForAudit() const
{
    if (bDetailCloseAnimating) return TEXT("DetailClose");
    if (bDetailClosed) return TEXT("DetailClosed");
    switch (DetailAnimationPhase)
    {
    case EDetailAnimationPhase::PowerShow: return TEXT("PowerShow");
    case EDetailAnimationPhase::ContentShow: return TEXT("ContentShow");
    default: return bRootOpenAnimating ? TEXT("DetailOpen") : TEXT("Idle");
    }
}

float URokResearchScreenWidget::GetAnimationElapsedSecondsForAudit() const
{
    if (bDetailCloseAnimating || bDetailClosed) return DetailCloseElapsedSeconds;
    return DetailAnimationPhase == EDetailAnimationPhase::PowerShow ||
        DetailAnimationPhase == EDetailAnimationPhase::ContentShow
        ? AnimationElapsedSeconds : RootOpenElapsedSeconds;
}

float URokResearchScreenWidget::GetAnimationNormalizedTimeForAudit() const
{
    return FMath::Clamp(GetAnimationElapsedSecondsForAudit() /
        (bDetailCloseAnimating || bDetailClosed ? DetailCloseDuration : DetailShowDuration),
        0.0f, 1.0f);
}
#endif

void URokResearchScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // Advance the prior clip before dispatching a newly queued action. A
    // Play("Show") issued by this tick then starts at the source's t=0 key.
    AdvanceDetailAnimation(InDeltaTime);
    if (PendingActionId != INDEX_NONE)
    {
        const int32 Action = PendingActionId;
        PendingActionId = INDEX_NONE;
        ExecuteIndexedAction(Action);
    }
}

void URokResearchScreenWidget::ResetDetailAnimation()
{
    if (DetailAnimationPhase != EDetailAnimationPhase::Idle || bRootOpenAnimating ||
        bDetailCloseAnimating)
        ++AnimationInterruptCount;
    DetailAnimationPhase = EDetailAnimationPhase::Idle;
    AnimationElapsedSeconds = 0.0f;
    RootOpenElapsedSeconds = 0.0f;
    DetailCloseElapsedSeconds = 0.0f;
    bRootOpenAnimating = false;
    bDetailCloseAnimating = false;
    bDetailClosed = false;
    if (DetailRoot)
    {
        DetailRoot->SetRenderScale(FVector2D(1.0f));
        DetailRoot->SetRenderOpacity(1.0f);
    }
    if (DetailUpdateRoot)
    {
        DetailUpdateRoot->SetRenderTranslation(FVector2D::ZeroVector);
        DetailUpdateRoot->SetRenderOpacity(1.0f);
    }
    if (PowerRoot)
    {
        PowerRoot->SetRenderTranslation(FVector2D::ZeroVector);
        PowerRoot->SetRenderOpacity(1.0f);
    }
}

void URokResearchScreenWidget::StartDetailClose()
{
    if (!DetailRoot || DetailRoot->GetVisibility() == ESlateVisibility::Collapsed ||
        bDetailCloseAnimating) return;
    // UIManager.CloseAnimatorUI uses IsAniCloseing to ignore only another
    // CloseUI request. The root Close state replaces a still-running Open;
    // rect/img_update and rect/pl_power retain their independent Animators.
    if (bRootOpenAnimating) ++AnimationInterruptCount;
    bRootOpenAnimating = false;
    bDetailCloseAnimating = true;
    bDetailClosed = false;
    DetailCloseElapsedSeconds = 0.0f;
    ApplyDetailAnimation();
}

void URokResearchScreenWidget::SetDetailVisible(bool bVisible)
{
    const bool bWasVisible = IsDetailVisible();
    if (DetailMask) DetailMask->SetVisibility(bVisible
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (DetailRoot) DetailRoot->SetVisibility(bVisible
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bWasVisible != bVisible) OnDetailVisibilityChanged.Broadcast(bVisible);
}

void URokResearchScreenWidget::FinishDetailClose()
{
    // UA_WinAnime_T1_Off invokes CloseAnimatorCallback at 0.15s; Unity then
    // removes UI_Win_ResearchUpate. Collapse the reusable UE subtree and
    // clear its detail identity at that same endpoint.
    bDetailCloseAnimating = false;
    bDetailClosed = true;
    bRootOpenAnimating = false;
    DetailAnimationPhase = EDetailAnimationPhase::Idle;
    PendingActionId = INDEX_NONE;
    SelectedStudyType = 0;
    ViewedConfigLevel = 1;
    SetDetailVisible(false);
    if (DetailUpdateRoot) DetailUpdateRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (PowerRoot) PowerRoot->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackIcon) PowerBackIcon->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackButton) PowerBackButton->SetVisibility(ESlateVisibility::Collapsed);
}

void URokResearchScreenWidget::StartDetailAnimation(EDetailAnimationPhase Phase)
{
    if (Phase == EDetailAnimationPhase::DetailOpen)
    {
        ResetDetailAnimation();
        bRootOpenAnimating = true;
        DetailAnimationPhase = Phase;
    }
    else
    {
        // The prefab's root and child use independent Animators. More may
        // start while the root's Open clip is still running.
        if (DetailAnimationPhase == EDetailAnimationPhase::PowerShow ||
            DetailAnimationPhase == EDetailAnimationPhase::ContentShow)
            ++AnimationInterruptCount;
        // Unity disables the outgoing GameObject without resetting its
        // Animator. Keep its last transform; Play("Show") sets only the newly
        // enabled target to the first key of its own clip.
        DetailAnimationPhase = Phase;
        AnimationElapsedSeconds = 0.0f;
    }
    ApplyDetailAnimation();
}

void URokResearchScreenWidget::AdvanceDetailAnimation(float DeltaSeconds)
{
    if (!bRootOpenAnimating && !bDetailCloseAnimating &&
        DetailAnimationPhase != EDetailAnimationPhase::PowerShow &&
        DetailAnimationPhase != EDetailAnimationPhase::ContentShow) return;
    const float ClampedDelta = FMath::Max(DeltaSeconds, 0.0f);
    if (bDetailCloseAnimating)
        DetailCloseElapsedSeconds = FMath::Min(DetailCloseElapsedSeconds +
            ClampedDelta, DetailCloseDuration);
    if (bRootOpenAnimating)
        RootOpenElapsedSeconds = FMath::Min(RootOpenElapsedSeconds +
            ClampedDelta, DetailShowDuration);
    if (DetailAnimationPhase == EDetailAnimationPhase::PowerShow ||
        DetailAnimationPhase == EDetailAnimationPhase::ContentShow)
        AnimationElapsedSeconds = FMath::Min(AnimationElapsedSeconds +
            ClampedDelta, DetailShowDuration);
    ApplyDetailAnimation();
    if (bDetailCloseAnimating && DetailCloseElapsedSeconds >= DetailCloseDuration)
    {
        FinishDetailClose();
        return;
    }
    if (bRootOpenAnimating && RootOpenElapsedSeconds >= DetailShowDuration)
    {
        bRootOpenAnimating = false;
        if (DetailAnimationPhase == EDetailAnimationPhase::DetailOpen)
            DetailAnimationPhase = EDetailAnimationPhase::Idle;
    }
    if ((DetailAnimationPhase == EDetailAnimationPhase::PowerShow ||
        DetailAnimationPhase == EDetailAnimationPhase::ContentShow) &&
        AnimationElapsedSeconds >= DetailShowDuration)
        DetailAnimationPhase = EDetailAnimationPhase::Idle;
}

void URokResearchScreenWidget::ApplyDetailAnimation()
{
    // UI_Win_ResearchUpate root defaults to UA_WinAnime_T1_On. The two
    // children default to Empty; their Show clips play only on More/Back.
    if (bDetailCloseAnimating && DetailRoot)
    {
        const FRokUiWindowAnimationSample Sample = FRokUiWindowAnimation::Evaluate(
            ERokUiWindowAnimationStyle::Type1, true, DetailCloseElapsedSeconds);
        DetailRoot->SetRenderScale(FVector2D(Sample.Scale));
        DetailRoot->SetRenderOpacity(Sample.Opacity);
    }
    else if (bRootOpenAnimating && DetailRoot)
    {
        const float Scale = FRokUiWindowAnimation::Evaluate(
            ERokUiWindowAnimationStyle::Type1, false, RootOpenElapsedSeconds).Scale;
        DetailRoot->SetRenderScale(FVector2D(Scale));
    }
    if (DetailAnimationPhase == EDetailAnimationPhase::PowerShow && PowerRoot)
    {
        const RokArmyShowPanelAnimation::FSample Sample =
            RokArmyShowPanelAnimation::Evaluate(AnimationElapsedSeconds, true);
        PowerRoot->SetRenderTranslation(FVector2D(Sample.X, 0.0f));
        PowerRoot->SetRenderOpacity(Sample.Opacity);
    }
    else if (DetailAnimationPhase == EDetailAnimationPhase::ContentShow && DetailUpdateRoot)
    {
        const RokArmyShowPanelAnimation::FSample Sample =
            RokArmyShowPanelAnimation::Evaluate(AnimationElapsedSeconds, false);
        DetailUpdateRoot->SetRenderTranslation(FVector2D(Sample.X, 0.0f));
        DetailUpdateRoot->SetRenderOpacity(Sample.Opacity);
    }
}

void URokResearchScreenWidget::HandleIndexedAction(int32 ActionId)
{
    if (PendingActionId == INDEX_NONE) PendingActionId = ActionId;
}

void URokResearchScreenWidget::Place(UCanvasPanel* Parent, UWidget* Child,
    const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
{
    UCanvasPanelSlot* CanvasSlot = Parent->AddChildToCanvas(Child);
    CanvasSlot->SetPosition(Position);
    CanvasSlot->SetSize(Size);
    CanvasSlot->SetZOrder(ZOrder);
}

UImage* URokResearchScreenWidget::MakeSprite(const FName& Name,
    const FString& Reference, const FVector2D& Size, bool bUseSourceGray)
{
    UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
    ApplySprite(Image, Reference, Size, bUseSourceGray);
    return Image;
}

bool URokResearchScreenWidget::ApplySprite(UImage* Image,
    const FString& Reference, const FVector2D& Size, bool bUseSourceGray)
{
    const FRokResearchSprite* Sprite = Sprites.Find(Reference);
    if (!Sprite)
    {
        Image->SetVisibility(ESlateVisibility::Collapsed);
        return false;
    }
    FString ObjectPath = Sprite->AssetPath;
    if (!ObjectPath.Contains(TEXT(".")))
    {
        ObjectPath += TEXT(".") + FPaths::GetBaseFilename(Sprite->AssetPath);
    }
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
    if (!Texture)
    {
        Image->SetVisibility(ESlateVisibility::Collapsed);
        return false;
    }
    FSlateBrush Brush;
    Brush.SetResourceObject(Texture);
    Brush.ImageSize = Size;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.Tiling = ESlateBrushTileType::NoTile;
    Brush.ImageType = ESlateBrushImageType::FullColor;
    const float Top = static_cast<float>(Sprite->AtlasSize.Y - Sprite->Rect.Max.Y);
    Brush.SetUVRegion(FBox2f(
        FVector2f(static_cast<float>(Sprite->Rect.Min.X) / Sprite->AtlasSize.X, Top / Sprite->AtlasSize.Y),
        FVector2f(static_cast<float>(Sprite->Rect.Max.X) / Sprite->AtlasSize.X,
            (Top + Sprite->Rect.Height()) / Sprite->AtlasSize.Y)));
    if (bUseSourceGray)
    {
        UMaterialInterface* Gray = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/RokPrototype/UI/Research/M_RokResearchGrayUI.M_RokResearchGrayUI"));
        if (!Gray)
        {
            UE_LOG(LogTemp, Error, TEXT("Research source gray UI material is missing"));
            Image->SetVisibility(ESlateVisibility::Collapsed);
            return false;
        }
        UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Gray, Image);
        Material->SetTextureParameterValue(TEXT("SourceAtlas"), Texture);
        Brush.SetResourceObject(Material);
    }
    Image->SetBrush(Brush);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    return true;
}

UCanvasPanel* URokResearchScreenWidget::MakeNineSlice(const FName& Name,
    const FString& Reference, const FVector2D& Size, const FMargin& SourceMargin)
{
    UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
    Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
    const FRokResearchSprite* Base = Sprites.Find(Reference);
    if (!Base) return Panel;
    const FRokResearchSprite Template = *Base;
    const FIntRect Source = Base->Rect;
    const int32 Left = FMath::RoundToInt(Source.Width() * SourceMargin.Left);
    const int32 Right = FMath::RoundToInt(Source.Width() * SourceMargin.Right);
    const int32 Top = FMath::RoundToInt(Source.Height() * SourceMargin.Top);
    const int32 Bottom = FMath::RoundToInt(Source.Height() * SourceMargin.Bottom);
    const int32 CenterWidth = Source.Width() - Left - Right;
    const int32 CenterHeight = Source.Height() - Top - Bottom;
    const float CenterDestWidth = Size.X - Left - Right;
    const float CenterDestHeight = Size.Y - Top - Bottom;
    const FString Stem = Name.ToString();
    auto AddPatch = [&](const TCHAR* Suffix, int32 X, int32 Y, int32 Width, int32 Height,
        float DX, float DY, float DW, float DH)
    {
        if (DW <= 0 || DH <= 0) return;
        // Keep a zero-width source center as a constant-U quad, not a 1 px
        // strip: pb_com_1000_1 has L10/R10 borders on a 20 px sprite. UE's
        // ElementBatcher accepts a valid BrushUV with SizeUV.X == 0 and still
        // draws the nonzero destination quad at that atlas seam.
        if (Width < 0 || Height < 0) return;
        FRokResearchSprite Patch = Template;
        Patch.Rect = FIntRect(X, Y, X + Width, Y + Height);
        Patch.Reference = FString::Printf(TEXT("%s_%s_Sprite"), *Stem, Suffix);
        Sprites.Add(Patch.Reference, Patch);
        UImage* Image = MakeSprite(*FString::Printf(TEXT("%s_%s"), *Stem, Suffix),
            Patch.Reference, FVector2D(DW, DH));
        Place(Panel, Image, FVector2D(DX, DY), FVector2D(DW, DH));
    };
    const int32 X0 = Source.Min.X, X1 = X0 + Left, X2 = Source.Max.X - Right;
    const int32 Y0 = Source.Min.Y, Y1 = Y0 + Bottom, Y2 = Source.Max.Y - Top;
    AddPatch(TEXT("TopLeft"), X0, Y2, Left, Top, 0, 0, Left, Top);
    AddPatch(TEXT("Top"), X1, Y2, CenterWidth, Top, Left, 0, CenterDestWidth, Top);
    AddPatch(TEXT("TopRight"), X2, Y2, Right, Top, Size.X - Right, 0, Right, Top);
    AddPatch(TEXT("Left"), X0, Y1, Left, CenterHeight, 0, Top, Left, CenterDestHeight);
    AddPatch(TEXT("Center"), X1, Y1, CenterWidth, CenterHeight,
        Left, Top, CenterDestWidth, CenterDestHeight);
    AddPatch(TEXT("Right"), X2, Y1, Right, CenterHeight,
        Size.X - Right, Top, Right, CenterDestHeight);
    AddPatch(TEXT("BottomLeft"), X0, Y0, Left, Bottom,
        0, Size.Y - Bottom, Left, Bottom);
    AddPatch(TEXT("Bottom"), X1, Y0, CenterWidth, Bottom,
        Left, Size.Y - Bottom, CenterDestWidth, Bottom);
    AddPatch(TEXT("BottomRight"), X2, Y0, Right, Bottom,
        Size.X - Right, Size.Y - Bottom, Right, Bottom);
    return Panel;
}

UTextBlock* URokResearchScreenWidget::MakeText(const FName& Name, const FString& Value,
    float PixelSize, const FLinearColor& Color, bool bBold)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
    Text->SetText(FText::FromString(Value));
    Text->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"),
        PixelSize * 72.0f / 96.0f));
    Text->SetColorAndOpacity(FSlateColor(Color));
    Text->SetJustification(ETextJustify::Center);
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Text;
}

URokUnityIndexedButton* URokResearchScreenWidget::MakeButton(const FName& Name, int32 ActionId)
{
    URokUnityIndexedButton* Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(
        URokUnityIndexedButton::StaticClass(), Name);
    FSlateBrush EmptyBrush;
    EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
    FButtonStyle Style;
    Style.SetNormal(EmptyBrush).SetHovered(EmptyBrush).SetPressed(EmptyBrush).SetDisabled(EmptyBrush);
    Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    Button->SetStyle(Style);
    Button->ConfigureAction(ActionId);
    Button->OnIndexedClicked.AddUObject(this, &URokResearchScreenWidget::HandleIndexedAction);
    return Button;
}

void URokResearchScreenWidget::SetSelectedType(int32 Type)
{
    if (Type != EconomyType && Type != MilitaryType) return;
    if (Type != SelectedType && DetailRoot && !bDetailCloseAnimating)
    {
        ResetDetailAnimation();
        SetDetailVisible(false);
    }
    SelectedType = Type;
    if (EconomyScroll) EconomyScroll->SetVisibility(Type == EconomyType
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (MilitaryScroll) MilitaryScroll->SetVisibility(Type == MilitaryType
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EconomySelected) EconomySelected->SetVisibility(Type == EconomyType
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (MilitarySelected) MilitarySelected->SetVisibility(Type == MilitaryType
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    for (int32 PageType : { EconomyType, MilitaryType })
    {
        const FString Prefix = PageType == EconomyType ? TEXT("ResearchEconomy") : TEXT("ResearchMilitary");
        const bool bSelected = PageType == Type;
        if (UWidget* Icon = WidgetTree->FindWidget(*FString::Printf(TEXT("%sIcon"), *Prefix)))
            Icon->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (UWidget* Icon = WidgetTree->FindWidget(*FString::Printf(TEXT("%sDarkIcon"), *Prefix)))
            Icon->SetVisibility(bSelected ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        if (UWidget* Dark = WidgetTree->FindWidget(*FString::Printf(TEXT("%sDark"), *Prefix)))
            Dark->SetVisibility(bSelected ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (WindowTitle) WindowTitle->SetText(FText::FromString(Type == EconomyType
        ? TEXT("经济科技") : TEXT("军事科技")));
}

void URokResearchScreenWidget::ShowSelectedDetail()
{
    if (!DetailRoot || SelectedStudyType <= 0) return;
    const bool bOpeningNewDetail = DetailRoot->GetVisibility() == ESlateVisibility::Collapsed;
    FRokResearchRow Row;
    if (!Catalog.GetByStudyLevel(SelectedStudyType, ViewedConfigLevel, Row)) return;
    if (DetailIcon) ApplySprite(DetailIcon, Row.IconSource, FVector2D(160, 160));
    if (DetailTitle) DetailTitle->SetText(FText::FromString(Row.Name));
    if (DetailDescription) DetailDescription->SetText(FText::FromString(Row.Description));
    if (DetailLevel) DetailLevel->SetText(FText::FromString(FString::Printf(
        TEXT("账号等级 —   ▷   配置等级 %d/%d"), Row.Level, Row.MaxLevel)));
    if (DetailProgress) DetailProgress->SetText(FText::FromString(FString::Printf(
        TEXT("--/%d"), Row.MaxLevel)));
    FRokResearchRow Previous;
    const bool bHasPrevious = Row.Level > 1 &&
        Catalog.GetByStudyLevel(Row.StudyType, Row.Level - 1, Previous);
    const bool bHasAttribute = !Row.DescriptionData.IsEmpty();
    for (const TCHAR* RegionName : { TEXT("ResearchDetailAttrNameRegion"),
        TEXT("ResearchDetailAttrBeforeRegion"), TEXT("ResearchDetailAttrDeltaRegion") })
    {
        if (UWidget* Region = WidgetTree->FindWidget(RegionName))
            Region->SetVisibility(bHasAttribute ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed);
    }
    if (DetailAttrName) DetailAttrName->SetText(FText::FromString(Row.BuffDescription));
    if (DetailAttrBefore) DetailAttrBefore->SetText(FText::FromString(FString::Printf(
        TEXT("配置前 %s"), bHasPrevious ? *Previous.DescriptionData : TEXT("0.0%"))));
    if (DetailAttrDelta && bHasAttribute)
    {
        const double Before = bHasPrevious ? FCString::Atod(*Previous.DescriptionData) : 0.0;
        const double After = FCString::Atod(*Row.DescriptionData);
        DetailAttrDelta->SetText(FText::FromString(FString::Printf(TEXT("+%.1f%%"), After - Before)));
    }
    for (const TCHAR* RegionName : { TEXT("ResearchDetailPowerNameRegion"),
        TEXT("ResearchDetailPowerBeforeRegion"), TEXT("ResearchDetailPowerDeltaRegion") })
    {
        if (UWidget* Region = WidgetTree->FindWidget(RegionName))
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Region->Slot))
                CanvasSlot->SetPosition(FVector2D(CanvasSlot->GetPosition().X,
                    bHasAttribute ? 298.7f : 258.7f));
    }
    if (UWidget* Panel = WidgetTree->FindWidget(TEXT("ResearchDetailAttrPanel")))
        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
            CanvasSlot->SetSize(FVector2D(536.0f, bHasAttribute ? 100.0f : 60.0f));
    if (DetailPowerBefore) DetailPowerBefore->SetText(FText::FromString(FString::Printf(
        TEXT("配置前 %lld"), bHasPrevious ? Previous.Power : 0LL)));
    if (DetailPowerDelta) DetailPowerDelta->SetText(FText::FromString(FString::Printf(
        TEXT("+%lld"), Row.Power - (bHasPrevious ? Previous.Power : 0LL))));
    if (DetailCosts)
    {
        DetailCosts->SetText(FText::FromString(
            TEXT("来源配置需求 · 账号资源、研究速度和队列未同步")));
    }
    const int64 Values[] = { Row.BaseFood, Row.BaseWood, Row.BaseStone,
        Row.BaseGold, Row.BaseSeconds, Row.CampusLevel };
    const TCHAR* Labels[] = { TEXT("食物"), TEXT("木材"), TEXT("石料"),
        TEXT("金币"), TEXT("基础秒数"), TEXT("学院要求") };
    for (int32 Index = 0; Index < DetailCostValues.Num() && Index < 6; ++Index)
        DetailCostValues[Index]->SetText(FText::FromString(FString::Printf(
            TEXT("%s  %lld"), Labels[Index], Values[Index])));
    RefreshPowerRows(Row);
    if (DetailUpdateRoot) DetailUpdateRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (PowerRoot) PowerRoot->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackIcon) PowerBackIcon->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackButton) PowerBackButton->SetVisibility(ESlateVisibility::Collapsed);
    SetDetailVisible(true);
    if (bOpeningNewDetail) StartDetailAnimation(EDetailAnimationPhase::DetailOpen);
}

void URokResearchScreenWidget::ExecuteIndexedAction(int32 ActionId)
{
    if (ActionId == CloseAction)
    {
        OnCloseRequested.Broadcast();
        return;
    }
    if (ActionId == EconomyAction || ActionId == MilitaryAction)
    {
        SetSelectedType(ActionId == EconomyAction ? EconomyType : MilitaryType);
        return;
    }
    if (ActionId >= NodeActionBase)
    {
        const int32 RequestedStudyType = ActionId - NodeActionBase;
        const FRokResearchRow* Found = RenderedNodes.FindByPredicate(
            [RequestedStudyType, this](const FRokResearchRow& Row)
            { return Row.StudyType == RequestedStudyType && Row.Type == SelectedType; });
        if (Found)
        {
            SelectedStudyType = RequestedStudyType;
            ViewedConfigLevel = 1;
            ShowSelectedDetail();
        }
        return;
    }
    if (ActionId == BackAction)
    {
        StartDetailClose();
        return;
    }
    if (ActionId == MoreAction)
    {
        if (DetailUpdateRoot) DetailUpdateRoot->SetVisibility(ESlateVisibility::Collapsed);
        if (PowerRoot) PowerRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (PowerBackIcon) PowerBackIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (PowerBackButton) PowerBackButton->SetVisibility(ESlateVisibility::Visible);
        if (PowerScroll) PowerScroll->SetScrollOffset(0.0f);
        StartDetailAnimation(EDetailAnimationPhase::PowerShow);
        return;
    }
    if (ActionId == PowerBackAction)
    {
        if (PowerRoot) PowerRoot->SetVisibility(ESlateVisibility::Collapsed);
        if (PowerBackIcon) PowerBackIcon->SetVisibility(ESlateVisibility::Collapsed);
        if (PowerBackButton) PowerBackButton->SetVisibility(ESlateVisibility::Collapsed);
        if (DetailUpdateRoot) DetailUpdateRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        StartDetailAnimation(EDetailAnimationPhase::ContentShow);
        return;
    }
    if (ActionId == PreviousLevelAction || ActionId == NextLevelAction)
    {
        FRokResearchRow Current;
        if (Catalog.GetByStudyLevel(SelectedStudyType, ViewedConfigLevel, Current))
        {
            ViewedConfigLevel = FMath::Clamp(ViewedConfigLevel +
                (ActionId == NextLevelAction ? 1 : -1), 1, Current.MaxLevel);
            ShowSelectedDetail();
        }
        return;
    }
    if (ActionId == BoundaryAction)
    {
        // ResearchUpdateMediator normally sends account/server technology
        // requests. This source-config preview only records the local intent.
        LastBoundaryActionId = ActionId;
        if (DetailCosts) DetailCosts->SetText(FText::FromString(
            TEXT("研究队列、资源与账号等级未同步；当前仅浏览来源配置。")));
    }
}

void URokResearchScreenWidget::BuildTree()
{
    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("ResearchCanvas"));
    WidgetTree->RootWidget = RootCanvas;
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    if (!LoadError.IsEmpty())
    {
        UTextBlock* Error = MakeText(TEXT("ResearchLoadError"),
            FString::Printf(TEXT("研究来源数据不可用：%s"), *LoadError), 20.0f,
            FLinearColor::White);
        Place(RootCanvas, Error, FVector2D(255, 340), FVector2D(930, 80), 8);
        return;
    }

    // Original UI_Win_ResearchMain/img_bg over the Type1 frame.
    Place(RootCanvas, MakeSprite(TEXT("ResearchBackground"),
        TEXT("img_research_bg[img_research_bg]"), FVector2D(988.1f, 658.1f)),
        FVector2D(226.0f, 103.0f), FVector2D(988.1f, 658.1f), 0);

    UBorder* TitleRegion = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("ResearchWindowTitleRegion"));
    TitleRegion->SetBrushColor(FLinearColor::Transparent);
    TitleRegion->SetPadding(FMargin(0));
    TitleRegion->SetVisibility(ESlateVisibility::HitTestInvisible);
    TitleRegion->SetHorizontalAlignment(HAlign_Center);
    TitleRegion->SetVerticalAlignment(VAlign_Center);
    WindowTitle = MakeText(TEXT("ResearchWindowTitle"), TEXT("经济科技"),
        30.0f, FLinearColor::White);
    TitleRegion->SetContent(WindowTitle);
    Place(RootCanvas, TitleRegion, FVector2D(139.5f, 76.4f),
        FVector2D(1161.1f, 40.0f), 8);

    EconomyScroll = BuildPage(EconomyType);
    MilitaryScroll = BuildPage(MilitaryType);

    for (int32 Type : { EconomyType, MilitaryType })
    {
        const bool bSelected = Type == EconomyType;
        const float Y = bSelected ? 188.1f : 301.1f;
        const FString Prefix = Type == EconomyType ? TEXT("ResearchEconomy") : TEXT("ResearchMilitary");
        UImage* Selection = MakeSprite(*FString::Printf(TEXT("%sSelected"), *Prefix),
            TEXT("ui_common[btn_com_1008_1]"), FVector2D(145, 103));
        if (Type == EconomyType) EconomySelected = Selection;
        else MilitarySelected = Selection;
        Place(RootCanvas, Selection, FVector2D(1258, Y), FVector2D(145, 103), 5);
        UImage* Dark = MakeSprite(*FString::Printf(TEXT("%sDark"), *Prefix),
            TEXT("ui_common[btn_com_1008_2]"), FVector2D(121, 102));
        Place(RootCanvas, Dark, FVector2D(1258, Y + 0.5f), FVector2D(121, 102), 4);
        const FString IconRef = Type == EconomyType ? TEXT("ui_page[paging_401]") :
            TEXT("ui_page[paging_402]");
        const FString DarkIconRef = Type == EconomyType ? TEXT("ui_page[paging_401_1]") :
            TEXT("ui_page[paging_402_1]");
        Place(RootCanvas, MakeSprite(*FString::Printf(TEXT("%sDarkIcon"), *Prefix),
            DarkIconRef, FVector2D(60, 60)), FVector2D(1282.8f, Y + 21.5f), FVector2D(60, 60), 6);
        UImage* Icon = MakeSprite(*FString::Printf(TEXT("%sIcon"), *Prefix),
            IconRef, FVector2D(60, 60));
        Place(RootCanvas, Icon, FVector2D(1283.4f, Y + 21.5f), FVector2D(60, 60), 7);
        // Selected/highlight sprites switch together; the opaque hit area
        // retains the source checkbox bounds even when its art is hidden.
        const bool bVisible = Type == SelectedType;
        Selection->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        Icon->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        Dark->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        URokUnityIndexedButton* Hit = MakeButton(Type == EconomyType
            ? TEXT("ResearchEconomyTab") : TEXT("ResearchMilitaryTab"),
            Type == EconomyType ? EconomyAction : MilitaryAction);
        Place(RootCanvas, Hit, FVector2D(1259.0f, Type == EconomyType ? 193.0f : 303.8f),
            FVector2D(131.0f, 94.8f), 9);
    }

    UTextBlock* Notice = MakeText(TEXT("ResearchAccountStateNotice"),
        TEXT("来源配置预览 · 学院、账号研究等级和队列未同步"), 14.0f,
        FLinearColor::FromSRGBColor(FColor(220, 214, 197)));
    Place(RootCanvas, Notice, FVector2D(350, 737), FVector2D(740, 23), 6);
    BuildDetail();
    SetSelectedType(EconomyType);
}

UScrollBox* URokResearchScreenWidget::BuildPage(int32 Type)
{
    const FString Prefix = Type == EconomyType ? TEXT("ResearchEconomy") : TEXT("ResearchMilitary");
    UCanvasPanel* Region = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sScrollRegion"), *Prefix));
    Place(RootCanvas, Region, FVector2D(191.71f, 121.48f), FVector2D(1056.59f, 644.05f), 2);
    UScrollBox* OutScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
        *FString::Printf(TEXT("%sScroll"), *Prefix));
    OutScroll->SetOrientation(EOrientation::Orient_Horizontal);
    OutScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    OutScroll->SetAnimateWheelScrolling(false);
    Place(Region, OutScroll, FVector2D::ZeroVector, FVector2D(1056.59f, 641.05f));
    USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
        *FString::Printf(TEXT("%sExtent"), *Prefix));
    Extent->SetWidthOverride(6305.025f);
    Extent->SetHeightOverride(666.05f);
    OutScroll->AddChild(Extent);
    UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
        *FString::Printf(TEXT("%sContent"), *Prefix));
    Content->SetRenderTranslation(FVector2D(0, -12.5f));
    Extent->SetContent(Content);

    for (int32 ColumnIndex = 1; ColumnIndex <= 15; ++ColumnIndex)
    {
        UCanvasPanel* Column = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
            *FString::Printf(TEXT("ResearchColumn_%d_%d"), Type, ColumnIndex));
        Place(Content, Column, FVector2D((ColumnIndex - 1) * 420.335f, 23.0f),
            FVector2D(420.335f, 620.05f));
        int32 ItemIndex = 0;
        for (const FRokResearchRow& Row : RenderedNodes)
        {
            if (Row.Type == Type && Row.Columns == ColumnIndex)
                BuildNode(Column, Row, Type, ItemIndex++);
        }
    }
    return OutScroll;
}

void URokResearchScreenWidget::BuildNode(UCanvasPanel* Column,
    const FRokResearchRow& Row, int32 Type, int32 ItemIndex)
{
    static const float EconomyPositions[] = {0, 210, 105, 0, -105, -210, 160};
    static const float MilitaryPositions[] = {0, 228, 158, 150, 76, 0, -76, -150, -158, -228};
    const float SourceY = Type == EconomyType
        ? EconomyPositions[Row.Location] : MilitaryPositions[Row.Location];
    UCanvasPanel* Node = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
        *FString::Printf(TEXT("ResearchNode_%d"), Row.StudyType));
    // UI_ResearchItem prefab's third child item has a source-local X offset
    // of +2 px; this is visible in every three-or-four-node native column.
    Place(Column, Node, FVector2D(102.16f + (ItemIndex == 2 ? 2.0f : 0.0f),
        260.02f - SourceY), FVector2D(100, 100), 1);

    const FRokResearchRenderedLine Line = DescribeSourceLine(Row, RenderedNodes);
    if (!Line.PrimarySprite.IsEmpty() || !Line.SecondarySprite.IsEmpty())
    {
        RenderedLines.Add(Line);
        if (const FRokResearchSprite* Sprite = Sprites.Find(Line.PrimarySprite))
        {
            const FVector2D Size(Sprite->Rect.Width(), Sprite->Rect.Height());
            float Y = 50.0f - Size.Y * 0.5f;
            if (Line.SameColumnChildCount == 1 && Line.CrossColumnChildCount == 0)
            {
                const FRokResearchRow* Child = RenderedNodes.FindByPredicate(
                    [&Row](const FRokResearchRow& Candidate)
                    {
                        return Candidate.Type == Row.Type && Candidate.Columns == Row.Columns + 1 &&
                            Candidate.Preconditions.ContainsByPredicate(
                                [&Row](const FRokResearchPrerequisite& Prerequisite)
                                { return Prerequisite.StudyType == Row.StudyType; });
                    });
                if (Child && Child->Location != Row.Location)
                    Y = Line.bPrimaryMirroredY ? 52.5f - Size.Y : 47.5f;
            }
            else if (Line.SameColumnChildCount == 1 && Line.CrossColumnChildCount > 0)
            {
                // UI_ResearchSubItem_SubView offsets the mixed one-child
                // branch by half the sprite height after LoadSprite completes.
                Y = 50.0f - Size.Y;
            }
            UImage* Primary = MakeSprite(*FString::Printf(TEXT("ResearchLine_%d_Primary"), Row.StudyType),
                Line.PrimarySprite, Size);
            Primary->SetRenderTransformPivot(FVector2D(0.5f));
            Primary->SetRenderScale(FVector2D(-1.0f, Line.bPrimaryMirroredY ? -1.0f : 1.0f));
            // The Unity image keeps its X center when LoadSprite changes its
            // native width; the 127 px starter sprite alone has left 194.5.
            Place(Node, Primary, FVector2D(258.0f - Size.X * 0.5f, Y), Size, 0);
        }
        if (const FRokResearchSprite* Sprite = Sprites.Find(Line.SecondarySprite))
        {
            const FVector2D Size(540.0f,
                Line.CrossColumnChildCount == 4 ? 446.0f : 5.0f);
            UImage* Secondary = MakeSprite(*FString::Printf(TEXT("ResearchLine_%d_Secondary"), Row.StudyType),
                Line.SecondarySprite, Size);
            Secondary->SetRenderTransformPivot(FVector2D(0.5f));
            Secondary->SetRenderScale(FVector2D(-1.0f, 1.0f));
            // img_line2 starts as a 553.2 px image centered 471.1 px from
            // the node and is resized to 540 px for the cross-column line.
            Place(Node, Secondary, FVector2D(471.1f - Size.X * 0.5f,
                49.2f - Size.Y * 0.5f), Size, 0);
        }
    }

    const bool bHasPrerequisite = Row.Preconditions.ContainsByPredicate(
        [](const FRokResearchPrerequisite& Prerequisite) { return Prerequisite.StudyType > 0; });
    const FString Background = bHasPrerequisite
        ? TEXT("ui_research[img_research_btnBgDark]")
        : TEXT("ui_research[img_research_btnBgLight]");
    Place(Node, MakeSprite(*FString::Printf(TEXT("ResearchNodeBackground_%d"), Row.StudyType),
        Background, FVector2D(308, 100)), FVector2D(-104, 0), FVector2D(308, 100), 1);
    Place(Node, MakeSprite(*FString::Printf(TEXT("ResearchNodeIconBackground_%d"), Row.StudyType),
        TEXT("ui_research[img_research_iconBg]"), FVector2D(104, 104), bHasPrerequisite),
        FVector2D(-90.6f, -14), FVector2D(104, 104), 2);
    Place(Node, MakeSprite(*FString::Printf(TEXT("ResearchNodeIcon_%d"), Row.StudyType),
        Row.IconSource, FVector2D(88, 88), bHasPrerequisite),
        FVector2D(-82.6f, -6), FVector2D(88, 88), 3);

    UBorder* NameRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
        *FString::Printf(TEXT("ResearchNodeNameRegion_%d"), Row.StudyType));
    NameRegion->SetBrushColor(FLinearColor::Transparent);
    NameRegion->SetPadding(FMargin(0));
    NameRegion->SetHorizontalAlignment(HAlign_Left);
    NameRegion->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Name = MakeText(*FString::Printf(TEXT("ResearchNodeName_%d"), Row.StudyType),
        Row.Name, 18.0f, FLinearColor::White);
    Name->SetJustification(ETextJustify::Left);
    NameRegion->SetContent(Name);
    Place(Node, NameRegion, FVector2D(27.51f, 11.8f), FVector2D(162.65f, 35.57f), 4);

    Place(Node, MakeNineSlice(*FString::Printf(TEXT("ResearchNodeProgressFrame_%d"), Row.StudyType),
        TEXT("ui_common[pb_com_1000_0]"), FVector2D(168.09f, 38.09f),
        FMargin(14.0f / 45.0f, 18.0f / 40.0f, 18.0f / 45.0f, 14.0f / 40.0f)),
        FVector2D(19.44f, 48.5f), FVector2D(168.09f, 38.09f), 3);
    Place(Node, MakeNineSlice(*FString::Printf(TEXT("ResearchNodeProgressEmpty_%d"), Row.StudyType),
        TEXT("ui_common[pb_com_1000_1]"), FVector2D(150.57f, 20.75f),
        FMargin(10.0f / 20.0f, 7.0f / 23.0f, 10.0f / 20.0f, 6.0f / 23.0f)),
        FVector2D(27.8f, 57.25f), FVector2D(150.57f, 20.75f), 4);
    UBorder* LevelRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
        *FString::Printf(TEXT("ResearchNodeLevelRegion_%d"), Row.StudyType));
    LevelRegion->SetBrushColor(FLinearColor::Transparent);
    LevelRegion->SetPadding(FMargin(0));
    LevelRegion->SetHorizontalAlignment(HAlign_Center);
    LevelRegion->SetVerticalAlignment(VAlign_Center);
    LevelRegion->SetContent(MakeText(*FString::Printf(TEXT("ResearchNodeLevel_%d"), Row.StudyType),
        FString::Printf(TEXT("--/%d"), Row.MaxLevel), 16.0f, FLinearColor::White));
    Place(Node, LevelRegion, FVector2D(27.8f, 57.25f), FVector2D(150.57f, 20.75f), 5);
    Place(Node, MakeButton(*FString::Printf(TEXT("ResearchNodeHit_%d"), Row.StudyType),
        NodeActionBase + Row.StudyType), FVector2D(-104, 0), FVector2D(308, 100), 9);
}

void URokResearchScreenWidget::BuildDetail()
{
    // UIManager.AddMask inserts a separate, fixed full-layer overlay below
    // UI_Win_ResearchUpate and above the ResearchMain window. It remains
    // active until CloseAnimatorCallback removes the detail window.
    DetailMask = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("ResearchDetailMask"));
    Place(RootCanvas, DetailMask, FVector2D::ZeroVector, FVector2D(1440, 810), 19);
    UBorder* MaskShade = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("ResearchDetailMaskShade"));
    MaskShade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
    MaskShade->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(DetailMask, MaskShade, FVector2D::ZeroVector, FVector2D(1440, 810));
    URokUnityIndexedButton* MaskHit = MakeButton(TEXT("ResearchDetailMaskHit"), BackAction);
    MaskHit->SetPressAnimationEnabled(false);
    Place(DetailMask, MaskHit, FVector2D::ZeroVector, FVector2D(1440, 810), 1);
    DetailMask->SetVisibility(ESlateVisibility::Collapsed);

    DetailRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("ResearchDetailRoot"));
    DetailRoot->SetRenderTransformPivot(FVector2D(0.5f));
    Place(RootCanvas, DetailRoot, FVector2D::ZeroVector, FVector2D(1440, 810), 20);

    // Unity's frame Images retain raycastTarget and consume clicks over the
    // two opaque window panels. Our atlas Images are decorative, so add
    // transparent hit regions at those source rects beneath child buttons.
    auto AddFrameHit = [this](const TCHAR* Name, const FVector2D& Position,
        const FVector2D& Size)
    {
        UBorder* Hit = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), Name);
        Hit->SetBrushColor(FLinearColor::Transparent);
        Hit->SetVisibility(ESlateVisibility::Visible);
        Place(DetailRoot, Hit, Position, Size, -1);
    };
    AddFrameHit(TEXT("ResearchDetailBodyHitLeft"),
        FVector2D(187, 94.2f), FVector2D(543, 660.8f));
    AddFrameHit(TEXT("ResearchDetailBodyHitRight"),
        FVector2D(710, 94.2f), FVector2D(543, 660.8f));
    AddFrameHit(TEXT("ResearchDetailHeaderHitLeft"),
        FVector2D(167, 63), FVector2D(560, 60));
    AddFrameHit(TEXT("ResearchDetailHeaderHitRight"),
        FVector2D(713, 63), FVector2D(560, 60));

    const FMargin BodyMargin(493.0f / 524.0f, 278.0f / 659.0f,
        15.0f / 524.0f, 291.0f / 659.0f);
    const FMargin HeaderMargin(539.0f / 560.0f, 0.0f,
        8.0f / 560.0f, 0.0f);
    UCanvasPanel* Body1 = MakeNineSlice(TEXT("ResearchDetailFrame1"),
        TEXT("ui_frame[pl_fra_1000]"), FVector2D(543, 660.8f), BodyMargin);
    UCanvasPanel* Body2 = MakeNineSlice(TEXT("ResearchDetailFrame2"),
        TEXT("ui_frame[pl_fra_1000]"), FVector2D(543, 660.8f), BodyMargin);
    Body2->SetRenderTransformPivot(FVector2D(0.5f));
    Body2->SetRenderScale(FVector2D(-1, 1));
    Place(DetailRoot, Body1, FVector2D(187, 94.2f), FVector2D(543, 660.8f));
    Place(DetailRoot, Body2, FVector2D(710, 94.2f), FVector2D(543, 660.8f));
    UCanvasPanel* Header1 = MakeNineSlice(TEXT("ResearchDetailFrame3"),
        TEXT("ui_frame[pl_fra_1002]"), FVector2D(560, 60), HeaderMargin);
    UCanvasPanel* Header2 = MakeNineSlice(TEXT("ResearchDetailFrame4"),
        TEXT("ui_frame[pl_fra_1002]"), FVector2D(560, 60), HeaderMargin);
    Header2->SetRenderTransformPivot(FVector2D(0.5f));
    Header2->SetRenderScale(FVector2D(-1, 1));
    Place(DetailRoot, Header1, FVector2D(167, 63), FVector2D(560, 60), 1);
    Place(DetailRoot, Header2, FVector2D(713, 63), FVector2D(560, 60), 1);

    DetailUpdateRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("ResearchDetailUpdateRoot"));
    DetailUpdateRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Place(DetailRoot, DetailUpdateRoot, FVector2D::ZeroVector,
        FVector2D(1440, 810), 2);

    // UI_Win_ResearchUpate/rect/img_update/pl_left source parchment tree.
    Place(DetailUpdateRoot, MakeSprite(TEXT("ResearchDetailParchmentBacking"),
        TEXT("ui_common[pl_com_1008]"), FVector2D(441, 620.6f)),
        FVector2D(194.4f, 114.1f), FVector2D(441, 620.6f), 2);
    UImage* Parchment = MakeSprite(TEXT("ResearchDetailParchment"),
        TEXT("ui_research[img_research_paperBg]"), FVector2D(468, 609));
    Parchment->SetRenderTransformPivot(FVector2D(0.5f));
    Parchment->SetRenderScale(FVector2D(-1, 1));
    Place(DetailUpdateRoot, Parchment,
        FVector2D(184, 118.7f), FVector2D(468, 609), 3);
    Place(DetailUpdateRoot, MakeSprite(TEXT("ResearchDetailIconBackground"),
        TEXT("ui_research[img_research_iconBg]"), FVector2D(190, 190)),
        FVector2D(322, 277.5f), FVector2D(190, 190), 4);
    DetailIcon = MakeSprite(TEXT("ResearchDetailIcon"),
        RenderedNodes.IsEmpty() ? TEXT("") : RenderedNodes[0].IconSource,
        FVector2D(160, 160));
    Place(DetailUpdateRoot, DetailIcon, FVector2D(337, 292.2f), FVector2D(160, 160), 5);
    UImage* Chest = MakeSprite(TEXT("ResearchDetailChest"),
        TEXT("ui_research[img_research_box]"), FVector2D(149, 145));
    Chest->SetRenderTransformPivot(FVector2D(0.5f));
    Chest->SetRenderScale(FVector2D(-1, 1));
    Place(DetailUpdateRoot, Chest,
        FVector2D(146.5f, 638.7f), FVector2D(149, 145), 5);
    Place(DetailUpdateRoot, MakeSprite(TEXT("ResearchDetailMoreIcon"),
        TEXT("ui_common[btn_com_1007]"), FVector2D(38, 38)),
        FVector2D(237, 215.2f), FVector2D(38, 38), 8);
    Place(DetailUpdateRoot, MakeButton(TEXT("ResearchDetailMore"), MoreAction),
        FVector2D(237, 215.2f), FVector2D(38, 38), 9);

    auto TextRegion = [this](UCanvasPanel* Parent, const FName& RegionName,
        UTextBlock* Text, const FVector2D& Position, const FVector2D& Size,
        EHorizontalAlignment Alignment, int32 Z)
    {
        UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), RegionName);
        Region->SetBrushColor(FLinearColor::Transparent);
        Region->SetPadding(FMargin(0));
        Region->SetHorizontalAlignment(Alignment);
        Region->SetVerticalAlignment(VAlign_Center);
        Region->SetContent(Text);
        Place(Parent, Region, Position, Size, Z);
    };

    DetailTitle = MakeText(TEXT("ResearchDetailTitle"), TEXT(""), 30,
        FLinearColor::White);
    TextRegion(DetailRoot, TEXT("ResearchDetailTitleRegion"), DetailTitle,
        FVector2D(307.6f, 70), FVector2D(824.7f, 40), HAlign_Center, 8);
    DetailDescription = MakeText(TEXT("ResearchDetailDescription"), TEXT(""), 20,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailDescriptionRegion"), DetailDescription,
        FVector2D(245.9f, 500.7f), FVector2D(350.1f, 59.8f), HAlign_Center, 6);
    const FMargin ResearchPanelMargins(16.0f / 63.0f, 13.0f / 55.0f,
        15.0f / 63.0f, 13.0f / 55.0f);
    Place(DetailUpdateRoot, MakeNineSlice(TEXT("ResearchDetailProgressFrame"),
        TEXT("ui_common[pl_com_1001]"), FVector2D(281.7f, 32), ResearchPanelMargins),
        FVector2D(283.3f, 584.1f), FVector2D(281.7f, 32), 5);
    Place(DetailUpdateRoot, MakeNineSlice(TEXT("ResearchDetailProgressBackground"),
        TEXT("ui_common[pb_com_1000_1]"), FVector2D(264, 24),
        FMargin(10.0f / 20.0f, 7.0f / 23.0f, 10.0f / 20.0f, 6.0f / 23.0f)),
        FVector2D(292.1f, 588.1f), FVector2D(264, 24), 6);
    DetailProgress = MakeText(TEXT("ResearchDetailProgress"), TEXT(""), 18,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailProgressRegion"), DetailProgress,
        FVector2D(286.9f, 587.4f), FVector2D(274.6f, 25.5f), HAlign_Center, 6);

    DetailLevel = MakeText(TEXT("ResearchDetailLevel"), TEXT(""), 24,
        FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailLevelRegion"), DetailLevel,
        FVector2D(692.7f, 177.2f), FVector2D(380, 30), HAlign_Left, 6);
    Place(DetailUpdateRoot, MakeSprite(TEXT("ResearchDetailAttrPanel"),
        TEXT("ui_common[pl_com_1001]"), FVector2D(536, 100)),
        FVector2D(656, 246.2f), FVector2D(536, 100), 4);
    DetailAttrName = MakeText(TEXT("ResearchDetailAttrName"), TEXT(""), 20,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailAttrNameRegion"), DetailAttrName,
        FVector2D(686.4f, 258.7f), FVector2D(275.8f, 35), HAlign_Left, 6);
    DetailAttrBefore = MakeText(TEXT("ResearchDetailAttrBefore"), TEXT(""), 20,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailAttrBeforeRegion"), DetailAttrBefore,
        FVector2D(884, 258.7f), FVector2D(198, 35), HAlign_Right, 6);
    DetailAttrDelta = MakeText(TEXT("ResearchDetailAttrDelta"), TEXT(""), 20,
        FLinearColor::FromSRGBColor(FColor(0, 128, 0)));
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailAttrDeltaRegion"), DetailAttrDelta,
        FVector2D(1083, 258.7f), FVector2D(79, 35), HAlign_Right, 6);
    UTextBlock* PowerName = MakeText(TEXT("ResearchDetailPowerName"), TEXT("战力"), 20,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailPowerNameRegion"), PowerName,
        FVector2D(686.4f, 298.7f), FVector2D(275.8f, 35), HAlign_Left, 6);
    DetailPowerBefore = MakeText(TEXT("ResearchDetailPowerBefore"), TEXT(""), 20,
        FLinearColor::White);
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailPowerBeforeRegion"), DetailPowerBefore,
        FVector2D(884, 298.7f), FVector2D(198, 35), HAlign_Right, 6);
    DetailPowerDelta = MakeText(TEXT("ResearchDetailPowerDelta"), TEXT(""), 20,
        FLinearColor::FromSRGBColor(FColor(0, 128, 0)));
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailPowerDeltaRegion"), DetailPowerDelta,
        FVector2D(1083, 298.7f), FVector2D(79, 35), HAlign_Right, 6);

    UTextBlock* Boundary = MakeText(TEXT("ResearchDetailUnknownState"),
        TEXT("账号研究等级与前置条件未同步，当前仅浏览来源配置。"), 22,
        FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailUnknownStateRegion"), Boundary,
        FVector2D(683.7f, 432.2f), FVector2D(484.5f, 105.5f), HAlign_Center, 6);

    // The source unlocked branch uses UI_Model_ResCost here. With no account
    // state we expose its static configuration values as a read-only panel.
    Place(DetailUpdateRoot, MakeSprite(TEXT("ResearchDetailCostPanel"),
        TEXT("ui_common[pl_com_1001]"), FVector2D(536, 142)),
        FVector2D(656, 550), FVector2D(536, 142), 4);
    DetailCosts = MakeText(TEXT("ResearchDetailCosts"), TEXT(""), 16,
        FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    TextRegion(DetailUpdateRoot, TEXT("ResearchDetailCostsRegion"), DetailCosts,
        FVector2D(670, 553), FVector2D(510, 24), HAlign_Center, 6);
    DetailCostValues.Reset();
    for (int32 Index = 0; Index < 6; ++Index)
    {
        UTextBlock* Value = MakeText(*FString::Printf(TEXT("ResearchDetailCost_%d"), Index),
            TEXT(""), 16, FLinearColor::White);
        TextRegion(DetailUpdateRoot, *FString::Printf(TEXT("ResearchDetailCostRegion_%d"), Index),
            Value, FVector2D(675 + (Index % 2) * 255, 578 + (Index / 2) * 34),
            FVector2D(240, 30), HAlign_Left, 6);
        DetailCostValues.Add(Value);
    }

    Place(DetailRoot, MakeSprite(TEXT("ResearchDetailCloseIcon"),
        TEXT("ui_common[btn_com_1000_1]"), FVector2D(69, 69)),
        FVector2D(1191.5f, 54.5f), FVector2D(69, 69), 12);
    Place(DetailRoot, MakeButton(TEXT("ResearchDetailBack"), BackAction),
        FVector2D(1191.5f, 54.5f), FVector2D(69, 69), 13);
    Place(DetailUpdateRoot, MakeButton(TEXT("ResearchDetailLevelPrev"), PreviousLevelAction),
        FVector2D(690, 174), FVector2D(38, 38), 10);
    Place(DetailUpdateRoot, MakeButton(TEXT("ResearchDetailLevelNext"), NextLevelAction),
        FVector2D(1045, 174), FVector2D(38, 38), 10);
    UTextBlock* PreviousLabel = MakeText(TEXT("ResearchDetailLevelPrevLabel"),
        TEXT("◀"), 19, FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    UTextBlock* NextLabel = MakeText(TEXT("ResearchDetailLevelNextLabel"),
        TEXT("▶"), 19, FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    Place(DetailUpdateRoot, PreviousLabel, FVector2D(690, 177), FVector2D(38, 30), 11);
    Place(DetailUpdateRoot, NextLabel, FVector2D(1045, 177), FVector2D(38, 30), 11);
    UTextBlock* LocalAction = MakeText(TEXT("ResearchDetailBoundaryLabel"),
        TEXT("研究服务未连接"), 16,
        FLinearColor::FromSRGBColor(FColor(230, 217, 180)));
    Place(DetailUpdateRoot, LocalAction, FVector2D(868, 700), FVector2D(200, 30), 8);
    Place(DetailUpdateRoot, MakeButton(TEXT("ResearchDetailBoundaryButton"), BoundaryAction),
        FVector2D(868, 697), FVector2D(200, 36), 10);
    BuildPowerPage();
    DetailRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void URokResearchScreenWidget::BuildPowerPage()
{
    // ResearchUpateMediator.onMore hides img_update and shows pl_power; its
    // ListView fills one UI_ResearchPowerItem per configured Study level.
    PowerRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
        UCanvasPanel::StaticClass(), TEXT("ResearchPowerRoot"));
    PowerRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Place(DetailRoot, PowerRoot, FVector2D::ZeroVector, FVector2D(1440, 810), 14);
    Place(PowerRoot, MakeSprite(TEXT("ResearchPowerHeaderBackground"),
        TEXT("ui_common[pl_com_1012_2]"), FVector2D(964, 43)),
        FVector2D(238, 128.2f), FVector2D(964, 43));

    auto AddHeader = [this](const FName& RegionName, const FName& TextName,
        const FString& TextValue, const FVector2D& Position, const FVector2D& Size)
    {
        UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), RegionName);
        Region->SetBrushColor(FLinearColor::Transparent);
        Region->SetPadding(FMargin(0));
        Region->SetHorizontalAlignment(HAlign_Center);
        Region->SetVerticalAlignment(VAlign_Center);
        // pl_power's three heading texts share serialized Unity Color32
        // (217, 212, 186); UI_ResearchPowerItem body text remains white.
        UTextBlock* Label = MakeText(TextName, TextValue, 24,
            FLinearColor::FromSRGBColor(FColor(217, 212, 186)));
        Region->SetContent(Label);
        Place(PowerRoot, Region, Position, Size, 2);
        return Label;
    };
    AddHeader(TEXT("ResearchPowerLevelHeaderRegion"), TEXT("ResearchPowerLevelHeader"),
        TEXT("等级"), FVector2D(190.05f, 128.2f), FVector2D(242.5f, 50));
    PowerHeaderDescription = AddHeader(TEXT("ResearchPowerEffectHeaderRegion"),
        TEXT("ResearchPowerEffectHeader"), TEXT(""),
        FVector2D(570, 128.2f), FVector2D(300, 50));
    AddHeader(TEXT("ResearchPowerValueHeaderRegion"), TEXT("ResearchPowerValueHeader"),
        TEXT("战力"), FVector2D(950, 128.2f), FVector2D(300, 50));

    PowerScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
        TEXT("ResearchPowerScroll"));
    PowerScroll->SetOrientation(EOrientation::Orient_Vertical);
    PowerScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    Place(PowerRoot, PowerScroll, FVector2D(201, 173.2f), FVector2D(1038, 564.9f), 2);
    UVerticalBox* Items = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("ResearchPowerItems"));
    PowerScroll->AddChild(Items);
    PowerRows.Reset();
    PowerLevels.Reset();
    PowerEffects.Reset();
    PowerValues.Reset();
    // Study.csv's maximum research level in the source catalog is ten. The
    // reusable rows avoid reconstructing same-name UWidgets on another node.
    for (int32 Level = 1; Level <= 10; ++Level)
    {
        USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
            *FString::Printf(TEXT("ResearchPowerRowExtent_%d"), Level));
        Extent->SetWidthOverride(1038);
        Extent->SetHeightOverride(50);
        Items->AddChildToVerticalBox(Extent);
        UCanvasPanel* ItemContent = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(),
            *FString::Printf(TEXT("ResearchPowerItemContent_%d"), Level));
        Extent->SetContent(ItemContent);
        UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ResearchPowerRow_%d"), Level));
        Place(ItemContent, Row, FVector2D(18, 0), FVector2D(1002, 50));
        Place(Row, MakeSprite(*FString::Printf(TEXT("ResearchPowerLine_%d"), Level),
            TEXT("ui_common[pl_com_1005]"), FVector2D(880, 2)),
            FVector2D(61, 48), FVector2D(880, 2));
        auto AddCell = [this, Row, Level](const TCHAR* Stem, const FVector2D& Position,
            const FVector2D& Size)
        {
            UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
                *FString::Printf(TEXT("ResearchPower%sRegion_%d"), Stem, Level));
            Region->SetBrushColor(FLinearColor::Transparent);
            Region->SetPadding(FMargin(0));
            Region->SetHorizontalAlignment(HAlign_Center);
            Region->SetVerticalAlignment(VAlign_Center);
            UTextBlock* Value = MakeText(
                *FString::Printf(TEXT("ResearchPower%s_%d"), Stem, Level),
                TEXT(""), 24, FLinearColor::White);
            Region->SetContent(Value);
            Place(Row, Region, Position, Size, 1);
            return Value;
        };
        PowerRows.Add(Extent);
        PowerLevels.Add(AddCell(TEXT("Level"), FVector2D(17.25f, 0), FVector2D(157.5f, 50)));
        PowerEffects.Add(AddCell(TEXT("Effect"), FVector2D(178.6f, 0), FVector2D(644.8f, 50)));
        PowerValues.Add(AddCell(TEXT("Value"), FVector2D(800, 0), FVector2D(193.5f, 50)));
    }
    // UI_Model_Window_Type1/btn_back is a root child, outside animated
    // rect/pl_power. It appears immediately when onMore enables it.
    PowerBackIcon = MakeSprite(TEXT("ResearchPowerBackIcon"),
        TEXT("ui_common[btn_com_1000]"), FVector2D(69, 69));
    Place(DetailRoot, PowerBackIcon,
        FVector2D(179.5f, 54.5f), FVector2D(69, 69), 15);
    PowerBackButton = MakeButton(TEXT("ResearchPowerBack"), PowerBackAction);
    Place(DetailRoot, PowerBackButton,
        FVector2D(179.5f, 54.5f), FVector2D(69, 69), 16);
    PowerBackIcon->SetVisibility(ESlateVisibility::Collapsed);
    PowerBackButton->SetVisibility(ESlateVisibility::Collapsed);
    PowerRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void URokResearchScreenWidget::RefreshPowerRows(const FRokResearchRow& SelectedRow)
{
    if (PowerHeaderDescription)
        PowerHeaderDescription->SetText(FText::FromString(SelectedRow.BuffDescription));
    for (int32 Index = 0; Index < PowerRows.Num(); ++Index)
    {
        const int32 Level = Index + 1;
        const bool bInRange = Level <= SelectedRow.MaxLevel;
        PowerRows[Index]->SetVisibility(bInRange
            ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        if (!bInRange) continue;
        FRokResearchRow Config;
        if (!Catalog.GetByStudyLevel(SelectedRow.StudyType, Level, Config)) continue;
        PowerLevels[Index]->SetText(FText::AsNumber(Level));
        PowerEffects[Index]->SetText(FText::FromString(Config.DescriptionData.IsEmpty()
            ? Config.FunctionText : Config.DescriptionData));
        // ClientUtils.FormatComma uses N0 for the source row's power delta.
        PowerValues[Index]->SetText(FText::FromString(TEXT("+") +
            FText::AsNumber(Config.Power).ToString()));
    }
}
