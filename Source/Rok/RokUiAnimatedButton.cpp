#include "RokUiAnimatedButton.h"

#include "Blueprint/UserWidget.h"
#include "Components/PanelWidget.h"
#include "HAL/PlatformTime.h"
#include "Types/SlateEnums.h"
#include "Widgets/Input/SButton.h"

namespace
{
// ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/
// CommonBtnDownAni.anim and CommonBtnUpAni.anim: zero in/out slopes at all keys.
constexpr float PressDuration = 0.08333334f;
constexpr float ReleaseSecondKey = 0.1833333f;
constexpr float ReleaseDuration = 0.2833333f;

FVector2D InterpolateScale(
	const FVector2D& Start,
	const FVector2D& End,
	float Alpha)
{
	const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return FMath::Lerp(Start, End, T * T * (3.0f - 2.0f * T));
}

bool IsWidgetHierarchyVisible(const UWidget* Widget)
{
	for (const UWidget* Current = Widget; Current;)
	{
		if (!Current->IsVisible())
		{
			return false;
		}
		if (const UPanelWidget* Parent = Current->GetParent())
		{
			Current = Parent;
		}
		else
		{
			// A UserWidget's root has no panel parent. Cross its WidgetTree outer
			// so a collapsed modal or tactical HUD also cancels its child motion.
			Current = Current->GetTypedOuter<UUserWidget>();
		}
	}
	return Widget != nullptr;
}
}

FVector2D URokUiAnimatedButton::EvaluatePressScale(
	float Seconds,
	const FVector2D& InitialScale)
{
	return InterpolateScale(InitialScale, FVector2D(0.9f), Seconds / PressDuration);
}

FVector2D URokUiAnimatedButton::EvaluateReleaseScale(
	float Seconds,
	const FVector2D& InitialScale)
{
	if (Seconds <= PressDuration)
	{
		return InterpolateScale(
			InitialScale, FVector2D(1.06f, 1.03f), Seconds / PressDuration);
	}
	if (Seconds <= ReleaseSecondKey)
	{
		return InterpolateScale(
			FVector2D(1.06f, 1.03f), FVector2D(0.97f, 0.95f),
			(Seconds - PressDuration) / (ReleaseSecondKey - PressDuration));
	}
	return InterpolateScale(
		FVector2D(0.97f, 0.95f), FVector2D(1.0f),
		(Seconds - ReleaseSecondKey) / (ReleaseDuration - ReleaseSecondKey));
}

void URokUiAnimatedButton::SetAnimationTarget(UWidget* InTarget)
{
	ResetPressAnimation();
	ConfiguredAnimationTarget = InTarget;
	bUseExternalAnimationTarget = InTarget != nullptr;
}

void URokUiAnimatedButton::SetPressAnimationEnabled(bool bEnabled)
{
	if (!bEnabled) ResetPressAnimation();
	bPressAnimationEnabled = bEnabled;
}

void URokUiAnimatedButton::ResetAnimationsInHierarchy(UWidget* Root)
{
	if (URokUiAnimatedButton* Button = Cast<URokUiAnimatedButton>(Root))
	{
		Button->ResetPressAnimation();
	}
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Root))
	{
		ResetAnimationsInHierarchy(UserWidget->GetRootWidget());
	}
	else if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			ResetAnimationsInHierarchy(Panel->GetChildAt(Index));
		}
	}
}

TSharedRef<SWidget> URokUiAnimatedButton::RebuildWidget()
{
	ResetPressAnimation();
	OnPressed.AddUniqueDynamic(this, &URokUiAnimatedButton::HandleAnimationPressed);
	OnReleased.AddUniqueDynamic(this, &URokUiAnimatedButton::HandleAnimationReleased);
	return Super::RebuildWidget();
}

void URokUiAnimatedButton::ReleaseSlateResources(bool bReleaseChildren)
{
	ResetPressAnimation();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void URokUiAnimatedButton::HandleAnimationPressed()
{
	if (!bPressAnimationEnabled) return;
	UWidget* Target = bUseExternalAnimationTarget
		? ConfiguredAnimationTarget.Get()
		: this;
	if (!GetIsEnabled() || !IsWidgetHierarchyVisible(this)
		|| !IsWidgetHierarchyVisible(Target))
	{
		ResetPressAnimation();
		return;
	}
	if (!bHasCapturedTransform)
	{
		ActiveAnimationTarget = Target;
		BaseRenderScale = Target->GetRenderTransform().Scale;
		BaseRenderPivot = Target->GetRenderTransformPivot();
		CurrentRelativeScale = FVector2D(1.0f);
		bHasCapturedTransform = true;
		Target->SetRenderTransformPivot(FVector2D(0.5f));
	}
	// Interrupt the current phase without snapping back to its authored first key.
	PhaseInitialScale = CurrentRelativeScale;
	AnimationSeconds = 0.0f;
	PhaseStartTimeSeconds = FPlatformTime::Seconds();
	AnimationPhase = EPressAnimationPhase::Press;
	StartActiveTimer();
}

void URokUiAnimatedButton::HandleAnimationReleased()
{
	if (!bPressAnimationEnabled) return;
	if (!bHasCapturedTransform || !GetIsEnabled())
	{
		ResetPressAnimation();
		return;
	}
	PhaseInitialScale = CurrentRelativeScale;
	AnimationSeconds = 0.0f;
	PhaseStartTimeSeconds = FPlatformTime::Seconds();
	AnimationPhase = EPressAnimationPhase::Release;
	StartActiveTimer();
}

void URokUiAnimatedButton::StartActiveTimer()
{
	if (!ActiveTimerHandle.IsValid() && MyButton.IsValid())
	{
		ActiveTimerWidget = MyButton;
		ActiveTimerHandle = MyButton->RegisterActiveTimer(
			0.0f,
			FWidgetActiveTimerDelegate::CreateUObject(
				this, &URokUiAnimatedButton::HandleAnimationTimer));
	}
}

void URokUiAnimatedButton::StopActiveTimer()
{
	if (TSharedPtr<SWidget> TimerWidget = ActiveTimerWidget.Pin())
	{
		if (TSharedPtr<FActiveTimerHandle> TimerHandle = ActiveTimerHandle.Pin())
		{
			TimerWidget->UnRegisterActiveTimer(TimerHandle.ToSharedRef());
		}
	}
	ActiveTimerHandle.Reset();
	ActiveTimerWidget.Reset();
}

bool URokUiAnimatedButton::AdvancePressAnimation(float DeltaSeconds)
{
	return AdvancePressAnimationToTime(AnimationSeconds + FMath::Max(0.0f, DeltaSeconds));
}

bool URokUiAnimatedButton::AdvancePressAnimationToTime(float ElapsedSeconds)
{
	UWidget* Target = ActiveAnimationTarget.Get();
	if (AnimationPhase == EPressAnimationPhase::None)
	{
		return false;
	}
	if (!Target || !GetIsEnabled() || !IsWidgetHierarchyVisible(this)
		|| !IsWidgetHierarchyVisible(Target))
	{
		RestoreTargetTransform();
		return false;
	}
	AnimationSeconds = FMath::Max(0.0f, ElapsedSeconds);
	const bool bReleasing = AnimationPhase == EPressAnimationPhase::Release;
	CurrentRelativeScale = bReleasing
		? EvaluateReleaseScale(AnimationSeconds, PhaseInitialScale)
		: EvaluatePressScale(AnimationSeconds, PhaseInitialScale);
	Target->SetRenderScale(BaseRenderScale * CurrentRelativeScale);
	if (AnimationSeconds >= (bReleasing ? ReleaseDuration : PressDuration))
	{
		AnimationPhase = EPressAnimationPhase::None;
		if (bReleasing)
		{
			RestoreTargetTransform();
		}
		// A held button keeps .9 without keeping Slate awake. Owners hiding a
		// persistent hierarchy mid-hold must call ResetPressAnimation().
		return false;
	}
	return true;
}

EActiveTimerReturnType URokUiAnimatedButton::HandleAnimationTimer(
	double CurrentTime,
	float DeltaSeconds)
{
	// Paint's DeltaSeconds includes time before a newly pressed/released phase
	// began (and can include an idle/slow frame). Measure only this phase's own
	// monotonic elapsed time, rather than skipping its first keys on wake-up.
	const float ElapsedSeconds = static_cast<float>(
		FMath::Max(0.0, FPlatformTime::Seconds() - PhaseStartTimeSeconds));
	if (AdvancePressAnimationToTime(ElapsedSeconds))
	{
		return EActiveTimerReturnType::Continue;
	}
	// Returning Stop unregisters after this callback; do not mutate Slate's
	// active-timer list from inside its own iteration.
	ActiveTimerHandle.Reset();
	ActiveTimerWidget.Reset();
	return EActiveTimerReturnType::Stop;
}

void URokUiAnimatedButton::RestoreTargetTransform()
{
	if (bHasCapturedTransform)
	{
		if (UWidget* Target = ActiveAnimationTarget.Get())
		{
			Target->SetRenderScale(BaseRenderScale);
			Target->SetRenderTransformPivot(BaseRenderPivot);
		}
	}
	ActiveAnimationTarget.Reset();
	bHasCapturedTransform = false;
	CurrentRelativeScale = FVector2D(1.0f);
	PhaseInitialScale = FVector2D(1.0f);
	AnimationSeconds = 0.0f;
	PhaseStartTimeSeconds = 0.0;
	AnimationPhase = EPressAnimationPhase::None;
}

void URokUiAnimatedButton::ResetPressAnimation()
{
	StopActiveTimer();
	RestoreTargetTransform();
}

#if WITH_DEV_AUTOMATION_TESTS
void URokUiAnimatedButton::AdvancePressAnimationForTesting(float DeltaSeconds)
{
	if (!AdvancePressAnimation(DeltaSeconds))
	{
		StopActiveTimer();
	}
}

bool URokUiAnimatedButton::HasActiveAnimationTimerForTesting() const
{
	return ActiveTimerHandle.IsValid();
}
#endif
