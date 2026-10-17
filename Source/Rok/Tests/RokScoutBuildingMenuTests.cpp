#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "../RokBuildingPanelWidget.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokScoutScreenWidget.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokScoutBuildingMenuFixtureTest,
	"Rok.Ui.Scout.BuildingMenuFixtureRoute", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokScoutBuildingMenuFixtureTest::RunTest(const FString&)
{
	// Isolated transient world and tagged actor are an explicit fixture. This test
	// does not create a Scout Camp in the real city or establish any server queue.
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokScoutBuildingMenuFixture"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Isolated fixture world"), World)) return false;
	FActorSpawnParameters Params;
	Params.Name = TEXT("ScoutCamp_ExplicitAutomationFixture"); Params.ObjectFlags |= RF_Transient;
	auto* Camp = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
	if (!TestNotNull(TEXT("Transient fixture camp"), Camp)) return false;
	Camp->Tags.Add(TEXT("UnityBuildingType_28"));
	Camp->Tags.Add(TEXT("UnityBuildingLevel_5"));
	Camp->Tags.Add(TEXT("RokAutomationFixture_NotRealCityState"));
	auto* Upgrade = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Actual building state subsystem"), Upgrade)) return false;
	const auto State = Upgrade->GetBuildingStateForActor(Camp);
	TestEqual(TEXT("Fixture tag resolves source type 28"), State.UnityBuildingTypeId, 28);
	TestEqual(TEXT("Fixture tag resolves explicit known level 5"), State.Level, 5);
	TestFalse(TEXT("Camp fixture is not upgrading"), State.bUpgradeInProgress);
	auto* Hud = NewObject<URokMainHudWidget>(World);
	Hud->Initialize(); Hud->TakeWidget(); Hud->SetSelectedBuilding(Camp);
	auto* Panel = Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget")));
	auto* Scout = Cast<URokScoutScreenWidget>(Hud->GetWidgetFromName(TEXT("ScoutScreenWidget")));
	auto* Feature = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	if (!TestNotNull(TEXT("Actual main HUD panel"), Panel) || !TestNotNull(TEXT("Actual HUD Scout child"), Scout)
		|| !TestNotNull(TEXT("Shared feature host"), Feature) || !TestNotNull(TEXT("Shared modal host"), Modal)) return false;
	Panel->AdvanceMenuAnimation(0.3f);
	auto* Button = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingFeatureButton")));
	auto* Icon = Cast<UImage>(Panel->GetWidgetFromName(TEXT("BuildingFeatureButtonImage")));
	if (!TestNotNull(TEXT("Actual building action button"), Button) || !TestNotNull(TEXT("Actual building action sprite"), Icon)) return false;
	TestEqual(TEXT("Source type 28 exposes its normal action"), Button->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("Source type 28 action accepts input"), Button->GetIsEnabled());
	UObject* Texture = Icon->GetBrush().GetResourceObject();
	if (!TestNotNull(TEXT("Real Scout action import is required"), Texture)) return false;
	TestEqual(TEXT("Scout uses its own btn_hud_1041 crop"), Texture->GetPathName(),
		FString(TEXT("/Game/RokPrototype/UI/BuildingMenu/rok_building_menu_scout.rok_building_menu_scout")));
	TestFalse(TEXT("Selecting the building alone does not auto-open Scout"), Scout->IsScreenOpen());
	Feature->OpenItems(); Modal->OpenPlayerProfile();
	// Delegate traverses the real Panel -> MainHUD -> Scout route. Not a direct Open call.
	Button->OnClicked.Broadcast();
	TestTrue(TEXT("Real HUD handler opens Scout for fixture building"), Scout->IsScreenOpen());
	TestFalse(TEXT("Route closes the previously open shared feature"), Feature->IsScreenOpen());
	TestFalse(TEXT("Route closes the previously open modal"), Modal->IsModalOpen());
	auto* Config = Cast<UTextBlock>(Scout->GetWidgetFromName(TEXT("ScoutCampConfiguration")));
	if (!TestNotNull(TEXT("Routed camp configuration"), Config)) return false;
	TestTrue(TEXT("Route forwards known level into capacity query"), Config->GetText().ToString().Contains(TEXT("可容纳 2 名斥候")));
	TestNotNull(TEXT("Even a known camp does not establish server queue state"), Scout->GetWidgetFromName(TEXT("ScoutQueueUnavailable")));
	Scout->CloseImmediately(); Hud->SetSelectedBuilding(nullptr);
	return true;
}
#endif
