#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokHospitalExplanationWidget.generated.h"
class URichTextBlock;
class UScrollBox;
UCLASS()
class ROK_API URokHospitalExplanationWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool InitializeExplanation();
    static bool HasExactSourcePaths(const TArray<FString>& Paths);
    FText GetSourceText() const { return SourceText; }
    FText GetRichText() const;
    float GetScrollOffset() const;
    FVector2D GetSourcePosition() const { return Position; }
    FVector2D GetSourceSize() const { return Size; }
    const FString& GetLoadError() const { return LoadError; }
private:
    UPROPERTY(Transient) TObjectPtr<URichTextBlock> Text;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> Scroll;
    UPROPERTY(Transient) FText SourceText;
    FVector2D Position,Size;
    FString LoadError;
};
