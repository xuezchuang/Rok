#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokTrainArmyDetail.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainArmyBrowseTest,
	"Rok.Ui.TrainArmy.TierBrowse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainArmyBrowseTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTrainArmyBrowseTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Catalog"), Catalog) || !TestNotNull(TEXT("Runtime state"), Runtime)
		|| !TestNotNull(TEXT("Resource boundary"), Buildings)) return false;
	Catalog->LoadCatalog(); Runtime->LoadRuntimeState();
	const auto BeforeStacks = Runtime->GetStarterTroopStacks();
	const auto BeforeTrainable = Runtime->GetTrainableTroopIds();
	const auto BeforeResources = Buildings->GetCurrentResources();
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize(); const auto ScreenSlate = Screen->TakeWidget();
	auto Click = [this, Screen](const TCHAR* Name)
	{
		auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
		if (!TestNotNull(Name, Button)) return false;
		Button->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting(); return true;
	};
	for (int32 Type = 1; Type <= 4; ++Type)
	{
		Screen->OpenTrainArmy(nullptr, Type); Screen->AdvanceWindowAnimationForTesting(0.3f);
		const auto Troops = Catalog->GetTroopsForArmyType(Type);
		for (int32 Tier = 1; Tier <= 5; ++Tier)
		{
			const auto* Troop = Troops.FindByPredicate([Tier](const auto& Row) { return Row.Level == Tier; });
			if (!TestNotNull(TEXT("Source standard tier"), Troop)) return false;
			FRokTrainArmyDetail Detail; FString Error;
			if (!TestTrue(TEXT("Source detail identity"), RokTrainArmyDetail::Load(Troop->Id, Detail, Error))) return false;
			if (!Click(*FString::Printf(TEXT("TrainArmyTier_%d"), Tier - 1))) return false;
			TestEqual(TEXT("Tier click selects regardless of unknown study"), Screen->GetSelectedTrainTierForAudit(), Tier);
			TestEqual(TEXT("Army type remains selected"), Screen->GetSelectedTrainArmyTypeForAudit(), Type);
			auto* Name = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmySelectedName")));
			if (!TestNotNull(TEXT("Selected source name"), Name)) return false;
			TestEqual(TEXT("Selection name follows exact Arms ID"), Name->GetText().ToString(), Detail.Name);
			const FName Controls[] = { TEXT("TrainArmyStart"), TEXT("TrainArmyQuantity"), TEXT("TrainArmyQuantityInput"),
				TEXT("TrainArmyQuantitySlider"), TEXT("TrainArmyQuantityRegion"), TEXT("TrainResourceCostRoot"),
				TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill"),
				TEXT("TrainArmyParameterQuantityBand"), TEXT("TrainArmyParameterCostBackground") };
			for (const FName& Control : Controls)
			{
				auto* Widget = Screen->GetWidgetFromName(Control);
				if (!TestNotNull(TEXT("Parameter/action control"), Widget)) return false;
				TestEqual(FString::Printf(TEXT("Study requirement controls source availability: %s"), *Control.ToString()),
					Widget->GetVisibility() == ESlateVisibility::Collapsed, Troop->StudyId != 0);
			}
			for (const FName& Control : {FName(TEXT("TrainArmyPlus")), FName(TEXT("TrainArmyMinus")),
				FName(TEXT("TrainArmyMax")), FName(TEXT("TrainArmyQuantityShade"))})
				TestEqual(FString::Printf(TEXT("Non-source control stays collapsed: %s"), *Control.ToString()),
					Screen->GetWidgetFromName(Control)->GetVisibility(), ESlateVisibility::Collapsed);
			if (Troop->StudyId != 0)
			{
				auto* Status = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyStudyStatus")));
				if (!TestNotNull(TEXT("Unknown study is explicit"), Status)) return false;
				TestTrue(TEXT("No claim of owned/unlocked research"), Status->GetText().ToString().Contains(TEXT("待同步")));
			}
			const int32 Count = Screen->GetSelectedTrainCountForAudit();
			if (!Click(TEXT("TrainArmyDetailButton"))) return false;
			Name = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailName")));
			if (!TestNotNull(TEXT("Selected tier detail"), Name)) return false;
			TestEqual(TEXT("Detail exact source name"), Name->GetText().ToString(), Detail.Name);
			auto* Description = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailDescription")));
			if (!TestNotNull(TEXT("Source description"), Description)) return false;
			const auto DescriptionSlate = Description->TakeWidget(); Description->ForceLayoutPrepass();
			TestEqual(TEXT("20 Unity pixels are 15 Slate points"), Description->GetFont().Size, 15.0f);
			TestEqual(TEXT("Description retains complete source text"), Description->GetText().ToString(), Detail.Description);
			TestTrue(TEXT("Source description fits 74.1 high without clipping source text"),
				Description->GetDesiredSize().Y > 0 && Description->GetDesiredSize().Y <= 74.1f);
			for (int32 Index = 0; Index < Detail.Skills.Num(); ++Index)
			{
				if (!Click(*FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), Index))) return false;
				auto* Tip = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailTipText")));
				if (!TestNotNull(TEXT("Source skill tip"), Tip)) return false;
				TestEqual(TEXT("Tip text follows selected tier skill ID"), Tip->GetText().ToString(), Detail.Skills[Index].Tip);
				TestEqual(TEXT("Source tooltip 18 pixels"), Tip->GetFont().Size, 13.5f);
				auto* TipSlot = Cast<UCanvasPanelSlot>(Tip->Slot);
				if (!TestNotNull(TEXT("Tooltip slot"), TipSlot)) return false;
				TestEqual(TEXT("Source SetWidth is text width"), TipSlot->GetSize().X, 500.0);
				if (!Click(TEXT("TrainArmyDetailTipClickAway"))) return false;
			}
			if (!Click(TEXT("TrainArmyDetailBackButton"))) return false;
			TestEqual(TEXT("Returning preserves tier"), Screen->GetSelectedTrainTierForAudit(), Tier);
			TestEqual(TEXT("Returning preserves requested quantity"), Screen->GetSelectedTrainCountForAudit(), Count);
		}
		Screen->CloseScreen(); Screen->OpenTrainArmy(nullptr, Type);
		TestEqual(TEXT("Reopen resets tier one"), Screen->GetSelectedTrainTierForAudit(), 1);
		TestEqual(TEXT("Reopen resets quantity one"), Screen->GetSelectedTrainCountForAudit(), 1);
	}
	const auto AfterStacks = Runtime->GetStarterTroopStacks();
	TestEqual(TEXT("Browsing never adds troop stacks"), AfterStacks.Num(), BeforeStacks.Num());
	for (int32 Index = 0; Index < FMath::Min(AfterStacks.Num(), BeforeStacks.Num()); ++Index)
	{
		TestEqual(TEXT("Owned ID unchanged"), AfterStacks[Index].TroopId, BeforeStacks[Index].TroopId);
		TestEqual(TEXT("Owned count unchanged"), AfterStacks[Index].Count, BeforeStacks[Index].Count);
	}
	TestTrue(TEXT("Browsing never grants trainable IDs"), Runtime->GetTrainableTroopIds() == BeforeTrainable);
	const auto AfterResources = Buildings->GetCurrentResources();
	TestEqual(TEXT("No food spend"), AfterResources.Food, BeforeResources.Food);
	TestEqual(TEXT("No wood spend"), AfterResources.Wood, BeforeResources.Wood);
	TestEqual(TEXT("No stone spend"), AfterResources.Stone, BeforeResources.Stone);
	TestEqual(TEXT("No gold spend"), AfterResources.Gold, BeforeResources.Gold);
	Screen->CloseScreen(); return true;
}
#endif
