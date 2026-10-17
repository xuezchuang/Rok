#include "RokBuildingPreviewGlowWidget.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/SlateResourceHandle.h"
#include "Widgets/SLeafWidget.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

class SRokBuildingPreviewGlow : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRokBuildingPreviewGlow) {} SLATE_END_ARGS()
    void Construct(const FArguments&, URokBuildingPreviewGlowWidget* InOwner)
    {
        Owner = InOwner;
        SetCanTick(true);
    }
    // ReferenceInputs.json records a 1440 x 810 projection surface. The
    // owner aligns its origin; this leaf must not infer a modal-frame offset.
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1440, 810); }
    virtual void Tick(const FGeometry&, double, float DeltaSeconds) override
    {
        if (Owner.IsValid() && Owner->bPlaybackActive && Owner->bAutoAdvance && DeltaSeconds > 0)
            Owner->AdvancePlayback(DeltaSeconds);
    }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        if (!Owner.IsValid()) return Layer;
        auto& Widget = *Owner.Get();
        Widget.LastPaintLayerCount = 0;
        if (!Widget.bInitialized || !Widget.bPlaybackActive) return Layer;

        TArray<RokBuildingPreviewGlow::FLayerSample> Samples;
        if (!Widget.Playback.Sample(Widget.Data, Samples) || Samples.Num() != 2)
        {
            Widget.RecordPaintFailure(TEXT("Source glow sampling failed"));
            return Layer;
        }
        const auto& Layers = Widget.Data.GetLayers();
        const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Widget.Brush);
        const float InheritedOpacity = FMath::Clamp(Style.GetColorAndOpacityTint().A, 0.f, 1.f);
        for (int32 Index = 0; Index < 2; ++Index)
        {
#if !UE_BUILD_SHIPPING
            if ((Widget.AuditLayerMask & (1 << Index)) == 0) continue;
#endif
            const auto& Source = Layers[Index];
            const auto& Sample = Samples[Index];
            if (Sample.ScreenVertices.Num() != 4 || Source.UV.Num() != 4 || Source.Triangles.Num() != 6
                || !FMath::IsFinite(Sample.Alpha))
            {
                Widget.RecordPaintFailure(TEXT("Source glow quad is invalid"));
                return Layer;
            }
            TArray<FSlateVertex> Vertices;
            TArray<SlateIndex> Indices;
            Vertices.Reserve(4);
            Indices.Reserve(6);
            // UI-Add is SrcAlpha One. Substrate UI's additive branch multiplies
            // Color by Opacity once, so RGB stays white and only vertex A varies.
            const FColor Color = FLinearColor(1, 1, 1,
                FMath::Clamp(static_cast<float>(Sample.Alpha) * InheritedOpacity, 0.f, 1.f)).ToFColor(true);
            for (int32 V = 0; V < 4; ++V)
            {
                const FVector2D& P = Sample.ScreenVertices[V];
                if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y))
                {
                    Widget.RecordPaintFailure(TEXT("Source glow projection is invalid"));
                    return Layer;
                }
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
                    Geometry.GetAccumulatedRenderTransform(), FVector2f(P),
                    FVector2f(Source.UV[V].X, 1.0 - Source.UV[V].Y), Color));
            }
            for (int32 TriangleIndex : Source.Triangles)
                Indices.Add(static_cast<SlateIndex>(TriangleIndex));
            FSlateDrawElement::MakeCustomVerts(Elements, ++Layer, Handle, Vertices, Indices,
                nullptr, 0, 0, ESlateDrawEffect::NoGamma);
            ++Widget.LastPaintLayerCount;
        }
        ++Widget.PaintCount;
        Widget.LastPaintSourcePhase = Widget.GetSourcePhase();
        return Layer;
    }
private:
    TWeakObjectPtr<URokBuildingPreviewGlowWidget> Owner;
};

bool URokBuildingPreviewGlowWidget::Initialize(const RokBuildingPreviewGlow::FData& InData,
    UMaterialInterface* InMaterial, FString& Error)
{
    Error.Reset();
    bInitialized = false;
    bPlaybackActive = false;
    Data = RokBuildingPreviewGlow::FData();
    GlowMaterial = nullptr;
    Brush = FSlateBrush();
    PaintCount = 0;
    LastPaintLayerCount = 0;
    LastPaintSourcePhase = -1;
#if !UE_BUILD_SHIPPING
    AuditLayerMask = 3;
#endif
    Playback.Reset();
    auto Fail = [&](const FString& Why)
    {
        Error = Why;
        LastError = Error;
        UE_LOG(LogTemp, Error, TEXT("Building preview glow unavailable: %s"), *Why);
        InvalidatePaint();
        return false;
    };
    if (!InData.IsLoaded() || InData.GetLayers().Num() != 2)
        return Fail(TEXT("Validated two-layer glow data missing"));
    for (const auto& Source : InData.GetLayers())
        if (Source.Vertices.Num() != 4 || Source.UV.Num() != 4 || Source.Triangles.Num() != 6)
            return Fail(TEXT("Source glow quad data missing"));
    if (!InMaterial
        || InMaterial->GetPathName() != TEXT("/Game/RokPrototype/UI/Materials/M_RokBuildingPreviewGlowUI.M_RokBuildingPreviewGlowUI")
        || !InMaterial->GetMaterial()
        || InMaterial->GetMaterial()->MaterialDomain != MD_UI
        || InMaterial->GetBlendMode() != BLEND_Additive)
        return Fail(TEXT("Dedicated additive UI material missing or invalid"));
    UTexture* Sprite = nullptr;
    if (!InMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Sprite")), Sprite)
        || !Sprite || Sprite->GetPathName() != TEXT("/Game/RokPrototype/UI/Materials/T_RokBuildingPreviewGlowStar003Raw.T_RokBuildingPreviewGlowStar003Raw"))
        return Fail(TEXT("Dedicated source star003 texture is not bound"));
    UTexture2D* Texture = Cast<UTexture2D>(Sprite);
#if WITH_EDITOR
    // A newly imported texture may report the engine's 32 x 32 default while
    // its platform data compiles. Complete only this texture before checking it.
    if (Texture && Texture->IsCompiling())
    {
        if (!IsInGameThread())
            return Fail(TEXT("Source glow texture compilation requires the game thread"));
        UTexture* RequestedTextures[] = { Texture };
        FTextureCompilingManager::Get().FinishCompilation(RequestedTextures);
        if (Texture->IsCompiling())
            return Fail(FString(TEXT("Source glow texture remains compiling: ")) + Texture->GetPathName());
    }
#endif
    if (!Texture || Texture->GetSizeX() != 256 || Texture->GetSizeY() != 256
        || Texture->SRGB || Texture->Filter != TF_Bilinear
        || Texture->AddressX != TA_Clamp || Texture->AddressY != TA_Clamp
        || Texture->MipGenSettings != TMGS_NoMipmaps)
        return Fail(TEXT("Source glow texture flags or size changed"));

    Data = InData;
    GlowMaterial = InMaterial;
    Brush.SetResourceObject(InMaterial);
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.ImageSize = FVector2D(256, 256);
    LastError.Reset();
    bInitialized = true;
    InvalidatePaint();
    return true;
}

void URokBuildingPreviewGlowWidget::SetPlaybackActive(bool bActive)
{
    if (bActive && !bInitialized)
    {
        RecordPaintFailure(TEXT("Cannot activate uninitialized source glow"));
        return;
    }
    if (bActive && !bPlaybackActive) ResetPlayback();
    bPlaybackActive = bActive;
    // Explicit lifecycle also protects against hidden-window clock drift.
    SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    InvalidatePaint();
}

void URokBuildingPreviewGlowWidget::ResetPlayback()
{
    Playback.Reset();
    LastPaintLayerCount = 0;
    LastPaintSourcePhase = -1;
    InvalidatePaint();
}

bool URokBuildingPreviewGlowWidget::AdvancePlayback(double DeltaSeconds)
{
    if (!bInitialized || !bPlaybackActive || !Playback.Advance(DeltaSeconds)) return false;
    InvalidatePaint();
    return true;
}

bool URokBuildingPreviewGlowWidget::SetPlaybackTime(double Seconds)
{
    if (!bInitialized || !FMath::IsFinite(Seconds) || Seconds < 0) return false;
    Playback.Reset();
    if (!Playback.Advance(Seconds)) return false;
    InvalidatePaint();
    return true;
}

#if !UE_BUILD_SHIPPING
bool URokBuildingPreviewGlowWidget::SetAuditLayerMask(uint8 Mask)
{
    if ((Mask & ~uint8(3)) != 0) return false;
    AuditLayerMask = Mask;
    InvalidatePaint();
    return true;
}
#endif

void URokBuildingPreviewGlowWidget::InvalidatePaint()
{
    if (SlateWidget.IsValid()) SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
}

void URokBuildingPreviewGlowWidget::RecordPaintFailure(const FString& Error) const
{
    if (LastError != Error)
        UE_LOG(LogTemp, Error, TEXT("Building preview glow paint failed: %s"), *Error);
    LastError = Error;
}

TSharedRef<SWidget> URokBuildingPreviewGlowWidget::RebuildWidget()
{
    SetVisibility(bPlaybackActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    return SAssignNew(SlateWidget, SRokBuildingPreviewGlow, this);
}

void URokBuildingPreviewGlowWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    SlateWidget.Reset();
}
