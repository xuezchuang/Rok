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
    { return Owner.IsValid()?(Owner->GetPresentationSize()):FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool) const override
    {
        if(!Owner.IsValid()||!Owner->Mesh||!Owner->Data)return Layer;
        const auto& M=*Owner->Mesh;
        const float Scale = Owner->PresentationFontSize > 0 && Owner->FontSize > 0
            ? Owner->PresentationFontSize / Owner->FontSize : 1.0f;
        FVector2D Offset = Owner->PresentationOffset;
        if (Owner->PresentationAlignment != ERokBarracksGlyphAlignment::Source && !M.XY.IsEmpty())
        {
            FBox2D Bounds(ForceInit);
            for (const FVector2f& Point : M.XY)
            {
                Bounds += FVector2D(Point) * Scale;
            }
            const int32 Anchor = static_cast<int32>(Owner->PresentationAlignment) - 1;
            const FVector2D Alignment((Anchor % 3) * 0.5, (Anchor / 3) * 0.5);
            Offset += (Owner->GetPresentationSize() - Bounds.GetSize()) * Alignment - Bounds.Min;
        }
        TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
        for(int32 I=0;I<M.XY.Num();++I)
        {
            const FColor Tint=(FLinearColor::FromSRGBColor(M.Colors[I])*Owner->Color*Owner->PresentationColor*Style.GetColorAndOpacityTint()).ToFColor(true);
            Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),M.XY[I] * Scale + FVector2f(Offset),M.UV[I],Tint));
        }
        for(uint16 I:M.Indices)Indices.Add(static_cast<SlateIndex>(I));
        const auto Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(Owner->Data->GetBrush(M.Atlas));
        FSlateDrawElement::MakeCustomVerts(Elements,Layer,Handle,Vertices,Indices,nullptr,0,0);
        ++Owner->PaintCount;Owner->LastPaintSourceKey=M.Key;return Layer;
    }
private:
    TWeakObjectPtr<URokBarracksSourceTextWidget> Owner;
};
FVector2D URokBarracksSourceTextWidget::GetPresentationSize() const
{
    const FVector2D CapturedSize = Mesh ? Mesh->Size : SourceSize;
    return FVector2D(PresentationTextSize.X > 0 ? PresentationTextSize.X : CapturedSize.X,
        PresentationTextSize.Y > 0 ? PresentationTextSize.Y : CapturedSize.Y);
}
void URokBarracksSourceTextWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (SlateText.IsValid())
    {
        SlateText->Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
    }
}
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
