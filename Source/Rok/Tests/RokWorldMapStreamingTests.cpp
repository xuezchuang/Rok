#if WITH_DEV_AUTOMATION_TESTS

#include <cmath>
#include <limits>

#include "Containers/Set.h"
#include "Misc/AutomationTest.h"
#include "../RokWorldMapRuntimeDataAsset.h"
#include "../RokWorldMapStreamingActor.h"
#include "UObject/UObjectGlobals.h"

namespace
{
URokWorldMapRuntimeDataAsset* MakeMinimalValidRuntimeData()
{
	URokWorldMapRuntimeDataAsset* Data =
		NewObject<URokWorldMapRuntimeDataAsset>();
	Data->CanonicalPayloadSha256 = FString::ChrN(64, TEXT('a'));
	Data->bStaticVisualReady = false;
	Data->bRuntimeReady = false;

	const FName TileTypeId(TEXT("TileA"));
	Data->Tiles.Reserve(
		URokWorldMapRuntimeDataAsset::ExpectedMapTileCount);
	for (int32 TileY = 0; TileY < Data->MapHeightTiles; ++TileY)
	{
		for (int32 TileX = 0; TileX < Data->MapWidthTiles; ++TileX)
		{
			FRokWorldMapTileRecord& Tile =
				Data->Tiles.AddDefaulted_GetRef();
			Tile.TileX = TileX;
			Tile.TileY = TileY;
			Tile.TileTypeId = TileTypeId;
			Tile.TemplateToWorld = FTransform::Identity;
		}
	}

	FRokWorldMapTileTypeRecord& TileType =
		Data->TileTypes.AddDefaulted_GetRef();
	TileType.TileTypeId = TileTypeId;
	for (int32 Lod = 0;
		Lod < URokWorldMapRuntimeDataAsset::ExpectedLodCount;
		++Lod)
	{
		const FName RecipeId(
			*FString::Printf(TEXT("TileA_Lod%d"), Lod));
		FRokWorldMapRenderRecipe& Recipe =
			Data->Recipes.AddDefaulted_GetRef();
		Recipe.RecipeId = RecipeId;
		Recipe.TileTypeId = TileTypeId;
		Recipe.Lod = Lod;

		FRokWorldMapLodBinding& Binding =
			TileType.LodBindings.AddDefaulted_GetRef();
		Binding.Lod = Lod;
		Binding.RecipeId = RecipeId;
	}
	return Data;
}

bool ErrorsContain(
	const TArray<FString>& Errors,
	const TCHAR* ExpectedFragment)
{
	return Errors.ContainsByPredicate(
		[ExpectedFragment](const FString& Error)
		{
			return Error.Contains(ExpectedFragment);
		});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingLodTest,
	"Rok.WorldMap.Streaming.LodFirstMaxInclusive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingLodTest::RunTest(const FString& Parameters)
{
	const URokWorldMapRuntimeDataAsset* Data =
		NewObject<URokWorldMapRuntimeDataAsset>();
	const TArray<float>& ExpectedThresholds =
		URokWorldMapRuntimeDataAsset::GetExpectedLodThresholds();
	TestEqual(TEXT("There are exactly six source LOD thresholds"),
		ExpectedThresholds.Num(),
		6);
	TestEqual(TEXT("LOD0 threshold"), ExpectedThresholds[0], 1500.0f);
	TestEqual(TEXT("LOD1 threshold"), ExpectedThresholds[1], 3000.0f);
	TestEqual(TEXT("LOD2 threshold"), ExpectedThresholds[2], 8000.0f);
	TestEqual(TEXT("LOD3 threshold"), ExpectedThresholds[3], 20000.0f);
	TestEqual(TEXT("LOD4 threshold"), ExpectedThresholds[4], 42500.0f);
	TestEqual(TEXT("LOD5 threshold"), ExpectedThresholds[5], 999999.0f);

	for (int32 Lod = 0; Lod < ExpectedThresholds.Num(); ++Lod)
	{
		TestEqual(
			*FString::Printf(TEXT("LOD%d exact maximum is inclusive"), Lod),
			Data->ResolveLod(ExpectedThresholds[Lod]),
			Lod);
		if (Lod + 1 < ExpectedThresholds.Num())
		{
			const float NextRepresentable = std::nextafter(
				ExpectedThresholds[Lod],
				std::numeric_limits<float>::infinity());
			TestEqual(
				*FString::Printf(
					TEXT("LOD%d advances at the next representable value"),
					Lod),
				Data->ResolveLod(NextRepresentable),
				Lod + 1);
			TestEqual(
				*FString::Printf(
					TEXT("LOD%d helper applies only an explicitly supplied epsilon"),
					Lod),
				URokWorldMapRuntimeDataAsset::
					ResolveLodFirstMaxInclusive(
						Data->LodLevels,
						ExpectedThresholds[Lod] + 0.5f,
						1.0f),
				Lod);
		}
	}
	TestEqual(
		TEXT("A value beyond the terminal source threshold remains at far LOD"),
		Data->ResolveLod(1000001.0f),
		5);
	TestEqual(
		TEXT("A non-finite Dxf does not resolve"),
		Data->ResolveLod(std::numeric_limits<float>::quiet_NaN()),
		INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingResidentKeysTest,
	"Rok.WorldMap.Streaming.ResidentKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingResidentKeysTest::RunTest(
	const FString& Parameters)
{
	const TArray<FIntPoint> CenterKeys =
		ARokWorldMapStreamingActor::BuildResidentChunkKeys(
			FIntPoint(12, 17));
	TSet<FIntPoint> UniqueCenterKeys;
	for (const FIntPoint Key : CenterKeys)
	{
		UniqueCenterKeys.Add(Key);
	}
	TestEqual(TEXT("A center neighborhood has nine logical keys"),
		CenterKeys.Num(),
		9);
	TestEqual(TEXT("Center neighborhood keys are unique"),
		UniqueCenterKeys.Num(),
		9);
	TestTrue(TEXT("Center key is resident"),
		UniqueCenterKeys.Contains(FIntPoint(12, 17)));

	const TArray<FIntPoint> EdgeKeys =
		ARokWorldMapStreamingActor::BuildResidentChunkKeys(
			FIntPoint::ZeroValue);
	TSet<FIntPoint> UniqueEdgeKeys;
	for (const FIntPoint Key : EdgeKeys)
	{
		UniqueEdgeKeys.Add(Key);
	}
	TestEqual(TEXT("An edge neighborhood still has nine logical keys"),
		EdgeKeys.Num(),
		9);
	TestEqual(TEXT("Edge neighborhood keys remain unique"),
		UniqueEdgeKeys.Num(),
		9);
	TestTrue(TEXT("Keys are not clamped at the low map edge"),
		UniqueEdgeKeys.Contains(FIntPoint(-1, -1)));
	TestTrue(TEXT("Positive neighbor remains resident at the edge"),
		UniqueEdgeKeys.Contains(FIntPoint(1, 1)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingIntersectionTest,
	"Rok.WorldMap.Streaming.TileAndChunkIntersection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingIntersectionTest::RunTest(
	const FString& Parameters)
{
	TestFalse(TEXT("Chunk ending at tile zero does not intersect"),
		ARokWorldMapStreamingActor::DoesChunkIntersectMap(
			FIntPoint(-1, 0),
			4,
			40,
			40));
	TestTrue(TEXT("Origin chunk intersects"),
		ARokWorldMapStreamingActor::DoesChunkIntersectMap(
			FIntPoint(0, 0),
			4,
			40,
			40));
	TestTrue(TEXT("Partially resident high-edge chunk intersects"),
		ARokWorldMapStreamingActor::DoesChunkIntersectMap(
			FIntPoint(9, 9),
			4,
			40,
			40));
	TestFalse(TEXT("Chunk starting at map width does not intersect"),
		ARokWorldMapStreamingActor::DoesChunkIntersectMap(
			FIntPoint(10, 9),
			4,
			40,
			40));

	FRokWorldMapTileRecord Tile;
	Tile.TileX = 3;
	Tile.TileY = 3;
	TestTrue(TEXT("Tile at exclusive-boundary minus one intersects"),
		ARokWorldMapStreamingActor::DoesTileIntersectChunk(
			Tile,
			FIntPoint(0, 0),
			4));
	Tile.TileX = 4;
	TestFalse(TEXT("Tile at exclusive chunk boundary does not intersect"),
		ARokWorldMapStreamingActor::DoesTileIntersectChunk(
			Tile,
			FIntPoint(0, 0),
			4));
	Tile.TileX = 39;
	Tile.TileY = 39;
	TestTrue(TEXT("Final map tile intersects the partially resident edge chunk"),
		ARokWorldMapStreamingActor::DoesTileIntersectChunk(
			Tile,
			FIntPoint(9, 9),
			4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingBatchKeyTest,
	"Rok.WorldMap.Streaming.BatchKeyIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingBatchKeyTest::RunTest(
	const FString& Parameters)
{
	FRokWorldMapStreamingBatchKey Base;
	Base.ChunkKey = FIntPoint(2, 3);
	Base.MeshPath =
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Base.MaterialPaths = {
		FSoftObjectPath(
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")),
		FSoftObjectPath(
			TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")),
	};
	Base.TranslucencySortPriority = 4;
	Base.bTree = true;
	Base.RenderFlags =
		static_cast<int32>(ERokWorldMapRenderFlag::CastShadow);

	FRokWorldMapStreamingBatchKey Equal = Base;
	TestTrue(TEXT("Identical batch keys compare equal"), Base == Equal);
	TestEqual(TEXT("Equal batch keys hash equally"),
		GetTypeHash(Base),
		GetTypeHash(Equal));

	FRokWorldMapStreamingBatchKey Different = Base;
	Different.ChunkKey.X++;
	TestFalse(TEXT("Chunk participates in batch identity"),
		Base == Different);
	Different = Base;
	Different.MeshPath =
		FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TestFalse(TEXT("Mesh participates in batch identity"),
		Base == Different);
	Different = Base;
	Swap(Different.MaterialPaths[0], Different.MaterialPaths[1]);
	TestFalse(TEXT("Ordered material slots participate in batch identity"),
		Base == Different);
	Different = Base;
	Different.TranslucencySortPriority++;
	TestFalse(TEXT("Sort priority participates in batch identity"),
		Base == Different);
	Different = Base;
	Different.bTree = false;
	TestFalse(TEXT("Tree policy participates in batch identity"),
		Base == Different);
	Different = Base;
	Different.RenderFlags |=
		static_cast<int32>(ERokWorldMapRenderFlag::ReceiveDecals);
	TestFalse(TEXT("Render flags participate in batch identity"),
		Base == Different);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapRuntimeDataValidationTest,
	"Rok.WorldMap.Streaming.RuntimeDataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapRuntimeDataValidationTest::RunTest(
	const FString& Parameters)
{
	URokWorldMapRuntimeDataAsset* Data =
		MakeMinimalValidRuntimeData();
	TArray<FString> Errors;
	TestTrue(TEXT("Minimal structurally complete static-pending data validates"),
		Data->Validate(Errors));
	TestEqual(TEXT("Minimal valid data has no errors"),
		Errors.Num(),
		0);
	TestEqual(TEXT("Tile recipe resolves at LOD4"),
		Data->ResolveRecipe(FName(TEXT("TileA")), 4)->RecipeId,
		FName(TEXT("TileA_Lod4")));

	Data->Tiles[1].TileX = Data->Tiles[0].TileX;
	Data->Tiles[1].TileY = Data->Tiles[0].TileY;
	Data->bRuntimeReady = true;
	TestFalse(TEXT("Duplicate coordinates and an unsupported ready claim fail"),
		Data->Validate(Errors));
	TestTrue(TEXT("Validation reports the duplicate coordinate"),
		ErrorsContain(Errors, TEXT("Duplicate tile coordinate")));
	TestTrue(TEXT("Validation reports readiness dependency"),
		ErrorsContain(
			Errors,
			TEXT("bRuntimeReady cannot be true while bStaticVisualReady is false")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingDeterminantTest,
	"Rok.WorldMap.Streaming.PositiveInstanceDeterminant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingDeterminantTest::RunTest(
	const FString& Parameters)
{
	const FTransform Positive(
		FRotator(0.0f, 35.0f, 0.0f),
		FVector(10.0f, 20.0f, 30.0f),
		FVector(2.0f, 3.0f, 4.0f));
	TestTrue(TEXT("Positive instance transform is accepted"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(Positive));

	const FTransform Negative(
		FRotator(0.0f, 35.0f, 0.0f),
		FVector(10.0f, 20.0f, 30.0f),
		FVector(-2.0f, 3.0f, 4.0f));
	TestFalse(TEXT("Negative instance determinant is rejected"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(Negative));

	const FTransform Singular(
		FQuat::Identity,
		FVector::ZeroVector,
		FVector(1.0f, 0.0f, 1.0f));
	TestFalse(TEXT("Singular instances are not promoted"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(
			Singular));
	const FTransform MirroredComponentBasis(
		FQuat::Identity,
		FVector::ZeroVector,
		FVector(-1.0f, 1.0f, 1.0f));
	TestFalse(TEXT("Component mirror stays separate from positive instances"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(
			MirroredComponentBasis));
	return true;
}

#endif
