#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingPrerequisiteParsingTest,
    "Rok.Runtime.BuildingPrerequisites.StrictThreeSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokBuildingPrerequisiteParsingTest::RunTest(const FString&)
{
    auto Row = MakeShared<FJsonObject>();
    for (int32 Slot = 1; Slot <= 3; ++Slot)
    {
        Row->SetNumberField(FString::Printf(TEXT("reqType%d"), Slot), Slot == 1 ? 1 : 0);
        Row->SetNumberField(FString::Printf(TEXT("reqLevel%d"), Slot), Slot == 1 ? 2 : 0);
    }
    FRokUnityBuildingLevelConfig Config;
    FRokUnityBuildingPrerequisite Missing; int32 Current = -1;
    TMap<int32, int32> Levels;
    TestFalse(TEXT("Unparsed config cannot permit an upgrade"), Config.CheckPrerequisites(Levels, Missing, Current));
    TestTrue(TEXT("All source slots parse"), Config.ParsePrerequisites(Row));
    TestFalse(TEXT("Absent required type is level zero"), Config.CheckPrerequisites(Levels, Missing, Current));
    TestEqual(TEXT("Missing requirement identity"), Missing.TypeId, 1);
    TestEqual(TEXT("Absent current level"), Current, 0);
    Levels.Add(1, 1);
    TestFalse(TEXT("Below threshold is blocked"), Config.CheckPrerequisites(Levels, Missing, Current));
    Levels.Add(1, 2);
    TestTrue(TEXT("Exact completed threshold passes"), Config.CheckPrerequisites(Levels, Missing, Current));
    Row->SetNumberField(TEXT("reqType2"), 9); Row->SetNumberField(TEXT("reqLevel2"), 3);
    Row->SetNumberField(TEXT("reqType3"), 15); Row->SetNumberField(TEXT("reqLevel3"), 4);
    TestTrue(TEXT("Three nonzero slots parse"), Config.ParsePrerequisites(Row));
    Levels.Add(9, 3);
    TestFalse(TEXT("First two passing cannot bypass third"), Config.CheckPrerequisites(Levels, Missing, Current));
    TestEqual(TEXT("Third failing identity is preserved"), Missing.TypeId, 15);
    Levels.Add(15, 4);
    TestTrue(TEXT("All three use AND"), Config.CheckPrerequisites(Levels, Missing, Current));
    Row->SetNumberField(TEXT("reqType3"), 0); Row->SetNumberField(TEXT("reqLevel3"), 99);
    Levels.Remove(15);
    TestTrue(TEXT("Source zero type ignores its level"), Config.ParsePrerequisites(Row) && Config.CheckPrerequisites(Levels, Missing, Current));
    Row->RemoveField(TEXT("reqLevel3"));
    TestFalse(TEXT("Missing ignored slot is still incomplete data"), Config.ParsePrerequisites(Row));
    TestFalse(TEXT("Failed reparse removes stale unlocked requirements"), Config.CheckPrerequisites(Levels, Missing, Current));
    Row->SetNumberField(TEXT("reqLevel3"), 0);
    const TArray<TSharedPtr<FJsonValue>> Invalid = {
        MakeShared<FJsonValueNumber>(-1), MakeShared<FJsonValueNumber>(1.25), MakeShared<FJsonValueNumber>(double(MAX_int32) + 1),
        MakeShared<FJsonValueString>(TEXT("1garbage")), MakeShared<FJsonValueString>(TEXT("2147483648")),
        MakeShared<FJsonValueString>(TEXT("")), MakeShared<FJsonValueBoolean>(true), MakeShared<FJsonValueNull>()};
    for (const auto& Value : Invalid)
    {
        Row->SetField(TEXT("reqType1"), Value);
        TestFalse(TEXT("Invalid number/text/type cannot silently become zero"), Config.ParsePrerequisites(Row));
    }
    Row->SetStringField(TEXT("reqType1"), TEXT("1")); Row->SetStringField(TEXT("reqLevel1"), TEXT("2"));
    TestTrue(TEXT("Exact decimal integer source strings are supported"), Config.ParsePrerequisites(Row));
    Row->SetNumberField(TEXT("reqLevel1"), 0);
    TestFalse(TEXT("Nonzero type requires a positive level"), Config.ParsePrerequisites(Row));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingPrerequisiteRuntimeTest,
    "Rok.Runtime.BuildingPrerequisites.CanonicalCompletedLevels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokBuildingPrerequisiteRuntimeTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(TEXT("RokBuildingPrerequisitesFixture"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    if (!TestNotNull(TEXT("Isolated fixture world"), World)) return false;
    auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
    if (!TestNotNull(TEXT("Actual local subsystem"), Buildings)) return false;
    const auto Spawn = [World](const TCHAR* Name, int32 InitId, int32 TypeId, int32 TaggedLevel = 1)
    {
        FActorSpawnParameters Parameters; Parameters.Name = FName(Name); Parameters.ObjectFlags |= RF_Transient;
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
        if (Actor)
        {
            Actor->Tags.Add(FName(*FString::Printf(TEXT("UnityInitId_%d"), InitId)));
            Actor->Tags.Add(FName(*FString::Printf(TEXT("UnityBuildingType_%d"), TypeId)));
            Actor->Tags.Add(FName(*FString::Printf(TEXT("UnityBuildingLevel_%d"), TaggedLevel)));
            Actor->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));
        }
        return Actor;
    };
    AActor* Barracks = Spawn(TEXT("PrereqBarracks"), 6, 9);
    AActor* Hospital = Spawn(TEXT("PrereqHospital"), 4, 14);
    AActor* Unknown = Spawn(TEXT("PrereqUnownedTownHall"), 99999, 1, 99);
    if (!TestNotNull(TEXT("Barracks fixture"), Barracks) || !TestNotNull(TEXT("Hospital fixture"), Hospital)
        || !TestNotNull(TEXT("Unowned fixture"), Unknown)) return false;
    int32 Events = 0;
    auto Handle = Buildings->OnLocalUpgradeCompleted.AddLambda([&](const FRokLocalBuildingUpgradeCompleted&) { ++Events; });
    ON_SCOPE_EXIT { Buildings->OnLocalUpgradeCompleted.Remove(Handle); };
    FString Failure;
    TestTrue(TEXT("Unselected initial CityHall1 satisfies hospital level-one prerequisite"), Buildings->CanStartUpgrade(Hospital, Failure));
    Buildings->GetBuildingStateForActor(Unknown);
    TestFalse(TEXT("Unknown high-level actor cannot supply CityHall2"), Buildings->CanStartUpgrade(Barracks, Failure));
    TestEqual(TEXT("Exact localized requirement feedback"), Failure, FString(TEXT("需要 2 级市政厅（当前 1 级）")));
    const auto Before = Buildings->GetCurrentResources();
    FRokBuildingUpgradePreview Preview;
    TestTrue(TEXT("Blocked preview remains available"), Buildings->GetUpgradePreview(Barracks, Preview));
    TestFalse(TEXT("Preview shares subsystem prerequisite gate"), Preview.bCanStartTimedUpgrade);
    TestEqual(TEXT("Preview preserves the same reason"), Preview.FailureReason, Failure);
    TestFalse(TEXT("Direct command cannot bypass disabled UI"), Buildings->StartUpgrade(Barracks, Failure));
    const auto After = Buildings->GetCurrentResources();
    TestTrue(TEXT("Denied command does not spend any resource"), Before.Food == After.Food && Before.Wood == After.Wood && Before.Stone == After.Stone && Before.Gold == After.Gold);
    TestEqual(TEXT("Denied command creates no queue"), Buildings->GetActiveUpgradeCount(), 0);
    Buildings->Tick(0);
    TestEqual(TEXT("Denied command emits no completion"), Events, 0);
    TestEqual(TEXT("Denied command gains no power"), Buildings->GetLocalCompletedBuildingPowerDelta(), 0);
    TestFalse(TEXT("Unknown actor cannot start an owned upgrade"), Buildings->StartUpgrade(Unknown, Failure));
    AActor* Hall = Spawn(TEXT("PrereqTownHall"), 5, 1);
    AActor* Alias = Spawn(TEXT("PrereqTownHallSecondVisual"), 5, 1, 99);
    if (!TestNotNull(TEXT("Town hall fixture"), Hall) || !TestNotNull(TEXT("Alias fixture"), Alias)) return false;
    TestEqual(TEXT("Known source state overrides visual tagged target level"), Buildings->GetBuildingStateForActor(Alias).Level, 1);
    TestFalse(TEXT("Multiple level-one visuals do not add up to level two"), Buildings->CanStartUpgrade(Barracks, Failure));
    if (!TestTrue(TEXT("CityHall2 has no prerequisite chain"), Buildings->StartUpgrade(Hall, Failure))) { AddError(Failure); return false; }
    const double Deadline = Buildings->GetBuildingStateForActor(Hall).UpgradeFinishesAt;
    TestTrue(TEXT("Source CityHall duration is two seconds"), FMath::IsNearlyEqual(Deadline - World->GetTimeSeconds(), 2.0, .001));
    // Synthetic time is confined to this isolated unit test; the 17-PNG audit waits real time.
    World->TimeSeconds = Deadline - .01;
    Buildings->Tick(0);
    TestFalse(TEXT("In-progress target level does not satisfy prerequisite"), Buildings->CanStartUpgrade(Barracks, Failure));
    TestEqual(TEXT("Canonical alias retains old completed level"), Buildings->GetBuildingStateForActor(Alias).Level, 1);
    World->TimeSeconds = Deadline; Buildings->Tick(0);
    TestEqual(TEXT("One hall completion"), Events, 1);
    TestEqual(TEXT("Hall source power increment"), Buildings->GetLocalCompletedBuildingPowerDelta(), 14);
    TestTrue(TEXT("Actual completed hall unlocks barracks"), Buildings->CanStartUpgrade(Barracks, Failure));
    if (!TestTrue(TEXT("Legal barracks command now starts"), Buildings->StartUpgrade(Barracks, Failure))) return false;
    World->TimeSeconds = Buildings->GetBuildingStateForActor(Barracks).UpgradeFinishesAt; Buildings->Tick(0);
    TestEqual(TEXT("Two independent completion events"), Events, 2);
    TestEqual(TEXT("Full source chain power delta"), Buildings->GetLocalCompletedBuildingPowerDelta(), 19);
    TestEqual(TEXT("Exact final food after both configured debits"), Buildings->GetCurrentResources().Food, Before.Food - 4000);
    TestEqual(TEXT("Exact final wood after both configured debits"), Buildings->GetCurrentResources().Wood, Before.Wood - 4300);
    Buildings->Tick(0); Buildings->CanStartUpgrade(Barracks, Failure);
    TestEqual(TEXT("Read/recheck never repeats completed events"), Events, 2);
    return true;
}
#endif
