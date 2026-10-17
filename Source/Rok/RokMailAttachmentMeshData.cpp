#include "RokMailAttachmentMeshData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace RokMailAttachmentMesh
{
// SHA-256 over exact source bytes. Core's generic GetSHA256Signature asserts on this Windows build.
FString Sha256(TConstArrayView<uint8> Bytes)
{
    static constexpr uint32 K[] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    uint32 H[] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint64 Bits = static_cast<uint64>(Bytes.Num()) * 8;
    const int64 Blocks = (static_cast<int64>(Bytes.Num()) + 9 + 63) / 64;
    auto R = [](uint32 X, int N) { return (X >> N) | (X << (32-N)); };
    for (int64 Block = 0; Block < Blocks; ++Block)
    {
        uint32 W[64] = {};
        for (int I = 0; I < 64; ++I)
        {
            const int64 Offset = Block * 64 + I;
            uint8 B = Offset < Bytes.Num() ? Bytes[Offset] : Offset == Bytes.Num() ? 0x80 : 0;
            if (Offset >= Blocks * 64 - 8) B = static_cast<uint8>(Bits >> ((Blocks * 64 - 1 - Offset) * 8));
            W[I/4] |= static_cast<uint32>(B) << (24 - I%4 * 8);
        }
        for (int I=16; I<64; ++I) W[I]=W[I-16]+(R(W[I-15],7)^R(W[I-15],18)^(W[I-15]>>3))+W[I-7]+(R(W[I-2],17)^R(W[I-2],19)^(W[I-2]>>10));
        uint32 A=H[0],B=H[1],C=H[2],D=H[3],E=H[4],F=H[5],G=H[6],V=H[7];
        for (int I=0; I<64; ++I)
        {
            const uint32 T1=V+(R(E,6)^R(E,11)^R(E,25))+((E&F)^(~E&G))+K[I]+W[I];
            const uint32 T2=(R(A,2)^R(A,13)^R(A,22))+((A&B)^(A&C)^(B&C));
            V=G;G=F;F=E;E=D+T1;D=C;C=B;B=A;A=T1+T2;
        }
        H[0]+=A;H[1]+=B;H[2]+=C;H[3]+=D;H[4]+=E;H[5]+=F;H[6]+=G;H[7]+=V;
    }
    FString Result; for (uint32 V : H) Result += FString::Printf(TEXT("%08x"), V); return Result;
}
namespace
{
using JObject = TSharedPtr<FJsonObject>;
using JArray = TArray<TSharedPtr<FJsonValue>>;
bool Obj(const TSharedPtr<FJsonValue>& V, JObject& O) { if (!V.IsValid() || V->Type != EJson::Object) return false; O=V->AsObject(); return O.IsValid(); }
bool Number(const JObject& O, const TCHAR* Key, double& N) { return O->TryGetNumberField(Key,N) && FMath::IsFinite(N); }
bool Value(const TSharedPtr<FJsonValue>& V, double& N) { return V.IsValid() && V->TryGetNumber(N) && FMath::IsFinite(N); }
bool String(const JObject& O, const TCHAR* Key, FString& S) { return O->TryGetStringField(Key,S) && !S.IsEmpty() && S.Len() <= 1024; }
bool Array(const JObject& O, const TCHAR* Key, const JArray*& A) { return O->TryGetArrayField(Key,A); }
bool Path(const FString& S) { return (S.StartsWith(TEXT("ref/client-unity/")) || S.StartsWith(TEXT("ref/config-tables/"))) && !S.Contains(TEXT("..")) && !S.Contains(TEXT(":")) && !S.Contains(TEXT("\\")); }
bool Vector(const JObject& O, const TCHAR* Key, TConstArrayView<double> Expected)
{ const JArray* A=nullptr; if (!Array(O,Key,A) || A->Num()!=Expected.Num()) return false; for(int32 I=0;I<A->Num();++I) {double N; if(!Value((*A)[I],N)||FMath::Abs(N-Expected[I])>.000001)return false;}return true; }
}
bool Parse(const FString& Json, FClip& Out, FString& Error)
{
    Out=FClip(); Error=TEXT("Invalid Mail attachment mesh schema or sample data");
    if(Json.Len()>64*1024*1024)return false;
    JObject Root; if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root.IsValid())return false;
    FString S; double N; bool B;
    if(!String(Root,TEXT("schema"),S)||S!=TEXT("rok.mail_attachment.spine_mesh.v1")||!Number(Root,TEXT("schema_version"),N)||N!=1)return false;
    if(!String(Root,TEXT("skin"),S)||S!=TEXT("gift5"))return false;
    if(!String(Root,TEXT("animation"),S)||S!=TEXT("UE_TreasureBox_stay")||!Root->TryGetBoolField(TEXT("loop"),B)||!B)return false;
    if(!Number(Root,TEXT("source_scale"),N)||FMath::Abs(N-.01)>.0000001)return false;
    if(!Number(Root,TEXT("duration"),N)||N<.001||N>60)return false;
    FClip Clip; Clip.Duration=static_cast<float>(N); 
    const TSharedPtr<FJsonObject>* PMA=nullptr; const TSharedPtr<FJsonObject>* Transform=nullptr;
    if(!Root->TryGetObjectField(TEXT("pma"),PMA)||!(*PMA)->TryGetBoolField(TEXT("vertex_colors"),B)||!B
        ||!String(*PMA,TEXT("source_blend"),S)||S!=TEXT("One OneMinusSrcAlpha"))return false;
    if(!String(*PMA,TEXT("texture"),S)||S!=TEXT("source atlas pixels unchanged")
        ||!String(*PMA,TEXT("exported_rgba"),S)||S!=TEXT("straight attachment*slot*skeleton tint; consumer must apply source PMA once"))return false;
    if(!Root->TryGetObjectField(TEXT("source_transform"),Transform))return false;
    if(!Number(*Transform,TEXT("skeleton_scale"),N)||FMath::Abs(N-.01)>.0000001)return false;
    const JArray* Bounds=nullptr;
    if(!Array(Root,TEXT("bounds"),Bounds)||Bounds->Num()!=4)return false;
    double Limits[4];for(int32 I=0;I<4;++I)if(!Value((*Bounds)[I],Limits[I])||FMath::Abs(Limits[I])>10000)return false;
    if(Limits[2]<=Limits[0]||Limits[3]<=Limits[1])return false;
    const JArray* Kinds=nullptr;
    if(!Array(Root,TEXT("attachment_types"),Kinds)||Kinds->IsEmpty())return false;
    for(const auto& Kind:*Kinds)if(!Kind->TryGetString(S)||(S!=TEXT("MeshAttachment")&&S!=TEXT("RegionAttachment")))return false;

    const double Scale[]={.06,.06,.06}, Anchor[]={.5,.5}, Position[]={0,-5.1}, Pivot[]={.5,.5};
    if(!Vector(*Transform,TEXT("rect_scale"),Scale)||!Vector(*Transform,TEXT("anchor"),Anchor)||!Vector(*Transform,TEXT("anchored_position"),Position)||!Vector(*Transform,TEXT("pivot"),Pivot))return false;
    const JArray* A=nullptr;
    if(!Array(Root,TEXT("sources"),A)||A->Num()<6||A->Num()>32)return false;
    for(const auto& V:*A)
    {
        JObject O; FString P,H; if(!Obj(V,O)||!String(O,TEXT("path"),P)||!Path(P)||!String(O,TEXT("sha256"),H)||H.Len()!=64||Clip.Sources.Contains(P))return false;
        for(TCHAR C:H)if(!FChar::IsHexDigit(C))return false;
        Clip.Sources.Add(P,H.ToLower());
    }
    const JArray* Frames=nullptr;
    if(!Array(Root,TEXT("frames"),Frames)||Frames->Num()<2||Frames->Num()>1000||!Number(Root,TEXT("frame_count"),N)||N!=Frames->Num())return false;
    int64 VertexBudget=0,IndexBudget=0;
    for(int32 F=0;F<Frames->Num();++F)
    {
        JObject O; if(!Obj((*Frames)[F],O)||!Number(O,TEXT("time"),N)||FMath::Abs(N-F*static_cast<double>(Clip.Duration)/Frames->Num())>.00001)return false;
        const JArray* Draws=nullptr; if(!Array(O,TEXT("draws"),Draws)||Draws->Num()<1||Draws->Num()>128)return false;
        FFrame Frame;
        for(const auto& V:*Draws)
        {
            JObject D; FDraw Draw; FString Slot,Attachment;
            if(!Obj(V,D)||!String(D,TEXT("slot"),Slot)||!String(D,TEXT("attachment"),Attachment)||!String(D,TEXT("texture"),Draw.Texture)||!Path(Draw.Texture)||!Clip.Sources.Contains(Draw.Texture)||!Draw.Texture.EndsWith(TEXT(".png")))return false;
            if(!String(D,TEXT("blend"),S)||S!=TEXT("Normal"))return false;
            Draw.Identity=Slot+TEXT("/")+Attachment;
            const JArray *XY=nullptr,*UV=nullptr,*Indices=nullptr,*Color=nullptr;
            if(!Array(D,TEXT("xy"),XY)||!Array(D,TEXT("uv"),UV)||XY->Num()<6||XY->Num()%2||XY->Num()!=UV->Num()||XY->Num()>65534)return false;
            VertexBudget+=XY->Num()/2; if(VertexBudget>2000000)return false;
            for(int32 I=0;I<XY->Num();I+=2)
            {
                double X,Y,U,W;
                if(!Value((*XY)[I],X)||!Value((*XY)[I+1],Y)||FMath::Abs(X)>10000||FMath::Abs(Y)>10000||!Value((*UV)[I],U)||!Value((*UV)[I+1],W)||U<0||U>1||W<0||W>1)return false;
                Draw.XY.Emplace(static_cast<float>(X),static_cast<float>(Y));Draw.UV.Emplace(static_cast<float>(U),static_cast<float>(W));
            }
            if(!Array(D,TEXT("triangles"),Indices)||Indices->Num()<3||Indices->Num()%3||Indices->Num()>200000)return false;
            IndexBudget+=Indices->Num();if(IndexBudget>6000000)return false;
            for(const auto& I:*Indices){if(!Value(I,N)||N<0||N>=Draw.XY.Num()||N!=FMath::FloorToDouble(N))return false;Draw.Indices.Add(static_cast<uint16>(N));}
            if(!Array(D,TEXT("rgba"),Color)||Color->Num()!=4)return false;
            float C[4];for(int32 I=0;I<4;++I){if(!Value((*Color)[I],N)||N<0||N>1)return false;C[I]=static_cast<float>(N);}Draw.Tint=FLinearColor(C[0],C[1],C[2],C[3]);
            Frame.Draws.Add(MoveTemp(Draw));
        }
        Clip.Frames.Add(MoveTemp(Frame));
    }
    Out=MoveTemp(Clip); Error.Empty();return true;
}
}
