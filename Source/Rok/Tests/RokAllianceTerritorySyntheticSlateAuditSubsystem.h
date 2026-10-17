#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokAllianceTerritorySyntheticSlateAuditSubsystem.generated.h"

class ARokPlayerController;
class FJsonObject;
class FJsonValue;
class FSlateVirtualUserHandle;
class UButton;
class URokBuildingUpgradeSubsystem;
class URokMainHudWidget;
class URokServerSubsystem;
class URokUnityFeatureScreenWidget;
class URokUnityModalWidget;
class URokUnityRuntimeStateSubsystem;

/** Opt-in, local-only Territory configuration audit through real offscreen Slate hits. */
UCLASS()
class ROK_API URokAllianceTerritorySyntheticSlateAuditSubsystem : public UTickableWorldSubsystem
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
    bool Identity() const;
    bool Begin();
    bool Expect(bool Condition, const TCHAR* Reason);
    bool Boundary() const;
    FString BusinessDigest() const;
    bool Ready(int32 Phase, int32 Group = INDEX_NONE, int32 Type = 0) const;
    bool Observe(int32 Phase, int32 Group, int32 Type, const TCHAR* Label);
    bool TargetInPath(UButton* Target) const;
    bool WheelTowards(UButton* Target);
    bool Click(UButton* Target, int32 ExpectedActionId, const TCHAR* Label);
    void Capture(const TCHAR* Phase, int32 Next);
    void ScreenshotProcessed();
    void AdvanceCapture();
    void Enter(int32 Next);
    void Finish(const FString& Error = FString());
    bool Save(bool bComplete) const;

    TWeakObjectPtr<ARokPlayerController> Controller;
    TWeakObjectPtr<URokMainHudWidget> Hud;
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
    TWeakObjectPtr<URokUnityModalWidget> Modal;
    TWeakObjectPtr<URokBuildingUpgradeSubsystem> Buildings;
    TWeakObjectPtr<URokUnityRuntimeStateSubsystem> Runtime;
    TWeakObjectPtr<URokServerSubsystem> Server;
    TWeakObjectPtr<UButton> StableTarget;
    TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
    FDelegateHandle ScreenshotHandle;
    TSharedPtr<FJsonObject> Inputs;
    TSharedPtr<FJsonObject> Pending;
    TArray<TSharedPtr<FJsonValue>> Actions;
    TArray<TSharedPtr<FJsonValue>> Wheels;
    TArray<TSharedPtr<FJsonValue>> Observations;
    TArray<TSharedPtr<FJsonValue>> Captures;
    TArray<FString> Failures;
    FString Directory;
    FString PendingPath;
    FString BaselineBusiness;
    FString ForegroundAtBegin;
    FVector2D LastPointer = FVector2D::ZeroVector;
    FVector2D StableTopLeft = FVector2D::ZeroVector;
    FVector2D StableBottomRight = FVector2D::ZeroVector;
    uint64 StableFrame = 0;
    uint64 ActionFrame = 0;
    uint64 ScreenshotFrame = 0;
    double StartedAt = 0.0;
    double StepStartedAt = 0.0;
    int32 Step = 0;
    int32 NextStep = 0;
    int32 StableFrames = 0;
    int32 ClickStableFrames = 0;
    int32 GroupCursor = 0;
    int32 RowCursor = 0;
    int32 WheelCount = 0;
    bool bBegun = false;
    bool bBaselineSet = false;
    bool bScreenshotDone = false;
    bool bFinished = false;
};
