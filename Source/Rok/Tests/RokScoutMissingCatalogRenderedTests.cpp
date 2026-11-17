#if WITH_DEV_AUTOMATION_TESTS

#include "../RokBuildingPanelWidget.h"
#include "../RokMainHudWidget.h"
#include "../RokScoutScreenWidget.h"
#include "../RokUiSpec.h"
#include "../RokUiWidgets.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectIterator.h"

namespace
{
class FCheckScoutCatalogWarning final : public IAutomationLatentCommand
{
public:
	explicit FCheckScoutCatalogWarning(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("Actual city HUD did not open for the Scout catalog check"));
			return true;
		}
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			UWorld* World = Hud->GetWorld();
			if (!Hud->IsInViewport() || !World || World->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity")) continue;
			Verify(*Hud, World);
			return true;
		}
		return false;
	}

private:
	void Verify(URokMainHudWidget* Hud, UWorld* World)
	{
		auto* Scout = Cast<URokScoutScreenWidget>(RokUiFind::Find(Hud, TEXT("ScoutScreenWidget")));
		auto* Panel = Cast<URokBuildingPanelWidget>(RokUiFind::Find(Hud, TEXT("BuildingPanelWidget")));
		if (!Test.TestTrue(TEXT("Actual city has Scout and building menu hosts"), Scout && Panel)) return;
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		auto* Camp = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
		if (!Test.TestNotNull(TEXT("Local fixture scout building"), Camp)) return;
		ON_SCOPE_EXIT
		{
			Scout->SetCatalogPathForTesting(FString());
			Scout->CloseImmediately();
			Hud->SetSelectedBuilding(nullptr);
			Camp->Destroy();
		};
		Camp->Tags = {TEXT("UnityBuildingType_28"), TEXT("UnityBuildingLevel_5"), TEXT("RokAutomationFixture_NotRealCityState")};
		Hud->SetSelectedBuilding(Camp);
		Panel->AdvanceMenuAnimation(0.3f);
		auto* Entry = Cast<UButton>(RokUiFind::Find(Panel, TEXT("BuildingFeatureButton")));
		if (!Test.TestTrue(TEXT("Level 5 Scout uses the original enabled building action"), Entry && Entry->GetIsEnabled())) return;
		const FString Missing = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("ScoutMissingCatalog_"), TEXT(".json"));
		if (!Test.TestFalse(TEXT("Catalog fixture points to an absent file"), IFileManager::Get().FileExists(*Missing))) return;
		Scout->SetCatalogPathForTesting(Missing);
		Entry->OnClicked.Broadcast();
		Scout->AdvanceForTesting(0.3f);
		auto* Notice = Cast<UTextBlock>(RokUiFind::Find(Scout, TEXT("ScoutCatalogUnavailable")));
		const auto* Layout = Notice ? Notice->GetTypedOuter<URokUiPageWidget>() : nullptr;
		if (!Test.TestTrue(TEXT("Missing catalog notice is visible after opening from the building action"),
			Scout->IsScreenOpen() && Notice && Notice->IsVisible() && Notice->GetText().ToString() == TEXT("斥候资料未加载"))) return;
		const bool bLegacy = URokUiSettings::IsLegacyForced();
		Test.TestTrue(TEXT("Expected notice presentation"), bLegacy ? !Layout : Layout
			&& Layout->GetClass()->GetName() == TEXT("WBP_RokScoutCatalogUnavailable_C"));
		if (bLegacy && FParse::Param(FCommandLine::Get(), TEXT("RokScoutWarningExport")))
		{
			const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*Directory, true);
			const auto* Content = Cast<UCanvasPanel>(RokUiFind::Find(Scout, TEXT("ScoutListContent")));
			FString Error;
			if (!Test.TestTrue(TEXT("Export the original notice presentation"), Content
				&& RokUiSpec::ExportChildren(Content, Directory / TEXT("scout_catalog_missing_source.json"), TEXT("ScoutCatalogUnavailable"), Error)))
			{
				if (!Error.IsEmpty()) Test.AddError(Error);
				return;
			}
		}
		if (!Click(Scout, TEXT("ScoutInfoButton"))) return;
		Test.TestTrue(TEXT("Info opens the original description view"), Scout->IsDescriptionVisible());
		if (!Click(Scout, TEXT("ScoutBackButton"))) return;
		Test.TestFalse(TEXT("Back returns to the list and retains its notice"), Scout->IsDescriptionVisible());
		Test.TestNotNull(TEXT("Catalog notice persists on the list"), RokUiFind::Find(Scout, TEXT("ScoutCatalogUnavailable")));
		if (!Click(Scout, TEXT("ScoutCloseButton"))) return;
		Scout->AdvanceForTesting(0.3f);
		if (!Test.TestFalse(TEXT("Original close action removes the warning"), Scout->IsScreenOpen())) return;
		Scout->SetCatalogPathForTesting(FString());
		Hud->SetSelectedBuilding(Camp);
		Panel->AdvanceMenuAnimation(0.3f);
		Entry->OnClicked.Broadcast();
		const auto* Configuration = Cast<UTextBlock>(RokUiFind::Find(Scout, TEXT("ScoutCampConfiguration")));
		Test.TestTrue(TEXT("Valid data reopens with the real camp capacity"), Scout->IsScreenOpen() && Configuration
			&& Configuration->GetText().ToString().Contains(TEXT("可容纳 2 名斥候")));
		Test.TestNull(TEXT("Valid data has no missing-catalog notice"), RokUiFind::Find(Scout, TEXT("ScoutCatalogUnavailable")));
	}

	bool Click(URokScoutScreenWidget* Scout, FName Name)
	{
		auto* Button = Cast<UButton>(RokUiFind::Find(Scout, Name));
		if (!Test.TestTrue(FString::Printf(TEXT("Original %s action available"), *Name.ToString()),
			Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		return true;
	}

	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 50.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokScoutCatalogWarningRenderedTest, "Rok.Ui.Scout.CatalogWarningRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokScoutCatalogWarningRenderedTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("Scout catalog check uses the offline offscreen city"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokNoNet")) && FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")))) return false;
	AddExpectedError(TEXT("Rok Scout: Scout catalog missing or invalid"), EAutomationExpectedErrorFlags::Contains, 1);
	ADD_LATENT_AUTOMATION_COMMAND(FCheckScoutCatalogWarning(*this));
	return true;
}

#endif
