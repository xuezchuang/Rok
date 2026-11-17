#if WITH_DEV_AUTOMATION_TESTS

#include "RokBarracksSourceTextWidget.h"
#include "RokBarracksUpgradeAttributes.h"
#include "RokBuildingLevelDetails.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUiWidgets.h"
#include "RokUnityModalWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingUpgradePageTest, "Rok.Ui.BuildingUpgrade.Page",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FRokBuildingUpgradePageTest::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokBuildingUpgradePage"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestNotNull(TEXT("City building subsystem"), Buildings)
		|| !TestTrue(TEXT("City HUD initializes"), Hud->Initialize()))
	{
		return false;
	}
	Hud->TakeWidget();
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	auto* Panel = Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget")));
	auto* BuildingUpgradeButton = Panel ? Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton"))) : nullptr;
	if (!TestNotNull(TEXT("Shared city modal"), Modal)
		|| !TestNotNull(TEXT("Building menu upgrade action"), BuildingUpgradeButton))
	{
		return false;
	}

	const bool bLegacy = URokUiSettings::IsLegacyForced();
	const auto Find = [Modal](FName Name) { return RokUiFind::Find(Modal, Name); };
	const auto Text = [&Find](FName Name)
	{
		const auto* Widget = Cast<UTextBlock>(Find(Name));
		return Widget ? Widget->GetText().ToString() : FString();
	};
	const auto OpenUpgrade = [&](AActor* Actor)
	{
		Hud->SetSelectedBuilding(Actor);
		BuildingUpgradeButton->OnClicked.Broadcast();
		return TestTrue(TEXT("Building menu opened upgrade"), Modal->IsModalOpen());
	};
	const auto VerifyPage = [&](FName Expected, FName Widget)
	{
		const UWidget* Control = Find(Widget);
		const auto* Page = Control ? Control->GetTypedOuter<URokUiPageWidget>() : nullptr;
		return TestTrue(TEXT("Mode content comes from the expected layout"),
			bLegacy ? Control && !Page : Page && Page->GetClass()->GetFName() == Expected);
	};

	auto* Farm = World->SpawnActor<AActor>();
	Buildings->ApplyServerBuilding(Farm, 2, 11, FIntPoint::ZeroValue);
	if (!OpenUpgrade(Farm)
		|| !VerifyPage(TEXT("WBP_RokModal_BuildingUpgrade_C"), TEXT("UpgradeCurrentLevel")))
	{
		return false;
	}
	FRokBuildingUpgradePreview FarmPreview;
	if (!TestTrue(TEXT("Farm has runtime upgrade data"), Buildings->GetUpgradePreview(Farm, FarmPreview)))
	{
		return false;
	}
	TestEqual(TEXT("Farm keeps its current level"), Text(TEXT("UpgradeCurrentLevel")), FString(TEXT("11")));
	TestEqual(TEXT("Farm shows production"), Text(TEXT("UpgradeStatCurrent_0")), FText::AsNumber(FarmPreview.CurrentProducePerHour).ToString());
	TestEqual(TEXT("Farm shows production capacity"), Text(TEXT("UpgradeStatCurrent_1")), FText::AsNumber(FarmPreview.CurrentGatherMax).ToString());
	TestEqual(TEXT("Farm shows power"), Text(TEXT("UpgradeStatCurrent_2")), FText::AsNumber(FarmPreview.CurrentPower).ToString());
	const auto VerifyCosts = [&](std::initializer_list<int32> Costs)
	{
		int32 Index = 0;
		for (const int32 Cost : Costs)
		{
			const auto* Icon = Cast<UImage>(Find(*FString::Printf(TEXT("UpgradeCostIcon_%d"), Index)));
			TestTrue(TEXT("Resource icon is enabled for painting"),
				Icon && Icon->GetVisibility() == ESlateVisibility::HitTestInvisible && Icon->GetBrush().GetResourceObject());
			TestEqual(TEXT("Visible resource amount matches actor preview"), Text(*FString::Printf(TEXT("UpgradeCostText_%d"), Index)),
				FText::AsNumber(Cost).ToString());
			++Index;
		}
		const UWidget* UnusedCost = Find(*FString::Printf(TEXT("UpgradeCostIcon_%d"), Index));
		TestTrue(TEXT("Unused cost slot is not drawn"), !UnusedCost || UnusedCost->GetVisibility() == ESlateVisibility::Collapsed);
	};
	TestTrue(TEXT("Farm uses two packed slots for wood and stone"),
		FarmPreview.Cost.Food == 0 && FarmPreview.Cost.Wood > 0 && FarmPreview.Cost.Stone > 0 && FarmPreview.Cost.Gold == 0);
	VerifyCosts({FarmPreview.Cost.Wood, FarmPreview.Cost.Stone});
	Modal->CloseModal();

	auto* Hall = World->SpawnActor<AActor>();
	Buildings->ApplyServerBuilding(Hall, 1, 11, FIntPoint::ZeroValue);
	if (!OpenUpgrade(Hall))
	{
		return false;
	}
	FRokBuildingUpgradePreview HallPreview;
	TestTrue(TEXT("City hall uses three packed slots for food, wood and stone"),
		Buildings->GetUpgradePreview(Hall, HallPreview) && HallPreview.Cost.Food > 0
		&& HallPreview.Cost.Wood > 0 && HallPreview.Cost.Stone > 0 && HallPreview.Cost.Gold == 0);
	VerifyCosts({HallPreview.Cost.Food, HallPreview.Cost.Wood, HallPreview.Cost.Stone});
	Modal->CloseModal();

	TArray<FString> Headers;
	TArray<FRokBarracksLevelDetailsRow> Rows;
	FString Error;
	if (!TestTrue(TEXT("Barracks level table loaded"), RokBuildingLevelDetails::LoadBarracks(Headers, Rows, Error)))
	{
		return false;
	}
	auto* Barracks = World->SpawnActor<AActor>();
	for (const int32 Level : {1, 2})
	{
		Buildings->ApplyServerBuilding(Barracks, 9, Level, FIntPoint::ZeroValue);
		if (!OpenUpgrade(Barracks)
			|| !VerifyPage(Level == 1 ? TEXT("WBP_RokModal_BarracksUpgradeSource_C") : TEXT("WBP_RokModal_BarracksUpgrade_C"),
				TEXT("BarracksLevelInfoButton")))
		{
			return false;
		}
		if (Level == 1)
		{
			const auto* Current = Cast<URokBarracksSourceTextWidget>(Find(TEXT("UpgradeCurrentLevel")));
			const auto* Attributes = Cast<URokBarracksUpgradeAttributes>(Find(TEXT("BarracksUpgradeAttributes")));
			TestTrue(TEXT("Initial barracks has source level glyph"), Current && Current->HasSourceMesh());
			TestTrue(TEXT("Initial barracks has source attributes"), Attributes && Attributes->GetAttributeCount() == 2);
		}
		else
		{
			TestEqual(TEXT("Later barracks shows current level"), Text(TEXT("UpgradeCurrentLevel")), FString(TEXT("2")));
			TestEqual(TEXT("Later barracks shows next level"), Text(TEXT("UpgradeNextLevel")), FString(TEXT("3")));
			TestNull(TEXT("Later barracks omits level 1 attributes"), Find(TEXT("BarracksUpgradeAttributes")));
		}

		auto* Info = Cast<UButton>(Find(TEXT("BarracksLevelInfoButton")));
		auto* Back = Cast<UButton>(Find(TEXT("BarracksLevelBackButton")));
		auto* Scroll = Cast<UScrollBox>(Find(TEXT("BarracksLevelDetailsScroll")));
		if (!TestNotNull(TEXT("Open level page"), Info)
			|| !TestNotNull(TEXT("Return from level page"), Back)
			|| !TestNotNull(TEXT("Level page scroll container"), Scroll))
		{
			return false;
		}
		Info->OnClicked.Broadcast();
		TestTrue(TEXT("Info opens level details"), Modal->IsBarracksLevelPageOpenForAudit());
		TestEqual(TEXT("New page starts at the first row"), Scroll->GetScrollOffset(), 0.0f);
		TestEqual(TEXT("Current level is the actor's actual level"), Modal->GetBarracksLevelSelectedLevelForAudit(), Level);
		for (int32 Column = 0; Column < Headers.Num(); ++Column)
		{
			const auto* Header = Cast<URokBarracksSourceTextWidget>(Find(*FString::Printf(TEXT("BarracksLevelHeader_%d"), Column)));
			TestTrue(TEXT("Barracks column preserves source glyph and text"),
				Header && Header->HasSourceMesh() && Header->GetText().ToString() == Headers[Column]);
		}
		for (const FRokBarracksLevelDetailsRow& Row : Rows)
		{
			const FString Values[] = {FText::AsNumber(Row.Level).ToString(), FText::AsNumber(Row.InfantryTrainNumber).ToString(),
				Row.InfantryAttackText, FText::AsNumber(Row.Power).ToString()};
			for (int32 Column = 0; Column < UE_ARRAY_COUNT(Values); ++Column)
			{
				const FName Name(*FString::Printf(TEXT("BarracksLevelCell_%d_%d"), Row.Level, Column));
				TestNotNull(TEXT("Barracks level cell is mounted"), Find(Name));
				TestEqual(TEXT("Barracks level cell uses source data"), Text(Name), Values[Column]);
			}
		}
		VerifyPage(TEXT("WBP_RokModalItem_BarracksLevelRow_C"), TEXT("BarracksLevelCell_25_3"));
		const UWidget* Highlight = Find(TEXT("BarracksLevelCurrentRow"));
		const UWidget* Grid = Find(TEXT("BarracksLevelRows"));
		TestTrue(TEXT("Highlight is positioned on the actor's level"), Highlight && Grid &&
			RokUiFind::CanvasPosition(Highlight, Grid).Equals(FVector2D(30, (Level - 1) * 50.0f - 1.5f), 0.02));
		Scroll->TakeWidget();
		Scroll->SetScrollOffset(420.f);
		Back->OnClicked.Broadcast();
		TestFalse(TEXT("Back restores the upgrade page"), Modal->IsBarracksLevelPageOpenForAudit());
		Info->OnClicked.Broadcast();
		TestTrue(TEXT("Returning to the level page keeps the list and scroll position"),
			Find(TEXT("BarracksLevelDetailsScroll")) == Scroll && FMath::IsNearlyEqual(Scroll->GetScrollOffset(), 420.f));
		Modal->CloseModal();
		TestNull(TEXT("Closing the modal detaches its detail controls"), Find(TEXT("BarracksLevelDetailsScroll")));
	}
	if (!bLegacy)
	{
		auto& Pages = GetMutableDefault<URokUiSettings>()->ModalPages;
		FRokUiPageEntry* RowEntry = Pages.FindByPredicate([](const FRokUiPageEntry& Entry)
		{
			return Entry.Page == TEXT("BarracksLevelRow");
		});
		if (!TestNotNull(TEXT("Barracks level row is configured"), RowEntry))
		{
			return false;
		}
		const TGuardValue<TSoftClassPtr<URokUiPageWidget>> MissingRowClass(RowEntry->Class, TSoftClassPtr<URokUiPageWidget>());
		AddExpectedError(TEXT("Rok UI: page BarracksLevelRow has no Widget Blueprint in URokUiSettings"),
			EAutomationExpectedErrorFlags::Contains, 1);
		if (!OpenUpgrade(Barracks))
		{
			return false;
		}
		auto* Info = Cast<UButton>(Find(TEXT("BarracksLevelInfoButton")));
		if (!TestTrue(TEXT("Missing row class opens the complete C++ barracks page"),
			Info && !Info->GetTypedOuter<URokUiPageWidget>()))
		{
			return false;
		}
		Info->OnClicked.Broadcast();
		TestTrue(TEXT("Fallback level details open"), Modal->IsBarracksLevelPageOpenForAudit());
		TestEqual(TEXT("Fallback includes first level"), Text(TEXT("BarracksLevelCell_1_0")), FString(TEXT("1")));
		TestEqual(TEXT("Fallback includes last level"), Text(TEXT("BarracksLevelCell_25_0")), FString(TEXT("25")));
		Modal->CloseModal();
	}
	return true;
}

#endif
