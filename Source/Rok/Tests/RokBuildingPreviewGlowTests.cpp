#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokBuildingPreviewGlowData.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGlowNativeSamples,"Rok.Ui.BuildingPreviewGlow.NativeSamples",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokGlowNativeSamples::RunTest(const FString& Parameters)
{
    using namespace RokBuildingPreviewGlow;FData Data;FString Error;
    const bool Loaded=Data.Load(FData::DefaultPath(),Error);if(!TestTrue(TEXT("Source data loads"),Loaded)){AddError(Error);return false;}
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*FData::DefaultPath())||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root))return false;
    double MaxAlpha=0,MaxAngle=0,MaxPixel=0;int32 Count=0,Projected=0;
    for(const auto& Value:Root->GetArrayField(TEXT("native_samples")))
    {
        const auto R=Value->AsObject();TArray<FLayerSample> Sample;
        if(!Data.Sample(R->GetNumberField(TEXT("time")),Sample))return false;
        const auto& Alpha=R->GetArrayField(TEXT("alpha"));const auto& Rotation=R->GetArrayField(TEXT("rotation"));const auto& Quad=R->GetArrayField(TEXT("screen_quad"));
        for(int32 I=0;I<2;++I)
        {
            MaxAlpha=FMath::Max(MaxAlpha,FMath::Abs(Sample[I].Alpha-Alpha[I]->AsNumber()));
            const auto& Q=Rotation[I]->AsArray();FQuat Native(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber());Native.Normalize();
            MaxAngle=FMath::Max(MaxAngle,FMath::RadiansToDegrees(Sample[I].LocalRotation.AngularDistance(Native)));
            if(R->GetBoolField(TEXT("projection_valid")))for(int32 V=0;V<4;++V){const auto& XY=Quad[I]->AsArray()[V]->AsArray();MaxPixel=FMath::Max(MaxPixel,FVector2D::Distance(Sample[I].ScreenVertices[V],FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber())));}
        }
        ++Count;if(R->GetBoolField(TEXT("projection_valid")))++Projected;
    }
    TestEqual(TEXT("All actual native alpha/quaternion samples"),Count,1400);TestEqual(TEXT("Projection excludes known pre-first-Camera.Render sample only"),Projected,1399);
    TestTrue(TEXT("Source alpha vs native"),MaxAlpha<.000002);
    TestTrue(TEXT("Source integrated local quaternion vs native"),MaxAngle<.001);
    TestTrue(TEXT("Source geometry and stable actual native projection"),MaxPixel<.02);
    AddInfo(FString::Printf(TEXT("max alpha %.9g angle %.9g pixel %.9g"),MaxAlpha,MaxAngle,MaxPixel));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGlowClock,"Rok.Ui.BuildingPreviewGlow.ClockAndCurve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokGlowClock::RunTest(const FString& Parameters)
{
    using namespace RokBuildingPreviewGlow;FData Data;FString Error;if(!Data.Load(FData::DefaultPath(),Error)){AddError(Error);return false;}
    TArray<FLayerSample> A,B;Data.Sample(.25,A);
    TestTrue(TEXT("Hermite first quarter is not linear interpolation"),FMath::IsNearlyEqual(A[0].Alpha,.1359375,1e-9));
    Data.Sample(0,A);Data.Sample(2,B);TestTrue(TEXT("Alpha loops exactly at 2 seconds"),FMath::IsNearlyEqual(A[0].Alpha,B[0].Alpha,1e-12));
    TestFalse(TEXT("Rotation does not reset at alpha loop"),A[0].LocalRotation.Equals(B[0].LocalRotation,1e-6));
    FPlayback Clock;TestTrue(TEXT("Clock advances explicit local delta"),Clock.Advance(.5));Clock.Reset();TestEqual(TEXT("Reset returns explicit time zero"),Clock.GetTime(),0.0);
    TestFalse(TEXT("Negative delta refused"),Clock.Advance(-1));TestFalse(TEXT("Infinite delta refused"),Clock.Advance(std::numeric_limits<double>::infinity()));
    TestFalse(TEXT("Negative sample refused"),Data.Sample(-1,A));TestFalse(TEXT("NaN sample refused"),Data.Sample(std::numeric_limits<double>::quiet_NaN(),A));
    Clock.Sample(Data,A);Data.Sample(0,B);TestTrue(TEXT("Reset reproduces source initial quaternion"),A[1].LocalRotation.Equals(B[1].LocalRotation,1e-12));
    return true;
}
#endif
