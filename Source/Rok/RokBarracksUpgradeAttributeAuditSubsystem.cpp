#include "RokBarracksUpgradeAttributeAuditSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokBuildingSourcePreviewWidget.h"
#include "RokBuildingPreviewGlowWidget.h"
#include "RokBarracksSourceTextWidget.h"
#include "Engine/Texture2D.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "RokTrainArmyMeshData.h"

namespace
{
constexpr int32 CaptureCount = 7;
bool TargetWorld(const UWorld* World)
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    return World&&World->WorldType==EWorldType::Game&&World->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityGameAppCity")&&FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer, false);
    T::StaticStruct()->SerializeItem(Archive, &Value, nullptr); return Bytes;
}
bool Visible(const UWidget* W)
{
    if (!W || !W->IsVisible() || W->GetRenderOpacity() <= 0) return false;
    for (const UWidget* P = W->GetParent(); P; P = P->GetParent())
        if (!P->IsVisible() || P->GetRenderOpacity() <= 0) return false;
    for (const UObject* O = W->GetOuter(); O; O = O->GetOuter())
        if (const auto* U = Cast<UUserWidget>(O); U && (!U->IsVisible() || U->GetRenderOpacity() <= 0)) return false;
    return true;
}
bool ParentPixelRect(const UObject* Context,const UWidget* Widget,TSharedPtr<FJsonObject>& Out)
{
    if(!Context||!Widget)return false;
    const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context),Geometry=Widget->GetCachedGeometry();
    const FVector2D ViewSize=Viewport.GetLocalSize(),Pixels=UWidgetLayoutLibrary::GetViewportSize(Context);
    if(ViewSize.X<=0||ViewSize.Y<=0||Geometry.GetLocalSize().X<=0||Geometry.GetLocalSize().Y<=0)return false;
    const auto Pixel=[&](FVector2D Local){const FVector2D P=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(Local));return FVector2D(P.X*Pixels.X/ViewSize.X,P.Y*Pixels.Y/ViewSize.Y);};
    const FVector2D A=Pixel(FVector2D::ZeroVector),B=Pixel(Geometry.GetLocalSize());
    Out=MakeShared<FJsonObject>();Out->SetNumberField(TEXT("x"),A.X);Out->SetNumberField(TEXT("y"),A.Y);
    Out->SetNumberField(TEXT("right"),B.X);Out->SetNumberField(TEXT("bottom"),B.Y);
    Out->SetNumberField(TEXT("local_width"),Geometry.GetLocalSize().X);Out->SetNumberField(TEXT("local_height"),Geometry.GetLocalSize().Y);
    return true;
}
}

bool URokBarracksUpgradeAttributeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(),TEXT("RokBarracksUpgradeAttributeAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokBarracksUpgradeAttributeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game; }
TStatId URokBarracksUpgradeAttributeAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokBarracksUpgradeAttributeAuditSubsystem,STATGROUP_Tickables); }
void URokBarracksUpgradeAttributeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsTargetWorld=TargetWorld(GetWorld());if(!bOwnsTargetWorld)return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/BarracksUpgradeAttributeAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokBarracksUpgradeAttributeAuditSubsystem::ScreenshotProcessed);
}
void URokBarracksUpgradeAttributeAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if(bOwnsTargetWorld&&!bFinished)Finish(TEXT("Target city world ended before completion"));
    Super::Deinitialize();
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Expect(bool Condition,const TCHAR* Error){if(!Condition)Finish(Error);return Condition;}
void URokBarracksUpgradeAttributeAuditSubsystem::Enter(int32 Next){Step=Next;StepStarted=Elapsed;}
bool URokBarracksUpgradeAttributeAuditSubsystem::Identity()
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    return Expect(FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"))&&GetWorld()&&GetWorld()->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityGameAppCity"),TEXT("Wrong project/map; no preview action allowed"));
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Begin()
{
    if(!Identity())return false;
    const TCHAR* Cursor=FCommandLine::Get();FString Token;
    while(FParse::Token(Cursor,Token,false))if(Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)&&!Token.Equals(TEXT("-RokBarracksUpgradeAttributeAudit"),ESearchCase::IgnoreCase)){Finish(TEXT("Only this audit flag is permitted"));return false;}
    const auto Host=[&](UClass* Class)->UUserWidget*{TArray<UUserWidget*> All;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,All,Class,false);UUserWidget* Result=nullptr;int32 Count=0;for(auto* W:All)if(W->GetWorld()==GetWorld()){Result=W;++Count;}return Count==1?Result:nullptr;};
    Hud=Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));Modal=Cast<URokUnityModalWidget>(Host(URokUnityModalWidget::StaticClass()));
    const auto* Feature=Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* E=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();auto* R=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if(!Expect(Hud.IsValid()&&Visible(Hud.Get())&&Modal.IsValid()&&!Modal->IsModalOpen()&&Feature&&!Feature->IsScreenOpen()&&E&&R,TEXT("Expected unique idle city UI hosts")))return false;
    int32 Count=0;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        bool Tagged=false;for(FName T:It->Tags)if(T.ToString().StartsWith(TEXT("UnityInitId_"))){Tagged=true;break;}if(!Tagged)continue;
        const auto S=E->GetBuildingStateForActor(*It);if(S.UnityInitId>0)Buildings.Add(*It,Snapshot(S));
        if(It->ActorHasTag(TEXT("UnityInitId_6")))
        {
            Barracks=*It;++Count;
            if(!Expect(It->ActorHasTag(TEXT("UnityBuildingType_9"))&&S.UnityInitId==6&&S.UnityBuildingTypeId==9&&S.Level==1&&S.Power==5&&!S.bUpgradeInProgress,TEXT("Expected actual source Init6/type9 level1 power5 barracks")))return false;
        }
    }
    if(!Expect(Count==1&&E->GetActiveUpgradeCount()==0,TEXT("Expected unique barracks and idle queues")))return false;
    OwnedQueues=E->GetOwnedBuildQueueCount();Profile=Snapshot(R->GetPlayerProfileState());Resources=Snapshot(E->GetCurrentResources());bBaseline=true;
    Hud->SetSelectedBuilding(Barracks.Get());return true;
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Unchanged()
{
    if(!bBaseline)return true;
    auto* E=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();auto* R=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if(!Expect(E&&R&&Snapshot(E->GetCurrentResources())==Resources&&Snapshot(R->GetPlayerProfileState())==Profile&&E->GetActiveUpgradeCount()==0&&E->GetOwnedBuildQueueCount()==OwnedQueues,TEXT("Preview changed resources/profile/power/queue")))return false;
    for(const auto& P:Buildings)if(!Expect(P.Key.IsValid()&&Snapshot(E->GetBuildingStateForActor(P.Key.Get()))==P.Value,TEXT("Preview changed source building state/level")))return false;
    return true;
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Click(UUserWidget* Owner,FName Name)
{
    if(!Identity()||!Unchanged())return false;
    if(!Expect(Name==TEXT("BuildingUpgradeButton")||Name==TEXT("Type2CloseButton"),TEXT("Only navigation delegates allowed; upgrade commands forbidden")))return false;
    auto* B=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
    if(!Expect(B&&Visible(B)&&B->GetIsEnabled(),TEXT("Missing enabled navigation button")))return false;
    auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("button"),Name.ToString());A->SetNumberField(TEXT("frame"),GFrameCounter);A->SetNumberField(TEXT("step"),Step);Actions.Add(MakeShared<FJsonValueObject>(A));B->OnClicked.Broadcast();
    if(Name==TEXT("BuildingUpgradeButton"))
    {
        // This audit isolates building Image visibility. Source glow animation
        // has its own natural-tick audit; fix its diagnostic phase for pixel diffs.
        auto* Glow=Cast<URokBuildingPreviewGlowWidget>(Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget")));
        if(!Expect(Glow&&Glow->IsInitialized(),TEXT("Source glow missing in preview regression")))return false;
        Glow->SetAutoAdvance(false);
        if(!Expect(Glow->SetPlaybackTime(0),TEXT("Failed to set diagnostic glow phase")))return false;
    }
    return true;
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Validate()
{
    if(!Identity()||!Unchanged())return false;
    const auto* Group=Cast<UUserWidget>(Modal->GetWidgetFromName(TEXT("BarracksUpgradeAttributes")));
    if(!bPageOpen)return Expect(!Modal->IsModalOpen()&&!Group&&!Modal->GetWidgetFromName(TEXT("UpgradeCurrentLevel"))&&!Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget")),TEXT("Closed preview retained old attribute tree"));
    if(!Expect(Modal->IsModalOpen()&&Group&&Visible(Group),TEXT("Barracks attribute page unavailable")))return false;
    TSharedPtr<FJsonObject> BodyRect,HeaderRect;
    if(!Expect(ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2BodyLeft")),BodyRect)&&ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2HeaderLeft")),HeaderRect)&&
        FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("x")),194.,1.)&&FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("y")),94.2,1.)&&
        FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("right")),737.,1.)&&FMath::IsNearlyEqual(BodyRect->GetNumberField(TEXT("bottom")),755.,1.)&&
        FMath::IsNearlyEqual(HeaderRect->GetNumberField(TEXT("y")),63.,1.)&&FMath::IsNearlyEqual(HeaderRect->GetNumberField(TEXT("bottom")),123.,1.),
        TEXT("Type9 parent BodyLeft/HeaderLeft actual pixel geometry differs from source contract")))return false;
    const auto* Glow=Cast<URokBuildingPreviewGlowWidget>(Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget")));
    if(!Expect(Glow&&Visible(Glow)&&Glow->IsInitialized()&&Glow->IsPlaybackActive()&&!Glow->IsAutoAdvance()&&Glow->GetPlaybackTime()==0&&Glow->GetLastPaintLayerCount()==2&&Glow->GetPaintCount()>0,TEXT("Preview regression requires painted source glow at diagnostic time zero")))return false;
#if !UE_BUILD_SHIPPING
    if(!Expect(Modal->GetSelectedBuildingActorForAudit()==Barracks.Get(),TEXT("Preview lost source actor context")))return false;
#endif
    const auto* Current=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(TEXT("UpgradeCurrentLevel")));const auto* Next=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(TEXT("UpgradeNextLevel")));
    if(!Expect(Current&&Next&&Current->GetText().ToString()==TEXT("等级1")&&Next->GetText().ToString()==TEXT("2"),TEXT("Wrong preview levels")))return false;
    const auto GlyphValid=[](const URokBarracksSourceTextWidget* W,const TCHAR* Key){return W&&Visible(W)&&W->HasSourceMesh()&&W->GetSourceKey()==Key&&W->GetPaintCount()>0&&W->GetLastPaintSourceKey()==W->GetSourceKey();};
    if(!Expect(GlyphValid(Current,TEXT("level_before"))&&GlyphValid(Next,TEXT("level_after")),TEXT("Source level glyph key has not actually painted")))return false;
    const auto* Arrow=Cast<UImage>(Modal->GetWidgetFromName(TEXT("UpgradeLevelArrow")));
    if(!Expect(Arrow&&Visible(Arrow)&&Arrow->GetRenderTransform().Scale.Equals(FVector2D(-1,1),.001),TEXT("Source Chinese level arrow must be mirrored")))return false;
    const auto* BeforeRegion=Modal->GetWidgetFromName(TEXT("UpgradeCurrentLevelRegion"));const auto* AfterRegion=Modal->GetWidgetFromName(TEXT("UpgradeNextLevelRegion"));
    const auto CheckRect=[](const UWidget* W,FVector2D P,FVector2D Size){const auto* Slot=W?Cast<UCanvasPanelSlot>(W->Slot):nullptr;return Slot&&Slot->GetPosition().Equals(P,.02)&&Slot->GetSize().Equals(Size,.02);};
    if(!Expect(CheckRect(BeforeRegion,FVector2D(702,181.82007),FVector2D(61,50))&&CheckRect(AfterRegion,FVector2D(826.7,181.82007),FVector2D(13,50))&&CheckRect(Arrow,FVector2D(779.3,193.82),FVector2D(24,26))&&Arrow->GetRenderTransformPivot().Equals(FVector2D(.5),.001),TEXT("Source level regions or mirrored arrow geometry differs")))return false;
    const auto* Preview=Cast<URokBuildingSourcePreviewWidget>(Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget")));
    const auto* Image=Preview?Cast<UImage>(Preview->GetWidgetFromName(TEXT("BuildingSourcePreviewImage"))):nullptr;
    if(Step>=5&&Step<=7&&!Expect(Image==PreviewImage.Get(),TEXT("Preview diagnostic replaced image instance")))return false;
    const auto* Texture=Image?Cast<UTexture2D>(Image->GetBrush().GetResourceObject()):nullptr;
    FString Model,Error;
    if(!Expect(RokBuildingSourcePreview::ResolveActorModel(Barracks.Get(),Model,Error)&&Model==TEXT("build_9_101_1")&&Preview&&Preview->IsPreviewReady()&&Preview->GetLoadError().IsEmpty()&&Preview->GetPresentationModel()==Model&&Visible(Preview)&&Image&&Visible(Image)==!bPreviewHidden,TEXT("Source model binding or preview visibility differs")))return false;
    if(!Expect(CheckRect(Preview,FVector2D(248.8000335693,290.5),FVector2D(358.3999786377,343))&&Texture&&Texture->GetPathName()==TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_9_101_1.UnityGameApp_build_9_101_1")&&Texture->GetSizeX()==512&&Texture->GetSizeY()==490&&Texture->SRGB&&Texture->AddressX==TA_Clamp&&Texture->AddressY==TA_Clamp&&Texture->GetNumMips()==1&&Image->GetColorAndOpacity()==FLinearColor::White,TEXT("Preview raw texture identity, flags or source geometry differs")))return false;
    const auto* Root=Group->GetWidgetFromName(TEXT("BarracksUpgradeAttributeRoot"));
    if(!Expect(Root&&FMath::IsNearlyEqual(Root->GetCachedGeometry().GetLocalSize().Y,110.,.1),TEXT("Source attribute container height must be110")))return false;
    const TCHAR* Labels[]={TEXT("步兵单位训练容量"),TEXT("战力")};const TCHAR* Values[]={TEXT("20 + <color=green>30</color>"),TEXT("5 + <color=green>5</color>")};
    for(int32 I=0;I<2;++I)
    {
        const auto* Label=Cast<URokBarracksSourceTextWidget>(Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatLabel_%d"),I)));
        const auto* Value=Cast<URokBarracksSourceTextWidget>(Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatCurrent_%d"),I)));
        const auto* Separator=Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatSeparator_%d"),I));
        if(!Expect(Label&&Value&&Separator&&Visible(Label)&&Visible(Value)&&Label->GetText().ToString()==Labels[I]&&Value->GetText().ToString()==Values[I]&&Visible(Separator)==(I==0),TEXT("Attribute text/last separator differs from source")))return false;
        if(!Expect(GlyphValid(Label,*FString::Printf(TEXT("label_%d"),I))&&GlyphValid(Value,*FString::Printf(TEXT("value_%d"),I)),TEXT("Source attribute glyph key has not actually painted")))return false;
    }
    if(!Expect(!Group->GetWidgetFromName(TEXT("UpgradeStatLabel_2"))&&!Modal->GetWidgetFromName(TEXT("UpgradeStatDelta_0")),TEXT("Zero attack or old farm attribute row leaked")))return false;
    const auto* A=Group->GetWidgetFromName(TEXT("UpgradeStatLabel_0"));const auto* B=Group->GetWidgetFromName(TEXT("UpgradeStatLabel_1"));
    const FVector2D LocalA=Root->GetCachedGeometry().AbsoluteToLocal(A->GetCachedGeometry().GetAbsolutePosition());const FVector2D LocalB=Root->GetCachedGeometry().AbsoluteToLocal(B->GetCachedGeometry().GetAbsolutePosition());
    return Expect(FMath::IsNearlyEqual(LocalB.Y-LocalA.Y,40.,.1),TEXT("Source row stride must be40"));
}
TSharedRef<FJsonObject> URokBarracksUpgradeAttributeAuditSubsystem::State() const
{
    auto S=MakeShared<FJsonObject>();S->SetNumberField(TEXT("frame"),GFrameCounter);S->SetNumberField(TEXT("step"),Step);S->SetBoolField(TEXT("open"),bPageOpen);S->SetBoolField(TEXT("state_unchanged"),true);
    const auto* Group=Cast<UUserWidget>(Modal->GetWidgetFromName(TEXT("BarracksUpgradeAttributes")));S->SetBoolField(TEXT("attributes_attached"),Group!=nullptr);
    const auto* Preview=Cast<URokBuildingSourcePreviewWidget>(Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget")));
    S->SetBoolField(TEXT("preview_attached"),Preview!=nullptr);S->SetBoolField(TEXT("preview_hidden"),bPreviewHidden);
    if(const auto* Glow=Cast<URokBuildingPreviewGlowWidget>(Modal->GetWidgetFromName(TEXT("BuildingPreviewGlowWidget"))))
    {
        S->SetBoolField(TEXT("glow_diagnostic_fixed_time"),!Glow->IsAutoAdvance());
        S->SetNumberField(TEXT("glow_time"),Glow->GetPlaybackTime());
        S->SetNumberField(TEXT("glow_painted_layers"),Glow->GetLastPaintLayerCount());
    }
    if(Preview)
    {
        TSharedPtr<FJsonObject> BodyRect,HeaderRect;
        if(ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2BodyLeft")),BodyRect)&&ParentPixelRect(this,Modal->GetWidgetFromName(TEXT("Type2HeaderLeft")),HeaderRect))
        {S->SetObjectField(TEXT("body_left_pixel_rect"),BodyRect);S->SetObjectField(TEXT("header_left_pixel_rect"),HeaderRect);}
        const auto* Image=Cast<UImage>(Preview->GetWidgetFromName(TEXT("BuildingSourcePreviewImage")));const auto* Texture=Cast<UTexture2D>(Image->GetBrush().GetResourceObject());
        auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("model"),Preview->GetPresentationModel());P->SetStringField(TEXT("texture"),Texture->GetPathName());P->SetBoolField(TEXT("visible"),Visible(Image));P->SetNumberField(TEXT("width"),Texture->GetSizeX());P->SetNumberField(TEXT("height"),Texture->GetSizeY());P->SetNumberField(TEXT("filter"),static_cast<int32>(Texture->Filter));P->SetNumberField(TEXT("compression"),static_cast<int32>(Texture->CompressionSettings));P->SetBoolField(TEXT("srgb"),Texture->SRGB);P->SetNumberField(TEXT("mips"),Texture->GetNumMips());P->SetNumberField(TEXT("address_x"),static_cast<int32>(Texture->AddressX));P->SetNumberField(TEXT("address_y"),static_cast<int32>(Texture->AddressY));P->SetBoolField(TEXT("render_resource"),Texture->GetResource()!=nullptr);P->SetNumberField(TEXT("brush_draw_as"),static_cast<int32>(Image->GetBrush().DrawAs));
        const FGeometry VG=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);const FVector2D VS=VG.GetLocalSize(),PS=UWidgetLayoutLibrary::GetViewportSize(this);const auto Pixel=[&](FVector2D L){const FVector2D Q=VG.AbsoluteToLocal(Preview->GetCachedGeometry().LocalToAbsolute(L));return FVector2D(Q.X*PS.X/VS.X,Q.Y*PS.Y/VS.Y);};const FVector2D A=Pixel(FVector2D::ZeroVector),B=Pixel(Preview->GetCachedGeometry().GetLocalSize());P->SetNumberField(TEXT("x"),A.X);P->SetNumberField(TEXT("y"),A.Y);P->SetNumberField(TEXT("right"),B.X);P->SetNumberField(TEXT("bottom"),B.Y);S->SetObjectField(TEXT("preview"),P);
    }
    if(Group)
    {
        const auto* Root=Group->GetWidgetFromName(TEXT("BarracksUpgradeAttributeRoot"));S->SetNumberField(TEXT("container_height"),Root->GetCachedGeometry().GetLocalSize().Y);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for(int32 I=0;I<2;++I)
        {
            const auto* L=Cast<URokBarracksSourceTextWidget>(Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatLabel_%d"),I)));const auto* V=Cast<URokBarracksSourceTextWidget>(Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatCurrent_%d"),I)));
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("label"),L->GetText().ToString());Row->SetStringField(TEXT("value"),V->GetText().ToString());Row->SetBoolField(TEXT("separator"),Visible(Group->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatSeparator_%d"),I))));
            const FVector2D P=Root->GetCachedGeometry().AbsoluteToLocal(L->GetCachedGeometry().GetAbsolutePosition());Row->SetNumberField(TEXT("local_y"),P.Y);
            const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);const FVector2D ViewSize=Viewport.GetLocalSize(),PixelSize=UWidgetLayoutLibrary::GetViewportSize(this);
            const auto Pixel=[&](const FGeometry& G,FVector2D Local){const FVector2D Q=Viewport.AbsoluteToLocal(G.LocalToAbsolute(Local));return FVector2D(Q.X*PixelSize.X/FMath::Max(1.,ViewSize.X),Q.Y*PixelSize.Y/FMath::Max(1.,ViewSize.Y));};
            const FVector2D LP=Pixel(L->GetCachedGeometry(),FVector2D::ZeroVector),VP=Pixel(V->GetCachedGeometry(),FVector2D::ZeroVector),LE=Pixel(L->GetCachedGeometry(),L->GetCachedGeometry().GetLocalSize()),VE=Pixel(V->GetCachedGeometry(),V->GetCachedGeometry().GetLocalSize());
            Row->SetNumberField(TEXT("pixel_x"),FMath::Min(LP.X,VP.X));Row->SetNumberField(TEXT("pixel_y"),FMath::Min(LP.Y,VP.Y));Row->SetNumberField(TEXT("pixel_right"),FMath::Max(LE.X,VE.X));Row->SetNumberField(TEXT("pixel_bottom"),FMath::Max(LE.Y,VE.Y));Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        TArray<TSharedPtr<FJsonValue>> Glyphs;
        for(const auto* Name:{TEXT("UpgradeCurrentLevel"),TEXT("UpgradeNextLevel"),TEXT("UpgradeStatLabel_0"),TEXT("UpgradeStatCurrent_0"),TEXT("UpgradeStatLabel_1"),TEXT("UpgradeStatCurrent_1")})
        {const auto* W=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(Name));if(!W)W=Cast<URokBarracksSourceTextWidget>(Group->GetWidgetFromName(Name));auto G=MakeShared<FJsonObject>();G->SetStringField(TEXT("name"),Name);G->SetStringField(TEXT("text"),W->GetText().ToString());G->SetStringField(TEXT("source_key"),W->GetSourceKey());G->SetStringField(TEXT("paint_key"),W->GetLastPaintSourceKey());G->SetNumberField(TEXT("paint_count"),W->GetPaintCount());Glyphs.Add(MakeShared<FJsonValueObject>(G));}S->SetArrayField(TEXT("glyphs"),Glyphs);
        S->SetArrayField(TEXT("rows"),Rows);
        const auto* Before=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(TEXT("UpgradeCurrentLevel")));const auto* After=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(TEXT("UpgradeNextLevel")));
        S->SetStringField(TEXT("level_before"),Before->GetText().ToString());S->SetStringField(TEXT("level_after"),After->GetText().ToString());
        const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);const FVector2D VS=Viewport.GetLocalSize(),PS=UWidgetLayoutLibrary::GetViewportSize(this);
        const auto Pixel=[&](const FGeometry& G,FVector2D Local){const FVector2D Q=Viewport.AbsoluteToLocal(G.LocalToAbsolute(Local));return FVector2D(Q.X*PS.X/FMath::Max(1.,VS.X),Q.Y*PS.Y/FMath::Max(1.,VS.Y));};
        S->SetNumberField(TEXT("attribute_pixel_top"),Pixel(Root->GetCachedGeometry(),FVector2D::ZeroVector).Y);
        TArray<TSharedPtr<FJsonValue>> LevelRegions;
        for(const auto* Name:{TEXT("UpgradeCurrentLevelRegion"),TEXT("UpgradeNextLevelRegion")})
        {const auto* W=Modal->GetWidgetFromName(Name);const FVector2D P=Pixel(W->GetCachedGeometry(),FVector2D::ZeroVector),E=Pixel(W->GetCachedGeometry(),W->GetCachedGeometry().GetLocalSize());auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("x"),P.X);R->SetNumberField(TEXT("y"),P.Y);R->SetNumberField(TEXT("right"),E.X);R->SetNumberField(TEXT("bottom"),E.Y);LevelRegions.Add(MakeShared<FJsonValueObject>(R));}
        S->SetArrayField(TEXT("level_pixel_regions"),LevelRegions);
        S->SetBoolField(TEXT("level_arrow_mirrored"),Modal->GetWidgetFromName(TEXT("UpgradeLevelArrow"))->GetRenderTransform().Scale.X<0);

    }
    return S;
}
void URokBarracksUpgradeAttributeAuditSubsystem::Capture(const TCHAR* Name)
{
    if(!Validate())return;if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Conflicting screenshot request"));return;}
    PendingPath=Directory/(FString(Name)+TEXT(".png"));Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("file"),PendingPath);Pending->SetObjectField(TEXT("request"),State());ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokBarracksUpgradeAttributeAuditSubsystem::ScreenshotProcessed()
{
    if(!bOwnsTargetWorld||bFinished||!Pending.IsValid()||!Validate())return;const int64 Size=IFileManager::Get().FileSize(*PendingPath);if(Size<=0){Finish(TEXT("Missing processed screenshot PNG"));return;}
    TArray<uint8> Bytes;const uint8 Signature[]={137,80,78,71,13,10,26,10};
    if(!FFileHelper::LoadFileToArray(Bytes,*PendingPath)||Bytes.Num()<24||FMemory::Memcmp(Bytes.GetData(),Signature,8)!=0){Finish(TEXT("Invalid processed PNG signature"));return;}
    const auto U32=[&](int32 I){return (uint32(Bytes[I])<<24)|(uint32(Bytes[I+1])<<16)|(uint32(Bytes[I+2])<<8)|uint32(Bytes[I+3]);};
    if(U32(16)!=1440||U32(20)!=810){Finish(TEXT("Processed game viewport must be1440x810"));return;}
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetObjectField(TEXT("processed"),State());Pending->SetNumberField(TEXT("file_bytes"),Size);Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();if(!Save(false)){Finish(TEXT("Incremental manifest write failed"));return;}Enter(Step+1);
}
void URokBarracksUpgradeAttributeAuditSubsystem::Tick(float DeltaTime)
{
    if(!bOwnsTargetWorld||bFinished||!GetWorld()||!GetWorld()->HasBegunPlay())return;Elapsed+=DeltaTime;if(Elapsed>60){Finish(TEXT("Barracks audit timeout"));return;}if(Pending.IsValid()){if(Elapsed-ShotStarted>10)Finish(TEXT("Screenshot callback timeout"));return;}if(Elapsed<3||Elapsed-StepStarted<.5)return;if(!Unchanged())return;
    switch(Step)
    {
    case 0:if(Begin())Enter(1);break;
    case 1:Capture(TEXT("01_selected_city"));break;
    case 2:if(Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))){bPageOpen=true;Enter(3);}break;
    case 3:Capture(TEXT("02_upgrade_attributes"));break;
    case 4:{auto* P=Cast<UUserWidget>(Modal->GetWidgetFromName(TEXT("BuildingSourcePreviewWidget")));PreviewImage=P?P->GetWidgetFromName(TEXT("BuildingSourcePreviewImage")):nullptr;if(Expect(PreviewImage.IsValid(),TEXT("Preview missing before hide"))){PreviewImage->SetVisibility(ESlateVisibility::Hidden);bPreviewHidden=true;Enter(5);}}break;
    case 5:Capture(TEXT("06_preview_hidden"));break;
    case 6:if(Expect(PreviewImage.IsValid(),TEXT("Hidden preview instance lost"))){PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);bPreviewHidden=false;Enter(7);}break;
    case 7:Capture(TEXT("07_preview_restored"));break;
    case 8:if(Click(Modal.Get(),TEXT("Type2CloseButton"))){bPageOpen=false;Enter(9);}break;
    case 9:Capture(TEXT("03_city_returned"));break;
    case 10:if(Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))){bPageOpen=true;Enter(11);}break;
    case 11:Capture(TEXT("04_reopened_attributes"));break;
    case 12:if(Click(Modal.Get(),TEXT("Type2CloseButton"))){bPageOpen=false;Enter(13);}break;
    case 13:Capture(TEXT("05_city_final"));break;
    case 14:Finish();break;
    default:Finish(TEXT("Invalid audit step"));break;
    }
}
bool URokBarracksUpgradeAttributeAuditSubsystem::Save(bool Complete)
{
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("complete"),Complete);R->SetBoolField(TEXT("passed"),Complete&&Failures.IsEmpty()&&Captures.Num()==CaptureCount);R->SetNumberField(TEXT("expected_capture_count"),CaptureCount);R->SetNumberField(TEXT("glow_diagnostic_contract"),1);R->SetStringField(TEXT("parent_layout_contract"),TEXT("type9_parent_v2"));
    R->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));R->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());R->SetStringField(TEXT("scope"),TEXT("Existing Init6/type9 actor selected through HUD API; actual upgrade-preview/close delegates and real ticks. No physical pointer test; no upgrade command. Full resource/profile/source building state and idle queue guards."));R->SetBoolField(TEXT("capture_includes_slate_ui"),true);R->SetBoolField(TEXT("owns_target_world"),bOwnsTargetWorld);
    R->SetArrayField(TEXT("captures"),Captures);R->SetArrayField(TEXT("actions"),Actions);TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));R->SetArrayField(TEXT("failures"),Errors);
    FString Json;return FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokBarracksUpgradeAttributeAuditSubsystem::Finish(const FString& Error)
{
    if(!bOwnsTargetWorld||bFinished)return;if(!Error.IsEmpty())Failures.Add(Error);if(Failures.IsEmpty()&&Captures.Num()!=CaptureCount)Failures.Add(TEXT("Expected seven screenshots"));bFinished=true;const bool Written=Save(Failures.IsEmpty());
    for(const auto& E:Failures)UE_LOG(LogTemp,Error,TEXT("Rok BarracksUpgradeAttributeAudit: %s"),*E);UE_LOG(LogTemp,Display,TEXT("Rok BarracksUpgradeAttributeAudit passed=%d captures=%d output=%s"),Failures.IsEmpty()&&Written,Captures.Num(),*Directory);FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
