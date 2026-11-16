#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityModalWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalInstantBuildingUpgradeTest, "Rok.Runtime.LocalInstantBuildingUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FRokLocalInstantBuildingUpgradeTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalInstantBuildingUpgrade"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestNotNull(TEXT("City upgrade subsystem"), Buildings)
		|| !TestTrue(TEXT("City HUD initializes"), Hud->Initialize())) return false;
	Hud->TakeWidget();
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	const auto* GemText = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("GemResourceValue")));
	if (!TestNotNull(TEXT("City upgrade modal"), Modal)
		|| !TestNotNull(TEXT("City gem balance"), GemText)) return false;

	const auto Spawn = [World](const TCHAR* Id)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (Actor) Actor->Tags.Add(FName(Id));
		return Actor;
	};
	AActor* Hall = Spawn(TEXT("UnityInitId_5"));
	AActor* Storehouse = Spawn(TEXT("UnityInitId_3"));
	AActor* Barracks = Spawn(TEXT("UnityInitId_6"));
	if (!TestNotNull(TEXT("Town hall"), Hall) || !TestNotNull(TEXT("Warehouse"), Storehouse)
		|| !TestNotNull(TEXT("Barracks"), Barracks)) return false;

	const auto InstantButton = [Modal]()
	{
		return Cast<UButton>(Modal->GetWidgetFromName(TEXT("InstantUpgradeButton")));
	};
	const auto Feedback = [Modal]()
	{
		const auto* Text = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("UpgradeFeedback")));
		return Text ? Text->GetText().ToString() : FString();
	};

	FString Failure;
	const auto Before = Buildings->GetCurrentResources();
	Hud->RefreshFromSubsystem();
	TestEqual(TEXT("HUD starts at the isolated wallet balance"), GemText->GetText().ToString(), FString(TEXT("350")));
	Modal->OpenBuildingUpgrade(Storehouse);
	auto* Button = InstantButton();
	if (!TestNotNull(TEXT("Warehouse instant upgrade action"), Button)) return false;
	TestFalse(TEXT("Warehouse must wait for Town Hall level 2"), Button->GetIsEnabled());
	TestTrue(TEXT("Feedback explains the prerequisite"), Feedback().Contains(TEXT("需要 2 级市政厅")));
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Blocked click does not change level"), Buildings->GetBuildingStateForActor(Storehouse).Level, 1);
	TestEqual(TEXT("Blocked click does not debit gems"), Buildings->GetCurrentResources().Denar, Before.Denar);
	Modal->CloseModal();

	Modal->OpenBuildingUpgrade(Hall);
	Button = InstantButton();
	if (!TestNotNull(TEXT("Town hall instant upgrade action"), Button)
		|| !TestTrue(TEXT("Town Hall can immediately reach level 2"), Button->GetIsEnabled())) return false;
	const auto* Price = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("InstantUpgradeButtonSecondary")));
	if (TestNotNull(TEXT("Displayed town hall price"), Price))
	{
		TestEqual(TEXT("Town hall price comes from the config"), Price->GetText().ToString(), FString(TEXT("1 宝石")));
	}
	Button->OnClicked.Broadcast();
	TestTrue(TEXT("First click requests confirmation"), Feedback().Contains(TEXT("再次点击确认")));
	TestEqual(TEXT("First click has no gem debit"), Buildings->GetCurrentResources().Denar, Before.Denar);
	TestEqual(TEXT("First click has no resource debit"), Buildings->GetCurrentResources().Food, Before.Food);
	TestFalse(TEXT("Stale price cannot commit"), Buildings->StartLocalInstantUpgrade(Hall, 1, 2, Failure));
	Button->OnClicked.Broadcast();
	TestFalse(TEXT("Success closes the upgrade modal"), Modal->IsModalOpen());
	TestEqual(TEXT("Town hall reaches level 2 immediately"), Buildings->GetBuildingStateForActor(Hall).Level, 2);
	TestEqual(TEXT("Exact gem debit"), Buildings->GetCurrentResources().Denar, Before.Denar - 1);
	TestEqual(TEXT("Exact food debit"), Buildings->GetCurrentResources().Food, Before.Food - 3500);
	TestEqual(TEXT("Exact wood debit"), Buildings->GetCurrentResources().Wood, Before.Wood - 3500);
	TestEqual(TEXT("Immediate upgrade takes no queue slot"), Buildings->GetActiveUpgradeCount(), 0);
	TestFalse(TEXT("Stale level cannot double-charge"), Buildings->StartLocalInstantUpgrade(Hall, 1, 1, Failure));
	Hud->RefreshFromSubsystem();
	TestEqual(TEXT("HUD reflects the debit"), GemText->GetText().ToString(), FString(TEXT("349")));

	if (!TestTrue(TEXT("Start a timed barracks upgrade"), Buildings->StartUpgrade(Barracks, Failure))) return false;
	int64 Quote = 0;
	TestFalse(TEXT("The same building cannot upgrade twice"),
		Buildings->CanStartLocalInstantUpgrade(Barracks, 1, Quote, Failure));
	Modal->OpenBuildingUpgrade(Storehouse);
	Button = InstantButton();
	if (!TestNotNull(TEXT("Warehouse action after prerequisite"), Button)
		|| !TestTrue(TEXT("Warehouse can upgrade while barracks occupies the queue"), Button->GetIsEnabled())) return false;
	Button->OnClicked.Broadcast();
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("Warehouse reaches level 2"), Buildings->GetBuildingStateForActor(Storehouse).Level, 2);
	TestEqual(TEXT("Warehouse debits one gem"), Buildings->GetCurrentResources().Denar, Before.Denar - 2);
	TestEqual(TEXT("Barracks keeps its original queue slot"), Buildings->GetActiveUpgradeCount(), 1);

	TestTrue(TEXT("Gems can cover resource shortfalls"), Buildings->CanStartLocalInstantUpgrade(Hall, 2, Quote, Failure));
	TestEqual(TEXT("Quote includes time plus both resource deficits"), Quote, int64(45));
	if (!TestTrue(TEXT("Confirmed shortfall purchase commits"),
		Buildings->StartLocalInstantUpgrade(Hall, 2, Quote, Failure))) return false;
	const auto After = Buildings->GetCurrentResources();
	TestEqual(TEXT("No negative food when gems cover the difference"), After.Food, 0);
	TestEqual(TEXT("No negative wood when gems cover the difference"), After.Wood, 0);
	TestEqual(TEXT("Exact debit with shortfall"), After.Denar, Before.Denar - 2 - 45);
	TestEqual(TEXT("Power uses the same completion logic as timed upgrades"), Buildings->GetLocalCompletedBuildingPowerDelta(), 57);
	return true;
}
#endif
