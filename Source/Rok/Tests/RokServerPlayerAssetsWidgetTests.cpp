#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "../RokCommanderMeshWidget.h"
#include "Components/TextBlock.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokServerInventorySubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerPlayerAssetsWidgetTest,
	"Rok.Net.ServerPlayerAssets.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerPlayerAssetsWidgetTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerPlayerAssetsWidgetTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	auto* Heroes = Instance->GetSubsystem<URokServerHeroSubsystem>();
	auto* Items = Instance->GetSubsystem<URokServerInventorySubsystem>();
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Hero source"), Heroes)
		|| !TestNotNull(TEXT("Inventory source"), Items)
		|| !TestNotNull(TEXT("Local resources"), Buildings)) return false;
	ON_SCOPE_EXIT { Heroes->ClearSnapshotForTesting(); Items->ClearSnapshotForTesting(); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Widget initializes"), Screen->Initialize())) return false;
	Screen->TakeWidget();
	auto Text = [Screen](const TCHAR* Name) -> FString
	{
		const UTextBlock* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
		return Label ? Label->GetText().ToString() : FString();
	};

	Screen->OpenCommander();
	TestEqual(TEXT("Offline commander does not present bootstrap heroes as owned"),
		Text(TEXT("CommanderOwnedCount")), FString(TEXT("离线预览")));
	Screen->SelectCommanderHero(1006);
	TestTrue(TEXT("Osman preview includes level-one power"), Text(TEXT("CommanderPower")).Contains(TEXT("4,500")));
	TestNull(TEXT("Offline catalog preview does not expose account skill upgrades"),
		Screen->GetWidgetFromName(TEXT("CommanderSkillUpgrade")));

	FRokServerHeroSnapshot HeroSnapshot;
	HeroSnapshot.bAvailable = true;
	HeroSnapshot.Signature = TEXT("hero-unknown");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->OpenCommander();
	TestTrue(TEXT("Missing hero collection waits"), Text(TEXT("CommanderServerWaiting")).Contains(TEXT("待服务器同步")));
	TestTrue(TEXT("Bootstrap hero is absent while connected"), Screen->GetSelectedCommanderIdForAudit() != 1006);

	HeroSnapshot.bLoaded = true;
	HeroSnapshot.bHeroesKnown = true;
	HeroSnapshot.bRowsComplete = true;
	HeroSnapshot.Signature = TEXT("hero-empty");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Known empty hero ownership"), Text(TEXT("CommanderOwnedCount")).Contains(TEXT("尚未拥有")));

	FRokServerHeroEntry& Hero4000 = HeroSnapshot.Heroes.AddDefaulted_GetRef();
	Hero4000.HeroId = 4000;
	Hero4000.Name = TEXT("测试统帅4000");
	Hero4000.bCatalogKnown = true;
	Hero4000.Level = 5;
	Hero4000.Star = 2;
	Hero4000.Exp = 123;
	Hero4000.bLevelKnown = true;
	Hero4000.bStarKnown = true;
	Hero4000.bExpKnown = true;
	FRokServerHeroEntry& Hero2007 = HeroSnapshot.Heroes.AddDefaulted_GetRef();
	Hero2007.HeroId = 2007;
	Hero2007.bCatalogKnown = true;
	Hero2007.Level = 7;
	Hero2007.Star = 3;
	Hero2007.bLevelKnown = true;
	Hero2007.bStarKnown = true;
	Hero2007.bSkillsKnown = true;
	Hero2007.bTalentTreesKnown = true;
	Hero2007.bTalentIndexKnown = true;
	Hero2007.TalentIndex = 1;
	FRokServerHeroTalentTree& Tree = Hero2007.TalentTrees.AddDefaulted_GetRef();
	Tree.Index = 1;
	Tree.Name = TEXT("方案一");
	FRokServerHeroSkill& Skill = Hero2007.Skills.AddDefaulted_GetRef();
	Skill.SkillId = 200701;
	Skill.Level = 3;
	Skill.bLevelKnown = true;
	HeroSnapshot.Signature = TEXT("hero-two");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Server owned count"), Text(TEXT("CommanderOwnedCount")).Contains(TEXT("2")));
	Screen->SelectCommanderHero(1002);
	TestNotNull(TEXT("Ivan has a source animated mesh"),
		Cast<URokCommanderMeshWidget>(Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait"))));
	const auto* Scene = Cast<UImage>(Screen->GetWidgetFromName(TEXT("CommanderSceneBackground")));
	if (TestNotNull(TEXT("Commander scene image"), Scene))
		TestEqual(TEXT("Ivan's environment is visible"), Scene->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Ivan preview power includes the initial skill"), Text(TEXT("CommanderPower")).Contains(TEXT("4,200")));
	TestTrue(TEXT("Ivan's configured initial EXP"), Text(TEXT("CommanderExp")).Contains(TEXT("0/120")));
	TestNotNull(TEXT("Unowned commander displays stars"), Screen->GetWidgetFromName(TEXT("CommanderStar_0")));
	TestEqual(TEXT("Unowned commander displays its default skill level"), Text(TEXT("CommanderSkillIndex_0")), FString(TEXT("1")));
	TestTrue(TEXT("Later skill has no fabricated level"), Text(TEXT("CommanderSkillIndex_1")).IsEmpty());
	auto* Recruit = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSummonAction")));
	if (TestNotNull(TEXT("Unowned commander has a real recruit button"), Recruit))
		TestFalse(TEXT("Recruit stays disabled while item balance is unknown"), Recruit->GetIsEnabled());
	Screen->SelectCommanderHero(4000);
	TestEqual(TEXT("Real hero 4000 selectable"), Screen->GetSelectedCommanderIdForAudit(), 4000);
	TestTrue(TEXT("Real hero level"), Text(TEXT("CommanderLevel")).Contains(TEXT("5")));
	TestTrue(TEXT("Real hero exp"), Text(TEXT("CommanderServerProgress")).Contains(TEXT("123")));
	TestTrue(TEXT("No fabricated account power"), Text(TEXT("CommanderPower")).Contains(TEXT("待同步")));
	Screen->SelectCommanderHero(2007);
	TestTrue(TEXT("Server skill level"), Text(TEXT("CommanderSelectedSkillSummary")).Contains(TEXT("Lv.3")));
	TestTrue(TEXT("Server talent index"), Text(TEXT("CommanderServerTalentState")).Contains(TEXT("方案 1")));
	HeroSnapshot.Heroes[1].Level = 8;
	HeroSnapshot.Heroes[1].Skills[0].Level = 4;
	HeroSnapshot.Heroes[1].TalentIndex = 2;
	HeroSnapshot.Signature = TEXT("hero-progress-changed");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Hero level refresh"), Text(TEXT("CommanderLevel")).Contains(TEXT("8")));
	TestTrue(TEXT("Skill level refresh"), Text(TEXT("CommanderSelectedSkillSummary")).Contains(TEXT("Lv.4")));
	TestTrue(TEXT("Talent selection refresh"), Text(TEXT("CommanderServerTalentState")).Contains(TEXT("方案 2")));
	HeroSnapshot.Heroes.RemoveAt(1);
	HeroSnapshot.Signature = TEXT("hero-2007-removed");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestEqual(TEXT("Removed selection falls back to real owned hero"), Screen->GetSelectedCommanderIdForAudit(), 4000);
	const FRokServerHeroEntry SurvivingHero = HeroSnapshot.Heroes[0];
	HeroSnapshot.Heroes.Reset();
	HeroSnapshot.bRowsComplete = false;
	HeroSnapshot.Signature = TEXT("hero-sparse");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Sparse hero collection is not called empty"),
		Text(TEXT("CommanderServerWaiting")).Contains(TEXT("待服务器同步")));
	HeroSnapshot.Heroes.Add(SurvivingHero);
	HeroSnapshot.bRowsComplete = true;
	HeroSnapshot.Signature = TEXT("hero-restored");
	Heroes->SetSnapshotForTesting(HeroSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	UButton* Talent = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderTalent")));
	if (!TestNotNull(TEXT("Owned hero talent boundary action"), Talent)) return false;
	Talent->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestTrue(TEXT("Talent editor waits for the account talent snapshot"),
		Text(TEXT("CommanderTalentEditorWaiting")).Contains(TEXT("待同步")));

	FRokServerInventorySnapshot ItemSnapshot;
	ItemSnapshot.bAvailable = true;
	ItemSnapshot.Signature = TEXT("items-unknown");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->OpenItems(true); // Connected inventory must override the explicit offline fixture.
	TestTrue(TEXT("Missing item collection waits"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("待服务器同步")));
	TestTrue(TEXT("Connected view has no fixture stacks"), Screen->GetVisibleItemsInventoryFingerprintForAudit().IsEmpty());
	ItemSnapshot.bLoaded = true;
	ItemSnapshot.bItemsKnown = true;
	ItemSnapshot.bRowsComplete = true;
	ItemSnapshot.Signature = TEXT("items-empty");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Known empty inventory"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("空空如也")));
	FRokServerInventoryStack& Stack0 = ItemSnapshot.Stacks.AddDefaulted_GetRef();
	Stack0.ItemIndex = 900;
	Stack0.ItemId = 101050010;
	Stack0.Overlay = 17;
	Stack0.BagType = 1;
	Stack0.bCatalogKnown = true;
	FRokServerInventoryStack& Stack1 = ItemSnapshot.Stacks.AddDefaulted_GetRef();
	Stack1.ItemIndex = 901;
	Stack1.ItemId = 101050010;
	Stack1.Overlay = 4;
	Stack1.BagType = 1;
	Stack1.bCatalogKnown = true;
	ItemSnapshot.Signature = TEXT("items-two-stacks");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Duplicate item IDs retain distinct stack indices"),
		Screen->GetVisibleItemsInventoryFingerprintForAudit().Contains(TEXT("900:101050010:17"))
		&& Screen->GetVisibleItemsInventoryFingerprintForAudit().Contains(TEXT("901:101050010:4")));
	Screen->SelectItemsStack(901);
	TestEqual(TEXT("Stack identity selected"), Screen->GetSelectedItemsStackIndexForAudit(), static_cast<int64>(901));
	TestTrue(TEXT("Selected stack overlay"), Text(TEXT("ItemsOwned")).Contains(TEXT("4")));
	const FString BeforeUse = Screen->GetVisibleItemsInventoryFingerprintForAudit();
	const FRokResourceStock BeforeResources = Buildings->GetCurrentResources();
	UButton* Use = Cast<UButton>(Screen->GetWidgetFromName(TEXT("ItemsUse")));
	if (!TestNotNull(TEXT("Catalog backed use boundary action"), Use)) return false;
	Use->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	TestTrue(TEXT("Use reports server boundary"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未改变")));
	TestEqual(TEXT("Use leaves stack state unchanged"), Screen->GetVisibleItemsInventoryFingerprintForAudit(), BeforeUse);
	TestEqual(TEXT("Use leaves food unchanged"), Buildings->GetCurrentResources().Food, BeforeResources.Food);
	TestEqual(TEXT("Use leaves wood unchanged"), Buildings->GetCurrentResources().Wood, BeforeResources.Wood);
	TestEqual(TEXT("Use leaves stone unchanged"), Buildings->GetCurrentResources().Stone, BeforeResources.Stone);
	TestEqual(TEXT("Use leaves gold unchanged"), Buildings->GetCurrentResources().Gold, BeforeResources.Gold);
	ItemSnapshot.Stacks[0].Overlay = 20;
	ItemSnapshot.Signature = TEXT("items-overlay-changed");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestEqual(TEXT("Overlay refresh preserves selected itemIndex"), Screen->GetSelectedItemsStackIndexForAudit(), static_cast<int64>(901));
	ItemSnapshot.Stacks.RemoveAt(1);
	ItemSnapshot.Signature = TEXT("items-selection-removed");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestEqual(TEXT("Removed stack selects surviving stack"), Screen->GetSelectedItemsStackIndexForAudit(), static_cast<int64>(900));
	Screen->SelectItemsCategory(1);
	TestTrue(TEXT("Native category filter"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("空空如也")));
	Screen->SelectItemsCategory(0);
	TestTrue(TEXT("Category switch restores server stack"), Screen->GetVisibleItemsInventoryFingerprintForAudit().Contains(TEXT("900:101050010:20")));
	ItemSnapshot.Stacks.Reset();
	FRokServerInventoryStack& Unknown = ItemSnapshot.Stacks.AddDefaulted_GetRef();
	Unknown.ItemIndex = 1001;
	Unknown.ItemId = 999999;
	Unknown.Overlay = 2;
	Unknown.bCatalogKnown = false;
	ItemSnapshot.Signature = TEXT("items-config-unknown");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->SelectItemsCategory(4);
	TestTrue(TEXT("Unknown catalog stack remains identifiable"),
		Screen->GetVisibleItemsInventoryFingerprintForAudit().Contains(TEXT("1001:999999:2")));
	TestTrue(TEXT("Unknown catalog is marked partial"), Text(TEXT("ItemsInventorySyncState")).Contains(TEXT("待同步")));
	ItemSnapshot.Stacks.Reset();
	ItemSnapshot.bRowsComplete = false;
	ItemSnapshot.Signature = TEXT("items-sparse");
	Items->SetSnapshotForTesting(ItemSnapshot);
	Screen->RefreshServerPlayerAssetsForTesting();
	TestTrue(TEXT("Sparse collection is not called empty"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("待同步")));
	return true;
}

#endif
