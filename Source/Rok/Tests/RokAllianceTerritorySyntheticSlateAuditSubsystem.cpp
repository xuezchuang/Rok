#include "RokAllianceTerritorySyntheticSlateAuditSubsystem.h"

#include "../RokAllianceTerritoryReadonlyData.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokServerSubsystem.h"
#include "../RokTrainArmyMeshData.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Null/NullPlatformApplicationMisc.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace RokTerritorySynthetic
{
constexpr TCHAR City[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR Flag[] = TEXT("-RokAllianceTerritorySyntheticSlateAuditV1");
struct FPin { const TCHAR* Path; const TCHAR* Sha; };
// These are staged Shadow product bytes, not the mutable live checkout.
constexpr FPin Pins[] = {
    {TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
    {TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("5b896308b0327f5e04c8f6434625a15703145f399893cd1306404b408724ae83")},
    {TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("a421b7c542f3f40bca8883960e0bea17c64b2fb956da46a1f70ad151f2923019")},
    {TEXT("Source/Rok/RokAllianceTerritoryReadonlyData.cpp"), TEXT("a217ccb749aa8b6c1c9dac33f32f0667ed54fe3855656674a16cfe40310077a1")},
    {TEXT("Source/Rok/RokAllianceTerritoryReadonlyData.h"), TEXT("4f41df63128530fee13e7e2588c929f25c292a5ff375979faa9b52bd3028ed39")},
};
FString HashFile(const FString& Path)
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
FString Text(const URokUnityFeatureScreenWidget* Screen, const TCHAR* Name)
{
    const UTextBlock* Label = Screen ? Cast<UTextBlock>(Screen->GetWidgetFromName(Name)) : nullptr;
    return Label && Visible(Label) ? Label->GetText().ToString() : FString();
}
FString Foreground()
{
#if PLATFORM_WINDOWS
    return FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
#else
    return TEXT("unsupported_platform");
#endif
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
    return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
        | (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
TSharedRef<FJsonObject> Point(const FVector2D& P)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"), P.X); Json->SetNumberField(TEXT("y"), P.Y);
    return Json;
}
int32 TypeInGroup(int32 Group, int32 Cursor)
{
    for (const auto& Row : RokAllianceTerritoryReadonly::GetRows())
        if (Row.GroupIndex == Group && Cursor-- == 0) return Row.UnityType;
    return 0;
}
bool OwnOffscreenWindow(const UWorld* World)
{
    const UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
    return Window.IsValid() && Window->IsVisible()
        && Window->GetViewportSize().Equals(FVector2D(1440, 810), .1f)
        && Native.IsValid() && Native->GetOSWindowHandle() == nullptr && Native->IsRenderingOffScreen()
        && FNullPlatformApplicationMisc::IsUsingNullApplication();
}
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
    return false;
#else
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return Cast<UWorld>(Outer) && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceTerritorySyntheticSlateAuditV1"))
        && FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
        && FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"))
        && FParse::Param(FCommandLine::Get(), TEXT("game"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAllianceTerritorySyntheticSlateAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game;
}
TStatId URokAllianceTerritorySyntheticSlateAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokAllianceTerritorySyntheticSlateAuditSubsystem, STATGROUP_Tickables);
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StartedAt = FPlatformTime::Seconds(); StepStartedAt = StartedAt;
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this, &URokAllianceTerritorySyntheticSlateAuditSubsystem::ScreenshotProcessed);
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    VirtualUser.Reset();
    if (!bFinished && !Directory.IsEmpty())
    {
        Failures.Add(TEXT("World/subsystem deinitialized before Territory audit completed"));
        bFinished = true;
        Save(false);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    Super::Deinitialize();
}
bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Identity() const
{
    const UWorld* World = GetWorld();
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
    FPaths::NormalizeDirectoryName(Project); FPaths::NormalizeDirectoryName(Engine);
    return World && World->WorldType == EWorldType::Game && World->GetOutermost()->GetName() == RokTerritorySynthetic::City
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok/Saved/UIB1"))
        && FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
        && FEngineVersion::Current().GetMajor() == 5 && FEngineVersion::Current().GetMinor() == 8
        && FApp::IsGame() && FSlateApplication::IsInitialized()
        && FSlateApplication::Get().IsRenderingOffScreen()
        && RokTerritorySynthetic::OwnOffscreenWindow(World);
}
bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
    if (!Condition) Finish(Reason);
    return Condition;
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Begin()
{
    if (!Expect(Identity(), TEXT("Expected exact Shadow UE5.8 city GameViewport Null offscreen SWindow"))) return false;
    const TCHAR* Cursor = FCommandLine::Get();
    FString Token;
    int32 AuditFlags = 0, LocalFlags = 0, FixtureFlags = 0, OutputFlags = 0, GameFlags = 0, OffscreenFlags = 0;
    while (FParse::Token(Cursor, Token, false))
    {
        if (Token.Equals(RokTerritorySynthetic::Flag, ESearchCase::IgnoreCase)) ++AuditFlags;
        else if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) ++LocalFlags;
        else if (Token.Equals(TEXT("-RokOpenModal=alliance_joined"), ESearchCase::IgnoreCase)) ++FixtureFlags;
        else if (Token.StartsWith(TEXT("-RokTerritorySyntheticOutputV1="), ESearchCase::IgnoreCase)) ++OutputFlags;
        else if (Token.Equals(TEXT("-game"), ESearchCase::IgnoreCase)) ++GameFlags;
        else if (Token.Equals(TEXT("-RenderOffScreen"), ESearchCase::IgnoreCase)) ++OffscreenFlags;
        else if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
            return Expect(false, TEXT("Unexpected Rok fixture or diagnostic command-line flag"));
    }
    if (!Expect(AuditFlags == 1 && LocalFlags == 1 && FixtureFlags == 1 && OutputFlags == 1
        && GameFlags == 1 && OffscreenFlags == 1, TEXT("Exact audit/local/fixture/output/game/offscreen flags required"))) return false;
    FString Output;
    if (!Expect(FParse::Value(FCommandLine::Get(), TEXT("RokTerritorySyntheticOutputV1="), Output)
        && !FPaths::IsRelative(Output), TEXT("Root-owned absolute fresh output required"))) return false;
    Output = FPaths::ConvertRelativePathToFull(Output);
    FPaths::NormalizeDirectoryName(Output);
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
        / TEXT("PendingAllianceTerritorySyntheticSlateAuditV1/Results"));
    if (!Expect(FPaths::CollapseRelativeDirectories(Output)
        && FPaths::IsSamePath(FPaths::GetPath(Output), Root)
        && FPaths::GetCleanFilename(Output).Len() >= 8
        && IFileManager::Get().DirectoryExists(*Root)
        && !IFileManager::Get().DirectoryExists(*Output)
        && !IFileManager::Get().FileExists(*Output), TEXT("Output must be a fresh direct child of Shadow Saved Results"))) return false;
#if PLATFORM_WINDOWS
    const DWORD RootAttributes = ::GetFileAttributesW(*Root);
    FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FPaths::NormalizeDirectoryName(Saved);
    const FString AuditParent = FPaths::GetPath(Root);
    const DWORD ParentAttributes = ::GetFileAttributesW(*AuditParent);
    const DWORD SavedAttributes = ::GetFileAttributesW(*Saved);
    if (!Expect(RootAttributes != INVALID_FILE_ATTRIBUTES
        && ParentAttributes != INVALID_FILE_ATTRIBUTES && SavedAttributes != INVALID_FILE_ATTRIBUTES
        && (RootAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0
        && (ParentAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0
        && (SavedAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
        TEXT("Output parent or Shadow Saved is a reparse point"))) return false;
#endif
    Directory = Output;
    if (!Expect(IFileManager::Get().MakeDirectory(*Directory, true), TEXT("Failed to create audit output"))) return false;
    Inputs = MakeShared<FJsonObject>();
    for (const auto& Pin : RokTerritorySynthetic::Pins)
    {
        const FString Actual = RokTerritorySynthetic::HashFile(FPaths::ProjectDir() / Pin.Path);
        if (!Expect(Actual == Pin.Sha, TEXT("Pinned Shadow product/source data differs"))) return false;
        Inputs->SetStringField(Pin.Path, Actual);
    }
    int32 Controllers = 0;
    for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
    {
        ++Controllers;
        if (ARokPlayerController* Rok = Cast<ARokPlayerController>(*It)) Controller = Rok;
    }
    TArray<UUserWidget*> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
    int32 Huds = 0;
    for (UUserWidget* Widget : Widgets)
        if (Widget && Widget->GetWorld() == GetWorld() && Widget->IsInViewport())
        {
            ++Huds; Hud = Cast<URokMainHudWidget>(Widget);
        }
    Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
    Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
    Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    UGameInstance* Game = GetWorld()->GetGameInstance();
    Runtime = Game ? Game->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
    Server = Game ? Game->GetSubsystem<URokServerSubsystem>() : nullptr;
    if (!Expect(Controllers == 1 && Controller.IsValid() && Controller.Get() == GetWorld()->GetFirstPlayerController()
        && Controller->GetPawn() && Huds == 1 && Hud.IsValid() && Hud->GetOwningPlayer() == Controller.Get()
        && Screen.IsValid() && Modal.IsValid() && !Modal->IsModalOpen() && Buildings.IsValid()
        && Runtime.IsValid() && Runtime->IsRuntimeStateLoaded() && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
        && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f)
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("FeatureFrameTitle")) == TEXT("联盟")
        && RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_1")))
        && !Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceBack")),
        TEXT("Expected real joined-alliance page-zero HUD, no Territory reference yet"))) return false;
    const auto Profile = Runtime->GetPlayerProfileState();
    if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId
        && Profile.RoleId.IsEmpty() && !Profile.bHasCivilization && Profile.CivilizationId == 0,
        TEXT("Expected anonymous local-preview account state"))) return false;
    FRokLocalConstructionTicket Ticket;
#if WITH_DEV_AUTOMATION_TESTS
    const int32 PendingUpgrades = Buildings->GetPendingCompletedUpgradeCountForTesting();
#else
    const int32 PendingUpgrades = -1;
#endif
    if (!Expect(Buildings->GetActiveUpgradeCount() == 0
        && Buildings->GetActiveLocalConstructionCount() == 0
        && PendingUpgrades == 0
        && !Buildings->GetLocalConstructionTicket(1, Ticket),
        TEXT("Territory audit requires idle local queues and no construction ticket"))) return false;
    const auto& Rows = RokAllianceTerritoryReadonly::GetRows();
    const auto& Groups = RokAllianceTerritoryReadonly::GetGroups();
    const int32 Counts[] = {3, 4, 1, 4};
    if (!Expect(Rows.Num() == 12 && Groups.Num() == 4, TEXT("Static territory table size differs"))) return false;
    for (int32 Group = 0; Group < 4; ++Group)
    {
        int32 Count = 0;
        for (const auto& Row : Rows) if (Row.GroupIndex == Group) ++Count;
        if (!Expect(Groups[Group].Index == Group && Count == Counts[Group], TEXT("Territory group membership differs"))) return false;
    }
    for (int32 Type = 1; Type <= 12; ++Type)
    {
        const auto* Row = RokAllianceTerritoryReadonly::FindRow(Type);
        if (!Expect(Row && Row->UnityType == Type && Row->NameLocalizationId > 0
            && Row->DescriptionLocalizationId > 0
            && !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->NameLocalizationId).IsEmpty()
            && !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DescriptionLocalizationId).IsEmpty()
            && (Row->DetailLocalizationId == 0
                || !RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DetailLocalizationId).IsEmpty())
            && Row->bSourceIconMissing == (Type == 7 || Type == 11),
            TEXT("Territory source text/detail/icon identity differs"))) return false;
    }
    VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(1);
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0,
        TEXT("Dedicated nonhardware Slate user unavailable"))) return false;
    ForegroundAtBegin = RokTerritorySynthetic::Foreground();
    BaselineBusiness = BusinessDigest();
    bBaselineSet = !BaselineBusiness.IsEmpty();
    return Expect(bBaselineSet, TEXT("Business snapshot unavailable"));
}

FString URokAllianceTerritorySyntheticSlateAuditSubsystem::BusinessDigest() const
{
    if (!Buildings.IsValid() || !Runtime.IsValid() || !Server.IsValid() || !Controller.IsValid()
        || !Controller->GetPawn()) return FString();
    auto Json = MakeShared<FJsonObject>();
    const auto Wallet = Buildings->GetCurrentResources();
    const auto Rate = Buildings->GetProductionRates();
    Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
    Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
    Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
    Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
    Json->SetNumberField(TEXT("rate_food"), Rate.FoodPerHour);
    Json->SetNumberField(TEXT("rate_wood"), Rate.WoodPerHour);
    Json->SetNumberField(TEXT("rate_stone"), Rate.StonePerHour);
    Json->SetNumberField(TEXT("rate_gold"), Rate.GoldPerHour);
    Json->SetNumberField(TEXT("owned_queues"), Buildings->GetOwnedBuildQueueCount());
    Json->SetNumberField(TEXT("active_upgrades"), Buildings->GetActiveUpgradeCount());
    Json->SetNumberField(TEXT("active_construction"), Buildings->GetActiveLocalConstructionCount());
#if WITH_DEV_AUTOMATION_TESTS
    Json->SetNumberField(TEXT("pending_completed_upgrades"), Buildings->GetPendingCompletedUpgradeCountForTesting());
#endif
    FRokLocalConstructionTicket Ticket;
    Json->SetBoolField(TEXT("construction_ticket_1_exists"), Buildings->GetLocalConstructionTicket(1, Ticket));
    const auto Profile = Runtime->GetPlayerProfileState();
    Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
    Json->SetBoolField(TEXT("profile_has_role"), Profile.bHasRoleId);
    Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
    Json->SetNumberField(TEXT("profile_preview_score"), Profile.LocalPreviewScore);
    Json->SetNumberField(TEXT("profile_hero_score"), Profile.LocalHeroScore);
    Json->SetNumberField(TEXT("profile_troop_power"), Profile.LocalTroopPower);
    const auto Task = Runtime->GetActiveTask();
    Json->SetNumberField(TEXT("task_id"), Task.TaskId);
    Json->SetNumberField(TEXT("task_current"), Task.Current);
    Json->SetNumberField(TEXT("task_target"), Task.Target);
    Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
    Json->SetNumberField(TEXT("http_dispatch"), Server->GetHttpDispatchAttemptCount());
    Json->SetNumberField(TEXT("http_suppressed"), Server->GetSuppressedLocalRequestCount());
    const auto Placement = Controller->GetCityPlacementPreviewSnapshot();
    Json->SetNumberField(TEXT("primary_world_presses"), static_cast<double>(Placement.PrimaryPressEventCount));
    const FVector P = Controller->GetPawn()->GetActorLocation();
    Json->SetNumberField(TEXT("camera_x"), P.X); Json->SetNumberField(TEXT("camera_y"), P.Y);
    Json->SetNumberField(TEXT("camera_z"), P.Z);
    TArray<FString> Actors;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->ActorHasTag(TEXT("RokLocalConstructedBuilding"))) Actors.Add(It->GetPathName());
    Actors.Sort();
    TArray<TSharedPtr<FJsonValue>> ActorJson;
    for (const FString& Path : Actors) ActorJson.Add(MakeShared<FJsonValueString>(Path));
    Json->SetArrayField(TEXT("constructed_actors"), ActorJson);
    return RokTerritorySynthetic::Encode(Json);
}
bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Boundary() const
{
    return Identity() && bBaselineSet && Server.IsValid() && Server->IsLocalOnlyRun()
        && Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
        && Modal.IsValid() && !Modal->IsModalOpen()
        && Runtime.IsValid() && Runtime->GetPlayerProfileState().StateKind == TEXT("local_preview")
        && BusinessDigest() == BaselineBusiness
        && RokTerritorySynthetic::Foreground() == ForegroundAtBegin;
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Ready(int32 Phase, int32 Group, int32 Type) const
{
    if (!Boundary() || !Hud.IsValid() || !Hud->IsInViewport() || !Screen.IsValid()
        || Screen->GetPendingActionIdForAudit() != INDEX_NONE) return false;
    if (Phase == 6) return !Screen->IsScreenOpen()
        && RokTerritorySynthetic::Visible(Hud->GetWidgetFromName(TEXT("AllianceFeatureButton")));
    if (!Screen->IsScreenOpen() || !RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")))
        || !RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))) return false;
    const bool bReference = Phase == 1 || Phase == 2 || Phase == 3 || Phase == 5;
    if (RokTerritorySynthetic::Text(Screen.Get(), TEXT("FeatureFrameTitle"))
        != (bReference ? TEXT("联盟领土静态配置参考") : TEXT("联盟"))) return false;
    if (!bReference) return RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("AllianceFeatureHit_1")))
        && Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceBack")) == nullptr;
    if (!RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceBack")))
        || RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceBoundary"))
            != TEXT("仅显示12种静态配置；联盟实际领土、拥有数量、建筑状态、坐标、产出与权限未同步。")) return false;
    if (Phase == 3)
    {
        const auto* Row = RokAllianceTerritoryReadonly::FindRow(Type);
        if (!Row || Row->GroupIndex != Group || Row->NameLocalizationId <= 0
            || Row->DescriptionLocalizationId <= 0
            || !RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReturnToDefinitions")))
            || Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceScroll")) != nullptr) return false;
        const FString Icon = Row->bSourceIconMissing ? TEXT("源图标缺失")
            : (Row->SourceIconReference[0] == 0 ? TEXT("源未配置图标") : TEXT("图标未接入"));
        const bool bDetail = Row->DetailLocalizationId > 0
            ? RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionRules"))
                == RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DetailLocalizationId)
            : Screen->GetWidgetFromName(TEXT("AllianceTerritoryDefinitionRules")) == nullptr;
        return bDetail
            && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionName"))
                == RokAllianceTerritoryReadonly::GetChineseSourceText(Row->NameLocalizationId)
            && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionDescription"))
                == RokAllianceTerritoryReadonly::GetChineseSourceText(Row->DescriptionLocalizationId)
            && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionIconState")) == Icon;
    }
    if (!RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(TEXT("AllianceTerritoryReferenceScroll")))
        || Screen->GetWidgetFromName(TEXT("AllianceTerritoryReturnToDefinitions")) != nullptr
        || RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceSelection"))
            != TEXT("点击分组展开配置定义；实际领土记录未同步")) return false;
    for (const auto& Header : RokAllianceTerritoryReadonly::GetGroups())
    {
        if (!RokTerritorySynthetic::Visible(Screen->GetWidgetFromName(
                *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Header.Index)))
            || RokTerritorySynthetic::Text(Screen.Get(),
                *FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Header.Index))
                != RokAllianceTerritoryReadonly::GetChineseSourceText(Header.TitleLocalizationId)
            || RokTerritorySynthetic::Text(Screen.Get(),
                *FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Header.Index))
                != RokAllianceTerritoryReadonly::GetChineseSourceText(Header.HeaderLocalizationId)) return false;
    }
    const int32 Expected[] = {3, 4, 1, 4};
    int32 Found = 0;
    for (const auto& Row : RokAllianceTerritoryReadonly::GetRows())
    {
        const UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(
            *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Row.UnityType)));
        if (Row.GroupIndex != Group) { if (Button) return false; continue; }
        const FString Name = Row.GroupIndex == 3
            ? FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Row.UnityType)
            : FString::Printf(TEXT("AllianceTerritoryName_%d"), Row.UnityType);
        const FString IconName = Row.GroupIndex == 3
            ? FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Row.UnityType)
            : FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Row.UnityType);
        const FString Icon = Row.bSourceIconMissing ? TEXT("源图标缺失")
            : (Row.SourceIconReference[0] == 0 ? TEXT("源未配置图标") : TEXT("图标未接入"));
        if (!Button || !RokTerritorySynthetic::Visible(Button)
            || RokTerritorySynthetic::Text(Screen.Get(), *Name)
                != RokAllianceTerritoryReadonly::GetChineseSourceText(Row.NameLocalizationId)
            || RokTerritorySynthetic::Text(Screen.Get(), *IconName) != Icon) return false;
        ++Found;
    }
    return Group == INDEX_NONE ? Found == 0 : Group >= 0 && Group < 4 && Found == Expected[Group];
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Observe(int32 Phase, int32 Group, int32 Type, const TCHAR* Label)
{
    if (GFrameCounter <= ActionFrame || !Ready(Phase, Group, Type)) return false;
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("phase"), Label);
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetNumberField(TEXT("action_frame"), static_cast<double>(ActionFrame));
    Json->SetNumberField(TEXT("group_inferred_from_exact_visible_rows"), Group);
    Json->SetNumberField(TEXT("type_inferred_from_exact_detail_text"), Type);
    Json->SetBoolField(TEXT("reference_open_inferred_from_title_and_boundary"),
        Phase == 1 || Phase == 2 || Phase == 3 || Phase == 5);
    Json->SetBoolField(TEXT("business_unchanged"), Boundary());
    Json->SetNumberField(TEXT("http_dispatch_attempts"), Server->GetHttpDispatchAttemptCount());
    Json->SetNumberField(TEXT("http_suppressed_requests"), Server->GetSuppressedLocalRequestCount());
    Observations.Add(MakeShared<FJsonValueObject>(Json));
    return true;
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::TargetInPath(UButton* Target) const
{
    if (!VirtualUser.IsValid() || !Target || !RokTerritorySynthetic::Visible(Target) || !Target->GetIsEnabled()) return false;
    const FGeometry Geometry = Target->GetCachedGeometry();
    if (Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0) return false;
    const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
    FSlateApplication& App = FSlateApplication::Get();
    const FWidgetPath Path = App.LocateWindowUnderMouse(
        Center, App.GetInteractiveTopLevelWindows(), false, VirtualUser->GetUserIndex());
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    if (!Path.IsValid() || !Window.IsValid() || &Path.GetWindow().Get() != Window.Get()) return false;
    const TSharedRef<SWidget> Slate = Target->TakeWidget();
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        if (&Path.Widgets[Index].Widget.Get() == &Slate.Get()) return true;
    return false;
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::WheelTowards(UButton* Target)
{
    UScrollBox* Scroll = Screen.IsValid() ? Cast<UScrollBox>(Screen->GetWidgetFromName(
        TEXT("AllianceTerritoryReferenceScroll"))) : nullptr;
    if (!Expect(Scroll && RokTerritorySynthetic::Visible(Scroll) && Target && WheelCount < 32 && Boundary(),
        TEXT("Territory scroll target unavailable"))) return false;
    const FGeometry G = Scroll->GetCachedGeometry();
    if (!Expect(G.GetLocalSize().X > 0 && G.GetLocalSize().Y > 0,
        TEXT("Territory scroll geometry empty"))) return false;
    const FVector2D Center = G.LocalToAbsolute(G.GetLocalSize() * .5f);
    const UCanvasPanelSlot* TargetSlot = Cast<UCanvasPanelSlot>(Target->Slot);
    float TargetContentTop = 0.0f;
    const UWidget* Child = Target;
    // A clipped child can have stale cached absolute geometry. Canvas slots still
    // describe its position in the real ScrollBox content built by the screen.
    for (; Child && Child != Scroll; Child = Child->GetParent())
        if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Child->Slot))
            TargetContentTop += Slot->GetPosition().Y;
    if (!Expect(Child == Scroll && TargetSlot && TargetSlot->GetSize().Y > 0.0f
        && FMath::IsFinite(TargetContentTop), TEXT("Territory target is not in scroll content"))) return false;
    const float Before = Scroll->GetScrollOffset();
    const float TargetContentCenter = TargetContentTop + TargetSlot->GetSize().Y * .5f;
    const float Delta = TargetContentCenter < Before ? 3.0f
        : TargetContentCenter >= Before + G.GetLocalSize().Y ? -3.0f : 0.0f;
    if (!Expect(Delta != 0.0f,
        TEXT("Territory target is in calculated viewport but absent exact Slate path"))) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const int32 User = VirtualUser->GetUserIndex();
    const FString ForegroundBefore = RokTerritorySynthetic::Foreground();
    const TSharedPtr<SWidget> FocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT CursorBefore = {}, CursorAfter = {};
    const bool bCursorBefore = ::GetCursorPos(&CursorBefore) != 0;
#endif
    App.ProcessMouseMoveEvent(FPointerEvent(User, 0, Center, LastPointer,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()), true);
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center,
        App.GetInteractiveTopLevelWindows(), false, User);
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const TSharedRef<SWidget> ScrollSlate = Scroll->TakeWidget();
    bool bScrollPath = false;
    for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
        bScrollPath |= &Path.Widgets[Index].Widget.Get() == &ScrollSlate.Get();
    if (!Expect(Path.IsValid() && Window.IsValid() && &Path.GetWindow().Get() == Window.Get()
        && bScrollPath && RokTerritorySynthetic::OwnOffscreenWindow(GetWorld()),
        TEXT("Wheel did not target exact own offscreen ScrollBox"))) return false;
    const bool bHandled = App.ProcessMouseWheelOrGestureEvent(FPointerEvent(User, 0, Center, Center,
        TSet<FKey>(), EKeys::Invalid, Delta, FModifierKeysState()), nullptr);
    const float After = Scroll->GetScrollOffset();
#if PLATFORM_WINDOWS
    const bool bCursorAfter = ::GetCursorPos(&CursorAfter) != 0;
    const bool bCursorSafe = bCursorBefore && bCursorAfter
        && CursorBefore.x == CursorAfter.x && CursorBefore.y == CursorAfter.y;
#else
    const bool bCursorSafe = false;
#endif
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("target"), Target->GetName());
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetNumberField(TEXT("virtual_user_index"), User);
    Json->SetNumberField(TEXT("wheel_delta"), Delta);
    Json->SetNumberField(TEXT("target_content_center_y"), TargetContentCenter);
    Json->SetNumberField(TEXT("scroll_viewport_height"), G.GetLocalSize().Y);
    Json->SetNumberField(TEXT("offset_before"), Before);
    Json->SetNumberField(TEXT("offset_after"), After);
    Json->SetBoolField(TEXT("scroll_exact_hit_path"), bScrollPath);
    Json->SetBoolField(TEXT("wheel_handled"), bHandled);
    Json->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == RokTerritorySynthetic::Foreground());
    Json->SetBoolField(TEXT("user0_focus_unchanged"), FocusBefore == App.GetUserFocusedWidget(0));
    Json->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorSafe);
    Wheels.Add(MakeShared<FJsonValueObject>(Json)); ++WheelCount;
    return Expect(bHandled && (After - Before) * (-Delta) > 0 && Boundary()
        && ForegroundBefore == RokTerritorySynthetic::Foreground()
        && FocusBefore == App.GetUserFocusedWidget(0) && bCursorSafe,
        TEXT("Actual virtual-user wheel missing displacement or input/business boundary changed"));
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Click(UButton* Target, int32 ExpectedActionId, const TCHAR* Label)
{
    if (!Expect(VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0 && Boundary(),
        TEXT("Virtual user or business boundary lost before click"))) return false;
    if (!Expect(Screen.IsValid() && Screen->GetPendingActionIdForAudit() == INDEX_NONE,
        TEXT("Earlier action has not finished before next Slate click"))) return false;
    if (!Target || !RokTerritorySynthetic::Visible(Target) || !Target->GetIsEnabled()) return false;
    const FGeometry G = Target->GetCachedGeometry();
    const FVector2D Size = G.GetLocalSize();
    if (Size.X <= 0 || Size.Y <= 0) return false;
    const FVector2D TopLeft = G.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D BottomRight = G.LocalToAbsolute(Size);
    if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
        || !StableBottomRight.Equals(BottomRight, .25f))
    {
        StableTarget = Target; StableTopLeft = TopLeft; StableBottomRight = BottomRight;
        StableFrame = GFrameCounter; ClickStableFrames = 1; return false;
    }
    if (GFrameCounter <= StableFrame) return false;
    StableFrame = GFrameCounter;
    if (++ClickStableFrames < 2) return false;
    FSlateApplication& App = FSlateApplication::Get();
    const int32 User = VirtualUser->GetUserIndex();
    const FVector2D Center = G.LocalToAbsolute(Size * .5f);
    const FVector2D Away = G.LocalToAbsolute(FVector2D(-30.0f, -30.0f));
    const FString ForegroundBefore = RokTerritorySynthetic::Foreground();
    const TSharedPtr<SWidget> FocusBefore = App.GetUserFocusedWidget(0);
#if PLATFORM_WINDOWS
    POINT CursorBefore = {}, CursorAfter = {};
    const bool bCursorBefore = ::GetCursorPos(&CursorBefore) != 0;
#endif
    App.ProcessMouseMoveEvent(FPointerEvent(User, 0, Away, LastPointer,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()), true);
    App.ProcessMouseMoveEvent(FPointerEvent(User, 0, Center, Away,
        TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()), true);
    LastPointer = Center;
    const FWidgetPath Path = App.LocateWindowUnderMouse(Center,
        App.GetInteractiveTopLevelWindows(), false, User);
    const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
    const bool bOwn = Window.IsValid() && Path.IsValid()
        && &Path.GetWindow().Get() == Window.Get() && RokTerritorySynthetic::OwnOffscreenWindow(GetWorld());
    const TSharedRef<SWidget> Slate = Target->TakeWidget();
    int32 ButtonIndex = INDEX_NONE;
    bool bUmgPath = false;
    TArray<TSharedPtr<FJsonValue>> PathTypes;
    for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
    {
        const SWidget* Widget = &Path.Widgets[Index].Widget.Get();
        const FString Type = Widget->GetTypeAsString();
        if (Widget == &Slate.Get()) ButtonIndex = Index;
        bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
        PathTypes.Add(MakeShared<FJsonValueString>(Type));
    }
    const bool bExactButton = ButtonIndex != INDEX_NONE
        && Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("label"), Label);
    Json->SetStringField(TEXT("target"), Target->GetName());
    Json->SetNumberField(TEXT("expected_product_action_id_from_pinned_constructor"), ExpectedActionId);
    Json->SetStringField(TEXT("target_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Target)));
    Json->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(&Slate.Get())));
    Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
    Json->SetNumberField(TEXT("virtual_user_index"), User);
    Json->SetNumberField(TEXT("virtual_user_slot"), VirtualUser->GetVirtualUserIndex());
    Json->SetObjectField(TEXT("center"), RokTerritorySynthetic::Point(Center));
    Json->SetArrayField(TEXT("hit_path_types"), PathTypes);
    Json->SetNumberField(TEXT("exact_button_index"), ButtonIndex);
    Json->SetBoolField(TEXT("umg_path"), bUmgPath);
    Json->SetBoolField(TEXT("own_null_offscreen_window"), bOwn);
    Json->SetBoolField(TEXT("exact_button"), bExactButton);
    Json->SetBoolField(TEXT("hovered"), Target->IsHovered());
    Json->SetStringField(TEXT("foreground_before"), ForegroundBefore);
    if (!bOwn || !bUmgPath || !bExactButton || !Target->IsHovered())
    {
        Actions.Add(MakeShared<FJsonValueObject>(Json));
        return Expect(false, TEXT("Click did not reach exact hovered UMG SButton in own offscreen path"));
    }
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
        FPointerEvent(User, 0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
    const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(User, 0, Center, Center,
        TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
    // The product defers UMG action execution to its next tick. This public
    // read-only hook proves the exact clicked action before that tick consumes it.
    const int32 PendingAction = Screen.IsValid() ? Screen->GetPendingActionIdForAudit() : INDEX_NONE;
#if PLATFORM_WINDOWS
    const bool bCursorAfter = ::GetCursorPos(&CursorAfter) != 0;
    const bool bCursorSafe = bCursorBefore && bCursorAfter
        && CursorBefore.x == CursorAfter.x && CursorBefore.y == CursorAfter.y;
#else
    const bool bCursorSafe = false;
#endif
    Json->SetBoolField(TEXT("down_dispatch_return"), Down);
    Json->SetBoolField(TEXT("up_dispatch_return"), Up);
    Json->SetBoolField(TEXT("dispatch_return_proves_action"), false);
    Json->SetNumberField(TEXT("actual_pending_action_id_after_slate_up"), PendingAction);
    Json->SetBoolField(TEXT("foreground_unchanged"), ForegroundBefore == RokTerritorySynthetic::Foreground());
    Json->SetBoolField(TEXT("user0_focus_unchanged"), FocusBefore == App.GetUserFocusedWidget(0));
    Json->SetBoolField(TEXT("hardware_cursor_unchanged"), bCursorSafe);
    Json->SetBoolField(TEXT("business_unchanged"), Boundary());
    Json->SetBoolField(TEXT("os_mouse_or_focus_api_called"), false);
    Actions.Add(MakeShared<FJsonValueObject>(Json));
    StableTarget.Reset(); ClickStableFrames = 0;
    return Expect(Up && PendingAction == ExpectedActionId
        && ForegroundBefore == RokTerritorySynthetic::Foreground()
        && FocusBefore == App.GetUserFocusedWidget(0) && bCursorSafe && Boundary(),
        TEXT("Exact pending action, Slate up/input or business boundary changed"));
}

void URokAllianceTerritorySyntheticSlateAuditSubsystem::Capture(const TCHAR* Phase, int32 Next)
{
    if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested() && Boundary(),
        TEXT("Screenshot already pending or business boundary changed"))) return;
    Pending = MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("phase"), Phase);
    Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
    Pending->SetStringField(TEXT("foreground_at_request"), RokTerritorySynthetic::Foreground());
    Pending->SetStringField(TEXT("title_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("FeatureFrameTitle")));
    Pending->SetStringField(TEXT("boundary_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceBoundary")));
    Pending->SetStringField(TEXT("selection_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceSelection")));
    Pending->SetStringField(TEXT("detail_name_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionName")));
    Pending->SetStringField(TEXT("detail_description_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionDescription")));
    Pending->SetStringField(TEXT("detail_icon_at_request"), RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionIconState")));
    PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
    Pending->SetStringField(TEXT("png"), PendingPath);
    NextStep = Next; bScreenshotDone = false;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
    TArray<uint8> Bytes;
    if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
        && FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
        && FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
        && RokTerritorySynthetic::PngDimension(Bytes, 16) == 1440
        && RokTerritorySynthetic::PngDimension(Bytes, 20) == 810,
        TEXT("Processed screenshot missing or not 1440x810 PNG"))) return;
    if (!Expect(Boundary() && RokTerritorySynthetic::Foreground() == Pending->GetStringField(TEXT("foreground_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("FeatureFrameTitle")) == Pending->GetStringField(TEXT("title_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceBoundary"))
            == Pending->GetStringField(TEXT("boundary_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryReferenceSelection"))
            == Pending->GetStringField(TEXT("selection_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionName"))
            == Pending->GetStringField(TEXT("detail_name_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionDescription"))
            == Pending->GetStringField(TEXT("detail_description_at_request"))
        && RokTerritorySynthetic::Text(Screen.Get(), TEXT("AllianceTerritoryDefinitionIconState"))
            == Pending->GetStringField(TEXT("detail_icon_at_request"))
        && static_cast<double>(GFrameCounter) >= Pending->GetNumberField(TEXT("requested_frame")),
        TEXT("Screenshot same-frame state or business changed"))) return;
    Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
    Pending->SetNumberField(TEXT("png_bytes"), Bytes.Num());
    Pending->SetNumberField(TEXT("png_width"), 1440);
    Pending->SetNumberField(TEXT("png_height"), 810);
    Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetBoolField(TEXT("business_unchanged_at_processed"), true);
    ScreenshotFrame = GFrameCounter; bScreenshotDone = true;
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::AdvanceCapture()
{
    if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
    if (!Expect(Boundary(), TEXT("Business boundary changed after processed screenshot"))) return;
    Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
    Captures.Add(MakeShared<FJsonValueObject>(Pending));
    Pending.Reset(); Enter(NextStep);
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::Enter(int32 Next)
{
    Step = Next; StepStartedAt = FPlatformTime::Seconds();
    StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}

bool URokAllianceTerritorySyntheticSlateAuditSubsystem::Save(bool bComplete) const
{
    if (Directory.IsEmpty()) return false;
    auto Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("schema"), TEXT("rok.alliance.territory.synthetic_slate.static_reference.v1"));
    Json->SetBoolField(TEXT("complete"), bComplete);
    Json->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty()
        && Actions.Num() == 32 && Observations.Num() == 33 && Captures.Num() == 21);
    Json->SetBoolField(TEXT("runtime_business_or_owned_territory_accepted"), false);
    Json->SetBoolField(TEXT("human_visual_accepted"), false);
    Json->SetBoolField(TEXT("full_unity_territory_layout_accepted"), false);
    Json->SetBoolField(TEXT("os_window_hit_audit"), false);
    Json->SetBoolField(TEXT("os_mouse_or_focus_input_used"), false);
    Json->SetStringField(TEXT("scope"), TEXT("Joined Alliance static configuration, 4 groups, 12 details, 32 real virtual-user Slate clicks"));
    Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Json->SetStringField(TEXT("engine"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
    Json->SetNumberField(TEXT("virtual_user_index"), VirtualUser.IsValid() ? VirtualUser->GetUserIndex() : INDEX_NONE);
    Json->SetNumberField(TEXT("action_count"), Actions.Num());
    Json->SetNumberField(TEXT("observation_count"), Observations.Num());
    Json->SetNumberField(TEXT("wheel_count"), Wheels.Num());
    Json->SetNumberField(TEXT("capture_count"), Captures.Num());
    Json->SetStringField(TEXT("foreground_at_begin"), ForegroundAtBegin);
    Json->SetStringField(TEXT("foreground_at_finish"), RokTerritorySynthetic::Foreground());
    Json->SetBoolField(TEXT("business_snapshot_unchanged"), Boundary());
    Json->SetStringField(TEXT("baseline_business_json"), BaselineBusiness);
    Json->SetStringField(TEXT("final_business_json"), BusinessDigest());
    Json->SetBoolField(TEXT("http_zero"), Server.IsValid() && Server->GetHttpDispatchAttemptCount() == 0
        && Server->GetSuppressedLocalRequestCount() == 0);
    Json->SetObjectField(TEXT("input_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
    Json->SetArrayField(TEXT("actions"), Actions);
    Json->SetArrayField(TEXT("observations"), Observations);
    Json->SetArrayField(TEXT("wheels"), Wheels);
    Json->SetArrayField(TEXT("captures"), Captures);
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Json->SetArrayField(TEXT("failures"), Errors);
    return FFileHelper::SaveStringToFile(RokTerritorySynthetic::Encode(Json),
        *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokAllianceTerritorySyntheticSlateAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    bFinished = true;
    const bool bComplete = Failures.IsEmpty() && Actions.Num() == 32
        && Observations.Num() == 33 && Captures.Num() == 21 && Wheels.Num() > 0
        && Boundary() && VirtualUser.IsValid() && VirtualUser->GetUserIndex() > 0;
    if (!bComplete && Failures.IsEmpty()) Failures.Add(TEXT("Territory synthetic Slate evidence incomplete"));
    const bool bSaved = Save(bComplete);
    UE_LOG(LogTemp, Display, TEXT("Territory synthetic Slate audit %s: %s"),
        bComplete && bSaved ? TEXT("passed") : TEXT("failed"), *Directory);
    FPlatformMisc::RequestExitWithStatus(false, bComplete && bSaved ? 0 : 1);
}

void URokAllianceTerritorySyntheticSlateAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return;
    if (!Identity()) { Finish(TEXT("Audit left exact Shadow city Null offscreen window")); return; }
    const double Now = FPlatformTime::Seconds();
    if (Now - StartedAt > 300.0) { Finish(TEXT("Territory total wall-clock timeout")); return; }
    if (Now - StartedAt < 2.0) return;
    if (Pending.IsValid())
    {
        if (Now - StepStartedAt > 25.0) Finish(TEXT("Screenshot callback timeout"));
        else AdvanceCapture();
        return;
    }
    if (Now - StepStartedAt > 25.0) { Finish(TEXT("Territory audit step timeout")); return; }
    const auto Stable = [this](int32 Phase, int32 Group = INDEX_NONE, int32 Type = 0)
    {
        StableFrames = Ready(Phase, Group, Type) ? StableFrames + 1 : 0;
        return StableFrames >= 2;
    };
    const auto Button = [this](const FString& Name)
    {
        return Screen.IsValid() ? Cast<UButton>(Screen->GetWidgetFromName(*Name)) : nullptr;
    };
    const auto Press = [this, &Button](const FString& Name, int32 ActionId, const TCHAR* Label)
    {
        if (!Click(Button(Name), ActionId, Label)) return false;
        ActionFrame = GFrameCounter; return true;
    };
    switch (Step)
    {
    case 0:
        if (!bBegun) bBegun = Begin();
        if (bBegun) Enter(1);
        break;
    case 1:
        if (Stable(0) && Observe(0, INDEX_NONE, 0, TEXT("joined_fixture_before_651")))
            Capture(TEXT("00_joined_fixture"), 2);
        break;
    case 2:
        if (Press(TEXT("AllianceFeatureHit_1"), 651, TEXT("open_territory_651"))) Enter(3);
        break;
    case 3:
        if (Stable(1) && Observe(1, INDEX_NONE, 0, TEXT("reference_open")))
        { GroupCursor = 0; RowCursor = 0; Capture(TEXT("01_reference_open"), 4); }
        break;
    case 4:
        if (GroupCursor < 4)
        {
            UButton* Target = Button(FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), GroupCursor));
            if (!TargetInPath(Target)) { if (WheelTowards(Target)) Enter(4); break; }
            if (Press(FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), GroupCursor), 19620 + GroupCursor,
                TEXT("expand_territory_group_19620_19623"))) Enter(5);
        }
        else if (Press(TEXT("AllianceTerritoryReferenceBack"), 695, TEXT("back_to_joined_695"))) Enter(11);
        break;
    case 5:
        if (Stable(2, GroupCursor) && Observe(2, GroupCursor, 0, TEXT("group_expanded")))
        {
            const FString Phase = FString::Printf(TEXT("%02d_group_%d"), 2 + GroupCursor, GroupCursor);
            Capture(*Phase, 6);
        }
        break;
    case 6:
    {
        const int32 Type = RokTerritorySynthetic::TypeInGroup(GroupCursor, RowCursor);
        if (Type > 0)
        {
            UButton* Target = Button(FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type));
            if (!TargetInPath(Target)) { if (WheelTowards(Target)) Enter(6); break; }
            if (Press(FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type), 19600 + Type,
                TEXT("select_static_territory_definition_19601_19612"))) Enter(7);
        }
        else { ++GroupCursor; RowCursor = 0; Enter(4); }
        break;
    }
    case 7:
    {
        const int32 Type = RokTerritorySynthetic::TypeInGroup(GroupCursor, RowCursor);
        if (!Type) { Finish(TEXT("Territory type cursor invalid")); break; }
        if (Stable(3, GroupCursor, Type) && Observe(3, GroupCursor, Type, TEXT("detail_static_definition")))
        {
            const FString Phase = FString::Printf(TEXT("detail_type_%02d"), Type);
            Capture(*Phase, 8);
        }
        break;
    }
    case 8:
        if (Press(TEXT("AllianceTerritoryReturnToDefinitions"), 19630, TEXT("return_to_group_19630"))) Enter(9);
        break;
    case 9:
        if (Stable(2, GroupCursor) && Observe(2, GroupCursor, 0, TEXT("returned_to_expanded_group")))
        { ++RowCursor; Enter(6); }
        break;
    case 11:
        if (Stable(4) && Observe(4, INDEX_NONE, 0, TEXT("returned_to_joined")))
            Capture(TEXT("18_joined_return"), 12);
        break;
    case 12:
        if (Press(TEXT("AllianceFeatureHit_1"), 651, TEXT("reopen_territory_651"))) Enter(13);
        break;
    case 13:
        if (Stable(5) && Observe(5, INDEX_NONE, 0, TEXT("reference_reopened_reset")))
            Capture(TEXT("19_reference_reopened"), 14);
        break;
    case 14:
        if (Press(TEXT("FeatureFrameClose"), 1, TEXT("close_feature_to_hud"))) Enter(15);
        break;
    case 15:
        if (Stable(6) && Observe(6, INDEX_NONE, 0, TEXT("hud_closed")))
            Capture(TEXT("20_hud_closed"), 16);
        break;
    case 16: Finish(); break;
    default: Finish(TEXT("Invalid Territory audit state")); break;
    }
}
