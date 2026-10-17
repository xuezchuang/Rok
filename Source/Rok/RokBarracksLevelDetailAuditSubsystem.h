#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokBarracksLevelDetailAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class URokBarracksSourceTextData;
class UUserWidget;
class FJsonObject;
class FJsonValue;

/** Opt-in real-city, read-only barracks upgrade level-detail navigation audit. */
UCLASS()
class ROK_API URokBarracksLevelDetailAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<URokUnityModalWidget> Modal;
    TWeakObjectPtr<AActor> Barracks;
    UPROPERTY(Transient) TObjectPtr<URokBarracksSourceTextData> HeaderData;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> Buildings;
    TArray<uint8> Profile, Resources;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FJsonObject> Inputs;
    TSharedPtr<FJsonObject> FailureState;
    FString Directory, PendingPath;
    FDelegateHandle ShotHandle;
    int32 Step = 0, Page = 0, OwnedQueues = 0;
    float ScrolledOffset = 0.0f;
    double Elapsed = 0, StepStarted = 0, ShotStarted = 0;
    bool bOwnsTargetWorld = false, bFinished = false, bBaseline = false;
    bool Begin();
    bool Identity();
    bool Unchanged();
    bool Validate();
    bool Expect(bool Condition, const TCHAR* Error);
    bool Click(UUserWidget* Owner, FName Name);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error = FString());
};
