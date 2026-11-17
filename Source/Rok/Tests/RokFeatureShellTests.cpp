#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GenericPlatform/GenericWindow.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SWindow.h"
#include "UnrealClient.h"
#include "../RokMainHudWidget.h"
#include "../RokUiWidgets.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokWorldMapHudWidget.h"

namespace
{
class FCheckFeatureShellInput final : public IAutomationLatentCommand
{
public:
	FCheckFeatureShellInput(FAutomationTestBase& InTest, FIntPoint InViewportSize, bool bInWorld)
		: Test(InTest), ExpectedViewportSize(InViewportSize), bWorld(bInWorld)
	{
	}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Feature shell input did not complete step %d at %dx%d"),
				Step, ExpectedViewportSize.X, ExpectedViewportSize.Y));
			return true;
		}

		FindActiveHud();
		if (!Screen.IsValid() || !FSlateApplication::IsInitialized())
		{
			return false;
		}
		const UGameViewportClient* Viewport = Hud->GetWorld() ? Hud->GetWorld()->GetGameViewport() : nullptr;
		const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
		if (!Window || !Viewport->Viewport || Viewport->Viewport->GetSizeXY() != ExpectedViewportSize)
		{
			return false;
		}
		const auto NativeWindow = Window->GetNativeWindow();
		if (!Test.TestTrue(TEXT("Only a Null-native offscreen window receives virtual input"),
			NativeWindow && NativeWindow->IsRenderingOffScreen() && NativeWindow->GetOSWindowHandle() == nullptr))
		{
			return true;
		}
		return bWorld ? UpdateWorld(*Window) : UpdateCity(*Window);
	}

private:
	void FindActiveHud()
	{
		if (Screen.IsValid() && Hud.IsValid())
		{
			return;
		}
		if (bWorld)
		{
			for (TObjectIterator<URokWorldMapHudWidget> Candidate; Candidate; ++Candidate)
			{
				if (Candidate->IsInViewport())
				{
					Hud = *Candidate;
					Screen = Cast<URokUnityFeatureScreenWidget>(RokUiFind::Find(*Candidate, TEXT("WorldMapUnityFeatureScreenWidget")));
					return;
				}
			}
			return;
		}
		for (TObjectIterator<URokMainHudWidget> Candidate; Candidate; ++Candidate)
		{
			if (Candidate->IsInViewport())
			{
				Hud = *Candidate;
				Screen = Candidate->GetFeatureScreenWidgetForPlacement();
				return;
			}
		}
	}

	bool CheckShellLayout()
	{
		const auto* ShellContent = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
		const auto* Design = Cast<USizeBox>(Screen->GetWidgetFromName(TEXT("FeatureExpandedDesignSize")));
		if (!ShellContent || !Design)
		{
			return false;
		}
		const auto* Shell = ShellContent->GetTypedOuter<URokUiPageWidget>();
		Test.TestTrue(TEXT("Feature shell uses the expected presentation"), URokUiSettings::IsLegacyForced()
			? !Shell : Shell && Shell->GetClass()->GetName() == TEXT("WBP_RokFeature_FeatureShell_C"));
		const double Aspect = static_cast<double>(ExpectedViewportSize.X) / ExpectedViewportSize.Y;
		const FVector2D ExpectedDesign = Aspect >= 1440.0 / 810.0 ? FVector2D(810.0 * Aspect, 810.0)
			: FVector2D(1440.0, 1440.0 / Aspect);
		Test.TestTrue(TEXT("Expanded design follows the live viewport"),
			FVector2D(Design->GetWidthOverride(), Design->GetHeightOverride()).Equals(ExpectedDesign, 1.0));
		const auto* Chrome = Screen->GetWidgetFromName(TEXT("CommanderChrome"));
		Test.TestTrue(TEXT("Commander chrome remains a sibling of the mode"), Chrome
			&& Chrome->GetParent() == ShellContent->GetParent());
		return true;
	}

	bool UpdateCity(const SWindow& Window)
	{
		switch (Step)
		{
		case 0:
			if (!Click(Window, Cast<UButton>(RokUiFind::Find(Hud, TEXT("MailFeatureHitButton")))))
			{
				return false;
			}
			break;
		case 1:
		{
			if (!Screen->IsScreenOpen() || Screen->GetWindowAnimationForTesting().IsAnimating())
			{
				return false;
			}
			const auto* Blocker = Screen->GetWidgetFromName(TEXT("FeatureScreenInputBlocker"));
			const auto* Dimmer = Cast<UBorder>(Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer")));
			if (!Blocker || !Dimmer || !CheckShellLayout())
			{
				return false;
			}
			Test.TestTrue(TEXT("The ordinary modal dims its underlay"), Dimmer->IsVisible()
				&& FMath::IsNearlyEqual(Dimmer->GetBrushColor().A, 0.74f));
			const FVector2D Blank = Blocker->GetCachedGeometry().LocalToAbsolute(FVector2D(10, 400));
			Test.TestTrue(TEXT("The modal captures outside input"),
				IsInPath(Window, *Blocker, Blank));
			if (!Click(Window, Cast<UButton>(Screen->GetWidgetFromName(TEXT("MailTab_2")))))
			{
				return false;
			}
			break;
		}
		case 2:
			if (Screen->GetSelectedMailTabForAudit() != 2)
			{
				return false;
			}
			if (!Click(Window, Cast<UButton>(Screen->GetWidgetFromName(TEXT("FeatureFrameClose")))))
			{
				return false;
			}
			break;
		case 3:
			if (Screen->IsScreenOpen())
			{
				return false;
			}
			if (!Click(Window, Cast<UButton>(RokUiFind::Find(Hud, TEXT("ChatButton")))))
			{
				return false;
			}
			break;
		case 4:
		{
			if (!Screen->IsScreenOpen() || !Screen->GetWidgetFromName(TEXT("ChatAnimationInputShield")))
			{
				return false;
			}
			const auto* Blocker = Screen->GetWidgetFromName(TEXT("FeatureScreenInputBlocker"));
			const auto* Dimmer = Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer"));
			Test.TestTrue(TEXT("Chat leaves surrounding HUD input available"), Blocker
				&& Blocker->GetVisibility() == ESlateVisibility::Collapsed);
			Test.TestTrue(TEXT("Chat has no modal dimmer"), Dimmer && Dimmer->GetVisibility() == ESlateVisibility::Collapsed);
			Test.AddInfo(FString::Printf(TEXT("Actual HUD Mail and Chat virtual Slate routes completed at %dx%d"),
				ExpectedViewportSize.X, ExpectedViewportSize.Y));
			return true;
		}
		default:
			return true;
		}
		++Step;
		return false;
	}

	bool UpdateWorld(const SWindow& Window)
	{
		switch (Step)
		{
		case 0:
			if (!Click(Window, Cast<UButton>(RokUiFind::Find(Hud, TEXT("WorldMapSearchButton")))))
			{
				return false;
			}
			break;
		case 1:
		{
			const auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("ResourceSearchSlider")));
			if (!Screen->IsResourceSearchOpen() || !Slider)
			{
				return false;
			}
			PreviousLevel = Slider->GetValue();
			if (!Click(Window, Cast<UButton>(Screen->GetWidgetFromName(TEXT("ResourceSearchLevelPlus")))))
			{
				return false;
			}
			if (!CheckShellLayout())
			{
				Test.AddError(TEXT("World search did not expose its containing shell"));
				return true;
			}
			const auto* Dimmer = Screen->GetWidgetFromName(TEXT("FeatureScreenDimmer"));
			Test.TestTrue(TEXT("World search leaves the world undimmed"), Dimmer
				&& Dimmer->GetVisibility() == ESlateVisibility::Collapsed);
			break;
		}
		case 2:
		{
			const auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(TEXT("ResourceSearchSlider")));
			if (!Slider || !FMath::IsNearlyEqual(Slider->GetValue(), PreviousLevel + 1.0f)
				|| !Click(Window, Cast<UButton>(Screen->GetWidgetFromName(TEXT("ResourceSearchTouchCloseMask"))),
					FVector2D(0.1, 0.1)))
			{
				return false;
			}
			break;
		}
		case 3:
			if (Screen->IsScreenOpen())
			{
				return false;
			}
			Test.AddInfo(FString::Printf(TEXT("Actual World HUD search, level increase, and click-away completed at %dx%d"),
				ExpectedViewportSize.X, ExpectedViewportSize.Y));
			return true;
		default:
			return true;
		}
		++Step;
		return false;
	}

	bool IsInPath(const SWindow& Window, const UWidget& Target, const FVector2D& Point)
	{
		if (!VirtualUser)
		{
			VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(9);
		}
		const int32 User = VirtualUser->GetUserIndex();
		const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(
			Point, FSlateApplication::Get().GetInteractiveTopLevelWindows(), false, User);
		const auto Cached = Target.GetCachedWidget();
		return Path.IsValid() && &Path.GetWindow().Get() == &Window && Cached
			&& Path.ContainsWidget(Cached.Get());
	}

	bool Click(const SWindow& Window, UButton* Button, FVector2D LocalFraction = FVector2D(0.5))
	{
		if (!Button || !Button->IsVisible() || !Button->GetIsEnabled())
		{
			return false;
		}
		const auto Geometry = Button->GetCachedGeometry();
		if (Geometry.GetLocalSize().X < 2.0 || Geometry.GetLocalSize().Y < 2.0)
		{
			return false;
		}
		const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * LocalFraction);
		if (!IsInPath(Window, *Button, Point))
		{
			return false;
		}
		FSlateApplication& Slate = FSlateApplication::Get();
		const auto Focus = Slate.GetUserFocusedWidget(0);
		const int32 User = VirtualUser->GetUserIndex();
		Slate.ProcessMouseMoveEvent(FPointerEvent(User, 0, Point, Point,
			TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
		Slate.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
			FPointerEvent(User, 0, Point, Point, {EKeys::LeftMouseButton},
				EKeys::LeftMouseButton, 0, FModifierKeysState()));
		const bool Up = Slate.ProcessMouseButtonUpEvent(FPointerEvent(User, 0, Point, Point,
			TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
		Test.TestTrue(TEXT("Virtual Slate input leaves hardware-user focus untouched"), Focus == Slate.GetUserFocusedWidget(0));
		Test.TestTrue(TEXT("The laid-out button handled virtual pointer release"), Up);
		return true;
	}

	FAutomationTestBase& Test;
	const FIntPoint ExpectedViewportSize;
	const double Deadline = FPlatformTime::Seconds() + 70.0;
	const bool bWorld;
	TWeakObjectPtr<UUserWidget> Hud;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
	int32 Step = 0;
	float PreviousLevel = 0.0f;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFeatureShellInputTest, "Rok.Ui.FeatureShell.RenderedInput",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokFeatureShellInputTest::RunTest(const FString&)
{
	FIntPoint Size = FIntPoint::ZeroValue;
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokNoNet"))
		|| !FParse::Value(FCommandLine::Get(), TEXT("ResX="), Size.X)
		|| !FParse::Value(FCommandLine::Get(), TEXT("ResY="), Size.Y)
		|| Size.X <= 0 || Size.Y <= 0)
	{
		AddError(TEXT("Run this test with -RenderOffscreen -RokLocalOnly -RokNoNet and an explicit viewport."));
		return false;
	}
	const bool bWorld = FParse::Param(FCommandLine::Get(), TEXT("RokFeatureShellWorldInput"));
	ADD_LATENT_AUTOMATION_COMMAND(FCheckFeatureShellInput(*this, Size, bWorld));
	return true;
}

#endif
