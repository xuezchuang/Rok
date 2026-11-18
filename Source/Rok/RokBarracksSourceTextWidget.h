#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RokBarracksSourceTextWidget.generated.h"
class URokBarracksSourceTextData;
class SRokBarracksSourceText;
struct FRokBarracksGlyphText;
/** Presentation placement; Source preserves the captured UGUI glyph positions. */
UENUM(BlueprintType)
enum class ERokBarracksGlyphAlignment : uint8
{
    Source,
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};
/** Readable semantic FText with original UGUI per-glyph geometry. */
UCLASS()
class ROK_API URokBarracksSourceTextWidget : public UWidget
{
    GENERATED_BODY()
public:
    // These presentation overrides never participate in the immutable source glyph key.
    /** Display glyph size in source pixels. Zero retains the captured font size. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barracks|Presentation", meta=(ClampMin="0", UIMin="0"))
    float PresentationFontSize = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barracks|Presentation")
    FLinearColor PresentationColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barracks|Presentation")
    ERokBarracksGlyphAlignment PresentationAlignment = ERokBarracksGlyphAlignment::Source;
    /** Zero on an axis retains the captured source text rectangle on that axis. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barracks|Presentation", meta=(ClampMin="0"))
    FVector2D PresentationTextSize = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Barracks|Presentation")
    FVector2D PresentationOffset = FVector2D::ZeroVector;
    virtual void SynchronizeProperties() override;
    void Configure(URokBarracksSourceTextData* InData,int32 InFontSize,bool InBold,int32 InAlignment,FVector2D InSize,FLinearColor InColor);
    bool SetText(const FText& InText);
    UFUNCTION(BlueprintPure,Category="Barracks") FText GetText() const { return Text; }
    bool HasSourceMesh() const { return Mesh!=nullptr; }
    const FString& GetSourceKey() const { return SourceKey; }
    uint64 GetPaintCount() const { return PaintCount; }
    const FString& GetLastPaintSourceKey() const { return LastPaintSourceKey; }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    friend class SRokBarracksSourceText;
    FVector2D GetPresentationSize() const;
    UPROPERTY(Transient) TObjectPtr<URokBarracksSourceTextData> Data;
    UPROPERTY(Transient) FText Text;
    const FRokBarracksGlyphText* Mesh=nullptr;
    FString SourceKey;
    int32 FontSize=0, Alignment=0;
    bool Bold=false;
    FVector2D SourceSize;
    FLinearColor Color=FLinearColor::White;
    TSharedPtr<SRokBarracksSourceText> SlateText;
    mutable uint64 PaintCount=0;
    mutable FString LastPaintSourceKey;
};
