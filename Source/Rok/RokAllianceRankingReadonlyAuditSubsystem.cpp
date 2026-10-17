#include "RokAllianceRankingReadonlyAuditSubsystem.h"

#include "RokAllianceRankingReadonlyCatalog.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
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

namespace
{
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AtlasPath[] = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player");
constexpr TCHAR CatalogPath[] = TEXT("Saved/RokUnityMigration/alliance_ranking_readonly_catalog.json");
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceRankingReadonlyAudit");
constexpr TCHAR CurrentFeatureCppSha[] = TEXT("a421b7c542f3f40bca8883960e0bea17c64b2fb956da46a1f70ad151f2923019");
constexpr TCHAR CurrentFeatureHeaderSha[] = TEXT("e5e395a0801e77b13455c74b01976024d070d3f11b458be2ceec6d8da08111e3");
struct FPin { const TCHAR* Path; const TCHAR* Sha; };
constexpr FPin FixedPins[] = {
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditV4/READY.json"), TEXT("38fdd5a8f789f5c7ab6cadc7d514ab49a4a40c8a0e431871ffda7ccae5477c09")},
    {TEXT("Saved/PendingRankingReadonlyFlowInventoryV1/READY.json"), TEXT("1c8fb632a77a9a99c084c79c22e1dcf42754f231104150a69eba724ab2b79a18")},
    {TEXT("Saved/PendingRankingReadonlyFlowInventoryV1/source_pins.json"), TEXT("1ec55c090c6850969465e957da08476629264d67f09eadd4ed31d0dd7e423037")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditV1/READY.json"), TEXT("34cc96d1dfffa582ba63ed35ccbb6731e29dad0cd70ee057c1f240e5365dc26f")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditV1/Source/Rok/RokAllianceRankingReadonlyAuditSubsystem.cpp"), TEXT("c22dd329e3f559fc1aa544f0daca2c306b91384a538071ee69e409d98941381d")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditV2/READY.json"), TEXT("e355465a562baf195bfe0c47457bfcbf119f4263c5a097339f259449bad4c53f")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditIndependentReviewV2/READY.json"), TEXT("3a98bf7d5b88e0e7094992545c8000120b7eb718a9cee4f51f718877102893b5")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditIndependentReviewV2/report.md"), TEXT("821e12816c34cb3769fe1a6b9ecf91baeb2edd1e6b4db749d9bee6a6d0913e58")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeAuditV3/READY.json"), TEXT("386398b321a9b4c1133a6fa56a033fc12f7bebd9017c5d2f87f33f93f9fc5986")},
    {TEXT("Saved/UIB1/Run5/execution_result.json"), TEXT("66a923e3cc724ff910a8e720662a19c6cdc313e178704a6b1b87143c5b281545")},
    {TEXT("Saved/UIB1/Run5/execute_process.json"), TEXT("2273c0abb00f0f4d3a93d347adff03aa90cf834d4a339dc2820f40491576ff81")},
    {TEXT("Saved/UIB1/Run5/execute.log"), TEXT("d96b73671967610f13b5cd590df5b62f7d3904e96d25edc27de3dd0e8c1acf57")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeContractV2/READY.json"), TEXT("ce81df1402a569cd05da1f36abe8b8eaa10310f94725e3563925500776f83bea")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeContractV2/contract.json"), TEXT("01b2132c5b79c01043f0a89c8e5d24ca667ff9ea670c4c1fc452f11a0e78a672")},
    {TEXT("Saved/PendingAllianceRankingReadonlyRuntimeContractRootReviewV2/review.json"), TEXT("e8e1319f4923b03d9334c8efd80bbc8b22666bd5534ee74cd30583edfb49642e")},
    {TEXT("Saved/PendingAllianceRankingReadonlyProductV1/Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("d893ee02a3a1b8f36287b626b50a9947947b6f5e74fe19283ffcd1e452a0daaf")},
    {TEXT("Saved/PendingAllianceRankingReadonlyProductV1/Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("87b3f3daa440378cfe59af8b71e91c24f77d01ee2edff03736a96da38797d804")},
    {TEXT("Saved/PendingAllianceRankingReadonlyProductIndependentReviewV1/READY.json"), TEXT("a2b7bbd0131296b19ebd46e93a5e6f6a46ddb69a281db0b48face8cbfde7dfa0")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogV2/READY.json"), TEXT("e224169305ea40cc68d5fefcd46b509103bea35221002cdf7aafbcedbf43ea04")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogIndependentReviewV2/READY.json"), TEXT("b3c661c8b4be9a0ce2b16a6514cdc5c46cd32afd77506fa88f7bb0dd540b4160")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogIndependentReviewV2/review.json"), TEXT("6d4e764c6edf04ff730a9a03539801c2e8057ade324ec0a25edf431993a45405")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogV2/Source/Rok/RokAllianceRankingReadonlyCatalog.cpp"), TEXT("244a5ffa986ac827b741de9e8d0cb08dab9955868c67ac78cc2e0ad6bcc04d1b")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogV2/Source/Rok/RokAllianceRankingReadonlyCatalog.h"), TEXT("2174b548836eeab7a67de219e9d03efb1631df8b2ddc3fd8129410c785771a16")},
    {TEXT("Saved/PendingAllianceRankingReadonlyCatalogV2/runs/normal/alliance_ranking_readonly_catalog.json"), TEXT("40f81f693e6b65f05a1c36f77d75b22cdc7e03e89a4e3d33fa6fb402b9daFA9c")},
    {TEXT("Saved/RokUnityMigration/RankingUiShadowCompiledAggregateV1_20261010T131702522739Z/archive.json"), TEXT("92c6d6d9a09bc1c07d039088296ca70c807cb106a3fbc21f08694cb097980ab7")},
    {TEXT("Saved/UIB1/Run4/execution_result.json"), TEXT("b22f82f7ffd8dd912e00ba4a7e4483dc6bf3d6313fa9edc980dcdbe45961fee6")},
};

FString FileSha(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
FString ForegroundIdentity()
{
#if PLATFORM_WINDOWS
    return FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
#else
    return TEXT("unsupported_platform");
#endif
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
    FString Out;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Out));
    return Out;
}
TSharedRef<FJsonObject> Point(const FVector2D& V)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"), V.X);
    Json->SetNumberField(TEXT("y"), V.Y);
    return Json;
}
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    return true;
}
TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("exists"), Widget != nullptr);
    Json->SetBoolField(TEXT("visible"), Visible(Widget));
    Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
    Json->SetStringField(TEXT("class"), Widget ? Widget->GetClass()->GetPathName() : FString());
    Json->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
    if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
    const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
    Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
    Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
    Json->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
    if (const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
    {
        Json->SetObjectField(TEXT("slot_position"), Point(Slot->GetPosition()));
        Json->SetObjectField(TEXT("slot_size"), Point(Slot->GetSize()));
    }
    if (const UImage* Icon = Cast<UImage>(Widget))
    {
        const FSlateBrush& Brush = Icon->GetBrush();
        const FBox2f UV = Brush.GetUVRegion();
        Json->SetStringField(TEXT("brush_resource"), Brush.GetResourceObject() ? Brush.GetResourceObject()->GetPathName() : FString());
        Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(UV.Min.X, UV.Min.Y)));
        Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(UV.Max.X, UV.Max.Y)));
    }
    return Json;
}
bool SlotMatches(const UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
{
    const UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
    return Slot && Slot->GetPosition().Equals(Position, .1f) && Slot->GetSize().Equals(Size, .1f)
        && Widget->GetCachedGeometry().GetLocalSize().X > 0 && Widget->GetCachedGeometry().GetLocalSize().Y > 0;
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
    return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
        | (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
bool TargetWorld(const UWorld* World)
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    FPaths::NormalizeDirectoryName(Project);
    FPaths::NormalizeDirectoryName(Engine);
    return World && World->WorldType == EWorldType::Game && World->GetOutermost()->GetName() == CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
        && FEngineVersion::Current().GetMajor() == 5 && FEngineVersion::Current().GetMinor() == 8;
}
}

bool URokAllianceRankingReadonlyAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceRankingReadonlyAudit"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
        && !IsRunningDedicatedServer()
        && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceRankingReadonlyAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokAllianceRankingReadonlyAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceRankingReadonlyAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceRankingReadonlyAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    const FString Parent = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingAllianceRankingReadonlyAuditOffscreenV5/RuntimeAudit"));
    int64 Ticks = FDateTime::UtcNow().GetTicks();
    do { Directory = Parent / FString::Printf(TEXT("%lld_%u"), Ticks++, FPlatformProcess::GetCurrentProcessId()); }
    while (IFileManager::Get().DirectoryExists(*Directory));
    FPaths::NormalizeDirectoryName(Directory);
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) Failures.Add(TEXT("Unique Saved output directory creation failed"));
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,
        &URokAllianceRankingReadonlyAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceRankingReadonlyAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    VirtualUser.Reset();
    if (!bFinished)
    {
        Failures.Add(TEXT("World/subsystem deinitialized before ranking audit completed"));
        bFinished = true; // Stop Tick and Finish from writing another receipt during teardown.
        if (!Save(false))
        {
            Failures.Add(TEXT("Ranking early-deinitialize failure receipt write failed"));
            UE_LOG(LogTemp, Error, TEXT("Ranking early-deinitialize failure receipt write failed: %s"), *Directory);
        }
        Super::Deinitialize();
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    Super::Deinitialize();
}
bool URokAllianceRankingReadonlyAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceRankingReadonlyAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
    if (!Condition) Finish(Reason);
    return Condition;
}
bool URokAllianceRankingReadonlyAuditSubsystem::OwnOffscreenWindow() const
{
#if PLATFORM_WINDOWS
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    return Window.IsValid() && Window->IsVisible()
        && Window->GetViewportSize().Equals(FVector2D(1440, 810), .1f)
        && Native.IsValid() && Native->GetOSWindowHandle() == nullptr
        && Native->IsRenderingOffScreen()
        && FSlateApplication::IsInitialized() && FSlateApplication::Get().IsRenderingOffScreen()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
#else
    return false;
#endif
}
bool URokAllianceRankingReadonlyAuditSubsystem::InputBoundary() const
{
    if (!OwnOffscreenWindow() || !VirtualUser.IsValid() || VirtualUser->GetUserIndex() <= 0
        || !FSlateApplication::IsInitialized()) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const TSharedPtr<FSlateUser> Zero = App.GetUser(0);
    bool bCursorSame = false;
#if PLATFORM_WINDOWS
    POINT Cursor = {};
    bCursorSame = bHardwareCursorRead && ::GetCursorPos(&Cursor) != 0
        && Cursor.x == HardwareCursorX && Cursor.y == HardwareCursorY;
#endif
    return Zero.IsValid() && Zero->GetFocusedWidget() == UserZeroFocus.Pin()
        && App.GetUser(VirtualUser->GetUserIndex()).IsValid()
        && ForegroundIdentity() == ForegroundAtBegin && bCursorSame;
}

bool URokAllianceRankingReadonlyAuditSubsystem::AwaitAtlasReady()
{
    UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, AtlasPath);
    if (!Atlas) return false; // Preserve the original strict load failure.
    const FTextureResource* Resource = Atlas->GetResource();
    bool bCompiling = false;
    bool bDefaultTexture = false;
#if WITH_EDITOR
    bCompiling = Atlas->IsCompiling();
    bDefaultTexture = Atlas->IsDefaultTexture();
#endif
    const bool bResourceProxy = Resource && Resource->IsProxy();
    const bool bPending = bCompiling || bDefaultTexture || bResourceProxy;
    if (!bPending && AtlasAsyncWaitStart < 0.0) return false;
    if (AtlasAsyncWaitStart < 0.0)
    {
        AtlasAsyncWaitStart = Elapsed;
        AtlasAsyncWaitDiagnostic = MakeShared<FJsonObject>();
        AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("first_frame"), GFrameCounter);
        AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("first_is_compiling"), bCompiling);
        AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("first_is_default_texture"), bDefaultTexture);
        AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("first_size_x"), Atlas->GetSizeX());
        AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("first_size_y"), Atlas->GetSizeY());
        AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("first_resource_present"), Resource != nullptr);
        AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("first_resource_proxy"), bResourceProxy);
        AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("first_texture_rhi_valid"),
            Resource && Resource->TextureRHI.IsValid());
    }
    ++AtlasAsyncWaitPolls;
    const double WaitElapsed = Elapsed - AtlasAsyncWaitStart;
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("poll_count"), AtlasAsyncWaitPolls);
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("start_elapsed_seconds"), AtlasAsyncWaitStart);
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("elapsed_seconds"), WaitElapsed);
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("last_frame"), GFrameCounter);
    AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("last_is_compiling"), bCompiling);
    AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("last_is_default_texture"), bDefaultTexture);
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("last_size_x"), Atlas->GetSizeX());
    AtlasAsyncWaitDiagnostic->SetNumberField(TEXT("last_size_y"), Atlas->GetSizeY());
    AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("last_resource_present"), Resource != nullptr);
    AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("last_resource_proxy"), bResourceProxy);
    AtlasAsyncWaitDiagnostic->SetBoolField(TEXT("last_texture_rhi_valid"),
        Resource && Resource->TextureRHI.IsValid());
    AtlasAsyncWaitDiagnostic->SetStringField(TEXT("status"),
        WaitElapsed >= 20.0 ? TEXT("timeout") : (bPending ? TEXT("waiting") : TEXT("ready")));
    CatalogAtlasDiagnostic = MakeShared<FJsonObject>();
    CatalogAtlasDiagnostic->SetStringField(TEXT("atlas_path"), AtlasPath);
    CatalogAtlasDiagnostic->SetObjectField(TEXT("atlas_async_wait"), AtlasAsyncWaitDiagnostic.ToSharedRef());
    if (WaitElapsed >= 20.0)
    {
        Finish(TEXT("Ranking atlas async readiness timeout"));
        return true;
    }
    if (!bPending) return false; // Begin still runs every original strict atlas/catalog gate.
    return true;
}

bool URokAllianceRankingReadonlyAuditSubsystem::Begin()
{
    if (!Expect(Identity() && FSlateApplication::IsInitialized()
        && IFileManager::Get().DirectoryExists(*Directory) && Failures.IsEmpty()
        && FParse::Param(FCommandLine::Get(), TEXT("game")) && FApp::IsGame()
        && FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
        && FNullPlatformApplicationMisc::IsUsingNullApplication(),
        TEXT("Wrong project/map/engine, Slate unavailable, or output missing"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token;
    int32 Audit = 0, Local = 0, Fixture = 0, Offscreen = 0, Game = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++Audit; continue; }
        if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++Local; continue; }
        if (Token.Equals(TEXT("-RokOpenModal=alliance_joined"), ESearchCase::IgnoreCase)) { ++Fixture; continue; }
        if (Token.Equals(TEXT("-RenderOffScreen"), ESearchCase::IgnoreCase)) { ++Offscreen; continue; }
        if (Token.Equals(TEXT("-game"), ESearchCase::IgnoreCase)) { ++Game; continue; }
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)) return Expect(false, TEXT("Unexpected Rok fixture/action flag"));
    }
    if (!Expect(Audit == 1 && Local == 1 && Fixture == 1 && Offscreen == 1 && Game == 1,
        TEXT("Exactly one opt-in, local-only, joined fixture, offscreen game flag required"))) return false;
    bBeginBoundaryObserved = true;
    bBeginOwnOffscreenWindow = OwnOffscreenWindow();
    if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
    {
        Viewport->GetViewportSize(BeginPhysicalViewport);
    }
    BeginWidgetViewport = UWidgetLayoutLibrary::GetViewportSize(this);
    BeginViewportDpiScale = UWidgetLayoutLibrary::GetViewportScale(this);
    BeginExpectedDpiScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(1440, 810));
    if (!Expect(bBeginOwnOffscreenWindow
        && BeginPhysicalViewport.Equals(FVector2D(1440, 810), .1f)
        && BeginWidgetViewport.Equals(FVector2D(1440, 810), .1f)
        && FMath::IsFinite(BeginViewportDpiScale) && BeginViewportDpiScale > 0.0f
        && FMath::IsFinite(BeginExpectedDpiScale) && BeginExpectedDpiScale > 0.0f
        && FMath::IsNearlyEqual(BeginViewportDpiScale, BeginExpectedDpiScale, .01f),
        TEXT("Audit requires owned Null offscreen 1440x810 viewport with valid configured DPI scale"))) return false;
    Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
    Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
    Server = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
    for (UUserWidget* Widget : Widgets) if (Widget && Widget->GetWorld() == GetWorld())
    {
        if (Hud.IsValid()) return Expect(false, TEXT("More than one GameApp HUD"));
        Hud = Cast<URokMainHudWidget>(Widget);
    }
    Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
    Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
    if (!Expect(Controller.IsValid() && Controller->GetPawn() && Runtime.IsValid()
        && Runtime->IsRuntimeStateLoaded() && Server.IsValid() && Server->IsLocalOnlyRun()
        && Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Modal->IsModalOpen()
        && Screen->IsScreenOpen()
        && Screen->ActiveMode == ERokUnityFeatureScreenMode::Alliance && Screen->bAllianceTestFixtureState
        && Screen->AlliancePreviewState == 2 && Screen->SelectedAlliancePage == 0
        && !Screen->bAllianceRankingReferenceOpen,
        TEXT("Expected anonymous local-only CLI joined Alliance fixture"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
        && !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
        TEXT("Audit requires anonymous local-preview profile"))) return false;
    Inputs = MakeShared<FJsonObject>();
    for (const FPin& Pin : FixedPins)
    {
        const FString Actual = FileSha(FString(TEXT("D:/ueDemo/Rok")) / Pin.Path);
        if (!Expect(Actual.Equals(Pin.Sha, ESearchCase::IgnoreCase), TEXT("Frozen Ranking input pin mismatch"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Name : {TEXT("RokUnityFeatureScreenWidget.cpp"), TEXT("RokUnityFeatureScreenWidget.h"),
        TEXT("RokAllianceRankingReadonlyAuditSubsystem.cpp"), TEXT("RokAllianceRankingReadonlyAuditSubsystem.h")})
    {
        const FString Staged = FString(TEXT("D:/ueDemo/Rok/Saved"))
            / ((FCString::Strcmp(Name, TEXT("RokAllianceRankingReadonlyAuditSubsystem.cpp")) == 0
                || FCString::Strcmp(Name, TEXT("RokAllianceRankingReadonlyAuditSubsystem.h")) == 0)
                ? TEXT("PendingRootRankingAtlasAsyncReadyFixV2/Source/Rok")
                : (FCString::Strcmp(Name, TEXT("RokUnityFeatureScreenWidget.cpp")) == 0
                    ? TEXT("PendingRokMailCategoryAutoSelectSliceV2/Source/Rok")
                    : TEXT("PendingRokMailLocalUnreadSliceV2/Source/Rok"))) / Name;
        const FString Live = FPaths::ProjectDir() / TEXT("Source/Rok") / Name;
        const FString Pin = FileSha(Staged);
        if (!Expect(Pin.Len() == 64 && Pin == FileSha(Live), TEXT("Integrated Ranking audit source differs from frozen candidate"))) return false;
        if (FCString::Strcmp(Name, TEXT("RokUnityFeatureScreenWidget.cpp")) == 0
            && !Expect(Pin.Equals(CurrentFeatureCppSha, ESearchCase::IgnoreCase),
                TEXT("Current Ranking Feature CPP is not the reviewed Shadow product"))) return false;
        if (FCString::Strcmp(Name, TEXT("RokUnityFeatureScreenWidget.h")) == 0
            && !Expect(Pin.Equals(CurrentFeatureHeaderSha, ESearchCase::IgnoreCase),
                TEXT("Current Ranking Feature header is not the reviewed Shadow product"))) return false;
        Inputs->SetStringField(Live, Pin);
    }
    for (const TCHAR* Name : {TEXT("RokAllianceRankingReadonlyCatalog.cpp"), TEXT("RokAllianceRankingReadonlyCatalog.h")})
    {
        const FString Staged = FString(TEXT("D:/ueDemo/Rok/Saved/PendingAllianceRankingReadonlyCatalogV2/Source/Rok")) / Name;
        const FString Live = FPaths::ProjectDir() / TEXT("Source/Rok") / Name;
        if (!Expect(FileSha(Staged).Len() == 64 && FileSha(Staged) == FileSha(Live),
            TEXT("Integrated CatalogV2 source differs from reviewed candidate"))) return false;
        Inputs->SetStringField(Live, FileSha(Live));
    }
    const FString Canonical = FileSha(RokDataPaths::ResolveProjectPath(CatalogPath));
    if (!Expect(Canonical.Equals(TEXT("40f81f693e6b65f05a1c36f77d75b22cdc7e03e89a4e3d33fa6fb402b9daFA9c"), ESearchCase::IgnoreCase),
        TEXT("Canonical ranking catalog missing or differs from reviewed CatalogV2"))) return false;
    Inputs->SetStringField(CatalogPath, Canonical);
    if (!Expect(CatalogAndAtlas(), TEXT("Catalog rows/atlas do not match reviewed local preview"))) return false;
    FSlateApplication& App = FSlateApplication::Get();
    if (!Expect(App.GetUser(0).IsValid() && OwnOffscreenWindow(),
        TEXT("Owned offscreen SWindow or Slate user zero missing"))) return false;
    UserZeroFocus = App.GetUserFocusedWidget(0);
    ForegroundAtBegin = ForegroundIdentity();
#if PLATFORM_WINDOWS
    POINT HardwareCursor = {};
    bHardwareCursorRead = ::GetCursorPos(&HardwareCursor) != 0;
    if (bHardwareCursorRead) { HardwareCursorX = HardwareCursor.x; HardwareCursorY = HardwareCursor.y; }
#endif
    if (!Expect(bHardwareCursorRead, TEXT("Cannot read external hardware cursor"))) return false;
    VirtualUser = App.FindOrCreateVirtualUser(1);
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0
        && App.GetUser(VirtualUser->GetUserIndex()).IsValid(),
        TEXT("Retained virtual Slate user unavailable"))) return false;
    BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    BaselineHttpDispatches = Server->GetHttpDispatchAttemptCount();
    BaselineSuppressedRequests = Server->GetSuppressedLocalRequestCount();
    if (!Expect(BaselineHttpDispatches == 0 && BaselineSuppressedRequests == 0,
        TEXT("Local server counters must start at zero"))) return false;
    bBaselineSet = true;
    return Expect(InputBoundary(), TEXT("Ranking offscreen input boundary changed before first click"));
}

bool URokAllianceRankingReadonlyAuditSubsystem::CatalogAndAtlas() const
{
    CatalogAtlasDiagnostic = MakeShared<FJsonObject>();
    CatalogAtlasDiagnostic->SetStringField(TEXT("atlas_path"), AtlasPath);
    if (AtlasAsyncWaitDiagnostic.IsValid())
        CatalogAtlasDiagnostic->SetObjectField(TEXT("atlas_async_wait"), AtlasAsyncWaitDiagnostic.ToSharedRef());
    auto Fail = [this](const TCHAR* Code, int32 RowIndex = INDEX_NONE, const TCHAR* Field = TEXT(""))
    {
        CatalogAtlasDiagnostic->SetStringField(TEXT("status"), TEXT("failed"));
        CatalogAtlasDiagnostic->SetStringField(TEXT("code"), Code);
        if (RowIndex != INDEX_NONE) CatalogAtlasDiagnostic->SetNumberField(TEXT("row_index"), RowIndex);
        if (Field[0] != 0) CatalogAtlasDiagnostic->SetStringField(TEXT("field"), Field);
        return false;
    };
    TArray<FRokLeaderboardRow> Rows;
    FString Error;
    if (!RokAllianceRankingReadonlyCatalog::Load(Rows, Error))
    {
        CatalogAtlasDiagnostic->SetStringField(TEXT("catalog_error"), Error);
        return Fail(TEXT("catalog_load"));
    }
    CatalogAtlasDiagnostic->SetNumberField(TEXT("catalog_row_count"), Rows.Num());
    if (Rows.Num() != 6) return Fail(TEXT("catalog_count"));
    UTexture2D* Atlas = LoadObject<UTexture2D>(nullptr, AtlasPath);
    if (!Atlas) return Fail(TEXT("atlas_load_null"));
    CatalogAtlasDiagnostic->SetNumberField(TEXT("atlas_width"), Atlas->GetSizeX());
    CatalogAtlasDiagnostic->SetNumberField(TEXT("atlas_height"), Atlas->GetSizeY());
    if (Atlas->GetSizeX() != 512) return Fail(TEXT("atlas_size"), INDEX_NONE, TEXT("width"));
    if (Atlas->GetSizeY() != 1024) return Fail(TEXT("atlas_size"), INDEX_NONE, TEXT("height"));
    const auto* Resource = Atlas->GetResource();
    if (!Resource) return Fail(TEXT("atlas_resource_null"));
    if (!Resource->TextureRHI.IsValid()) return Fail(TEXT("atlas_texture_rhi_invalid"));
    const int32 Types[] = {4, 5, 7, 8, 9, 10};
    const TCHAR* Icons[] = {TEXT("ui_player[img_player_201]"), TEXT("ui_player[img_player_205]"),
        TEXT("ui_player[img_player_204]"), TEXT("ui_player[img_player_206]"),
        TEXT("ui_player[img_player_210]"), TEXT("ui_player[img_player_209]")};
    const int32 Rects[][4] = {{301,141,148,176},{151,319,148,176},{301,497,148,176},
        {1,141,148,176},{301,675,148,176},{1,497,148,176}};
    for (int32 I = 0; I < 6; ++I)
    {
        const FRokLeaderboardRow& Row = Rows[I];
        if (Row.Id != 300 + I) return Fail(TEXT("catalog_row"), I, TEXT("id"));
        if (Row.Target != 3) return Fail(TEXT("catalog_row"), I, TEXT("target"));
        if (Row.Type != Types[I]) return Fail(TEXT("catalog_row"), I, TEXT("type"));
        if (Row.ShowLimit != 999) return Fail(TEXT("catalog_row"), I, TEXT("show_limit"));
        if (Row.Name.IsEmpty()) return Fail(TEXT("catalog_row"), I, TEXT("name"));
        if (Row.Columns.Num() != 2) return Fail(TEXT("catalog_row"), I, TEXT("columns"));
        if (Row.Icon != Icons[I]) return Fail(TEXT("catalog_row"), I, TEXT("icon"));
        if (Row.IconRect.Min.X != Rects[I][0]) return Fail(TEXT("catalog_row"), I, TEXT("rect_x"));
        if (Row.IconRect.Min.Y != Rects[I][1]) return Fail(TEXT("catalog_row"), I, TEXT("rect_y"));
        if (Row.IconRect.Width() != Rects[I][2]) return Fail(TEXT("catalog_row"), I, TEXT("rect_width"));
        if (Row.IconRect.Height() != Rects[I][3]) return Fail(TEXT("catalog_row"), I, TEXT("rect_height"));
    }
    CatalogAtlasDiagnostic->SetStringField(TEXT("status"), TEXT("passed"));
    return true;
}

bool URokAllianceRankingReadonlyAuditSubsystem::EvidenceState(int32 ExpectedPage, bool bRanking, int32 ExpectedCategory) const
{
    if (!Screen.IsValid() || !Screen->IsScreenOpen() || Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance
        || Screen->AlliancePreviewState != 2 || !Screen->bAllianceTestFixtureState
        || Screen->SelectedAlliancePage != ExpectedPage || Screen->bAllianceRankingReferenceOpen != bRanking
        || Screen->SelectedAllianceRankingCategoryId != ExpectedCategory
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE
        || Screen->bAllianceTerritoryReferenceOpen || Screen->bAllianceGiftReferenceOpen
        || Screen->bAllianceShopReferenceOpen || Screen->bAllianceFlagEditorOpen
        || Screen->bAllianceOfficeReferenceOpen || Screen->bAllianceTechnologyReferenceOpen) return false;
    const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureFrameTitle")));
    if (!Title || !Visible(Title)) return false;
    if (bRanking && Title->GetText().ToString() != TEXT("联盟排行榜")) return false;
    if (!bRanking && ExpectedPage == 2 && Title->GetText().ToString() != TEXT("联盟设置")) return false;
    if (!Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Mode || !Visible(Mode) || !FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0f, .01f)
        || !Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01f)
        || !Mode->GetRenderTransform().Translation.IsNearlyZero(.25f)) return false;
    if (!bRanking) return ExpectedPage != 2 || Visible(Screen->GetWidgetFromName(TEXT("AllianceSettingHit_2")));
    if (!CatalogAndAtlas() || Visible(Screen->GetWidgetFromName(TEXT("AllianceRankingCatalogUnavailable")))) return false;
    if (!SlotMatches(Screen->GetWidgetFromName(TEXT("AllianceRankingContentShade")), FVector2D(183,91), FVector2D(1074,664))
        || !SlotMatches(Screen->GetWidgetFromName(TEXT("AllianceRankingBack")), FVector2D(155,32), FVector2D(138,54))) return false;
    if (ExpectedCategory == INDEX_NONE)
    {
        const UTextBlock* Boundary = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceRankingOverviewBoundary")));
        if (!Boundary || !Visible(Boundary) || !Boundary->GetText().ToString().Contains(TEXT("排名数据未接入"))) return false;
        TArray<FRokLeaderboardRow> Rows;
        FString Error;
        if (!RokAllianceRankingReadonlyCatalog::Load(Rows, Error) || Rows.Num() != 6) return false;
        const FVector2D Slots[] = {FVector2D(200,141),FVector2D(550,141),FVector2D(900,141),
            FVector2D(200,385),FVector2D(550,385),FVector2D(900,385)};
        for (int32 I = 0; I < 6; ++I)
        {
            const FString Prefix(TEXT("AllianceRankingCategory"));
            const UWidget* Panel = Screen->GetWidgetFromName(*FString::Printf(TEXT("%sPanel_%d"), *Prefix, Rows[I].Id));
            const UTextBlock* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("%sName_%d"), *Prefix, Rows[I].Id)));
            const UTextBlock* Empty = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("%sEmpty_%d"), *Prefix, Rows[I].Id)));
            const UImage* Icon = Cast<UImage>(Screen->GetWidgetFromName(*FString::Printf(TEXT("%sIcon_%d"), *Prefix, Rows[I].Id)));
            const UButton* Hit = Cast<UButton>(Screen->GetWidgetFromName(*FString::Printf(TEXT("%sHit_%d"), *Prefix, Rows[I].Id)));
            if (!SlotMatches(Panel, Slots[I], FVector2D(340,181)) || !Visible(Panel)
                || !Name || !Visible(Name) || Name->GetText().ToString() != Rows[I].Name
                || !Empty || !Visible(Empty) || Empty->GetText().ToString() != TEXT("暂无排名数据")
                || !Icon || !Visible(Icon) || !Hit || !Visible(Hit) || !Hit->GetIsEnabled()) return false;
            const FSlateBrush& Brush = Icon->GetBrush();
            const FBox2f UV = Brush.GetUVRegion();
            const float Top = static_cast<float>(1024 - Rows[I].IconRect.Max.Y);
            if (!Brush.GetResourceObject() || Brush.GetResourceObject()->GetPathName() != AtlasPath
                || !FMath::IsNearlyEqual(UV.Min.X, Rows[I].IconRect.Min.X / 512.0f, .0001f)
                || !FMath::IsNearlyEqual(UV.Max.X, Rows[I].IconRect.Max.X / 512.0f, .0001f)
                || !FMath::IsNearlyEqual(UV.Min.Y, Top / 1024.0f, .0001f)
                || !FMath::IsNearlyEqual(UV.Max.Y, (Top + Rows[I].IconRect.Height()) / 1024.0f, .0001f)) return false;
        }
        return !Screen->GetWidgetFromName(TEXT("AllianceRankingDetailTitle"))
            && !Screen->GetWidgetFromName(TEXT("AllianceRankingEmptyList"));
    }
    TArray<FRokLeaderboardRow> Rows;
    FString Error;
    if (!RokAllianceRankingReadonlyCatalog::Load(Rows, Error)) return false;
    const FRokLeaderboardRow* Selected = Rows.FindByPredicate([ExpectedCategory](const FRokLeaderboardRow& Row)
        { return Row.Id == ExpectedCategory; });
    const UTextBlock* TitleDetail = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceRankingDetailTitle")));
    const UTextBlock* EmptyList = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceRankingEmptyList")));
    const UTextBlock* NoSelf = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("AllianceRankingNoSelfData")));
    return Selected && TitleDetail && Visible(TitleDetail) && TitleDetail->GetText().ToString() == Selected->Name
        && EmptyList && Visible(EmptyList) && EmptyList->GetText().ToString() == TEXT("暂无排名数据")
        && NoSelf && Visible(NoSelf) && NoSelf->GetText().ToString() == TEXT("当前排名与得分未接入")
        && SlotMatches(Screen->GetWidgetFromName(TEXT("AllianceRankingEmptyListPanel")),
            FVector2D(192.915f,227.23f), FVector2D(1054.17f,531.22f))
        && !Screen->GetWidgetFromName(TEXT("AllianceRankingOverviewBoundary"));
}

bool URokAllianceRankingReadonlyAuditSubsystem::Ready(int32 ExpectedPage, bool bRanking,
    int32 ExpectedCategory, bool bClosed) const
{
    if (!Identity() || !InputBoundary() || !Controller.IsValid() || !Hud.IsValid()
        || !Screen.IsValid() || !Runtime.IsValid() || !Server.IsValid() || !Server->IsLocalOnlyRun()
        || !Modal.IsValid() || Modal->IsModalOpen()
        || Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary
        || Server->GetHttpDispatchAttemptCount() != BaselineHttpDispatches
        || Server->GetSuppressedLocalRequestCount() != BaselineSuppressedRequests) return false;
    if (bClosed) return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    return EvidenceState(ExpectedPage, bRanking, ExpectedCategory);
}

TSharedRef<FJsonObject> URokAllianceRankingReadonlyAuditSubsystem::State() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetBoolField(TEXT("owned_null_offscreen_input_boundary"), InputBoundary());
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("local_only"), Server.IsValid() && Server->IsLocalOnlyRun());
    if (Screen.IsValid())
    {
        Json->SetNumberField(TEXT("mode"), static_cast<int32>(Screen->ActiveMode));
        Json->SetNumberField(TEXT("alliance_preview_state"), Screen->AlliancePreviewState);
        Json->SetBoolField(TEXT("local_fixture"), Screen->bAllianceTestFixtureState);
        Json->SetNumberField(TEXT("selected_alliance_page"), Screen->SelectedAlliancePage);
        Json->SetBoolField(TEXT("ranking_open"), Screen->bAllianceRankingReferenceOpen);
        Json->SetNumberField(TEXT("ranking_category_id"), Screen->SelectedAllianceRankingCategoryId);
        Json->SetNumberField(TEXT("pending_action_id"), Screen->GetPendingActionIdForAudit());
        TArray<TSharedPtr<FJsonValue>> Widgets;
        for (const TCHAR* Name : {TEXT("FeatureFrameTitle"), TEXT("AlliancePageHit_2"), TEXT("AllianceSettingHit_2"),
            TEXT("AllianceRankingContentShade"), TEXT("AllianceRankingOverviewBoundary"), TEXT("AllianceRankingBack"),
            TEXT("AllianceRankingDetailTitle"), TEXT("AllianceRankingEmptyListPanel"), TEXT("AllianceRankingEmptyList"),
            TEXT("AllianceRankingNoSelfData"), TEXT("FeatureFrameClose"), TEXT("FeatureBoundaryFeedback")})
            Widgets.Add(MakeShared<FJsonValueObject>(WidgetState(Screen->GetWidgetFromName(Name))));
        for (int32 Id = 300; Id <= 305; ++Id)
            for (const TCHAR* Prefix : {TEXT("AllianceRankingCategoryPanel"), TEXT("AllianceRankingCategoryName"),
                TEXT("AllianceRankingCategoryIcon"), TEXT("AllianceRankingCategoryEmpty"), TEXT("AllianceRankingCategoryHit")})
                Widgets.Add(MakeShared<FJsonValueObject>(WidgetState(Screen->GetWidgetFromName(
                    *FString::Printf(TEXT("%s_%d"), Prefix, Id)))));
        Json->SetArrayField(TEXT("widgets"), Widgets);
    }
    return Json;
}
FString URokAllianceRankingReadonlyAuditSubsystem::ComparableState() const
{
    TSharedRef<FJsonObject> Snapshot = State();
    Snapshot->RemoveField(TEXT("frame"));
    return Encode(Snapshot);
}

bool URokAllianceRankingReadonlyAuditSubsystem::Click(UButton* Target, int32 ActionId, const TCHAR* Label)
{
    if (!Expect(Target && Visible(Target) && Target->GetIsEnabled() && InputBoundary(),
        TEXT("Ranking Slate target unavailable or offscreen boundary lost"))) return false;
    URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
    if (!Expect(Indexed != nullptr, TEXT("Ranking target must be an indexed button"))) return false;
    const FGeometry Geometry = Target->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Ranking target has zero Slate geometry"))) return false;
    if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
        || !StableBottomRight.Equals(BottomRight, .25f))
    {
        StableTarget = Target; StableTopLeft = TopLeft; StableBottomRight = BottomRight;
        StableTargetFrame = GFrameCounter; ClickStableFrames = 1;
        return false;
    }
    if (GFrameCounter <= StableTargetFrame) return false;
    if (GFrameCounter != StableTargetFrame + 1)
    {
        StableTargetFrame = GFrameCounter; ClickStableFrames = 1;
        return false;
    }
    StableTargetFrame = GFrameCounter;
    if (++ClickStableFrames < 3) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30,-30));
    const int32 UserIndex = VirtualUser->GetUserIndex();
    const FString ForegroundBefore = ForegroundIdentity();
    const TSharedPtr<SWidget> FocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT CursorBefore = {};
    const bool bCursorBefore = ::GetCursorPos(&CursorBefore) != 0;
#endif
    App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Away, Away, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    App.ProcessMouseMoveEvent(FPointerEvent(UserIndex, 0, Center, Away, TSet<FKey>(),
        EKeys::Invalid, 0, FModifierKeysState()), true);
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, UserIndex);
    const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
    int32 ButtonIndex = INDEX_NONE;
    bool bUmgPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    for (int32 I = 0; Path.IsValid() && I < Path.Widgets.Num(); ++I)
    {
        const FString Type = Path.Widgets[I].Widget->GetTypeAsString();
        PathTypes.Add(MakeShared<FJsonValueString>(Type));
        bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
        if (&Path.Widgets[I].Widget.Get() == &ButtonSlate.Get()) ButtonIndex = I;
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwnWindow = Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get();
    const bool bExact = ButtonIndex != INDEX_NONE
        && Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetStringField(TEXT("method"), TEXT("actual_slate_mouse_down_up"));
    Receipt->SetStringField(TEXT("label"), Label);
    Receipt->SetNumberField(TEXT("action_id"), ActionId);
    Receipt->SetStringField(TEXT("target"), Target->GetName());
    Receipt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Receipt->SetObjectField(TEXT("absolute_center"), Point(Center));
    Receipt->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
    Receipt->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
    Receipt->SetNumberField(TEXT("stable_target_frames"), ClickStableFrames);
    Receipt->SetNumberField(TEXT("stable_target_first_frame"), static_cast<double>(GFrameCounter - ClickStableFrames + 1));
    Receipt->SetNumberField(TEXT("minimum_stable_target_frames_required"), 3);
    Receipt->SetArrayField(TEXT("slate_path_types"), PathTypes);
    Receipt->SetBoolField(TEXT("exact_button_in_path"), bExact);
    Receipt->SetBoolField(TEXT("umg_path"), bUmgPath);
    Receipt->SetBoolField(TEXT("owned_game_window"), bOwnWindow);
    Receipt->SetBoolField(TEXT("owned_null_offscreen_input_boundary"), InputBoundary());
    Receipt->SetObjectField(TEXT("before"), State());
    if (!Expect(bExact && bUmgPath && bOwnWindow && InputBoundary() && Target->IsHovered(),
        TEXT("Ranking button not proven in owned Null offscreen Slate path"))) return false;
    const uint64 PrimaryBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    int32 CallbackCount = 0;
    int32 ActualActionId = INDEX_NONE;
    const FDelegateHandle ObservationHandle = Indexed->OnIndexedClicked.AddLambda(
        [&CallbackCount, &ActualActionId](int32 ObservedAction)
        {
            ++CallbackCount;
            ActualActionId = ObservedAction;
        });
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(UserIndex, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(UserIndex, 0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    Indexed->OnIndexedClicked.Remove(ObservationHandle);
    const FString ForegroundAfter = ForegroundIdentity();
    const TSharedPtr<SWidget> FocusAfter = App.GetUserFocusedWidget(0);
    bool bCursorUnchanged = false;
#if PLATFORM_WINDOWS
    POINT CursorAfter = {};
    bCursorUnchanged = bCursorBefore && ::GetCursorPos(&CursorAfter) != 0
        && CursorBefore.x == CursorAfter.x && CursorBefore.y == CursorAfter.y;
#endif
    const bool bInputReadOnly = ForegroundBefore == ForegroundAfter
        && ForegroundAfter == ForegroundAtBegin && FocusBefore == FocusAfter
        && FocusAfter == UserZeroFocus.Pin() && bCursorUnchanged;
    const uint64 PrimaryAfter = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Receipt->SetBoolField(TEXT("down_handled"), Down);
    Receipt->SetBoolField(TEXT("up_handled"), Up);
    Receipt->SetNumberField(TEXT("virtual_user_index"), UserIndex);
    Receipt->SetNumberField(TEXT("indexed_callback_count"), CallbackCount);
    Receipt->SetNumberField(TEXT("actual_action_id"), ActualActionId);
    Receipt->SetBoolField(TEXT("action_identity_matched"), CallbackCount == 1 && ActualActionId == ActionId);
    Receipt->SetBoolField(TEXT("os_foreground_cursor_user0_focus_unchanged"), bInputReadOnly);
    Receipt->SetNumberField(TEXT("primary_before"), static_cast<double>(PrimaryBefore));
    Receipt->SetNumberField(TEXT("primary_after"), static_cast<double>(PrimaryAfter));
    Receipt->SetNumberField(TEXT("pending_action_after"), Screen->GetPendingActionIdForAudit());
    Receipt->SetObjectField(TEXT("after_input"), State());
    Actions.Add(MakeShared<FJsonValueObject>(Receipt));
    StableTarget.Reset(); ClickStableFrames = 0;
    if (!Expect(Down && Up && bInputReadOnly && CallbackCount == 1 && ActualActionId == ActionId
        && InputBoundary() && PrimaryBefore == PrimaryAfter && PrimaryAfter == BaselinePrimary
        && Screen->GetPendingActionIdForAudit() == ActionId,
        TEXT("Ranking Slate click did not dispatch deferred local action safely"))) return false;
    EffectFrame = GFrameCounter;
    ++SlateClicks;
    return true;
}

bool URokAllianceRankingReadonlyAuditSubsystem::InjectLocalNegative(int32 ActionId, const TCHAR* Label)
{
    if (!Expect(Screen.IsValid() && Screen->bAllianceTestFixtureState && Server.IsValid()
        && Server->IsLocalOnlyRun() && InputBoundary()
        && Screen->GetPendingActionIdForAudit() == INDEX_NONE,
        TEXT("Local negative injection requires settled anonymous fixture"))) return false;
    const int32 BeforePage = Screen->SelectedAlliancePage;
    const bool BeforeOpen = Screen->bAllianceRankingReferenceOpen;
    const int32 BeforeCategory = Screen->SelectedAllianceRankingCategoryId;
    const uint64 PrimaryBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetStringField(TEXT("method"), TEXT("local_action_entry_injection_not_slate"));
    Receipt->SetStringField(TEXT("label"), Label);
    Receipt->SetNumberField(TEXT("action_id"), ActionId);
    Receipt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Receipt->SetObjectField(TEXT("before"), State());
    Screen->ExecuteIndexedAction(ActionId);
    Receipt->SetObjectField(TEXT("after"), State());
    const bool bUnchanged = Screen->IsScreenOpen() && Screen->SelectedAlliancePage == BeforePage
        && Screen->bAllianceRankingReferenceOpen == BeforeOpen
        && Screen->SelectedAllianceRankingCategoryId == BeforeCategory
        && Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == PrimaryBefore;
    const UTextBlock* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
    const bool bFeedback = ActionId != 672 || (Feedback && Visible(Feedback)
        && Feedback->GetText().ToString() == TEXT("该联盟功能暂未开放。"));
    Receipt->SetBoolField(TEXT("state_unchanged"), bUnchanged);
    Receipt->SetBoolField(TEXT("expected_boundary_feedback"), bFeedback);
    Injections.Add(MakeShared<FJsonValueObject>(Receipt));
    ++LocalInjections;
    return Expect(bUnchanged && bFeedback, TEXT("Rejected ranking local action mutated state"));
}

void URokAllianceRankingReadonlyAuditSubsystem::Capture(const FString& Phase, int32 After)
{
    if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
        && bBaselineSet && InputBoundary(), TEXT("Ranking screenshot request/offscreen boundary invalid"))) return;
    Pending = MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("phase"), Phase);
    Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
    Pending->SetObjectField(TEXT("requested_state"), State());
    PendingComparableState = ComparableState();
    bPendingExpectedClosed = !Screen.IsValid() || !Screen->IsScreenOpen();
    PendingExpectedPage = Screen.IsValid() ? Screen->SelectedAlliancePage : 0;
    bPendingExpectedRanking = Screen.IsValid() && Screen->bAllianceRankingReferenceOpen;
    PendingExpectedCategory = Screen.IsValid() ? Screen->SelectedAllianceRankingCategoryId : INDEX_NONE;
    PendingPath = Directory / (Phase + TEXT(".png"));
    Pending->SetStringField(TEXT("png"), PendingPath);
    NextStep = After; bScreenshotDone = false;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokAllianceRankingReadonlyAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
    TArray<uint8> Bytes;
    if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
        && FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
        && FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
        && PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
        && GFrameCounter >= static_cast<uint64>(Pending->GetNumberField(TEXT("requested_frame")))
        && Ready(PendingExpectedPage, bPendingExpectedRanking, PendingExpectedCategory, bPendingExpectedClosed)
        && ComparableState() == PendingComparableState,
        TEXT("Ranking screenshot PNG/extent/window/state invalid"))) return;
    Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetNumberField(TEXT("png_bytes"), Bytes.Num());
    Pending->SetNumberField(TEXT("png_width"), 1440);
    Pending->SetNumberField(TEXT("png_height"), 810);
    Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
    Pending->SetObjectField(TEXT("processed_state"), State());
    ScreenshotFrame = GFrameCounter; bScreenshotDone = true;
}
void URokAllianceRankingReadonlyAuditSubsystem::AdvanceCapture()
{
    if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
    if (!Expect(Ready(PendingExpectedPage, bPendingExpectedRanking, PendingExpectedCategory, bPendingExpectedClosed)
        && ComparableState() == PendingComparableState,
        TEXT("Ranking screenshot next-frame state/geometry drift"))) return;
    Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
    Pending->SetObjectField(TEXT("next_frame_state"), State());
    Captures.Add(MakeShared<FJsonValueObject>(Pending));
    Pending.Reset();
    if (!Save(false)) { Finish(TEXT("Ranking in-progress manifest write failed")); return; }
    Enter(NextStep);
}
void URokAllianceRankingReadonlyAuditSubsystem::Enter(int32 NewStep)
{
    Step = NewStep; StepSince = Elapsed; StableFrames = 0; StableTarget.Reset(); ClickStableFrames = 0;
}
bool URokAllianceRankingReadonlyAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    const bool Passed = Complete && Failures.IsEmpty() && SlateClicks == 23 && Actions.Num() == 23
        && LocalInjections == 2 && Injections.Num() == 2 && Captures.Num() == 23
        && CategoryCursor == 6 && bInWrongContext && bInvalidActionTested && bReopened
        && Ready(2, true, INDEX_NONE);
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_ranking_readonly.local_runtime_audit.v5"));
    Json->SetStringField(TEXT("scope"), TEXT("local_ranking_navigation_audit"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Passed);
    Json->SetBoolField(TEXT("global_request_suppression_verified"), false);
    Json->SetStringField(TEXT("global_request_suppression_status"), TEXT("PENDING_DISPATCHER_OR_TRACE_PROOF"));
    Json->SetBoolField(TEXT("ranking_data_or_visual_parity_accepted"), false);
    Json->SetBoolField(TEXT("live_product_acceptance"), false);
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetBoolField(TEXT("begin_boundary_observed"), bBeginBoundaryObserved);
    Json->SetBoolField(TEXT("begin_own_null_offscreen_window"), bBeginOwnOffscreenWindow);
    Json->SetObjectField(TEXT("begin_physical_viewport"), Point(BeginPhysicalViewport));
    Json->SetObjectField(TEXT("begin_widget_viewport"), Point(BeginWidgetViewport));
    Json->SetNumberField(TEXT("begin_viewport_dpi_scale"), BeginViewportDpiScale);
    Json->SetNumberField(TEXT("begin_expected_viewport_dpi_scale"), BeginExpectedDpiScale);
    Json->SetNumberField(TEXT("viewport_dpi_scale"), UWidgetLayoutLibrary::GetViewportScale(this));
    Json->SetNumberField(TEXT("expected_viewport_dpi_scale"),
        GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(1440, 810)));
    Json->SetStringField(TEXT("output_directory"), Directory);
    Json->SetStringField(TEXT("entry_route"), TEXT("-RokOpenModal=alliance_joined; OpenAlliance(true) local fixture"));
    Json->SetNumberField(TEXT("slate_click_count"), SlateClicks);
    Json->SetNumberField(TEXT("virtual_user_index"), VirtualUser.IsValid() ? VirtualUser->GetUserIndex() : INDEX_NONE);
    Json->SetStringField(TEXT("external_foreground_at_begin"), ForegroundAtBegin);
    Json->SetStringField(TEXT("external_foreground_at_end"), ForegroundIdentity());
    Json->SetBoolField(TEXT("external_foreground_unchanged"), ForegroundIdentity() == ForegroundAtBegin);
    Json->SetBoolField(TEXT("os_hit_audit"), false);
    Json->SetNumberField(TEXT("local_action_negative_injections"), LocalInjections);
    Json->SetBoolField(TEXT("local_server_available"), Server.IsValid());
    Json->SetNumberField(TEXT("local_server_http_dispatch_attempts_observed"),
        Server.IsValid() ? static_cast<double>(Server->GetHttpDispatchAttemptCount()) : -1.0);
    Json->SetObjectField(TEXT("catalog_atlas_diagnostic"), CatalogAtlasDiagnostic.IsValid() ? CatalogAtlasDiagnostic.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetObjectField(TEXT("input_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("slate_actions"), Actions);
    Json->SetArrayField(TEXT("local_negative_injections"), Injections);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("final_state"), State());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokAllianceRankingReadonlyAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    bool Complete = Failures.IsEmpty() && SlateClicks == 23 && Actions.Num() == 23
        && LocalInjections == 2 && Injections.Num() == 2 && Captures.Num() == 23
        && CategoryCursor == 6 && bInWrongContext && bInvalidActionTested && bReopened
        && Ready(2, true, INDEX_NONE);
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Ranking local audit incomplete"));
    if (!Save(Complete))
    {
        Complete = false;
        Failures.Add(TEXT("Ranking local audit manifest write failed"));
        Save(false);
        UE_LOG(LogTemp, Error, TEXT("Ranking local audit manifest save failed: %s"), *Directory);
    }
    if (Complete)
    {
        UE_LOG(LogTemp, Display, TEXT("Ranking local audit passed: %s"), *Directory);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Ranking local audit failed: %s"), *Directory);
    }
    VirtualUser.Reset();
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}

void URokAllianceRankingReadonlyAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return;
    Elapsed += FMath::Max(0.0f, DeltaTime);
    if (Elapsed < 2.0) return;
    if (Elapsed > 480.0) { Finish(TEXT("Ranking audit global timeout")); return; }
    if (!Identity()) { Finish(TEXT("Ranking audit world identity changed")); return; }
    if (Pending.IsValid())
    {
        if (Elapsed - StepSince > 30.0) Finish(TEXT("Ranking screenshot callback timeout"));
        else AdvanceCapture();
        return;
    }
    if (Elapsed - StepSince > 35.0) { Finish(TEXT("Ranking audit step timeout")); return; }
    auto Stable = [this](int32 Page, bool Ranking, int32 Category, bool Closed = false)
    {
        StableFrames = Ready(Page, Ranking, Category, Closed) ? StableFrames + 1 : 0;
        return StableFrames >= 3 && GFrameCounter > EffectFrame;
    };
    auto Press = [this](const FString& Name, int32 Action, const TCHAR* Label)
    {
        return Click(Cast<UButton>(Screen->GetWidgetFromName(*Name)), Action, Label);
    };
    switch (Step)
    {
    case 0:
        if (!bBegun && AwaitAtlasReady()) break;
        if (!bBegun) bBegun = Begin();
        if (bBegun) Enter(1);
        break;
    case 1: if (Stable(0,false,INDEX_NONE)) Capture(TEXT("00_joined_fixture"),2); break;
    case 2: if (Press(TEXT("AlliancePageHit_2"),632,TEXT("settings_page_632"))) Enter(3); break;
    case 3: if (Stable(2,false,INDEX_NONE)) Capture(TEXT("01_settings_632"),4); break;
    case 4: if (Press(TEXT("AlliancePageHit_0"),630,TEXT("wrong_context_setup"))) Enter(5); break;
    case 5:
        if (Stable(0,false,INDEX_NONE) && InjectLocalNegative(672,TEXT("672_wrong_context")))
        { bInWrongContext = true; Capture(TEXT("02_wrong_context_672"),6); }
        break;
    case 6: if (Press(TEXT("AlliancePageHit_2"),632,TEXT("restore_settings_632"))) Enter(7); break;
    case 7: if (Stable(2,false,INDEX_NONE) && Press(TEXT("AllianceSettingHit_2"),672,TEXT("open_ranking_672"))) Enter(8); break;
    case 8: if (Stable(2,true,INDEX_NONE)) Capture(TEXT("03_ranking_overview"),9); break;
    case 9:
        if (Stable(2,true,INDEX_NONE) && InjectLocalNegative(19706,TEXT("invalid_category_19706")))
        { bInvalidActionTested = true; Capture(TEXT("04_invalid_19706"),10); }
        break;
    case 10:
        if (CategoryCursor < 6)
        {
            if (Press(FString::Printf(TEXT("AllianceRankingCategoryHit_%d"),300+CategoryCursor),
                19700+CategoryCursor,TEXT("open_category"))) Enter(11);
        }
        else Enter(15);
        break;
    case 11:
        if (Stable(2,true,300+CategoryCursor))
            Capture(FString::Printf(TEXT("detail_%d"),300+CategoryCursor),12);
        break;
    case 12: if (Press(TEXT("AllianceRankingBack"),19710,TEXT("detail_back_overview"))) Enter(13); break;
    case 13:
        if (Stable(2,true,INDEX_NONE))
            Capture(FString::Printf(TEXT("return_overview_%d"),300+CategoryCursor),14);
        break;
    case 14: ++CategoryCursor; Enter(10); break;
    case 15: if (Press(TEXT("AllianceRankingCategoryHit_300"),19700,TEXT("action1_detail_setup"))) Enter(16); break;
    case 16: if (Stable(2,true,300)) Capture(TEXT("action1_detail_before"),17); break;
    case 17: if (Press(TEXT("FeatureFrameClose"),1,TEXT("action1_detail_to_overview"))) Enter(18); break;
    case 18: if (Stable(2,true,INDEX_NONE)) Capture(TEXT("action1_overview_after"),19); break;
    case 19: if (Press(TEXT("AllianceRankingBack"),19710,TEXT("overview_back_settings"))) Enter(20); break;
    case 20: if (Stable(2,false,INDEX_NONE)) Capture(TEXT("back_to_settings"),21); break;
    case 21: if (Press(TEXT("AllianceSettingHit_2"),672,TEXT("reopen_overview_for_close"))) Enter(22); break;
    case 22:
        if (Stable(2,true,INDEX_NONE)
            && Press(TEXT("FeatureFrameClose"),1,TEXT("overview_action1_close"))) Enter(23);
        break;
    case 23: if (Stable(2,false,INDEX_NONE,true)) Capture(TEXT("overview_closed"),24); break;
    case 24:
        if (!Expect(Screen.IsValid() && !Screen->IsScreenOpen() && InputBoundary(),
            TEXT("Ranking reopen requires settled closed screen"))) return;
        Screen->OpenAlliance(true); bReopened = true; EffectFrame = GFrameCounter; Enter(25); break;
    case 25:
        if (Stable(0,false,INDEX_NONE)
            && Press(TEXT("AlliancePageHit_2"),632,TEXT("reopened_settings_632"))) Enter(26);
        break;
    case 26: if (Stable(2,false,INDEX_NONE)) Capture(TEXT("reopened_settings_reset"),27); break;
    case 27: if (Press(TEXT("AllianceSettingHit_2"),672,TEXT("reopened_ranking_672"))) Enter(28); break;
    case 28: if (Stable(2,true,INDEX_NONE)) Capture(TEXT("reopened_overview_reset"),29); break;
    case 29: Finish(); break;
    default: Finish(TEXT("Invalid ranking audit step")); break;
    }
}
