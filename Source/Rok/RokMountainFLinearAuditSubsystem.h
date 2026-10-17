#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RokWorldMapCameraPawn.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokMountainFLinearAuditSubsystem.generated.h"

class FJsonObject;
class IConsoleVariable;

UCLASS()
class ROK_API URokMountainFLinearAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
    TWeakObjectPtr<UStaticMeshComponent> Body;
    TStrongObjectPtr<UMaterialInterface> OriginalMaterial;
    TStrongObjectPtr<UMaterialInstanceDynamic> AuditMaterial;
    TStrongObjectPtr<UTexture2D> Texture;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    TStrongObjectPtr<USceneCaptureComponent2D> Capture;
    TArray<FLinearColor> Colors;
    TArray<FString> Names;
    TArray<TSharedPtr<FJsonObject>> Frames;
    TArray<FString> Failures;
    FString Directory;
    FString SourceSha, ProductSha, TextureMip0Sha;
    TArray<FString> PlatformMipShas;
    FString GeometryPath, GeometrySha;
    FVector2D SavedCenter = FVector2D::ZeroVector;
    float SavedDxf = 0;
    int32 SavedForcedLod = 0;
    IConsoleVariable* PreExposureCvar = nullptr;
    float SavedPreExposure = 0;
    bool bPreExposureChanged = false;
    bool bCameraSaved = false;
    bool bMaterialSaved = false;
    bool bLodSaved = false;
    bool bFinished = false;
    int32 Stage = -1;
    uint64 StageFrame = 0;
    double Elapsed = 0;
    void Begin();
    void CaptureStage();
    void Finish(const FString& Error = FString());
    bool Restore();
};
