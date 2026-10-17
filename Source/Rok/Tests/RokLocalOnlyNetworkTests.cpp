#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "../RokServerSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLocalConstructionLocalOnlyNetworkBoundaryTest,
	"Rok.Runtime.LocalConstruction.LocalOnlyNetworkBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLocalConstructionLocalOnlyNetworkBoundaryTest::RunTest(const FString&)
{
	// This test never constructs a GameInstance in an ordinary network-enabled run.
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))
	{
		AddInfo(TEXT("Requires -RokLocalOnly; no server subsystem was initialized."));
		return true;
	}
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokLocalOnlyNetworkBoundary"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Local-only GameInstance world"), World)) return false;
	auto* Server = Instance->GetSubsystem<URokServerSubsystem>();
	if (!TestNotNull(TEXT("Server subsystem"), Server)) return false;
	TestTrue(TEXT("The local-only switch is recorded during initialization"), Server->IsLocalOnlyRun());
	TestEqual(TEXT("Initialization makes no HTTP dispatch attempt"), Server->GetHttpDispatchAttemptCount(), uint64(0));
	TestEqual(TEXT("Initialization returns before auto health/connect calls"),
		Server->GetSuppressedLocalRequestCount(), uint64(0));
	TestFalse(TEXT("No server connection is accepted"), Server->bConnected);
	TestTrue(TEXT("No session token is created"), Server->SessionToken.IsEmpty());
	TestTrue(TEXT("No failed-network warning is stored"), Server->LastError.IsEmpty());

	Server->PingHealth();
	Server->ConnectAsGuest(TEXT("local-only-fixture"));
	TestEqual(TEXT("Explicit health and guest calls are suppressed"),
		Server->GetSuppressedLocalRequestCount(), uint64(2));
	TestEqual(TEXT("Suppressed calls never reach ProcessRequest"),
		Server->GetHttpDispatchAttemptCount(), uint64(0));
	TestFalse(TEXT("Suppressed guest call cannot mark connected"), Server->bConnected);
	TestTrue(TEXT("Suppressed guest call cannot create a session"), Server->SessionToken.IsEmpty());
	TestTrue(TEXT("Local suppression does not report a network failure"), Server->LastError.IsEmpty());
	return true;
}
#endif
