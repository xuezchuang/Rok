#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokWorldDayNightState.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldDayNightTimingTest,"Rok.WorldMap.DayNight.ContinuousTiming",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokWorldDayNightTimingTest::RunTest(const FString& Parameters)
{
 FRokWorldDayNightState S;if(!TestTrue(TEXT("Source data"),S.Initialize(13)))return false;
 S.Register(TEXT("a"));S.Tick(0,1796);TestEqual(TEXT("Initial dusk"),S.Phase,4);
 S.Tick(1,1797);TestEqual(TEXT("No early phase polling"),S.Phase,4);
 S.Tick(1,1798);TestEqual(TEXT("Two second poll enters transition late"),S.Phase,5);
 TestFalse(TEXT("Continuous phase5 does not request night"),S.bNight);
 const auto Expected=RokVillageDayNight::SpriteColor(S.Data.Presets[2],S.Data.Presets[3],2.f/3.f,1);
 TestTrue(TEXT("Passed source second plus frame dt"),S.SpriteColor.Equals(Expected,.00001f));
 S.Tick(2,1800);TestTrue(TEXT("Phase6 requests night"),S.bNight);
 TestEqual(TEXT("One layer independently queued"),S.Layers[TEXT("a")].Waiting.Num(),1);
 const auto OldFrame=S.Frame;S.Tick(10,1810);
 TestEqual(TEXT("Long frame does not synthesize poll ticks"),S.Frame,OldFrame+1);
 TestTrue(TEXT("Loop restart uses current scaled time"),S.LastPoll==S.ScaledTime);
 FRokWorldDayNightState Initial;Initial.Initialize(13);Initial.Register(TEXT("a"));Initial.Tick(0,1798);
 TestTrue(TEXT("First phase5 does request night"),Initial.bNight);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldDayNightLifecycleTest,"Rok.WorldMap.DayNight.LayerLifecycle",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokWorldDayNightLifecycleTest::RunTest(const FString& Parameters)
{
 FRokWorldDayNightState S;if(!S.Initialize(7))return false;S.Register(TEXT("a"));S.Register(TEXT("b"));
 S.Tick(0,1801);TestTrue(TEXT("Independent random wait"),S.Layers[TEXT("a")].Waiting[0].Due!=S.Layers[TEXT("b")].Waiting[0].Due);
 S.SetActive(TEXT("a"),false);TestEqual(TEXT("GameObject disable cancels waiting"),S.Layers[TEXT("a")].Waiting.Num(),0);
 S.Request(TEXT("a"),false);TestEqual(TEXT("Inactive request dispatches immediately"),S.Layers[TEXT("a")].Loading.Num(),1);
 S.SetActive(TEXT("a"),true);TestEqual(TEXT("Night OnEnable queues another request"),S.Layers[TEXT("a")].Waiting.Num(),1);
 S.SetEnabled(TEXT("a"),false);TestEqual(TEXT("Component disabled preserves coroutine"),S.Layers[TEXT("a")].Waiting.Num(),1);
 S.Tick(1.1f,1802);TestEqual(TEXT("Disabled component pending coroutine still dispatches"),S.Layers[TEXT("a")].Loading.Num(),2);
 const auto Requests=S.Layers[TEXT("a")].Loading;
 S.CompleteMaterialRequest(TEXT("a"),Requests[1].Serial);S.CompleteMaterialRequest(TEXT("a"),Requests[0].Serial);
 TestFalse(TEXT("Older request completing last wins: no invented latest-wins filter"),S.Layers[TEXT("a")].bNightMaterial);
 TestFalse(TEXT("Unknown/duplicate callback refused"),S.CompleteMaterialRequest(TEXT("a"),Requests[0].Serial));
 S.Request(TEXT("a"),true);S.SetActive(TEXT("a"),false);
 TestEqual(TEXT("GameObject inactive does not cancel submitted asset load"),S.Layers[TEXT("a")].Loading.Num(),1);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldDayNightNativeReplayTest,"Rok.WorldMap.DayNight.NativeContinuousReplay",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokWorldDayNightNativeReplayTest::RunTest(const FString& Parameters)
{
 FString Text;TSharedPtr<FJsonObject> Root,Verification;
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/WorldDayNightContinuous/verify.json")))
  ||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Verification)||!Verification||!Verification->GetBoolField(TEXT("passed")))return false;
 const auto Orders=Verification->GetObjectField(TEXT("observed_update_order"));
 if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/WorldDayNightContinuous/native_timeline.json")))
  ||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root)return false;
 FRokWorldDayNightState S;FString Scenario;double StartTime=0;int32 Compared=0,Groups=0;float Maximum=0;
 for(const auto& Value:Root->GetArrayField(TEXT("frames")))
 {
  const auto F=Value->AsObject();const FString Name=F->GetStringField(TEXT("scenario"));const double Time=F->GetNumberField(TEXT("time"));
  const auto& RGB=F->GetArrayField(TEXT("rgb"));const FLinearColor Expected(RGB[0]->AsNumber(),RGB[1]->AsNumber(),RGB[2]->AsNumber(),1);
  const int64 Seconds=F->GetNumberField(TEXT("server"));const float Dt=F->GetNumberField(TEXT("dt"));
  if(Name!=Scenario)
  {
   Scenario=Name;StartTime=Time;++Groups;if(!S.Initialize(1))return false;
   FString Order;if(Orders->TryGetStringField(Name,Order))
   {
    if(Order!=TEXT("poll_before_light")&&Order!=TEXT("light_before_poll"))return false;
    S.bPollBeforeLightUpdate=Order==TEXT("poll_before_light");
   }
   // The source global shader value survives manager destruction. The first
   // coroutine-frame value is an explicit replay initial condition, not a
   // claimed day preset. All subsequent actual frames are predicted.
   S.SpriteColor=Expected;S.Start(Seconds);continue;
  }
  S.ScaledTime=Time-StartTime-Dt;
  if(!S.Tick(Dt,Seconds))return false;
  Maximum=FMath::Max(Maximum,FMath::Max3(FMath::Abs(S.SpriteColor.R-Expected.R),FMath::Abs(S.SpriteColor.G-Expected.G),FMath::Abs(S.SpriteColor.B-Expected.B)));
  if(!S.SpriteColor.Equals(Expected,.00002f)){AddError(FString::Printf(TEXT("Native replay differs %s source frame %.0f error %g"),*Name,F->GetNumberField(TEXT("frame")),Maximum));return false;}
  ++Compared;
 }
 TestEqual(TEXT("Five source scenarios"),Groups,5);TestTrue(TEXT("Over 2000 predicted native frames"),Compared>2000);
 AddInfo(FString::Printf(TEXT("Native continuous frames=%d maximum RGB error=%g; source-observed Update order selected per scenario"),Compared,Maximum));return true;
}
#endif
