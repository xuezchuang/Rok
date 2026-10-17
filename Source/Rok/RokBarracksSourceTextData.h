#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Styling/SlateBrush.h"
#include "RokBarracksSourceTextData.generated.h"
class UTexture2D;
struct FRokBarracksGlyphText
{
    FString Key, Text;
    FVector2D Size;
    int32 FontSize=0, EffectiveFontSize=0, Style=0, Alignment=0, Lines=0, Atlas=0;
    TArray<FVector2f> XY, UV;
    TArray<uint16> Indices;
    TArray<FColor> Colors;
};
/** Per-barracks immutable source glyph package. No system font file is loaded. */
UCLASS()
class ROK_API URokBarracksSourceTextData : public UObject
{
    GENERATED_BODY()
public:
    bool Load(FString& Error);
    bool LoadLevelHeaders(FString& Error);
    const FRokBarracksGlyphText* Find(const FString& Text,int32 FontSize,bool Bold,int32 Alignment,float Width) const;
    const FRokBarracksGlyphText* FindKey(const FString& Key) const { return Texts.Find(Key); }
    const FSlateBrush& GetBrush(int32 Index) const { return Brushes[Index]; }
    int32 Num() const { return Texts.Num(); }
private:
    bool LoadPackage(FString& Error,bool bLevelHeaders);
    TMap<FString,FRokBarracksGlyphText> Texts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
    TArray<FSlateBrush> Brushes;
};
