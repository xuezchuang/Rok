#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Styling/SlateBrush.h"
#include "RokHelpSourceTextData.generated.h"
class UTexture2D;
struct FRokHelpGlyphText
{
    FString Key, Text;
    FVector2D Size;
    int32 FontSize=0, Style=0, Alignment=0, Lines=0, Atlas=0;
    TArray<FVector2f> XY, UV;
    TArray<uint16> Indices;
};
/** Per-Help immutable source glyph package. No system font file is loaded. */
UCLASS()
class ROK_API URokHelpSourceTextData : public UObject
{
    GENERATED_BODY()
public:
    bool Load(FString& Error);
    const FRokHelpGlyphText* Find(const FString& Text,int32 FontSize,bool Bold,int32 Alignment,float Width) const;
    const FRokHelpGlyphText* FindKey(const FString& Key) const { return Texts.Find(Key); }
    const FSlateBrush& GetBrush(int32 Index) const { return Brushes[Index]; }
    int32 Num() const { return Texts.Num(); }
private:
    TMap<FString,FRokHelpGlyphText> Texts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
    TArray<FSlateBrush> Brushes;
};
