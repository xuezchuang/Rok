#include "RokVillageDayNightAuditSubsystem.h"
#include "RokVillagePresentationComponent.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapConfigActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
bool ReadJson(const FString& Name, TSharedPtr<FJsonObject>& Out)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration") / Name))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
bool ReadColor(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Count, FLinearColor& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object || !Object->TryGetArrayField(Key, Values) || !Values || Values->Num() != Count) return false;
    float V[4] = {0,0,0,1};
    for (int32 I=0; I<Count; ++I) { double Number=0; if (!(*Values)[I] || !(*Values)[I]->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false; V[I]=Number; }
    Out = FLinearColor(V[0],V[1],V[2],V[3]); return true;
}
bool SameRGB(FLinearColor A, FLinearColor B)
{
    return FMath::IsNearlyEqual(A.R,B.R,.00001f) && FMath::IsNearlyEqual(A.G,B.G,.00001f) && FMath::IsNearlyEqual(A.B,B.B,.00001f);
}
TArray<TSharedPtr<FJsonValue>> ColorValues(FLinearColor C)
{
    return {MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B),MakeShared<FJsonValueNumber>(C.A)};
}
bool ParentVisible(const USceneComponent* C, bool OwnerVisible)
{
    for (auto* P=C; P; P=P->GetAttachParent()) if (!P->IsRegistered() || !P->IsVisible() || P->bHiddenInGame || (OwnerVisible && P->GetOwner()->IsHidden())) return false;
    return C != nullptr;
}
bool ActualMaskBlueZero(UTexture2D* Texture)
{
    if (!Texture || Texture->SRGB || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty()) return false;
    auto* Data=Texture->GetPlatformData();
    if (Data->PixelFormat != PF_B8G8R8A8 && Data->PixelFormat != PF_R8G8B8A8) return false;
    auto& Mip=Data->Mips[0]; const int64 Pixels=int64(Mip.SizeX)*Mip.SizeY;
    if (Pixels<=0 || Mip.BulkData.GetBulkDataSize()!=Pixels*4) return false;
    const uint8* Bytes=static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
    bool Zero=Bytes!=nullptr;
    const int32 Blue=Data->PixelFormat==PF_B8G8R8A8 ? 0 : 2;
    if (Bytes) for (int64 I=0;I<Pixels;++I) if(Bytes[I*4+Blue]!=0) {Zero=false;break;}
    Mip.BulkData.Unlock(); return Zero;
}
}
bool URokVillageDayNightAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(),TEXT("RokVillageDayNightAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokVillageDayNightAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game; }
void URokVillageDayNightAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/VillageDayNightAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokVillageDayNightAuditSubsystem::ScreenshotProcessed);
}
void URokVillageDayNightAuditSubsystem::Deinitialize() { FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle); Super::Deinitialize(); }
TStatId URokVillageDayNightAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokVillageDayNightAuditSubsystem,STATGROUP_Tickables); }
bool URokVillageDayNightAuditSubsystem::CheckIdentity()
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
    if (!FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok")) || GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")) { Finish(TEXT("Wrong project/map; no presentation changes authorized.")); return false; }
    return true;
}
bool URokVillageDayNightAuditSubsystem::Begin()
{
    if (!CheckIdentity()) return false;
    TArray<FString> Tokens; FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
    for (const auto& Token:Tokens) if (Token.StartsWith(TEXT("-Rok")) && Token.Contains(TEXT("Audit")) && Token!=TEXT("-RokVillageDayNightAudit")) {Finish(TEXT("Run the day/night audit without another audit flag."));return false;}
    int32 Cameras=0,Entities=0;
    for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld());It;++It) {Camera=*It;++Cameras;}
    for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld());It;++It) if(It->GetSourceId()==639 && It->GetEntityKind()==TEXT("village")) {Entity=*It;++Entities;}
    if(Cameras!=1 || Entities!=1) {Finish(TEXT("Expected actual village 639 and one runtime camera."));return false;}
    Village=Entity->FindComponentByClass<URokVillagePresentationComponent>();
    auto* Player=GetWorld()->GetFirstPlayerController();
    if(!Village.IsValid() || !Player || Player->GetPawn()!=Camera.Get() || Player->GetViewTarget()!=Camera.Get() || !Entity->GetUnityPosition().Equals(FVector2D(6852,948),.001)) {Finish(TEXT("Source actor/camera identity mismatch."));return false;}
    if(Village->HasLocalDayNightSample() || !Geometry.Load() || Geometry.Items.Num()!=13) {Finish(TEXT("Expected unmodified default day and thirteen source layers."));return false;}
    if(!LoadStages()) {Finish(TEXT("Source day/night audit manifest is invalid."));return false;}
    Camera->FocusDisplayCoordinate(FVector2D(1142,158)); Next(); return true;
}
bool URokVillageDayNightAuditSubsystem::LoadStages()
{
    TSharedPtr<FJsonObject> Root;
    if(!ReadJson(TEXT("unity_village_day_night.json"),Root)) return false;
    const TArray<TSharedPtr<FJsonValue>> *Samples=nullptr,*Masks=nullptr;
    const TSharedPtr<FJsonObject> *Materials=nullptr,*Day=nullptr,*Night=nullptr;
    if(!Root->TryGetArrayField(TEXT("samples"),Samples) || !Samples || Samples->Num()!=24 || !Root->TryGetArrayField(TEXT("masks"),Masks) || !Masks || Masks->Num()!=13
        || !Root->TryGetObjectField(TEXT("materials"),Materials) || !Materials || !(*Materials)->TryGetObjectField(TEXT("day"),Day) || !Day || !(*Materials)->TryGetObjectField(TEXT("night"),Night) || !Night) return false;
    TSet<FString> Ids; for(const auto& Item:Geometry.Items) Ids.Add(Item.SourceTransformId);
    for(const auto& Value:*Masks) { if(!Value || Value->Type!=EJson::Object)return false; auto M=Value->AsObject(); FString Id; double Blue=-1;
        if(!M->TryGetStringField(TEXT("transform_id"),Id) || !Ids.Remove(Id) || !M->TryGetNumberField(TEXT("blue_max"),Blue) || Blue!=0)return false; }
    FLinearColor Day1,Day2,Night1,Night2,Night3;
    if(!ReadColor(*Day,TEXT("color1"),4,Day1) || !ReadColor(*Day,TEXT("color2"),4,Day2) || !ReadColor(*Night,TEXT("color1"),4,Night1) || !ReadColor(*Night,TEXT("color2"),4,Night2) || !ReadColor(*Night,TEXT("color3"),4,Night3))return false;
    FStage Default;Default.Name=TEXT("default_day_unmodified");Default.bApply=false;
    Default.Expected.SpriteColor=FLinearColor(1.07203974f,1.07203974f,1.07203974f,1);Default.Expected.Color1=Day1;Default.Expected.Color2=Day2;Default.Expected.NightLight=FLinearColor::Black;
    Stages.Add(Default);
    const auto Append=[&](int32 Index)->bool {
        const auto& V=(*Samples)[Index];if(!V || V->Type!=EJson::Object)return false; auto S=V->AsObject();FStage Stage;
        double From=0,To=0,Duration=0,Time=0,Delay=0; FString Scenario;
        if(!S->TryGetStringField(TEXT("name"),Stage.Name) || !S->TryGetStringField(TEXT("scenario"),Scenario) || !S->TryGetNumberField(TEXT("from_preset"),From) || !S->TryGetNumberField(TEXT("to_preset"),To) || !S->TryGetNumberField(TEXT("duration"),Duration) || !S->TryGetNumberField(TEXT("elapsed"),Time) || !S->TryGetNumberField(TEXT("fixed_delay"),Delay) || !S->TryGetBoolField(TEXT("resolved_night"),Stage.Expected.bNightMaterial) || !ReadColor(S,TEXT("sprite_color"),3,Stage.Expected.SpriteColor))return false;
        const bool On=Scenario==TEXT("night_on_phase5_initial"),Off=Scenario==TEXT("night_off_phase0_boundary");
        if((!On&&!Off) || Delay!=1 || Duration!=3 || !FMath::IsFinite(Time) || Time<0 || Time>3 || From!=(On?2:0) || To!=(On?3:0))return false;
        Stage.Sample.FromPreset=From;Stage.Sample.ToPreset=To;Stage.Sample.DurationSeconds=Duration;Stage.Sample.ElapsedSeconds=Time;Stage.Sample.MaterialDelaySeconds=Delay;
        Stage.Sample.bPreviousNight=Off;Stage.Sample.bRequestedNight=On;
        Stage.Expected.Color1=Stage.Expected.bNightMaterial?Night1:Day1;Stage.Expected.Color2=Stage.Expected.bNightMaterial?Night2:Day2;
        Stage.Expected.NightLight=Stage.Expected.bNightMaterial?Night3*.5f:FLinearColor::Black;
        if(Stage.Expected.bNightMaterial!=(Time>=Delay?On:Off))return false;
        Stages.Add(Stage);return true;
    };
    for(int32 I:{8,9,10,11})if(!Append(I))return false;
    const auto* Population=GetWorld()->GetGameInstance()->GetSubsystem<URokWorldMapEntitySubsystem>();
    const auto* Record=Population?Population->GetEntities().FindByPredicate([](const auto& R){return R.SourceId==639 && R.Kind==TEXT("village");}):nullptr;
    if(!Record || Record->TacticalDetailMaxDxf!=3000 || Record->FarIconMaxDxf!=5400)return false;
    const float Distances[]={4200,6600,480}; const TCHAR* Names[]={TEXT("night_far_icon"),TEXT("night_hidden"),TEXT("night_return480")};
    int32 Configs=0;ARokWorldMapConfigActor* Config=nullptr;for(TActorIterator<ARokWorldMapConfigActor> It(GetWorld());It;++It){Config=*It;++Configs;}
    if(Configs!=1)return false;
    for(int32 I=0;I<3;++I){if(!FMath::IsNearlyEqual(Config->ClampDxf(Distances[I]),Distances[I],.001f))return false; FStage S=Stages[4];S.Name=Names[I];S.Dxf=Distances[I];S.bApply=false;Stages.Add(S);}
    for(int32 I:{20,22,23})if(!Append(I))return false;
    FStage Stable=Default;Stable.Name=TEXT("restore_stable_day_preset1");Stable.bApply=true;Stages.Add(Stable);
    return Stages.Num()==12;
}
void URokVillageDayNightAuditSubsystem::Next()
{
    if(++StageIndex==Stages.Num()){Finish();return;}
    const auto& S=Stages[StageIndex];
    if(S.bApply && !Village->ApplyLocalDayNightSample(S.Sample)){Finish(TEXT("Runtime rejected a valid explicit source snapshot."));return;}
    Camera->SetCurrentDxf(S.Dxf);StageStarted=Elapsed;StageFrame=Frame;
}
bool URokVillageDayNightAuditSubsystem::Validate(TSharedPtr<FJsonObject>& Out)
{
    if(!CheckIdentity())return false;
    if(!Entity.IsValid() || !Camera.IsValid() || !Village.IsValid() || !Stages.IsValidIndex(StageIndex)){Finish(TEXT("Audit runtime objects lost."));return false;}
    const auto& S=Stages[StageIndex];const bool Far=S.Dxf==4200,Hidden=S.Dxf==6600,Detail=!Far&&!Hidden;
    if(!FMath::IsNearlyEqual(Camera->GetCurrentDxf(),S.Dxf,.001f) || !Camera->GetViewCenterDisplay().Equals(FVector2D(1142,158),.001)
        || Entity->IsHidden()!=Hidden || !ParentVisible(Village.Get(),false) || Village->HasLocalDayNightSample()!=(StageIndex!=0)
        || (Far && Entity->GetCurrentLodBand()!=ERokWorldMapEntityLodBand::FarIcon)
        || (Hidden && Entity->GetCurrentLodBand()!=ERokWorldMapEntityLodBand::Hidden)) {Finish(TEXT("Camera/LOD/default-day state mismatch after real tick."));return false;}
    const float ExpectedScale=Geometry.EvaluateScale(S.Dxf);
    if(!FMath::IsFinite(ExpectedScale) || !FMath::IsNearlyEqual(Village->GetSourceScale(),ExpectedScale,.0001f)) {Finish(TEXT("Source zoom update did not complete."));return false;}
    if(StageIndex!=0)
    {
        const auto& Applied=Village->GetLocalDayNightState();
        if(Applied.bNightMaterial!=S.Expected.bNightMaterial || !SameRGB(Applied.SpriteColor,S.Expected.SpriteColor)
            || !SameRGB(Applied.Color1,S.Expected.Color1) || !SameRGB(Applied.Color2,S.Expected.Color2) || !SameRGB(Applied.NightLight,S.Expected.NightLight)) {Finish(TEXT("Applied snapshot differs from source manifest oracle."));return false;}
    }
    TArray<UStaticMeshComponent*> Meshes;Entity->GetComponents(Meshes);TSet<FString> Seen;
    TArray<TSharedPtr<FJsonValue>> Rows;bool FoundLegacy=false,FoundFar=false;
    for(auto* Mesh:Meshes)
    {
        if(Mesh->GetName()==TEXT("DetailVisual")) {FoundLegacy=true;if(Mesh->IsVisible() || !Mesh->bHiddenInGame){Finish(TEXT("Legacy village card overlaps source layers."));return false;}continue;}
        if(Mesh->GetName()==TEXT("FarVisual")) {FoundFar=true;if(Mesh->IsVisible()!=Far || Mesh->bHiddenInGame==Far || !Mesh->IsRegistered()
            || Mesh->GetAttachParent()!=Entity->GetRootComponent() || (Far && (!ParentVisible(Mesh,true) || !Mesh->GetMaterial(0) || !Mesh->GetStaticMesh() || FMath::Abs(Mesh->GetRelativeScale3D().X*Mesh->GetRelativeScale3D().Y)<=0))) {Finish(TEXT("Far icon state invalid."));return false;}continue;}
        if(!Mesh->GetName().StartsWith(TEXT("Village_")))continue;
        const FString Id=Mesh->GetName().RightChop(8);
        const auto* Item=Geometry.Items.FindByPredicate([&Id](const auto& I){return I.SourceTransformId==Id;});
        if(!Item || Seen.Contains(Id) || !Mesh->ComponentHasTag(FName(*Id)) || !Mesh->ComponentHasTag(FName(*Item->SourceName))
            || !Mesh->ComponentHasTag(FName(*(TEXT("UnitySpriteRenderer_")+Item->SourceRendererId))) || !Mesh->IsRegistered() || Mesh->GetAttachParent()!=Village.Get()
            || !Mesh->GetStaticMesh() || Mesh->GetStaticMesh()->GetPathName()!=TEXT("/Engine/BasicShapes/Plane.Plane")
            || Mesh->IsVisible()!=Detail || Mesh->bHiddenInGame==Detail || (Detail && !ParentVisible(Mesh,true))) {Finish(TEXT("Source ID/card attachment/visibility mismatch."));return false;}
        Seen.Add(Id);
        const FVector Scale=Mesh->GetRelativeScale3D();const float X=Item->SpriteSize.X*ExpectedScale*Geometry.UnitsToCentimeters/100;
        const float Y=-Item->SpriteSize.Y*ExpectedScale*Geometry.UnitsToCentimeters/100;
        if(!FMath::IsNearlyEqual(Scale.X,X,FMath::Max(.0001f,FMath::Abs(X)*.0001f)) || !FMath::IsNearlyEqual(Scale.Y,Y,FMath::Max(.0001f,FMath::Abs(Y)*.0001f)) || (Detail && Scale.X*Scale.Y>=0)) {Finish(TEXT("Village source scale or UV basis changed."));return false;}
        auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
        if(!MID || !MID->Parent || MID->Parent->GetPathName()!=TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokVillagePresentationDay.M_RokVillagePresentationDay")) {Finish(TEXT("Dedicated source material missing."));return false;}
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("transform_id"),Id);Row->SetStringField(TEXT("renderer_id"),Item->SourceRendererId);
        const TCHAR* Names[]={TEXT("Color1"),TEXT("Color2"),TEXT("SpriteColor"),TEXT("NightLight")};
        const FLinearColor Expected[]={S.Expected.Color1,S.Expected.Color2,S.Expected.SpriteColor,S.Expected.NightLight};
        for(int32 I=0;I<4;++I) {FLinearColor Value;if(!MID->GetVectorParameterValue(FMaterialParameterInfo(Names[I]),Value) || !SameRGB(Value,Expected[I])) {Finish(FString::Printf(TEXT("Actual MID differs: %s / %s"),*Id,Names[I]));return false;}Row->SetArrayField(Names[I],ColorValues(Value));}
        auto* Sprite=Cast<UTexture2D>(MID->K2_GetTextureParameterValue(TEXT("Sprite")));
        auto* Mask=Cast<UTexture2D>(MID->K2_GetTextureParameterValue(TEXT("Mask")));
        if(!Sprite || Sprite->SRGB || !ActualMaskBlueZero(Mask) || !MID->K2_GetVectorParameterValue(TEXT("RendererColor")).Equals(FLinearColor::White,.00001f)
            || !FMath::IsNearlyEqual(MID->K2_GetScalarParameterValue(TEXT("CityTransparency")),1.f,.00001f)) {Finish(TEXT("Actual raw mask blue channel or source opacity contract failed."));return false;}
        Row->SetBoolField(TEXT("actual_mask_blue_zero"),true);Row->SetBoolField(TEXT("visible"),Detail);Row->SetNumberField(TEXT("plane_area"),FMath::Abs(Scale.X*Scale.Y));Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    if(!FoundLegacy || !FoundFar || Seen.Num()!=13) {Finish(TEXT("Expected thirteen unique source cards and legacy/far components."));return false;}
    if(Detail) {const auto Active=Village->GetActiveSourceItems();if(Active.Num()!=13){Finish(TEXT("Return480 did not recover thirteen source items."));return false;}for(const auto& Id:Active)if(!Seen.Contains(Id)){Finish(TEXT("Unknown active source ID."));return false;}}
    Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("sample"),S.Name);Out->SetNumberField(TEXT("stage"),StageIndex);Out->SetNumberField(TEXT("frame"),double(Frame));
    Out->SetNumberField(TEXT("actual_dxf"),Camera->GetCurrentDxf());Out->SetNumberField(TEXT("source_scale"),Village->GetSourceScale());Out->SetNumberField(TEXT("elapsed_runtime"),Elapsed);
    Out->SetBoolField(TEXT("night_material"),S.Expected.bNightMaterial);Out->SetBoolField(TEXT("has_local_snapshot"),Village->HasLocalDayNightSample());Out->SetBoolField(TEXT("actor_hidden"),Entity->IsHidden());
    Out->SetNumberField(TEXT("phase_elapsed"),S.Sample.ElapsedSeconds);Out->SetNumberField(TEXT("fixed_delay"),S.Sample.MaterialDelaySeconds);Out->SetNumberField(TEXT("shader_seconds"),S.Sample.ShaderTimeSeconds);
    Out->SetBoolField(TEXT("legacy_hidden"),true);Out->SetBoolField(TEXT("far_visible"),Far);Out->SetArrayField(TEXT("cards"),Rows);return true;
}
void URokVillageDayNightAuditSubsystem::Tick(float DeltaTime)
{
    if(bFinished || !GetWorld()->HasBegunPlay())return;
    Elapsed+=FMath::Max(DeltaTime,0.f);++Frame;
    if(Elapsed>90){Finish(TEXT("Day/night runtime audit timed out."));return;}
    if(StageIndex<0){if(Elapsed>3)Begin();return;}
    if(Pending){if(Elapsed-StageStarted>15)Finish(TEXT("Screenshot processing timed out."));return;}
    if(Elapsed-StageStarted<.5 || Frame<StageFrame+2)return;
    if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Unexpected competing screenshot request."));return;}
    TSharedPtr<FJsonObject> State;if(!Validate(State))return;
    PendingPath=Directory/FString::Printf(TEXT("%02d_%s.png"),StageIndex,*Stages[StageIndex].Name);
    Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("path"),PendingPath);Pending->SetObjectField(TEXT("state_at_request"),State);
    StageStarted=Elapsed;FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokVillageDayNightAuditSubsystem::ScreenshotProcessed()
{
    if(bFinished || !Pending)return;
    TSharedPtr<FJsonObject> State;if(!Validate(State))return;
    TArray<uint8> Bytes;const uint8 Signature[]={137,80,78,71,13,10,26,10};
    if(!FFileHelper::LoadFileToArray(Bytes,*PendingPath) || Bytes.Num()<24 || FMemory::Memcmp(Bytes.GetData(),Signature,8)!=0){Finish(TEXT("Screenshot callback lacks PNG evidence."));return;}
    const auto Read=[&](int32 I){return(uint32(Bytes[I])<<24)|(uint32(Bytes[I+1])<<16)|(uint32(Bytes[I+2])<<8)|uint32(Bytes[I+3]);};
    if(Read(16)<2 || Read(20)<2){Finish(TEXT("Invalid PNG dimensions."));return;}
    Pending->SetNumberField(TEXT("width"),Read(16));Pending->SetNumberField(TEXT("height"),Read(20));Pending->SetNumberField(TEXT("bytes"),Bytes.Num());
    Pending->SetObjectField(TEXT("state_at_processed"),State);Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();
    if(!Save(false)){Finish(TEXT("Audit manifest write failed."));return;}Next();
}
bool URokVillageDayNightAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("complete"),Complete);Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty() && Captures.Num()==12);
    Root->SetNumberField(TEXT("source_id"),639);Root->SetNumberField(TEXT("expected_captures"),12);Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));Root->SetArrayField(TEXT("captures"),Captures);
    TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));Root->SetArrayField(TEXT("failures"),Errors);
    Root->SetStringField(TEXT("scope"),TEXT("Seven representative source post-delay snapshots, untouched default day, explicit stable-day restoration and three LOD regressions. Actual runtime MID/raw-mask/IDs checked at screenshot request and processed. Dawn phase0 differs from stable day preset1. No server clock/random coroutine/full map night simulation. Gamma/native visual comparison remains a separate human gate."));
    FString Text;return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text)) && FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokVillageDayNightAuditSubsystem::Finish(const FString& Failure)
{
    if(bFinished)return;if(!Failure.IsEmpty())Failures.Add(Failure);else if(Captures.Num()!=12)Failures.Add(TEXT("Expected twelve processed captures."));bFinished=true;
    if(!Save(true)){Failures.Add(TEXT("Final manifest save failed."));UE_LOG(LogTemp,Error,TEXT("Day/night final manifest write failed."));}
    UE_LOG(LogTemp,Display,TEXT("Rok VillageDayNight audit passed=%d output=%s failures=%s"),Failures.IsEmpty(),*Directory,*FString::Join(Failures,TEXT("; ")));
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()?0:1);
}
