#pragma once

#include "CoreMinimal.h"

class FRenderTarget;

/** Counters describe renderer callbacks and completed GPU validation readbacks, not visual acceptance. */
struct ROKRENDERING_API FRokCityGammaCompositorStats
{
	uint64 BeforePasses = 0;
	uint64 AfterPasses = 0;
	uint64 ValidPreExposurePasses = 0;
	uint64 InvalidPreExposurePasses = 0;
	uint64 EligibleViewsWithoutBefore = 0;
	uint64 UnpairedBeforePasses = 0;
	uint64 UnpairedAfterPasses = 0;
	uint64 TargetConflicts = 0;
	uint64 RejectedTextures = 0;
	uint64 PendingGpuValidations = 0;
	uint64 RegisteredTargets = 0;
};

/**
 * Explicit, scoped opt-in for one dedicated SceneCapture render target. Obtain Target from
 * UTextureRenderTarget::GameThread_GetRenderTargetResource() on the game thread. The target
 * resource must remain alive until Reset() returns; neither render thread nor shader touches
 * the UObject. Do not register a normal viewport target or an unconstrained GameApp capture.
 */
class ROKRENDERING_API FRokCityGammaTargetRegistration
{
public:
	FRokCityGammaTargetRegistration() = default;
	~FRokCityGammaTargetRegistration();
	FRokCityGammaTargetRegistration(const FRokCityGammaTargetRegistration&) = delete;
	FRokCityGammaTargetRegistration& operator=(const FRokCityGammaTargetRegistration&) = delete;
	FRokCityGammaTargetRegistration(FRokCityGammaTargetRegistration&& Other) noexcept;
	FRokCityGammaTargetRegistration& operator=(FRokCityGammaTargetRegistration&& Other) noexcept;

	/** Fails closed for a duplicate/null target or unavailable renderer module. Game thread only. */
	bool Register(const FRenderTarget* Target, FString& OutError);
	/** Waits for the render-thread unregister before the caller may release Target. */
	void Reset();
	bool IsRegistered() const { return Token != 0; }

private:
	uint64 Token = 0;
};

class ROKRENDERING_API FRokCityGammaCompositor
{
public:
	/** Waits for render-thread counters; pending GPU validations remain marked pending. */
	static FRokCityGammaCompositorStats GetStats();
	static bool IsAvailable();

private:
	friend class FRokCityGammaTargetRegistration;
	friend class FRokRenderingModule;
	static bool RegisterTarget(const FRenderTarget* Target, uint64& OutToken, FString& OutError);
	static void UnregisterTarget(uint64 Token);
	static void Startup();
	static void Shutdown();
};
