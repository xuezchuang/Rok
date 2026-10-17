#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokWorldDayNightState.h"
#include "RokWorldDayNightSubsystem.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
UCLASS()
class ROK_API URokWorldDayNightSubsystem:public UTickableWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Tick(float DeltaTime) override;
 virtual TStatId GetStatId() const override;
 void RegisterSourceCard(UStaticMeshComponent* Card,const FString& SourceTransformId);
 void UpdateSourceActive(UStaticMeshComponent* Card,bool bActive);
 const FRokWorldDayNightState& GetLocalState() const {return State;}
 bool IsReady() const {return bReady;}
 bool ReanchorLocalSimulationSeconds(int64 Seconds);
 int32 GetEnvironmentMaterialCount() const {return EnvironmentMaterials.Num();}
 const TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>& GetEnvironmentMaterials() const {return EnvironmentMaterials;}
protected:
 virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
 FRokWorldDayNightState State;
 TMap<TWeakObjectPtr<UStaticMeshComponent>,FString> CardIds;
 TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> EnvironmentMaterials;
 double ClockAnchor=0;
 int64 LocalBaseSeconds=0;
 bool bReady=false,bEnvironmentBound=false;
 void BindEnvironment();
};
