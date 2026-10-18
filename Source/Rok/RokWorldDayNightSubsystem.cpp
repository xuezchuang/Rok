#include "RokWorldDayNightSubsystem.h"
#include "RokWorldMapEntityActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

bool URokWorldDayNightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 const UWorld* World=Cast<UWorld>(Outer);
 // The startup temporary Game world has no source map or local day/night state.
 return World&&World->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")
  &&FParse::Param(FCommandLine::Get(),TEXT("RokWorldDayNightLocal"))&&!IsRunningDedicatedServer()&&Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokWorldDayNightSubsystem::DoesSupportWorldType(EWorldType::Type Type) const{return Type==EWorldType::Game;}
void URokWorldDayNightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 int32 Seed=int32(FPlatformTime::Cycles());
 FParse::Value(FCommandLine::Get(),TEXT("RokWorldDayNightSeed="),Seed);
 // No implicit wall-clock/server substitution. The caller chooses a clearly
 // identified local simulation epoch; elapsed real time advances that epoch.
 if(!FParse::Value(FCommandLine::Get(),TEXT("RokWorldDayNightLocalSeconds="),LocalBaseSeconds)||LocalBaseSeconds<1
  ||GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")||!State.Initialize(Seed))
 {UE_LOG(LogTemp,Error,TEXT("Continuous world day/night needs valid local seconds, source data and exact world map."));return;}
 bReady=true;
}
TStatId URokWorldDayNightSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorldDayNightSubsystem,STATGROUP_Tickables);}
bool URokWorldDayNightSubsystem::ReanchorLocalSimulationSeconds(int64 Seconds)
{
 if(!bReady||Seconds<1)return false;LocalBaseSeconds=Seconds;ClockAnchor=FPlatformTime::Seconds();return true;
}
void URokWorldDayNightSubsystem::RegisterSourceCard(UStaticMeshComponent* Card,const FString& Transform)
{
 if(!bReady||!Card)return;
 const auto* Owner=Cast<ARokWorldMapEntityActor>(Card->GetOwner());
 if(!Owner)return;
 const FString Id=FString::Printf(TEXT("%lld/%s"),int64(Owner->GetSourceId()),*Transform);
 if(CardIds.Contains(Card))return;
 auto* MID=Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0));FLinearColor Value;
 if(!MID||!MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("NightLight")),Value))
 {UE_LOG(LogTemp,Error,TEXT("Continuous source material lacks NightLight: %s"),*Id);bReady=false;return;}
 CardIds.Add(Card,Id);State.Register(Id);
}
void URokWorldDayNightSubsystem::UpdateSourceActive(UStaticMeshComponent* Card,bool bActive)
{
 if(const auto* Id=CardIds.Find(Card))State.SetActive(*Id,bActive);
}
void URokWorldDayNightSubsystem::BindEnvironment()
{
 // Only generated source material families with a defined UnitySpriteColor
 // input participate. No directional-light approximation of unlit shaders.
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  for(auto* Mesh:Meshes)for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
  {
   auto* Material=Mesh->GetMaterial(Slot);if(!Material)continue;
   const FString Path=Material->GetPathName();
   if(!Path.StartsWith(TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_"))
    &&!Path.StartsWith(TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_")))continue;
   FLinearColor Value;if(!Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")),Value))continue;
   if(auto* MID=Mesh->CreateDynamicMaterialInstance(Slot))EnvironmentMaterials.Add(MID);
  }
 }
 bEnvironmentBound=true;
 UE_LOG(LogTemp,Display,TEXT("Continuous local world lighting bound %d environment materials; per-family Gamma fidelity remains separate."),EnvironmentMaterials.Num());
}
void URokWorldDayNightSubsystem::Tick(float DeltaTime)
{
 if(!bReady||!GetWorld()->HasBegunPlay())return;
 if(ClockAnchor==0)ClockAnchor=FPlatformTime::Seconds();
 // Loaded UE materials complete previously dispatched requests in their
 // arrival order on this frame. This is an explicit warm-asset policy, not a
 // claim about Addressables cold-load latency; state exposes completion IDs.
 for(auto It=CardIds.CreateIterator();It;++It)
 {
  if(!It.Key().IsValid()){State.Layers.Remove(It.Value());State.RegistrationOrder.Remove(It.Value());It.RemoveCurrent();continue;}
  const auto* Layer=State.Layers.Find(It.Value());if(!Layer)continue;
  const auto Requests=Layer->Loading;for(const auto& R:Requests)State.CompleteMaterialRequest(It.Value(),R.Serial);
 }
 const int64 Seconds=LocalBaseSeconds+int64(FPlatformTime::Seconds()-ClockAnchor);
 if(!State.Tick(DeltaTime,Seconds)){bReady=false;UE_LOG(LogTemp,Error,TEXT("Invalid continuous day/night tick"));return;}
 for(const auto& Pair:CardIds)
 {
  auto* Card=Pair.Key.Get();if(!Card)continue;auto* MID=Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0));if(!MID)continue;
  const auto R=State.ResultFor(Pair.Value);
  MID->SetVectorParameterValue(TEXT("Color1"),R.Color1);MID->SetVectorParameterValue(TEXT("Color2"),R.Color2);
  MID->SetVectorParameterValue(TEXT("SpriteColor"),R.SpriteColor);MID->SetVectorParameterValue(TEXT("NightLight"),R.NightLight);
 }
 if(!bEnvironmentBound)BindEnvironment();
 for(const auto& Weak:EnvironmentMaterials)if(auto* MID=Weak.Get())MID->SetVectorParameterValue(TEXT("UnitySpriteColor"),State.SpriteColor);
}
