#include "RokLocalConstructionVisualFactory.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokCitySpriteActor.h"
#include "RokLocalCitySimulationContext.h"
#include "RokStrategyCameraPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"

namespace
{
// Farm/Wood: CityResourcePresentationAssets/ue_assets_verify.json.
// Commander hall, scout camp, archery range: CityBuildingPresentationAssets/ue_assets_create_2.json
// (mesh_topology; probe run 20261104_135322_891_47244).
// Child poses are the prefab main sprite child (float values widened to double).
const FRokLocalSourceBuildingVisual SourceVisuals[] = {
	{2, TEXT("build_2_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/SM_build_2_101_1_SourceSpriteMesh.SM_build_2_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/M_build_2_101_1_SourceRG.M_build_2_101_1_SourceRG"),
		FVector(0.0430000014603138, 0.05299999937415123, 0.03099999949336052), 0.13229629397392273,
		133, 163, FVector(-234.9999847412, 0.0, -222.0), FVector(243.0, 0.0, 240.0000152588), true},
	{3, TEXT("build_3_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/SM_build_3_101_1_SourceSpriteMesh.SM_build_3_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/M_build_3_101_1_SourceRG.M_build_3_101_1_SourceRG"),
		FVector(0.003000000026077032, 0.04100000113248825, 0.0560000017285347), 0.1359577178955078,
		43, 41, FVector(-236.9999847412, 0.0, -190.5), FVector(256.0, 0.0, 208.5), true},
	{18, TEXT("build_18_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_1/SM_build_18_101_1_SourceSpriteMesh.SM_build_18_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_1/M_build_18_101_1_SourceRG.M_build_18_101_1_SourceRG"),
		FVector(0.020999999716877937, 0.03700000047683716, 0.03200000151991844), 0.1124904677271843,
		47, 46, FVector(-256.0, 0.0, -216.0), FVector(256.0, 0.0, 216.0), false},
	// Server construction models from the verified city-building and academy asset reports.
	// Alliance Center uses the native prefab pose and ue_assets_create_7.json topology.
	{16, TEXT("build_16_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_16_101_1/SM_build_16_101_1_SourceSpriteMesh.SM_build_16_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_16_101_1/M_build_16_101_1_SourceRG.M_build_16_101_1_SourceRG"),
		FVector(0.018, 0.046, 0.046), 0.126913920044899,
		4, 2, FVector(-256.0, 0.0, -234.0), FVector(256.0, 0.0, 234.0), false, false, nullptr, 0.11784864217042923},
	{2, TEXT("build_2_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_2_101_2/SM_build_2_101_2_SourceSpriteMesh.SM_build_2_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_2_101_2/M_build_2_101_2_SourceRG.M_build_2_101_2_SourceRG"),
		FVector(0.0204, 0.0303, 0.0223), 0.117848642170429,
		63, 61, FVector(-256.0, 0.0, -216.5), FVector(256.0, 0.0, 216.5), false, false},
	{3, TEXT("build_3_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_3_101_2/SM_build_3_101_2_SourceSpriteMesh.SM_build_3_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_3_101_2/M_build_3_101_2_SourceRG.M_build_3_101_2_SourceRG"),
		FVector(0.0234, 0.0312, 0.0216), 0.116848587989807,
		92, 109, FVector(-256.0, 0.0, -217.5), FVector(256.0, 0.0, 217.5), false, false},
	{4, TEXT("build_4_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_4_101_2/SM_build_4_101_2_SourceSpriteMesh.SM_build_4_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_4_101_2/M_build_4_101_2_SourceRG.M_build_4_101_2_SourceRG"),
		FVector(0.0229, 0.0362, 0.029), 0.119507685303688,
		57, 59, FVector(-256.0, 0.0, -217.0), FVector(256.0, 0.0, 217.0), false, false},
	{13, TEXT("build_13_101_2"),
		TEXT("/Game/RokUnity/City/Academy/build_13_101_2/SM_build_13_101_2_SourceSpriteMesh.SM_build_13_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/Academy/build_13_101_2/M_build_13_101_2_SourceRG.M_build_13_101_2_SourceRG"),
		FVector(0.032, 0.038, 0.022), 0.13261755,
		49, 47, FVector(-256.0, 0.0, -213.0), FVector(256.0, 0.0, 213.0), false, false,
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")},
	{28, TEXT("build_28_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_28_101_1/SM_build_28_101_1_SourceSpriteMesh.SM_build_28_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_28_101_1/M_build_28_101_1_SourceRG.M_build_28_101_1_SourceRG"),
		FVector(0.017999999225139618, 0.029999999329447746, 0.02500000037252903), 0.14244478940963745,
		48, 47, FVector(-249.0, 0.0, -199.5), FVector(256.0, 0.0, 196.5), false, false},
	{11, TEXT("build_11_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_11_101_1/SM_build_11_101_1_SourceSpriteMesh.SM_build_11_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_11_101_1/M_build_11_101_1_SourceRG.M_build_11_101_1_SourceRG"),
		FVector(0.04399999976158142, 0.09629999846220016, 0.04399999976158142), 0.17851075530052185,
		73, 71, FVector(-256.0, 0.0, -233.0), FVector(256.0, 0.0, 233.0), false, false},
};
// Exact classical-era variants from CityBuildingPresentationAssets/ue_assets_create_5.json.
const FRokLocalSourceBuildingVisual ClassicalSourceVisuals[] = {
	{1, TEXT("build_1_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_1_101_2/SM_build_1_101_2_SourceSpriteMesh.SM_build_1_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_1_101_2/M_build_1_101_2_SourceRG.M_build_1_101_2_SourceRG"),
		FVector(0.035, 0.047, 0.032), 0.11867597,
		183, 181, FVector(-512.0, 0.0, -419.0), FVector(512.0, 0.0, 419.0), false, false},
	{7, TEXT("build_7_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_7_101_2/SM_build_7_101_2_SourceSpriteMesh.SM_build_7_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_7_101_2/M_build_7_101_2_SourceRG.M_build_7_101_2_SourceRG"),
		FVector(0.016, 0.019, 0.011), 0.11078519,
		4, 2, FVector(-256.0, 0.0, -213.5), FVector(256.0, 0.0, 213.5), false, false},
	{9, TEXT("build_9_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_9_101_2/SM_build_9_101_2_SourceSpriteMesh.SM_build_9_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_9_101_2/M_build_9_101_2_SourceRG.M_build_9_101_2_SourceRG"),
		FVector(0.057, 0.091, 0.072), 0.17795195,
		50, 48, FVector(-256.0, 0.0, -233.99998474121094), FVector(256.0, 0.0, 233.99998474121094), false, false},
	{10, TEXT("build_10_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_10_101_2/SM_build_10_101_2_SourceSpriteMesh.SM_build_10_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_10_101_2/M_build_10_101_2_SourceRG.M_build_10_101_2_SourceRG"),
		FVector(0.037, 0.057, 0.044), 0.16753879,
		67, 65, FVector(-256.0, 0.0, -228.0), FVector(256.0, 0.0, 228.0), false, false},
	{11, TEXT("build_11_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_11_101_2/SM_build_11_101_2_SourceSpriteMesh.SM_build_11_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_11_101_2/M_build_11_101_2_SourceRG.M_build_11_101_2_SourceRG"),
		FVector(0.0577, 0.0814, 0.0562), 0.1704743,
		87, 84, FVector(-256.0, 0.0, -241.00001525878906), FVector(256.0, 0.0, 241.00001525878906), false, false},
	{14, TEXT("build_14_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_14_101_2/SM_build_14_101_2_SourceSpriteMesh.SM_build_14_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_14_101_2/M_build_14_101_2_SourceRG.M_build_14_101_2_SourceRG"),
		FVector(0.0271, 0.0456, 0.0372), 0.11941965,
		48, 46, FVector(-256.0, 0.0, -220.0), FVector(256.0, 0.0, 220.0), false, false},
	{16, TEXT("build_16_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_16_101_2/SM_build_16_101_2_SourceSpriteMesh.SM_build_16_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_16_101_2/M_build_16_101_2_SourceRG.M_build_16_101_2_SourceRG"),
		FVector(0.042, 0.0447, 0.0213), 0.1440521627664566,
		47, 45, FVector(-248.0, 0.0, -212.5), FVector(248.0, 0.0, 212.5), false, false},
	{15, TEXT("build_15_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_15_101_2/SM_build_15_101_2_SourceSpriteMesh.SM_build_15_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_15_101_2/M_build_15_101_2_SourceRG.M_build_15_101_2_SourceRG"),
		FVector(0.0323, 0.0418, 0.0271), 0.13898994,
		4, 2, FVector(-256.0, 0.0, -219.0), FVector(256.0, 0.0, 219.0), false, false},
	{18, TEXT("build_18_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_2/SM_build_18_101_2_SourceSpriteMesh.SM_build_18_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_18_101_2/M_build_18_101_2_SourceRG.M_build_18_101_2_SourceRG"),
		FVector(0.0282, 0.0409, 0.0294), 0.103737876,
		69, 68, FVector(-256.0, 0.0, -232.5), FVector(256.0, 0.0, 232.5), false, false},
	{28, TEXT("build_28_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_28_101_2/SM_build_28_101_2_SourceSpriteMesh.SM_build_28_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_28_101_2/M_build_28_101_2_SourceRG.M_build_28_101_2_SourceRG"),
		FVector(0.0062, 0.0013, -0.0044), 0.14083761,
		38, 36, FVector(-256.0, 0.0, -177.0), FVector(256.0, 0.0, 177.0), false, false},
	// Remaining civilization 101 age-2 menu models (ue_assets_create_9.json).
	{5, TEXT("build_5_101_3"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_5_101_3/SM_build_5_101_3_SourceSpriteMesh.SM_build_5_101_3_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_5_101_3/M_build_5_101_3_SourceRG.M_build_5_101_3_SourceRG"),
		FVector(0.016, 0.027, 0.022), 0.1127586439,
		126, 167, FVector(-256.0, 0.0, -225.50001525878906), FVector(256.0, 0.0, 225.50001525878906), false, false},
	{12, TEXT("build_12_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_12_101_2/SM_build_12_101_2_SourceSpriteMesh.SM_build_12_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_12_101_2/M_build_12_101_2_SourceRG.M_build_12_101_2_SourceRG"),
		FVector(0.032, 0.044, 0.03), 0.1665976793,
		48, 46, FVector(-256.0, 0.0, -217.5), FVector(256.0, 0.0, 217.5), false, false},
	{17, TEXT("build_17_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_17_101_2/SM_build_17_101_2_SourceSpriteMesh.SM_build_17_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_17_101_2/M_build_17_101_2_SourceRG.M_build_17_101_2_SourceRG"),
		FVector(0.0271, 0.0399, 0.0292), 0.1424124241,
		46, 44, FVector(-256.0, 0.0, -208.99998474121094), FVector(256.0, 0.0, 208.99998474121094), false, false},
	{19, TEXT("build_19_101_3"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_19_101_3/SM_build_19_101_3_SourceSpriteMesh.SM_build_19_101_3_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_19_101_3/M_build_19_101_3_SourceRG.M_build_19_101_3_SourceRG"),
		FVector(0.02, 0.025, 0.015), 0.1440094709,
		75, 73, FVector(-256.0, 0.0, -202.50001525878906), FVector(256.0, 0.0, 202.50001525878906), false, false},
	{20, TEXT("build_20_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_20_101_2/SM_build_20_101_2_SourceSpriteMesh.SM_build_20_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_20_101_2/M_build_20_101_2_SourceRG.M_build_20_101_2_SourceRG"),
		FVector(0.0247, 0.0383, 0.0297), 0.1067019626,
		50, 48, FVector(-256.0, 0.0, -227.0), FVector(256.0, 0.0, 227.0), false, false},
	{21, TEXT("build_21_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_21_101_2/SM_build_21_101_2_SourceSpriteMesh.SM_build_21_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_21_101_2/M_build_21_101_2_SourceRG.M_build_21_101_2_SourceRG"),
		FVector(0.04, 0.064, 0.05), 0.1469019949,
		49, 47, FVector(-256.0, 0.0, -234.99998474121094), FVector(256.0, 0.0, 234.99998474121094), false, false},
	{29, TEXT("build_29_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_29_101_2/SM_build_29_101_2_SourceSpriteMesh.SM_build_29_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_29_101_2/M_build_29_101_2_SourceRG.M_build_29_101_2_SourceRG"),
		FVector(0.0219, 0.0303, 0.0217), 0.07262527198,
		31, 29, FVector(-200.0, 0.0, -186.0), FVector(200.0, 0.0, 186.0), false, false, nullptr, 0.07697037607, 0.09931660444},
	{30, TEXT("build_30_101_2"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_30_101_2/SM_build_30_101_2_SourceSpriteMesh.SM_build_30_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_30_101_2/M_build_30_101_2_SourceRG.M_build_30_101_2_SourceRG"),
		FVector(0.061, 0.115, 0.102), 0.1105793938,
		48, 44, FVector(-217.0, 0.0, -300.0), FVector(256.0, 0.0, 232.0), false, false},
	{31, TEXT("build_31_101_4"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_31_101_4/SM_build_31_101_4_SourceSpriteMesh.SM_build_31_101_4_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_31_101_4/M_build_31_101_4_SourceRG.M_build_31_101_4_SourceRG"),
		FVector(0.041, 0.054, 0.036), 0.1660228223,
		46, 45, FVector(-256.0, 0.0, -231.5), FVector(256.0, 0.0, 231.5), false, false},
	{51, TEXT("build_51_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_51_101_1/SM_build_51_101_1_SourceSpriteMesh.SM_build_51_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_51_101_1/M_build_51_101_1_SourceRG.M_build_51_101_1_SourceRG"),
		FVector(0.094, -0.037, 0.266), 0.200000003,
		106, 187, FVector(-166.5, 0.0, -128.0), FVector(166.5, 0.0, 128.0), false, false},
	{52, TEXT("build_52_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_52_101_1/SM_build_52_101_1_SourceSpriteMesh.SM_build_52_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_52_101_1/M_build_52_101_1_SourceRG.M_build_52_101_1_SourceRG"),
		FVector(-0.1046, 0.197, 0.098), 0.224999994,
		55, 85, FVector(-152.0, 0.0, -104.49999237060547), FVector(152.0, 0.0, 104.49999237060547), false, false},
	{53, TEXT("build_53_101_1"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_53_101_1/SM_build_53_101_1_SourceSpriteMesh.SM_build_53_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/SourceBuildings/build_53_101_1/M_build_53_101_1_SourceRG.M_build_53_101_1_SourceRG"),
		FVector(-0.113, 0.245, 0.18799999), 0.1940174401,
		94, 142, FVector(-199.0, 0.0, -140.5), FVector(199.0, 0.0, 140.5), false, false},
};
// Generated from the verified Unity export and UE asset topology.
const FRokLocalSourceBuildingVisual ImportedSourceVisuals[] = {
#include "RokCityBuildingVisuals.generated.inl"
};
constexpr TCHAR ServerBuildingTag[] = TEXT("RokServerBuilding");
constexpr TCHAR PendingTag[] = TEXT("RokLocalConstructionPending");
constexpr TCHAR SourceLevelZeroTag[] = TEXT("RokLocalSourceLevel_0");
constexpr TCHAR SourceLevelOneTag[] = TEXT("RokLocalSourceLevel_1");
constexpr double BoundsToleranceCm = 0.05;

struct FSourceVisual
{
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	FString ModelId;
	FTransform Child = FTransform::Identity;
	FTransform Actor = FTransform::Identity;
	FRokCityConstructionFootprint Footprint;
	float CityDxf = 0.0f;
};

FName IndexedTag(const TCHAR* Prefix, int64 Value)
{
	return FName(*FString::Printf(TEXT("%s%lld"), Prefix, static_cast<long long>(Value)));
}

FName TicketTag(uint64 Id)
{
	return FName(*FString::Printf(TEXT("RokLocalBuildingId_%llu"), static_cast<unsigned long long>(Id)));
}

FName SourceModelTag(const FString& ModelId)
{
	return FName(*FString::Printf(TEXT("RokLocalSourceModel_%s"), *ModelId));
}

void ApplyOwnerCityTint(ARokCitySpriteActor& Actor)
{
	// Unity CityBuildingProxy sets RS.buildGreen for the player's own city.
	// TownBuilding passes it to _Color_1 in gamma space.
	const FName ColorParameter(TEXT("Color1"));
	UStaticMeshComponent* Mesh = Actor.GetSpriteMeshComponent();
	UMaterialInterface* Material = Mesh->GetMaterial(0);
	FLinearColor DefaultColor;
	if (Material && Material->GetVectorParameterValue(FMaterialParameterInfo(ColorParameter), DefaultColor))
	{
		if (UMaterialInstanceDynamic* Instance = Mesh->CreateDynamicMaterialInstance(0))
		{
			Instance->SetVectorParameterValue(ColorParameter, FLinearColor(62.0f / 255.0f, 125.0f / 255.0f, 195.0f / 255.0f));
		}
	}
}

bool MatchBounds(const UStaticMesh& Mesh, const FRokLocalSourceBuildingVisual& Visual)
{
	const FBoxSphereBounds Bounds = Mesh.GetBounds();
	const FVector Lo = Bounds.Origin - Bounds.BoxExtent;
	const FVector Hi = Bounds.Origin + Bounds.BoxExtent;
	return Lo.Equals(Visual.BoundsLo, BoundsToleranceCm)
		&& Hi.Equals(Visual.BoundsHi, BoundsToleranceCm);
}

bool ValidateSourceAsset(UStaticMesh* Mesh, UMaterialInterface* Material,
	const FRokLocalSourceBuildingVisual& Visual, FString& OutError)
{
	if (!IsValid(Mesh) || !IsValid(Material)
		|| Mesh->GetPathName() != Visual.MeshPath || Material->GetPathName() != Visual.MaterialPath
		|| Mesh->GetNumLODs() != 1 || Mesh->GetNumVertices(0) != Visual.Vertices
		|| Mesh->GetNumTriangles(0) != Visual.Triangles || Mesh->GetNumTexCoords(0) != 1
		|| !MatchBounds(*Mesh, Visual)
		|| (Visual.MeshDefaultMaterialPath
			? GetPathNameSafe(Mesh->GetMaterial(0)) != Visual.MeshDefaultMaterialPath
			: Mesh->GetMaterial(0) != Material))
	{
		OutError = FString::Printf(TEXT("%s source mesh, RG material, LOD0 or pivot differs from verified assets"),
			Visual.ModelId);
		return false;
	}
	return true;
}

bool ResolveSource(UWorld* World, const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request, const FRokLocalConstructionPreview& Preview,
	FSourceVisual& Out, FString& OutError)
{
	Out = FSourceVisual();
	OutError.Reset();
	const FRokLocalSourceBuildingVisual* Visual =
		FRokLocalConstructionVisualFactory::FindSourceVisual(Request.UnityBuildingTypeId);
	if (!IsValid(World) || !World->IsGameWorld() || !Frame.IsValid()
		|| !Preview.bContextVerified || !Preview.bSourceEligible || !Preview.bPositionEvaluated
		|| !Visual
		|| (Request.CivilizationId != 0 && Request.CivilizationId != 101)
		|| (Request.Age != 0 && Request.Age != 1)
		|| Preview.VisualCivilization != 101 || Preview.ResolvedAge != 1
		|| Preview.Footprint != FIntPoint(5, 5) || Preview.SourceCellIndices.Num() != 25)
	{
		OutError = TEXT("Only eligible source-visual 101 age-one 5x5 city construction is supported");
		return false;
	}
	Out.ModelId = Visual->ModelId;
	if (Preview.SourceModelId != Out.ModelId)
	{
		OutError = TEXT("Local construction source model ID differs from the verified visual variant");
		return false;
	}
	FRokLocalCitySimulationContext Context;
	FRokCityPlacementWorldFrame ResolvedFrame;
	if (!Context.Load(OutError) || !Context.ValidateWorld(World, OutError)
		|| Context.GetVisualCivilization() != 101 || Context.GetInitialAge() != 1
		|| !FRokCityPlacementWorldFrame::ResolveForWorld(World, ResolvedFrame, OutError)
		|| Frame.GetGridSize() != ResolvedFrame.GetGridSize()
		|| !Frame.GetCityContainerTransform().Equals(ResolvedFrame.GetCityContainerTransform(), 0.01))
	{
		if (OutError.IsEmpty()) OutError = TEXT("City source frame or world identity differs");
		return false;
	}
	Out.Footprint = {Request.Grid, Preview.Footprint};
	FVector CenterWorld;
	if (!Frame.GridCenterToWorld(Out.Footprint, CenterWorld, OutError))
	{
		return false;
	}
	Out.Actor = Frame.GetCityContainerTransform();
	Out.Actor.SetLocation(CenterWorld);
	Out.Child = FRokLocalConstructionVisualFactory::GetSourceChildTransform(*Visual);
	if (!Out.Actor.IsValid() || !Out.Child.IsValid())
	{
		OutError = TEXT("Source city root or child pose is invalid");
		return false;
	}
	const APlayerController* Controller = World->GetFirstPlayerController();
	const ARokStrategyCameraPawn* Camera = Controller
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	Out.CityDxf = Camera ? Camera->GetCurrentCityDxf() : 0.0f;
	if (!FMath::IsFinite(Out.CityDxf) || Out.CityDxf <= 0.0f || Out.CityDxf >= 3000.0f)
	{
		OutError = TEXT("Verified city camera LOD state is unavailable");
		return false;
	}
	return FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(*Visual, Out.Mesh, Out.Material, OutError);
}

bool HasForbiddenIdentity(const ARokCitySpriteActor& Actor)
{
	for (const FName& Tag : Actor.Tags)
	{
		const FString Value = Tag.ToString();
		if (Value.StartsWith(TEXT("UnityInitId_")) || Value == TEXT("RokUnityConfigInitBuilding")
			|| Value == TEXT("RokPlacementPreviewTemporary"))
		{
			return true;
		}
	}
	return false;
}
}

const FRokLocalSourceBuildingVisual* FRokLocalConstructionVisualFactory::FindSourceVisual(int32 TypeId)
{
	const FRokLocalSourceBuildingVisual* Visual = FindBuildingVisual(TypeId);
	return Visual && Visual->bLocalConstruction ? Visual : nullptr;
}

const FRokLocalSourceBuildingVisual* FRokLocalConstructionVisualFactory::FindBuildingVisual(int32 TypeId, const FString& ModelId)
{
	for (const FRokLocalSourceBuildingVisual& Visual : SourceVisuals)
	{
		if (Visual.TypeId == TypeId && (ModelId.IsEmpty() || ModelId == Visual.ModelId)) return &Visual;
	}
	if (!ModelId.IsEmpty())
	{
		for (const FRokLocalSourceBuildingVisual& Visual : ClassicalSourceVisuals)
		{
			if (Visual.TypeId == TypeId && ModelId == Visual.ModelId) return &Visual;
		}
		static const TMap<FString, const FRokLocalSourceBuildingVisual*> ByModel = []
		{
			TMap<FString, const FRokLocalSourceBuildingVisual*> Models;
			for (const FRokLocalSourceBuildingVisual& Visual : ImportedSourceVisuals)
			{
				Models.Add(FString::Printf(TEXT("%d:%s"), Visual.TypeId, Visual.ModelId), &Visual);
			}
			return Models;
		}();
		return ByModel.FindRef(FString::Printf(TEXT("%d:%s"), TypeId, *ModelId));
	}
	return nullptr;
}

FTransform FRokLocalConstructionVisualFactory::GetSourceChildTransform(const FRokLocalSourceBuildingVisual& Visual)
{
	const FVector& Position = Visual.ChildPosition;
	const double ScaleX = Visual.ChildScale;
	const double ScaleY = Visual.ChildVerticalScale > 0.0 ? Visual.ChildVerticalScale : ScaleX;
	const double NormalScale = Visual.ChildNormalScale > 0.0 ? Visual.ChildNormalScale : ScaleX;
	// All supported source prefabs share this child rotation after XYZ -> XZY reflection.
	const FQuat CityRotation(-0.3535533845424652, 0.1464466005563736,
		-0.3535533845424652, 0.8535534739494324);
	// Source OBJ uses 100 cm/unit. Keep a nonzero normal scale for planar meshes.
	return FTransform(CityRotation, FVector(Position.X, Position.Z, Position.Y),
		FVector(-ScaleX, NormalScale, ScaleY) / 100.0);
}

bool FRokLocalConstructionVisualFactory::LoadVerifiedSourceMesh(const FRokLocalSourceBuildingVisual& Visual,
	UStaticMesh*& OutMesh, UMaterialInterface*& OutMaterial, FString& OutError)
{
	OutMesh = Cast<UStaticMesh>(FSoftObjectPath(Visual.MeshPath).TryLoad());
	OutMaterial = Cast<UMaterialInterface>(FSoftObjectPath(Visual.MaterialPath).TryLoad());
	return ValidateSourceAsset(OutMesh, OutMaterial, Visual, OutError);
}

bool FRokLocalConstructionVisualFactory::ApplyInitialBuildingVisual(ARokCitySpriteActor& Actor,
	const FRokCityPlacementWorldFrame& Frame, int32 TypeId, const FString& ModelId, FString& OutError)
{
	OutError.Reset();
	const FName ModelTag = SourceModelTag(ModelId);
	if (!ModelId.IsEmpty() && Actor.ActorHasTag(ModelTag)) return true;
	const FRokLocalSourceBuildingVisual* Visual = ModelId.IsEmpty()
		|| Actor.ActorHasTag(FName(*FString::Printf(TEXT("UnityModel_%s"), *ModelId)))
		? nullptr : FindBuildingVisual(TypeId, ModelId);
	if (Visual)
	{
		UStaticMesh* Mesh = nullptr;
		UMaterialInterface* Material = nullptr;
		if (!LoadVerifiedSourceMesh(*Visual, Mesh, Material, OutError)) return false;

		FTransform SourceRoot = Frame.GetCityContainerTransform();
		SourceRoot.SetLocation(Actor.GetVisualAnchorWorldPosition());
		const FTransform MeshWorld = GetSourceChildTransform(*Visual) * SourceRoot;
		Actor.SetRuntimeSourceMesh(Mesh, Material, MeshWorld.GetRelativeTransform(Actor.GetActorTransform()));
		ApplyOwnerCityTint(Actor);
	}
	else
	{
		Actor.RestoreMapSourceMesh();
	}
	Actor.Tags.RemoveAll([](FName Tag) { return Tag.ToString().StartsWith(TEXT("RokLocalSourceModel_")); });
	if (Visual)
	{
		Actor.Tags.Add(ModelTag);
		UE_LOG(LogTemp, Display, TEXT("Rok server city: initial building %s uses %s."), *Actor.GetName(), *ModelId);
	}
	return true;
}

ARokCitySpriteActor* FRokLocalConstructionVisualFactory::SpawnServerBuilding(UWorld* World,
	const FRokCityPlacementWorldFrame& Frame, const FRokLocalSourceBuildingVisual& Visual,
	const FIntPoint& Origin, const FIntPoint& Footprint, FString& OutError)
{
	OutError.Reset();
	if (!IsValid(World) || !Frame.IsValid() || Footprint.X <= 0 || Footprint.Y <= 0)
	{
		OutError = TEXT("Server building needs a game world, a city frame and a footprint");
		return nullptr;
	}
	const FRokCityConstructionFootprint Cells{Origin, Footprint};
	FVector CenterWorld;
	if (!Frame.GridCenterToWorld(Cells, CenterWorld, OutError))
	{
		return nullptr;
	}
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	if (!LoadVerifiedSourceMesh(Visual, Mesh, Material, OutError))
	{
		return nullptr;
	}
	FTransform ActorTransform = Frame.GetCityContainerTransform();
	ActorTransform.SetLocation(CenterWorld);
	const APlayerController* Controller = World->GetFirstPlayerController();
	const ARokStrategyCameraPawn* Camera = Controller
		? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	float CityDxf = Camera ? Camera->GetCurrentCityDxf() : 0.0f;
	if (!FMath::IsFinite(CityDxf) || CityDxf <= 0.0f || CityDxf >= 3000.0f)
	{
		CityDxf = 150.0f; // CameraParamLod city_default
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>(
		ARokCitySpriteActor::StaticClass(), ActorTransform, Params);
	if (!IsValid(Actor))
	{
		OutError = TEXT("Server building actor could not be spawned");
		return nullptr;
	}
	Actor->ConfigureSourceMesh(Mesh, Material, GetSourceChildTransform(Visual), 2995);
	ApplyOwnerCityTint(*Actor);
	Actor->ConfigurePlacement(Footprint, Origin, FVector::ZeroVector);
	Actor->ConfigureUnityCameraLod(FVector::ZeroVector, CityDxf);
	Actor->SetSelectionCollisionEnabled(true);
	Actor->Tags.AddUnique(FName(ServerBuildingTag));
	Actor->Tags.AddUnique(TEXT("RokBuilding"));
	Actor->Tags.AddUnique(SourceModelTag(Visual.ModelId));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityBuildingType_"), Visual.TypeId));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridX_"), Origin.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridY_"), Origin.Y));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridWidth_"), Footprint.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridLength_"), Footprint.Y));
	return Actor;
}

bool FRokLocalConstructionVisualFactory::Preflight(UWorld* World,
	const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request,
	const FRokLocalConstructionPreview& Preview, FString& OutError)
{
	FSourceVisual Source;
	return ResolveSource(World, Frame, Request, Preview, Source, OutError);
}

bool FRokLocalConstructionVisualFactory::Prepare(UWorld* World,
	const FRokCityPlacementWorldFrame& Frame,
	const FRokLocalConstructionRequest& Request,
	const FRokLocalConstructionPreview& Preview,
	ARokCitySpriteActor*& OutHiddenActor, FString& OutError)
{
	OutHiddenActor = nullptr;
	FSourceVisual Source;
	if (!ResolveSource(World, Frame, Request, Preview, Source, OutError)) return false;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>(
		ARokCitySpriteActor::StaticClass(), Source.Actor, Params);
	if (!IsValid(Actor))
	{
		OutError = TEXT("Source city building actor could not be spawned");
		return false;
	}
	Actor->SetActorHiddenInGame(true);
	Actor->SetSelectionCollisionEnabled(false);
	Actor->ConfigureSourceMesh(Source.Mesh, Source.Material, Source.Child, 2995);
	Actor->ConfigurePlacement(Source.Footprint.Size, Source.Footprint.Origin, FVector::ZeroVector);
	Actor->ConfigureUnityCameraLod(FVector::ZeroVector, Source.CityDxf);
	UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
	if (!Component || Component->GetStaticMesh().Get() != Source.Mesh
		|| Component->GetMaterial(0) != Source.Material
		|| Actor->GetGameplayTile() != Source.Footprint.Origin
		|| Actor->GetFootprintSize() != Source.Footprint.Size
		|| !Actor->GetActorTransform().Equals(Source.Actor, 0.01))
	{
		Actor->Destroy();
		OutError = TEXT("Prepared source actor did not preserve mesh, material, grid or city pose");
		return false;
	}
	Actor->Tags.AddUnique(SourceModelTag(Source.ModelId));
	Actor->Tags.AddUnique(FName(SourceLevelZeroTag));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityBuildingType_"), Request.UnityBuildingTypeId));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridX_"), Request.Grid.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridY_"), Request.Grid.Y));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridWidth_"), Source.Footprint.Size.X));
	Actor->Tags.AddUnique(IndexedTag(TEXT("UnityGridLength_"), Source.Footprint.Size.Y));
	OutHiddenActor = Actor;
	OutError.Reset();
	return true;
}

void FRokLocalConstructionVisualFactory::ActivatePending(
	ARokCitySpriteActor& Actor, const FRokLocalConstructionTicket& Ticket)
{
	Actor.Tags.AddUnique(TicketTag(Ticket.Id));
	Actor.Tags.AddUnique(FName(PendingTag));
	Actor.SetActorHiddenInGame(false);
}

bool FRokLocalConstructionVisualFactory::ValidateHeldActor(const ARokCitySpriteActor& Actor,
	const FRokLocalConstructionTicket& Ticket, FString& OutError)
{
	OutError.Reset();
	const int32 TypeId = Ticket.TypeId;
	const FRokLocalSourceBuildingVisual* Visual = FindSourceVisual(TypeId);
	const UStaticMeshComponent* Component = Actor.GetSpriteMeshComponent();
	if (!Visual || Ticket.Civilization != 101 || Ticket.Age != 1
		|| Ticket.Id == 0 || Ticket.Footprint.Size != FIntPoint(5, 5)
		|| !Actor.Tags.Contains(FName(PendingTag)) || !Actor.Tags.Contains(TicketTag(Ticket.Id))
		|| !Actor.Tags.Contains(SourceModelTag(Visual->ModelId))
		|| !Actor.Tags.Contains(FName(SourceLevelZeroTag)) || HasForbiddenIdentity(Actor)
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityBuildingType_"), TypeId))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridX_"), Ticket.Footprint.Origin.X))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridY_"), Ticket.Footprint.Origin.Y))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridWidth_"), Ticket.Footprint.Size.X))
		|| !Actor.Tags.Contains(IndexedTag(TEXT("UnityGridLength_"), Ticket.Footprint.Size.Y))
		|| Actor.GetGameplayTile() != Ticket.Footprint.Origin
		|| Actor.GetFootprintSize() != Ticket.Footprint.Size || Actor.IsHidden()
		|| !Component || Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision
		|| !ValidateSourceAsset(Component ? Component->GetStaticMesh().Get() : nullptr,
			Component ? Component->GetMaterial(0) : nullptr, *Visual, OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Pending local actor source identity or footprint differs");
		return false;
	}
	FRokCityPlacementWorldFrame Frame;
	FVector CenterWorld;
	if (!FRokCityPlacementWorldFrame::ResolveForWorld(Actor.GetWorld(), Frame, OutError)
		|| !Frame.GridCenterToWorld(Ticket.Footprint, CenterWorld, OutError))
	{
		return false;
	}
	FTransform ExpectedActor = Frame.GetCityContainerTransform();
	ExpectedActor.SetLocation(CenterWorld);
	const FTransform Child = GetSourceChildTransform(*Visual);
	const double Lod = Actor.GetUnityLodScale();
	if (!FMath::IsFinite(Lod) || Lod <= 0.0 || Lod > 7.8
		|| !Actor.GetActorTransform().Equals(ExpectedActor, 0.01)
		|| !Component->GetRelativeRotation().Equals(Child.GetRotation().Rotator(), 0.01)
		|| !Component->GetRelativeLocation().Equals(Child.GetLocation() * Lod, 0.001)
		|| !Component->GetRelativeScale3D().Equals(Child.GetScale3D() * Lod, 0.00001))
	{
		OutError = TEXT("Pending local actor source pivot, child transform or LOD differs");
		return false;
	}
	return true;
}

void FRokLocalConstructionVisualFactory::MarkCompletedVisual(
	ARokCitySpriteActor& Actor, const FRokLocalConstructionTicket&)
{
	Actor.Tags.Remove(FName(PendingTag));
	Actor.Tags.Remove(FName(SourceLevelZeroTag));
	Actor.Tags.AddUnique(FName(SourceLevelOneTag));
	Actor.SetSelectionCollisionEnabled(true);
}
