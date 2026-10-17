#include "RokBuildingSourcePreviewWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

void URokBuildingSourcePreviewWidget::ResetPreview()
{
    Image=nullptr;RawTexture=nullptr;Layout=RokBuildingSourcePreview::FLayout();LoadError.Reset();
    if(WidgetTree)WidgetTree->RootWidget=nullptr;SetVisibility(ESlateVisibility::Collapsed);
}
bool URokBuildingSourcePreviewWidget::InitializePreview(const FString& Model,UTexture2D* Texture,const FString& MetadataPath)
{
    ResetPreview();
    auto Fail=[&](const FString& Error){LoadError=Error;UE_LOG(LogTemp,Warning,TEXT("Building source preview unavailable: %s"),*Error);return false;};
    if(!RokBuildingSourcePreview::LoadLayout(Model,MetadataPath.IsEmpty()?RokBuildingSourcePreview::MetadataFile():MetadataPath,Layout,LoadError))return Fail(LoadError);
    // Caller supplies an import whose raw source pixels have been independently verified.
    // Editor async platform compilation exposes the engine's default texture dimensions
    // until this particular imported texture finishes. Complete only that texture before
    // enforcing source semantics; never accept or substitute the default resource.
    const auto Describe=[&]()
    {
        if(!Texture)return FString(TEXT("texture=null"));
        bool bCompiling=false;
#if WITH_EDITOR
        bCompiling=Texture->IsCompiling();
#endif
        return FString::Printf(TEXT("texture=%s size=%dx%d expected=%dx%d srgb=%d address=%d,%d mips=%d resource=%d compiling=%d"),
            *Texture->GetPathName(),Texture->GetSizeX(),Texture->GetSizeY(),Layout.TextureSize.X,Layout.TextureSize.Y,
            Texture->SRGB,static_cast<int32>(Texture->AddressX),static_cast<int32>(Texture->AddressY),Texture->GetNumMips(),Texture->GetResource()!=nullptr,bCompiling);
    };
#if WITH_EDITOR
    if(Texture&&Texture->IsCompiling())
    {
        UE_LOG(LogTemp,Display,TEXT("Building source preview waiting for imported texture: %s"),*Describe());
        if(!IsInGameThread())return Fail(TEXT("Source texture compilation must complete on the game thread"));
        UTexture* RequestedTextures[]={Texture};
        FTextureCompilingManager::Get().FinishCompilation(RequestedTextures);
        UE_LOG(LogTemp,Display,TEXT("Building source preview compilation completed: %s"),*Describe());
        if(Texture->IsCompiling())return Fail(FString(TEXT("Raw UI texture remains compiling: "))+Describe());
    }
#endif
    if(!Texture||Texture->GetSizeX()!=Layout.TextureSize.X||Texture->GetSizeY()!=Layout.TextureSize.Y||!Texture->SRGB||Texture->AddressX!=TA_Clamp||Texture->AddressY!=TA_Clamp||Texture->GetNumMips()!=1)
    {
        const FString Error=FString(TEXT("Raw UI texture missing or import semantics differ: "))+Describe();
        Layout=RokBuildingSourcePreview::FLayout();return Fail(Error);
    }
    RawTexture=Texture;
    auto* Root=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("BuildingSourcePreviewRoot"));WidgetTree->RootWidget=Root;
    Image=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("BuildingSourcePreviewImage"));
    FSlateBrush Brush;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.SetResourceObject(Texture);Brush.ImageSize=Layout.ImageSize;
    Image->SetBrush(Brush);Image->SetColorAndOpacity(FLinearColor::White);Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* ImageSlot=Root->AddChildToCanvas(Image);ImageSlot->SetPosition(Layout.ImagePosition);ImageSlot->SetSize(Layout.ImageSize);
    SetVisibility(ESlateVisibility::HitTestInvisible);return true;
}
