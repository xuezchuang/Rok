#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/MemoryWriter.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokGameNetSubsystem.h"
#include "../RokResearchScreenWidget.h"
#include "../RokServerCitySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokResearchWidgetTest, "Rok.Net.Research.WidgetState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokResearchWidgetTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(TEXT("RokResearchWidgetTest"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    if (!TestNotNull(TEXT("Fixture world"), World)) return false;
    auto* Net = Instance->GetSubsystem<URokGameNetSubsystem>();
    if (!TestNotNull(TEXT("Network subsystem"), Net)) return false;
    FString Error;
    if (!TestTrue(TEXT("Schema loads"), Net->LoadSchema(Error))) return false;
    Net->State = ERokGameNetState::RoleReady;
    auto Role = MakeShared<FJsonObject>();
    auto Queue = MakeShared<FJsonObject>();
    Role->SetArrayField(TEXT("technologies"), {});
    Role->SetObjectField(TEXT("technologyQueue"), Queue);
    for (const TCHAR* Field : {TEXT("food"), TEXT("wood"), TEXT("stone"), TEXT("gold"), TEXT("denar")})
        Role->SetNumberField(Field, 10000);
    auto* City = World->GetSubsystem<URokServerCitySubsystem>();
    if (!TestNotNull(TEXT("Server city fixture"), City)) return false;
    FRokServerBuilding Academy; Academy.Index = 27; Academy.Type = 13; Academy.Level = 2; Academy.FinishTime = -1;
    City->AppliedBuildings.Add(Academy);
    auto Push = [&]() {
        auto Body = MakeShared<FJsonObject>(); Body->SetObjectField(TEXT("roleInfo"), Role);
        Net->MergePush(*Net->Schema.FindProtocol(TEXT("Role_RoleInfo")), Body);
    };
    Push();
    auto* Host = NewObject<URokUnityFeatureScreenWidget>(World);
    if (!TestTrue(TEXT("Feature screen initializes"), Host->Initialize())) return false;
    Host->TakeWidget();
    Host->OpenResearch();
    auto* Screen = Host->GetResearchScreenForAudit();
    if (!TestNotNull(TEXT("Normal entry opens the research Widget Blueprint"), Screen)) return false;
    TestTrue(TEXT("Research screen is an authored Blueprint class"), Screen->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
    const auto* Detail = Screen->DetailView.Get();
    TestTrue(TEXT("Detail is an authored Blueprint class"), Detail && Detail->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
    TestEqual(TEXT("All catalog nodes have Blueprint cards"), Screen->NodeLevels.Num(), 71);
    auto Click = [this, Screen](const TCHAR* Name) {
        auto* Button = Cast<UButton>(Screen->GetWidgetFromName(Name));
        if (!TestNotNull(Name, Button)) return;
        Button->OnClicked.Broadcast();
        const int32 Action = Screen->PendingActionId;
        Screen->PendingActionId = INDEX_NONE;
        if (TestTrue(TEXT("Blueprint button is wired to C++"), Action != INDEX_NONE))
            Screen->ExecuteIndexedAction(Action);
    };
    Click(TEXT("ResearchMilitaryTab"));
    TestEqual(TEXT("Military tab switches the Designer-authored page"), Screen->TreePages->GetActiveWidgetIndex(), 1);
    Click(TEXT("ResearchEconomyTab"));
    TestEqual(TEXT("Economy tab switches back"), Screen->TreePages->GetActiveWidgetIndex(), 0);
    auto Text = [Screen](const TCHAR* Name) {
        const auto* Label = Cast<UTextBlock>(Screen->GetWidgetFromName(Name));
        return Label ? Label->GetText().ToString() : FString();
    };
    TestNull(TEXT("Tree has no extra account-state notice"), Screen->GetWidgetFromName(TEXT("ResearchAccountStateNotice")));
    TestEqual(TEXT("Known empty technology collection is zero"), Text(TEXT("ResearchNodeLevel_101")), FString(TEXT("0/1")));
    Click(TEXT("ResearchNodeHit_101"));
    TestTrue(TEXT("Node opens the detail blueprint"), Screen->IsDetailVisible());
    Click(TEXT("ResearchDetailMore"));
    TestTrue(TEXT("More opens the power page"), Screen->IsPowerPageForAudit());
    Click(TEXT("ResearchPowerBack"));
    TestFalse(TEXT("Back returns to the research details"), Screen->IsPowerPageForAudit());
    TestEqual(TEXT("Unity current level wording"), Text(TEXT("ResearchDetailLevel")), FString(TEXT("等级0")));
    TestEqual(TEXT("Unity next level"), Text(TEXT("ResearchDetailLevelNextValue")), FString(TEXT("1")));
    auto* LevelArrow = Cast<UImage>(Screen->GetWidgetFromName(TEXT("ResearchDetailLevelArrow")));
    TestTrue(TEXT("Level arrow uses Unity atlas sprite"), LevelArrow && LevelArrow->GetVisibility() == ESlateVisibility::HitTestInvisible && LevelArrow->GetBrush().GetResourceObject());
    int64 Seconds = 0, Gems = 0;
    TestTrue(TEXT("Quote loads academy and source prices"), Screen->GetResearchQuote(Seconds, Gems));
    TestEqual(TEXT("Academy level two gives Unity duration"), Seconds, int64(59));
    TestEqual(TEXT("Unity source instant cost"), Gems, int64(2));
    if (FParse::Param(FCommandLine::Get(), TEXT("RokResearchRenderCheck")))
    {
        Screen->AdvanceDetailAnimation(1.0f);
        FlushRenderingCommands();
        FWidgetRenderer Renderer(true);
        auto* RenderTarget = Renderer.DrawWidget(Screen->TakeWidget(), FVector2D(1440, 810));
        FlushRenderingCommands();
        if (RenderTarget) Renderer.DrawWidget(RenderTarget, Screen->TakeWidget(), FVector2D(1440, 810), 0.0f);
        FlushRenderingCommands();
        TArray<uint8> Png;
        FMemoryWriter Writer(Png);
        const FString Path = FPaths::ProjectSavedDir() / TEXT("ResearchDetailCheck.png");
        TestTrue(TEXT("Research detail renders to PNG"), RenderTarget &&
            FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, Writer) && FFileHelper::SaveArrayToFile(Png, *Path));
    }
    TestEqual(TEXT("Only source resource amount"), Text(TEXT("ResearchDetailCost_0")), FString(TEXT("100")));
    TestEqual(TEXT("Wood cost"), Text(TEXT("ResearchDetailCost_1")), FString(TEXT("100")));
    for (const TCHAR* Name : {TEXT("ResearchInstantBackground"), TEXT("ResearchStartBackground"), TEXT("ResearchDetailCostIcon_0"), TEXT("ResearchDetailCostIcon_1")})
    {
        auto* Art = Cast<UImage>(Screen->GetWidgetFromName(Name));
        TestTrue(FString::Printf(TEXT("Visible Unity art %s is loaded"), Name), Art &&
            Art->GetVisibility() == ESlateVisibility::HitTestInvisible && Art->GetBrush().GetResourceObject());
    }
    TestNotNull(TEXT("Immediate button"), Screen->GetWidgetFromName(TEXT("ResearchInstantButton")));
    TestNotNull(TEXT("Timed button"), Screen->GetWidgetFromName(TEXT("ResearchDetailBoundaryButton")));
    TestEqual(TEXT("Zero stone is hidden"), Screen->GetWidgetFromName(TEXT("ResearchDetailCostItem_2"))->GetVisibility(), ESlateVisibility::Collapsed);
    TestTrue(TEXT("Selected level has a real research action"), Text(TEXT("ResearchDetailBoundaryLabel")).Contains(TEXT("研究")));
    auto Technology = MakeShared<FJsonObject>();
    Technology->SetNumberField(TEXT("technologyType"), 101);
    Technology->SetNumberField(TEXT("level"), 1);
    Role->SetArrayField(TEXT("technologies"), {MakeShared<FJsonValueObject>(Technology)});
    Push(); Screen->RefreshAccountState();
    TestEqual(TEXT("Maximum level hides arrow"), LevelArrow->GetVisibility(), ESlateVisibility::Collapsed);
    Technology->SetNumberField(TEXT("level"), 0);
    Push(); Screen->RefreshAccountState();
    TestEqual(TEXT("Arrow returns below maximum"), LevelArrow->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Queue->SetNumberField(TEXT("technologyType"), 101);
    Queue->SetNumberField(TEXT("finishTime"), 10000);
    Push(); Screen->RefreshAccountState();
    TestEqual(TEXT("Active queue offers cancellation"), Text(TEXT("ResearchDetailBoundaryLabel")), FString(TEXT("取消研究")));
    Click(TEXT("ResearchDetailBoundaryButton"));
    TestEqual(TEXT("First cancellation asks for confirmation"), Text(TEXT("ResearchDetailBoundaryLabel")), FString(TEXT("确认取消")));
    Queue->SetNumberField(TEXT("finishTime"), -1);
    Push(); Screen->RefreshAccountState();
    TestEqual(TEXT("Finished queue offers collection"), Text(TEXT("ResearchDetailBoundaryLabel")), FString(TEXT("领取研究")));
    Click(TEXT("ResearchDetailBoundaryButton"));
    TestTrue(TEXT("Disconnected fixture cannot write"), Text(TEXT("ResearchDetailUnknownState")).Contains(TEXT("服务器连接尚未就绪")));
    return true;
}
#endif
