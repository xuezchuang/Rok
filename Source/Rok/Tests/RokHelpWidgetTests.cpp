#include "../RokHelpSourceTextWidget.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../RokHelpWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHelpWidgetTest,"Rok.Ui.Help.LocalFaq",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokHelpWidgetTest::RunTest(const FString&)
{
    auto* Instance=NewObject<UGameInstance>(GEngine);Instance->InitializeStandalone(TEXT("RokHelpTest"));UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT{Instance->Shutdown();GEngine->DestroyWorldContext(World);};
    auto* Help=NewObject<URokHelpWidget>(World);Help->Initialize();
    if(!TestTrue(TEXT("Source catalog loads"),Help->InitializeHelp()))return false;
    const auto Slate=Help->TakeWidget();
    auto Click=[&](const TCHAR* N){auto* B=Cast<UButton>(Help->GetWidgetFromName(N));if(TestNotNull(N,B))B->OnClicked.Broadcast();};
    TestEqual(TEXT("All source questions loaded"),Help->GetQuestionCount(),70);
    TestEqual(TEXT("Initial source groups collapsed"),Help->GetExpandedGroups().Num(),0);
    Click(TEXT("HelpGroupButton_900005"));Click(TEXT("HelpGroupButton_900004"));
    TestEqual(TEXT("Independent group expansion"),Help->GetExpandedGroups().Num(),2);
    TestEqual(TEXT("Source flat tree: 5 palace + 24 system questions visible"),Help->GetVisibleQuestionIds().Num(),29);
    auto* Scroll=CastChecked<UScrollBox>(Help->GetWidgetFromName(TEXT("HelpListScroll")));const auto ScrollSlate=Scroll->TakeWidget();Scroll->SetScrollOffset(150);
    Click(TEXT("HelpQuestionButton_5001"));TestEqual(TEXT("Stable config ID selected"),Help->GetSelectedQuestionId(),5001);
    auto* Answer=CastChecked<URokHelpSourceTextWidget>(Help->GetWidgetFromName(TEXT("HelpAnswerText")));TestFalse(TEXT("Full local answer present"),Answer->GetText().IsEmpty());
    Click(TEXT("HelpBackButton"));TestEqual(TEXT("Back clears detail identity"),Help->GetSelectedQuestionId(),0);
    TestEqual(TEXT("Back preserves both group states"),Help->GetExpandedGroups().Num(),2);
    TestEqual(TEXT("Back preserves actual list object"),Help->GetWidgetFromName(TEXT("HelpListScroll")),static_cast<UWidget*>(Scroll));
    TestEqual(TEXT("Back preserves scroll"),Help->GetListScrollOffset(),150.f);
    Click(TEXT("HelpGroupButton_900005"));TestEqual(TEXT("Other group remains expanded"),Help->GetVisibleQuestionIds().Num(),24);
    Click(TEXT("HelpCustomerServiceButton"));TestNotNull(TEXT("Unavailable SDK explicitly explained"),Help->GetWidgetFromName(TEXT("HelpCustomerServiceBoundary")));
    bool Closed=false;Help->OnCloseRequested.AddLambda([&Closed]{Closed=true;});Click(TEXT("HelpCloseButton"));TestTrue(TEXT("Close delegates to host T1 lifecycle"),Closed);
    Click(TEXT("HelpGroupButton_900005"));TestEqual(TEXT("Synthetic child actions also blocked during host close"),Help->GetExpandedGroups().Num(),1);
    TestTrue(TEXT("Reopen reloads complete source"),Help->InitializeHelp());TestEqual(TEXT("Reopen resets expanded groups"),Help->GetExpandedGroups().Num(),0);TestEqual(TEXT("Reopen resets detail"),Help->GetSelectedQuestionId(),0);
    return true;
}
#endif
