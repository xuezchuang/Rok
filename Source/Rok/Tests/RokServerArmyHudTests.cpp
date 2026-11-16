#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "../RokMainHudWidget.h"
#include "../RokServerArmySubsystem.h"
#include "../RokTroopPortrait.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerArmyHudTest, "Rok.Net.ServerArmy.Hud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokServerArmyHudTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokServerArmyHudTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	URokServerArmySubsystem* Army = Instance->GetSubsystem<URokServerArmySubsystem>();
	if (!TestNotNull(TEXT("Army service"), Army)) return false;
	URokMainHudWidget* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestTrue(TEXT("HUD initializes"), Hud->Initialize())) return false;
	Hud->TakeWidget();
	UCanvasPanel* Entry = Cast<UCanvasPanel>(Hud->GetWidgetFromName(TEXT("ArmyQueueEntry")));
	UTextBlock* Count = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("ArmyQueueEntryState")));
	if (!TestNotNull(TEXT("Queue entry"), Entry) || !TestNotNull(TEXT("Queue count"), Count)) return false;
	FRokServerArmySnapshot Snapshot;
	Snapshot.bAvailable = true; Snapshot.bLoaded = true; Snapshot.bHudQueueKnown = true;
	Snapshot.bDispatchUsedKnown = true; Snapshot.bDispatchCapacityKnown = true;
	Snapshot.DispatchSlotsMax = 1; Snapshot.Signature = TEXT("empty");
	Army->SetSnapshotForTesting(Snapshot); Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Known empty queues hide the entry"), Entry->GetVisibility(), ESlateVisibility::Collapsed);
	Snapshot.ActiveScoutCount = 1; Snapshot.HudQueueRowCount = 1; Snapshot.Signature = TEXT("scout");
	Army->SetSnapshotForTesting(Snapshot); Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Active scout reveals queue entry"), Entry->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Scout does not consume dispatch slot"), Count->GetText().ToString(), FString(TEXT("0/1")));
	Snapshot.DispatchSlotsUsed = 1; Snapshot.HudQueueRowCount = 2; Snapshot.Signature = TEXT("army");
	Army->SetSnapshotForTesting(Snapshot); Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Army consumes server dispatch capacity"), Count->GetText().ToString(), FString(TEXT("1/1")));
	Snapshot.bDispatchCapacityKnown = false; Snapshot.Signature = TEXT("capacity-missing");
	Army->SetSnapshotForTesting(Snapshot); Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Unknown capacity does not use local defaults"), Count->GetText().ToString(), FString(TEXT("1/--")));
	Army->ClearSnapshotForTesting(); Hud->RefreshFromSubsystem();
	TestEqual(TEXT("Unavailable server hides stale entry"), Entry->GetVisibility(), ESlateVisibility::Collapsed);
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Catalog"), Catalog)) return false;
	for (int32 Id : Catalog->GetTroopIds())
	{
		FRokUnityCatalogTroop Troop; FIntRect Rect;
		if (!Catalog->GetTroop(Id, Troop) || !Troop.Icon.StartsWith(TEXT("troops_head["))) continue;
		TestTrue(FString::Printf(TEXT("Source troop portrait %d resolves"), Id), RokTroopPortrait::Resolve(Troop.Icon, Rect));
		TestTrue(TEXT("Portrait stays in imported atlas"), Rect.Min.X >= 0 && Rect.Min.Y >= 0 && Rect.Max.X <= 1024 && Rect.Max.Y <= 1024);
	}
	return true;
}
#endif
