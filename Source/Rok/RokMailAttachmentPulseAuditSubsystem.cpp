#include "RokMailAttachmentPulseAuditSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokMailAttachmentMeshWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

bool URokMailAttachmentPulseAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(), TEXT("RokMailAttachmentPulseAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMailAttachmentPulseAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokMailAttachmentPulseAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/MailAttachmentPulseAudit")
        / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokMailAttachmentPulseAuditSubsystem::ScreenshotProcessed);
}
void URokMailAttachmentPulseAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    Super::Deinitialize();
}
TStatId URokMailAttachmentPulseAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokMailAttachmentPulseAuditSubsystem, STATGROUP_Tickables); }
bool URokMailAttachmentPulseAuditSubsystem::Identity()
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
    if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) || !GetWorld()
        || GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
    { Finish(TEXT("Wrong project/map; no UI actions allowed.")); return false; }
    return true;
}
bool URokMailAttachmentPulseAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor = FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor, Token, false))
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
            && ((Token.Contains(TEXT("Audit"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokMailAttachmentPulseAudit"), ESearchCase::IgnoreCase))
                || Token.StartsWith(TEXT("-RokOpen"), ESearchCase::IgnoreCase) || Token.StartsWith(TEXT("-RokAuto"), ESearchCase::IgnoreCase)))
        { Finish(TEXT("Run pulse audit alone without competing audit/open/auto flags.")); return false; }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets) if (Widget->GetWorld() == GetWorld() && Widget->IsVisible())
    {
        if (Hud.IsValid()) { Finish(TEXT("Multiple city HUDs.")); return false; }
        Hud = Cast<URokMainHudWidget>(Widget);
    }
    Widgets.Reset();
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokUnityFeatureScreenWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets) if (Widget->GetWorld() == GetWorld())
    {
        if (Screen.IsValid()) { Finish(TEXT("Multiple feature screens.")); return false; }
        Screen = Cast<URokUnityFeatureScreenWidget>(Widget);
    }
    if (!Hud.IsValid() || !Screen.IsValid() || Screen->IsScreenOpen()) { Finish(TEXT("Expected city HUD and closed feature screen.")); return false; }
    return Click(Hud.Get(), TEXT("MailFeatureHitButton"));
}
bool URokMailAttachmentPulseAuditSubsystem::Click(UUserWidget* Owner, const TCHAR* Name)
{
    if (!Identity() || !Owner) return false;
    auto* Button = Cast<UButton>(Owner->GetWidgetFromName(Name));
    if (!Button || !Button->IsVisible() || !Button->GetIsEnabled()) { Finish(FString::Printf(TEXT("Missing enabled button %s"), Name)); return false; }
    Button->OnClicked.Broadcast(); // Native delegate; never claim pointer hit-testing.
    return true;
}
UWidget* URokMailAttachmentPulseAuditSubsystem::Pulse(int32 Index) const
{
    return Screen.IsValid() ? Screen->GetWidgetFromName(*FString::Printf(TEXT("MailAttachmentPulse_%d"), Index)) : nullptr;
}
URokMailAttachmentMeshWidget* URokMailAttachmentPulseAuditSubsystem::Mesh(int32 Index) const
{
    return Screen.IsValid() ? Cast<URokMailAttachmentMeshWidget>(Screen->GetWidgetFromName(*FString::Printf(TEXT("MailAttachmentMesh_%d"), Index))) : nullptr;
}
void URokMailAttachmentPulseAuditSubsystem::RecordMesh(const TSharedPtr<FJsonObject>& Object, const FString& Prefix) const
{
    const auto* Widget = Mesh(0);
    Object->SetBoolField(Prefix + TEXT("mesh_present"), Widget != nullptr);
    if (!Widget) return;
    Object->SetNumberField(Prefix + TEXT("animation_time"), Widget->GetAnimationTime());
    Object->SetNumberField(Prefix + TEXT("paint_frame"), Widget->GetLastPaintFrameIndex());
    Object->SetNumberField(Prefix + TEXT("paint_count"), Widget->GetPaintCount());
    Object->SetNumberField(Prefix + TEXT("load_generation"), Widget->GetLoadGeneration());
    Object->SetNumberField(Prefix + TEXT("object_id"), Widget->GetUniqueID());
}
bool URokMailAttachmentPulseAuditSubsystem::RecordSourceGeometry(const TSharedPtr<FJsonObject>& Object) const
{
    // strict-approved native_20261008_114400_572 reference_single, frame 269:
    // row [66.684,127.972,457.046,114.242], bag [455.130,159.405,31.897,28.479]
    // at scale 1.139065. The bag is right/top anchored, so compare its
    // unscaled 28x25 source extent using the actual UE pulse phase.
    if (!Screen.IsValid() || !Object) return false;
    const UWidget* Row = Screen->GetWidgetFromName(TEXT("MailRow_0"));
    const UWidget* Parent = Pulse(0);
    const UWidget* Child = Mesh(0);
    if (!Row || !Parent || !Child) return false;
    const FSlateRect RootRect = Screen->GetCachedGeometry().GetRenderBoundingRect();
    auto ToViewport = [&RootRect](FSlateRect Rect)
    {
        Rect.Left -= RootRect.Left; Rect.Right -= RootRect.Left;
        Rect.Top -= RootRect.Top; Rect.Bottom -= RootRect.Top;
        return Rect;
    };
    const FSlateRect RowRect = ToViewport(Row->GetCachedGeometry().GetRenderBoundingRect());
    const FSlateRect ParentRect = ToViewport(Parent->GetCachedGeometry().GetRenderBoundingRect());
    const FSlateRect ChildRect = ToViewport(Child->GetCachedGeometry().GetRenderBoundingRect());
    auto SaveRect = [Object](const TCHAR* Key, const FSlateRect& Rect)
    {
        auto Value = MakeShared<FJsonObject>();
        Value->SetNumberField(TEXT("x"), Rect.Left);
        Value->SetNumberField(TEXT("y"), Rect.Top);
        Value->SetNumberField(TEXT("width"), Rect.Right - Rect.Left);
        Value->SetNumberField(TEXT("height"), Rect.Bottom - Rect.Top);
        Object->SetObjectField(Key, Value);
    };
    SaveRect(TEXT("row_cached_rect"), RowRect);
    SaveRect(TEXT("pulse_cached_rect"), ParentRect);
    SaveRect(TEXT("mesh_cached_rect"), ChildRect);
    SaveRect(TEXT("viewport_cached_rect"), ToViewport(RootRect));
    const double Scale = Parent->GetRenderTransform().Scale.X;
    Object->SetNumberField(TEXT("pulse_scale"), Scale);
    constexpr double SourceBagRight = 487.02655029296875;
    constexpr double SourceBagTop = 159.4052734375;
    constexpr double SourceRowX = 66.68431091308594;
    constexpr double SourceRowY = 127.97210693359375;
    constexpr double SourceRowWidth = 457.04595947265625;
    constexpr double SourceRowHeight = 114.24151611328125;
    const bool bViewport = FMath::Abs((RootRect.Right - RootRect.Left) - 1440.0) <= 1.0
        && FMath::Abs((RootRect.Bottom - RootRect.Top) - 810.0) <= 1.0;
    const bool bRow = FMath::Abs(RowRect.Left - SourceRowX) <= 1.0
        && FMath::Abs(RowRect.Top - SourceRowY) <= 1.0
        && FMath::Abs((RowRect.Right - RowRect.Left) - SourceRowWidth) <= 1.0
        && FMath::Abs((RowRect.Bottom - RowRect.Top) - SourceRowHeight) <= 1.0;
    const bool bBag = FMath::Abs(ParentRect.Right - SourceBagRight) <= 1.0
        && FMath::Abs(ParentRect.Top - SourceBagTop) <= 1.0
        && FMath::Abs((ParentRect.Right - ParentRect.Left) - 28.0 * Scale) <= 1.0
        && FMath::Abs((ParentRect.Bottom - ParentRect.Top) - 25.0 * Scale) <= 1.0;
    Object->SetBoolField(TEXT("source_row_geometry_pass"), bRow);
    Object->SetBoolField(TEXT("source_bag_geometry_pass"), bBag);
    Object->SetBoolField(TEXT("source_viewport_geometry_pass"), bViewport);
    return bViewport && bRow && bBag;
}
void URokMailAttachmentPulseAuditSubsystem::RecordDeferredAction(const TCHAR* Name)
{
    ActionStarted = Elapsed; ActionFrame = GFrameCounter;
    // Feature executes PendingActionId in its next widget tick. Include the
    // actual Slate frame in which the delegate was queued, then each observed
    // frame until the changed widget tree is visible to this audit.
    ActionSlateBudget = FMath::Max(0.0, static_cast<double>(FSlateApplication::Get().GetDeltaTime()));
    auto Event = MakeShared<FJsonObject>(); Event->SetStringField(TEXT("action"), Name);
    Event->SetNumberField(TEXT("click_elapsed"), Elapsed); Event->SetNumberField(TEXT("click_frame"), ActionFrame);
    Event->SetNumberField(TEXT("phase_before"), ActionMeshTime); Event->SetNumberField(TEXT("paused_phase"), PausedMeshTime);
    MeshSamples.Add(MakeShared<FJsonValueObject>(Event));
}
bool URokMailAttachmentPulseAuditSubsystem::ValidateDeferredPhase(float BeforeTime)
{
    const auto* Current = RetainedMesh.Get();
    if (!Current) { Finish(TEXT("Source Spine instance lost during deferred action.")); return false; }
    const float After = Current->GetAnimationTime();
    const float Advanced = After >= BeforeTime ? After - BeforeTime : After + Current->GetAnimationDuration() - BeforeTime;
    auto Event = MakeShared<FJsonObject>(); Event->SetStringField(TEXT("action"), TEXT("deferred_phase_observed"));
    Event->SetNumberField(TEXT("observed_elapsed"), Elapsed); Event->SetNumberField(TEXT("observed_frame"), GFrameCounter);
    Event->SetNumberField(TEXT("phase_before"), BeforeTime); Event->SetNumberField(TEXT("phase_after"), After);
    Event->SetNumberField(TEXT("phase_advance"), Advanced); Event->SetNumberField(TEXT("active_frame_budget"), ActionSlateBudget);
    MeshSamples.Add(MakeShared<FJsonValueObject>(Event));
    if (ActionSlateBudget >= Current->GetAnimationDuration() || Advanced > ActionSlateBudget + .0001)
    { Finish(TEXT("Deferred source Spine advanced beyond observed active-frame time budget.")); return false; }
    return true;
}
bool URokMailAttachmentPulseAuditSubsystem::ValidateEmpty()
{
    if (!Screen.IsValid() || !Screen->IsScreenOpen() || !Screen->GetWidgetFromName(TEXT("MailEmptyState")))
    { Finish(TEXT("Expected the real empty mail category.")); return false; }
    for (int32 I = 0; I < 6; ++I) if (Pulse(I) || Mesh(I)) { Finish(TEXT("Empty mail retained an attachment pulse/mesh.")); return false; }
    return true;
}
bool URokMailAttachmentPulseAuditSubsystem::ValidateReference()
{
    if (!Screen.IsValid() || !Screen->IsScreenOpen() || !Screen->GetWidgetFromName(TEXT("MailReferencePreviewNotice")))
    { Finish(TEXT("Reference mailbox must remain explicitly labelled.")); return false; }
    for (int32 I = 0; I < 6; ++I)
    {
        UWidget* Parent = Pulse(I);
        if (I == 1) { if (Parent) { Finish(TEXT("Source fixture row without attachment gained pulse.")); return false; } continue; }
        auto* Chest = Mesh(I);
        if (!Parent || !Chest || Chest->GetParent() != Parent || !Parent->GetRenderTransformPivot().Equals(FVector2D(1, 0), .0001))
        { Finish(TEXT("Source Chinese attachment parent/child/pivot differs.")); return false; }
        const auto* ParentSlot = Cast<UCanvasPanelSlot>(Parent->Slot);
        const auto* ChildSlot = Cast<UCanvasPanelSlot>(Chest->Slot);
        if (!ParentSlot || !ChildSlot || !ParentSlot->GetPosition().Equals(FVector2D(392.32, 31.43), .02)
            || !ParentSlot->GetSize().Equals(FVector2D(28, 25), .001) || !ChildSlot->GetPosition().Equals(FVector2D(0, 0), .001)
            || !ChildSlot->GetSize().Equals(FVector2D(28, 25), .001) || Chest->GetRenderTransform().Angle != 0.f
            || !Chest->IsModelLoaded() || Chest->GetFrameCount() != 96 || !FMath::IsNearlyEqual(Chest->GetAnimationDuration(), 1.6f))
        { Finish(TEXT("Source attachment Spine geometry/clip contract differs.")); return false; }
        const FVector2D Scale = Parent->GetRenderTransform().Scale;
        if (Scale.X < .99999 || Scale.X > 1.15001 || !FMath::IsNearlyEqual(Scale.X, Scale.Y, .00001) || Parent->GetRenderOpacity() != 1.f)
        { Finish(TEXT("Pulse changed unsupported transform/opacity or left source range.")); return false; }
    }
    return true;
}
void URokMailAttachmentPulseAuditSubsystem::Enter(int32 Next) { Step = Next; StepStarted = Elapsed; }
void URokMailAttachmentPulseAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Identity() || Pending.IsValid()) return;
    PendingPath = Directory / (FString(Name) + TEXT(".png"));
    Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("file"), PendingPath);
    Pending->SetNumberField(TEXT("request_elapsed"), Elapsed);
    Pending->SetNumberField(TEXT("step"), Step);
    if (const auto* Parent = Pulse(0)) Pending->SetNumberField(TEXT("request_scale"), Parent->GetRenderTransform().Scale.X);
    RecordMesh(Pending, TEXT("request_"));
    ScreenshotStarted = Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokMailAttachmentPulseAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid()) return;
    if (!Identity()) return;
    if (Step == 1 || Step == 7 || Step == 13)
    {
        if (!ValidateEmpty()) return;
        if (Step == 13 && Screen->GetWidgetFromName(TEXT("MailReferencePreviewNotice")))
        { Finish(TEXT("Processed normal reentry leaked reference state.")); return; }
    }
    else if (Step == 3 || Step == 5 || Step == 9 || Step == 15)
    {
        if (!ValidateReference()) return;
        const double Scale = Pulse(0)->GetRenderTransform().Scale.X;
        Pending->SetNumberField(TEXT("processed_scale"), Scale);
        if (Mesh(0)->GetPaintCount() == 0 || Mesh(0)->GetLastPaintDrawCount() != 2)
        { Finish(TEXT("Processed attachment screenshot lacks actual Spine mesh paint.")); return; }
        if ((Step == 3 || Step == 9) && Scale <= 1.14)
        { Finish(TEXT("Processed peak screenshot no longer shows the requested pulse peak.")); return; }
        if (Step == 5 && Scale < 1.05)
        { Finish(TEXT("Processed selection screenshot no longer shows the continuing pulse.")); return; }
    }
    else if (Step != 11 || !Screen.IsValid() || Screen->IsScreenOpen() || Pulse(0) || Mesh(0))
    { Finish(TEXT("Processed screenshot does not match its expected page.")); return; }
    Pending->SetNumberField(TEXT("processed_elapsed"), Elapsed);
    RecordMesh(Pending, TEXT("processed_"));
    if (Step == 3 || Step == 5 || Step == 9 || Step == 15)
        if (!RecordSourceGeometry(Pending))
        { Finish(TEXT("Processed mail row/attachment cached geometry differs from final Unity source frame.")); return; }
    TArray<uint8> Bytes;
    const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0)
    { Finish(TEXT("Processed pulse screenshot missing or invalid.")); return; }
    Pending->SetNumberField(TEXT("file_bytes"), Bytes.Num());
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset(); Enter(Step + 1);
}
void URokMailAttachmentPulseAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !GetWorld()->HasBegunPlay()) return;
    Elapsed += FMath::Max(0.f, DeltaTime);
    if ((Step == 5 || (Step == 7 && !bPauseBaselineSet) || (Step == 9 && !bTabResumed)) && GFrameCounter != ActionFrame)
    { ActionSlateBudget += FMath::Max(0.0, static_cast<double>(FSlateApplication::Get().GetDeltaTime())); ActionFrame = GFrameCounter; }
    if (Screen.IsValid())
    {
        auto Sample = MakeShared<FJsonObject>(); Sample->SetNumberField(TEXT("elapsed"), Elapsed); Sample->SetNumberField(TEXT("step"), Step);
        RecordMesh(Sample, TEXT("")); MeshSamples.Add(MakeShared<FJsonValueObject>(Sample));
    }
    if (Elapsed > 60) { Finish(TEXT("Pulse audit timed out.")); return; }
    if (Pending.IsValid()) { if (Elapsed - ScreenshotStarted > 10) Finish(TEXT("Screenshot callback timed out.")); return; }
    const double Delay = Step == 0 ? 3.0 : (Step == 3 || Step == 4 || Step == 5 || Step == 7 || Step == 9 ? 0.0 : .5);
    if (Elapsed - StepStarted < Delay) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: if (ValidateEmpty()) Capture(TEXT("01_normal_mail_empty")); break;
    case 2:
        Screen->OpenMail(true);
        if (!ValidateReference()) break;
        if (Mesh(0)->GetAnimationTime() != 0) { Finish(TEXT("New source instance did not initialize at zero.")); break; }
        RetainedMesh = Mesh(0); PreviousMeshTime = 0;
        TraceStarted = Elapsed; Peaks = 0; bAbovePeak = false; Enter(3); break;
    case 3:
        if (ValidateReference())
        {
            const double Scale = Pulse(0)->GetRenderTransform().Scale.X;
            auto Sample = MakeShared<FJsonObject>(); Sample->SetNumberField(TEXT("elapsed"), Elapsed - TraceStarted); Sample->SetNumberField(TEXT("scale"), Scale);
            Samples.Add(MakeShared<FJsonValueObject>(Sample));
            const float ChildTime = Mesh(0)->GetAnimationTime();
            if (PreviousMeshTime > 1.2f && ChildTime < .4f) ++MeshLoops;
            PreviousMeshTime = ChildTime;
            if (Scale > 1.14 && !bAbovePeak) { ++Peaks; bAbovePeak = true; }
            if (Scale < 1.1) bAbovePeak = false;
            if (Peaks >= 3 && MeshLoops >= 1 && Scale > 1.14 && Elapsed - TraceStarted > 2.0) Capture(TEXT("02_reference_loop_peak"));
        }
        break;
    case 4:
        if (ValidateReference() && Pulse(0)->GetRenderTransform().Scale.X > 1.14)
        {
            RetainedMesh = Mesh(0); ActionMeshTime = RetainedMesh->GetAnimationTime(); ActionLoadGeneration = RetainedMesh->GetLoadGeneration();
            RecordDeferredAction(TEXT("select_row_2"));
            if (Click(Screen.Get(), TEXT("MailRowHit_2")))
            {
                Enter(5);
            }
        }
        break;
    case 5:
        {
        const auto* SelectedTitle = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("MailRowTitle_2")));
        const auto* PreviousTitle = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("MailRowTitle_0")));
        // Final Chinese native row lbl_title is pure black when selected.
        // Verify both sides of the state transition, not merely that row 2 exists.
        if (!SelectedTitle || !PreviousTitle
            || !SelectedTitle->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::Black, .0001f)
            || !PreviousTitle->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::FromSRGBColor(FColor(226, 209, 155)), .0001f))
        { if (Elapsed - ActionStarted > 1) Finish(TEXT("Deferred mail selection did not update the actual selected row.")); break; }
        if (ValidateReference())
        {
            if (Mesh(0) != RetainedMesh.Get() || Mesh(0)->GetLoadGeneration() != ActionLoadGeneration)
            { Finish(TEXT("Deferred selection replaced/reinitialized source Spine.")); break; }
            if (!ValidateDeferredPhase(ActionMeshTime)) break;
            bSelectionContinued = true;
            if (Pulse(0)->GetRenderTransform().Scale.X < 1.05) { Finish(TEXT("Selecting mail reset a still-visible parent pulse.")); break; }
            Capture(TEXT("03_selection_preserves_pulse"));
        }
        }
        break;
    case 6:
        RetainedMesh = Mesh(0);
        if (!RetainedMesh.IsValid()) { Finish(TEXT("Missing mesh before tab hide.")); break; }
        ActionMeshTime = RetainedMesh->GetAnimationTime(); RecordDeferredAction(TEXT("hide_report_tab"));
        if (Click(Screen.Get(), TEXT("MailTab_1"))) { bPauseBaselineSet = false; Enter(7); }
        break;
    case 7:
        if (Mesh(0) || !Screen->GetWidgetFromName(TEXT("MailEmptyState")))
        { if (Elapsed - ActionStarted > 1) Finish(TEXT("Deferred report tab did not remove the reference rows.")); break; }
        if (!bPauseBaselineSet)
        {
            if (!ValidateDeferredPhase(ActionMeshTime)) break;
            PausedMeshTime = RetainedMesh->GetAnimationTime(); PausedPaintCount = RetainedMesh->GetPaintCount();
            bPauseBaselineSet = true; StepStarted = Elapsed; break;
        }
        if (Elapsed - StepStarted < .5) break;
        if (ValidateEmpty())
        {
            if (!RetainedMesh.IsValid() || RetainedMesh->GetAnimationTime() != PausedMeshTime || RetainedMesh->GetPaintCount() != PausedPaintCount)
            { Finish(TEXT("Detached source Spine did not pause while hidden.")); break; }
            bTabPaused = true; Capture(TEXT("04_report_page_cleared"));
        }
        break;
    case 8:
        if (!RetainedMesh.IsValid() || RetainedMesh->GetAnimationTime() != PausedMeshTime)
        { Finish(TEXT("Hidden source Spine changed before reentry.")); break; }
        ActionMeshTime = PausedMeshTime; ActionLoadGeneration = RetainedMesh->GetLoadGeneration(); RecordDeferredAction(TEXT("resume_system_tab"));
        if (Click(Screen.Get(), TEXT("MailTab_0"))) { bTabResumed = false; Enter(9); }
        break;
    case 9:
        if (!Mesh(0)) { if (Elapsed - ActionStarted > 1) Finish(TEXT("Deferred system tab did not attach source mesh.")); break; }
        if (!bTabResumed)
        {
            if (Mesh(0) != RetainedMesh.Get() || Mesh(0)->GetLoadGeneration() != ActionLoadGeneration)
            { Finish(TEXT("Deferred tab reentry replaced/reinitialized source Spine.")); break; }
            if (!ValidateDeferredPhase(PausedMeshTime)) break;
            bTabResumed = true;
        }
        if (ValidateReference() && Pulse(0)->GetRenderTransform().Scale.X > 1.14)
        {
            if (Mesh(0)->GetPaintCount() <= PausedPaintCount || Mesh(0)->GetAnimationTime() == PausedMeshTime)
            { Finish(TEXT("Reentered source Spine did not resume ticking/painting.")); break; }
            Capture(TEXT("05_system_reentry_peak"));
        }
        break;
    case 10: if (Click(Screen.Get(), TEXT("FeatureFrameClose"))) Enter(11); break;
    case 11:
        if (Screen->IsScreenOpen() || Pulse(0) || Mesh(0)) { Finish(TEXT("Close retained mail pulse/mesh presentation.")); break; }
        Capture(TEXT("06_city_after_close")); break;
    case 12: if (Click(Hud.Get(), TEXT("MailFeatureHitButton"))) Enter(13); break;
    case 13:
        if (ValidateEmpty())
        {
            if (Screen->GetWidgetFromName(TEXT("MailReferencePreviewNotice"))) { Finish(TEXT("Normal reentry leaked reference state.")); break; }
            Capture(TEXT("07_normal_reentry_empty"));
        }
        break;
    case 14:
        Screen->OpenMail(true);
        if (!ValidateReference()) break;
        // Same WidgetTree/name may reconstruct a UObject at the previous address.
        // Fresh lifecycle is established by Close/empty checks plus initialized
        // model state, not allocator identity. Selection/tab checks stay strict.
        bFreshReusedObjectAddress = Mesh(0) == RetainedMesh.Get();
        if (Mesh(0)->GetAnimationTime() != 0 || Mesh(0)->GetPaintCount() != 0
            || Mesh(0)->GetLoadGeneration() != 1 || Mesh(0)->GetLastLoadStartTime() != 0
            || Mesh(0)->GetLastPaintFrameIndex() != INDEX_NONE || !Mesh(0)->GetLoadError().IsEmpty())
        { Finish(TEXT("Reference reopen after Close retained old source model/phase state.")); break; }
        bFreshAfterClose = true; Enter(15); break;
    case 15: if (ValidateReference()) Capture(TEXT("08_fresh_reference_after_close")); break;
    case 16: if (Click(Screen.Get(), TEXT("FeatureFrameClose"))) Enter(17); break;
    case 17: if (!Screen->IsScreenOpen() && !Pulse(0) && !Mesh(0)) Finish(); break;
    default: Finish(TEXT("Unexpected audit state.")); break;
    }
}
void URokMailAttachmentPulseAuditSubsystem::Finish(const FString& Failure)
{
    if (bFinished) return;
    if (!Failure.IsEmpty()) Failures.Add(Failure);
    if (Failures.IsEmpty() && (Captures.Num() != 8 || MeshLoops < 1 || !bSelectionContinued || !bTabPaused || !bTabResumed || !bFreshAfterClose))
        Failures.Add(TEXT("Expected eight screenshots and complete source Spine lifecycle checks."));
    auto Root = MakeShared<FJsonObject>();
    Root->SetBoolField(TEXT("passed"), Failures.IsEmpty()); Root->SetBoolField(TEXT("complete"), Failures.IsEmpty());
    Root->SetStringField(TEXT("scope"), TEXT("Real HUD normal-mail delegates; explicit OpenMail(true) fixture; real parent pulse and child gift5/stay Slate paint/tick. No claim, authority or physical pointer test."));
    Root->SetStringField(TEXT("geometry_contract"), TEXT("mail_native_final_v2"));
    Root->SetStringField(TEXT("geometry_source"), TEXT("Saved/UnityMailRewardNativeProbe/Artifacts/native_20261008_114400_572/render_evidence.json#samples[0].leftMailRowGeometry"));
    Root->SetStringField(TEXT("geometry_source_sha256"), TEXT("34918ef7746a74dbfd08bb4512222d653b10af7dcc4b3e9ec9edd096973f4e8a"));
    Root->SetArrayField(TEXT("spine_tick_samples"), MeshSamples); Root->SetNumberField(TEXT("spine_loops"), MeshLoops);
    Root->SetBoolField(TEXT("selection_same_instance_continued"), bSelectionContinued);
    Root->SetBoolField(TEXT("tab_paused"), bTabPaused); Root->SetBoolField(TEXT("tab_same_instance_resumed"), bTabResumed);
    Root->SetBoolField(TEXT("close_reopen_fresh_zero"), bFreshAfterClose);
    Root->SetBoolField(TEXT("fresh_reused_object_address_diagnostic_only"), bFreshReusedObjectAddress);
    Root->SetNumberField(TEXT("observed_peaks"), Peaks); Root->SetArrayField(TEXT("native_tick_samples"), Samples); Root->SetArrayField(TEXT("captures"), Captures);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Root->SetArrayField(TEXT("failures"), Errors);
    FString Json; auto Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer) || !FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json"))))
    {
        Failures.Add(TEXT("Final audit manifest save failed."));
        UE_LOG(LogTemp, Error, TEXT("Mail attachment pulse audit failed to save manifest."));
    }
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("Rok mail attachment pulse audit passed=%d captures=%d output=%s"), Failures.IsEmpty(), Captures.Num(), *Directory);
    for (const FString& Error : Failures) UE_LOG(LogTemp, Error, TEXT("Mail attachment pulse audit: %s"), *Error);
    FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1);
}
