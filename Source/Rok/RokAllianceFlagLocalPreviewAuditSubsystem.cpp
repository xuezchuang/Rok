#include "RokAllianceFlagLocalPreviewAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokAllianceFlagLocalPreviewData.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif




namespace
{
struct FUiSample
{
    int32 DirectChildren = 0;
    int32 UnsettledChildren = 0;
    int32 RemainingTextureCompilations = -1;
    int32 TextureCount = 0;
    bool bTextureRhiReady = false;
    bool bReady = false;
};
FUiSample SampleUi(const URokUnityFeatureScreenWidget* Screen)
{
    FUiSample Result;
    if (!Screen || !Screen->IsScreenOpen()) return Result;
    const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
    if (!Canvas || !Screen->GetWidgetFromName(TEXT("FeatureFrameClose"))) return Result;
    for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
    {
        const UWidget* Child = Canvas->GetChildAt(Index);
        if (!Child) return Result;
        ++Result.DirectChildren;
        if (Child->GetRenderOpacity() < .995f
            || !Child->GetRenderTransform().Translation.IsNearlyZero(.25f)) ++Result.UnsettledChildren;
    }
#if WITH_EDITOR
    Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
    static const TCHAR* Paths[] = {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.rok_fullscreen_alliance_flag")};
    Result.bTextureRhiReady = true;
    for (const TCHAR* Asset : Paths)
    {
        UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Asset);
        const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
        if (!Resource || !Resource->TextureRHI.IsValid()) Result.bTextureRhiReady = false;
        else ++Result.TextureCount;
    }
    Result.bReady = Result.DirectChildren > 5 && Result.UnsettledChildren == 0
        && Result.RemainingTextureCompilations == 0 && Result.bTextureRhiReady
        && Result.TextureCount == UE_ARRAY_COUNT(Paths);
    return Result;
}
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceFlagLocalPreviewAudit");
constexpr TCHAR CreateNameText[] = TEXT("FlagAuditCity");
constexpr TCHAR CreateTagText[] = TEXT("FAG");
constexpr TCHAR CreateNoticeText[] = TEXT("Local preview only");
struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
constexpr FInputPin Pinned[] = {
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("55ede9000d689b2bcd23979f1db9081b0d06ae79881511a878623c2a95ce00a4")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("7819bfe968c28308c2d88974b2bca9f742039ae38885d7603d20f0859f0a76cd")},
    {TEXT("Source/Rok/RokAllianceFlagLocalPreviewData.cpp"), TEXT("488d145991bb1ccbbe8fc406f81ebd6bd12629ee9ed594824df7382ca7a419ba")},
    {TEXT("Source/Rok/RokAllianceFlagLocalPreviewData.h"), TEXT("face9b9d15a39faef0528c0eedd113b806e2ccf238ab3dd822d69c0c5ea8106d")},
    {TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c")},
    {TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
    {TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
    {TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
    {TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
    {TEXT("Saved/PendingAllianceFlagLocalPreviewV1/source_manifest.json"), TEXT("d77d3446efc0b6174827ab4181752e5cda186016de15df6ad4bb409e02b4e11d")},
    {TEXT("ref/config-tables/编译客户端配置/CSV/AllianceSign.csv"), TEXT("334722d36c72c9acbd233106c53107a280345e00d3f23fb5c2a6739f09f4cb9e")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/alliance_flag.tpsheet"), TEXT("0c282be7c25e5f71900964c7951f1dc249d47cac244b98a945d9fd285f92a49f")},
    {TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_alliance/alliance_flag.png"), TEXT("aeaf8c1359c1bd69866e32ec4dc93545689010719d539eb338f30b95df816222")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildFlagSettingMediator.cs"), TEXT("1d0e607ea2f017cadc49a2646bf13f3291d6a1df46b16d3655b16f5ce45aad3b")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Model_GuildFlag_SubView.cs"), TEXT("499b74e7afc0c19dac0574b874f9499469e701f3d8c864150181d156bbd6710d")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/AllianceProxy.cs"), TEXT("67c0dea2cb390479f764a902ef4c903f8fd14ca8898c43662d61bc6422d66dbc")},
};
bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		&& FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
		&& FEngineVersion::Current().GetMajor() == 5
		&& FEngineVersion::Current().GetMinor() == 8;
}
FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
TSharedRef<FJsonObject> Color(const FLinearColor& Value)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("r"), Value.R); Json->SetNumberField(TEXT("g"), Value.G);
    Json->SetNumberField(TEXT("b"), Value.B); Json->SetNumberField(TEXT("a"), Value.A);
    return Json;
}
TSharedRef<FJsonObject> FlagRectJson(const FIntRect& Value)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("min_x"), Value.Min.X); Json->SetNumberField(TEXT("min_y"), Value.Min.Y);
    Json->SetNumberField(TEXT("max_x"), Value.Max.X); Json->SetNumberField(TEXT("max_y"), Value.Max.Y);
    return Json;
}
const RokAllianceFlagLocalPreview::FEntry* EntryAt(int32 Type, int32 Index)
{
    int32 Current = 0;
    for (const auto& Entry : RokAllianceFlagLocalPreview::Rows())
        if (Entry.Type == Type && Current++ == Index) return &Entry;
    return nullptr;
}
int32 CountAt(int32 Type)
{
    int32 Count = 0;
    for (const auto& Entry : RokAllianceFlagLocalPreview::Rows()) if (Entry.Type == Type) ++Count;
    return Count;
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
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
	Json->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
	Json->SetNumberField(TEXT("opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Json->SetObjectField(TEXT("scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
	if (const UEditableTextBox* Input = Cast<UEditableTextBox>(Widget)) Json->SetStringField(TEXT("text"), Input->GetText().ToString());
	if (const UBorder* Border = Cast<UBorder>(Widget)) Json->SetObjectField(TEXT("brush_color"), Color(Border->GetBrushColor()));
	if (const UImage* Image = Cast<UImage>(Widget))
	{
		const FSlateBrush& Brush = Image->GetBrush();
		const FBox2f Uv = static_cast<FBox2f>(Brush.GetUVRegion());
		Json->SetObjectField(TEXT("color"), Color(Image->GetColorAndOpacity()));
		Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(Uv.Min.X, Uv.Min.Y)));
		Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(Uv.Max.X, Uv.Max.Y)));
        Json->SetObjectField(TEXT("brush_image_size"), Point(FVector2D(Brush.ImageSize.X, Brush.ImageSize.Y)));
		Json->SetStringField(TEXT("texture"), Brush.GetResourceObject() ? Brush.GetResourceObject()->GetPathName() : FString());
	}
	return Json;
}
bool HasTag(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName& Tag : Actor->Tags) if (Tag.ToString().StartsWith(Prefix)) return true;
	return false;
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
	return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
		| (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
}




bool URokAllianceFlagLocalPreviewAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer))
        && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceFlagLocalPreviewAudit"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceFlagLocalPreviewAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game; }
TStatId URokAllianceFlagLocalPreviewAuditSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceFlagLocalPreviewAuditSubsystem, STATGROUP_Tickables); }
void URokAllianceFlagLocalPreviewAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingAllianceFlagLocalPreviewRuntimeAuditV2/RuntimeAudit")
        / FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this, &URokAllianceFlagLocalPreviewAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceFlagLocalPreviewAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    Super::Deinitialize();
}
bool URokAllianceFlagLocalPreviewAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceFlagLocalPreviewAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
    if (!Condition) Finish(Reason);
    return Condition;
}


bool URokAllianceFlagLocalPreviewAuditSubsystem::PrepareOwnWindowFocus()
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	RECT Client = {};
	if (!Expect(Handle && ::IsWindow(Handle) && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810, TEXT("Audit requires own 1440x810 GameApp window"))) return false;
	if (::GetForegroundWindow() == Handle) return true;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	const HWND Foreground = ::GetForegroundWindow();
	const DWORD ForegroundThread = Foreground ? ::GetWindowThreadProcessId(Foreground, nullptr) : 0;
	const DWORD CurrentThread = ::GetCurrentThreadId();
	const bool bAttached = ForegroundThread && ForegroundThread != CurrentThread
		&& ::AttachThreadInput(CurrentThread, ForegroundThread, 1) != 0;
	::BringWindowToTop(Handle);
	::SetForegroundWindow(Handle);
	if (bAttached) ::AttachThreadInput(CurrentThread, ForegroundThread, 0);
	return ::GetForegroundWindow() == Handle;
#else
	return Expect(false, TEXT("Audit requires Windows game window"));
#endif
}


bool URokAllianceFlagLocalPreviewAuditSubsystem::Begin()
{
    if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong world/project or Slate unavailable"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token;
    int32 AuditFlags = 0, LocalFlags = 0, FixtureFlags = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
        if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
        if (Token.Equals(TEXT("-RokAlliancePreviewFixture"), ESearchCase::IgnoreCase)) { ++FixtureFlags; continue; }
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
            return Expect(false, TEXT("Unexpected Rok fixture/diagnostic flag"));
    }
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && FixtureFlags == 0,
        TEXT("Exactly one audit/local flag and no search fixture flag required"))) return false;
    Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
    Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
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
    if (!Expect(Controller.IsValid() && Controller->GetPawn() && Buildings.IsValid()
        && Runtime.IsValid() && Runtime->IsRuntimeStateLoaded() && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
        && Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Screen->IsScreenOpen()
        && !Modal->IsModalOpen() && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
        TEXT("Expected idle local-only 1440x810 GameApp HUD"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
        && !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
        TEXT("Expected anonymous local-preview account boundary"))) return false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("UnityInitId_5")))
        {
            if (Hall.IsValid()) return Expect(false, TEXT("Ambiguous UnityInitId_5 Hall"));
            Hall = *It;
        }
    if (!Expect(Hall.IsValid() && Hall->ActorHasTag(TEXT("UnityBuildingType_1"))
        && Hall->GetWorld() == GetWorld(), TEXT("Source Hall identity missing"))) return false;
    for (const TCHAR* Asset : {
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
        TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame"),
        TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance"),
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.rok_fullscreen_alliance_flag")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
            TEXT("Required Alliance texture failed to load"))) return false;
    OriginalPointer = FSlateApplication::Get().GetCursorPos();
    LastPointer = OriginalPointer;
    bPointerSaved = true;
    Inputs = MakeShared<FJsonObject>();
    for (const FInputPin& Pin : Pinned)
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Path : {TEXT("Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceFlagLocalPreviewAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokAllianceFlagLocalPreviewRuntime.py")})
    {
        const FString Actual = FileSha(FPaths::ProjectDir() / Path);
        if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
        this, &URokAllianceFlagLocalPreviewAuditSubsystem::OnUpgradeCompleted);
    int32 Counts[4] = {0, 0, 0, 0};
    TSet<int32> Ids;
    for (const auto& Entry : RokAllianceFlagLocalPreview::Rows())
    {
        if (!Expect(Entry.Type >= 1 && Entry.Type <= 4 && !Ids.Contains(Entry.Id),
            TEXT("Flag config type or ID collision"))) return false;
        ++Counts[Entry.Type - 1]; Ids.Add(Entry.Id);
    }
    if (!Expect(Ids.Num() == 63 && Counts[0] == 16 && Counts[1] == 16
        && Counts[2] == 24 && Counts[3] == 7
        && RokAllianceFlagLocalPreview::IsValidDraft(RokAllianceFlagLocalPreview::FDraft()),
        TEXT("Flag source rows/default identity mismatch"))) return false;
    return true;
}


TSharedRef<FJsonObject> URokAllianceFlagLocalPreviewAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Buildings.IsValid() || !Server.IsValid() || !Hall.IsValid()) return Json;
	FRokBuildingState HallState;
	const bool bCached = Buildings->TryPeekExistingBuildingStateForActor(Hall.Get(), HallState);
	Json->SetBoolField(TEXT("hall_cached_exact_binding"), bCached);
	Json->SetStringField(TEXT("hall_actor_path"), Hall->GetPathName());
	Json->SetStringField(TEXT("hall_actor_class"), Hall->GetClass()->GetPathName());
	TArray<FString> Tags;
	for (const FName& Tag : Hall->Tags) Tags.Add(Tag.ToString());
	Tags.Sort();
	TArray<TSharedPtr<FJsonValue>> TagValues;
	for (const FString& Tag : Tags) TagValues.Add(MakeShared<FJsonValueString>(Tag));
	Json->SetArrayField(TEXT("hall_tags"), TagValues);
	Json->SetStringField(TEXT("hall_key"), HallState.BuildingKey.ToString());
	Json->SetStringField(TEXT("hall_display_name"), HallState.DisplayName.ToString());
	Json->SetNumberField(TEXT("hall_type"), static_cast<int32>(HallState.Type));
	Json->SetNumberField(TEXT("hall_init_id"), HallState.UnityInitId);
	Json->SetNumberField(TEXT("hall_source_type"), HallState.UnityBuildingTypeId);
	Json->SetNumberField(TEXT("hall_grid_x"), HallState.UnityGridX);
	Json->SetNumberField(TEXT("hall_grid_y"), HallState.UnityGridY);
	Json->SetNumberField(TEXT("hall_level"), HallState.Level);
	Json->SetNumberField(TEXT("hall_power"), HallState.Power);
	Json->SetNumberField(TEXT("hall_produce_per_hour"), HallState.ProducePerHour);
	Json->SetNumberField(TEXT("hall_gather_min"), HallState.GatherMin);
	Json->SetNumberField(TEXT("hall_gather_max"), HallState.GatherMax);
	Json->SetBoolField(TEXT("hall_upgrade_in_progress"), HallState.bUpgradeInProgress);
	Json->SetNumberField(TEXT("hall_upgrade_started_at"), HallState.UpgradeStartedAt);
	Json->SetNumberField(TEXT("hall_upgrade_finishes_at"), HallState.UpgradeFinishesAt);
	Json->SetStringField(TEXT("hall_icon"), HallState.Icon.ToString());
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	const auto Rates = Buildings->GetProductionRates();
	Json->SetNumberField(TEXT("rate_food"), Rates.FoodPerHour);
	Json->SetNumberField(TEXT("rate_wood"), Rates.WoodPerHour);
	Json->SetNumberField(TEXT("rate_stone"), Rates.StonePerHour);
	Json->SetNumberField(TEXT("rate_gold"), Rates.GoldPerHour);
	Json->SetNumberField(TEXT("owned_queues"), Buildings->GetOwnedBuildQueueCount());
	Json->SetNumberField(TEXT("active_upgrades"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("active_construction"), Buildings->GetActiveLocalConstructionCount());
#if WITH_DEV_AUTOMATION_TESTS
	Json->SetNumberField(TEXT("pending_completed_upgrades"), Buildings->GetPendingCompletedUpgradeCountForTesting());
#else
	Json->SetNumberField(TEXT("pending_completed_upgrades"), -1);
#endif
	Json->SetNumberField(TEXT("completed_upgrade_events"), CompletionEvents);
	// IDs are consecutive from one and tickets are never removed. Absence of ID 1
	// plus zero active constructions proves this fresh-city fixture has no ticket.
	FRokLocalConstructionTicket Ticket;
	Json->SetBoolField(TEXT("construction_ticket_1_exists"), Buildings->GetLocalConstructionTicket(1, Ticket));
	TArray<FString> CityActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (HasTag(*It, TEXT("UnityInitId_")) || It->ActorHasTag(TEXT("RokLocalConstructedBuilding")))
			CityActors.Add(It->GetPathName());
	CityActors.Sort();
	TArray<TSharedPtr<FJsonValue>> CityValues;
	for (const FString& Path : CityActors) CityValues.Add(MakeShared<FJsonValueString>(Path));
	Json->SetArrayField(TEXT("city_actor_paths"), CityValues);
	const auto Task = Runtime->GetActiveTask();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_type"), Task.Type);
	Json->SetNumberField(TEXT("task_param1"), Task.Param1);
	Json->SetNumberField(TEXT("task_param2"), Task.Param2);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
	Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
	Json->SetNumberField(TEXT("task_next_id"), Task.NextId);
	Json->SetNumberField(TEXT("task_reward_group"), Task.Reward);
	Json->SetStringField(TEXT("task_progress_rule"), Task.ProgressRule);
	Json->SetStringField(TEXT("task_name"), Task.Name);
	Json->SetStringField(TEXT("task_description"), Task.Desc);
	Json->SetNumberField(TEXT("task_require"), Task.Require);
	Json->SetStringField(TEXT("task_icon"), Task.Icon);
	TArray<TSharedPtr<FJsonValue>> TaskChain;
	for (const auto& Row : Runtime->GetActiveTaskChain())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Row.TaskId);
		Entry->SetStringField(TEXT("name"), Row.Name);
		Entry->SetStringField(TEXT("description"), Row.Desc);
		Entry->SetNumberField(TEXT("type"), Row.Type);
		Entry->SetNumberField(TEXT("param1"), Row.Param1);
		Entry->SetNumberField(TEXT("param2"), Row.Param2);
		Entry->SetNumberField(TEXT("require"), Row.Require);
		Entry->SetNumberField(TEXT("reward"), Row.Reward);
		Entry->SetNumberField(TEXT("next_id"), Row.NextId);
		Entry->SetStringField(TEXT("progress_rule"), Row.ProgressRule);
		Entry->SetNumberField(TEXT("current"), Row.Current);
		Entry->SetNumberField(TEXT("target"), Row.Target);
		Entry->SetBoolField(TEXT("complete"), Row.bComplete);
		Entry->SetStringField(TEXT("icon"), Row.Icon);
		TaskChain.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("task_chain"), TaskChain);
	const auto Profile = Runtime->GetPlayerProfileState();
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetBoolField(TEXT("profile_has_role_id"), Profile.bHasRoleId);
	Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
	Json->SetBoolField(TEXT("profile_has_display_name"), Profile.bHasDisplayName);
	Json->SetStringField(TEXT("profile_display_name"), Profile.DisplayName);
	Json->SetBoolField(TEXT("profile_has_civilization"), Profile.bHasCivilization);
	Json->SetNumberField(TEXT("profile_civilization_id"), Profile.CivilizationId);
	Json->SetStringField(TEXT("profile_civilization_name"), Profile.CivilizationName);
	Json->SetNumberField(TEXT("profile_local_hero_score"), Profile.LocalHeroScore);
	Json->SetNumberField(TEXT("profile_local_troop_power"), Profile.LocalTroopPower);
	Json->SetNumberField(TEXT("profile_local_preview_score"), Profile.LocalPreviewScore);
	Json->SetNumberField(TEXT("profile_local_building_upgrade_score"), Profile.LocalBuildingUpgradeScore);
	TArray<TSharedPtr<FJsonValue>> Heroes;
	for (const auto& Hero : Runtime->GetStarterHeroes())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Hero.HeroId);
		Entry->SetNumberField(TEXT("level"), Hero.Level);
		Entry->SetNumberField(TEXT("star"), Hero.Star);
		Entry->SetNumberField(TEXT("max_star"), Hero.MaxStar);
		Entry->SetNumberField(TEXT("rare"), Hero.Rare);
		Entry->SetNumberField(TEXT("score"), Hero.Score);
		Entry->SetNumberField(TEXT("skill_count"), Hero.SkillCount);
		Entry->SetNumberField(TEXT("talent_count"), Hero.TalentCount);
		Entry->SetStringField(TEXT("icon"), Hero.Icon);
		Entry->SetStringField(TEXT("model"), Hero.Model);
		Heroes.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_heroes"), Heroes);
	TArray<TSharedPtr<FJsonValue>> Troops;
	for (const auto& Troop : Runtime->GetStarterTroopStacks())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Troop.TroopId);
		Entry->SetNumberField(TEXT("arms_type"), Troop.ArmsType);
		Entry->SetNumberField(TEXT("level"), Troop.Level);
		Entry->SetNumberField(TEXT("count"), Troop.Count);
		Entry->SetNumberField(TEXT("power_per_unit"), Troop.PowerPerUnit);
		Entry->SetNumberField(TEXT("power"), Troop.TotalPower);
		Entry->SetNumberField(TEXT("train_seconds"), Troop.TrainSeconds);
		Entry->SetStringField(TEXT("icon"), Troop.Icon);
		Entry->SetStringField(TEXT("model"), Troop.Model);
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	TArray<TSharedPtr<FJsonValue>> Trainable;
	for (const int32 Id : Runtime->GetTrainableTroopIds()) Trainable.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("trainable_troop_ids"), Trainable);
	Json->SetBoolField(TEXT("server_local_only"), Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	return Json;
}

bool URokAllianceFlagLocalPreviewAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}


TSharedRef<FJsonObject> URokAllianceFlagLocalPreviewAuditSubsystem::State() const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetObjectField(TEXT("business"), BusinessState());
    const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    const HWND Foreground = ::GetForegroundWindow();
    DWORD OwnerPid = 0, ForegroundPid = 0;
    RECT Client = {};
    if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
    if (Foreground) ::GetWindowThreadProcessId(Foreground, &ForegroundPid);
    const bool bClient = Handle && ::GetClientRect(Handle, &Client);
    Json->SetBoolField(TEXT("window_foreground"), Handle && Foreground == Handle);
    Json->SetNumberField(TEXT("window_owner_pid"), OwnerPid);
    Json->SetNumberField(TEXT("foreground_owner_pid"), ForegroundPid);
    Json->SetNumberField(TEXT("client_width"), bClient ? Client.right - Client.left : 0);
    Json->SetNumberField(TEXT("client_height"), bClient ? Client.bottom - Client.top : 0);
    Json->SetObjectField(TEXT("viewport_origin"), Point(UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(FVector2D::ZeroVector)));
    Json->SetStringField(TEXT("own_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle)));
    Json->SetStringField(TEXT("foreground_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Foreground)));
#endif
    Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
    const FUiSample Sample = SampleUi(Screen.Get());
    auto Ui = MakeShared<FJsonObject>();
    Ui->SetBoolField(TEXT("ready"), Sample.bReady);
    Ui->SetNumberField(TEXT("direct_children"), Sample.DirectChildren);
    Ui->SetNumberField(TEXT("unsettled_children"), Sample.UnsettledChildren);
    Ui->SetNumberField(TEXT("remaining_texture_compilations"), Sample.RemainingTextureCompilations);
    Ui->SetNumberField(TEXT("texture_count"), Sample.TextureCount);
    Ui->SetBoolField(TEXT("texture_rhi_ready"), Sample.bTextureRhiReady);
    Json->SetObjectField(TEXT("alliance_ui_settled"), Ui);
    const auto Placement = Controller.IsValid() ? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
    Json->SetNumberField(TEXT("primary_press_count"), static_cast<double>(Placement.PrimaryPressEventCount));
    Json->SetStringField(TEXT("selected_actor"), Placement.CurrentSelection.IsValid()
        ? Placement.CurrentSelection->GetPathName() : FString());
    const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
    const FTransform Camera = Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
    const FVector L = Camera.GetLocation(); const FRotator R = Camera.Rotator();
    Json->SetNumberField(TEXT("camera_x"), L.X); Json->SetNumberField(TEXT("camera_y"), L.Y);
    Json->SetNumberField(TEXT("camera_z"), L.Z); Json->SetNumberField(TEXT("camera_pitch"), R.Pitch);
    Json->SetNumberField(TEXT("camera_yaw"), R.Yaw); Json->SetNumberField(TEXT("camera_roll"), R.Roll);
    Json->SetNumberField(TEXT("camera_scale_x"), Camera.GetScale3D().X);
    Json->SetNumberField(TEXT("camera_scale_y"), Camera.GetScale3D().Y);
    Json->SetNumberField(TEXT("camera_scale_z"), Camera.GetScale3D().Z);
Json->SetNumberField(TEXT("active_mode"), Screen.IsValid() ? static_cast<int32>(Screen->ActiveMode) : -1);
    Json->SetNumberField(TEXT("alliance_preview_state"), Screen.IsValid() ? Screen->AlliancePreviewState : -1);
    Json->SetNumberField(TEXT("pending_action_id"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
    Json->SetBoolField(TEXT("flag_editor_open"), Screen.IsValid() && Screen->bAllianceFlagEditorOpen);
    Json->SetNumberField(TEXT("selected_type"), Screen.IsValid() ? Screen->SelectedAllianceFlagType : 0);
    auto Draft = [](const RokAllianceFlagLocalPreview::FDraft& Value)
    {
        auto Row = MakeShared<FJsonObject>();
        for (int32 Type = 1; Type <= 4; ++Type)
            Row->SetNumberField(FString::FromInt(Type), Value.GetId(Type));
        return Row;
    };
    Json->SetObjectField(TEXT("create_draft"), Draft(Screen.IsValid() ? Screen->AllianceCreateFlagDraft : RokAllianceFlagLocalPreview::FDraft()));
    Json->SetObjectField(TEXT("edit_draft"), Draft(Screen.IsValid() ? Screen->AllianceFlagEditDraft : RokAllianceFlagLocalPreview::FDraft()));
    Json->SetStringField(TEXT("create_name_draft"), Screen.IsValid() ? Screen->AllianceCreateNameDraft : FString());
    Json->SetStringField(TEXT("create_tag_draft"), Screen.IsValid() ? Screen->AllianceCreateTagDraft : FString());
    Json->SetStringField(TEXT("create_notice_draft"), Screen.IsValid() ? Screen->AllianceCreateNoticeDraft : FString());
    auto Widgets = MakeShared<FJsonObject>();
    Widgets->SetObjectField(TEXT("AllianceFeatureButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("AllianceWelcomeJoin"),
        TEXT("AllianceWelcomeCreate"), TEXT("AllianceCreateFlagPanel"), TEXT("AllianceFlagEdit"),
        TEXT("AllianceCreateNameInput"), TEXT("AllianceCreateTagInput"), TEXT("AllianceCreateNoticeInput"), TEXT("AllianceCreateFlagCloth"),
        TEXT("AllianceCreateFlagEmblem"), TEXT("AllianceCreateConfirm"), TEXT("AllianceCreateBack"),
        TEXT("AllianceFlagEditorPreviewPanel"), TEXT("AllianceFlagEditorCloth"),
        TEXT("AllianceFlagEditorEmblem"), TEXT("AllianceFlagEditorSourceIds"),
        TEXT("AllianceFlagEditorCancel"), TEXT("AllianceFlagEditorConfirm"),
        TEXT("FeatureBoundaryFeedback")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    for (int32 Type = 1; Type <= 4; ++Type)
    {
        const FString Tab = FString::Printf(TEXT("AllianceFlagTab_%d"), Type);
        const FString Panel = FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Type);
        Widgets->SetObjectField(Tab, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(*Tab) : nullptr));
        Widgets->SetObjectField(Panel, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(*Panel) : nullptr));
    }
    Json->SetObjectField(TEXT("widgets"), Widgets);
    TArray<TSharedPtr<FJsonValue>> Items;
    const int32 ActiveType = Screen.IsValid() && Screen->bAllianceFlagEditorOpen ? Screen->SelectedAllianceFlagType : 0;
    for (int32 Index = 0; Index < CountAt(ActiveType); ++Index)
    {
        const auto* Entry = EntryAt(ActiveType, Index);
        if (!Entry) continue;
        auto Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("id"), Entry->Id);
        Item->SetNumberField(TEXT("type"), Entry->Type);
        Item->SetNumberField(TEXT("index"), Index);
        Item->SetNumberField(TEXT("action_id"), 19000 + ActiveType * 100 + Index);
        Item->SetBoolField(TEXT("has_sprite"), Entry->bHasSprite);
        Item->SetBoolField(TEXT("has_color"), Entry->bHasColor);
        Item->SetObjectField(TEXT("show_rect"), FlagRectJson(Entry->ShowRect));
        Item->SetObjectField(TEXT("reality_rect"), FlagRectJson(Entry->RealityRect));
        Item->SetObjectField(TEXT("rgb"), Color(RokAllianceFlagLocalPreview::LinearColor(*Entry)));
        for (const TCHAR* Kind : {TEXT("Panel"), TEXT("Swatch"), TEXT("Sprite"), TEXT("Hit"), TEXT("Id")})
        {
            const FString Name = FString::Printf(TEXT("AllianceFlagItem%s_%d"), Kind, Entry->Id);
            Item->SetObjectField(Kind, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(*Name) : nullptr));
        }
        Items.Add(MakeShared<FJsonValueObject>(Item));
    }
    Json->SetArrayField(TEXT("visible_items"), Items);
    return Json;
}


bool URokAllianceFlagLocalPreviewAuditSubsystem::Ready(int32 Phase) const
{
    if (GFrameCounter <= ActionFrame || !Identity() || !Hud.IsValid() || !Screen.IsValid()
        || !Modal.IsValid() || !Runtime.IsValid() || !Server.IsValid() || !Server->IsLocalOnlyRun()
        || Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
        || Modal->IsModalOpen()) return false;
    const bool bHud = Phase == 0 || Phase == 12 || Phase == 15;
    const bool bDraftText = Phase >= 3 && Phase <= 11;
    if (Screen->AllianceCreateNameDraft != (bDraftText ? CreateNameText : TEXT(""))
        || Screen->AllianceCreateTagDraft != (bDraftText ? CreateTagText : TEXT(""))
        || Screen->AllianceCreateNoticeDraft != (bDraftText ? CreateNoticeText : TEXT(""))) return false;
    if (bHud) return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    if (!Screen->IsScreenOpen() || Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE || !SampleUi(Screen.Get()).bReady
        || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const UWidget* Canvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Visible(Canvas) || Canvas->GetRenderOpacity() < .995f
        || !Canvas->GetRenderTransform().Scale.Equals(FVector2D(1), .01f)) return false;
    if (Phase == 1 || Phase == 13)
        return Screen->AlliancePreviewState == 0 && !Screen->bAllianceFlagEditorOpen
            && Visible(Screen->GetWidgetFromName(TEXT("AllianceWelcomeCreate")))
            && !Screen->GetWidgetFromName(TEXT("AllianceCreateFlagPanel"));
    const bool bEditor = Phase == 3 || (Phase >= 4 && Phase <= 7) || Phase == 9 || Phase == 10;
    const bool bCreate = Phase == 2 || Phase == 8 || Phase == 11 || Phase == 14;
    if (!bEditor && !bCreate) return false;
    if (Screen->AlliancePreviewState != 3 || Screen->bAllianceFlagEditorOpen != bEditor
        || !RokAllianceFlagLocalPreview::IsValidDraft(Screen->AllianceCreateFlagDraft)
        || !RokAllianceFlagLocalPreview::IsValidDraft(Screen->AllianceFlagEditDraft)) return false;
    if (bEditor)
    {
        const int32 ExpectedType = Phase == 3 || Phase == 9 ? 1 : Phase == 10 ? 4 : Phase - 3;
        if (Screen->SelectedAllianceFlagType != ExpectedType
            || !Visible(Screen->GetWidgetFromName(TEXT("AllianceFlagEditorCloth")))
            || !Visible(Screen->GetWidgetFromName(TEXT("AllianceFlagEditorEmblem")))
            || !Visible(Screen->GetWidgetFromName(TEXT("AllianceFlagEditorConfirm")))) return false;
        for (int32 Type = 1; Type <= 4; ++Type)
        {
            const FString Name = FString::Printf(TEXT("AllianceFlagTab_%d"), Type);
            if (!Visible(Screen->GetWidgetFromName(*Name))) return false;
        }
        for (int32 Index = 0; Index < CountAt(ExpectedType); ++Index)
        {
            const auto* Entry = EntryAt(ExpectedType, Index);
            if (!Entry) return false;
            const FString Name = FString::Printf(TEXT("AllianceFlagItemHit_%d"), Entry->Id);
            const UWidget* Hit = Screen->GetWidgetFromName(*Name);
            if (!Visible(Hit)) return false;
            const FGeometry Geo = Hit->GetCachedGeometry();
            const FVector2D Origin = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this).LocalToAbsolute(FVector2D::ZeroVector);
            const FVector2D A = Geo.LocalToAbsolute(FVector2D::ZeroVector) - Origin;
            const FVector2D B = Geo.LocalToAbsolute(Geo.GetLocalSize()) - Origin;
            if (Geo.GetLocalSize().X < 75 || Geo.GetLocalSize().Y < 75
                || A.X < 0 || A.Y < 0 || B.X > 1440 || B.Y > 810) return false;
        }
    }
    else if (!Visible(Screen->GetWidgetFromName(TEXT("AllianceCreateFlagCloth")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceCreateFlagEmblem")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceCreateConfirm")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceCreateFlagPanel")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceFlagEdit")))) return false;
    if (bCreate)
    {
        const bool bTyped = Phase != 14;
        const TCHAR* Names[] = {TEXT("AllianceCreateNameInput"), TEXT("AllianceCreateTagInput"), TEXT("AllianceCreateNoticeInput")};
        const TCHAR* Values[] = {CreateNameText, CreateTagText, CreateNoticeText};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const UEditableTextBox* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(Names[Index]));
            if (!Input || !Visible(Input) || Input->GetText().ToString() != (bTyped ? Values[Index] : TEXT(""))) return false;
        }
    }
    RokAllianceFlagLocalPreview::FDraft Expected;
    if (Phase >= 4 && Phase <= 7)
        for (int32 Type = 1; Type <= Phase - 3; ++Type)
            Expected.SetId(Type, EntryAt(Type, CountAt(Type) - 1)->Id);
    if (Phase == 10 || Phase == 11)
        for (int32 Type = 1; Type <= 4; ++Type) Expected.SetId(Type, EntryAt(Type, 0)->Id);
    for (int32 Type = 1; Type <= 4; ++Type)
    {
        if (Screen->AllianceFlagEditDraft.GetId(Type) != Expected.GetId(Type)) return false;
        const int32 CreateExpected = Phase == 11 ? EntryAt(Type, 0)->Id : RokAllianceFlagLocalPreview::FDraft().GetId(Type);
        if (Screen->AllianceCreateFlagDraft.GetId(Type) != CreateExpected) return false;
    }
    return true;
}


void URokAllianceFlagLocalPreviewAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance office read-only route emitted an upgrade completion event"));
}

bool URokAllianceFlagLocalPreviewAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
{
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("label"), Label);
	Attempt->SetStringField(TEXT("kind"), TEXT("button"));
	Attempt->SetNumberField(TEXT("step"), Step);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
	Attempt->SetNumberField(TEXT("pending_before"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
	Attempt->SetStringField(TEXT("target"), Target ? Target->GetName() : FString());
	Attempt->SetBoolField(TEXT("target_exists"), Target != nullptr);
	if (!Target || !Visible(Target) || !Target->GetIsEnabled())
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target missing, disabled, or hidden"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target unavailable"));
	}
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (Size.X <= 0 || Size.Y <= 0)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target has zero cached geometry"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target has zero geometry"));
	}
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target;
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
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	int32 ButtonIndex = INDEX_NONE;
	TArray<TSharedPtr<FJsonValue>> PathTypes;
	TArray<TSharedPtr<FJsonValue>> PathNodes;
	bool bUmgPath = false;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Node = &Path.Widgets[Index].Widget.Get();
		const FString Type = Node->GetTypeAsString();
		PathTypes.Add(MakeShared<FJsonValueString>(Type));
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("index"), Index);
		Entry->SetStringField(TEXT("type"), Type);
		Entry->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Node)));
		PathNodes.Add(MakeShared<FJsonValueObject>(Entry));
		bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
		if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = false;
	uint32 OwnerPid = 0;
	FString OwnWindowId;
	FString ForegroundWindowId;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND OwnHandle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	if (OwnHandle) { DWORD NativePid = 0; ::GetWindowThreadProcessId(OwnHandle, &NativePid); OwnerPid = NativePid; }
	OwnWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(OwnHandle));
	ForegroundWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
	bForeground = OwnHandle && ::GetForegroundWindow() == OwnHandle
		&& OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
	const bool bExactButton = ButtonIndex != INDEX_NONE
		&& Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	Attempt->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Attempt->SetObjectField(TEXT("absolute_center"), Point(Center));
	Attempt->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
	Attempt->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
	Attempt->SetObjectField(TEXT("local_size"), Point(Size));
	Attempt->SetArrayField(TEXT("hit_path_types"), PathTypes);
	Attempt->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
	Attempt->SetStringField(TEXT("terminal_type"), Path.IsValid() && Path.Widgets.Num()
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString());
	Attempt->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
	Attempt->SetBoolField(TEXT("exact_button_in_path"), bExactButton);
	Attempt->SetBoolField(TEXT("umg_path_present"), bUmgPath);
	Attempt->SetStringField(TEXT("target_widget_class"), Target->GetClass()->GetPathName());
	const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
	Attempt->SetNumberField(TEXT("action_id"), Indexed ? Indexed->ActionId : 0);
	Attempt->SetStringField(TEXT("target_widget_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Target)));
	Attempt->SetStringField(TEXT("target_slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(&ButtonSlate.Get())));
	Attempt->SetBoolField(TEXT("target_hovered"), Target->IsHovered());
	Attempt->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Attempt->SetBoolField(TEXT("foreground"), bForeground);
	Attempt->SetStringField(TEXT("own_window_identity"), OwnWindowId);
	Attempt->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
	Attempt->SetNumberField(TEXT("own_window_pid"), OwnerPid);
	Attempt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Attempt->SetObjectField(TEXT("business_before"), BusinessState());
	Attempt->SetObjectField(TEXT("before_state"), State());
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered() || !bOwnWindow || !bForeground)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("Slate path/hover/window did not prove exact UButton"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Exact target button was not hit in foreground game window"));
	}
	if (Indexed && Indexed->ActionId == 623)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("server create action 623 forbidden"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Audit cannot click server creation action"));
	}
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetBoolField(TEXT("down_handled"), Down);
	Attempt->SetBoolField(TEXT("up_handled"), Up);
	Attempt->SetNumberField(TEXT("pending_after"), Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2);
	Attempt->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	Attempt->SetObjectField(TEXT("business_after"), BusinessState());
	Attempt->SetObjectField(TEXT("requested_state"), State());
	Attempt->SetBoolField(TEXT("screen_open_after"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_after"), Modal.IsValid() && Modal->IsModalOpen());
	const int32 PendingAfter = Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : -2;
	const bool bDeferredCorrect = Indexed ? PendingAfter == Indexed->ActionId : PendingAfter == INDEX_NONE;
	const bool bRouteSafe = Down && Up && bDeferredCorrect && Before == After && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, wrong deferred action, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click missed deferred action or changed baseline state"));
}


bool URokAllianceFlagLocalPreviewAuditSubsystem::ClickItem(int32 Type, int32 Index)
{
    const auto* Entry = EntryAt(Type, Index);
    if (!Expect(Entry && Screen.IsValid() && Screen->bAllianceFlagEditorOpen
        && Screen->SelectedAllianceFlagType == Type && Boundary(), TEXT("Flag item route unavailable"))) return false;
    const FString Name = FString::Printf(TEXT("AllianceFlagItemHit_%d"), Entry->Id);
    UButton* Target = Cast<UButton>(Screen->GetWidgetFromName(*Name));
    const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
    if (!Expect(Indexed && Indexed->ActionId == 19000 + Type * 100 + Index,
        TEXT("Flag item action ID mismatch"))) return false;
    const FString Label = FString::Printf(TEXT("select_%d_%d_%d"), Type, Index, Entry->Id);
    if (!Click(Target, *Label)) return false;
    LastClickedId = Entry->Id;
    auto Action = Actions.Last()->AsObject();
    Action->SetStringField(TEXT("kind"), TEXT("item"));
    Action->SetNumberField(TEXT("selected_type"), Type);
    Action->SetNumberField(TEXT("selected_id"), Entry->Id);
    Action->SetNumberField(TEXT("item_index"), Index);
    Action->SetObjectField(TEXT("source_show_rect"), FlagRectJson(Entry->ShowRect));
    Action->SetObjectField(TEXT("source_reality_rect"), FlagRectJson(Entry->RealityRect));
    Action->SetObjectField(TEXT("source_color"), Color(RokAllianceFlagLocalPreview::LinearColor(*Entry)));
    return true;
}
bool URokAllianceFlagLocalPreviewAuditSubsystem::EnterCreateText(const TCHAR* WidgetName, const TCHAR* Label, const FString& Text)
{
    UEditableTextBox* Input = Screen.IsValid()
        ? Cast<UEditableTextBox>(Screen->GetWidgetFromName(WidgetName)) : nullptr;
    if (!Expect(Boundary() && Input && Visible(Input) && Input->GetIsEnabled()
        && FSlateApplication::IsInitialized(), TEXT("Alliance create text input unavailable"))) return false;
    const FGeometry Geometry = Input->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Alliance create input has no geometry"))) return false;
    const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
    if (StableTarget.Get() != Input || !StableTopLeft.Equals(TopLeft, .25f)
        || !StableBottomRight.Equals(BottomRight, .25f))
    {
        StableTarget = Input; StableTopLeft = TopLeft; StableBottomRight = BottomRight;
        StableFrame = GFrameCounter; ClickStableFrames = 1; return false;
    }
    if (GFrameCounter <= StableFrame) return false;
    StableFrame = GFrameCounter;
    if (++ClickStableFrames < 2) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
    const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
    if (!PrepareOwnWindowFocus()) return false;
    FSlateApplication& App = FSlateApplication::Get();
    App.SetCursorPos(Center);
    App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
    App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
    const TSharedRef<SWidget> InputSlate = Input->TakeWidget();
    bool bExactInput = false, bEditablePath = false, bUmgPath = false;
    TArray<TSharedPtr<FJsonValue>> PathNodes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Node = &Path.Widgets[Index].Widget.Get();
        const FString Type = Node->GetTypeAsString();
        auto Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("index"), Index);
        Entry->SetStringField(TEXT("type"), Type);
        Entry->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Node)));
        PathNodes.Add(MakeShared<FJsonValueObject>(Entry));
        bExactInput |= Node == &InputSlate.Get();
        bEditablePath |= Type.Contains(TEXT("SEditableText"));
        bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
    }
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwn = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
    bool bForeground = false;
    uint32 OwnerPid = 0;
    FString OwnWindowId, ForegroundWindowId;
#if PLATFORM_WINDOWS
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (Handle) { DWORD Pid = 0; ::GetWindowThreadProcessId(Handle, &Pid); OwnerPid = Pid; }
    OwnWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle));
    ForegroundWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
    bForeground = Handle && ::GetForegroundWindow() == Handle
        && OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
    auto Receipt = MakeShared<FJsonObject>();
    Receipt->SetStringField(TEXT("label"), Label);
    Receipt->SetStringField(TEXT("target"), Input->GetName());
    Receipt->SetStringField(TEXT("method"), TEXT("ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent+ProcessKeyCharEvent"));
    Receipt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Receipt->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
    Receipt->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
    Receipt->SetStringField(TEXT("target_widget_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Input)));
    Receipt->SetStringField(TEXT("target_slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(&InputSlate.Get())));
    Receipt->SetBoolField(TEXT("exact_input_in_path"), bExactInput);
    Receipt->SetBoolField(TEXT("editable_text_in_path"), bEditablePath);
    Receipt->SetBoolField(TEXT("umg_path_present"), bUmgPath);
    Receipt->SetBoolField(TEXT("target_hovered"), Input->IsHovered());
    Receipt->SetBoolField(TEXT("own_game_window"), bOwn);
    Receipt->SetBoolField(TEXT("foreground"), bForeground);
    Receipt->SetStringField(TEXT("own_window_identity"), OwnWindowId);
    Receipt->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
    Receipt->SetNumberField(TEXT("own_window_pid"), OwnerPid);
    Receipt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
    Receipt->SetStringField(TEXT("text_before"), Input->GetText().ToString());
    Receipt->SetStringField(TEXT("requested_text"), Text);
    Receipt->SetObjectField(TEXT("before_state"), State());
    Receipt->SetObjectField(TEXT("business_before"), BusinessState());
    const uint64 PrimaryBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Receipt->SetNumberField(TEXT("primary_before"), static_cast<double>(PrimaryBefore));
    if (!bExactInput || !bEditablePath || !bUmgPath || !Input->IsHovered() || !bOwn || !bForeground
        || !Input->GetText().IsEmpty())
    {
        Receipt->SetStringField(TEXT("failure"), TEXT("wrong target/window or nonempty input"));
        Attempts.Add(MakeShared<FJsonValueObject>(Receipt));
        return Expect(false, TEXT("Alliance create input target not safely empty"));
    }
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const TSharedPtr<SWidget> Focused = App.GetKeyboardFocusedWidget();
    const bool bFocusOwned = Focused.IsValid() && Focused->GetTypeAsString().Contains(TEXT("EditableText"))
        && Path.ContainsWidget(Focused.Get());
    Receipt->SetBoolField(TEXT("focus_down_handled"), Down);
    Receipt->SetBoolField(TEXT("focus_up_handled"), Up);
    Receipt->SetBoolField(TEXT("keyboard_focus_in_hit_path"), bFocusOwned);
    Receipt->SetStringField(TEXT("keyboard_focused_identity"), Focused.IsValid()
        ? FString::Printf(TEXT("0x%p"), static_cast<const void*>(Focused.Get())) : FString());
    if (!Down || !Up || !bFocusOwned)
    {
        Receipt->SetStringField(TEXT("failure"), TEXT("actual Slate focus click failed"));
        Attempts.Add(MakeShared<FJsonValueObject>(Receipt));
        return Expect(false, TEXT("Alliance create text input did not receive keyboard focus"));
    }
    bool bCharacters = true;
    TArray<TSharedPtr<FJsonValue>> Characters;
    for (TCHAR Character : Text)
    {
        const bool bHandled = App.ProcessKeyCharEvent(FCharacterEvent(Character, FModifierKeysState(), 0, false));
        bCharacters &= bHandled;
        auto Event = MakeShared<FJsonObject>();
        Event->SetStringField(TEXT("character"), FString(1, &Character));
        Event->SetBoolField(TEXT("handled"), bHandled);
        Characters.Add(MakeShared<FJsonValueObject>(Event));
    }
    Receipt->SetArrayField(TEXT("character_events"), Characters);
    Receipt->SetBoolField(TEXT("all_characters_handled"), bCharacters);
    Receipt->SetBoolField(TEXT("focus_retained"), App.GetKeyboardFocusedWidget() == Focused);
    Receipt->SetStringField(TEXT("text_after"), Input->GetText().ToString());
    Receipt->SetNumberField(TEXT("primary_after"), static_cast<double>(Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount));
    Receipt->SetObjectField(TEXT("business_after"), BusinessState());
    Receipt->SetObjectField(TEXT("requested_state"), State());
    const bool bSafe = bCharacters && Input->GetText().ToString() == Text
        && App.GetKeyboardFocusedWidget() == Focused
        && Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount == PrimaryBefore
        && Boundary();
    Receipt->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("Slate text not applied or business changed"));
    TextInputs.Add(MakeShared<FJsonValueObject>(Receipt));
    StableTarget.Reset(); ClickStableFrames = 0;
    return Expect(bSafe, TEXT("Alliance create real Slate text entry failed"));
}
bool URokAllianceFlagLocalPreviewAuditSubsystem::CheckClickedEffect()
{
    if (GFrameCounter <= ActionFrame || !Screen.IsValid() || !Boundary()) return false;
    if (!Expect(Screen->GetPendingActionIdForAudit() == INDEX_NONE
        && Screen->bAllianceFlagEditorOpen && Screen->SelectedAllianceFlagType == CurrentType
        && Screen->AllianceFlagEditDraft.GetId(CurrentType) == LastClickedId
        && SampleUi(Screen.Get()).bReady, TEXT("Flag item deferred action did not rebuild selected preview"))) return false;
    auto Action = Actions.Last()->AsObject();
    Action->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
    Action->SetObjectField(TEXT("processed_state"), State());
    return true;
}


void URokAllianceFlagLocalPreviewAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& (bBaselineSet ? Boundary() : Ready(0)), TEXT("Screenshot request/boundary invalid"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("requested_state"), State());
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}

void URokAllianceFlagLocalPreviewAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& (!bBaselineSet || Boundary()), TEXT("Processed screenshot PNG/extent/business state invalid"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("png_width"), 1440);
	Pending->SetNumberField(TEXT("png_height"), 810);
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}

void URokAllianceFlagLocalPreviewAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(!bBaselineSet || Boundary(), TEXT("Post-screenshot business state changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}

void URokAllianceFlagLocalPreviewAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset(); ActionFrame = GFrameCounter;
}



bool URokAllianceFlagLocalPreviewAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_flag.local_preview.runtime_audit.v2"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 16
        && Actions.Num() == 83 && TextInputs.Num() == 3 && Attempts.IsEmpty() && CompletionEvents == 0
        && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
    Json->SetStringField(TEXT("scope"), TEXT("Native HUD Alliance Create flag editor; all 63 config options; local draft only"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
    Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
    Json->SetBoolField(TEXT("alliance_membership_accepted"), false);
    Json->SetBoolField(TEXT("server_create_accepted"), false);
    Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
    Json->SetBoolField(TEXT("full_migration_accepted"), false);
    Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
    Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("text_inputs"), TextInputs);
    Json->SetArrayField(TEXT("attempts"), Attempts);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid()
        ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
    auto Config = MakeShared<FJsonObject>();
    Config->SetStringField(TEXT("atlas"), TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.rok_fullscreen_alliance_flag"));
    Config->SetObjectField(TEXT("atlas_size"), Point(FVector2D(1024, 2048)));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Type = 1; Type <= 4; ++Type)
        for (int32 Index = 0; Index < CountAt(Type); ++Index)
        {
            const auto* Entry = EntryAt(Type, Index);
            auto Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("id"), Entry->Id);
            Row->SetNumberField(TEXT("type"), Type);
            Row->SetNumberField(TEXT("index"), Index);
            Row->SetNumberField(TEXT("action_id"), 19000 + Type * 100 + Index);
            Row->SetObjectField(TEXT("show_rect"), FlagRectJson(Entry->ShowRect));
            Row->SetObjectField(TEXT("reality_rect"), FlagRectJson(Entry->RealityRect));
            Row->SetObjectField(TEXT("color"), Color(RokAllianceFlagLocalPreview::LinearColor(*Entry)));
            Row->SetBoolField(TEXT("has_sprite"), Entry->bHasSprite);
            Row->SetBoolField(TEXT("has_color"), Entry->bHasColor);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
    Config->SetArrayField(TEXT("rows"), Rows);
    Json->SetObjectField(TEXT("source_config"), Config);
    Json->SetObjectField(TEXT("final_state"), State());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokAllianceFlagLocalPreviewAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    bool bFinalTick = false;
    if (Captures.Num() == 16)
    {
        const auto Last = Captures.Last()->AsObject();
        bFinalTick = Last.IsValid() && GFrameCounter == static_cast<uint64>(Last->GetNumberField(TEXT("next_frame"))) + 1;
    }
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 16
        && Actions.Num() == 83 && TextInputs.Num() == 3 && Attempts.IsEmpty() && CompletionEvents == 0
        && bFinalTick && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance flag local-preview audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok alliance flag local-preview audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok alliance flag audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokAllianceFlagLocalPreviewAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !Identity()) return;
    Elapsed += FMath::Max(0.0f, DeltaTime);
    if (Elapsed < 2.0) return;
    if (Pending.IsValid())
    {
        if (Elapsed - StepSince > 30.0) Finish(TEXT("Screenshot callback timeout"));
        else AdvanceCapture();
        return;
    }
    if (Elapsed - StepSince > 35.0) { Finish(TEXT("Audit step timeout")); return; }
    auto Stable = [this](int32 Phase)
    {
        StableFrames = Ready(Phase) ? StableFrames + 1 : 0;
        return StableFrames >= 2;
    };
    if (Step == 0) { if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); return; }
    if (Actions.Num())
    {
        const auto Last = Actions.Last()->AsObject();
        if (Last.IsValid() && !Last->HasField(TEXT("processed_frame")))
        {
            if (GFrameCounter <= static_cast<uint64>(Last->GetNumberField(TEXT("frame")))) return;
            if (Screen.IsValid() && Screen->IsScreenOpen() && !SampleUi(Screen.Get()).bReady) return;
            if (!Expect(Screen.IsValid() && Screen->GetPendingActionIdForAudit() == INDEX_NONE
                && Boundary(), TEXT("Clicked action not processed safely on a later tick"))) return;
            if (Last->GetStringField(TEXT("kind")) == TEXT("item") && !CheckClickedEffect()) return;
            Last->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
            Last->SetObjectField(TEXT("processed_state"), State());
            return;
        }
        if (Last.IsValid() && !Last->HasField(TEXT("next_frame")))
        {
            const uint64 Processed = static_cast<uint64>(Last->GetNumberField(TEXT("processed_frame")));
            if (GFrameCounter <= Processed) return;
            if (!Expect(GFrameCounter == Processed + 1 && Boundary(),
                TEXT("Action next-frame continuity or business boundary failed"))) return;
            Last->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
            Last->SetObjectField(TEXT("next_frame_state"), State());
            return;
        }
    }
    if (Step == 1)
    {
        if (Stable(0))
        {
            FRokLocalConstructionTicket Ticket;
            int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
            PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
            if (!Expect(Buildings->GetActiveUpgradeCount() == 0 && PendingCount == 0
                && Buildings->GetActiveLocalConstructionCount() == 0
                && !Buildings->GetLocalConstructionTicket(1, Ticket),
                TEXT("Flag audit requires idle business queues"))) return;
            BaselineBusinessJson = BusinessState();
            BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true;
            Capture(TEXT("00_city_hud"), 2);
        }
        return;
    }
    if (!Expect(Boundary(), TEXT("Flag audit business/server boundary changed"))) return;
    auto Button = [this](const TCHAR* Name) -> UButton*
    { return Cast<UButton>(Screen->GetWidgetFromName(Name)); };
    auto HudButton = [this]() -> UButton*
    { return Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))); };
    auto Take = [this](UButton* Target, const TCHAR* Label, int32 Next)
    { if (Click(Target, Label)) Enter(Next); };
    auto Shot = [this, &Stable](int32 Phase, const TCHAR* Label, int32 Next)
    { if (Stable(Phase)) Capture(Label, Next); };
    switch (Step)
    {
    case 2: Take(HudButton(), TEXT("open_alliance_from_hud"), 3); break;
    case 3: Shot(1, TEXT("01_alliance_welcome"), 4); break;
    case 4: Take(Button(TEXT("AllianceWelcomeCreate")), TEXT("open_create_601"), 5); break;
    case 5:
        if (TextCursor < 3)
        {
            static const TCHAR* Names[] = {TEXT("AllianceCreateNameInput"), TEXT("AllianceCreateTagInput"), TEXT("AllianceCreateNoticeInput")};
            static const TCHAR* Labels[] = {TEXT("type_create_name"), TEXT("type_create_tag"), TEXT("type_create_notice")};
            static const TCHAR* Values[] = {CreateNameText, CreateTagText, CreateNoticeText};
            if (EnterCreateText(Names[TextCursor], Labels[TextCursor], Values[TextCursor]))
            { ++TextCursor; Enter(5); }
        }
        else Shot(2, TEXT("02_create_default"), 6);
        break;
    case 6: Take(Button(TEXT("AllianceFlagEdit")), TEXT("open_flag_editor_625"), 7); break;
    case 7: Shot(3, TEXT("03_editor_cloth_default"), 8); break;
    case 8: CurrentType = 1; ItemCursor = 0; bSecondPass = false; Enter(20); break;
    case 20:
        if (ItemCursor < (bSecondPass ? 1 : CountAt(CurrentType)))
        {
            if (ClickItem(CurrentType, ItemCursor)) { ++ItemCursor; Enter(20); }
        }
        else if (bSecondPass)
        {
            if (CurrentType == 4) Shot(10, TEXT("10_editor_four_first_ids"), 50);
            else Enter(30);
        }
        else
        {
            static const TCHAR* Names[] = {TEXT("04_cloth_all16"), TEXT("05_cloth_colors_all16"),
                TEXT("06_emblems_all24"), TEXT("07_emblem_colors_all7")};
            Shot(3 + CurrentType, Names[CurrentType - 1], CurrentType == 4 ? 40 : 30);
        }
        break;
    case 30:
        {
            const int32 NextType = CurrentType + 1;
            const FString Name = FString::Printf(TEXT("AllianceFlagTab_%d"), NextType);
            const FString Label = FString::Printf(TEXT("switch_tab_%d"), NextType);
            if (Click(Button(*Name), *Label)) { CurrentType = NextType; ItemCursor = 0; Enter(20); }
        }
        break;
    case 40: Take(Button(TEXT("AllianceFlagEditorCancel")), TEXT("cancel_local_draft"), 41); break;
    case 41: Shot(8, TEXT("08_cancel_restores_create"), 42); break;
    case 42: Take(Button(TEXT("AllianceFlagEdit")), TEXT("reopen_flag_editor"), 43); break;
    case 43: Shot(9, TEXT("09_editor_reopened_default"), 44); break;
    case 44: CurrentType = 1; ItemCursor = 0; bSecondPass = true; Enter(20); break;
    case 50: Take(Button(TEXT("AllianceFlagEditorConfirm")), TEXT("confirm_local_draft"), 51); break;
    case 51: Shot(11, TEXT("11_create_draft_confirmed"), 52); break;
    case 52: Take(Button(TEXT("FeatureFrameClose")), TEXT("close_create_to_hud"), 53); break;
    case 53: Shot(12, TEXT("12_hud_closed"), 54); break;
    case 54: Take(HudButton(), TEXT("reopen_alliance"), 55); break;
    case 55: Shot(13, TEXT("13_welcome_reopened"), 56); break;
    case 56: Take(Button(TEXT("AllianceWelcomeCreate")), TEXT("reopen_create_reset"), 57); break;
    case 57: Shot(14, TEXT("14_create_reset"), 58); break;
    case 58: Take(Button(TEXT("FeatureFrameClose")), TEXT("final_close_to_hud"), 59); break;
    case 59: Shot(15, TEXT("15_final_hud"), 60); break;
    case 60: Finish(); break;
    default: Finish(TEXT("Invalid flag audit step")); break;
    }
}
