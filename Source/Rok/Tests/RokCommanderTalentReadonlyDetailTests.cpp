#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRokCommanderTalentReadonlyDetailTest,
    "Rok.City.Ui.CommanderTalentReadonlyDetail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderTalentReadonlyDetailTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->InitializeStandalone(TEXT("RokCommanderTalentReadonlyDetailTest"));
    URokUnityFeatureScreenWidget* Widget =
        NewObject<URokUnityFeatureScreenWidget>(GameInstance->GetWorld());
    if (!TestNotNull(TEXT("Feature widget"), Widget)
        || !TestTrue(TEXT("Feature widget initializes"), Widget->Initialize()))
    {
        GameInstance->Shutdown();
        return false;
    }
    Widget->TakeWidget();
    Widget->OpenCommander();
    TestEqual(TEXT("Default unowned commander has no selected detail"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    if (UButton* Unsupported = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderTalentLabel_0"))))
    {
        Unsupported->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
    }
    TestEqual(TEXT("Unsupported commander cannot open talent detail"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    TestNull(TEXT("Unsupported commander has no detail card"),
        Widget->GetWidgetFromName(TEXT("CommanderTalentDetailFrame")));

    UButton* FirstHero = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderHeadAction_0")));
    if (!TestNotNull(TEXT("First preview hero button"), FirstHero))
    {
        GameInstance->Shutdown();
        return false;
    }
    FirstHero->OnClicked.Broadcast();
    Widget->FlushPendingActionForTesting();
    const int32 HeroId = Widget->GetSelectedCommanderIdForAudit();
    const TArray<int32> ExplicitPreviewHeroes = {1006, 1007, 1009, 1010, 1012};
    TestTrue(TEXT("Only one of five local bootstrap heroes is previewed"),
        ExplicitPreviewHeroes.Contains(HeroId));
    URokUnityGameplayCatalogSubsystem* Catalog =
        GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
    URokUnityRuntimeStateSubsystem* Runtime =
        GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    FRokUnityCatalogHero Hero;
    if (!TestNotNull(TEXT("Gameplay catalog"), Catalog)
        || !TestNotNull(TEXT("Runtime bootstrap"), Runtime)
        || !TestTrue(TEXT("Selected hero has catalog config"),
            Catalog->GetHero(HeroId, Hero)))
    {
        GameInstance->Shutdown();
        return false;
    }
    const TArray<FRokUnityRuntimeHeroState> BeforeHeroes = Runtime->GetStarterHeroes();
    TestEqual(TEXT("Three source talent IDs"), Hero.TalentIds.Num(), 3);
    for (int32 Index = 0; Index < 3 && Hero.TalentIds.IsValidIndex(Index); ++Index)
    {
        const FName LabelName(*FString::Printf(TEXT("CommanderTalentLabel_%d"), Index));
        UButton* Label = Cast<UButton>(Widget->GetWidgetFromName(LabelName));
        if (!TestNotNull(TEXT("Clickable talent label"), Label)) continue;
        Label->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
        const int32 TalentId = Hero.TalentIds[Index];
        TestEqual(TEXT("Selected exact source talent ID"),
            Widget->GetSelectedCommanderTalentIdForAudit(), TalentId);
        FRokUnityCatalogHeroTalent Talent;
        if (!TestTrue(TEXT("Talent config exists"), Catalog->GetHeroTalent(TalentId, Talent))) continue;
        UTextBlock* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(
            TEXT("CommanderTalentDetailTitle")));
        UTextBlock* Tips = Cast<UTextBlock>(Widget->GetWidgetFromName(
            TEXT("CommanderTalentDetailTips")));
        if (TestNotNull(TEXT("Localized detail title"), Title))
        {
            TestEqual(TEXT("Title equals catalog localization"), Title->GetText().ToString(),
                !Talent.DisplayNameZhCn.IsEmpty() ? Talent.DisplayNameZhCn : Talent.DisplayNameEn);
            const FLinearColor SourcePopupText(0.13725491f, 0.13725491f, 0.1254902f, 1.0f);
            TestTrue(TEXT("Popup title uses Unity dark text RGBA"),
                Title->GetColorAndOpacity().GetSpecifiedColor().Equals(SourcePopupText));
            TestTrue(TEXT("Popup title has no fabricated dark shadow"),
                FMath::IsNearlyZero(Title->GetShadowColorAndOpacity().A));
        }
        if (TestNotNull(TEXT("Localized DisplayTips"), Tips))
        {
            TestEqual(TEXT("Tips equal catalog localization"), Tips->GetText().ToString(),
                !Talent.DisplayTipsZhCn.IsEmpty() ? Talent.DisplayTipsZhCn : Talent.DisplayTipsEn);
            const FLinearColor SourcePopupText(0.13725491f, 0.13725491f, 0.1254902f, 1.0f);
            TestTrue(TEXT("Popup tips use Unity dark text RGBA"),
                Tips->GetColorAndOpacity().GetSpecifiedColor().Equals(SourcePopupText));
            TestTrue(TEXT("Popup tips have no fabricated dark shadow"),
                FMath::IsNearlyZero(Tips->GetShadowColorAndOpacity().A));
        }
        UImage* Center = Cast<UImage>(Widget->GetWidgetFromName(
            TEXT("CommanderTalentDetailFrame_Center")));
        if (TestNotNull(TEXT("Source popup center patch is rendered"), Center))
        {
            UCanvasPanelSlot* CenterSlot = Cast<UCanvasPanelSlot>(Center->Slot);
            if (TestNotNull(TEXT("Center patch has canvas geometry"), CenterSlot))
            {
                TestTrue(TEXT("Center patch has positive width and height"),
                    CenterSlot->GetSize().X > 0.0f && CenterSlot->GetSize().Y > 0.0f);
            }
        }
        TestNotNull(TEXT("Detail has a close button"), Cast<UButton>(
            Widget->GetWidgetFromName(TEXT("CommanderTalentDetailClose"))));
    }
    if (UButton* Close = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderTalentDetailClose"))))
    {
        Close->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
    }
    TestEqual(TEXT("Closing clears local detail selection"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    TestNull(TEXT("Closed detail is removed"),
        Widget->GetWidgetFromName(TEXT("CommanderTalentDetailFrame")));
    Widget->OpenCommander();
    TestEqual(TEXT("Reopening commander keeps detail closed"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    const TArray<FRokUnityRuntimeHeroState> AfterHeroes = Runtime->GetStarterHeroes();
    TestEqual(TEXT("Local bootstrap hero count unchanged"), AfterHeroes.Num(), BeforeHeroes.Num());
    for (int32 Index = 0; Index < BeforeHeroes.Num() && AfterHeroes.IsValidIndex(Index); ++Index)
    {
        TestEqual(TEXT("Hero identity unchanged"), AfterHeroes[Index].HeroId, BeforeHeroes[Index].HeroId);
        TestEqual(TEXT("Hero level unchanged"), AfterHeroes[Index].Level, BeforeHeroes[Index].Level);
        TestEqual(TEXT("Hero star unchanged"), AfterHeroes[Index].Star, BeforeHeroes[Index].Star);
        TestEqual(TEXT("Hero score unchanged"), AfterHeroes[Index].Score, BeforeHeroes[Index].Score);
    }
    GameInstance->Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRokCommanderTalentMissingCatalogTest,
    "Rok.City.Ui.CommanderTalentMissingCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderTalentMissingCatalogTest::RunTest(const FString& Parameters)
{
    URokUnityFeatureScreenWidget* Widget =
        NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
    if (!TestNotNull(TEXT("Transient feature widget"), Widget)
        || !TestTrue(TEXT("Transient feature widget initializes"), Widget->Initialize()))
    {
        return false;
    }
    Widget->TakeWidget();
    Widget->OpenCommander();
    UButton* Label = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderTalentLabel_0")));
    if (TestNotNull(TEXT("Unavailable label remains clickable"), Label))
    {
        Label->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
    }
    TestEqual(TEXT("Missing catalog preserves closed state"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    TestNull(TEXT("Missing catalog does not fabricate detail"),
        Widget->GetWidgetFromName(TEXT("CommanderTalentDetailFrame")));
    return true;
}

#endif
