#pragma once
#include "CoreMinimal.h"

struct FRokVillageLightPreset
{
	FLinearColor Ambient = FLinearColor::Black;
	FLinearColor Directional = FLinearColor::Black;
	float Intensity = 0;
};

// Explicit local sampling input, not a server clock or a coroutine scheduler.
// Presets: dawn=0, day=1, dusk=2, night=3. Material switching is independent
// of lighting interpolation. At a delay deadline the sample uses the completed
// switch state (>=); earlier samples keep the explicitly supplied previous state.
struct FRokVillageDayNightSample
{
	int32 FromPreset = 1, ToPreset = 1;
	float DurationSeconds = 1, ElapsedSeconds = 0, MaterialDelaySeconds = 0;
	float ShaderTimeSeconds = 0, FadeAlpha = 1;
	bool bPreviousNight = false, bRequestedNight = false;
};

struct FRokVillageDayNightResult
{
	FLinearColor SpriteColor = FLinearColor::White;
	FLinearColor NightLight = FLinearColor::Black;
	FLinearColor Color1 = FLinearColor::White, Color2 = FLinearColor::White;
	bool bNightMaterial = false;
};

struct FRokVillageDayNightData
{
	TArray<FRokVillageLightPreset> Presets;
	FLinearColor DayColor1, DayColor2, NightColor1, NightColor2, NightColor3;
	bool Load();
	bool LoadJson(const FString& Text);
	bool Evaluate(const FRokVillageDayNightSample& Sample, FRokVillageDayNightResult& Out) const;
};

namespace RokVillageDayNight
{
	FLinearColor SpriteColor(const FRokVillageLightPreset& From, const FRokVillageLightPreset& To, float Alpha, float FadeAlpha);
	float NightPulse(float ShaderTimeSeconds);
	// Source shader arithmetic BEFORE UE linear conversion. Lighting alpha is
	// intentionally excluded from output alpha, as in both source shaders.
	FLinearColor ShadeGamma(FLinearColor Main, FLinearColor Mask, FLinearColor Renderer,
		const FRokVillageDayNightResult& State, float CityTransparency);
}
