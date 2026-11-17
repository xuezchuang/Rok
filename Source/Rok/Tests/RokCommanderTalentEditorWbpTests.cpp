#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Misc/Paths.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderTalentEditorWbpTest,
	"Rok.City.Ui.CommanderTalentEditorWbp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderTalentEditorWbpTest::RunTest(const FString&)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokCommanderTalentEditorWbp"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Fixture world"), World)) return false;
	auto* Heroes = Instance->GetSubsystem<URokServerHeroSubsystem>();
	if (!TestNotNull(TEXT("Hero source"), Heroes)) return false;
	ON_SCOPE_EXIT { Heroes->ClearSnapshotForTesting(); };
	FRokServerHeroSnapshot Snapshot;
	Snapshot.bAvailable = Snapshot.bLoaded = Snapshot.bHeroesKnown = Snapshot.bRowsComplete = true;
	Snapshot.Signature = TEXT("talent-wbp-fixture");
	FRokServerHeroEntry& Hero = Snapshot.Heroes.AddDefaulted_GetRef();
	Hero.HeroId = 2007;
	Hero.bCatalogKnown = Hero.bLevelKnown = Hero.bStarKnown = Hero.bTalentTreesKnown = Hero.bTalentIndexKnown = true;
	Hero.Level = 20;
	Hero.Star = 3;
	Hero.TalentIndex = 1;
	FRokServerHeroTalentTree& Tree = Hero.TalentTrees.AddDefaulted_GetRef();
	Tree.Index = 1;
	Tree.bNameKnown = Tree.bTalentIdsKnown = true;
	Tree.Name = TEXT("方案一");
	Heroes->SetSnapshotForTesting(Snapshot);
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Feature initializes"), Screen->Initialize())) return false;
	Screen->TakeWidget();
	Screen->OpenCommander();
	Screen->SelectCommanderHero(2007);
	auto* Talent = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderTalent")));
	if (!TestNotNull(TEXT("Owned hero talent button"), Talent)) return false;
	Talent->OnClicked.Broadcast();
	Screen->FlushPendingActionForTesting();
	if (!TestNotNull(TEXT("Talent editor"), Screen->GetWidgetFromName(TEXT("CommanderTalentEditor")))) return false;
	UScrollBox* Scroll = Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("CommanderTalentNodeScroll")));
	if (TestNotNull(TEXT("Configured talent nodes"), Scroll))
		TestTrue(TEXT("Talent list has data"), Scroll->GetChildrenCount() > 0);
	if (URokUiSettings::IsLegacyForced())
	{
		FString Error;
		const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUi/Specs/commander_talent_editor.json");
		TestTrue(FString::Printf(TEXT("Export legacy talent editor: %s"), *Error), Screen->ExportCurrentModeSpec(Path, Error));
	}
	auto* Page2 = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderTalentPage_2")));
	if (TestNotNull(TEXT("Second talent page"), Page2))
	{
		Page2->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		TestNotNull(TEXT("Talent list remains available after switching"), Screen->GetWidgetFromName(TEXT("CommanderTalentNodeScroll")));
		auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("CommanderTalentNameInput")));
		auto* Rename = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderTalentRename")));
		if (TestNotNull(TEXT("Talent page name input"), Input) && TestNotNull(TEXT("Rename action"), Rename))
		{
			Input->SetText(FText::FromString(TEXT("LocalPreview")));
			Rename->OnClicked.Broadcast();
			Screen->FlushPendingActionForTesting();
			if (TestNotNull(TEXT("Rename confirmation"), Screen->GetWidgetFromName(TEXT("CommanderConfirmShade")))
				&& URokUiSettings::IsLegacyForced())
			{
				FString Error;
				const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUi/Specs/commander_talent_confirm.json");
				TestTrue(FString::Printf(TEXT("Export legacy talent confirmation: %s"), *Error), Screen->ExportCurrentModeSpec(Path, Error));
			}
			auto* Cancel = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderConfirmCancel")));
			if (TestNotNull(TEXT("Cancel confirmation"), Cancel))
			{
				Cancel->OnClicked.Broadcast();
				Screen->FlushPendingActionForTesting();
				TestNull(TEXT("Confirmation closes without request"), Screen->GetWidgetFromName(TEXT("CommanderConfirmShade")));
			}
		}
	}
	auto* Back = Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderTalentEditorBack")));
	if (TestNotNull(TEXT("Talent editor back"), Back))
	{
		Back->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		TestNull(TEXT("Talent editor closes"), Screen->GetWidgetFromName(TEXT("CommanderTalentEditor")));
	}
	return true;
}
#endif
