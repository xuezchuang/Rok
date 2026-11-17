#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityEconomyCatalogSubsystem.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckStoreRendered final : public IAutomationLatentCommand
{
public:
	explicit FCheckStoreRendered(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/StoreAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Store timed out at step %d. Run the city with -RokOpenModal=store -RokLocalOnly."), Step));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0) return WaitForStore();
		if (!Test.TestNotNull(TEXT("Open store host"), Screen.Get())) return true;
		if (Step == 1) return ExerciseStore();
		if (Step == 2)
		{
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			++Step;
		}
		return !Screen->IsScreenOpen();
	}

private:
	bool WaitForStore()
	{
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			if (!Hud->IsInViewport()) continue;
			auto* Feature = Hud->GetFeatureScreenWidgetForPlacement();
			if (!Feature || !Feature->IsScreenOpen() || Feature->GetWindowAnimationForTesting().IsAnimating()
				|| !RokUiFind::Find(Feature, TEXT("StorePage"))) continue;
			Screen = Feature;
			const auto* Catalog = Hud->GetGameInstance()->GetSubsystem<URokUnityEconomyCatalogSubsystem>();
			if (!Test.TestNotNull(TEXT("Live economy catalog"), Catalog)) return true;
			Resources = Catalog->GetShopItemsByCategory(1);
			Speedups = Catalog->GetShopItemsByCategory(2);
			if (!Test.TestTrue(TEXT("Catalog has multiple resource pages and a short speedup page"),
				Resources.Num() > 24 && Resources.Num() % 12 != 0 && Speedups.Num() > 1 && Speedups.Num() < 12)) return true;
			CheckLayoutClass(TEXT("StoreContentShade"), TEXT("WBP_RokFeature_Store_C"));
			CheckLayoutClass(TEXT("StoreCellHit_0"), TEXT("WBP_RokFeature_StoreCell_C"));
			CheckPage(1, Resources.Num());
			CheckSelection(Resources[0], 1);
			Test.TestNotNull(TEXT("Full page renders the twelfth product"), RokUiFind::Find(Screen, TEXT("StoreCellHit_11")));
			Capture(TEXT("Resources"));
			++Step;
			return false;
		}
		return false;
	}

	bool ExerciseStore()
	{
		const int32 LastPage = FMath::DivideAndRoundUp(Resources.Num(), 12);
		for (int32 Page = 1; Page < LastPage; ++Page)
		{
			if (!Click(TEXT("StoreNextPage"))) return true;
		}
		CheckPage(LastPage, Resources.Num());
		CheckSelection(Resources[(LastPage - 1) * 12], 1);
		Test.TestNull(TEXT("Last resource page omits the unused twelfth slot"), RokUiFind::Find(Screen, TEXT("StoreCellHit_11")));
		if (!Click(TEXT("StoreNextPage"))) return true;
		CheckPage(LastPage, Resources.Num());
		if (!Click(TEXT("StorePreviousPage"))) return true;
		CheckPage(LastPage - 1, Resources.Num());
		CheckSelection(Resources[(LastPage - 2) * 12], 1);

		if (!Click(TEXT("StoreCategory_2")) || !Click(TEXT("StoreCategory_3")) || !Click(TEXT("StoreCategory_1"))) return true;
		CheckPage(1, Speedups.Num());
		CheckSelection(Speedups[0], 1);
		Test.TestNull(TEXT("Short category omits unused product buttons"),
			RokUiFind::Find(Screen, *FString::Printf(TEXT("StoreCellHit_%d"), Speedups.Num())));
		if (!Click(TEXT("StoreQuantityMinus"))) return true;
		CheckSelection(Speedups[0], 1);
		if (!Click(TEXT("StoreCellHit_1")) || !Click(TEXT("StoreQuantityPlus"))) return true;
		CheckSelection(Speedups[1], 2);
		Test.TestNotNull(TEXT("Selected frame follows the item"), RokUiFind::Find(Screen, TEXT("StoreCellSelected_1")));
		Test.TestNull(TEXT("Old selection frame is absent"), RokUiFind::Find(Screen, TEXT("StoreCellSelected_0")));
		if (!Click(TEXT("StoreQuantityMax")) || !Click(TEXT("StoreQuantityPlus"))) return true;
		CheckSelection(Speedups[1], 9999);
		if (!Click(TEXT("StoreQuantityMinus"))) return true;
		CheckSelection(Speedups[1], 9998);
		if (!Click(TEXT("StoreCellHit_1")) || !Click(TEXT("StoreQuantityPlus"))) return true;
		CheckSelection(Speedups[1], 2);

		const auto* Buildings = Screen->GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
		if (!Test.TestNotNull(TEXT("Offline city resource store"), Buildings)) return true;
		const int32 GemsBefore = Buildings->GetCurrentResources().Denar;
		if (!Click(TEXT("StoreBuyButton"))) return true;
		const FString Feedback = Text(TEXT("FeatureBoundaryFeedback"));
		Test.TestTrue(TEXT("Purchase feedback reports the selected item and the offline boundary"),
			Feedback.Contains(FText::AsNumber(Speedups[1].Id).ToString()) && Feedback.Contains(TEXT("× 2"))
			&& Feedback.Contains(TEXT("Shop_BuyShopItem")));
		Test.TestEqual(TEXT("Offline purchase attempt never spends gems"), Buildings->GetCurrentResources().Denar, GemsBefore);
		Capture(TEXT("Speedups"));
		++Step;
		return false;
	}

	void CheckLayoutClass(FName WidgetName, const TCHAR* ClassName)
	{
		const auto* Widget = RokUiFind::Find(Screen, WidgetName);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s resolves to the expected layout"), *WidgetName.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	void CheckPage(int32 Current, int32 NumItems)
	{
		const FString Prefix = FString::Printf(TEXT("%d/%d"), Current, FMath::DivideAndRoundUp(NumItems, 12));
		Test.TestTrue(TEXT("Visible page count follows the catalog"), Text(TEXT("StorePage")).StartsWith(Prefix));
	}

	void CheckSelection(const FRokUnityEconomyItem& Item, int32 Quantity)
	{
		const FString Id = FText::AsNumber(Item.Id).ToString();
		const FString Total = FText::AsNumber(static_cast<int64>(Item.ShopPrice) * Quantity).ToString();
		Test.TestTrue(TEXT("Detail renders the selected catalog item"), Text(TEXT("StoreDetailSource")).Contains(TEXT("Item ") + Id));
		Test.TestEqual(TEXT("Quantity follows the actual controls"), Text(TEXT("StoreQuantity")), FText::AsNumber(Quantity).ToString());
		Test.TestTrue(TEXT("Total updates from the selected catalog price"), Text(TEXT("StoreTotalPrice")).Contains(Total));
	}

	FString Text(FName Name) const
	{
		const auto* Label = Cast<UTextBlock>(RokUiFind::Find(Screen, Name));
		return Label ? Label->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(RokUiFind::Find(Screen, Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->GetIsEnabled() && Button->IsVisible())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok Store audit capture: %s"), *CapturePath);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TArray<FRokUnityEconomyItem> Resources;
	TArray<FRokUnityEconomyItem> Speedups;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokStoreRenderedTest, "Rok.Ui.Store.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokStoreRenderedTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckStoreRendered(*this));
	return true;
}

#endif
