#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "../RokGameNetSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokPlayerController.h"
#include "../RokServerCitySubsystem.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
class FCheckBuildCityDrawer final : public IAutomationLatentCommand
{
public:
	explicit FCheckBuildCityDrawer(FAutomationTestBase& InTest)
		: Test(InTest), Output(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUi/BuildCityAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))))
	{
		IFileManager::Get().MakeDirectory(*Output, true);
	}

	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() >= Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Construction drawer timed out at step %d. Run the age-two city snapshot offline."), Step));
			return true;
		}
		if (!CapturePath.IsEmpty() && IFileManager::Get().FileSize(*CapturePath) <= 0) return false;
		if (Step == 0) return OpenFromHud();
		if (!Test.TestTrue(TEXT("Live drawer host and controller"), Screen.IsValid() && Controller.IsValid() && Hud.IsValid())) return true;
		switch (Step)
		{
		case 1:
			if (!IsCardArtReady()) return false;
			Capture(TEXT("Economy"));
			break;
		case 2:
		{
			auto* Scroll = FindScroll();
			auto* Card = CardAsset(FirstLockedType);
			if (!Test.TestTrue(TEXT("Locked card is in the live scroll"), Scroll && Card)) return true;
			Scroll->ScrollWidgetIntoView(Card, false, EDescendantScrollDestination::Center);
			break;
		}
		case 3:
		{
			const auto* Scroll = FindScroll();
			if (!Scroll || Scroll->GetScrollOffset() <= 0.0f || !IsCardArtReady()) return false;
			Capture(TEXT("Locked"));
			break;
		}
		case 4:
			if (!Click(Screen.Get(), TEXT("BuildCityCategory_1"))) return true;
			Test.TestNotNull(TEXT("Military filter displays barracks"), CardAsset(9));
			Test.TestNull(TEXT("Military filter removes quarry"), CardAsset(4));
			break;
		case 5:
			if (!IsCardArtReady()) return false;
			Capture(TEXT("Military"));
			break;
		case 6:
			if (!Click(Screen.Get(), TEXT("BuildCityCategory_2"))) return true;
			Test.TestNotNull(TEXT("Decorations show source type 51"), CardAsset(51));
			Test.TestNull(TEXT("Decorations remove barracks"), CardAsset(9));
			break;
		case 7:
			if (!IsCardArtReady()) return false;
			Capture(TEXT("Decoration"));
			break;
		case 8:
			if (!Click(Screen.Get(), TEXT("BuildCityCategory_0")) || !Click(Screen.Get(), TEXT("BuildCityCardButton_4"))) return true;
			CheckPlacement();
			break;
		case 9:
			if (!Click(Hud.Get(), TEXT("CityPlacementCancelButton"))) return true;
			Test.TestFalse(TEXT("Existing HUD cancel discards preview"), Controller->GetCityPlacementPreviewSnapshot().bActive);
			if (!Click(Hud.Get(), TEXT("BuildCityButton"))) return true;
			break;
		case 10:
		{
			const auto* Drawer = RokUiFind::Find(Screen, TEXT("BuildCityAnimatedRoot"));
			if (!Test.TestNotNull(TEXT("Reopened animated drawer"), Drawer)) return true;
			if (Drawer->GetRenderOpacity() < 0.999f) return false;
			if (!Click(Screen.Get(), TEXT("BuildCityMaskClose"))) return true;
			break;
		}
		case 11:
			return !Screen->IsScreenOpen();
		default:
			return true;
		}
		++Step;
		return false;
	}

private:
	bool OpenFromHud()
	{
		for (TObjectIterator<URokMainHudWidget> It; It; ++It)
		{
			auto* World = It->GetWorld();
			if (!World || !It->IsInViewport()) continue;
			const auto* City = World->GetSubsystem<URokServerCitySubsystem>();
			const auto* Net = It->GetGameInstance()->GetSubsystem<URokGameNetSubsystem>();
			if (!City || !City->HasAppliedServerCity() || !Net || !Net->IsSnapshotReplay()) continue;
			Hud = *It;
			Screen = It->GetFeatureScreenWidgetForPlacement();
			Controller = Cast<ARokPlayerController>(It->GetOwningPlayer());
			if (!Test.TestTrue(TEXT("City HUD attaches the real mode and controller"), Screen.IsValid() && Controller.IsValid())
				|| !Click(Hud.Get(), TEXT("BuildCityButton"))) return true;
			if (!Test.TestTrue(TEXT("HUD opens the BuildCity mode"), Screen->IsBuildCityModeForAudit())) return true;
			CheckLayoutClass(TEXT("BuildCityAnimatedRoot"), TEXT("WBP_RokFeature_BuildCity_C"));
			CheckLayoutClass(TEXT("BuildCityCardButton_4"), TEXT("WBP_RokFeature_BuildCityCardAvailable_C"));
			const int32 Types[] = {2, 3, 4, 13};
			auto* Scroll = FindScroll();
			if (!Test.TestNotNull(TEXT("Native horizontal scroll"), Scroll)) return true;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Types); ++Index)
			{
				const auto* Card = Cast<USizeBox>(Scroll->GetChildAt(Index));
				Test.TestTrue(TEXT("Economy cards retain their stable type and order"), Card && Card == CardAsset(Types[Index]));
			}
			for (int32 Type : {5, 7, 15, 16, 19, 20, 21, 30})
			{
				const FName LockedName(*FString::Printf(TEXT("BuildCityCardLocked_%d"), Type));
				if (!RokUiFind::Find(Screen, LockedName)) continue;
				FirstLockedType = Type;
				CheckLayoutClass(LockedName, TEXT("WBP_RokFeature_BuildCityCardLocked_C"));
				Test.TestNull(TEXT("Locked card exposes no build hit target"),
					RokUiFind::Find(Screen, *FString::Printf(TEXT("BuildCityCardButton_%d"), Type)));
				break;
			}
			if (!Test.TestTrue(TEXT("Catalog contains a gated card"), FirstLockedType != INDEX_NONE)) return true;
			Screen->SampleBuildCityAnimationForCapture(10.0f);
			++Step;
			return false;
		}
		return false;
	}

	void CheckPlacement()
	{
		const auto State = Controller->GetCityPlacementPreviewSnapshot();
		Test.TestFalse(TEXT("Real card closes the drawer"), Screen->IsScreenOpen());
		Test.TestTrue(TEXT("Real quarry card transfers into a visible, valid placement preview"),
			State.bActive && State.bGridValid && State.PreviewActor.IsValid() && !State.PreviewActor->IsHidden());
		Test.TestEqual(TEXT("Placement keeps the quarry type ID"), State.TypeId, 4);
		Test.TestFalse(TEXT("Offline snapshot cannot commit construction"), State.bCanCommit);
	}

	void CheckLayoutClass(FName WidgetName, const TCHAR* ClassName)
	{
		const auto* Widget = RokUiFind::Find(Screen, WidgetName);
		const auto* Page = Widget ? Widget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		Test.TestTrue(*FString::Printf(TEXT("%s uses the expected layout"), *WidgetName.ToString()),
			Widget && (URokUiSettings::IsLegacyForced() ? !Page : Page && Page->GetClass()->GetName() == ClassName));
	}

	UScrollBox* FindScroll() const { return Cast<UScrollBox>(RokUiFind::Find(Screen, TEXT("BuildCityBuildingScroll"))); }
	UWidget* CardAsset(int32 Type) const { return RokUiFind::Find(Screen, *FString::Printf(TEXT("BuildCityCardSize_%d"), Type)); }

	bool IsCardArtReady()
	{
		const auto* Scroll = FindScroll();
		if (!Scroll || Scroll->GetChildrenCount() == 0) return false;
		for (int32 Index = 0; Index < Scroll->GetChildrenCount(); ++Index)
		{
			FString Type = Scroll->GetChildAt(Index)->GetName();
			if (!Type.RemoveFromStart(TEXT("BuildCityCardSize_"))) return false;
			for (const TCHAR* Stem : {TEXT("BuildCityCardLeft_"), TEXT("BuildCityCardPreview_")})
			{
				const auto* Image = Cast<UImage>(RokUiFind::Find(Screen, *(FString(Stem) + Type)));
				const auto* Texture = Image ? Cast<UTexture2D>(Image->GetBrush().GetResourceObject()) : nullptr;
				if (!Texture) continue;
#if WITH_EDITOR
				if (Texture->IsCompiling()) return false;
#endif
				const FTextureResource* Resource = Texture->GetResource();
				if (!Resource || !Resource->TextureRHI.IsValid()) return false;
			}
		}
		return true;
	}

	bool Click(UUserWidget* Owner, FName Name)
	{
		auto* Button = Cast<UButton>(RokUiFind::Find(Owner, Name));
		if (!Test.TestTrue(*Name.ToString(), Button && Button->GetIsEnabled() && Button->IsVisible())) return false;
		Button->OnClicked.Broadcast();
		if (Owner == Screen.Get()) Screen->FlushPendingActionForTesting();
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		CapturePath = Output / (FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
		UE_LOG(LogTemp, Display, TEXT("Rok BuildCity drawer audit capture: %s"), *CapturePath);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokMainHudWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TWeakObjectPtr<ARokPlayerController> Controller;
	FString Output;
	FString CapturePath;
	double Deadline = FPlatformTime::Seconds() + 45.0;
	int32 Step = 0;
	int32 FirstLockedType = INDEX_NONE;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildCityDrawerReplayTest, "Rok.Ui.BuildCity.DrawerReplay",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokBuildCityDrawerReplayTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckBuildCityDrawer(*this));
	return true;
}

#endif
