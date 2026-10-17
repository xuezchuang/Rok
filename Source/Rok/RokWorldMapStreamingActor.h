#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RokWorldMapRuntimeDataAsset.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "RokWorldMapStreamingActor.generated.h"

class ARokWorldMapCameraPawn;
struct FStreamableHandle;
class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;

/** Read-only facts from actual HISM components, never a count copied from JSON. */
struct FRokWorldMapStreamingDiagnosticState
{
	bool bHasRequestedState = false;
	bool bPendingLoad = false;
	int32 Lod = INDEX_NONE;
	FIntPoint CenterChunk = FIntPoint::ZeroValue;
	uint64 Generation = 0;
	int32 ComponentCount = 0;
	int32 VisibleComponentCount = 0;
	int32 InstanceCount = 0;
	TArray<FName> ComponentNames;
	int32 RegisteredComponentCount = 0;
	int32 RegisteredInstanceCount = 0;
	TArray<FName> RegisteredComponentNames;
	bool bRegisteredMatchesActive = false;
};

/** Complete identity of one HISM batch. Material slot order is significant. */
USTRUCT()
struct FRokWorldMapStreamingBatchKey
{
	GENERATED_BODY()

	FIntPoint ChunkKey = FIntPoint::ZeroValue;
	FSoftObjectPath MeshPath;
	TArray<FSoftObjectPath> MaterialPaths;
	int32 TranslucencySortPriority = 0;
	bool bTree = false;
	int32 RenderFlags = 0;

	bool operator==(const FRokWorldMapStreamingBatchKey& Other) const;
};

ROK_API uint32 GetTypeHash(const FRokWorldMapStreamingBatchKey& Key);

/** Transient identity of one actual fixed-camera probe component. */
struct FRokWorldFixedProbeBatch
{
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
    FRokWorldMapStreamingBatchKey Key;
};

UCLASS()
class ROK_API ARokWorldMapStreamingActor : public AActor
{
	GENERATED_BODY()

public:
	ARokWorldMapStreamingActor();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr int32 ResidentChunkRadius = 1;
	static constexpr int32 ResidentChunkCount = 9;
	static constexpr float TreeScaleMaterialChangeTolerance = 0.001f;

	/** Always returns the 3x3 logical neighborhood without edge clamping. */
	static TArray<FIntPoint> BuildResidentChunkKeys(FIntPoint CenterChunk);

	static bool DoesChunkIntersectMap(
		FIntPoint ChunkKey,
		int32 ChunkTileSpan,
		int32 MapWidthTiles,
		int32 MapHeightTiles);

	static bool DoesTileIntersectChunk(
		const FRokWorldMapTileRecord& Tile,
		FIntPoint ChunkKey,
		int32 ChunkTileSpan);

	static FIntPoint ComputeCenterChunk(
		FVector2D ViewCenterUnity,
		float TileSizeUnityUnits,
		int32 ChunkTileSpan);

	static bool HasPositiveDeterminant(const FTransform& Transform);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TObjectPtr<URokWorldMapRuntimeDataAsset> RuntimeData;

	/** Only the named diagnostic map may inspect source LOD0/1 before full visual promotion. */
	UPROPERTY(EditAnywhere, Category = "Rok|World Map|Diagnostic")
	bool bEnableScopedDiagnosticLod01 = false;

	FRokWorldMapStreamingDiagnosticState GetDiagnosticState() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class ARokWorldInstancedMaterialDiagnosticGameMode;
	/** Only the named non-shipping fixed-camera probe may override source LOD. */
	bool SetFixedCameraProbeLod(int32 LodOrIndexNone);
	bool GetFixedCameraProbeBatches(TArray<FRokWorldFixedProbeBatch>& OutBatches) const;

	struct FPreparedBatch
	{
		FRokWorldMapStreamingBatchKey Key;
		TArray<FTransform> InstanceTransforms;
		bool bContainsApprovedSourceMirror = false;
	};

	struct FPreparedBuild
	{
		int32 Lod = INDEX_NONE;
		FIntPoint CenterChunk = FIntPoint::ZeroValue;
		float TreeScale = 1.0f;
		int32 NonEmptyChunkCount = 0;
		int32 InstanceCount = 0;
		FString CanonicalPayloadSha256;
		TArray<FPreparedBatch> Batches;
		TArray<FSoftObjectPath> AssetsToLoad;
	};

	UPROPERTY(VisibleAnywhere, Category = "Rok|World Map|Streaming")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<ARokWorldMapCameraPawn> CameraPawn;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ActiveComponents;
	int32 FixedCameraProbeLod = INDEX_NONE;
	TArray<FRokWorldMapStreamingBatchKey> FixedCameraProbeActiveKeys;

	TSharedPtr<FStreamableHandle> PendingLoadHandle;
	uint64 RequestedGeneration = 0;
	int32 LastRequestedLod = INDEX_NONE;
	FIntPoint LastRequestedCenterChunk = FIntPoint::ZeroValue;
	float LastRequestedTreeScale = 1.0f;
	FString LastRequestedPayloadSha256;
	bool bHasRequestedState = false;
	bool bLoggedNotStaticReady = false;

	void ResolveCameraPawn();
	void EvaluateStreamingState();
	bool PrepareBuild(
		int32 Lod,
		FIntPoint CenterChunk,
		float TreeScale,
		FPreparedBuild& OutBuild) const;
	void RequestPreparedBuild(FPreparedBuild&& Build);
	void CompletePreparedBuild(
		uint64 Generation,
		const TSharedRef<FPreparedBuild, ESPMode::ThreadSafe>& Build);
	void DestroyActiveComponents();
	bool IsScopedDiagnosticLod01Allowed() const;
	bool IsFixedCameraProbeAllowed() const;
};
