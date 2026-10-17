#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "../RokTrainArmyDetail.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainArmyDetailTest,
	"Rok.Ui.Animation.TrainArmy.DetailNavigation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainArmyDetailTest::RunTest(const FString&)
{
	TestTrue(TEXT("Detail source midpoint"), RokTrainArmyDetail::Evaluate(0.125f, true).Equals(FVector2D(20, 0.5)));
	TestTrue(TEXT("Parameters source midpoint"), RokTrainArmyDetail::Evaluate(0.125f, false).Equals(FVector2D(-20, 0.5)));
	TestTrue(TEXT("Three skills keep source sibling order 3,2,1 after Chinese corner flip"),
		RokTrainArmyDetail::SkillCellX(3, 2) < RokTrainArmyDetail::SkillCellX(3, 1)
		&& RokTrainArmyDetail::SkillCellX(3, 1) < RokTrainArmyDetail::SkillCellX(3, 0));
	TestTrue(TEXT("Hidden third skill leaves order 2,1"), RokTrainArmyDetail::SkillCellX(2, 1) < RokTrainArmyDetail::SkillCellX(2, 0));
	TestEqual(TEXT("Single skill cell centers"), RokTrainArmyDetail::SkillCellX(1, 0), 262.0f);
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTrainArmyDetailTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!TestNotNull(TEXT("Runtime catalog"), Catalog)) return false;
	Catalog->LoadCatalog();
	const auto Troops = Catalog->GetTroopsForArmyType(1);
	const auto* Troop = Troops.FindByPredicate([](const auto& Row) { return Row.Level == 1; });
	if (!TestNotNull(TEXT("Actual infantry T1"), Troop)) return false;
	FRokTrainArmyDetail Detail; FString Error;
	if (!TestTrue(TEXT("Source detail loads by Unity ID"), RokTrainArmyDetail::Load(Troop->Id, Detail, Error)))
	{ AddError(Error); return false; }
	TestEqual(TEXT("Seven source attributes"), Detail.Attributes.Num(), 7);
	FRokTrainArmyDetail Missing;
	TestFalse(TEXT("Unknown ID never invents stats"), RokTrainArmyDetail::Load(-1, Missing, Error));
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize(); Screen->TakeWidget(); Screen->OpenTrainArmy(nullptr, 1);
	auto Click = [this, Screen](const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting(); return true;
	};
	auto* Start = Screen->GetWidgetFromName(TEXT("TrainArmyStart"));
	if (!TestNotNull(TEXT("Actual training action"), Start)) return false;
	TestEqual(TEXT("First open does not play child Show"), Start->GetRenderOpacity(), 1.0f);
	Screen->AdvanceWindowAnimationForTesting(0.3f);
	auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantitySlider")));
	FRokUnityCatalogBuildingType Barracks;
	if (!TestNotNull(TEXT("Source quantity slider"), Slider)
		|| !TestTrue(TEXT("Source barracks capacity"), Catalog->GetBuildingType(9, Barracks))) return false;
	const int32 Capacity = Barracks.GetTrainingCapacity(1);
	if (!TestTrue(TEXT("Capacity supports quantity fixture"), Capacity >= 7)) return false;
	const float SliderValue = 7.0f / Capacity;
	Slider->SetValue(SliderValue); Slider->OnValueChanged.Broadcast(SliderValue);
	TestEqual(TEXT("Real source slider changes quantity"), Screen->GetSelectedTrainCountForAudit(), 7);
	auto* Quantity = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantity")));
	if (!TestNotNull(TEXT("Quantity before details"), Quantity)) return false;
	const FString BeforeQuantity = Quantity->GetText().ToString();
	if (!Click(TEXT("TrainArmyDetailButton"))) return false;
	auto* Panel = Screen->GetWidgetFromName(TEXT("TrainArmyDetailRoot"));
	if (!TestNotNull(TEXT("Actual detail animator child"), Panel)) return false;
	TestEqual(TEXT("Detail starts transparent"), Panel->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Detail starts +40"), FMath::IsNearlyEqual(Panel->GetRenderTransform().Translation.X, 40.0));
	TestNull(TEXT("Training action hidden in details"), Screen->GetWidgetFromName(TEXT("TrainArmyStart")));
	TestFalse(TEXT("Navigation preserves window timeline"), Screen->GetWindowAnimationForTesting().IsAnimating());
	for (int32 Index = 0; Index < 7; ++Index)
	{
		auto* Value = Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("TrainArmyDetailAttribute_%d"), Index)));
		if (!TestNotNull(TEXT("Attribute value"), Value)) return false;
		const auto& Attribute = Detail.Attributes[Index];
		TestEqual(TEXT("Source base value plus explicit unknown modifier"), Value->GetText().ToString(),
			Attribute.Value + (Attribute.bModifierUnknown ? TEXT(" + —") : TEXT("")));
	}
	if (!Click(TEXT("TrainArmyDetailAttrButton_1"))) return false;
	auto* Tip = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailTipText")));
	if (!TestNotNull(TEXT("Attribute explanation opens"), Tip)) return false;
	TestEqual(TEXT("Exact localized attribute tip"), Tip->GetText().ToString(), Detail.Attributes[1].Tip);
	if (!Click(TEXT("TrainArmyDetailTipInside"))) return false;
	TestNotNull(TEXT("Inside click keeps tip"), Screen->GetWidgetFromName(TEXT("TrainArmyDetailTipText")));
	if (!Click(TEXT("TrainArmyDetailTipClickAway"))) return false;
	TestNull(TEXT("Click away destroys tip"), Screen->GetWidgetFromName(TEXT("TrainArmyDetailTipText")));
	for (int32 Index = 0; Index < Detail.Skills.Num(); ++Index)
	{
		if (!Click(*FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), Index))) return false;
		Tip = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailTipText")));
		if (!TestNotNull(TEXT("Skill explanation"), Tip)) return false;
		TestEqual(TEXT("Exact localized readonly skill"), Tip->GetText().ToString(), Detail.Skills[Index].Tip);
		if (!Click(TEXT("TrainArmyDetailTipClickAway"))) return false;
	}
	Screen->AdvanceTrainArmyDetailAnimationForTesting(0.125f);
	TestEqual(TEXT("Detail true child midpoint"), Panel->GetRenderOpacity(), 0.5f);
	if (!Click(TEXT("TrainArmyDetailBackButton"))) return false;
	Start = Screen->GetWidgetFromName(TEXT("TrainArmyStart"));
	if (!TestNotNull(TEXT("Parameter page restored"), Start)) return false;
	auto* Parameters = Screen->GetWidgetFromName(TEXT("TrainParameterRoot"));
	if (!TestNotNull(TEXT("Actual parameter animator ancestor"), Parameters)) return false;
	TestEqual(TEXT("Return begins parameter animation"), Parameters->GetRenderOpacity(), 0.0f);
	TestTrue(TEXT("Return starts -40"), FMath::IsNearlyEqual(Parameters->GetRenderTransform().Translation.X, -40.0));
	TestEqual(TEXT("Button inherits animation without duplicate opacity"), Start->GetRenderOpacity(), 1.0f);
	Quantity = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantity")));
	if (!TestNotNull(TEXT("Restored quantity"), Quantity)) return false;
	TestEqual(TEXT("Read-only details preserve requested quantity"), Quantity->GetText().ToString(), BeforeQuantity);
	Screen->AdvanceTrainArmyDetailAnimationForTesting(0.125f);
	if (!Click(TEXT("TrainArmyDetailButton"))) return false;
	Panel = Screen->GetWidgetFromName(TEXT("TrainArmyDetailRoot"));
	TestEqual(TEXT("Rapid reverse replays target Show"), Panel->GetRenderOpacity(), 0.0f);
	Screen->OpenItems(); Screen->OpenTrainArmy(nullptr, 1);
	TestNull(TEXT("Reopen resets detail page"), Screen->GetWidgetFromName(TEXT("TrainArmyDetailRoot")));
	Start = Screen->GetWidgetFromName(TEXT("TrainArmyStart"));
	if (!TestNotNull(TEXT("Reopened training action"), Start)) return false;
	TestEqual(TEXT("Reopen skips return-only animation"), Start->GetRenderOpacity(), 1.0f);
	Screen->CloseScreen();
	return true;
}
#endif
