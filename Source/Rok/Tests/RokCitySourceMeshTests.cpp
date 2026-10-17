#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "../RokCitySpriteActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCitySourceMeshTransformTest,
	"Rok.City.Academy.SourceMeshTransformAndLegacySwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCitySourceMeshTransformTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCitySourceMeshTransformTest"));
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Isolated test world"), World))
	{
		Instance->Shutdown();
		return false;
	}

	ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>();
	UStaticMeshComponent* Component = Actor ? Actor->GetSpriteMeshComponent() : nullptr;
	UStaticMesh* SourceMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterial* Material = UMaterial::GetDefaultMaterial(MD_Surface);
	if (TestNotNull(TEXT("City sprite actor"), Actor)
		&& TestNotNull(TEXT("City sprite mesh component"), Component)
		&& TestNotNull(TEXT("Distinct source mesh fixture"), SourceMesh)
		&& TestNotNull(TEXT("Legacy plane fixture"), PlaneMesh)
		&& TestNotNull(TEXT("Default test material"), Material))
	{
		const FVector RootLocation(123.0, -456.0, 78.0);
		const FRotator RootRotation(0.0, -37.0, 0.0);
		const FVector SourcePivotOffset(-16.0, 11.0, 19.0);
		const FQuat SourceChildRotation = FRotator(-22.0, 15.0, 7.0).Quaternion();
		const FVector SourceChildScale(0.66308774);
		const FTransform SourceChildTransform(SourceChildRotation, SourcePivotOffset, SourceChildScale);
		const FVector VisualAnchorOffset(9.0, -3.0, 5.0);
		constexpr float LodAt300 = 1.28f; // 1 + (300 - 250) * .0056
		constexpr float SelectionScale = 1.035f;
		Actor->SetActorLocationAndRotation(RootLocation, RootRotation);
		Actor->ConfigurePlacement(FIntPoint(3, 4), FIntPoint(8, 12), VisualAnchorOffset);
		Actor->ConfigureSprite(Material, 120.0f, 80.0f, 3);
		TestEqual(TEXT("Initial legacy configuration keeps engine plane"), Component->GetStaticMesh().Get(), PlaneMesh);
		TestTrue(TEXT("Initial legacy configuration keeps original sizing"),
			Component->GetRelativeScale3D().Equals(FVector(1.2, 0.8, 1.0), 0.0001));
		TestTrue(TEXT("Source fixture is distinct from legacy plane"), SourceMesh != PlaneMesh);
		Actor->ConfigureSourceMesh(SourceMesh, Material, SourceChildTransform, 13);
		TestEqual(TEXT("Source mesh replaces legacy plane"), Component->GetStaticMesh().Get(), SourceMesh);
		TestTrue(TEXT("Source material is assigned to the mesh component"), Component->GetMaterial(0) == Material);
		TestTrue(TEXT("Unscaled source pivot offset remains relative to rotated root"),
			Component->GetRelativeLocation().Equals(SourcePivotOffset, 0.001));
		TestTrue(TEXT("Source child rotation preserved"),
			Component->GetRelativeTransform().GetRotation().Equals(SourceChildRotation, 0.0001));
		TestTrue(TEXT("Source child scale includes source PPU ratio"),
			Component->GetRelativeScale3D().Equals(SourceChildScale, 0.0001));
		TestEqual(TEXT("Source sort priority"), Component->TranslucencySortPriority, 13);

		Actor->ConfigureUnityCameraLod(FVector(31.0, -9.0, 8.0), 300.0f);
		const FVector ExpectedSourceWorld = Actor->GetActorTransform().TransformPosition(SourcePivotOffset * LodAt300);
		TestTrue(TEXT("LOD scales source child offset around root pivot"),
			Component->GetComponentLocation().Equals(ExpectedSourceWorld, 0.01));
		TestTrue(TEXT("LOD scales source tight mesh without changing its pivot"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * LodAt300, 0.0001));
		TestTrue(TEXT("LOD retains source child rotation"),
			Component->GetRelativeTransform().GetRotation().Equals(SourceChildRotation, 0.0001));
		TestTrue(TEXT("Actor world root is not moved by source LOD"),
			Actor->GetActorLocation().Equals(RootLocation, 0.01));

		Actor->SetSelected(true);
		TestTrue(TEXT("Selection enlarges source mesh around its pivot"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * LodAt300 * SelectionScale, 0.0001));
		TestTrue(TEXT("Selection does not drift source pivot offset"),
			Component->GetComponentLocation().Equals(ExpectedSourceWorld, 0.01));
		Actor->ConfigureUnityCameraLod(FVector(31.0, -9.0, 8.0), 300.0f);
		TestTrue(TEXT("Repeated source LOD setup does not recapture enlarged component scale"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * LodAt300 * SelectionScale, 0.0001));
		TestTrue(TEXT("Repeated source LOD setup does not recapture moved component origin"),
			Component->GetComponentLocation().Equals(ExpectedSourceWorld, 0.01));
		Actor->ConfigureUnityCameraLod(FVector(31.0, -9.0, 8.0), 500.0f);
		const FVector ExpectedAt500 = Actor->GetActorTransform().TransformPosition(SourcePivotOffset * 2.4);
		TestTrue(TEXT("Changing source LOD reads original child scale"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * 2.4 * SelectionScale, 0.0001));
		TestTrue(TEXT("Changing source LOD reads original child pivot offset"),
			Component->GetComponentLocation().Equals(ExpectedAt500, 0.01));
		Actor->ConfigureUnityCameraLod(FVector(31.0, -9.0, 8.0), 300.0f);
		TestTrue(TEXT("Returning source LOD to 300 has no accumulated drift"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * LodAt300 * SelectionScale, 0.0001)
			&& Component->GetComponentLocation().Equals(ExpectedSourceWorld, 0.01));
		Actor->SetSelected(false);
		TestTrue(TEXT("Deselection returns to source LOD scale"),
			Component->GetRelativeScale3D().Equals(SourceChildScale * LodAt300, 0.0001));

		Actor->SetSelected(true);
		Actor->ConfigureSprite(Material, 250.0f, 170.0f, 7);
		const FVector LegacyScale(2.5, 1.7, 1.0);
		TestEqual(TEXT("Switch back restores engine plane"), Component->GetStaticMesh().Get(), PlaneMesh);
		TestTrue(TEXT("Switch back clears source child pivot offset"),
			Component->GetRelativeLocation().Equals(FVector::ZeroVector, 0.001));
		TestTrue(TEXT("Switch back clears source child rotation"),
			Component->GetRelativeTransform().GetRotation().Equals(FQuat::Identity, 0.0001));
		TestTrue(TEXT("Switch back preserves selected legacy enlargement once"),
			Component->GetRelativeScale3D().Equals(LegacyScale * SelectionScale, 0.0001));
		TestTrue(TEXT("Switch back clears stale source LOD scale"),
			FMath::IsNearlyEqual(Actor->GetUnityLodScale(), 1.0f, 0.0001f));
		TestEqual(TEXT("Switch back assigns legacy sort priority"), Component->TranslucencySortPriority, 7);
		Actor->SetSelected(false);
		TestTrue(TEXT("Legacy deselection restores new plane baseline"),
			Component->GetRelativeScale3D().Equals(LegacyScale, 0.0001));

		Actor->ConfigureUnityCameraLod(FVector(31.0, -9.0, 8.0), 300.0f);
		TestTrue(TEXT("Legacy LOD now scales its own plane baseline"),
			Component->GetRelativeScale3D().Equals(LegacyScale * LodAt300, 0.0001));
		TestTrue(TEXT("Legacy LOD uses legacy anchor/center path"),
			Component->GetComponentLocation().Equals(
				RootLocation + VisualAnchorOffset + FVector(31.0, -9.0, 8.0) * LodAt300, 0.01));

		Actor->Tags.AddUnique(TEXT("UnityBuildingType_2"));
		Actor->SetActorScale3D(FVector(500.0, 500.0, -500.0));
		TestFalse(TEXT("Legacy move arrows still reject enlarged root scale"), Actor->CanShowMoveArrows());
		Actor->ConfigureSourceMesh(SourceMesh, Material, SourceChildTransform, 13);
		Actor->ConfigureUnityCameraLod(FVector::ZeroVector, 150.0f);
		TestTrue(TEXT("Source mesh at Dxf 150 permits arrows despite 500 UU root scale"),
			Actor->CanShowMoveArrows());
		Actor->ConfigureUnityCameraLod(FVector::ZeroVector, 300.0f);
		TestFalse(TEXT("Source mesh enlarged by Unity camera LOD rejects arrows"),
			Actor->CanShowMoveArrows());
		Actor->ConfigureSprite(Material, 120.0f, 80.0f, 3);
		TestFalse(TEXT("Switching back to legacy restores root-scale arrow rule"),
			Actor->CanShowMoveArrows());
		Actor->SetActorScale3D(FVector::OneVector);
		TestTrue(TEXT("Legacy normal root scale permits Farm arrows"), Actor->CanShowMoveArrows());
		Actor->Tags.Remove(TEXT("UnityBuildingType_2"));
		Actor->Tags.AddUnique(TEXT("UnityBuildingType_6"));
		TestFalse(TEXT("CityWall remains excluded from move arrows"), Actor->CanShowMoveArrows());
		Actor->Tags.Remove(TEXT("UnityBuildingType_6"));
		Actor->Tags.AddUnique(TEXT("UnityBuildingType_8"));
		TestFalse(TEXT("GuardTower remains excluded from move arrows"), Actor->CanShowMoveArrows());
		Actor->Destroy();
	}
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
