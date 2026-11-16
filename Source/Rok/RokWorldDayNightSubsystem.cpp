#include "RokWorldDayNightSubsystem.h"
#include "RokGameNetSubsystem.h"
#include "RokBuildingSelectionIndicator.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokCitySpriteActor.h"
#include "Camera/CameraComponent.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapEntityActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

bool URokWorldDayNightSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
 const UWorld* World=Cast<UWorld>(Outer);
 if (!World || IsRunningDedicatedServer() || !Super::ShouldCreateSubsystem(Outer)) return false;
 const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
 return Map == TEXT("/Game/Maps/L_RokUnityWorldMapPrototype")
  || Map == TEXT("/Game/Maps/L_RokUnityGameAppCity");
}
bool URokWorldDayNightSubsystem::DoesSupportWorldType(EWorldType::Type Type) const{return Type==EWorldType::Game || Type==EWorldType::PIE;}
void URokWorldDayNightSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 bInteriorCity = UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == TEXT("/Game/Maps/L_RokUnityGameAppCity");
 int32 Seed=int32(FPlatformTime::Cycles());
 FParse::Value(FCommandLine::Get(),TEXT("RokWorldDayNightSeed="),Seed);
 // Explicit audit time can override the synchronized server clock.
 if ((FParse::Param(FCommandLine::Get(),TEXT("RokWorldDayNightLocal"))
   && (!FParse::Value(FCommandLine::Get(),TEXT("RokWorldDayNightLocalSeconds="),LocalBaseSeconds) || LocalBaseSeconds<1))
   || !State.Initialize(Seed))
 {UE_LOG(LogTemp,Error,TEXT("World day/night needs valid source data and an explicit epoch in local mode."));return;}
 bReady=true;
}
TStatId URokWorldDayNightSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorldDayNightSubsystem,STATGROUP_Tickables);}
bool URokWorldDayNightSubsystem::ReanchorLocalSimulationSeconds(int64 Seconds)
{
 if(!bReady||LocalBaseSeconds<1||Seconds<1)return false;LocalBaseSeconds=Seconds;ClockAnchor=FPlatformTime::Seconds();return true;
}
void URokWorldDayNightSubsystem::RegisterSourceCard(UStaticMeshComponent* Card,const FString& Transform)
{
 if(!bReady||!Card)return;
 const auto* Owner=Cast<ARokWorldMapEntityActor>(Card->GetOwner());
 if(!Owner)return;
 const FString ServerId = Owner->GetServerObjectId();
 const FString Id = ServerId.IsEmpty() ? FString::Printf(TEXT("%lld/%s"),int64(Owner->GetSourceId()),*Transform)
  : FString::Printf(TEXT("server/%s/%s/%s"), *ServerId, *Owner->GetName(), *Transform);
 if(CardIds.Contains(Card))return;
 auto* MID=Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0));FLinearColor Value;
 if(!MID||!MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("NightLight")),Value))
 {UE_LOG(LogTemp,Warning,TEXT("Continuous source material lacks NightLight: %s"),*Id);return;}
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
 UMaterialInterface* GroundGamma = LocalBaseSeconds > 0 ? nullptr : LoadObject<UMaterialInterface>(nullptr,
  TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldGroundGammaLighting.M_RokWorldGroundGammaLighting"));
 for(TActorIterator<AActor> It(GetWorld());It;++It)
 {
  if (auto* City = Cast<ARokWorldMapCityActor>(*It)) HomeCity = City;
  TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
  for(auto* Mesh:Meshes)for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
  {
   auto* Material=Mesh->GetMaterial(Slot);if(!Material)continue;
   const FString Path=GetPathNameSafe(Material->GetBaseMaterial());
   const bool bGround = Path.StartsWith(TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldGround_"));
   if(!bGround && !Path.StartsWith(TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_")))continue;
   FLinearColor Value;if(!Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")),Value))continue;
   if (bGround && GroundGamma)
   {
    TArray<UTexture*> Textures;
    Material->GetUsedTextures(Textures);
    if (Textures.Num() == 1 && Textures[0])
    {
     if (auto* MID = Mesh->CreateDynamicMaterialInstance(Slot, GroundGamma))
     {
      MID->SetTextureParameterValue(TEXT("GroundTexture"), Textures[0]);
      EnvironmentMaterials.Add(MID);
      continue;
     }
    }
   }
   if(auto* MID=Mesh->CreateDynamicMaterialInstance(Slot))EnvironmentMaterials.Add(MID);
  }
 }
 bEnvironmentBound=true;
 UE_LOG(LogTemp,Display,TEXT("World lighting bound %d environment materials (clock=%s)."),
  EnvironmentMaterials.Num(), LocalBaseSeconds > 0 ? TEXT("local") : TEXT("server"));
}
void URokWorldDayNightSubsystem::Tick(float DeltaTime)
{
 if(!bReady||!GetWorld()->HasBegunPlay())return;
 if(ClockAnchor==0)ClockAnchor=FPlatformTime::Seconds();
 int64 Seconds = 0;
 if (LocalBaseSeconds > 0) Seconds = LocalBaseSeconds + int64(FPlatformTime::Seconds() - ClockAnchor);
 else
 {
  const UGameInstance* Game = GetWorld()->GetGameInstance();
  const auto* Net = Game ? Game->GetSubsystem<URokGameNetSubsystem>() : nullptr;
  double SynchronizedSeconds = 0.0;
  if (!Net || !Net->TryGetServerTimeSeconds(SynchronizedSeconds)) return;
  Seconds = int64(SynchronizedSeconds);
 }
 if (bInteriorCity)
 {
  if (State.Tick(DeltaTime, Seconds)) ApplyInteriorLighting();
  return;
 }
 // Loaded UE materials complete previously dispatched requests in their
 // arrival order on this frame. This is an explicit warm-asset policy, not a
 // claim about Addressables cold-load latency; state exposes completion IDs.
 for(auto It=CardIds.CreateIterator();It;++It)
 {
  if(!It.Key().IsValid()){State.Layers.Remove(It.Value());State.RegistrationOrder.Remove(It.Value());It.RemoveCurrent();continue;}
  const auto* Layer=State.Layers.Find(It.Value());if(!Layer)continue;
  const auto Requests=Layer->Loading;for(const auto& R:Requests)State.CompleteMaterialRequest(It.Value(),R.Serial);
 }
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
 if (auto* City = HomeCity.Get())
 {
  // city_building_night.mat _Color_3, modulated by the source shader pulse.
  const FLinearColor Light = State.bNight
   ? FLinearColor(0.6509804f, 0.3292642f, 0.047058832f) * RokVillageDayNight::NightPulse(State.ScaledTime)
   : FLinearColor::Black;
  City->ApplyDayNightLighting(State.SpriteColor, Light);
 }
}

void URokWorldDayNightSubsystem::BindInteriorWindowMesh(UStaticMeshComponent* Mesh)
{
	if (!Mesh) return;
	UMaterialInterface* Source = Mesh->GetMaterial(0);
	if (!Source) return;
	if (const auto* Bound = InteriorMaterials.Find(Mesh); Bound && Bound->Get() == Source) return;

	UTexture* Sprite = nullptr;
	UTexture* Mask = nullptr;
	if (!Source->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Sprite")), Sprite)
		|| !Source->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Mask")), Mask)
		|| !Sprite || !Mask)
	{
		return;
	}
	// Reuse the city's RGB-mask shader, retaining each mesh's own sprite,
	// mask, faction colors and opacity instead of the material's hall defaults.
	if (auto* Instance = Mesh->CreateDynamicMaterialInstance(0, InteriorWindowMaterial))
	{
		Instance->SetTextureParameterValue(TEXT("Sprite"), Sprite);
		Instance->SetTextureParameterValue(TEXT("Mask"), Mask);
		for (const FName Name : {FName(TEXT("Color1")), FName(TEXT("Color2")),
			FName(TEXT("RendererColor")), FName(TEXT("SpriteColor"))})
		{
			FLinearColor Value;
			if (Source->GetVectorParameterValue(FMaterialParameterInfo(Name), Value))
				Instance->SetVectorParameterValue(Name, Value);
		}
		float Transparency = 1.0f;
		if (Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("CityTransparency")), Transparency))
			Instance->SetScalarParameterValue(TEXT("CityTransparency"), Transparency);
		InteriorMaterials.Add(Mesh, Instance);
	}
}

void URokWorldDayNightSubsystem::ApplyInteriorLighting()
{
	const auto* Controller = GetWorld()->GetFirstPlayerController();
	auto* Camera = Controller && Controller->GetPawn()
		? Controller->GetPawn()->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!Camera) return;

	// Legacy city terrain, walls and props already bake the Unity day colors.
	// Grade the rendered scene relative to that preset so their blending and
	// depth behavior are preserved. Slate HUD colors remain independent.
	const auto& Day = State.Data.Presets[1];
	const FLinearColor DayColor = RokVillageDayNight::SpriteColor(Day, Day, 0.0f, 1.0f);
	const FLinearColor GammaGain = State.SpriteColor / DayColor;
	const bool bNeedsGrade = !GammaGain.Equals(FLinearColor::White, KINDA_SMALL_NUMBER);
	if (bNeedsGrade && !bInteriorGradeLoadAttempted)
	{
		bInteriorGradeLoadAttempted = true;
		auto* Base = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/RokUnity/City/Lighting/M_RokCityDayNightGamma.M_RokCityDayNightGamma"));
		if (Base) InteriorGrade = UMaterialInstanceDynamic::Create(Base, this);
		InteriorArrowMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldGroundGammaLighting.M_RokWorldGroundGammaLighting"));
	}
	if (!InteriorGrade) return;
	InteriorGrade->SetVectorParameterValue(TEXT("SceneGammaGain"), GammaGain);
	Camera->PostProcessSettings.AddBlendable(InteriorGrade.Get(), bNeedsGrade ? 1.0f : 0.0f);

	// World-space placement markers are guides, independent of the time of day.
	const FLinearColor InverseGain = FLinearColor::White / GammaGain;
	if (InteriorArrowMaterial)
	{
		for (TActorIterator<ARokBuildingSelectionIndicator> It(GetWorld()); It; ++It)
			if (!It->IsHidden()) It->ApplySceneGammaCompensation(InteriorArrowMaterial, InverseGain);
	}
	const FLinearColor LinearCompensation(
		FMath::Pow(InverseGain.R, 2.2f), FMath::Pow(InverseGain.G, 2.2f), FMath::Pow(InverseGain.B, 2.2f));
	for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
		It->SetFootprintLightingCompensation(LinearCompensation);

	if (State.ScaledTime >= NextInteriorMaterialScan)
	{
		NextInteriorMaterialScan = State.ScaledTime + 0.5;
		if (!InteriorWindowMaterial && State.bNight)
			InteriorWindowMaterial = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldPlayerCity_build_1_101_2.M_WorldPlayerCity_build_1_101_2"));
		if (InteriorWindowMaterial && State.bNight)
		{
			for (TActorIterator<ARokCitySpriteActor> It(GetWorld()); It; ++It)
				BindInteriorWindowMesh(It->GetSpriteMeshComponent());
			for (TActorIterator<ARokCityPlacementPreviewActor> It(GetWorld()); It; ++It)
				BindInteriorWindowMesh(It->GetBuildingMeshComponent());
		}
	}
	// Unity adds the blue-channel window light after its Gamma lighting
	// multiplier. Compensate for the camera's subsequent scene-wide grade.
	const FLinearColor WindowLight = State.bNight
		? FLinearColor(0.6509804f, 0.3292642f, 0.047058832f) * RokVillageDayNight::NightPulse(State.ScaledTime) / GammaGain
		: FLinearColor::Black;
	for (auto It = InteriorMaterials.CreateIterator(); It; ++It)
	{
		auto* Mesh = It.Key().Get();
		auto* Instance = It.Value().Get();
		if (!Mesh || !Instance || Mesh->GetMaterial(0) != Instance)
		{
			It.RemoveCurrent();
			continue;
		}
		Instance->SetVectorParameterValue(TEXT("NightLight"), WindowLight);
	}
}
