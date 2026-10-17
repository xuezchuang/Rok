#include "RokHelpSourceTextData.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
bool ReadJson(const FString& Path,TSharedPtr<FJsonObject>& Out)
{
    const int64 Size=IFileManager::Get().FileSize(*Path);FString Json;
    return Size>0&&Size<32*1024*1024&&FFileHelper::LoadFileToString(Json,*Path)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Out)&&Out.IsValid();
}
bool HashMatches(const FString& Path,const FString& Expected)
{
    if(Expected.Len()!=64||!Expected.ToLower().Equals(Expected)||Expected.Contains(TEXT("/")))return false;
    TArray<uint8> Bytes;const int64 Size=IFileManager::Get().FileSize(*Path);
    return Size>0&&Size<32*1024*1024&&FFileHelper::LoadFileToArray(Bytes,*Path)&&RokTrainArmyMesh::Sha256(Bytes)==Expected;
}
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& V,double Min,double Max)
{return O->TryGetNumberField(Key,V)&&FMath::IsFinite(V)&&V>=Min&&V<=Max;}
bool Pair(const TSharedPtr<FJsonValue>& V,FVector2f& Out,float Min,float Max)
{
    if(!V.IsValid()||V->Type!=EJson::Object)return false;
    const auto O=V->AsObject();double X,Y;
    if(!Number(O,TEXT("x"),X,Min,Max)||!Number(O,TEXT("y"),Y,Min,Max))return false;
    Out=FVector2f(static_cast<float>(X),static_cast<float>(Y));return true;
}
}
bool URokHelpSourceTextData::Load(FString& Error)
{
    Texts.Reset();Textures.Reset();Brushes.Reset();
    auto Fail=[this,&Error](const TCHAR* Message){Texts.Reset();Textures.Reset();Brushes.Reset();Error=Message;return false;};
    const FString Base=RokDataPaths::Resolve(TEXT("HelpSourceText"));
    TSharedPtr<FJsonObject> Manifest,Root,Inputs;FString Schema,Hash;
    if(!ReadJson(Base/TEXT("manifest.json"),Manifest)||!Manifest->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.help.source_text.package.v1"))return Fail(TEXT("Help glyph package manifest missing/invalid"));
    for(const auto& P:{TPair<const TCHAR*,FString>(TEXT("texts_sha256"),Base/TEXT("texts.json")),
        TPair<const TCHAR*,FString>(TEXT("input_sha256"),Base/TEXT("ReferenceInputs.json")),
        TPair<const TCHAR*,FString>(TEXT("native_mesh_sha256"),Base/TEXT("source_text_meshes.json")),
        TPair<const TCHAR*,FString>(TEXT("catalog_sha256"),RokDataPaths::Resolve(TEXT("help_catalog.json")))})
        if(!Manifest->TryGetStringField(P.Key,Hash)||!HashMatches(P.Value,Hash))return Fail(TEXT("Help glyph source/package hash mismatch"));
    if(!ReadJson(Base/TEXT("texts.json"),Root)||!ReadJson(Base/TEXT("ReferenceInputs.json"),Inputs)||!Root->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.help.source_text.v1"))return Fail(TEXT("Help source glyph schema invalid"));
    FString InputHash;if(!Root->TryGetStringField(TEXT("inputSha256"),InputHash)||!Manifest->TryGetStringField(TEXT("input_sha256"),Hash)||InputHash!=Hash)return Fail(TEXT("Help glyph input provenance mismatch"));
    const TArray<TSharedPtr<FJsonValue>> *Rows=nullptr,*Specs=nullptr,*AtlasRows=nullptr;
    if(!Root->TryGetArrayField(TEXT("texts"),Rows)||Rows->Num()!=222||!Inputs->TryGetArrayField(TEXT("sourceTexts"),Specs)||Specs->Num()!=222||!Manifest->TryGetArrayField(TEXT("atlases"),AtlasRows)||AtlasRows->IsEmpty()||AtlasRows->Num()>8)return Fail(TEXT("Help source glyph coverage invalid"));
    TMap<FString,TSharedPtr<FJsonObject>> Expected;
    for(const auto& V:*Specs){if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid source spec"));const auto S=V->AsObject();FString Key;
        if(!S->TryGetStringField(TEXT("key"),Key)||Key.IsEmpty()||Expected.Contains(Key))return Fail(TEXT("Duplicate source spec"));Expected.Add(Key,S);}
    TMap<FString,int32> AtlasIds;TMap<FString,FString> AtlasHashes;
    for(const auto& V:*AtlasRows)
    {
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid glyph atlas entry"));const auto A=V->AsObject();FString Name;
        if(!A->TryGetStringField(TEXT("path"),Name)||Name!=FPaths::GetCleanFilename(Name)||!Name.StartsWith(TEXT("glyph_"))||!Name.EndsWith(TEXT(".png"))||AtlasIds.Contains(Name)||!A->TryGetStringField(TEXT("sha256"),Hash)||!HashMatches(Base/Name,Hash))return Fail(TEXT("Glyph atlas path/hash invalid"));
        UTexture2D* Texture=FImageUtils::ImportFileAsTexture2D(Base/Name);
        if(!Texture||Texture->GetSizeX()>8192||Texture->GetSizeY()>8192)return Fail(TEXT("Glyph atlas decode invalid"));
        Texture->SRGB=true;Texture->Filter=TF_Bilinear;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;Texture->UpdateResource();
        AtlasIds.Add(Name,Textures.Num());AtlasHashes.Add(Name,Hash);Textures.Add(Texture);
        FSlateBrush Brush;Brush.SetResourceObject(Texture);Brush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());Brushes.Add(Brush);
    }
    for(const auto& V:*Rows)
    {
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid glyph text entry"));const auto O=V->AsObject();FRokHelpGlyphText T;FString Atlas;
        if(!O->TryGetStringField(TEXT("key"),T.Key)||Texts.Contains(T.Key)||!Expected.Contains(T.Key)||!O->TryGetStringField(TEXT("text"),T.Text)||T.Text.IsEmpty()||T.Text.Len()>4096||!O->TryGetStringField(TEXT("atlas"),Atlas)||!AtlasIds.Contains(Atlas)||!O->TryGetStringField(TEXT("atlasSha256"),Hash)||Hash!=AtlasHashes[Atlas])return Fail(TEXT("Glyph text identity invalid"));
        const auto S=Expected[T.Key];FString SourceText;if(!S->TryGetStringField(TEXT("text"),SourceText)||SourceText!=T.Text)return Fail(TEXT("Semantic source text mismatch"));
        double W,H,F,B,A,L,SourceW,SourceF,SourceB,SourceA,PPU;
        if(!Number(O,TEXT("width"),W,1,1440)||!Number(O,TEXT("height"),H,1,10000)||!Number(O,TEXT("fontSize"),F,1,100)||!Number(O,TEXT("style"),B,0,1)||!Number(O,TEXT("alignment"),A,0,8)||!Number(O,TEXT("lineCount"),L,1,1000)||!Number(O,TEXT("pixelsPerUnit"),PPU,1,1)
            ||!Number(S,TEXT("width"),SourceW,1,1440)||!Number(S,TEXT("fontSize"),SourceF,1,100)||!Number(S,TEXT("style"),SourceB,0,1)||!Number(S,TEXT("alignment"),SourceA,0,8)||F!=SourceF||B!=SourceB||A!=SourceA||FMath::Abs(W-SourceW)>.01||F!=FMath::FloorToDouble(F)||A!=FMath::FloorToDouble(A)||L!=FMath::FloorToDouble(L))return Fail(TEXT("Glyph source layout mismatch"));
        T.Size=FVector2D(W,H);T.FontSize=static_cast<int32>(F);T.Style=static_cast<int32>(B);T.Alignment=static_cast<int32>(A);T.Lines=static_cast<int32>(L);T.Atlas=AtlasIds[Atlas];
        const TArray<TSharedPtr<FJsonValue>> *XY=nullptr,*UV=nullptr,*Triangles=nullptr;
        if(!O->TryGetArrayField(TEXT("xy"),XY)||!O->TryGetArrayField(TEXT("uv"),UV)||!O->TryGetArrayField(TEXT("triangles"),Triangles)||XY->IsEmpty()||XY->Num()>10000||XY->Num()%4||UV->Num()!=XY->Num()||Triangles->Num()!=XY->Num()/4*6)return Fail(TEXT("Glyph topology invalid"));
        for(int32 I=0;I<XY->Num();++I){FVector2f P,U;if(!Pair((*XY)[I],P,-10000,10000)||!Pair((*UV)[I],U,0,1))return Fail(TEXT("Glyph coordinate invalid"));T.XY.Add(P);T.UV.Add(FVector2f(U.X,1.0f-U.Y));}
        for(const auto& I:*Triangles){double Index;if(!I.IsValid()||!I->TryGetNumber(Index)||!FMath::IsFinite(Index)||Index<0||Index>=T.XY.Num()||Index!=FMath::FloorToDouble(Index))return Fail(TEXT("Glyph index invalid"));T.Indices.Add(static_cast<uint16>(Index));}
        Texts.Add(T.Key,MoveTemp(T));
    }
    Error.Empty();return true;
}
const FRokHelpGlyphText* URokHelpSourceTextData::Find(const FString& Text,int32 FontSize,bool Bold,int32 Alignment,float Width) const
{
    for(const auto& Pair:Texts){const auto& T=Pair.Value;if(T.Text==Text&&T.FontSize==FontSize&&T.Style==(Bold?1:0)&&T.Alignment==Alignment&&FMath::Abs(T.Size.X-Width)<.01)return &T;}
    return nullptr;
}
