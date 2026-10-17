#include "RokWorldDayNightAuditSubsystem.h"
#include "RokWorldDayNightSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "RokVillagePresentationComponent.h"
#include "RokTrainArmyMeshData.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
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
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
namespace
{
bool IsWorldDayNightAuditTarget(const UWorld* World)
{
 FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
 return World&&World->WorldType==EWorldType::Game&&World->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")
  &&FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"));
}
const TCHAR* Names[]={TEXT("dusk_continuous"),TEXT("night_transition_start"),TEXT("night_transition_middle"),TEXT("night_material_stagger"),TEXT("night_all_layers"),TEXT("night_crop1500"),TEXT("night_return480"),TEXT("night_far4200"),TEXT("night_return_from_far"),TEXT("dawn_continuous_transition"),TEXT("dawn_materials_restored")};
TArray<TSharedPtr<FJsonValue>> RGB(FLinearColor C){return {MakeShared<FJsonValueNumber>(C.R),MakeShared<FJsonValueNumber>(C.G),MakeShared<FJsonValueNumber>(C.B)};}
bool SameRGB(FLinearColor A,FLinearColor B){return FMath::IsNearlyEqual(A.R,B.R,.00001f)&&FMath::IsNearlyEqual(A.G,B.G,.00001f)&&FMath::IsNearlyEqual(A.B,B.B,.00001f);}
FString GroveSourcePath(){return FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/GroveCenterScale/manifest.json");}
FString FileHash(const FString& Path){TArray<uint8> Bytes;return FFileHelper::LoadFileToArray(Bytes,*Path)?RokTrainArmyMesh::Sha256(Bytes):FString();}
TSharedPtr<FJsonObject> JsonObject(const TSharedPtr<FJsonValue>& V){return V&&V->Type==EJson::Object?V->AsObject():nullptr;}
TSharedPtr<FJsonObject> JsonChild(const TSharedPtr<FJsonObject>& O,const TCHAR* K){const TSharedPtr<FJsonObject>* V=nullptr;return O&&O->TryGetObjectField(K,V)?*V:nullptr;}
bool JsonNumber(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double& N){const auto V=O?O->TryGetField(K):nullptr;return V&&V->Type==EJson::Number&&V->TryGetNumber(N)&&FMath::IsFinite(N);}
bool JsonVector(const TSharedPtr<FJsonObject>& O,FVector& V){return JsonNumber(O,TEXT("x"),V.X)&&JsonNumber(O,TEXT("y"),V.Y)&&JsonNumber(O,TEXT("z"),V.Z);}
TArray<TSharedPtr<FJsonValue>> XYZ(FVector V){return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};}
bool MatchesStage(int32 Stage,int32 Phase,int32 Nights,int32 Visible,float Dxf,ERokWorldMapEntityLodBand Band)
{
 const float ExpectedDxf=Stage==5?1500.f:Stage==7?4200.f:480.f;
 if(!FMath::IsNearlyEqual(Dxf,ExpectedDxf,.001f))return false;
 switch(Stage)
 {
 case 0:return Phase==4&&Nights==0&&Visible==13;
 case 1:case 2:return Phase==5&&Visible==13;
 case 3:return Phase==6&&Nights>0&&Nights<98&&Visible==13;
 case 4:case 6:case 8:return Phase==6&&Nights==98&&Visible==13;
 case 5:return Phase==6&&Nights==98&&Visible==1;
 case 7:return Phase==6&&Nights==98&&Visible==0&&Band==ERokWorldMapEntityLodBand::FarIcon;
 case 9:return Phase==7&&Nights==98&&Visible==13;
 case 10:return Phase==0&&Nights==0&&Visible==13;
 default:return false;
 }
}
}
bool URokWorldDayNightAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 return IsWorldDayNightAuditTarget(Cast<UWorld>(Outer))&&FParse::Param(FCommandLine::Get(),TEXT("RokWorldDayNightAudit"))&&!IsRunningDedicatedServer()&&Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokWorldDayNightAuditSubsystem::DoesSupportWorldType(EWorldType::Type T)const{return T==EWorldType::Game;}
void URokWorldDayNightAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);bOwnsTargetWorld=IsWorldDayNightAuditTarget(GetWorld());
 if(!bOwnsTargetWorld)return;
 Collection.InitializeDependency<URokWorldDayNightSubsystem>();
 Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/WorldDayNightAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
 IFileManager::Get().MakeDirectory(*Directory,true);ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokWorldDayNightAuditSubsystem::Processed);
}
void URokWorldDayNightAuditSubsystem::Deinitialize()
{
 FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
 // Initialization owns the target even if it ends before its first tick. Temporary worlds never own it.
 if(bOwnsTargetWorld&&!bFinished)Finish(TEXT("Target world ended before continuous day/night audit completed."));
 Super::Deinitialize();
}
TStatId URokWorldDayNightAuditSubsystem::GetStatId()const{RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorldDayNightAuditSubsystem,STATGROUP_Tickables);}
bool URokWorldDayNightAuditSubsystem::Begin()
{
 int64 Seconds=0;
 if(!FParse::Value(FCommandLine::Get(),TEXT("RokWorldDayNightLocalSeconds="),Seconds)||Seconds!=1792
  ||GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")
  ||!FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()),TEXT("D:/ueDemo/Rok/"))){Finish(TEXT("Requires exact world map and explicit local epoch 1792."));return false;}
 TArray<FString> Tokens;FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);for(const auto& T:Tokens)if(T.StartsWith(TEXT("-Rok"))&&T.Contains(TEXT("Audit"))&&T!=TEXT("-RokWorldDayNightAudit")){Finish(TEXT("Conflicting audit flags."));return false;}
 Lighting=GetWorld()->GetSubsystem<URokWorldDayNightSubsystem>();if(!Lighting.IsValid()||!Lighting->IsReady()||Lighting->GetLocalState().Layers.Num()!=98){Finish(TEXT("Expected ready continuous subsystem with 98 actual resource cards."));return false;}
 int32 Cameras=0,Villages=0;for(TActorIterator<ARokWorldMapCameraPawn> It(GetWorld());It;++It){Camera=*It;++Cameras;}
 for(TActorIterator<ARokWorldMapEntityActor> It(GetWorld());It;++It)if(It->GetSourceId()==639&&It->GetEntityKind()==TEXT("village")){Village=*It;++Villages;}
 auto* PC=GetWorld()->GetFirstPlayerController();
 if(Cameras!=1||Villages!=1||!PC||PC->GetPawn()!=Camera.Get()||PC->GetViewTarget()!=Camera.Get()||!Village->GetUnityPosition().Equals(FVector2D(6852,948),.001)){Finish(TEXT("Source village/camera identity mismatch."));return false;}
 if(!BindGroves())return false;
 Camera->FocusDisplayCoordinate(FVector2D(1142,158));Camera->SetCurrentDxf(480);Next();return true;
}
bool URokWorldDayNightAuditSubsystem::BindGroves()
{
 FString Text;TSharedPtr<FJsonObject> Source;FString Schema,Prefab;
 const TArray<TSharedPtr<FJsonValue>> *Files=nullptr,*Placements=nullptr;
 if(!FFileHelper::LoadFileToString(Text,*GroveSourcePath())||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Source)||!Source
  ||!Source->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.grove_center_scale.v1")
  ||!Source->TryGetStringField(TEXT("prefab"),Prefab)||Prefab!=TEXT("Grove_co_tree_01_adorning")
  ||!Source->TryGetArrayField(TEXT("source_files"),Files)||Files->Num()!=6
  ||!Source->TryGetArrayField(TEXT("instances"),Placements)||Placements->Num()!=11)
 {Finish(TEXT("Missing exact Grove source catalog for continuous cross-check"));return false;}
 TSet<FString> Paths;
 for(const auto& V:*Files)
 {
  const auto O=JsonObject(V);FString Path,Hash;
  if(!O||!O->TryGetStringField(TEXT("path"),Path)||!Path.StartsWith(TEXT("ref/"))||Path.Contains(TEXT(".."))||Paths.Contains(Path)
   ||!O->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64||FileHash(FPaths::ProjectDir()/Path)!=Hash)
  {Finish(TEXT("Grove source provenance changed"));return false;}
  Paths.Add(Path);
 }
 GroveSourceHash=FileHash(GroveSourcePath());
 for(const auto& V:*Placements)
 {
  const auto O=JsonObject(V),Instance=JsonChild(O,TEXT("source_instance"));const TArray<TSharedPtr<FJsonValue>>* Tile=nullptr;
  FVector Position,Scale;double X=0,Y=0,Index=0,Yaw=0;FString Id;
  if(!O||!O->TryGetArrayField(TEXT("tile"),Tile)||Tile->Num()!=2||!(*Tile)[0]||!(*Tile)[1]||(*Tile)[0]->Type!=EJson::Number||(*Tile)[1]->Type!=EJson::Number
   ||!(*Tile)[0]->TryGetNumber(X)||!(*Tile)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)
   ||!Instance||!Instance->TryGetStringField(TEXT("prefab_id"),Id)||Id!=Prefab
   ||!JsonVector(JsonChild(Instance,TEXT("ue_relative_to_focus")),Position)||!JsonVector(JsonChild(Instance,TEXT("ue_actor_scale_for_centimeter_asset")),Scale)
   ||!JsonNumber(Instance,TEXT("ue_yaw"),Yaw)||!JsonNumber(Instance,TEXT("template_index"),Index)
   ||X<0||Y<0||Index<0||X>10000||Y>10000||Index>10000||X!=FMath::FloorToDouble(X)||Y!=FMath::FloorToDouble(Y)||Index!=FMath::FloorToDouble(Index))
  {Finish(TEXT("Malformed source Grove placement"));return false;}
  FGroveBinding Row;Row.TileTag=FString::Printf(TEXT("UnityTile_%.0f_%.0f"),X,Y);Row.IndexTag=FString::Printf(TEXT("UnityObjectIndex_%.0f"),Index);
  Row.SourceTransform=FTransform(FRotator(0,Yaw,0),Position,Scale);
  for(const auto& Existing:Groves)if(Existing.TileTag==Row.TileTag&&Existing.IndexTag==Row.IndexTag){Finish(TEXT("Duplicate Grove source placement"));return false;}
  Groves.Add(Row);
 }
 int32 Count=0;
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if(!It->ActorHasTag(TEXT("RokGroveCenterScaleV1")))continue;++Count;FGroveBinding* Match=nullptr;
  for(auto& Row:Groves)if(It->ActorHasTag(*Row.TileTag)&&It->ActorHasTag(*Row.IndexTag)){Match=&Row;break;}
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  if(!Match||Match->Actor.IsValid()||!It->ActorHasTag(TEXT("UnityPrefab_Grove_co_tree_01_adorning"))||!It->ActorHasTag(TEXT("RokWorldMapExactSource"))
   ||Meshes.Num()!=1||!It->GetActorTransform().Equals(Match->SourceTransform,.01)||!Meshes[0]->GetComponentTransform().Equals(Match->SourceTransform,.01))
  {Finish(TEXT("Actual Grove is missing, duplicated or differs from source transform"));return false;}
  Match->Actor=*It;Match->Component=Meshes[0];Match->InitialActorTransform=It->GetActorTransform();Match->InitialComponentTransform=Meshes[0]->GetComponentTransform();
 }
 if(Count!=11){Finish(TEXT("Expected exact eleven upgraded source Grove actors"));return false;}
 for(const auto& Row:Groves)if(!Row.Actor.IsValid()){Finish(TEXT("Unbound source Grove instance"));return false;}
 return true;
}
bool URokWorldDayNightAuditSubsystem::InspectGroves(TArray<TSharedPtr<FJsonValue>>& Out)
{
 if(Groves.Num()!=11||FileHash(GroveSourcePath())!=GroveSourceHash){Finish(TEXT("Continuous Grove source snapshot changed"));return false;}
 int32 Count=0;for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->ActorHasTag(TEXT("RokGroveCenterScaleV1")))++Count;
 if(Count!=11){Finish(TEXT("Continuous Grove actor set changed"));return false;}
 const auto& S=Lighting->GetLocalState();const float ExpectedScale=ARokWorldMapCameraPawn::EvaluateUnityTreeScale(Camera->GetCurrentDxf());
 for(const auto& Row:Groves)
 {
  auto* Actor=Row.Actor.Get();auto* Mesh=Row.Component.Get();
  if(!Actor||!Mesh||Mesh->GetOwner()!=Actor||!Mesh->IsRegistered()||!Actor->ActorHasTag(TEXT("RokGroveCenterScaleV1"))||!Actor->ActorHasTag(TEXT("RokWorldMapExactSource"))
   ||!Actor->ActorHasTag(TEXT("UnityPrefab_Grove_co_tree_01_adorning"))||!Actor->ActorHasTag(*Row.TileTag)||!Actor->ActorHasTag(*Row.IndexTag)
   ||!Actor->GetActorTransform().Equals(Row.SourceTransform,.01)||!Actor->GetActorTransform().Equals(Row.InitialActorTransform,.00001)
   ||!Mesh->GetComponentTransform().Equals(Row.InitialComponentTransform,.00001))
  {Finish(TEXT("Continuous Grove source/initial transform or identity changed"));return false;}
  auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));FLinearColor ActualColor=FLinearColor::Transparent;float ActualScale=0;
  if(!MID||!MID->Parent||MID->Parent->GetPathName()!=TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_GroveCenterScale.M_WorldSource_GroveCenterScale")
   ||!Lighting->GetEnvironmentMaterials().ContainsByPredicate([MID](const auto& Weak){return Weak.Get()==MID;})
   ||!MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")),ActualColor)||!ActualColor.Equals(S.SpriteColor,.00001f)
   ||!MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RokSourceTreeScale")),ActualScale)||!FMath::IsFinite(ActualScale)||!FMath::IsNearlyEqual(ActualScale,ExpectedScale,.000002f))
  {Finish(TEXT("Current Grove renderer MID is unregistered or lacks simultaneous day/night color and source tree scale"));return false;}
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("actor"),Actor->GetName());R->SetStringField(TEXT("tile_tag"),Row.TileTag);R->SetStringField(TEXT("index_tag"),Row.IndexTag);
  R->SetStringField(TEXT("component"),Mesh->GetPathName());R->SetStringField(TEXT("actual_slot_mid"),MID->GetPathName());R->SetStringField(TEXT("material_parent"),MID->Parent->GetPathName());
  R->SetBoolField(TEXT("current_mid_in_environment_registry"),true);R->SetBoolField(TEXT("source_transform_match"),true);R->SetBoolField(TEXT("initial_transform_unchanged"),true);
  R->SetArrayField(TEXT("location"),XYZ(Actor->GetActorLocation()));R->SetArrayField(TEXT("scale"),XYZ(Actor->GetActorScale3D()));
  const FQuat Q=Actor->GetActorQuat();R->SetArrayField(TEXT("rotation_quaternion"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});
  R->SetArrayField(TEXT("actual_sprite_color"),RGB(ActualColor));R->SetNumberField(TEXT("actual_sprite_alpha"),ActualColor.A);R->SetNumberField(TEXT("expected_sprite_alpha"),S.SpriteColor.A);
  R->SetNumberField(TEXT("actual_tree_scale"),ActualScale);R->SetNumberField(TEXT("expected_tree_scale"),ExpectedScale);Out.Add(MakeShared<FJsonValueObject>(R));
 }
 return true;
}
void URokWorldDayNightAuditSubsystem::Next()
{
 if(++Stage==UE_ARRAY_COUNT(Names)){Finish();return;}
 StageStarted=Elapsed;
 if(Stage==5)Camera->SetCurrentDxf(1500);
 if(Stage==6||Stage==8)Camera->SetCurrentDxf(480);
 if(Stage==7)Camera->SetCurrentDxf(4200);
 if(Stage==9&&!Lighting->ReanchorLocalSimulationSeconds(2396))Finish(TEXT("Local clock correction failed."));
}
bool URokWorldDayNightAuditSubsystem::Inspect(TSharedPtr<FJsonObject>& Out,int32& Nights,int32& Visible)
{
 if(!Lighting.IsValid()||!Lighting->IsReady()||!Village.IsValid()||!Camera.IsValid()){Finish(TEXT("Runtime objects lost."));return false;}
 auto* PC=GetWorld()->GetFirstPlayerController();int32 Width=0,Height=0;if(PC)PC->GetViewportSize(Width,Height);
 const FIntPoint ActualViewportSize(Width,Height);
 if(Width<=0||Height<=0||(ObservedViewportSize!=FIntPoint::ZeroValue&&ObservedViewportSize!=ActualViewportSize))
 {Finish(FString::Printf(TEXT("Actual game viewport missing or resized: %dx%d (initial %dx%d)."),Width,Height,ObservedViewportSize.X,ObservedViewportSize.Y));return false;}
 ObservedViewportSize=ActualViewportSize;
 const auto& S=Lighting->GetLocalState();int32 Count=0,Waiting=0,Loading=0;Nights=0;Visible=0;
 TSet<FString> Seen;
 for(TActorIterator<ARokWorldMapEntityActor> It(GetWorld());It;++It)
 {
  const FString Kind=It->GetEntityKind();if(Kind!=TEXT("food")&&Kind!=TEXT("wood")&&Kind!=TEXT("stone")&&Kind!=TEXT("village"))continue;
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  for(auto* Mesh:Meshes)
  {
   FString Prefix=Kind==TEXT("food")?TEXT("Food_"):Kind==TEXT("wood")?TEXT("Wood_"):Kind==TEXT("stone")?TEXT("Stone_"):TEXT("Village_");
   if(!Mesh->GetName().StartsWith(Prefix))continue;
   const FString Id=FString::Printf(TEXT("%lld/%s"),int64(It->GetSourceId()),*Mesh->GetName().RightChop(Prefix.Len()));
   const auto* Layer=S.Layers.Find(Id);auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
   if(!Layer||!MID||Seen.Contains(Id)){Finish(TEXT("Actual card state/material identity missing: ")+Id);return false;}Seen.Add(Id);++Count;
   const auto R=S.ResultFor(Id);const TCHAR* Keys[]={TEXT("Color1"),TEXT("Color2"),TEXT("SpriteColor"),TEXT("NightLight")};const FLinearColor Expected[]={R.Color1,R.Color2,R.SpriteColor,R.NightLight};
   for(int32 I=0;I<4;++I){FLinearColor Actual;if(!MID->GetVectorParameterValue(FMaterialParameterInfo(Keys[I]),Actual)||!SameRGB(Actual,Expected[I])){Finish(TEXT("Actual continuous MID differs: ")+Id+TEXT("/")+Keys[I]);return false;}}
   Nights+=Layer->bNightMaterial;Waiting+=Layer->Waiting.Num();Loading+=Layer->Loading.Num();
   if(It->GetSourceId()==639&&Mesh->IsVisible()&&!Mesh->bHiddenInGame&&!It->IsHidden())++Visible;
  }
 }
 if(Count!=98||S.Layers.Num()!=98||Lighting->GetEnvironmentMaterialCount()<1){Finish(TEXT("Missing continuous resource or environment coverage."));return false;}
 for(const auto& Weak:Lighting->GetEnvironmentMaterials())
 {
  auto* MID=Weak.Get();FLinearColor Actual;
  if(!MID||!MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")),Actual)||!SameRGB(Actual,S.SpriteColor))
  {Finish(TEXT("Actual generated environment MID was not advanced."));return false;}
 }
 TArray<TSharedPtr<FJsonValue>> GroveRows;if(!InspectGroves(GroveRows))return false;
 TArray<UStaticMeshComponent*> VillageMeshes;Village->GetComponents(VillageMeshes);bool LegacyHidden=false;
 for(auto* M:VillageMeshes)if(M->GetName()==TEXT("DetailVisual"))LegacyHidden=!M->IsVisible()&&M->bHiddenInGame;
 if(!LegacyHidden||!Camera->GetViewCenterDisplay().Equals(FVector2D(1142,158),.001)){Finish(TEXT("Legacy/focus regression."));return false;}
 Out=MakeShared<FJsonObject>();Out->SetNumberField(TEXT("frame"),double(S.Frame));Out->SetNumberField(TEXT("scaled_time"),S.ScaledTime);Out->SetNumberField(TEXT("phase"),S.Phase);Out->SetArrayField(TEXT("actual_verified_sprite_color"),RGB(S.SpriteColor));
 Out->SetArrayField(TEXT("actual_viewport_size"),{MakeShared<FJsonValueNumber>(Width),MakeShared<FJsonValueNumber>(Height)});
 Out->SetNumberField(TEXT("actual_cards"),Count);Out->SetNumberField(TEXT("night_materials"),Nights);Out->SetNumberField(TEXT("waiting"),Waiting);Out->SetNumberField(TEXT("loading"),Loading);Out->SetNumberField(TEXT("village_visible"),Visible);Out->SetNumberField(TEXT("dxf"),Camera->GetCurrentDxf());Out->SetNumberField(TEXT("environment_materials"),Lighting->GetEnvironmentMaterialCount());
 Out->SetNumberField(TEXT("actual_grove_instances"),GroveRows.Num());Out->SetArrayField(TEXT("groves"),GroveRows);return true;
}
void URokWorldDayNightAuditSubsystem::Tick(float Dt)
{
 if(!bOwnsTargetWorld||bFinished||!GetWorld()->HasBegunPlay())return;Elapsed+=FMath::Max(Dt,0.f);
 if(Elapsed>70){Finish(TEXT("Continuous runtime audit timeout."));return;}
 if(Stage<0){if(Elapsed>.75)Begin();return;}
 TSharedPtr<FJsonObject> State;int32 Nights=0,Visible=0;if(!Inspect(State,Nights,Visible))return;Frames.Add(MakeShared<FJsonValueObject>(State));
 if(Pending)return;
 const auto& S=Lighting->GetLocalState();
 const bool Settled=Stage==2?Elapsed-StageStarted>.5:(Stage>=5&&Stage<=8?Elapsed-StageStarted>.3:true);
 const bool Ready=Settled&&MatchesStage(Stage,S.Phase,Nights,Visible,Camera->GetCurrentDxf(),Village->GetCurrentLodBand());
 if(!Ready)return;
 if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Competing screenshot request."));return;}
 PendingPath=Directory/FString::Printf(TEXT("%02d_%s.png"),Stage,Names[Stage]);Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("name"),Names[Stage]);Pending->SetStringField(TEXT("path"),PendingPath);Pending->SetObjectField(TEXT("at_request"),State);
 FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokWorldDayNightAuditSubsystem::Processed()
{
 if(!bOwnsTargetWorld||bFinished||!Pending)return;TSharedPtr<FJsonObject> State;int32 Nights,Visible;if(!Inspect(State,Nights,Visible))return;
 if(!MatchesStage(Stage,Lighting->GetLocalState().Phase,Nights,Visible,Camera->GetCurrentDxf(),Village->GetCurrentLodBand()))
 {Finish(FString::Printf(TEXT("Processed screenshot missed its actual continuous phase: %s"),Names[Stage]));return;}
 TArray<uint8> Data;const uint8 Signature[]={137,80,78,71,13,10,26,10};
 if(!FFileHelper::LoadFileToArray(Data,*PendingPath)||Data.Num()<24||FMemory::Memcmp(Data.GetData(),Signature,8)!=0){Finish(TEXT("Processed screenshot missing PNG."));return;}
 const auto U32=[&Data](int32 Offset){return (uint32(Data[Offset])<<24)|(uint32(Data[Offset+1])<<16)|(uint32(Data[Offset+2])<<8)|uint32(Data[Offset+3]);};
 const uint32 Width=U32(16),Height=U32(20);
 if(Width!=uint32(ObservedViewportSize.X)||Height!=uint32(ObservedViewportSize.Y))
 {Finish(FString::Printf(TEXT("Processed PNG %ux%u differs from actual game viewport %dx%d."),Width,Height,ObservedViewportSize.X,ObservedViewportSize.Y));return;}
 Pending->SetArrayField(TEXT("png_size"),{MakeShared<FJsonValueNumber>(Width),MakeShared<FJsonValueNumber>(Height)});
 Pending->SetNumberField(TEXT("bytes"),Data.Num());Pending->SetObjectField(TEXT("at_processed"),State);Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();Next();
}
bool URokWorldDayNightAuditSubsystem::Save()
{
 if(!bOwnsTargetWorld)return false;
 auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("passed"),Failures.IsEmpty()&&Captures.Num()==UE_ARRAY_COUNT(Names));R->SetArrayField(TEXT("frames"),Frames);R->SetArrayField(TEXT("captures"),Captures);
 R->SetBoolField(TEXT("owns_target_world"),bOwnsTargetWorld);
 R->SetNumberField(TEXT("audit_version"),3);R->SetStringField(TEXT("grove_source_manifest_sha256"),GroveSourceHash);
 R->SetArrayField(TEXT("actual_viewport_size"),{MakeShared<FJsonValueNumber>(ObservedViewportSize.X),MakeShared<FJsonValueNumber>(ObservedViewportSize.Y)});
 R->SetStringField(TEXT("screenshot_scope"),TEXT("Actual game viewport including Slate; observed viewport stays constant across frames/request/processed and each PNG must match it. Command-line ResX/ResY are not treated as actual viewport dimensions."));
 R->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));R->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
 R->SetStringField(TEXT("grove_cross_scope"),TEXT("Every recorded frame checks all eleven source placements, unchanged actor/component transforms and the actual slot MID's membership in the environment registry, simultaneous UnitySpriteColor and source TreeScale. Same source hash links to the separate nine-zoom buffer audit; this does not claim GPU vertex readback or complete visual parity."));
 TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));R->SetArrayField(TEXT("failures"),Errors);
 R->SetStringField(TEXT("scope"),TEXT("Continuous local-simulation epoch1792 then clock correction2396; genuine frame tick and 98 actual MID checks; independent random material staggering, Grid crop/enable and far return. Source native replay is separate. No server time/Addressables cold latency/Gamma edge acceptance."));
 FString Text;return FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokWorldDayNightAuditSubsystem::Finish(const FString& Failure)
{
 if(!bOwnsTargetWorld||bFinished)return;bFinished=true;if(!Failure.IsEmpty())Failures.Add(Failure);if(!Save())Failures.Add(TEXT("Manifest save failed"));
 UE_LOG(LogTemp,Display,TEXT("Rok continuous day/night audit passed=%d captures=%d output=%s failures=%s"),Failures.IsEmpty(),Captures.Num(),*Directory,*FString::Join(Failures,TEXT("; ")));
 FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()?0:1);
}
