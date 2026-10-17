#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RenderCommandFence.h"
#include "RokWorldInstancedMaterialDiagnosticGameMode.generated.h"

class ARokWorldMapCameraPawn;
class ARokWorldMapStreamingActor;
class FJsonObject;
class FJsonValue;
class FViewport;
class FSceneViewFamily;
class UHierarchicalInstancedStaticMeshComponent;
struct FMinimalViewInfo;
class FRokWorldDiagnosticViewExtension;
struct FRokWorldHismProxyProbeResult;

/** Isolated -game oracle. Never assign this GameMode to the product world map. */
UCLASS()
class ROK_API ARokWorldInstancedMaterialDiagnosticGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARokWorldInstancedMaterialDiagnosticGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FRokWorldDiagnosticViewExtension;
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
	FDelegateHandle ViewportRenderedHandle;
	TSharedPtr<FRokWorldDiagnosticViewExtension, ESPMode::ThreadSafe> ViewExtension;
	FRenderCommandFence SubmittedViewFence;
	FRenderCommandFence HismProxyProbeFence;
	bool bSubmittedViewFenceArmed = false;
	bool bHismProxyProbeArmed = false;
	bool bHismProxyProbeEverQueued = false;
	TSharedPtr<FRokWorldHismProxyProbeResult, ESPMode::ThreadSafe> HismProxyProbeResult;
	TArray<uint32> HismProxyProbeIds;
	uint64 LastAliasMutationFrame = 0;
	uint64 AliasMutationSerial = 0;
	uint64 HismProxyProbeGeneration = 0;
	uint64 HismProxyProbeAliasSerial = 0;
	uint64 HismProxyProbeSerial = 0;
	uint64 SubmittedViewFenceHismProxySerial = 0;
	uint64 HismReadyObservedFrame = 0;
	uint64 LastWorldViewSubmissionFrame = 0;
	uint64 LastViewportRenderedFrame = 0;
	uint64 SubmittedViewFenceFrame = 0;
	uint64 SubmittedViewFenceAliasSerial = 0;
	int32 LastWorldViewSubmissionStep = -1;
	int32 SubmittedViewFenceStep = -1;
	int32 HismProxyProbeStep = -1;
	int32 LastWorldViewCount = 0;
	FIntRect LastWorldViewRect;
	TSharedPtr<FJsonObject> LastFixedProbeSubmittedView;
	bool bLastWorldViewRendering = false;
	bool bLastWorldViewStaticMeshes = false;
	bool bLastWorldViewInstancedStaticMeshes = false;
	FString RunDirectory;
	FString CoverageRunId;
	FString CoverageRunStartTicks;
	FString CoverageProcessId;
	uint64 CoveragePendingRequestTicks = 0;
	uint64 CoveragePendingProcessedTicks = 0;
	TSharedPtr<FJsonObject> CoveragePinnedSources;
	FString CoveragePinnedSourcesJson;
	TSharedPtr<FJsonObject> CoverageProjectionRegistration;
	TSharedPtr<FJsonObject> CoveragePreflightObservation;
	FString CoverageProjectionRegistrationJson;
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
	FString AliasInputSha256;
	FString AliasInputPath;
	FString VariantsReceiptSha256;
	FString RiverReceiptSha256;
	FString BridgeReceiptSha256;
	TMap<FString, FString> AliasBySource;
	TMap<FString, FString> SourceByAlias;
	TMap<FString, FString> ObservedBeforeAliasBySlot;
	TMap<FString, FString> TargetPackageShaByObject;
	TMap<FString, int64> TargetPackageBytesByObject;
	bool bAliasLoaded = false;
	// Explicit, isolated single-source-tree pixel attribution branch.
	bool bCoverageMode = false;
	bool bFixedCameraProbeMode = false;
	uint64 FixedProbeLod0Generation = 0;
	uint64 FixedProbeLod1Generation = 0;
	FString FixedProbeTargetKey;
	FIntRect FixedProbeRoi;
	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> CoverageTarget;
	int32 CoverageInstanceIndex = INDEX_NONE;
	uint64 CoverageGeneration = 0;
	bool bCoverageTargetHidden = false;
	bool bCoverageRestoreAttempted = false;
	bool bCoverageRestored = false;
	bool bCoverageOtherSixExcluded = false;
	FIntRect CoverageProjectedRect;
	FIntRect CoverageRoi;
	FTransform CoverageInstanceWorld;
	FBox CoverageMeshLocalBounds;
	FTransform CoverageCameraViewTransform;
	float CoverageCameraViewFov = 0.0f;
	float CoverageCameraViewAspect = 0.0f;
	TArray<FTransform> CoverageOtherInstanceWorldTransforms;
	FString CoverageComparableBaseline;

	bool LoadPinnedAlias(FString& OutReason);
	bool ApplyAliasesToActiveComponents(FString& OutReason);
	bool CheckLocalOnlyBoundary(FString& OutReason) const;
	void WriteLocalOnlyBoundary(FJsonObject& Object) const;
	void Fail(const FString& Reason);
	void Finish(bool bPassed, const FString& Reason);
	void Record(const FString& Label);
	void ScreenshotProcessed();
	void AdvanceAfterScreenshot();
	void TickCoverage(const struct FRokWorldMapStreamingDiagnosticState& State);
	void TickFixedCameraProbe(const struct FRokWorldMapStreamingDiagnosticState& State);
	bool CaptureFixedProbeGroundSlot(TSharedPtr<FJsonObject>& OutSlot, FString& OutReason);
	bool CaptureFixedProbeActualView(const FMinimalViewInfo& View,
		const FIntRect& ViewRect, TSharedPtr<FJsonObject>& OutView, FString& OutReason) const;
	void FinishFixedCameraProbe(bool bPassed, const FString& Reason);
	bool LocateCoverageTarget(FString& OutReason);
	bool CaptureCoveragePinnedSources(TSharedPtr<FJsonObject>& OutPins, FString& OutReason) const;
	bool CaptureCoverageProjection(TSharedPtr<FJsonObject>& OutProjection,
		FString& OutReason) const;
	bool CheckCoverageStableInputs(TSharedPtr<FJsonObject>& OutProjection,
		FString& OutReason) const;
	bool CoverageComparableState(const TSharedPtr<FJsonObject>& State,
		FString& OutComparable) const;
	bool SetCoverageTargetHidden(bool bShouldHideCoverageTarget, FString& OutReason);
	void FinishCoverage(bool bPassed, const FString& Reason);
	void ClearStability();
	void CheckPendingScreenshot();
	void ObserveWorldViewSubmission(const FSceneViewFamily& ViewFamily);
	void ViewportRendered(FViewport* Viewport);
	TSharedPtr<FJsonObject> CaptureReadiness() const;
	TSharedPtr<FJsonObject> CaptureHismRenderReadiness() const;
	bool IsSubmittedViewReady() const;
	bool CaptureExpectedConstrainedViewRect(FIntRect& OutRect,
		FMinimalViewInfo& OutViewInfo, float& OutEffectiveAspect,
		float& OutViewportDesiredAspect) const;
	bool IsExpectedSubmittedViewRect() const;
	TSharedPtr<FJsonObject> CaptureVisualState() const;
	TSharedPtr<FJsonObject> CaptureRuntimeData() const;
	TArray<TSharedPtr<FJsonValue>> CaptureActualComponents() const;
};
