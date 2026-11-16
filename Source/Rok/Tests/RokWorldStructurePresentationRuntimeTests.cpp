#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "../RokWorldStructurePresentationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldStructureRuntimeTest,
    "Rok.WorldMap.StructurePresentation.RuntimeLayers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldStructureRuntimeTest::RunTest(const FString&)
{
    UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(TEXT("RokWorldStructureRuntimeTest"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    if (!TestNotNull(TEXT("Presentation world"), World)) return false;
    const TMap<FString, int32> Counts = {
        {TEXT("build_BarbarianCitadel_Big"), 1}, {TEXT("build_alliance_1003"), 1},
        {TEXT("build_alliance_1004"), 1}, {TEXT("build_alliance_1005"), 1},
        {TEXT("build_alliance_1006"), 1}, {TEXT("build_Checkpoint_Big"), 4},
        {TEXT("build_HolyLand1_Big"), 1}, {TEXT("build_HolyLand2_Big"), 1},
        {TEXT("build_Rssgold_Big"), 13}};
    for (const auto& Pair : Counts)
    {
        AActor* Owner = World->SpawnActor<AActor>();
        auto* Presentation = NewObject<URokWorldStructurePresentationComponent>(Owner);
        Owner->AddInstanceComponent(Presentation);
        Owner->SetRootComponent(Presentation);
        Presentation->RegisterComponent();
        if (!TestTrue(*FString::Printf(TEXT("%s complete source initializes"), *Pair.Key),
            Presentation->InitializeSource(Pair.Key))) { Owner->Destroy(); continue; }
        TestEqual(TEXT("Every source sprite has runtime geometry"), Presentation->GetCardCount(), Pair.Value);
        TestEqual(TEXT("Complete checkpoint has all four FBX meshes"), Presentation->GetMeshCount(),
            Pair.Key == TEXT("build_Checkpoint_Big") ? 4 : 0);
        Presentation->UpdatePresentation(480, true);
        TestEqual(TEXT("Initially active source GameObjects render independently of collision flags"),
            Presentation->GetActiveCardCount(), Pair.Value);
        TArray<UStaticMeshComponent*> Cards;
        Owner->GetComponents(Cards);
        for (const auto* Card : Cards)
        {
            const auto* Material = Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0));
            if (!TestNotNull(TEXT("Source layer has dynamic shader"), Material)) continue;
            UTexture* Texture = nullptr;
            TestTrue(TEXT("Source layer has actual texture bound"),
                Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Sprite")), Texture) && Texture != nullptr);
            TestTrue(TEXT("Source basis produces nonzero geometry"), !Card->GetRelativeScale3D().IsNearlyZero());
        }
        if (Pair.Key == TEXT("build_Checkpoint_Big"))
        {
            TArray<UProceduralMeshComponent*> Meshes;
            Owner->GetComponents(Meshes);
            int32 TriangleCount = 0, BackFacingCount = 0;
            for (auto* Mesh : Meshes)
            {
                const FProcMeshSection* Section = Mesh->GetProcMeshSection(0);
                if (!TestNotNull(TEXT("Source mesh render section"), Section)) continue;
                for (int32 Triangle = 0; Triangle < Section->ProcIndexBuffer.Num(); Triangle += 3)
                {
                    const FProcMeshVertex& A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Triangle]];
                    const FProcMeshVertex& B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Triangle + 1]];
                    const FProcMeshVertex& C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[Triangle + 2]];
                    const FVector Face = FVector::CrossProduct(C.Position - A.Position, B.Position - A.Position);
                    BackFacingCount += FVector::DotProduct(Face, A.Normal) <= 0.0 ? 1 : 0;
                    ++TriangleCount;
                }
            }
            TestEqual(TEXT("Source checkpoint topology retained"), TriangleCount, 5299);
            TestEqual(TEXT("UE triangle winding faces the source outward normals"), BackFacingCount, 0);
            TestEqual(TEXT("All opaque source mesh layers render near"), Presentation->GetActiveMeshCount(), 4);
        }
        Presentation->UpdatePresentation(900, true);
        if (Pair.Key == TEXT("build_Rssgold_Big"))
            TestEqual(TEXT("Native 900 gold survivors"), Presentation->GetActiveCardCount(), 12);
        Presentation->UpdatePresentation(2500, true);
        if (Pair.Key == TEXT("build_Rssgold_Big"))
            TestEqual(TEXT("Native 2500 gold survivor"), Presentation->GetActiveCardCount(), 1);
        else if (Pair.Key != TEXT("build_Checkpoint_Big"))
            TestEqual(TEXT("Initially false collision flags do not erase visible source objects"),
                Presentation->GetActiveCardCount(), Pair.Value);
        Presentation->UpdatePresentation(3500, false);
        TestEqual(TEXT("Far map band hides close sprites"), Presentation->GetActiveCardCount(), 0);
        TestEqual(TEXT("Far map band hides close meshes"), Presentation->GetActiveMeshCount(), 0);
        Presentation->UpdatePresentation(480, true);
        TestEqual(TEXT("Return to near band restores source pieces"), Presentation->GetActiveCardCount(), Pair.Value);
        Owner->Destroy();
    }
    return !HasAnyErrors();
}
#endif
