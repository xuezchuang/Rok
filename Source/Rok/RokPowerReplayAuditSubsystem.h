#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokPowerReplayAuditSubsystem.generated.h"
class URokMainHudWidget;
class URokPowerGainWidget;
class APlayerController;
class AActor;
class FJsonObject;
class FJsonValue;

/** Explicit local UI notification injection, never a gameplay/server power update. */
UCLASS()
class ROK_API URokPowerReplayAuditSubsystem : public UTickableWorldSubsystem
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
	TWeakObjectPtr<URokPowerGainWidget> Effect;
	TWeakObjectPtr<APlayerController> Controller;
	TWeakObjectPtr<AActor> ViewTarget;
	TArray<TSharedPtr<FJsonValue>> Trace, Captures;
	TSharedPtr<FJsonObject> PendingCapture;
	FDelegateHandle ScreenshotHandle;
	FString Directory, PendingPath;
	double Elapsed = 0, Origin = 0;
	double WallOrigin = 0;
	float CaptureTime = .8f;
	int32 Notification = 0, CaptureIndex = 0, Baseline = 0;
	bool bStarted = false, bFinished = false;
	bool bOwnsTargetWorld = false;
	TSharedRef<FJsonObject> State(const FString& Kind) const;
	TSharedRef<FJsonObject> Identity() const;
	void Processed();
	void Finish(const FString& Failure);
};
