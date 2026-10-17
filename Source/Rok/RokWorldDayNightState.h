#pragma once
#include "CoreMinimal.h"
#include "RokVillageDayNight.h"

// Two clocks are deliberate: GEM polls local/server seconds with a scaled
// two-second timer; LightManager and WaitForSeconds advance with scaled dt.
struct FRokWorldDayNightRequest
{
 uint64 Serial=0, EarliestFrame=0;
 double Due=0;
 bool bNight=false;
};
struct FRokWorldDayNightLayer
{
 bool bActive=true,bEnabled=true,bNightMaterial=false;
 TArray<FRokWorldDayNightRequest> Waiting,Loading;
};
struct FRokWorldDayNightState
{
 FRokVillageDayNightData Data;
 TMap<FString,FRokWorldDayNightLayer> Layers;
 TArray<FString> RegistrationOrder;
 FRandomStream Random;
 FLinearColor SpriteColor=FLinearColor(1.07203974f,1.07203974f,1.07203974f,1);
 double ScaledTime=0,LastPoll=0;
 uint64 Frame=0,NextSerial=1;
 int32 Phase=-1,From=1,To=1;
 float Duration=1,Passed=0;
 bool bNight=false,bInterpolating=false,bStarted=false;
 // Unity default Update order is not fixed: the native probe observed both
 // orders after LightManager reconstruction. Replay selects the observed one.
 bool bPollBeforeLightUpdate=true;
 bool Initialize(int32 Seed);
 void Start(int64 LocalSimulationSeconds);
 void Register(const FString& Id,bool bActive=true);
 void SetActive(const FString& Id,bool bActive);
 void SetEnabled(const FString& Id,bool bEnabled);
 void Request(const FString& Id,bool bValue,float ExplicitDelay=-1);
 // Advances real continuous state. Does not fabricate skipped timer callbacks.
 bool Tick(float DeltaSeconds,int64 LocalSimulationSeconds);
 bool CompleteMaterialRequest(const FString& Id,uint64 Serial);
 FRokVillageDayNightResult ResultFor(const FString& Id) const;
private:
 void Poll(int64 Seconds);
 void SetNight(bool bValue);
};
