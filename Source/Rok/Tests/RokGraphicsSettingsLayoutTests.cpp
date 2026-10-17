#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../RokGraphicsSettingsLayout.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGraphicsSettingsLayoutTest, "Rok.Ui.Settings.GraphicsLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokGraphicsSettingsLayoutTest::RunTest(const FString&)
{
    using namespace RokGraphicsSettingsLayout;
    TestEqual(TEXT("2000 MB is low"), RecommendedTier(2000), 1);
    TestEqual(TEXT("2001 MB is medium"), RecommendedTier(2001), 2);
    TestEqual(TEXT("3000 MB is medium"), RecommendedTier(3000), 2);
    TestEqual(TEXT("3001 MB is high"), RecommendedTier(3001), 3);
    TestEqual(TEXT("Integer slider clamps low"), Tier(-2), 1);
    TestEqual(TEXT("Integer slider rounds"), Tier(1.7f), 2);
    TestEqual(TEXT("Integer slider clamps high"), Tier(4), 3);
    auto* Instance = NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone(TEXT("RokGraphicsSettingsLayoutTest"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World); Screen->Initialize(); const auto Slate = Screen->TakeWidget();
    Screen->OpenGeneralSettings();
    auto* Card = Screen->GetWidgetFromName(TEXT("GeneralSettingsGraphicsCard"));
    if (!TestNotNull(TEXT("Source first card exists"), Card)) return false;
    TestTrue(TEXT("Source 1000 by 367 card"), CastChecked<UCanvasPanelSlot>(Card->Slot)->GetSize().Equals(CardSize(), .001));
    for (int32 Row = 0; Row < 2; ++Row)
    {
        const FString Prefix = Row == 0 ? TEXT("GeneralSettingsGraphics") : TEXT("GeneralSettingsFrameRate");
        auto* Slider = Cast<USlider>(Screen->GetWidgetFromName(*(Prefix + TEXT("Slider"))));
        if (!TestNotNull(TEXT("Stable real slider identity"), Slider)) continue;
        TestEqual(TEXT("Source minimum"), Slider->GetMinValue(), 1.f);
        TestEqual(TEXT("Source maximum"), Slider->GetMaxValue(), 3.f);
        TestTrue(TEXT("Discrete pointer input"), Slider->MouseUsesStep);
        auto* Slot = CastChecked<UCanvasPanelSlot>(Slider->Slot);
        TestTrue(TEXT("Chinese source slider center track"), Slot->GetPosition().Equals(SliderPosition(Row), .001));
        TestTrue(TEXT("Handle travels 866 px"), Slot->GetSize().Equals(SliderSize(), .001));
        const TCHAR* Labels[] = { TEXT("低"), TEXT("中"), TEXT("高") };
        for (int32 Level = 0; Level < 3; ++Level)
            TestEqual(TEXT("Source left-to-right scale"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("%sScale_%d"), *Prefix, Level)))->GetText().ToString(), FString(Labels[Level]));
    }
    TestNotNull(TEXT("Actual-device recommendation exists"), Screen->GetWidgetFromName(TEXT("GeneralSettingsGraphicsRecommendation")));
    TestTrue(TEXT("Music has a runtime audio consumer"), Screen->GetWidgetFromName(TEXT("GeneralSettingsMusicSlider"))->GetIsEnabled());
    TestTrue(TEXT("SFX has a runtime audio consumer"), Screen->GetWidgetFromName(TEXT("GeneralSettingsSfxSlider"))->GetIsEnabled());
    return true;
}
#endif
