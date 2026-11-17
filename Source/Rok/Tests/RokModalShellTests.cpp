#if WITH_DEV_AUTOMATION_TESTS

#include "../RokMainHudWidget.h"
#include "../RokUiSpec.h"
#include "../RokUiWidgets.h"
#include "../RokUnityModalWidget.h"
#include "../RokWorldMapHudWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace
{
class FCheckModalShellInput final : public IAutomationLatentCommand
{
public:
	FCheckModalShellInput(FAutomationTestBase& InTest, FIntPoint InResolution, bool bInWorldMap)
		: Test(InTest), Resolution(InResolution), bWorldMap(bInWorldMap)
	{
		FParse::Value(FCommandLine::Get(), TEXT("RokModalShellSpec="), SpecPath);
		FParse::Value(FCommandLine::Get(), TEXT("RokModalShellScreenshot="), ScreenshotPath);
	}

	bool Update() override
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			Test.AddError(FString::Printf(TEXT("Modal shell input timed out at step %d (%dx%d, world=%d)"),
				Step, Resolution.X, Resolution.Y, bWorldMap));
			return true;
		}
		if (!Hud.IsValid())
		{
			Hud = bWorldMap ? FindViewportHud<URokWorldMapHudWidget>() : FindViewportHud<URokMainHudWidget>();
			if (!Hud.IsValid())
			{
				return false;
			}
			Modal = Cast<URokUnityModalWidget>(RokUiFind::Find(Hud, bWorldMap
				? TEXT("WorldMapUnityModalWidget") : TEXT("UnityModalWidget")));
		}
		const UGameViewportClient* Viewport = Hud->GetWorld() ? Hud->GetWorld()->GetGameViewport() : nullptr;
		const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
		if (!Window || !Viewport->Viewport || Viewport->Viewport->GetSizeXY() != Resolution)
		{
			return false;
		}
		const auto NativeWindow = Window->GetNativeWindow();
		if (!Test.TestTrue(TEXT("Virtual input restricted to the OS-handle-free offscreen window"),
			FSlateApplication::IsInitialized() && NativeWindow && NativeWindow->IsRenderingOffScreen()
			&& NativeWindow->GetOSWindowHandle() == nullptr))
		{
			return true;
		}
		if (!Test.TestNotNull(TEXT("Real HUD modal host"), Modal.Get()))
		{
			return true;
		}
		if (!VirtualUser)
		{
			VirtualUser = FSlateApplication::Get().FindOrCreateVirtualUser(9);
		}

		const FName ProfileButton(bWorldMap ? TEXT("WorldMapPlayerProfileHitButton") : TEXT("PlayerProfileHitButton"));
		const auto* Mask = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ModalDismissMask")));
		const auto* Blocker = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker")));
		if (!Test.TestTrue(TEXT("Both input layers retain direct legacy names"), Mask && Blocker))
		{
			return true;
		}
		const FGeometry MaskGeometry = Mask->GetCachedGeometry();
		const FVector2D Outside = MaskGeometry.LocalToAbsolute(FVector2D(8, MaskGeometry.GetLocalSize().Y * 0.65));
		const FRokUiWindowAnimation& Animation = Modal->GetWindowAnimationForTesting();
		switch (Step)
		{
		case 0:
		{
			const FName ExpectedClass = URokUiSettings::IsLegacyForced()
				? URokUnityModalWidget::StaticClass()->GetFName() : FName(TEXT("WBP_RokModalShell_C"));
			if (!Test.TestEqual(TEXT("Expected production shell class selected"), Modal->GetClass()->GetFName(), ExpectedClass))
			{
				return true;
			}
			if (!SpecPath.IsEmpty() && !bSpecExported)
			{
				const auto* Root = Cast<UCanvasPanel>(Modal->GetRootWidget());
				const auto* Mode = Modal->GetModeCanvas();
				FString Error;
				if (!Test.TestTrue(TEXT("Export the idle native shell with no mode children"), URokUiSettings::IsLegacyForced()
					&& Root && Mode && Mode->GetChildrenCount() == 0
					&& RokUiSpec::ExportChildren(Root, SpecPath, TEXT("ModalShell"), Error)))
				{
					Test.AddError(Error);
					return true;
				}
				bSpecExported = true;
			}
			if (!ClickButton(*Window, RokUiFind::Find(Hud, ProfileButton)))
			{
				return false;
			}
			break;
		}
		case 1:
			if (!Test.TestTrue(TEXT("The avatar button opened the animated profile"), Modal->IsModalOpen()
				&& Animation.IsAnimating() && !Animation.IsClosing()))
			{
				return true;
			}
			if (!InHitPath(*Window, Blocker, Outside))
			{
				return false;
			}
			ClickAt(Outside);
			Test.TestFalse(TEXT("Outside pointer input on entry does not dismiss"), Animation.IsClosing());
			break;
		case 2:
			if (Animation.IsAnimating())
			{
				return false;
			}
			CheckSettledGeometry(*Mask, *Blocker);
			Test.TestTrue(TEXT("Outside point reaches the original dismiss button"), InHitPath(*Window, Mask, Outside));
			if (!ScreenshotPath.IsEmpty())
			{
				FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false);
			}
			break;
		case 3:
			if ((!ScreenshotPath.IsEmpty() && IFileManager::Get().FileSize(*ScreenshotPath) <= 0)
				|| !ClickButton(*Window, RokUiFind::Find(Modal, TEXT("ProfileRenameButton"))))
			{
				return false;
			}
			break;
		case 4:
			if (Animation.IsAnimating())
			{
				return false;
			}
			if (!Test.TestNotNull(TEXT("In-window pointer input opened the rename child mode"),
				RokUiFind::Find(Modal, TEXT("ChangeNameInput"))))
			{
				return true;
			}
			if (!InHitPath(*Window, Mask, Outside))
			{
				return false;
			}
			ClickAt(Outside);
			Test.TestTrue(TEXT("Outside input starts the child exit and keeps the host live"), Animation.IsClosing() && Modal->IsModalOpen());
			break;
		case 5:
			if (!Test.TestTrue(TEXT("Exit click cannot leak through while the animation is live"),
				Animation.IsClosing() && Modal->IsVisible() && InHitPath(*Window, Blocker, Outside)))
			{
				return true;
			}
			ClickAt(Outside);
			break;
		case 6:
			if (Animation.IsAnimating())
			{
				return false;
			}
			if (!Test.TestNotNull(TEXT("Closing rename returns to the live profile"), RokUiFind::Find(Modal, TEXT("ProfilePlayerName"))))
			{
				return true;
			}
			if (!InHitPath(*Window, Mask, Outside))
			{
				return false;
			}
			ClickAt(Outside);
			break;
		case 7:
			if (Modal->IsModalOpen())
			{
				return false;
			}
			Test.TestEqual(TEXT("Finished exit collapses the full-screen host"), Modal->GetVisibility(), ESlateVisibility::Collapsed);
			Test.TestNull(TEXT("Finished exit releases old profile content"), RokUiFind::Find(Modal, TEXT("ProfilePlayerName")));
			if (!ClickButton(*Window, RokUiFind::Find(Hud, ProfileButton)))
			{
				return false;
			}
			Test.TestTrue(TEXT("Underlay avatar accepts input after the full modal exit"), Modal->IsModalOpen());
			Modal->CloseModal();
			return true;
		default:
			return true;
		}
		++Step;
		return false;
	}

private:
	template<typename WidgetType>
	static UUserWidget* FindViewportHud()
	{
		for (TObjectIterator<WidgetType> It; It; ++It)
		{
			if (It->IsInViewport())
			{
				return *It;
			}
		}
		return nullptr;
	}

	void CheckSettledGeometry(const UButton& Mask, const UButton& Blocker)
	{
		const auto* Mode = Modal->GetModeCanvas();
		const auto* Design = Cast<USizeBox>(Modal->GetWidgetFromName(TEXT("ModalExpandedDesignSize")));
		const auto* Dimmer = Cast<UBorder>(Modal->GetWidgetFromName(TEXT("ModalDimmer")));
		const FGeometry RootGeometry = Modal->GetRootWidget()->GetCachedGeometry();
		const double Aspect = RootGeometry.GetLocalSize().X / RootGeometry.GetLocalSize().Y;
		const FVector2D Expanded = Aspect >= 1440.0 / 810.0 ? FVector2D(810.0 * Aspect, 810.0) : FVector2D(1440.0, 1440.0 / Aspect);
		Test.TestTrue(TEXT("Expanded design follows the actual viewport"), Design
			&& FVector2D(Design->GetWidthOverride(), Design->GetHeightOverride()).Equals(Expanded, 1.0));
		Test.TestTrue(TEXT("Dimmer style and full viewport mask"), Dimmer && Dimmer->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& FMath::IsNearlyEqual(Dimmer->GetBrushColor().A, 0.72f) && FMath::IsNearlyEqual(Mask.GetRenderOpacity(), 1.0f)
			&& Mask.GetRenderTransform().Scale.Equals(FVector2D(1.0))
			&& Mask.GetCachedGeometry().GetLocalSize().Equals(RootGeometry.GetLocalSize(), 0.5));
		Test.TestEqual(TEXT("Settled input blocker collapsed"), Blocker.GetVisibility(), ESlateVisibility::Collapsed);
		if (Test.TestNotNull(TEXT("Native mode API returns the authored content canvas"), Mode))
		{
			const FGeometry ModeGeometry = Mode->GetCachedGeometry();
			Test.TestTrue(TEXT("Fixed 1440x810 mode canvas stays centred and its descendants are hit-testable"),
				ModeGeometry.GetLocalSize().Equals(FVector2D(1440.0, 810.0), 0.5)
				&& ModeGeometry.LocalToAbsolute(ModeGeometry.GetLocalSize() * 0.5).Equals(
					RootGeometry.LocalToAbsolute(RootGeometry.GetLocalSize() * 0.5), 1.0)
				&& Mode->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
		}
	}

	bool InHitPath(const SWindow& Window, const UWidget* Target, const FVector2D& Point) const
	{
		const auto Cached = Target ? Target->GetCachedWidget() : nullptr;
		FSlateApplication& Slate = FSlateApplication::Get();
		const FWidgetPath Path = Slate.LocateWindowUnderMouse(Point, Slate.GetInteractiveTopLevelWindows(),
			false, VirtualUser->GetUserIndex());
		return Path.IsValid() && &Path.GetWindow().Get() == &Window && Cached && Path.ContainsWidget(Cached.Get());
	}

	void ClickAt(const FVector2D& Point)
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const auto Focus = Slate.GetUserFocusedWidget(0);
		const int32 User = VirtualUser->GetUserIndex();
		Slate.ProcessMouseMoveEvent(FPointerEvent(User, 0, Point, Point, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
		Slate.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(), FPointerEvent(User, 0, Point, Point,
			{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState()));
		const bool bHandled = Slate.ProcessMouseButtonUpEvent(FPointerEvent(User, 0, Point, Point,
			TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
		Test.TestTrue(TEXT("Target layer handled the virtual pointer release"), bHandled);
		Test.TestTrue(TEXT("Virtual input preserves hardware-user focus"), Focus == Slate.GetUserFocusedWidget(0));
	}

	bool ClickButton(const SWindow& Window, const UWidget* Widget)
	{
		const auto* Button = Cast<UButton>(Widget);
		if (!Button || !Button->IsVisible() || !Button->GetIsEnabled())
		{
			return false;
		}
		const FGeometry Geometry = Button->GetCachedGeometry();
		const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5);
		if (Geometry.GetLocalSize().IsNearlyZero() || !InHitPath(Window, Button, Center))
		{
			return false;
		}
		ClickAt(Center);
		return true;
	}

	FAutomationTestBase& Test;
	const FIntPoint Resolution;
	const bool bWorldMap;
	const double Deadline = FPlatformTime::Seconds() + 60.0;
	TWeakObjectPtr<UUserWidget> Hud;
	TWeakObjectPtr<URokUnityModalWidget> Modal;
	TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
	FString SpecPath;
	FString ScreenshotPath;
	int32 Step = 0;
	bool bSpecExported = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokModalShellInputTest, "Rok.Ui.ModalShell.RenderedInput",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokModalShellInputTest::RunTest(const FString&)
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FIntPoint Resolution = FIntPoint::ZeroValue;
	if (!TestTrue(TEXT("Modal shell input test is offscreen and offline at an explicit resolution"),
		FParse::Param(CommandLine, TEXT("RokLocalOnly")) && FParse::Param(CommandLine, TEXT("RokNoNet"))
		&& FParse::Param(CommandLine, TEXT("RenderOffscreen")) && FParse::Value(CommandLine, TEXT("ResX="), Resolution.X)
		&& FParse::Value(CommandLine, TEXT("ResY="), Resolution.Y) && Resolution.X > 0 && Resolution.Y > 0))
	{
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCheckModalShellInput(*this, Resolution, FParse::Param(CommandLine, TEXT("RokModalShellWorld"))));
	return true;
}

#endif
