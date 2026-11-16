#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "../RokGameNetSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGameNetStartupSettingsTest, "Rok.Net.Startup.UserConfigRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokGameNetStartupSettingsTest::RunTest(const FString&)
{
	const FString Config = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("RokNetStartup"), TEXT(".ini"));
	ON_SCOPE_EXIT { IFileManager::Get().Delete(*Config); };
	const auto Save = [&Config](bool bEnabled)
	{
		return FFileHelper::SaveStringToFile(FString::Printf(
			TEXT("[/Script/Rok.RokGameNetSettings]\nbConnectOnStart=%s\n"), bEnabled ? TEXT("True") : TEXT("False")), *Config);
	};
	TestFalse(TEXT("Fresh preview retains configured offline default"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT(""), false, false, Config));
	if (!TestTrue(TEXT("Write per-project opt-in"), Save(true))) return false;
	TestTrue(TEXT("PIE uses disk opt-in even with a stale false settings CDO"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT(""), false, false, Config));
	TestFalse(TEXT("Explicit offline beats every opt-in"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT("-RokNet -RokNoNet"), false, true, Config));
	TestFalse(TEXT("Local-only runs skip the TCP startup"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT("-RokLocalOnly"), false, true, Config));
	TestFalse(TEXT("Automation never makes a live login"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT("-RokNet"), true, true, Config));

	if (!TestTrue(TEXT("Change opt-in while editor remains open"), Save(false))) return false;
	TestFalse(TEXT("Next Play honors the changed file over the old true CDO"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT(""), false, true, Config));
	TestTrue(TEXT("Explicit live launch still overrides the local default"),
		URokGameNetSubsystem::ShouldAutoConnect(TEXT("-RokNet"), false, false, Config));
	return true;
}

#endif
