#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokHospitalExplanationWidget.generated.h"
class URichTextBlock;
class UScrollBox;
class UBorder;
class USizeBox;
UCLASS()
class ROK_API URokHospitalExplanationWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static TSubclassOf<URokHospitalExplanationWidget> ResolvePresentationClass();
    bool InitializeExplanation();
    static bool HasExactSourcePaths(const TArray<FString>& Paths);
    FText GetSourceText() const { return SourceText; }
    FText GetRichText() const;
    float GetScrollOffset() const;
    FVector2D GetSourcePosition() const { return Position; }
    FVector2D GetSourceSize() const { return Size; }
    const FString& GetLoadError() const { return LoadError; }
private:
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<URichTextBlock> HospitalExplanationText;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> HospitalExplanationScroll;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> HospitalExplanationInset;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> HospitalExplanationTextWidth;
    UPROPERTY(Transient) FText SourceText;
    FVector2D Position,Size;
    FString LoadError;
};
