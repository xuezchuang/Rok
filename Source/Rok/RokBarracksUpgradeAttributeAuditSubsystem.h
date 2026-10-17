#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokBarracksUpgradeAttributeAuditSubsystem.generated.h"
class URokMainHudWidget;
class URokUnityModalWidget;
class UUserWidget;
class UWidget;
class FJsonObject;
class FJsonValue;
UCLASS()
class ROK_API URokBarracksUpgradeAttributeAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<UWidget> PreviewImage;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> Buildings;
    TArray<uint8> Profile, Resources;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    FString Directory, PendingPath;
    FDelegateHandle ShotHandle;
    int32 Step=0, OwnedQueues=0;
    double Elapsed=0, StepStarted=0, ShotStarted=0;
    bool bOwnsTargetWorld=false, bFinished=false, bBaseline=false, bPageOpen=false, bPreviewHidden=false;
    bool Begin();
    bool Identity();
    bool Unchanged();
    bool Validate();
    bool Expect(bool Condition,const TCHAR* Error);
    bool Click(UUserWidget* Owner,FName Name);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error=FString());
};
