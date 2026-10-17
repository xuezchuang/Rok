#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokHospitalExplanationAuditSubsystem.generated.h"
class URokMainHudWidget;
class URokUnityFeatureScreenWidget;
class UUserWidget;
class UWidget;
class UScrollBox;
class FJsonObject;
class FJsonValue;

/** Opt-in read-only hospital explanation route; real delegates, never treatment. */
UCLASS()
class ROK_API URokHospitalExplanationAuditSubsystem : public UTickableWorldSubsystem
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
    enum class EPage { City, Main, Explanation };
    EPage Page = EPage::City;
    TWeakObjectPtr<URokMainHudWidget> Hud;
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
    TWeakObjectPtr<AActor> Hospital;
    TWeakObjectPtr<UScrollBox> OriginalScroll;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> BuildingSnapshots;
    TArray<uint8> ProfileSnapshot, ResourceSnapshot;
    TMap<FName, FString> MainTexts;
    TMap<FName, bool> MainOptions;
    FString ExpectedRichText, Directory, PendingPath;
    TArray<FString> YellowSegments, Failures;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TSharedPtr<FJsonObject> Pending;
    FDelegateHandle ScreenshotHandle;
    double Elapsed = 0, StepStarted = 0, ShotStarted = 0;
    float BottomOffset = 0;
    int32 Step = 0, InitialQueueCount = 0, InitialOwnedQueues = 0;
    bool bFinished = false, bBaseline = false, bExpectBottom = false;
    bool Begin();
    bool Identity();
    bool Unchanged();
    bool Validate();
    bool Click(UUserWidget* Owner, FName Name);
    bool Expect(bool Condition, const TCHAR* Error);
    UWidget* Find(FName Name) const;
    UScrollBox* Scroll() const;
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error = FString());
};
