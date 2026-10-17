#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokMountainAGammaAuditSubsystem.generated.h"
class ARokWorldMapCameraPawn;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class FJsonObject;
class FJsonValue;
UCLASS()
class ROK_API URokMountainAGammaAuditSubsystem : public UTickableWorldSubsystem
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
 TWeakObjectPtr<UStaticMeshComponent> Mountain;
 TWeakObjectPtr<UMaterialInstanceDynamic> Material;
 TWeakObjectPtr<UTexture2D> Raw;
 TArray<FLinearColor> Colors;
 TArray<FString> Names,Failures;
 TArray<TSharedPtr<FJsonValue>> Captures;
 TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>,FIntPoint>> SavedVisibility;
 TArray<TSharedPtr<FJsonValue>> VisibilityRecords;
 TSharedPtr<FJsonObject> Pending;
 FDelegateHandle ScreenshotHandle;
 FString Directory,PendingPath;
 double Elapsed=0,StageStart=0;
 uint64 StageFrame=0;
 int32 Stage=-1;
 bool bFinished=false;
 bool bIsolate=false;
 bool RestoreVisibility();
 bool Begin();
 bool Inspect(TSharedPtr<FJsonObject>& Out,bool& bFramed);
 void Processed();
 void Finish(const FString& Failure=FString());
};
