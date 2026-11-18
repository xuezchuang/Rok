#include "RokResearchScreenWidget.h"

#include "RokArmyShowPanelAnimation.h"
#include "RokDataPaths.h"
#include "RokGameNetSubsystem.h"
#include "RokServerCitySubsystem.h"
#include "RokUnityConfigSubsystem.h"
#include "RokInstantPrice.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "RokUiWindowAnimation.h"
#include "RokUiWidgets.h"
#include "Components/TextBlock.h"
#include "RokUnityFeatureScreenWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/WidgetSwitcher.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
constexpr int32 InstantAction = 31;
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
    const FString Path = RokDataPaths::Resolve(TEXT("ResearchNative/atlas_manifest.json"));
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
    if (!LoadResearchData() || !BindPresentation()) return false;
    RefreshAccountState();
    return true;
}

TSubclassOf<URokResearchScreenWidget> URokResearchScreenWidget::LoadPresentationClass()
{
    return LoadClass<URokResearchScreenWidget>(nullptr,
        TEXT("/Game/RokPrototype/UI/Research/WBP_RokResearchTree.WBP_RokResearchTree_C"));
}

void URokResearchScreenWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (!IsDesignTime() || !LoadResearchData()) return;
    BindNodePresentation(false);
    // The designer displays collapsed controls, including the runtime click-away mask.
    if (auto* Mask = Super::GetWidgetFromName(TEXT("ResearchDetailMask")))
        Mask->SetRenderOpacity(0.0f);
}

bool URokResearchScreenWidget::LoadResearchData()
{
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
    return bLoaded && LoadSpriteManifest(LoadError);
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
    AccountRefreshElapsed += InDeltaTime;
    if (AccountRefreshElapsed >= 1.0f)
    {
        AccountRefreshElapsed = 0.0f;
        RefreshAccountState();
    }
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

void URokResearchScreenWidget::RefreshAccountState()
{
    const auto* Instance = GetGameInstance();
    const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
    const auto Role = Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
    TMap<int32, int32> Levels;
    bool bLevelsKnown = false;
    bool bQueueKnown = false;
    int64 Study = 0, Finish = 0;
    if (Role)
    {
        auto ReadRow = [&Levels](const TSharedPtr<FJsonObject>& Row)
        {
            int64 Type = 0, Level = 0;
            if (!RokSprotoJson::TryGetInt64(Row, TEXT("technologyType"), Type) || Type <= 0 || Type > MAX_int32 ||
                !RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level) || Level < 0 || Level > MAX_int32) return false;
            Levels.Add(static_cast<int32>(Type), static_cast<int32>(Level));
            return true;
        };
        const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
        const TSharedPtr<FJsonObject>* Map = nullptr;
        if (Role->TryGetArrayField(TEXT("technologies"), Rows))
        {
            bLevelsKnown = true;
            for (const auto& Value : *Rows)
            {
                const TSharedPtr<FJsonObject>* Row = nullptr;
                if (!Value || !Value->TryGetObject(Row) || !ReadRow(*Row)) { bLevelsKnown = false; break; }
            }
        }
        else if (Role->TryGetObjectField(TEXT("technologies"), Map))
        {
            bLevelsKnown = true;
            for (const auto& Pair : (*Map)->Values)
            {
                const TSharedPtr<FJsonObject>* Row = nullptr;
                if (!Pair.Value || !Pair.Value->TryGetObject(Row) || !ReadRow(*Row)) { bLevelsKnown = false; break; }
            }
        }
        const TSharedPtr<FJsonObject>* Queue = nullptr;
        if (Role->TryGetObjectField(TEXT("technologyQueue"), Queue))
        {
            // Server defaults may encode an empty queue as an empty struct.
            bQueueKnown = true;
            if ((*Queue)->HasField(TEXT("technologyType")) &&
                (!RokSprotoJson::TryGetInt64(*Queue, TEXT("technologyType"), Study) || Study < 0 || Study > MAX_int32)) bQueueKnown = false;
            if (Study > 0 && !RokSprotoJson::TryGetInt64(*Queue, TEXT("finishTime"), Finish)) bQueueKnown = false;
        }
    }
    if (!bLevelsKnown) Levels.Reset();
    bAccountResearchSynced = bLevelsKnown;
    bResearchQueueSynced = bQueueKnown;
    if (!bQueueKnown || QueueStudyType != Study || QueueFinishTime != Finish) bConfirmInstant = false;
    QueueStudyType = bQueueKnown ? static_cast<int32>(Study) : 0;
    QueueFinishTime = bQueueKnown ? Finish : 0;
    AccountLevels = MoveTemp(Levels);
    for (const FRokResearchRow& Row : RenderedNodes)
    {
        if (UTextBlock* Level = NodeLevels.FindRef(Row.StudyType))
            Level->SetText(FText::FromString(bLevelsKnown ? FString::Printf(TEXT("%d/%d"), AccountLevels.FindRef(Row.StudyType), Row.MaxLevel)
                : FString::Printf(TEXT("--/%d"), Row.MaxLevel)));
    }
    if (SelectedStudyType > 0 && IsDetailVisible())
    {
        FRokResearchRow Row;
        if (Catalog.GetByStudyLevel(SelectedStudyType, ViewedConfigLevel, Row))
        {
            const FString Current = bLevelsKnown ? FString::FromInt(AccountLevels.FindRef(Row.StudyType)) : TEXT("—");
            if (DetailLevel) DetailLevel->SetText(FText::FromString(FString::Printf(TEXT("等级%s"), *Current)));
            const bool bShowNext = !bLevelsKnown || AccountLevels.FindRef(Row.StudyType) < Row.MaxLevel;
            if (DetailLevelNext)
            {
                DetailLevelNext->SetText(FText::AsNumber(Row.Level));
                DetailLevelNext->SetVisibility(bShowNext ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            }
            if (DetailLevelArrow) DetailLevelArrow->SetVisibility(bShowNext ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (DetailProgress) DetailProgress->SetText(FText::FromString(FString::Printf(
                TEXT("%s/%d"), bLevelsKnown ? *Current : TEXT("--"), Row.MaxLevel)));
        }
    }
    if (IsDetailVisible())
    {
        const FString Status = ResearchActionStatus();
        if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailUnknownState"))))
            Text->SetText(FText::FromString(!ResearchResult.IsEmpty() ? ResearchResult : Status));
        if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailBoundaryLabel"))))
        {
            const bool bPending = Net && Net->IsResearchWritePending();
            const FString Action = bPending ? TEXT("等待服务器") : QueueStudyType == SelectedStudyType && QueueStudyType > 0
                ? (QueueFinishTime > 0 ? (bConfirmStop ? TEXT("确认取消") : TEXT("取消研究")) : TEXT("领取研究"))
                : TEXT("开始研究");
            Label->SetText(FText::FromString(Action == TEXT("开始研究") ? TEXT("研究") : Action));
        }
        const bool bIdle = bResearchQueueSynced && QueueStudyType == 0;
        for (const TCHAR* Name : { TEXT("ResearchInstantBackground"), TEXT("ResearchInstantButton"),
            TEXT("ResearchInstantLabel"), TEXT("ResearchInstantGem"), TEXT("ResearchInstantPrice") })
            if (UWidget* Item = GetWidgetFromName(Name))
                Item->SetVisibility(bIdle ? (FString(Name) == TEXT("ResearchInstantButton") ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible) : ESlateVisibility::Collapsed);
        int64 Seconds = 0, Gems = 0;
        const bool bQuote = GetResearchQuote(Seconds, Gems);
        if (UTextBlock* Time = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDuration"))))
            Time->SetText(FText::FromString(bQuote ? FString::Printf(TEXT("%02lld:%02lld:%02lld"), Seconds / 3600, (Seconds / 60) % 60, Seconds % 60) : TEXT("--:--:--")));
        if (UTextBlock* Price = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchInstantPrice"))))
            Price->SetText(FText::FromString(bQuote ? FString::Printf(TEXT("%lld"), Gems) : TEXT("—")));
    }
}

FString URokResearchScreenWidget::ResearchActionStatus(bool bIgnoreResourceShortage) const
{
    const auto* Instance = GetGameInstance();
    const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
    if (!Net || Net->GetState() != ERokGameNetState::RoleReady) return TEXT("连接服务器后可研究科技。");
    if (Net->IsSnapshotReplay()) return TEXT("快照回放仅供查看，无法研究。");
    if (Net->IsResearchWritePending()) return TEXT("正在等待服务器确认。");
    if (!bAccountResearchSynced || !bResearchQueueSynced) return TEXT("账号科技或队列状态尚未同步。");
    if (QueueStudyType > 0)
    {
        if (QueueStudyType != SelectedStudyType) return TEXT("已有其他科技正在研究或等待领取。");
        if (QueueFinishTime <= 0) return TEXT("研究已完成，可以领取。");
        return bConfirmStop ? TEXT("取消仅返还部分资源，再次点击确认取消。") : TEXT("研究进行中，可取消研究。");
    }
    FRokResearchRow Row;
    const int32 Level = AccountLevels.FindRef(SelectedStudyType);
    if (!Catalog.GetByStudyLevel(SelectedStudyType, Level + 1, Row)) return TEXT("该科技已达最高等级。");
    if (ViewedConfigLevel != Level + 1) return TEXT("请选择当前等级的下一级进行研究。");
    for (const auto& Prerequisite : Row.Preconditions)
        if (Prerequisite.StudyType > 0 && AccountLevels.FindRef(Prerequisite.StudyType) < Prerequisite.Level)
        {
            FRokResearchRow Required;
            const FString Name = Catalog.GetByStudyLevel(Prerequisite.StudyType, 1, Required) ? Required.Name : TEXT("前置科技");
            return FString::Printf(TEXT("需要 %s 达到 %d 级。"), *Name, Prerequisite.Level);
        }
    const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
    if (!City || !City->HasAppliedServerCity()) return TEXT("学院状态尚未同步。");
    const FRokServerBuilding* Academy = City->GetAppliedBuildings().FindByPredicate(
        [](const FRokServerBuilding& Building) { return Building.Type == 13 && Building.Level > 0; });
    if (!Academy) return TEXT("请先建造学院。");
    if (Academy->FinishTime > 0) return TEXT("学院升级中，无法研究。");
    if (Academy->Level < Row.CampusLevel) return FString::Printf(TEXT("需要学院达到 %d 级。"), Row.CampusLevel);
    const auto Role = Net->GetRoleInfo();
    const TCHAR* Names[] = { TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold") };
    const TCHAR* Labels[] = { TEXT("食物"), TEXT("木材"), TEXT("石料"), TEXT("金币") };
    const int64 Costs[] = { Row.BaseFood, Row.BaseWood, Row.BaseStone, Row.BaseGold };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        int64 Available = 0;
        if (!RokSprotoJson::TryGetInt64(Role, Names[Index], Available)) return TEXT("账号资源尚未同步。");
        if (!bIgnoreResourceShortage && Available < Costs[Index]) return FString::Printf(TEXT("%s不足。"), Labels[Index]);
    }
    return FString();
}

bool URokResearchScreenWidget::GetResearchQuote(int64& Seconds, int64& Gems) const
{
    const auto* Instance = GetGameInstance();
    const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
    auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
    const auto Role = Net ? Net->GetRoleInfo() : nullptr;
    const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
    FRokResearchRow Row;
    if (!Role || !Config || !City || !Catalog.GetByStudyLevel(SelectedStudyType, ViewedConfigLevel, Row)) return false;
    const auto* Academy = City->GetAppliedBuildings().FindByPredicate(
        [](const FRokServerBuilding& B) { return B.Type == 13 && B.Level > 0; });
    int32 AcademySpeed = 0;
    if (!Academy || !Config->GetIntFieldByKey(TEXT("BuildingCampus"), TEXT("ID"), Academy->Level,
        TEXT("researchSpeedMulti"), AcademySpeed)) return false;
    int64 Speed = AcademySpeed;
    // The server attribute, when present, includes technology and other bonuses.
    RokSprotoJson::TryGetInt64(Role, TEXT("researchSpeedMulti"), Speed);
    Seconds = FMath::FloorToInt64(Row.BaseSeconds / FMath::Max(0.01, 1.0 + Speed / 1000.0));
    int64 Inputs[5] = { Seconds, 0, 0, 0, 0 };
    const TCHAR* Fields[] = { TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold") };
    const int64 Costs[] = { Row.BaseFood, Row.BaseWood, Row.BaseStone, Row.BaseGold };
    for (int32 I = 0; I < 4; ++I)
    {
        int64 Available = 0;
        if (!RokSprotoJson::TryGetInt64(Role, Fields[I], Available) || Available < 0) return false;
        Inputs[I + 1] = FMath::Max<int64>(0, Costs[I] - Available);
    }
    const auto* Prices = Config->GetTableRows(TEXT("InstantPrice"));
    double Total = 0;
    if (!Prices || !RokInstantPrice::AddQuote(*Prices, Inputs, Total)) return false;
    Gems = FMath::FloorToInt64(Total);
    return true;
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
    if (DetailMask)
    {
        DetailMask->SetRenderOpacity(bVisible ? 1.0f : 0.0f);
        DetailMask->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    }
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
    if (DetailPages) DetailPages->SetActiveWidget(DetailUpdateRoot);
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

bool URokResearchScreenWidget::ApplySprite(UImage* Image,
    const FString& Reference, bool bUseSourceGray)
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
    FSlateBrush Brush = Image->GetBrush();
    Brush.SetResourceObject(Texture);
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

void URokResearchScreenWidget::SetSelectedType(int32 Type)
{
    if (Type != EconomyType && Type != MilitaryType) return;
    if (Type != SelectedType && DetailRoot && !bDetailCloseAnimating)
    {
        ResetDetailAnimation();
        SetDetailVisible(false);
    }
    SelectedType = Type;
    if (TreePages)
        TreePages->SetActiveWidgetIndex(Type == EconomyType ? 0 : 1);
    if (EconomyTabStyle)
        EconomyTabStyle->SetActiveWidgetIndex(Type == EconomyType ? 0 : 1);
    if (MilitaryTabStyle)
        MilitaryTabStyle->SetActiveWidgetIndex(Type == MilitaryType ? 0 : 1);
    if (WindowTitle) WindowTitle->SetText(FText::FromString(Type == EconomyType
        ? TEXT("经济科技") : TEXT("军事科技")));
}

void URokResearchScreenWidget::ShowSelectedDetail()
{
    if (!DetailRoot || SelectedStudyType <= 0) return;
    const bool bOpeningNewDetail = DetailRoot->GetVisibility() == ESlateVisibility::Collapsed;
    FRokResearchRow Row;
    if (!Catalog.GetByStudyLevel(SelectedStudyType, ViewedConfigLevel, Row)) return;
    if (DetailIcon) ApplySprite(DetailIcon, Row.IconSource);
    if (DetailTitle) DetailTitle->SetText(FText::FromString(Row.Name));
    if (DetailDescription) DetailDescription->SetText(FText::FromString(Row.Description));
    if (DetailLevel) DetailLevel->SetText(FText::FromString(FString::Printf(TEXT("等级%s"),
        bAccountResearchSynced ? *FString::FromInt(AccountLevels.FindRef(Row.StudyType)) : TEXT("—"))));
    if (DetailLevelNext) DetailLevelNext->SetText(FText::AsNumber(Row.Level));
    if (DetailProgress) DetailProgress->SetText(FText::FromString(FString::Printf(
        TEXT("%s/%d"), bAccountResearchSynced ? *FString::FromInt(AccountLevels.FindRef(Row.StudyType)) : TEXT("--"), Row.MaxLevel)));
    FRokResearchRow Previous;
    const bool bHasPrevious = Row.Level > 1 &&
        Catalog.GetByStudyLevel(Row.StudyType, Row.Level - 1, Previous);
    const bool bHasAttribute = !Row.DescriptionData.IsEmpty();
    if (UWidget* AttributeRow = GetWidgetFromName(TEXT("ResearchDetailAttrRow")))
        AttributeRow->SetVisibility(bHasAttribute ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (DetailAttrName) DetailAttrName->SetText(FText::FromString(Row.BuffDescription));
    if (DetailAttrBefore) DetailAttrBefore->SetText(FText::FromString(FString::Printf(
        TEXT("%s"), bHasPrevious ? *Previous.DescriptionData : TEXT("0.0%"))));
    if (DetailAttrDelta && bHasAttribute)
    {
        const double Before = bHasPrevious ? FCString::Atod(*Previous.DescriptionData) : 0.0;
        const double After = FCString::Atod(*Row.DescriptionData);
        DetailAttrDelta->SetText(FText::FromString(FString::Printf(TEXT("+%.1f%%"), After - Before)));
    }
    if (DetailPowerBefore) DetailPowerBefore->SetText(FText::FromString(FString::Printf(
        TEXT("%lld"), bHasPrevious ? Previous.Power : 0LL)));
    if (DetailPowerDelta) DetailPowerDelta->SetText(FText::FromString(FString::Printf(
        TEXT("+%lld"), Row.Power - (bHasPrevious ? Previous.Power : 0LL))));
    const int64 Values[] = { Row.BaseFood, Row.BaseWood, Row.BaseStone, Row.BaseGold };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Values); ++Index)
    {
        const FName Name(*FString::Printf(TEXT("ResearchDetailCostItem_%d"), Index));
        if (UWidget* Item = GetWidgetFromName(Name))
            Item->SetVisibility(Values[Index] > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (DetailCostValues.IsValidIndex(Index) && DetailCostValues[Index]) DetailCostValues[Index]->SetText(FText::AsNumber(Values[Index]));
    }
    ResearchResult.Reset();
    RefreshPowerRows(Row);
    if (DetailPages) DetailPages->SetActiveWidget(DetailUpdateRoot);
    if (DetailUpdateRoot) DetailUpdateRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (PowerRoot) PowerRoot->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackIcon) PowerBackIcon->SetVisibility(ESlateVisibility::Collapsed);
    if (PowerBackButton) PowerBackButton->SetVisibility(ESlateVisibility::Collapsed);
    SetDetailVisible(true);
    RefreshAccountState();
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
            bConfirmStop = false;
            bConfirmInstant = false;
            ResearchResult.Reset();
            ViewedConfigLevel = bAccountResearchSynced ? FMath::Clamp(AccountLevels.FindRef(RequestedStudyType) + 1, 1, Found->MaxLevel) : 1;
            ShowSelectedDetail();
        }
        return;
    }
    if (ActionId == BackAction)
    {
        bConfirmStop = false;
        bConfirmInstant = false;
        ResearchResult.Reset();
        StartDetailClose();
        return;
    }
    if (ActionId == MoreAction)
    {
        if (DetailPages) DetailPages->SetActiveWidget(PowerRoot);
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
        if (DetailPages) DetailPages->SetActiveWidget(DetailUpdateRoot);
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
            bConfirmStop = false;
            bConfirmInstant = false;
            ResearchResult.Reset();
            ViewedConfigLevel = FMath::Clamp(ViewedConfigLevel +
                (ActionId == NextLevelAction ? 1 : -1), 1, Current.MaxLevel);
            ShowSelectedDetail();
        }
        return;
    }
    if (ActionId == BoundaryAction || ActionId == InstantAction)
    {
        const bool bInstant = ActionId == InstantAction;
        if (!bInstant) bConfirmInstant = false;
        LastBoundaryActionId = ActionId;
        ResearchResult.Reset();
        RefreshAccountState();
        const FString Status = ResearchActionStatus(bInstant);
        const bool bIsOwnQueue = QueueStudyType > 0 && QueueStudyType == SelectedStudyType;
        const bool bStop = bIsOwnQueue && QueueFinishTime > 0;
        const bool bAward = bIsOwnQueue && QueueFinishTime <= 0;
        if (!Status.IsEmpty() && !bIsOwnQueue) { ResearchResult = Status; RefreshAccountState(); return; }
        if (bStop && !bConfirmStop)
        {
            bConfirmStop = true;
            RefreshAccountState();
            return;
        }
        auto* Instance = GetGameInstance();
        auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
        if (!Net) { ResearchResult = TEXT("服务器连接尚未就绪。"); RefreshAccountState(); return; }
        if (bInstant)
        {
            int64 Seconds = 0, Price = 0, Balance = 0;
            if (!GetResearchQuote(Seconds, Price) || !RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("denar"), Balance))
            { ResearchResult = TEXT("宝石价格或余额尚未同步。"); RefreshAccountState(); return; }
            if (Balance < Price) { ResearchResult = TEXT("宝石不足。"); RefreshAccountState(); return; }
            if (!bConfirmInstant || Price != ConfirmedInstantPrice)
            {
                bConfirmInstant = true; ConfirmedInstantPrice = Price;
                ResearchResult = FString::Printf(TEXT("预计消耗 %lld 宝石；实际费用以服务器为准。再次点击确认。"), Price);
                RefreshAccountState(); return;
            }
        }
        bConfirmInstant = false;
        const ERokResearchCommand Command = bInstant ? ERokResearchCommand::Instant : bStop ? ERokResearchCommand::Stop : bAward ? ERokResearchCommand::Award : ERokResearchCommand::Start;
        TWeakObjectPtr<URokResearchScreenWidget> WeakThis(this);
        FString Error;
        bConfirmStop = false;
        if (!Net->RequestResearchCommand(Command, SelectedStudyType, [WeakThis, Command](const FString& Failure)
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->RefreshAccountState();
            if (Failure.IsEmpty() && (Command == ERokResearchCommand::Award || Command == ERokResearchCommand::Instant) && WeakThis->IsDetailVisible())
            {
                FRokResearchRow Current;
                if (WeakThis->Catalog.GetByStudyLevel(WeakThis->SelectedStudyType, 1, Current))
                {
                    WeakThis->ViewedConfigLevel = FMath::Clamp(
                        WeakThis->AccountLevels.FindRef(WeakThis->SelectedStudyType) + 1, 1, Current.MaxLevel);
                    WeakThis->ShowSelectedDetail();
                }
            }
            WeakThis->ResearchResult = Failure.IsEmpty() ? (Command == ERokResearchCommand::Start ? TEXT("研究已开始。") :
                (Command == ERokResearchCommand::Award || Command == ERokResearchCommand::Instant) ? TEXT("科技已完成。") : TEXT("研究已取消。")) : Failure;
            WeakThis->RefreshAccountState();
        }, Error)) ResearchResult = Error;
        RefreshAccountState();
    }
}

UWidget* URokResearchScreenWidget::GetWidgetFromName(const FName& Name) const
{
    if (UWidget* Widget = Super::GetWidgetFromName(Name)) return Widget;
    if (DetailView)
    {
        if (UWidget* Widget = DetailView->GetWidgetFromName(Name)) return Widget;
    }
    // Keep the audit names for per-study widgets after moving the card into a reusable asset.
    FString Stem, StudyType;
    if (Name.ToString().Split(TEXT("_"), &Stem, &StudyType, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
        && Stem.StartsWith(TEXT("ResearchNode")) && StudyType.IsNumeric())
    {
        const FName CardName(*FString::Printf(TEXT("ResearchNode_%s"), *StudyType));
        if (const auto* Card = Cast<UUserWidget>(Super::GetWidgetFromName(CardName)))
            return Card->GetWidgetFromName(*Stem);
    }
    return nullptr;
}

bool URokResearchScreenWidget::BindAction(UWidget* Widget, int32 ActionId, bool bAnimatePress)
{
    auto* Button = Cast<URokUnityIndexedButton>(Widget);
    if (!Button) return false;
    Button->SetPressAnimationEnabled(bAnimatePress);
    Button->ConfigureAction(ActionId);
    Button->OnIndexedClicked.RemoveAll(this);
    Button->OnIndexedClicked.AddUObject(this, &URokResearchScreenWidget::HandleIndexedAction);
    return true;
}

void URokResearchScreenWidget::BindNodePresentation(bool bBindActions)
{
    NodeLevels.Reset();
    RenderedLines.Reset();
    URokUiPageWidget* FrameStyles = nullptr;
    if (!URokUiSettings::IsLegacyForced())
    {
        const auto StyleClass = LoadClass<URokUiPageWidget>(nullptr,
            TEXT("/Game/RokUI/Research/WBP_RokResearchLockedFrameStyles.WBP_RokResearchLockedFrameStyles_C"));
        if (StyleClass)
        {
            FrameStyles = CreateWidget<URokUiPageWidget>(this, StyleClass);
        }
    }
    const auto* FrameStyle = FrameStyles
        ? Cast<UImage>(FrameStyles->GetWidgetFromName(TEXT("ResearchLockedIconFrame"))) : nullptr;
    const FSlateBrush* FrameBrush = FrameStyle ? &FrameStyle->GetBrush() : nullptr;
    UObject* FrameResource = FrameBrush ? FrameBrush->GetResourceObject() : nullptr;
    const auto* FrameMaterial = Cast<UMaterialInterface>(FrameResource);
    const FBox2f FrameUV = FrameBrush ? static_cast<FBox2f>(FrameBrush->GetUVRegion()) : FBox2f(ForceInit);
    if (!FrameResource || (!Cast<UTexture2D>(FrameResource) && (!FrameMaterial || !FrameMaterial->IsUIMaterial()))
        || !FrameUV.bIsValid)
    {
        FrameBrush = nullptr;
    }
    for (const FRokResearchRow& Row : RenderedNodes)
    {
        const FName CardName(*FString::Printf(TEXT("ResearchNode_%d"), Row.StudyType));
        auto* Card = Cast<UUserWidget>(Super::GetWidgetFromName(CardName));
        if (!Card) continue;
        const FRokResearchRenderedLine Line = DescribeSourceLine(Row, RenderedNodes);
        if (!Line.PrimarySprite.IsEmpty() || !Line.SecondarySprite.IsEmpty()) RenderedLines.Add(Line);
        const bool bLocked = Row.Preconditions.ContainsByPredicate(
            [](const FRokResearchPrerequisite& Prerequisite) { return Prerequisite.StudyType > 0; });
        if (auto* Icon = Cast<UImage>(Card->GetWidgetFromName(TEXT("ResearchNodeIcon"))))
            ApplySprite(Icon, Row.IconSource, bLocked);
        if (auto* Background = Card->GetWidgetFromName(TEXT("ResearchNodeBackground")))
            Background->SetVisibility(bLocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        if (auto* Background = Card->GetWidgetFromName(TEXT("ResearchNodeBackgroundLocked")))
            Background->SetVisibility(bLocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bLocked)
        {
            if (auto* Frame = Cast<UImage>(Card->GetWidgetFromName(TEXT("ResearchNodeIconBackground"))))
            {
                if (FrameBrush)
                {
                    // Match ApplySprite's mutation scope; the visible node keeps its size and tint.
                    FSlateBrush Brush = Frame->GetBrush();
                    Brush.SetResourceObject(FrameResource);
                    Brush.SetUVRegion(FrameUV);
                    Frame->SetBrush(Brush);
                    Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
                }
                else
                {
                    ApplySprite(Frame, TEXT("ui_research[img_research_iconBg]"), true);
                }
            }
        }
        if (auto* Name = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("ResearchNodeName"))))
            Name->SetText(FText::FromString(Row.Name));
        if (auto* Level = Cast<UTextBlock>(Card->GetWidgetFromName(TEXT("ResearchNodeLevel"))))
        {
            Level->SetText(FText::FromString(FString::Printf(TEXT("%s/%d"), bBindActions ? TEXT("--") : TEXT("0"), Row.MaxLevel)));
            NodeLevels.Add(Row.StudyType, Level);
        }
        if (bBindActions) BindAction(Card->GetWidgetFromName(TEXT("ResearchNodeHit")), NodeActionBase + Row.StudyType);
    }
}

bool URokResearchScreenWidget::BindPresentation()
{
    TreePages = Cast<UWidgetSwitcher>(Super::GetWidgetFromName(TEXT("ResearchTreePages")));
    const auto DetailClass = LoadClass<UUserWidget>(nullptr,
        TEXT("/Game/RokPrototype/UI/Research/WBP_RokResearchDetail.WBP_RokResearchDetail_C"));
    if (DetailClass && Super::GetWidgetFromName(TEXT("ResearchDetailHost")))
    {
        DetailView = CreateWidget<UUserWidget>(this, DetailClass);
        SetContentForSlot(TEXT("ResearchDetailHost"), DetailView);
    }
    DetailPages = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("ResearchDetailPages")));
    EconomyScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ResearchEconomyScroll")));
    MilitaryScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ResearchMilitaryScroll")));
    WindowTitle = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchWindowTitle")));
    EconomyTabStyle = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("ResearchEconomyTabStyle")));
    MilitaryTabStyle = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("ResearchMilitaryTabStyle")));
    DetailMask = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ResearchDetailMask")));
    DetailRoot = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ResearchDetailRoot")));
    DetailUpdateRoot = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ResearchDetailUpdateRoot")));
    PowerRoot = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ResearchPowerRoot")));
    PowerBackIcon = GetWidgetFromName(TEXT("ResearchPowerBackIcon"));
    PowerBackButton = GetWidgetFromName(TEXT("ResearchPowerBack"));
    PowerScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ResearchPowerScroll")));
    PowerHeaderDescription = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchPowerEffectHeader")));
    DetailTitle = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailTitle")));
    DetailDescription = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailDescription")));
    DetailLevel = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailLevel")));
    DetailLevelNext = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailLevelNextValue")));
    DetailLevelArrow = Cast<UImage>(GetWidgetFromName(TEXT("ResearchDetailLevelArrow")));
    DetailIcon = Cast<UImage>(GetWidgetFromName(TEXT("ResearchDetailIcon")));
    DetailProgress = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailProgress")));
    DetailAttrName = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailAttrName")));
    DetailAttrBefore = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailAttrBefore")));
    DetailAttrDelta = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailAttrDelta")));
    DetailPowerBefore = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailPowerBefore")));
    DetailPowerDelta = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResearchDetailPowerDelta")));
    if (!TreePages || !EconomyTabStyle || !MilitaryTabStyle || !DetailPages || !DetailView || !DetailRoot || !DetailMask || !DetailUpdateRoot || !PowerRoot
        || !EconomyScroll || !MilitaryScroll || !PowerScroll)
    {
        LoadError = TEXT("Research Widget Blueprints are missing required panels.");
        return false;
    }
    DetailCostValues.Reset();
    for (int32 Index = 0; Index < 4; ++Index)
        DetailCostValues.Add(Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ResearchDetailCost_%d"), Index))));
    PowerRows.Reset();
    PowerLevels.Reset();
    PowerEffects.Reset();
    PowerValues.Reset();
    for (int32 Level = 1; Level <= 10; ++Level)
    {
        auto* Extent = GetWidgetFromName(*FString::Printf(TEXT("ResearchPowerRowExtent_%d"), Level));
        auto* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ResearchPowerLevel_%d"), Level)));
        auto* Effect = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ResearchPowerEffect_%d"), Level)));
        auto* Value = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("ResearchPowerValue_%d"), Level)));
        if (!Extent || !Label || !Effect || !Value)
        {
            LoadError = TEXT("Research detail Widget Blueprint is missing a power row.");
            return false;
        }
        PowerRows.Add(Extent);
        PowerLevels.Add(Label);
        PowerEffects.Add(Effect);
        PowerValues.Add(Value);
    }
    const TPair<const TCHAR*, int32> Actions[] = {
        {TEXT("ResearchCloseButton"), CloseAction}, {TEXT("ResearchEconomyTab"), EconomyAction},
        {TEXT("ResearchMilitaryTab"), MilitaryAction}, {TEXT("ResearchDetailMaskHit"), BackAction},
        {TEXT("ResearchDetailMore"), MoreAction}, {TEXT("ResearchDetailBack"), BackAction},
        {TEXT("ResearchPowerBack"), PowerBackAction}, {TEXT("ResearchInstantButton"), InstantAction},
        {TEXT("ResearchDetailBoundaryButton"), BoundaryAction},
    };
    for (const auto& [Name, Action] : Actions)
    {
        if (!BindAction(GetWidgetFromName(Name), Action, FCString::Strcmp(Name, TEXT("ResearchDetailMaskHit")) != 0))
        {
            LoadError = FString::Printf(TEXT("Research Widget Blueprint is missing button %s."), Name);
            return false;
        }
    }
    BindNodePresentation(true);
    if (NodeLevels.Num() != RenderedNodes.Num())
    {
        LoadError = TEXT("Research tree Widget Blueprint does not contain all source technology nodes.");
        return false;
    }
    DetailView->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SetDetailVisible(false);
    SetSelectedType(EconomyType);
    return true;
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
