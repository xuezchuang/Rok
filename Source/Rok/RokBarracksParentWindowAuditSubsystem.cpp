#include "RokBarracksParentWindowAuditSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokModalNineSlice.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 CaptureCount = 6;
const FName ParentNames[] = { TEXT("Type2BodyLeft"), TEXT("Type2BodyRight"), TEXT("Type2HeaderLeft"), TEXT("Type2HeaderRight"), TEXT("Type2Title"), TEXT("Type2CloseButton") };
const FVector2D ParentSizes[] = { {543,660.8}, {543,660.8}, {560,60}, {560,60}, {824.7,40}, {69,69} };
const FVector2D BarracksPositions[] = { {194,94.2}, {703,94.2}, {160,63}, {720,63}, {307.65,70}, {1191.5,54.5} };
const FVector2D OtherPositions[] = { {177,139.2}, {720,139.2}, {160,108}, {720,108}, {307.65,115}, {1191.5,99.5} };
const int32 ParentZ[] = { 0, 0, 2, 2, 3, 4 };
const TCHAR* PreviewAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const FIntRect PreviewSpriteRect(1003,932,1403,974);
const FMargin PreviewSpriteMargin(182.0f/400.0f,13.0f/42.0f,115.0f/400.0f,12.0f/42.0f);

bool TargetWorld(const UWorld* World)
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity")
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer, false);
    T::StaticStruct()->SerializeItem(Archive, &Value, nullptr);
    return Bytes;
}
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
        if (const auto* User = Cast<UUserWidget>(Outer); User && (!User->IsVisible() || User->GetRenderOpacity() <= 0)) return false;
    return true;
}
TSharedRef<FJsonObject> MakeParentRectJson(FVector2D Origin, FVector2D Size)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("x"), Origin.X); Result->SetNumberField(TEXT("y"), Origin.Y);
    Result->SetNumberField(TEXT("width"), Size.X); Result->SetNumberField(TEXT("height"), Size.Y);
    return Result;
}
bool PreviewFrameSourceMatches(URokUnityModalWidget* Modal)
{
    if (!Modal) return false;
    for (int32 Index=0; Index<9; ++Index)
    {
        const auto* Image=Cast<UImage>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BuildingUpgradePreviewFrameSlice_%d"),Index)));
        const auto* Slot=Image?Cast<UCanvasPanelSlot>(Image->Slot):nullptr;
        if (!Image || !Slot || !Visible(Image)) return false;
        const auto Patch=RokModalNineSlice::Patch(PreviewSpriteRect,PreviewSpriteMargin,FVector2D(450,620),Index);
        if (!Patch.IsVisible() || !Slot->GetPosition().Equals(Patch.Position,.02)
            || !Slot->GetSize().Equals(Patch.Size,.02)) return false;
        const FSlateBrush& Brush=Image->GetBrush();
        if (!Brush.GetResourceObject() || Brush.GetResourceObject()->GetPathName()!=PreviewAtlasPath) return false;
        const FBox2f UV=Brush.GetUVRegion();
        const float PatchTop=float(1024-Patch.Source.Max.Y);
        if (!UV.Min.Equals(FVector2f(float(Patch.Source.Min.X)/2048.f,PatchTop/1024.f),.00001f)
            || !UV.Max.Equals(FVector2f(float(Patch.Source.Max.X)/2048.f,(PatchTop+float(Patch.Source.Height()))/1024.f),.00001f)) return false;
    }
    return true;
}
}

bool URokBarracksParentWindowAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(), TEXT("RokBarracksParentWindowAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokBarracksParentWindowAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokBarracksParentWindowAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokBarracksParentWindowAuditSubsystem, STATGROUP_Tickables); }
void URokBarracksParentWindowAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsTargetWorld = TargetWorld(GetWorld()); if (!bOwnsTargetWorld) return;
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/BarracksParentWindowAudit")
        / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ShotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokBarracksParentWindowAuditSubsystem::ScreenshotProcessed);
}
void URokBarracksParentWindowAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsTargetWorld && !bFinished) Finish(TEXT("Target city world ended before completion"));
    Super::Deinitialize();
}
bool URokBarracksParentWindowAuditSubsystem::Expect(bool Condition, const TCHAR* Error) { if (!Condition) Finish(Error); return Condition; }
void URokBarracksParentWindowAuditSubsystem::Enter(int32 Next) { Step = Next; StepStarted = Elapsed; }
bool URokBarracksParentWindowAuditSubsystem::Identity()
{
    return Expect(TargetWorld(GetWorld()), TEXT("Wrong project/map; no UI action allowed"));
}
bool URokBarracksParentWindowAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor = FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor, Token, false))
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
            && !Token.Equals(TEXT("-RokBarracksParentWindowAudit"), ESearchCase::IgnoreCase))
        { Finish(TEXT("Only this Rok audit flag is permitted")); return false; }
    const auto Host = [&](UClass* Class)->UUserWidget*
    {
        TArray<UUserWidget*> All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, All, Class, false);
        UUserWidget* Result = nullptr; int32 Count = 0;
        for (auto* Widget : All) if (Widget->GetWorld() == GetWorld()) { Result = Widget; ++Count; }
        return Count == 1 ? Result : nullptr;
    };
    Hud = Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));
    Modal = Cast<URokUnityModalWidget>(Host(URokUnityModalWidget::StaticClass()));
    const auto* Feature = Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Modal.IsValid() && !Modal->IsModalOpen()
        && Feature && !Feature->IsScreenOpen() && Economy && Runtime, TEXT("Expected unique idle city UI hosts"))) return false;
    int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        bool Tagged = false;
        for (FName Tag : It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { Tagged = true; break; }
        if (!Tagged) continue;
        const auto State = Economy->GetBuildingStateForActor(*It);
        if (State.UnityInitId > 0) Buildings.Add(*It, Snapshot(State));
        if (It->ActorHasTag(TEXT("UnityInitId_6")))
        {
            Barracks = *It; ++Count;
            if (!Expect(It->ActorHasTag(TEXT("UnityBuildingType_9")) && State.UnityInitId == 6
                && State.UnityBuildingTypeId == 9 && State.Level == 1 && State.Power == 5
                && !State.bUpgradeInProgress, TEXT("Expected real Init6/type9 level1 barracks"))) return false;
        }
    }
    if (!Expect(Count == 1 && Economy->GetActiveUpgradeCount() == 0, TEXT("Expected unique barracks and idle queues"))) return false;
    OwnedQueues = Economy->GetOwnedBuildQueueCount();
    Profile = Snapshot(Runtime->GetPlayerProfileState()); Resources = Snapshot(Economy->GetCurrentResources()); bBaseline = true;
    Hud->SetSelectedBuilding(Barracks.Get());
    return true;
}
bool URokBarracksParentWindowAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime && Snapshot(Economy->GetCurrentResources()) == Resources
        && Snapshot(Runtime->GetPlayerProfileState()) == Profile && Economy->GetActiveUpgradeCount() == 0
        && Economy->GetOwnedBuildQueueCount() == OwnedQueues, TEXT("Parent-window navigation changed economy/profile/queues"))) return false;
    for (const auto& Pair : Buildings)
        if (!Expect(Pair.Key.IsValid() && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get())) == Pair.Value,
            TEXT("Parent-window navigation changed source building state"))) return false;
    return true;
}
bool URokBarracksParentWindowAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Name == TEXT("BuildingUpgradeButton") || Name == TEXT("BuildingInfoButton")
        || Name == TEXT("Type2CloseButton"), TEXT("Only navigation delegates allowed"))) return false;
    auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
    if (!Expect(Button && Visible(Button) && Button->GetIsEnabled(), TEXT("Missing enabled navigation button"))) return false;
    auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"), Name.ToString());
    Action->SetNumberField(TEXT("frame"), GFrameCounter); Action->SetNumberField(TEXT("step"), Step);
    Actions.Add(MakeShared<FJsonValueObject>(Action)); Button->OnClicked.Broadcast();
    return true;
}
bool URokBarracksParentWindowAuditSubsystem::Validate()
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Modal.IsValid() && Hud.IsValid(), TEXT("Lost city UI host"))) return false;
    const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
    const FVector2D ViewportPixels = UWidgetLayoutLibrary::GetViewportSize(this);
    if (!Expect(ViewportPixels.Equals(FVector2D(1440,810), .1) && Viewport.GetLocalSize().X > 0
        && Viewport.GetLocalSize().Y > 0, TEXT("Real game viewport must be 1440x810"))) return false;
    if (PageKind == 0)
        return Expect(!Modal->IsModalOpen() && !Modal->GetWidgetFromName(ParentNames[0])
            && !Modal->GetWidgetFromName(ParentNames[5]), TEXT("Closed Type2 parent remained attached"));
#if !UE_BUILD_SHIPPING
    if (!Expect(Modal->GetSelectedBuildingActorForAudit() == Barracks.Get(), TEXT("Type2 page lost selected Init6 actor"))) return false;
#endif
    if (!Expect(Modal->IsModalOpen() && Visible(Modal.Get()), TEXT("Type2 page did not open"))) return false;
    const bool bUpgrade = PageKind == 1;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ParentNames); ++Index)
    {
        const auto* Widget = Modal->GetWidgetFromName(ParentNames[Index]);
        const auto* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
        const FVector2D Expected = bUpgrade ? BarracksPositions[Index] : OtherPositions[Index];
        if (!Expect(Widget && Slot && Visible(Widget) && Slot->GetPosition().Equals(Expected,.02)
            && Slot->GetSize().Equals(ParentSizes[Index],.02) && Slot->GetZOrder() == ParentZ[Index],
            TEXT("Type2 parent slot differs from scoped source/default geometry"))) return false;
        const FGeometry Geometry = Widget->GetCachedGeometry();
        const auto Pixel = [&](FVector2D Point)
        {
            const FVector2D Local = Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Point));
            return FVector2D(Local.X * ViewportPixels.X / Viewport.GetLocalSize().X,
                Local.Y * ViewportPixels.Y / Viewport.GetLocalSize().Y);
        };
        const FVector2D CornerA = Pixel(FVector2D::ZeroVector);
        const FVector2D CornerB = Pixel(Geometry.GetLocalSize());
        const FVector2D Origin(FMath::Min(CornerA.X,CornerB.X),FMath::Min(CornerA.Y,CornerB.Y));
        const FVector2D Size(FMath::Abs(CornerB.X-CornerA.X),FMath::Abs(CornerB.Y-CornerA.Y));
        if (!Expect(Origin.Equals(Expected,.15) && Size.Equals(ParentSizes[Index],.15),
            TEXT("Actual Slate Type2 parent pixels differ after opening animation"))) return false;
        if ((Index == 1 || Index == 3) && !Expect(Widget->GetRenderTransform().Scale.Equals(FVector2D(-1,1),.001),
            TEXT("Source right parent half lost horizontal mirror"))) return false;
    }
    const auto* Title = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("Type2Title")));
    if (bUpgrade && !Expect(Title && !Title->GetText().IsEmpty(),
        TEXT("Real barracks upgrade title missing"))) return false;
    if (bUpgrade)
    {
        const auto* PreviewFrame = Cast<UBorder>(Modal->GetWidgetFromName(TEXT("BuildingUpgradePreviewFrame")));
        const auto* FrameSlot = PreviewFrame ? Cast<UCanvasPanelSlot>(PreviewFrame->Slot) : nullptr;
        if (!Expect(FrameSlot && Visible(PreviewFrame)
            && FrameSlot->GetPosition().Equals(FVector2D(203,110.3),.02)
            && FrameSlot->GetSize().Equals(FVector2D(450,620),.02)
            && FrameSlot->GetZOrder()==0
            && PreviewFrame->GetRenderTransform().Scale.Equals(FVector2D(1,1),.001)
            && PreviewFrameSourceMatches(Modal.Get()),
            TEXT("Native Left/bg rectangle, orientation or pl_com_1008 slices differ"))) return false;
        const FGeometry FrameGeometry=PreviewFrame->GetCachedGeometry();
        const FVector2D FrameStart=Viewport.AbsoluteToLocal(FrameGeometry.LocalToAbsolute(FVector2D::ZeroVector));
        const FVector2D FrameEnd=Viewport.AbsoluteToLocal(FrameGeometry.LocalToAbsolute(FrameGeometry.GetLocalSize()));
        const FVector2D FramePixelStart(FrameStart.X*ViewportPixels.X/Viewport.GetLocalSize().X,
            FrameStart.Y*ViewportPixels.Y/Viewport.GetLocalSize().Y);
        const FVector2D FramePixelEnd(FrameEnd.X*ViewportPixels.X/Viewport.GetLocalSize().X,
            FrameEnd.Y*ViewportPixels.Y/Viewport.GetLocalSize().Y);
        if (!Expect(FramePixelStart.Equals(FVector2D(203,110.3),.15)
            && (FramePixelEnd-FramePixelStart).Equals(FVector2D(450,620),.15)
            && FramePixelEnd.Y < BarracksPositions[0].Y+ParentSizes[0].Y,
            TEXT("Native Left/bg actual pixels extend beyond source parent body"))) return false;
        const auto* Preview = Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget"));
        const auto* Glow = Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget"));
        const auto* Attributes = Modal->GetWidgetFromName(TEXT("BarracksUpgradeAttributes"));
        const auto* PreviewSlot = Preview ? Cast<UCanvasPanelSlot>(Preview->Slot) : nullptr;
        const auto* GlowSlot = Glow ? Cast<UCanvasPanelSlot>(Glow->Slot) : nullptr;
        const auto* AttributesSlot = Attributes ? Cast<UCanvasPanelSlot>(Attributes->Slot) : nullptr;
        if (!Expect(PreviewSlot && GlowSlot && AttributesSlot && Visible(Preview) && Visible(Glow) && Visible(Attributes)
            && PreviewSlot->GetPosition().Equals(FVector2D(248.80003,290.5),.02)
            && PreviewSlot->GetSize().Equals(FVector2D(358.39998,343),.02)
            && GlowSlot->GetPosition().Equals(FVector2D::ZeroVector,.02)
            && GlowSlot->GetSize().Equals(FVector2D(1440,810),.02)
            && AttributesSlot->GetPosition().Equals(FVector2D(670,228.17004),.02)
            && AttributesSlot->GetSize().Equals(FVector2D(536,110),.02),
            TEXT("Accepted preview/glow/attribute absolute layout moved with parent"))) return false;
    }
    else if (!Expect(!Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget")),
        TEXT("Building info page inherited barracks upgrade glow"))) return false;
    return true;
}
TSharedRef<FJsonObject> URokBarracksParentWindowAuditSubsystem::State() const
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("page_kind"), PageKind == 1 ? TEXT("barracks_upgrade") : PageKind == 2 ? TEXT("barracks_info") : TEXT("closed"));
    Result->SetNumberField(TEXT("frame"), GFrameCounter); Result->SetBoolField(TEXT("state_unchanged"), true);
    const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
    const FVector2D ViewportPixels = UWidgetLayoutLibrary::GetViewportSize(this);
    Result->SetObjectField(TEXT("viewport_pixels"), MakeParentRectJson(FVector2D::ZeroVector, ViewportPixels));
    auto Parents = MakeShared<FJsonObject>();
    if (PageKind != 0)
    {
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(ParentNames); ++Index)
        {
            const UWidget* Widget = Modal->GetWidgetFromName(ParentNames[Index]);
            const auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
            const FGeometry Geometry = Widget->GetCachedGeometry();
            const auto Pixel = [&](FVector2D Point)
            {
                const FVector2D Local = Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Point));
                return FVector2D(Local.X * ViewportPixels.X / Viewport.GetLocalSize().X,
                    Local.Y * ViewportPixels.Y / Viewport.GetLocalSize().Y);
            };
            const FVector2D CornerA = Pixel(FVector2D::ZeroVector);
            const FVector2D CornerB = Pixel(Geometry.GetLocalSize());
            const FVector2D Origin(FMath::Min(CornerA.X,CornerB.X),FMath::Min(CornerA.Y,CornerB.Y));
            const FVector2D Size(FMath::Abs(CornerB.X-CornerA.X),FMath::Abs(CornerB.Y-CornerA.Y));
            auto Entry = MakeShared<FJsonObject>();
            Entry->SetObjectField(TEXT("slot_rect"), MakeParentRectJson(Slot->GetPosition(), Slot->GetSize()));
            Entry->SetObjectField(TEXT("pixel_rect"), MakeParentRectJson(Origin, Size));
            Entry->SetNumberField(TEXT("z_order"), Slot->GetZOrder());
            Entry->SetBoolField(TEXT("visible"), Visible(Widget));
            Entry->SetBoolField(TEXT("mirror_x"), Widget->GetRenderTransform().Scale.X < 0);
            if (Index == 4) Entry->SetStringField(TEXT("text"), Cast<UTextBlock>(Widget)->GetText().ToString());
            Parents->SetObjectField(ParentNames[Index].ToString(), Entry);
        }
    }
    Result->SetObjectField(TEXT("parent_widgets"), Parents);
    if (PageKind == 1)
    {
        const auto* Frame=Cast<UBorder>(Modal->GetWidgetFromName(TEXT("BuildingUpgradePreviewFrame")));
        const auto* Slot=Cast<UCanvasPanelSlot>(Frame->Slot);
        const FGeometry Geometry=Frame->GetCachedGeometry();
        const FVector2D A=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
        const FVector2D B=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize()));
        const FVector2D PixelA(A.X*ViewportPixels.X/Viewport.GetLocalSize().X,A.Y*ViewportPixels.Y/Viewport.GetLocalSize().Y);
        const FVector2D PixelB(B.X*ViewportPixels.X/Viewport.GetLocalSize().X,B.Y*ViewportPixels.Y/Viewport.GetLocalSize().Y);
        const auto* Slice=Cast<UImage>(Modal->GetWidgetFromName(TEXT("BuildingUpgradePreviewFrameSlice_0")));
        auto Background=MakeShared<FJsonObject>();
        Background->SetObjectField(TEXT("slot_rect"),MakeParentRectJson(Slot->GetPosition(),Slot->GetSize()));
        Background->SetObjectField(TEXT("pixel_rect"),MakeParentRectJson(PixelA,PixelB-PixelA));
        Background->SetNumberField(TEXT("z_order"),Slot->GetZOrder());
        Background->SetBoolField(TEXT("visible"),Visible(Frame));
        Background->SetBoolField(TEXT("mirror_x"),Frame->GetRenderTransform().Scale.X<0);
        Background->SetStringField(TEXT("atlas_texture"),Slice->GetBrush().GetResourceObject()->GetPathName());
        Background->SetStringField(TEXT("native_sprite"),TEXT("pl_com_1008"));
        Background->SetBoolField(TEXT("nine_slices_source_verified"),PreviewFrameSourceMatches(Modal.Get()));
        Result->SetObjectField(TEXT("preview_background"),Background);
    }
    return Result;
}
void URokBarracksParentWindowAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Validate()) return;
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Conflicting screenshot request")); return; }
    PendingPath = Directory / (FString(Name) + TEXT(".png"));
    Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("file"), PendingPath);
    Pending->SetObjectField(TEXT("request"), State()); ShotStarted = Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokBarracksParentWindowAuditSubsystem::ScreenshotProcessed()
{
    if (!bOwnsTargetWorld || bFinished || !Pending.IsValid() || !Validate()) return;
    TArray<uint8> Bytes; const uint8 Signature[] = { 137,80,78,71,13,10,26,10 };
    if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24
        || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0) { Finish(TEXT("Missing processed real UI PNG")); return; }
    const auto U32 = [&](int32 Offset) { return (uint32(Bytes[Offset])<<24) | (uint32(Bytes[Offset+1])<<16)
        | (uint32(Bytes[Offset+2])<<8) | uint32(Bytes[Offset+3]); };
    if (U32(16) != 1440 || U32(20) != 810) { Finish(TEXT("Processed viewport must be 1440x810")); return; }
    Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetNumberField(TEXT("file_bytes"), Bytes.Num());
    Pending->SetObjectField(TEXT("processed"), State());
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
    if (!Save(false)) { Finish(TEXT("Incremental parent manifest write failed")); return; }
    Enter(Step+1);
}
void URokBarracksParentWindowAuditSubsystem::Tick(float DeltaTime)
{
    if (!bOwnsTargetWorld || bFinished || !GetWorld() || !GetWorld()->HasBegunPlay()) return;
    Elapsed += FMath::Max(0.f, DeltaTime);
    if (Elapsed > 60) { Finish(TEXT("Parent-window audit timeout")); return; }
    if (Pending.IsValid()) { if (Elapsed-ShotStarted > 10) Finish(TEXT("Parent screenshot callback timeout")); return; }
    if (Elapsed < 3 || Elapsed-StepStarted < .5) return;
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: Capture(TEXT("01_selected_city")); break;
    case 2: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))) { PageKind=1; Enter(3); } break;
    case 3: Capture(TEXT("02_barracks_upgrade")); break;
    case 4: if (Click(Modal.Get(),TEXT("Type2CloseButton"))) { PageKind=0; Enter(5); } break;
    case 5: Capture(TEXT("03_closed")); break;
    case 6: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))) { PageKind=1; Enter(7); } break;
    case 7: Capture(TEXT("04_barracks_reopened")); break;
    case 8: if (Click(Modal.Get(),TEXT("Type2CloseButton"))) { PageKind=0; Enter(9); } break;
    case 9: if (Validate() && Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingInfoButton"))) { PageKind=2; Enter(10); } break;
    case 10: Capture(TEXT("05_barracks_info_default")); break;
    case 11: if (Click(Modal.Get(),TEXT("Type2CloseButton"))) { PageKind=0; Enter(12); } break;
    case 12: Capture(TEXT("06_final_city")); break;
    case 13: Finish(); break;
    default: Finish(TEXT("Invalid parent-window audit step")); break;
    }
}
bool URokBarracksParentWindowAuditSubsystem::Save(bool Complete)
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.barracks.parent_window.runtime.v2"));
    Root->SetBoolField(TEXT("complete"),Complete);
    Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty() && Captures.Num()==CaptureCount);
    Root->SetNumberField(TEXT("expected_capture_count"),CaptureCount);
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("scope"),TEXT("Native Type1 parent and visible Left/bg rectangles on real Init6/type9 upgrade, close/reopen and same-actor BuildingInfo Type2 default regression. Real Slate screenshots; navigation delegates only. No upgrade command, physical pointer test, full-content parity, or normal-world visual acceptance."));
    Root->SetBoolField(TEXT("capture_includes_slate_ui"),true);
    Root->SetBoolField(TEXT("owns_target_world"),bOwnsTargetWorld);
    Root->SetArrayField(TEXT("captures"),Captures); Root->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Failure : Failures) Errors.Add(MakeShared<FJsonValueString>(Failure));
    Root->SetArrayField(TEXT("failures"),Errors);
    FString Json; return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))
        && FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokBarracksParentWindowAuditSubsystem::Finish(const FString& Error)
{
    if (!bOwnsTargetWorld || bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num()!=CaptureCount) Failures.Add(TEXT("Expected six real UI screenshots"));
    bFinished=true; const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures) UE_LOG(LogTemp,Error,TEXT("Rok BarracksParentWindowAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok BarracksParentWindowAudit passed=%d captures=%d output=%s"),Failures.IsEmpty()&&Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
