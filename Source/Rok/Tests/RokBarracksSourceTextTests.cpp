#include "../RokBarracksSourceTextData.h"
#include "../RokBarracksSourceTextWidget.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBarracksSourceTextTest,"Rok.Ui.BuildingUpgrade.BarracksSourceGlyphText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokBarracksSourceTextTest::RunTest(const FString& Parameters)
{
    auto* Data=NewObject<URokBarracksSourceTextData>();FString Error;
    if(!TestTrue(TEXT("Verified six-text package loads"),Data->Load(Error))){AddError(Error);return false;}
    TestEqual(TEXT("Bounded source coverage"),Data->Num(),6);
    const auto* Level=Data->FindKey(TEXT("level_before"));const auto* Capacity=Data->FindKey(TEXT("value_0"));
    if(!TestNotNull(TEXT("Source level text exists"),Level)||!TestNotNull(TEXT("Source rich value exists"),Capacity))return false;
    TestEqual(TEXT("Level retains source semantics"),Level->Text,FString(TEXT("等级1")));
    TestEqual(TEXT("Bold source style"),Level->Style,1);
    TestEqual(TEXT("Source font is 24 design px"),Level->FontSize,24);
    TestTrue(TEXT("Original source rectangle"),Level->Size.Equals(FVector2D(61,50),.01));
    TestEqual(TEXT("Rich semantic text retained"),Capacity->Text,FString(TEXT("20 + <color=green>30</color>")));
    TestTrue(TEXT("Current and plus source beige present"),Capacity->Colors.Contains(FColor(226,209,155)));
    TestTrue(TEXT("Increment original green present"),Capacity->Colors.Contains(FColor(0,128,0)));
    TestEqual(TEXT("Each vertex has source tint"),Capacity->Colors.Num(),Capacity->XY.Num());
    auto* Widget=NewObject<URokBarracksSourceTextWidget>();
    Widget->Configure(Data,24,true,3,FVector2D(61,50),FLinearColor::White);
    TestTrue(TEXT("Exact semantic mesh selected"),Widget->SetText(FText::FromString(TEXT("等级1"))));
    TestEqual(TEXT("FText remains readable"),Widget->GetText().ToString(),FString(TEXT("等级1")));
    TestEqual(TEXT("Known source key"),Widget->GetSourceKey(),FString(TEXT("level_before")));
    TestFalse(TEXT("Unknown text cannot silently draw old glyphs"),Widget->SetText(FText::FromString(TEXT("等级999"))));
    TestFalse(TEXT("Failed text clears mesh"),Widget->HasSourceMesh());
    TestTrue(TEXT("Failed text clears paint identity"),Widget->GetLastPaintSourceKey().IsEmpty());
    TestEqual(TEXT("Failed text cannot borrow old draw count"),Widget->GetPaintCount(),uint64(0));
    return true;
}
#endif
