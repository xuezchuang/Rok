#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokMountainFGammaAuditSubsystem.generated.h"

class ARokWorldMapCameraPawn;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
class FJsonObject;
class FJsonValue;
class IConsoleVariable;
class FViewport;
class ISlateViewport;
struct FRokMountainFGpuPackedCapture;

UCLASS()
class ROK_API URokMountainFGammaAuditSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
    struct FBody
    {
        TWeakObjectPtr<UStaticMeshComponent> Mesh;
        TStrongObjectPtr<UMaterialInterface> OriginalMaterial;
        TStrongObjectPtr<UMaterialInstanceDynamic> AuditMaterial;
        FString Key;
    };
    TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
    TWeakObjectPtr<UStaticMeshComponent> Representative;
    TWeakObjectPtr<UTexture2D> Texture;
    TArray<FBody> Bodies;
    TArray<FLinearColor> Colors;
    TArray<FString> Names;
    TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, FIntPoint>> SavedVisibility;
    TArray<TSharedPtr<FJsonValue>> VisibilityRecords, BindingRecords, MipRecords, Captures;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FJsonObject> TextureState;
    FDelegateHandle ScreenshotHandle;
    FDelegateHandle RawScreenshotHandle;
    FDelegateHandle SdrViewportRenderedHandle;
    TSharedPtr<ISlateViewport> SdrViewportOwner; // Game thread only; keeps borrowed render-thread pointer alive.
    TSharedPtr<FRokMountainFGpuPackedCapture, ESPMode::ThreadSafe> SdrRawReadback;
    uint64 SdrRawCaptureSerial = 0;
    uint64 SdrRawRequestGameFrame = 0;
    double SdrRawRequestSeconds = 0;
    bool bSdrScreenshotProcessed = false;
    TStrongObjectPtr<USceneCaptureComponent2D> SdrHdrCapture;
    TStrongObjectPtr<UTextureRenderTarget2D> SdrHdrTarget;
    FIntRect SdrEffectiveViewRect;
    FString SdrRepresentativeKey;
    TArray<FString> SdrHiddenBodyKeys;
    FTransform SdrLockedCameraTransform;
    float SdrFramingInitialDxf = 0, SdrLockedDxf = 0, SdrLockedFov = 0;
    int32 SdrFramingSteps = 0;
    int32 SdrConfiguredLutDimension = 0;
    bool bSdrFramingLocked = false;
    int32 SdrSavedForcedLod = 0;
    bool bSdrLodSaved = false, bSdrTonemapperSaved = false, bSdrOriginalTonemapper = false;
    bool bSdrViewportFlagsSaved = false, bSdrOriginalAntiAliasing = false;
    bool bSdrOriginalTemporalAA = false, bSdrOriginalEyeAdaptation = false, bSdrOriginalMotionBlur = false;
    bool bSdrOutputDiagnostic = false, bSdrPreExposureChanged = false;
    IConsoleVariable* SdrPreExposureCvar = nullptr;
    float SdrSavedPreExposure = 0;
    FString Directory, PendingPath, ProductManifestSha, SourceDataSha, SourceRenderSha, SourceVerifySha;
    double Elapsed = 0, StageStart = 0;
    uint64 StageFrame = 0;
    int32 Stage = -1;
    bool bFinished = false, bIsolate = false;
    FVector2D SavedViewCenter = FVector2D::ZeroVector;
    float SavedDxf = 0;
    bool bCameraSaved = false;
    bool Begin();
    bool Inspect(TSharedPtr<FJsonObject>& Out, bool& bFramed);
    bool LoadSourceAndProduct();
    bool BindFreshActors();
    bool CheckTextureMips();
    bool RestoreRuntimeState();
    void SetStageColor();
    void Processed();
    void CapturedRaw(int32 Width, int32 Height, const TArray<FColor>& Pixels);
    void SdrViewportRendered(FViewport* Viewport);
    void PollSdrRawReadback();
    bool StoreSdrRawReadback();
    void CompleteCapture();
    bool SetupSdrDiagnostic();
    bool CaptureSdrHdr(TSharedPtr<FJsonObject>& Frame);
    bool RecordSdrViewport(TSharedPtr<FJsonObject>& State);
    void Finish(const FString& Failure = FString());
};
