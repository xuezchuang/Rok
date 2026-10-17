#include "RokCommanderSyntheticSlateAuditSubsystem.h"

#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokServerSubsystem.h"
#include "../RokTrainArmyMeshData.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
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

namespace RokCommanderSyntheticAudit
{
constexpr const TCHAR* CityMap = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr const TCHAR* Flag = TEXT("-RokCommanderSyntheticSlateAuditV3");
struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
static const FInputPin Pinned[] = {
    {TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("8bcc9d18fe579fa938558fcf7bb845b13e85efeea38e6aa1f5d0460d94217eaa")},
    {TEXT("Content/Maps/L_RokUnityGameAppCity.umap"), TEXT("26609d28ca4f42f3c9d0d7cb735c88a71862d1df1fd00f362da6693111b0ddc2")},
    {TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"), TEXT("2633514629f9e39744226b2665f6f30e1c2d007e46a7d2ac019a45d72e498804")},
    {TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
    {TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
    {TEXT("Saved/RokUnityMigration/Commander/hero_2007_idle_mesh.json"), TEXT("3766b4b343ab75bc8903b28207d5957f1c0f55c10b7ccb3e7ce411c4c1d29a6b")},
    {TEXT("ref/client-unity/Assets/BundleAssets/Troop/spine_hero/Hero_2007_SD/Materials/Hero_2007.png"), TEXT("019d3d8fa6613a7eebfa2e86130598d4b9b93f13f316695b707b76cb1d02f3c8")},
};

FString FileSha(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}

FString Encode(const TSharedRef<FJsonObject>& Object)
{
    FString Text;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
    return Text;
}

bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0.0f) return false;
    for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0.0f) return false;
    return true;
}

FString ForegroundIdentity()
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

TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"), Value.X);
    Json->SetNumberField(TEXT("y"), Value.Y);
    return Json;
}

FString SortLabel(int32 SortType)
{
    switch (SortType)
    {
    case 1: return TEXT("星级 ▼");
    case 2: return TEXT("等级 ▼");
    case 3: return TEXT("战力 ▼");
    default: return TEXT("品质 ▼");
    }
}
}

bool URokCommanderSyntheticSlateAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return Cast<UWorld>(Outer)
        && FPaths::IsSamePath(Project,
            TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokCommanderSyntheticSlateAuditV3"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
        && FParse::Param(FCommandLine::Get(), TEXT("game"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokCommanderSyntheticSlateAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game;
}

TStatId URokCommanderSyntheticSlateAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokCommanderSyntheticSlateAuditSubsystem, STATGROUP_Tickables);
}

void URokCommanderSyntheticSlateAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this, &URokCommanderSyntheticSlateAuditSubsystem::ScreenshotProcessed);
    StartedAt = FPlatformTime::Seconds();
    StepStartedAt = StartedAt;
}

void URokCommanderSyntheticSlateAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    VirtualUser.Reset();
    Super::Deinitialize();
}

bool URokCommanderSyntheticSlateAuditSubsystem::Identity() const
{
    const UWorld* World = GetWorld();
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    FPaths::NormalizeDirectoryName(Project);
    FPaths::NormalizeDirectoryName(Engine);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == RokCommanderSyntheticAudit::CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
        && FEngineVersion::Current().GetMajor() == 5
        && FEngineVersion::Current().GetMinor() == 8
        && FApp::IsGame()
        && FSlateApplication::IsInitialized()
        && FSlateApplication::Get().IsRenderingOffScreen()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
}

bool URokCommanderSyntheticSlateAuditSubsystem::Expect(bool Condition, const TCHAR* Error)
{
    if (!Condition) Finish(Error);
    return Condition;
}

bool URokCommanderSyntheticSlateAuditSubsystem::Begin()
{
    if (!Expect(Identity() && FSlateApplication::IsInitialized(),
        TEXT("Wrong Shadow project/city world/engine or Slate unavailable"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token;
    int32 AuditFlags = 0, LocalFlags = 0, OutputFlags = 0;
    int32 OffscreenFlags = 0, GameFlags = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(RokCommanderSyntheticAudit::Flag, ESearchCase::IgnoreCase)) ++AuditFlags;
        else if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) ++LocalFlags;
        else if (Token.StartsWith(TEXT("-RokCommanderAuditOutputV3="), ESearchCase::IgnoreCase)) ++OutputFlags;
        else if (Token.Equals(TEXT("-RenderOffScreen"), ESearchCase::IgnoreCase)) ++OffscreenFlags;
        else if (Token.Equals(TEXT("-game"), ESearchCase::IgnoreCase)) ++GameFlags;
        else if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
            return Expect(false, TEXT("Unexpected Rok fixture/diagnostic flag"));
    }
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && OutputFlags == 1
        && OffscreenFlags == 1 && GameFlags == 1,
        TEXT("V3 audit/local-only/output/offscreen/game flags must occur once"))) return false;
    FString Output;
    if (!Expect(FParse::Value(FCommandLine::Get(), TEXT("RokCommanderAuditOutputV3="), Output),
        TEXT("Root-specified fresh output directory required"))) return false;
    Output = FPaths::ConvertRelativePathToFull(Output);
    FPaths::NormalizeDirectoryName(Output);
    if (!Expect(FPaths::CollapseRelativeDirectories(Output), TEXT("Output path normalization failed"))) return false;
    const FString ResultRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingCommanderSyntheticSlateAuditV3/Results"));
    if (!Expect(FPaths::IsSamePath(FPaths::GetPath(Output), ResultRoot)
        && FPaths::GetCleanFilename(Output).Len() >= 8
        && !IFileManager::Get().DirectoryExists(*Output)
        && !IFileManager::Get().FileExists(*Output),
        TEXT("Output must be a fresh direct child of Shadow Saved audit Results"))) return false;
    Directory = Output;
    if (!Expect(IFileManager::Get().MakeDirectory(*Directory, true),
        TEXT("Could not create fresh audit directory"))) return false;
    Inputs = MakeShared<FJsonObject>();
    for (const RokCommanderSyntheticAudit::FInputPin& Pin : RokCommanderSyntheticAudit::Pinned)
    {
        const FString Actual = RokCommanderSyntheticAudit::FileSha(FPaths::ProjectDir() / Pin.Path);
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned Run14 product/map/local-data input differs"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }

    int32 ControllerCount = 0;
    for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
    {
        ++ControllerCount;
        if (ARokPlayerController* Rok = Cast<ARokPlayerController>(*It)) Controller = Rok;
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
    UGameInstance* Game = GetWorld()->GetGameInstance();
    Catalog = Game ? Game->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
    Runtime = Game ? Game->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
    Server = Game ? Game->GetSubsystem<URokServerSubsystem>() : nullptr;
    if (!Expect(ControllerCount == 1 && Controller.IsValid()
        && Controller.Get() == GetWorld()->GetFirstPlayerController()
        && HudCount == 1 && Hud.IsValid() && Hud->GetOwningPlayer() == Controller.Get()
        && Screen.IsValid()
        && !Screen->IsScreenOpen() && Catalog.IsValid() && Catalog->IsCatalogLoaded()
        && Runtime.IsValid() && Runtime->IsRuntimeStateLoaded()
        && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0
        && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
        TEXT("Real idle 1440x810 city HUD/catalog/local-only boundary absent"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
        && Profile.RoleId.IsEmpty(), TEXT("Expected anonymous local-preview state"))) return false;
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
    if (!Expect(Window.IsValid() && Window->IsVisible() && Native.IsValid()
        && Native->GetOSWindowHandle() == nullptr && Native->IsRenderingOffScreen()
        && FNullPlatformApplicationMisc::IsUsingNullApplication(),
        TEXT("Expected visible own GameViewport SWindow backed by Null offscreen window"))) return false;
    const FVector2D WindowViewportSize = Window->GetViewportSize();
    if (!Expect(WindowViewportSize.Equals(FVector2D(1440, 810), .1f),
        TEXT("Expected own offscreen GameViewport SWindow 1440x810"))) return false;
#else
    return Expect(false, TEXT("Windows Null offscreen window identity probe required"));
#endif
    // The handle owns registration; releasing it unregisters the virtual Slate user.
    VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(1);
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
        TEXT("Dedicated non-hardware Slate user unavailable"))) return false;
    ForegroundAtBegin = RokCommanderSyntheticAudit::ForegroundIdentity();
    LastPointer = FVector2D::ZeroVector;
    return true;
}

bool URokCommanderSyntheticSlateAuditSubsystem::Ready(bool bOpen, int32 SortType) const
{
    if (!Identity() || !Hud.IsValid() || !Screen.IsValid()
        || !Hud->IsInViewport() || Screen->IsScreenOpen() != bOpen) return false;
    if (!bOpen) return true;
    const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel")));
    return Label && RokCommanderSyntheticAudit::Visible(Label)
        && Label->GetText().ToString() == RokCommanderSyntheticAudit::SortLabel(SortType)
        && RokCommanderSyntheticAudit::Visible(Screen->GetWidgetFromName(TEXT("CommanderSortAction")))
        && RokCommanderSyntheticAudit::Visible(Screen->GetWidgetFromName(TEXT("CommanderBack")))
        && Screen->GetWidgetFromName(TEXT("CommanderHeadAction_0")) != nullptr;
}

bool URokCommanderSyntheticSlateAuditSubsystem::Click(UButton* Button, const TCHAR* Label)
{
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
        TEXT("Dedicated virtual Slate user was lost"))) return false;
    if (!Button || !RokCommanderSyntheticAudit::Visible(Button) || !Button->GetIsEnabled()) return false;
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
    const int32 UserIndex = VirtualUser->GetUserIndex();
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
    const FString ForegroundBefore = RokCommanderSyntheticAudit::ForegroundIdentity();
    const TSharedPtr<SWidget> UserZeroFocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT HardwareCursorBefore = {};
    const bool bCursorReadBefore = ::GetCursorPos(&HardwareCursorBefore) != 0;
#endif
    const bool bScreenBefore = Screen.IsValid() && Screen->IsScreenOpen();
    const UTextBlock* LabelBefore = Screen.IsValid()
        ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel"))) : nullptr;
    const FString SortBefore = LabelBefore ? LabelBefore->GetText().ToString() : FString();
    App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Away, LastPointer, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Center, Away, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(
        Center, App.GetInteractiveTopLevelWindows(), false, UserIndex);
    const TSharedRef<SWidget> ButtonSlate = Button->TakeWidget();
    int32 ButtonIndex = INDEX_NONE;
    bool bUmgPath = false;
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        const FString Type = Node->GetTypeAsString();
        bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
        if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
        Nodes.Add(MakeShared<FJsonValueString>(Type));
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwnWindow = Window.IsValid() && Path.IsValid()
        && &Path.GetWindow().Get() == Window.Get();
    const bool bExactButton = ButtonIndex != INDEX_NONE
        && Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
    auto Attempt = MakeShared<FJsonObject>();
    Attempt->SetStringField(TEXT("label"), Label);
    Attempt->SetStringField(TEXT("target"), Button->GetName());
    Attempt->SetStringField(TEXT("target_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Button)));
    Attempt->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(&ButtonSlate.Get())));
    Attempt->SetStringField(TEXT("method"), TEXT("in_process_synthetic_slate_move_down_up"));
    Attempt->SetNumberField(TEXT("virtual_user_index"), UserIndex);
    Attempt->SetNumberField(TEXT("virtual_user_slot"), VirtualUser->GetVirtualUserIndex());
    Attempt->SetBoolField(TEXT("move_event_is_synthetic"), true);
    Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Attempt->SetObjectField(TEXT("center"), RokCommanderSyntheticAudit::Point(Center));
    Attempt->SetObjectField(TEXT("top_left"), RokCommanderSyntheticAudit::Point(TopLeft));
    Attempt->SetObjectField(TEXT("bottom_right"), RokCommanderSyntheticAudit::Point(BottomRight));
    Attempt->SetArrayField(TEXT("hit_path_types"), Nodes);
    Attempt->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
    Attempt->SetBoolField(TEXT("umg_path"), bUmgPath);
    Attempt->SetBoolField(TEXT("own_window"), bOwnWindow);
    Attempt->SetBoolField(TEXT("exact_button"), bExactButton);
    Attempt->SetBoolField(TEXT("hovered"), Button->IsHovered());
    Attempt->SetStringField(TEXT("foreground_before"), ForegroundBefore);
    Attempt->SetStringField(TEXT("user0_focus_before"), FString::Printf(TEXT("0x%p"),
        static_cast<void*>(UserZeroFocusBefore.Get())));
#if PLATFORM_WINDOWS
    Attempt->SetBoolField(TEXT("hardware_cursor_read_before"), bCursorReadBefore);
    if (bCursorReadBefore)
    {
        Attempt->SetNumberField(TEXT("hardware_cursor_x_before"), HardwareCursorBefore.x);
        Attempt->SetNumberField(TEXT("hardware_cursor_y_before"), HardwareCursorBefore.y);
    }
#endif
    Attempt->SetBoolField(TEXT("screen_open_before"), bScreenBefore);
    Attempt->SetStringField(TEXT("sort_label_before"), SortBefore);
    Attempt->SetStringField(TEXT("window_identity"), Window.IsValid()
        ? FString::Printf(TEXT("0x%p"), static_cast<void*>(Window.Get())) : FString());
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const bool bOwnedOffscreenWindow = Native.IsValid() && Native->GetOSWindowHandle() == nullptr
        && Native->IsRenderingOffScreen() && Window->IsVisible()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
    Attempt->SetBoolField(TEXT("null_platform_window"), bOwnedOffscreenWindow);
    Attempt->SetBoolField(TEXT("native_hwnd_absent"), Native.IsValid()
        && Native->GetOSWindowHandle() == nullptr);
#else
    const bool bOwnedOffscreenWindow = false;
#endif
    if (!Path.IsValid() || !bOwnWindow || !bOwnedOffscreenWindow || !bExactButton || !bUmgPath)
    {
        Attempt->SetStringField(TEXT("failure"), TEXT("Exact own offscreen SWindow/UButton missing from Slate hit path"));
        Actions.Add(MakeShared<FJsonValueObject>(Attempt));
        return Expect(false, TEXT("Synthetic Slate target hit proof failed"));
    }
    TSet<FKey> Pressed;
    Pressed.Add(EKeys::LeftMouseButton);
    const bool DownDispatch = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(UserIndex, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool UpDispatch = App.ProcessMouseButtonUpEvent(FPointerEvent(UserIndex, 0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const FString ForegroundAfter = RokCommanderSyntheticAudit::ForegroundIdentity();
    const TSharedPtr<SWidget> UserZeroFocusAfter = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT HardwareCursorAfter = {};
    const bool bCursorReadAfter = ::GetCursorPos(&HardwareCursorAfter) != 0;
    const bool bCursorUnchanged = bCursorReadBefore && bCursorReadAfter
        && HardwareCursorBefore.x == HardwareCursorAfter.x
        && HardwareCursorBefore.y == HardwareCursorAfter.y;
    Attempt->SetBoolField(TEXT("hardware_cursor_read_after"), bCursorReadAfter);
    if (bCursorReadAfter)
    {
        Attempt->SetNumberField(TEXT("hardware_cursor_x_after"), HardwareCursorAfter.x);
        Attempt->SetNumberField(TEXT("hardware_cursor_y_after"), HardwareCursorAfter.y);
    }
#else
    const bool bCursorUnchanged = false;
#endif
    Attempt->SetBoolField(TEXT("down_dispatch_return"), DownDispatch);
    Attempt->SetBoolField(TEXT("up_dispatch_return"), UpDispatch);
    Attempt->SetBoolField(TEXT("dispatch_return_proves_click"), false);
    Attempt->SetStringField(TEXT("user0_focus_after"), FString::Printf(TEXT("0x%p"),
        static_cast<void*>(UserZeroFocusAfter.Get())));
    Attempt->SetBoolField(TEXT("user0_focus_unchanged"), UserZeroFocusBefore == UserZeroFocusAfter);
    Attempt->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorUnchanged);
    Attempt->SetStringField(TEXT("foreground_after"), ForegroundAfter);
    Attempt->SetBoolField(TEXT("screen_open_after"), Screen.IsValid() && Screen->IsScreenOpen());
    const UTextBlock* LabelAfter = Screen.IsValid()
        ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel"))) : nullptr;
    Attempt->SetStringField(TEXT("sort_label_after"), LabelAfter ? LabelAfter->GetText().ToString() : FString());
    Attempt->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == ForegroundAfter);
    Attempt->SetBoolField(TEXT("foreground_is_own_window_required"), false);
    Attempt->SetBoolField(TEXT("win32_input_or_focus_api_called"), false);
    Actions.Add(MakeShared<FJsonValueObject>(Attempt));
    StableTarget.Reset();
    ClickStableFrames = 0;
    // UE's down-event bool is true even for an unhandled normal route. Later
    // Ready/ObserveSort/capture gates prove the actual product state changed.
    return Expect(UpDispatch && ForegroundBefore == ForegroundAfter
        && UserZeroFocusBefore == UserZeroFocusAfter && bCursorUnchanged,
        TEXT("Virtual Slate up unhandled or real cursor/focus/foreground changed"));
}

bool URokCommanderSyntheticSlateAuditSubsystem::ObserveSort(int32 SortType, const TCHAR* Phase)
{
    if (!Expect(Ready(true, SortType), TEXT("Commander sort screen/label not ready"))) return false;
    const TArray<FRokUnityRuntimeHeroState> RuntimeHeroes = Runtime->GetStarterHeroes();
    TSet<int32> Owned;
    for (const FRokUnityRuntimeHeroState& Hero : RuntimeHeroes) Owned.Add(Hero.HeroId);
    TArray<int32> Ids;
    for (int32 Id : Catalog->GetHeroIds())
    {
        FRokUnityCatalogHero Hero;
        if (Catalog->GetHero(Id, Hero) && Hero.ListDisplay != 1) Ids.Add(Id);
    }
    if (!Expect(Ids.Num() > 5, TEXT("Commander catalog candidate list unavailable"))) return false;
    const auto SortValue = [this, &RuntimeHeroes, SortType](int32 Id) -> int64
    {
        FRokUnityCatalogHero Hero;
        const bool bCatalogHero = Catalog->GetHero(Id, Hero);
        const FRokUnityRuntimeHeroState* RuntimeHero = RuntimeHeroes.FindByPredicate(
            [Id](const FRokUnityRuntimeHeroState& Item) { return Item.HeroId == Id; });
        switch (SortType)
        {
        case 1: return RuntimeHero ? RuntimeHero->Star : (bCatalogHero ? Hero.InitStar : 0);
        case 2: return RuntimeHero ? RuntimeHero->Level : 1;
        case 3: return RuntimeHero ? RuntimeHero->Score : (bCatalogHero ? Hero.BasePower : 0);
        default: return bCatalogHero ? Hero.Rare : 0;
        }
    };
    Ids.Sort([this, &Owned, &SortValue](int32 Left, int32 Right)
    {
        const bool bLeftOwned = Owned.Contains(Left), bRightOwned = Owned.Contains(Right);
        if (bLeftOwned != bRightOwned) return bLeftOwned;
        const int64 L = SortValue(Left), R = SortValue(Right);
        if (L != R) return L > R;
        FRokUnityCatalogHero A, B;
        if (Catalog->GetHero(Left, A) && Catalog->GetHero(Right, B) && A.Rare != B.Rare)
            return A.Rare > B.Rare;
        return Left < Right;
    });
    TArray<FString> ActualOrder;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Ids.Num(); ++Index)
    {
        FRokUnityCatalogHero Hero;
        if (!Expect(Catalog->GetHero(Ids[Index], Hero), TEXT("Commander catalog row vanished"))) return false;
        const FString Name = !Hero.DisplayNameZhCn.IsEmpty() ? Hero.DisplayNameZhCn : Hero.DisplayNameEn;
        const FString Expected = FString::Printf(TEXT("%s · %s"), *Name,
            Owned.Contains(Ids[Index]) ? TEXT("已拥有") : TEXT("未招募"));
        const UButton* Head = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("CommanderHeadAction_%d"), Index)));
        const FString Actual = Head ? Head->GetToolTipText().ToString() : FString();
        if (!Expect(Actual == Expected, TEXT("Commander roster tooltip order differs from catalog/runtime"))) return false;
        ActualOrder.Add(Actual);
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("index"), Index);
        Row->SetNumberField(TEXT("hero_id"), Ids[Index]);
        Row->SetStringField(TEXT("tooltip"), Actual);
        Row->SetBoolField(TEXT("owned"), Owned.Contains(Ids[Index]));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if (SortType == 0) QualityOrder = ActualOrder;
    if (SortType == 3 && !Expect(ActualOrder != QualityOrder,
        TEXT("Power sort did not change initial quality roster order"))) return false;
    if (SortType == 2 && FCString::Strcmp(Phase, TEXT("reopened_level")) != 0)
    {
        PersistedOrder = ActualOrder;
        PersistedLabel = RokCommanderSyntheticAudit::SortLabel(2);
    }
    if (FCString::Strcmp(Phase, TEXT("reopened_level")) == 0
        && !Expect(ActualOrder == PersistedOrder
            && Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel")))->GetText().ToString() == PersistedLabel,
            TEXT("Same-widget Commander sort label/order failed to persist after close/reopen"))) return false;
    auto Observation = MakeShared<FJsonObject>();
    Observation->SetStringField(TEXT("phase"), Phase);
    Observation->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Observation->SetNumberField(TEXT("sort_type_inferred_from_label"), SortType);
    Observation->SetStringField(TEXT("label"), RokCommanderSyntheticAudit::SortLabel(SortType));
    Observation->SetStringField(TEXT("hud_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Hud.Get())));
    Observation->SetStringField(TEXT("screen_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Screen.Get())));
    Observation->SetArrayField(TEXT("roster"), Rows);
    Observations.Add(MakeShared<FJsonValueObject>(Observation));
    return true;
}

void URokCommanderSyntheticSlateAuditSubsystem::Capture(const TCHAR* Phase, int32 Next)
{
    if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested(),
        TEXT("Screenshot request already pending"))) return;
    Pending = MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("phase"), Phase);
    Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
    Pending->SetStringField(TEXT("foreground_at_request"), RokCommanderSyntheticAudit::ForegroundIdentity());
    Pending->SetBoolField(TEXT("screen_open_at_request"), Screen.IsValid() && Screen->IsScreenOpen());
    const UTextBlock* Label = Screen.IsValid()
        ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel"))) : nullptr;
    Pending->SetStringField(TEXT("sort_label_at_request"), Label ? Label->GetText().ToString() : FString());
    PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
    Pending->SetStringField(TEXT("png"), PendingPath);
    NextStep = Next;
    bScreenshotDone = false;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokCommanderSyntheticSlateAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
    TArray<uint8> Bytes;
    if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
        && FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
        && FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
        && RokCommanderSyntheticAudit::PngDimension(Bytes, 16) == 1440
        && RokCommanderSyntheticAudit::PngDimension(Bytes, 20) == 810,
        TEXT("Processed screenshot PNG missing or not 1440x810"))) return;
    const UTextBlock* Label = Screen.IsValid()
        ? Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSortLabel"))) : nullptr;
    const FString CurrentLabel = Label ? Label->GetText().ToString() : FString();
    if (!Expect(Screen.IsValid() && Screen->IsScreenOpen() == Pending->GetBoolField(TEXT("screen_open_at_request"))
        && CurrentLabel == Pending->GetStringField(TEXT("sort_label_at_request"))
        && RokCommanderSyntheticAudit::ForegroundIdentity()
            == Pending->GetStringField(TEXT("foreground_at_request")),
        TEXT("Screen state or foreground changed during processed screenshot"))) return;
    Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
    Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
    Pending->SetNumberField(TEXT("png_width"), 1440);
    Pending->SetNumberField(TEXT("png_height"), 810);
    Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetStringField(TEXT("foreground_at_processed"), RokCommanderSyntheticAudit::ForegroundIdentity());
    Pending->SetStringField(TEXT("sort_label_at_processed"), CurrentLabel);
    ScreenshotFrame = GFrameCounter;
    bScreenshotDone = true;
}

void URokCommanderSyntheticSlateAuditSubsystem::AdvanceCapture()
{
    if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
    Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
    Captures.Add(MakeShared<FJsonValueObject>(Pending));
    Pending.Reset();
    Enter(NextStep);
}

void URokCommanderSyntheticSlateAuditSubsystem::Enter(int32 Next)
{
    Step = Next;
    StepStartedAt = FPlatformTime::Seconds();
    StableFrames = 0;
    ClickStableFrames = 0;
    StableTarget.Reset();
}

bool URokCommanderSyntheticSlateAuditSubsystem::Save(bool bComplete) const
{
    if (Directory.IsEmpty()) return false;
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.commander.synthetic_slate.real_hud_audit.v3"));
    Json->SetBoolField(TEXT("complete"), bComplete);
    Json->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty()
        && Actions.Num() >= 12 && Captures.Num() == 7 && bReopenOptionVerified
        && bFeatureSetupVerified);
    Json->SetStringField(TEXT("scope"), TEXT("GameAppCity real PlayerController->viewport HUD; in-process synthetic Slate Commander sorting; same-widget session persistence"));
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetBoolField(TEXT("null_platform_offscreen_required"), true);
    Json->SetBoolField(TEXT("slate_rendering_offscreen"), FSlateApplication::IsInitialized()
        && FSlateApplication::Get().IsRenderingOffScreen());
    Json->SetBoolField(TEXT("native_hwnd_required"), false);
    Json->SetBoolField(TEXT("os_window_hit_audit"), false);
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetStringField(TEXT("controller_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Controller.Get())));
    Json->SetStringField(TEXT("hud_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Hud.Get())));
    Json->SetStringField(TEXT("screen_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Screen.Get())));
    Json->SetBoolField(TEXT("hud_in_viewport"), Hud.IsValid() && Hud->IsInViewport());
    Json->SetBoolField(TEXT("hud_owned_by_controller"), Hud.IsValid()
        && Hud->GetOwningPlayer() == Controller.Get());
    Json->SetStringField(TEXT("foreground_at_begin"), ForegroundAtBegin);
    Json->SetStringField(TEXT("foreground_at_finish"), RokCommanderSyntheticAudit::ForegroundIdentity());
    Json->SetBoolField(TEXT("foreground_required_to_be_game_window"), false);
    Json->SetBoolField(TEXT("os_mouse_or_focus_input_used"), false);
    Json->SetNumberField(TEXT("virtual_slate_user_index"), VirtualUser.IsValid()
        ? VirtualUser->GetUserIndex() : INDEX_NONE);
    Json->SetBoolField(TEXT("cross_character_or_relaunch_persistence_proved"), false);
    Json->SetBoolField(TEXT("world_map_entry_tested"), false);
    Json->SetBoolField(TEXT("human_visual_acceptance"), false);
    Json->SetNumberField(TEXT("final_step"), Step);
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetObjectField(TEXT("feature_bar_setup"), FeatureBarSetup.IsValid()
        ? FeatureBarSetup.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("observations"), Observations);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("input_sha256"), Inputs.IsValid()
        ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(RokCommanderSyntheticAudit::Encode(Json),
        *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokCommanderSyntheticSlateAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    const bool bComplete = Failures.IsEmpty() && Actions.Num() >= 12
        && Captures.Num() == 7 && bReopenOptionVerified && bFeatureSetupVerified
        && Identity() && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0;
    if (!bComplete && Failures.IsEmpty()) Failures.Add(TEXT("Commander real-HUD audit incomplete"));
    Save(bComplete);
    UE_LOG(LogTemp, Display, TEXT("Rok Commander synthetic Slate audit %s: %s"),
        bComplete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!bComplete) UE_LOG(LogTemp, Error, TEXT("Rok Commander audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, bComplete ? 0 : 1);
}

void URokCommanderSyntheticSlateAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return;
    if (!Identity()) { Finish(TEXT("Audit left the exact Shadow GameAppCity world")); return; }
    const double Now = FPlatformTime::Seconds();
    if (Now - StartedAt > 180.0) { Finish(TEXT("Audit total wall-clock timeout")); return; }
    if (Now - StartedAt < 2.0) return;
    if (Pending.IsValid())
    {
        if (Now - StepStartedAt > 20.0) Finish(TEXT("Screenshot callback timeout"));
        else AdvanceCapture();
        return;
    }
    if (Now - StepStartedAt > 20.0) { Finish(TEXT("Commander audit step timeout")); return; }
    const auto Stable = [this](bool bOpen, int32 SortType)
    {
        StableFrames = Ready(bOpen, SortType) ? StableFrames + 1 : 0;
        return StableFrames >= 2;
    };
    const auto HudButton = [this](const TCHAR* Name)
    {
        return Hud.IsValid() ? Cast<UButton>(Hud->GetWidgetFromName(Name)) : nullptr;
    };
    const auto ScreenButton = [this](const TCHAR* Name)
    {
        return Screen.IsValid() ? Cast<UButton>(Screen->GetWidgetFromName(Name)) : nullptr;
    };
    switch (Step)
    {
    case 0: if (!bBegun) bBegun = Begin(); if (bBegun) Enter(1); break;
    case 1: if (Stable(false, 0)) Capture(TEXT("00_city_hud"), 2); break;
    case 2:
    {
        const UWidget* Parent = Hud.IsValid()
            ? Hud->GetWidgetFromName(TEXT("CommanderFeature")) : nullptr;
        if (!Expect(Parent != nullptr, TEXT("Commander feature parent missing from real HUD"))) break;
        const ESlateVisibility Visibility = Parent->GetVisibility();
        const TCHAR* State = Visibility == ESlateVisibility::SelfHitTestInvisible
            ? TEXT("SelfHitTestInvisible")
            : (Visibility == ESlateVisibility::Collapsed ? TEXT("Collapsed") : nullptr);
        if (!State) { StableFrames = 0; InitialFeatureParentState.Empty(); break; }
        if (!InitialFeatureParentState.Equals(State))
        {
            InitialFeatureParentState = State;
            StableFrames = 1;
            break;
        }
        if (++StableFrames < 2) break;
        if (!FeatureBarSetup.IsValid())
        {
            FeatureBarSetup = MakeShared<FJsonObject>();
            FeatureBarSetup->SetStringField(TEXT("initial_parent_visibility"), State);
            FeatureBarSetup->SetNumberField(TEXT("initial_stable_frame"), static_cast<double>(GFrameCounter));
            FeatureBarSetup->SetBoolField(TEXT("initial_fold_required"),
                Visibility == ESlateVisibility::SelfHitTestInvisible);
        }
        if (Visibility == ESlateVisibility::SelfHitTestInvisible)
        {
            if (Click(HudButton(TEXT("MoreFeatureButton")), TEXT("fold_initial_feature_bar")))
            {
                FeatureBarSetup->SetNumberField(TEXT("fold_clicked_frame"), static_cast<double>(GFrameCounter));
                Enter(22);
            }
        }
        else
        {
            FeatureBarSetup->SetNumberField(TEXT("collapsed_stable_frame"), static_cast<double>(GFrameCounter));
            Enter(23);
        }
        break;
    }
    case 22:
    {
        const UWidget* Parent = Hud.IsValid()
            ? Hud->GetWidgetFromName(TEXT("CommanderFeature")) : nullptr;
        StableFrames = Parent && Parent->GetVisibility() == ESlateVisibility::Collapsed
            ? StableFrames + 1 : 0;
        if (StableFrames >= 2)
        {
            FeatureBarSetup->SetNumberField(TEXT("collapsed_stable_frame"), static_cast<double>(GFrameCounter));
            Enter(23);
        }
        break;
    }
    case 23:
        if (Click(HudButton(TEXT("MoreFeatureButton")), TEXT("open_more_feature_bar")))
        {
            FeatureBarSetup->SetNumberField(TEXT("open_clicked_frame"), static_cast<double>(GFrameCounter));
            Enter(24);
        }
        break;
    case 24:
    {
        const UWidget* Parent = Hud.IsValid()
            ? Hud->GetWidgetFromName(TEXT("CommanderFeature")) : nullptr;
        const UButton* Commander = HudButton(TEXT("CommanderFeatureButton"));
        StableFrames = Parent && Parent->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
            && Commander && RokCommanderSyntheticAudit::Visible(Commander) && Commander->GetIsEnabled()
            && Commander->GetCachedGeometry().GetLocalSize().X > 0.0f
            && Commander->GetCachedGeometry().GetLocalSize().Y > 0.0f
            ? StableFrames + 1 : 0;
        if (StableFrames >= 2)
        {
            FeatureBarSetup->SetNumberField(TEXT("expanded_interactive_stable_frame"), static_cast<double>(GFrameCounter));
            bFeatureSetupVerified = true;
            Enter(3);
        }
        break;
    }
    case 3: if (Click(HudButton(TEXT("CommanderFeatureButton")), TEXT("open_commander_from_real_hud"))) Enter(4); break;
    case 4: if (Stable(true, 0) && ObserveSort(0, TEXT("first_quality"))) Capture(TEXT("01_first_quality"), 5); break;
    case 5: if (Click(ScreenButton(TEXT("CommanderSortAction")), TEXT("open_sort_menu_for_power"))) Enter(6); break;
    case 6: if (Click(ScreenButton(TEXT("CommanderSortOption_3")), TEXT("select_power_475"))) Enter(7); break;
    case 7: if (Stable(true, 3) && ObserveSort(3, TEXT("power"))) Capture(TEXT("02_power"), 8); break;
    case 8: if (Click(ScreenButton(TEXT("CommanderSortAction")), TEXT("open_sort_menu_for_star"))) Enter(9); break;
    case 9: if (Click(ScreenButton(TEXT("CommanderSortOption_1")), TEXT("select_star_473"))) Enter(10); break;
    case 10: if (Stable(true, 1) && ObserveSort(1, TEXT("star"))) Capture(TEXT("03_star"), 11); break;
    case 11: if (Click(ScreenButton(TEXT("CommanderSortAction")), TEXT("open_sort_menu_for_level"))) Enter(12); break;
    case 12: if (Click(ScreenButton(TEXT("CommanderSortOption_2")), TEXT("select_level_474"))) Enter(13); break;
    case 13: if (Stable(true, 2) && ObserveSort(2, TEXT("level_before_close"))) Capture(TEXT("04_level"), 14); break;
    case 14: if (Click(ScreenButton(TEXT("CommanderBack")), TEXT("close_commander_back_1"))) Enter(15); break;
    case 15: if (Stable(false, 0)) Capture(TEXT("05_closed"), 16); break;
    case 16:
        if (RokCommanderSyntheticAudit::Visible(HudButton(TEXT("CommanderFeatureButton")))) Enter(17);
        else if (Click(HudButton(TEXT("MoreFeatureButton")), TEXT("reopen_more_feature_bar"))) Enter(17);
        break;
    case 17: if (Click(HudButton(TEXT("CommanderFeatureButton")), TEXT("reopen_commander_same_hud"))) Enter(18); break;
    case 18:
        if (Stable(true, 2) && Hud->GetFeatureScreenWidgetForPlacement() == Screen.Get()
            && ObserveSort(2, TEXT("reopened_level"))) Capture(TEXT("06_reopened_level"), 19);
        break;
    case 19: if (Click(ScreenButton(TEXT("CommanderSortAction")), TEXT("inspect_persisted_option"))) Enter(20); break;
    case 20:
    {
        bool bOptions = true;
        const TCHAR* ExpectedLabels[] = {TEXT("品质"), TEXT("星级"), TEXT("等级"), TEXT("战力")};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const UTextBlock* Text = Cast<UTextBlock>(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("CommanderSortOption_%dLabel"), Index)));
            const FLinearColor Expected = Index == 2
                ? FLinearColor(1.0f, 0.78f, 0.24f, 1.0f)
                : FLinearColor(0.94f, 0.94f, 0.92f, 1.0f);
            bOptions &= Text && RokCommanderSyntheticAudit::Visible(Text)
                && Text->GetText().ToString() == ExpectedLabels[Index]
                && Text->GetColorAndOpacity().GetSpecifiedColor().Equals(Expected, .001f);
        }
        if (!bOptions) break;
        bReopenOptionVerified = true;
        auto OptionProof = MakeShared<FJsonObject>();
        OptionProof->SetStringField(TEXT("phase"), TEXT("reopened_level_option_highlight"));
        OptionProof->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
        OptionProof->SetNumberField(TEXT("selected_option_inferred_from_highlight"), 2);
        OptionProof->SetBoolField(TEXT("all_four_labels_and_colors_verified"), true);
        Observations.Add(MakeShared<FJsonValueObject>(OptionProof));
        if (Click(ScreenButton(TEXT("CommanderSortOption_2")), TEXT("confirm_persisted_level_option"))) Enter(21);
        break;
    }
    case 21: if (Stable(true, 2) && ObserveSort(2, TEXT("confirmed_option"))) Finish(); break;
    default: Finish(TEXT("Invalid Commander audit step")); break;
    }
}
