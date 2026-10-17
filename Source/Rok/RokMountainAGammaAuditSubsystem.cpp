#include "RokMountainAGammaAuditSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "StaticMeshResources.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
namespace
{
const TCHAR* BasePath=TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_A.M_WorldSource_build_mountain_A");
TArray<TSharedPtr<FJsonValue>> XYZ(FVector V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
}
bool URokMountainAGammaAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 return FParse::Param(FCommandLine::Get(),TEXT("RokMountainAGammaAudit"))&&!IsRunningDedicatedServer()&&Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMountainAGammaAuditSubsystem::DoesSupportWorldType(EWorldType::Type T)const{return T==EWorldType::Game;}
void URokMountainAGammaAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/MountainAGammaAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
 IFileManager::Get().MakeDirectory(*Directory,true);
 ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokMountainAGammaAuditSubsystem::Processed);
}
void URokMountainAGammaAuditSubsystem::Deinitialize(){RestoreVisibility();FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);Super::Deinitialize();}
bool URokMountainAGammaAuditSubsystem::RestoreVisibility()
{
 bool Restored=true;
 for(const auto& Saved:SavedVisibility)
 {
  auto* Mesh=Saved.Key.Get();if(!Mesh){Restored=false;continue;}
  Mesh->SetVisibility(Saved.Value.X!=0,false);Mesh->SetHiddenInGame(Saved.Value.Y!=0,false);
  Restored&=Mesh->IsVisible()==(Saved.Value.X!=0)&&Mesh->bHiddenInGame==(Saved.Value.Y!=0);
 }
 SavedVisibility.Reset();return Restored;
}
TStatId URokMountainAGammaAuditSubsystem::GetStatId()const{RETURN_QUICK_DECLARE_CYCLE_STAT(URokMountainAGammaAuditSubsystem,STATGROUP_Tickables);}
bool URokMountainAGammaAuditSubsystem::Begin()
{
 if(GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")||!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()),TEXT("D:/ueDemo/Rok/")))
 {Finish(TEXT("Wrong project/world"));return false;}
 TArray<FString> Tokens;FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
 for(const auto& T:Tokens)if(T.StartsWith(TEXT("-RokWorldDayNightLocal"))||(T.StartsWith(TEXT("-Rok"))&&T.Contains(TEXT("Audit"))&&T!=TEXT("-RokMountainAGammaAudit")))
 {Finish(TEXT("Conflicting audit or continuous lighting writer"));return false;}
 FString Text;TSharedPtr<FJsonObject> Input;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/MountainAGamma/manifest.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Input)||!Input)
 {Finish(TEXT("Missing source sample manifest"));return false;}
 const TArray<TSharedPtr<FJsonValue>>* Samples=nullptr;
 if(!Input->TryGetArrayField(TEXT("samples"),Samples)||Samples->Num()!=3){Finish(TEXT("Expected three source samples"));return false;}
 const TCHAR* ExpectedNames[]={TEXT("stable_day"),TEXT("dusk_to_night_mid"),TEXT("stable_night")};
 for(int32 I=0;I<3;++I)
 {
  const TSharedPtr<FJsonObject>* Row=nullptr;const TArray<TSharedPtr<FJsonValue>>* RGB=nullptr;FString Name;
  if(!(*Samples)[I].IsValid()||!(*Samples)[I]->TryGetObject(Row)||!Row||!Row->IsValid()||!(*Row)->TryGetStringField(TEXT("name"),Name)||Name!=ExpectedNames[I]||!(*Row)->TryGetArrayField(TEXT("spriteColor"),RGB)||RGB->Num()!=3)
  {Finish(TEXT("Malformed source sample"));return false;}
  double R,G,B;if(!(*RGB)[0]->TryGetNumber(R)||!(*RGB)[1]->TryGetNumber(G)||!(*RGB)[2]->TryGetNumber(B)||!FMath::IsFinite(R)||!FMath::IsFinite(G)||!FMath::IsFinite(B)){Finish(TEXT("Non-finite source color"));return false;}
  Names.Add(Name);Colors.Add(FLinearColor(R,G,B,1));
 }
 int32 Cameras=0,Configs=0;ARokWorldMapConfigActor* Config=nullptr;
 for(TActorIterator<ARokWorldMapCameraPawn> It(GetWorld());It;++It){Camera=*It;++Cameras;}
 for(TActorIterator<ARokWorldMapConfigActor> It(GetWorld());It;++It){Config=*It;++Configs;}
 auto* PC=GetWorld()->GetFirstPlayerController();
 if(Cameras!=1||Configs!=1||!PC||PC->GetPawn()!=Camera.Get()||PC->GetViewTarget()!=Camera.Get()){Finish(TEXT("Actual world camera/config mismatch"));return false;}
 double Nearest=TNumericLimits<double>::Max();
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(!It->ActorHasTag(TEXT("UnityPrefab_build_mountain_A"))||!It->ActorHasTag(TEXT("RokWorldMapExactSource")))continue;
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  for(auto* Mesh:Meshes)if(Mesh->GetMaterial(0)&&Mesh->GetMaterial(0)->GetPathName()==BasePath&&Mesh->GetStaticMesh())
  {
   const double Distance=FVector::DistSquared(Mesh->Bounds.Origin,Config->Projection.UnityToUe(Camera->GetViewCenterUnity()));
   if(Distance<Nearest){Nearest=Distance;Mountain=Mesh;}
  }
 }
 if(!Mountain.IsValid()){Finish(TEXT("No existing exact source Mountain A actor"));return false;}
 Raw=LoadObject<UTexture2D>(nullptr,TEXT("/Game/RokUnityWorldMapPrototype/Textures/T_WorldSource_build_mountain_A_RawGamma.T_WorldSource_build_mountain_A_RawGamma"));
 if(!Raw.IsValid()||Raw->SRGB){Finish(TEXT("Raw mountain texture missing or sRGB enabled"));return false;}
 Material=Mountain->CreateDynamicMaterialInstance(0);if(!Material.IsValid()){Finish(TEXT("Runtime MID creation failed"));return false;}
 TArray<UTexture*> UsedTextures;Material->GetUsedTextures(UsedTextures);
 if(!UsedTextures.Contains(Raw.Get())){Finish(TEXT("Actual mountain material does not sample the raw texture"));return false;}
 const FVector Forward=Camera->GetCameraComponent()->GetForwardVector();
 if(FMath::Abs(Forward.Z)<.1){Finish(TEXT("Unexpected source camera direction"));return false;}
 const FVector Center=Mountain->Bounds.Origin;
 const FVector GroundFocus=Center-Forward*((Center.Z-Config->Projection.GroundZ)/Forward.Z);
 Camera->SetViewCenterUnity(Config->Projection.UeToUnity(GroundFocus));
 Camera->SetCurrentDxf(1509);
 bIsolate=FParse::Param(FCommandLine::Get(),TEXT("RokMountainAGammaIsolate"));
 if(bIsolate)
 {
  // Only this disposable runtime fixture changes visibility, never source data.
  // Preserve both independent flags and restore even when capture fails.
  for(TActorIterator<AActor> It(GetWorld());It;++It)
  {
   // Persistent generated world geometry; runtime entity components have their
   // own visibility state machines and are outside this isolation fixture.
   if(!It->IsA<AStaticMeshActor>())continue;
   TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
   for(auto* Mesh:Meshes)if(Mesh!=Mountain.Get())
   {
    SavedVisibility.Emplace(Mesh,FIntPoint(Mesh->IsVisible()?1:0,Mesh->bHiddenInGame?1:0));
    auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("component"),Mesh->GetPathName());Record->SetBoolField(TEXT("was_visible"),Mesh->IsVisible());Record->SetBoolField(TEXT("was_hidden_in_game"),Mesh->bHiddenInGame);VisibilityRecords.Add(MakeShared<FJsonValueObject>(Record));
    Mesh->SetVisibility(false,false);Mesh->SetHiddenInGame(true,false);
   }
  }
 }
 Stage=0;StageStart=Elapsed;StageFrame=GFrameCounter;Material->SetVectorParameterValue(TEXT("UnitySpriteColor"),Colors[Stage]);return true;
}
bool URokMountainAGammaAuditSubsystem::Inspect(TSharedPtr<FJsonObject>& Out,bool& bFramed)
{
 if(!Mountain.IsValid()||!Camera.IsValid()||!Material.IsValid()||!Raw.IsValid()||Mountain->GetMaterial(0)!=Material.Get()||Mountain->GetOwner()->IsHidden()||!Mountain->IsVisible()||Mountain->bHiddenInGame)
 {Finish(TEXT("Lost or hidden actual mountain component"));return false;}
 FLinearColor Actual;if(!Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")),Actual)||!Actual.Equals(Colors[Stage],.00001f))
 {Finish(TEXT("Actual mountain MID differs from source snapshot"));return false;}
 if(!Material->Parent||Material->Parent->GetPathName()!=BasePath||Raw->SRGB||Raw->GetNumResidentMips()<1){Finish(TEXT("Material parent/raw texture residency regression"));return false;}
 if(bIsolate)for(const auto& Saved:SavedVisibility)
 {
  auto* Mesh=Saved.Key.Get();if(!Mesh||Mesh->IsVisible()||!Mesh->bHiddenInGame){Finish(TEXT("Isolation visibility changed during capture"));return false;}
 }
 auto* PC=GetWorld()->GetFirstPlayerController();if(!PC||PC->GetViewTarget()!=Camera.Get()){Finish(TEXT("Camera route changed"));return false;}
 int32 W=0,H=0;PC->GetViewportSize(W,H);if(W<100||H<100){Finish(TEXT("Missing runtime viewport"));return false;}
 const FBox Box=Mountain->Bounds.GetBox();FVector2D Lo(DBL_MAX,DBL_MAX),Hi(-DBL_MAX,-DBL_MAX);bFramed=true;
 for(int32 I=0;I<8;++I)
 {
  const FVector P((I&1)?Box.Max.X:Box.Min.X,(I&2)?Box.Max.Y:Box.Min.Y,(I&4)?Box.Max.Z:Box.Min.Z);FVector2D Screen;
  if(!PC->ProjectWorldLocationToScreen(P,Screen)){bFramed=false;continue;}
  Lo.X=FMath::Min(Lo.X,Screen.X);Lo.Y=FMath::Min(Lo.Y,Screen.Y);Hi.X=FMath::Max(Hi.X,Screen.X);Hi.Y=FMath::Max(Hi.Y,Screen.Y);
 }
 bFramed=bFramed&&Lo.X>W*.05&&Hi.X<W*.95&&Lo.Y>H*.08&&Hi.Y<H*(bIsolate?.85:.92)&&(Hi.X-Lo.X)>80&&(Hi.Y-Lo.Y)>60;
 const auto* RenderData=Mountain->GetStaticMesh()->GetRenderData();
 if(!RenderData||RenderData->LODResources.IsEmpty()||RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords()<1){Finish(TEXT("Missing actual mesh/UV channel"));return false;}
 Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("actor"),Mountain->GetOwner()->GetName());Out->SetStringField(TEXT("component"),Mountain->GetPathName());Out->SetStringField(TEXT("mesh"),Mountain->GetStaticMesh()->GetPathName());
 Out->SetStringField(TEXT("material_instance"),Material->GetPathName());Out->SetStringField(TEXT("parent"),Material->Parent->GetPathName());Out->SetArrayField(TEXT("actual_sprite_color"),XYZ(FVector(Actual.R,Actual.G,Actual.B)));
 Out->SetArrayField(TEXT("bounds_min"),XYZ(Box.Min));Out->SetArrayField(TEXT("bounds_max"),XYZ(Box.Max));Out->SetArrayField(TEXT("camera_position"),XYZ(Camera->GetCameraComponent()->GetComponentLocation()));
 Out->SetNumberField(TEXT("dxf"),Camera->GetCurrentDxf());Out->SetNumberField(TEXT("frame"),double(GFrameCounter));Out->SetNumberField(TEXT("elapsed"),Elapsed);
 Out->SetArrayField(TEXT("screen_rect"),{MakeShared<FJsonValueNumber>(Lo.X),MakeShared<FJsonValueNumber>(Lo.Y),MakeShared<FJsonValueNumber>(Hi.X),MakeShared<FJsonValueNumber>(Hi.Y)});
 Out->SetNumberField(TEXT("uv_channels"),RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());Out->SetNumberField(TEXT("vertices"),RenderData->LODResources[0].GetNumVertices());
 Out->SetStringField(TEXT("raw_texture"),Raw->GetPathName());Out->SetBoolField(TEXT("srgb"),Raw->SRGB);Out->SetNumberField(TEXT("texture_mips"),Raw->GetNumMips());Out->SetNumberField(TEXT("resident_mips"),Raw->GetNumResidentMips());Out->SetNumberField(TEXT("texture_width"),Raw->GetSizeX());Out->SetNumberField(TEXT("texture_height"),Raw->GetSizeY());
 return true;
}
void URokMountainAGammaAuditSubsystem::Tick(float Dt)
{
 if(bFinished||!GetWorld()->HasBegunPlay())return;Elapsed+=FMath::Max(0.f,Dt);
 if(Elapsed>30){Finish(TEXT("Mountain runtime audit timeout"));return;}
 if(Stage<0){if(Elapsed>.75)Begin();return;}
 if(Pending||Elapsed-StageStart<.5||GFrameCounter<StageFrame+3)return;
 TSharedPtr<FJsonObject> State;bool bFramed=false;if(!Inspect(State,bFramed))return;
 if(!bFramed)
 {
  if(Stage!=0||Camera->GetCurrentDxf()>8000){Finish(TEXT("Actual source mountain does not fit the source camera viewport"));return;}
  Camera->SetCurrentDxf(Camera->GetCurrentDxf()*1.15f);StageStart=Elapsed;StageFrame=GFrameCounter;return;
 }
 if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Competing screenshot writer"));return;}
 PendingPath=Directory/FString::Printf(TEXT("%02d_%s.png"),Stage,*Names[Stage]);Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("name"),Names[Stage]);Pending->SetStringField(TEXT("path"),PendingPath);Pending->SetObjectField(TEXT("at_request"),State);
 FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokMountainAGammaAuditSubsystem::Processed()
{
 if(bFinished||!Pending)return;TSharedPtr<FJsonObject> State;bool bFramed=false;if(!Inspect(State,bFramed))return;
 if(!bFramed){Finish(TEXT("Screenshot callback lost visible framing"));return;}
 TArray<uint8> Bytes;const uint8 Signature[]={137,80,78,71,13,10,26,10};
 if(!FFileHelper::LoadFileToArray(Bytes,*PendingPath)||Bytes.Num()<24||FMemory::Memcmp(Bytes.GetData(),Signature,8)!=0){Finish(TEXT("Missing processed PNG"));return;}
 Pending->SetObjectField(TEXT("at_processed"),State);Pending->SetNumberField(TEXT("bytes"),Bytes.Num());Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();
 if(++Stage==3){Finish();return;}StageStart=Elapsed;StageFrame=GFrameCounter;Material->SetVectorParameterValue(TEXT("UnitySpriteColor"),Colors[Stage]);
}
void URokMountainAGammaAuditSubsystem::Finish(const FString& Failure)
{
 if(bFinished)return;bFinished=true;if(!Failure.IsEmpty())Failures.Add(Failure);
 const bool bRestored=RestoreVisibility();if(!bRestored)Failures.Add(TEXT("Isolation visibility restoration failed"));
 auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Failures.IsEmpty()&&Captures.Num()==3);Report->SetArrayField(TEXT("captures"),Captures);
 Report->SetBoolField(TEXT("isolated_existing_asset"),bIsolate);Report->SetBoolField(TEXT("visibility_restored"),bRestored);Report->SetArrayField(TEXT("temporarily_hidden_components"),VisibilityRecords);
 TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));Report->SetArrayField(TEXT("failures"),Errors);
 Report->SetStringField(TEXT("scope"),TEXT("Existing main-map Mountain A actor, runtime-only camera/MID fixture with three stable source colors. No map save, geometry alteration, source UV rewrite, full-map equivalence or continuous-clock acceptance. Actual mesh UV-channel/mip metadata is recorded; UV pixel correspondence and occlusion require image review."));
 if(bIsolate)Report->SetStringField(TEXT("isolation_scope"),TEXT("Other existing static mesh components were temporarily hidden, original visibility flags restored at finish/failure/deinitialize. This verifies an isolated actual asset, NOT unobstructed normal-world presentation. Original non-isolated captures remain separate."));
 FString Text;if(!FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text))||!FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))Failures.Add(TEXT("Manifest save failed"));
 UE_LOG(LogTemp,Display,TEXT("Mountain A Gamma audit passed=%d captures=%d output=%s failures=%s"),Failures.IsEmpty()&&Captures.Num()==3,Captures.Num(),*Directory,*FString::Join(Failures,TEXT("; ")));
 FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Captures.Num()==3?0:1);
}
