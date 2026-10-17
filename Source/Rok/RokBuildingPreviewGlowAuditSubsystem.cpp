#include "RokBuildingPreviewGlowAuditSubsystem.h"
#include "RokBuildingPreviewGlowWidget.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
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
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 CaptureCount=12;
bool TargetWorld(const UWorld* World)
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    return World&&World->WorldType==EWorldType::Game&&World->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityGameAppCity")&&FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FObjectAndNameAsStringProxyArchive Archive(Writer,false);
    T::StaticStruct()->SerializeItem(Archive,&Value,nullptr);return Bytes;
}
bool Visible(const UWidget* Widget)
{
    if(!Widget||!Widget->IsVisible()||Widget->GetRenderOpacity()<=0)return false;
    for(const UWidget* Parent=Widget->GetParent();Parent;Parent=Parent->GetParent())
        if(!Parent->IsVisible()||Parent->GetRenderOpacity()<=0)return false;
    for(const UObject* Outer=Widget->GetOuter();Outer;Outer=Outer->GetOuter())
        if(const auto* User=Cast<UUserWidget>(Outer);User&&(!User->IsVisible()||User->GetRenderOpacity()<=0))return false;
    return true;
}
int32 LayerCount(uint8 Mask){return int32((Mask&1)!=0)+int32((Mask&2)!=0);}
bool ParentPixelRect(const UObject* Context, const UWidget* Widget, TSharedPtr<FJsonObject>& Out)
{
    if(!Context||!Widget)return false;
    const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context);
    const FVector2D ViewSize=Viewport.GetLocalSize(),Pixels=UWidgetLayoutLibrary::GetViewportSize(Context);
    const FGeometry Geometry=Widget->GetCachedGeometry();
    if(ViewSize.X<=0||ViewSize.Y<=0||Geometry.GetLocalSize().X<=0||Geometry.GetLocalSize().Y<=0)return false;
    const auto Pixel=[&](FVector2D Local){const FVector2D P=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Local));return FVector2D(P.X*Pixels.X/ViewSize.X,P.Y*Pixels.Y/ViewSize.Y);};
    const FVector2D A=Pixel(FVector2D::ZeroVector),B=Pixel(Geometry.GetLocalSize());
    Out=MakeShared<FJsonObject>();Out->SetNumberField(TEXT("x"),A.X);Out->SetNumberField(TEXT("y"),A.Y);
    Out->SetNumberField(TEXT("right"),B.X);Out->SetNumberField(TEXT("bottom"),B.Y);
    Out->SetNumberField(TEXT("local_width"),Geometry.GetLocalSize().X);Out->SetNumberField(TEXT("local_height"),Geometry.GetLocalSize().Y);
    return true;
}
int32 CanvasZ(const UWidget* Widget)
{
    const auto* Slot=Widget?Cast<UCanvasPanelSlot>(Widget->Slot):nullptr;
    return Slot?Slot->GetZOrder():MIN_int32;
}
}

bool URokBuildingPreviewGlowAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer))&&FParse::Param(FCommandLine::Get(),TEXT("RokBuildingPreviewGlowAudit"))&&!IsRunningDedicatedServer()&&Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokBuildingPreviewGlowAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const{return Type==EWorldType::Game;}
TStatId URokBuildingPreviewGlowAuditSubsystem::GetStatId() const{RETURN_QUICK_DECLARE_CYCLE_STAT(URokBuildingPreviewGlowAuditSubsystem,STATGROUP_Tickables);}
void URokBuildingPreviewGlowAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);bOwnsTargetWorld=TargetWorld(GetWorld());if(!bOwnsTargetWorld)return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/BuildingPreviewGlowAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokBuildingPreviewGlowAuditSubsystem::ScreenshotProcessed);
}
void URokBuildingPreviewGlowAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if(bOwnsTargetWorld&&!bFinished)Finish(TEXT("Target city world ended before glow acceptance completed"));
    Super::Deinitialize();
}
bool URokBuildingPreviewGlowAuditSubsystem::Expect(bool Condition,const TCHAR* Error){if(!Condition)Finish(Error);return Condition;}
void URokBuildingPreviewGlowAuditSubsystem::Enter(int32 Next){Step=Next;StepStarted=Elapsed;}
bool URokBuildingPreviewGlowAuditSubsystem::Identity()
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    return Expect(GetWorld()&&GetWorld()->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityGameAppCity")&&FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok")),TEXT("Wrong project/map; no preview action allowed"));
}
bool URokBuildingPreviewGlowAuditSubsystem::Begin()
{
    if(!Identity())return false;
    const TCHAR* Cursor=FCommandLine::Get();FString Token;
    while(FParse::Token(Cursor,Token,false))
        if(Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)&&!Token.Equals(TEXT("-RokBuildingPreviewGlowAudit"),ESearchCase::IgnoreCase))
        {Finish(TEXT("Only -RokBuildingPreviewGlowAudit is permitted"));return false;}
    const auto Host=[&](UClass* Class)->UUserWidget*{TArray<UUserWidget*> All;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,All,Class,false);UUserWidget* Result=nullptr;int32 Count=0;for(auto* W:All)if(W->GetWorld()==GetWorld()){Result=W;++Count;}return Count==1?Result:nullptr;};
    Hud=Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));Modal=Cast<URokUnityModalWidget>(Host(URokUnityModalWidget::StaticClass()));
    const auto* Feature=Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if(!Expect(Hud.IsValid()&&Visible(Hud.Get())&&Modal.IsValid()&&!Modal->IsModalOpen()&&Feature&&!Feature->IsScreenOpen()&&Economy&&Runtime,TEXT("Expected unique idle city UI hosts")))return false;
    FString DataError;if(!Expect(SourceData.Load(RokBuildingPreviewGlow::FData::DefaultPath(),DataError),TEXT("Validated two-layer source glow data unavailable")))return false;
    int32 Count=0;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        bool Tagged=false;for(FName Tag:It->Tags)if(Tag.ToString().StartsWith(TEXT("UnityInitId_"))){Tagged=true;break;}if(!Tagged)continue;
        const auto State=Economy->GetBuildingStateForActor(*It);if(State.UnityInitId>0)Buildings.Add(*It,Snapshot(State));
        if(It->ActorHasTag(TEXT("UnityInitId_6")))
        {
            Barracks=*It;++Count;
            if(!Expect(It->ActorHasTag(TEXT("UnityBuildingType_9"))&&State.UnityInitId==6&&State.UnityBuildingTypeId==9&&State.Level==1&&State.Power==5&&!State.bUpgradeInProgress,TEXT("Expected actual Init6/type9 level1 barracks")))return false;
        }
    }
    if(!Expect(Count==1&&Economy->GetActiveUpgradeCount()==0,TEXT("Expected unique barracks and idle build queues")))return false;
    OwnedQueues=Economy->GetOwnedBuildQueueCount();Profile=Snapshot(Runtime->GetPlayerProfileState());Resources=Snapshot(Economy->GetCurrentResources());bBaseline=true;
    Hud->SetSelectedBuilding(Barracks.Get());return true;
}
bool URokBuildingPreviewGlowAuditSubsystem::Unchanged()
{
    if(!bBaseline)return true;
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if(!Expect(Economy&&Runtime&&Snapshot(Economy->GetCurrentResources())==Resources&&Snapshot(Runtime->GetPlayerProfileState())==Profile&&Economy->GetActiveUpgradeCount()==0&&Economy->GetOwnedBuildQueueCount()==OwnedQueues,TEXT("Glow preview changed resources/profile/queues")))return false;
    for(const auto& Pair:Buildings)if(!Expect(Pair.Key.IsValid()&&Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get()))==Pair.Value,TEXT("Glow preview changed source building state")))return false;
    return true;
}
bool URokBuildingPreviewGlowAuditSubsystem::Click(UUserWidget* Owner,FName Name)
{
    if(!Identity()||!Unchanged())return false;
    if(!Expect(Name==TEXT("BuildingUpgradeButton")||Name==TEXT("Type2CloseButton"),TEXT("Only preview navigation delegates are allowed")))return false;
    auto* Button=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
    if(!Expect(Button&&Visible(Button)&&Button->GetIsEnabled(),TEXT("Missing enabled preview navigation button")))return false;
    auto Action=MakeShared<FJsonObject>();Action->SetStringField(TEXT("button"),Name.ToString());Action->SetNumberField(TEXT("frame"),GFrameCounter);Action->SetNumberField(TEXT("step"),Step);Actions.Add(MakeShared<FJsonValueObject>(Action));
    Button->OnClicked.Broadcast();return true;
}
bool URokBuildingPreviewGlowAuditSubsystem::Validate()
{
    if(!Identity()||!Unchanged()||!Modal.IsValid())return false;
    auto* Widget=Cast<URokBuildingPreviewGlowWidget>(Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget")));
    if(!bPageOpen)return Expect(!Modal->IsModalOpen()&&!Widget,TEXT("Closed preview retained source glow widget"));
#if !UE_BUILD_SHIPPING
    if(!Expect(Modal->GetSelectedBuildingActorForAudit()==Barracks.Get(),TEXT("Glow preview lost actual barracks context")))return false;
#endif
    if(!Expect(Modal->IsModalOpen()&&Widget&&Visible(Widget)&&Widget->IsInitialized()&&Widget->IsPlaybackActive()&&Widget->GetLastError().IsEmpty()&&Widget->GetPaintCount()>0,TEXT("Source glow not active and painted in real modal")))return false;
    TSharedPtr<FJsonObject> BodyRect,HeaderRect,PreviewRect,InfoButtonRect,InfoGlyphRect;
    const auto* SourcePreview=Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget"));
    const auto* InfoButton=Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButton"));
    const auto* InfoGlyph=Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButtonImage"));
    if(!Expect(ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2BodyLeft")),BodyRect)&&ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2HeaderLeft")),HeaderRect)&&
        FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("x")),194.,1.)&&FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("y")),94.2,1.)&&
        FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("right")),737.,1.)&&FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("bottom")),755.,1.)&&
        FMath::IsNearlyEqual(HeaderRect->GetNumberField(TEXT("y")),63.,1.)&&FMath::IsNearlyEqual(HeaderRect->GetNumberField(TEXT("bottom")),123.,1.),
        TEXT("Type9 parent BodyLeft/HeaderLeft actual pixel geometry differs from source contract")))return false;
    if(!Expect(Visible(SourcePreview)&&Visible(InfoButton)&&Visible(InfoGlyph)&&
        ParentPixelRect(this,SourcePreview,PreviewRect)&&ParentPixelRect(this,InfoButton,InfoButtonRect)&&ParentPixelRect(this,InfoGlyph,InfoGlyphRect)&&
        FMath::IsNearlyEqual(PreviewRect->GetNumberField(TEXT("x")),248.8,1.)&&FMath::IsNearlyEqual(PreviewRect->GetNumberField(TEXT("y")),290.5,1.)&&
        FMath::IsNearlyEqual(PreviewRect->GetNumberField(TEXT("right")),607.2,1.)&&FMath::IsNearlyEqual(PreviewRect->GetNumberField(TEXT("bottom")),633.5,1.)&&
        FMath::IsNearlyEqual(InfoButtonRect->GetNumberField(TEXT("x")),211.,1.)&&FMath::IsNearlyEqual(InfoButtonRect->GetNumberField(TEXT("y")),227.,1.)&&
        FMath::IsNearlyEqual(InfoButtonRect->GetNumberField(TEXT("right")),271.,1.)&&FMath::IsNearlyEqual(InfoButtonRect->GetNumberField(TEXT("bottom")),287.,1.)&&
        FMath::IsNearlyEqual(InfoGlyphRect->GetNumberField(TEXT("x")),222.,1.)&&FMath::IsNearlyEqual(InfoGlyphRect->GetNumberField(TEXT("y")),238.,1.)&&
        FMath::IsNearlyEqual(InfoGlyphRect->GetNumberField(TEXT("right")),260.,1.)&&FMath::IsNearlyEqual(InfoGlyphRect->GetNumberField(TEXT("bottom")),276.,1.)&&
        CanvasZ(Widget)==1&&CanvasZ(SourcePreview)==2&&CanvasZ(InfoButton)==5&&InfoGlyph->GetParent()==InfoButton,
        TEXT("Type9 preview/info glyph geometry or paint layering differs from source")))return false;
    Glow=Widget;
    if(bManualClock)
    {
        if(!Expect(!Widget->IsAutoAdvance()&&Widget->GetLastPaintLayerCount()==LayerCount(DiagnosticMask)&&Widget->GetPaintCount()>MaskStartPaintCount,TEXT("Diagnostic mask did not produce a real Slate paint")))return false;
    }
    else if(!Expect(Widget->IsAutoAdvance()&&Widget->GetLastPaintLayerCount()==2,TEXT("Natural source glow did not paint both layers")))return false;
    const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this),Geometry=Widget->GetCachedGeometry();
    const FVector2D ViewLocal=Viewport.GetLocalSize(),Pixels=UWidgetLayoutLibrary::GetViewportSize(this);
    const auto Pixel=[&](FVector2D Local){const FVector2D P=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Local));return FVector2D(P.X*Pixels.X/FMath::Max(1.,ViewLocal.X),P.Y*Pixels.Y/FMath::Max(1.,ViewLocal.Y));};
    const FVector2D Center=Pixel(SourceData.GetLayers()[0].Center);
    return Expect(FMath::IsNearlyEqual(Pixels.X,1440.,.1)&&FMath::IsNearlyEqual(Pixels.Y,810.,.1)&&Center.Equals(FVector2D(428,406),2.),TEXT("Glow window projection is not source 1440x810 center428,406"));
}
bool URokBuildingPreviewGlowAuditSubsystem::SetDiagnostic(uint8 Mask,double SourceTime)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    if(!Identity()||!Unchanged()||!Glow.IsValid()||Mask>3)return false;
    bManualClock=true;DiagnosticMask=Mask;Glow->SetAutoAdvance(false);MaskStartPaintCount=Glow->GetPaintCount();
    if(!Expect(Glow->SetPlaybackTime(SourceTime),TEXT("Cannot set diagnostic source time")))return false;
    return Expect(Glow->SetAuditLayerMask(Mask),TEXT("Cannot apply diagnostic source layer mask"));
#endif
}
TSharedRef<FJsonObject> URokBuildingPreviewGlowAuditSubsystem::State() const
{
    auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("frame"),GFrameCounter);State->SetNumberField(TEXT("step"),Step);State->SetNumberField(TEXT("elapsed"),Elapsed);
    State->SetBoolField(TEXT("open"),bPageOpen);State->SetBoolField(TEXT("state_unchanged"),true);State->SetBoolField(TEXT("manual_clock"),bManualClock);State->SetNumberField(TEXT("audit_layer_mask"),bManualClock?DiagnosticMask:3);
    const auto* Widget=Modal.IsValid()?Cast<URokBuildingPreviewGlowWidget>(Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget"))):nullptr;
    State->SetBoolField(TEXT("glow_attached"),Widget!=nullptr);
    if(Widget)
    {
        TSharedPtr<FJsonObject> BodyRect,HeaderRect,PreviewRect,InfoButtonRect,InfoGlyphRect;
        if(ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2BodyLeft")),BodyRect)&&ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2HeaderLeft")),HeaderRect))
        {State->SetObjectField(TEXT("body_left_pixel_rect"),BodyRect);State->SetObjectField(TEXT("header_left_pixel_rect"),HeaderRect);}
        const auto* SourcePreview=Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget"));
        const auto* InfoButton=Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButton"));
        const auto* InfoGlyph=Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButtonImage"));
        if(ParentPixelRect(this,SourcePreview,PreviewRect)&&ParentPixelRect(this,InfoButton,InfoButtonRect)&&ParentPixelRect(this,InfoGlyph,InfoGlyphRect))
        {
            State->SetObjectField(TEXT("source_preview_pixel_rect"),PreviewRect);
            State->SetObjectField(TEXT("info_button_pixel_rect"),InfoButtonRect);
            State->SetObjectField(TEXT("info_glyph_pixel_rect"),InfoGlyphRect);
            State->SetNumberField(TEXT("glow_canvas_z"),CanvasZ(Widget));
            State->SetNumberField(TEXT("source_preview_canvas_z"),CanvasZ(SourcePreview));
            State->SetNumberField(TEXT("info_button_canvas_z"),CanvasZ(InfoButton));
            State->SetBoolField(TEXT("info_glyph_is_button_child"),InfoGlyph->GetParent()==InfoButton);
        }
        const double Time=Widget->GetPlaybackTime();State->SetNumberField(TEXT("playback_time"),Time);State->SetNumberField(TEXT("source_phase"),Widget->GetSourcePhase());
        State->SetNumberField(TEXT("last_paint_phase"),Widget->GetLastPaintSourcePhase());State->SetNumberField(TEXT("paint_count"),static_cast<double>(Widget->GetPaintCount()));
        State->SetNumberField(TEXT("last_paint_layer_count"),Widget->GetLastPaintLayerCount());State->SetBoolField(TEXT("auto_advance"),Widget->IsAutoAdvance());State->SetBoolField(TEXT("playback_active"),Widget->IsPlaybackActive());State->SetStringField(TEXT("last_error"),Widget->GetLastError());
        TArray<RokBuildingPreviewGlow::FLayerSample> Samples;TArray<TSharedPtr<FJsonValue>> Layers;
        if(SourceData.Sample(Time,Samples)&&Samples.Num()==2)
            for(int32 I=0;I<2;++I)
            {
                auto Layer=MakeShared<FJsonObject>();Layer->SetStringField(TEXT("name"),SourceData.GetLayers()[I].Name);Layer->SetNumberField(TEXT("alpha"),Samples[I].Alpha);Layer->SetNumberField(TEXT("degrees_per_second"),SourceData.GetLayers()[I].DegreesPerSecond);
                TArray<TSharedPtr<FJsonValue>> Points;
                for(const FVector2D& P:Samples[I].ScreenVertices){auto Pair=MakeShared<FJsonObject>();Pair->SetNumberField(TEXT("x"),P.X);Pair->SetNumberField(TEXT("y"),P.Y);Points.Add(MakeShared<FJsonValueObject>(Pair));}
                Layer->SetArrayField(TEXT("source_quad"),Points);Layers.Add(MakeShared<FJsonValueObject>(Layer));
            }
        State->SetArrayField(TEXT("layers"),Layers);
        const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this),Geometry=Widget->GetCachedGeometry();
        const FVector2D LocalSize=Viewport.GetLocalSize(),Pixels=UWidgetLayoutLibrary::GetViewportSize(this);
        const auto Pixel=[&](FVector2D Local){const FVector2D P=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Local));return FVector2D(P.X*Pixels.X/FMath::Max(1.,LocalSize.X),P.Y*Pixels.Y/FMath::Max(1.,LocalSize.Y));};
        const FVector2D Origin=Pixel(FVector2D::ZeroVector),Center=Pixel(SourceData.GetLayers()[0].Center),End=Pixel(FVector2D(1440,810));
        auto Projection=MakeShared<FJsonObject>();Projection->SetNumberField(TEXT("origin_x"),Origin.X);Projection->SetNumberField(TEXT("origin_y"),Origin.Y);Projection->SetNumberField(TEXT("center_x"),Center.X);Projection->SetNumberField(TEXT("center_y"),Center.Y);Projection->SetNumberField(TEXT("right"),End.X);Projection->SetNumberField(TEXT("bottom"),End.Y);State->SetObjectField(TEXT("projection_pixels"),Projection);
    }
    return State;
}
void URokBuildingPreviewGlowAuditSubsystem::Capture(const TCHAR* Name)
{
    if(!Validate())return;if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Conflicting screenshot request"));return;}
    PendingPath=Directory/(FString(Name)+TEXT(".png"));Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("file"),PendingPath);Pending->SetObjectField(TEXT("request"),State());ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokBuildingPreviewGlowAuditSubsystem::ScreenshotProcessed()
{
    if(!bOwnsTargetWorld||bFinished||!Pending.IsValid()||!Validate())return;
    const int64 Size=IFileManager::Get().FileSize(*PendingPath);if(Size<=0){Finish(TEXT("Missing processed glow screenshot PNG"));return;}
    TArray<uint8> Bytes;const uint8 Signature[]={137,80,78,71,13,10,26,10};
    if(!FFileHelper::LoadFileToArray(Bytes,*PendingPath)||Bytes.Num()<24||FMemory::Memcmp(Bytes.GetData(),Signature,8)!=0){Finish(TEXT("Invalid processed glow PNG signature"));return;}
    const auto U32=[&](int32 I){return (uint32(Bytes[I])<<24)|(uint32(Bytes[I+1])<<16)|(uint32(Bytes[I+2])<<8)|uint32(Bytes[I+3]);};
    if(U32(16)!=1440||U32(20)!=810){Finish(TEXT("Processed game viewport must be 1440x810"));return;}
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Bytes));Pending->SetObjectField(TEXT("processed"),State());Pending->SetNumberField(TEXT("file_bytes"),Size);
    Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();
    if(Step==3)FirstNaturalTime=Glow->GetPlaybackTime();if(Step==5)LastNaturalTime=Glow->GetPlaybackTime();
    if(!Save(false)){Finish(TEXT("Incremental glow manifest write failed"));return;}Enter(Step+1);
}
void URokBuildingPreviewGlowAuditSubsystem::Tick(float DeltaTime)
{
    if(!bOwnsTargetWorld||bFinished||!GetWorld()||!GetWorld()->HasBegunPlay())return;
    Elapsed+=DeltaTime;if(Elapsed>80){Finish(TEXT("Building glow audit timeout"));return;}
    if(Pending.IsValid()){if(Elapsed-ShotStarted>10)Finish(TEXT("Glow screenshot callback timeout"));return;}
    // Type1 modal entry/exit animates the parent canvas. Wait for its settled
    // geometry before testing source projection or the closed/reopened tree.
    if(Elapsed<3||Elapsed-StepStarted<.5)return;if(!Unchanged())return;
    switch(Step)
    {
    case 0:if(Begin())Enter(1);break;
    case 1:Capture(TEXT("01_selected_city"));break;
    case 2:if(Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))){bPageOpen=true;bManualClock=false;OpenStarted=Elapsed;Enter(3);}break;
    case 3:Capture(TEXT("02_natural_early"));break;
    case 4:if(Glow.IsValid()&&Glow->GetPlaybackTime()>=1.1)Capture(TEXT("03_natural_mid"));break;
    case 5:if(Glow.IsValid()&&Glow->GetPlaybackTime()>=2.3)Capture(TEXT("04_natural_loop"));break;
    case 6:if(Expect(FirstNaturalTime>=0&&LastNaturalTime>=2.3&&LastNaturalTime-FirstNaturalTime>1.6,TEXT("Natural Slate clock did not cross a full alpha loop"))&&SetDiagnostic(0,.75))Enter(7);break;
    case 7:Capture(TEXT("05_diagnostic_none"));break;
    case 8:if(SetDiagnostic(1,.75))Enter(9);break;
    case 9:Capture(TEXT("06_diagnostic_layer0"));break;
    case 10:if(SetDiagnostic(2,.75))Enter(11);break;
    case 11:Capture(TEXT("07_diagnostic_layer1"));break;
    case 12:if(SetDiagnostic(3,.75))Enter(13);break;
    case 13:Capture(TEXT("08_diagnostic_both"));break;
    case 14:if(SetDiagnostic(3,2.75))Enter(15);break;
    case 15:Capture(TEXT("09_diagnostic_both_after_loop"));break;
    case 16:if(Click(Modal.Get(),TEXT("Type2CloseButton"))){bPageOpen=false;bManualClock=false;Glow.Reset();Enter(17);}break;
    case 17:Capture(TEXT("10_closed"));break;
    case 18:if(Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))){bPageOpen=true;OpenStarted=Elapsed;Enter(19);}break;
    case 19:if(Expect(Elapsed-OpenStarted<.8,TEXT("Reopened source glow did not reset promptly")))Capture(TEXT("11_reopened_reset"));break;
    case 20:if(Click(Modal.Get(),TEXT("Type2CloseButton"))){bPageOpen=false;Glow.Reset();Enter(21);}break;
    case 21:Capture(TEXT("12_final_city"));break;
    case 22:Finish();break;
    default:Finish(TEXT("Invalid glow audit step"));break;
    }
}
bool URokBuildingPreviewGlowAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("complete"),Complete);Root->SetBoolField(TEXT("passed"),Complete&&Failures.IsEmpty()&&Captures.Num()==CaptureCount);
    Root->SetStringField(TEXT("parent_layout_contract"),TEXT("type9_parent_v3_info_button"));
    Root->SetNumberField(TEXT("expected_capture_count"),CaptureCount);Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("scope"),TEXT("Actual Init6/type9 barracks preview/close delegates, Slate window screenshots, natural widget tick through >2 seconds and reset on reopen. Four diagnostic layer-mask frames share manual source time; no upgrade command, physical hit-test or gameplay mutation."));
    Root->SetBoolField(TEXT("capture_includes_slate_ui"),true);Root->SetBoolField(TEXT("owns_target_world"),bOwnsTargetWorld);Root->SetNumberField(TEXT("first_natural_time"),FirstNaturalTime);Root->SetNumberField(TEXT("last_natural_time"),LastNaturalTime);
    Root->SetArrayField(TEXT("captures"),Captures);Root->SetArrayField(TEXT("actions"),Actions);TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& Error:Failures)Errors.Add(MakeShared<FJsonValueString>(Error));Root->SetArrayField(TEXT("failures"),Errors);
    FString Json;return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokBuildingPreviewGlowAuditSubsystem::Finish(const FString& Error)
{
    if(!bOwnsTargetWorld||bFinished)return;if(!Error.IsEmpty())Failures.Add(Error);if(Failures.IsEmpty()&&Captures.Num()!=CaptureCount)Failures.Add(TEXT("Expected twelve source glow screenshots"));
    bFinished=true;const bool Written=Save(Failures.IsEmpty());for(const auto& Failure:Failures)UE_LOG(LogTemp,Error,TEXT("Rok BuildingPreviewGlowAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok BuildingPreviewGlowAudit passed=%d captures=%d output=%s"),Failures.IsEmpty()&&Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
