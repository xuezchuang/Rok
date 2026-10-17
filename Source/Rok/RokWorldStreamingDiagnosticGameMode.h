#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RokWorldStreamingDiagnosticGameMode.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapStreamingActor;
class FJsonObject;
class FJsonValue;

/** Isolated -game oracle. Never assign this GameMode to the product world map. */
UCLASS()
class ROK_API ARokWorldStreamingDiagnosticGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARokWorldStreamingDiagnosticGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TWeakObjectPtr<ARokWorldMapCameraPawn> Camera;
	TWeakObjectPtr<ARokWorldMapStreamingActor> Streaming;
	TArray<TSharedPtr<FJsonObject>> Samples;
	TSharedPtr<FJsonObject> BeginBoundary;
	TArray<FName> InitialComponentNames;
	int32 Step = 0;
	double StartedAtSeconds = 0.0;
	uint64 InitialGeneration = 0;
	uint64 MovedGeneration = 0;
	uint64 LodOneGeneration = 0;
	bool bFinished = false;
	FDelegateHandle ScreenshotHandle;
	FString RunDirectory;
	FString PendingScreenshotPath;
	FString PendingStateJson;
	FString StableStateJson;
	TSharedPtr<FJsonObject> PendingSample;
	TSharedPtr<FJsonObject> LastObservedSceneState;
	uint64 LastObservedSceneFrame = 0;
	uint64 ScreenshotRequestFrame = 0;
	uint64 ScreenshotProcessedFrame = 0;
	uint64 StableLastFrame = 0;
	double ScreenshotRequestedAtSeconds = 0.0;
	int32 StableStep = -1;
	int32 StableObservationCount = 0;
	bool bScreenshotProcessed = false;

	bool CheckLocalOnlyBoundary(FString& OutReason) const;
	void WriteLocalOnlyBoundary(FJsonObject& Object) const;
	void Fail(const FString& Reason);
	void Finish(bool bPassed, const FString& Reason);
	void Record(const FString& Label);
	void ScreenshotProcessed();
	void AdvanceAfterScreenshot();
	void ClearStability();
	void CheckPendingScreenshot();
	TSharedPtr<FJsonObject> CaptureVisualState() const;
	TSharedPtr<FJsonObject> CaptureRuntimeData() const;
	TArray<TSharedPtr<FJsonValue>> CaptureActualComponents() const;
};
