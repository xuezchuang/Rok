#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RokBuildingPreviewGlowData.h"
#include "Styling/SlateBrush.h"
#include "RokBuildingPreviewGlowWidget.generated.h"

class UMaterialInterface;
class SRokBuildingPreviewGlow;

/** Source quad presentation for UI_Effect_BuildShow. Its local origin is the
 *  native projection's screen origin; the owning window supplies placement. */
UCLASS()
class ROK_API URokBuildingPreviewGlowWidget : public UWidget
{
    GENERATED_BODY()
public:
    bool Initialize(const RokBuildingPreviewGlow::FData& InData, UMaterialInterface* InMaterial, FString& Error);
    void SetPlaybackActive(bool bActive);
    void ResetPlayback();
    bool AdvancePlayback(double DeltaSeconds);
    bool SetPlaybackTime(double Seconds);
    void SetAutoAdvance(bool bEnabled) { bAutoAdvance = bEnabled; }

    bool IsInitialized() const { return bInitialized; }
    bool IsPlaybackActive() const { return bPlaybackActive; }
    bool IsAutoAdvance() const { return bAutoAdvance; }
#if !UE_BUILD_SHIPPING
    // Bit 0/1 selects the two source siblings independently for visual audit.
    bool SetAuditLayerMask(uint8 Mask);
    uint8 GetAuditLayerMask() const { return AuditLayerMask; }
#endif
    double GetPlaybackTime() const { return Playback.GetTime(); }
    double GetSourcePhase() const { return FMath::Fmod(Playback.GetTime(), 2.0); }
    uint64 GetPaintCount() const { return PaintCount; }
    int32 GetLastPaintLayerCount() const { return LastPaintLayerCount; }
    double GetLastPaintSourcePhase() const { return LastPaintSourcePhase; }
    const FString& GetLastError() const { return LastError; }

    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    friend class SRokBuildingPreviewGlow;
    void InvalidatePaint();
    void RecordPaintFailure(const FString& Error) const;

    RokBuildingPreviewGlow::FData Data;
    RokBuildingPreviewGlow::FPlayback Playback;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GlowMaterial;
    FSlateBrush Brush;
    TSharedPtr<SRokBuildingPreviewGlow> SlateWidget;
    bool bInitialized = false;
    bool bPlaybackActive = false;
    bool bAutoAdvance = true;
#if !UE_BUILD_SHIPPING
    uint8 AuditLayerMask = 3;
#endif
    mutable uint64 PaintCount = 0;
    mutable int32 LastPaintLayerCount = 0;
    mutable double LastPaintSourcePhase = -1;
    mutable FString LastError;
};
