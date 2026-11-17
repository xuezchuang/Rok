#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"

namespace
{
class FCheckAllianceShopReference final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceShopReference(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceShopReferenceAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Alliance shop reference timed out in step %d. Last capture gate: %s. Start on the city map with -RokLocalOnly -RokOpenModal=alliance_joined."),
				Step, *CaptureGate));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0)
		{
			for (TObjectIterator<URokMainHudWidget> Candidate; Candidate; ++Candidate)
			{
				if (!Candidate->IsInViewport()) continue;
				Hud = *Candidate;
				Screen = Hud->GetFeatureScreenWidgetForPlacement();
				if (!Find(TEXT("AllianceJoinedFixtureTag")) || !WindowSettled()) return false;
				Catalog = Hud->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
				if (!Test.TestTrue(TEXT("Offline static shop catalog"), Catalog.IsValid() && Catalog->LoadAllianceShopReadonly())) return true;
				Test.TestEqual(TEXT("Configured shop definitions"), Catalog->GetAllianceShopReadonlyRows().Num(), 21);
				Test.TestEqual(TEXT("Configured shop groups"), Catalog->GetAllianceShopReadonlyGroups().Num(), 4);
				if (!Click(TEXT("AllianceFeatureHit_7"))) return true;
				++Step;
				break;
			}
			return false;
		}
		if (!Test.TestTrue(TEXT("Live city alliance shop"), Screen.IsValid() && Catalog.IsValid() && Hud.IsValid())) return true;
		const auto& Definitions = Catalog->GetAllianceShopReadonlyRows();
		switch (Step)
		{
		case 1:
			if (!CaptureState(1, FirstInGroup(1), TEXT("Group1"))) return false;
			CheckLayout(TEXT("AllianceShopReferenceBack"), TEXT("WBP_RokFeature_AllianceShopReference_C"));
			CheckLayout(*FString::Printf(TEXT("AllianceShopSelect_%d"), FirstInGroup(1)), TEXT("WBP_RokFeature_AllianceShopReferenceRow_C"));
			Test.TestTrue(TEXT("Shop is static reference data"), Label(TEXT("AllianceShopReferenceBoundary")).Contains(TEXT("购买状态均需服务器数据")));
			break;
		case 2:
			if (!Test.TestTrue(TEXT("Second row belongs to group one"), Definitions.IsValidIndex(1) && Definitions[1].Group == 1)
				|| !Click(ItemButton(Definitions[1].ItemType))) return true;
			CheckGroup(1, Definitions[1].ItemType);
			if (!Click(TEXT("AllianceShopGroup_2"))) return true;
			break;
		case 3:
			if (!CaptureState(2, FirstInGroup(2), TEXT("Group2"))) return false;
			break;
		case 4:
			if (!Click(TEXT("AllianceShopGroup_3"))) return true;
			break;
		case 5:
			if (!CaptureState(3, FirstInGroup(3), TEXT("Group3"))) return false;
			break;
		case 6:
			if (!Click(TEXT("AllianceShopGroup_4"))) return true;
			break;
		case 7:
			if (!CaptureState(4, FirstInGroup(4), TEXT("Group4"))) return false;
			break;
		case 8:
			if (!Test.TestTrue(TEXT("Last directory row belongs to group four"), !Definitions.IsEmpty() && Definitions.Last().Group == 4)
				|| !Click(ItemButton(Definitions.Last().ItemType))) return true;
			break;
		case 9:
			if (!CaptureState(4, Definitions.Last().ItemType, TEXT("LastItem"))) return false;
			break;
		case 10:
			if (!Click(TEXT("AllianceShopGroup_1"))) return true;
			CheckGroup(1, FirstInGroup(1));
			if (!Click(TEXT("AllianceShopGroup_4"))) return true;
			CheckGroup(4, FirstInGroup(4));
			if (!Click(TEXT("AllianceShopReferenceBack"))) return true;
			Test.TestTrue(TEXT("Back starts the existing close animation with the shop mounted"),
				Find(TEXT("AllianceShopReferenceBack")) && Screen->GetWindowAnimationForTesting().IsAnimating());
			break;
		case 11:
			if (!Find(TEXT("AllianceFeatureHit_7")) || !WindowSettled()) return false;
			Test.TestNull(TEXT("Animated back detaches shop"), Find(TEXT("AllianceShopReferenceBack")));
			if (!Click(TEXT("AllianceFeatureHit_7"))) return true;
			break;
		case 12:
			if (!WindowSettled()) return false;
			CheckGroup(1, FirstInGroup(1));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		case 13:
		{
			if (Screen->IsScreenOpen()) return false;
			auto* Button = Cast<UButton>(RokUiFind::Find(Hud, TEXT("AllianceFeatureButton")));
			if (!Test.TestTrue(TEXT("Live city HUD exposes alliance"), Button && Button->GetIsEnabled())) return true;
			Button->OnClicked.Broadcast();
			Test.TestNotNull(TEXT("Normal alliance opens the unjoined welcome"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Normal welcome does not enter joined shop"), Find(TEXT("AllianceFeatureHit_7")));
			return true;
		}
		}
		++Step;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }
	bool WindowSettled() const { return Screen.IsValid() && !Screen->GetWindowAnimationForTesting().IsAnimating(); }
	static FName ItemButton(int32 ItemType) { return *FString::Printf(TEXT("AllianceShopSelect_%d"), ItemType); }

	FString Label(FName Name) const
	{
		const auto* Widget = Cast<UTextBlock>(Find(Name));
		return Widget ? Widget->GetText().ToString() : FString();
	}

	int32 FirstInGroup(int32 Group) const
	{
		const auto* Row = Catalog->GetAllianceShopReadonlyRows().FindByPredicate([Group](const FRokUnityAllianceShopReadonlyRow& Item)
		{
			return Item.Group == Group;
		});
		return Row ? Row->ItemType : 0;
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void CheckLayout(FName Name, const TCHAR* ClassName)
	{
		const auto* Widget = Find(Name);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s comes from the expected layout"), *Name.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	bool IconMatches(FName Name, const FRokUnityAllianceShopReadonlyRow& Definition) const
	{
		const auto* Image = Cast<UImage>(Find(Name));
		const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
		if (!Texture || !Texture->GetName().EndsWith(Definition.IconAtlas)) return false;
		const FVector2f Size(Definition.AtlasSize.X, Definition.AtlasSize.Y);
		const auto& Rect = Definition.IconRect;
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		return UV.Min.Equals(FVector2f(Rect.Min.X / Size.X, 1.0f - Rect.Max.Y / Size.Y), 1e-4f)
			&& UV.Max.Equals(FVector2f(Rect.Max.X / Size.X, 1.0f - Rect.Min.Y / Size.Y), 1e-4f);
	}

	void CheckGroup(int32 Group, int32 ItemType)
	{
		const auto* Selected = Catalog->FindAllianceShopReadonly(ItemType);
		if (!Test.TestTrue(TEXT("Detail item belongs to the active group"), Selected && Selected->Group == Group)) return;
		int32 RowCount = 0;
		for (const auto& Definition : Catalog->GetAllianceShopReadonlyRows())
		{
			const auto* Button = Cast<UButton>(Find(ItemButton(Definition.ItemType)));
			if (Definition.Group != Group)
			{
				Test.TestNull(TEXT("Other groups do not have active item buttons"), Button);
				continue;
			}
			if (Button && Button->IsVisible() && Button->GetIsEnabled()) ++RowCount;
			const auto* Border = Cast<UBorder>(Find(*FString::Printf(TEXT("AllianceShopRow_%d"), Definition.ItemType)));
			const FLinearColor Selection = ItemType == Definition.ItemType ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f) : FLinearColor(0.19f, 0.18f, 0.16f, 0.98f);
			Test.TestTrue(TEXT("Active row exposes expected selection style"), Border && Border->GetBrushColor().Equals(Selection));
			Test.TestEqual(TEXT("Row name and identity come from its catalog entry"), Label(*FString::Printf(TEXT("AllianceShopName_%d"), Definition.ItemType)),
				FString::Printf(TEXT("%s · ID %d · 品质 %d"), *Definition.Name, Definition.ItemType, Definition.Quality));
			Test.TestTrue(*FString::Printf(TEXT("Row %d icon uses the catalog atlas and UV"), Definition.ItemType),
				IconMatches(*FString::Printf(TEXT("AllianceShopIcon_%d"), Definition.ItemType), Definition));
		}
		Test.TestEqual(TEXT("Group has all selectable definitions"), RowCount, Group == 4 ? 9 : 4);
		Test.TestTrue(*FString::Printf(TEXT("Detail for %d has the selected atlas and UV"), ItemType),
			IconMatches(TEXT("AllianceShopDetailIcon"), *Selected));
		Test.TestNotNull(TEXT("Detail shows the item quality backing"), Find(TEXT("AllianceShopDetailQuality")));
		Test.TestEqual(TEXT("Detail name"), Label(TEXT("AllianceShopDetailName")), Selected->Name);
		Test.TestEqual(TEXT("Detail identity"), Label(TEXT("AllianceShopDetailIdentity")), FString::Printf(TEXT("物品 ID %d · 配置品质 %d"), ItemType, Selected->Quality));
		Test.TestEqual(TEXT("Detail selling configuration is shown"), Label(TEXT("AllianceShopDetailBuyConfigPrice")), FString::Printf(TEXT("个人积分购买配置价：%d"), Selected->SellingPrice));
		Test.TestEqual(TEXT("Detail stock configuration is shown"), Label(TEXT("AllianceShopDetailStockConfigPrice")), FString::Printf(TEXT("联盟积分补货配置价：%d"), Selected->StockPrice));
	}

	bool AreVisibleTexturesReady(const UWidget* Widget, int32& TextureCount)
	{
		if (!Widget || !Widget->IsVisible()) return true;
		if (const auto* Image = Cast<UImage>(Widget))
		{
			const auto* Texture = Cast<UTexture2D>(Image->GetBrush().GetResourceObject());
			if (!Texture) return true;
			++TextureCount;
#if WITH_EDITOR
			if (Texture->IsCompiling())
			{
				CaptureGate = FString::Printf(TEXT("%s is compiling %s"), *Image->GetName(), *Texture->GetPathName());
				return false;
			}
#endif
			const FTextureResource* Resource = Texture->GetResource();
			if (!Resource || !Resource->TextureRHI.IsValid())
			{
				CaptureGate = FString::Printf(TEXT("%s has no ready RHI for %s"), *Image->GetName(), *Texture->GetPathName());
				return false;
			}
		}
		else if (const auto* Nested = Cast<UUserWidget>(Widget))
		{
			return AreVisibleTexturesReady(Nested->GetRootWidget(), TextureCount);
		}
		else if (const auto* Panel = Cast<UPanelWidget>(Widget))
		{
			for (const UWidget* Child : Panel->GetAllChildren())
			{
				if (!AreVisibleTexturesReady(Child, TextureCount)) return false;
			}
		}
		return true;
	}

	bool CaptureState(int32 Group, int32 ItemType, const TCHAR* Name)
	{
		CaptureGate = TEXT("Shop window animation");
		bool bReady = WindowSettled();
		if (bReady)
		{
			CaptureGate = TEXT("No visible shop textures are bound");
			int32 TextureCount = 0;
			bReady = AreVisibleTexturesReady(Find(TEXT("FeatureModeCanvas")), TextureCount) && TextureCount > 0;
		}
		StableFrames = bReady ? StableFrames + 1 : 0;
		if (bReady) CaptureGate = TEXT("Waiting for consecutive ready frames");
		if (StableFrames < 3) return false;
		StableFrames = 0;
		CheckGroup(Group, ItemType);
		if (URokUiSettings::IsLegacyForced() && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceShopExportSpecs")))
		{
			const FString SpecDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*SpecDir, true);
			FString Error;
			if (!Screen->ExportCurrentModeSpec(SpecDir / FString::Printf(TEXT("feature_AllianceShopReference%s.json"), Name), Error))
			{
				Test.AddError(FString::Printf(TEXT("Exporting shop layout %s failed: %s"), Name, *Error));
			}
		}
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok alliance shop audit capture: %s"), *CapturePath);
		return true;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<URokUnityGameplayCatalogSubsystem> Catalog;
	FString Output;
	FString CapturePath;
	FString CaptureGate = TEXT("No capture requested");
	double Deadline = FPlatformTime::Seconds() + 55.0;
	int32 Step = 0;
	int32 StableFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceShopReferenceRenderedTest, "Rok.Ui.Alliance.ShopReferenceRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceShopReferenceRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Shop reference replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceShopReference(*this));
	return true;
}

#endif
