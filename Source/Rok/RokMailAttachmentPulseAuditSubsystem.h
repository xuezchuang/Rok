#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokMailAttachmentPulseAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class UUserWidget;
class UWidget;
class FJsonValue;
class FJsonObject;
class URokMailAttachmentMeshWidget;

/** Opt-in real NativeTick mail-reference pulse audit. Never claims attachments. */
UCLASS()
class ROK_API URokMailAttachmentPulseAuditSubsystem : public UTickableWorldSubsystem
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
    FString Directory, PendingPath;
    FDelegateHandle ScreenshotHandle;
    TArray<TSharedPtr<FJsonValue>> Samples, Captures, MeshSamples;
    TWeakObjectPtr<URokMailAttachmentMeshWidget> RetainedMesh;
    float PausedMeshTime = 0, PreviousMeshTime = 0;
    uint64 PausedPaintCount = 0;
    float ActionMeshTime = 0;
    uint64 ActionLoadGeneration = 0, ActionFrame = 0;
    double ActionStarted = 0, ActionSlateBudget = 0;
    bool bPauseBaselineSet = false;
    int32 MeshLoops = 0;
    bool bSelectionContinued = false, bTabPaused = false, bTabResumed = false, bFreshAfterClose = false;
    bool bFreshReusedObjectAddress = false;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    double Elapsed = 0, StepStarted = 0, TraceStarted = 0, ScreenshotStarted = 0;
    int32 Step = 0, Peaks = 0;
    bool bFinished = false, bAbovePeak = false;
    bool Begin();
    bool Identity();
    bool Click(UUserWidget* Owner, const TCHAR* Name);
    bool ValidateEmpty();
    bool ValidateReference();
    UWidget* Pulse(int32 Index) const;
    URokMailAttachmentMeshWidget* Mesh(int32 Index) const;
    void RecordMesh(const TSharedPtr<FJsonObject>& Object, const FString& Prefix) const;
    bool RecordSourceGeometry(const TSharedPtr<FJsonObject>& Object) const;
    void RecordDeferredAction(const TCHAR* Name);
    bool ValidateDeferredPhase(float BeforeTime);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    void Finish(const FString& Failure = FString());
};
