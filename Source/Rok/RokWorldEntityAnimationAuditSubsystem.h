#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokWorldEntityAnimationAuditSubsystem.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapEntityActor;
class FJsonObject;
class FJsonValue;

/** Opt-in standalone actor/material audit. Does not edit assets or maps. */
UCLASS()
class ROK_API URokWorldEntityAnimationAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
	TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
	TWeakObjectPtr<ARokWorldMapEntityActor> Entity;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<FString> Failures;
	TSharedPtr<FJsonObject> PendingCapture;
	TMap<FString, FString> PreviousTextures;
	FDelegateHandle ScreenshotHandle;
	FString OutputDirectory;
	FString ScreenshotPath;
	double Elapsed = 0.0;
	double StageStarted = 0.0;
	double RequestedAt = 0.0;
	uint64 Frame = 0;
	int32 Step = -1;
	int32 Stage = 0;
	int32 EntityCount = 0;
	int32 TextureTransitions = 0;
	float SampleMaxTickDelta = 0.0f;
	bool bFinished = false;
	bool FindRuntime();
	void BeginStep();
	TSharedRef<FJsonObject> ReadState(bool bTrackAnimation);
	void Capture();
	void ScreenshotProcessed();
	bool Save(bool bComplete);
	void Fail(const FString& Reason);
	void Finish();
};
