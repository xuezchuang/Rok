#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "RokAcademyPresentationAuditSubsystem.generated.h"

class AActor;
class APlayerController;
class ARokCitySpriteActor;
class UMaterialInstanceDynamic;
class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class FJsonObject;

/** A transient, source-camera Academy rendering audit. It never builds a city building. */
UCLASS()
class ROK_API URokAcademyPresentationAuditSubsystem : public UTickableWorldSubsystem
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
    bool Begin();
    bool Capture(const TCHAR* Name, const FLinearColor& SourceLighting, const FLinearColor& SourceClear);
    bool CaptureGameAppContext();
    bool RecordMesh(TSharedPtr<FJsonObject>& Out);
    bool Restore();
    void Finish(const FString& Error = FString());

    FString RunDirectory;
    FString SourceCatalogSha;
    FString NativeRenderSha;
    FString NativeVerifySha;
    FString MeshManifestSha;
    FString MeshAssetSha;
    FString MaterialAssetSha;
    FString BackgroundMaterialAssetSha;
    FString EconomicSnapshot;
    TArray<TSharedPtr<FJsonObject>> Frames;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> MeshRecord;
    TSharedPtr<FJsonObject> ContextRecord;
    TWeakObjectPtr<ARokCitySpriteActor> TemporaryActor;
    TWeakObjectPtr<APlayerController> PlayerController;
    TWeakObjectPtr<AActor> SavedViewTarget;
    TStrongObjectPtr<UStaticMesh> SourceMesh;
    TStrongObjectPtr<UMaterialInstanceDynamic> AuditMaterial;
    TStrongObjectPtr<UMaterialInstanceDynamic> BackgroundMaterial;
    TWeakObjectPtr<UStaticMeshComponent> BackgroundComponent;
    TStrongObjectPtr<USceneCaptureComponent2D> CaptureComponent;
    TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget;
    FTransform SavedCameraTransform;
    float SavedCameraFov = 0.0f;
    int32 InitialActorCount = 0;
    int32 InitialTemporaryActorCount = 0;
    int32 Stage = -1;
    double Elapsed = 0.0;
    double StageStart = 0.0;
    bool bFinished = false;
    bool bRestored = false;
};
