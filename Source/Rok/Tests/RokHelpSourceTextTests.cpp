#include "Misc/AutomationTest.h"
#include "../RokHelpSourceTextData.h"
#include "../RokHelpSourceTextWidget.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHelpSourceGlyphTest,"Rok.Ui.Help.SourceGlyphText",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokHelpSourceGlyphTest::RunTest(const FString&)
{
    auto* Data=NewObject<URokHelpSourceTextData>();FString Error;
    if(!TestTrue(TEXT("Strict source package loads"),Data->Load(Error))){AddError(Error);return false;}
    TestEqual(TEXT("All source semantic roles exported"),Data->Num(),222);
    const auto* Group=Data->FindKey(TEXT("group:900005"));const auto* Answer=Data->FindKey(TEXT("answer:5001"));
    const auto* Question=Data->FindKey(TEXT("question:5001"));
    if(!Group||!Answer||!Question){AddError(TEXT("Required source identities missing"));return false;}
    TestEqual(TEXT("Source category actual bold style"),Group->Style,1);
    TestTrue(TEXT("Native category quad begins at source centered line y16"),Group->XY[0].Equals(FVector2f(-1,16),.001f));
    TestTrue(TEXT("Native rotated glyph atlas UV converted once to PNG V"),Group->UV[0].Equals(FVector2f(.541015625f,.375f),.000001f));
    TestEqual(TEXT("5001 source answer wraps to two lines"),Answer->Lines,2);
    TestTrue(TEXT("Source answer desired extent includes second line"),Answer->Size.Equals(FVector2D(894.7,45),.001));
    auto* Text=NewObject<URokHelpSourceTextWidget>();
    Text->Configure(Data,24,true,3,FVector2D(894,57.3),FLinearColor::White);
    TestTrue(TEXT("Known source question matches exact text/layout"),Text->SetText(FText::FromString(Question->Text)));
    TestEqual(TEXT("Semantic FText remains readable"),Text->GetText().ToString(),Question->Text);
    TestTrue(TEXT("Actual glyph mesh available"),Text->HasSourceMesh());
    const TSharedRef<SWidget> Slate=Text->TakeWidget();Slate->SlatePrepass();
    TestTrue(TEXT("Widget reserves source rect without auto fitting"),Slate->GetDesiredSize().Equals(FVector2D(894,57.3),.001));
    TestFalse(TEXT("Unknown text fails instead of silently falling back"),Text->SetText(FText::FromString(TEXT("unknown FAQ source glyph text"))));
    TestFalse(TEXT("Failure clears stale glyph mesh"),Text->HasSourceMesh());
    TestFalse(TEXT("Failed text remains semantically inspectable"),Text->GetText().IsEmpty());
    TestTrue(TEXT("Known text can recover"),Text->SetText(FText::FromString(Question->Text)));
    TestTrue(TEXT("Empty construction state is explicit"),Text->SetText(FText::GetEmpty()));
    TestFalse(TEXT("Empty clears old question mesh"),Text->HasSourceMesh());
    return true;
}
#endif
