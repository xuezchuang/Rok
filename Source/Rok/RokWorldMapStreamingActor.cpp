#include "RokWorldMapStreamingActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Engine/AssetManager.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokWorldMapCameraPawn.h"
#include "Templates/TypeHash.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogRokWorldMapStreaming, Log, All);

namespace
{
bool HasRenderFlag(int32 RenderFlags, ERokWorldMapRenderFlag Flag)
{
	return (RenderFlags & static_cast<int32>(Flag)) != 0;
}

bool IsFiniteTransformForStreaming(const FTransform& Transform)
{
	const FVector Translation = Transform.GetTranslation();
	const FVector Scale = Transform.GetScale3D();
	const FQuat Rotation = Transform.GetRotation();
	return !Translation.ContainsNaN()
		&& !Scale.ContainsNaN()
		&& FMath::IsFinite(Rotation.X)
		&& FMath::IsFinite(Rotation.Y)
		&& FMath::IsFinite(Rotation.Z)
		&& FMath::IsFinite(Rotation.W);
}
}

bool FRokWorldMapStreamingBatchKey::operator==(
	const FRokWorldMapStreamingBatchKey& Other) const
{
	return ChunkKey == Other.ChunkKey
		&& MeshPath == Other.MeshPath
		&& MaterialPaths == Other.MaterialPaths
		&& TranslucencySortPriority == Other.TranslucencySortPriority
		&& bTree == Other.bTree
		&& RenderFlags == Other.RenderFlags;
}

uint32 GetTypeHash(const FRokWorldMapStreamingBatchKey& Key)
{
	uint32 Hash = GetTypeHash(Key.ChunkKey);
	Hash = HashCombineFast(Hash, GetTypeHash(Key.MeshPath));
	for (const FSoftObjectPath& MaterialPath : Key.MaterialPaths)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(MaterialPath));
	}
	Hash = HashCombineFast(
		Hash,
		GetTypeHash(Key.TranslucencySortPriority));
	Hash = HashCombineFast(Hash, GetTypeHash(Key.bTree));
	Hash = HashCombineFast(Hash, GetTypeHash(Key.RenderFlags));
	return Hash;
}

ARokWorldMapStreamingActor::ARokWorldMapStreamingActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	// The streamed HISM children are Static. UE aborts their attachment when
	// the parent is Movable, even though the components still register/count.
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);
}

void ARokWorldMapStreamingActor::BeginPlay()
{
	Super::BeginPlay();
	// Also cover a saved diagnostic actor whose serialized root predates the CDO.
	if (SceneRoot && SceneRoot->Mobility != EComponentMobility::Static)
	{
		SceneRoot->SetMobility(EComponentMobility::Static);
	}
	ResolveCameraPawn();
	EvaluateStreamingState();
}

void ARokWorldMapStreamingActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	++RequestedGeneration;
	if (PendingLoadHandle.IsValid())
	{
		PendingLoadHandle->CancelHandle();
		PendingLoadHandle.Reset();
	}
	DestroyActiveComponents();
	Super::EndPlay(EndPlayReason);
}

void ARokWorldMapStreamingActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsValid(CameraPawn.Get()))
	{
		ResolveCameraPawn();
	}
	EvaluateStreamingState();
}

TArray<FIntPoint> ARokWorldMapStreamingActor::BuildResidentChunkKeys(
	FIntPoint CenterChunk)
{
	TArray<FIntPoint> Result;
	Result.Reserve(ResidentChunkCount);
	for (int32 YOffset = -ResidentChunkRadius;
		YOffset <= ResidentChunkRadius;
		++YOffset)
	{
		for (int32 XOffset = -ResidentChunkRadius;
			XOffset <= ResidentChunkRadius;
			++XOffset)
		{
			Result.Emplace(
				CenterChunk.X + XOffset,
				CenterChunk.Y + YOffset);
		}
	}
	return Result;
}

bool ARokWorldMapStreamingActor::DoesChunkIntersectMap(
	FIntPoint ChunkKey,
	int32 ChunkTileSpan,
	int32 MapWidthTiles,
	int32 MapHeightTiles)
{
	if (ChunkTileSpan <= 0
		|| MapWidthTiles <= 0
		|| MapHeightTiles <= 0)
	{
		return false;
	}
	const int64 MinimumX =
		static_cast<int64>(ChunkKey.X) * ChunkTileSpan;
	const int64 MinimumY =
		static_cast<int64>(ChunkKey.Y) * ChunkTileSpan;
	const int64 MaximumX = MinimumX + ChunkTileSpan;
	const int64 MaximumY = MinimumY + ChunkTileSpan;
	return MaximumX > 0
		&& MaximumY > 0
		&& MinimumX < MapWidthTiles
		&& MinimumY < MapHeightTiles;
}

bool ARokWorldMapStreamingActor::DoesTileIntersectChunk(
	const FRokWorldMapTileRecord& Tile,
	FIntPoint ChunkKey,
	int32 ChunkTileSpan)
{
	if (ChunkTileSpan <= 0)
	{
		return false;
	}
	const int64 MinimumX =
		static_cast<int64>(ChunkKey.X) * ChunkTileSpan;
	const int64 MinimumY =
		static_cast<int64>(ChunkKey.Y) * ChunkTileSpan;
	const int64 MaximumX = MinimumX + ChunkTileSpan;
	const int64 MaximumY = MinimumY + ChunkTileSpan;
	return Tile.TileX >= MinimumX
		&& Tile.TileX < MaximumX
		&& Tile.TileY >= MinimumY
		&& Tile.TileY < MaximumY;
}

FIntPoint ARokWorldMapStreamingActor::ComputeCenterChunk(
	FVector2D ViewCenterUnity,
	float TileSizeUnityUnits,
	int32 ChunkTileSpan)
{
	if (ViewCenterUnity.ContainsNaN()
		|| !FMath::IsFinite(TileSizeUnityUnits)
		|| TileSizeUnityUnits <= UE_SMALL_NUMBER
		|| ChunkTileSpan <= 0)
	{
		return FIntPoint::ZeroValue;
	}
	const double ChunkSizeUnity =
		static_cast<double>(TileSizeUnityUnits) * ChunkTileSpan;
	return FIntPoint(
		FMath::FloorToInt(ViewCenterUnity.X / ChunkSizeUnity),
		FMath::FloorToInt(ViewCenterUnity.Y / ChunkSizeUnity));
}

bool ARokWorldMapStreamingActor::HasPositiveDeterminant(
	const FTransform& Transform)
{
	if (!IsFiniteTransformForStreaming(Transform))
	{
		return false;
	}
	const FVector Scale = Transform.GetScale3D();
	const double Determinant =
		static_cast<double>(Scale.X)
		* static_cast<double>(Scale.Y)
		* static_cast<double>(Scale.Z);
	return FMath::IsFinite(Determinant)
		&& Determinant > UE_SMALL_NUMBER;
}

void ARokWorldMapStreamingActor::ResolveCameraPawn()
{
	CameraPawn = nullptr;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TActorIterator<ARokWorldMapCameraPawn> CameraIterator(World);
	if (CameraIterator)
	{
		CameraPawn = *CameraIterator;
	}
}

void ARokWorldMapStreamingActor::EvaluateStreamingState()
{
	if (!RuntimeData)
	{
		if (!ActiveComponents.IsEmpty() || PendingLoadHandle.IsValid())
		{
			++RequestedGeneration;
			if (PendingLoadHandle.IsValid())
			{
				PendingLoadHandle->CancelHandle();
				PendingLoadHandle.Reset();
			}
			DestroyActiveComponents();
			bHasRequestedState = false;
		}
		return;
	}
	if (!IsValid(CameraPawn.Get()))
	{
		return;
	}
	const bool bScopedDiagnostic = IsScopedDiagnosticLod01Allowed();
	if (!RuntimeData->bStaticVisualReady && !bScopedDiagnostic)
	{
		if (!ActiveComponents.IsEmpty() || PendingLoadHandle.IsValid())
		{
			++RequestedGeneration;
			if (PendingLoadHandle.IsValid())
			{
				PendingLoadHandle->CancelHandle();
				PendingLoadHandle.Reset();
			}
			DestroyActiveComponents();
			bHasRequestedState = false;
		}
		if (!bLoggedNotStaticReady)
		{
			UE_LOG(
				LogRokWorldMapStreaming,
				Warning,
				TEXT("Rok world-map streaming is gated: static_visual_ready=false runtime_ready=%s hash=%s."),
				RuntimeData->bRuntimeReady ? TEXT("true") : TEXT("false"),
				*RuntimeData->CanonicalPayloadSha256);
			bLoggedNotStaticReady = true;
		}
		return;
	}
	bLoggedNotStaticReady = false;

	const int32 Lod = IsFixedCameraProbeAllowed() && FixedCameraProbeLod != INDEX_NONE
		? FixedCameraProbeLod : RuntimeData->ResolveLod(CameraPawn->GetCurrentDxf());
	const int32 ChunkTileSpan = RuntimeData->ResolveChunkTileSpan(Lod);
	if (bScopedDiagnostic && Lod > 1)
	{
		if (!ActiveComponents.IsEmpty() || PendingLoadHandle.IsValid() || bHasRequestedState)
		{
			++RequestedGeneration;
			if (PendingLoadHandle.IsValid())
			{
				PendingLoadHandle->CancelHandle();
				PendingLoadHandle.Reset();
			}
			DestroyActiveComponents();
			bHasRequestedState = false;
			LastRequestedLod = INDEX_NONE;
			UE_LOG(LogRokWorldMapStreaming, Warning,
				TEXT("Scoped world-stream diagnostic rejected LOD%d and unloaded all components; LOD2+ dependencies are not promoted."), Lod);
		}
		return;
	}
	if (Lod == INDEX_NONE || ChunkTileSpan <= 0)
	{
		return;
	}
	const FIntPoint CenterChunk = ComputeCenterChunk(
		CameraPawn->GetViewCenterUnity(),
		RuntimeData->TileSizeUnityUnits,
		ChunkTileSpan);
	const float TreeScale =
		ARokWorldMapCameraPawn::EvaluateUnityTreeScale(
			CameraPawn->GetCurrentDxf());
	const bool bTreeScaleChanged =
		!bHasRequestedState
		|| !FMath::IsNearlyEqual(
			TreeScale,
			LastRequestedTreeScale,
			TreeScaleMaterialChangeTolerance);
	const bool bStateChanged =
		!bHasRequestedState
		|| Lod != LastRequestedLod
		|| CenterChunk != LastRequestedCenterChunk
		|| bTreeScaleChanged
		|| RuntimeData->CanonicalPayloadSha256
			!= LastRequestedPayloadSha256;
	if (!bStateChanged)
	{
		return;
	}

	TArray<FString> ValidationErrors;
	if (!RuntimeData->Validate(ValidationErrors))
	{
		UE_LOG(
			LogRokWorldMapStreaming,
			Error,
			TEXT("Rok world-map streaming data validation failed: errors=%d first=%s."),
			ValidationErrors.Num(),
			ValidationErrors.IsEmpty()
				? TEXT("unknown")
				: *ValidationErrors[0]);
		return;
	}

	FPreparedBuild Build;
	if (!PrepareBuild(Lod, CenterChunk, TreeScale, Build))
	{
		return;
	}
	LastRequestedLod = Lod;
	LastRequestedCenterChunk = CenterChunk;
	LastRequestedTreeScale = TreeScale;
	LastRequestedPayloadSha256 = RuntimeData->CanonicalPayloadSha256;
	bHasRequestedState = true;
	RequestPreparedBuild(MoveTemp(Build));
}

bool ARokWorldMapStreamingActor::PrepareBuild(
	int32 Lod,
	FIntPoint CenterChunk,
	float TreeScale,
	FPreparedBuild& OutBuild) const
{
	if (!RuntimeData || !FMath::IsFinite(TreeScale) || TreeScale <= 0.0f)
	{
		return false;
	}
	const int32 ChunkTileSpan =
		RuntimeData->ResolveChunkTileSpan(Lod);
	if (ChunkTileSpan <= 0)
	{
		return false;
	}

	OutBuild = FPreparedBuild();
	OutBuild.Lod = Lod;
	OutBuild.CenterChunk = CenterChunk;
	OutBuild.TreeScale = TreeScale;
	OutBuild.CanonicalPayloadSha256 =
		RuntimeData->CanonicalPayloadSha256;

	const TArray<FIntPoint> ResidentKeys =
		BuildResidentChunkKeys(CenterChunk);
	TSet<FIntPoint> ResidentKeySet;
	ResidentKeySet.Reserve(ResidentKeys.Num());
	for (const FIntPoint Key : ResidentKeys)
	{
		ResidentKeySet.Add(Key);
	}
	if (ResidentKeys.Num() != ResidentChunkCount
		|| ResidentKeySet.Num() != ResidentChunkCount)
	{
		UE_LOG(
			LogRokWorldMapStreaming,
			Error,
			TEXT("Resident chunk invariant failed: logical=%d unique=%d."),
			ResidentKeys.Num(),
			ResidentKeySet.Num());
		return false;
	}

	TSet<FIntPoint> IntersectingChunkKeys;
	for (const FIntPoint Key : ResidentKeys)
	{
		if (DoesChunkIntersectMap(
			Key,
			ChunkTileSpan,
			RuntimeData->MapWidthTiles,
			RuntimeData->MapHeightTiles))
		{
			IntersectingChunkKeys.Add(Key);
		}
	}

	TMap<FRokWorldMapStreamingBatchKey, int32> BatchIndices;
	TSet<FIntPoint> NonEmptyChunkKeys;
	TSet<FSoftObjectPath> UniqueAssetPaths;
	for (const FRokWorldMapTileRecord& Tile : RuntimeData->Tiles)
	{
		const FIntPoint TileChunk(
			Tile.TileX / ChunkTileSpan,
			Tile.TileY / ChunkTileSpan);
		if (!IntersectingChunkKeys.Contains(TileChunk)
			|| !DoesTileIntersectChunk(
				Tile,
				TileChunk,
				ChunkTileSpan))
		{
			continue;
		}

		const FRokWorldMapRenderRecipe* Recipe =
			RuntimeData->ResolveRecipe(Tile.TileTypeId, Lod);
		if (!Recipe)
		{
			UE_LOG(
				LogRokWorldMapStreaming,
				Error,
				TEXT("Missing world-map render recipe: tile_type=%s lod=%d."),
				*Tile.TileTypeId.ToString(),
				Lod);
			return false;
		}
		for (const FRokWorldMapRenderPart& Part : Recipe->Parts)
		{
			if (Part.Mesh.IsNull())
			{
				continue;
			}

			FRokWorldMapStreamingBatchKey BatchKey;
			BatchKey.ChunkKey = TileChunk;
			BatchKey.MeshPath = Part.Mesh.ToSoftObjectPath();
			BatchKey.TranslucencySortPriority =
				Part.TranslucencySortPriority;
			BatchKey.bTree = Part.bTree;
			BatchKey.RenderFlags = Part.RenderFlags;
			BatchKey.MaterialPaths.Reserve(Part.Materials.Num());
			for (const TSoftObjectPtr<UMaterialInterface>& Material :
				Part.Materials)
			{
				BatchKey.MaterialPaths.Add(
					Material.ToSoftObjectPath());
			}

			int32* ExistingBatchIndex = BatchIndices.Find(BatchKey);
			int32 BatchIndex = INDEX_NONE;
			if (ExistingBatchIndex)
			{
				BatchIndex = *ExistingBatchIndex;
			}
			else
			{
				BatchIndex = OutBuild.Batches.Num();
				FPreparedBatch& NewBatch =
					OutBuild.Batches.AddDefaulted_GetRef();
				NewBatch.Key = BatchKey;
				BatchIndices.Add(BatchKey, BatchIndex);
				UniqueAssetPaths.Add(BatchKey.MeshPath);
				for (const FSoftObjectPath& MaterialPath :
					BatchKey.MaterialPaths)
				{
					if (!MaterialPath.IsNull())
					{
						UniqueAssetPaths.Add(MaterialPath);
					}
				}
			}

			FTransform InstanceTransform =
				Part.LocalTransform * Tile.TemplateToWorld;
			if (Part.bTree)
			{
				InstanceTransform.SetScale3D(
					InstanceTransform.GetScale3D() * TreeScale);
			}
			const bool bApprovedSourceMirror =
				RuntimeData->IsApprovedSourceMirroredGroundPart(*Recipe, Part);
			const FVector InstanceScale = InstanceTransform.GetScale3D();
			const double InstanceDeterminant =
				static_cast<double>(InstanceScale.X)
				* static_cast<double>(InstanceScale.Y)
				* static_cast<double>(InstanceScale.Z);
			const bool bValidSourceMirror = bApprovedSourceMirror
				&& IsFiniteTransformForStreaming(InstanceTransform)
				&& FMath::IsFinite(InstanceDeterminant)
				&& InstanceDeterminant < -UE_SMALL_NUMBER;
			if ((bApprovedSourceMirror && !bValidSourceMirror)
				|| (!bApprovedSourceMirror
					&& !HasPositiveDeterminant(InstanceTransform)))
			{
				UE_LOG(
					LogRokWorldMapStreaming,
					Error,
					TEXT("Skipping render-data contract violation with unexpected instance determinant: tile=(%d,%d) recipe=%s source_mirror=%s."),
					Tile.TileX,
					Tile.TileY,
					*Recipe->RecipeId.ToString(),
					bApprovedSourceMirror ? TEXT("true") : TEXT("false"));
				continue;
			}
			OutBuild.Batches[BatchIndex].bContainsApprovedSourceMirror |=
				bValidSourceMirror;
			OutBuild.Batches[BatchIndex].InstanceTransforms.Add(
				InstanceTransform);
			++OutBuild.InstanceCount;
			NonEmptyChunkKeys.Add(TileChunk);
		}
	}

	for (int32 BatchIndex = OutBuild.Batches.Num() - 1;
		BatchIndex >= 0;
		--BatchIndex)
	{
		if (OutBuild.Batches[BatchIndex].InstanceTransforms.IsEmpty())
		{
			OutBuild.Batches.RemoveAt(BatchIndex);
		}
	}
	OutBuild.NonEmptyChunkCount = NonEmptyChunkKeys.Num();
	OutBuild.AssetsToLoad.Reserve(UniqueAssetPaths.Num());
	for (const FSoftObjectPath& AssetPath : UniqueAssetPaths)
	{
		OutBuild.AssetsToLoad.Add(AssetPath);
	}
	OutBuild.AssetsToLoad.Sort(
		[](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
		{
			return Left.ToString() < Right.ToString();
		});
	return true;
}

void ARokWorldMapStreamingActor::RequestPreparedBuild(
	FPreparedBuild&& Build)
{
	++RequestedGeneration;
	const uint64 Generation = RequestedGeneration;
	if (PendingLoadHandle.IsValid())
	{
		PendingLoadHandle->CancelHandle();
		PendingLoadHandle.Reset();
	}

	const TSharedRef<FPreparedBuild, ESPMode::ThreadSafe> SharedBuild =
		MakeShared<FPreparedBuild, ESPMode::ThreadSafe>(
			MoveTemp(Build));
	if (SharedBuild->AssetsToLoad.IsEmpty())
	{
		CompletePreparedBuild(Generation, SharedBuild);
		return;
	}

	FStreamableManager& StreamableManager =
		UAssetManager::GetStreamableManager();
	PendingLoadHandle = StreamableManager.RequestAsyncLoad(
		SharedBuild->AssetsToLoad,
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, Generation, SharedBuild]()
			{
				CompletePreparedBuild(Generation, SharedBuild);
			}),
		FStreamableManager::DefaultAsyncLoadPriority,
		false,
		false,
		TEXT("RokWorldMapStreaming"));
	if (!PendingLoadHandle.IsValid())
	{
		UE_LOG(
			LogRokWorldMapStreaming,
			Error,
			TEXT("Could not start world-map async asset load: generation=%llu."),
			Generation);
	}
}

void ARokWorldMapStreamingActor::CompletePreparedBuild(
	uint64 Generation,
	const TSharedRef<FPreparedBuild, ESPMode::ThreadSafe>& Build)
{
	if (Generation != RequestedGeneration)
	{
		UE_LOG(
			LogRokWorldMapStreaming,
			Verbose,
			TEXT("Discarded stale world-map streaming revision: loaded=%llu current=%llu."),
			Generation,
			RequestedGeneration);
		return;
	}

	for (const FPreparedBatch& Batch : Build->Batches)
	{
		if (!Cast<UStaticMesh>(Batch.Key.MeshPath.ResolveObject()))
		{
			PendingLoadHandle.Reset();
			UE_LOG(
				LogRokWorldMapStreaming,
				Error,
				TEXT("World-map async load did not resolve mesh '%s'; keeping old revision."),
				*Batch.Key.MeshPath.ToString());
			return;
		}
		for (const FSoftObjectPath& MaterialPath :
			Batch.Key.MaterialPaths)
		{
			if (!MaterialPath.IsNull()
				&& !Cast<UMaterialInterface>(
					MaterialPath.ResolveObject()))
			{
				PendingLoadHandle.Reset();
				UE_LOG(
					LogRokWorldMapStreaming,
					Error,
					TEXT("World-map async load did not resolve material '%s'; keeping old revision."),
					*MaterialPath.ToString());
				return;
			}
			if (Batch.bContainsApprovedSourceMirror
				&& !MaterialPath.IsNull()
				&& !CastChecked<UMaterialInterface>(
					MaterialPath.ResolveObject())->IsTwoSided())
			{
				PendingLoadHandle.Reset();
				UE_LOG(LogRokWorldMapStreaming, Error,
					TEXT("Source-mirrored ground requires a two-sided material '%s'; keeping old revision."),
					*MaterialPath.ToString());
				return;
			}
		}
	}

	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>
		NewComponents;
	NewComponents.Reserve(Build->Batches.Num());
	for (int32 BatchIndex = 0;
		BatchIndex < Build->Batches.Num();
		++BatchIndex)
	{
		const FPreparedBatch& Batch = Build->Batches[BatchIndex];
		UStaticMesh* StaticMesh =
			CastChecked<UStaticMesh>(
				Batch.Key.MeshPath.ResolveObject());
		const FName ComponentName = MakeUniqueObjectName(
			this,
			UHierarchicalInstancedStaticMeshComponent::StaticClass(),
			*FString::Printf(
				TEXT("WorldMapChunk_%d_%d_Batch_%d_Gen_%llu"),
				Batch.Key.ChunkKey.X,
				Batch.Key.ChunkKey.Y,
				BatchIndex,
				Generation));
		UHierarchicalInstancedStaticMeshComponent* Component =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(
				this,
				ComponentName,
				RF_Transient);
		if (!Component)
		{
			UE_LOG(
				LogRokWorldMapStreaming,
				Error,
				TEXT("Could not allocate world-map HISM; keeping old revision."));
			for (UHierarchicalInstancedStaticMeshComponent* NewComponent :
				NewComponents)
			{
				if (NewComponent)
				{
					NewComponent->DestroyComponent();
				}
			}
			return;
		}

		Component->SetMobility(EComponentMobility::Static);
		Component->SetupAttachment(SceneRoot);
		Component->SetRelativeTransform(FTransform(
			FQuat::Identity,
			FVector::ZeroVector,
			FVector(-1.0f, 1.0f, 1.0f)));
		Component->SetStaticMesh(StaticMesh);
		for (int32 MaterialIndex = 0;
			MaterialIndex < Batch.Key.MaterialPaths.Num();
			++MaterialIndex)
		{
			const FSoftObjectPath& MaterialPath =
				Batch.Key.MaterialPaths[MaterialIndex];
			if (!MaterialPath.IsNull())
			{
				Component->SetMaterial(
					MaterialIndex,
					CastChecked<UMaterialInterface>(
						MaterialPath.ResolveObject()));
			}
		}
		Component->TranslucencySortPriority =
			Batch.Key.TranslucencySortPriority;
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCollisionResponseToAllChannels(ECR_Ignore);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCastShadow(HasRenderFlag(
			Batch.Key.RenderFlags,
			ERokWorldMapRenderFlag::CastShadow));
		Component->SetReceivesDecals(HasRenderFlag(
			Batch.Key.RenderFlags,
			ERokWorldMapRenderFlag::ReceiveDecals));
		Component->bUseAsOccluder = HasRenderFlag(
			Batch.Key.RenderFlags,
			ERokWorldMapRenderFlag::UseAsOccluder);
		Component->SetVisibleInRayTracing(HasRenderFlag(
			Batch.Key.RenderFlags,
			ERokWorldMapRenderFlag::VisibleInRayTracing));
		Component->SetAffectDistanceFieldLighting(HasRenderFlag(
			Batch.Key.RenderFlags,
			ERokWorldMapRenderFlag::AffectDistanceFieldLighting));
		Component->SetVisibility(false, true);
		AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->AddInstances(
			Batch.InstanceTransforms,
			false,
			false,
			false);
		NewComponents.Add(Component);
	}

	DestroyActiveComponents();
	ActiveComponents = MoveTemp(NewComponents);
	FixedCameraProbeActiveKeys.Reset();
	if (IsFixedCameraProbeAllowed())
	{
		FixedCameraProbeActiveKeys.Reserve(Build->Batches.Num());
		for (const FPreparedBatch& Batch : Build->Batches)
		{
			FixedCameraProbeActiveKeys.Add(Batch.Key);
		}
	}
	for (UHierarchicalInstancedStaticMeshComponent* Component :
		ActiveComponents)
	{
		if (Component)
		{
			Component->SetVisibility(true, true);
		}
	}
	PendingLoadHandle.Reset();

	UE_LOG(
		LogRokWorldMapStreaming,
		Display,
		TEXT("Rok world-map static streaming applied: lod=%d center=(%d,%d) resident=%d nonempty=%d batches=%d instances=%d static_visual_ready=%s scoped_diagnostic=%s runtime_ready=%s hash=%s."),
		Build->Lod,
		Build->CenterChunk.X,
		Build->CenterChunk.Y,
		ResidentChunkCount,
		Build->NonEmptyChunkCount,
		Build->Batches.Num(),
		Build->InstanceCount,
		RuntimeData->bStaticVisualReady ? TEXT("true") : TEXT("false"),
		IsScopedDiagnosticLod01Allowed() ? TEXT("true") : TEXT("false"),
		RuntimeData && RuntimeData->bRuntimeReady
			? TEXT("true")
			: TEXT("false"),
		*Build->CanonicalPayloadSha256);
}

void ARokWorldMapStreamingActor::DestroyActiveComponents()
{
	for (UHierarchicalInstancedStaticMeshComponent* Component :
		ActiveComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	ActiveComponents.Reset();
	FixedCameraProbeActiveKeys.Reset();
}

bool ARokWorldMapStreamingActor::IsFixedCameraProbeAllowed() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	return IsScopedDiagnosticLod01Allowed()
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorldLodFixedCameraMaterialProbe"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorldInstancedMaterialDiagnostic"))
		&& !FParse::Param(FCommandLine::Get(), TEXT("RokWorldAliasPixelCoverageDiagnostic"))
		&& World && World->GetOutermost()->GetName()
			== TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticV1")
		&& RuntimeData && RuntimeData->GetPathName()
			== TEXT("/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.DA_RokWorldStreamSourceLod01")
		&& RuntimeData->CanonicalPayloadSha256
			== TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f");
#else
	return false;
#endif
}

bool ARokWorldMapStreamingActor::SetFixedCameraProbeLod(int32 LodOrIndexNone)
{
	if (LodOrIndexNone == INDEX_NONE)
	{
		FixedCameraProbeLod = INDEX_NONE;
		return true;
	}
	if (!IsFixedCameraProbeAllowed() || (LodOrIndexNone != 0 && LodOrIndexNone != 1))
	{
		return false;
	}
	FixedCameraProbeLod = LodOrIndexNone;
	return true;
}

bool ARokWorldMapStreamingActor::GetFixedCameraProbeBatches(
	TArray<FRokWorldFixedProbeBatch>& OutBatches) const
{
	OutBatches.Reset();
	if (!IsFixedCameraProbeAllowed()
		|| FixedCameraProbeActiveKeys.Num() != ActiveComponents.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < ActiveComponents.Num(); ++Index)
	{
		FRokWorldFixedProbeBatch Entry;
		Entry.Component = ActiveComponents[Index].Get();
		Entry.Key = FixedCameraProbeActiveKeys[Index];
		OutBatches.Add(MoveTemp(Entry));
	}
	return true;
}

bool ARokWorldMapStreamingActor::IsScopedDiagnosticLod01Allowed() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	const bool bOldV1Diagnostic = World
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticV1")
		&& RuntimeData
		&& RuntimeData->GetPathName() == TEXT("/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.DA_RokWorldStreamSourceLod01")
		&& RuntimeData->CanonicalPayloadSha256 == TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f");
	const bool bGroundSortCoverageDiagnostic =
		FParse::Param(FCommandLine::Get(), TEXT("RokWorldAliasPixelCoverageDiagnostic"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorldInstancedMaterialDiagnostic"))
		&& World
		&& World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticGroundSortV2")
		&& RuntimeData
		&& RuntimeData->GetPathName() == TEXT("/Game/RokDiagnostics/WorldStreaming/V2/DA_RokWorldStreamGroundSortLod01V2.DA_RokWorldStreamGroundSortLod01V2")
		&& RuntimeData->CanonicalPayloadSha256 == TEXT("31862a9e4847eb2856fd1c1801ecc6dfd5b6b77673cb2cc6315fb6c85d2ea1ad");
	return bEnableScopedDiagnosticLod01
		&& FParse::Param(FCommandLine::Get(), TEXT("RokWorldStreamingDiagnostic"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& (bOldV1Diagnostic || bGroundSortCoverageDiagnostic)
		&& !RuntimeData->bStaticVisualReady
		&& !RuntimeData->bRuntimeReady
		&& RuntimeData->SourceManifestSha256 == TEXT("fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0");
#else
	return false;
#endif
}

FRokWorldMapStreamingDiagnosticState ARokWorldMapStreamingActor::GetDiagnosticState() const
{
	FRokWorldMapStreamingDiagnosticState State;
	State.bHasRequestedState = bHasRequestedState;
	State.bPendingLoad = PendingLoadHandle.IsValid();
	State.Lod = LastRequestedLod;
	State.CenterChunk = LastRequestedCenterChunk;
	State.Generation = RequestedGeneration;
	bool bEveryActiveRegistered = true;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : ActiveComponents)
	{
		if (IsValid(Component) && Component->IsRegistered())
		{
			++State.ComponentCount;
			State.VisibleComponentCount += Component->IsVisible() ? 1 : 0;
			State.InstanceCount += Component->GetInstanceCount();
			State.ComponentNames.Add(Component->GetFName());
		}
		else
		{
			bEveryActiveRegistered = false;
		}
	}
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> AllComponents;
	GetComponents(AllComponents);
	for (const UHierarchicalInstancedStaticMeshComponent* Component : AllComponents)
	{
		if (IsValid(Component) && Component->GetOwner() == this && Component->IsRegistered())
		{
			++State.RegisteredComponentCount;
			State.RegisteredInstanceCount += Component->GetInstanceCount();
			State.RegisteredComponentNames.Add(Component->GetFName());
		}
	}
	State.ComponentNames.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
	State.RegisteredComponentNames.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
	State.bRegisteredMatchesActive = bEveryActiveRegistered
		&& State.RegisteredComponentCount == State.ComponentCount
		&& State.RegisteredInstanceCount == State.InstanceCount
		&& State.RegisteredComponentNames == State.ComponentNames;
	return State;
}
