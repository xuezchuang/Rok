#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokBuildingPreviewGlowData.h"
#include "RokBuildingPreviewGlowAuditSubsystem.generated.h"

class URokMainHudWidget;
class URokUnityModalWidget;
class URokBuildingPreviewGlowWidget;
class UUserWidget;
class FJsonObject;
class FJsonValue;

/** Read-only game-window acceptance for the source two-layer building glow. */
UCLASS()
class ROK_API URokBuildingPreviewGlowAuditSubsystem : public UTickableWorldSubsystem
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
    TWeakObjectPtr<URokBuildingPreviewGlowWidget> Glow;
    TWeakObjectPtr<AActor> Barracks;
    TMap<TWeakObjectPtr<AActor>, TArray<uint8>> Buildings;
    TArray<uint8> Profile, Resources;
    RokBuildingPreviewGlow::FData SourceData;
    TArray<TSharedPtr<FJsonValue>> Captures, Actions;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    FString Directory, PendingPath;
    FDelegateHandle ShotHandle;
    int32 Step=0, OwnedQueues=0;
    uint64 MaskStartPaintCount=0;
    double Elapsed=0, StepStarted=0, ShotStarted=0, OpenStarted=0;
    double FirstNaturalTime=-1, LastNaturalTime=-1;
    bool bOwnsTargetWorld=false, bFinished=false, bBaseline=false, bPageOpen=false, bManualClock=false;
    uint8 DiagnosticMask=3;
    bool Begin();
    bool Identity();
    bool Unchanged();
    bool Validate();
    bool Expect(bool Condition,const TCHAR* Error);
    bool Click(UUserWidget* Owner,FName Name);
    bool SetDiagnostic(uint8 Mask,double SourceTime);
    void Enter(int32 Next);
    void Capture(const TCHAR* Name);
    void ScreenshotProcessed();
    TSharedRef<FJsonObject> State() const;
    bool Save(bool Complete);
    void Finish(const FString& Error=FString());
};
