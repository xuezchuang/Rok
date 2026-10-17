#include "RokResearchMainMaskAuditSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokResearchScreenWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
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
#include "Widgets/SWindow.h"

namespace
{
constexpr int32 ExpectedCaptureCount=13;
constexpr TCHAR AuditFlag[]=TEXT("-RokResearchMainMaskAudit");
constexpr TCHAR CityMap[]=TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR SourceContract[]=TEXT("Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json");
constexpr TCHAR MainReference[]=TEXT("Saved/UnityResearchNativeProbe/Artifacts/native_20261008_124117_758/render_evidence.json");
constexpr TCHAR MainNative[]=TEXT("Saved/UnityResearchMainMaskProbe/Artifacts/native_20261008_151902_577/render_evidence.json");
constexpr TCHAR MainNativeVerified[]=TEXT("Saved/RokUnityMigration/ResearchMainMaskNative/verified_capture.json");
constexpr TCHAR ApprovedMainNativeSha256[]=TEXT("71503eb9a3e581907c6e802fd8552a07cf809646c75edc6bb1ba6ce40a330b01");
constexpr TCHAR ApprovedMainNativeVerifiedSha256[]=TEXT("47a1c386d9f73860830b64a528051b8649da04432f3695cfb52e70ea8ada44da");
constexpr TCHAR ReviewedFeatureUiSourceSha256[]=TEXT("6fdd7026d345f8036edbd428e097fddd31e79e5737a684f35a1933d2b9c1c210");
constexpr TCHAR ReviewedResearchUiSourceSha256[]=TEXT("fe2e13b0c6524fde86f4432c68fdbc01de6d81d2eec3c0fc47389dc93a889e49");

bool TargetWorld(const UWorld* World)
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return World && World->WorldType==EWorldType::Game
        && World->GetOutermost()->GetName()==CityMap
        && FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer,false);
    T::StaticStruct()->SerializeItem(Archive,&Value,nullptr);
    return Bytes;
}
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity()<=0.0f) return false;
    for (const UWidget* Parent=Widget->GetParent();Parent;Parent=Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity()<=0.0f) return false;
    for (const UObject* Outer=Widget->GetOuter();Outer;Outer=Outer->GetOuter())
        if (const auto* User=Cast<UUserWidget>(Outer);
            User && (!User->IsVisible() || User->GetRenderOpacity()<=0.0f)) return false;
    return true;
}
TSharedRef<FJsonObject> MakeResearchMainMaskRectJson(FVector2D Origin,FVector2D Size)
{
    auto Json=MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"),Origin.X); Json->SetNumberField(TEXT("y"),Origin.Y);
    Json->SetNumberField(TEXT("width"),Size.X); Json->SetNumberField(TEXT("height"),Size.Y);
    return Json;
}
TSharedRef<FJsonObject> WidgetState(const UObject* Context,const UWidget* Widget)
{
    auto Json=MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("attached"),Widget!=nullptr);
    if (!Widget) return Json;
    Json->SetBoolField(TEXT("visible"),Visible(Widget));
    Json->SetBoolField(TEXT("self_visible"),Widget->IsVisible());
    Json->SetBoolField(TEXT("hovered"),Widget->IsHovered());
    Json->SetStringField(TEXT("visibility"),StaticEnum<ESlateVisibility>()->GetNameStringByValue(
        static_cast<int64>(Widget->GetVisibility())));
    Json->SetStringField(TEXT("parent"),Widget->GetParent()?Widget->GetParent()->GetName():FString());
    Json->SetNumberField(TEXT("render_opacity"),Widget->GetRenderOpacity());
    const FWidgetTransform Transform=Widget->GetRenderTransform();
    Json->SetNumberField(TEXT("render_scale_x"),Transform.Scale.X);
    Json->SetNumberField(TEXT("render_scale_y"),Transform.Scale.Y);
    Json->SetNumberField(TEXT("render_translation_x"),Transform.Translation.X);
    Json->SetNumberField(TEXT("render_translation_y"),Transform.Translation.Y);
    if (const auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        Json->SetObjectField(TEXT("slot_rect"),MakeResearchMainMaskRectJson(Slot->GetPosition(),Slot->GetSize()));
    const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context);
    const FVector2D Pixels=UWidgetLayoutLibrary::GetViewportSize(Context);
    const FGeometry Geometry=Widget->GetCachedGeometry();
    if (Viewport.GetLocalSize().X>0 && Viewport.GetLocalSize().Y>0)
    {
        const auto Pixel=[&](FVector2D P)
        {
            const FVector2D Local=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(P));
            return FVector2D(Local.X*Pixels.X/Viewport.GetLocalSize().X,
                Local.Y*Pixels.Y/Viewport.GetLocalSize().Y);
        };
        const FVector2D A=Pixel(FVector2D::ZeroVector),B=Pixel(Geometry.GetLocalSize());
        Json->SetObjectField(TEXT("pixel_rect"),MakeResearchMainMaskRectJson(
            FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),
            FVector2D(FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y))));
    }
    return Json;
}
TSharedRef<FJsonObject> Color(const FLinearColor& Value)
{
    auto Json=MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("r"),Value.R); Json->SetNumberField(TEXT("g"),Value.G);
    Json->SetNumberField(TEXT("b"),Value.B); Json->SetNumberField(TEXT("a"),Value.A);
    return Json;
}
}

bool URokResearchMainMaskAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer))
        && FParse::Param(FCommandLine::Get(),TEXT("RokResearchMainMaskAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokResearchMainMaskAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type==EWorldType::Game; }
TStatId URokResearchMainMaskAuditSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(URokResearchMainMaskAuditSubsystem,STATGROUP_Tickables); }
void URokResearchMainMaskAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsWorld=TargetWorld(GetWorld()); if (!bOwnsWorld) return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/ResearchMainMaskAudit")
        /FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
            FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this,&URokResearchMainMaskAuditSubsystem::ScreenshotProcessed);
}
void URokResearchMainMaskAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsWorld && !bFinished) Finish(TEXT("Target city world ended before ResearchMain mask audit completion"));
    Super::Deinitialize();
}
bool URokResearchMainMaskAuditSubsystem::Expect(bool Condition,const TCHAR* Error)
{ if (!Condition) Finish(Error); return Condition; }
bool URokResearchMainMaskAuditSubsystem::Identity()
{ return Expect(TargetWorld(GetWorld()),TEXT("Wrong project/map; no ResearchMain mask action permitted")); }
void URokResearchMainMaskAuditSubsystem::Enter(int32 Next) { Step=Next; StepStarted=Elapsed; }

bool URokResearchMainMaskAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    FString Entry; FParse::Value(FCommandLine::Get(),TEXT("RokOpenModal="),Entry);
    if (!Expect(Entry.Equals(TEXT("research_reference"),ESearchCase::IgnoreCase),
        TEXT("ResearchMain mask audit requires -RokOpenModal=research_reference"))) return false;
    const TCHAR* Cursor=FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor,Token,false))
        if (Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)
            && !Token.Equals(AuditFlag,ESearchCase::IgnoreCase)
            && !Token.Equals(TEXT("-RokOpenModal=research_reference"),ESearchCase::IgnoreCase))
        { Finish(TEXT("ResearchMain mask audit requires an isolated Rok process")); return false; }
    const auto Unique=[&](UClass* Class)->UUserWidget*
    {
        TArray<UUserWidget*> All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,All,Class,false);
        UUserWidget* Result=nullptr; int32 Count=0;
        for (auto* Widget:All) if (Widget->GetWorld()==GetWorld()) { Result=Widget; ++Count; }
        return Count==1?Result:nullptr;
    };
    Hud=Cast<URokMainHudWidget>(Unique(URokMainHudWidget::StaticClass()));
    Screen=Cast<URokUnityFeatureScreenWidget>(Unique(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Screen.IsValid()
        && Economy && Runtime,TEXT("Expected unique HUD, Research reference and state subsystems"))) return false;
    Research=Screen->GetResearchScreenForAudit();
    if (!Expect(Research.IsValid() && Screen->IsResearchModeForAudit()
        && Research->IsReferencePreviewForAudit() && Research->GetLoadErrorForAudit().IsEmpty()
        && !Research->IsAccountResearchSyncedForAudit()
        && !Research->IsResearchQueueSyncedForAudit()
        && FVector2D(1440,810).Equals(UWidgetLayoutLibrary::GetViewportSize(this),.1),
        TEXT("Research reference preview, unsynced boundary or 1440x810 viewport differs"))) return false;
    Inputs=MakeShared<FJsonObject>();
    const TCHAR* Required[]={SourceContract,MainReference,MainNative,MainNativeVerified,
        TEXT("Source/Rok/RokResearchMainMaskAuditSubsystem.h"),
        TEXT("Source/Rok/RokResearchMainMaskAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokResearchMainMaskRuntime.py"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
        TEXT("Source/Rok/RokResearchScreenWidget.h"),
        TEXT("Source/Rok/RokResearchScreenWidget.cpp"),
        TEXT("Source/Rok/RokUiWindowAnimation.cpp"),
        TEXT("Source/Rok/RokMainHudWidget.cpp"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/UI.cs"),
        TEXT("ref/client-unity/Assets/Skyunion/RunTime/UIManager/UIManager.cs"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs")};
    for (const TCHAR* Relative:Required)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/Relative))
            && !Bytes.IsEmpty(),TEXT("Required ResearchMain source unavailable"))) return false;
        const FString Hash=RokTrainArmyMesh::Sha256(Bytes);
        if (FString(Relative)==TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp")
            && !Expect(Hash==ReviewedFeatureUiSourceSha256,
                TEXT("ResearchMain mask/X action source changed since review"))) return false;
        if (FString(Relative)==TEXT("Source/Rok/RokResearchScreenWidget.cpp")
            && !Expect(Hash==ReviewedResearchUiSourceSha256,
                TEXT("Research detail mask source changed since review"))) return false;
        if (FString(Relative)==MainNative
            && !Expect(Hash==ApprovedMainNativeSha256,
                TEXT("Isolated Unity ResearchMain mask render evidence changed"))) return false;
        if (FString(Relative)==MainNativeVerified
            && !Expect(Hash==ApprovedMainNativeVerifiedSha256,
                TEXT("Strict Unity ResearchMain mask verification record changed"))) return false;
        auto File=MakeShared<FJsonObject>(); File->SetStringField(TEXT("sha256"),Hash);
        File->SetNumberField(TEXT("bytes"),Bytes.Num()); Inputs->SetObjectField(Relative,File);
    }
    int32 AcademyActors=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        bool Tagged=false;
        for (FName Tag:It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_")))
        { Tagged=true; break; }
        if (!Tagged) continue;
        const auto Building=Economy->GetBuildingStateForActor(*It);
        if (Building.UnityInitId>0) BuildingSnapshots.Add(*It,Snapshot(Building));
        if (Building.UnityBuildingTypeId==13) ++AcademyActors;
    }
    if (!Expect(AcademyActors==0 && BuildingSnapshots.Num()>0,
        TEXT("Initial city Academy/reference scope changed"))) return false;
    QueueCount=Economy->GetActiveUpgradeCount(); OwnedQueueCount=Economy->GetOwnedBuildQueueCount();
    ProfileSnapshot=Snapshot(Runtime->GetPlayerProfileState());
    ResourceSnapshot=Snapshot(Economy->GetCurrentResources());
    for (const auto& Stack:Runtime->GetStarterTroopStacks()) TroopSnapshots.Add(Snapshot(Stack));
    InventoryFingerprint=Screen->GetVisibleItemsInventoryFingerprintForAudit();
    Baseline=MakeShared<FJsonObject>();
    Baseline->SetStringField(TEXT("profile_sha256"),RokTrainArmyMesh::Sha256(ProfileSnapshot));
    Baseline->SetStringField(TEXT("resources_sha256"),RokTrainArmyMesh::Sha256(ResourceSnapshot));
    Baseline->SetNumberField(TEXT("active_build_upgrades"),QueueCount);
    Baseline->SetNumberField(TEXT("owned_build_queues"),OwnedQueueCount);
    Baseline->SetNumberField(TEXT("academy_actors"),AcademyActors);
    Baseline->SetNumberField(TEXT("source_building_snapshots"),BuildingSnapshots.Num());
    Baseline->SetStringField(TEXT("local_inventory_fingerprint"),InventoryFingerprint);
    TArray<TSharedPtr<FJsonValue>> Buildings,Troops;
    for (const auto& Pair:BuildingSnapshots)
    {
        auto Item=MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("actor"),Pair.Key.IsValid()?Pair.Key->GetName():FString());
        Item->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(Pair.Value));
        Buildings.Add(MakeShared<FJsonValueObject>(Item));
    }
    for (const auto& Bytes:TroopSnapshots)
        Troops.Add(MakeShared<FJsonValueString>(RokTrainArmyMesh::Sha256(Bytes)));
    Baseline->SetArrayField(TEXT("building_state_sha256"),Buildings);
    Baseline->SetArrayField(TEXT("troop_state_sha256"),Troops);
    bBaseline=true; return ValidateResearch();
}

bool URokResearchMainMaskAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime
        && Snapshot(Economy->GetCurrentResources())==ResourceSnapshot
        && Snapshot(Runtime->GetPlayerProfileState())==ProfileSnapshot
        && Economy->GetActiveUpgradeCount()==QueueCount
        && Economy->GetOwnedBuildQueueCount()==OwnedQueueCount,
        TEXT("ResearchMain navigation changed resources/profile/build queues"))) return false;
    const auto Stacks=Runtime->GetStarterTroopStacks();
    if (!Expect(Stacks.Num()==TroopSnapshots.Num(),
        TEXT("ResearchMain navigation changed troop count"))) return false;
    for (int32 I=0;I<Stacks.Num();++I)
        if (!Expect(Snapshot(Stacks[I])==TroopSnapshots[I],
            TEXT("ResearchMain navigation changed troop state"))) return false;
    for (const auto& Pair:BuildingSnapshots)
        if (!Expect(Pair.Key.IsValid()
            && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get()))==Pair.Value,
            TEXT("ResearchMain navigation changed building state"))) return false;
    return Expect(Screen.IsValid()
        && Screen->GetVisibleItemsInventoryFingerprintForAudit()==InventoryFingerprint,
        TEXT("ResearchMain navigation changed inventory projection"));
}

bool URokResearchMainMaskAuditSubsystem::ValidateResearch(bool bDetail)
{
    if (!Identity() || !Unchanged()) return false;
    Research=Screen.IsValid()?Screen->GetResearchScreenForAudit():nullptr;
    if (!Expect(Screen.IsValid() && Screen->IsScreenOpen()
        && Screen->IsResearchModeForAudit() && Research.IsValid()
        && Research->IsReferencePreviewForAudit() && !Research->IsAccountResearchSyncedForAudit()
        && !Research->IsResearchQueueSyncedForAudit()
        && Research->GetLoadErrorForAudit().IsEmpty()
        && Research->GetRenderedNodesForAudit().Num()==71
        && Research->GetSelectedTypeForAudit()==1,
        TEXT("Research reference/account boundary changed"))) return false;
    return Expect(Visible(Research->GetWidgetFromName(TEXT("ResearchDetailRoot")))==bDetail,
        TEXT("Research detail state differs from Main mask test"));
}

bool URokResearchMainMaskAuditSubsystem::SlateClick(FName Name,bool bRequireHandled)
{
    if (!Identity() || !Unchanged() || !Screen.IsValid()) return false;
    const bool Allowed=Name==TEXT("FeatureScreenInputBlocker")
        || Name==TEXT("ResearchCloseButton") || Name==TEXT("ResearchNodeHit_102")
        || Name==TEXT("ResearchDetailMaskHit")
        || Name==TEXT("ResearchMainBodyHitLeft") || Name==TEXT("ResearchMainHeaderHitLeft");
    if (!Expect(Allowed,TEXT("Unreviewed ResearchMain audit action"))) return false;
    UWidget* Target=Name==TEXT("ResearchNodeHit_102") || Name==TEXT("ResearchDetailMaskHit")
        ? Research.IsValid()?Research->GetWidgetFromName(Name):nullptr
        : Screen->GetWidgetFromName(Name);
    if (!Expect(Target && Visible(Target) && Target->GetIsEnabled()
        && FSlateApplication::IsInitialized(),TEXT("ResearchMain Slate target unavailable"))) return false;
    UWidget* Underlay=Name==TEXT("ResearchDetailMaskHit")
        ? Research->GetWidgetFromName(TEXT("ResearchMilitaryTab")):nullptr;
    const FGeometry Geometry=Underlay?Underlay->GetCachedGeometry():Target->GetCachedGeometry();
    const FVector2D Point=Name==TEXT("FeatureScreenInputBlocker")
        ? Geometry.LocalToAbsolute(FVector2D(80,400))
        : Name==TEXT("ResearchMainBodyHitLeft") || Name==TEXT("ResearchMainHeaderHitLeft")
        ? Geometry.LocalToAbsolute(FVector2D(10,Geometry.GetLocalSize().Y*0.5f))
        : Geometry.LocalToAbsolute(Geometry.GetLocalSize()*0.5f);
    FSlateApplication& App=FSlateApplication::Get();
    const FVector2D Away=Target->GetCachedGeometry().LocalToAbsolute(FVector2D(-32,-32));
    const FWidgetPath AwayPath=App.LocateWindowUnderMouse(
        Away,App.GetInteractiveTopLevelWindows(),false,0);
    const bool TargetInAwayPath=AwayPath.IsValid()
        && AwayPath.ContainsWidget(&Target->TakeWidget().Get());
    if (!Expect(!TargetInAwayPath,TEXT("ResearchMain virtual away point still inside target")))
        return false;
    const FPointerEvent MoveAway(0,Away,Point,TSet<FKey>(),
        EKeys::Invalid,0,FModifierKeysState());
    const bool AwayHandled=App.ProcessMouseMoveEvent(MoveAway);
    const FPointerEvent Move(0,Point,Away,TSet<FKey>(),
        EKeys::Invalid,0,FModifierKeysState());
    const bool MoveHandled=App.ProcessMouseMoveEvent(Move);
    const FWidgetPath Path=App.LocateWindowUnderMouse(
        Point,App.GetInteractiveTopLevelWindows(),false,0);
    const bool TargetInPath=Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get());
    const bool UnderlayInPath=Underlay && Path.IsValid()
        && Path.ContainsWidget(&Underlay->TakeWidget().Get());
    if (!TargetInPath || !Target->IsHovered() || UnderlayInPath
        || (Underlay && Underlay->IsHovered()))
    {
        FString PathTypes;
        if (Path.IsValid()) for (int32 I=0;I<Path.Widgets.Num();++I)
            PathTypes+=Path.Widgets[I].Widget->GetTypeAsString()+TEXT("/");
        Finish(FString::Printf(TEXT("ResearchMain Slate hit failure target=%s point=(%.1f,%.1f) away=(%.1f,%.1f) away_path_valid=%d away_in_target=%d path_valid=%d target_in_path=%d target_hovered=%d underlay_in_path=%d underlay_hovered=%d path=%s"),
            *Name.ToString(),Point.X,Point.Y,Away.X,Away.Y,AwayPath.IsValid(),TargetInAwayPath,
            Path.IsValid(),TargetInPath,
            Target->IsHovered(),UnderlayInPath,Underlay?Underlay->IsHovered():false,*PathTypes));
        return false;
    }
    auto Action=MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("button"),Name.ToString());
    Action->SetStringField(TEXT("method"),
        TEXT("FSlateApplication.ProcessMouseMoveEventAway+MoveEventTarget+DownEvent+UpEvent"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetNumberField(TEXT("world_elapsed_seconds"),Elapsed);
    Action->SetNumberField(TEXT("absolute_x"),Point.X); Action->SetNumberField(TEXT("absolute_y"),Point.Y);
    Action->SetNumberField(TEXT("away_absolute_x"),Away.X);
    Action->SetNumberField(TEXT("away_absolute_y"),Away.Y);
    Action->SetBoolField(TEXT("target_in_away_path"),TargetInAwayPath);
    Action->SetBoolField(TEXT("away_path_valid"),AwayPath.IsValid());
    Action->SetBoolField(TEXT("move_handled"),MoveHandled);
    Action->SetBoolField(TEXT("move_away_handled"),AwayHandled);
    Action->SetBoolField(TEXT("target_in_hit_path"),true);
    Action->SetBoolField(TEXT("target_hovered_after_move"),Target->IsHovered());
    Action->SetBoolField(TEXT("underlay_in_hit_path"),false);
    Action->SetObjectField(TEXT("before"),State());
    TArray<TSharedPtr<FJsonValue>> Types;
    for (int32 I=0;I<Path.Widgets.Num();++I)
        Types.Add(MakeShared<FJsonValueString>(Path.Widgets[I].Widget->GetTypeAsString()));
    Action->SetArrayField(TEXT("slate_widget_path_types"),Types);
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const FPointerEvent Down(0,Point,Point,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());
    const FPointerEvent Up(0,Point,Point,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    const bool DownHandled=App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),Down);
    const bool UpHandled=App.ProcessMouseButtonUpEvent(Up);
    Action->SetBoolField(TEXT("down_handled"),DownHandled);
    Action->SetBoolField(TEXT("up_handled"),UpHandled);
    Action->SetObjectField(TEXT("after"),State());
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    return Expect(!bRequireHandled || (DownHandled && UpHandled),
        TEXT("ResearchMain virtual click failed Slate routing"));
}

bool URokResearchMainMaskAuditSubsystem::CheckCloseClock(float Minimum,float Maximum)
{
    if (!Screen.IsValid() || !Screen->IsResearchMainMaskClosingForAudit()) return false;
    const float Time=Screen->GetResearchMainMaskCloseElapsedSecondsForAudit();
    if (Time>Maximum)
    { Finish(FString::Printf(TEXT("Missed live ResearchMain Close interval [%.3f,%.3f]: %.3f"),
        Minimum,Maximum,Time)); return false; }
    return Time>=Minimum;
}

TSharedRef<FJsonObject> URokResearchMainMaskAuditSubsystem::State() const
{
    auto Json=MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("step"),Step);
    Json->SetNumberField(TEXT("frame"),GFrameCounter);
    Json->SetNumberField(TEXT("world_elapsed_seconds"),Elapsed);
    Json->SetBoolField(TEXT("state_unchanged"),true);
    Json->SetObjectField(TEXT("viewport_pixels"),MakeResearchMainMaskRectJson(
        FVector2D::ZeroVector,UWidgetLayoutLibrary::GetViewportSize(this)));
    Json->SetBoolField(TEXT("screen_open"),Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("research_mode"),Screen.IsValid() && Screen->IsResearchModeForAudit());
    const auto Feature=[&](const TCHAR* Name)
    { return WidgetState(this,Screen.IsValid()?Screen->GetWidgetFromName(Name):nullptr); };
    Json->SetObjectField(TEXT("feature_mode_canvas"),Feature(TEXT("FeatureModeCanvas")));
    Json->SetObjectField(TEXT("feature_input_blocker"),Feature(TEXT("FeatureScreenInputBlocker")));
    Json->SetObjectField(TEXT("feature_dimmer"),Feature(TEXT("FeatureScreenDimmer")));
    Json->SetObjectField(TEXT("research_close_button"),Feature(TEXT("ResearchCloseButton")));
    Json->SetObjectField(TEXT("main_body_hit_left"),Feature(TEXT("ResearchMainBodyHitLeft")));
    Json->SetObjectField(TEXT("main_body_hit_right"),Feature(TEXT("ResearchMainBodyHitRight")));
    Json->SetObjectField(TEXT("main_header_hit_left"),Feature(TEXT("ResearchMainHeaderHitLeft")));
    Json->SetObjectField(TEXT("main_header_hit_right"),Feature(TEXT("ResearchMainHeaderHitRight")));
    if (const auto* Dimmer=Screen.IsValid()
        ? Cast<UBorder>(Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer"))):nullptr)
        Json->SetObjectField(TEXT("feature_dimmer_color"),Color(Dimmer->GetBrushColor()));
    if (Screen.IsValid())
    {
        Json->SetBoolField(TEXT("mask_closing"),Screen->IsResearchMainMaskClosingForAudit());
        Json->SetNumberField(TEXT("mask_close_elapsed_seconds"),
            Screen->GetResearchMainMaskCloseElapsedSecondsForAudit());
        Json->SetStringField(TEXT("mask_phase"),Screen->GetResearchMainMaskPhaseForAudit().ToString());
        Json->SetNumberField(TEXT("pending_action_id"),Screen->GetPendingActionIdForAudit());
        const auto Sample=Screen->GetResearchMainMaskSampleForAudit();
        Json->SetNumberField(TEXT("diagnostic_sample_scale"),Sample.Scale);
        Json->SetNumberField(TEXT("diagnostic_sample_opacity"),Sample.Opacity);
    }
    if (!Research.IsValid()) return Json;
    Json->SetBoolField(TEXT("reference_preview"),Research->IsReferencePreviewForAudit());
    Json->SetBoolField(TEXT("account_research_synced"),Research->IsAccountResearchSyncedForAudit());
    Json->SetBoolField(TEXT("research_queue_synced"),Research->IsResearchQueueSyncedForAudit());
    Json->SetNumberField(TEXT("selected_type"),Research->GetSelectedTypeForAudit());
    Json->SetNumberField(TEXT("selected_study_type"),Research->GetSelectedStudyTypeForAudit());
    Json->SetNumberField(TEXT("economy_scroll_offset"),Research->GetEconomyScrollOffsetForAudit());
    Json->SetBoolField(TEXT("detail_closing"),Research->IsDetailClosingForAudit());
    Json->SetBoolField(TEXT("detail_closed"),Research->IsDetailClosedForAudit());
    const auto Detail=[&](const TCHAR* Name)
    { return WidgetState(this,Research->GetWidgetFromName(Name)); };
    Json->SetObjectField(TEXT("detail_root"),Detail(TEXT("ResearchDetailRoot")));
    Json->SetObjectField(TEXT("detail_mask"),Detail(TEXT("ResearchDetailMask")));
    Json->SetObjectField(TEXT("detail_mask_hit"),Detail(TEXT("ResearchDetailMaskHit")));
    Json->SetObjectField(TEXT("military_tab"),Detail(TEXT("ResearchMilitaryTab")));
    Json->SetObjectField(TEXT("node_102"),Detail(TEXT("ResearchNodeHit_102")));
    return Json;
}

void URokResearchMainMaskAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Unchanged()) return;
    if (FScreenshotRequest::IsScreenshotRequested())
    { Finish(TEXT("Conflicting ResearchMain screenshot request")); return; }
    PendingPath=Directory/(FString(Name)+TEXT(".png"));
    Pending=MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("file"),PendingPath);
    Pending->SetStringField(TEXT("phase_name"),Name);
    Pending->SetObjectField(TEXT("request"),State());
    ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false);
}

void URokResearchMainMaskAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || !Unchanged()) return;
    const int64 Size=IFileManager::Get().FileSize(*PendingPath);
    if (Size<=0) { Finish(TEXT("ResearchMain screenshot callback missing PNG")); return; }
    Pending->SetObjectField(TEXT("processed"),State());
    Pending->SetNumberField(TEXT("file_bytes"),Size);
    TArray<uint8> Png;
    if (!FFileHelper::LoadFileToArray(Png,*PendingPath))
    { Finish(TEXT("ResearchMain screenshot cannot be hashed")); return; }
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Png));
    Captures.Add(MakeShared<FJsonValueObject>(Pending));
    Pending.Reset(); Save(false); Enter(Step+1);
}

void URokResearchMainMaskAuditSubsystem::Tick(float DeltaTime)
{
    if (!bOwnsWorld || bFinished) return;
    Elapsed+=FMath::Max(0.0f,DeltaTime);
    // HUD construction precedes the first Slate viewport geometry in GameApp.
    if (Elapsed<3.0) return;
    if (Pending.IsValid())
    {
        if (Elapsed-ShotStarted>25.0) Finish(TEXT("ResearchMain screenshot callback timeout"));
        return;
    }
    if (Step>0 && Elapsed-StepStarted>12.0)
    { Finish(FString::Printf(TEXT("ResearchMain mask step %d timeout"),Step)); return; }
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: if (Elapsed-StepStarted>.4 && ValidateResearch()) Capture(TEXT("01_main_open")); break;
    case 2:
        if (ValidateResearch() && SlateClick(TEXT("ResearchMainBodyHitLeft"),false)
            && SlateClick(TEXT("ResearchMainHeaderHitLeft"),false)
            && Expect(!Screen->IsResearchMainMaskClosingForAudit(),
                TEXT("ResearchMain decorative frame click reached background mask")))
        {
            if (SlateClick(TEXT("FeatureScreenInputBlocker"))) Enter(3);
        }
        break;
    case 3: if (CheckCloseClock(.025f,.085f)) Capture(TEXT("02_main_mask_close_early")); break;
    case 4: if (Screen.IsValid() && !Screen->IsScreenOpen()) Capture(TEXT("03_city_after_mask_close")); break;
    case 5: if (Unchanged()) { Hud->OpenCommandLineModal(); ReopenStarted=Elapsed; Enter(6); } break;
    case 6: if (Elapsed-ReopenStarted>.4 && ValidateResearch()) Capture(TEXT("04_main_reopened")); break;
    case 7: if (ValidateResearch() && SlateClick(TEXT("FeatureScreenInputBlocker"))) Enter(8); break;
    case 8:
        if (CheckCloseClock(.035f,.075f))
        {
            RepeatCloseBefore=Screen->GetResearchMainMaskCloseElapsedSecondsForAudit();
            if (SlateClick(TEXT("FeatureScreenInputBlocker"))
                && Expect(Screen->IsResearchMainMaskClosingForAudit()
                    && Screen->GetResearchMainMaskCloseElapsedSecondsForAudit()+.002f>=RepeatCloseBefore,
                    TEXT("Duplicate ResearchMain mask click restarted Close clock"))) Enter(9);
        }
        break;
    case 9: if (CheckCloseClock(.08f,.14f)) Capture(TEXT("05_main_mask_duplicate_mid")); break;
    case 10: if (Screen.IsValid() && !Screen->IsScreenOpen()) Capture(TEXT("06_duplicate_close_terminal")); break;
    case 11: if (Unchanged()) { Hud->OpenCommandLineModal(); ReopenStarted=Elapsed; Enter(12); } break;
    case 12:
        if (Screen.IsValid() && Screen->IsResearchModeForAudit()
            && Screen->GetResearchScreenForAudit() && ValidateResearch())
        {
            const UWidget* Mode=Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
            if (Mode && Mode->GetRenderTransform().Scale.X>1.051f
                && Mode->GetRenderTransform().Scale.X<1.099f)
            {
                if (SlateClick(TEXT("FeatureScreenInputBlocker"))
                    && Expect(Screen->IsResearchMainMaskClosingForAudit()
                        && FMath::IsNearlyEqual(Mode->GetRenderTransform().Scale.X,1.0f,.005f)
                        && FMath::IsNearlyEqual(Mode->GetRenderOpacity(),1.0f,.005f),
                        TEXT("Open-interrupted ResearchMain Close did not start from source 1/1"))) Enter(13);
            }
        }
        break;
    case 13: if (CheckCloseClock(.025f,.085f)) Capture(TEXT("07_open_preempted_close")); break;
    case 14: if (Screen.IsValid() && !Screen->IsScreenOpen()) Capture(TEXT("08_open_preempted_terminal")); break;
    case 15: if (Unchanged()) { Hud->OpenCommandLineModal(); ReopenStarted=Elapsed; Enter(16); } break;
    case 16: if (Elapsed-ReopenStarted>.4 && ValidateResearch()
        && SlateClick(TEXT("ResearchNodeHit_102"))) Enter(17); break;
    case 17:
        if (Research.IsValid() && Visible(Research->GetWidgetFromName(TEXT("ResearchDetailRoot")))
            && ValidateResearch(true)) Capture(TEXT("09_detail_open"));
        break;
    case 18: if (ValidateResearch(true) && SlateClick(TEXT("ResearchDetailMaskHit"))) Enter(19); break;
    case 19:
        if (ValidateResearch(true)
            && Expect(!Screen->IsResearchMainMaskClosingForAudit()
                && Research->GetSelectedTypeForAudit()==1,
                TEXT("Detail mask click incorrectly closed Main or switched tabs")))
            Capture(TEXT("10_detail_mask_only"));
        break;
    case 20:
        if (Research.IsValid() && !Visible(Research->GetWidgetFromName(TEXT("ResearchDetailRoot")))
            && ValidateResearch(false)) Capture(TEXT("11_main_survived_detail"));
        break;
    case 21: if (ValidateResearch() && SlateClick(TEXT("ResearchCloseButton"))) Enter(22); break;
    case 22: if (Screen.IsValid() && !Screen->IsScreenOpen()
        && Expect(Elapsed-StepStarted<.15,TEXT("Main X did not force immediate close")))
        Capture(TEXT("12_x_force_closed")); break;
    case 23: if (Unchanged()) { Hud->OpenCommandLineModal(); ReopenStarted=Elapsed; Enter(24); } break;
    case 24: if (Elapsed-ReopenStarted>.4 && ValidateResearch()
        && SlateClick(TEXT("FeatureScreenInputBlocker"))) Enter(25); break;
    case 25: if (CheckCloseClock(.04f,.09f)
        && SlateClick(TEXT("ResearchCloseButton"))) Enter(26); break;
    case 26: if (Screen.IsValid() && !Screen->IsScreenOpen()
        && Expect(Elapsed-StepStarted<.15,TEXT("Main X did not interrupt mask Close immediately")))
        Capture(TEXT("13_x_force_interrupts_mask_close")); break;
    case 27: Finish(); break;
    default: Finish(TEXT("Invalid ResearchMain mask audit step")); break;
    }
}

bool URokResearchMainMaskAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.research_main_mask.runtime.v1"));
    Root->SetBoolField(TEXT("complete"),Complete);
    Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty()
        && Captures.Num()==ExpectedCaptureCount);
    Root->SetStringField(TEXT("audit_flag"),AuditFlag);
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("entry_kind"),TEXT("HUD command_line_reference research_reference"));
    Root->SetBoolField(TEXT("real_academy_entry_verified"),false);
    Root->SetBoolField(TEXT("account_research_state_synced"),false);
    Root->SetBoolField(TEXT("research_queue_synced"),false);
    Root->SetStringField(TEXT("scope"),TEXT("Real GameApp HUD reference entry; virtual Slate Move/Down/Up; actual ResearchMain opacity/scale and screenshots. Isolated Unity source Main prefab/Animator sampled in eleven PNGs, with UIManager mask/removal rule simulated. No complete Unity UIManager/login, account research request, hardware pointer, Academy actor or Academy entry verified."));
    Root->SetNumberField(TEXT("expected_capture_count"),ExpectedCaptureCount);
    Root->SetStringField(TEXT("source_contract"),SourceContract);
    Root->SetStringField(TEXT("main_reference"),MainReference);
    Root->SetStringField(TEXT("main_native_evidence"),MainNative);
    Root->SetStringField(TEXT("main_native_verified"),MainNativeVerified);
    if (Inputs.IsValid()) Root->SetObjectField(TEXT("input_hashes"),Inputs.ToSharedRef());
    if (Baseline.IsValid()) Root->SetObjectField(TEXT("baseline"),Baseline.ToSharedRef());
    Root->SetArrayField(TEXT("captures"),Captures);
    Root->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const auto& Error:Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Root->SetArrayField(TEXT("failures"),Errors);
    if (FailureState.IsValid()) Root->SetObjectField(TEXT("failure_state"),FailureState.ToSharedRef());
    FString Json;
    return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))
        && FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokResearchMainMaskAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num()!=ExpectedCaptureCount)
        Failures.Add(TEXT("ResearchMain mask audit did not process all thirteen screenshots"));
    if (!Failures.IsEmpty()) FailureState=State();
    bFinished=true;
    const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures)
        UE_LOG(LogTemp,Error,TEXT("Rok ResearchMainMaskAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok ResearchMainMaskAudit passed=%d captures=%d directory=%s"),
        Failures.IsEmpty() && Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty() && Written?0:1);
}
