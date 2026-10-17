#include "../RokWorldMapRuntimeDataAsset.h"
#include "../RokWorldMapStreamingActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapStreamingSourceMirrorTest,
	"Rok.WorldMap.Streaming.SourcePinnedSnowMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapStreamingSourceMirrorTest::RunTest(const FString& Parameters)
{
	URokWorldMapRuntimeDataAsset* Data = NewObject<URokWorldMapRuntimeDataAsset>();
	Data->CanonicalPayloadSha256 = TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f");
	Data->SourceManifestSha256 = TEXT("fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0");
	FRokWorldMapRenderRecipe Recipe;
	Recipe.TileTypeId = FName(TEXT("Tile-snow-shang"));
	Recipe.RecipeId = FName(TEXT("Tile-snow-shang:lod0"));
	Recipe.Lod = 0;
	FRokWorldMapRenderPart Part;
	Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60")));
	Part.Materials.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_I_TYPE_TEST_guodu03.M_WorldGround_I_TYPE_TEST_guodu03"))));
	Part.LocalTransform = FTransform(FQuat::Identity, FVector::ZeroVector,
		FVector(-5.0, 5.0, 5.0));
	Part.RenderFlags = 0;
	TestTrue(TEXT("Pinned source snow-ground reflection is approved"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	TestFalse(TEXT("General positive-only instance gate stays restrictive"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(Part.LocalTransform));

	// The actor validates Part * Tile in HISM local space. Its existing parent
	// component X mirror is then applied once; the final world sign is opposite.
	const FTransform TileToWorld(FQuat::Identity, FVector(100.0, 200.0, 0.0),
		FVector(1.0, 1.0, 1.0));
	const FTransform OrdinaryPart(FQuat::Identity, FVector::ZeroVector,
		FVector(5.0, 5.0, 5.0));
	const FTransform ComponentHandedness(FQuat::Identity, FVector::ZeroVector,
		FVector(-1.0, 1.0, 1.0));
	const FTransform OrdinaryInstance = OrdinaryPart * TileToWorld;
	const FTransform SnowInstance = Part.LocalTransform * TileToWorld;
	TestTrue(TEXT("Ordinary Part*Tile instance-local determinant is positive"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(OrdinaryInstance));
	TestFalse(TEXT("Approved snow Part*Tile instance-local determinant is negative"),
		ARokWorldMapStreamingActor::HasPositiveDeterminant(SnowInstance));
	const FTransform OrdinaryWorld = OrdinaryInstance * ComponentHandedness;
	const FTransform SnowWorld = SnowInstance * ComponentHandedness;
	TestTrue(TEXT("Ordinary local positive times component X mirror is world-negative"),
		OrdinaryWorld.ToMatrixWithScale().Determinant() < -UE_SMALL_NUMBER);
	TestTrue(TEXT("Snow local negative times component X mirror is world-positive"),
		SnowWorld.ToMatrixWithScale().Determinant() > UE_SMALL_NUMBER);

	Data->CanonicalPayloadSha256 = FString::ChrN(64, TEXT('0'));
	TestFalse(TEXT("Different payload cannot claim the source exception"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	Data->CanonicalPayloadSha256 = TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f");
	Recipe.TileTypeId = FName(TEXT("Tile-grass"));
	TestFalse(TEXT("Another tile family cannot mirror"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	Recipe.TileTypeId = FName(TEXT("Tile-snow-shang"));
	Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	TestFalse(TEXT("A different mesh cannot borrow the source mirror"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60")));
	Part.LocalTransform.SetScale3D(FVector(-5.0, 0.0, 5.0));
	TestFalse(TEXT("Degenerate transform still fails"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	Part.LocalTransform.SetScale3D(FVector(-4.0, 5.0, 5.0));
	TestFalse(TEXT("Arbitrary negative scale still fails"),
		Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapGroundSortDiagnosticSourceMirrorTest,
	"Rok.WorldMap.Streaming.GroundSortDiagnosticSnowMirror",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapGroundSortDiagnosticSourceMirrorTest::RunTest(const FString& Parameters)
{
	URokWorldMapRuntimeDataAsset* Data = NewObject<URokWorldMapRuntimeDataAsset>();
	const FString OriginalPayload = TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f");
	const FString GroundSortPayload = TEXT("31862a9e4847eb2856fd1c1801ecc6dfd5b6b77673cb2cc6315fb6c85d2ea1ad");
	const FString SourceManifest = TEXT("fdce978db9b7d6be4c1fcb2da3a24f30bb49835acc16698fc54ccbfa747b7ad0");
	Data->SourceManifestSha256 = SourceManifest;
	FRokWorldMapRenderRecipe Recipe;
	Recipe.TileTypeId = FName(TEXT("Tile-snow-shang"));
	FRokWorldMapRenderPart Part;
	Part.LocalTransform = FTransform(FQuat::Identity, FVector::ZeroVector,
		FVector(-5.0, 5.0, 5.0));
	Part.Materials.Add(TSoftObjectPtr<UMaterialInterface>());
	for (int32 Lod = 0; Lod < 3; ++Lod)
	{
		Recipe.Lod = Lod;
		Recipe.RecipeId = FName(*FString::Printf(TEXT("Tile-snow-shang:lod%d"), Lod));
		Part.RenderFlags = Lod == 2 ? 1 : 0;
		Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Lod < 2
			? TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60")
			: TEXT("/Game/RokUnityWorldMapStreaming/Meshes/SM_RokStream_Plane60_60_119ef2cac5.SM_RokStream_Plane60_60_119ef2cac5")));
		Part.Materials[0] = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Lod < 2
			? TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_I_TYPE_TEST_guodu03.M_WorldGround_I_TYPE_TEST_guodu03")
			: TEXT("/Game/RokUnityWorldMapStreaming/Materials/MI_RokStream_I_TYPE_TEST_guodu03_025cf81471.MI_RokStream_I_TYPE_TEST_guodu03_025cf81471")));

		Data->CanonicalPayloadSha256 = OriginalPayload;
		Part.TranslucencySortPriority = 0;
		TestTrue(*FString::Printf(TEXT("Original LOD%d still approves priority 0"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.TranslucencySortPriority = -5;
		TestFalse(*FString::Printf(TEXT("Original LOD%d rejects priority -5"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));

		Data->CanonicalPayloadSha256 = GroundSortPayload;
		Part.TranslucencySortPriority = Lod < 2 ? -5 : 0;
		TestTrue(*FString::Printf(TEXT("Derived LOD%d approves only source priority"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.TranslucencySortPriority = Lod < 2 ? 0 : -5;
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects opposite priority"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));

		Part.TranslucencySortPriority = Lod < 2 ? -5 : 0;
		Data->SourceManifestSha256 = FString::ChrN(64, TEXT('0'));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects foreign source"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Data->SourceManifestSha256 = SourceManifest;
		Data->CanonicalPayloadSha256 = FString::ChrN(64, TEXT('f'));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects unknown payload"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Data->CanonicalPayloadSha256 = GroundSortPayload;
		Part.bTree = true;
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects tree"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.bTree = false;
		Part.Materials[0] = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects foreign material"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.Materials[0] = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(Lod < 2
			? TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_I_TYPE_TEST_guodu03.M_WorldGround_I_TYPE_TEST_guodu03")
			: TEXT("/Game/RokUnityWorldMapStreaming/Materials/MI_RokStream_I_TYPE_TEST_guodu03_025cf81471.MI_RokStream_I_TYPE_TEST_guodu03_025cf81471")));
		Part.RenderFlags = Lod == 2 ? 0 : 1;
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects wrong render flags"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.RenderFlags = Lod == 2 ? 1 : 0;
		Recipe.TileTypeId = FName(TEXT("Tile-grass"));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects foreign tile"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Recipe.TileTypeId = FName(TEXT("Tile-snow-shang"));
		Part.LocalTransform.SetScale3D(FVector(-4.0, 5.0, 5.0));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects wrong mirror scale"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
		Part.LocalTransform.SetScale3D(FVector(-5.0, 5.0, 5.0));
		Part.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
		TestFalse(*FString::Printf(TEXT("Derived LOD%d rejects foreign mesh"), Lod),
			Data->IsApprovedSourceMirroredGroundPart(Recipe, Part));
	}
	Data->CanonicalPayloadSha256 = GroundSortPayload;
	TArray<FString> Errors;
	Data->Validate(Errors);
	TestTrue(TEXT("Derived source retains the 69-per-LOD mirrored row-count gate"),
		Errors.Contains(TEXT("Pinned snow-transition source must retain exactly 69 approved mirrored parts at each LOD0/1/2.")));
	return true;
}
#endif
