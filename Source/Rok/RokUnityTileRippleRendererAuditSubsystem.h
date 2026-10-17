#pragma once

#include "CoreMinimal.h"
#include "RenderCommandFence.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokUnityTileRippleRendererAuditSubsystem.generated.h"

class ARokUnityTileRippleDiagnosticActor;
class FJsonObject;
class FJsonValue;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** Opt-in local world-map render/screenshot diagnostic, not source-frame pixel acceptance. */
UCLASS()
class ROK_API URokUnityTileRippleRendererAuditSubsystem : public UTickableWorldSubsystem
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
	UPROPERTY(Transient)
	TObjectPtr<ARokUnityTileRippleDiagnosticActor> DiagnosticActor;
	UPROPERTY(Transient)
	TObjectPtr<USceneCaptureComponent2D> PrivateCapture;
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PrivateTarget;
	FRenderCommandFence RenderFence;
	FDelegateHandle ScreenshotHandle;
	TArray<TSharedPtr<FJsonValue>> Captures;
	TArray<TSharedPtr<FJsonValue>> PrivateGpuFrames;
	TSharedPtr<FJsonObject> PrivateGpuMetrics;
	TSharedPtr<FJsonObject> PrivateTargetDiagnostic;
	TSharedPtr<FJsonObject> PendingCapture;
	FString RunDirectory;
	FString PendingPath;
	FString FailureReason;
	double WallSeconds = 0.0;
	uint64 ScreenshotFrame = 0;
	int32 CaptureIndex = 0;
	int32 Stage = 0;
	bool bFinished = false;
	bool bScreenshotDone = false;
	bool Begin();
	bool CapturePrivateGpuFrames(FString& Error);
	bool Boundary() const;
	TSharedRef<FJsonObject> RenderState() const;
	void ScreenshotProcessed();
	bool Save(bool bComplete) const;
	void Finish(const FString& Failure = FString());
};
