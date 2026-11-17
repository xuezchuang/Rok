#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Misc/Paths.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokServerInventorySubsystem.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderSummonWbpTest,
	"Rok.City.Ui.CommanderSummonWbp", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderSummonWbpTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCommanderSummonWbp"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	auto* Heroes = Instance->GetSubsystem<URokServerHeroSubsystem>();
	auto* Items = Instance->GetSubsystem<URokServerInventorySubsystem>();
	auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Hero fixture"), Heroes) || !TestNotNull(TEXT("Inventory fixture"), Items)
		|| !TestNotNull(TEXT("Catalog"), Catalog)) return false;
	ON_SCOPE_EXIT { Heroes->ClearSnapshotForTesting(); Items->ClearSnapshotForTesting(); };
	FRokServerHeroSnapshot HeroSnapshot;
	HeroSnapshot.bAvailable = HeroSnapshot.bLoaded = HeroSnapshot.bHeroesKnown = HeroSnapshot.bRowsComplete = true;
	HeroSnapshot.Signature = TEXT("summon-empty-heroes");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Feature initializes"), Screen->Initialize())) return false;
	Screen->TakeWidget();
	FRokServerHeroSnapshot WaitingSnapshot;
	WaitingSnapshot.bAvailable = true;
	WaitingSnapshot.Signature = TEXT("summon-waiting");
	Heroes->SetSnapshotForTesting(WaitingSnapshot);
	Screen->OpenCommander();
	if (URokUiSettings::IsLegacyForced())
	{
		FString Error;
		const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUi/Specs/commander_server_waiting.json");
		TestTrue(FString::Printf(TEXT("Export commander waiting state: %s"), *Error), Screen->ExportCurrentModeSpec(Path, Error));
	}
	TestNotNull(TEXT("Server waiting message"), Screen->GetWidgetFromName(TEXT("CommanderServerWaiting")));
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->OpenCommander();
	FRokUnityCatalogHero Hero;
	if (!TestTrue(TEXT("Unowned hero configuration"), Catalog->GetHero(1002, Hero))
		|| !TestTrue(TEXT("Recruit has source item requirement"), Hero.GetItem > 0 && Hero.GetItemNum > 0)) return false;
	FRokServerInventorySnapshot Inventory;
	Inventory.bAvailable = Inventory.bLoaded = Inventory.bItemsKnown = Inventory.bRowsComplete = true;
	Inventory.Signature = TEXT("summon-enough");
	auto& Stack = Inventory.Stacks.AddDefaulted_GetRef();
	Stack.ItemIndex = 1;
	Stack.ItemId = Hero.GetItem;
	Stack.Overlay = Hero.GetItemNum;
	Stack.bCatalogKnown = true;
	Items->SetSnapshotForTesting(Inventory);
	Screen->RefreshServerPlayerAssetsForTesting();
	Screen->SelectCommanderHero(1002);
	auto* Summon = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSummonAction")));
	if (TestNotNull(TEXT("Summon button"), Summon))
		TestTrue(TEXT("Source inventory enables summon"), Summon->GetIsEnabled());
	if (URokUiSettings::IsLegacyForced())
	{
		FString Error;
		const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUi/Specs/commander_summon_enabled.json");
		TestTrue(FString::Printf(TEXT("Export enabled summon: %s"), *Error), Screen->ExportCurrentModeSpec(Path, Error));
	}
	Inventory.Stacks[0].Overlay = 0;
	Inventory.Signature = TEXT("summon-empty");
	Items->SetSnapshotForTesting(Inventory);
	Screen->RefreshServerPlayerAssetsForTesting();
	Summon = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSummonAction")));
	if (TestNotNull(TEXT("Summon button after inventory update"), Summon))
		TestFalse(TEXT("Insufficient inventory disables summon"), Summon->GetIsEnabled());
	FRokServerHeroEntry& UnknownHero = HeroSnapshot.Heroes.AddDefaulted_GetRef();
	UnknownHero.HeroId = 999999;
	UnknownHero.Name = TEXT("LocalMissingPreviewFixture");
	HeroSnapshot.Signature = TEXT("summon-missing-preview");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	AddExpectedMessage(TEXT("Commander 999999 could not load its generated source animation"),
		EAutomationExpectedMessageFlags::Contains, 1);
	Screen->SelectCommanderHero(999999);
	TestNotNull(TEXT("Missing model displays a status message"), Screen->GetWidgetFromName(TEXT("CommanderPreviewUnavailable")));
	return true;
}
#endif
