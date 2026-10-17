#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../RokDataPaths.h"
#include "../RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainArmyMeshTest, "Rok.Ui.TrainArmy.SourceMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainArmyMeshTest::RunTest(const FString&)
{
    using namespace RokTrainArmyMesh;
    const uint8 ABC[] = { 'a','b','c' }; TArray<uint8> Empty;
    TestEqual(TEXT("SHA256 empty vector"), Sha256(Empty), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    TestEqual(TEXT("SHA256 abc vector"), Sha256(ABC), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    TArray<uint8> Million; Million.Init('a',1000000);
    TestEqual(TEXT("SHA256 million-a padding and block vector"), Sha256(Million), FString(TEXT("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")));
    TestTrue(TEXT("Source anchor projection"), Project(FVector2f::ZeroVector).Equals(FVector2f(481,676.7f),.001f));
    TestTrue(TEXT("PPU100 and rect .85, y-up conversion once"), Project(FVector2f(1,2)).Equals(FVector2f(566,506.7f),.001f));
    TestTrue(TEXT("Loop preserves remainder"), FMath::IsNearlyEqual(Advance(3.2f,.2f,3.3333333f),.0666667f,.0001f));
    TestEqual(TEXT("Negative delta does not reverse"), Advance(1,-1,3),1.f);
    const auto C=PremultipliedTint(FLinearColor(.8f,.6f,.4f,.5f),FLinearColor(1,1,1,.5f));
    TestTrue(TEXT("PMA applies slot alpha and parent opacity to RGB/A once"), C.Equals(FLinearColor(.2f,.15f,.1f,.25f),.00001f));
    FString Json, Error; FClip Clip;
    if (!TestTrue(TEXT("Read accepted actual Arms10101 source mesh"), FFileHelper::LoadFileToString(Json,*(RokDataPaths::Resolve(TEXT("TrainArmy/arms_10101_idle_mesh.json")))))) return false;
    if (!TestTrue(TEXT("Parse actual source sample"), Parse(Json,10101,TEXT("Arms1_1_SD"),Clip,Error))) {AddError(Error);return false;}
    TestEqual(TEXT("200 sampled source poses"),Clip.Frames.Num(),200);
    bool Deformed=false;
    for(int32 D=0;D<Clip.Frames[0].Draws.Num();++D)
    {
        const auto& A=Clip.Frames[0].Draws[D];const auto& B=Clip.Frames[30].Draws[D];
        if(CanInterpolate(A,B))for(int32 V=0;V<A.XY.Num();++V)Deformed|=!A.XY[V].Equals(B.XY[V],.0001f);
    }
    TestTrue(TEXT("Idle changes source vertices, not a moving raster card"),Deformed);
    FClip Invalid;
    TestFalse(TEXT("Actual Arms identity mismatch rejected"),Parse(Json,10102,TEXT("Arms1_1_SD"),Invalid,Error));
    TestFalse(TEXT("World model cannot substitute armsShow"),Parse(Json,10101,TEXT("Arms1_1"),Invalid,Error));
    auto Mutated = [&](TFunction<void(TSharedPtr<FJsonObject>)> Mutation)
    {
        TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);Mutation(Root);
        FString Text;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));
        return Parse(Text,10101,TEXT("Arms1_1_SD"),Invalid,Error);
    };
    TestFalse(TEXT("Unknown schema rejected"),Mutated([](auto Root){Root->SetNumberField(TEXT("schema_version"),2);}));
    TestFalse(TEXT("Invalid duration rejected"),Mutated([](auto Root){Root->SetNumberField(TEXT("duration"),0);}));
    TestFalse(TEXT("Out of range triangle rejected"),Mutated([](auto Root){auto D=Root->GetArrayField(TEXT("frames"))[0]->AsObject()->GetArrayField(TEXT("draws"))[0]->AsObject();auto I=D->GetArrayField(TEXT("triangles"));I[0]=MakeShared<FJsonValueNumber>(65535);D->SetArrayField(TEXT("triangles"),I);}));
    TestFalse(TEXT("Unsupported blend rejected"),Mutated([](auto Root){Root->GetArrayField(TEXT("frames"))[0]->AsObject()->GetArrayField(TEXT("draws"))[0]->AsObject()->SetStringField(TEXT("blend"),TEXT("Additive"));}));
    TestFalse(TEXT("Non-number vertex rejected"),Mutated([](auto Root){auto D=Root->GetArrayField(TEXT("frames"))[0]->AsObject()->GetArrayField(TEXT("draws"))[0]->AsObject();auto XY=D->GetArrayField(TEXT("xy"));XY[0]=MakeShared<FJsonValueString>(TEXT("NaN"));D->SetArrayField(TEXT("xy"),XY);}));
    TestFalse(TEXT("Source path traversal rejected"),Mutated([](auto Root){Root->GetArrayField(TEXT("sources"))[0]->AsObject()->SetStringField(TEXT("path"),TEXT("ref/client-unity/../secret"));}));
    TestTrue(TEXT("Failed parse clears partial output"),Invalid.Frames.IsEmpty());
    return true;
}
#endif
