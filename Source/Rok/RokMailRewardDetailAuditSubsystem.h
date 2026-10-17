#pragma once

#include "CoreMinimal.h"
#include "RokMailRewardDetails.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokMailRewardDetailAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class UUserWidget;
class UWidget;
class FJsonObject;
class FJsonValue;

/** Opt-in actual HUD and explicit local-fixture mail reward presentation audit. */
UCLASS()
class ROK_API URokMailRewardDetailAuditSubsystem : public UTickableWorldSubsystem
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
    TMap<int32, FRokMailRewardPayload> FixturePayloads;
    TMap<int32, FRokMailRewardPayload> FixtureBaseline;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> Buildings;
    TArray<TArray<uint8>> TroopStacks;
    TArray<uint8> Profile, Resources;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Inputs, LocalCountFont, Pending, FailureState;
    FString Directory, PendingPath, FixtureSha256, InventoryFingerprint, ScreenFixtureFingerprint;
    FDelegateHandle ShotHandle;
    int32 Step = 0, ExpectedPage = 0, ExpectedTooltipKind = 0, OwnedQueues = 0;
    double Elapsed = 0, StepStarted = 0, ShotStarted = 0;
    bool bOwnsTargetWorld = false, bFinished = false, bBaseline = false, bScrolledMailDetail = false;
    bool Begin();
    bool Identity();
    bool Expect(bool Condition, const TCHAR* Error);
    bool Unchanged();
    bool Validate();
    bool Click(UUserWidget* Owner, FName Name);
    bool SlateClick(FName Name);
    bool VerifyClaimIsLocalFeedbackOnly() const;
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error = FString());
};
