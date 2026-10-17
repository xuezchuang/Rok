#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RokMailAttachmentMeshData.h"
#include "Styling/SlateBrush.h"
#include "RokMailAttachmentMeshWidget.generated.h"
class UTexture2D;
class SRokMailAttachmentMesh;
UCLASS()
class ROK_API URokMailAttachmentMeshWidget : public UWidget
{
    GENERATED_BODY()
public:
    bool LoadSource();
    void SetAnimationTime(float Seconds);
    void ResetModel();
    void AdvanceAnimation(float Delta);
    uint64 GetLoadGeneration() const { return LoadGeneration; }
    float GetLastLoadStartTime() const { return LastLoadStartTime; }
    bool IsModelLoaded() const { return !Clip.Frames.IsEmpty(); }
    uint64 GetPaintCount() const { return PaintCount; }
    int32 GetLastPaintFrameIndex() const { return LastPaintFrameIndex; }
    int32 GetLastPaintDrawCount() const { return LastPaintDrawCount; }
    int32 GetFrameCount() const { return Clip.Frames.Num(); }
    float GetAnimationTime() const { return Time; }
    float GetAnimationDuration() const { return Clip.Duration; }
    const FString& GetLoadError() const { return LoadError; }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    friend class SRokMailAttachmentMesh;
    RokMailAttachmentMesh::FClip Clip;
    TMap<FString,int32> TextureIndices;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
    TArray<FSlateBrush> Brushes;
    TSharedPtr<SRokMailAttachmentMesh> MeshWidget;
    FString LoadError;
    uint64 LoadGeneration = 0;
    float LastLoadStartTime = 0;
    mutable uint64 PaintCount = 0;
    mutable int32 LastPaintFrameIndex = INDEX_NONE, LastPaintDrawCount = 0;
    float Time = 0;

};
