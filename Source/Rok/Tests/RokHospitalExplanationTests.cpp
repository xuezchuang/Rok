#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../RokHospitalExplanationWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokHospitalExplanationTest,"Rok.Ui.Hospital.Explanation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokHospitalExplanationTest::RunTest(const FString&)
{
    FString Json;TSharedPtr<FJsonObject> Source;
    if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/hospital_explanation.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Source))return false;
    TArray<FString> Paths;for(const auto& Row:Source->GetArrayField(TEXT("sources")))Paths.Add(Row->AsObject()->GetStringField(TEXT("path")));
    TestTrue(TEXT("Complete exact source set accepted"),URokHospitalExplanationWidget::HasExactSourcePaths(Paths));
    TestFalse(TEXT("Empty source set rejected"),URokHospitalExplanationWidget::HasExactSourcePaths({}));
    auto Missing=Paths;Missing.Pop();TestFalse(TEXT("Missing mandatory source rejected"),URokHospitalExplanationWidget::HasExactSourcePaths(Missing));
    auto Duplicate=Paths;Duplicate[1]=Duplicate[0];TestFalse(TEXT("Duplicate replacing mandatory source rejected"),URokHospitalExplanationWidget::HasExactSourcePaths(Duplicate));
    auto Unexpected=Paths;Unexpected[0]=TEXT("ref/unexpected.cs");TestFalse(TEXT("Unexpected source rejected"),URokHospitalExplanationWidget::HasExactSourcePaths(Unexpected));
    auto* Instance=NewObject<UGameInstance>(GEngine);Instance->InitializeStandalone(TEXT("RokHospitalExplanationTest"));UWorld* World=Instance->GetWorld();
    ON_SCOPE_EXIT{Instance->Shutdown();GEngine->DestroyWorldContext(World);};
    auto* Page=NewObject<URokHospitalExplanationWidget>(World);Page->Initialize();
    if(!TestTrue(TEXT("Source explanation loads"),Page->InitializeExplanation())){AddError(Page->GetLoadError());return false;}
    TestTrue(TEXT("Exact source viewport position"),Page->GetSourcePosition().Equals(FVector2D(659.95,146.6),.001));
    TestTrue(TEXT("Exact source viewport size"),Page->GetSourceSize().Equals(FVector2D(522.7,579.2),.001));
    const FString Raw=Page->GetSourceText().ToString();
    for(int32 I=1;I<=7;++I)TestTrue(TEXT("Each numbered source rule preserved"),Raw.Contains(FString::Printf(TEXT("\n%d."),I)));
    TestTrue(TEXT("Final capacity warning preserved"),Raw.Contains(TEXT("医院容量已满")));
    TestEqual(TEXT("Only source rich-text tag translation"),Page->GetRichText().ToString(),Raw.Replace(TEXT("<color=yellow>"),TEXT("<yellow>")).Replace(TEXT("</color>"),TEXT("</>")));
    auto* Text=CastChecked<URichTextBlock>(Page->GetWidgetFromName(TEXT("HospitalExplanationText")));
    const auto* Yellow=Text->GetTextStyleSet()->FindRow<FRichTextStyleRow>(TEXT("yellow"),TEXT("HospitalTest"));
    TestNotNull(TEXT("Yellow rich text style exists"),Yellow);
    if(Yellow)TestEqual(TEXT("Runtime Language181001 named yellow, not old prefab gold"),Yellow->TextStyle.ColorAndOpacity.GetSpecifiedColor().ToFColor(true),FColor(255,255,0));
    const auto Slate=Page->TakeWidget();auto* Scroll=CastChecked<UScrollBox>(Page->GetWidgetFromName(TEXT("HospitalExplanationScroll")));const auto ScrollSlate=Scroll->TakeWidget();
    Scroll->SetScrollOffset(150);TestEqual(TEXT("Scroll setup precondition"),Page->GetScrollOffset(),150.f);
    Page->SetVisibility(ESlateVisibility::Collapsed);Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    TestEqual(TEXT("Hide/show retains same scroll state"),Page->GetScrollOffset(),150.f);
    TestEqual(TEXT("Hide/show retains source text widget"),Page->GetWidgetFromName(TEXT("HospitalExplanationText")),static_cast<UWidget*>(Text));
    auto* Fresh=NewObject<URokHospitalExplanationWidget>(World);Fresh->Initialize();TestTrue(TEXT("Fresh window loads"),Fresh->InitializeExplanation());
    TestEqual(TEXT("Fresh window resets explanation scroll"),Fresh->GetScrollOffset(),0.f);
    return true;
}
#endif
