#include "RokVillageDayNight.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FLinearColor RokVillageDayNight::SpriteColor(const FRokVillageLightPreset& From, const FRokVillageLightPreset& To, float Alpha, float FadeAlpha)
{
	const float T = FMath::Clamp(Alpha, 0.f, 1.f);
	// LightManager interpolates these three inputs separately. Interpolating
	// completed SpriteColor values would lose the intensity/color cross term.
	FLinearColor Color = FMath::Lerp(From.Ambient, To.Ambient, T)
		+ FMath::Lerp(From.Directional, To.Directional, T) * (FMath::Lerp(From.Intensity, To.Intensity, T) * .75f);
	Color.A = FadeAlpha;
	return Color;
}

float RokVillageDayNight::NightPulse(float ShaderTimeSeconds)
{
	return FMath::Clamp(FMath::Abs(FMath::Sin(ShaderTimeSeconds * .5f)) + .5f, 0.f, 1.f);
}

FLinearColor RokVillageDayNight::ShadeGamma(FLinearColor Main, FLinearColor Mask, FLinearColor Renderer,
	const FRokVillageDayNightResult& State, float CityTransparency)
{
	const FLinearColor Col = Main * Renderer;
	const float Red2 = Mask.R * 2.f, Team1 = FMath::Clamp(Red2 - 1.f, 0.f, 1.f);
	FLinearColor Color = Col * (FLinearColor::White * (1.f - Team1 - Mask.G)
		+ State.Color1 * (2.f * Team1) + State.Color2 * (2.f * Mask.G));
	Color = Color * State.SpriteColor + State.NightLight * Mask.B;
	Color.A = Col.A * FMath::Clamp(Red2, 0.f, 1.f) * CityTransparency;
	return Color;
}

bool FRokVillageDayNightData::Evaluate(const FRokVillageDayNightSample& Sample, FRokVillageDayNightResult& Out) const
{
	if (!Presets.IsValidIndex(Sample.FromPreset) || !Presets.IsValidIndex(Sample.ToPreset)
		|| !FMath::IsFinite(Sample.DurationSeconds) || Sample.DurationSeconds <= 0
		|| !FMath::IsFinite(Sample.ElapsedSeconds) || Sample.ElapsedSeconds < 0
		|| !FMath::IsFinite(Sample.MaterialDelaySeconds) || Sample.MaterialDelaySeconds < 0
		|| !FMath::IsFinite(Sample.ShaderTimeSeconds) || Sample.ShaderTimeSeconds < 0
		|| !FMath::IsFinite(Sample.FadeAlpha) || Sample.FadeAlpha < 0 || Sample.FadeAlpha > 1) return false;
	FRokVillageDayNightResult Result;
	Result.SpriteColor = RokVillageDayNight::SpriteColor(Presets[Sample.FromPreset], Presets[Sample.ToPreset],
		Sample.ElapsedSeconds / Sample.DurationSeconds, Sample.FadeAlpha);
	Result.bNightMaterial = Sample.ElapsedSeconds >= Sample.MaterialDelaySeconds ? Sample.bRequestedNight : Sample.bPreviousNight;
	Result.Color1 = Result.bNightMaterial ? NightColor1 : DayColor1;
	Result.Color2 = Result.bNightMaterial ? NightColor2 : DayColor2;
	Result.NightLight = Result.bNightMaterial ? NightColor3 * RokVillageDayNight::NightPulse(Sample.ShaderTimeSeconds) : FLinearColor::Black;
	Out = Result;
	return true;
}

bool FRokVillageDayNightData::Load()
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_village_day_night.json"))) && LoadJson(Text);
}

bool FRokVillageDayNightData::LoadJson(const FString& Text)
{
	*this = FRokVillageDayNightData();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	FString Schema;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.village.day_night.v1")) return false;
	const TSharedPtr<FJsonObject> *Contract = nullptr, *Materials = nullptr, *Day = nullptr, *Night = nullptr;
	bool SnapshotOnly = false;
	if (!Root->TryGetObjectField(TEXT("contract"), Contract) || !(*Contract)->TryGetBoolField(TEXT("snapshot_only"), SnapshotOnly) || !SnapshotOnly
		|| !Root->TryGetObjectField(TEXT("materials"), Materials) || !(*Materials)->TryGetObjectField(TEXT("day"), Day)
		|| !(*Materials)->TryGetObjectField(TEXT("night"), Night)) return false;
	const auto Color = [](const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int32 Count, FLinearColor& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || Values->Num() != Count) return false;
		float Components[4] = {0,0,0,1};
		for (int32 I = 0; I < Count; ++I)
		{
			double Number = 0;
			if (!(*Values)[I].IsValid() || !(*Values)[I]->TryGetNumber(Number) || !FMath::IsFinite(Number) || Number < 0 || Number > 1) return false;
			Components[I] = Number;
		}
		Out = FLinearColor(Components[0], Components[1], Components[2], Components[3]); return true;
	};
	FRokVillageDayNightData Candidate;
	const TArray<TSharedPtr<FJsonValue>>* PresetRows = nullptr;
	if (!Root->TryGetArrayField(TEXT("presets"), PresetRows) || PresetRows->Num() != 4) return false;
	const TCHAR* Names[] = {TEXT("dawn"),TEXT("day"),TEXT("dusk"),TEXT("night")};
	for (int32 I = 0; I < 4; ++I)
	{
		const auto& Value = (*PresetRows)[I];
		if (!Value.IsValid() || Value->Type != EJson::Object) return false;
		const auto Row = Value->AsObject(); FRokVillageLightPreset Preset; double Intensity = 0; FString Name;
		if (!Row->TryGetStringField(TEXT("name"), Name) || Name != Names[I]
			|| !Color(Row, TEXT("ambient"), 3, Preset.Ambient) || !Color(Row, TEXT("directional"), 3, Preset.Directional)
			|| !Row->TryGetNumberField(TEXT("intensity"), Intensity) || !FMath::IsFinite(Intensity) || Intensity < 0) return false;
		Preset.Intensity = Intensity; Candidate.Presets.Add(Preset);
	}
	if (!Color(*Day, TEXT("color1"), 4, Candidate.DayColor1) || !Color(*Day, TEXT("color2"), 4, Candidate.DayColor2)
		|| !Color(*Night, TEXT("color1"), 4, Candidate.NightColor1) || !Color(*Night, TEXT("color2"), 4, Candidate.NightColor2)
		|| !Color(*Night, TEXT("color3"), 4, Candidate.NightColor3)) return false;
	*this = MoveTemp(Candidate); return true;
}
