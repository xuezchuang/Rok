#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokMailLocalUnreadNativeSlateAuditSubsystem.generated.h"

class FJsonObject;
class FJsonValue;
class FSlateVirtualUserHandle;
class UButton;
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class URokServerSubsystem;
class UWidget;

/** Opt-in, local-only real-HUD mail audit; never substitutes a direct handler call for Slate input. */
UCLASS()
class ROK_API URokMailLocalUnreadNativeSlateAuditSubsystem : public UTickableWorldSubsystem
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
    bool Expect(bool Condition, const TCHAR* Message);
    bool Visible(const UWidget* Widget) const;
    UButton* HudButton(const TCHAR* Name) const;
    UButton* MailButton(const TCHAR* Name) const;
    bool Click(UButton* Button, const TCHAR* Label);
    bool Observe(const TCHAR* Phase, int32 Tab, int32 SelectedIndex, int32 SystemUnread,
        int32 ReportUnread, bool bSystemRead, bool bReportRead, bool bLocal,
        bool bDetail, bool bEmpty);
    void Capture(const TCHAR* Phase, int32 Next);
    void ScreenshotProcessed();
    void Enter(int32 Next);
    void Finish(const FString& Error = FString());
    bool Save(bool bPassed) const;

    TWeakObjectPtr<URokMainHudWidget> Hud;
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
    TWeakObjectPtr<URokServerSubsystem> Server;
    TWeakObjectPtr<UButton> StableTarget;
    TSharedPtr<FSlateVirtualUserHandle> VirtualUser;
    TSharedPtr<FJsonObject> PendingCapture;
    TSharedPtr<FJsonObject> Inputs;
    TArray<TSharedPtr<FJsonValue>> Actions;
    TArray<TSharedPtr<FJsonValue>> Observations;
    TArray<TSharedPtr<FJsonValue>> Captures;
    TArray<FString> Failures;
    TSet<FString> ObservedPhases;
    FDelegateHandle ScreenshotHandle;
    FString Directory;
    FString PendingPng;
    FString Mode;
    FString Nonce;
    FString StartedUtc;
    int32 ProcessId = 0;
    FVector2D LastPointer = FVector2D::ZeroVector;
    FVector2D StableTopLeft = FVector2D::ZeroVector;
    FVector2D StableBottomRight = FVector2D::ZeroVector;
    uint64 StableFrame = 0;
    uint64 ScreenshotFrame = 0;
    double StartedAt = 0;
    double StepStartedAt = 0;
    int32 Step = 0;
    int32 NextStep = 0;
    int32 StableFrames = 0;
    int32 ClickStableFrames = 0;
    bool bFinished = false;
    bool bScreenshotDone = false;
    bool bSameWidgetReopen = false;
};
