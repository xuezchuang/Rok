#pragma once

#include "CoreMinimal.h"

/** UIAnimation/UA_Chat_On and UA_Chat_Off: root X and CanvasGroup alpha,
 * 20 frames at 60 Hz, unweighted zero tangents. No scale track. */
struct FRokChatDrawerAnimation
{
	static constexpr float Duration = 0.33333334f;
	float Opacity = 0.0f;
	float Elapsed = 0.0f;

	static float EvaluateOpacity(bool bClosing, float Seconds)
	{
		const float T = FMath::Clamp(Seconds / Duration, 0.0f, 1.0f);
		const float Smooth = T * T * (3.0f - 2.0f * T);
		return bClosing ? 1.0f - Smooth : Smooth;
	}
	float GetTranslationX() const { return -1000.0f * (1.0f - Opacity); }
	bool IsAnimating() const { return bAnimating; }
	bool IsClosing() const { return bAnimating && bClosing; }
	void Reset() { *this = FRokChatDrawerAnimation(); }
	void Open()
	{
		StartOpacity = Opacity;
		Elapsed = 0.0f;
		bClosing = false;
		bAnimating = true;
	}
	void Close()
	{
		if (IsClosing()) { return; }
		StartOpacity = Opacity;
		Elapsed = 0.0f;
		bClosing = true;
		bAnimating = true;
	}
	/** Source curve for uninterrupted entry/exit; reversing starts at the
	 * displayed value and cancels the previous completion, without a jump. */
	bool Advance(float DeltaSeconds)
	{
		if (!bAnimating) { return false; }
		Elapsed = FMath::Min(Duration, Elapsed + FMath::Max(0.0f, DeltaSeconds));
		Opacity = FMath::Lerp(StartOpacity, bClosing ? 0.0f : 1.0f,
			EvaluateOpacity(false, Elapsed));
		if (Elapsed < Duration) { return false; }
		bAnimating = false;
		return bClosing;
	}

private:
	float StartOpacity = 0.0f;
	bool bAnimating = false;
	bool bClosing = false;
};

/** UI_Win_ChatMediator.OnHide/PopContactList moves pl_content, not the
 * window root: contact width 330, 0.2 seconds, DOTween Ease.InCubic. */
struct FRokChatContactAnimation
{
	static constexpr float Duration = 0.2f;
	float TranslationX = 0.0f;
	float Elapsed = Duration;
	bool bHidden = false;
	void Reset() { *this = FRokChatContactAnimation(); }
	void Toggle()
	{
		StartX = TranslationX;
		bHidden = !bHidden;
		Elapsed = 0.0f;
	}
	void Advance(float DeltaSeconds)
	{
		Elapsed = FMath::Min(Duration, Elapsed + FMath::Max(0.0f, DeltaSeconds));
		const float T = Elapsed / Duration;
		TranslationX = FMath::Lerp(StartX, bHidden ? -330.0f : 0.0f, T * T * T);
	}
private:
	float StartX = 0.0f;
};
