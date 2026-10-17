#include "RokBarracksSourceTextWidget.h"
#include "RokBarracksSourceTextData.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SLeafWidget.h"

class SRokBarracksSourceText : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRokBarracksSourceText) {} SLATE_END_ARGS()
    void Construct(const FArguments&,URokBarracksSourceTextWidget* InOwner) { Owner=InOwner; }
    virtual FVector2D ComputeDesiredSize(float) const override
    { return Owner.IsValid()?(Owner->Mesh?Owner->Mesh->Size:Owner->SourceSize):FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool) const override
    {
        if(!Owner.IsValid()||!Owner->Mesh||!Owner->Data)return Layer;
        const auto& M=*Owner->Mesh;
        TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
        for(int32 I=0;I<M.XY.Num();++I)
        {
            const FColor Tint=(FLinearColor::FromSRGBColor(M.Colors[I])*Owner->Color*Style.GetColorAndOpacityTint()).ToFColor(true);
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),M.XY[I],M.UV[I],Tint));
        }
        for(uint16 I:M.Indices)Indices.Add(static_cast<SlateIndex>(I));
        const auto Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(Owner->Data->GetBrush(M.Atlas));
        FSlateDrawElement::MakeCustomVerts(Elements,Layer,Handle,Vertices,Indices,nullptr,0,0);
        ++Owner->PaintCount;Owner->LastPaintSourceKey=M.Key;return Layer;
    }
private:
    TWeakObjectPtr<URokBarracksSourceTextWidget> Owner;
};
void URokBarracksSourceTextWidget::Configure(URokBarracksSourceTextData* InData,int32 InFontSize,bool InBold,int32 InAlignment,FVector2D InSize,FLinearColor InColor)
{Data=InData;FontSize=InFontSize;Bold=InBold;Alignment=InAlignment;SourceSize=InSize;Color=InColor;}
bool URokBarracksSourceTextWidget::SetText(const FText& InText)
{
    Text=InText;Mesh=nullptr;SourceKey.Empty();PaintCount=0;LastPaintSourceKey.Empty();
    if(!Text.IsEmpty()&&Data)Mesh=Data->Find(Text.ToString(),FontSize,Bold,Alignment,static_cast<float>(SourceSize.X));
    if(Mesh)SourceKey=Mesh->Key;
    if(SlateText.IsValid())SlateText->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    return Text.IsEmpty()||Mesh!=nullptr;
}
TSharedRef<SWidget> URokBarracksSourceTextWidget::RebuildWidget(){return SAssignNew(SlateText,SRokBarracksSourceText,this);}
void URokBarracksSourceTextWidget::ReleaseSlateResources(bool bReleaseChildren){Super::ReleaseSlateResources(bReleaseChildren);SlateText.Reset();}
