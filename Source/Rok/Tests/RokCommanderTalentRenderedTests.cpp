#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokCommanderMeshWidget.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCommanderTalentRenderedFlow final : public IAutomationLatentCommand
{
public:
	explicit FCommanderTalentRenderedFlow(FAutomationTestBase& InTest)
		: Test(InTest), Deadline(FPlatformTime::Seconds() + 40.0)
	{
		OutputDir = FPaths::ProjectSavedDir() / TEXT("Automation/CommanderTalentWbp")
			/ FGuid::NewGuid().ToString(EGuidFormats::Digits);
		IFileManager::Get().MakeDirectory(*OutputDir, true);
		Test.AddInfo(FString::Printf(TEXT("Commander talent captures: %s"), *OutputDir));
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now > Deadline)
		{
			Test.AddError(TEXT("Timed out waiting for commander talent runtime flow or screenshot"));
			return true;
		}
		if (Now < Resume) return false;
		if (!Screen.IsValid())
		{
			UWorld* World = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr;
			for (TObjectIterator<URokUnityFeatureScreenWidget> It; It; ++It)
			{
				if (It->GetWorld() == World && It->IsScreenOpen() && It->GetWidgetFromName(TEXT("CommanderRosterScroll")))
				{
					Screen = *It;
					break;
				}
			}
			if (!Screen.IsValid()) return false;
			auto* Heroes = Screen->GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>();
			if (!Test.TestNotNull(TEXT("Hero fixture subsystem"), Heroes)) return true;
			FRokServerHeroSnapshot Snapshot;
			Snapshot.bAvailable = Snapshot.bLoaded = Snapshot.bHeroesKnown = Snapshot.bRowsComplete = true;
			Snapshot.Signature = TEXT("talent-render-fixture");
			auto& Hero = Snapshot.Heroes.AddDefaulted_GetRef();
			Hero.HeroId = 2007;
			Hero.bCatalogKnown = Hero.bLevelKnown = Hero.bStarKnown = Hero.bTalentTreesKnown = Hero.bTalentIndexKnown = true;
			Hero.Level = 20;
			Hero.Star = 3;
			Hero.TalentIndex = 1;
			auto& Tree = Hero.TalentTrees.AddDefaulted_GetRef();
			Tree.Index = 1;
			Tree.bNameKnown = Tree.bTalentIdsKnown = true;
			Tree.Name = TEXT("方案一");
			Heroes->SetSnapshotForTesting(Snapshot);
			Screen->OpenCommander();
			Screen->SelectCommanderHero(2007);
			auto* Portrait = Cast<URokCommanderMeshWidget>(Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")));
			if (!Test.TestNotNull(TEXT("Runtime commander model in the authored slot"), Portrait)) return true;
			Test.TestTrue(TEXT("Commander model has source animation data"), Portrait->HasDeformedSamplesForTesting());
			if (!Click(TEXT("CommanderPortraitAction"))) return true;
			if (!Click(TEXT("CommanderTalent"))) return true;
			Resume = Now + 0.8;
			return false;
		}
		if (!PendingScreenshot.IsEmpty())
		{
			if (IFileManager::Get().FileSize(*PendingScreenshot) <= 0) return false;
			PendingScreenshot.Reset();
			++Phase;
		}
		if (Phase == 0)
		{
			if (!Test.TestNotNull(TEXT("Talent editor opened through commander"), Screen->GetWidgetFromName(TEXT("CommanderTalentNodeScroll")))) return true;
			return Capture(TEXT("editor"), Now);
		}
		if (Phase == 1)
		{
			if (!Click(TEXT("CommanderTalentPage_2"))) return true;
			auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("CommanderTalentNameInput")));
			if (!Test.TestNotNull(TEXT("Page name input"), Input)) return true;
			Input->SetText(FText::FromString(TEXT("LocalPreview")));
			if (!Click(TEXT("CommanderTalentRename"))) return true;
			if (!Test.TestNotNull(TEXT("Rename confirmation"), Screen->GetWidgetFromName(TEXT("CommanderConfirmShade")))) return true;
			Phase = 2;
			Resume = Now + 0.5;
			return false;
		}
		if (Phase == 2) return Capture(TEXT("confirm"), Now);
		if (Phase == 3)
		{
			if (!Click(TEXT("CommanderConfirmCancel")) || !Click(TEXT("CommanderTalentEditorBack"))) return true;
			Test.TestNull(TEXT("Talent editor closes"), Screen->GetWidgetFromName(TEXT("CommanderTalentEditor")));
			if (!Click(TEXT("CommanderBack"))) return true;
			Screen->AdvanceWindowAnimationForTesting(1.0f);
			Test.TestFalse(TEXT("Commander back closes the screen"), Screen->IsScreenOpen());
			Screen->GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>()->ClearSnapshotForTesting();
			return true;
		}
		return true;
	}

private:
	bool Click(const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!Test.TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}
	bool Capture(const TCHAR* Name, double Now)
	{
		PendingScreenshot = OutputDir / FString(Name).Append(TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(PendingScreenshot, true, false);
		Resume = Now + 0.2;
		return false;
	}
	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString OutputDir;
	FString PendingScreenshot;
	double Deadline;
	double Resume = 0.0;
	int32 Phase = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderTalentRenderedTest,
	"Rok.Ui.CommanderTalent.RenderedFlow", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderTalentRenderedTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCommanderTalentRenderedFlow(*this));
	return true;
}
#endif
