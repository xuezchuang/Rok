#if WITH_DEV_AUTOMATION_TESTS

#include "../RokDataPaths.h"
#include "../RokUiWidgets.h"
#include "../RokWorldMapEntityActor.h"
#include "../RokWorldMapHudWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"

namespace
{
class FCheckWorldResourceDescriptionStyles final : public IAutomationLatentCommand
{
public:
	explicit FCheckWorldResourceDescriptionStyles(FAutomationTestBase& InTest) : Test(InTest) {}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(TEXT("World map with offline food and stone resource actors did not become ready"));
			return true;
		}
		for (TObjectIterator<URokWorldMapHudWidget> Hud; Hud; ++Hud)
		{
			UWorld* World = Hud->GetWorld();
			if (!Hud->IsInViewport() || !World || World->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")) continue;
			ARokWorldMapEntityActor* Food = nullptr;
			ARokWorldMapEntityActor* Stone = nullptr;
			ARokWorldMapEntityActor* Barbarian = nullptr;
			for (TActorIterator<ARokWorldMapEntityActor> Actor(World); Actor; ++Actor)
			{
				if (!Food && Actor->GetEntityKind() == TEXT("food")) Food = *Actor;
				if (!Stone && Actor->GetEntityKind() == TEXT("stone")) Stone = *Actor;
				if (!Barbarian && Actor->GetEntityKind() == TEXT("barbarian")) Barbarian = *Actor;
				if (Food && Stone && Barbarian) break;
			}
			if (!Food || !Stone || !Barbarian) return false;
			Verify(*Hud, *Food, *Stone, *Barbarian);
			return true;
		}
		return false;
	}

private:
	void Verify(URokWorldMapHudWidget* Hud, ARokWorldMapEntityActor& Food, ARokWorldMapEntityActor& Stone,
		ARokWorldMapEntityActor& Barbarian)
	{
		const auto Find = [Hud](FName Name) { return RokUiFind::Find(Hud, Name); };
		auto* Text = Cast<URichTextBlock>(Find(TEXT("WorldObjectInfoDetailsText")));
		auto* Open = Cast<UButton>(Find(TEXT("WorldObjectInfoDetailsButton")));
		auto* Back = Cast<UButton>(Find(TEXT("WorldObjectDescriptionBack")));
		const auto* Scroll = Cast<UScrollBox>(Find(TEXT("WorldObjectDescriptionScroll")));
		const auto* Normal = Find(TEXT("WorldObjectInfoPanelContent"));
		const auto* Details = Find(TEXT("WorldObjectInfoDetailsRoot"));
		if (!Test.TestTrue(TEXT("Actual world HUD has source details, scroll, and both actions"), Text && Open && Back
			&& Scroll && Normal && Details && RokUiFind::IsInside(Text, Scroll))) return;
		const auto* Styles = Text->GetTextStyleSet();
		if (!Test.TestNotNull(TEXT("Details have a text style table"), Styles)) return;
		const auto* Regular = Styles->FindRow<FRichTextStyleRow>(TEXT("Default"), TEXT("World description replay"), false);
		const auto* Bold = Styles->FindRow<FRichTextStyleRow>(TEXT("b"), TEXT("World description replay"), false);
		if (!Test.TestTrue(TEXT("Both source style rows exist"), Regular && Bold)) return;
		const auto* Layout = Text->GetTypedOuter<URokUiPageWidget>();
		const bool bLegacy = URokUiSettings::IsLegacyForced();
		if (bLegacy)
		{
			Test.TestTrue(TEXT("Legacy mounts the original native text style fallback"), !Layout && !Styles->IsAsset());
		}
		else
		{
			Test.TestTrue(TEXT("Actual popup comes from the WorldObjectInfo WBP"), Layout
				&& Layout->GetClass()->GetName() == TEXT("WBP_RokWorldObjectInfo_C"));
			Test.TestTrue(TEXT("Runtime uses the saved WBP style table"), Styles->IsAsset()
				&& Styles->GetPathName() == TEXT("/Game/RokUI/Hud/DT_RokWorldResourceDescriptionStyles.DT_RokWorldResourceDescriptionStyles"));
			Test.TestTrue(TEXT("Saved styles use a non-transient font asset"), Regular->TextStyle.Font.FontObject
				&& Regular->TextStyle.Font.FontObject == Bold->TextStyle.Font.FontObject.Get()
				&& Regular->TextStyle.Font.FontObject->IsAsset());
		}
		Test.TestEqual(TEXT("Source regular face"), Regular->TextStyle.Font.TypefaceFontName, FName(TEXT("Regular")));
		Test.TestEqual(TEXT("Source bold face"), Bold->TextStyle.Font.TypefaceFontName, FName(TEXT("Bold")));
		Test.TestEqual(TEXT("Source regular size"), Regular->TextStyle.Font.Size, 20.0f);
		Test.TestEqual(TEXT("Source bold size"), Bold->TextStyle.Font.Size, 20.0f);
		Test.TestTrue(TEXT("Both rows use the original ink color and zero shadow offset"),
			Regular->TextStyle.ColorAndOpacity.GetSpecifiedColor().Equals(FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1))
			&& Bold->TextStyle.ColorAndOpacity.GetSpecifiedColor().Equals(Regular->TextStyle.ColorAndOpacity.GetSpecifiedColor())
			&& Regular->TextStyle.ShadowOffset.IsNearlyZero() && Bold->TextStyle.ShadowOffset.IsNearlyZero());

		FString Json;
		TSharedPtr<FJsonObject> Source;
		const TSharedPtr<FJsonObject>* Descriptions = nullptr;
		if (!Test.TestTrue(TEXT("Original world resource descriptions are available"),
			FFileHelper::LoadFileToString(Json, *RokDataPaths::Resolve(TEXT("world_resource_descriptions.json")))
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Source) && Source.IsValid()
			&& Source->TryGetObjectField(TEXT("descriptions"), Descriptions))) return;
		const auto FindSlot = [&Find](FName Name)
		{
			const UWidget* Widget = Find(Name);
			return Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
		};
		auto* Popup = FindSlot(TEXT("WorldObjectInfoRoot"));
		auto* ResourceDesign = FindSlot(TEXT("WorldObjectInfoPanel"));
		auto* BarbarianDesign = FindSlot(TEXT("WorldBarbarianInfo"));
		if (!Test.TestTrue(TEXT("Popup and each content page have their own slots"), Popup && ResourceDesign && BarbarianDesign)) return;
		const FVector2D ResourceSize = bLegacy ? FVector2D(479, 357) : FVector2D(521, 377);
		const FVector2D BarbarianSize = bLegacy ? FVector2D(486, 434) : FVector2D(523, 451);
		if (!bLegacy)
		{
			ResourceDesign->SetSize(ResourceSize);
			BarbarianDesign->SetSize(BarbarianSize);
		}
		for (auto* Resource : {&Food, &Stone})
		{
			const TSharedPtr<FJsonObject>* Description = nullptr;
			FString Expected;
			if (!Test.TestTrue(TEXT("Selected resource has original rich text with emphasis"),
				(*Descriptions)->TryGetObjectField(Resource->GetEntityKind(), Description)
				&& (*Description)->TryGetStringField(TEXT("slate_text"), Expected) && Expected.Contains(TEXT("<b>")))) return;
			Hud->ShowWorldObjectInfo(Resource);
			Test.TestTrue(TEXT("Resource popup uses the selected authored rectangle"), Popup->GetSize().Equals(ResourceSize));
			if (!Test.TestTrue(TEXT("Selected live resource opens a usable details action"),
				Hud->IsWorldObjectInfoOpen() && Open->IsVisible() && Open->GetIsEnabled())) return;
			Open->OnClicked.Broadcast();
			Hud->AdvanceWorldObjectPageAnimation(1.0f);
			Test.TestTrue(TEXT("Actual details action reveals the source prose"), Details->IsVisible() && !Normal->IsVisible()
				&& Text->GetText().ToString() == Expected && Text->GetTextStyleSet() == Styles);
			if (!Test.TestTrue(TEXT("Original description return action is available"), Back->IsVisible() && Back->GetIsEnabled())) return;
			Back->OnClicked.Broadcast();
			Hud->AdvanceWorldObjectPageAnimation(1.0f);
			Test.TestTrue(TEXT("Returning restores the resource overview"), Normal->IsVisible() && !Details->IsVisible());
		}
		Hud->ShowWorldObjectInfo(&Barbarian);
		Test.TestTrue(TEXT("Barbarian popup retains its separate authored rectangle"), Popup->GetSize().Equals(BarbarianSize));
		Hud->ShowWorldObjectInfo(&Food);
		Test.TestTrue(TEXT("Returning to resource restores its own bounds"), Popup->GetSize().Equals(ResourceSize));
		Hud->HideWorldObjectInfo();
		Hud->AdvanceWorldObjectAnimation(1.0f);
	}

	FAutomationTestBase& Test;
	const double Deadline = FPlatformTime::Seconds() + 55.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldResourceDescriptionStyleTest, "Rok.Ui.World.ResourceDescriptionStyles",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokWorldResourceDescriptionStyleTest::RunTest(const FString&)
{
	if (!TestTrue(TEXT("The resource description check runs offline in the offscreen world"),
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) && FParse::Param(FCommandLine::Get(), TEXT("RokNoNet"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RenderOffscreen")))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FCheckWorldResourceDescriptionStyles(*this));
	return true;
}

#endif
