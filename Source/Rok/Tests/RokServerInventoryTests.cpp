#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "../RokServerInventorySubsystem.h"
#include "../RokUnityEconomyCatalogSubsystem.h"

namespace
{
TSharedPtr<FJsonObject> Item(int64 Index, int64 Id, int64 Overlay, int64 HeroId = 0)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("itemIndex"), Index);
	Row->SetNumberField(TEXT("itemId"), Id);
	Row->SetNumberField(TEXT("overlay"), Overlay);
	Row->SetNumberField(TEXT("heroId"), HeroId);
	return Row;
}

void Add(TSharedPtr<FJsonObject>& Collection, int64 Index, int64 Id, int64 Overlay, int64 HeroId = 0)
{
	Collection->SetObjectField(FString::Printf(TEXT("%lld"), Index), Item(Index, Id, Overlay, HeroId));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerInventoryProjectionTest,
	"Rok.Net.ServerInventory.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerInventoryProjectionTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	const FName PackageName(*FString::Printf(TEXT("/Temp/RokServerInventory_%s/L_RokServerInventory"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Instance->InitializeStandalone(TEXT("L_RokServerInventory"), NewObject<UPackage>(nullptr, PackageName, RF_Transient));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	const URokUnityEconomyCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityEconomyCatalogSubsystem>();
	if (!TestNotNull(TEXT("Economy catalog"), Catalog)
		|| !TestTrue(TEXT("Packaged catalog loaded"), Catalog->IsCatalogLoaded())) return false;

	const FRokServerInventorySnapshot Missing = URokServerInventorySubsystem::ProjectSnapshot(nullptr, Catalog);
	TestTrue(TEXT("Role-ready projection available"), Missing.bAvailable);
	TestTrue(TEXT("Catalog loaded with missing collection"), Missing.bLoaded);
	TestFalse(TEXT("Missing collection stays unknown"), Missing.bItemsKnown);
	TestFalse(TEXT("Missing collection is incomplete"), Missing.bRowsComplete);
	TestEqual(TEXT("Missing collection has no rows"), Missing.Stacks.Num(), 0);
	const TSharedPtr<FJsonObject> EmptyCollection = MakeShared<FJsonObject>();
	const FRokServerInventorySnapshot Empty = URokServerInventorySubsystem::ProjectSnapshot(EmptyCollection, Catalog);
	TestTrue(TEXT("Empty collection is known"), Empty.bItemsKnown);
	TestTrue(TEXT("Known empty collection is complete"), Empty.bRowsComplete);
	TestEqual(TEXT("Known empty collection has no rows"), Empty.Stacks.Num(), 0);
	TestNotEqual(TEXT("Missing and empty signatures differ"), Missing.Signature, Empty.Signature);
	TSharedPtr<FJsonObject> Tombstones = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Deleted = MakeShared<FJsonObject>();
	Deleted->SetNumberField(TEXT("overlay"), 0);
	Tombstones->SetObjectField(TEXT("41"), Deleted); // Deletion push may omit itemId.
	Deleted = MakeShared<FJsonObject>();
	Deleted->SetNumberField(TEXT("overlay"), -1);
	Tombstones->SetObjectField(TEXT("42"), Deleted);
	const FRokServerInventorySnapshot DeletedOnly = URokServerInventorySubsystem::ProjectSnapshot(Tombstones, Catalog);
	TestTrue(TEXT("All tombstones are a known complete empty bag"), DeletedOnly.bRowsComplete);
	TestEqual(TEXT("All tombstones have no visible rows"), DeletedOnly.Stacks.Num(), 0);
	TSharedPtr<FJsonObject> IncompleteOnly = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> MissingOverlay = MakeShared<FJsonObject>();
	MissingOverlay->SetNumberField(TEXT("itemId"), 101040010);
	IncompleteOnly->SetObjectField(TEXT("43"), MissingOverlay);
	const FRokServerInventorySnapshot PartialEmpty = URokServerInventorySubsystem::ProjectSnapshot(IncompleteOnly, Catalog);
	TestTrue(TEXT("Sparse collection is present"), PartialEmpty.bItemsKnown);
	TestFalse(TEXT("Sparse collection is incomplete"), PartialEmpty.bRowsComplete);
	TestEqual(TEXT("Sparse-only visible count"), PartialEmpty.Stacks.Num(), 0);
	TestNotEqual(TEXT("Partial empty differs from known empty"), PartialEmpty.Signature, Empty.Signature);
	TSharedPtr<FJsonObject> MissingIdentityOnly = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> MissingIdentity = MakeShared<FJsonObject>();
	MissingIdentity->SetNumberField(TEXT("itemId"), 101040010);
	MissingIdentity->SetNumberField(TEXT("overlay"), 1);
	MissingIdentityOnly->SetObjectField(TEXT("not-an-index"), MissingIdentity);
	const FRokServerInventorySnapshot NoIdentity = URokServerInventorySubsystem::ProjectSnapshot(MissingIdentityOnly, Catalog);
	TestFalse(TEXT("Positive overlay without stack identity is incomplete"), NoIdentity.bRowsComplete);
	TestEqual(TEXT("No identity is not a visible stack"), NoIdentity.Stacks.Num(), 0);
	TSharedPtr<FJsonObject> UnknownOnly = MakeShared<FJsonObject>();
	Add(UnknownOnly, 44, 1999999999, 2);
	const FRokServerInventorySnapshot UnknownConfig = URokServerInventorySubsystem::ProjectSnapshot(UnknownOnly, Catalog);
	TestTrue(TEXT("Unknown catalog ID with valid source fields is complete"), UnknownConfig.bRowsComplete);
	TestEqual(TEXT("Unknown catalog ID remains a source stack"), UnknownConfig.Stacks.Num(), 1);
	if (UnknownConfig.Stacks.Num() == 1) TestFalse(TEXT("Unknown catalog metadata stays unknown"), UnknownConfig.Stacks[0].bCatalogKnown);

	// Same 21 item IDs and counts as the observed RoleReady LastSession Item_ItemInfo payload.
	const int64 Ids[] = {
		502030004, 101040010, 502020001, 101050020, 201010010, 201010013, 502070001,
		502070003, 501010001, 101050010, 509030003, 201030020, 101030010, 201020016,
		201020023, 101040030, 101030030, 101050030, 301040010, 301030010, 301110010
	};
	const int64 Counts[] = {8, 1, 1, 1, 32, 10, 10, 10, 8, 3, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	TSharedPtr<FJsonObject> RealShape = MakeShared<FJsonObject>();
	for (int32 I = 0; I < UE_ARRAY_COUNT(Ids); ++I) Add(RealShape, I + 1, Ids[I], Counts[I]);
	const FRokServerInventorySnapshot Live = URokServerInventorySubsystem::ProjectSnapshot(RealShape, Catalog);
	TestTrue(TEXT("Real-shape collection known"), Live.bItemsKnown);
	TestTrue(TEXT("Real-shape collection complete"), Live.bRowsComplete);
	TestEqual(TEXT("Real-shape stack count"), Live.Stacks.Num(), 21);
	if (Live.Stacks.Num() == 21)
	{
		TestEqual(TEXT("First source index"), Live.Stacks[0].ItemIndex, int64(1));
		TestEqual(TEXT("First source item ID"), Live.Stacks[0].ItemId, int64(502030004));
		TestEqual(TEXT("First source overlay"), Live.Stacks[0].Overlay, int64(8));
		TestTrue(TEXT("First source catalog row found"), Live.Stacks[0].bCatalogKnown);
		TestTrue(TEXT("First source bag type known"), Live.Stacks[0].BagType > 0);
		TestEqual(TEXT("Last source index"), Live.Stacks[20].ItemIndex, int64(21));
		int32 CountsByType[6] = {};
		int64 OverlaysByType[6] = {};
		for (const FRokServerInventoryStack& Stack : Live.Stacks)
		{
			if (Stack.BagType >= 1 && Stack.BagType <= 5)
			{
				++CountsByType[Stack.BagType];
				OverlaysByType[Stack.BagType] += Stack.Overlay;
			}
		}
		const int32 ExpectedCounts[6] = {0, 7, 5, 3, 0, 6};
		const int64 ExpectedOverlays[6] = {0, 9, 45, 3, 0, 42};
		for (int32 Type = 1; Type <= 5; ++Type)
		{
			TestEqual(FString::Printf(TEXT("Bag type %d stack count"), Type),
				CountsByType[Type], ExpectedCounts[Type]);
			TestEqual(FString::Printf(TEXT("Bag type %d overlay sum"), Type),
				OverlaysByType[Type], ExpectedOverlays[Type]);
		}
	}

	TSharedPtr<FJsonObject> Sparse = MakeShared<FJsonObject>();
	Add(Sparse, 9000000001LL, 101040010, 3);
	Add(Sparse, 9000000002LL, 101040010, 7, 101);
	Add(Sparse, 9000000003LL, 101050020, 0); // Server deletion marker.
	Add(Sparse, 9000000004LL, 101050020, -1);
	Add(Sparse, 9000000005LL, 1999999999, 2); // Unknown config ID stays source data.
	Sparse->SetObjectField(TEXT("9000000006"), MakeShared<FJsonObject>()); // Sparse, incomplete push.
	Sparse->SetStringField(TEXT("malformed"), TEXT("not an item object"));
	TSharedPtr<FJsonObject> KeyOnly = Item(9000000007LL, 101050010, 1);
	KeyOnly->RemoveField(TEXT("itemIndex"));
	Sparse->SetObjectField(TEXT("9000000007"), KeyOnly);
	const FRokServerInventorySnapshot Projected = URokServerInventorySubsystem::ProjectSnapshot(Sparse, Catalog);
	TestFalse(TEXT("Malformed row makes collection incomplete"), Projected.bRowsComplete);
	TestEqual(TEXT("Only live complete stacks are visible"), Projected.Stacks.Num(), 4);
	if (Projected.Stacks.Num() == 4)
	{
		TestEqual(TEXT("64-bit source index preserved"), Projected.Stacks[0].ItemIndex, int64(9000000001LL));
		TestEqual(TEXT("Duplicate config IDs remain separate"), Projected.Stacks[1].ItemId, Projected.Stacks[0].ItemId);
		TestEqual(TEXT("Equipped stack retained"), Projected.Stacks[1].HeroId, int64(101));
		TestEqual(TEXT("Equipped stack quantity retained"), Projected.Stacks[1].Overlay, int64(7));
		TestFalse(TEXT("Unknown config is not fabricated"), Projected.Stacks[2].bCatalogKnown);
		TestTrue(TEXT("Unknown config name empty"), Projected.Stacks[2].Name.IsEmpty());
		TestEqual(TEXT("Unknown config bag type empty"), Projected.Stacks[2].BagType, 0);
		TestEqual(TEXT("Sparse index falls back to map key"), Projected.Stacks[3].ItemIndex, int64(9000000007LL));
	}
	const FRokServerInventorySnapshot Repeat = URokServerInventorySubsystem::ProjectSnapshot(Sparse, Catalog);
	TestEqual(TEXT("Stable projection signature"), Projected.Signature, Repeat.Signature);
	Add(Sparse, 9000000002LL, 101040010, 8, 101);
	const FRokServerInventorySnapshot Changed = URokServerInventorySubsystem::ProjectSnapshot(Sparse, Catalog);
	TestNotEqual(TEXT("Overlay change changes signature"), Projected.Signature, Changed.Signature);
	return true;
}

#endif
