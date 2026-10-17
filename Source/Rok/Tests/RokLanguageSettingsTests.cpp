#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokLanguageSettingsLayout.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"
#include "Components/CanvasPanelSlot.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLanguageSettingsTest,"Rok.Ui.Settings.LanguageDraft",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokLanguageSettingsTest::RunTest(const FString&)
{
    using namespace RokLanguageSettingsLayout;
    auto* Instance=NewObject<UGameInstance>(GEngine);Instance->InitializeStandalone(TEXT("RokLanguageSettingsTest"));UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT {Instance->Shutdown();GEngine->DestroyWorldContext(World);};
    auto* Screen=NewObject<URokUnityFeatureScreenWidget>(World);Screen->Initialize();const auto Slate=Screen->TakeWidget();
    const FString Culture=FInternationalization::Get().GetCurrentCulture()->GetName();
    Screen->OpenLanguageSettings();Screen->AdvanceWindowAnimationForTesting(1);
    TestEqual(TEXT("Each open starts at applied Chinese preview"),Screen->GetLanguageDraftIdForAudit(),40);
    auto* Frame=Screen->GetWidgetFromName(TEXT("LanguageSettingsSourceFrame"));
    TestTrue(TEXT("TypeMid frame is centered on window, independent of list offset"),CastChecked<UCanvasPanelSlot>(Frame->Slot)->GetPosition().Equals(FramePosition(),.001));
    TestTrue(TEXT("Source frame has exact native size"),CastChecked<UCanvasPanelSlot>(Frame->Slot)->GetSize().Equals(FrameSize(),.001));
    auto* TitleRegion=Screen->GetWidgetFromName(TEXT("LanguageSettingsTitleRegion"));
    TestTrue(TEXT("Source title does not overlap first card"),CastChecked<UCanvasPanelSlot>(TitleRegion->Slot)->GetPosition().Equals(TitlePosition(),.001));
    auto* Arabic=CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("LanguageOptionLabel_1")));
    TestTrue(TEXT("Chinese layout fixes Arabic flow to left without changing glyph text"),Arabic->GetFlowDirectionPreference()==EFlowDirectionPreference::LeftToRight);
    const auto* OptionsProperty=FindFProperty<FStructProperty>(Arabic->GetClass(),TEXT("ShapedTextOptions"));
    if (!TestNotNull(TEXT("Text layout shaping options are reflected"),OptionsProperty)) return false;
    const auto* TextOptions=OptionsProperty->ContainerPtrToValuePtr<FShapedTextOptions>(Arabic);
    TestTrue(TEXT("Text paragraph direction is explicitly LTR, independently of widget flow"),TextOptions->bOverride_TextFlowDirection && TextOptions->TextFlowDirection==ETextFlowDirection::LeftToRight);
    TestFalse(TEXT("Arabic shaping method remains enabled at engine default"),TextOptions->bOverride_TextShapingMethod);
    TestEqual(TEXT("Arabic remains logical text for Slate shaping"),Arabic->GetText().ToString(),FString(TEXT("العربية")));
    auto* ConfirmLabel=CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("LanguageConfirmButtonLabel")));
    TestTrue(TEXT("Confirm inherits source dark-blue label"),ConfirmLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::FromSRGBColor(FColor(10,27,46,255)),.0001f));
    TestEqual(TEXT("Confirm displayed gamma bytes round-trip to source RGB"),ConfirmLabel->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true),FColor(10,27,46,255));
    TestEqual(TEXT("Confirm source 32px font becomes 24pt"),ConfirmLabel->GetFont().Size,24.f);
    auto* ConfirmRegion=Screen->GetWidgetFromName(TEXT("LanguageConfirmLabelRegion"));
    TestTrue(TEXT("Confirm label uses source inner rectangle"),CastChecked<UCanvasPanelSlot>(ConfirmRegion->Slot)->GetPosition().Equals(FVector2D(0,10),.001));
    const FName Buttons[]={TEXT("LanguageOptionEnglishButton"),TEXT("LanguageOptionArabicButton"),TEXT("LanguageOptionChineseButton"),TEXT("LanguageOptionTurkishButton")};
    auto* Original=Screen->GetWidgetFromName(Buttons[0]);
    for(int32 Index: {1,0,3,2})
    {
        auto* Cell=Screen->GetWidgetFromName(*FString::Printf(TEXT("LanguageOptionCell_%d"),Index));
        TestTrue(TEXT("Chinese source grid ordering and negative middle alignment offset"),CastChecked<UCanvasPanelSlot>(Cell->Slot)->GetPosition().Equals(CellPosition(Index),.001));
        CastChecked<UButton>(Screen->GetWidgetFromName(Buttons[Index]))->OnClicked.Broadcast();Screen->FlushPendingActionForTesting();
        TestEqual(TEXT("Radio changes only draft source ID"),Screen->GetLanguageDraftIdForAudit(),SourceId(Index));
        int32 Checked=0;for(int32 I=0;I<4;++I)Checked+=Screen->GetWidgetFromName(*FString::Printf(TEXT("LanguageOptionRadio_%d"),I))->GetVisibility()!=ESlateVisibility::Collapsed;
        TestEqual(TEXT("Exactly one source radio selected"),Checked,1);
        TestEqual(TEXT("Selection preserves widget identity"),Screen->GetWidgetFromName(Buttons[0]),Original);
        TestEqual(TEXT("No locale application"),FInternationalization::Get().GetCurrentCulture()->GetName(),Culture);
        TestEqual(TEXT("Confirm only closes unchanged language"),Screen->GetWidgetFromName(TEXT("LanguageConfirmButton"))->GetIsEnabled(),Index==2);
    }
    CastChecked<UButton>(Screen->GetWidgetFromName(Buttons[0]))->OnClicked.Broadcast();Screen->FlushPendingActionForTesting();
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("LanguageConfirmButton")))->OnClicked.Broadcast();Screen->FlushPendingActionForTesting();
    TestTrue(TEXT("Even synthetic disabled-confirm delegate cannot apply or close different language"),Screen->IsScreenOpen());
    Screen->CloseScreen();Screen->OpenLanguageSettings();Screen->AdvanceWindowAnimationForTesting(1);
    TestEqual(TEXT("Cancel and reopen discard draft"),Screen->GetLanguageDraftIdForAudit(),40);
    CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("LanguageConfirmButton")))->OnClicked.Broadcast();Screen->FlushPendingActionForTesting();Screen->AdvanceWindowAnimationForTesting(1);
    TestFalse(TEXT("Source unchanged-language confirm closes"),Screen->IsScreenOpen());
    return true;
}
#endif
