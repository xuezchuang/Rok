#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokAllianceFlagLocalPreviewData.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckAllianceFlagStyles final : public IAutomationLatentCommand
{
public:
	explicit FCheckAllianceFlagStyles(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/AllianceFlagStyleAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Alliance flag style replay timed out at tab %d."), Tab));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (!Screen.IsValid())
		{
			for (TObjectIterator<URokMainHudWidget> Hud; Hud; ++Hud)
			{
				if (!Hud->IsInViewport()) continue;
				Screen = Hud->GetFeatureScreenWidgetForPlacement();
				auto* Alliance = Cast<UButton>(RokUiFind::Find(*Hud, TEXT("AllianceFeatureButton")));
				if (!Test.TestTrue(TEXT("Live city HUD has its alliance entry"), Screen.IsValid() && Alliance && Alliance->GetIsEnabled())) return true;
				Alliance->OnClicked.Broadcast();
				if (!Click(TEXT("AllianceWelcomeCreate")) || !Click(TEXT("AllianceFlagEdit"))) return true;
				break;
			}
			return false;
		}
		if (bAdvanceTab)
		{
			bAdvanceTab = false;
			++Tab;
			if (Tab > 4)
			{
				if (!URokUiSettings::IsLegacyForced()) CheckTransientDesignerColors();
				if (!Click(TEXT("AllianceFlagEditorCancel"))) return true;
				bClosing = true;
				return false;
			}
			if (!Click(*FString::Printf(TEXT("AllianceFlagTab_%d"), Tab))) return true;
		}
		if (bClosing)
		{
			if (Screen->GetWindowAnimationForTesting().IsAnimating()) return false;
			Test.TestNotNull(TEXT("Cancel returns to the create form"), Find(TEXT("AllianceCreateNoticeInput")));
			return true;
		}
		const bool bArtReady = !Screen->GetWindowAnimationForTesting().IsAnimating()
			&& TextureReady(TEXT("FeatureFrameBodyLeft_Center")) && TextureReady(TEXT("AllianceFlagEditorCloth"));
		StableFrames = bArtReady ? StableFrames + 1 : 0;
		if (StableFrames < 3) return false;
		StableFrames = 0;
		CheckTabAndOptionStyles();
		CapturePath = Output / FString::Printf(TEXT("Tab%d.png"), Tab);
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok alliance flag style audit capture: %s"), *CapturePath);
		bAdvanceTab = true;
		return false;
	}

private:
	UWidget* Find(FName Name) const { return RokUiFind::Find(Screen, Name); }
	static FName TabPanel(int32 Type) { return *FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Type); }
	static FName ItemPanel(int32 Id) { return *FString::Printf(TEXT("AllianceFlagItemPanel_%d"), Id); }

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(Find(Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->IsVisible() && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	bool TextureReady(FName Name) const
	{
		const auto* Image = Cast<UImage>(Find(Name));
		const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
		if (!Texture || !Texture->GetResource() || !Texture->GetResource()->TextureRHI.IsValid()) return false;
#if WITH_EDITOR
		if (Texture->IsCompiling()) return false;
#endif
		return true;
	}

	bool CheckBorder(FName LegacyName, FName AuthoredName, const FLinearColor& Color)
	{
		const auto* Border = Cast<UBorder>(Find(LegacyName));
		return Test.TestTrue(*LegacyName.ToString(), Border && Border->IsVisible() && Border->GetBrushColor().Equals(Color)
			&& (URokUiSettings::IsLegacyForced() || Border->GetFName() == AuthoredName));
	}

	void CheckTabAndOptionStyles()
	{
		for (int32 Type = 1; Type <= 4; ++Type)
		{
			const bool bSelected = Type == Tab;
			const FName Name = TabPanel(Type);
			const FName StyleName = bSelected ? FName(*(Name.ToString() + TEXT("Selected"))) : Name;
			const FLinearColor LegacyColor = bSelected ? FLinearColor(0.48f, 0.36f, 0.14f, 1.0f) : FLinearColor(0.22f, 0.21f, 0.20f, 1.0f);
			CheckAuthoredBorder(TEXT("AllianceFlagEditor"), StyleName, Name, LegacyColor);
		}
		const RokAllianceFlagLocalPreview::FDraft Defaults;
		for (const auto& Entry : RokAllianceFlagLocalPreview::Rows())
		{
			if (Entry.Type != Tab) continue;
			const bool bSelected = Entry.Id == Defaults.GetId(Tab);
			const FName Page = Entry.bHasColor ? FName(TEXT("AllianceFlagColorOption")) : FName(TEXT("AllianceFlagSpriteOption"));
			const FName StyleName = bSelected ? FName(TEXT("PanelSelected")) : FName(TEXT("Panel"));
			const FLinearColor LegacyColor = bSelected ? FLinearColor(0.67f, 0.48f, 0.13f, 1.0f) : FLinearColor(0.20f, 0.20f, 0.19f, 1.0f);
			CheckAuthoredBorder(Page, StyleName, ItemPanel(Entry.Id), LegacyColor);
		}
	}

	void CheckAuthoredBorder(FName Page, FName StyleName, FName LiveName, const FLinearColor& LegacyColor)
	{
		const bool bLegacy = URokUiSettings::IsLegacyForced();
		const UBorder* Style = bLegacy ? nullptr : ArchetypeBorder(Page, StyleName);
		if (!bLegacy && !Test.TestNotNull(TEXT("Selection style is authored in its WBP"), Style)) return;
		CheckBorder(LiveName, StyleName, Style ? Style->GetBrushColor() : LegacyColor);
	}

	UBorder* ArchetypeBorder(FName Page, FName Name) const
	{
		const auto* Class = Cast<UWidgetBlueprintGeneratedClass>(URokUiSettings::ResolvePage(GetDefault<URokUiSettings>()->FeaturePages, Page));
		const auto* Tree = Class ? Class->GetWidgetTreeArchetype() : nullptr;
		return Tree ? Tree->FindWidget<UBorder>(Name) : nullptr;
	}

	void CheckTransientDesignerColors()
	{
		auto* SelectedTab = ArchetypeBorder(TEXT("AllianceFlagEditor"), TEXT("AllianceFlagTabPanel_1Selected"));
		auto* NormalTab = ArchetypeBorder(TEXT("AllianceFlagEditor"), TabPanel(2));
		auto* SelectedSprite = ArchetypeBorder(TEXT("AllianceFlagSpriteOption"), TEXT("PanelSelected"));
		auto* NormalColor = ArchetypeBorder(TEXT("AllianceFlagColorOption"), TEXT("Panel"));
		if (!Test.TestTrue(TEXT("All four probe borders are authored in their WBP archetypes"), SelectedTab && NormalTab && SelectedSprite && NormalColor)) return;
		const FLinearColor Original[] = {SelectedTab->GetBrushColor(), NormalTab->GetBrushColor(), SelectedSprite->GetBrushColor(), NormalColor->GetBrushColor()};
		ON_SCOPE_EXIT {
			SelectedTab->SetBrushColor(Original[0]);
			NormalTab->SetBrushColor(Original[1]);
			SelectedSprite->SetBrushColor(Original[2]);
			NormalColor->SetBrushColor(Original[3]);
		};
		// Exercise edited styles in memory without saving the asset package.
		SelectedTab->SetBrushColor(FLinearColor::Red);
		NormalTab->SetBrushColor(FLinearColor::Green);
		SelectedSprite->SetBrushColor(FLinearColor::Blue);
		NormalColor->SetBrushColor(FLinearColor::White);
		if (!Click(TEXT("AllianceFlagTab_1"))) return;
		CheckBorder(TabPanel(1), TEXT("AllianceFlagTabPanel_1Selected"), FLinearColor::Red);
		CheckBorder(TabPanel(2), TabPanel(2), FLinearColor::Green);
		CheckBorder(ItemPanel(102), TEXT("PanelSelected"), FLinearColor::Blue);
		if (Click(TEXT("AllianceFlagTab_2"))) CheckBorder(ItemPanel(201), TEXT("Panel"), FLinearColor::White);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 55.0;
	int32 Tab = 1;
	int32 StableFrames = 0;
	bool bAdvanceTab = false;
	bool bClosing = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllianceFlagStyleRenderedTest, "Rok.Ui.Alliance.FlagSelectionStyles",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FRokAllianceFlagStyleRenderedTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("Flag styles replay is explicitly offline"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FCheckAllianceFlagStyles(*this));
	return true;
}

#endif
