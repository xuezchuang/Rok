#include "Misc/AutomationTest.h"
#include "../RokBuildingSourcePreviewData.h"
#include "../RokBuildingSourcePreviewWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuildingSourcePreviewBindingTest,"Rok.Ui.Building.SourcePreviewBinding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokBuildingSourcePreviewBindingTest::RunTest(const FString& Parameters)
{
    const FString Row=TEXT("{\"init_id\":6,\"type_id\":9,\"visual\":{\"model_id\":\"build_9_101_1\",\"sprite_name\":\"build_9_101_1\",\"sprite_guid\":\"4bb871ef2938a6d48a9b46af5cd8f287\"}}");
    const FString Json=TEXT("{\"buildings\":[")+Row+TEXT("]}");
    const TArray<FName> Tags={TEXT("RokUnityConfigInitBuilding"),TEXT("UnityInitId_6"),TEXT("UnityBuildingType_9")};
    FString Model,Error;
    TestTrue(TEXT("Exact declared scene model resolves without role/country"),RokBuildingSourcePreview::ResolveDeclaredModel(Tags,Json,Model,Error));
    TestEqual(TEXT("Preserve explicitly declared model"),Model,FString(TEXT("build_9_101_1")));
    auto Missing=Tags;Missing.Remove(TEXT("UnityInitId_6"));TestFalse(TEXT("Unknown actor has no civilization default"),RokBuildingSourcePreview::ResolveDeclaredModel(Missing,Json,Model,Error));TestTrue(TEXT("Failure clears previous model"),Model.IsEmpty());
    auto Ambiguous=Tags;Ambiguous.Add(TEXT("UnityInitId_7"));TestFalse(TEXT("Ambiguous identity rejected"),RokBuildingSourcePreview::ResolveDeclaredModel(Ambiguous,Json,Model,Error));
    TestFalse(TEXT("Duplicate manifest row rejected"),RokBuildingSourcePreview::ResolveDeclaredModel(Tags,TEXT("{\"buildings\":[")+Row+TEXT(",")+Row+TEXT("]}"),Model,Error));
    TestFalse(TEXT("Missing declared model stays unknown"),RokBuildingSourcePreview::ResolveDeclaredModel(Tags,Json.Replace(TEXT("model_id"),TEXT("missing_model")),Model,Error));
    TestFalse(TEXT("Type disagreement rejected"),RokBuildingSourcePreview::ResolveDeclaredModel(Tags,Json.Replace(TEXT("\"type_id\":9"),TEXT("\"type_id\":10")),Model,Error));
    FString Actual;TestTrue(TEXT("Existing source initial manifest available"),FFileHelper::LoadFileToString(Actual,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/unity_initial_city_state.json"))));
    TestTrue(TEXT("Actual Init6 declared model resolves"),RokBuildingSourcePreview::ResolveDeclaredModel(Tags,Actual,Model,Error));TestEqual(TEXT("Actual declared model"),Model,FString(TEXT("build_9_101_1")));
    RokBuildingSourcePreview::FLayout Layout;
    TestFalse(TEXT("Empty explicit model never falls back"),RokBuildingSourcePreview::LoadLayout(FString(),RokBuildingSourcePreview::MetadataFile(),Layout,Error));
    TestTrue(TEXT("Verified native metadata resolves explicit model"),RokBuildingSourcePreview::LoadLayout(TEXT("build_9_101_1"),RokBuildingSourcePreview::MetadataFile(),Layout,Error));
    TestEqual(TEXT("Raw UI source size"),Layout.TextureSize,FIntPoint(512,490));
    TestTrue(TEXT("Native quad has positive geometry"),Layout.Size.X>0&&Layout.Size.Y>0);
    TestFalse(TEXT("Other model cannot borrow this preview"),RokBuildingSourcePreview::LoadLayout(TEXT("build_9_501_1"),RokBuildingSourcePreview::MetadataFile(),Layout,Error));
    auto* Widget=NewObject<URokBuildingSourcePreviewWidget>();Widget->Initialize();
    TestFalse(TEXT("Unknown preview remains unavailable"),Widget->InitializePreview(FString(),nullptr));TestFalse(TEXT("No placeholder image on missing identity"),Widget->IsPreviewReady());
    TestFalse(TEXT("Unknown state has explicit reason"),Widget->GetLoadError().IsEmpty());Widget->ResetPreview();TestFalse(TEXT("Reset clears presentation"),Widget->IsPreviewReady());
    return true;
}
