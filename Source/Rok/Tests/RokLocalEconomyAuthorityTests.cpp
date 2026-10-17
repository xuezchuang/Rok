#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalEconomyAuthorityTest,
	"Rok.Runtime.LocalEconomy.UpgradeAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokLocalEconomyAuthorityTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalEconomyAuthorityFixture"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated fixture world"), World)) return false;

	URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	URokUnityRuntimeStateSubsystem* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!TestNotNull(TEXT("World building ledger"), Buildings)
		|| !TestNotNull(TEXT("Game-instance runtime profile"), Runtime)) return false;
	ON_SCOPE_EXIT { Buildings->ClearLocalEconomyAuthorityForTesting(); };
	if (!TestTrue(TEXT("Current local-preview runtime loaded"), Runtime->IsRuntimeStateLoaded())) return false;
	const FRokUnityRuntimePlayerProfileState ActualProfile = Runtime->GetPlayerProfileState();
	if (!TestEqual(TEXT("Current fixture remains local_preview"), ActualProfile.StateKind, FString(TEXT("local_preview")))) return false;
	TestFalse(TEXT("Current fixture has no server role identity"), ActualProfile.bHasRoleId);
	TestFalse(TEXT("Current fixture has no account civilization"), ActualProfile.bHasCivilization);

	FActorSpawnParameters Parameters;
	Parameters.Name = TEXT("LocalEconomyTownHall");
	Parameters.ObjectFlags |= RF_Transient;
	AActor* Hall = World->SpawnActor<AActor>(AActor::StaticClass(), Parameters);
	if (!TestNotNull(TEXT("Source-owned Hall fixture"), Hall)) return false;
	Hall->Tags.Add(TEXT("UnityInitId_5"));
	Hall->Tags.Add(TEXT("UnityBuildingType_1"));
	Hall->Tags.Add(TEXT("UnityBuildingLevel_1"));
	Hall->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));

	const FRokResourceStock Before = Buildings->GetCurrentResources();
	const int32 QueuesBefore = Buildings->GetActiveUpgradeCount();
	FRokLocalEconomyAuthoritySnapshot Snapshot;
	Snapshot.bRuntimeLoaded = true;
	Snapshot.Profile = ActualProfile;
	const auto CheckDenied = [this, Buildings, Hall, &Before, QueuesBefore](
		const TCHAR* CaseName, const FRokLocalEconomyAuthoritySnapshot& Denied)
	{
		Buildings->SetLocalEconomyAuthorityForTesting(Denied);
		FString Reason;
		TestFalse(*FString::Printf(TEXT("%s cannot preview a local debit"), CaseName),
			Buildings->CanStartUpgrade(Hall, Reason));
		TestFalse(*FString::Printf(TEXT("%s cannot submit a local debit"), CaseName),
			Buildings->StartUpgrade(Hall, Reason));
		TestFalse(*FString::Printf(TEXT("%s has an explicit denial"), CaseName), Reason.IsEmpty());
		const FRokResourceStock After = Buildings->GetCurrentResources();
		TestTrue(*FString::Printf(TEXT("%s preserves every wallet balance"), CaseName),
			Before.Food == After.Food && Before.Wood == After.Wood
			&& Before.Stone == After.Stone && Before.Gold == After.Gold);
		TestEqual(*FString::Printf(TEXT("%s creates no upgrade queue"), CaseName),
			Buildings->GetActiveUpgradeCount(), QueuesBefore);
	};

	FRokLocalEconomyAuthoritySnapshot Denied = Snapshot;
	Denied.bRuntimeLoaded = false;
	CheckDenied(TEXT("Runtime not ready"), Denied);
	Denied = Snapshot;
	Denied.Profile.StateKind = TEXT("server_account");
	CheckDenied(TEXT("Non-local profile"), Denied);
	Denied = Snapshot;
	Denied.Profile.bHasRoleId = true;
	Denied.Profile.RoleId = TEXT("synthetic-account-id");
	CheckDenied(TEXT("Recognized role account"), Denied);
	Denied = Snapshot;
	Denied.Profile.RoleId = TEXT("synthetic-account-id");
	CheckDenied(TEXT("Role identity despite false flag"), Denied);
	Denied = Snapshot;
	Denied.Profile.CivilizationId = 101;
	CheckDenied(TEXT("Account civilization despite false flag"), Denied);
	Denied = Snapshot;
	Denied.Profile.bHasCivilization = true;
	CheckDenied(TEXT("Account civilization flag without ID"), Denied);
	Denied = Snapshot;
	Denied.Profile.CivilizationName = TEXT("synthetic civilization");
	CheckDenied(TEXT("Account civilization name despite missing ID and flag"), Denied);

	FRokLocalEconomyAuthoritySnapshot NamedPreview = Snapshot;
	NamedPreview.Profile.bHasDisplayName = true;
	NamedPreview.Profile.DisplayName = TEXT("local preview name");
	NamedPreview.Profile.bHasAlliance = true;
	NamedPreview.Profile.AllianceName = TEXT("local preview alliance");
	Buildings->SetLocalEconomyAuthorityForTesting(NamedPreview);
	FString NameOnlyReason;
	TestTrue(TEXT("Local preview name and alliance do not imply account ownership"),
		Buildings->CanStartUpgrade(Hall, NameOnlyReason));
	const FRokResourceStock AfterNameOnly = Buildings->GetCurrentResources();
	TestTrue(TEXT("Name-only preview does not spend resources"),
		Before.Food == AfterNameOnly.Food && Before.Wood == AfterNameOnly.Wood
		&& Before.Stone == AfterNameOnly.Stone && Before.Gold == AfterNameOnly.Gold);
	TestEqual(TEXT("Name-only preview creates no queue"), Buildings->GetActiveUpgradeCount(), QueuesBefore);

	Buildings->ClearLocalEconomyAuthorityForTesting();
	FString Reason;
	if (!TestTrue(TEXT("Actual unknown-civilization local preview can still upgrade"),
		Buildings->CanStartUpgrade(Hall, Reason))) { AddError(Reason); return false; }
	const FRokResourceCost Cost = Buildings->GetNextUpgradeCost(Hall);
	if (!TestTrue(TEXT("Source Hall cost is nonzero and affordable"),
		(Cost.Food > 0 || Cost.Wood > 0 || Cost.Stone > 0 || Cost.Gold > 0)
		&& Before.Food >= Cost.Food && Before.Wood >= Cost.Wood
		&& Before.Stone >= Cost.Stone && Before.Gold >= Cost.Gold)) return false;
	if (!TestTrue(TEXT("Local preview starts original timed upgrade"),
		Buildings->StartUpgrade(Hall, Reason))) { AddError(Reason); return false; }
	const FRokResourceStock AfterLocal = Buildings->GetCurrentResources();
	TestEqual(TEXT("Local debit food"), AfterLocal.Food, Before.Food - Cost.Food);
	TestEqual(TEXT("Local debit wood"), AfterLocal.Wood, Before.Wood - Cost.Wood);
	TestEqual(TEXT("Local debit stone"), AfterLocal.Stone, Before.Stone - Cost.Stone);
	TestEqual(TEXT("Local debit gold"), AfterLocal.Gold, Before.Gold - Cost.Gold);
	TestEqual(TEXT("Local upgrade occupies exactly one queue"),
		Buildings->GetActiveUpgradeCount(), QueuesBefore + 1);
	return true;
}

#endif
