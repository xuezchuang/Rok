#include "RokClientSettingsSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr int32 CurrentClientSettingsVersion = 1;
constexpr float LowFrameRateLimit = 30.0f;
constexpr float MediumFrameRateLimit = 30.0f;
constexpr float HighFrameRateLimit = 60.0f;

FRokPersonalityToggleDefinition MakePersonalityDefinition(
	const TCHAR* Name,
	int32 UnityId,
	bool bDefaultEnabled,
	int32 UnityResetSeconds,
	bool bVisibleInUnity,
	int32 UnityDisplayOrder)
{
	FRokPersonalityToggleDefinition Definition;
	Definition.Name = FName(Name);
	Definition.UnityId = UnityId;
	Definition.bDefaultEnabled = bDefaultEnabled;
	Definition.UnityResetSeconds = UnityResetSeconds;
	Definition.bVisibleInUnity = bVisibleInUnity;
	Definition.UnityDisplayOrder = UnityDisplayOrder;
	return Definition;
}

const TArray<FRokPersonalityToggleDefinition>& GetKnownPersonalityDefinitions()
{
	static const TArray<FRokPersonalityToggleDefinition> Definitions = {
		MakePersonalityDefinition(TEXT("HideMenusWhileScrollingMap"), 1, false, 0, true, 1),
		MakePersonalityDefinition(TEXT("ConfirmDiamondSpending"), 2, true, 0, true, 2),
		MakePersonalityDefinition(TEXT("ConfirmRecruitPurchase"), 3, true, 86400, true, 3),
		MakePersonalityDefinition(TEXT("BlockEmotesAndQuickReplies"), 6, false, 86400, true, 4),
		MakePersonalityDefinition(TEXT("ConfirmAllianceShopPersonalPointSpending"), 4, true, 0, true, 5),
		MakePersonalityDefinition(TEXT("ConfirmAllianceShopAlliancePointSpending"), 5, true, 0, true, 6)
	};
	return Definitions;
}

const FRokPersonalityToggleDefinition* FindPersonalityDefinition(FName Name)
{
	return GetKnownPersonalityDefinitions().FindByPredicate(
		[Name](const FRokPersonalityToggleDefinition& Definition)
		{
			return Definition.Name == Name;
		});
}

ERokClientGraphicsQualityTier NormalizeGraphicsQualityTier(int32 Value)
{
	switch (Value)
	{
	case static_cast<int32>(ERokClientGraphicsQualityTier::Low):
		return ERokClientGraphicsQualityTier::Low;
	case static_cast<int32>(ERokClientGraphicsQualityTier::Medium):
		return ERokClientGraphicsQualityTier::Medium;
	case static_cast<int32>(ERokClientGraphicsQualityTier::High):
	default:
		return ERokClientGraphicsQualityTier::High;
	}
}

ERokClientFrameRateOption NormalizeFrameRateOption(int32 Value)
{
	switch (Value)
	{
	case static_cast<int32>(ERokClientFrameRateOption::Low):
		return ERokClientFrameRateOption::Low;
	case static_cast<int32>(ERokClientFrameRateOption::Medium):
		return ERokClientFrameRateOption::Medium;
	case static_cast<int32>(ERokClientFrameRateOption::High):
	default:
		return ERokClientFrameRateOption::High;
	}
}

ERokClientGraphicsQualityTier DeriveGraphicsQualityTier(const UGameUserSettings* GameUserSettings)
{
	if (!GameUserSettings)
	{
		return ERokClientGraphicsQualityTier::High;
	}

	int32 OverallLevel = GameUserSettings->GetOverallScalabilityLevel();
	if (OverallLevel < 0)
	{
		// A custom UE profile has no overall level; view distance is a stable,
		// non-destructive hint for the first migration into the three Unity tiers.
		OverallLevel = GameUserSettings->GetViewDistanceQuality();
	}

	if (OverallLevel <= 0)
	{
		return ERokClientGraphicsQualityTier::Low;
	}
	if (OverallLevel == 1)
	{
		return ERokClientGraphicsQualityTier::Medium;
	}
	return ERokClientGraphicsQualityTier::High;
}

ERokClientFrameRateOption DeriveFrameRateOption(const UGameUserSettings* GameUserSettings)
{
	if (!GameUserSettings)
	{
		return ERokClientFrameRateOption::High;
	}

	const float ExistingLimit = GameUserSettings->GetFrameRateLimit();
	if (ExistingLimit <= 0.0f || ExistingLimit > 45.0f)
	{
		return ERokClientFrameRateOption::High;
	}
	if (ExistingLimit > LowFrameRateLimit)
	{
		return ERokClientFrameRateOption::Medium;
	}
	return ERokClientFrameRateOption::Low;
}

float GetFrameRateLimit(ERokClientFrameRateOption Option)
{
	switch (Option)
	{
	case ERokClientFrameRateOption::Low:
		return LowFrameRateLimit;
	case ERokClientFrameRateOption::Medium:
		return MediumFrameRateLimit;
	case ERokClientFrameRateOption::High:
	default:
		return HighFrameRateLimit;
	}
}
}

void URokClientSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSettings();
}

bool URokClientSettingsSubsystem::LoadSettings()
{
	Settings = GetMutableDefault<URokClientSettingsConfig>();
	if (!Settings)
	{
		bSettingsLoaded = false;
		return false;
	}

	Settings->ReloadConfig(Settings->GetClass(), *GGameUserSettingsIni);

	UGameUserSettings* GameUserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (GameUserSettings)
	{
		GameUserSettings->LoadSettings(false);
	}

	bool bNeedsSave = false;
	if (Settings->SettingsVersion < CurrentClientSettingsVersion)
	{
		Settings->GraphicsQualityTier = static_cast<int32>(DeriveGraphicsQualityTier(GameUserSettings));
		Settings->FrameRateOption = static_cast<int32>(DeriveFrameRateOption(GameUserSettings));
		Settings->SettingsVersion = CurrentClientSettingsVersion;
		bNeedsSave = true;
	}
	else
	{
		const int32 NormalizedGraphicsTier = static_cast<int32>(
			NormalizeGraphicsQualityTier(Settings->GraphicsQualityTier));
		const int32 NormalizedFrameRateOption = static_cast<int32>(
			NormalizeFrameRateOption(Settings->FrameRateOption));
		bNeedsSave |= Settings->GraphicsQualityTier != NormalizedGraphicsTier;
		bNeedsSave |= Settings->FrameRateOption != NormalizedFrameRateOption;
		Settings->GraphicsQualityTier = NormalizedGraphicsTier;
		Settings->FrameRateOption = NormalizedFrameRateOption;
	}

	// Persisted config is untrusted input. NaN/Inf cannot be compared or
	// clamped safely and must never reach an active audio component.
	const float ClampedMusicVolume = FMath::IsFinite(Settings->MusicVolume)
		? FMath::Clamp(Settings->MusicVolume, 0.0f, 1.0f) : 1.0f;
	const float ClampedSfxVolume = FMath::IsFinite(Settings->SfxVolume)
		? FMath::Clamp(Settings->SfxVolume, 0.0f, 1.0f) : 1.0f;
	bNeedsSave |= !FMath::IsFinite(Settings->MusicVolume)
		|| !FMath::IsNearlyEqual(Settings->MusicVolume, ClampedMusicVolume);
	bNeedsSave |= !FMath::IsFinite(Settings->SfxVolume)
		|| !FMath::IsNearlyEqual(Settings->SfxVolume, ClampedSfxVolume);
	Settings->MusicVolume = ClampedMusicVolume;
	Settings->SfxVolume = ClampedSfxVolume;

	bSettingsLoaded = true;
	if (bNeedsSave)
	{
		PersistProjectSettings();
	}
	ApplyVideoSettings();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Loaded Rok client settings: graphics=%d frame_rate=%d music=%.2f sfx=%.2f personality_overrides=%d"),
		Settings->GraphicsQualityTier,
		Settings->FrameRateOption,
		Settings->MusicVolume,
		Settings->SfxVolume,
		Settings->PersonalityToggleOverrides.Num());
	return true;
}

ERokClientGraphicsQualityTier URokClientSettingsSubsystem::GetGraphicsQualityTier() const
{
	return Settings
		? NormalizeGraphicsQualityTier(Settings->GraphicsQualityTier)
		: ERokClientGraphicsQualityTier::High;
}

void URokClientSettingsSubsystem::SetGraphicsQualityTier(ERokClientGraphicsQualityTier NewTier)
{
	if (!Settings)
	{
		LoadSettings();
	}
	if (!Settings)
	{
		return;
	}

	Settings->GraphicsQualityTier = static_cast<int32>(
		NormalizeGraphicsQualityTier(static_cast<int32>(NewTier)));
	ApplyVideoSettings();
	PersistProjectSettings();
}

ERokClientFrameRateOption URokClientSettingsSubsystem::GetFrameRateOption() const
{
	return Settings
		? NormalizeFrameRateOption(Settings->FrameRateOption)
		: ERokClientFrameRateOption::High;
}

void URokClientSettingsSubsystem::SetFrameRateOption(ERokClientFrameRateOption NewOption)
{
	if (!Settings)
	{
		LoadSettings();
	}
	if (!Settings)
	{
		return;
	}

	Settings->FrameRateOption = static_cast<int32>(
		NormalizeFrameRateOption(static_cast<int32>(NewOption)));
	ApplyVideoSettings();
	PersistProjectSettings();
}

float URokClientSettingsSubsystem::GetCurrentFrameRateLimit() const
{
	return GetFrameRateLimit(GetFrameRateOption());
}

float URokClientSettingsSubsystem::GetMusicVolume() const
{
	return Settings && FMath::IsFinite(Settings->MusicVolume)
		? FMath::Clamp(Settings->MusicVolume, 0.0f, 1.0f) : 1.0f;
}

void URokClientSettingsSubsystem::SetMusicVolume(float NewVolume)
{
	if (!FMath::IsFinite(NewVolume)) return;
	if (!Settings)
	{
		LoadSettings();
	}
	if (!Settings)
	{
		return;
	}

	const float ClampedVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Settings->MusicVolume, ClampedVolume))
	{
		return;
	}

	Settings->MusicVolume = ClampedVolume;
	PersistProjectSettings();
	OnMusicVolumeChanged.Broadcast(ClampedVolume);
}

float URokClientSettingsSubsystem::GetSfxVolume() const
{
	return Settings && FMath::IsFinite(Settings->SfxVolume)
		? FMath::Clamp(Settings->SfxVolume, 0.0f, 1.0f) : 1.0f;
}

void URokClientSettingsSubsystem::SetSfxVolume(float NewVolume)
{
	if (!FMath::IsFinite(NewVolume)) return;
	if (!Settings)
	{
		LoadSettings();
	}
	if (!Settings)
	{
		return;
	}

	const float ClampedVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Settings->SfxVolume, ClampedVolume))
	{
		return;
	}

	Settings->SfxVolume = ClampedVolume;
	PersistProjectSettings();
	OnSfxVolumeChanged.Broadcast(ClampedVolume);
}

bool URokClientSettingsSubsystem::GetPersonalityToggle(FName Name) const
{
	const FRokPersonalityToggleDefinition* Definition = FindPersonalityDefinition(Name);
	if (!Definition)
	{
		return false;
	}

	if (Settings)
	{
		if (const bool* Override = Settings->PersonalityToggleOverrides.Find(Name))
		{
			return *Override;
		}
	}
	return Definition->bDefaultEnabled;
}

void URokClientSettingsSubsystem::SetPersonalityToggle(FName Name, bool bEnabled)
{
	if (Name.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ignored Rok personality setting with an empty name."));
		return;
	}

	const FRokPersonalityToggleDefinition* Definition = FindPersonalityDefinition(Name);
	if (!Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ignored unknown Rok personality setting: %s"), *Name.ToString());
		return;
	}

	if (!Settings)
	{
		LoadSettings();
	}
	if (!Settings)
	{
		return;
	}

	if (const bool* Existing = Settings->PersonalityToggleOverrides.Find(Name))
	{
		if (*Existing == bEnabled)
		{
			return;
		}
	}
	else
	{
		if (Definition->bDefaultEnabled == bEnabled)
		{
			return;
		}
	}

	Settings->PersonalityToggleOverrides.Add(Name, bEnabled);
	PersistProjectSettings();
	OnPersonalityToggleChanged.Broadcast(Name, bEnabled);
}

bool URokClientSettingsSubsystem::IsKnownPersonalityToggle(FName Name) const
{
	return FindPersonalityDefinition(Name) != nullptr;
}

TArray<FRokPersonalityToggleDefinition> URokClientSettingsSubsystem::GetPersonalityToggleDefinitions() const
{
	return GetKnownPersonalityDefinitions();
}

void URokClientSettingsSubsystem::ApplyVideoSettings() const
{
	if (!Settings || !GEngine || IsRunningDedicatedServer())
	{
		return;
	}

	UGameUserSettings* GameUserSettings = GEngine->GetGameUserSettings();
	if (!GameUserSettings)
	{
		return;
	}

	const ERokClientGraphicsQualityTier GraphicsTier =
		NormalizeGraphicsQualityTier(Settings->GraphicsQualityTier);
	const ERokClientFrameRateOption FrameRateOption =
		NormalizeFrameRateOption(Settings->FrameRateOption);

	// Unity levels 1..3 map to UE scalability levels 0..2.
	GameUserSettings->SetOverallScalabilityLevel(static_cast<int32>(GraphicsTier) - 1);
	GameUserSettings->SetFrameRateLimit(GetFrameRateLimit(FrameRateOption));
	// Unity QualitySetting explicitly disables v-sync before applying its cap.
	GameUserSettings->SetVSyncEnabled(false);
	GameUserSettings->ApplyNonResolutionSettings();
	// Saving scalability from PIE writes global editor scalability to
	// GEditorSettingsIni. The project-owned option values still persist through
	// URokClientSettingsConfig and are reapplied on the next PIE session.
	if (!GIsEditor)
	{
		GameUserSettings->SaveSettings();
	}
}

void URokClientSettingsSubsystem::PersistProjectSettings() const
{
	if (!Settings)
	{
		return;
	}

	Settings->SaveConfig(CPF_Config, *GGameUserSettingsIni);
	if (GConfig)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}
