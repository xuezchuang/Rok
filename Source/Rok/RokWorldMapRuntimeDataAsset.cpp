#include "RokWorldMapRuntimeDataAsset.h"

#include "Containers/Set.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "UObject/PrimaryAssetId.h"

namespace
{
bool IsFiniteTransform(const FTransform& Transform)
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

bool HasPositiveScaleDeterminant(const FTransform& Transform)
{
	if (!IsFiniteTransform(Transform))
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

bool IsSha256(const FString& Value)
{
	if (Value.Len() != 64)
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsHexDigit(Character))
		{
			return false;
		}
	}
	return true;
}

enum class ESnowTransitionSourceVariant
{
	None,
	Original,
	GroundSortDiagnostic
};

ESnowTransitionSourceVariant GetSnowTransitionSourceVariant(
	const URokWorldMapRuntimeDataAsset& Data)
{
	if (Data.SourceManifestSha256 !=
		TEXT("fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0"))
	{
		return ESnowTransitionSourceVariant::None;
	}
	if (Data.CanonicalPayloadSha256 ==
		TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f"))
	{
		return ESnowTransitionSourceVariant::Original;
	}
	if (Data.CanonicalPayloadSha256 ==
		TEXT("31862a9e4847eb2856fd1c1801ecc6dfd5b6b77673cb2cc6315fb6c85d2ea1ad"))
	{
		return ESnowTransitionSourceVariant::GroundSortDiagnostic;
	}
	return ESnowTransitionSourceVariant::None;
}

bool IsPinnedSnowTransitionSource(const URokWorldMapRuntimeDataAsset& Data)
{
	return GetSnowTransitionSourceVariant(Data) != ESnowTransitionSourceVariant::None;
}

bool IsRuntimeBlockingStatus(ERokWorldMapPendingPolicyStatus Status)
{
	return Status == ERokWorldMapPendingPolicyStatus::Pending;
}
}

URokWorldMapRuntimeDataAsset::URokWorldMapRuntimeDataAsset()
{
	const TArray<float>& Thresholds = GetExpectedLodThresholds();
	LodLevels.Reserve(Thresholds.Num());
	for (int32 Lod = 0; Lod < Thresholds.Num(); ++Lod)
	{
		FRokWorldMapLodLevel& Level = LodLevels.AddDefaulted_GetRef();
		Level.Lod = Lod;
		Level.MaxDxfInclusive = Thresholds[Lod];
		Level.ChunkTileSpan = 1;
	}
}

FPrimaryAssetId URokWorldMapRuntimeDataAsset::GetPrimaryAssetId() const
{
	static const FPrimaryAssetType AssetType(TEXT("RokWorldMapRuntimeData"));
	return FPrimaryAssetId(AssetType, GetFName());
}

bool URokWorldMapRuntimeDataAsset::IsApprovedSourceMirroredGroundPart(
	const FRokWorldMapRenderRecipe& Recipe,
	const FRokWorldMapRenderPart& Part) const
{
	const ESnowTransitionSourceVariant SourceVariant = GetSnowTransitionSourceVariant(*this);
	const int32 ExpectedPriority =
		SourceVariant == ESnowTransitionSourceVariant::GroundSortDiagnostic && Recipe.Lod < 2
		? -5 : 0;
	if (SourceVariant == ESnowTransitionSourceVariant::None
		|| Recipe.Lod < 0 || Recipe.Lod > 2
		|| Part.bTree || Part.TranslucencySortPriority != ExpectedPriority
		|| Part.RenderFlags != (Recipe.Lod == 2 ? 1 : 0)
		|| Part.Materials.Num() != 1
		|| !IsFiniteTransform(Part.LocalTransform))
	{
		return false;
	}

	static const TCHAR* const ApprovedTileTypes[] = {
		TEXT("Tile-snow-shang"), TEXT("Tile-snow-shang_fz"),
		TEXT("Tile-snow-shang_fz1"), TEXT("Tile-snow-youshang"),
		TEXT("Tile-snow-youxia"), TEXT("Tile-snow-zuo"),
		TEXT("Tile-snow-zuo_fz"), TEXT("Tile-snow-zuo_fz1"),
		TEXT("Tile-snow-zuoshang"), TEXT("Tile-snow-zuoxia")
	};
	bool bApprovedTileType = false;
	for (const TCHAR* TileType : ApprovedTileTypes)
	{
		if (Recipe.TileTypeId == FName(TileType))
		{
			bApprovedTileType = true;
			break;
		}
	}
	if (!bApprovedTileType
		|| Recipe.RecipeId.ToString() != FString::Printf(
			TEXT("%s:lod%d"), *Recipe.TileTypeId.ToString(), Recipe.Lod))
	{
		return false;
	}

	const FString MeshPath = Part.Mesh.ToSoftObjectPath().ToString();
	const FString ExpectedMeshPath = Recipe.Lod < 2
		? TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60")
		: TEXT("/Game/RokUnityWorldMapStreaming/Meshes/SM_RokStream_Plane60_60_119ef2cac5.SM_RokStream_Plane60_60_119ef2cac5");
	if (MeshPath != ExpectedMeshPath)
	{
		return false;
	}

	static const TCHAR* const Lod2MaterialHashes[] = {
		TEXT("77d6de66a6"), TEXT("deb42777e7"),
		TEXT("025cf81471"), TEXT("443e0950ea"),
		TEXT("eaee1de93c"), TEXT("cb7b25b13b"),
		TEXT("19d765ee51"), TEXT("cf32322eba"),
		TEXT("67894b57ef")
	};
	const FString MaterialPath = Part.Materials[0].ToSoftObjectPath().ToString();
	bool bApprovedMaterial = false;
	for (int32 MaterialNumber = 1; MaterialNumber <= 9; ++MaterialNumber)
	{
		const FString ExpectedMaterialPath = Recipe.Lod < 2
			? FString::Printf(
				TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_I_TYPE_TEST_guodu%02d.M_WorldGround_I_TYPE_TEST_guodu%02d"),
				MaterialNumber, MaterialNumber)
			: FString::Printf(
				TEXT("/Game/RokUnityWorldMapStreaming/Materials/MI_RokStream_I_TYPE_TEST_guodu%02d_%s.MI_RokStream_I_TYPE_TEST_guodu%02d_%s"),
				MaterialNumber, Lod2MaterialHashes[MaterialNumber - 1],
				MaterialNumber, Lod2MaterialHashes[MaterialNumber - 1]);
		if (MaterialPath == ExpectedMaterialPath)
		{
			bApprovedMaterial = true;
			break;
		}
	}
	if (!bApprovedMaterial)
	{
		return false;
	}

	const FVector Scale = Part.LocalTransform.GetScale3D();
	return FMath::IsNearlyEqual(Scale.X, -5.0, 0.001)
		&& FMath::IsNearlyEqual(Scale.Y, 5.0, 0.001)
		&& FMath::IsNearlyEqual(Scale.Z, 5.0, 0.001);
}

bool URokWorldMapRuntimeDataAsset::Validate(TArray<FString>& OutErrors) const
{
	OutErrors.Reset();
	auto AddError = [&OutErrors](const FString& Message)
	{
		OutErrors.Add(Message);
	};

	if (Schema != TEXT("rok.world_map_runtime_data_asset.v1"))
	{
		AddError(FString::Printf(TEXT("Unexpected schema '%s'."), *Schema));
	}
	if (!IsSha256(CanonicalPayloadSha256))
	{
		AddError(TEXT("CanonicalPayloadSha256 must contain exactly 64 hexadecimal characters."));
	}
	if (!SourceManifestSha256.IsEmpty() && !IsSha256(SourceManifestSha256))
	{
		AddError(TEXT("SourceManifestSha256 must be empty or contain exactly 64 hexadecimal characters."));
	}
	if (!Projection.IsValid())
	{
		AddError(TEXT("Projection is invalid."));
	}
	if (MapWidthTiles <= 0 || MapHeightTiles <= 0)
	{
		AddError(TEXT("Map dimensions must be positive."));
	}
	if (MapWidthTiles * MapHeightTiles != ExpectedMapTileCount)
	{
		AddError(FString::Printf(
			TEXT("Map dimensions must contain exactly %d logical tiles."),
			ExpectedMapTileCount));
	}
	if (!FMath::IsFinite(TileSizeUnityUnits)
		|| TileSizeUnityUnits <= UE_SMALL_NUMBER)
	{
		AddError(TEXT("TileSizeUnityUnits must be finite and positive."));
	}

	const TArray<float>& ExpectedThresholds = GetExpectedLodThresholds();
	if (LodLevels.Num() != ExpectedLodCount)
	{
		AddError(FString::Printf(
			TEXT("LodLevels must contain exactly %d records."),
			ExpectedLodCount));
	}
	for (int32 Index = 0; Index < LodLevels.Num(); ++Index)
	{
		const FRokWorldMapLodLevel& Level = LodLevels[Index];
		if (Level.Lod != Index)
		{
			AddError(FString::Printf(
				TEXT("LodLevels[%d] has Lod=%d."),
				Index,
				Level.Lod));
		}
		if (!FMath::IsFinite(Level.MaxDxfInclusive))
		{
			AddError(FString::Printf(
				TEXT("LOD%d has a non-finite MaxDxfInclusive."),
				Index));
		}
		if (Index < ExpectedThresholds.Num()
			&& Level.MaxDxfInclusive != ExpectedThresholds[Index])
		{
			AddError(FString::Printf(
				TEXT("LOD%d MaxDxfInclusive %.9g does not match %.9g."),
				Index,
				Level.MaxDxfInclusive,
				ExpectedThresholds[Index]));
		}
		if (Index > 0
			&& Level.MaxDxfInclusive <= LodLevels[Index - 1].MaxDxfInclusive)
		{
			AddError(FString::Printf(
				TEXT("LOD%d threshold is not strictly increasing."),
				Index));
		}
		if (Level.ChunkTileSpan <= 0)
		{
			AddError(FString::Printf(
				TEXT("LOD%d ChunkTileSpan must be positive."),
				Index));
		}
	}

	if (Tiles.Num() != ExpectedMapTileCount)
	{
		AddError(FString::Printf(
			TEXT("Tiles contains %d records; expected %d."),
			Tiles.Num(),
			ExpectedMapTileCount));
	}
	TSet<FIntPoint> TileCoordinates;
	TileCoordinates.Reserve(Tiles.Num());
	TSet<FName> ReferencedTileTypeIds;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		const FRokWorldMapTileRecord& Tile = Tiles[Index];
		const FIntPoint Coordinate(Tile.TileX, Tile.TileY);
		if (Tile.TileX < 0
			|| Tile.TileX >= MapWidthTiles
			|| Tile.TileY < 0
			|| Tile.TileY >= MapHeightTiles)
		{
			AddError(FString::Printf(
				TEXT("Tiles[%d] coordinate (%d,%d) is outside the map."),
				Index,
				Tile.TileX,
				Tile.TileY));
		}
		if (TileCoordinates.Contains(Coordinate))
		{
			AddError(FString::Printf(
				TEXT("Duplicate tile coordinate (%d,%d)."),
				Tile.TileX,
				Tile.TileY));
		}
		TileCoordinates.Add(Coordinate);
		if (Tile.TileTypeId.IsNone())
		{
			AddError(FString::Printf(
				TEXT("Tiles[%d] has no TileTypeId."),
				Index));
		}
		else
		{
			ReferencedTileTypeIds.Add(Tile.TileTypeId);
		}
		if (!IsFiniteTransform(Tile.TemplateToWorld))
		{
			AddError(FString::Printf(
				TEXT("Tiles[%d] has a non-finite TemplateToWorld transform."),
				Index));
		}
		else if (!HasPositiveScaleDeterminant(Tile.TemplateToWorld))
		{
			AddError(FString::Printf(
				TEXT("Tiles[%d] TemplateToWorld must have a positive determinant."),
				Index));
		}
	}

	TSet<FName> TileTypeIds;
	TSet<FName> RecipeIds;
	int32 ApprovedMirroredPartsByLod[3] = {0, 0, 0};
	for (int32 Index = 0; Index < Recipes.Num(); ++Index)
	{
		const FRokWorldMapRenderRecipe& Recipe = Recipes[Index];
		if (Recipe.RecipeId.IsNone())
		{
			AddError(FString::Printf(TEXT("Recipes[%d] has no RecipeId."), Index));
		}
		else if (RecipeIds.Contains(Recipe.RecipeId))
		{
			AddError(FString::Printf(
				TEXT("Duplicate RecipeId '%s'."),
				*Recipe.RecipeId.ToString()));
		}
		RecipeIds.Add(Recipe.RecipeId);
		if (Recipe.TileTypeId.IsNone())
		{
			AddError(FString::Printf(
				TEXT("Recipe '%s' has no TileTypeId."),
				*Recipe.RecipeId.ToString()));
		}
		if (Recipe.Lod < 0 || Recipe.Lod >= ExpectedLodCount)
		{
			AddError(FString::Printf(
				TEXT("Recipe '%s' has invalid LOD %d."),
				*Recipe.RecipeId.ToString(),
				Recipe.Lod));
		}
		if (bStaticVisualReady && Recipe.Parts.IsEmpty())
		{
			AddError(FString::Printf(
				TEXT("Static-ready recipe '%s' contains no render parts."),
				*Recipe.RecipeId.ToString()));
		}
		for (int32 PartIndex = 0; PartIndex < Recipe.Parts.Num(); ++PartIndex)
		{
			const FRokWorldMapRenderPart& Part = Recipe.Parts[PartIndex];
			if (bStaticVisualReady && Part.Mesh.IsNull())
			{
				AddError(FString::Printf(
					TEXT("Recipe '%s' part %d has no static mesh."),
					*Recipe.RecipeId.ToString(),
					PartIndex));
			}
			if (!IsFiniteTransform(Part.LocalTransform))
			{
				AddError(FString::Printf(
					TEXT("Recipe '%s' part %d has a non-finite transform."),
					*Recipe.RecipeId.ToString(),
					PartIndex));
			}
			else if (!HasPositiveScaleDeterminant(Part.LocalTransform)
				&& !IsApprovedSourceMirroredGroundPart(Recipe, Part))
			{
				AddError(FString::Printf(
					TEXT("Recipe '%s' part %d transform must have a positive determinant."),
					*Recipe.RecipeId.ToString(),
					PartIndex));
			}
			else if (!HasPositiveScaleDeterminant(Part.LocalTransform)
				&& Recipe.Lod >= 0 && Recipe.Lod < 3)
			{
				++ApprovedMirroredPartsByLod[Recipe.Lod];
			}
		}
	}
	if (IsPinnedSnowTransitionSource(*this)
		&& (ApprovedMirroredPartsByLod[0] != 69
			|| ApprovedMirroredPartsByLod[1] != 69
			|| ApprovedMirroredPartsByLod[2] != 69))
	{
		AddError(TEXT("Pinned snow-transition source must retain exactly 69 approved mirrored parts at each LOD0/1/2."));
	}

	for (int32 Index = 0; Index < TileTypes.Num(); ++Index)
	{
		const FRokWorldMapTileTypeRecord& TileType = TileTypes[Index];
		if (TileType.TileTypeId.IsNone())
		{
			AddError(FString::Printf(TEXT("TileTypes[%d] has no TileTypeId."), Index));
			continue;
		}
		if (TileTypeIds.Contains(TileType.TileTypeId))
		{
			AddError(FString::Printf(
				TEXT("Duplicate TileTypeId '%s'."),
				*TileType.TileTypeId.ToString()));
		}
		TileTypeIds.Add(TileType.TileTypeId);
		if (TileType.LodBindings.Num() != ExpectedLodCount)
		{
			AddError(FString::Printf(
				TEXT("Tile type '%s' must bind exactly %d LODs."),
				*TileType.TileTypeId.ToString(),
				ExpectedLodCount));
		}

		TSet<int32> BoundLods;
		for (const FRokWorldMapLodBinding& Binding : TileType.LodBindings)
		{
			if (Binding.Lod < 0 || Binding.Lod >= ExpectedLodCount)
			{
				AddError(FString::Printf(
					TEXT("Tile type '%s' has invalid binding LOD %d."),
					*TileType.TileTypeId.ToString(),
					Binding.Lod));
			}
			if (BoundLods.Contains(Binding.Lod))
			{
				AddError(FString::Printf(
					TEXT("Tile type '%s' binds LOD %d more than once."),
					*TileType.TileTypeId.ToString(),
					Binding.Lod));
			}
			BoundLods.Add(Binding.Lod);
			if (!RecipeIds.Contains(Binding.RecipeId))
			{
				AddError(FString::Printf(
					TEXT("Tile type '%s' binds missing recipe '%s'."),
					*TileType.TileTypeId.ToString(),
					*Binding.RecipeId.ToString()));
			}
			else
			{
				const FRokWorldMapRenderRecipe* Recipe = Recipes.FindByPredicate(
					[&Binding](const FRokWorldMapRenderRecipe& Candidate)
					{
						return Candidate.RecipeId == Binding.RecipeId;
					});
				if (Recipe
					&& (Recipe->TileTypeId != TileType.TileTypeId
						|| Recipe->Lod != Binding.Lod))
				{
					AddError(FString::Printf(
						TEXT("Binding '%s' does not match tile type '%s' LOD %d."),
						*Binding.RecipeId.ToString(),
						*TileType.TileTypeId.ToString(),
						Binding.Lod));
				}
			}
		}
	}

	for (const FName ReferencedTileTypeId : ReferencedTileTypeIds)
	{
		if (!TileTypeIds.Contains(ReferencedTileTypeId))
		{
			AddError(FString::Printf(
				TEXT("Tiles reference missing tile type '%s'."),
				*ReferencedTileTypeId.ToString()));
		}
	}
	for (const FRokWorldMapRenderRecipe& Recipe : Recipes)
	{
		if (!TileTypeIds.Contains(Recipe.TileTypeId))
		{
			AddError(FString::Printf(
				TEXT("Recipe '%s' references missing tile type '%s'."),
				*Recipe.RecipeId.ToString(),
				*Recipe.TileTypeId.ToString()));
		}
	}

	TSet<FName> PolicyIds;
	bool bHasPendingRuntimePolicy = false;
	for (const FRokWorldMapPendingPolicy& Policy : PendingPolicies)
	{
		if (Policy.PolicyId.IsNone())
		{
			AddError(TEXT("A pending-policy record has no PolicyId."));
		}
		else if (PolicyIds.Contains(Policy.PolicyId))
		{
			AddError(FString::Printf(
				TEXT("Duplicate pending-policy id '%s'."),
				*Policy.PolicyId.ToString()));
		}
		PolicyIds.Add(Policy.PolicyId);
		if (Policy.SourceComponentCount < 0)
		{
			AddError(FString::Printf(
				TEXT("Policy '%s' has a negative SourceComponentCount."),
				*Policy.PolicyId.ToString()));
		}
		bHasPendingRuntimePolicy |= IsRuntimeBlockingStatus(Policy.Status);
	}
	if (bRuntimeReady && !bStaticVisualReady)
	{
		AddError(TEXT("bRuntimeReady cannot be true while bStaticVisualReady is false."));
	}
	if (bRuntimeReady && (bHasPendingRuntimePolicy || !ReadinessBlockers.IsEmpty()))
	{
		AddError(TEXT("bRuntimeReady cannot be true while blockers or pending policies remain."));
	}

	return OutErrors.IsEmpty();
}

int32 URokWorldMapRuntimeDataAsset::ResolveLod(float Dxf) const
{
	return ResolveLodFirstMaxInclusive(LodLevels, Dxf);
}

int32 URokWorldMapRuntimeDataAsset::ResolveChunkTileSpan(int32 Lod) const
{
	const FRokWorldMapLodLevel* Level = LodLevels.FindByPredicate(
		[Lod](const FRokWorldMapLodLevel& Candidate)
		{
			return Candidate.Lod == Lod;
		});
	return Level && Level->ChunkTileSpan > 0
		? Level->ChunkTileSpan
		: 0;
}

const FRokWorldMapRenderRecipe* URokWorldMapRuntimeDataAsset::ResolveRecipe(
	FName TileTypeId,
	int32 Lod) const
{
	const FRokWorldMapTileTypeRecord* TileType = TileTypes.FindByPredicate(
		[TileTypeId](const FRokWorldMapTileTypeRecord& Candidate)
		{
			return Candidate.TileTypeId == TileTypeId;
		});
	if (!TileType)
	{
		return nullptr;
	}

	const FRokWorldMapLodBinding* Binding = TileType->LodBindings.FindByPredicate(
		[Lod](const FRokWorldMapLodBinding& Candidate)
		{
			return Candidate.Lod == Lod;
		});
	if (!Binding)
	{
		return nullptr;
	}

	return Recipes.FindByPredicate(
		[Binding, TileTypeId, Lod](const FRokWorldMapRenderRecipe& Candidate)
		{
			return Candidate.RecipeId == Binding->RecipeId
				&& Candidate.TileTypeId == TileTypeId
				&& Candidate.Lod == Lod;
		});
}

int32 URokWorldMapRuntimeDataAsset::ResolveLodFirstMaxInclusive(
	const TArray<FRokWorldMapLodLevel>& Levels,
	float Dxf,
	float Epsilon)
{
	if (Levels.IsEmpty() || !FMath::IsFinite(Dxf))
	{
		return INDEX_NONE;
	}
	const float SafeEpsilon = FMath::IsFinite(Epsilon)
		? FMath::Max(0.0f, Epsilon)
		: 0.0f;
	for (const FRokWorldMapLodLevel& Level : Levels)
	{
		if (FMath::IsFinite(Level.MaxDxfInclusive)
			&& Dxf <= Level.MaxDxfInclusive + SafeEpsilon)
		{
			return Level.Lod;
		}
	}
	return Levels.Last().Lod;
}

const TArray<float>& URokWorldMapRuntimeDataAsset::GetExpectedLodThresholds()
{
	static const TArray<float> Thresholds = {
		1500.0f,
		3000.0f,
		8000.0f,
		20000.0f,
		42500.0f,
		999999.0f,
	};
	return Thresholds;
}
