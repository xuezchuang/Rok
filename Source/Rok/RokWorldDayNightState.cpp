#include "RokWorldDayNightState.h"

bool FRokWorldDayNightState::Initialize(int32 Seed)
{
 *this=FRokWorldDayNightState();Random.Initialize(Seed);return Data.Load();
}
void FRokWorldDayNightState::Register(const FString& Id,bool bActive)
{
 if(Layers.Contains(Id))return;
 auto& Layer=Layers.Add(Id);RegistrationOrder.Add(Id);Layer.bActive=bActive;
 if(bActive&&bNight)Request(Id,true);
}
void FRokWorldDayNightState::SetActive(const FString& Id,bool bActive)
{
 auto* Layer=Layers.Find(Id);if(!Layer||Layer->bActive==bActive)return;
 Layer->bActive=bActive;
 // SetActive(false) stops coroutines, not asset callbacks already dispatched.
 if(!bActive)Layer->Waiting.Reset();
 else if(Layer->bEnabled&&bNight)Request(Id,true);
}
void FRokWorldDayNightState::SetEnabled(const FString& Id,bool bEnabled)
{
 auto* Layer=Layers.Find(Id);if(!Layer||Layer->bEnabled==bEnabled)return;
 Layer->bEnabled=bEnabled;
 // Disabling a MonoBehaviour does not stop its existing coroutines.
 if(bEnabled&&Layer->bActive&&bNight)Request(Id,true);
}
void FRokWorldDayNightState::Request(const FString& Id,bool bValue,float ExplicitDelay)
{
 auto* Layer=Layers.Find(Id);if(!Layer)return;
 FRokWorldDayNightRequest R;R.Serial=NextSerial++;R.bNight=bValue;
 if(!Layer->bActive||!Layer->bEnabled){Layer->Loading.Add(R);return;}
 R.Due=ScaledTime+(ExplicitDelay>=0?ExplicitDelay:Random.FRand());R.EarliestFrame=Frame+1;
 Layer->Waiting.Add(R);
}
void FRokWorldDayNightState::SetNight(bool bValue)
{
 if(bNight!=bValue)for(const auto& Id:RegistrationOrder)if(Layers.Contains(Id))Request(Id,bValue);
 bNight=bValue;
}
void FRokWorldDayNightState::Poll(int64 Seconds)
{
 if(Seconds<1)return;
 static constexpr int32 Starts[]={0,57,60,1737,1740,1797,1800,2397};
 static constexpr int32 Durations[]={57,3,1677,3,57,3,597,3};
 const int32 Cycle=Seconds%2400;int32 NewPhase=7;
 while(NewPhase>0&&Cycle<Starts[NewPhase])--NewPhase;
 if(NewPhase==Phase)return;
 const int32 Old=Phase;Phase=NewPhase;
 if((Phase%2)==1||Old==-1)
 {
  From=Phase/2;To=((Phase+1)/2)%4;Duration=Durations[Phase];Passed=Cycle-Starts[Phase];bInterpolating=true;
 }
 if((Phase%2)==0||Old==-1)SetNight(Phase>=5);
}
bool FRokWorldDayNightState::Tick(float Dt,int64 Seconds)
{
 if(!FMath::IsFinite(Dt)||Dt<0||Data.Presets.Num()!=4)return false;
 ScaledTime+=Dt;++Frame;
 const bool Due=!bStarted||ScaledTime-LastPoll>=2;
 if(Due&&bPollBeforeLightUpdate){bStarted=true;LastPoll=ScaledTime;Poll(Seconds);}
 if(bInterpolating)
 {
  Passed+=Dt;const float Alpha=FMath::Min(Passed/Duration,1.f);
  SpriteColor=RokVillageDayNight::SpriteColor(Data.Presets[From],Data.Presets[To],Alpha,1);
  if(Alpha>=1){bInterpolating=false;Passed=0;}
 }
 if(Due&&!bPollBeforeLightUpdate){bStarted=true;LastPoll=ScaledTime;Poll(Seconds);}
 for(auto& Pair:Layers)
 {
  auto& L=Pair.Value;
  for(int32 I=0;I<L.Waiting.Num();)
  {
   const auto R=L.Waiting[I];
   if(Frame>=R.EarliestFrame&&ScaledTime>=R.Due){L.Loading.Add(R);L.Waiting.RemoveAt(I);}
   else ++I;
  }
 }
 return true;
}
void FRokWorldDayNightState::Start(int64 Seconds)
{
 if(bStarted)return;bStarted=true;LastPoll=ScaledTime;Poll(Seconds);
}
bool FRokWorldDayNightState::CompleteMaterialRequest(const FString& Id,uint64 Serial)
{
 auto* L=Layers.Find(Id);if(!L)return false;
 const int32 Index=L->Loading.IndexOfByPredicate([Serial](const auto& R){return R.Serial==Serial;});
 if(Index==INDEX_NONE)return false;
 L->bNightMaterial=L->Loading[Index].bNight;L->Loading.RemoveAt(Index);return true;
}
FRokVillageDayNightResult FRokWorldDayNightState::ResultFor(const FString& Id) const
{
 FRokVillageDayNightResult R;const auto* L=Layers.Find(Id);R.bNightMaterial=L&&L->bNightMaterial;R.SpriteColor=SpriteColor;
 R.Color1=R.bNightMaterial?Data.NightColor1:Data.DayColor1;R.Color2=R.bNightMaterial?Data.NightColor2:Data.DayColor2;
 R.NightLight=R.bNightMaterial?Data.NightColor3*RokVillageDayNight::NightPulse(ScaledTime):FLinearColor::Black;return R;
}
