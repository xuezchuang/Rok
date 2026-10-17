#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokVillagePresentationData.h"
#include "RokVillageDayNight.h"
#include "RokVillageDayNightAuditSubsystem.generated.h"
class ARokWorldMapCameraPawn;
class ARokWorldMapEntityActor;
class URokVillagePresentationComponent;
class FJsonObject;
class FJsonValue;

UCLASS()
class ROK_API URokVillageDayNightAuditSubsystem : public UTickableWorldSubsystem
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
    struct FStage
    {
        FString Name;
        float Dxf = 480;
        bool bApply = true;
        FRokVillageDayNightSample Sample;
        FRokVillageDayNightResult Expected;
    };
    TArray<FStage> Stages;
    FRokVillagePresentationData Geometry;
    TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
    TWeakObjectPtr<ARokWorldMapEntityActor> Entity;
    TWeakObjectPtr<URokVillagePresentationComponent> Village;
    TArray<TSharedPtr<FJsonValue>> Captures;
    TArray<FString> Failures;
    TSharedPtr<FJsonObject> Pending;
    FDelegateHandle ScreenshotHandle;
    FString Directory, PendingPath;
    double Elapsed = 0, StageStarted = 0;
    uint64 Frame = 0, StageFrame = 0;
    int32 StageIndex = -1;
    bool bFinished = false;
    bool Begin();
    bool LoadStages();
    bool CheckIdentity();
    bool Validate(TSharedPtr<FJsonObject>& Out);
    void Next();
    void ScreenshotProcessed();
    bool Save(bool Complete);
    void Finish(const FString& Failure = FString());
};
