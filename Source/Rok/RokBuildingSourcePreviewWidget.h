#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokBuildingSourcePreviewData.h"
#include "RokBuildingSourcePreviewWidget.generated.h"
class UTexture2D;
class UImage;
/** Explicit local scene-presentation model only; does not infer player civilization. */
UCLASS()
class ROK_API URokBuildingSourcePreviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool InitializePreview(const FString& ExplicitModelId,UTexture2D* VerifiedRawTexture,const FString& MetadataPath=FString());
    void ResetPreview();
    bool IsPreviewReady() const { return Image!=nullptr; }
    const FString& GetLoadError() const { return LoadError; }
    const FString& GetPresentationModel() const { return Layout.ModelId; }
    FVector2D GetSourcePosition() const { return Layout.Position; }
    FVector2D GetSourceSize() const { return Layout.Size; }
private:
    UPROPERTY(Transient) TObjectPtr<UTexture2D> RawTexture;
    UPROPERTY(Transient) TObjectPtr<UImage> Image;
    RokBuildingSourcePreview::FLayout Layout;
    FString LoadError;
};
