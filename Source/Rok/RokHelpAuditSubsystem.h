#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokHelpAuditSubsystem.generated.h"
class URokMainHudWidget;
class URokUnityModalWidget;
class URokUnityFeatureScreenWidget;
class URokHelpWidget;
class UUserWidget;
class UScrollBox;
class FJsonObject;
class FJsonValue;

UCLASS()
class ROK_API URokHelpAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<URokUnityFeatureScreenWidget> Feature;
    TWeakObjectPtr<URokHelpWidget> Help;
    TWeakObjectPtr<UScrollBox> OriginalScroll;
    TArray<uint8> InitialProfile,InitialResources;
    TMap<int32,TPair<FString,FString>> SourceQuestions;
    TArray<TSharedPtr<FJsonValue>> Captures,Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    FDelegateHandle ScreenshotHandle;
    FString Directory,PendingPath;
    double Elapsed=0,StageStarted=0,ShotStarted=0;
    uint64 Frame=0;
    int32 Stage=0;
    bool bFinished=false,bBaseline=false;
    bool Begin();
    bool Unchanged();
    bool Click(UUserWidget* Owner,const TCHAR* Name);
    bool Expect(bool Condition,const TCHAR* Failure);
    void Enter(int32 NewStage);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Failure=FString());
};
