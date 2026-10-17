#include "RokTrainArmyMeshWidget.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SLeafWidget.h"

class SRokTrainArmyMesh : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRokTrainArmyMesh) {} SLATE_END_ARGS()
    void Construct(const FArguments&, URokTrainArmyMeshWidget* InOwner) { Owner=InOwner; SetCanTick(true); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1440,810); }
    virtual void Tick(const FGeometry&, double, float Delta) override
    {
        if(Owner.IsValid()) { Owner->AdvanceAnimation(Delta); Invalidate(EInvalidateWidgetReason::Paint); }
    }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements,
        int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        if(!Owner.IsValid()||Owner->Clip.Frames.IsEmpty())return Layer;
        const auto& O=*Owner.Get();
        const float Sample=O.Time/O.Clip.Duration*O.Clip.Frames.Num();
        const int32 Index=FMath::FloorToInt(Sample)%O.Clip.Frames.Num();
        const auto& A=O.Clip.Frames[Index];const auto& B=O.Clip.Frames[(Index+1)%O.Clip.Frames.Num()];const float T=FMath::Frac(Sample);
        ++O.PaintCount; O.LastPaintFrameIndex=Index; O.LastPaintDrawCount=A.Draws.Num();
        for(int32 D=0;D<A.Draws.Num();++D)
        {
            const auto& Draw=A.Draws[D];const auto* Next=B.Draws.IsValidIndex(D)?&B.Draws[D]:nullptr;
            const bool Interpolate=Next&&RokTrainArmyMesh::CanInterpolate(Draw,*Next);
            const FLinearColor Tint=Interpolate?FMath::Lerp(Draw.Tint,Next->Tint,T):Draw.Tint;
            // Source Gamma project: PNG already PMA. Keep texture bytes, no second texture-alpha multiply.
            // Slate VS decodes vertex RGB; encode our single-PMA tint once for that transport.
            const FColor Color=RokTrainArmyMesh::PremultipliedTint(Tint,Style.GetColorAndOpacityTint()).ToFColor(true);
            TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
            Vertices.Reserve(Draw.XY.Num());Indices.Reserve(Draw.Indices.Num());
            for(int32 V=0;V<Draw.XY.Num();++V)
            {
                const auto P=Interpolate?FMath::Lerp(Draw.XY[V],Next->XY[V],T):Draw.XY[V];
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
                    RokTrainArmyMesh::Project(P),Draw.UV[V],Color));
            }
            for(uint16 I:Draw.Indices)Indices.Add(static_cast<SlateIndex>(I));
            const auto Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(O.Brushes[O.TextureIndices.FindChecked(Draw.Texture)]);
            FSlateDrawElement::MakeCustomVerts(Elements,++Layer,Handle,Vertices,Indices,nullptr,0,0,
                ESlateDrawEffect::PreMultipliedAlpha|ESlateDrawEffect::NoGamma);
        }
        return Layer;
    }
private:
    TWeakObjectPtr<URokTrainArmyMeshWidget> Owner;
};

void URokTrainArmyMeshWidget::ResetModel()
{
    PaintCount=0; LastPaintFrameIndex=INDEX_NONE; LastPaintDrawCount=0;
    Clip={};TextureIndices.Reset();Textures.Reset();Brushes.Reset();Time=0;bFirstTick=true;
    if(MeshWidget.IsValid())MeshWidget->Invalidate(EInvalidateWidgetReason::Paint);
}
void URokTrainArmyMeshWidget::AdvanceAnimation(float Delta)
{
    if(bFirstTick){bFirstTick=false;return;}
    Time=RokTrainArmyMesh::Advance(Time,Delta,Clip.Duration);
}
TSharedRef<SWidget> URokTrainArmyMeshWidget::RebuildWidget()
{
    return SAssignNew(MeshWidget,SRokTrainArmyMesh,this);
}
void URokTrainArmyMeshWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);MeshWidget.Reset();
}
bool URokTrainArmyMeshWidget::LoadArms(int32 ArmsId)
{
    if (Clip.ArmsId == ArmsId && IsModelLoaded()) return true;
    // The shared C++ troop row's Model is the world model, not Arms.armsShow.
    FString Json, Show; TSharedPtr<FJsonObject> Root;
    const TArray<TSharedPtr<FJsonValue>>* Troops = nullptr;
    if (FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/unity_gameplay_catalog.json")))
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()
        && Root->TryGetArrayField(TEXT("troops"), Troops))
    {
        for (const auto& Value : *Troops)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object) continue;
            const auto Row = Value->AsObject(); double Id;
            if (Row.IsValid() && Row->TryGetNumberField(TEXT("id"), Id) && Id == ArmsId)
            { Row->TryGetStringField(TEXT("show_asset"), Show); break; }
        }
    }
    return LoadArms(ArmsId, Show);
}

bool URokTrainArmyMeshWidget::LoadArms(int32 ArmsId,const FString& ArmsShow)
{
    if(Clip.ArmsId==ArmsId&&Clip.ArmsShow==ArmsShow&&!Clip.Frames.IsEmpty())return true;
    ResetModel();LoadError.Empty();
    auto Fail=[this,ArmsId](const FString& Why){ResetModel();LoadError=Why;UE_LOG(LogTemp,Error,TEXT("TrainArmy mesh %d rejected: %s"),ArmsId,*Why);return false;};
    const FString Relative=FString::Printf(TEXT("Saved/RokUnityMigration/TrainArmy/arms_%d_idle_mesh.json"),ArmsId);
    const FString Filename=FPaths::ProjectDir()/Relative;
    if(ArmsShow.IsEmpty()||IFileManager::Get().FileSize(*Filename)<=0||IFileManager::Get().FileSize(*Filename)>64*1024*1024)return Fail(TEXT("Missing/oversized source mesh or empty armsShow"));
    TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Filename))return Fail(TEXT("Cannot read source mesh"));
    FString Report;TSharedPtr<FJsonObject> ReportRoot;
    if(!FFileHelper::LoadFileToString(Report,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/TrainArmy/export_report.json")))
        ||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Report),ReportRoot)||!ReportRoot.IsValid())return Fail(TEXT("Missing export hash manifest"));
    const TArray<TSharedPtr<FJsonValue>>* Results=nullptr;bool Matched=false;
    if(!ReportRoot->TryGetArrayField(TEXT("results"),Results))return Fail(TEXT("Invalid export hash manifest"));
    for(const auto& Value:*Results)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object)continue;
        const auto Entry=Value->AsObject();double Id;FString Model,Output,Hash,Status;
        if(!Entry.IsValid()||!Entry->TryGetNumberField(TEXT("arms_id"),Id)||Id!=ArmsId)continue;
        if(!Entry->TryGetStringField(TEXT("arms_show"),Model)||Model!=ArmsShow||!Entry->TryGetStringField(TEXT("output"),Output)||Output!=Relative
            ||!Entry->TryGetStringField(TEXT("status"),Status)||Status!=TEXT("exported")||!Entry->TryGetStringField(TEXT("sha256"),Hash)
            ||RokTrainArmyMesh::Sha256(Bytes)!=Hash.ToLower())return Fail(TEXT("Arms identity or mesh SHA256 mismatch"));
        Matched=true;break;
    }
    if(!Matched)return Fail(TEXT("Arms absent from accepted source export"));
    FString Json; FFileHelper::BufferToString(Json,Bytes.GetData(),Bytes.Num());
    RokTrainArmyMesh::FClip Candidate;FString Error;
    if(!RokTrainArmyMesh::Parse(Json,ArmsId,ArmsShow,Candidate,Error))return Fail(Error);
    for(const auto& Source:Candidate.Sources)
    {
        const FString Path=FPaths::ProjectDir()/Source.Key;
        const int64 Size=IFileManager::Get().FileSize(*Path);
        TArray<uint8> SourceBytes;
        if(Size<=0||Size>64*1024*1024||!FFileHelper::LoadFileToArray(SourceBytes,*Path)||RokTrainArmyMesh::Sha256(SourceBytes)!=Source.Value)
            return Fail(FString::Printf(TEXT("Source missing or SHA256 changed: %s"),*Source.Key));
    }
    for(const auto& Frame:Candidate.Frames)for(const auto& Draw:Frame.Draws)
    {
        if(TextureIndices.Contains(Draw.Texture))continue;
        UTexture2D* Texture=FImageUtils::ImportFileAsTexture2D(FPaths::ProjectDir()/Draw.Texture);
        if(!Texture)return Fail(TEXT("Failed to decode source atlas"));
        // Source ProjectSettings.m_ActiveColorSpace=0 (Gamma); preserve PMA atlas samples in that domain.
        Texture->SRGB=false;Texture->Filter=TF_Bilinear;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;Texture->UpdateResource();
        TextureIndices.Add(Draw.Texture,Textures.Num());Textures.Add(Texture);
        FSlateBrush Brush;Brush.SetResourceObject(Texture);Brush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());Brushes.Add(Brush);
    }
    Clip=MoveTemp(Candidate); ++LoadGeneration; LastLoadStartTime=Time;
    UE_LOG(LogTemp,Display,TEXT("TrainArmy mesh loaded: arms=%d show=%s frames=%d duration=%f source_ppu=100 rect_scale=.85 pma=1"),ArmsId,*ArmsShow,Clip.Frames.Num(),Clip.Duration);
    return true;
}
