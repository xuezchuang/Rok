#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "RokUiAnimatedButton.generated.h"

class FActiveTimerHandle;
class SWidget;
enum class EActiveTimerReturnType : uint8;

/** Unity BtnAnimation/CommonBtnDownAni/CommonBtnUpAni presentation only.
 * Click routing and button styles remain owned by the caller. */
UCLASS()
class ROK_API URokUiAnimatedButton : public UButton
{
	GENERATED_BODY()

public:
	/** Animate a visual behind a transparent hit button; nullptr animates self. */
	void SetAnimationTarget(UWidget* InTarget);
	/** Opt out where the Unity prefab has no BtnAnimation component. */
	void SetPressAnimationEnabled(bool bEnabled);
	/** Call before hiding a persistent parent while a button is held down. */
	void ResetPressAnimation();
	/** Reset before collapsing/removing a subtree: hidden Slate widgets may not
	 * paint, so their active timers are not guaranteed another callback. */
	static void ResetAnimationsInHierarchy(UWidget* Root);

	static FVector2D EvaluatePressScale(
		float Seconds,
		const FVector2D& InitialScale = FVector2D(1.0f));
	static FVector2D EvaluateReleaseScale(
		float Seconds,
		const FVector2D& InitialScale = FVector2D(0.9f));

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_DEV_AUTOMATION_TESTS
	void AdvancePressAnimationForTesting(float DeltaSeconds);
	bool HasActiveAnimationTimerForTesting() const;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	enum class EPressAnimationPhase : uint8 { None, Press, Release };

	UFUNCTION()
	void HandleAnimationPressed();
	UFUNCTION()
	void HandleAnimationReleased();

	void StartActiveTimer();
	void StopActiveTimer();
	bool AdvancePressAnimation(float DeltaSeconds);
	bool AdvancePressAnimationToTime(float ElapsedSeconds);
	void RestoreTargetTransform();
	EActiveTimerReturnType HandleAnimationTimer(double CurrentTime, float DeltaSeconds);

	TWeakObjectPtr<UWidget> ConfiguredAnimationTarget;
	TWeakObjectPtr<UWidget> ActiveAnimationTarget;
	TWeakPtr<SWidget> ActiveTimerWidget;
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;
	FVector2D BaseRenderScale = FVector2D(1.0f);
	FVector2D BaseRenderPivot = FVector2D(0.5f);
	FVector2D CurrentRelativeScale = FVector2D(1.0f);
	FVector2D PhaseInitialScale = FVector2D(1.0f);
	float AnimationSeconds = 0.0f;
	double PhaseStartTimeSeconds = 0.0;
	EPressAnimationPhase AnimationPhase = EPressAnimationPhase::None;
	bool bUseExternalAnimationTarget = false;
	bool bHasCapturedTransform = false;
	bool bPressAnimationEnabled = true;
};
