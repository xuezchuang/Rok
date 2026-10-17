#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCommanderSyntheticSlateAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class FSlateVirtualUserHandle;
class UButton;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokUnityGameplayCatalogSubsystem;
class URokUnityRuntimeStateSubsystem;
class URokServerSubsystem;

/** Opt-in GameAppCity real-HUD audit using in-process synthetic Slate mouse events. */
UCLASS()
class ROK_API URokCommanderSyntheticSlateAuditSubsystem : public UTickableWorldSubsystem
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
    bool Begin();
    bool Identity() const;
    bool Expect(bool Condition, const TCHAR* Error);
    bool Click(UButton* Button, const TCHAR* Label);
    bool ObserveSort(int32 SortType, const TCHAR* Phase);
    bool Ready(bool bOpen, int32 SortType) const;
    void Capture(const TCHAR* Phase, int32 Next);
    void ScreenshotProcessed();
    void AdvanceCapture();
    void Enter(int32 Next);
    void Finish(const FString& Error = FString());
    bool Save(bool bComplete) const;

    TWeakObjectPtr<ARokPlayerController> Controller;
    TWeakObjectPtr<URokMainHudWidget> Hud;
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
    TWeakObjectPtr<URokUnityGameplayCatalogSubsystem> Catalog;
    TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
    TWeakObjectPtr<URokServerSubsystem> Server;
    TWeakObjectPtr<UButton> StableTarget;
    FDelegateHandle ScreenshotHandle;
    TArray<TSharedPtr<FJsonValue>> Actions;
    TArray<TSharedPtr<FJsonValue>> Captures;
    TArray<TSharedPtr<FJsonValue>> Observations;
    TArray<FString> Failures;
    TArray<FString> QualityOrder;
    TArray<FString> PersistedOrder;
    TSharedPtr<FJsonObject> Inputs;
    TSharedPtr<FJsonObject> FeatureBarSetup;
    TSharedPtr<FJsonObject> Pending;
    TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
    FString Directory;
    FString PendingPath;
    FString ForegroundAtBegin;
    FString PersistedLabel;
    FString InitialFeatureParentState;
    FVector2D LastPointer = FVector2D::ZeroVector;
    FVector2D StableTopLeft = FVector2D::ZeroVector;
    FVector2D StableBottomRight = FVector2D::ZeroVector;
    uint64 StableFrame = 0;
    uint64 ScreenshotFrame = 0;
    double StartedAt = 0.0;
    double StepStartedAt = 0.0;
    int32 Step = 0;
    int32 NextStep = 0;
    int32 StableFrames = 0;
    int32 ClickStableFrames = 0;
    bool bBegun = false;
    bool bScreenshotDone = false;
    bool bReopenOptionVerified = false;
    bool bFeatureSetupVerified = false;
    bool bFinished = false;
};
