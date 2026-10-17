#include "RokResearchAuditSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokResearchScreenWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
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
constexpr int32 CaptureCount = 20;
constexpr const TCHAR* AuditFlag = TEXT("-RokResearchAudit");
constexpr const TCHAR* CityMap = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr const TCHAR* NativeBaselineEvidence = TEXT("Saved/UnityResearchNativeProbe/Artifacts/native_20261008_124117_758/render_evidence.json");
constexpr const TCHAR* NativePowerEvidence = TEXT("Saved/UnityResearchNativeProbe/Artifacts/native_20261008_131955_853/render_evidence.json");
constexpr const TCHAR* NativePowerFixture = TEXT("Saved/RokUnityMigration/ResearchNative/research_power_fixture.json");
constexpr const TCHAR* NativePowerVerified = TEXT("Saved/RokUnityMigration/ResearchNative/verified_power_capture.json");
// Whole-file identity pins the reviewed local-only ActionId 30 feedback path.
constexpr const TCHAR* ReviewedResearchUiSourceSha256 = TEXT("fe2e13b0c6524fde86f4432c68fdbc01de6d81d2eec3c0fc47389dc93a889e49");

bool TargetWorld(const UWorld* World)
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer, false);
    T::StaticStruct()->SerializeItem(Archive, &Value, nullptr);
    return Bytes;
}
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent=Widget->GetParent(); Parent; Parent=Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    for (const UObject* Outer=Widget->GetOuter(); Outer; Outer=Outer->GetOuter())
        if (const auto* User=Cast<UUserWidget>(Outer); User && (!User->IsVisible() || User->GetRenderOpacity() <= 0)) return false;
    return true;
}
TSharedRef<FJsonObject> MakeResearchRectJson(FVector2D Origin, FVector2D Size)
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("x"),Origin.X); Result->SetNumberField(TEXT("y"),Origin.Y);
    Result->SetNumberField(TEXT("width"),Size.X); Result->SetNumberField(TEXT("height"),Size.Y);
    return Result;
}
TSharedRef<FJsonObject> WidgetJson(const UObject* Context,const UWidget* Widget)
{
    auto Result=MakeShared<FJsonObject>(); Result->SetBoolField(TEXT("attached"),Widget!=nullptr);
    if (!Widget) return Result;
    Result->SetBoolField(TEXT("visible"),Visible(Widget));
    Result->SetNumberField(TEXT("opacity"),Widget->GetRenderOpacity());
    Result->SetBoolField(TEXT("hovered"),Widget->IsHovered());
    if (const auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        Result->SetObjectField(TEXT("slot_rect"),MakeResearchRectJson(Slot->GetPosition(),Slot->GetSize()));
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
        const FVector2D A=Pixel(FVector2D::ZeroVector), B=Pixel(Geometry.GetLocalSize());
        Result->SetObjectField(TEXT("pixel_rect"),MakeResearchRectJson(
            FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),
            FVector2D(FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y))));
    }
    if (const auto* Text=Cast<UTextBlock>(Widget))
    {
        Result->SetStringField(TEXT("text"),Text->GetText().ToString());
        Result->SetNumberField(TEXT("font_points"),Text->GetFont().Size);
        Result->SetStringField(TEXT("font_style"),Text->GetFont().TypefaceFontName.ToString());
        const FColor C=Text->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true);
        auto Color=MakeShared<FJsonObject>(); Color->SetNumberField(TEXT("r"),C.R);
        Color->SetNumberField(TEXT("g"),C.G); Color->SetNumberField(TEXT("b"),C.B);
        Color->SetNumberField(TEXT("a"),C.A); Result->SetObjectField(TEXT("display_srgb"),Color);
    }
    if (const auto* Image=Cast<UImage>(Widget))
    {
        const FSlateBrush& Brush=Image->GetBrush();
        Result->SetStringField(TEXT("texture"),Brush.GetResourceObject()?Brush.GetResourceObject()->GetPathName():FString());
        const FBox2f UV=Brush.GetUVRegion(); auto Region=MakeShared<FJsonObject>();
        Region->SetNumberField(TEXT("min_x"),UV.Min.X); Region->SetNumberField(TEXT("min_y"),UV.Min.Y);
        Region->SetNumberField(TEXT("max_x"),UV.Max.X); Region->SetNumberField(TEXT("max_y"),UV.Max.Y);
        Result->SetObjectField(TEXT("uv"),Region);
    }
    return Result;
}
TSharedRef<FJsonObject> RowJson(const FRokResearchRow& Row)
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("id"),Row.Id); Result->SetNumberField(TEXT("type"),Row.Type);
    Result->SetNumberField(TEXT("study_type"),Row.StudyType); Result->SetNumberField(TEXT("level"),Row.Level);
    Result->SetNumberField(TEXT("max_level"),Row.MaxLevel); Result->SetNumberField(TEXT("campus_level"),Row.CampusLevel);
    Result->SetNumberField(TEXT("columns"),Row.Columns); Result->SetBoolField(TEXT("has_location"),Row.bHasLocation);
    Result->SetNumberField(TEXT("location"),Row.Location); Result->SetNumberField(TEXT("source_row"),Row.SourceRow);
    Result->SetNumberField(TEXT("food"),Row.BaseFood); Result->SetNumberField(TEXT("wood"),Row.BaseWood);
    Result->SetNumberField(TEXT("stone"),Row.BaseStone); Result->SetNumberField(TEXT("gold"),Row.BaseGold);
    Result->SetNumberField(TEXT("seconds"),Row.BaseSeconds); Result->SetNumberField(TEXT("power"),Row.Power);
    Result->SetStringField(TEXT("name"),Row.Name); Result->SetStringField(TEXT("description"),Row.Description);
    Result->SetStringField(TEXT("function_text"),Row.FunctionText);
    Result->SetStringField(TEXT("icon_source"),Row.IconSource);
    Result->SetStringField(TEXT("icon_atlas"),Row.IconAtlas);
    Result->SetStringField(TEXT("icon_sprite"),Row.IconSprite);
    Result->SetObjectField(TEXT("icon_rect"),MakeResearchRectJson(
        FVector2D(Row.IconRect.Min.X,Row.IconRect.Min.Y),
        FVector2D(Row.IconRect.Width(),Row.IconRect.Height())));
    Result->SetObjectField(TEXT("icon_atlas_size"),MakeResearchRectJson(FVector2D::ZeroVector,
        FVector2D(Row.IconAtlasSize.X,Row.IconAtlasSize.Y)));
    TArray<TSharedPtr<FJsonValue>> Preconditions;
    for (const auto& Pre:Row.Preconditions)
    {
        auto P=MakeShared<FJsonObject>(); P->SetNumberField(TEXT("slot"),Pre.Slot);
        P->SetNumberField(TEXT("study_type"),Pre.StudyType); P->SetNumberField(TEXT("level"),Pre.Level);
        Preconditions.Add(MakeShared<FJsonValueObject>(P));
    }
    Result->SetArrayField(TEXT("preconditions"),Preconditions);
    return Result;
}
bool SameRow(const FRokResearchRow& A,const FRokResearchRow& B)
{
    if (A.Id!=B.Id || A.Type!=B.Type || A.StudyType!=B.StudyType || A.Level!=B.Level
        || A.MaxLevel!=B.MaxLevel || A.CampusLevel!=B.CampusLevel || A.Columns!=B.Columns
        || A.bHasLocation!=B.bHasLocation || A.Location!=B.Location || A.Name!=B.Name
        || A.Description!=B.Description || A.FunctionText!=B.FunctionText
        || A.IconSource!=B.IconSource || A.IconAtlas!=B.IconAtlas || A.IconSprite!=B.IconSprite
        || A.IconRect!=B.IconRect || A.IconAtlasSize!=B.IconAtlasSize
        || A.Preconditions.Num()!=B.Preconditions.Num()) return false;
    for (int32 I=0;I<A.Preconditions.Num();++I)
        if (A.Preconditions[I].Slot!=B.Preconditions[I].Slot
            || A.Preconditions[I].StudyType!=B.Preconditions[I].StudyType
            || A.Preconditions[I].Level!=B.Preconditions[I].Level) return false;
    return true;
}
}

bool URokResearchAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(),TEXT("RokResearchAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokResearchAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game; }
TStatId URokResearchAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokResearchAuditSubsystem,STATGROUP_Tickables); }
void URokResearchAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsWorld=TargetWorld(GetWorld()); if (!bOwnsWorld) return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/ResearchAudit")
        /FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokResearchAuditSubsystem::ScreenshotProcessed);
}
void URokResearchAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsWorld && !bFinished) Finish(TEXT("Target city world ended before research audit completion"));
    Super::Deinitialize();
}
bool URokResearchAuditSubsystem::Expect(bool Condition,const TCHAR* Error)
{ if (!Condition) Finish(Error); return Condition; }
bool URokResearchAuditSubsystem::Identity()
{ return Expect(TargetWorld(GetWorld()),TEXT("Wrong project/map; no research UI action permitted")); }
void URokResearchAuditSubsystem::Enter(int32 Next) { Step=Next; StepStarted=Elapsed; }

bool URokResearchAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    FString Entry; FParse::Value(FCommandLine::Get(),TEXT("RokOpenModal="),Entry);
    if (!Expect(Entry.Equals(TEXT("research_reference"),ESearchCase::IgnoreCase),
        TEXT("Research audit requires -RokOpenModal=research_reference"))) return false;
    const TCHAR* Cursor=FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor,Token,false))
        if (Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)
            && !Token.Equals(AuditFlag,ESearchCase::IgnoreCase)
            && !Token.Equals(TEXT("-RokOpenModal=research_reference"),ESearchCase::IgnoreCase))
        { Finish(TEXT("Research audit requires a dedicated process without other Rok flags")); return false; }
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
        && Research->IsReferencePreviewForAudit() && Research->GetLoadErrorForAudit().IsEmpty(),
        TEXT("HUD command-line research reference did not open"))) return false;
    FString Error;
    if (!Expect(Catalog.Load(Error) && Catalog.NumRows()==410 && Catalog.NumCivilizationOverrides()==8,
        TEXT("Source research catalog failed to load exact 410-row configuration"))) return false;
    InputHashes=MakeShared<FJsonObject>();
    const TCHAR* Required[]={
        TEXT("Saved/RokUnityMigration/research_catalog.json"),
        TEXT("Saved/RokUnityMigration/ResearchNative/atlas_manifest.json"),
        TEXT("Saved/RokUnityMigration/ResearchNative/research_fixture.json"),
        TEXT("Saved/RokUnityMigration/ResearchNative/verified_capture.json"),
        NativePowerFixture,NativePowerVerified,NativePowerEvidence,
        TEXT("Saved/RokUnityMigration/ResearchNative/gray_material_source.json"),
        TEXT("Saved/RokUnityMigration/ResearchNative/gray_material_ue.json"),
        NativeBaselineEvidence,
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"),
        TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"),
        TEXT("Source/Rok/RokResearchAuditSubsystem.h"),TEXT("Source/Rok/RokResearchAuditSubsystem.cpp"),
        TEXT("Scripts/VerifyRokResearchRuntime.py"),
        TEXT("Source/Rok/RokResearchCatalog.h"),TEXT("Source/Rok/RokResearchCatalog.cpp"),
        TEXT("Source/Rok/RokResearchScreenWidget.h"),TEXT("Source/Rok/RokResearchScreenWidget.cpp"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
        TEXT("Source/Rok/RokMainHudWidget.cpp"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/Proxy/ResearchProxy.cs"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchMainMediator.cs"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Research/ResearchUpateMediator.cs")};
    for (const TCHAR* Relative:Required)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/Relative)) && !Bytes.IsEmpty(),
            TEXT("Required research source input unavailable"))) return false;
        auto File=MakeShared<FJsonObject>(); File->SetNumberField(TEXT("bytes"),Bytes.Num());
        File->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(Bytes));
        InputHashes->SetObjectField(Relative,File);
        if (FString(Relative)==TEXT("Saved/RokUnityMigration/research_catalog.json"))
            CatalogHash=RokTrainArmyMesh::Sha256(Bytes);
    }
    int32 AcademyActors=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        bool Tagged=false;
        for (FName Tag:It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { Tagged=true; break; }
        if (!Tagged) continue;
        const auto State=Economy->GetBuildingStateForActor(*It);
        if (State.UnityInitId>0) BuildingSnapshots.Add(*It,Snapshot(State));
        if (State.UnityBuildingTypeId==13) ++AcademyActors;
    }
    if (!Expect(AcademyActors==0 && BuildingSnapshots.Num()>0,
        TEXT("Reference scope changed: source initial city now contains an Academy actor"))) return false;
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
    TArray<TSharedPtr<FJsonValue>> BuildingHashes, TroopHashes;
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
    return Validate();
}

bool URokResearchAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime && Snapshot(Economy->GetCurrentResources())==ResourceSnapshot
        && Snapshot(Runtime->GetPlayerProfileState())==ProfileSnapshot
        && Economy->GetActiveUpgradeCount()==QueueCount && Economy->GetOwnedBuildQueueCount()==OwnedQueueCount,
        TEXT("Research browsing changed resources/profile/build queue"))) return false;
    const auto Stacks=Runtime->GetStarterTroopStacks();
    if (!Expect(Stacks.Num()==TroopSnapshots.Num(),TEXT("Research browsing changed troop stack count"))) return false;
    for (int32 I=0;I<Stacks.Num();++I)
        if (!Expect(Snapshot(Stacks[I])==TroopSnapshots[I],TEXT("Research browsing changed troop state"))) return false;
    for (const auto& Pair:BuildingSnapshots)
        if (!Expect(Pair.Key.IsValid() && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get()))==Pair.Value,
            TEXT("Research browsing changed city building state"))) return false;
    return Expect(Screen.IsValid()
        && Screen->GetVisibleItemsInventoryFingerprintForAudit()==InventoryFingerprint,
        TEXT("Research browsing changed local inventory projection"));
}

bool URokResearchAuditSubsystem::Validate()
{
    if (!Identity() || !Unchanged()) return false;
    const FVector2D Pixels=UWidgetLayoutLibrary::GetViewportSize(this);
    if (!Expect(Pixels.Equals(FVector2D(1440,810),.1),TEXT("Research audit viewport must be 1440x810"))) return false;
    if (ExpectedMode==EMode::Closed)
        return Expect(!Screen->IsScreenOpen() && !Screen->GetResearchScreenForAudit(),
            TEXT("Closed feature retained research child"));
    if (ExpectedMode==EMode::Mail)
        return Expect(Screen->IsScreenOpen() && Screen->IsMailModeForAudit()
            && !Screen->GetResearchScreenForAudit(),TEXT("Switch to existing mail feature retained research tree"));
    Research=Screen->GetResearchScreenForAudit();
    if (!Expect(Screen->IsScreenOpen() && Screen->IsResearchModeForAudit() && Research.IsValid()
        && Research->IsReferencePreviewForAudit() && !Research->IsAccountResearchSyncedForAudit()
        && !Research->IsResearchQueueSyncedForAudit() && Research->GetLoadErrorForAudit().IsEmpty(),
        TEXT("Research reference context or unknown account/queue boundary changed"))) return false;
    if (!Expect(Research->GetSelectedTypeForAudit()==ExpectedType
        && Research->GetSelectedStudyTypeForAudit()==ExpectedStudyType
        && Research->GetViewedConfigLevelForAudit()==ExpectedLevel
        && Research->GetLastBoundaryActionIdForAudit()==ExpectedBoundaryActionId,
        TEXT("Research tab/detail/config level differs from navigation expectation"))) return false;
    const auto* Detail=Research->GetWidgetFromName(TEXT("ResearchDetailRoot"));
    if (!Expect(Visible(Detail)==bExpectDetail
        && Research->IsPowerPageForAudit()==bExpectPower,
        TEXT("Research detail/power page visibility differs"))) return false;
    if (bExpectDetail && !Expect(
        Visible(Research->GetWidgetFromName(TEXT("ResearchPowerRoot")))==bExpectPower
        && Visible(Research->GetWidgetFromName(TEXT("ResearchDetailUpdateRoot")))==!bExpectPower,
        TEXT("Research detail/power subpage visibility differs"))) return false;
    const auto& Nodes=Research->GetRenderedNodesForAudit();
    if (!Expect(Nodes.Num()==71,TEXT("Research UI must render all 71 distinct first-level source nodes"))) return false;
    TSet<int32> Seen;
    int32 Counts[3]={0,0,0};
    for (const auto& Row:Nodes)
    {
        FRokResearchRow Source;
        if (!Expect(Row.Level==1 && (Row.Type==1 || Row.Type==2) && !Seen.Contains(Row.StudyType)
            && Catalog.GetByStudyLevel(Row.StudyType,1,Source) && SameRow(Row,Source),
            TEXT("Rendered research node differs from source level-one catalog"))) return false;
        Seen.Add(Row.StudyType); ++Counts[Row.Type];
        if (!Expect(Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNode_%d"),Row.StudyType))
            && Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeIcon_%d"),Row.StudyType))
            && Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeName_%d"),Row.StudyType))
            && Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeHit_%d"),Row.StudyType)),
            TEXT("Research catalog node lacks rendered UMG widget mapping"))) return false;
    }
    if (!Expect(Counts[1]==28 && Counts[2]==43,
        TEXT("Research economy/military first-level node distribution differs"))) return false;
    if (!Expect(Visible(Research->GetWidgetFromName(TEXT("ResearchEconomyScroll")))==(ExpectedType==1)
        && Visible(Research->GetWidgetFromName(TEXT("ResearchMilitaryScroll")))==(ExpectedType==2),
        TEXT("Research tab scroll visibility differs"))) return false;
    if (bExpectDetail)
    {
        FRokResearchRow Config;
        if (!Expect(Catalog.GetByStudyLevel(ExpectedStudyType,ExpectedLevel,Config),
            TEXT("Research detail config level is absent from source catalog"))) return false;
        const auto* Title=Cast<UTextBlock>(Research->GetWidgetFromName(TEXT("ResearchDetailTitle")));
        const auto* Level=Cast<UTextBlock>(Research->GetWidgetFromName(TEXT("ResearchDetailLevel")));
        if (!Expect(Title && Level && (bExpectPower || (Visible(Title) && Visible(Level)))
            && Title->GetText().ToString().Contains(Config.Name),
            TEXT("Research detail title/config source name differs"))) return false;
    }
    return true;
}

bool URokResearchAuditSubsystem::VerifyLocalBoundary() const
{
    if (FString(ReviewedResearchUiSourceSha256).IsEmpty()) return false;
    TArray<uint8> Bytes;
    const FString Path=FPaths::ProjectDir()/TEXT("Source/Rok/RokResearchScreenWidget.cpp");
    return FFileHelper::LoadFileToArray(Bytes,*Path)
        && RokTrainArmyMesh::Sha256(Bytes)==ReviewedResearchUiSourceSha256;
}

bool URokResearchAuditSubsystem::SlateClick(FName Name)
{
    if (!Identity() || !Unchanged() || !Validate()) return false;
    const FString S=Name.ToString();
    const bool Allowed=S==TEXT("ResearchEconomyTab") || S==TEXT("ResearchMilitaryTab")
        || S==TEXT("ResearchNodeHit_102") || S==TEXT("ResearchNodeHit_128")
        || S==TEXT("ResearchNodeHit_202") || S==TEXT("ResearchDetailMore")
        || S==TEXT("ResearchPowerBack")
        || S==TEXT("ResearchDetailLevelNext") || S==TEXT("ResearchDetailLevelPrev")
        || S==TEXT("ResearchDetailBack") || S==TEXT("ResearchCloseButton")
        || S==TEXT("ResearchDetailBoundaryButton");
    if (!Expect(Allowed,TEXT("Research audit attempted unreviewed UI action"))) return false;
    if (Name==TEXT("ResearchDetailBoundaryButton")
        && !Expect(VerifyLocalBoundary(),TEXT("Research boundary source differs from reviewed local-only code"))) return false;
    UWidget* Target=Name==TEXT("ResearchCloseButton")
        ? Screen->GetWidgetFromName(Name)
        : Research.IsValid()?Research->GetWidgetFromName(Name):nullptr;
    if (!Expect(Target && Visible(Target) && Target->GetIsEnabled() && FSlateApplication::IsInitialized(),
        TEXT("Research virtual pointer target unavailable"))) return false;
    const FGeometry Geometry=Target->GetCachedGeometry();
    const FVector2D Point=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*0.5f);
    FSlateApplication& App=FSlateApplication::Get();
    const FPointerEvent Move(0,Point,Point-FVector2D(16,16),TSet<FKey>(),
        EKeys::Invalid,0,FModifierKeysState());
    const bool MoveHandled=App.ProcessMouseMoveEvent(Move);
    const FWidgetPath Path=App.LocateWindowUnderMouse(Point,App.GetInteractiveTopLevelWindows(),false,0);
    if (!Expect(Path.IsValid() && Path.ContainsWidget(&Target->TakeWidget().Get())
        && Target->IsHovered(),TEXT("Research virtual pointer did not reach hovered target"))) return false;
    auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),S);
    Action->SetStringField(TEXT("method"),TEXT("FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetNumberField(TEXT("absolute_x"),Point.X); Action->SetNumberField(TEXT("absolute_y"),Point.Y);
    Action->SetBoolField(TEXT("move_handled"),MoveHandled);
    Action->SetBoolField(TEXT("slate_hit_path_valid"),true);
    Action->SetBoolField(TEXT("target_in_hit_path"),true);
    Action->SetBoolField(TEXT("target_hovered_after_move"),Target->IsHovered());
    Action->SetBoolField(TEXT("boundary_local_only_source_reviewed"),
        Name==TEXT("ResearchDetailBoundaryButton") && VerifyLocalBoundary());
    TArray<TSharedPtr<FJsonValue>> Types;
    for (int32 I=0;I<Path.Widgets.Num();++I)
        Types.Add(MakeShared<FJsonValueString>(Path.Widgets[I].Widget->GetTypeAsString()));
    Action->SetArrayField(TEXT("slate_widget_path_types"),Types);
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const FPointerEvent Down(0,Point,Point,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());
    const FPointerEvent Up(0,Point,Point,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    const bool DownHandled=App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),Down);
    const bool UpHandled=App.ProcessMouseButtonUpEvent(Up);
    Action->SetBoolField(TEXT("down_handled"),DownHandled); Action->SetBoolField(TEXT("up_handled"),UpHandled);
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    return Expect(DownHandled && UpHandled,TEXT("Research virtual left click did not route through Slate"));
}

bool URokResearchAuditSubsystem::DelegateClick(UUserWidget* Owner,FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Name==TEXT("MailFeatureHitButton") || Name==TEXT("FeatureFrameClose"),
        TEXT("Research audit attempted unreviewed HUD delegate"))) return false;
    auto* Button=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
    if (!Expect(Button && Button->GetIsEnabled(),TEXT("Research cross-feature HUD/close button unavailable"))) return false;
    auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),Name.ToString());
    Action->SetStringField(TEXT("method"),TEXT("UButton.OnClicked.Broadcast"));
    Action->SetStringField(TEXT("scope"),TEXT("Programmatic existing HUD/feature delegate; not a physical Slate hit"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Actions.Add(MakeShared<FJsonValueObject>(Action)); Button->OnClicked.Broadcast(); return true;
}
bool URokResearchAuditSubsystem::ScrollToEnd(int32 Type)
{
    if (!Validate() || (Type!=1 && Type!=2)) return false;
    auto* Box=Cast<UScrollBox>(Research->GetWidgetFromName(Type==1?TEXT("ResearchEconomyScroll"):TEXT("ResearchMilitaryScroll")));
    if (!Expect(Box && Visible(Box) && Box->GetScrollOffsetOfEnd()>0,
        TEXT("Research horizontal source tree has no scrollable end"))) return false;
    auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),Type==1?TEXT("ResearchEconomyScroll"):TEXT("ResearchMilitaryScroll"));
    Action->SetStringField(TEXT("method"),TEXT("UScrollBox.ScrollToEnd"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Actions.Add(MakeShared<FJsonValueObject>(Action)); Box->ScrollToEnd(); return true;
}

TSharedRef<FJsonObject> URokResearchAuditSubsystem::State() const
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("step"),Step); Result->SetNumberField(TEXT("frame"),GFrameCounter);
    Result->SetNumberField(TEXT("elapsed"),Elapsed);
    Result->SetStringField(TEXT("page"),ExpectedMode==EMode::Research?(bExpectPower?TEXT("power")
        :bExpectDetail?TEXT("detail"):TEXT("tree"))
        :ExpectedMode==EMode::Mail?TEXT("mail"):TEXT("closed"));
    Result->SetBoolField(TEXT("state_unchanged"),true);
    Result->SetObjectField(TEXT("viewport_pixels"),MakeResearchRectJson(FVector2D::ZeroVector,
        UWidgetLayoutLibrary::GetViewportSize(this)));
    Result->SetBoolField(TEXT("feature_open"),Screen.IsValid() && Screen->IsScreenOpen());
    if (ExpectedMode!=EMode::Research || !Research.IsValid())
    {
        Result->SetBoolField(TEXT("research_attached"),Screen.IsValid() && Screen->GetResearchScreenForAudit()!=nullptr);
        return Result;
    }
    Result->SetBoolField(TEXT("research_attached"),true);
    Result->SetBoolField(TEXT("reference_preview"),Research->IsReferencePreviewForAudit());
    Result->SetBoolField(TEXT("account_research_synced"),Research->IsAccountResearchSyncedForAudit());
    Result->SetBoolField(TEXT("research_queue_synced"),Research->IsResearchQueueSyncedForAudit());
    Result->SetNumberField(TEXT("selected_type"),Research->GetSelectedTypeForAudit());
    Result->SetNumberField(TEXT("selected_study_type"),Research->GetSelectedStudyTypeForAudit());
    Result->SetNumberField(TEXT("viewed_config_level"),Research->GetViewedConfigLevelForAudit());
    Result->SetNumberField(TEXT("economy_scroll_offset"),Research->GetEconomyScrollOffsetForAudit());
    Result->SetNumberField(TEXT("military_scroll_offset"),Research->GetMilitaryScrollOffsetForAudit());
    Result->SetNumberField(TEXT("last_boundary_action_id"),Research->GetLastBoundaryActionIdForAudit());
    Result->SetBoolField(TEXT("power_page_open"),Research->IsPowerPageForAudit());
    Result->SetNumberField(TEXT("power_scroll_offset"),Research->GetPowerScrollOffsetForAudit());
    Result->SetStringField(TEXT("load_error"),Research->GetLoadErrorForAudit());
    const auto Geometry=[&](const TCHAR* Name){return WidgetJson(this,Research->GetWidgetFromName(Name));};
    Result->SetObjectField(TEXT("root"),Geometry(TEXT("ResearchCanvas")));
    Result->SetObjectField(TEXT("economy_tab"),Geometry(TEXT("ResearchEconomyTab")));
    Result->SetObjectField(TEXT("military_tab"),Geometry(TEXT("ResearchMilitaryTab")));
    Result->SetObjectField(TEXT("economy_scroll"),Geometry(TEXT("ResearchEconomyScroll")));
    Result->SetObjectField(TEXT("military_scroll"),Geometry(TEXT("ResearchMilitaryScroll")));
    Result->SetObjectField(TEXT("detail_root"),Geometry(TEXT("ResearchDetailRoot")));
    Result->SetObjectField(TEXT("detail_update_root"),Geometry(TEXT("ResearchDetailUpdateRoot")));
    Result->SetObjectField(TEXT("detail_more"),Geometry(TEXT("ResearchDetailMore")));
    Result->SetObjectField(TEXT("power_root"),Geometry(TEXT("ResearchPowerRoot")));
    Result->SetObjectField(TEXT("power_scroll"),Geometry(TEXT("ResearchPowerScroll")));
    Result->SetObjectField(TEXT("power_back"),Geometry(TEXT("ResearchPowerBack")));
    Result->SetObjectField(TEXT("power_header_background"),Geometry(TEXT("ResearchPowerHeaderBackground")));
    Result->SetObjectField(TEXT("power_back_icon"),Geometry(TEXT("ResearchPowerBackIcon")));
    Result->SetObjectField(TEXT("power_level_header"),Geometry(TEXT("ResearchPowerLevelHeader")));
    Result->SetObjectField(TEXT("power_effect_header"),Geometry(TEXT("ResearchPowerEffectHeader")));
    Result->SetObjectField(TEXT("power_value_header"),Geometry(TEXT("ResearchPowerValueHeader")));
    Result->SetObjectField(TEXT("power_level_header_region"),Geometry(TEXT("ResearchPowerLevelHeaderRegion")));
    Result->SetObjectField(TEXT("power_effect_header_region"),Geometry(TEXT("ResearchPowerEffectHeaderRegion")));
    Result->SetObjectField(TEXT("power_value_header_region"),Geometry(TEXT("ResearchPowerValueHeaderRegion")));
    TArray<TSharedPtr<FJsonValue>> PowerRows;
    for (int32 Level=1;Level<=10;++Level)
    {
        auto PowerRow=MakeShared<FJsonObject>();
        PowerRow->SetNumberField(TEXT("level"),Level);
        PowerRow->SetObjectField(TEXT("extent"),Geometry(*FString::Printf(TEXT("ResearchPowerRowExtent_%d"),Level)));
        PowerRow->SetObjectField(TEXT("root"),Geometry(*FString::Printf(TEXT("ResearchPowerRow_%d"),Level)));
        PowerRow->SetObjectField(TEXT("line"),Geometry(*FString::Printf(TEXT("ResearchPowerLine_%d"),Level)));
        PowerRow->SetObjectField(TEXT("level_region"),Geometry(*FString::Printf(TEXT("ResearchPowerLevelRegion_%d"),Level)));
        PowerRow->SetObjectField(TEXT("effect_region"),Geometry(*FString::Printf(TEXT("ResearchPowerEffectRegion_%d"),Level)));
        PowerRow->SetObjectField(TEXT("power_region"),Geometry(*FString::Printf(TEXT("ResearchPowerValueRegion_%d"),Level)));
        PowerRow->SetObjectField(TEXT("level_text"),Geometry(*FString::Printf(TEXT("ResearchPowerLevel_%d"),Level)));
        PowerRow->SetObjectField(TEXT("effect_text"),Geometry(*FString::Printf(TEXT("ResearchPowerEffect_%d"),Level)));
        PowerRow->SetObjectField(TEXT("power_text"),Geometry(*FString::Printf(TEXT("ResearchPowerValue_%d"),Level)));
        PowerRows.Add(MakeShared<FJsonValueObject>(PowerRow));
    }
    Result->SetArrayField(TEXT("power_rows"),PowerRows);
    Result->SetObjectField(TEXT("detail_title"),Geometry(TEXT("ResearchDetailTitle")));
    Result->SetObjectField(TEXT("detail_title_region"),Geometry(TEXT("ResearchDetailTitleRegion")));
    Result->SetObjectField(TEXT("detail_description"),Geometry(TEXT("ResearchDetailDescription")));
    Result->SetObjectField(TEXT("detail_description_region"),Geometry(TEXT("ResearchDetailDescriptionRegion")));
    Result->SetObjectField(TEXT("detail_level"),Geometry(TEXT("ResearchDetailLevel")));
    Result->SetObjectField(TEXT("detail_level_region"),Geometry(TEXT("ResearchDetailLevelRegion")));
    Result->SetObjectField(TEXT("detail_costs"),Geometry(TEXT("ResearchDetailCosts")));
    Result->SetObjectField(TEXT("detail_icon"),Geometry(TEXT("ResearchDetailIcon")));
    Result->SetObjectField(TEXT("detail_progress"),Geometry(TEXT("ResearchDetailProgress")));
    Result->SetObjectField(TEXT("detail_attribute_name"),Geometry(TEXT("ResearchDetailAttrName")));
    Result->SetObjectField(TEXT("detail_attribute_before"),Geometry(TEXT("ResearchDetailAttrBefore")));
    Result->SetObjectField(TEXT("detail_attribute_delta"),Geometry(TEXT("ResearchDetailAttrDelta")));
    Result->SetObjectField(TEXT("detail_power_before"),Geometry(TEXT("ResearchDetailPowerBefore")));
    Result->SetObjectField(TEXT("detail_power_delta"),Geometry(TEXT("ResearchDetailPowerDelta")));
    TArray<TSharedPtr<FJsonValue>> CostValues;
    for (int32 I=0;I<6;++I)
        CostValues.Add(MakeShared<FJsonValueObject>(Geometry(*FString::Printf(TEXT("ResearchDetailCost_%d"),I))));
    Result->SetArrayField(TEXT("detail_cost_values"),CostValues);
    Result->SetObjectField(TEXT("detail_boundary"),Geometry(TEXT("ResearchDetailBoundaryButton")));
    Result->SetObjectField(TEXT("reference_notice"),Geometry(TEXT("ResearchAccountStateNotice")));
    Result->SetObjectField(TEXT("queue_unknown"),Geometry(TEXT("ResearchQueueUnknown")));
    if (const auto* Box=Cast<UScrollBox>(Research->GetWidgetFromName(TEXT("ResearchPowerScroll"))))
        Result->SetNumberField(TEXT("power_scroll_end"),Box->GetScrollOffsetOfEnd());
    for (int32 Type=1;Type<=2;++Type)
    {
        const auto* Box=Cast<UScrollBox>(Research->GetWidgetFromName(Type==1?TEXT("ResearchEconomyScroll"):TEXT("ResearchMilitaryScroll")));
        Result->SetNumberField(Type==1?TEXT("economy_scroll_end"):TEXT("military_scroll_end"),
            Box?Box->GetScrollOffsetOfEnd():0);
    }
    TArray<TSharedPtr<FJsonValue>> Columns;
    for (int32 Type=1;Type<=2;++Type)
        for (int32 Index=1;Index<=15;++Index)
        {
            auto Column=MakeShared<FJsonObject>();
            Column->SetNumberField(TEXT("type"),Type);
            Column->SetNumberField(TEXT("column"),Index);
            Column->SetObjectField(TEXT("widget"),Geometry(*FString::Printf(
                TEXT("ResearchColumn_%d_%d"),Type,Index)));
            Columns.Add(MakeShared<FJsonValueObject>(Column));
        }
    Result->SetArrayField(TEXT("columns"),Columns);
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (const auto& Row:Research->GetRenderedNodesForAudit())
    {
        auto Node=RowJson(Row);
        Node->SetObjectField(TEXT("root"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNode_%d"),Row.StudyType))));
        Node->SetObjectField(TEXT("icon"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeIcon_%d"),Row.StudyType))));
        Node->SetObjectField(TEXT("name_widget"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeName_%d"),Row.StudyType))));
        Node->SetObjectField(TEXT("level_widget"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeLevel_%d"),Row.StudyType))));
        Node->SetObjectField(TEXT("hit"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchNodeHit_%d"),Row.StudyType))));
        Nodes.Add(MakeShared<FJsonValueObject>(Node));
    }
    Result->SetArrayField(TEXT("nodes"),Nodes);
    TArray<TSharedPtr<FJsonValue>> Lines;
    for (const auto& Line:Research->GetRenderedLinesForAudit())
    {
        auto L=MakeShared<FJsonObject>();
        L->SetNumberField(TEXT("source_study_type"),Line.SourceStudyType);
        L->SetStringField(TEXT("primary_sprite"),Line.PrimarySprite);
        L->SetStringField(TEXT("secondary_sprite"),Line.SecondarySprite);
        L->SetBoolField(TEXT("primary_mirrored_y"),Line.bPrimaryMirroredY);
        L->SetNumberField(TEXT("same_column_child_count"),Line.SameColumnChildCount);
        L->SetNumberField(TEXT("cross_column_child_count"),Line.CrossColumnChildCount);
        L->SetObjectField(TEXT("primary_widget"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchLine_%d_Primary"),Line.SourceStudyType))));
        L->SetObjectField(TEXT("secondary_widget"),WidgetJson(this,Research->GetWidgetFromName(*FString::Printf(TEXT("ResearchLine_%d_Secondary"),Line.SourceStudyType))));
        Lines.Add(MakeShared<FJsonValueObject>(L));
    }
    Result->SetArrayField(TEXT("lines"),Lines);
    return Result;
}

void URokResearchAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Validate()) return;
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Conflicting screenshot request")); return; }
    PendingPath=Directory/(FString(Name)+TEXT(".png")); Pending=MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("file"),PendingPath);
    Pending->SetObjectField(TEXT("request"),State());
    ShotStarted=Elapsed; FScreenshotRequest::RequestScreenshot(PendingPath,true,false);
}
void URokResearchAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || !Validate()) return;
    const int64 Size=IFileManager::Get().FileSize(*PendingPath);
    if (Size<=0) { Finish(TEXT("Research screenshot callback missing PNG")); return; }
    Pending->SetObjectField(TEXT("processed"),State()); Pending->SetNumberField(TEXT("file_bytes"),Size);
    TArray<uint8> Png;
    if (!FFileHelper::LoadFileToArray(Png,*PendingPath)) { Finish(TEXT("Research screenshot cannot be hashed")); return; }
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Png));
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset(); Save(false); Enter(Step+1);
}

void URokResearchAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !bOwnsWorld) return;
    Elapsed+=DeltaTime;
    if (Elapsed>150) { Finish(TEXT("Research audit timeout")); return; }
    if (Pending.IsValid()) { if (Elapsed-ShotStarted>12) Finish(TEXT("Research screenshot timeout")); return; }
    if (Elapsed<3 || Elapsed-StepStarted<.35) return;
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: Capture(TEXT("01_economy_start")); break;
    case 2: if (SlateClick(TEXT("ResearchNodeHit_102"))) { ExpectedStudyType=102; bExpectDetail=true; Enter(3); } break;
    case 3: Capture(TEXT("02_economy_detail_level1")); break;
    case 4: if (SlateClick(TEXT("ResearchDetailLevelNext"))) { ExpectedLevel=2; Enter(5); } break;
    case 5: Capture(TEXT("03_economy_detail_level2")); break;
    case 6: if (SlateClick(TEXT("ResearchDetailBoundaryButton"))) { ExpectedBoundaryActionId=30; Enter(7); } break;
    case 7: Capture(TEXT("04_local_research_boundary")); break;
    case 8: if (SlateClick(TEXT("ResearchDetailBack"))) { bExpectDetail=false;
        ExpectedStudyType=0; ExpectedLevel=1; Enter(9); } break;
    case 9: Capture(TEXT("05_economy_return")); break;
    case 10: if (ScrollToEnd(1)) Enter(11); break;
    case 11: Capture(TEXT("06_economy_end")); break;
    case 12: if (SlateClick(TEXT("ResearchNodeHit_128"))) { ExpectedStudyType=128;
        ExpectedLevel=1; bExpectDetail=true; Enter(13); } break;
    case 13: Capture(TEXT("07_economy_end_detail_128")); break;
    case 14: if (SlateClick(TEXT("ResearchDetailMore"))) { bExpectPower=true; Enter(15); } break;
    case 15: Capture(TEXT("08_power_levels_128")); break;
    case 16: if (SlateClick(TEXT("ResearchPowerBack"))) { bExpectPower=false; Enter(17); } break;
    case 17: Capture(TEXT("09_economy_end_after_power_back")); break;
    case 18: if (SlateClick(TEXT("ResearchDetailBack"))) { bExpectDetail=false;
        ExpectedStudyType=0; ExpectedLevel=1; Enter(19); } break;
    case 19: Capture(TEXT("10_economy_end_return")); break;
    case 20: if (SlateClick(TEXT("ResearchMilitaryTab"))) { ExpectedType=2; Enter(21); } break;
    case 21: Capture(TEXT("11_military_start")); break;
    case 22: if (SlateClick(TEXT("ResearchNodeHit_202"))) { ExpectedStudyType=202;
        ExpectedLevel=1; bExpectDetail=true; Enter(23); } break;
    case 23: Capture(TEXT("12_military_detail_level1")); break;
    case 24: if (SlateClick(TEXT("ResearchDetailLevelNext"))) { ExpectedLevel=2; Enter(25); } break;
    case 25: Capture(TEXT("13_military_detail_level2")); break;
    case 26: if (SlateClick(TEXT("ResearchDetailBack"))) { bExpectDetail=false;
        ExpectedStudyType=0; ExpectedLevel=1; Enter(27); } break;
    case 27: Capture(TEXT("14_military_return")); break;
    case 28: if (ScrollToEnd(2)) Enter(29); break;
    case 29: Capture(TEXT("15_military_end")); break;
    case 30: if (SlateClick(TEXT("ResearchEconomyTab"))) { ExpectedType=1; Enter(31); } break;
    case 31: Capture(TEXT("16_economy_retained_end")); break;
    case 32: if (SlateClick(TEXT("ResearchCloseButton"))) { ExpectedMode=EMode::Closed; Research.Reset(); Enter(33); } break;
    case 33: Capture(TEXT("17_city_after_research")); break;
    case 34:
        if (Validate()) { Hud->OpenCommandLineModal(); ExpectedMode=EMode::Research; ExpectedType=1;
            ExpectedStudyType=0; ExpectedLevel=1; ExpectedBoundaryActionId=INDEX_NONE;
            bExpectDetail=false; bExpectPower=false; Enter(35); } break;
    case 35: Capture(TEXT("18_reopened_economy_start")); break;
    case 36: if (DelegateClick(Hud.Get(),TEXT("MailFeatureHitButton"))) { ExpectedMode=EMode::Mail; Research.Reset(); Enter(37); } break;
    case 37: Capture(TEXT("19_mail_feature_switch")); break;
    case 38: if (DelegateClick(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedMode=EMode::Closed; Enter(39); } break;
    case 39: Capture(TEXT("20_final_city")); break;
    case 40: Finish(); break;
    default: Finish(TEXT("Invalid research audit step")); break;
    }
}

bool URokResearchAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.research.runtime.v1"));
    Root->SetBoolField(TEXT("complete"),Complete);
    Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty() && Captures.Num()==CaptureCount);
    Root->SetStringField(TEXT("audit_flag"),AuditFlag);
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("entry_kind"),TEXT("HUD command_line_reference research_reference; no Academy actor"));
    Root->SetBoolField(TEXT("real_academy_entry_verified"),false);
    Root->SetBoolField(TEXT("account_research_state_synced"),false);
    Root->SetBoolField(TEXT("research_queue_synced"),false);
    Root->SetStringField(TEXT("scope"),TEXT("Actual GameApp City HUD command-line reference opens Research UMG; virtual Slate pointer routes test in-screen navigation. Unity isolated all-unresearched evidence is static presentation only; no Academy actor, account research state, queue state, research request, hardware pointer, or production campus entry is verified."));
    Root->SetBoolField(TEXT("capture_includes_slate_ui"),true);
    Root->SetNumberField(TEXT("expected_capture_count"),CaptureCount);
    Root->SetStringField(TEXT("catalog_sha256"),CatalogHash);
    Root->SetStringField(TEXT("native_baseline_evidence"),NativeBaselineEvidence);
    Root->SetStringField(TEXT("native_power_supplement_evidence"),NativePowerEvidence);
    Root->SetStringField(TEXT("native_power_fixture"),NativePowerFixture);
    Root->SetStringField(TEXT("native_power_verified"),NativePowerVerified);
    Root->SetStringField(TEXT("boundary_reviewed_ui_sha256"),ReviewedResearchUiSourceSha256);
    if (InputHashes.IsValid()) Root->SetObjectField(TEXT("input_hashes"),InputHashes.ToSharedRef());
    if (Baseline.IsValid()) Root->SetObjectField(TEXT("baseline"),Baseline.ToSharedRef());
    Root->SetArrayField(TEXT("captures"),Captures); Root->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const auto& Error:Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
    Root->SetArrayField(TEXT("failures"),Errors);
    if (FailureState.IsValid()) Root->SetObjectField(TEXT("failure_state"),FailureState.ToSharedRef());
    FString Json;
    return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))
        && FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokResearchAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num()!=CaptureCount)
        Failures.Add(TEXT("Research audit did not process all twenty screenshots"));
    if (!Failures.IsEmpty() && Screen.IsValid()) FailureState=State();
    bFinished=true;
    const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures) UE_LOG(LogTemp,Error,TEXT("Rok ResearchAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok ResearchAudit passed=%d captures=%d directory=%s"),
        Failures.IsEmpty() && Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty() && Written?0:1);
}
