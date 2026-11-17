#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokAllianceRankingReadonlyCatalog.h"
#include "../RokAllianceTerritoryReadonlyData.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckAllianceTerritoryRanking final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceTerritoryRanking(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceTerritoryRankingAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Alliance territory/ranking timed out in step %d (%s). Use -RokLocalOnly -RokOpenModal=alliance_joined."),
				Step, *CaptureGate));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0)
		{
			for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
			{
				if (!Hud->IsInViewport()) continue;
				Screen = Hud->GetFeatureScreenWidgetForPlacement();
				if (!Find(TEXT("AllianceJoinedFixtureTag")) || !WindowSettled()) return false;
				Test.TestEqual(TEXT("The territory fixture contains four groups"), RokAllianceTerritoryReadonly::GetGroups().Num(), 4);
				Test.TestEqual(TEXT("The territory fixture contains twelve definitions"), RokAllianceTerritoryReadonly::GetRows().Num(), 12);
				if (!Click(TEXT("AllianceFeatureHit_1"))) return true;
				++Step;
				break;
			}
			return false;
		}
		if (!Test.TestTrue(TEXT("Live city alliance reference"), Screen.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (!CaptureState(TEXT("AllianceTerritoryCollapsed"))) return false;
			CheckLayout(TEXT("AllianceTerritoryReferenceBack"), TEXT("WBP_RokFeature_AllianceTerritoryReference_C"));
			CheckTerritoryOverview(INDEX_NONE);
			break;
		case 2:
			if (!Click(GroupHit(0))) return true;
			break;
		case 3:
			if (!CaptureState(TEXT("AllianceTerritoryGroup0"))) return false;
			CheckTerritoryOverview(0);
			break;
		case 4:
			if (!Click(TerritoryHit(1))) return true;
			break;
		case 5:
			if (!CaptureState(TEXT("AllianceTerritoryBuildingDetail"))) return false;
			CheckTerritoryDetail(1);
			break;
		case 6:
			if (!Click(TEXT("AllianceTerritoryReturnToDefinitions"))) return true;
			CheckTerritoryOverview(0);
			if (!Click(GroupHit(0))) return true;
			CheckTerritoryOverview(INDEX_NONE);
			break;
		case 7:
			if (!Click(GroupHit(TerritoryGroup))) return true;
			break;
		case 8:
			if (!CaptureState(*FString::Printf(TEXT("AllianceTerritoryGroup%d"), TerritoryGroup))) return false;
			CheckTerritoryOverview(TerritoryGroup);
			break;
		case 9:
			if (TerritoryGroup < 3)
			{
				if (!Click(GroupHit(TerritoryGroup))) return true;
				++TerritoryGroup;
				Step = 7;
				return false;
			}
			if (!ScrollTerritoryIntoView(7)) return true;
			break;
		case 10:
			if (!IsTerritoryHitInView(7)) return false;
			if (!Click(TerritoryHit(7))) return true;
			break;
		case 11:
			if (!CaptureState(TEXT("AllianceTerritoryResourceDetail"))) return false;
			CheckTerritoryDetail(7);
			break;
		case 12:
			if (!Click(TEXT("AllianceTerritoryReturnToDefinitions"))) return true;
			CheckTerritoryOverview(3);
			if (!Click(TEXT("AllianceTerritoryReferenceBack"))) return true;
			break;
		case 13:
			if (!WindowSettled()) return false;
			Test.TestNull(TEXT("Territory back detaches the reference"), Find(TEXT("AllianceTerritoryReferenceBack")));
			if (!Click(TEXT("AlliancePageHit_2"))) return true;
			{
				FString Error;
				if (!Test.TestTrue(TEXT("Six local ranking categories load"),
					RokAllianceRankingReadonlyCatalog::Load(Categories, Error) && Categories.Num() == 6)) return true;
			}
			if (!Click(TEXT("AllianceSettingHit_2"))) return true;
			break;
		case 14:
			if (!CaptureState(TEXT("AllianceRankingOverview"), true)) return false;
			CheckRankingOverview();
			CheckLayout(TEXT("AllianceRankingBack"), TEXT("WBP_RokFeature_AllianceRankingReference_C"));
			CheckLayout(RankingHit(300), TEXT("WBP_RokFeature_AllianceRankingReferenceCard_C"));
			break;
		case 15:
			if (!Click(RankingHit(Categories[RankingIndex].Id))) return true;
			break;
		case 16:
			if (RankingIndex == 0 && !CaptureState(TEXT("AllianceRankingDetail"))) return false;
			if (RankingIndex == Categories.Num() - 1 && !CaptureState(TEXT("AllianceRankingLastDetail"))) return false;
			CheckRankingDetail(Categories[RankingIndex]);
			break;
		case 17:
			if (!Click(RankingIndex == 0 ? FName(TEXT("FeatureFrameClose")) : FName(TEXT("AllianceRankingBack")))) return true;
			Test.TestNotNull(TEXT("Detail close returns to ranking categories"), Find(RankingHit(300)));
			if (++RankingIndex < Categories.Num())
			{
				Step = 15;
				return false;
			}
			break;
		case 18:
			if (!Click(TEXT("AllianceRankingBack"))) return true;
			Test.TestNotNull(TEXT("Ranking overview back returns to alliance settings"), Find(TEXT("AllianceSettingHit_2")));
			Test.TestNull(TEXT("Ranking reference is detached"), Find(TEXT("AllianceRankingBack")));
			return true;
		}
		++Step;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }
	bool WindowSettled() const { return Screen.IsValid() && !Screen->GetWindowAnimationForTesting().IsAnimating(); }
	static FName GroupHit(int32 Index) { return *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Index); }
	static FName TerritoryHit(int32 Type) { return *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type); }
	static FName RankingHit(int32 Id) { return *FString::Printf(TEXT("AllianceRankingCategoryHit_%d"), Id); }
	UScrollBox* TerritoryScroll() const { return Cast<UScrollBox>(Find(TEXT("AllianceTerritoryReferenceScroll"))); }

	FString Label(FName Name) const
	{
		const auto* Widget = Cast<UTextBlock>(Find(Name));
		return Widget ? Widget->GetText().ToString() : FString();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	bool ScrollTerritoryIntoView(int32 Type)
	{
		auto* Scroll = TerritoryScroll();
		auto* Hit = Find(TerritoryHit(Type));
		if (!Test.TestTrue(TEXT("Expanded territory exposes a live scroll viewport and resource button"), Scroll && Hit)) return false;
		Scroll->ScrollWidgetIntoView(Hit, false, EDescendantScrollDestination::Center);
		return true;
	}

	bool IsTerritoryHitInView(int32 Type)
	{
		CaptureGate = FString::Printf(TEXT("Scroll to territory type %d"), Type);
		const auto* Scroll = TerritoryScroll();
		const auto* Hit = Find(TerritoryHit(Type));
		if (!Scroll || !Hit || Scroll->GetScrollOffset() <= 0) return false;
		const FGeometry& HitGeometry = Hit->GetCachedGeometry();
		return HitGeometry.GetLocalSize().Y > 1.f
			&& Scroll->GetCachedGeometry().IsUnderLocation(HitGeometry.LocalToAbsolute(HitGeometry.GetLocalSize() * .5));
	}

	void CheckLayout(FName Name, const TCHAR* ClassName)
	{
		const auto* Widget = Find(Name);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s comes from the expected layout"), *Name.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	void CheckTerritoryOverview(int32 Expanded)
	{
		Test.TestNotNull(TEXT("Territory overview has the scroll viewport"), TerritoryScroll());
		Test.TestNull(TEXT("Definition detail is detached in the overview"), Find(TEXT("AllianceTerritoryDefinitionDetailScroll")));
		Test.TestTrue(TEXT("Territory UI describes the offline ownership boundary"), Label(TEXT("AllianceTerritoryReferenceBoundary")).Contains(TEXT("拥有数量")));
		for (const auto& Group : RokAllianceTerritoryReadonly::GetGroups())
		{
			Test.TestNotNull(TEXT("Group keeps its action"), Find(GroupHit(Group.Index)));
			Test.TestEqual(TEXT("Group shows pinned source title"), Label(*FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Group.Index)),
				RokAllianceTerritoryReadonly::GetChineseSourceText(Group.TitleLocalizationId));
			Test.TestEqual(TEXT("Group expansion state"), Label(*FString::Printf(TEXT("AllianceTerritoryExpand_%d"), Group.Index)),
				Group.Index == Expanded ? FString(TEXT("−")) : FString(TEXT("+")));
		}
		for (const auto& Definition : RokAllianceTerritoryReadonly::GetRows())
		{
			const auto* Hit = Cast<UButton>(Find(TerritoryHit(Definition.UnityType)));
			if (Definition.GroupIndex != Expanded)
			{
				Test.TestNull(TEXT("Collapsed groups do not expose definition actions"), Hit);
				continue;
			}
			Test.TestTrue(TEXT("Expanded definition has its active real button"), Hit && Hit->GetIsEnabled());
			const TCHAR* NamePrefix = Expanded == 3 ? TEXT("AllianceTerritoryResourceName") : TEXT("AllianceTerritoryName");
			Test.TestEqual(TEXT("Definition shows pinned source name"), Label(*FString::Printf(TEXT("%s_%d"), NamePrefix, Definition.UnityType)),
				RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.NameLocalizationId));
		}
	}

	void CheckTerritoryDetail(int32 Type)
	{
		const auto* Definition = RokAllianceTerritoryReadonly::FindRow(Type);
		if (!Test.TestNotNull(TEXT("Static territory definition exists"), Definition)) return;
		Test.TestNull(TEXT("Overview is detached in detail"), TerritoryScroll());
		Test.TestNotNull(TEXT("Definition retains its detail scroll"), Find(TEXT("AllianceTerritoryDefinitionDetailScroll")));
		Test.TestEqual(TEXT("Definition name"), Label(TEXT("AllianceTerritoryDefinitionName")),
			RokAllianceTerritoryReadonly::GetChineseSourceText(Definition->NameLocalizationId));
		Test.TestEqual(TEXT("Definition description"), Label(TEXT("AllianceTerritoryDefinitionDescription")),
			RokAllianceTerritoryReadonly::GetChineseSourceText(Definition->DescriptionLocalizationId));
		Test.TestEqual(TEXT("Definition icon availability"), Label(TEXT("AllianceTerritoryDefinitionIconState")),
			FString(Definition->bSourceIconMissing ? TEXT("源图标缺失") : TEXT("源未配置图标")));
		if (Definition->DetailLocalizationId > 0)
		{
			Test.TestEqual(TEXT("Building definition rules"), Label(TEXT("AllianceTerritoryDefinitionRules")),
				RokAllianceTerritoryReadonly::GetChineseSourceText(Definition->DetailLocalizationId));
		}
		else
		{
			Test.TestNull(TEXT("Resource definition has no invented rules"), Find(TEXT("AllianceTerritoryDefinitionRules")));
		}
	}

	void CheckRankingOverview()
	{
		Test.TestTrue(TEXT("Ranking overview labels the unsynchronized rankings"), Label(TEXT("AllianceRankingOverviewBoundary")).Contains(TEXT("排名数据未接入")));
		for (const auto& Category : Categories)
		{
			Test.TestNotNull(TEXT("Every catalog category has a button"), Find(RankingHit(Category.Id)));
			Test.TestEqual(TEXT("Ranking card uses the source category name"), Label(*FString::Printf(TEXT("AllianceRankingCategoryName_%d"), Category.Id)), Category.Name);
			Test.TestEqual(TEXT("No leaderboard data in a category card"), Label(*FString::Printf(TEXT("AllianceRankingCategoryEmpty_%d"), Category.Id)), FString(TEXT("暂无排名数据")));
			const auto* Icon = Cast<UImage>(Find(*FString::Printf(TEXT("AllianceRankingCategoryIcon_%d"), Category.Id)));
			if (!Test.TestNotNull(TEXT("Ranking category has its atlas image"), Icon)) continue;
			const FBox2f UV = Icon->GetBrush().GetUVRegion();
			const FIntRect& Rect = Category.IconRect;
			Test.TestTrue(TEXT("Ranking icon uses the catalog source UV"),
				UV.Min.Equals(FVector2f(Rect.Min.X / 512.f, 1.f - Rect.Max.Y / 1024.f), 1e-4f)
				&& UV.Max.Equals(FVector2f(Rect.Max.X / 512.f, 1.f - Rect.Min.Y / 1024.f), 1e-4f));
		}
	}

	void CheckRankingDetail(const FRokLeaderboardRow& Category)
	{
		Test.TestNull(TEXT("Ranking detail removes category buttons"), Find(RankingHit(300)));
		Test.TestEqual(TEXT("Ranking detail renders the selected category"), Label(TEXT("AllianceRankingDetailTitle")), Category.Name);
		Test.TestEqual(TEXT("Ranking detail keeps the empty list"), Label(TEXT("AllianceRankingEmptyList")), FString(TEXT("暂无排名数据")));
		Test.TestEqual(TEXT("Ranking detail keeps the empty self-data state"), Label(TEXT("AllianceRankingNoSelfData")), FString(TEXT("当前排名与得分未接入")));
	}

	bool TextureReady(FName Name)
	{
		CaptureGate = FString::Printf(TEXT("Texture for %s"), *Name.ToString());
		const auto* Image = Cast<UImage>(Find(Name));
		const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->TextureRHI.IsValid()) return false;
#if WITH_EDITOR
		if (Texture->IsCompiling()) return false;
#endif
		return true;
	}

	bool CaptureState(const TCHAR* Name, bool bRankingOverview = false)
	{
		CaptureGate = TEXT("Window animation");
		const bool bReady = WindowSettled() && TextureReady(TEXT("FeatureFrameBodyLeft_Center"))
			&& (!bRankingOverview || TextureReady(TEXT("AllianceRankingCategoryIcon_300")));
		StableFrames = bReady ? StableFrames + 1 : 0;
		if (StableFrames < 3) return false;
		StableFrames = 0;
		if (URokUiSettings::IsLegacyForced() && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceTerritoryRankingExportSpecs")))
		{
			const FString SpecDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*SpecDir, true);
			FString Error;
			if (!Screen->ExportCurrentModeSpec(SpecDir / FString::Printf(TEXT("feature_%s.json"), Name), Error))
			{
				Test.AddError(FString::Printf(TEXT("Exporting %s failed: %s"), Name, *Error));
			}
		}
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok alliance territory/ranking audit capture: %s"), *CapturePath);
		return true;
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TArray<FRokLeaderboardRow> Categories;
	FString Output;
	FString CapturePath;
	FString CaptureGate = TEXT("No capture requested");
	double Deadline = FPlatformTime::Seconds() + 55.0;
	int32 Step = 0;
	int32 TerritoryGroup = 1;
	int32 RankingIndex = 0;
	int32 StableFrames = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceTerritoryRankingRenderedTest, "Rok.Ui.Alliance.TerritoryRankingRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceTerritoryRankingRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Territory and ranking reference replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceTerritoryRanking(*this));
	return true;
}

#endif
