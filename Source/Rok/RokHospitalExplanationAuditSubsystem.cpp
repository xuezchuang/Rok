#include "RokHospitalExplanationAuditSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 CaptureCount = 8;
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer, false);
    T::StaticStruct()->SerializeItem(Archive, &Value, nullptr); return Bytes;
}
bool Visible(const UWidget* W)
{
    if (!W || !W->IsVisible() || W->GetRenderOpacity() <= 0) return false;
    for (const UWidget* P = W->GetParent(); P; P = P->GetParent())
        if (!P->IsVisible() || P->GetRenderOpacity() <= 0) return false;
    for (const UObject* O = W->GetOuter(); O; O = O->GetOuter())
        if (const auto* U = Cast<UUserWidget>(O); U && (!U->IsVisible() || U->GetRenderOpacity() <= 0)) return false;
    return true;
}
}
bool URokHospitalExplanationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(), TEXT("RokHospitalExplanationAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokHospitalExplanationAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokHospitalExplanationAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokHospitalExplanationAuditSubsystem, STATGROUP_Tickables); }
void URokHospitalExplanationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/HospitalExplanationAudit") / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokHospitalExplanationAuditSubsystem::ScreenshotProcessed);
}
void URokHospitalExplanationAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    if (bBaseline && !bFinished) { Failures.Add(TEXT("World teardown before audit completion")); Save(false); }
    Super::Deinitialize();
}
bool URokHospitalExplanationAuditSubsystem::Expect(bool Condition, const TCHAR* Error) { if (!Condition) Finish(Error); return Condition; }
void URokHospitalExplanationAuditSubsystem::Enter(int32 Next) { Step = Next; StepStarted = Elapsed; }
bool URokHospitalExplanationAuditSubsystem::Identity()
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
    return Expect(FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) && GetWorld() && GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity"), TEXT("Wrong project/map; no UI action permitted"));
}
bool URokHospitalExplanationAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor = FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor, Token, false))
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokHospitalExplanationAudit"), ESearchCase::IgnoreCase))
        { Finish(TEXT("Hospital audit requires a dedicated process without other Rok flags")); return false; }
    const auto FindHost = [&](UClass* Class) -> UUserWidget*
    {
        TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, Class, false);
        UUserWidget* Result = nullptr; int32 Count = 0;
        for (auto* W : Widgets) if (W->GetWorld() == GetWorld()) { Result = W; ++Count; }
        return Count == 1 ? Result : nullptr;
    };
    Hud = Cast<URokMainHudWidget>(FindHost(URokMainHudWidget::StaticClass()));
    Feature = Cast<URokUnityFeatureScreenWidget>(FindHost(URokUnityFeatureScreenWidget::StaticClass()));
    const auto* Modal = Cast<URokUnityModalWidget>(FindHost(URokUnityModalWidget::StaticClass()));
    auto* Economy = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Feature.IsValid() && !Feature->IsScreenOpen() && Modal && !Modal->IsModalOpen() && Economy && Runtime, TEXT("Expected closed feature/modal and unique live city HUD"))) return false;
    int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        bool SourceBuilding = false;
        for (const FName Tag : It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { SourceBuilding = true; break; }
        if (!SourceBuilding) continue;
        const auto S = Economy->GetBuildingStateForActor(*It);
        if (S.UnityInitId > 0) BuildingSnapshots.Add(*It, Snapshot(S));
        if (It->ActorHasTag(TEXT("UnityInitId_4")))
        {
            Hospital = *It; ++Count;
            if (!Expect(It->ActorHasTag(TEXT("UnityBuildingType_14")) && S.UnityInitId == 4 && S.UnityBuildingTypeId == 14, TEXT("Init4 actor must be source hospital type14"))) return false;
        }
    }
    if (!Expect(Count == 1 && BuildingSnapshots.Num() > 0, TEXT("Expected one existing Init4 hospital"))) return false;
    InitialQueueCount = Economy->GetActiveUpgradeCount(); InitialOwnedQueues = Economy->GetOwnedBuildQueueCount();
    if (!Expect(InitialQueueCount == 0, TEXT("Read-only hospital audit requires idle building queues"))) return false;
    ProfileSnapshot = Snapshot(Runtime->GetPlayerProfileState()); ResourceSnapshot = Snapshot(Economy->GetCurrentResources()); bBaseline = true;
    FString Text; TSharedPtr<FJsonObject> Catalog;
    if (!FFileHelper::LoadFileToString(Text, *(RokDataPaths::Resolve(TEXT("hospital_explanation.json")))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Catalog) || !Catalog.IsValid())
    { Finish(TEXT("Source explanation catalog unavailable")); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Yellows = nullptr;
    if (!Expect(Catalog->TryGetStringField(TEXT("rich_text"), ExpectedRichText) && Catalog->TryGetArrayField(TEXT("yellow_segments"), Yellows) && Yellows->Num() == 13, TEXT("Expected exact source explanation and 13 yellow spans"))) return false;
    for (const auto& Y : *Yellows) YellowSegments.Add(Y->AsString());
    for (int32 I = 1; I <= 7; ++I) if (!Expect(ExpectedRichText.Contains(FString::Printf(TEXT("%d."), I)), TEXT("Missing one of seven source clauses"))) return false;
    Hud->SetSelectedBuilding(Hospital.Get()); // Existing verified actor; not a physical world pointer test.
    return true;
}
bool URokHospitalExplanationAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* E = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* R = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(E && R && Snapshot(R->GetPlayerProfileState()) == ProfileSnapshot && Snapshot(E->GetCurrentResources()) == ResourceSnapshot && E->GetActiveUpgradeCount() == InitialQueueCount && E->GetOwnedBuildQueueCount() == InitialOwnedQueues, TEXT("Read-only route changed resources/profile/queue"))) return false;
    for (const auto& Pair : BuildingSnapshots)
        if (!Expect(Pair.Key.IsValid() && Snapshot(E->GetBuildingStateForActor(Pair.Key.Get())) == Pair.Value, TEXT("Building state changed during explanation navigation"))) return false;
    return true;
}
UWidget* URokHospitalExplanationAuditSubsystem::Find(FName Name) const
{
    if (!Feature.IsValid()) return nullptr;
    if (auto* W = Feature->GetWidgetFromName(Name)) return W;
    const auto* Child = Cast<UUserWidget>(Feature->GetWidgetFromName(TEXT("HospitalExplanationWidget")));
    return Child ? Child->GetWidgetFromName(Name) : nullptr;
}
UScrollBox* URokHospitalExplanationAuditSubsystem::Scroll() const { return Cast<UScrollBox>(Find(TEXT("HospitalExplanationScroll"))); }
bool URokHospitalExplanationAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    auto* B = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
    if (!B && Owner == Feature.Get()) B = Cast<UButton>(Find(Name));
    if (!Expect(B && Visible(B) && B->GetIsEnabled(), TEXT("Required navigation button unavailable"))) return false;
    auto A = MakeShared<FJsonObject>(); A->SetStringField(TEXT("button"), Name.ToString()); A->SetNumberField(TEXT("frame"), GFrameCounter); A->SetNumberField(TEXT("step"), Step); A->SetNumberField(TEXT("elapsed"), Elapsed); Actions.Add(MakeShared<FJsonValueObject>(A));
    B->OnClicked.Broadcast(); return true; // Feature consumes its pending action on the next NativeTick.
}
bool URokHospitalExplanationAuditSubsystem::Validate()
{
    if (!Identity() || !Unchanged()) return false;
    if (Page == EPage::City) return Expect(!Feature->IsScreenOpen() && !Find(TEXT("HospitalExplanationWidget")) && !Find(TEXT("HospitalCapacityValue")), TEXT("Hospital retained presentation after close completed"));
    if (!Expect(Feature->IsScreenOpen(), TEXT("Hospital closed unexpectedly"))) return false;
#if !UE_BUILD_SHIPPING
    if (!Expect(Feature->GetFeatureContextActorForAudit() == Hospital.Get(), TEXT("Hospital lost actual actor context"))) return false;
#endif
    for (int32 I = 0; I < 4; ++I)
    {
        const auto* T = Cast<UTextBlock>(Find(*FString::Printf(TEXT("HospitalTroopValue_%d"), I)));
        if (!Expect(T && T->GetText().ToString() == TEXT("--"), TEXT("Wounded count was fabricated"))) return false;
    }
    const auto* Capacity = Cast<UTextBlock>(Find(TEXT("HospitalCapacityValue")));
    if (!Expect(Capacity && Capacity->GetText().ToString().Contains(TEXT("--")), TEXT("Capacity must retain unknown wounded count"))) return false;
    const bool Explanation = Page == EPage::Explanation;
    if (!Expect(Visible(Find(TEXT("HospitalExplanationWidget"))) == Explanation, TEXT("Wrong hospital page visible"))) return false;
    if (!Explanation)
    {
        for (const auto& Pair : MainTexts)
        {
            const auto* T = Cast<UTextBlock>(Find(Pair.Key));
            if (!Expect(T && Visible(T) && T->GetText().ToString() == Pair.Value, TEXT("Main page unknown text changed on return"))) return false;
        }
        for (const auto& Pair : MainOptions)
        {
            const auto* B = Cast<UButton>(Find(Pair.Key));
            if (!Expect(B && Visible(B) && B->GetIsEnabled() == Pair.Value, TEXT("Treatment option changed on return"))) return false;
        }
        return true;
    }
    for (const auto& Pair : MainOptions)
        if (!Expect(!Visible(Find(Pair.Key)), TEXT("Treatment controls overlap explanation"))) return false;
    if (!Expect(!Visible(Capacity) && Visible(Find(TEXT("HospitalExplanationBackButton"))), TEXT("Explanation source count/back visibility differs"))) return false;
    const auto* T = Cast<URichTextBlock>(Find(TEXT("HospitalExplanationText")));
    auto* S = Scroll();
    if (!Expect(T && S && Visible(T) && T->GetText().ToString() == ExpectedRichText && S->GetCachedGeometry().GetLocalSize().Y > 1, TEXT("Source rich text or actual scroll geometry missing"))) return false;
    const auto* Styles = T->GetTextStyleSet();
    const auto* Yellow = Styles ? Styles->FindRow<FRichTextStyleRow>(TEXT("yellow"), TEXT("HospitalAudit"), false) : nullptr;
    if (!Expect(Yellow && Yellow->TextStyle.ColorAndOpacity.GetSpecifiedColor().Equals(FLinearColor(1, 1, 0, 1), .0001f) && FMath::IsNearlyEqual(Yellow->TextStyle.Font.Size, 15.f, .001f), TEXT("Source yellow rich-text style/font differs"))) return false;
    if (bExpectBottom)
        return Expect(OriginalScroll.Get() == S && BottomOffset > 0 && FMath::IsNearlyEqual(S->GetScrollOffset(), BottomOffset, .1f) && FMath::IsNearlyEqual(S->GetScrollOffset(), S->GetScrollOffsetOfEnd(), .1f), TEXT("Explanation did not retain real bottom scroll offset"));
    return Expect(FMath::IsNearlyZero(S->GetScrollOffset(), .1f), TEXT("Fresh explanation must start at top"));
}
TSharedRef<FJsonObject> URokHospitalExplanationAuditSubsystem::State() const
{
    auto S = MakeShared<FJsonObject>(); S->SetNumberField(TEXT("step"), Step); S->SetNumberField(TEXT("frame"), GFrameCounter); S->SetNumberField(TEXT("elapsed"), Elapsed);
    S->SetStringField(TEXT("page"), Page == EPage::City ? TEXT("city") : Page == EPage::Main ? TEXT("hospital") : TEXT("explanation"));
    S->SetBoolField(TEXT("explanation_attached"), Find(TEXT("HospitalExplanationWidget")) != nullptr);
    S->SetBoolField(TEXT("capacity_attached"), Find(TEXT("HospitalCapacityValue")) != nullptr);
    S->SetBoolField(TEXT("feature_open"), Feature.IsValid() && Feature->IsScreenOpen()); S->SetBoolField(TEXT("explanation_visible"), Visible(Find(TEXT("HospitalExplanationWidget"))));
    if (const auto* Box = Scroll()) { S->SetNumberField(TEXT("scroll_offset"), Box->GetScrollOffset()); S->SetNumberField(TEXT("scroll_end"), Box->GetScrollOffsetOfEnd()); S->SetNumberField(TEXT("scroll_id"), Box->GetUniqueID()); }
    if (const auto* T = Cast<URichTextBlock>(Find(TEXT("HospitalExplanationText")))) S->SetStringField(TEXT("rich_text"), T->GetText().ToString());
    if (const auto* T = Cast<URichTextBlock>(Find(TEXT("HospitalExplanationText"))))
    {
        const auto* Styles = T->GetTextStyleSet(); const auto* Y = Styles ? Styles->FindRow<FRichTextStyleRow>(TEXT("yellow"), TEXT("HospitalAudit"), false) : nullptr;
        S->SetBoolField(TEXT("yellow_style_exact"), Y && Y->TextStyle.ColorAndOpacity.GetSpecifiedColor().Equals(FLinearColor(1, 1, 0, 1), .0001f));
        S->SetNumberField(TEXT("text_local_height"), T->GetCachedGeometry().GetLocalSize().Y);
    }
    S->SetBoolField(TEXT("read_only_state_unchanged"), true); return S;
}
void URokHospitalExplanationAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Validate()) return;
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Conflicting screenshot request")); return; }
    PendingPath = Directory / (FString(Name) + TEXT(".png")); Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("file"), PendingPath); Pending->SetObjectField(TEXT("request"), State());
    ShotStarted = Elapsed; FScreenshotRequest::RequestScreenshot(PendingPath, true, false); // Include Slate UI; scene-only captures cannot verify this slice.
}
void URokHospitalExplanationAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || !Validate()) return;
    const int64 Size = IFileManager::Get().FileSize(*PendingPath);
    if (Size <= 0) { Finish(TEXT("Screenshot callback missing output PNG")); return; }
    Pending->SetObjectField(TEXT("processed"), State()); Pending->SetNumberField(TEXT("file_bytes"), Size); Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset(); Save(false); Enter(Step + 1);
}
void URokHospitalExplanationAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return; Elapsed += DeltaTime;
    if (Elapsed > 80) { Finish(TEXT("Hospital audit timeout")); return; }
    if (Pending.IsValid()) { if (Elapsed - ShotStarted > 10) Finish(TEXT("Screenshot callback timeout")); return; }
    if (Elapsed < 3 || Elapsed - StepStarted < .5) return;
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"))) { Page = EPage::Main; Enter(2); } break;
    case 2:
        for (const auto* Name : {TEXT("HospitalWoundedState"), TEXT("HospitalTreatmentCost"), TEXT("HospitalCapacityValue")})
        { const auto* T = Cast<UTextBlock>(Find(Name)); if (!Expect(T != nullptr, TEXT("Missing hospital boundary text"))) return; MainTexts.Add(Name, T->GetText().ToString()); }
        for (const auto* Name : {TEXT("HospitalMaxButton"), TEXT("HospitalHealButton"), TEXT("HospitalHealNowButton")})
        { const auto* B = Cast<UButton>(Find(Name)); if (!Expect(B != nullptr, TEXT("Missing treatment option"))) return; MainOptions.Add(Name, B->GetIsEnabled()); }
        Capture(TEXT("01_hospital_unknown")); break;
    case 3: if (Click(Feature.Get(), TEXT("HospitalCapacityInfo"))) { Page = EPage::Explanation; Enter(4); } break;
    case 4: OriginalScroll = Scroll(); Capture(TEXT("02_explanation_top")); break;
    case 5: if (Validate()) { Scroll()->ScrollToEnd(); Enter(6); } break;
    case 6: BottomOffset = Scroll() ? Scroll()->GetScrollOffset() : 0; bExpectBottom = true; Capture(TEXT("03_explanation_bottom")); break;
    case 7: if (Click(Feature.Get(), TEXT("HospitalExplanationBackButton"))) { Page = EPage::Main; Enter(8); } break;
    case 8: Capture(TEXT("04_hospital_return_unknown")); break;
    case 9: if (Click(Feature.Get(), TEXT("HospitalCapacityInfo"))) { Page = EPage::Explanation; Enter(10); } break;
    case 10: Capture(TEXT("05_explanation_retained_bottom")); break;
    case 11: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) { Page = EPage::City; Enter(12); } break;
    case 12: Capture(TEXT("06_city_closed")); break;
    case 13: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"))) { Page = EPage::Main; Enter(14); } break;
    case 14: if (Validate() && Click(Feature.Get(), TEXT("HospitalCapacityInfo"))) { Page = EPage::Explanation; bExpectBottom = false; Enter(15); } break;
    case 15: Capture(TEXT("07_reopened_explanation_top")); break;
    case 16: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) { Page = EPage::City; Enter(17); } break;
    case 17: Capture(TEXT("08_city_final")); break;
    case 18: Finish(); break;
    default: Finish(TEXT("Invalid hospital audit step")); break;
    }
}
bool URokHospitalExplanationAuditSubsystem::Save(bool Complete)
{
    auto R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("complete"), Complete); R->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == CaptureCount);
    R->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())); R->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName()); R->SetNumberField(TEXT("expected_capture_count"), CaptureCount);
    R->SetStringField(TEXT("scope"), TEXT("Existing Init4/type14 selected through HUD API, real UMG delegates and ScrollToEnd across real ticks; screenshot callback revalidates each state. No physical pointer test, treatment request or authoritative wounded fixture."));
    R->SetBoolField(TEXT("capture_includes_slate_ui"), true);
    R->SetNumberField(TEXT("source_clauses"), 7); R->SetNumberField(TEXT("yellow_spans"), YellowSegments.Num()); R->SetNumberField(TEXT("bottom_offset"), BottomOffset);
    R->SetBoolField(TEXT("unity_visual_comparison_complete"), false); R->SetArrayField(TEXT("captures"), Captures); R->SetArrayField(TEXT("actions"), Actions);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& E : Failures) Errors.Add(MakeShared<FJsonValueString>(E)); R->SetArrayField(TEXT("failures"), Errors);
    FString Json; return FJsonSerializer::Serialize(R, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokHospitalExplanationAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return; if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num() != CaptureCount) Failures.Add(TEXT("Expected eight processed screenshots"));
    bFinished = true; const bool Written = Save(Failures.IsEmpty());
    for (const auto& E : Failures) UE_LOG(LogTemp, Error, TEXT("Rok HospitalExplanationAudit: %s"), *E);
    UE_LOG(LogTemp, Display, TEXT("Rok HospitalExplanationAudit passed=%d captures=%d directory=%s"), Failures.IsEmpty() && Written, Captures.Num(), *Directory);
    FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() && Written ? 0 : 1);
}
