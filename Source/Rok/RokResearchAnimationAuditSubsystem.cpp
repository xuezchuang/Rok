#include "RokResearchAnimationAuditSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokResearchScreenWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
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
constexpr int32 ExpectedCaptureCount=20;
constexpr TCHAR AuditFlag[]=TEXT("-RokResearchAnimationAudit");
constexpr TCHAR CityMap[]=TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR SourceContract[]=TEXT("Saved/RokUnityMigration/ResearchAnimationNative/animation_data.json");
constexpr TCHAR NativeBaselineEvidence[]=TEXT("Saved/UnityResearchAnimationProbe/Artifacts/native_20261008_135653_326/render_evidence.json");
constexpr TCHAR NativeBaselineVerified[]=TEXT("Saved/RokUnityMigration/ResearchAnimationNative/verified_animation_capture.json");
constexpr TCHAR NativeEvidence[]=TEXT("Saved/UnityResearchAnimationProbe/Artifacts/native_20261008_141523_430/render_evidence.json");
constexpr TCHAR NativeVerified[]=TEXT("Saved/RokUnityMigration/ResearchAnimationNative/verified_animation_close_capture.json");
constexpr TCHAR ApprovedNativeSha256[]=TEXT("373d40bbc9f229a0c34512fa1a0671ea76bf5a403ab08d49a1b3dcca12871de0");
constexpr TCHAR ApprovedNativeVerifiedSha256[]=TEXT("ecb5b5b19968a9fe64dd557141c6fcf76bed4897fb5af8c5ee178bdc6df07a40");
constexpr TCHAR ReviewedResearchUiSourceSha256[]=TEXT("fe2e13b0c6524fde86f4432c68fdbc01de6d81d2eec3c0fc47389dc93a889e49");
constexpr TCHAR ReviewedFeatureUiSourceSha256[]=TEXT("2af290091b67d2fa614056caee9c7e18405953f8cee88936dee3a162790b3701");

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
TSharedRef<FJsonObject> MakeResearchAnimationRect(FVector2D Origin,FVector2D Size)
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
    Json->SetNumberField(TEXT("render_opacity"),Widget->GetRenderOpacity());
    Json->SetBoolField(TEXT("hovered"),Widget->IsHovered());
    Json->SetStringField(TEXT("visibility"),StaticEnum<ESlateVisibility>()->GetNameStringByValue(
        static_cast<int64>(Widget->GetVisibility())));
    Json->SetStringField(TEXT("parent"),Widget->GetParent()?Widget->GetParent()->GetName():FString());
    const FWidgetTransform Transform=Widget->GetRenderTransform();
    Json->SetNumberField(TEXT("render_translation_x"),Transform.Translation.X);
    Json->SetNumberField(TEXT("render_translation_y"),Transform.Translation.Y);
    Json->SetNumberField(TEXT("render_scale_x"),Transform.Scale.X);
    Json->SetNumberField(TEXT("render_scale_y"),Transform.Scale.Y);
    Json->SetNumberField(TEXT("render_angle"),Transform.Angle);
    if (const auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        Json->SetObjectField(TEXT("slot_rect"),MakeResearchAnimationRect(Slot->GetPosition(),Slot->GetSize()));
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
        Json->SetObjectField(TEXT("pixel_rect"),MakeResearchAnimationRect(
            FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),
            FVector2D(FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y))));
    }
    return Json;
}
}

bool URokResearchAnimationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer))
        && FParse::Param(FCommandLine::Get(),TEXT("RokResearchAnimationAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokResearchAnimationAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type==EWorldType::Game; }
TStatId URokResearchAnimationAuditSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(URokResearchAnimationAuditSubsystem,STATGROUP_Tickables); }
void URokResearchAnimationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsWorld=TargetWorld(GetWorld()); if (!bOwnsWorld) return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/ResearchAnimationAudit")
        /FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
            FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
        this,&URokResearchAnimationAuditSubsystem::ScreenshotProcessed);
}
void URokResearchAnimationAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsWorld && !bFinished) Finish(TEXT("Target city world ended before research animation audit completion"));
    Super::Deinitialize();
}
bool URokResearchAnimationAuditSubsystem::Expect(bool Condition,const TCHAR* Error)
{ if (!Condition) Finish(Error); return Condition; }
bool URokResearchAnimationAuditSubsystem::Identity()
{ return Expect(TargetWorld(GetWorld()),TEXT("Wrong project/map; no research animation action permitted")); }
void URokResearchAnimationAuditSubsystem::Enter(int32 Next) { Step=Next; StepStarted=Elapsed; }

bool URokResearchAnimationAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    FString Entry; FParse::Value(FCommandLine::Get(),TEXT("RokOpenModal="),Entry);
    if (!Expect(Entry.Equals(TEXT("research_reference"),ESearchCase::IgnoreCase),
        TEXT("Animation audit requires -RokOpenModal=research_reference"))) return false;
    const TCHAR* Cursor=FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor,Token,false))
        if (Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)
            && !Token.Equals(AuditFlag,ESearchCase::IgnoreCase)
            && !Token.Equals(TEXT("-RokOpenModal=research_reference"),ESearchCase::IgnoreCase))
        { Finish(TEXT("Animation audit requires a dedicated process without other Rok flags")); return false; }
    const auto Host=[&](UClass* Class)->UUserWidget*
    {
        TArray<UUserWidget*> All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,All,Class,false);
        UUserWidget* Result=nullptr; int32 Count=0;
        for (auto* Widget:All) if (Widget->GetWorld()==GetWorld()) { Result=Widget; ++Count; }
        return Count==1?Result:nullptr;
    };
    Hud=Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));
    Screen=Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Screen.IsValid()
        && Economy && Runtime,TEXT("Expected unique city HUD and research feature host"))) return false;
    Research=Screen->GetResearchScreenForAudit();
    if (!Expect(Research.IsValid() && Screen->IsResearchModeForAudit()
        && Research->IsReferencePreviewForAudit() && Research->GetLoadErrorForAudit().IsEmpty()
        && !Research->IsAccountResearchSyncedForAudit() && !Research->IsResearchQueueSyncedForAudit(),
        TEXT("HUD reference did not open unsynced research preview"))) return false;
    if (!Expect(FVector2D(1440,810).Equals(UWidgetLayoutLibrary::GetViewportSize(this),.1),
        TEXT("Animation audit viewport must be 1440x810"))) return false;
    InputHashes=MakeShared<FJsonObject>();
    const TCHAR* Required[]={
        SourceContract,NativeBaselineEvidence,NativeBaselineVerified,
        NativeEvidence,NativeVerified,
        TEXT("Saved/RokUnityMigration/research_catalog.json"),
        TEXT("Saved/RokUnityMigration/ResearchNative/research_power_fixture.json"),
        TEXT("Source/Rok/RokResearchAnimationAuditSubsystem.h"),
        TEXT("Source/Rok/RokResearchAnimationAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokResearchAnimationRuntime.py"),
        TEXT("Source/Rok/RokResearchScreenWidget.h"),
        TEXT("Source/Rok/RokResearchScreenWidget.cpp"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
        TEXT("Source/Rok/RokMainHudWidget.cpp"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchUpateMediator.cs")
    };
    for (const TCHAR* Relative:Required)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes,*(RokDataPaths::ResolveProjectPath(Relative)))
            && !Bytes.IsEmpty(),TEXT("Required animation source input unavailable"))) return false;
        auto File=MakeShared<FJsonObject>(); File->SetNumberField(TEXT("bytes"),Bytes.Num());
        const FString Hash=RokTrainArmyMesh::Sha256(Bytes);
        if (FString(Relative)==NativeEvidence
            && !Expect(Hash==ApprovedNativeSha256,TEXT("Native animation evidence changed"))) return false;
        if (FString(Relative)==NativeVerified
            && !Expect(Hash==ApprovedNativeVerifiedSha256,TEXT("Verified native animation record changed"))) return false;
        if (FString(Relative)==TEXT("Source/Rok/RokResearchScreenWidget.cpp")
            && !Expect(Hash==ReviewedResearchUiSourceSha256,
                TEXT("Research UI animation/request boundary changed since review"))) return false;
        if (FString(Relative)==TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp")
            && !Expect(Hash==ReviewedFeatureUiSourceSha256,
                TEXT("Main Research force-close route changed since review"))) return false;
        File->SetStringField(TEXT("sha256"),Hash);
        InputHashes->SetObjectField(Relative,File);
    }
    int32 AcademyActors=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        bool Tagged=false;
        for (FName Tag:It->Tags)
            if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { Tagged=true; break; }
        if (!Tagged) continue;
        const auto Building=Economy->GetBuildingStateForActor(*It);
        if (Building.UnityInitId>0) BuildingSnapshots.Add(*It,Snapshot(Building));
        if (Building.UnityBuildingTypeId==13) ++AcademyActors;
    }
    if (!Expect(AcademyActors==0 && BuildingSnapshots.Num()>0,
        TEXT("Reference scope changed: initial city now has Academy or no source buildings"))) return false;
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
    TArray<TSharedPtr<FJsonValue>> BuildingHashes,TroopHashes;
    for (const auto& Pair:BuildingSnapshots)
    {
        auto Item=MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("actor"),Pair.Key.IsValid()?Pair.Key->GetName():FString());
        Item->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(Pair.Value));
        BuildingHashes.Add(MakeShared<FJsonValueObject>(Item));
    }
    for (const auto& Bytes:TroopSnapshots)
        TroopHashes.Add(MakeShared<FJsonValueString>(RokTrainArmyMesh::Sha256(Bytes)));
    Baseline->SetArrayField(TEXT("building_state_sha256"),BuildingHashes);
    Baseline->SetArrayField(TEXT("troop_state_sha256"),TroopHashes);
    bBaseline=true;
    return ValidateResearch(false,false,0);
}

bool URokResearchAnimationAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime
        && Snapshot(Economy->GetCurrentResources())==ResourceSnapshot
        && Snapshot(Runtime->GetPlayerProfileState())==ProfileSnapshot
        && Economy->GetActiveUpgradeCount()==QueueCount
        && Economy->GetOwnedBuildQueueCount()==OwnedQueueCount,
        TEXT("Animation browsing changed resources/profile/build queue"))) return false;
    const auto Stacks=Runtime->GetStarterTroopStacks();
    if (!Expect(Stacks.Num()==TroopSnapshots.Num(),TEXT("Animation browsing changed troop stack count"))) return false;
    for (int32 I=0;I<Stacks.Num();++I)
        if (!Expect(Snapshot(Stacks[I])==TroopSnapshots[I],TEXT("Animation browsing changed troop state"))) return false;
    for (const auto& Pair:BuildingSnapshots)
        if (!Expect(Pair.Key.IsValid()
            && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get()))==Pair.Value,
            TEXT("Animation browsing changed city building state"))) return false;
    return Expect(Screen.IsValid()
        && Screen->GetVisibleItemsInventoryFingerprintForAudit()==InventoryFingerprint,
        TEXT("Animation browsing changed local inventory projection"));
}
bool URokResearchAnimationAuditSubsystem::ValidateResearch(bool bDetail,bool bPower,int32 StudyType)
{
    if (!Identity() || !Unchanged()) return false;
    Research=Screen.IsValid()?Screen->GetResearchScreenForAudit():nullptr;
    if (!Expect(Screen.IsValid() && Screen->IsScreenOpen() && Screen->IsResearchModeForAudit()
        && Research.IsValid() && Research->IsReferencePreviewForAudit()
        && !Research->IsAccountResearchSyncedForAudit()
        && !Research->IsResearchQueueSyncedForAudit()
        && Research->GetLoadErrorForAudit().IsEmpty(),
        TEXT("Reference research context or unsynced account boundary changed"))) return false;
    if (!Expect(Research->GetSelectedTypeForAudit()==1
        && Research->GetSelectedStudyTypeForAudit()==StudyType
        && Research->GetViewedConfigLevelForAudit()==1
        && Research->GetRenderedNodesForAudit().Num()==71,
        TEXT("Research animation catalog/selection differs"))) return false;
    if (!Expect(Visible(Research->GetWidgetFromName(TEXT("ResearchDetailRoot")))==bDetail
        && Research->IsPowerPageForAudit()==bPower,
        TEXT("Research detail/power page differs from expected lifecycle"))) return false;
    if (bDetail && !Expect(
        Visible(Research->GetWidgetFromName(TEXT("ResearchPowerRoot")))==bPower
        && Visible(Research->GetWidgetFromName(TEXT("ResearchDetailUpdateRoot")))==!bPower,
        TEXT("Research animation source child visibility differs"))) return false;
    return true;
}
bool URokResearchAnimationAuditSubsystem::SlateClick(FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    const bool Allowed=Name==TEXT("ResearchNodeHit_102") || Name==TEXT("ResearchNodeHit_128")
        || Name==TEXT("ResearchDetailMore") || Name==TEXT("ResearchPowerBack")
        || Name==TEXT("ResearchDetailBack") || Name==TEXT("ResearchCloseButton")
        || Name==TEXT("ResearchDetailMaskHit");
    if (!Expect(Allowed,TEXT("Animation audit attempted unreviewed UI action"))) return false;
    UWidget* Target=Name==TEXT("ResearchCloseButton")
        ? Screen->GetWidgetFromName(Name)
        : Research.IsValid()?Research->GetWidgetFromName(Name):nullptr;
    if (!Expect(Target && Visible(Target) && Target->GetIsEnabled()
        && FSlateApplication::IsInitialized(),TEXT("Animation Slate target unavailable"))) return false;
    UWidget* Underlay=Name==TEXT("ResearchDetailMaskHit")
        ? Research->GetWidgetFromName(TEXT("ResearchMilitaryTab")):nullptr;
    if (Name==TEXT("ResearchDetailMaskHit")
        && !Expect(Underlay && !Underlay->IsHovered(),
            TEXT("Masked Military tab missing or already hovered"))) return false;
    const FGeometry Geometry=Underlay?Underlay->GetCachedGeometry():Target->GetCachedGeometry();
    const FVector2D Point=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*0.5f);
    FSlateApplication& App=FSlateApplication::Get();
    const FPointerEvent Move(0,Point,Point-FVector2D(16,16),TSet<FKey>(),
        EKeys::Invalid,0,FModifierKeysState());
    const bool MoveHandled=App.ProcessMouseMoveEvent(Move);
    const FWidgetPath Path=App.LocateWindowUnderMouse(
        Point,App.GetInteractiveTopLevelWindows(),false,0);
    if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get())
        && Target->IsHovered()
        && (!Underlay || (!Path.ContainsWidget(&Underlay->TakeWidget().Get())
            && !Underlay->IsHovered())),
        TEXT("Animation pointer did not reach hovered Slate target or passed through mask"))) return false;
    auto Action=MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("button"),Name.ToString());
    Action->SetStringField(TEXT("method"),
        TEXT("FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetNumberField(TEXT("world_elapsed_seconds"),Elapsed);
    Action->SetNumberField(TEXT("absolute_x"),Point.X);
    Action->SetNumberField(TEXT("absolute_y"),Point.Y);
    Action->SetBoolField(TEXT("move_handled"),MoveHandled);
    Action->SetBoolField(TEXT("target_in_hit_path"),true);
    Action->SetBoolField(TEXT("target_hovered_after_move"),Target->IsHovered());
    if (Underlay)
    {
        Action->SetStringField(TEXT("masked_underlay"),Underlay->GetName());
        Action->SetBoolField(TEXT("underlay_in_hit_path"),false);
        Action->SetBoolField(TEXT("underlay_hovered_after_move"),Underlay->IsHovered());
    }
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
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    return Expect(DownHandled && UpHandled,TEXT("Animation virtual click failed Slate routing"));
}
bool URokResearchAnimationAuditSubsystem::ScrollEconomyToEnd()
{
    if (!ValidateResearch(false,false,0)) return false;
    auto* Box=Cast<UScrollBox>(Research->GetWidgetFromName(TEXT("ResearchEconomyScroll")));
    if (!Expect(Box && Visible(Box) && Box->GetScrollOffsetOfEnd()>0,
        TEXT("Research economy tree has no source scroll end"))) return false;
    auto Action=MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("button"),TEXT("ResearchEconomyScroll"));
    Action->SetStringField(TEXT("method"),TEXT("UScrollBox.ScrollToEnd"));
    Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetNumberField(TEXT("step"),Step);
    Action->SetNumberField(TEXT("world_elapsed_seconds"),Elapsed);
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    Box->ScrollToEnd(); return true;
}
bool URokResearchAnimationAuditSubsystem::CheckClock(
    const TCHAR* Phase,float MinSeconds,float MaxSeconds)
{
    if (!Research.IsValid()) return false;
    if (!Research->GetAnimationPhaseForAudit().IsEqual(FName(Phase))) return false;
    const float Time=Research->GetAnimationElapsedSecondsForAudit();
    if (Time>MaxSeconds)
    {
        Finish(FString::Printf(TEXT("Missed live %s animation interval [%.3f, %.3f], actual %.3f"),
            Phase,MinSeconds,MaxSeconds,Time));
        return false;
    }
    return Time>=MinSeconds;
}
bool URokResearchAnimationAuditSubsystem::CheckCloseClock(
    float MinSeconds,float MaxSeconds)
{
    if (!Research.IsValid() || !Research->IsDetailClosingForAudit()) return false;
    const float Time=Research->GetDetailCloseElapsedSecondsForAudit();
    if (Time>MaxSeconds)
    {
        Finish(FString::Printf(TEXT("Missed live DetailClose interval [%.3f, %.3f], actual %.3f"),
            MinSeconds,MaxSeconds,Time));
        return false;
    }
    return Time>=MinSeconds;
}
TSharedRef<FJsonObject> URokResearchAnimationAuditSubsystem::State() const
{
    auto Json=MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("step"),Step);
    Json->SetNumberField(TEXT("frame"),GFrameCounter);
    Json->SetNumberField(TEXT("world_elapsed_seconds"),Elapsed);
    Json->SetBoolField(TEXT("state_unchanged"),true);
    Json->SetObjectField(TEXT("viewport_pixels"),MakeResearchAnimationRect(
        FVector2D::ZeroVector,UWidgetLayoutLibrary::GetViewportSize(this)));
    Json->SetBoolField(TEXT("screen_open"),Screen.IsValid() && Screen->IsScreenOpen());
    Json->SetBoolField(TEXT("research_mode"),Screen.IsValid() && Screen->IsResearchModeForAudit());
    Json->SetObjectField(TEXT("feature_close"),WidgetState(this,
        Screen.IsValid()?Screen->GetWidgetFromName(TEXT("ResearchCloseButton")):nullptr));
    Json->SetObjectField(TEXT("feature_dimmer"),WidgetState(this,
        Screen.IsValid()?Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer")):nullptr));
    if (const auto* Dimmer=Screen.IsValid()
        ? Cast<UBorder>(Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer"))):nullptr)
    {
        const FLinearColor Color=Dimmer->GetBrushColor();
        auto ColorJson=MakeShared<FJsonObject>();
        ColorJson->SetNumberField(TEXT("r"),Color.R);
        ColorJson->SetNumberField(TEXT("g"),Color.G);
        ColorJson->SetNumberField(TEXT("b"),Color.B);
        ColorJson->SetNumberField(TEXT("a"),Color.A);
        Json->SetObjectField(TEXT("feature_dimmer_color"),ColorJson);
    }
    if (!Research.IsValid()) return Json;
    Json->SetStringField(TEXT("phase"),Research->GetAnimationPhaseForAudit().ToString());
    Json->SetNumberField(TEXT("animation_elapsed_seconds"),
        Research->GetAnimationElapsedSecondsForAudit());
    Json->SetNumberField(TEXT("animation_normalized_time"),
        Research->GetAnimationNormalizedTimeForAudit());
    Json->SetNumberField(TEXT("root_open_elapsed_seconds"),
        Research->GetRootOpenElapsedSecondsForAudit());
    Json->SetNumberField(TEXT("child_show_elapsed_seconds"),
        Research->GetChildShowElapsedSecondsForAudit());
    Json->SetBoolField(TEXT("root_open_active"),
        Research->IsRootOpenAnimationActiveForAudit());
    Json->SetBoolField(TEXT("child_show_active"),
        Research->IsChildShowAnimationActiveForAudit());
    Json->SetNumberField(TEXT("pending_action_id"),Research->GetPendingActionIdForAudit());
    Json->SetNumberField(TEXT("interrupt_count"),Research->GetAnimationInterruptCountForAudit());
    Json->SetBoolField(TEXT("detail_closing"),Research->IsDetailClosingForAudit());
    Json->SetBoolField(TEXT("detail_closed"),Research->IsDetailClosedForAudit());
    Json->SetNumberField(TEXT("detail_close_elapsed_seconds"),
        Research->GetDetailCloseElapsedSecondsForAudit());
    Json->SetNumberField(TEXT("selected_type"),Research->GetSelectedTypeForAudit());
    Json->SetNumberField(TEXT("selected_study_type"),Research->GetSelectedStudyTypeForAudit());
    Json->SetNumberField(TEXT("viewed_config_level"),Research->GetViewedConfigLevelForAudit());
    Json->SetBoolField(TEXT("reference_preview"),Research->IsReferencePreviewForAudit());
    Json->SetBoolField(TEXT("account_research_synced"),Research->IsAccountResearchSyncedForAudit());
    Json->SetBoolField(TEXT("research_queue_synced"),Research->IsResearchQueueSyncedForAudit());
    Json->SetNumberField(TEXT("economy_scroll_offset"),Research->GetEconomyScrollOffsetForAudit());
    const auto Geometry=[&](const TCHAR* Name)
    { return WidgetState(this,Research->GetWidgetFromName(Name)); };
    Json->SetObjectField(TEXT("detail_root"),Geometry(TEXT("ResearchDetailRoot")));
    Json->SetObjectField(TEXT("detail_mask"),Geometry(TEXT("ResearchDetailMask")));
    Json->SetObjectField(TEXT("detail_mask_hit"),Geometry(TEXT("ResearchDetailMaskHit")));
    Json->SetObjectField(TEXT("detail_mask_shade"),Geometry(TEXT("ResearchDetailMaskShade")));
    if (const auto* Shade=Cast<UBorder>(Research->GetWidgetFromName(TEXT("ResearchDetailMaskShade"))))
    {
        const FLinearColor Color=Shade->GetBrushColor();
        auto ColorJson=MakeShared<FJsonObject>();
        ColorJson->SetNumberField(TEXT("r"),Color.R);
        ColorJson->SetNumberField(TEXT("g"),Color.G);
        ColorJson->SetNumberField(TEXT("b"),Color.B);
        ColorJson->SetNumberField(TEXT("a"),Color.A);
        Json->SetObjectField(TEXT("detail_mask_shade_color"),ColorJson);
    }
    Json->SetObjectField(TEXT("military_tab"),Geometry(TEXT("ResearchMilitaryTab")));
    Json->SetObjectField(TEXT("detail_update_root"),Geometry(TEXT("ResearchDetailUpdateRoot")));
    Json->SetObjectField(TEXT("power_root"),Geometry(TEXT("ResearchPowerRoot")));
    Json->SetObjectField(TEXT("detail_more"),Geometry(TEXT("ResearchDetailMore")));
    Json->SetObjectField(TEXT("power_back"),Geometry(TEXT("ResearchPowerBack")));
    Json->SetObjectField(TEXT("detail_back"),Geometry(TEXT("ResearchDetailBack")));
    Json->SetObjectField(TEXT("node_102"),Geometry(TEXT("ResearchNodeHit_102")));
    Json->SetObjectField(TEXT("node_128"),Geometry(TEXT("ResearchNodeHit_128")));
    return Json;
}
void URokResearchAnimationAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Unchanged()) return;
    if (FScreenshotRequest::IsScreenshotRequested())
    { Finish(TEXT("Conflicting screenshot request")); return; }
    PendingPath=Directory/(FString(Name)+TEXT(".png"));
    Pending=MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("file"),PendingPath);
    Pending->SetStringField(TEXT("phase_name"),Name);
    Pending->SetObjectField(TEXT("request"),State());
    ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false);
}
void URokResearchAnimationAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || !Unchanged()) return;
    const int64 Size=IFileManager::Get().FileSize(*PendingPath);
    if (Size<=0) { Finish(TEXT("Animation screenshot callback missing PNG")); return; }
    Pending->SetObjectField(TEXT("processed"),State());
    Pending->SetNumberField(TEXT("file_bytes"),Size);
    TArray<uint8> Png;
    if (!FFileHelper::LoadFileToArray(Png,*PendingPath))
    { Finish(TEXT("Animation screenshot cannot be hashed")); return; }
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Png));
    Captures.Add(MakeShared<FJsonValueObject>(Pending));
    Pending.Reset(); Save(false); Enter(Step+1);
}
void URokResearchAnimationAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !bOwnsWorld) return;
    Elapsed+=DeltaTime;
    if (Elapsed>100) { Finish(TEXT("Research animation audit timeout")); return; }
    if (Pending.IsValid())
    { if (Elapsed-ShotStarted>12) Finish(TEXT("Animation screenshot timeout")); return; }
    if (Elapsed<3.0) return;
    if (Step!=0 && Elapsed-StepStarted>4.0)
    { Finish(FString::Printf(TEXT("Research animation step %d timeout"),Step)); return; }
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: if (ValidateResearch(false,false,0) && SlateClick(TEXT("ResearchNodeHit_102"))) Enter(2); break;
    case 2: if (CheckClock(TEXT("DetailOpen"),.025f,.09f) && ValidateResearch(true,false,102))
        Capture(TEXT("01_open102_early")); break;
    case 3: if (Research.IsValid() && !Research->IsRootOpenAnimationActiveForAudit()
        && ValidateResearch(true,false,102) && SlateClick(TEXT("ResearchDetailMaskHit"))) Enter(4); break;
    case 4: if (CheckCloseClock(.025f,.08f) && ValidateResearch(true,false,102))
        Capture(TEXT("02_close102_early")); break;
    case 5: if (Research.IsValid() && Research->IsDetailClosedForAudit()
        && ValidateResearch(false,false,0)) Capture(TEXT("03_close102_callback")); break;
    case 6: if (ValidateResearch(false,false,0) && SlateClick(TEXT("ResearchNodeHit_102"))) Enter(7); break;
    case 7: if (CheckClock(TEXT("DetailOpen"),.10f,.19f) && ValidateResearch(true,false,102))
        Capture(TEXT("04_open102_mid")); break;
    case 8: if (Research.IsValid() && !Research->IsRootOpenAnimationActiveForAudit()
        && ValidateResearch(true,false,102) && SlateClick(TEXT("ResearchDetailBack"))) Enter(9); break;
    case 9:
        if (CheckCloseClock(.035f,.075f) && ValidateResearch(true,false,102))
        {
            CloseRepeatBefore=Research->GetDetailCloseElapsedSecondsForAudit();
            if (SlateClick(TEXT("ResearchDetailBack"))) Enter(10);
        }
        break;
    case 10:
        if (CheckCloseClock(.07f,.14f) && ValidateResearch(true,false,102)
            && Expect(Research->GetDetailCloseElapsedSecondsForAudit()>CloseRepeatBefore,
                TEXT("Repeated DetailBack reset source Close clock")))
            Capture(TEXT("05_close102_duplicate_mid")); break;
    case 11: if (Research.IsValid() && Research->IsDetailClosedForAudit()
        && ValidateResearch(false,false,0)) Capture(TEXT("06_close102_duplicate_callback")); break;
    case 12: if (ValidateResearch(false,false,0) && SlateClick(TEXT("ResearchNodeHit_102"))) Enter(13); break;
    case 13: if (CheckClock(TEXT("DetailOpen"),.18f,.249f) && ValidateResearch(true,false,102))
        Capture(TEXT("07_open102_late")); break;
    case 14: if (Research.IsValid() && !Research->IsRootOpenAnimationActiveForAudit()
        && ValidateResearch(true,false,102) && SlateClick(TEXT("ResearchDetailBack"))) Enter(15); break;
    case 15: if (Research.IsValid() && Research->IsDetailClosedForAudit()
        && ValidateResearch(false,false,0) && ScrollEconomyToEnd()) Enter(16); break;
    case 16: if (Elapsed-StepStarted>.12 && ValidateResearch(false,false,0)
        && SlateClick(TEXT("ResearchNodeHit_128"))) Enter(17); break;
    case 17: if (Research.IsValid() && Research->IsRootOpenAnimationActiveForAudit()
        && Research->GetRootOpenElapsedSecondsForAudit()>=.035f
        && Research->GetRootOpenElapsedSecondsForAudit()<.09f
        && ValidateResearch(true,false,128)
        && SlateClick(TEXT("ResearchDetailBack"))) Enter(18); break;
    case 18: if (CheckCloseClock(.025f,.10f) && ValidateResearch(true,false,128))
        Capture(TEXT("08_close128_during_open")); break;
    case 19: if (Research.IsValid() && Research->IsDetailClosedForAudit()
        && ValidateResearch(false,false,0)) Capture(TEXT("09_close128_callback")); break;
    case 20: if (ValidateResearch(false,false,0)
        && SlateClick(TEXT("ResearchNodeHit_128"))) Enter(21); break;
    case 21: if (Research.IsValid() && Research->IsRootOpenAnimationActiveForAudit()
        && Research->GetRootOpenElapsedSecondsForAudit()>=.035f
        && Research->GetRootOpenElapsedSecondsForAudit()<.09f
        && ValidateResearch(true,false,128)
        && SlateClick(TEXT("ResearchDetailMore"))) Enter(22); break;
    case 22: if (CheckClock(TEXT("PowerShow"),.025f,.11f)
        && ValidateResearch(true,true,128)) Capture(TEXT("10_power_overlap_early")); break;
    case 23: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && !Research->IsRootOpenAnimationActiveForAudit()
        && ValidateResearch(true,true,128)
        && SlateClick(TEXT("ResearchPowerBack"))) Enter(24); break;
    case 24: if (CheckClock(TEXT("ContentShow"),.025f,.11f)
        && ValidateResearch(true,false,128)) Capture(TEXT("11_content_early")); break;
    case 25: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,false,128)) Enter(26); break;
    case 26: if (SlateClick(TEXT("ResearchDetailMore"))) Enter(27); break;
    case 27: if (CheckClock(TEXT("PowerShow"),.105f,.19f)
        && ValidateResearch(true,true,128)) Capture(TEXT("12_power_mid")); break;
    case 28: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,true,128)
        && SlateClick(TEXT("ResearchPowerBack"))) Enter(29); break;
    case 29: if (CheckClock(TEXT("ContentShow"),.105f,.19f)
        && ValidateResearch(true,false,128)) Capture(TEXT("13_content_mid")); break;
    case 30: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,false,128)) Enter(31); break;
    case 31: if (SlateClick(TEXT("ResearchDetailMore"))) Enter(32); break;
    case 32: if (CheckClock(TEXT("PowerShow"),.18f,.249f)
        && ValidateResearch(true,true,128)) Capture(TEXT("14_power_late")); break;
    case 33: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,true,128)
        && SlateClick(TEXT("ResearchPowerBack"))) Enter(34); break;
    case 34: if (CheckClock(TEXT("ContentShow"),.18f,.249f)
        && ValidateResearch(true,false,128)) Capture(TEXT("15_content_late")); break;
    case 35: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,false,128)) Enter(36); break;
    case 36: if (SlateClick(TEXT("ResearchDetailMore"))) Enter(37); break;
    case 37: if (Research.IsValid() && Research->IsChildShowAnimationActiveForAudit()
        && Research->GetChildShowElapsedSecondsForAudit()>=.05f
        && Research->GetChildShowElapsedSecondsForAudit()<.11f
        && ValidateResearch(true,true,128))
        {
            InitialInterruptCount=Research->GetAnimationInterruptCountForAudit();
            if (SlateClick(TEXT("ResearchPowerBack"))) Enter(38);
        }
        break;
    case 38: if (CheckClock(TEXT("ContentShow"),.025f,.11f)
        && ValidateResearch(true,false,128)
        && Expect(Research->GetAnimationInterruptCountForAudit()==InitialInterruptCount+1,
            TEXT("PowerBack did not interrupt active PowerShow")))
        Capture(TEXT("16_content_after_power_interrupt")); break;
    case 39: if (Research.IsValid() && !Research->IsChildShowAnimationActiveForAudit()
        && ValidateResearch(true,false,128)) Enter(40); break;
    case 40: if (SlateClick(TEXT("ResearchDetailMore"))) Enter(41); break;
    case 41: if (Research.IsValid() && Research->IsChildShowAnimationActiveForAudit()
        && Research->GetChildShowElapsedSecondsForAudit()>=.05f
        && Research->GetChildShowElapsedSecondsForAudit()<.11f
        && ValidateResearch(true,true,128)
        && SlateClick(TEXT("ResearchDetailBack"))) Enter(42); break;
    case 42: if (CheckCloseClock(.025f,.08f)
        && ValidateResearch(true,true,128))
        {
            CloseAtChildSwitch=Research->GetDetailCloseElapsedSecondsForAudit();
            if (SlateClick(TEXT("ResearchPowerBack"))) Enter(43);
        }
        break;
    case 43: if (CheckCloseClock(.055f,.14f)
        && ValidateResearch(true,false,128)
        && Expect(Research->GetDetailCloseElapsedSecondsForAudit()>CloseAtChildSwitch
            && Research->IsChildShowAnimationActiveForAudit(),
            TEXT("Close stopped root clock or blocked source child Back")))
        {
            CloseAtMaskClick=Research->GetDetailCloseElapsedSecondsForAudit();
            if (SlateClick(TEXT("ResearchDetailMaskHit")))
                Capture(TEXT("17_close_with_child_show"));
        }
        break;
    case 44:
        if (Research.IsValid() && Research->IsDetailClosingForAudit()
            && !Expect(Research->GetDetailCloseElapsedSecondsForAudit()+.005f>=CloseAtMaskClick,
                TEXT("Masked click restarted source detail Close clock"))) break;
        if (Research.IsValid() && Research->IsDetailClosedForAudit()
            && ValidateResearch(false,false,0)) Capture(TEXT("18_close_from_power_callback")); break;
    case 45: if (ValidateResearch(false,false,0)
        && SlateClick(TEXT("ResearchCloseButton"))) Enter(46); break;
    case 46:
        if (Screen.IsValid() && !Screen->IsScreenOpen()
            && !Screen->GetResearchScreenForAudit())
        {
            if (!Expect(Elapsed-StepStarted<.15,
                TEXT("Main Research force-close unexpectedly waited for animation"))) break;
            Research.Reset(); Capture(TEXT("19_main_force_closed"));
        }
        break;
    case 47: if (Screen.IsValid() && !Screen->IsScreenOpen() && Hud.IsValid())
        { Hud->OpenCommandLineModal(); Enter(48); } break;
    case 48:
        Research=Screen.IsValid()?Screen->GetResearchScreenForAudit():nullptr;
        if (Research.IsValid() && ValidateResearch(false,false,0)
            && Expect(Research->GetAnimationInterruptCountForAudit()==0
                && !Research->IsRootOpenAnimationActiveForAudit()
                && !Research->IsChildShowAnimationActiveForAudit()
                && Research->GetAnimationPhaseForAudit()==TEXT("Idle")
                && FMath::IsNearlyZero(Research->GetEconomyScrollOffsetForAudit(),.01f),
                TEXT("New UE Research instance retained animation/tab/scroll state")))
            Capture(TEXT("20_reference_reopened_reset")); break;
    case 49: Finish(); break;
    default: Finish(TEXT("Invalid research animation audit step")); break;
    }
}
bool URokResearchAnimationAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.research_animation.runtime.v1"));
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
    Root->SetStringField(TEXT("scope"),TEXT("GameApp city HUD reference Research; virtual Slate Move/Down/Up, actual UMG animation clocks/transforms and PNG phases. Isolated Unity preview is source presentation only. No Academy actor, account research, research request, physical hardware pointer, or production Academy entry verified."));
    Root->SetNumberField(TEXT("expected_capture_count"),ExpectedCaptureCount);
    Root->SetStringField(TEXT("source_contract"),SourceContract);
    Root->SetStringField(TEXT("native_baseline_evidence"),NativeBaselineEvidence);
    Root->SetStringField(TEXT("native_baseline_verified"),NativeBaselineVerified);
    Root->SetStringField(TEXT("native_evidence"),NativeEvidence);
    Root->SetStringField(TEXT("native_verified"),NativeVerified);
    if (InputHashes.IsValid()) Root->SetObjectField(TEXT("input_hashes"),InputHashes.ToSharedRef());
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
void URokResearchAnimationAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num()!=ExpectedCaptureCount)
        Failures.Add(TEXT("Research animation audit did not process all twenty screenshots"));
    if (!Failures.IsEmpty()) FailureState=State();
    bFinished=true;
    const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures)
        UE_LOG(LogTemp,Error,TEXT("Rok ResearchAnimationAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok ResearchAnimationAudit passed=%d captures=%d directory=%s"),
        Failures.IsEmpty() && Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty() && Written?0:1);
}
