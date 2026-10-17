#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RokClientSettingsSubsystem.generated.h"

UENUM(BlueprintType)
enum class ERokClientGraphicsQualityTier : uint8
{
	Invalid = 0 UMETA(Hidden),
	Low = 1 UMETA(DisplayName="Low"),
	Medium = 2 UMETA(DisplayName="Medium"),
	High = 3 UMETA(DisplayName="High")
};

UENUM(BlueprintType)
enum class ERokClientFrameRateOption : uint8
{
	Invalid = 0 UMETA(Hidden),
	Low = 1 UMETA(DisplayName="Low (30 FPS)"),
	Medium = 2 UMETA(DisplayName="Medium (30 FPS)"),
	High = 3 UMETA(DisplayName="High (60 FPS)")
};

/**
 * Source metadata from Unity SettingPersonality.csv.
 *
 * UnityResetSeconds is reference-only. IDs 3 and 6 use authoritative server
 * time in GeneralSettingProxy, so this local subsystem deliberately does not
 * emulate their timed reset with the device clock. Hidden Unity ID 7 (hide
 * alliance-gift purchase details) is also omitted because that visibility
 * cannot be enforced by a client-only setting.
 */
USTRUCT(BlueprintType)
struct FRokPersonalityToggleDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	int32 UnityId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	bool bDefaultEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	int32 UnityResetSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	bool bVisibleInUnity = true;

	UPROPERTY(BlueprintReadOnly, Category="Rok Client Settings")
	int32 UnityDisplayOrder = 0;
};

/**
 * Project-owned values that do not belong to UGameUserSettings.
 *
 * This object persists in the platform GameUserSettings.ini beside the engine
 * settings, under its own /Script/Rok.RokClientSettingsConfig section.
 */
UCLASS(Config=GameUserSettings, ConfigDoNotCheckDefaults)
class ROK_API URokClientSettingsConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	int32 SettingsVersion = 0;

	UPROPERTY(Config)
	int32 GraphicsQualityTier = static_cast<int32>(ERokClientGraphicsQualityTier::High);

	UPROPERTY(Config)
	int32 FrameRateOption = static_cast<int32>(ERokClientFrameRateOption::High);

	UPROPERTY(Config)
	float MusicVolume = 1.0f;

	UPROPERTY(Config)
	float SfxVolume = 1.0f;

	UPROPERTY(Config)
	TMap<FName, bool> PersonalityToggleOverrides;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRokClientVolumeChanged, float, Volume);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRokClientPersonalityToggleChanged, FName, Name, bool, bEnabled);

/**
 * Local, restart-persistent client settings used by the Unity GeneralSetting UI
 * migration. Graphics and frame-rate choices are applied through
 * UGameUserSettings. Audio values are persisted and broadcast to the
 * project-owned per-component Music/SFX router.
 */
UCLASS()
class ROK_API URokClientSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Reloads persisted values, validates them, and reapplies video settings. */
	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	bool LoadSettings();

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	bool AreSettingsLoaded() const { return bSettingsLoaded; }

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	ERokClientGraphicsQualityTier GetGraphicsQualityTier() const;

	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	void SetGraphicsQualityTier(ERokClientGraphicsQualityTier NewTier);

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	ERokClientFrameRateOption GetFrameRateOption() const;

	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	void SetFrameRateOption(ERokClientFrameRateOption NewOption);

	/** Returns the concrete limit used by the current frame-rate option. */
	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	float GetCurrentFrameRateLimit() const;

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	float GetMusicVolume() const;

	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	float GetSfxVolume() const;

	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	void SetSfxVolume(float NewVolume);

	/**
	 * Returns a stored override, or the SettingPersonality.csv default for a
	 * known name. Unknown names default to false.
	 */
	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	bool GetPersonalityToggle(FName Name) const;

	UFUNCTION(BlueprintCallable, Category="Rok Client Settings")
	void SetPersonalityToggle(FName Name, bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	bool IsKnownPersonalityToggle(FName Name) const;

	UFUNCTION(BlueprintPure, Category="Rok Client Settings")
	TArray<FRokPersonalityToggleDefinition> GetPersonalityToggleDefinitions() const;

	UPROPERTY(BlueprintAssignable, Category="Rok Client Settings")
	FRokClientVolumeChanged OnMusicVolumeChanged;

	UPROPERTY(BlueprintAssignable, Category="Rok Client Settings")
	FRokClientVolumeChanged OnSfxVolumeChanged;

	UPROPERTY(BlueprintAssignable, Category="Rok Client Settings")
	FRokClientPersonalityToggleChanged OnPersonalityToggleChanged;

private:
	UPROPERTY(Transient)
	TObjectPtr<URokClientSettingsConfig> Settings;

	bool bSettingsLoaded = false;

	void ApplyVideoSettings() const;
	void PersistProjectSettings() const;
};
