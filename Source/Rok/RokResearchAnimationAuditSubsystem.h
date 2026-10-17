#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokResearchAnimationAuditSubsystem.generated.h"

class AActor;
class FJsonObject;
class FJsonValue;
class URokMainHudWidget;
class URokResearchScreenWidget;
class URokUnityFeatureScreenWidget;
class UWidget;

/** Opt-in, reference-only GameApp HUD animation audit. Never submits a research command. */
UCLASS()
class ROK_API URokResearchAnimationAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<URokMainHudWidget> Hud;
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
    TWeakObjectPtr<URokResearchScreenWidget> Research;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> BuildingSnapshots;
    TArray<TArray<uint8>> TroopSnapshots;
    TArray<uint8> ProfileSnapshot, ResourceSnapshot;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> InputHashes, Baseline, Pending, FailureState;
    FString Directory, PendingPath, InventoryFingerprint;
    FDelegateHandle ShotHandle;
    double Elapsed=0.0, StepStarted=0.0, ShotStarted=0.0;
    float CloseRepeatBefore=0.0f, CloseAtChildSwitch=0.0f, CloseAtMaskClick=0.0f;
    int32 Step=0, QueueCount=0, OwnedQueueCount=0, InitialInterruptCount=0;
    bool bOwnsWorld=false, bFinished=false, bBaseline=false;
    bool Begin();
    bool Identity();
    bool Expect(bool Condition, const TCHAR* Error);
    bool Unchanged();
    bool ValidateResearch(bool bDetail, bool bPower, int32 StudyType);
    bool SlateClick(FName Name);
    bool ScrollEconomyToEnd();
    bool CheckClock(const TCHAR* Phase, float MinSeconds, float MaxSeconds);
    bool CheckCloseClock(float MinSeconds, float MaxSeconds);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error=FString());
};
