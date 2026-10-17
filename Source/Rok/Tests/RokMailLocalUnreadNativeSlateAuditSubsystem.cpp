#include "RokMailLocalUnreadNativeSlateAuditSubsystem.h"

#include "../RokMainHudWidget.h"
#include "../RokServerSubsystem.h"
#include "../RokTrainArmyMeshData.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUiWindowAnimation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "Null/NullPlatformApplicationMisc.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace RokMailNativeAudit
{
constexpr const TCHAR* CityMap = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr const TCHAR* AuditFlag = TEXT("-RokMailLocalUnreadNativeSlateAuditV4");
constexpr float SettledTolerance = 0.001f;
bool IsSettledMailCanvas(const UWidget* Canvas)
{
    if (!Canvas || !Canvas->IsVisible()) return false;
    const FVector2D Scale = Canvas->GetRenderTransform().Scale;
    return FMath::IsNearlyEqual(Scale.X, 1.0f, SettledTolerance)
        && FMath::IsNearlyEqual(Scale.Y, 1.0f, SettledTolerance)
        && FMath::IsNearlyEqual(Canvas->GetRenderOpacity(), 1.0f, SettledTolerance);
}
struct FPin { const TCHAR* Path; const TCHAR* Sha; };
static const FPin SourcePins[] = {
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("e5e395a0801e77b13455c74b01976024d070d3f11b458be2ceec6d8da08111e3")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("a421b7c542f3f40bca8883960e0bea17c64b2fb956da46a1f70ad151f2923019")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("5b896308b0327f5e04c8f6434625a15703145f399893cd1306404b408724ae83")},
    {TEXT("Source/Rok/RokMailLocalUnreadState.h"), TEXT("c88b7fda3e65ba226361a6bd3fe84cc27d1b9b8dfb8ac3e92047e246e605d5c3")},
    {TEXT("Source/Rok/Tests/RokMailLocalUnreadStateTests.cpp"), TEXT("f7a028fb3c6a5d9c36862450d771477cb848deb79f2283c30d8b67ef251db3ab")}
};
FString Foreground()
{
#if PLATFORM_WINDOWS
    return FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
#else
    return TEXT("unsupported_platform");
#endif
}
int32 PngDimension(const TArray<uint8>& Bytes, int32 Offset)
{
    return (static_cast<int32>(Bytes[Offset]) << 24) | (static_cast<int32>(Bytes[Offset + 1]) << 16)
        | (static_cast<int32>(Bytes[Offset + 2]) << 8) | static_cast<int32>(Bytes[Offset + 3]);
}
FString JsonText(const TSharedRef<FJsonObject>& Object)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
    return Text;
}
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == RokMailNativeAudit::CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokMailLocalUnreadNativeSlateAuditV4"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
        && FParse::Param(FCommandLine::Get(), TEXT("game"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game;
}

TStatId URokMailLocalUnreadNativeSlateAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokMailLocalUnreadNativeSlateAuditSubsystem, STATGROUP_Tickables);
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this, &URokMailLocalUnreadNativeSlateAuditSubsystem::ScreenshotProcessed);
    StartedAt = StepStartedAt = FPlatformTime::Seconds();
    StartedUtc = FDateTime::UtcNow().ToIso8601();
    ProcessId = static_cast<int32>(FPlatformProcess::GetCurrentProcessId());
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    VirtualUser.Reset();
    Super::Deinitialize();
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Identity() const
{
    const UWorld* World = GetWorld();
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    FPaths::NormalizeDirectoryName(Project);
    FPaths::NormalizeDirectoryName(Engine);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == RokMailNativeAudit::CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
        && FEngineVersion::Current().GetMajor() == 5 && FEngineVersion::Current().GetMinor() == 8
        && FApp::IsGame() && FSlateApplication::IsInitialized()
        && FSlateApplication::Get().IsRenderingOffScreen()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Expect(bool Condition, const TCHAR* Message)
{
    if (!Condition) Finish(Message);
    return Condition;
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Visible(const UWidget* Widget) const
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    return true;
}

UButton* URokMailLocalUnreadNativeSlateAuditSubsystem::HudButton(const TCHAR* Name) const
{
    return Hud.IsValid() ? Cast<UButton>(Hud->GetWidgetFromName(Name)) : nullptr;
}

UButton* URokMailLocalUnreadNativeSlateAuditSubsystem::MailButton(const TCHAR* Name) const
{
    return Screen.IsValid() ? Cast<UButton>(Screen->GetWidgetFromName(Name)) : nullptr;
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Begin()
{
    if (!Expect(Identity(), TEXT("Wrong Shadow project, UE5.8, city map, or Null offscreen mode"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token;
    int32 Audit = 0, Local = 0, Offscreen = 0, Game = 0, OutputCount = 0, NonceCount = 0, MailCount = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(RokMailNativeAudit::AuditFlag, ESearchCase::IgnoreCase)) ++Audit;
        else if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) ++Local;
        else if (Token.Equals(TEXT("-RenderOffScreen"), ESearchCase::IgnoreCase)) ++Offscreen;
        else if (Token.Equals(TEXT("-game"), ESearchCase::IgnoreCase)) ++Game;
        else if (Token.StartsWith(TEXT("-RokMailNativeAuditOutputRootV4="), ESearchCase::IgnoreCase)) ++OutputCount;
        else if (Token.StartsWith(TEXT("-RokMailNativeAuditNonceV4="), ESearchCase::IgnoreCase)) ++NonceCount;
        else if (Token.StartsWith(TEXT("-RokOpenModal="), ESearchCase::IgnoreCase)) ++MailCount;
        else if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
            return Expect(false, TEXT("Unexpected Rok fixture or audit flag"));
    }
    FString Modal, OutputRoot;
    if (!Expect(Audit == 1 && Local == 1 && Offscreen == 1 && Game == 1
        && OutputCount == 1 && NonceCount == 1 && MailCount == 1
        && FParse::Value(FCommandLine::Get(), TEXT("RokOpenModal="), Modal)
        && FParse::Value(FCommandLine::Get(), TEXT("RokMailNativeAuditOutputRootV4="), OutputRoot)
        && FParse::Value(FCommandLine::Get(), TEXT("RokMailNativeAuditNonceV4="), Nonce),
        TEXT("Audit/local-only/offscreen/game/output-root/nonce/mail flags must occur exactly once"))) return false;
    Modal = Modal.TrimStartAndEnd().ToLower();
    if (!Expect(Modal == TEXT("mail_local_unread") || Modal == TEXT("mail"),
        TEXT("Only explicit local unread and ordinary unbound mail modes are accepted"))) return false;
    Mode = Modal == TEXT("mail_local_unread") ? TEXT("local_fixture") : TEXT("ordinary_unbound");
    bool bNonceAsciiAlnum = true;
    for (int32 Index = 0; Index < Nonce.Len(); ++Index)
    {
        const TCHAR Ch = Nonce[Index];
        bNonceAsciiAlnum &= (Ch >= '0' && Ch <= '9')
            || (Ch >= 'A' && Ch <= 'Z')
            || (Ch >= 'a' && Ch <= 'z');
    }
    if (!Expect(Nonce.Len() >= 16 && Nonce.Len() <= 64 && bNonceAsciiAlnum,
        TEXT("Nonce must be 16-64 ASCII alphanumeric characters"))) return false;
    OutputRoot = FPaths::ConvertRelativePathToFull(OutputRoot);
    FPaths::NormalizeDirectoryName(OutputRoot);
    const FString Results = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingRokMailCategoryAutoSelectSliceV2/NativeAuditV4/Results"));
    if (!Expect(FPaths::CollapseRelativeDirectories(OutputRoot)
        && FPaths::IsSamePath(OutputRoot, Results)
        && IFileManager::Get().MakeDirectory(*Results, true),
        TEXT("Output root must be exact Shadow Saved V4 Results"))) return false;
    Directory = Results / FString::Printf(TEXT("%s_pid%d_%s"), *Mode, ProcessId, *Nonce);
#if PLATFORM_WINDOWS
    // Win32 CreateDirectory fails with ERROR_ALREADY_EXISTS; no shared or recycled run directory.
    if (!Expect(::CreateDirectoryW(*Directory, nullptr) != 0,
        TEXT("PID+nonce output directory must be newly created"))) return false;
#else
    return Expect(false, TEXT("Windows create-only output identity required"));
#endif
    Inputs = MakeShared<FJsonObject>();
    for (const RokMailNativeAudit::FPin& Pin : RokMailNativeAudit::SourcePins)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectDir() / Pin.Path))
            && RokTrainArmyMesh::Sha256(Bytes) == Pin.Sha,
            TEXT("Integrated Mail category auto-select source pin differs"))) return false;
        Inputs->SetStringField(Pin.Path, Pin.Sha);
    }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
    int32 HudCount = 0;
    for (UUserWidget* Widget : Widgets)
        if (Widget && Widget->GetWorld() == GetWorld() && Widget->IsInViewport())
        {
            ++HudCount;
            Hud = Cast<URokMainHudWidget>(Widget);
        }
    Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
    UGameInstance* GameInstance = GetWorld()->GetGameInstance();
    Server = GameInstance ? GameInstance->GetSubsystem<URokServerSubsystem>() : nullptr;
    if (!Expect(HudCount == 1 && Hud.IsValid() && Screen.IsValid()
        && Hud->GetOwningPlayer() == GetWorld()->GetFirstPlayerController()
        && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0
        && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
        TEXT("Real 1440x810 city HUD or local-only boundary missing"))) return false;
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
    if (!Expect(Window.IsValid() && Window->IsVisible() && Native.IsValid()
        && Native->GetOSWindowHandle() == nullptr && Native->IsRenderingOffScreen()
        && Window->GetViewportSize().Equals(FVector2D(1440, 810), .1f)
        && FNullPlatformApplicationMisc::IsUsingNullApplication(),
        TEXT("Expected own visible Null-native offscreen 1440x810 SWindow"))) return false;
#else
    return Expect(false, TEXT("Windows Null native offscreen window required"));
#endif
    VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(8);
    return Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
        TEXT("Dedicated virtual Slate user 8 unavailable"));
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Click(UButton* Button, const TCHAR* Label)
{
    if (!VirtualUser.IsValid() || !Button || !Visible(Button) || !Button->GetIsEnabled()) return false;
    const FGeometry Geometry = Button->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X <= 0 || Size.Y <= 0) return false;
    const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
    if (StableTarget.Get() != Button || !StableTopLeft.Equals(TopLeft, .25f)
        || !StableBottomRight.Equals(BottomRight, .25f))
    {
        StableTarget = Button;
        StableTopLeft = TopLeft;
        StableBottomRight = BottomRight;
        StableFrame = GFrameCounter;
        ClickStableFrames = 1;
        return false;
    }
    if (GFrameCounter <= StableFrame) return false;
    StableFrame = GFrameCounter;
    if (++ClickStableFrames < 2) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const int32 User = VirtualUser->GetUserIndex();
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
    const FString ForegroundBefore = RokMailNativeAudit::Foreground();
    const TSharedPtr<SWidget> FocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT CursorBefore = {}, CursorAfter = {};
    const bool CursorReadBefore = ::GetCursorPos(&CursorBefore) != 0;
#endif
    App.ProcessMouseMoveEvent(FPointerEvent(User, 0, Away, LastPointer, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    App.ProcessMouseMoveEvent(FPointerEvent(User, 0, Center, Away, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(
        Center, App.GetInteractiveTopLevelWindows(), false, User);
    const TSharedRef<SWidget> ButtonSlate = Button->TakeWidget();
    int32 ButtonIndex = INDEX_NONE;
    bool bUmgPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        const FString Type = Node->GetTypeAsString();
        bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
        if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
        PathTypes.Add(MakeShared<FJsonValueString>(Type));
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const bool bOwnWindow = Window.IsValid() && Path.IsValid()
        && &Path.GetWindow().Get() == Window.Get();
#if PLATFORM_WINDOWS
    const bool bNullNative = Native.IsValid() && Native->GetOSWindowHandle() == nullptr
        && Native->IsRenderingOffScreen() && Window->IsVisible()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
#else
    const bool bNullNative = false;
#endif
    const bool bExactButton = ButtonIndex != INDEX_NONE
        && Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("label"), Label);
    Action->SetStringField(TEXT("target"), Button->GetName());
    Action->SetStringField(TEXT("method"), TEXT("virtual_slate_move_down_up"));
    Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Action->SetNumberField(TEXT("virtual_user_index"), User);
    Action->SetNumberField(TEXT("virtual_user_slot"), VirtualUser->GetVirtualUserIndex());
    Action->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
    Action->SetBoolField(TEXT("own_window"), bOwnWindow);
    Action->SetBoolField(TEXT("null_native_offscreen"), bNullNative);
    Action->SetBoolField(TEXT("umg_path"), bUmgPath);
    Action->SetBoolField(TEXT("exact_button"), bExactButton);
    Action->SetArrayField(TEXT("path_types"), PathTypes);
    if (!Path.IsValid() || !bOwnWindow || !bNullNative || !bExactButton || !bUmgPath)
    {
        Actions.Add(MakeShared<FJsonValueObject>(Action));
        return Expect(false, TEXT("Exact target missing from own Null-native offscreen Slate hit path"));
    }
    TSet<FKey> Pressed;
    Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(User, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(User, 0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
#if PLATFORM_WINDOWS
    const bool CursorReadAfter = ::GetCursorPos(&CursorAfter) != 0;
    const bool bCursorUnchanged = CursorReadBefore && CursorReadAfter
        && CursorBefore.x == CursorAfter.x && CursorBefore.y == CursorAfter.y;
#else
    const bool bCursorUnchanged = false;
#endif
    Action->SetBoolField(TEXT("down_dispatch_return"), Down);
    Action->SetBoolField(TEXT("up_dispatch_return"), Up);
    Action->SetBoolField(TEXT("dispatch_return_proves_business_change"), false);
    Action->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorUnchanged);
    Action->SetBoolField(TEXT("user0_focus_unchanged"), FocusBefore == App.GetUserFocusedWidget(0));
    Action->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == RokMailNativeAudit::Foreground());
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    StableTarget.Reset();
    ClickStableFrames = 0;
    return Expect(Up && bCursorUnchanged && FocusBefore == App.GetUserFocusedWidget(0)
        && ForegroundBefore == RokMailNativeAudit::Foreground(),
        TEXT("Virtual Slate click failed or hardware cursor/focus/foreground changed"));
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Observe(const TCHAR* Phase, int32 Tab,
    int32 SelectedIndex, int32 SystemUnread, int32 ReportUnread, bool bSystemRead,
    bool bReportRead, bool bLocal, bool bDetail, bool bEmpty)
{
    if (!Screen.IsValid() || !Screen->IsScreenOpen()) return false;
    const UWidget* Detail = Screen->GetWidgetFromName(TEXT("MailLocalDetailTitle"));
    const UWidget* Empty = Screen->GetWidgetFromName(TEXT("MailEmptyState"));
    const UWidget* Notice = Screen->GetWidgetFromName(TEXT("MailLocalUnreadNotice"));
    const UWidget* Row = Screen->GetWidgetFromName(TEXT("MailRowHit_0"));
    const UWidget* ReportRow = Screen->GetWidgetFromName(TEXT("MailRowHit_2"));
    const UWidget* Badge = Screen->GetWidgetFromName(TEXT("MailLocalTabUnreadCount_0"));
    const bool bRowsCorrect = !bLocal ? !Visible(Row) && !Visible(ReportRow)
        : Tab == 0 ? Visible(Row) && !Visible(ReportRow)
        : Tab == 1 ? !Visible(Row) && Visible(ReportRow)
        : !Visible(Row) && !Visible(ReportRow);
    const bool bSelectionCorrect = !bLocal || Screen->GetSelectedMailIndexForAudit() == SelectedIndex;
    const bool bGood = Screen->GetSelectedMailTabForAudit() == Tab
        && Screen->IsMailLocalUnreadFixtureForAudit() == bLocal
        && Screen->GetMailLocalUnreadCountForAudit(0) == SystemUnread
        && Screen->IsMailLocalFixtureRowReadForAudit(0) == bSystemRead
        && (!bLocal || Screen->IsMailLocalFixtureRowReadForAudit(2) == bReportRead)
        && Visible(Detail) == bDetail && Visible(Empty) == bEmpty
        && Visible(Notice) == bLocal
        && Visible(Badge) == (bLocal && SystemUnread > 0)
        && (!bLocal || Screen->GetMailLocalUnreadCountForAudit(1) == ReportUnread)
        && bRowsCorrect && bSelectionCorrect;
    if (!bGood) return false;
    auto State = MakeShared<FJsonObject>();
    State->SetStringField(TEXT("phase"), Phase);
    State->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    State->SetNumberField(TEXT("tab"), Tab);
    State->SetNumberField(TEXT("system_unread"), SystemUnread);
    State->SetNumberField(TEXT("report_unread"), Screen->GetMailLocalUnreadCountForAudit(1));
    State->SetBoolField(TEXT("first_system_row_read"), bSystemRead);
    State->SetBoolField(TEXT("first_report_row_read"), Screen->IsMailLocalFixtureRowReadForAudit(2));
    State->SetBoolField(TEXT("local_fixture"), bLocal);
    State->SetBoolField(TEXT("detail_visible"), Visible(Detail));
    State->SetBoolField(TEXT("empty_visible"), Visible(Empty));
    State->SetBoolField(TEXT("fixture_notice_visible"), Visible(Notice));
    State->SetBoolField(TEXT("system_badge_visible"), Visible(Badge));
    State->SetBoolField(TEXT("rows_correct_for_tab"), bRowsCorrect);
    State->SetNumberField(TEXT("selected_mail_index"), Screen->GetSelectedMailIndexForAudit());
    State->SetStringField(TEXT("hud_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Hud.Get())));
    State->SetStringField(TEXT("screen_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Screen.Get())));
    if (!ObservedPhases.Contains(Phase))
    {
        ObservedPhases.Add(Phase);
        Observations.Add(MakeShared<FJsonValueObject>(State));
    }
    return true;
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Capture(const TCHAR* Phase, int32 Next)
{
    if (!Expect(!PendingCapture.IsValid() && !FScreenshotRequest::IsScreenshotRequested(),
        TEXT("Screenshot request already pending"))) return;
    const bool bOrdinary = Mode == TEXT("ordinary_unbound");
    const UWidget* Canvas = bOrdinary && Screen.IsValid()
        ? Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")) : nullptr;
    const float OpenDuration = FRokUiWindowAnimation::GetDuration(
        ERokUiWindowAnimationStyle::Type1, false);
    const double ElapsedSinceReopen = FPlatformTime::Seconds() - StepStartedAt;
    if (bOrdinary && !Expect(FCString::Strcmp(Phase, TEXT("01_ordinary_unbound")) == 0
        && Actions.Num() == 2 && StableFrames >= 2
        && GFrameCounter >= static_cast<uint64>(Actions[1]->AsObject()->GetIntegerField(TEXT("frame"))) + 2
        && ElapsedSinceReopen >= OpenDuration
        && RokMailNativeAudit::IsSettledMailCanvas(Canvas),
        TEXT("Ordinary Mail capture requested before the real Type1 window settled"))) return;
    PendingCapture = MakeShared<FJsonObject>();
    PendingCapture->SetStringField(TEXT("phase"), Phase);
    PendingCapture->SetStringField(TEXT("requested_utc"), FDateTime::UtcNow().ToIso8601());
    PendingCapture->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
    PendingCapture->SetStringField(TEXT("foreground"), RokMailNativeAudit::Foreground());
    PendingCapture->SetNumberField(TEXT("tab"), Screen->GetSelectedMailTabForAudit());
    PendingCapture->SetNumberField(TEXT("selected_mail_index"), Screen->GetSelectedMailIndexForAudit());
    PendingCapture->SetNumberField(TEXT("system_unread"), Screen->GetMailLocalUnreadCountForAudit(0));
    PendingCapture->SetNumberField(TEXT("report_unread"), Screen->GetMailLocalUnreadCountForAudit(1));
    PendingCapture->SetBoolField(TEXT("row_read"), Screen->IsMailLocalFixtureRowReadForAudit(0));
    PendingCapture->SetBoolField(TEXT("report_row_read"), Screen->IsMailLocalFixtureRowReadForAudit(2));
    if (bOrdinary)
    {
        const FVector2D Scale = Canvas->GetRenderTransform().Scale;
        PendingCapture->SetStringField(TEXT("window_canvas_name"), TEXT("FeatureModeCanvas"));
        PendingCapture->SetBoolField(TEXT("window_settled_requested"), true);
        PendingCapture->SetNumberField(TEXT("window_reopen_action_frame"),
            Actions[1]->AsObject()->GetIntegerField(TEXT("frame")));
        PendingCapture->SetNumberField(TEXT("window_stable_frames"), StableFrames);
        PendingCapture->SetNumberField(TEXT("window_open_duration_seconds"), OpenDuration);
        PendingCapture->SetNumberField(TEXT("window_elapsed_since_reopen_seconds"), ElapsedSinceReopen);
        PendingCapture->SetNumberField(TEXT("window_scale_x"), Scale.X);
        PendingCapture->SetNumberField(TEXT("window_scale_y"), Scale.Y);
        PendingCapture->SetNumberField(TEXT("window_opacity"), Canvas->GetRenderOpacity());
    }
    PendingPng = Directory / FString::Printf(TEXT("%s.png"), Phase);
    PendingCapture->SetStringField(TEXT("png"), PendingPng);
    NextStep = Next;
    bScreenshotDone = false;
    FScreenshotRequest::RequestScreenshot(PendingPng, true, false);
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !PendingCapture.IsValid() || bScreenshotDone) return;
    TArray<uint8> Bytes;
    if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPng) && Bytes.Num() > 24
        && FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
        && FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
        && RokMailNativeAudit::PngDimension(Bytes, 16) == 1440
        && RokMailNativeAudit::PngDimension(Bytes, 20) == 810,
        TEXT("Processed screenshot missing or not 1440x810 PNG"))) return;
    if (!Expect(Screen.IsValid() && Screen->IsScreenOpen()
        && Screen->GetSelectedMailTabForAudit() == PendingCapture->GetIntegerField(TEXT("tab"))
        && Screen->GetSelectedMailIndexForAudit() == PendingCapture->GetIntegerField(TEXT("selected_mail_index"))
        && Screen->GetMailLocalUnreadCountForAudit(0) == PendingCapture->GetIntegerField(TEXT("system_unread"))
        && Screen->GetMailLocalUnreadCountForAudit(1) == PendingCapture->GetIntegerField(TEXT("report_unread"))
        && Screen->IsMailLocalFixtureRowReadForAudit(0) == PendingCapture->GetBoolField(TEXT("row_read"))
        && Screen->IsMailLocalFixtureRowReadForAudit(2) == PendingCapture->GetBoolField(TEXT("report_row_read"))
        && RokMailNativeAudit::Foreground() == PendingCapture->GetStringField(TEXT("foreground")),
        TEXT("Mail state or foreground changed during screenshot"))) return;
    if (Mode == TEXT("ordinary_unbound"))
    {
        const UWidget* Canvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
        const double ElapsedSinceReopen = FPlatformTime::Seconds() - StepStartedAt;
        if (!Expect(RokMailNativeAudit::IsSettledMailCanvas(Canvas)
            && ElapsedSinceReopen >= FRokUiWindowAnimation::GetDuration(
                ERokUiWindowAnimationStyle::Type1, false),
            TEXT("Ordinary Mail window changed before screenshot processing"))) return;
        const FVector2D Scale = Canvas->GetRenderTransform().Scale;
        PendingCapture->SetBoolField(TEXT("window_settled_processed"), true);
        PendingCapture->SetNumberField(TEXT("window_processed_elapsed_since_reopen_seconds"),
            ElapsedSinceReopen);
        PendingCapture->SetNumberField(TEXT("window_processed_scale_x"), Scale.X);
        PendingCapture->SetNumberField(TEXT("window_processed_scale_y"), Scale.Y);
        PendingCapture->SetNumberField(TEXT("window_processed_opacity"), Canvas->GetRenderOpacity());
    }
    PendingCapture->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
    PendingCapture->SetStringField(TEXT("processed_utc"), FDateTime::UtcNow().ToIso8601());
    PendingCapture->SetNumberField(TEXT("png_bytes"), Bytes.Num());
    PendingCapture->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    ScreenshotFrame = GFrameCounter;
    bScreenshotDone = true;
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Enter(int32 Next)
{
    Step = Next;
    StepStartedAt = FPlatformTime::Seconds();
    StableFrames = ClickStableFrames = 0;
    StableTarget.Reset();
}

bool URokMailLocalUnreadNativeSlateAuditSubsystem::Save(bool bPassed) const
{
    if (Directory.IsEmpty()) return false;
    auto Manifest = MakeShared<FJsonObject>();
    Manifest->SetStringField(TEXT("schema"), TEXT("rok.mail.local_unread_native_slate_audit.v4"));
    Manifest->SetStringField(TEXT("mode"), Mode);
    Manifest->SetNumberField(TEXT("pid"), ProcessId);
    Manifest->SetStringField(TEXT("nonce"), Nonce);
    Manifest->SetStringField(TEXT("output_directory"), Directory);
    Manifest->SetStringField(TEXT("process_started_utc"), StartedUtc);
    Manifest->SetStringField(TEXT("process_finished_utc"), FDateTime::UtcNow().ToIso8601());
    Manifest->SetBoolField(TEXT("passed"), bPassed);
    Manifest->SetBoolField(TEXT("runtime_accepted"), bPassed);
    Manifest->SetBoolField(TEXT("real_city_hud"), Hud.IsValid() && Hud->IsInViewport());
    Manifest->SetBoolField(TEXT("same_widget_reopen"), bSameWidgetReopen);
    Manifest->SetBoolField(TEXT("null_native_offscreen_required"), true);
    Manifest->SetBoolField(TEXT("os_window_hit_audit"), false);
    Manifest->SetBoolField(TEXT("network_account_reward_mutation_tested"), false);
    Manifest->SetBoolField(TEXT("no_http_dispatch"), Server.IsValid() && Server->GetHttpDispatchAttemptCount() == 0);
    Manifest->SetBoolField(TEXT("no_suppressed_request"), Server.IsValid() && Server->GetSuppressedLocalRequestCount() == 0);
    Manifest->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Manifest->SetStringField(TEXT("engine"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Manifest->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Manifest->SetNumberField(TEXT("virtual_user_slot"), VirtualUser.IsValid() ? VirtualUser->GetVirtualUserIndex() : INDEX_NONE);
    Manifest->SetNumberField(TEXT("final_step"), Step);
    Manifest->SetArrayField(TEXT("actions"), Actions);
    Manifest->SetArrayField(TEXT("observations"), Observations);
    Manifest->SetArrayField(TEXT("captures"), Captures);
    Manifest->SetObjectField(TEXT("input_sha256"), Inputs.IsValid()
        ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Manifest->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(RokMailNativeAudit::JsonText(Manifest),
        *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    const bool bLocal = Mode == TEXT("local_fixture");
    bool Passed = Failures.IsEmpty() && Identity() && Server.IsValid()
        && Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0 && bSameWidgetReopen
        && Actions.Num() == (bLocal ? 6 : 2)
        && Observations.Num() == (bLocal ? 6 : 2)
        && Captures.Num() == (bLocal ? 4 : 1);
    if (!Passed && Failures.IsEmpty()) Failures.Add(TEXT("Mail native Slate audit incomplete"));
    if (!Save(Passed))
    {
        Passed = false;
        UE_LOG(LogTemp, Error, TEXT("Mail native Slate audit manifest write failed: %s"), *Directory);
    }
    UE_LOG(LogTemp, Display, TEXT("Mail native Slate audit %s: %s"), Passed ? TEXT("passed") : TEXT("failed"), *Directory);
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
}

void URokMailLocalUnreadNativeSlateAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return;
    if (!Identity()) { Finish(TEXT("Audit left exact Shadow GameAppCity world")); return; }
    const double Now = FPlatformTime::Seconds();
    if (Now - StartedAt > 180) { Finish(TEXT("Mail audit total timeout")); return; }
    if (Now - StartedAt < 2) return;
    if (PendingCapture.IsValid())
    {
        if (Now - StepStartedAt > 20) { Finish(TEXT("Screenshot callback timeout")); return; }
        if (bScreenshotDone && GFrameCounter > ScreenshotFrame)
        {
            Captures.Add(MakeShared<FJsonValueObject>(PendingCapture));
            PendingCapture.Reset();
            Enter(NextStep);
        }
        return;
    }
    if (Now - StepStartedAt > 20) { Finish(TEXT("Mail audit step timeout")); return; }
    if (Step == 0) { if (Begin()) Enter(1); return; }
    if (!Hud.IsValid() || !Screen.IsValid() || !Hud->IsInViewport())
    { Finish(TEXT("Real HUD or mail widget disappeared")); return; }
    const bool bLocal = Mode == TEXT("local_fixture");
    switch (Step)
    {
    case 1:
        if (bLocal && Screen->IsScreenOpen()
            && Observe(TEXT("initial_auto_selected"), /*Tab=*/0, /*SelectedIndex=*/0,
                /*SystemUnread=*/0, /*ReportUnread=*/1, true, false, true, true, false))
            Capture(TEXT("01_auto_selected"), 2);
        else if (!bLocal && Screen->IsScreenOpen()
            && Observe(TEXT("ordinary_initial"), 0, INDEX_NONE, 0, 0, false, false, false, false, true))
            Enter(20);
        break;
    case 2: if (Click(MailButton(TEXT("MailRowHit_1")), TEXT("select_second_system_row"))) Enter(3); break;
    case 3:
        // Row 1002 was seeded as read. Clicking it changes selection to index 1,
        // while the first row remains read and System unread stays zero.
        if (Observe(TEXT("second_row"), /*Tab=*/0, /*SelectedIndex=*/1,
            /*SystemUnread=*/0, /*ReportUnread=*/1, true, false, true, true, false))
            Capture(TEXT("02_second_row"), 4);
        break;
    case 4: if (Click(MailButton(TEXT("MailTab_1")), TEXT("switch_report"))) Enter(5); break;
    case 5:
        if (Observe(TEXT("report_auto_selected"), /*Tab=*/1, /*SelectedIndex=*/2,
            /*SystemUnread=*/0, /*ReportUnread=*/0, true, true, true, true, false)
            && Click(MailButton(TEXT("MailTab_0")), TEXT("return_system"))) Enter(6);
        break;
    case 6:
        if (Observe(TEXT("system_return"), /*Tab=*/0, /*SelectedIndex=*/0,
            /*SystemUnread=*/0, /*ReportUnread=*/0, true, true, true, true, false)
            && Click(MailButton(TEXT("FeatureFrameClose")), TEXT("close_mail"))) Enter(7);
        break;
    case 7:
        if (!Screen->IsScreenOpen() && ++StableFrames >= 2
            && Click(HudButton(TEXT("MailFeatureHitButton")), TEXT("reopen_from_real_hud"))) Enter(8);
        break;
    case 8:
        if (Screen->IsScreenOpen() && Observe(TEXT("reopened"), /*Tab=*/0, /*SelectedIndex=*/0,
            /*SystemUnread=*/0, /*ReportUnread=*/0, true, true, true, true, false))
        {
            bSameWidgetReopen = true;
            Capture(TEXT("03_reopened"), 9);
        }
        break;
    case 9: if (Click(MailButton(TEXT("MailTab_4")), TEXT("empty_sent_tab"))) Enter(10); break;
    case 10:
        if (Observe(TEXT("empty_sent"), /*Tab=*/4, /*SelectedIndex=*/INDEX_NONE,
            /*SystemUnread=*/0, /*ReportUnread=*/0, true, true, true, false, true)
            && !Visible(Screen->GetWidgetFromName(TEXT("MailRowHit_0"))))
            Capture(TEXT("04_empty_sent"), 11);
        break;
    case 11: Finish(); break;
    case 20: if (Click(MailButton(TEXT("FeatureFrameClose")), TEXT("close_ordinary_mail"))) Enter(21); break;
    case 21:
        if (!Screen->IsScreenOpen() && ++StableFrames >= 2
            && Click(HudButton(TEXT("MailFeatureHitButton")), TEXT("reopen_ordinary_mail_from_real_hud")))
        {
            StableFrame = 0;
            Enter(22);
        }
        break;
    case 22:
    {
        const UWidget* Canvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
        const bool bSettled = Screen->IsScreenOpen()
            && FPlatformTime::Seconds() - StepStartedAt >= FRokUiWindowAnimation::GetDuration(
                ERokUiWindowAnimationStyle::Type1, false)
            && RokMailNativeAudit::IsSettledMailCanvas(Canvas);
        if (GFrameCounter != StableFrame)
        {
            StableFrame = GFrameCounter;
            StableFrames = bSettled ? StableFrames + 1 : 0;
        }
        if (bSettled && StableFrames >= 2
            && Observe(TEXT("ordinary_reopened"), 0, INDEX_NONE, 0, 0,
                false, false, false, false, true))
        {
            bSameWidgetReopen = true;
            Capture(TEXT("01_ordinary_unbound"), 23);
        }
        break;
    }
    case 23: Finish(); break;
    default: Finish(TEXT("Unexpected Mail audit state")); break;
    }
}
