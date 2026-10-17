#pragma once

#include "CoreMinimal.h"
#include "RokCityAudioCatalog.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "RokAudioRouterSubsystem.generated.h"

class UAudioComponent;
class URokClientSettingsSubsystem;
class USoundBase;

// Read-only diagnostics. ComponentVolumeMultiplier is read from the actual
// UAudioComponent; elapsed seconds are the router's world-tick clock, not a
// decoded sample cursor or proof of audible output.
struct FRokAudioRouterVoiceSnapshot
{
	float MusicSetting = 1.0f;
	float SfxSetting = 1.0f;
	int32 CityBgmEventId = 0;
	FString CurrentBgmTrackName;
	FString CurrentBgmSoundObjectPath;
	bool bBgmComponentPlaying = false;
	float BgmComponentVolumeMultiplier = -1.0f;
	double BgmTrackStartedWorldSeconds = -1.0;
	double BgmTrackElapsedSeconds = 0.0;
	int32 BgmNaturallyFinishedCount = 0;
	float PendingNextDelaySeconds = 0.0f;
	int32 ActiveSfxComponents = 0;
	uint64 LastSfxFeedbackSequence = 0;
	float LastSfxFeedbackPreChangeVolume = -1.0f;
	float LastSfxFeedbackComponentVolumeMultiplier = -1.0f;
	float LastSfxFeedbackCurrentVolumeMultiplier = -1.0f;
	bool bLastSfxFeedbackCurrentlyPlaying = false;
	FString LastSfxFeedbackSoundObjectPath;
	bool bLastSfxFeedbackComponentCreated = false;
};

// Project-owned, local-only channel router. No server/account requests and no
// mutation of existing imported SoundWave assets.
UCLASS()
class ROK_API URokAudioRouterSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

	UFUNCTION(BlueprintCallable, Category="Rok Audio")
	bool PlaySfx(USoundBase* Sound);

	/** Unity slider feedback ID 4005; call before changing either channel value. */
	UFUNCTION(BlueprintCallable, Category="Rok Audio")
	bool PlaySourceSfx4005();

	/** Source event 1007 (day) or 1008 (night); caller owns day/night choice. */
	UFUNCTION(BlueprintCallable, Category="Rok Audio")
	bool SetCityBgmNight(bool bNight);

	UFUNCTION(BlueprintCallable, Category="Rok Audio")
	void StopCityBgm();

	UFUNCTION(BlueprintPure, Category="Rok Audio")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category="Rok Audio")
	float GetSfxVolume() const { return SfxVolume; }

	UFUNCTION(BlueprintPure, Category="Rok Audio")
	int32 GetCurrentCityBgmEventId() const { return CurrentBgmEventId; }

	UFUNCTION(BlueprintPure, Category="Rok Audio")
	bool IsCityBgmActive() const { return CurrentBgmEventId != 0; }

	UFUNCTION(BlueprintPure, Category="Rok Audio")
	bool AreSourceEventsLoaded() const { return bSourceEventsLoaded; }

	int32 GetActiveTrackedSfxCount() const;
	FRokAudioRouterVoiceSnapshot GetVoiceSnapshot() const;

private:
	UFUNCTION()
	void HandleMusicVolumeChanged(float Volume);
	UFUNCTION()
	void HandleSfxVolumeChanged(float Volume);
	void HandleSfxFinished(UAudioComponent* Component);
	void HandleBgmFinished(UAudioComponent* Component);
	bool PlaySfxTracked(USoundBase* Sound, UAudioComponent*& OutComponent);
	bool StartBgmClip(int32 ClipIndex);
	void RetireCurrentBgm(float FadeSeconds);
	void ApplyMusicVolume();
	void ApplySfxVolume();

	UPROPERTY(Transient)
	TObjectPtr<URokClientSettingsSubsystem> ClientSettings;
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BgmComponent;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> RetiringBgmComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> CurrentBgmSounds;
	TArray<TWeakObjectPtr<UAudioComponent>> ActiveSfxComponents;
	TMap<int32, TWeakObjectPtr<UAudioComponent>> LastSfxBySourceEvent;
	TWeakObjectPtr<UAudioComponent> LastFeedbackComponent;
	TMap<int32, FRokCityAudioEvent> SourceEvents;
	FRokCityBgmSequence BgmSequence;
	FRandomStream Random;
	float MusicVolume = 1.0f;
	float SfxVolume = 1.0f;
	int32 CurrentBgmEventId = 0;
	double CurrentTrackStartWorldSeconds = -1.0;
	double CurrentTrackElapsedSeconds = 0.0;
	uint64 FeedbackSequence = 0;
	float LastFeedbackPreChangeVolume = -1.0f;
	float LastFeedbackComponentVolume = -1.0f;
	FString LastFeedbackSoundPath;
	bool bLastFeedbackComponentCreated = false;
	int32 TotalBgmNaturallyFinishedCount = 0;
	bool bSourceEventsLoaded = false;
	bool bInitialized = false;
};
