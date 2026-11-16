#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Camera/CameraComponent.h"
#include "Components/Button.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/ScopeExit.h"
#include "Materials/Material.h"
#include "UObject/UObjectIterator.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokStrategyCameraPawn.h"
#include "../RokUnityFeatureScreenWidget.h"
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
	FRokCityCameraFramingTest,
	"Rok.WorldMap.City.Framing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraFramingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Default city dxf matches Unity city_default"),
		ARokStrategyCameraPawn::GetDefaultCityDxf(), 150.0f);
	TestTrue(TEXT("Default city dxf sits below the world-map return threshold"),
		ARokStrategyCameraPawn::GetDefaultCityDxf() < 250.0f);
	TestTrue(TEXT("City min dxf matches Unity CameraParamLod min"),
		FMath::IsNearlyEqual(ARokStrategyCameraPawn::GetCityMinDxf(), 82.4551f, 0.001f));
	TestTrue(TEXT("City max dxf matches Unity CameraParamLod city_bound"),
		FMath::IsNearlyEqual(ARokStrategyCameraPawn::GetCityMaxDxf(), 250.0f, 0.001f));
	TestTrue(TEXT("Default camera distance maps to the default city dxf"),
		FMath::IsNearlyEqual(
			ARokStrategyCameraPawn::CalculateCameraDistanceForDxf(
				ARokStrategyCameraPawn::GetDefaultCityDxf(), 7.5f, 500.0f),
			10000.0f, 0.001f));
	TestTrue(TEXT("City min is below the default and the default is below the max"),
		ARokStrategyCameraPawn::GetCityMinDxf() < ARokStrategyCameraPawn::GetDefaultCityDxf()
		&& ARokStrategyCameraPawn::GetDefaultCityDxf() < ARokStrategyCameraPawn::GetCityMaxDxf());

	// CameraParamLod forward is (0, -0.7, 0.7) for every key; Unity (x, y, z) maps to
	// UE (-x, z, y). A yaw of 112 here rotated the whole city ~20 degrees against Unity.
	const ARokStrategyCameraPawn* Pawn = GetDefault<ARokStrategyCameraPawn>();
	const FFloatProperty* YawProperty = FindFProperty<FFloatProperty>(
		ARokStrategyCameraPawn::StaticClass(), TEXT("DefaultCameraYaw"));
	const FFloatProperty* PitchProperty = FindFProperty<FFloatProperty>(
		ARokStrategyCameraPawn::StaticClass(), TEXT("DefaultCameraPitch"));
	if (TestNotNull(TEXT("Default yaw property"), YawProperty)
		&& TestNotNull(TEXT("Default pitch property"), PitchProperty))
	{
		const FRotator Rotation(PitchProperty->GetPropertyValue_InContainer(Pawn),
			YawProperty->GetPropertyValue_InContainer(Pawn), 0.0f);
		const FVector UnityForward(0.0, -0.7, 0.7);
		const FVector Expected = FVector(-UnityForward.X, UnityForward.Z, UnityForward.Y).GetSafeNormal();
		TestTrue(TEXT("Camera forward matches Unity CameraParamLod forward"),
			Rotation.Vector().Equals(Expected, 1.0e-4));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityCameraWheelZoomTest,
	"Rok.WorldMap.City.WheelZoom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraWheelZoomTest::RunTest(const FString& Parameters)
{
	// Unity Skyunion InputManager passes scrollRate = 1/(mouseWheel + 1) with
	// axis sensitivity 0.1: one notch multiplies dxf by 10/9 out, 10/11 in.
	const float ZoomOutFactor = 10.0f / 9.0f;
	const float ZoomInFactor = 10.0f / 11.0f;

	float OutDxf = ARokStrategyCameraPawn::GetDefaultCityDxf();
	for (int32 Notch = 1; Notch <= 10; ++Notch)
	{
		OutDxf = ARokStrategyCameraPawn::CalculateWheelZoomDxf(OutDxf, 1.0f);
		TestTrue(
			FString::Printf(TEXT("Zoom-out notch %d gives 150*(10/9)^%d"), Notch, Notch),
			FMath::IsNearlyEqual(OutDxf, 150.0f * FMath::Pow(ZoomOutFactor, static_cast<float>(Notch)), 0.01f));
	}

	float InDxf = ARokStrategyCameraPawn::GetCityMaxDxf();
	for (int32 Notch = 1; Notch <= 10; ++Notch)
	{
		InDxf = ARokStrategyCameraPawn::CalculateWheelZoomDxf(InDxf, -1.0f);
		TestTrue(
			FString::Printf(TEXT("Zoom-in notch %d gives 250*(10/11)^%d"), Notch, Notch),
			FMath::IsNearlyEqual(InDxf, 250.0f * FMath::Pow(ZoomInFactor, static_cast<float>(Notch)), 0.01f));
	}

	TestEqual(TEXT("Zero zoom direction keeps the dxf"),
		ARokStrategyCameraPawn::CalculateWheelZoomDxf(150.0f, 0.0f), 150.0f);
	TestEqual(TEXT("Non-positive dxf is passed through untouched"),
		ARokStrategyCameraPawn::CalculateWheelZoomDxf(0.0f, 1.0f), 0.0f);

	// Frame-rate independence on live pawns: one call is one wheel notch and the
	// step is a pure dxf multiplication, so 30 fps and 120 fps must land on the
	// same framing. The pawns need a world because ApplyCameraDefaults moves
	// them and drives their camera component.
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCityCameraWheelZoomTest"));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Isolated test world for the live pawns"), World))
	{
		Instance->Shutdown();
		return false;
	}

	ARokStrategyCameraPawn* SlowPawn = World->SpawnActor<ARokStrategyCameraPawn>();
	ARokStrategyCameraPawn* FastPawn = World->SpawnActor<ARokStrategyCameraPawn>();
	if (TestNotNull(TEXT("30 fps pawn"), SlowPawn)
		&& TestNotNull(TEXT("120 fps pawn"), FastPawn))
	{
		for (int32 Notch = 0; Notch < 6; ++Notch)
		{
			SlowPawn->ApplyZoomInput(-1.0f, 1.0f / 30.0f);
			FastPawn->ApplyZoomInput(-1.0f, 1.0f / 120.0f);
		}
		TestTrue(TEXT("30 fps and 120 fps zoom-in produce the same dxf"),
			FMath::IsNearlyEqual(SlowPawn->GetCurrentCityDxf(), FastPawn->GetCurrentCityDxf(), 0.0001f));
		TestTrue(TEXT("Six zoom-in notches give 150*(10/11)^6"),
			FMath::IsNearlyEqual(SlowPawn->GetCurrentCityDxf(), 84.6711f, 0.01f));

		// Stay below the 250 world-map return threshold on the zoom-out leg.
		ARokStrategyCameraPawn* SlowOut = World->SpawnActor<ARokStrategyCameraPawn>();
		ARokStrategyCameraPawn* FastOut = World->SpawnActor<ARokStrategyCameraPawn>();
		if (TestNotNull(TEXT("30 fps zoom-out pawn"), SlowOut)
			&& TestNotNull(TEXT("120 fps zoom-out pawn"), FastOut))
		{
			for (int32 Notch = 0; Notch < 4; ++Notch)
			{
				SlowOut->ApplyZoomInput(1.0f, 1.0f / 30.0f);
				FastOut->ApplyZoomInput(1.0f, 1.0f / 120.0f);
			}
			TestTrue(TEXT("30 fps and 120 fps zoom-out produce the same dxf"),
				FMath::IsNearlyEqual(SlowOut->GetCurrentCityDxf(), FastOut->GetCurrentCityDxf(), 0.0001f));
			TestTrue(TEXT("Four zoom-out notches give 150*(10/9)^4"),
				FMath::IsNearlyEqual(SlowOut->GetCurrentCityDxf(), 228.6237f, 0.01f));
			SlowOut->Destroy();
			FastOut->Destroy();
		}

		TestFalse(TEXT("Zero zoom direction is rejected"),
			SlowPawn->ApplyZoomInput(0.0f, 1.0f / 60.0f));
		TestFalse(TEXT("Non-positive delta seconds is rejected"),
			SlowPawn->ApplyZoomInput(1.0f, 0.0f));
		SlowPawn->Destroy();
		FastPawn->Destroy();
	}

	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityCameraCommanderWheelTest, "Rok.WorldMap.City.CommanderWheel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraCommanderWheelTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCityCameraCommanderWheelTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Controller = World->SpawnActor<ARokPlayerController>();
	auto* Pawn = World->SpawnActor<ARokStrategyCameraPawn>();
	Controller->Possess(Pawn);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);

	auto* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestTrue(TEXT("City HUD initializes"), Hud->Initialize())) return false;
	Hud->TakeWidget();
	auto* Feature = Hud->GetFeatureScreenWidgetForPlacement();
	const auto* HudProperty = FindFProperty<FObjectPropertyBase>(ARokPlayerController::StaticClass(), TEXT("MainHudWidget"));
	if (!TestNotNull(TEXT("City HUD property"), HudProperty) || !TestNotNull(TEXT("Real feature host"), Feature)) return false;
	HudProperty->SetObjectPropertyValue_InContainer(Controller, Hud);

	auto Wheel = [&](const FKey& Key)
	{
		constexpr float Step = 1.0f / 60.0f;
		const auto Device = FInputDeviceId::CreateFromInternalId(0);
		const uint64 Timestamp = FPlatformTime::Cycles64();
		Controller->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, Device, Key, IE_Pressed, Timestamp));
		Controller->PlayerInput->InputKey(FInputKeyEventArgs(nullptr, Device, Key, IE_Released, Timestamp));
		Controller->PlayerInput->ProcessInputStack({}, Step, false);
		TestTrue(TEXT("Controller received the wheel notch"), Controller->WasInputKeyJustPressed(Key));
		Pawn->Tick(Step);
	};

	Feature->OpenCommander();
	TestTrue(TEXT("Commander blocks map input"), Controller->IsCityCameraInputBlocked());
	const float Before = Pawn->GetCurrentCityDxf();
	Wheel(EKeys::MouseScrollUp);
	TestEqual(TEXT("Commander blocks zoom-in behind it"), Pawn->GetCurrentCityDxf(), Before);
	Wheel(EKeys::MouseScrollDown);
	TestEqual(TEXT("Commander blocks zoom-out behind it"), Pawn->GetCurrentCityDxf(), Before);
	Feature->CloseScreen();
	TestFalse(TEXT("Closing commander restores map input"), Controller->IsCityCameraInputBlocked());
	Wheel(EKeys::MouseScrollUp);
	TestTrue(TEXT("City zoom resumes"), Pawn->GetCurrentCityDxf() < Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityCameraZoomClampTest,
	"Rok.WorldMap.City.ZoomClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraZoomClampTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Dxf below the Unity min clamps to CameraParamLod min"),
		FMath::IsNearlyEqual(ARokStrategyCameraPawn::ClampCityDxf(1.0f), 82.4551f, 0.001f));
	TestTrue(TEXT("Dxf above the Unity max clamps to CameraParamLod city_bound"),
		FMath::IsNearlyEqual(ARokStrategyCameraPawn::ClampCityDxf(5000.0f), 250.0f, 0.001f));
	TestTrue(TEXT("In-range dxf is untouched"),
		FMath::IsNearlyEqual(ARokStrategyCameraPawn::ClampCityDxf(150.0f), 150.0f, 0.001f));
	const float NonFiniteDxf = FMath::Pow(10.0f, 100.0f);
	TestTrue(TEXT("Non-finite dxf falls back to the default city dxf"),
		FMath::IsNearlyEqual(
			ARokStrategyCameraPawn::ClampCityDxf(-NonFiniteDxf),
			ARokStrategyCameraPawn::GetDefaultCityDxf(),
			0.001f));

	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCityCameraZoomClampTest"));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Isolated test world for the clamp pawns"), World))
	{
		Instance->Shutdown();
		return false;
	}

	ARokStrategyCameraPawn* Pawn = World->SpawnActor<ARokStrategyCameraPawn>();
	if (TestNotNull(TEXT("Clamp pawn"), Pawn))
	{
		for (int32 Notch = 0; Notch < 20; ++Notch)
		{
			Pawn->ApplyZoomInput(-1.0f, 1.0f / 60.0f);
		}
		TestTrue(TEXT("Twenty zoom-in notches clamp at the Unity min dxf"),
			FMath::IsNearlyEqual(Pawn->GetCurrentCityDxf(), ARokStrategyCameraPawn::GetCityMinDxf(), 0.001f));
		Pawn->Destroy();
	}

	ARokStrategyCameraPawn* OutPawn = World->SpawnActor<ARokStrategyCameraPawn>();
	if (TestNotNull(TEXT("Zoom-out pawn"), OutPawn))
	{
		for (int32 Notch = 0; Notch < 3; ++Notch)
		{
			OutPawn->ApplyZoomInput(1.0f, 1.0f / 60.0f);
		}
		TestTrue(TEXT("Three zoom-out notches stay below the world-map threshold"),
			OutPawn->GetCurrentCityDxf() < 250.0f);
		OutPawn->Destroy();
	}

	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityCameraFocusCenterTest,
	"Rok.WorldMap.City.FocusCityCenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraFocusCenterTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCityCameraFocusCenterTest"));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Isolated test world for the focus pawn"), World))
	{
		Instance->Shutdown();
		return false;
	}

	ARokStrategyCameraPawn* Pawn = World->SpawnActor<ARokStrategyCameraPawn>();
	if (TestNotNull(TEXT("Focus camera pawn"), Pawn))
	{
		// The castle button only exists outside the city band, so push the pawn
		// there first, then assert the focus snaps back to city_default.
		for (int32 Notch = 0; Notch < 4; ++Notch)
		{
			Pawn->ApplyZoomInput(1.0f, 1.0f / 60.0f);
		}
		TestTrue(TEXT("Four zoom-out notches leave the city band"),
			Pawn->GetCurrentCityDxf() > 200.0f);
		Pawn->FrameCityCenter();
		TestTrue(TEXT("Focus restores the city_default dxf"),
			FMath::IsNearlyEqual(Pawn->GetCurrentCityDxf(), 150.0f, 0.01f));
		Pawn->Destroy();
	}

	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokCityCameraAspectFramingTest,
	"Rok.WorldMap.City.AspectFraming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityCameraAspectFramingTest::RunTest(const FString& Parameters)
{
	// Read the state the production code writes into the camera component.
	// (The first draft re-derived the maths from the constants instead, so it
	// could not have failed if ApplyCameraDefaults stopped configuring the camera.)
	UCameraComponent* Camera = NewObject<UCameraComponent>(GetTransientPackage());
	if (!TestNotNull(TEXT("Transient camera component"), Camera))
	{
		return false;
	}
	ARokStrategyCameraPawn::ApplyCityCameraProjection(Camera, 7.5f, 16.0f / 10.0f);

	TestFalse(TEXT("City camera does not constrain the aspect ratio"),
		Camera->bConstrainAspectRatio);
	TestTrue(TEXT("City camera overrides the aspect-ratio axis constraint"),
		Camera->bOverrideAspectRatioAxisConstraint);
	TestEqual(TEXT("City camera pins the projection to the vertical FOV axis"),
		Camera->AspectRatioAxisConstraint,
		EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
	TestTrue(TEXT("Camera aspect reference stays 16:10"),
		FMath::IsNearlyEqual(Camera->AspectRatio, 16.0f / 10.0f, 0.0001f));
	TestTrue(TEXT("Horizontal FOV at 16:10 is the vertical-to-horizontal conversion"),
		FMath::IsNearlyEqual(Camera->FieldOfView, 11.9731f, 0.001f));

	// The engine path in FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle
	// (CameraStackTypes.cpp:326-333) turns the stored horizontal FOV back into a
	// vertical FOV using the camera's own aspect ratio.
	auto VerticalFovFromCamera = [](const UCameraComponent& Component) {
		const float HalfXFov = FMath::DegreesToRadians(
			FMath::Max(0.001f, Component.FieldOfView) * 0.5f);
		const float HalfYFov = FMath::Atan(FMath::Tan(HalfXFov) / Component.AspectRatio);
		return FMath::RadiansToDegrees(2.0f * HalfYFov);
	};
	TestTrue(TEXT("The engine recovers the Unity 7.5 degree vertical FOV"),
		FMath::IsNearlyEqual(VerticalFovFromCamera(*Camera), 7.5f, 0.0001f));

	// With the vertical axis fixed, a wider window widens the horizontal FOV
	// instead of adding black bars (Unity fills the window at any aspect).
	const float WideWindowHorizontalFov = FMath::RadiansToDegrees(2.0f * FMath::Atan(
		FMath::Tan(FMath::DegreesToRadians(VerticalFovFromCamera(*Camera)) * 0.5f)
		* (21.0f / 9.0f)));
	TestTrue(TEXT("A 21:9 window widens the horizontal FOV instead of pillarboxing"),
		WideWindowHorizontalFov > Camera->FieldOfView);
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

namespace
{
class FWorldMapExitButtonReplay final : public IAutomationLatentCommand
{
public:
	explicit FWorldMapExitButtonReplay(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("City exit replay timed out waiting for the HUD or world population"));
			return true;
		}
		if (!bClicked)
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				if (auto* Button = Cast<UButton>(It->GetWidgetFromName(TEXT("WorldGlobeButton"))))
				{
					bClicked = true;
					Button->OnClicked.Broadcast();
					break;
				}
			}
			return false;
		}

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			const auto* Game = World ? World->GetAuthGameMode<ARokWorldMapGameMode>() : nullptr;
			if (!Game || Game->GetServerPopulation().EntityActors == 0) continue;
			const auto* Controller = World->GetFirstPlayerController();
			const auto* Camera = Controller ? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
			if (Test.TestNotNull(TEXT("World camera"), Camera))
			{
				Test.TestEqual(TEXT("Exit button opens the tactical camera"), Camera->GetCurrentDxf(), 1509.0f);
			}
			TActorIterator<ARokWorldMapCityActor> City(World);
			if (Test.TestTrue(TEXT("Home city is present"), bool(City)))
			{
				auto* Material = City->GetVisualMeshComponent()->GetMaterial(0);
				FLinearColor Light;
				Test.TestTrue(TEXT("Level five city uses the classical hall"),
					Material && GetNameSafe(Material->GetBaseMaterial()) == TEXT("M_WorldPlayerCity_build_1_101_2"));
				Test.TestTrue(TEXT("Classical hall has source-sized bounds even on first load"),
					City->GetVisualBoundsExtent().X > 1000.0);
				Test.TestTrue(TEXT("Server night clock lights city windows"),
					Material && Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("NightLight")), Light)
					&& Light.R > 0.0f);
			}
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 30.0;
	bool bClicked = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldMapExitButtonReplayTest, "Rok.WorldMap.City.ExitButtonReplay",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldMapExitButtonReplayTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FWorldMapExitButtonReplay(*this));
	return true;
}

#endif
