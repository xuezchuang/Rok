#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokMailAttachmentPulse.h"
#include "Components/CanvasPanel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokMailAttachmentPulseTest,
    "Rok.Ui.Mail.AttachmentPulse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokMailAttachmentPulseTest::RunTest(const FString&)
{
    constexpr double Times[] = {0.0, 0.2, 0.36666667, 0.56666666, 0.75, 1.5, 2.0, 2.2};
    constexpr float Values[] = {1.0f, 1.15f, 1.0f, 1.15f, 1.0f, 1.0f, 1.0f, 1.15f};
    for (int32 I = 0; I < UE_ARRAY_COUNT(Times); ++I)
        TestTrue(*FString::Printf(TEXT("Exact source key/hold/loop %.8f"), Times[I]),
            FMath::IsNearlyEqual(RokMailAttachmentPulse::Evaluate(Times[I]), Values[I], 0.000001f));
    TestTrue(TEXT("Source zero-tangent first midpoint"), FMath::IsNearlyEqual(RokMailAttachmentPulse::Evaluate(0.1), 1.075f, 0.000001f));
    TestTrue(TEXT("Source first falling midpoint"), FMath::IsNearlyEqual(RokMailAttachmentPulse::Evaluate((0.2 + 0.36666667) / 2.0), 1.075f, 0.000001f));
    TestEqual(TEXT("Negative time cannot invent a reverse pulse"), RokMailAttachmentPulse::Evaluate(-0.1), 1.0f);

    FRokMailAttachmentPulse Playback;
    auto* First = NewObject<UCanvasPanel>();
    First->SetRenderOpacity(0.8f);
    auto* Second = NewObject<UCanvasPanel>();
    Playback.BeginRebuild(); Playback.Register(9001, First); Playback.EndRebuild();
    Playback.Advance(0.2);
    TestTrue(TEXT("Parent reaches source peak"), FMath::IsNearlyEqual(First->GetRenderTransform().Scale.X, 1.15f));
    TestEqual(TEXT("Chinese source pivot converts Unity top to UMG top"), First->GetRenderTransformPivot(), FVector2D(1.0, 0.0));
    TestEqual(TEXT("Pulse never changes parent opacity"), First->GetRenderOpacity(), 0.8f);
    Playback.BeginRebuild(); Playback.Register(9001, Second); Playback.EndRebuild();
    TestTrue(TEXT("Selecting another row/rebuilding keeps live attachment phase"), FMath::IsNearlyEqual(Second->GetRenderTransform().Scale.X, 1.15f));
    TestEqual(TEXT("Rebuild preserves time by mail identity"), Playback.GetSeconds(9001), 0.2);
    Playback.Advance(-2.0);
    TestEqual(TEXT("Invalid delta does not rewind"), Playback.GetSeconds(9001), 0.2);
    Playback.BeginRebuild(); Playback.EndRebuild();
    TestEqual(TEXT("Leaving page or absent attachment removes clock"), Playback.Num(), 0);
    Playback.BeginRebuild(); Playback.Register(9001, Second); Playback.EndRebuild();
    TestTrue(TEXT("Reactivated legacy autoplay begins at one"), FMath::IsNearlyEqual(Second->GetRenderTransform().Scale.X, 1.0));
    Playback.Advance(0.2); Playback.Reset();
    TestEqual(TEXT("Close clears every clock"), Playback.Num(), 0);
    TestTrue(TEXT("Close resets surviving target scale"), FMath::IsNearlyEqual(Second->GetRenderTransform().Scale.X, 1.0));
    Playback.Advance(10.0);
    TestEqual(TEXT("Normal empty mail never registers a fake attachment"), Playback.Num(), 0);
    return true;
}
#endif
