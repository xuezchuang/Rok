#pragma once

#include "CoreMinimal.h"

/** Only shared Animator families verified in the Unity UI prefabs. */
enum class ERokUiWindowAnimationStyle : uint8
{
	None,
	Type1,
	Interface,
	Pop,
	WorldObject,
	// Scale applies only to pl_Title; opacity applies to the entire menu.
	BuildingMenu,
};

struct FRokUiWindowAnimationSample
{
	float Scale = 1.0f;
	float Opacity = 1.0f;
};

/** Native equivalent of shared_uianimation clips, independent of widget lifetime.
 * The owning screen keeps its unanimated input mask until Advance reports close.
 */
struct ROK_API FRokUiWindowAnimation
{
	void Open(ERokUiWindowAnimationStyle InStyle);
	void Close();
	void Reset();
	/** Returns true exactly once when the close motion completes. */
	bool Advance(float DeltaSeconds);
	bool IsAnimating() const { return Phase == EPhase::Opening || Phase == EPhase::Closing; }
	bool IsClosing() const { return Phase == EPhase::Closing; }
	const FRokUiWindowAnimationSample& GetSample() const { return Sample; }
	ERokUiWindowAnimationStyle GetStyle() const { return Style; }
	float GetElapsedSeconds() const { return ElapsedSeconds; }
	static FRokUiWindowAnimationSample Evaluate(
		ERokUiWindowAnimationStyle InStyle, bool bClosing, float Seconds);
	static float GetDuration(ERokUiWindowAnimationStyle InStyle, bool bClosing);

private:
	enum class EPhase : uint8 { Closed, Opening, Open, Closing };
	EPhase Phase = EPhase::Closed;
	ERokUiWindowAnimationStyle Style = ERokUiWindowAnimationStyle::None;
	float ElapsedSeconds = 0.0f;
	FRokUiWindowAnimationSample Sample;
	FRokUiWindowAnimationSample StartSample;
	bool bInterrupted = false;
};
