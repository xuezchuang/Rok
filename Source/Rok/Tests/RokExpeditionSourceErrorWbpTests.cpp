#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWidget.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokExpeditionSourceErrorWbpTest,
	"Rok.Ui.Expedition.SourceErrorWbp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokExpeditionSourceErrorWbpTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionSourceErrorFixture")))
	{
		AddWarning(TEXT("Run with -RokLocalOnly -RokExpeditionSourceErrorFixture to exercise the offline source-error entry."));
		return true;
	}

	auto* Instance = NewObject<UGameInstance>(GEngine);
	if (!TestNotNull(TEXT("Standalone expedition fixture game instance"), Instance)) return false;
	Instance->InitializeStandalone(TEXT("RokExpeditionSourceErrorWbpTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); if (GEngine && World) GEngine->DestroyWorldContext(World); };
	if (!TestNotNull(TEXT("Standalone expedition fixture world"), World)) return false;

	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	if (!TestTrue(TEXT("Feature screen initializes"), Screen->Initialize())) return false;
	const TSharedRef<SWidget> ScreenSlate = Screen->TakeWidget();
	Screen->OpenExpedition();
	const FName ErrorTextName = URokUiSettings::IsLegacyForced()
		? FName(TEXT("ExpeditionSourceError")) : FName(TEXT("ExpeditionSourceErrorText"));
	const UTextBlock* Error = Cast<UTextBlock>(RokUiFind::Find(Screen, ErrorTextName));
	if (!TestNotNull(TEXT("Source error state is rendered"), Error)) return false;
	TestTrue(TEXT("Diagnostic state explains the local missing-source case"),
		Error->GetText().ToString().Contains(TEXT("本地 UI 诊断夹具")));
	const UButton* CloseButton = Cast<UButton>(RokUiFind::Find(Screen, TEXT("FeatureFrameClose")));
	TestTrue(TEXT("Source-error state preserves the split-window close action"),
		CloseButton && CloseButton->IsVisible() && CloseButton->GetIsEnabled());
	const URokUiPageWidget* ErrorPage = Error->GetTypedOuter<URokUiPageWidget>();
	if (URokUiSettings::IsLegacyForced())
	{
		TestNull(TEXT("Legacy mode uses its native source-error fallback"), ErrorPage);
	}
	else
	{
		TestNotNull(TEXT("WBP mode mounts the editable source-error page"), ErrorPage);
		TestTrue(TEXT("Source-error widget belongs to the registered WBP"),
			ErrorPage && ErrorPage->GetClass()->GetPathName().Contains(TEXT("WBP_RokFeature_ExpeditionSourceError_C")));
		const auto* PageSlot = ErrorPage ? Cast<UCanvasPanelSlot>(ErrorPage->Slot) : nullptr;
		TestTrue(TEXT("Source-error page paints above the split-frame layer"), PageSlot && PageSlot->GetZOrder() > 10);
	}
	return true;
}

#endif
