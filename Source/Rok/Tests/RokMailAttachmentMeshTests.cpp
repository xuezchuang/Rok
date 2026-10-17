#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

#include "../RokDataPaths.h"
#include "../RokMailAttachmentMeshData.h"
#include "../RokMailAttachmentMeshWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailAttachmentMeshTest,"Rok.Ui.Mail.AttachmentMesh",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokMailAttachmentMeshTest::RunTest(const FString& Parameters)
{
    FString Json,Error;RokMailAttachmentMesh::FClip Clip;
    if(!TestTrue(TEXT("Export exists"),FFileHelper::LoadFileToString(Json,*(RokDataPaths::Resolve(TEXT("MailSpine/gift5_stay_mesh.json"))))))return false;
    if(!TestTrue(TEXT("Exact source gift5/stay parses"),RokMailAttachmentMesh::Parse(Json,Clip,Error)))return false;
    TestEqual(TEXT("Source frame count"),Clip.Frames.Num(),96);
    TestTrue(TEXT("Independent child duration"),FMath::IsNearlyEqual(Clip.Duration,1.6f));
    TestEqual(TEXT("Original transparent mesh and visible quad"),Clip.Frames[0].Draws.Num(),2);
    TestEqual(TEXT("Open mesh stays transparent"),Clip.Frames[0].Draws[0].Tint.A,0.f);
    TestEqual(TEXT("Stay quad stays opaque"),Clip.Frames[0].Draws[1].Tint.A,1.f);
    TestTrue(TEXT("Source center plus child offset"),RokMailAttachmentMesh::Project(FVector2f(0,0)).Equals(FVector2f(14,17.6f),.0001f));
    FString Wrong=Json.Replace(TEXT("UE_TreasureBox_stay"),TEXT("UE_TreasureBox_open"));
    TestFalse(TEXT("Reject another animation identity"),RokMailAttachmentMesh::Parse(Wrong,Clip,Error));
    auto* Widget=NewObject<URokMailAttachmentMeshWidget>();
    if(!TestTrue(TEXT("Source hashes and atlas load"),Widget->LoadSource()))return false;
    Widget->AdvanceAnimation(.4f);const auto Generation=Widget->GetLoadGeneration();
    TestTrue(TEXT("Load existing row again"),Widget->LoadSource());
    TestEqual(TEXT("Same instance retains source animation"),Widget->GetLoadGeneration(),Generation);
    TestTrue(TEXT("Same instance phase retained"),FMath::IsNearlyEqual(Widget->GetAnimationTime(),.4f));
    Widget->SetVisibility(ESlateVisibility::Collapsed);Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    TestTrue(TEXT("Visibility does not initialize Spine again"),FMath::IsNearlyEqual(Widget->GetAnimationTime(),.4f));
    Widget->AdvanceAnimation(1.4f);TestTrue(TEXT("Source child loops independently"),FMath::IsNearlyEqual(Widget->GetAnimationTime(),.2f,.00001f));
    Widget->ResetModel();TestFalse(TEXT("Destroy row clears mesh"),Widget->IsModelLoaded());
    TestEqual(TEXT("Destroy row clears phase"),Widget->GetAnimationTime(),0.f);
    return true;
}
#endif
