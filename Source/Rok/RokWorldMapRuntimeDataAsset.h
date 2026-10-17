#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RokWorldMapTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "RokWorldMapRuntimeDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType, meta = (Bitflags))
enum class ERokWorldMapRenderFlag : uint8
{
	None = 0 UMETA(Hidden),
	CastShadow = 1 << 0,
	ReceiveDecals = 1 << 1,
	UseAsOccluder = 1 << 2,
	VisibleInRayTracing = 1 << 3,
	AffectDistanceFieldLighting = 1 << 4,
};
ENUM_CLASS_FLAGS(ERokWorldMapRenderFlag)

UENUM(BlueprintType)
enum class ERokWorldMapPendingPolicyStatus : uint8
{
	Pending,
	SourceExplicitNoOp,
	AcceptedOmission,
	Promoted,
};

USTRUCT(BlueprintType)
struct FRokWorldMapLodLevel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 Lod = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	float MaxDxfInclusive = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming", meta = (ClampMin = "1"))
	int32 ChunkTileSpan = 1;
};

USTRUCT(BlueprintType)
struct FRokWorldMapTileRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 TileX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 TileY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName TileTypeId = NAME_None;

	/**
	 * Actor-local transform in the unmirrored component basis. The streaming
	 * component supplies the project Unity-X mirror once, at component level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FTransform TemplateToWorld = FTransform::Identity;
};

USTRUCT(BlueprintType)
struct FRokWorldMapRenderPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Material slots are positional. Null entries retain the mesh default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	/**
	 * Flat, recipe-local transform. Recursive Unity prefab transforms must be
	 * collapsed into this value by the deterministic data generation step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FTransform LocalTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 TranslucencySortPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	bool bTree = false;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Rok|World Map|Streaming",
		meta = (Bitmask, BitmaskEnum = "/Script/Rok.ERokWorldMapRenderFlag"))
	int32 RenderFlags = static_cast<int32>(ERokWorldMapRenderFlag::CastShadow);
};

USTRUCT(BlueprintType)
struct FRokWorldMapRenderRecipe
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName TileTypeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 Lod = 0;

	/** Flat static-render parts; no recursive prefab nodes are stored at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapRenderPart> Parts;
};

USTRUCT(BlueprintType)
struct FRokWorldMapLodBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 Lod = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName RecipeId = NAME_None;
};

USTRUCT(BlueprintType)
struct FRokWorldMapTileTypeRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName TileTypeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapLodBinding> LodBindings;
};

USTRUCT(BlueprintType)
struct FRokWorldMapPendingPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FName PolicyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	ERokWorldMapPendingPolicyStatus Status = ERokWorldMapPendingPolicyStatus::Pending;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	int32 SourceComponentCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	FString RequiredDecision;
};

UCLASS(BlueprintType)
class ROK_API URokWorldMapRuntimeDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URokWorldMapRuntimeDataAsset();

	static constexpr int32 ExpectedLodCount = 6;
	static constexpr int32 ExpectedMapTileCount = 1600;
	/** Source policy is exact first-max-inclusive; runtime adds no tolerance. */
	static constexpr float LodSelectionEpsilon = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Metadata")
	FString Schema = TEXT("rok.world_map_runtime_data_asset.v1");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Metadata")
	FString CanonicalPayloadSha256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Metadata")
	FString SourceManifestPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Metadata")
	FString SourceManifestSha256;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	FRokWorldMapProjection Projection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	int32 MapWidthTiles = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	int32 MapHeightTiles = 40;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map")
	float TileSizeUnityUnits = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapLodLevel> LodLevels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapTileRecord> Tiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapTileTypeRecord> TileTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Streaming")
	TArray<FRokWorldMapRenderRecipe> Recipes;

	/**
	 * True only when the static mesh/material recipe paths are generated and
	 * accepted. The streaming actor will not render an unaccepted data asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Readiness")
	bool bStaticVisualReady = false;

	/**
	 * Runtime authority/collision/effects readiness. Static visual streaming
	 * deliberately does not require this to be true and never promotes it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Readiness")
	bool bRuntimeReady = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Readiness")
	TArray<FString> ReadinessBlockers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rok|World Map|Readiness")
	TArray<FRokWorldMapPendingPolicy> PendingPolicies;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Structural validation independent of editor-only data-validation modules. */
	bool Validate(TArray<FString>& OutErrors) const;

	/** Exact source-pinned snow-transition reflection; other negative scales remain invalid. */
	bool IsApprovedSourceMirroredGroundPart(
		const FRokWorldMapRenderRecipe& Recipe,
		const FRokWorldMapRenderPart& Part) const;

	/** Exact source first-MaxDxfInclusive match. */
	int32 ResolveLod(float Dxf) const;

	int32 ResolveChunkTileSpan(int32 Lod) const;

	const FRokWorldMapRenderRecipe* ResolveRecipe(FName TileTypeId, int32 Lod) const;

	static int32 ResolveLodFirstMaxInclusive(
		const TArray<FRokWorldMapLodLevel>& Levels,
		float Dxf,
		/** Nonzero only for explicit diagnostic callers; ResolveLod uses zero. */
		float Epsilon = LodSelectionEpsilon);

	static const TArray<float>& GetExpectedLodThresholds();
};
