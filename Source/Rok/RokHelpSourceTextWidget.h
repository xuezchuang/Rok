#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RokHelpSourceTextWidget.generated.h"
class URokHelpSourceTextData;
class SRokHelpSourceText;
struct FRokHelpGlyphText;
/** Readable semantic FText with original UGUI per-glyph geometry. */
UCLASS()
class ROK_API URokHelpSourceTextWidget : public UWidget
{
    GENERATED_BODY()
public:
    /** Exact source glyph key and paint values serialized in a Help WBP. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Help|Source Glyph") int32 SourceFontSize=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Help|Source Glyph") bool bSourceBold=false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Help|Source Glyph") int32 SourceAlignment=0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Help|Source Glyph") FVector2D SourceGlyphSize=FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Help|Source Glyph") FLinearColor SourceGlyphColor=FLinearColor::White;
    void Configure(URokHelpSourceTextData* InData,int32 InFontSize,bool InBold,int32 InAlignment,FVector2D InSize,FLinearColor InColor);
    /** Binds runtime glyph data while preserving the WBP-authored glyph key and color. */
    void BindData(URokHelpSourceTextData* InData);
    bool SetText(const FText& InText);
    UFUNCTION(BlueprintPure,Category="Help") FText GetText() const { return Text; }
    bool HasSourceMesh() const { return Mesh!=nullptr; }
    const FString& GetSourceKey() const { return SourceKey; }
    uint64 GetPaintCount() const { return PaintCount; }
    const FString& GetLastPaintSourceKey() const { return LastPaintSourceKey; }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    friend class SRokHelpSourceText;
    UPROPERTY(Transient) TObjectPtr<URokHelpSourceTextData> Data;
    UPROPERTY(Transient) FText Text;
    const FRokHelpGlyphText* Mesh=nullptr;
    FString SourceKey;
    int32 FontSize=0, Alignment=0;
    bool Bold=false;
    FVector2D SourceSize;
    FLinearColor Color=FLinearColor::White;
    TSharedPtr<SRokHelpSourceText> SlateText;
    mutable uint64 PaintCount=0;
    mutable FString LastPaintSourceKey;
};
