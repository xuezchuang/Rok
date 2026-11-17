#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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
class FCheckAllianceTechnologyReference final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceTechnologyReference(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceTechnologyReferenceAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Alliance technology reference timed out in step %d (%s). Use -RokLocalOnly -RokOpenModal=alliance_joined."),
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
				if (!Test.TestTrue(TEXT("Offline static AllianceStudy catalog"), Catalog.IsValid() && Catalog->LoadAllianceTechnologyReadonly())) return true;
				Test.TestEqual(TEXT("Configured technology rows"), Catalog->GetAllianceTechnologyReadonlyRows().Num(), 460);
				if (!Click(TEXT("AllianceFeatureHit_5"))) return true;
				++Step;
				break;
			}
			return false;
		}
		if (!Test.TestTrue(TEXT("Live city technology reference"), Screen.IsValid() && Catalog.IsValid() && Hud.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (!CaptureState(1, 101, 1, TEXT("Develop"))) return false;
			CheckLayout(TEXT("AllianceTechnologyDefinitionScroll"), TEXT("WBP_RokFeature_AllianceTechnologyReference_C"));
			CheckLayout(DefinitionHit(101), TEXT("WBP_RokFeature_AllianceTechnologyReferenceRow_C"));
			Test.TestTrue(TEXT("Research remains server authority"), Label(TEXT("AllianceTechnologyReferenceBoundary")).Contains(TEXT("捐献与解锁由服务器决定")));
			break;
		case 2:
			if (!Click(TEXT("AllianceTechnologyConfiguredPrevious"))) return true;
			CheckDetail(101, 1);
			if (!Click(TEXT("AllianceTechnologyConfiguredNext"))) return true;
			break;
		case 3:
			if (!CaptureState(1, 101, 2, TEXT("DevelopLevel2"))) return false;
			break;
		case 4:
			for (int32 Level = 3; Level <= 5; ++Level)
			{
				if (!Click(TEXT("AllianceTechnologyConfiguredNext"))) return true;
				CheckDetail(101, Level);
			}
			if (!Click(TEXT("AllianceTechnologyConfiguredNext"))) return true;
			CheckDetail(101, 5);
			if (!ScrollToEnd()) return true;
			break;
		case 5:
			if (!LastIsInViewport(122)) return false;
			if (!Click(DefinitionHit(122))) return true;
			CheckDetail(122, 1);
			if (!ScrollToEnd()) return true;
			break;
		case 6:
			if (!LastIsInViewport(122) || !CaptureState(1, 122, 1, TEXT("DevelopLast"))) return false;
			break;
		case 7:
			if (!Click(TEXT("AllianceTechnologyCategory_2"))) return true;
			break;
		case 8:
			if (!CaptureState(2, 201, 1, TEXT("Territory"))) return false;
			break;
		case 9:
			if (!ScrollToEnd()) return true;
			break;
		case 10:
			if (!LastIsInViewport(216)) return false;
			if (!Click(DefinitionHit(216))) return true;
			CheckDetail(216, 1);
			if (!Click(TEXT("AllianceTechnologyCategory_3"))) return true;
			break;
		case 11:
			if (!CaptureState(3, 301, 1, TEXT("War"))) return false;
			break;
		case 12:
			if (!ScrollToEnd()) return true;
			break;
		case 13:
			if (!LastIsInViewport(322)) return false;
			if (!Click(DefinitionHit(322))) return true;
			CheckDetail(322, 1);
			if (!Click(TEXT("AllianceTechnologyReferenceBack"))) return true;
			break;
		case 14:
			if (!WindowSettled()) return false;
			Test.TestNotNull(TEXT("Joined entry returns to alliance overview"), Find(TEXT("AllianceFeatureHit_5")));
			Test.TestNull(TEXT("Return detaches technology"), Find(TEXT("AllianceTechnologyReferenceBack")));
			if (!Click(TEXT("FeatureFrameClose"))) return true;
			break;
		case 15:
		{
			if (Screen->IsScreenOpen()) return false;
			auto* Button = Cast<UButton>(RokUiFind::Find(Hud, TEXT("AllianceFeatureButton")));
			if (!Test.TestTrue(TEXT("Live city HUD exposes alliance"), Button && Button->GetIsEnabled())) return true;
			Button->OnClicked.Broadcast();
			Test.TestNotNull(TEXT("Normal HUD opens alliance welcome"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Normal alliance is not the joined preview"), Find(TEXT("AllianceJoinedFixtureTag")));
			if (!Click(TEXT("AllianceTechnologyReferenceOpen"))) return true;
			CheckDetail(101, 1);
			if (!Click(TEXT("AllianceTechnologyReferenceBack"))) return true;
			Test.TestNotNull(TEXT("Welcome technology entry returns to welcome"), Find(TEXT("AllianceWelcomeJoin")));
			Test.TestNull(TEXT("Reference is closed after the welcome return"), Find(TEXT("AllianceTechnologyReferenceBack")));
			return true;
		}
		}
		++Step;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }
	bool WindowSettled() const { return Screen.IsValid() && !Screen->GetWindowAnimationForTesting().IsAnimating(); }
	static FName DefinitionHit(int32 StudyType) { return *FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), StudyType); }
	UScrollBox* Scroll() const { return Cast<UScrollBox>(Find(TEXT("AllianceTechnologyDefinitionScroll"))); }

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

	bool ScrollToEnd()
	{
		auto* List = Scroll();
		if (!Test.TestNotNull(TEXT("Technology definitions have a live ScrollBox"), List)) return false;
		List->ScrollToEnd();
		return true;
	}

	bool LastIsInViewport(int32 StudyType)
	{
		CaptureGate = FString::Printf(TEXT("Scroll to definition %d"), StudyType);
		const auto* List = Scroll();
		const auto* Button = Find(DefinitionHit(StudyType));
		if (!List || !Button || List->GetScrollOffsetOfEnd() <= 0.0f
			|| List->GetScrollOffset() < List->GetScrollOffsetOfEnd() - 0.5f) return false;
		const FGeometry& View = List->GetCachedGeometry();
		const FGeometry& Hit = Button->GetCachedGeometry();
		const FVector2D Center = Hit.LocalToAbsolute(Hit.GetLocalSize() * .5);
		return Hit.GetLocalSize().Y > 1.f && View.IsUnderLocation(Center);
	}

	void CheckLayout(FName Name, const TCHAR* ClassName)
	{
		const auto* Widget = Find(Name);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s comes from the expected layout"), *Name.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	const UTexture2D* ImageTexture(FName Name) const
	{
		const auto* Image = Cast<UImage>(Find(Name));
		return Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
	}

	bool TextureReady(FName Name)
	{
		CaptureGate = FString::Printf(TEXT("Texture for %s"), *Name.ToString());
		const auto* Texture = ImageTexture(Name);
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->TextureRHI.IsValid()) return false;
#if WITH_EDITOR
		if (Texture->IsCompiling()) return false;
#endif
		return true;
	}

	bool IconMatches(FName Name, const FRokUnityAllianceTechnologyReadonlyRow& Row) const
	{
		const auto* Image = Cast<UImage>(Find(Name));
		const auto* Texture = ImageTexture(Name);
		if (!Texture || !Texture->GetName().EndsWith(Row.IconAtlas)) return false;
		const FVector2f Size(Row.AtlasSize.X, Row.AtlasSize.Y);
		const FBox2f UV = Image->GetBrush().GetUVRegion();
		return UV.Min.Equals(FVector2f(Row.IconRect.Min.X / Size.X, 1.0f - Row.IconRect.Max.Y / Size.Y), 1e-4f)
			&& UV.Max.Equals(FVector2f(Row.IconRect.Max.X / Size.X, 1.0f - Row.IconRect.Min.Y / Size.Y), 1e-4f);
	}

	void CheckDetail(int32 StudyType, int32 Level)
	{
		const auto* Definition = Catalog->GetAllianceTechnologyReadonlyRows().FindByPredicate([StudyType, Level](const FRokUnityAllianceTechnologyReadonlyRow& Row)
		{
			return Row.StudyType == StudyType && Row.LevelKey == Level;
		});
		if (!Test.TestNotNull(TEXT("Selected level exists in AllianceStudy"), Definition)) return;
		Test.TestEqual(TEXT("Technology title"), Label(TEXT("AllianceTechnologyDetailTitle")), Definition->Title);
		Test.TestEqual(TEXT("Technology identity shows the selected configuration row"), Label(TEXT("AllianceTechnologyDetailIdentity")),
			FString::Printf(TEXT("源配置行 ID %d · studyType %d · 配置等级 %d"), Definition->Id, StudyType, Level));
		Test.TestEqual(TEXT("Technology description"), Label(TEXT("AllianceTechnologyDetailDescription")), Definition->Description);
		TArray<FString> PrerequisiteIds;
		for (const FRokUnityAllianceTechnologyPrerequisite& Edge : Definition->Prerequisites) PrerequisiteIds.Add(FString::FromInt(Edge.SourceRowId));
		const FString Prerequisites = PrerequisiteIds.IsEmpty() ? FString(TEXT("无")) : FString::Join(PrerequisiteIds, TEXT("、"));
		Test.TestEqual(TEXT("Technology lists its configured prerequisite IDs"), Label(TEXT("AllianceTechnologyDetailPrerequisites")),
			FString(TEXT("配置前置 ID：")) + Prerequisites);
		Test.TestTrue(TEXT("Detail uses source icon atlas and UV"), IconMatches(TEXT("AllianceTechnologyDetailIcon"), *Definition));
	}

	void CheckCategory(int32 Category, int32 SelectedType)
	{
		int32 Count = 0;
		for (const auto& Definition : Catalog->GetAllianceTechnologyReadonlyRows())
		{
			if (Definition.LevelKey != 1) continue;
			const auto* Button = Cast<UButton>(Find(DefinitionHit(Definition.StudyType)));
			if (Definition.CategoryType != Category)
			{
				Test.TestNull(TEXT("Other categories do not have list actions"), Button);
				continue;
			}
			if (Button && Button->GetIsEnabled()) ++Count;
			const auto* Border = Cast<UBorder>(Find(*FString::Printf(TEXT("AllianceTechnologyDefinitionBg_%d"), Definition.StudyType)));
			const FLinearColor Selection = Definition.StudyType == SelectedType ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f) : FLinearColor(0.18f, 0.18f, 0.17f, 0.98f);
			Test.TestTrue(TEXT("Definition background displays selection"), Border && Border->GetBrushColor().Equals(Selection));
			Test.TestEqual(TEXT("List row uses the level-one title"), Label(*FString::Printf(TEXT("AllianceTechnologyName_%d"), Definition.StudyType)), Definition.Title);
			Test.TestTrue(*FString::Printf(TEXT("List definition %d uses its catalog icon"), Definition.StudyType),
				IconMatches(*FString::Printf(TEXT("AllianceTechnologyIcon_%d"), Definition.StudyType), Definition));
		}
		Test.TestEqual(TEXT("Expected definitions for this category"), Count, Category == 2 ? 16 : 22);
	}

	bool CaptureState(int32 Category, int32 StudyType, int32 Level, const TCHAR* Name)
	{
		CaptureGate = TEXT("Technology window animation");
		bool bReady = WindowSettled() && TextureReady(TEXT("FeatureFrameBodyLeft_Center"))
			&& TextureReady(TEXT("AllianceTechnologyReferenceBackBackground_Center")) && TextureReady(TEXT("AllianceTechnologyDetailIcon"));
		for (const auto& Row : Catalog->GetAllianceTechnologyReadonlyRows())
		{
			if (Row.LevelKey == 1 && Row.CategoryType == Category)
				bReady = bReady && TextureReady(*FString::Printf(TEXT("AllianceTechnologyIcon_%d"), Row.StudyType));
		}
		StableFrames = bReady ? StableFrames + 1 : 0;
		if (StableFrames < 3) return false;
		StableFrames = 0;
		CheckCategory(Category, StudyType);
		CheckDetail(StudyType, Level);
		if (URokUiSettings::IsLegacyForced() && FParse::Param(FCommandLine::Get(), TEXT("RokAllianceTechnologyExportSpecs")))
		{
			const FString SpecDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/Specs"));
			IFileManager::Get().MakeDirectory(*SpecDir, true);
			FString Error;
			if (!Screen->ExportCurrentModeSpec(SpecDir / FString::Printf(TEXT("feature_AllianceTechnologyReference%s.json"), Name), Error))
			{
				Test.AddError(FString::Printf(TEXT("Exporting technology layout %s failed: %s"), Name, *Error));
			}
		}
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok alliance technology audit capture: %s"), *CapturePath);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceTechnologyReferenceRenderedTest, "Rok.Ui.Alliance.TechnologyReferenceRenderedFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceTechnologyReferenceRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Technology reference replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceTechnologyReference(*this));
	return true;
}

#endif
