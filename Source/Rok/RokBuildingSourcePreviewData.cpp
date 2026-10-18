#include "RokBuildingSourcePreviewData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace RokBuildingSourcePreview
{
FString MetadataFile(){return RokDataPaths::Resolve(TEXT("BuildingSourcePreview/source_preview.json"));}
bool ResolveDeclaredModel(const TArray<FName>& Tags,const FString& Json,FString& Model,FString& Error)
{
    Model.Reset();Error.Reset();auto Fail=[&](const TCHAR* E){Error=E;return false;};
    if(!Tags.Contains(TEXT("RokUnityConfigInitBuilding")))return Fail(TEXT("Actor has no declared initial-building identity"));
    int32 Init=0,Type=0,InitCount=0,TypeCount=0;
    for(FName Tag:Tags)
    {
        const FString S=Tag.ToString();
        if(S.StartsWith(TEXT("UnityInitId_"))){++InitCount;if(!LexTryParseString(Init,*S.RightChop(12)))return Fail(TEXT("Malformed initial-building tag"));}
        if(S.StartsWith(TEXT("UnityBuildingType_"))){++TypeCount;if(!LexTryParseString(Type,*S.RightChop(18)))return Fail(TEXT("Malformed building-type tag"));}
    }
    if(InitCount!=1||TypeCount!=1||Init<=0||Type!=9)return Fail(TEXT("Missing, ambiguous or unsupported source actor identity"));
    TSharedPtr<FJsonObject> Root;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid())return Fail(TEXT("Invalid initial-state manifest"));
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Root->TryGetArrayField(TEXT("buildings"),Rows))return Fail(TEXT("Initial-state buildings missing"));
    int32 Matches=0;FString Candidate;
    for(const auto& V:*Rows)
    {
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid initial-state row"));const auto R=V->AsObject();double Id=0;if(!R->TryGetNumberField(TEXT("init_id"),Id))return Fail(TEXT("Missing initial row id"));if(Id!=Init)continue;++Matches;
        double RowType=0;const TSharedPtr<FJsonObject>* Visual=nullptr;
        if(!R->TryGetNumberField(TEXT("type_id"),RowType)||RowType!=Type||!R->TryGetObjectField(TEXT("visual"),Visual)||!(*Visual)->TryGetStringField(TEXT("model_id"),Candidate)||Candidate.IsEmpty())return Fail(TEXT("Source model unknown or conflicts with actor type"));
        FString Sprite,Guid;if(!(*Visual)->TryGetStringField(TEXT("sprite_name"),Sprite)||Sprite!=Candidate||!(*Visual)->TryGetStringField(TEXT("sprite_guid"),Guid)||Guid.IsEmpty())return Fail(TEXT("Declared model lacks exact scene sprite identity"));
    }
    if(Matches!=1)return Fail(TEXT("Initial actor row missing or ambiguous"));
    Model=Candidate;return true;
}
bool ResolveActorModel(AActor* Actor,FString& Model,FString& Error)
{
    Model.Reset();Error.Reset();
    if(!Actor||!Actor->GetWorld()||Actor->GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityGameAppCity")) {Error=TEXT("No live source city actor");return false;}
    int32 Count=0;FName Identity;
    for(FName Tag:Actor->Tags)if(Tag.ToString().StartsWith(TEXT("UnityInitId_")))Identity=Tag;
    for(TActorIterator<AActor> It(Actor->GetWorld());It;++It)if(It->ActorHasTag(Identity)&&It->ActorHasTag(TEXT("RokUnityConfigInitBuilding")))++Count;
    if(Count!=1){Error=TEXT("Source city actor identity is ambiguous");return false;}
    FString Json;if(!FFileHelper::LoadFileToString(Json,*(RokDataPaths::Resolve(TEXT("unity_initial_city_state.json"))))){Error=TEXT("Initial-state visual metadata missing");return false;}
    if(!ResolveDeclaredModel(Actor->Tags,Json,Model,Error))return false;
    FLayout Layout;if(!LoadLayout(Model,MetadataFile(),Layout,Error)){Model.Reset();return false;}
    return true;
}
bool LoadLayout(const FString& ExplicitModel,const FString& MetadataPath,FLayout& Out,FString& Error)
{
    Out=FLayout();Error.Reset();auto Fail=[&](const TCHAR* E){Error=E;Out=FLayout();return false;};
    if(ExplicitModel.IsEmpty())return Fail(TEXT("Presentation model is unknown"));
    FString Json;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Json,*MetadataPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid())return Fail(TEXT("Source preview metadata unavailable"));
    FString Schema,Model;
    if(!Root->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.building.source_preview.v1")||!Root->TryGetStringField(TEXT("model_id"),Model)||Model!=ExplicitModel)return Fail(TEXT("Explicit model does not match source preview metadata"));
    const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;
    if(!Root->TryGetArrayField(TEXT("sources"),Sources)||Sources->Num()<3)return Fail(TEXT("Source preview provenance missing"));
    // Only packaged data (the initial-city manifest) is re-hashed; ref/ and Scripts/ do not ship.
    bool InitialVerified=false;TMap<FString,FString> Verified;
    for(const auto& V:*Sources)
    {
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid preview source entry"));
        FString Path,Hash;const auto S=V->AsObject();TArray<uint8> Bytes;
        if(!S->TryGetStringField(TEXT("path"),Path)||FPaths::IsRelative(Path)==false||Path.Contains(TEXT(".."))||!S->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64)return Fail(TEXT("Unsafe source preview path/hash"));
        const bool bInitial=Path==TEXT("Saved/RokUnityMigration/unity_initial_city_state.json")||Path==TEXT("Content/RokData/unity_initial_city_state.json");
        if(!Path.StartsWith(TEXT("ref/"))&&!Path.StartsWith(TEXT("Scripts/"))&&!bInitial)return Fail(TEXT("Unexpected preview provenance root"));
        const bool bPackaged=RokDataPaths::IsPackagedDataPath(Path);
        if(Verified.Contains(Path)||(bPackaged&&(!FFileHelper::LoadFileToArray(Bytes,*(RokDataPaths::ResolveProjectPath(Path)))||RokTrainArmyMesh::Sha256(Bytes)!=Hash)))return Fail(TEXT("Source preview provenance changed"));
        Verified.Add(Path,Hash);if(bInitial&&bPackaged)InitialVerified=true;
    }
    if(!InitialVerified)return Fail(TEXT("Declared scene model has no validated initial-state provenance"));
    const TSharedPtr<FJsonObject>* Sprite=nullptr;const TSharedPtr<FJsonObject>* Image=nullptr;const TSharedPtr<FJsonObject>* Layout=nullptr;
    if(!Root->TryGetObjectField(TEXT("sprite"),Sprite)||!Root->TryGetObjectField(TEXT("image"),Image)||!Root->TryGetObjectField(TEXT("layout"),Layout))return Fail(TEXT("Source image/native layout missing"));
    bool Pending=false;if((*Layout)->TryGetBoolField(TEXT("pending"),Pending)&&Pending)return Fail(TEXT("Native source preview geometry has not been validated"));
    auto Vec=[](const TSharedPtr<FJsonObject>& J,const TCHAR* K,FVector2D& V){const TArray<TSharedPtr<FJsonValue>>* A=nullptr;double X,Y;if(!J->TryGetArrayField(K,A)||A->Num()!=2||!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y))return false;V=FVector2D(X,Y);return true;};
    FVector2D TextureSize,ImageSize,Scale,Pivot,Anchored;
    if(!(*Sprite)->TryGetStringField(TEXT("path"),Out.SpritePath)||!(*Sprite)->TryGetStringField(TEXT("guid"),Out.SpriteGuid)||!(*Sprite)->TryGetStringField(TEXT("sha256"),Out.SpriteHash)||Out.SpriteHash.Len()!=64||!Verified.Contains(Out.SpritePath)||Verified.FindRef(Out.SpritePath)!=Out.SpriteHash||!Vec(*Sprite,TEXT("size"),TextureSize))return Fail(TEXT("Raw UI sprite identity mismatch"));
    if(!Vec(*Image,TEXT("size"),ImageSize)||!Vec(*Image,TEXT("scale"),Scale)||!Vec(*Image,TEXT("pivot"),Pivot)||!Vec(*Image,TEXT("anchored_position"),Anchored))return Fail(TEXT("Source image transforms missing"));
    // This slice implements only the independently decoded static barracks UI prefab.
    if(Model!=TEXT("build_9_101_1")||Out.SpriteGuid!=TEXT("4bb871ef2938a6d48a9b46af5cd8f287")||!TextureSize.Equals(FVector2D(512,490),.001)||!ImageSize.Equals(TextureSize,.001)||!Scale.Equals(FVector2D(.7),.0001)||!Pivot.Equals(FVector2D(.5),.001)||!Anchored.IsNearlyZero(.001))return Fail(TEXT("Unsupported source preview image semantics"));
    if(!Vec(*Layout,TEXT("position"),Out.Position)||!Vec(*Layout,TEXT("size"),Out.Size)||!Vec(*Layout,TEXT("image_position"),Out.ImagePosition)||!Vec(*Layout,TEXT("image_size"),Out.ImageSize)||!Out.ImagePosition.IsNearlyZero(.001)||!Out.ImageSize.Equals(Out.Size,.001)||Out.Size.X<=0||Out.Size.Y<=0||Out.Size.X>1440||Out.Size.Y>900)return Fail(TEXT("Invalid native preview rectangle"));
    // The native capture is Unity probe evidence under Saved/ and does not ship. The generator checked its
    // model, sprite GUID and input hash and embedded its projected quad and UV here.
    const TSharedPtr<FJsonObject>* Native=nullptr;FString NativePath,NativeHash,InputHash;
    if(!Root->TryGetObjectField(TEXT("native_evidence"),Native)||!(*Native)->TryGetStringField(TEXT("path"),NativePath)||!NativePath.StartsWith(TEXT("Saved/UnityBuildingSourcePreviewProbe/Artifacts/"))||NativePath.Contains(TEXT(".."))||!(*Native)->TryGetStringField(TEXT("sha256"),NativeHash)||NativeHash.Len()!=64||!(*Native)->TryGetStringField(TEXT("input_sha256"),InputHash)||InputHash.Len()!=64)return Fail(TEXT("Native source geometry evidence missing"));
    const TArray<TSharedPtr<FJsonValue>>* Quad=nullptr;const TArray<TSharedPtr<FJsonValue>>* UV=nullptr;
    if(!(*Native)->TryGetArrayField(TEXT("quad"),Quad)||Quad->Num()!=4||!(*Native)->TryGetArrayField(TEXT("uv"),UV)||UV->Num()!=4)return Fail(TEXT("Native image must be a simple four-vertex quad"));
    TSet<int32> Corners;
    for(int32 I=0;I<4;++I)
    {
        if((*Quad)[I]->Type!=EJson::Object||(*UV)[I]->Type!=EJson::Object)return Fail(TEXT("Native quad format invalid"));
        double X,Y,U,V;if(!(*Quad)[I]->AsObject()->TryGetNumberField(TEXT("x"),X)||!(*Quad)[I]->AsObject()->TryGetNumberField(TEXT("y"),Y)||!(*UV)[I]->AsObject()->TryGetNumberField(TEXT("x"),U)||!(*UV)[I]->AsObject()->TryGetNumberField(TEXT("y"),V))return Fail(TEXT("Native quad coordinates missing"));
        const double NX=(X-Out.Position.X)/Out.Size.X,NY=(Y-Out.Position.Y)/Out.Size.Y;
        if(!FMath::IsNearlyEqual(NX,U,.0001)||!FMath::IsNearlyEqual(1.-NY,V,.0001)||(FMath::Abs(NX)>.0001&&FMath::Abs(NX-1.)>.0001)||(FMath::Abs(NY)>.0001&&FMath::Abs(NY-1.)>.0001))return Fail(TEXT("Unsupported native image UV/rotation; no guessed flip"));
        Corners.Add(FMath::RoundToInt(NX)+2*FMath::RoundToInt(NY));
    }
    if(Corners.Num()!=4)return Fail(TEXT("Native source image quad is degenerate"));
    Out.ModelId=Model;Out.TextureSize=FIntPoint(512,490);return true;
}

}
