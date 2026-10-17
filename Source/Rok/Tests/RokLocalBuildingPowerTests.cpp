#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalBuildingPowerTest,
    "Rok.Runtime.LocalBuildingPowerProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalBuildingPowerTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(TEXT("RokLocalBuildingPowerFixture"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    if (!TestNotNull(TEXT("Isolated fixture world"), World)) return false;
    auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!TestNotNull(TEXT("Real building subsystem"), Buildings)
        || !TestNotNull(TEXT("Real runtime projection"), Runtime)) return false;
    const auto Spawn = [World](FName Name, int32 InitId, int32 TypeId)
    {
        FActorSpawnParameters Parameters;
        Parameters.Name = Name; Parameters.ObjectFlags |= RF_Transient;
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
        if (Actor)
        {
            Actor->Tags.Add(FName(*FString::Printf(TEXT("UnityInitId_%d"), InitId)));
            Actor->Tags.Add(FName(*FString::Printf(TEXT("UnityBuildingType_%d"), TypeId)));
            Actor->Tags.Add(TEXT("UnityBuildingLevel_1"));
            Actor->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));
        }
        return Actor;
    };
    auto* Hall = Spawn(TEXT("PowerFixture_TownHall"), 5, 1);
    auto* First = Spawn(TEXT("PowerFixture_BarracksPartA"), 6, 9);
    auto* Second = Spawn(TEXT("PowerFixture_BarracksPartB"), 6, 9);
    auto* Storehouse = Spawn(TEXT("PowerFixture_Storehouse"), 3, 15);
    if (!TestNotNull(TEXT("Source prerequisite fixture"), Hall) || !TestNotNull(TEXT("First visual fixture"), First) || !TestNotNull(TEXT("Second visual fixture"), Second)
        || !TestNotNull(TEXT("Reentrant registration fixture"), Storehouse)) return false;
    const int32 OriginalBaseline = Runtime->GetPlayerProfileState().LocalPreviewScore;
    FString Failure;
    if (!TestTrue(TEXT("Start legal CityHall prerequisite queue"), Buildings->StartUpgrade(Hall, Failure))) { AddError(Failure); return false; }
    // Synthetic clock only in this isolated unit test; no direct level/power/resource mutation.
    World->TimeSeconds = Buildings->GetBuildingStateForActor(Hall).UpgradeFinishesAt;
    Buildings->Tick(0);
    const int32 Baseline = Runtime->GetPlayerProfileState().LocalPreviewScore;
    TestEqual(TEXT("Prerequisite contributes actual source delta fourteen"), Baseline, OriginalBaseline + 14);
    TestEqual(TEXT("Unvisited source actors add no score beyond completed prerequisite"), Buildings->GetLocalCompletedBuildingPowerDelta(), 14);
    const auto Initial = Buildings->GetBuildingStateForActor(First);
    const auto Alias = Buildings->GetBuildingStateForActor(Second);
    TestEqual(TEXT("Known source identity replaces visual actor name"), Initial.BuildingKey, FName(TEXT("UnityInitId_6")));
    TestEqual(TEXT("Visual parts share one logical state"), Alias.BuildingKey, Initial.BuildingKey);
    TestEqual(TEXT("Source barracks starts at level one"), Initial.Level, 1);
    TestEqual(TEXT("Source barracks baseline power"), Initial.Power, 5);
    FRokBuildingUpgradePreview Preview;
    if (!TestTrue(TEXT("Read actual next-level configuration"), Buildings->GetUpgradePreview(First, Preview))) return false;
    TestEqual(TEXT("Actual source level-two power"), Preview.NextPower, 10);
    int32 Events = 0;
    FRokLocalBuildingUpgradeCompleted Last;
    const FDelegateHandle Handle = Buildings->OnLocalUpgradeCompleted.AddLambda(
        [&](const FRokLocalBuildingUpgradeCompleted& Event)
        {
            ++Events; Last = Event;
            // A new state inserted by an observer must not invalidate Tick's map iteration.
            Buildings->GetBuildingStateForActor(Storehouse);
        });
    ON_SCOPE_EXIT { Buildings->OnLocalUpgradeCompleted.Remove(Handle); };
    if (!TestTrue(TEXT("Start actual local queue"), Buildings->StartUpgrade(First, Failure)))
    { AddError(Failure); return false; }
    TestTrue(TEXT("Other visual sees active queue"), Buildings->GetBuildingStateForActor(Second).bUpgradeInProgress);
    TestFalse(TEXT("Alias cannot start a second queue"), Buildings->StartUpgrade(Second, Failure));
    const double Deadline = Buildings->GetBuildingStateForActor(First).UpgradeFinishesAt;
    TestTrue(TEXT("Queue retains actual source duration"), FMath::IsNearlyEqual(Deadline - World->GetTimeSeconds(), 20.0, .001));
    // Explicit synthetic unit-test clock. Real-time HUD/effect acceptance is a separate runtime audit.
    World->TimeSeconds = Deadline - .01;
    Buildings->Tick(0);
    TestEqual(TEXT("No early completion"), Events, 0);
    TestEqual(TEXT("Pending queue adds nothing beyond prerequisite"), Buildings->GetLocalCompletedBuildingPowerDelta(), 14);
    World->TimeSeconds = Deadline;
    TestEqual(TEXT("Projection never completes timers itself"), Buildings->GetLocalCompletedBuildingPowerDelta(), 14);
    TestEqual(TEXT("GI projection remains read-only at deadline"), Runtime->GetPlayerProfileState().LocalPreviewScore, Baseline);
    Buildings->Tick(0);
    TestEqual(TEXT("One committed completion event"), Events, 1);
    TestEqual(TEXT("Event retains source identity"), Last.UnityInitId, 6);
    TestEqual(TEXT("Event old level"), Last.PreviousLevel, 1);
    TestEqual(TEXT("Event new level"), Last.CurrentLevel, 2);
    TestEqual(TEXT("Event old config power"), Last.PreviousPower, 5);
    TestEqual(TEXT("Event new config power"), Last.CurrentPower, 10);
    for (int32 Repeat = 0; Repeat < 3; ++Repeat)
    {
        Buildings->GetBuildingStateForActor(Second);
        Buildings->GetUpgradePreview(First, Preview);
        Buildings->Tick(0);
        TestEqual(TEXT("Repeated observation cannot accumulate delta"), Buildings->GetLocalCompletedBuildingPowerDelta(), 19);
        const auto Profile = Runtime->GetPlayerProfileState();
        TestEqual(TEXT("Building contribution stays explicitly local"), Profile.LocalBuildingUpgradeScore, 19);
        TestEqual(TEXT("Projection leaves bootstrap unmodified"), Profile.LocalPreviewScore, Baseline + 5);
    }
    TestEqual(TEXT("Repeated reads/ticks cannot rebroadcast completion"), Events, 1);
    TestEqual(TEXT("Reentrant new source state was safely registered"), Buildings->GetBuildingStateForActor(Storehouse).Level, 1);
    if (!TestTrue(TEXT("Start second independent source queue"), Buildings->StartUpgrade(Storehouse, Failure))) return false;
    World->TimeSeconds = Buildings->GetBuildingStateForActor(Storehouse).UpgradeFinishesAt;
    TestEqual(TEXT("Existing state getter can commit a due queue"), Buildings->GetBuildingStateForActor(Storehouse).Level, 2);
    TestEqual(TEXT("Getter completion defers observer delivery"), Events, 1);
    Buildings->Tick(0);
    TestEqual(TEXT("Deferred completion delivered once on next tick"), Events, 2);
    Buildings->Tick(0);
    TestEqual(TEXT("Deferred queue is drained"), Events, 2);
    TestEqual(TEXT("Independent completed source deltas are summed"), Buildings->GetLocalCompletedBuildingPowerDelta(), 24);

    auto* FreshInstance = NewObject<UGameInstance>(GEngine);
    FreshInstance->InitializeStandalone(TEXT("RokLocalBuildingPowerFreshWorld"));
    UWorld* FreshWorld = FreshInstance->GetWorld();
    ON_SCOPE_EXIT { FreshInstance->Shutdown(); GEngine->DestroyWorldContext(FreshWorld); };
    if (!TestNotNull(TEXT("Independent fresh-world fixture"), FreshWorld)) return false;
    auto* FreshBuildings = FreshWorld->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* FreshRuntime = FreshInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!TestNotNull(TEXT("Fresh building state"), FreshBuildings) || !TestNotNull(TEXT("Fresh runtime state"), FreshRuntime)) return false;
    TestEqual(TEXT("World progress does not leak into new world"), FreshBuildings->GetLocalCompletedBuildingPowerDelta(), 0);
    TestEqual(TEXT("Fresh projection starts at immutable source bootstrap"), FreshRuntime->GetPlayerProfileState().LocalPreviewScore, OriginalBaseline);
    return true;
}
#endif
