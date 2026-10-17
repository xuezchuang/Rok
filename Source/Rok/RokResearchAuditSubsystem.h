#pragma once

#include "CoreMinimal.h"
#include "RokResearchCatalog.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokResearchAuditSubsystem.generated.h"

class URokMainHudWidget;
class AActor;
class URokUnityFeatureScreenWidget;
class URokResearchScreenWidget;
class UUserWidget;
class UWidget;
class UScrollBox;
class FJsonObject;
class FJsonValue;

/** Opt-in HUD research-reference presentation audit; never submits research. */
UCLASS()
class ROK_API URokResearchAuditSubsystem : public UTickableWorldSubsystem
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
    FRokResearchCatalog Catalog;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> BuildingSnapshots;
    TArray<TArray<uint8>> TroopSnapshots;
    TArray<uint8> ProfileSnapshot, ResourceSnapshot;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> InputHashes, Baseline, Pending, FailureState;
    FString Directory, PendingPath, CatalogHash, InventoryFingerprint;
    FDelegateHandle ShotHandle;
    int32 Step = 0, QueueCount = 0, OwnedQueueCount = 0;
    int32 ExpectedType = 1, ExpectedStudyType = 0, ExpectedLevel = 1;
    int32 ExpectedBoundaryActionId = INDEX_NONE;
    double Elapsed = 0, StepStarted = 0, ShotStarted = 0;
    bool bOwnsWorld = false, bFinished = false, bBaseline = false;
    enum class EMode : uint8 { Research, Mail, Closed };
    EMode ExpectedMode = EMode::Research;
    bool bExpectDetail = false, bExpectPower = false;
    bool Begin();
    bool Identity();
    bool Expect(bool Condition, const TCHAR* Error);
    bool Unchanged();
    bool Validate();
    bool SlateClick(FName Name);
    bool VerifyLocalBoundary() const;
    bool DelegateClick(UUserWidget* Owner, FName Name);
    bool ScrollToEnd(int32 Type);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error = FString());
};
