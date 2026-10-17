#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "../RokTrainArmyQuantity.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainArmyQuantityTest,
	"Rok.Ui.TrainArmy.QuantityInput", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainArmyQuantityTest::RunTest(const FString&)
{
	using namespace RokTrainArmyQuantity;
	TestEqual(TEXT("Leading zeros normalize"), FromText(TEXT("0007"), 20), 7);
	TestEqual(TEXT("Empty input returns source minimum"), FromText(TEXT(""), 20), 1);
	TestEqual(TEXT("Nonnumeric is rejected"), FromText(TEXT("7x"), 20), 1);
	TestEqual(TEXT("Decimal is rejected"), FromText(TEXT("1.5"), 20), 1);
	TestEqual(TEXT("Negative clamps"), FromText(TEXT("-2"), 20), 1);
	TestEqual(TEXT("Beyond capacity clamps"), FromText(TEXT("25"), 20), 20);
	TestEqual(TEXT("int.Parse overflow returns minimum"), FromText(TEXT("2147483648"), 20), 1);
	TestEqual(TEXT("Slider floors instead of rounds"), FromSlider(.399f, 20), 7);
	TestEqual(TEXT("Slider zero means minimum"), FromSlider(0, 20), 1);
	TestEqual(TEXT("Near zero means minimum"), FromSlider(.001f, 20), 1);
	TestEqual(TEXT("Single capacity remains one"), FromSlider(.5f, 1), 1);
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTrainArmyQuantityTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize(); const auto Slate = Screen->TakeWidget();
	Screen->OpenTrainArmy(nullptr, 1);
	auto* Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantityInput")));
	auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantitySlider")));
	if (!TestNotNull(TEXT("Actual numeric entry"), Input) || !TestNotNull(TEXT("Actual source slider"), Slider)) return false;
	Input->SetText(FText::FromString(TEXT("0007"))); Input->OnTextChanged.Broadcast(Input->GetText());
	TestEqual(TEXT("Real input normalizes text"), Input->GetText().ToString(), FString(TEXT("7")));
	TestEqual(TEXT("Real input updates selected count"), Screen->GetSelectedTrainCountForAudit(), 7);
	TestTrue(TEXT("Input synchronizes source ratio count/max"), FMath::IsNearlyEqual(Slider->GetValue(), .35f));
	TestTrue(TEXT("Chinese slider is not mirrored"), Slider->GetRenderTransform().Scale.X > 0);
	auto* Cost = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyCostAndTime")));
	if (!TestNotNull(TEXT("Quantity cost text"), Cost)) return false;
	const FString Before = Cost->GetText().ToString();
	Slider->OnValueChanged.Broadcast(.5f);
	TestEqual(TEXT("Slider updates count"), Screen->GetSelectedTrainCountForAudit(), 10);
	TestEqual(TEXT("Slider updates input"), Input->GetText().ToString(), FString(TEXT("10")));
	TestTrue(TEXT("Quantity change refreshes cost preview"), Cost->GetText().ToString() != Before);
	Slider->OnValueChanged.Broadcast(.399f);
	TestEqual(TEXT("Fractional drag floors quantity"), Screen->GetSelectedTrainCountForAudit(), 7);
	TestTrue(TEXT("Fractional drag preserves source thumb position"), FMath::IsNearlyEqual(Slider->GetValue(), .399f));
	Slider->OnValueChanged.Broadcast(0);
	TestEqual(TEXT("Zero drag keeps minimum quantity"), Screen->GetSelectedTrainCountForAudit(), 1);
	TestEqual(TEXT("Zero drag retains source zero thumb"), Slider->GetValue(), 0.f);
	Slider->OnValueChanged.Broadcast(.001f);
	TestTrue(TEXT("Only positive subminimum drag snaps to one/max"), FMath::IsNearlyEqual(Slider->GetValue(), .05f));
	TestEqual(TEXT("Typing does not rebuild input or lose its identity"), Screen->GetWidgetFromName(TEXT("TrainArmyQuantityInput")), static_cast<UWidget*>(Input));
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyTier_1")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	const int32 UnknownCount = Screen->GetSelectedTrainCountForAudit();
	const FName Hidden[] = { TEXT("TrainArmyQuantityInput"), TEXT("TrainArmyQuantitySlider"),
		TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill") };
	for (const FName& Name : Hidden)
	{
		auto* Control = Screen->GetWidgetFromName(Name);
		if (!TestNotNull(TEXT("Unknown-tier quantity subtree"), Control)) return false;
		TestEqual(TEXT("Unknown research hides entire quantity group"), Control->GetVisibility(), ESlateVisibility::Collapsed);
	}
	// A retained old widget delegate cannot modify a now-ineligible selection.
	Input->OnTextChanged.Broadcast(FText::FromString(TEXT("3")));
	TestEqual(TEXT("Unknown research ignores stale quantity event"), Screen->GetSelectedTrainCountForAudit(), UnknownCount);
	Screen->CloseScreen(); Screen->OpenTrainArmy(nullptr, 1);
	Input = Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantityInput")));
	if (!TestNotNull(TEXT("Reopened input"), Input)) return false;
	TestEqual(TEXT("Reopen initializes count and input"), Input->GetText().ToString(), FString(TEXT("1")));
	Screen->CloseScreen(); return true;
}
#endif
