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
	void Advance(float DeltaSeconds);
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
	struct FDraw
	{
		TArray<FVector2f> Positions, UVs;
		TArray<FLinearColor> Colors;
		TArray<uint32> Indices;
	};
	bool LoadSource();
	FDraw Mesh;
	TArray<FDraw> ParticleFrames;
	struct FParticlePlayback { float Age = 0; bool bPendingBirth = true; };
	TArray<FParticlePlayback> ParticleTimes;
	float MeshTime = 0;
	float Remaining = 0;
	float SampleRate = 120;
	float LastParticleSampleAge = 1;
	bool bPlaying = false;
	bool bHasPowerObservation = false;
#if WITH_EDITOR
	bool bShaderPreloadRequested = false;
#endif
	UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> Textures;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
	TArray<FSlateBrush> Brushes;
	TSharedPtr<SRokPowerGain> SlateWidget;
};
