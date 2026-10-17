#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokGroveCenterScaleAuditSubsystem.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapConfigActor;
class UStaticMeshComponent;
class FJsonObject;
class FJsonValue;

/** Explicit Game-only source grove zoom audit. Never edits assets or actor transforms. */
UCLASS()
class ROK_API URokGroveCenterScaleAuditSubsystem : public UTickableWorldSubsystem
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
        TWeakObjectPtr<AActor> Actor;
        TWeakObjectPtr<UStaticMeshComponent> Mesh;
    };
    TArray<FSourceVertex> Vertices;
    TArray<FIntVector> Triangles;
    TArray<FInstance> Instances;
    TArray<FString> SampleNames, Failures;
    TArray<float> Dxfs, Scales;
    TArray<TSharedPtr<FJsonValue>> Captures, VisibilityRecords;
    TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, FIntPoint>> SavedVisibility;
    TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
    TWeakObjectPtr<ARokWorldMapConfigActor> Config;
    TWeakObjectPtr<UStaticMeshComponent> FocusMesh;
    TSharedPtr<FJsonObject> Source, Native, MeshEvidence, Pending;
    FDelegateHandle ScreenshotHandle;
    FVector2D FocusUnity = FVector2D::ZeroVector;
    FString Directory, PendingPath, SourceHash, NativeHash;
    double Elapsed = 0, StageStarted = 0, RequestStarted = 0, WallStarted = 0;
    uint64 StageFrame = 0;
    int32 SampleIndex = -1;
    bool bFinished = false, bIsolate = false, bVisibilityRestored = true, bOwnsTargetWorld = false;
    bool Identity();
    bool LoadSource();
    bool Begin();
    bool InspectMesh();
    bool Inspect(TSharedPtr<FJsonObject>& Out);
    void NextSample();
    void Processed();
    bool RestoreVisibility();
    bool Save(bool Complete);
    void Finish(const FString& Failure = FString());
};
