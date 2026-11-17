#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokServerInventorySubsystem.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckItemsRendered final : public IAutomationLatentCommand
{
public:
	explicit FCheckItemsRendered(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/ItemsAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	~FCheckItemsRendered()
	{
		if (Inventory.IsValid()) Inventory->ClearSnapshotForTesting();
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Items timed out in step %d. Start the city with -RokLocalOnly."), Step));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0) return OpenFromHud();
		if (!Test.TestTrue(TEXT("Live bag and read-only inventory fixture"), Screen.IsValid() && Inventory.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (Screen->GetWindowAnimationForTesting().IsAnimating()) return false;
			CheckLayoutClass(TEXT("ItemsInventoryEmptyState"), TEXT("WBP_RokFeature_Items_C"));
			Test.TestTrue(TEXT("Empty local account shows the Unity message"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("空空如也")));
			Test.TestNull(TEXT("No grid when empty"), RokUiFind::Find(Screen, TEXT("ItemsInventoryScroll")));
			Capture(TEXT("Empty"));
			break;
		case 2:
			if (!FillAndExerciseGrid()) return true;
			++Step;
			[[fallthrough]];
		case 3:
			if (!AreAtlasesReadyToCapture()) return false;
			Capture(TEXT("Partial"));
			break;
		case 4:
			if (!Click(TEXT("ItemsCategory_4"))) return true;
			Test.TestEqual(TEXT("Other selects the unresolved item by stack ID"), Screen->GetSelectedItemsStackIndexForAudit(), int64(1001));
			Test.TestTrue(TEXT("Unknown item stays distinguishable"), Text(TEXT("ItemsDetailName")).Contains(TEXT("999999")));
			Test.TestNotNull(TEXT("Routed usage text is shown for unresolved items"), RokUiFind::Find(Screen, TEXT("ItemsRoutedUse")));
			Test.TestNull(TEXT("Routed items expose no direct use"), RokUiFind::Find(Screen, TEXT("ItemsUse")));
			Test.TestNull(TEXT("Unknown items expose no batch quantity"), RokUiFind::Find(Screen, TEXT("ItemsQuantity")));
			Capture(TEXT("Routed"));
			break;
		case 5:
			Snapshot.bItemsKnown = false;
			Snapshot.bRowsComplete = false;
			Snapshot.Signature = TEXT("items-layout-waiting");
			Publish();
			Test.TestTrue(TEXT("Unknown account state waits for sync"), Text(TEXT("ItemsInventoryEmptyState")).Contains(TEXT("待服务器同步")));
			Test.TestNull(TEXT("Unknown account state leaves no stale grid"), RokUiFind::Find(Screen, TEXT("ItemsInventoryScroll")));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		default:
			return !Screen->IsScreenOpen();
		}
		++Step;
		return false;
	}

private:
	bool OpenFromHud()
	{
		for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
		{
			if (!Hud->IsInViewport()) continue;
			auto* Button = Cast<UButton>(RokUiFind::Find(*Hud, TEXT("ItemsFeatureButton")));
			if (!Test.TestTrue(TEXT("Real city bag button"), Button && Button->IsVisible() && Button->GetIsEnabled())) return true;
			Screen = Hud->GetFeatureScreenWidgetForPlacement();
			Inventory = Hud->GetGameInstance()->GetSubsystem<URokServerInventorySubsystem>();
			if (!Test.TestTrue(TEXT("Real feature host and inventory subsystem"), Screen.IsValid() && Inventory.IsValid())) return true;
			Button->OnClicked.Broadcast();
			if (!Test.TestTrue(TEXT("HUD click opens the bag"), Screen->IsScreenOpen())) return true;
			++Step;
			return false;
		}
		return false;
	}

	bool FillAndExerciseGrid()
	{
		Snapshot.bAvailable = Snapshot.bLoaded = Snapshot.bItemsKnown = Snapshot.bRowsComplete = true;
		Snapshot.Signature = TEXT("items-layout-partial");
		for (int32 Index = 0; Index < 18; ++Index)
		{
			auto& Stack = Snapshot.Stacks.AddDefaulted_GetRef();
			Stack.ItemIndex = 900 + Index;
			Stack.ItemId = 101050010;
			Stack.Overlay = Index == 0 ? 10000 : (Index == 1 ? 4 : 10 + Index);
			Stack.bCatalogKnown = true;
			Stack.BagType = 1;
		}
		auto& Unknown = Snapshot.Stacks.AddDefaulted_GetRef();
		Unknown.ItemIndex = 1001;
		Unknown.ItemId = 999999;
		Unknown.Overlay = 2;
		Publish();
		CheckLayoutClass(TEXT("ItemsInventoryRow_0"), TEXT("WBP_RokFeature_ItemsInventoryRow_C"));
		CheckLayoutClass(TEXT("ItemsCellHit_0"), TEXT("WBP_RokFeature_ItemsCell_C"));
		auto* Scroll = Cast<UScrollBox>(RokUiFind::Find(Screen, TEXT("ItemsInventoryScroll")));
		if (!Test.TestTrue(TEXT("Four columns per row, with a partial last row"), Scroll && Scroll->GetChildrenCount() == 5
			&& RokUiFind::Find(Screen, TEXT("ItemsCellHit_17")) && !RokUiFind::Find(Screen, TEXT("ItemsCellHit_18")))) return false;
		Test.TestTrue(TEXT("Partial catalog status is displayed"), Text(TEXT("ItemsInventorySyncState")).Contains(TEXT("待同步")));
		if (!Click(TEXT("ItemsCellHit_1"))) return false;
		Test.TestEqual(TEXT("Duplicate product IDs select distinct stacks"), Screen->GetSelectedItemsStackIndexForAudit(), int64(901));
		Test.TestTrue(TEXT("Selected stack shows its own quantity"), Text(TEXT("ItemsOwned")).EndsWith(TEXT("4")));
		if (!Click(TEXT("ItemsMax"))) return false;
		Test.TestEqual(TEXT("Max uses the selected stack count"), Text(TEXT("ItemsQuantity")), FText::AsNumber(4).ToString());
		const auto* Plus = Cast<UButton>(RokUiFind::Find(Screen, TEXT("ItemsPlus")));
		Test.TestTrue(TEXT("Plus is disabled at the stack cap"), Plus && !Plus->GetIsEnabled());
		Scroll = Cast<UScrollBox>(RokUiFind::Find(Screen, TEXT("ItemsInventoryScroll")));
		if (!Test.TestNotNull(TEXT("Scroll exists after selection"), Scroll)) return false;
		Scroll->SetScrollOffset(90.0f);
		const float Offset = Scroll->GetScrollOffset();
		Snapshot.Stacks[0].Overlay = 10001;
		Snapshot.Signature = TEXT("items-layout-changed");
		Publish();
		Scroll = Cast<UScrollBox>(RokUiFind::Find(Screen, TEXT("ItemsInventoryScroll")));
		Test.TestTrue(TEXT("Inventory rebuild preserves scroll position"), Scroll && FMath::IsNearlyEqual(Offset, Scroll->GetScrollOffset()));
		Test.TestEqual(TEXT("Delta refresh preserves stack identity"), Screen->GetSelectedItemsStackIndexForAudit(), int64(901));
		if (!Click(TEXT("ItemsCellHit_0")) || !Click(TEXT("ItemsMax"))) return false;
		Test.TestEqual(TEXT("Batch quantity caps at 9999"), Text(TEXT("ItemsQuantity")), FText::AsNumber(9999).ToString());
		const FString Fingerprint = Screen->GetVisibleItemsInventoryFingerprintForAudit();
		const auto* Buildings = Screen->GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
		if (!Test.TestNotNull(TEXT("Local resource store"), Buildings)) return false;
		const FRokResourceStock Before = Buildings->GetCurrentResources();
		if (!Click(TEXT("ItemsUse"))) return false;
		Test.TestTrue(TEXT("Use reports the current account boundary"), Text(TEXT("FeatureBoundaryFeedback")).Contains(TEXT("未改变")));
		Test.TestEqual(TEXT("Preview use leaves the inventory unchanged"), Screen->GetVisibleItemsInventoryFingerprintForAudit(), Fingerprint);
		const FRokResourceStock After = Buildings->GetCurrentResources();
		Test.TestTrue(TEXT("Preview use leaves balances unchanged"), After.Food == Before.Food && After.Wood == Before.Wood
			&& After.Stone == Before.Stone && After.Gold == Before.Gold && After.Denar == Before.Denar);
		return true;
	}

	bool AreAtlasesReadyToCapture()
	{
		if (LastAtlasFrame == GFrameCounter) return false;
		LastAtlasFrame = GFrameCounter;
		TArray<const UTexture2D*> Textures;
		const bool bFirstSample = !bSampledAtlasState;
		bool bReady = true;
		for (const FName Name : {FName(TEXT("ItemsCellSelect_0")), FName(TEXT("ItemsCellQuality_0")),
			FName(TEXT("ItemsCellIcon_0")), FName(TEXT("ItemsDetailQuality")), FName(TEXT("ItemsDetailIcon"))})
		{
			const auto* Image = Cast<UImage>(RokUiFind::Find(Screen, Name));
			const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
			bReady &= Texture != nullptr;
			if (Texture) Textures.AddUnique(Texture);
			if (bFirstSample)
			{
				Test.TestNotNull(*FString::Printf(TEXT("Live atlas bound to %s"), *Name.ToString()), Texture);
			}
		}
		FString State;
		for (const UTexture2D* Texture : Textures)
		{
			const FTextureResource* Resource = Texture->GetResource();
			const bool bHasRhi = Resource && Resource->TextureRHI.IsValid();
#if WITH_EDITOR
			const bool bCompiling = Texture->IsCompiling();
#else
			constexpr bool bCompiling = false;
#endif
			bReady &= bHasRhi && !bCompiling;
			State += FString::Printf(TEXT(" %s compiling=%d rhi=%d"), *Texture->GetName(), bCompiling, bHasRhi);
		}
		if (bFirstSample || State != LastAtlasState)
		{
			UE_LOG(LogTemp, Display, TEXT("Rok Items audit atlas frame=%llu ready=%d:%s"),
				static_cast<unsigned long long>(GFrameCounter), bReady, *State);
			LastAtlasState = MoveTemp(State);
		}
		bSampledAtlasState = true;
		ReadyAtlasFrames = bReady ? ReadyAtlasFrames + 1 : 0;
		return ReadyAtlasFrames >= 3;
	}

	void Publish()
	{
		Inventory->SetSnapshotForTesting(Snapshot);
		Screen->RefreshServerPlayerAssetsForTesting();
	}

	void CheckLayoutClass(FName WidgetName, const TCHAR* ClassName)
	{
		const auto* Widget = RokUiFind::Find(Screen, WidgetName);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s resolves to the expected layout"), *WidgetName.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
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
		UE_LOG(LogTemp, Display, TEXT("Rok Items audit capture: %s"), *CapturePath);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokServerInventorySubsystem> Inventory;
	FRokServerInventorySnapshot Snapshot;
	FString Output;
	FString CapturePath;
	FString LastAtlasState;
	uint64 LastAtlasFrame = MAX_uint64;
	int32 ReadyAtlasFrames = 0;
	bool bSampledAtlasState = false;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	int32 Step = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokItemsRenderedTest, "Rok.Ui.Items.RenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokItemsRenderedTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckItemsRendered(*this));
	return true;
}

#endif
