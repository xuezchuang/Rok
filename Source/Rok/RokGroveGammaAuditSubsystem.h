#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokGroveGammaAuditSubsystem.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapConfigActor;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class FJsonObject;
class FJsonValue;

/** Explicit Game-only source grove zoom audit. Never edits assets or actor transforms. */
UCLASS()
class ROK_API URokGroveGammaAuditSubsystem : public UTickableWorldSubsystem
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
    struct FSourceVertex
    {
        FVector3f Position;
        FVector2f UV0, UV1, UV2;
    };
    struct FInstance
    {
        FString TileTag, IndexTag;
        FTransform SourceTransform;
        FTransform OriginalComponentTransform;
        FTransform OriginalActorTransform;
        TWeakObjectPtr<UMaterialInstanceDynamic> Material;
        FLinearColor OriginalColor = FLinearColor::Transparent;
        float OriginalScale = 0;
        bool bColorSaved = false;
        TWeakObjectPtr<AActor> Actor;
        TWeakObjectPtr<UStaticMeshComponent> Mesh;
    };
    TArray<FSourceVertex> Vertices;
    TArray<FIntVector> Triangles;
    TArray<FInstance> Instances;
    TArray<FString> SampleNames, Failures;
    TArray<float> Dxfs, Scales;
    TArray<FLinearColor> Colors;
    TArray<FString> NativeImageHashes;
    TArray<TSharedPtr<FJsonValue>> Captures, VisibilityRecords;
    TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, FIntPoint>> SavedVisibility;
    TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
    TWeakObjectPtr<ARokWorldMapConfigActor> Config;
    TWeakObjectPtr<UStaticMeshComponent> FocusMesh;
    TSharedPtr<FJsonObject> Source, Native, MeshEvidence, Pending;
    TSharedPtr<FJsonObject> GammaSource, GammaNative, TextureEvidence;
    TArray<TSharedPtr<FJsonValue>> ParameterRestoreRecords;
    TWeakObjectPtr<UTexture2D> RawTexture, OpacityTexture;
    FDelegateHandle ScreenshotHandle;
    FVector2D FocusUnity = FVector2D::ZeroVector;
    FString Directory, PendingPath, SourceHash, NativeHash;
    FString GammaHash, GammaNativeHash, DecodedHash;
    FVector2D OriginalFocus = FVector2D::ZeroVector;
    float OriginalDxf = 0;
    bool bCameraSaved = false, bParametersRestored = true;
    double Elapsed = 0, StageStarted = 0, RequestStarted = 0, WallStarted = 0;
    uint64 StageFrame = 0;
    int32 SampleIndex = -1;
    bool bFinished = false, bIsolate = false, bVisibilityRestored = true, bOwnsTargetWorld = false;
    bool Identity();
    bool LoadSource();
    bool LoadGamma();
    bool BindMaterials();
    bool CaptureTexture(UTexture2D* Texture, const TCHAR* Role, TSharedPtr<FJsonObject>& Out);
    bool Begin();
    bool InspectMesh();
    bool Inspect(TSharedPtr<FJsonObject>& Out);
    void NextSample();
    void Processed();
    bool RestoreVisibility();
    bool RestoreParameters();
    bool Save(bool Complete);
    void Finish(const FString& Failure = FString());
};
