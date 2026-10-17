#include "RokAllianceFixtureSearchAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
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
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance")};
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
constexpr TCHAR AuditFlag[] = TEXT("-RokAllianceFixtureSearchAudit");
struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
constexpr FInputPin Pinned[] = {
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("b09141b49e0b76301b4c042b5ca0c9deca7cc578653994d2946dbc26f18e25c8")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("69d137477abf0e169cbd7eedf38595c4f0a0ab90a0e380be1f42e63996d84d7f")},
    {TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9dc34a7279c15f7cf153c8f063cb06d565be9f9792e50def35bac2195241f05c")},
    {TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("4c1059fefad4a983f1b666446c1a87e255aeaa37fec3a1c9e097df910ea6b7ef")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
    {TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
    {TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
    {TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
    {TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
    {TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Alliance/UI_Win_GuildJoinMediator.cs"), TEXT("2f196e448bfa072e9754ffd1d9357877659f5d159688f860cb1f333b4884899e")},
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




bool URokAllianceFixtureSearchAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer))
        && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceFixtureSearchAudit"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceFixtureSearchAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game; }
TStatId URokAllianceFixtureSearchAuditSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceFixtureSearchAuditSubsystem, STATGROUP_Tickables); }
void URokAllianceFixtureSearchAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingAllianceFixtureSearchRuntimeAuditV3/RuntimeAudit")
        / FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this, &URokAllianceFixtureSearchAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceFixtureSearchAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    Super::Deinitialize();
}
bool URokAllianceFixtureSearchAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokAllianceFixtureSearchAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
    if (!Condition) Finish(Reason);
    return Condition;
}


bool URokAllianceFixtureSearchAuditSubsystem::PrepareOwnWindowFocus()
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


bool URokAllianceFixtureSearchAuditSubsystem::Begin()
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
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && FixtureFlags <= 1,
        TEXT("Exactly one audit/local flag and zero or one fixture flag required"))) return false;
    bFixture = FixtureFlags == 1;
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
        TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance")})
        if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
            TEXT("Required Alliance texture failed to load"))) return false;
    OriginalPointer = FSlateApplication::Get().GetCursorPos();
    LastPointer = OriginalPointer;
    bPointerSaved = true;
    Inputs = MakeShared<FJsonObject>();
    for (const FInputPin& Pin : Pinned)
    {
        const FString Actual = FileSha(RokDataPaths::ResolveProjectPath(Pin.Path));
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    for (const TCHAR* Path : {TEXT("Source/Rok/RokAllianceFixtureSearchAuditSubsystem.h"),
        TEXT("Source/Rok/RokAllianceFixtureSearchAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokAllianceFixtureSearchRuntime.py")})
    {
        const FString Actual = FileSha(RokDataPaths::ResolveProjectPath(Path));
        if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
        Inputs->SetStringField(Path, Actual);
    }
    CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
        this, &URokAllianceFixtureSearchAuditSubsystem::OnUpgradeCompleted);
    return true;
}


TSharedRef<FJsonObject> URokAllianceFixtureSearchAuditSubsystem::BusinessState() const
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

bool URokAllianceFixtureSearchAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}


TSharedRef<FJsonObject> URokAllianceFixtureSearchAuditSubsystem::State() const
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
    Json->SetBoolField(TEXT("fixture_state"), Screen.IsValid() && Screen->bAllianceTestFixtureState);
    Json->SetNumberField(TEXT("selected_original_index"), Screen.IsValid() ? Screen->SelectedAllianceListIndex : INDEX_NONE);
    Json->SetStringField(TEXT("draft"), Screen.IsValid() ? Screen->AllianceFixtureSearchInput : FString());
    Json->SetStringField(TEXT("submitted_query"), Screen.IsValid() ? Screen->AllianceFixtureSearchQuery : FString());
    const UEditableTextBox* Input = Screen.IsValid()
        ? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceSearchInput"))) : nullptr;
    Json->SetStringField(TEXT("input_text"), Input ? Input->GetText().ToString() : FString());
    TArray<TSharedPtr<FJsonValue>> VisibleIds;
    if (Screen.IsValid()) for (int32 Index : Screen->VisibleAllianceFixtureIndices)
        VisibleIds.Add(MakeShared<FJsonValueNumber>(Index));
    Json->SetArrayField(TEXT("visible_original_indices"), VisibleIds);
    auto Widgets = MakeShared<FJsonObject>();
    Widgets->SetObjectField(TEXT("AllianceFeatureButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")) : nullptr));
    for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("FeatureFrameClose"), TEXT("FeatureFrameTitle"),
        TEXT("AllianceWelcomeJoin"), TEXT("AllianceSearchInput"), TEXT("AllianceSearch"),
        TEXT("AllianceJoinFixtureTag"), TEXT("AllianceJoinFixtureEmpty"), TEXT("AllianceJoinEmptyState"),
        TEXT("AllianceJoinServiceState"), TEXT("AllianceJoinDetailBg"), TEXT("AllianceJoinDetailName"),
        TEXT("AllianceJoinDetailEmpty"), TEXT("AllianceJoinConfirm"), TEXT("AllianceJoinBack"),
        TEXT("FeatureBoundaryFeedback")})
        Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("original_index"), Index);
        Row->SetNumberField(TEXT("action_id"), 610 + Index);
        for (const TCHAR* Kind : {TEXT("RowBg"), TEXT("Name"), TEXT("Stats"), TEXT("Type"), TEXT("RowHit")})
        {
            const FString Name = FString::Printf(TEXT("AllianceJoin%s_%d"), Kind, Index);
            Row->SetObjectField(Kind, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(*Name) : nullptr));
        }
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Json->SetArrayField(TEXT("rows"), Rows);
    Json->SetObjectField(TEXT("widgets"), Widgets);
    return Json;
}



bool URokAllianceFixtureSearchAuditSubsystem::Ready(int32 Phase) const
{
    if (GFrameCounter <= ActionFrame) return false;
    if (!Identity() || !Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid()
        || !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
        || Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
    const bool bHud = Phase == 0 || (bFixture && (Phase == 12 || Phase == 15))
        || (!bFixture && Phase == 5);
    if (bHud) return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    if (!Screen->IsScreenOpen() || Screen->ActiveMode != ERokUnityFeatureScreenMode::Alliance
        || Screen->bAllianceTestFixtureState != bFixture || Screen->GetPendingActionIdForAudit() != INDEX_NONE
        || !SampleUi(Screen.Get()).bReady || !Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const UWidget* ModeCanvas = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
    if (!Visible(ModeCanvas) || ModeCanvas->GetRenderOpacity() < .995f
        || !ModeCanvas->GetRenderTransform().Scale.Equals(FVector2D(1), .01f)) return false;
    const bool bWelcome = (Phase == 1 || (bFixture && (Phase == 10 || Phase == 13))
        || (!bFixture && Phase == 4));
    if (bWelcome)
        return Screen->AlliancePreviewState == 0 && Screen->SelectedAllianceListIndex == 0
            && Screen->AllianceFixtureSearchInput.IsEmpty() && Screen->AllianceFixtureSearchQuery.IsEmpty()
            && Screen->VisibleAllianceFixtureIndices.IsEmpty()
            && Visible(Screen->GetWidgetFromName(TEXT("AllianceWelcomeJoin")))
            && !Screen->GetWidgetFromName(TEXT("AllianceSearchInput"));
    if (Screen->AlliancePreviewState != 1 || !Visible(Screen->GetWidgetFromName(TEXT("AllianceSearchInput")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceSearch")))
        || !Visible(Screen->GetWidgetFromName(TEXT("AllianceJoinBack")))) return false;
    const UEditableTextBox* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceSearchInput")));
    if (!Input) return false;
    if (!bFixture)
    {
        const UTextBlock* Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
        return (Phase == 2 || Phase == 3) && Screen->VisibleAllianceFixtureIndices.IsEmpty()
            && Screen->AllianceFixtureSearchInput.IsEmpty() && Screen->AllianceFixtureSearchQuery.IsEmpty()
            && Input->GetText().ToString() == (Phase == 3 ? TEXT("ROK") : TEXT(""))
            && Visible(Screen->GetWidgetFromName(TEXT("AllianceJoinEmptyState")))
            && !Screen->GetWidgetFromName(TEXT("AllianceJoinRowHit_0"))
            && !Screen->GetWidgetFromName(TEXT("AllianceJoinConfirm"))
            && (Phase != 3 || (Feedback
                && Feedback->GetText().ToString().Contains(TEXT("联盟搜索需要服务器数据"))));
    }
    if (Phase == 2 || Phase == 7 || Phase == 11 || Phase == 14)
        return Input->GetText().IsEmpty() && Screen->AllianceFixtureSearchQuery.IsEmpty()
            && Screen->VisibleAllianceFixtureIndices.Num() == 5 && Screen->SelectedAllianceListIndex == 0;
    const FString Query = Phase == 3 || Phase == 8 || Phase == 9 ? TEXT("ROK")
        : Phase == 4 ? TEXT("sNoW") : Phase == 5 ? TEXT("RO") : TEXT("ZZZ");
    const int32 Count = (Phase == 3 || Phase == 4 || Phase == 8 || Phase == 9) ? 1 : 0;
    const int32 OriginalIndex = Phase == 4 ? 0 : 1;
    return Input->GetText().ToString() == Query && Screen->AllianceFixtureSearchInput == Query
        && Screen->AllianceFixtureSearchQuery == Query
        && Screen->VisibleAllianceFixtureIndices.Num() == Count
        && (Count == 0 || Screen->VisibleAllianceFixtureIndices[0] == OriginalIndex)
        && Screen->SelectedAllianceListIndex == (Count ? OriginalIndex : INDEX_NONE)
        && (Count != 0 || (!Screen->GetWidgetFromName(TEXT("AllianceJoinDetailName"))
            && !Screen->GetWidgetFromName(TEXT("AllianceJoinConfirm"))));
}


void URokAllianceFixtureSearchAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Alliance office read-only route emitted an upgrade completion event"));
}

bool URokAllianceFixtureSearchAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
{
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("label"), Label);
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
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered() || !bOwnWindow || !bForeground)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("Slate path/hover/window did not prove exact UButton"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Exact target button was not hit in foreground game window"));
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


bool URokAllianceFixtureSearchAuditSubsystem::Edit(const TCHAR* Label, const FString& Text)
{
    UEditableTextBox* Input = Screen.IsValid()
        ? Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("AllianceSearchInput"))) : nullptr;
    if (!Expect(Boundary() && Input && Visible(Input) && Input->GetIsEnabled()
        && FSlateApplication::IsInitialized(), TEXT("Alliance search input unavailable"))) return false;
    const FGeometry Geometry = Input->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (!Expect(Size.X > 0 && Size.Y > 0, TEXT("Alliance search input has no Slate geometry"))) return false;
    const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
    if (StableTarget.Get() != Input || !StableTopLeft.Equals(TopLeft, .25f)
        || !StableBottomRight.Equals(BottomRight, .25f))
    {
        StableTarget = Input;
        StableTopLeft = TopLeft;
        StableBottomRight = BottomRight;
        StableFrame = GFrameCounter;
        ClickStableFrames = 1;
        return false;
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
    TArray<TSharedPtr<FJsonValue>> PathTypes, PathNodes;
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
    auto Action = MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("label"), Label);
    Action->SetStringField(TEXT("kind"), TEXT("edit"));
    Action->SetStringField(TEXT("method"), TEXT("Slate pointer focus click; Ctrl+A; Backspace; ProcessKeyCharEvent"));
    Action->SetNumberField(TEXT("step"), Step);
    Action->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Action->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
    Action->SetStringField(TEXT("target"), Input->GetName());
    Action->SetStringField(TEXT("target_widget_class"), Input->GetClass()->GetPathName());
    Action->SetStringField(TEXT("target_widget_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Input)));
    Action->SetStringField(TEXT("target_slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(&InputSlate.Get())));
    Action->SetObjectField(TEXT("absolute_center"), Point(Center));
    Action->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
    Action->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
    Action->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Action->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
    Action->SetBoolField(TEXT("exact_input_in_path"), bExactInput);
    Action->SetBoolField(TEXT("editable_text_in_path"), bEditablePath);
    Action->SetBoolField(TEXT("umg_path_present"), bUmgPath);
    Action->SetBoolField(TEXT("target_hovered"), Input->IsHovered());
    Action->SetBoolField(TEXT("own_game_window"), bOwn);
    Action->SetBoolField(TEXT("foreground"), bForeground);
    Action->SetStringField(TEXT("own_window_identity"), OwnWindowId);
    Action->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
    Action->SetNumberField(TEXT("own_window_pid"), OwnerPid);
    Action->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
    Action->SetObjectField(TEXT("business_before"), BusinessState());
    const uint64 PrimaryBefore = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Action->SetNumberField(TEXT("primary_before"), static_cast<double>(PrimaryBefore));
    Action->SetStringField(TEXT("text_before"), Input->GetText().ToString());
    if (!(bExactInput && bEditablePath && bUmgPath && Input->IsHovered() && bOwn && bForeground))
    {
        Action->SetStringField(TEXT("failure"), TEXT("Wrong input hit path/focus window"));
        Attempts.Add(MakeShared<FJsonValueObject>(Action));
        return Expect(false, TEXT("Alliance input not hit in foreground own game window"));
    }
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const TSharedPtr<SWidget> Focused = App.GetKeyboardFocusedWidget();
    const FString FocusedType = Focused.IsValid() ? Focused->GetTypeAsString() : FString();
    const bool bFocusOwned = Focused.IsValid() && FocusedType.Contains(TEXT("EditableText"))
        && Path.ContainsWidget(Focused.Get());
    Action->SetBoolField(TEXT("focus_down_handled"), Down);
    Action->SetBoolField(TEXT("focus_up_handled"), Up);
    Action->SetStringField(TEXT("keyboard_focused_type"), FocusedType);
    Action->SetStringField(TEXT("keyboard_focused_identity"), Focused.IsValid()
        ? FString::Printf(TEXT("0x%p"), static_cast<const void*>(Focused.Get())) : FString());
    Action->SetBoolField(TEXT("keyboard_focus_in_hit_path"), bFocusOwned);
    if (!Down || !Up || !bFocusOwned)
    {
        Action->SetStringField(TEXT("failure"), TEXT("Actual pointer focus click failed"));
        Attempts.Add(MakeShared<FJsonValueObject>(Action));
        return Expect(false, TEXT("Alliance text box did not receive Slate keyboard focus"));
    }
    const FString Before = Input->GetText().ToString();
    const FModifierKeysState NoMods;
    const FModifierKeysState Control(false, false, true, false, false, false, false, false, false);
    const bool CtrlDown = App.ProcessKeyDownEvent(FKeyEvent(EKeys::LeftControl, Control, 0, false, 0, 0));
    const bool SelectAll = App.ProcessKeyDownEvent(FKeyEvent(EKeys::A, Control, 0, false, 0, 0));
    const bool AUp = App.ProcessKeyUpEvent(FKeyEvent(EKeys::A, Control, 0, false, 0, 0));
    const bool CtrlUp = App.ProcessKeyUpEvent(FKeyEvent(EKeys::LeftControl, NoMods, 0, false, 0, 0));
    const bool Erase = App.ProcessKeyDownEvent(FKeyEvent(EKeys::BackSpace, NoMods, 0, false, 0, 0));
    const bool BackUp = App.ProcessKeyUpEvent(FKeyEvent(EKeys::BackSpace, NoMods, 0, false, 0, 0));
    const FString AfterErase = Input->GetText().ToString();
    bool bCharacters = true;
    TArray<TSharedPtr<FJsonValue>> CharacterEvents;
    for (TCHAR Character : Text)
    {
        const bool bHandled = App.ProcessKeyCharEvent(FCharacterEvent(Character, NoMods, 0, false));
        bCharacters &= bHandled;
        auto Event = MakeShared<FJsonObject>();
        Event->SetStringField(TEXT("character"), FString(1, &Character));
        Event->SetBoolField(TEXT("handled"), bHandled);
        CharacterEvents.Add(MakeShared<FJsonValueObject>(Event));
    }
    const TSharedPtr<SWidget> FocusedAfter = App.GetKeyboardFocusedWidget();
    const uint64 PrimaryAfter = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
    Action->SetBoolField(TEXT("ctrl_down_handled"), CtrlDown);
    Action->SetBoolField(TEXT("select_all_handled"), SelectAll);
    Action->SetBoolField(TEXT("a_up_handled"), AUp);
    Action->SetBoolField(TEXT("ctrl_up_handled"), CtrlUp);
    Action->SetBoolField(TEXT("backspace_handled"), Erase);
    Action->SetBoolField(TEXT("backspace_up_handled"), BackUp);
    Action->SetBoolField(TEXT("backspace_empty_noop"), Before.IsEmpty() && AfterErase.IsEmpty());
    Action->SetBoolField(TEXT("all_characters_handled"), bCharacters);
    Action->SetArrayField(TEXT("character_events"), CharacterEvents);
    Action->SetStringField(TEXT("text_after_erase"), AfterErase);
    Action->SetStringField(TEXT("requested_text"), Text);
    Action->SetStringField(TEXT("text_after"), Input->GetText().ToString());
    Action->SetBoolField(TEXT("focus_retained"), FocusedAfter == Focused);
    Action->SetNumberField(TEXT("primary_after"), static_cast<double>(PrimaryAfter));
    Action->SetObjectField(TEXT("business_after"), BusinessState());
    const bool bSafe = SelectAll && AfterErase.IsEmpty() && (Erase || Before.IsEmpty())
        && bCharacters && Input->GetText().ToString() == Text && FocusedAfter == Focused
        && PrimaryBefore == PrimaryAfter && Boundary();
    Action->SetStringField(TEXT("failure"), bSafe ? TEXT("") : TEXT("Unrouted key, wrong input, or business mutation"));
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    StableTarget.Reset(); ClickStableFrames = 0;
    return Expect(bSafe, TEXT("Alliance real Slate keyboard edit failed"));
}


void URokAllianceFixtureSearchAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
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

void URokAllianceFixtureSearchAuditSubsystem::ScreenshotProcessed()
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

void URokAllianceFixtureSearchAuditSubsystem::AdvanceCapture()
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

void URokAllianceFixtureSearchAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset(); ActionFrame = GFrameCounter;
}


bool URokAllianceFixtureSearchAuditSubsystem::Save(bool Complete) const
{
    auto Json = MakeShared<FJsonObject>();
    const int32 ExpectedCaptures = bFixture ? 16 : 6;
    const int32 ExpectedActions = bFixture ? 21 : 6;
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance_fixture_search.runtime_audit.v3"));
    Json->SetStringField(TEXT("mode"), bFixture ? TEXT("fixture") : TEXT("production"));
    Json->SetBoolField(TEXT("complete"), Complete);
    Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == ExpectedCaptures
        && Actions.Num() == ExpectedActions && Attempts.IsEmpty() && CompletionEvents == 0
        && bBaselineSet && Boundary());
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
    Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetNumberField(TEXT("utc_ticks"), static_cast<double>(FDateTime::UtcNow().GetTicks()));
    Json->SetStringField(TEXT("scope"), TEXT("Native HUD Alliance welcome/join search, actual Slate edit and click; anonymous local-only"));
    Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
    Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
    Json->SetBoolField(TEXT("alliance_membership_accepted"), false);
    Json->SetBoolField(TEXT("account_alliance_state_accepted"), false);
    Json->SetBoolField(TEXT("server_query_response_accepted"), false);
    Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
    Json->SetBoolField(TEXT("full_migration_accepted"), false);
    Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
    Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("attempts"), Attempts);
    Json->SetArrayField(TEXT("captures"), Captures);
    Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid()
        ? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetObjectField(TEXT("final_state"), State());
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokAllianceFixtureSearchAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
    const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == (bFixture ? 16 : 6)
        && Actions.Num() == (bFixture ? 21 : 6) && Attempts.IsEmpty()
        && CompletionEvents == 0 && Boundary();
    if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Alliance fixture search audit incomplete"));
    Save(Complete);
    UE_LOG(LogTemp, Display, TEXT("Rok alliance fixture search audit %s: %s"),
        Complete ? TEXT("passed") : TEXT("failed"), *Directory);
    if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok alliance fixture search audit: %s"), *Failures.Last());
    FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokAllianceFixtureSearchAuditSubsystem::Tick(float DeltaTime)
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
                TEXT("Alliance audit requires idle local business queues"))) return;
            BaselineBusinessJson = BusinessState();
            BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
            BaselineCamera = Controller->GetPawn()->GetActorTransform();
            BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
            bBaselineSet = true;
            Capture(TEXT("00_city_hud"), 2);
        }
        return;
    }
    if (!Expect(Boundary(), TEXT("Alliance audit business/server boundary changed"))) return;
    auto Button = [this](const TCHAR* Name) -> UButton*
    { return Cast<UButton>(Screen->GetWidgetFromName(Name)); };
    auto HudButton = [this]() -> UButton*
    { return Cast<UButton>(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton"))); };
    auto Take = [this](UButton* Target, const TCHAR* Label, int32 Next)
    { if (Click(Target, Label)) Enter(Next); };
    auto Type = [this](const TCHAR* Label, const TCHAR* Text, int32 Next)
    { if (Edit(Label, Text)) Enter(Next); };
    auto Shot = [this, &Stable](int32 Phase, const TCHAR* Label, int32 Next)
    { if (Stable(Phase)) Capture(Label, Next); };
    if (!bFixture)
    {
        switch (Step)
        {
        case 2: Take(HudButton(), TEXT("open_alliance_from_hud"), 3); break;
        case 3: Shot(1, TEXT("01_alliance_welcome"), 4); break;
        case 4: Take(Button(TEXT("AllianceWelcomeJoin")), TEXT("open_join_list"), 5); break;
        case 5: Shot(2, TEXT("02_join_empty"), 6); break;
        case 6: Type(TEXT("type_ROK_production"), TEXT("ROK"), 7); break;
        case 7: Take(Button(TEXT("AllianceSearch")), TEXT("search_production_boundary"), 8); break;
        case 8: Shot(3, TEXT("03_search_server_boundary"), 9); break;
        case 9: Take(Button(TEXT("AllianceJoinBack")), TEXT("back_to_welcome"), 10); break;
        case 10: Shot(4, TEXT("04_welcome_return"), 11); break;
        case 11: Take(Button(TEXT("FeatureFrameClose")), TEXT("close_to_hud"), 12); break;
        case 12: Shot(5, TEXT("05_final_hud"), 13); break;
        case 13: Finish(); break;
        default: Finish(TEXT("Invalid production audit step")); break;
        }
        return;
    }
    switch (Step)
    {
    case 2: Take(HudButton(), TEXT("open_alliance_from_hud"), 3); break;
    case 3: Shot(1, TEXT("01_alliance_welcome"), 4); break;
    case 4: Take(Button(TEXT("AllianceWelcomeJoin")), TEXT("open_join_list"), 5); break;
    case 5: Shot(2, TEXT("02_join_default_five"), 6); break;
    case 6: Type(TEXT("type_ROK"), TEXT("ROK"), 7); break;
    case 7: Take(Button(TEXT("AllianceSearch")), TEXT("search_ROK"), 8); break;
    case 8: Shot(3, TEXT("03_ROK_original_1"), 9); break;
    case 9: Type(TEXT("type_sNoW"), TEXT("sNoW"), 10); break;
    case 10: Take(Button(TEXT("AllianceSearch")), TEXT("search_sNoW"), 11); break;
    case 11: Shot(4, TEXT("04_sNoW_original_0"), 12); break;
    case 12: Type(TEXT("type_RO"), TEXT("RO"), 13); break;
    case 13: Take(Button(TEXT("AllianceSearch")), TEXT("search_RO_short"), 14); break;
    case 14: Shot(5, TEXT("05_RO_minimum"), 15); break;
    case 15: Type(TEXT("type_ZZZ"), TEXT("ZZZ"), 16); break;
    case 16: Take(Button(TEXT("AllianceSearch")), TEXT("search_ZZZ"), 17); break;
    case 17: Shot(6, TEXT("06_ZZZ_no_result"), 18); break;
    case 18: Type(TEXT("clear_search"), TEXT(""), 19); break;
    case 19: Take(Button(TEXT("AllianceSearch")), TEXT("search_empty_restore"), 20); break;
    case 20: Shot(7, TEXT("07_empty_restores_five"), 21); break;
    case 21: Type(TEXT("type_ROK_again"), TEXT("ROK"), 22); break;
    case 22: Take(Button(TEXT("AllianceSearch")), TEXT("search_ROK_again"), 23); break;
    case 23: Shot(8, TEXT("08_ROK_before_row"), 24); break;
    case 24: Take(Button(TEXT("AllianceJoinRowHit_1")), TEXT("select_visible_original_1"), 25); break;
    case 25: Shot(9, TEXT("09_row_1_preserves_draft"), 26); break;
    case 26: Take(Button(TEXT("AllianceJoinBack")), TEXT("back_to_welcome"), 27); break;
    case 27: Shot(10, TEXT("10_welcome_reset"), 28); break;
    case 28: Take(Button(TEXT("AllianceWelcomeJoin")), TEXT("reenter_join_reset"), 29); break;
    case 29: Shot(11, TEXT("11_join_reset_five"), 30); break;
    case 30: Take(Button(TEXT("FeatureFrameClose")), TEXT("close_to_hud"), 31); break;
    case 31: Shot(12, TEXT("12_hud_closed"), 32); break;
    case 32: Take(HudButton(), TEXT("reopen_alliance_from_hud"), 33); break;
    case 33: Shot(13, TEXT("13_welcome_reopened"), 34); break;
    case 34: Take(Button(TEXT("AllianceWelcomeJoin")), TEXT("reopen_join_reset"), 35); break;
    case 35: Shot(14, TEXT("14_join_reopened_five"), 36); break;
    case 36: Take(Button(TEXT("FeatureFrameClose")), TEXT("final_close_to_hud"), 37); break;
    case 37: Shot(15, TEXT("15_final_hud"), 38); break;
    case 38: Finish(); break;
    default: Finish(TEXT("Invalid fixture audit step")); break;
    }
}

