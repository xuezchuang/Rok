#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../RokTrainResourceCost.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainResourceCostTest, "Rok.Ui.TrainArmy.ResourceCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainResourceCostTest::RunTest(const FString&)
{
	TestEqual(TEXT("No unit below threshold"), RokTrainResourceCost::Format(999), FString(TEXT("999")));
	TestEqual(TEXT("Source retains decimal at threshold"), RokTrainResourceCost::Format(1000), FString(TEXT("1.0K")));
	TestEqual(TEXT("Source truncates rather than rounds"), RokTrainResourceCost::Format(1999), FString(TEXT("1.9K")));
	TestEqual(TEXT("Source million unit"), RokTrainResourceCost::Format(1234567), FString(TEXT("1.2M")));
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTrainResourceCostTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize(); const auto Slate = Screen->TakeWidget();
	Screen->OpenTrainArmy(nullptr, 1);
	auto* Input = CastChecked<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantityInput")));
	auto* Food = CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainResourceCostAmount_0")));
	TestEqual(TEXT("T1 infantry exact food"), Food->GetText().ToString(), FString(TEXT("50")));
	TestEqual(TEXT("Zero stone is hidden"), Screen->GetWidgetFromName(TEXT("TrainResourceCostCell_2"))->GetVisibility(), ESlateVisibility::Collapsed);
	Input->SetText(FText::FromString(TEXT("20"))); Input->OnTextChanged.Broadcast(Input->GetText());
	TestEqual(TEXT("Live input refreshes food format"), Food->GetText().ToString(), FString(TEXT("1.0K")));
	TestEqual(TEXT("Live input retains cell identity"), Screen->GetWidgetFromName(TEXT("TrainResourceCostAmount_0")), static_cast<UWidget*>(Food));
	TestTrue(TEXT("Base time explicitly excludes unknown bonuses"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainResourceBaseTime")))->GetText().ToString().Contains(TEXT("00:05:00")));
	const float FoodX = CastChecked<UCanvasPanelSlot>(Screen->GetWidgetFromName(TEXT("TrainResourceCostCell_0"))->Slot)->GetPosition().X;
	const float WoodX = CastChecked<UCanvasPanelSlot>(Screen->GetWidgetFromName(TEXT("TrainResourceCostCell_1"))->Slot)->GetPosition().X;
	TestEqual(TEXT("Chinese source grid order and spacing"), WoodX - FoodX, 137.f);
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailButton")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestNull(TEXT("Details remove parameter cost subtree"), Screen->GetWidgetFromName(TEXT("TrainResourceCostRoot")));
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailBackButton")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Return retains requested count in costs"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainResourceCostAmount_0")))->GetText().ToString(), FString(TEXT("1.0K")));
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyTier_1")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Unknown study hides entire cost root"), Screen->GetWidgetFromName(TEXT("TrainResourceCostRoot"))->GetVisibility(), ESlateVisibility::Collapsed);
	Screen->OpenTrainArmy(nullptr, 1);
	TestEqual(TEXT("Reopen resets shown source cost"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainResourceCostAmount_0")))->GetText().ToString(), FString(TEXT("50")));
	return true;
}
#endif
