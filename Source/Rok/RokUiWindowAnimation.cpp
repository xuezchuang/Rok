#include "RokUiWindowAnimation.h"

namespace
{
	// ref/client-unity/Assets/BundleAssets/UI/UIAnimation/shared_uianimation/
	// UA_WinAnime_T1_On, UA_WinAnime_IF_On, UA_WinAnime_T1_Off,
	// UA_PopAnime_On/Off. All XY/alpha keys have unweighted zero tangents.
	constexpr float OvershootTime = 0.11666667f;
	constexpr float OpenDuration = 0.25f;
	constexpr float WindowCloseDuration = 0.15f;

	float SmoothSegment(float From, float To, float Time, float Start, float End)
	{
		const float Alpha = FMath::Clamp((Time - Start) / (End - Start), 0.0f, 1.0f);
		return FMath::Lerp(From, To, Alpha * Alpha * (3.0f - 2.0f * Alpha));
	}
}

float FRokUiWindowAnimation::GetDuration(ERokUiWindowAnimationStyle InStyle, bool bClosing)
{
	if (InStyle == ERokUiWindowAnimationStyle::None)
	{
		return 0.0f;
	}
	// UA_WinAnime_WorldObjectInfo_Off: frame 10 / 60, unlike shared Pop.
	if (bClosing && InStyle == ERokUiWindowAnimationStyle::WorldObject) return 0.16666667f;
	if (InStyle == ERokUiWindowAnimationStyle::BuildingMenu) return 0.25f;
	return bClosing && InStyle != ERokUiWindowAnimationStyle::Pop
		? WindowCloseDuration : OpenDuration;
}

FRokUiWindowAnimationSample FRokUiWindowAnimation::Evaluate(
	ERokUiWindowAnimationStyle InStyle, bool bClosing, float Seconds)
{
	FRokUiWindowAnimationSample Result;
	if (InStyle == ERokUiWindowAnimationStyle::None)
	{
		return Result;
	}
	if (InStyle == ERokUiWindowAnimationStyle::BuildingMenu)
	{
		// UA_BuildingMenu_On/Off: pl_Title XY scale and root CanvasGroup alpha.
		const float PeakTime = bClosing ? 0.083333336f : OvershootTime;
		Result.Scale = Seconds <= PeakTime
			? SmoothSegment(bClosing ? 1.0f : 0.0f, 1.2f, Seconds, 0.0f, PeakTime)
			: SmoothSegment(1.2f, bClosing ? 0.0f : 1.0f, Seconds, PeakTime, 0.25f);
		Result.Opacity = bClosing
			? SmoothSegment(1.0f, 0.0f, Seconds, PeakTime, 0.25f)
			: SmoothSegment(0.0f, 1.0f, Seconds, 0.0f, PeakTime);
		return Result;
	}
	if (bClosing)
	{
		if (InStyle == ERokUiWindowAnimationStyle::Pop || InStyle == ERokUiWindowAnimationStyle::WorldObject)
		{
			Result.Scale = SmoothSegment(1.0f, 0.0f, Seconds, 0.0f, GetDuration(InStyle, true));
		}
		else
		{
			Result.Scale = SmoothSegment(1.0f, 1.1f, Seconds, 0.0f, WindowCloseDuration);
			Result.Opacity = SmoothSegment(1.0f, 0.0f, Seconds, 0.0f, WindowCloseDuration);
		}
	}
	else if (InStyle == ERokUiWindowAnimationStyle::Interface)
	{
		Result.Scale = SmoothSegment(1.1f, 1.0f, Seconds, 0.0f, OpenDuration);
		Result.Opacity = SmoothSegment(0.0f, 1.0f, Seconds, 0.0f, OpenDuration);
	}
	else
	{
		const float StartScale = (InStyle == ERokUiWindowAnimationStyle::Pop || InStyle == ERokUiWindowAnimationStyle::WorldObject) ? 0.0f : 1.05f;
		Result.Scale = Seconds <= OvershootTime
			? SmoothSegment(StartScale, 1.1f, Seconds, 0.0f, OvershootTime)
			: SmoothSegment(1.1f, 1.0f, Seconds, OvershootTime, OpenDuration);
	}
	return Result;
}

void FRokUiWindowAnimation::Open(ERokUiWindowAnimationStyle InStyle)
{
	const bool bReopening = IsClosing() && Style == InStyle;
	StartSample = Sample;
	Style = InStyle;
	ElapsedSeconds = 0.0f;
	bInterrupted = bReopening;
	Phase = Style == ERokUiWindowAnimationStyle::None ? EPhase::Open : EPhase::Opening;
	Sample = bReopening ? StartSample : Evaluate(Style, false, 0.0f);
}

void FRokUiWindowAnimation::Close()
{
	if (Phase == EPhase::Closed || IsClosing())
	{
		return;
	}
	StartSample = Sample;
	bInterrupted = Phase == EPhase::Opening;
	ElapsedSeconds = 0.0f;
	Phase = EPhase::Closing;
}

void FRokUiWindowAnimation::Reset()
{
	*this = FRokUiWindowAnimation();
}

bool FRokUiWindowAnimation::Advance(float DeltaSeconds)
{
	if (!IsAnimating())
	{
		return false;
	}
	const bool bClosing = IsClosing();
	const float Duration = GetDuration(Style, bClosing);
	ElapsedSeconds = FMath::Min(ElapsedSeconds + FMath::Max(0.0f, DeltaSeconds), Duration);
	Sample = Evaluate(Style, bClosing, ElapsedSeconds);
	if (bInterrupted && Duration > 0.0f)
	{
		// Retain the current visual value on interrupted close/reopen, while
		// preserving source key timings and endpoints for uninterrupted clips.
		const FRokUiWindowAnimationSample DefaultStart = Evaluate(Style, bClosing, 0.0f);
		const float ScaleCorrectionEnd = !bClosing && Style != ERokUiWindowAnimationStyle::Interface
			? OvershootTime : Duration;
		Sample.Scale += SmoothSegment(StartSample.Scale - DefaultStart.Scale, 0.0f,
			ElapsedSeconds, 0.0f, ScaleCorrectionEnd);
		Sample.Opacity += SmoothSegment(StartSample.Opacity - DefaultStart.Opacity, 0.0f,
			ElapsedSeconds, 0.0f, Duration);
	}
	if (ElapsedSeconds >= Duration)
	{
		Phase = bClosing ? EPhase::Closed : EPhase::Open;
		return bClosing;
	}
	return false;
}
