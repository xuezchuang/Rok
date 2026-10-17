#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/Guid.h"
#include "UObject/Package.h"
#include "../RokCommanderSkillReadonlyDetail.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRokCommanderSkillConfigFormattingTest,
    "Rok.City.Ui.CommanderSkillConfigFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderSkillConfigFormattingTest::RunTest(const FString& Parameters)
{
    FRokUnityCatalogHeroSkill Skill;
    Skill.Id = 100601;
    Skill.DisplayNameZhCn = TEXT("配置技能");
    Skill.DisplayTypeZhCn = TEXT("主动技能");
    Skill.DisplayOpenZhCn = TEXT("统帅升至1星");
    Skill.DisplayMessageZhCn = TEXT("攻击提升{0}，伤害提升{1}。");
    Skill.Anger = 1000;
    Skill.LevelEffects.SetNum(4);
    Skill.LevelEffects[0].LocaleId = 165021;
    Skill.LevelEffects[0].DisplayTextZhCn = TEXT("攻击提升");
    Skill.LevelEffects[0].Data = TEXT("10%|12%|14%|16%|20%");
    Skill.LevelEffects[1].LocaleId = 165000;
    Skill.LevelEffects[1].DisplayTextZhCn = TEXT("伤害提升");
    Skill.LevelEffects[1].Data = TEXT("10%|14%|18%|24%|30%");
    FRokCommanderSkillReadonlyDetail Detail;
    TestTrue(TEXT("Four source groups build a config-only detail"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    TestEqual(TEXT("Localized skill name"), Detail.Title, FString(TEXT("配置技能")));
    TestTrue(TEXT("Level-one message substitutes source group values"),
        Detail.Body.Contains(TEXT("攻击提升10%，伤害提升10%")));
    TestTrue(TEXT("Fifth configured values are displayed"),
        Detail.Body.Contains(TEXT("5:20%")) && Detail.Body.Contains(TEXT("5:30%")));
    TestTrue(TEXT("Type, rage and unlock configuration are present"),
        Detail.Body.Contains(TEXT("主动技能")) && Detail.Body.Contains(TEXT("怒气消耗：1000"))
        && Detail.Body.Contains(TEXT("统帅升至1星")));
    TestTrue(TEXT("No account-current skill level is asserted"),
        Detail.Body.Contains(TEXT("非账户当前等级")));
    Skill.Anger = 0;
    TestTrue(TEXT("Zero-rage source config still formats"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    TestFalse(TEXT("Unity omits the zero-rage line"),
        Detail.Body.Contains(TEXT("怒气消耗")));
    Skill.Anger = 1000;
    TestTrue(TEXT("Positive-rage source config still formats"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    TestTrue(TEXT("Positive rage line returns exactly once"),
        Detail.Body.Contains(TEXT("怒气消耗：1000\n"))
        && Detail.Body.Find(TEXT("怒气消耗"), ESearchCase::CaseSensitive,
            ESearchDir::FromEnd) == Detail.Body.Find(TEXT("怒气消耗")));

    Skill.LevelEffects[1].Data = TEXT("10%|14%|18%");
    TestFalse(TEXT("Malformed five-level config rejected"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    Skill.LevelEffects[1].Data = TEXT("10%|14%|18%|24%|30%");
    Skill.LevelEffects[1].LocaleId = 0;
    TestFalse(TEXT("Data without locale identity rejected"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    Skill.LevelEffects[1].LocaleId = 165000;
    Skill.DisplayMessageZhCn = TEXT("攻击提升{3}");
    TestFalse(TEXT("Placeholder without source group rejected"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    Skill.DisplayMessageZhCn = TEXT("无占位符效果");
    Skill.Type = 4;
    Skill.Open = 5;
    TestTrue(TEXT("Awakening config remains displayable"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    TestTrue(TEXT("Awakening is not asserted as account state"),
        Detail.Body.Contains(TEXT("未验证账户觉醒状态")));
    Skill.LevelEffects.Empty();
    TestFalse(TEXT("Not-four source groups rejected"),
        RokCommanderSkillReadonlyDetail::Build(Skill, Detail));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRokCommanderSkillReadonlySelectionTest,
    "Rok.City.Ui.CommanderSkillReadonlySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCommanderSkillReadonlySelectionTest::RunTest(const FString& Parameters)
{
    UPackage* WorldPackage = CreatePackage(*FString::Printf(
        TEXT("/Temp/CommanderSkillReadonly_%s/L_RokUnityGameAppCity"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    WorldPackage->SetFlags(RF_Transient);
    UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
    GameInstance->InitializeStandalone(TEXT("RokCommanderSkillReadonlySelectionTest"), WorldPackage);
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
    TestEqual(TEXT("Unsupported default hero has no detail"),
        Widget->GetSelectedCommanderSkillDetailIdForAudit(), INDEX_NONE);
    UButton* FirstHero = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderHeadAction_0")));
    if (!TestNotNull(TEXT("Local preview hero button"), FirstHero))
    {
        GameInstance->Shutdown();
        return false;
    }
    FirstHero->OnClicked.Broadcast();
    Widget->FlushPendingActionForTesting();
    const int32 HeroId = Widget->GetSelectedCommanderIdForAudit();
    const TArray<int32> ExplicitHeroes = {1006, 1007, 1009, 1010, 1012};
    TestTrue(TEXT("Selected hero belongs to explicit local preview"),
        ExplicitHeroes.Contains(HeroId));
    URokUnityGameplayCatalogSubsystem* Catalog =
        GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
    URokUnityRuntimeStateSubsystem* Runtime =
        GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    FRokUnityCatalogHero Hero;
    if (!TestNotNull(TEXT("Catalog"), Catalog)
        || !TestNotNull(TEXT("Runtime bootstrap"), Runtime)
        || !TestTrue(TEXT("Hero catalog row"), Catalog->GetHero(HeroId, Hero)))
    {
        GameInstance->Shutdown();
        return false;
    }
    const TArray<FRokUnityRuntimeHeroState> BeforeHeroes = Runtime->GetStarterHeroes();
    for (int32 Index = 0; Index < 2; ++Index)
    {
        UButton* SkillButton = Cast<UButton>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("CommanderSkillAction_%d"), Index))));
        if (!TestNotNull(TEXT("Skill action"), SkillButton)) continue;
        SkillButton->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
        const int32 SkillId = Hero.SkillIds.IsValidIndex(Index) ? Hero.SkillIds[Index] : INDEX_NONE;
        TestEqual(TEXT("Clicked exact source skill ID"),
            Widget->GetSelectedCommanderSkillDetailIdForAudit(), SkillId);
        FRokUnityCatalogHeroSkill Skill;
        FRokCommanderSkillReadonlyDetail Expected;
        if (TestTrue(TEXT("Skill config"), Catalog->GetHeroSkill(SkillId, Skill))
            && TestTrue(TEXT("Config-only format"),
                RokCommanderSkillReadonlyDetail::Build(Skill, Expected)))
        {
            UTextBlock* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(
                TEXT("CommanderSkillDetailTitle")));
            UTextBlock* Body = Cast<UTextBlock>(Widget->GetWidgetFromName(
                TEXT("CommanderSkillDetailBody")));
            if (TestNotNull(TEXT("Popup title"), Title))
            {
                TestEqual(TEXT("Title equals config"), Title->GetText().ToString(), Expected.Title);
                TestTrue(TEXT("Skill HelpTip title uses source dark RGBA"),
                    Title->GetColorAndOpacity().GetSpecifiedColor().Equals(
                        FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f)));
                TestTrue(TEXT("Skill HelpTip title has no fabricated shadow"),
                    FMath::IsNearlyZero(Title->GetShadowColorAndOpacity().A));
            }
            if (TestNotNull(TEXT("Popup body"), Body))
            {
                TestEqual(TEXT("Body equals config"), Body->GetText().ToString(), Expected.Body);
                TestTrue(TEXT("Skill HelpTip body uses source dark RGBA"),
                    Body->GetColorAndOpacity().GetSpecifiedColor().Equals(
                        FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f)));
                TestTrue(TEXT("Skill HelpTip body has no fabricated shadow"),
                    FMath::IsNearlyZero(Body->GetShadowColorAndOpacity().A));
            }
            UImage* Center = Cast<UImage>(Widget->GetWidgetFromName(
                TEXT("CommanderSkillDetailFrame_Center")));
            if (TestNotNull(TEXT("Source atlas center is rendered"), Center))
            {
                UCanvasPanelSlot* CenterSlot = Cast<UCanvasPanelSlot>(Center->Slot);
                if (TestNotNull(TEXT("Center patch geometry"), CenterSlot))
                    TestTrue(TEXT("Nine-slice center remains positive"),
                        CenterSlot->GetSize().X > 0.0f && CenterSlot->GetSize().Y > 0.0f);
            }
            TestNotNull(TEXT("Long source detail is scrollable"),
                Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("CommanderSkillDetailScroll"))));
        }
    }
    if (UButton* Close = Cast<UButton>(Widget->GetWidgetFromName(
        TEXT("CommanderSkillDetailClose"))))
    {
        Close->OnClicked.Broadcast();
        Widget->FlushPendingActionForTesting();
    }
    TestEqual(TEXT("Close clears only local skill detail"),
        Widget->GetSelectedCommanderSkillDetailIdForAudit(), INDEX_NONE);
    TestNull(TEXT("Closed skill detail removed"),
        Widget->GetWidgetFromName(TEXT("CommanderSkillDetailFrame")));
    TestEqual(TEXT("Talent detail remains closed"),
        Widget->GetSelectedCommanderTalentIdForAudit(), INDEX_NONE);
    const TArray<FRokUnityRuntimeHeroState> AfterHeroes = Runtime->GetStarterHeroes();
    TestEqual(TEXT("Bootstrap hero count unchanged"), AfterHeroes.Num(), BeforeHeroes.Num());
    for (int32 Index = 0; Index < BeforeHeroes.Num() && AfterHeroes.IsValidIndex(Index); ++Index)
    {
        TestEqual(TEXT("Hero ID unchanged"), AfterHeroes[Index].HeroId, BeforeHeroes[Index].HeroId);
        TestEqual(TEXT("Hero level unchanged"), AfterHeroes[Index].Level, BeforeHeroes[Index].Level);
        TestEqual(TEXT("Hero star unchanged"), AfterHeroes[Index].Star, BeforeHeroes[Index].Star);
        TestEqual(TEXT("Hero score unchanged"), AfterHeroes[Index].Score, BeforeHeroes[Index].Score);
    }
    GameInstance->Shutdown();
    return true;
}

#endif
