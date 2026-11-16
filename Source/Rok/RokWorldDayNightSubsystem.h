#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokWorldDayNightState.h"
#include "RokWorldDayNightSubsystem.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ARokWorldMapCityActor;
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
 TWeakObjectPtr<ARokWorldMapCityActor> HomeCity;
 double ClockAnchor=0;
 int64 LocalBaseSeconds=0;
 bool bReady=false,bEnvironmentBound=false;
 void BindEnvironment();

 bool bInteriorCity = false;
 double NextInteriorMaterialScan = 0.0;
 UPROPERTY(Transient)
 TObjectPtr<UMaterialInterface> InteriorWindowMaterial;
 UPROPERTY(Transient)
 TObjectPtr<UMaterialInterface> InteriorArrowMaterial;
 UPROPERTY(Transient)
 TObjectPtr<UMaterialInstanceDynamic> InteriorGrade;
 bool bInteriorGradeLoadAttempted = false;
 TMap<TWeakObjectPtr<UStaticMeshComponent>, TWeakObjectPtr<UMaterialInstanceDynamic>> InteriorMaterials;
 void ApplyInteriorLighting();
 void BindInteriorWindowMesh(UStaticMeshComponent* Mesh);
};
