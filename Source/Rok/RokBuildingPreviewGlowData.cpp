#include "RokBuildingPreviewGlowData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace RokBuildingPreviewGlow
{
namespace
{
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& Out)
{ return O.IsValid()&&O->TryGetNumberField(Key,Out)&&FMath::IsFinite(Out); }
bool ArrayNumbers(const TSharedPtr<FJsonValue>& V,int32 Count,TArray<double>& Out)
{
    Out.Reset();if(!V.IsValid()||V->Type!=EJson::Array||V->AsArray().Num()!=Count)return false;
    for(const auto& X:V->AsArray()){double N;if(!X.IsValid()||!X->TryGetNumber(N)||!FMath::IsFinite(N))return false;Out.Add(N);}return true;
}
bool FieldNumbers(const TSharedPtr<FJsonObject>& O,const TCHAR* K,int32 Count,TArray<double>& Out)
{const auto* V=O->Values.Find(K);return V&&ArrayNumbers(*V,Count,Out);}
}
FString FData::DefaultPath(){return RokDataPaths::Resolve(TEXT("BuildingPreviewGlow/glow_data.json"));}
bool FData::Load(const FString& Path,FString& Error)
{
    Layers.Reset();Error.Reset();auto Fail=[&](const TCHAR* Why){Error=Why;return false;};
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root.IsValid())return Fail(TEXT("Glow data unavailable"));
    FString Schema;double Duration;
    if(!Root->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.building.preview_glow.data.v1")||!Number(Root,TEXT("duration"),Duration)||Duration!=2)return Fail(TEXT("Wrong source glow schema/duration"));
    const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;
    if(!Root->TryGetArrayField(TEXT("sources"),Sources)||Sources->Num()!=5)return Fail(TEXT("Glow provenance missing"));
    // Only packaged data is re-hashed. The source glow report stays in Saved/RokUnityMigration and, like the
    // Unity probe evidence and Scripts/, does not ship; Verify* scripts check those hashes.
    TSet<FString> Seen;
    for(const auto& V:*Sources)
    {
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid provenance"));
        FString P,H;const auto O=V->AsObject();TArray<uint8> Bytes;
        if(!O->TryGetStringField(TEXT("path"),P)||!O->TryGetStringField(TEXT("sha256"),H)||H.Len()!=64||!FPaths::IsRelative(P)||P.Contains(TEXT(".."))||Seen.Contains(P)
            ||(!P.StartsWith(TEXT("Saved/"))&&!P.StartsWith(TEXT("Scripts/"))&&!RokDataPaths::IsDataPath(P))
            ||(RokDataPaths::IsPackagedDataPath(P)&&(!FFileHelper::LoadFileToArray(Bytes,*(RokDataPaths::ResolveProjectPath(P)))||RokTrainArmyMesh::Sha256(Bytes)!=H)))return Fail(TEXT("Glow provenance changed or unsafe"));
        Seen.Add(P);
    }
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Root->TryGetArrayField(TEXT("layers"),Rows)||Rows->Num()!=2)return Fail(TEXT("Expected two source layers"));
    TArray<FLayer> Parsed;
    for(int32 Index=0;Index<2;++Index)
    {
        const auto V=(*Rows)[Index];if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid layer"));const auto O=V->AsObject();FLayer L;TArray<double> A;
        if(!O->TryGetStringField(TEXT("name"),L.Name)||L.Name!=(Index==0?TEXT("img_star003_2"):TEXT("img_star003_1")))return Fail(TEXT("Wrong source layer order"));
        if(!FieldNumbers(O,TEXT("center"),2,A))return Fail(TEXT("Missing center"));L.Center=FVector2D(A[0],A[1]);
        if(!FieldNumbers(O,TEXT("size"),2,A)||A[0]!=250||A[1]!=250)return Fail(TEXT("Source image size changed"));L.Size=FVector2D(A[0],A[1]);
        if(!FieldNumbers(O,TEXT("scale"),3,A))return Fail(TEXT("Missing scale"));L.Scale=FVector(A[0],A[1],A[2]);
        const double ExpectedScale=Index==0?1.7:2.2;if(!L.Scale.Equals(FVector(ExpectedScale),.000001))return Fail(TEXT("Source scale changed"));
        if(!FieldNumbers(O,TEXT("initial_rotation"),4,A))return Fail(TEXT("Missing initial quaternion"));L.InitialRotation=FQuat(A[0],A[1],A[2],A[3]);if(L.InitialRotation.SizeSquared()<.99||L.InitialRotation.SizeSquared()>1.01)return Fail(TEXT("Invalid quaternion norm"));if(!L.InitialRotation.IsNormalized())L.InitialRotation.Normalize();
        if(!Number(O,TEXT("degrees_per_second"),L.DegreesPerSecond)||L.DegreesPerSecond!=(Index==0?4:3))return Fail(TEXT("Source angular rate changed"));
        const TArray<TSharedPtr<FJsonValue>>* Keys=nullptr;if(!O->TryGetArrayField(TEXT("alpha_keys"),Keys)||Keys->Num()!=3)return Fail(TEXT("Source curve missing"));
        for(int32 K=0;K<3;++K){const auto KV=(*Keys)[K];if(!KV.IsValid()||KV->Type!=EJson::Object)return Fail(TEXT("Invalid key"));FAlphaKey Key;const auto KO=KV->AsObject();if(!Number(KO,TEXT("time"),Key.Time)||Key.Time!=K||!Number(KO,TEXT("value"),Key.Value)||Key.Value<0||Key.Value>1||!Number(KO,TEXT("inSlope"),Key.InSlope)||!Number(KO,TEXT("outSlope"),Key.OutSlope))return Fail(TEXT("Invalid alpha key"));L.AlphaKeys.Add(Key);}
        const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
        if(!O->TryGetArrayField(TEXT("vertices"),Points)||Points->Num()!=4)return Fail(TEXT("Missing native quad"));for(const auto& P:*Points){if(!ArrayNumbers(P,3,A))return Fail(TEXT("Invalid vertex"));L.Vertices.Add(FVector(A[0],A[1],A[2]));}
        if(!O->TryGetArrayField(TEXT("uv"),Points)||Points->Num()!=4)return Fail(TEXT("Missing native UV"));for(const auto& P:*Points){if(!ArrayNumbers(P,2,A)||A[0]<0||A[0]>1||A[1]<0||A[1]>1)return Fail(TEXT("Invalid UV"));L.UV.Add(FVector2D(A[0],A[1]));}
        if(!FieldNumbers(O,TEXT("triangles"),6,A))return Fail(TEXT("Missing source triangles"));for(double I:A){if(I<0||I>3||I!=FMath::FloorToDouble(I))return Fail(TEXT("Invalid triangle index"));L.Triangles.Add(static_cast<int32>(I));}
        Parsed.Add(MoveTemp(L));
    }
    Layers=MoveTemp(Parsed);return true;
}
bool FData::Sample(double Seconds,TArray<FLayerSample>& Out) const
{
    Out.Reset();if(!IsLoaded()||!FMath::IsFinite(Seconds)||Seconds<0)return false;
    const double Phase=FMath::Fmod(Seconds,2.0);const int32 Segment=Phase<1?0:1;const double T=Phase-Segment,T2=T*T,T3=T2*T;
    for(const FLayer& L:Layers)
    {
        FLayerSample S;const auto& A=L.AlphaKeys[Segment];const auto& B=L.AlphaKeys[Segment+1];
        S.Alpha=(2*T3-3*T2+1)*A.Value+(T3-2*T2+T)*A.OutSlope+(-2*T3+3*T2)*B.Value+(T3-T2)*B.InSlope;
        const double Angle=FMath::DegreesToRadians(FMath::Fmod(Seconds,360.0/L.DegreesPerSecond)*L.DegreesPerSecond);
        S.LocalRotation=L.InitialRotation*FQuat(FVector::ZAxisVector,Angle);
        for(const FVector& V:L.Vertices){const FVector P=S.LocalRotation.RotateVector(V*L.Scale);S.ScreenVertices.Add(L.Center+FVector2D(P.X,-P.Y));}
        Out.Add(MoveTemp(S));
    }return true;
}
bool FPlayback::Advance(double DeltaSeconds)
{if(!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<0||!FMath::IsFinite(Seconds+DeltaSeconds))return false;Seconds+=DeltaSeconds;return true;}
}
