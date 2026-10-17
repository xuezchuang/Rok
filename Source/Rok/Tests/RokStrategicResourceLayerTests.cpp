#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "../RokStrategicResourceLayer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokStrategicResourceLifecycleTest,
	"Rok.WorldMap.StrategicResources.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokStrategicResourceLifecycleTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Grid origin"), ARokStrategicResourceLayer::SourceCell(FVector2D::ZeroVector), FIntPoint(0, 0));
	TestEqual(TEXT("Cell boundary"), ARokStrategicResourceLayer::SourceCell(FVector2D(300, 600)), FIntPoint(1, 2));
	TestEqual(TEXT("Bounded edge"), ARokStrategicResourceLayer::SourceCell(FVector2D(7200, -1)), FIntPoint(23, 0));
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokStrategicResourcesTest"));
	UWorld* World = Instance->GetWorld();
	auto* Layer = World->SpawnActor<ARokStrategicResourceLayer>();
	if (TestNotNull(TEXT("Presentation layer spawned"), Layer))
	{
		const FRokWorldMapProjection Projection;
		Layer->UpdateView(true, Projection, FVector2D(6876, 942), 6000);
		const int32 InitialCount = Layer->GetVisiblePointCount();
		TestEqual(TEXT("Source nine-cell population at initial city"), InitialCount, 52);
		Layer->UpdateView(true, Projection, FVector2D(6876, 942), 10000);
		TestEqual(TEXT("Zoom does not duplicate points"), Layer->GetVisiblePointCount(), InitialCount);
		Layer->UpdateView(false, Projection, FVector2D(6876, 942), 6000);
		TestEqual(TEXT("Toggle off releases fixed points"), Layer->GetVisiblePointCount(), 0);
		TArray<UStaticMeshComponent*> Meshes;
		Layer->GetComponents(Meshes);
		TestEqual(TEXT("Only cached overlay remains"), Meshes.Num(), 1);
		if (Meshes.Num() == 1) TestFalse(TEXT("Cached overlay is hidden"), Meshes[0]->IsVisible());
		Layer->UpdateView(true, Projection, FVector2D(6876, 942), 6000);
		TestEqual(TEXT("Reopen restores same source population"), Layer->GetVisiblePointCount(), InitialCount);
		Layer->UpdateView(true, Projection, FVector2D(3600, 3600), 6000);
		Layer->UpdateView(true, Projection, FVector2D(6876, 942), 6000);
		TestEqual(TEXT("Pan away and return does not retain other cells"), Layer->GetVisiblePointCount(), InitialCount);
		Layer->Destroy();
	}
	Instance->Shutdown();
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokStrategicExplorationLifecycleTest,
	"Rok.WorldMap.StrategicExploration.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokStrategicExplorationLifecycleTest::RunTest(const FString& Parameters)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokStrategicExplorationTest"));
	UWorld* World = Instance->GetWorld();
	auto* Layer = World->SpawnActor<ARokStrategicResourceLayer>();
	if (TestNotNull(TEXT("Exploration layer spawned"), Layer))
	{
		Layer->ConfigureExploration();
		const FRokWorldMapProjection Projection;
		const FVector2D Center(6876,942);
		Layer->UpdateView(true, Projection, Center, 6000);
		TestEqual(TEXT("Source nine-cell exploration population"), Layer->GetVisiblePointCount(), 34);
		TArray<UStaticMeshComponent*> Meshes;
		Layer->GetComponents(Meshes);
		TestEqual(TEXT("Exploration has no resource heatmap"), Meshes.Num(), 34);
		TArray<UMaterialInterface*> Before;
		for (auto* Mesh : Meshes) Before.Add(Mesh->GetMaterial(0));
		TMap<int32,uint64> Words;
		Words.Add(1, uint64(1)<<30); // Source cave ID30 is in the initial neighborhood.
		Layer->SetExplorationProgress(Words);
		Layer->UpdateView(true, Projection, Center, 6000);
		int32 Changed = 0;
		for (int32 Index = 0; Index < Meshes.Num(); ++Index)
			Changed += Meshes[Index]->GetMaterial(0) != Before[Index];
		TestEqual(TEXT("Progress replaces only the affected point's sprite without a camera move"), Changed, 1);
		Layer->UpdateView(false, Projection, Center, 6000);
		Meshes.Reset(); Layer->GetComponents(Meshes);
		TestEqual(TEXT("Close removes all exploration meshes"), Meshes.Num(), 0);
		Layer->UpdateView(true, Projection, Center, 6000);
		TestEqual(TEXT("Reopen restores population"), Layer->GetVisiblePointCount(), 34);
		Layer->UpdateView(true, Projection, FVector2D(3600,3600), 9000);
		Layer->UpdateView(true, Projection, Center, 6000);
		TestEqual(TEXT("Pan return does not duplicate points"), Layer->GetVisiblePointCount(), 34);
		Layer->Destroy();
	}
	Instance->Shutdown(); GEngine->DestroyWorldContext(World);
	return true;
}

#endif
