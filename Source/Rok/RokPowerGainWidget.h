#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "RokPowerGainWidget.generated.h"

class UMaterialInstanceDynamic;
class UTexture2D;
class SRokPowerGain;

/** UI_10010 geometry exported by ExportRokUnityPowerEffect.cs. Presentation only;
 * the HUD caller supplies either explicitly labelled preview scores or authority. */
UCLASS()
class URokPowerGainWidget : public UWidget
{

	GENERATED_BODY()
public:
	bool Play();
	// Explicit scaled game clock for the root/Animator; particle simulation uses
	// the independent frame delta. The audit supplies recorded Unity Time.time.
	bool PlayAtClock(double AbsoluteSeconds);
	void Advance(float DeltaSeconds);
	bool AdvanceAtClock(float ParticleDeltaSeconds, double AbsoluteSeconds);
	void ObservePower(int32 Previous, int32 Current, bool bOwnerVisible);
	bool IsPlaying() const { return bPlaying; }
	float GetRemainingTime() const { return Remaining; }
	float GetPlaybackTime() const { return MeshTime; }
#if !UE_BUILD_SHIPPING
	// Read-only observation: no playback, emitter or rendering mutation.
	void GetParticleStateForAudit(TArray<float>& Ages, TArray<bool>& Pending) const
	{
		Ages.Reset(); Pending.Reset();
		for (const auto& Particle : ParticleTimes) { Ages.Add(Particle.Age); Pending.Add(Particle.bPendingBirth); }
	}
#endif
	static float EvaluateUTiling(float Seconds);
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	friend class SRokPowerGain;
	friend class FRokPowerGainPreloadTest;
	friend class FRokPowerGainParticleClockTest;
	friend class FRokPowerGainReplayTimingTest;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRokPowerGainReplayOffscreenTest;
	friend class FRokPowerGainClockDomainTest;
	// Paint-only diagnostic selection. Production always paints both layers.
	uint8 AuditPaintLayers = 3;
#endif
	struct FDraw
	{
		TArray<FVector2f> Positions, UVs;
		TArray<FLinearColor> Colors;
		TArray<uint32> Indices;
	};
	bool LoadSource();
	void AdvanceInternal(float ClockDeltaSeconds, float ParticleDeltaSeconds);
	FDraw Mesh;
	TArray<FDraw> ParticleFrames;
	struct FParticlePlayback { float Age = 0; bool bPendingBirth = true; };
	TArray<FParticlePlayback> ParticleTimes;
	float MeshTime = 0;
	// MeshTime remains the request-relative playback clock used by callers.
	// The source Animator applies replay on the next positive update, so the
	// rendered UV phase has a separate clock during a replay.
	float RenderMeshTime = 0;
	bool bAnimatorRestartPending = false;
	float Remaining = 0;
	float SampleRate = 120;
	float LastParticleSampleAge = 1;
	bool bPlaying = false;
	double LastAbsoluteClockSeconds = 0;
	bool bHasAbsoluteClock = false;
	bool bHasPowerObservation = false;
#if WITH_EDITOR
	bool bShaderPreloadRequested = false;
#endif
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	TArray<FSlateBrush> Brushes;
	TSharedPtr<SRokPowerGain> SlateWidget;
};
