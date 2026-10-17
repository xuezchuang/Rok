#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokWorldDayNightAuditSubsystem.generated.h"
class ARokWorldMapCameraPawn;
class ARokWorldMapEntityActor;
class URokWorldDayNightSubsystem;
class UStaticMeshComponent;
class FJsonValue;
class FJsonObject;
UCLASS()
class ROK_API URokWorldDayNightAuditSubsystem:public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override;
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
 TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
 TWeakObjectPtr<ARokWorldMapEntityActor> Village;
 TWeakObjectPtr<URokWorldDayNightSubsystem> Lighting;
 TArray<TSharedPtr<FJsonValue>> Frames,Captures;
 TArray<FString> Failures;
 TSharedPtr<FJsonObject> Pending;
 struct FGroveBinding
 {
  TWeakObjectPtr<AActor> Actor;
  TWeakObjectPtr<UStaticMeshComponent> Component;
  FString TileTag,IndexTag;
  FTransform SourceTransform,InitialActorTransform,InitialComponentTransform;
 };
 TArray<FGroveBinding> Groves;
 FString GroveSourceHash;
 FString Directory,PendingPath;
 FIntPoint ObservedViewportSize=FIntPoint::ZeroValue;
 FDelegateHandle ScreenshotHandle;
 double Elapsed=0,StageStarted=0;
 int32 Stage=-1;
 bool bFinished=false,bOwnsTargetWorld=false;
 bool Begin();
 bool BindGroves();
 bool InspectGroves(TArray<TSharedPtr<FJsonValue>>& Out);
 bool Inspect(TSharedPtr<FJsonObject>& Out,int32& Nights,int32& Visible);
 void Next();
 void Processed();
 void Finish(const FString& Failure=FString());
 bool Save();
};
