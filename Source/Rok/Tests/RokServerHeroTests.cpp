#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokServerInventorySubsystem.h"
#include "../RokUnityConfigSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

namespace
{
TSharedPtr<FJsonObject> Hero(int32 Id, int32 Level, int32 SkillId)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("heroId"), Id);
	Row->SetNumberField(TEXT("level"), Level);
	Row->SetNumberField(TEXT("star"), 1);
	Row->SetNumberField(TEXT("exp"), 0);
	Row->SetNumberField(TEXT("starExp"), 0);
	Row->SetNumberField(TEXT("talentIndex"), 1);
	Row->SetNumberField(TEXT("head"), 0);
	TSharedPtr<FJsonObject> Skill = MakeShared<FJsonObject>();
	Skill->SetNumberField(TEXT("skillId"), SkillId);
	Skill->SetNumberField(TEXT("skillLevel"), 1);
	TArray<TSharedPtr<FJsonValue>> Skills;
	Skills.Add(MakeShared<FJsonValueObject>(Skill));
	Row->SetArrayField(TEXT("skills"), Skills);
	Row->SetArrayField(TEXT("talentTrees"), {});
	return Row;
}

TSharedPtr<FJsonObject> Recorded()
{
	TSharedPtr<FJsonObject> Rows = MakeShared<FJsonObject>();
	Rows->SetObjectField(TEXT("4000"), Hero(4000, 1, 400001));
	Rows->SetObjectField(TEXT("2007"), Hero(2007, 1, 200701));
	return Rows;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerHeroProjectionTest,
	"Rok.Net.ServerHero.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerHeroProjectionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerHero_%s/L_RokServerHero"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Instance->InitializeStandalone(TEXT("L_RokServerHero"), NewObject<UPackage>(nullptr, PackageName, RF_Transient));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Catalog"), Catalog) || !TestTrue(TEXT("Catalog loaded"), Catalog->IsCatalogLoaded())) return false;

	const FRokServerHeroSnapshot Missing = URokServerHeroSubsystem::ProjectSnapshot(nullptr, Catalog);
	TestTrue(TEXT("RoleReady while collection absent"), Missing.bAvailable);
	TestFalse(TEXT("Missing hero collection unknown"), Missing.bHeroesKnown);
	TestFalse(TEXT("Missing hero collection incomplete"), Missing.bRowsComplete);
	TestFalse(TEXT("Missing hero collection not loaded"), Missing.bLoaded);
	const FRokServerHeroSnapshot Empty = URokServerHeroSubsystem::ProjectSnapshot(MakeShared<FJsonObject>(), Catalog);
	TestTrue(TEXT("Empty hero collection known"), Empty.bHeroesKnown);
	TestTrue(TEXT("Empty hero collection complete"), Empty.bRowsComplete);
	TestTrue(TEXT("Empty hero collection loaded"), Empty.bLoaded);
	TestEqual(TEXT("Empty hero count"), Empty.Heroes.Num(), 0);
	TestNotEqual(TEXT("Missing versus empty signature"), Missing.Signature, Empty.Signature);

	const TSharedPtr<FJsonObject> Rows = Recorded();
	const FRokServerHeroSnapshot Snapshot = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog);
	TestEqual(TEXT("Recorded hero count"), Snapshot.Heroes.Num(), 2);
	TestTrue(TEXT("Recorded rows complete"), Snapshot.bRowsComplete);
	if (Snapshot.Heroes.Num() != 2) return false;
	TestEqual(TEXT("Sorted first hero"), Snapshot.Heroes[0].HeroId, 2007);
	TestEqual(TEXT("Sorted second hero"), Snapshot.Heroes[1].HeroId, 4000);
	TestTrue(TEXT("Zero exp known"), Snapshot.Heroes[0].bExpKnown);
	TestEqual(TEXT("Zero exp"), Snapshot.Heroes[0].Exp, int64(0));
	TestTrue(TEXT("Empty talent tree list known"), Snapshot.Heroes[0].bTalentTreesKnown);
	TestEqual(TEXT("Empty talent tree list"), Snapshot.Heroes[0].TalentTrees.Num(), 0);
	TestTrue(TEXT("Recorded skill known"), Snapshot.Heroes[0].bSkillsKnown);
	if (!TestEqual(TEXT("Recorded skill count"), Snapshot.Heroes[0].Skills.Num(), 1)) return false;
	TestEqual(TEXT("Recorded skill ID"), Snapshot.Heroes[0].Skills[0].SkillId, 200701);
	TestEqual(TEXT("Recorded skill level"), Snapshot.Heroes[0].Skills[0].Level, 1);
	TestTrue(TEXT("Zero gear known"), Snapshot.Heroes[0].Gear.bHeadKnown);
	Rows->Values[TEXT("2007")]->AsObject()->SetNumberField(TEXT("head"), 4294967297.0);
	const FRokServerHeroSnapshot LargeGear = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog);
	TestEqual(TEXT("64-bit gear index"), LargeGear.Heroes[0].Gear.Head, int64(4294967297LL));
	TestNotEqual(TEXT("Gear changes signature"), LargeGear.Signature, Snapshot.Signature);
	Rows->Values[TEXT("2007")]->AsObject()->SetNumberField(TEXT("head"), 0);
	TestTrue(TEXT("Catalog identity"), Snapshot.Heroes[0].bCatalogKnown);
	TestFalse(TEXT("Catalog name not fabricated"), Snapshot.Heroes[0].Name.IsEmpty());

	// A sparse row is still owned; absent fields remain unknown rather than zero.
	TSharedPtr<FJsonObject> SparseRows = MakeShared<FJsonObject>();
	SparseRows->SetObjectField(TEXT("2007"), MakeShared<FJsonObject>());
	SparseRows->SetObjectField(TEXT("0"), Hero(0, 1, 1));
	SparseRows->SetObjectField(TEXT("3000"), MakeShared<FJsonObject>());
	const FRokServerHeroSnapshot Sparse = URokServerHeroSubsystem::ProjectSnapshot(SparseRows, Catalog);
	TestEqual(TEXT("Only positive owned keys"), Sparse.Heroes.Num(), 2);
	TestFalse(TEXT("Invalid key makes collection incomplete"), Sparse.bRowsComplete);
	if (Sparse.Heroes.Num() != 2) return false;
	TestEqual(TEXT("Sparse sorted first"), Sparse.Heroes[0].HeroId, 2007);
	TestFalse(TEXT("Missing level unknown"), Sparse.Heroes[0].bLevelKnown);
	TestFalse(TEXT("Missing exp unknown"), Sparse.Heroes[0].bExpKnown);
	TestFalse(TEXT("Missing skills unknown"), Sparse.Heroes[0].bSkillsKnown);
	TestFalse(TEXT("Missing talent trees unknown"), Sparse.Heroes[0].bTalentTreesKnown);
	TestFalse(TEXT("Missing gear unknown"), Sparse.Heroes[0].Gear.bHeadKnown);
	TestNotEqual(TEXT("Sparse differs from recorded"), Sparse.Signature, Snapshot.Signature);
	TSharedPtr<FJsonObject> ValidSparseRows = MakeShared<FJsonObject>();
	ValidSparseRows->SetObjectField(TEXT("2007"), MakeShared<FJsonObject>());
	const FRokServerHeroSnapshot ValidSparse = URokServerHeroSubsystem::ProjectSnapshot(ValidSparseRows, Catalog);
	TestTrue(TEXT("Valid sparse hero row remains complete"), ValidSparse.bRowsComplete);
	TestEqual(TEXT("Valid sparse hero remains owned"), ValidSparse.Heroes.Num(), 1);
	TSharedPtr<FJsonObject> BadRow = MakeShared<FJsonObject>();
	BadRow->SetField(TEXT("2007"), MakeShared<FJsonValueNumber>(0));
	const FRokServerHeroSnapshot Malformed = URokServerHeroSubsystem::ProjectSnapshot(BadRow, Catalog);
	TestFalse(TEXT("Nonobject row incomplete"), Malformed.bRowsComplete);
	TestEqual(TEXT("Malformed row not displayed"), Malformed.Heroes.Num(), 0);
	TestNotEqual(TEXT("Malformed differs from empty"), Malformed.Signature, Empty.Signature);

	// HeroInfo has no delete/tombstone field. Unknown fields must not remove a row.
	TSharedPtr<FJsonObject> SparseUpdate = MakeShared<FJsonObject>();
	SparseUpdate->SetNumberField(TEXT("level"), 2);
	Rows->SetObjectField(TEXT("2007"), SparseUpdate);
	const FRokServerHeroSnapshot ChangedLevel = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog);
	TestEqual(TEXT("Sparse update remains owned"), ChangedLevel.Heroes.Num(), 2);
	TestEqual(TEXT("Sparse update level"), ChangedLevel.Heroes[0].Level, 2);
	TestNotEqual(TEXT("Level changes signature"), ChangedLevel.Signature, Snapshot.Signature);

	Rows->SetObjectField(TEXT("2007"), Hero(2007, 1, 200701));
	const TArray<TSharedPtr<FJsonValue>>* SkillArray = nullptr;
	Rows->Values[TEXT("2007")]->AsObject()->TryGetArrayField(TEXT("skills"), SkillArray);
	(*SkillArray)[0]->AsObject()->SetNumberField(TEXT("skillLevel"), 2);
	const FRokServerHeroSnapshot ChangedSkill = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog);
	TestEqual(TEXT("Changed skill level"), ChangedSkill.Heroes[0].Skills[0].Level, 2);
	TestNotEqual(TEXT("Skill level changes signature"), ChangedSkill.Signature, Snapshot.Signature);

	TSharedPtr<FJsonObject> Tree = MakeShared<FJsonObject>();
	Tree->SetNumberField(TEXT("index"), 1);
	Tree->SetStringField(TEXT("name"), TEXT("First"));
	Tree->SetArrayField(TEXT("talentTree"), {MakeShared<FJsonValueNumber>(31), MakeShared<FJsonValueNumber>(42)});
	TSharedPtr<FJsonObject> TreeMap = MakeShared<FJsonObject>();
	TreeMap->SetObjectField(TEXT("1"), Tree);
	Rows->Values[TEXT("2007")]->AsObject()->SetObjectField(TEXT("talentTrees"), TreeMap);
	const FRokServerHeroSnapshot WithTree = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog);
	TestEqual(TEXT("Keyed talent tree"), WithTree.Heroes[0].TalentTrees.Num(), 1);
	TestEqual(TEXT("Talent IDs preserved"), WithTree.Heroes[0].TalentTrees[0].TalentIds.Num(), 2);
	TestNotEqual(TEXT("Talent tree changes signature"), WithTree.Signature, ChangedSkill.Signature);

	TSharedPtr<FJsonObject> Reverse = MakeShared<FJsonObject>();
	Reverse->SetObjectField(TEXT("2007"), Rows->Values[TEXT("2007")]->AsObject());
	Reverse->SetObjectField(TEXT("4000"), Rows->Values[TEXT("4000")]->AsObject());
	TestEqual(TEXT("Map insertion order stable"), WithTree.Signature,
		URokServerHeroSubsystem::ProjectSnapshot(Reverse, Catalog).Signature);

	URokServerHeroSubsystem* Subsystem = Instance->GetSubsystem<URokServerHeroSubsystem>();
	if (TestNotNull(TEXT("Subsystem"), Subsystem))
	{
		Subsystem->SetSnapshotForTesting(WithTree);
		TestEqual(TEXT("Injected snapshot"), Subsystem->GetSnapshot().Signature, WithTree.Signature);
		Subsystem->ClearSnapshotForTesting();
		TestFalse(TEXT("Cleared snapshot offline"), Subsystem->GetSnapshot().bAvailable);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerHeroProgressionTest, "Rok.Net.ServerHero.Progression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerHeroProgressionTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerHeroProgressionTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Heroes = Instance->GetSubsystem<URokServerHeroSubsystem>();
	auto* Inventory = Instance->GetSubsystem<URokServerInventorySubsystem>();
	auto* Config = Instance->GetSubsystem<URokUnityConfigSubsystem>();
	const auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	const auto Rows = Recorded();
	const auto Snapshot = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog, Config);
	if (!TestEqual(TEXT("Recorded roster has two commanders"), Snapshot.Heroes.Num(), 2)) return false;
	const auto& Baibars = Snapshot.Heroes[0];
	TestTrue(TEXT("Baibars has derived power"), Baibars.bPowerKnown);
	TestEqual(TEXT("Baibars base + level + skill power"), Baibars.Power, 2300);
	Heroes->SetSnapshotForTesting(Snapshot);
	FRokServerInventorySnapshot Bag;
	Bag.bItemsKnown = Bag.bRowsComplete = true;
	auto& Sculptures = Bag.Stacks.AddDefaulted_GetRef();
	Sculptures.ItemId = 509020008;
	Sculptures.Overlay = 10;
	Inventory->SetSnapshotForTesting(Bag);

	FRokHeroCommand Request;
	Request.HeroId = 2007;
	Request.Type = ERokHeroCommand::UpgradeSkill;
	FRokHeroCommandCost Cost;
	FString Error;
	TestTrue(TEXT("First skill upgrade uses authoritative sculpture balance"), Heroes->GetCommandCost(Request, Cost, Error));
	TestEqual(TEXT("First skill upgrade costs ten sculptures"), Cost.ItemCount, 10);
	TestEqual(TEXT("First skill upgrade uses Baibars sculptures"), Cost.ItemId, 509020008);
	TestFalse(TEXT("Old skill push does not confirm the upgrade"),
		URokServerHeroSubsystem::IsCommandApplied(Request, Rows, 200701, 2));
	Rows->Values[TEXT("2007")]->AsObject()->GetArrayField(TEXT("skills"))[0]->AsObject()
		->SetNumberField(TEXT("skillLevel"), 2);
	TestTrue(TEXT("Matching hero and skill level confirms the upgrade"),
		URokServerHeroSubsystem::IsCommandApplied(Request, Rows, 200701, 2));

	TArray<FRokHeroTalentNode> Nodes;
	int32 Points = 0;
	TestTrue(TEXT("Baibars has a playable talent page"), Heroes->GetTalentPage(Baibars, 1, Nodes, Points));
	TestEqual(TEXT("Level one grants one talent point"), Points, 1);
	TSet<int32> Groups;
	for (const auto& Node : Nodes) Groups.Add(Node.Group);
	TestEqual(TEXT("Talent page has the three configured branches"), Groups.Num(), 3);
	const auto* First = Nodes.FindByPredicate([](const auto& Node) { return Node.Level == 1; });
	if (!TestNotNull(TEXT("A first-tier talent is available"), First)) return false;
	Request.Type = ERokHeroCommand::LearnTalent;
	Request.Page = 1;
	Request.TalentId = First->Id;
	TestTrue(TEXT("The first tier can be learned with the available point"), Heroes->GetCommandCost(Request, Cost, Error));
	TestFalse(TEXT("Empty page cannot confirm a learned node"), URokServerHeroSubsystem::IsCommandApplied(Request, Rows, 0, 0));

	const auto Tree = MakeShared<FJsonObject>();
	Tree->SetNumberField(TEXT("index"), 1);
	Tree->SetArrayField(TEXT("talentTree"), {MakeShared<FJsonValueNumber>(First->Id)});
	const auto TreeMap = MakeShared<FJsonObject>();
	TreeMap->SetObjectField(TEXT("1"), Tree);
	Rows->Values[TEXT("2007")]->AsObject()->SetObjectField(TEXT("talentTrees"), TreeMap);
	TestTrue(TEXT("Matching page and talent confirms learning"), URokServerHeroSubsystem::IsCommandApplied(Request, Rows, 0, 0));
	const auto Learned = URokServerHeroSubsystem::ProjectSnapshot(Rows, Catalog, Config);
	TestTrue(TEXT("Updated skill and learned talent raise power"), Learned.Heroes[0].Power > Baibars.Power);
	Heroes->GetTalentPage(Learned.Heroes[0], 1, Nodes, Points);
	TestEqual(TEXT("Learning spends the point"), Points, 0);

	auto* Widget = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Commander widget initializes"), Widget->Initialize())) return false;
	Widget->TakeWidget();
	Widget->OpenCommander();
	auto* Talent = Cast<UButton>(Widget->GetWidgetFromName(TEXT("CommanderTalent")));
	if (!TestNotNull(TEXT("Owned commander talent action"), Talent)) return false;
	Talent->OnClicked.Broadcast();
	Widget->FlushPendingActionForTesting();
	TestNotNull(TEXT("Talent editor has a scrollable list"), Widget->GetWidgetFromName(TEXT("CommanderTalentNodeScroll")));
	TestNotNull(TEXT("Talent editor displays points"),
		Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("CommanderTalentPoints"))));
	return true;
}

#endif
