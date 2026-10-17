#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"
#include "RokGraphicsSettingsAuditSubsystem.generated.h"
class URokMainHudWidget;
class URokUnityModalWidget;
class URokUnityFeatureScreenWidget;
class URokClientSettingsSubsystem;
class UUserWidget;
class FJsonObject;
class FJsonValue;
UCLASS()
class ROK_API URokGraphicsSettingsAuditSubsystem : public UTickableWorldSubsystem
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
 struct FFileSnapshot
 {
  FString Key, Path;
  TArray<uint8> Bytes;
  bool Exists=false;
  TOptional<FConfigFile> Cache, FinalCombined;
  TOptional<FConfigCommandStream> SavedLayer, RuntimeChanges;
 };
 struct FCVarSnapshot { FString Value; uint32 Flags=0; };
 TArray<FFileSnapshot> Files;
 TMap<FString,FCVarSnapshot> CVars;
 TArray<uint8> ClientBytes, UserBytes;
 Scalability::FQualityLevels OriginalQuality, OriginalUserQuality;
 bool OriginalDynamicResolution=false;
 TWeakObjectPtr<URokMainHudWidget> Hud;
 TWeakObjectPtr<URokUnityModalWidget> Modal;
 TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
 TWeakObjectPtr<URokClientSettingsSubsystem> Settings;
 TSharedPtr<FJsonObject> Pending, InitialState, DiskState;
 TArray<TSharedPtr<FJsonValue>> Captures, Actions;
 TArray<FString> Failures;
 FString Directory, PendingPath;
 FDelegateHandle ScreenshotHandle;
 double Elapsed=0, StepStarted=0, ScreenshotStarted=0;
 int32 Step=0, ExpectedGraphics=0, ExpectedFrame=0;
 bool bFinished=false, bSnapshot=false, bRestored=false, bApplied=false;
 bool Begin();
 bool Snapshot();
 bool Restore();
 bool Validate();
 bool ValidateDisk();
 bool Click(UUserWidget* Owner,const TCHAR* Name);
 bool SetSlider(const TCHAR* Name,int32 Value);
 void Next();
 void Capture(const TCHAR* Name);
 void ScreenshotProcessed();
 TSharedRef<FJsonObject> State() const;
 bool Save(bool Complete);
 void Finish(const FString& Error=FString());
};
