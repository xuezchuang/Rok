#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "../RokStrategyCameraPawn.h"
#include "../RokWorldMapCameraPawn.h"
#include "../RokWorldMapCityActor.h"
#include "../RokWorldMapCityPulse.h"
#include "../RokWorldMapGameMode.h"
#include "../RokWorldMapTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityPulseTest, "Rok.WorldMap.City.Pulse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityPulseTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Own city scale starts at source zero key"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(3000), 0.0f);
	TestEqual(TEXT("Own city first visible key"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(3000.001f), 6.0f);
	TestTrue(TEXT("Own city explore midpoint"), FMath::IsNearlyEqual(ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(4200), 6.2f, 0.00001f));
	TestEqual(TEXT("Own city overview endpoint"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(5400), 6.4f);
	TestTrue(TEXT("Own city remains scaled in Strategic"), FMath::IsNearlyEqual(
		ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(20169.9375f), 20.818f, 0.01f));
	TestEqual(TEXT("Own city source far key"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(44939.875f), 45.0f);
	TestEqual(TEXT("Own city Nationwide key"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(72720), 55.0f);
	TestEqual(TEXT("Curve clamps after final key"), ARokWorldMapCityActor::EvaluateOwnCityOverviewScale(80000), 55.0f);
	using namespace RokWorldMapCityPulse;
	TestEqual(TEXT("Burst starts at source size"), Evaluate(0, 3).X, 0.49377325);
	TestTrue(TEXT("Burst reaches alpha peak"), FMath::IsNearlyEqual(
		Evaluate(1.5 * 8288.0 / 65535.0, 3).Y, 0.3137255, 0.00001));
	TestTrue(TEXT("Dead gap hides burst"), Evaluate(1.7, 3).IsNearlyZero());
	TestTrue(TEXT("Burst repeats after 2.2 seconds"), Evaluate(0.5, 3).Equals(Evaluate(2.7, 3), 0.00001));
	TestTrue(TEXT("Prewarm has visible particles immediately"), Evaluate(0, 1).Y > 0 && Evaluate(0, 2).Y > 0);
	TestTrue(TEXT("Invalid particle is rejected"), Evaluate(0, 4).IsNearlyZero());
	TestTrue(TEXT("Negative time is rejected"), Evaluate(-1, 0).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapCoordinateRoundTripTest,
	"Rok.WorldMap.Projection.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCoordinateRoundTripTest::RunTest(const FString& Parameters)
{
	const FRokWorldMapProjection Projection;
	TestTrue(TEXT("Default world-map projection is valid"), Projection.IsValid());

	const FVector2D SourceUnity(6543.25f, 1234.5f);
	const FVector UePosition = Projection.UnityToUe(SourceUnity);
	const FVector2D RoundTripUnity = Projection.UeToUnity(UePosition);
	TestTrue(TEXT("Unity -> rebased UE -> Unity round trip is stable"),
		RoundTripUnity.Equals(SourceUnity, 0.001f));
	TestTrue(TEXT("Projection always uses the configured ground plane"),
		FMath::IsNearlyEqual(UePosition.Z, Projection.GroundZ));

	const FVector FocusUe = Projection.UnityToUe(Projection.UnityRebaseOrigin);
	TestTrue(TEXT("The authoritative focus is the UE local origin"),
		FocusUe.Equals(FVector(0.0f, 0.0f, Projection.GroundZ), 0.001f));
	TestTrue(TEXT("World-map directions use the same mirrored-X basis as positions"),
		FRokWorldMapMath::UnityDirectionToUe(FVector(1.0f, 0.0f, 0.0f)).Equals(
			FVector(-1.0f, 0.0f, 0.0f),
			0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapHudCoordinateTest,
	"Rok.WorldMap.Coordinates.HudFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapHudCoordinateTest::RunTest(const FString& Parameters)
{
	const FRokWorldMapProjection Projection;
	const FVector2D HudCoordinate(1146.0f, 157.0f);
	const FVector2D UnityCoordinate = Projection.DisplayToUnity(HudCoordinate);
	TestTrue(TEXT("HUD X:1146 Y:157 resolves to Unity 6876,942"),
		UnityCoordinate.Equals(FVector2D(6876.0f, 942.0f), 0.001f));
	TestTrue(TEXT("Unity focus converts back to HUD X:1146 Y:157"),
		Projection.UnityToDisplay(UnityCoordinate).Equals(HudCoordinate, 0.001f));
	TestTrue(TEXT("HUD focus is rebased to UE origin"),
		Projection.UnityToUe(UnityCoordinate).Equals(
			FVector(0.0f, 0.0f, Projection.GroundZ),
			0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapCameraKeysTest,
	"Rok.WorldMap.Camera.DefaultKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCameraKeysTest::RunTest(const FString& Parameters)
{
	const TArray<FRokCityViewCameraKey> CameraKeys = FRokCityViewMath::MakeDefaultCameraKeys();
	TestTrue(TEXT("Default CameraParamLod keys are sorted by dxf"),
		FRokWorldMapMath::AreCameraKeysSortedByDxf(CameraKeys));

	const float MinimumDxf = FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		FRokWorldMapMath::MinimumCameraKey(),
		-1.0f);
	const float TacticalDxf = FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		FRokWorldMapMath::TacticalCameraKey(),
		-1.0f);
	const float CityEntryThresholdDxf = FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		FRokWorldMapMath::CityEntryThresholdCameraKey(),
		-1.0f);
	const float CityEntryTargetDxf = FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		FRokWorldMapMath::CityEntryTargetCameraKey(),
		-1.0f);
	const float MaximumDxf = FRokWorldMapMath::GetNamedDxf(
		CameraKeys,
		FRokWorldMapMath::MaximumCameraKey(),
		-1.0f);
	TestTrue(TEXT("limit_min matches the Unity CameraParamLod source"),
		FMath::IsNearlyEqual(MinimumDxf, 76.691f, 0.001f));
	TestTrue(TEXT("map_tactical matches the Unity CameraParamLod source"),
		FMath::IsNearlyEqual(TacticalDxf, 1509.0f, 0.001f));
	TestTrue(TEXT("city_bound matches the Unity automatic city-entry threshold"),
		FMath::IsNearlyEqual(CityEntryThresholdDxf, 250.0f, 0.001f));
	TestTrue(TEXT("city_default matches the Unity city-entry target"),
		FMath::IsNearlyEqual(CityEntryTargetDxf, 150.0f, 0.001f));
	TestTrue(TEXT("limit_max matches the Unity CameraParamLod source"),
		FMath::IsNearlyEqual(MaximumDxf, 72750.0f, 0.001f));
	TestTrue(TEXT("Named lower dxf clamp is enforced"),
		FMath::IsNearlyEqual(
			FRokWorldMapMath::ClampDxfByNamedKeys(
				CameraKeys,
				0.0f,
				FRokWorldMapMath::MinimumCameraKey(),
				FRokWorldMapMath::MaximumCameraKey()),
			MinimumDxf,
			0.001f));
	TestTrue(TEXT("Named upper dxf clamp is enforced"),
		FMath::IsNearlyEqual(
			FRokWorldMapMath::ClampDxfByNamedKeys(
				CameraKeys,
				200000.0f,
				FRokWorldMapMath::MinimumCameraKey(),
				FRokWorldMapMath::MaximumCameraKey()),
			MaximumDxf,
			0.001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapPlayerCityEntryTest,
	"Rok.WorldMap.City.EntryZoom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapPlayerCityEntryTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Zooming in across city_bound starts player-city entry"),
		FRokWorldMapMath::ShouldBeginPlayerCityEntry(
			288.316f,
			244.336f,
			250.0f));
	TestTrue(
		TEXT("Further zoom-in can still enter when already inside city_bound"),
		FRokWorldMapMath::ShouldBeginPlayerCityEntry(
			200.0f,
			169.491f,
			250.0f));
	TestFalse(
		TEXT("Zooming out never starts player-city entry"),
		FRokWorldMapMath::ShouldBeginPlayerCityEntry(
			244.336f,
			288.316f,
			250.0f));
	TestFalse(
		TEXT("Zooming in above city_bound stays on the world map"),
		FRokWorldMapMath::ShouldBeginPlayerCityEntry(
			400.0f,
			338.983f,
			250.0f));
	TestTrue(
		TEXT("The stable player-city tag matches generated world-map actors"),
		FRokWorldMapMath::WorldMapPlayerCityTag()
			== FName(TEXT("RokWorldMapPlayerCity")));
	TestTrue(
		TEXT("city_default dxf maps to the continuous city camera distance"),
		FMath::IsNearlyEqual(
			ARokStrategyCameraPawn::CalculateCameraDistanceForDxf(
				150.0f,
				7.5f,
				500.0f),
			10000.0f,
			0.001f));
	TestFalse(
		TEXT("City zoom-out below the boundary stays in the city"),
		ARokStrategyCameraPawn::ShouldReturnToWorldMap(
			150.0f,
			244.336f,
			250.0f));
	TestTrue(
		TEXT("City zoom-out across the boundary returns to the world map"),
		ARokStrategyCameraPawn::ShouldReturnToWorldMap(
			244.336f,
			288.316f,
			250.0f));
	TestFalse(
		TEXT("City zoom-in never returns to the world map"),
		ARokStrategyCameraPawn::ShouldReturnToWorldMap(
			288.316f,
			244.336f,
			250.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapBoundsAndSelectionTest,
	"Rok.WorldMap.BoundsAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapBoundsAndSelectionTest::RunTest(const FString& Parameters)
{
	const FRokWorldMapProjection Projection;
	TestTrue(TEXT("World-map focus clamps to the authoritative 0..7200 range"),
		Projection.ClampUnity(FVector2D(-10.0f, 8000.0f)).Equals(
			FVector2D(0.0f, 7200.0f),
			0.001f));

	TArray<FName> WorldOnlyTags = {FRokWorldMapMath::WorldMapSelectableTag()};
	TestTrue(TEXT("The dedicated world-map tag is selectable"),
		FRokWorldMapMath::AreTagsWorldMapSelectable(WorldOnlyTags));
	TestFalse(TEXT("Untagged actors are never selected"),
		FRokWorldMapMath::AreTagsWorldMapSelectable(TArray<FName>()));

	WorldOnlyTags.Add(FName(TEXT("CityBuilding")));
	TestFalse(TEXT("A city-tagged actor is rejected even if it also has the world tag"),
		FRokWorldMapMath::AreTagsWorldMapSelectable(WorldOnlyTags));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapCollisionPolicyTest,
	"Rok.WorldMap.Collision.SelectionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCollisionPolicyTest::RunTest(const FString& Parameters)
{
	UStaticMeshComponent* TerrainComponent =
		NewObject<UStaticMeshComponent>(GetTransientPackage());
	TerrainComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics);
	TerrainComponent->SetCollisionResponseToAllChannels(ECR_Block);
	const TArray<FName> TerrainTags = {
		FRokWorldMapMath::WorldMapTerrainTag(),
	};
	TestTrue(
		TEXT("Terrain is handled by the world-map collision policy"),
		ARokWorldMapGameMode::ApplyWorldMapCollisionPolicy(
			TerrainTags,
			TerrainComponent));
	TestEqual(
		TEXT("Terrain cannot intercept selection traces"),
		TerrainComponent->GetCollisionEnabled(),
		ECollisionEnabled::NoCollision);
	TestEqual(
		TEXT("Terrain ignores Visibility after normalization"),
		TerrainComponent->GetCollisionResponseToChannel(ECC_Visibility),
		ECR_Ignore);

	UStaticMeshComponent* CityVisualComponent =
		NewObject<UStaticMeshComponent>(GetTransientPackage());
	CityVisualComponent->SetCollisionEnabled(
		ECollisionEnabled::QueryAndPhysics);
	const TArray<FName> SelectableTags = {
		FRokWorldMapMath::WorldMapSelectableTag(),
	};
	TestTrue(
		TEXT("Selectable city visual is handled by the world-map collision policy"),
		ARokWorldMapGameMode::ApplyWorldMapCollisionPolicy(
			SelectableTags,
			CityVisualComponent));
	TestEqual(
		TEXT("Selectable city visual cannot intercept selection traces"),
		CityVisualComponent->GetCollisionEnabled(),
		ECollisionEnabled::NoCollision);

	UStaticMeshComponent* CitySelectionComponent =
		NewObject<UStaticMeshComponent>(GetTransientPackage());
	CitySelectionComponent->ComponentTags.Add(
		FRokWorldMapMath::WorldMapSelectionColliderTag());
	CitySelectionComponent->SetCollisionEnabled(
		ECollisionEnabled::NoCollision);
	TestTrue(
		TEXT("Dedicated city collider is handled by the world-map collision policy"),
		ARokWorldMapGameMode::ApplyWorldMapCollisionPolicy(
			SelectableTags,
			CitySelectionComponent));
	TestEqual(
		TEXT("Dedicated city collider is query-only"),
		CitySelectionComponent->GetCollisionEnabled(),
		ECollisionEnabled::QueryOnly);
	TestEqual(
		TEXT("Dedicated city collider blocks Visibility selection traces"),
		CitySelectionComponent->GetCollisionResponseToChannel(ECC_Visibility),
		ECR_Block);
	TestEqual(
		TEXT("Dedicated city collider ignores camera traces"),
		CitySelectionComponent->GetCollisionResponseToChannel(ECC_Camera),
		ECR_Ignore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapTreeLodTest,
	"Rok.WorldMap.Tree.UnityScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapTreeLodTest::RunTest(const FString& Parameters)
{
	const TArray<FName> ExplicitTreeTags = {
		FRokWorldMapMath::WorldMapTreeTag(),
	};
	TestTrue(
		TEXT("Explicit world-map tree tags enter runtime LOD binding"),
		FRokWorldMapMath::IsWorldMapTree(
			ExplicitTreeTags,
			NAME_None));
	TestTrue(
		TEXT("The base Unity tree material enters runtime LOD binding"),
		FRokWorldMapMath::IsWorldMapTree(
			TArray<FName>(),
			FName(TEXT("M_WorldSource_map_tree_nomask"))));
	TestTrue(
		TEXT("The Unity tree material variant enters runtime LOD binding"),
		FRokWorldMapMath::IsWorldMapTree(
			TArray<FName>(),
			FName(TEXT("M_WorldSource_map_tree_nomask_1"))));
	TestFalse(
		TEXT("Non-tree source materials stay out of runtime tree LOD binding"),
		FRokWorldMapMath::IsWorldMapTree(
			TArray<FName>(),
			FName(TEXT("M_WorldSource_build_mountain_A"))));
	TestTrue(
		TEXT("Unity tree scale starts at the first GameApp curve key"),
		FMath::IsNearlyEqual(
			ARokWorldMapCameraPawn::EvaluateUnityTreeScale(80.0f),
			0.23f,
			0.000001f));
	TestTrue(
		TEXT("Unity tree scale matches the source curve at map_tactical"),
		FMath::IsNearlyEqual(
			ARokWorldMapCameraPawn::EvaluateUnityTreeScale(1509.0f),
			1.0352981926f,
			0.000001f));
	TestTrue(
		TEXT("Unity tree scale reaches the source plateau"),
		FMath::IsNearlyEqual(
			ARokWorldMapCameraPawn::EvaluateUnityTreeScale(1800.3723f),
			1.2f,
			0.000001f));
	TestTrue(
		TEXT("Unity tree scale remains on the source plateau at limit_max"),
		FMath::IsNearlyEqual(
			ARokWorldMapCameraPawn::EvaluateUnityTreeScale(72750.0f),
			1.2f,
			0.000001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokWorldMapCityLodTest,
	"Rok.WorldMap.City.UnityLod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapCityLodTest::RunTest(const FString& Parameters)
{
	constexpr float AgeOneLodCap = 7.8f * 41.0f / 57.0f;
	TestTrue(TEXT("Unity city LOD starts at one at dxf 250"),
		FMath::IsNearlyEqual(
			ARokWorldMapCityActor::EvaluateUnityCityLodScale(
				250.0f,
				AgeOneLodCap),
			1.0f,
			0.0001f));
	TestTrue(TEXT("Age-one city cap applies at dxf 1232.823"),
		FMath::IsNearlyEqual(
			ARokWorldMapCityActor::EvaluateUnityCityLodScale(
				1232.823f,
				AgeOneLodCap),
			AgeOneLodCap,
			0.0001f));
	TestTrue(TEXT("Age-one city cap applies at map_tactical dxf 1509"),
		FMath::IsNearlyEqual(
			ARokWorldMapCityActor::EvaluateUnityCityLodScale(
				1509.0f,
				AgeOneLodCap),
			AgeOneLodCap,
			0.0001f));
	TestTrue(TEXT("Unity city visual is hidden at dxf 3000"),
		FMath::IsNearlyZero(
			ARokWorldMapCityActor::EvaluateUnityCityLodScale(
				3000.0f,
				AgeOneLodCap),
			0.0001f));
	return true;
}

#endif
