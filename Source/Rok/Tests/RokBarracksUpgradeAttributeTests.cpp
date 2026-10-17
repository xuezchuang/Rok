#include "../RokBarracksUpgradeAttributes.h"
#include "../RokBarracksSourceTextData.h"
#include "../RokBarracksSourceTextWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBarracksUpgradeAttributeTest,"Rok.Ui.BuildingUpgrade.BarracksAttributes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokBarracksUpgradeAttributeTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Only source barracks transition supported"),URokBarracksUpgradeAttributes::Supports(9,1,2));
    TestFalse(TEXT("Farm must retain its own path"),URokBarracksUpgradeAttributes::Supports(2,1,2));
    TestFalse(TEXT("Unmigrated barracks transition cannot reuse level1 data"),URokBarracksUpgradeAttributes::Supports(9,2,3));
    TestFalse(TEXT("No next level cannot show upgrade delta"),URokBarracksUpgradeAttributes::Supports(9,1,1));
    TestTrue(TEXT("Native current level placement"),URokBarracksUpgradeAttributes::LevelPosition(false).Equals(FVector2D(702,181.82007),.001));
    TestTrue(TEXT("Native next level placement follows source preferred-width hierarchy"),URokBarracksUpgradeAttributes::LevelPosition(true).Equals(FVector2D(826.7,181.82007),.001));
    TestTrue(TEXT("Source current level preferred size"),URokBarracksUpgradeAttributes::LevelSize(false).Equals(FVector2D(61,50),.001));
    TestTrue(TEXT("Source next level preferred size"),URokBarracksUpgradeAttributes::LevelSize(true).Equals(FVector2D(13,50),.001));
    auto* Instance=NewObject<UGameInstance>(GEngine);Instance->InitializeStandalone(TEXT("RokBarracksAttributeTest"));UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT{Instance->Shutdown();GEngine->DestroyWorldContext(World);};
    auto* Page=NewObject<URokBarracksUpgradeAttributes>(World);Page->Initialize();
    if(!TestTrue(TEXT("Has exact source data and atlas"),Page->InitializeAttributes())){AddError(Page->GetLoadError());return false;}
    TestEqual(TEXT("Unchanged attack omitted"),Page->GetAttributeCount(),2);
    TestEqual(TEXT("Source 40*2+30 container"),Page->GetSourceHeight(),110.f);
    TestNull(TEXT("No erroneous food third row"),Page->GetWidgetFromName(TEXT("UpgradeStatLabel_2")));
    TestNull(TEXT("No separate green plus label"),Page->GetWidgetFromName(TEXT("UpgradeStatDelta_0")));
    const FString Names[]={TEXT("步兵单位训练容量"),TEXT("战力")},Values[]={TEXT("20 + <color=green>30</color>"),TEXT("5 + <color=green>5</color>")};
    auto* GlyphData=NewObject<URokBarracksSourceTextData>(Page);FString GlyphError;
    if(!TestTrue(TEXT("Exact source glyph package"),GlyphData->Load(GlyphError))){AddError(GlyphError);return false;}
    for(int32 I=0;I<2;++I)
    {
        auto* Label=CastChecked<URokBarracksSourceTextWidget>(Page->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatLabel_%d"),I)));
        auto* Value=CastChecked<URokBarracksSourceTextWidget>(Page->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatCurrent_%d"),I)));
        TestEqual(TEXT("Exact source label"),Label->GetText().ToString(),Names[I]);
        TestEqual(TEXT("Exact combined source value with green increment only"),Value->GetText().ToString(),Values[I]);
        const auto* LabelMesh=GlyphData->FindKey(FString::Printf(TEXT("label_%d"),I));
        const auto* ValueMesh=GlyphData->FindKey(FString::Printf(TEXT("value_%d"),I));
        if(!TestNotNull(TEXT("Label source mesh"),LabelMesh)||!TestNotNull(TEXT("Value source mesh"),ValueMesh))return false;
        TestTrue(TEXT("Actual glyph widgets loaded"),Label->HasSourceMesh()&&Value->HasSourceMesh());
        TestEqual(TEXT("Label source identity"),Label->GetSourceKey(),LabelMesh->Key);
        TestEqual(TEXT("Value source identity"),Value->GetSourceKey(),ValueMesh->Key);
        TestEqual(TEXT("Source label 20px"),LabelMesh->FontSize,20);
        TestEqual(TEXT("Source value 20px"),ValueMesh->FontSize,20);
        TestEqual(TEXT("Source label regular"),LabelMesh->Style,0);
        TestEqual(TEXT("Source value regular"),ValueMesh->Style,0);
        TestEqual(TEXT("Chinese label MiddleLeft"),LabelMesh->Alignment,3);
        TestEqual(TEXT("Source value MiddleRight"),ValueMesh->Alignment,5);
        TestTrue(TEXT("Label vertices white"),LabelMesh->Colors.Num()>0&&!LabelMesh->Colors.ContainsByPredicate([](FColor C){return C!=FColor::White;}));
        TestTrue(TEXT("Source current and plus beige"),ValueMesh->Colors.Contains(FColor(226,209,155)));
        TestTrue(TEXT("Unity named green is 008000"),ValueMesh->Colors.Contains(FColor(0,128,0)));
        auto* Region=Page->GetWidgetFromName(*FString::Printf(TEXT("BarracksUpgradeTextRegion_%d_0"),I));
        auto* CanvasSlot=CastChecked<UCanvasPanelSlot>(Region->Slot);
        TestTrue(TEXT("Native source local label rect"),CanvasSlot->GetPosition().Equals(FVector2D(30.4,22.5+40*I),.001));
        TestTrue(TEXT("Native source label size"),CanvasSlot->GetSize().Equals(FVector2D(275.76,35),.001));
        auto* Line=Page->GetWidgetFromName(*FString::Printf(TEXT("UpgradeStatSeparator_%d"),I));
        TestEqual(TEXT("Last separator hidden"),Line->GetVisibility(),I==0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);

    }
    return true;
}
#endif
