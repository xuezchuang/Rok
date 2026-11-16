#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokCommanderMeshWidget.h"
#include "../RokUnityConfigSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

namespace
{
class FCheckRenderedCommanderRoster final : public IAutomationLatentCommand
{
public:
	explicit FCheckRenderedCommanderRoster(FAutomationTestBase& InTest)
		: Test(InTest), Deadline(FPlatformTime::Seconds() + 30.0)
	{
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!Screen.IsValid())
		{
			for (TObjectIterator<URokUnityFeatureScreenWidget> It; It; ++It)
			{
				if (It->IsScreenOpen() && It->GetWidgetFromName(TEXT("CommanderRosterScroll")))
				{
					Screen = *It;
					break;
				}
			}
			if (!Screen.IsValid())
			{
				if (Now < Deadline) return false;
				Test.AddError(TEXT("Open the city commander screen with -RokOpenModal=commander."));
				return true;
			}
			return !InitializeRoster();
		}

		if (!Screenshot.IsEmpty())
		{
			if (IFileManager::Get().FileSize(*Screenshot) <= 0 && Now < Deadline) return false;
			Test.TestTrue(Screenshot, IFileManager::Get().FileSize(*Screenshot) > 0);
			Screenshot.Reset();
			++Index;
			bSelected = false;
		}
		if (!Roster.IsValidIndex(Index)) return true;

		const auto& [HeroId, Scene] = Roster[Index];
		const FString Label = FString::Printf(TEXT("Commander %d (%s)"), HeroId, *Scene);
		if (!bSelected)
		{
			Screen->SelectCommanderHero(HeroId);
			const auto* Portrait = Cast<URokCommanderMeshWidget>(Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")));
			const auto* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderName")));
			const auto* Power = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderPower")));
			const auto* Feedback = Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"));
			Test.TestEqual(Label + TEXT(" selects the real roster entry"), Screen->GetSelectedCommanderIdForAudit(), HeroId);
			Test.TestTrue(Label + TEXT(" loads a deforming full-body portrait"), Portrait && Portrait->HasDeformedSamplesForTesting());
			Test.TestTrue(Label + TEXT(" shows name and power"), Name && !Name->GetText().IsEmpty() && Power && !Power->GetText().IsEmpty());
			Test.TestTrue(Label + TEXT(" selection leaves feedback collapsed"), Feedback && !Feedback->IsVisible());
			bSelected = true;
			Deadline = Now + 12.0;
			return false;
		}

		const auto* Background = Cast<UImage>(Screen->GetWidgetFromName(TEXT("CommanderSceneBackground")));
		const auto* Video = Background ? Cast<UMediaTexture>(Background->GetBrush().GetResourceObject()) : nullptr;
		const auto* Player = Video ? Video->GetMediaPlayer() : nullptr;
		const bool bPlayingScene = Player && Player->IsPlaying() && Player->IsLooping()
			&& Player->GetUrl().EndsWith(Scene + TEXT(".mp4")) && Player->GetTime().GetTotalSeconds() >= 0.25;
		const auto* Portrait = Cast<URokCommanderMeshWidget>(Screen->GetWidgetFromName(TEXT("CommanderAnimatedPortrait")));
		const bool bSettled = Portrait && FMath::IsNearlyZero(Portrait->GetSelectionCameraX());
		if ((!bPlayingScene || !bSettled) && Now < Deadline) return false;
		Test.TestTrue(Label + TEXT(" displays the configured looping video"), bPlayingScene);
		Test.TestTrue(Label + TEXT(" settles in the preview camera"), bSettled);
		Screenshot = Output / FString::Printf(TEXT("%d_%s.png"), HeroId, *Scene);
		FScreenshotRequest::RequestScreenshot(Screenshot, true, false);
		Deadline = Now + 10.0;
		return false;
	}

private:
	bool InitializeRoster()
	{
		auto* Instance = Screen->GetGameInstance();
		auto* Catalog = Instance ? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
		auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
		if (!Test.TestNotNull(TEXT("Gameplay catalog"), Catalog)
			|| !Test.TestNotNull(TEXT("Unity Hero configuration"), Config)) return false;
		for (int32 HeroId : Catalog->GetHeroIds())
		{
			FRokUnityCatalogHero Hero;
			if (!Catalog->GetHero(HeroId, Hero) || Hero.ListDisplay == 1) continue;
			FString Scene;
			Test.TestTrue(FString::Printf(TEXT("Hero %d scene configured"), HeroId),
				Config->GetStringFieldByKey(TEXT("Hero"), TEXT("ID"), HeroId, TEXT("heroScene"), Scene));
			Roster.Emplace(HeroId, MoveTemp(Scene));
		}
		if (!Test.TestTrue(TEXT("Roster is populated"), !Roster.IsEmpty())) return false;
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/CommanderRoster")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		Test.AddInfo(FString::Printf(TEXT("Capturing %d commanders in %s"), Roster.Num(), *Output));
		return Test.TestTrue(TEXT("Screenshot directory"), IFileManager::Get().MakeDirectory(*Output, true));
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TArray<TPair<int32, FString>> Roster;
	FString Output;
	FString Screenshot;
	double Deadline;
	int32 Index = 0;
	bool bSelected = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCommanderRenderedRosterTest,
	"Rok.Ui.Animation.Commander.RenderedRoster", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderRenderedRosterTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckRenderedCommanderRoster(*this));
	return true;
}

#endif
