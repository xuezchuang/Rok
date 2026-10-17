#include "RokAudioRouterSubsystem.h"

#include "RokClientSettingsSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

namespace
{
bool IsAudioDeviceSuppressedForAutomation()
{
#if WITH_DEV_AUTOMATION_TESTS
	return FAutomationTestFramework::Get().GetCurrentTest() != nullptr;
#else
	return false;
#endif
}
}

void URokAudioRouterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Random.Initialize(FMath::Rand());
	FString CatalogError;
	bSourceEventsLoaded = RokCityAudioCatalog::Load(SourceEvents, CatalogError);
	if (!bSourceEventsLoaded)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok city audio source catalog unavailable: %s"), *CatalogError);
	}
	ClientSettings = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokClientSettingsSubsystem>() : nullptr;
	if (ClientSettings)
	{
		// LoadSettings does not broadcast. Pull before binding so late consumers
		// start at the persisted values rather than briefly playing at full gain.
		MusicVolume = ClientSettings->GetMusicVolume();
		SfxVolume = ClientSettings->GetSfxVolume();
		ClientSettings->OnMusicVolumeChanged.AddUniqueDynamic(this, &URokAudioRouterSubsystem::HandleMusicVolumeChanged);
		ClientSettings->OnSfxVolumeChanged.AddUniqueDynamic(this, &URokAudioRouterSubsystem::HandleSfxVolumeChanged);
	}
	bInitialized = true;
}

void URokAudioRouterSubsystem::Deinitialize()
{
	bInitialized = false;
	if (ClientSettings)
	{
		ClientSettings->OnMusicVolumeChanged.RemoveDynamic(this, &URokAudioRouterSubsystem::HandleMusicVolumeChanged);
		ClientSettings->OnSfxVolumeChanged.RemoveDynamic(this, &URokAudioRouterSubsystem::HandleSfxVolumeChanged);
	}
	StopCityBgm();
	for (const TWeakObjectPtr<UAudioComponent>& WeakComponent : ActiveSfxComponents)
	{
		if (UAudioComponent* Component = WeakComponent.Get()) Component->Stop();
	}
	ActiveSfxComponents.Reset();
	LastSfxBySourceEvent.Reset();
	LastFeedbackComponent.Reset();
	SourceEvents.Reset();
	ClientSettings = nullptr;
	Super::Deinitialize();
}

TStatId URokAudioRouterSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokAudioRouterSubsystem, STATGROUP_Tickables);
}

bool URokAudioRouterSubsystem::IsTickable() const
{
	return bInitialized && !IsTemplate() && CurrentBgmEventId != 0;
}

UWorld* URokAudioRouterSubsystem::GetTickableGameObjectWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void URokAudioRouterSubsystem::Tick(float DeltaTime)
{
	if (!IsTickable() || !FMath::IsFinite(DeltaTime) || DeltaTime < 0.0f) return;
	if (BgmComponent && BgmComponent->IsPlaying()) CurrentTrackElapsedSeconds += DeltaTime;
	const FRokCityAudioEvent* Event = SourceEvents.Find(CurrentBgmEventId);
	if (Event && BgmSequence.IsWaiting())
	{
		int32 NextClipIndex = INDEX_NONE;
		if (BgmSequence.Advance(*Event, DeltaTime, Random.FRand(), NextClipIndex)
			&& !StartBgmClip(NextClipIndex))
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok city BGM next source clip failed to start; event=%d"), CurrentBgmEventId);
			StopCityBgm();
		}
	}
}

bool URokAudioRouterSubsystem::PlaySfxTracked(USoundBase* Sound, UAudioComponent*& OutComponent)
{
	OutComponent = nullptr;
	UWorld* World = GetTickableGameObjectWorld();
	if (!bInitialized || !Sound || !World || IsAudioDeviceSuppressedForAutomation()
		|| !World->bAllowAudioPlayback || World->IsNetMode(NM_DedicatedServer)) return false;
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Sound, SfxVolume,
		1.0f, 0.0f, nullptr, false, true);
	if (!Component) return false;
	// AudioInfo's bounded UI events each have maxCount=1 and StopPrevious.
	// The interval field is not a one-shot throttle: AudioService destroys a
	// non-looping handler before its interval branch (see UpdateHandler).
	for (const TPair<int32, FRokCityAudioEvent>& Pair : SourceEvents)
	{
		const FRokCityAudioEvent& Event = Pair.Value;
		if (!Event.bMusic && Event.ClipObjectPaths.Num() == 1
			&& Event.ClipObjectPaths[0] == Sound->GetPathName())
		{
			if (TWeakObjectPtr<UAudioComponent>* Prior = LastSfxBySourceEvent.Find(Event.Id))
			{
				if (UAudioComponent* PreviousComponent = Prior->Get()) PreviousComponent->Stop();
			}
			LastSfxBySourceEvent.Add(Event.Id, Component);
			break;
		}
	}
	Component->OnAudioFinishedNative.AddUObject(this, &URokAudioRouterSubsystem::HandleSfxFinished);
	ActiveSfxComponents.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Item) { return !Item.IsValid(); });
	ActiveSfxComponents.Add(Component);
	OutComponent = Component;
	Component->Play();
	return true;
}

bool URokAudioRouterSubsystem::PlaySfx(USoundBase* Sound)
{
	UAudioComponent* Component = nullptr;
	return PlaySfxTracked(Sound, Component);
}

bool URokAudioRouterSubsystem::PlaySourceSfx4005()
{
	++FeedbackSequence;
	LastFeedbackPreChangeVolume = SfxVolume;
	LastFeedbackComponentVolume = -1.0f;
	LastFeedbackSoundPath.Reset();
	bLastFeedbackComponentCreated = false;
	LastFeedbackComponent.Reset();
	const FRokCityAudioEvent* Event = bSourceEventsLoaded ? SourceEvents.Find(4005) : nullptr;
	if (!Event || Event->ClipObjectPaths.Num() != 1) return false;
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Event->ClipObjectPaths[0]);
	if (!Sound) return false;
	LastFeedbackSoundPath = Sound->GetPathName();
	UAudioComponent* Component = nullptr;
	bLastFeedbackComponentCreated = PlaySfxTracked(Sound, Component);
	if (Component)
	{
		LastFeedbackComponent = Component;
		LastFeedbackComponentVolume = Component->VolumeMultiplier;
	}
	return bLastFeedbackComponentCreated;
}

bool URokAudioRouterSubsystem::SetCityBgmNight(bool bNight)
{
	if (!bInitialized || !bSourceEventsLoaded || IsAudioDeviceSuppressedForAutomation()) return false;
	const int32 EventId = bNight ? 1008 : 1007;
	if (CurrentBgmEventId == EventId) return true;
	const FRokCityAudioEvent* Event = SourceEvents.Find(EventId);
	UWorld* World = GetTickableGameObjectWorld();
	if (!Event || !Event->bMusic || Event->ClipObjectPaths.Num() != 2
		|| !World || !World->bAllowAudioPlayback || World->IsNetMode(NM_DedicatedServer)) return false;
	TArray<TObjectPtr<USoundBase>> NewSounds;
	for (const FString& ObjectPath : Event->ClipObjectPaths)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *ObjectPath);
		// A muted BGM must keep its current track clock running, so the newly
		// imported city tracks require PlayWhenSilent and individual non-looping clips.
		if (!Sound || Sound->VirtualizationMode != EVirtualizationMode::PlayWhenSilent || Sound->IsLooping()) return false;
		NewSounds.Add(Sound);
	}
	int32 FirstClip = INDEX_NONE;
	FRokCityBgmSequence NewSequence;
	if (!NewSequence.Start(*Event, Random.FRand(), FirstClip)) return false;
	RetireCurrentBgm(0.8f); // AudioService.PlayBgm fades the previous event to stop.
	CurrentBgmSounds = MoveTemp(NewSounds);
	CurrentBgmEventId = EventId;
	BgmSequence = MoveTemp(NewSequence);
	if (!StartBgmClip(FirstClip))
	{
		StopCityBgm();
		return false;
	}
	return true;
}

bool URokAudioRouterSubsystem::StartBgmClip(int32 ClipIndex)
{
	const FRokCityAudioEvent* Event = SourceEvents.Find(CurrentBgmEventId);
	UWorld* World = GetTickableGameObjectWorld();
	if (!Event || !CurrentBgmSounds.IsValidIndex(ClipIndex) || !World) return false;
	USoundBase* Sound = CurrentBgmSounds[ClipIndex];
	if (!Sound) return false;
	if (!BgmComponent)
	{
		BgmComponent = UGameplayStatics::CreateSound2D(World, Sound, MusicVolume,
			1.0f, 0.0f, nullptr, true, false);
		if (!BgmComponent) return false;
		BgmComponent->OnAudioFinishedNative.AddUObject(this, &URokAudioRouterSubsystem::HandleBgmFinished);
	}
	else
	{
		BgmComponent->SetSound(Sound);
		BgmComponent->SetVolumeMultiplier(MusicVolume);
	}
	CurrentTrackStartWorldSeconds = World->GetTimeSeconds();
	CurrentTrackElapsedSeconds = 0.0;
	BgmComponent->FadeIn(Event->FadeSeconds, 1.0f);
	return true;
}

void URokAudioRouterSubsystem::RetireCurrentBgm(float FadeSeconds)
{
	UAudioComponent* Old = BgmComponent;
	BgmComponent = nullptr;
	if (!Old) return;
	if (FadeSeconds > 0.0f && Old->IsPlaying())
	{
		RetiringBgmComponents.Add(Old);
		Old->FadeOut(FadeSeconds, 0.0f);
	}
	else
	{
		Old->Stop();
		Old->DestroyComponent();
	}
}

void URokAudioRouterSubsystem::StopCityBgm()
{
	CurrentBgmEventId = 0;
	BgmSequence.Reset();
	CurrentBgmSounds.Reset();
	CurrentTrackStartWorldSeconds = -1.0;
	CurrentTrackElapsedSeconds = 0.0;
	RetireCurrentBgm(0.0f);
	// Stop may synchronously invoke OnAudioFinishedNative. Move the list out
	// before iterating so that callback cannot mutate the active iteration.
	TArray<TObjectPtr<UAudioComponent>> ToStop = MoveTemp(RetiringBgmComponents);
	RetiringBgmComponents.Reset();
	for (UAudioComponent* Old : ToStop)
	{
		if (Old) { Old->Stop(); Old->DestroyComponent(); }
	}
}

void URokAudioRouterSubsystem::HandleBgmFinished(UAudioComponent* Component)
{
	if (!Component) return;
	if (Component == BgmComponent)
	{
		const FRokCityAudioEvent* Event = SourceEvents.Find(CurrentBgmEventId);
		if (Event && BgmSequence.FinishNaturally(*Event, Random.FRand()))
		{
			++TotalBgmNaturallyFinishedCount;
		}
	}
	else if (RetiringBgmComponents.Remove(Component) > 0)
	{
		Component->DestroyComponent();
	}
}

void URokAudioRouterSubsystem::HandleSfxFinished(UAudioComponent* Component)
{
	ActiveSfxComponents.RemoveAll([Component](const TWeakObjectPtr<UAudioComponent>& Item)
	{
		return !Item.IsValid() || Item.Get() == Component;
	});
}

void URokAudioRouterSubsystem::HandleMusicVolumeChanged(float Volume)
{
	if (!FMath::IsFinite(Volume)) return;
	MusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyMusicVolume();
}

void URokAudioRouterSubsystem::HandleSfxVolumeChanged(float Volume)
{
	if (!FMath::IsFinite(Volume)) return;
	SfxVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplySfxVolume();
}

void URokAudioRouterSubsystem::ApplyMusicVolume()
{
	if (BgmComponent) BgmComponent->SetVolumeMultiplier(MusicVolume);
	for (UAudioComponent* Old : RetiringBgmComponents)
	{
		if (Old) Old->SetVolumeMultiplier(MusicVolume);
	}
}

void URokAudioRouterSubsystem::ApplySfxVolume()
{
	ActiveSfxComponents.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Item) { return !Item.IsValid(); });
	for (const TWeakObjectPtr<UAudioComponent>& WeakComponent : ActiveSfxComponents)
	{
		if (UAudioComponent* Component = WeakComponent.Get())
		{
			if (Component->IsPlaying()) Component->SetVolumeMultiplier(SfxVolume);
		}
	}
}

int32 URokAudioRouterSubsystem::GetActiveTrackedSfxCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UAudioComponent>& WeakComponent : ActiveSfxComponents)
	{
		if (const UAudioComponent* Component = WeakComponent.Get())
		{
			if (Component->IsPlaying()) ++Count;
		}
	}
	return Count;
}

FRokAudioRouterVoiceSnapshot URokAudioRouterSubsystem::GetVoiceSnapshot() const
{
	FRokAudioRouterVoiceSnapshot Result;
	Result.MusicSetting = MusicVolume;
	Result.SfxSetting = SfxVolume;
	Result.CityBgmEventId = CurrentBgmEventId;
	Result.BgmNaturallyFinishedCount = TotalBgmNaturallyFinishedCount;
	Result.PendingNextDelaySeconds = BgmSequence.GetPendingDelaySeconds();
	Result.BgmTrackStartedWorldSeconds = CurrentTrackStartWorldSeconds;
	Result.BgmTrackElapsedSeconds = CurrentTrackElapsedSeconds;
	Result.ActiveSfxComponents = GetActiveTrackedSfxCount();
	Result.LastSfxFeedbackSequence = FeedbackSequence;
	Result.LastSfxFeedbackPreChangeVolume = LastFeedbackPreChangeVolume;
	Result.LastSfxFeedbackComponentVolumeMultiplier = LastFeedbackComponentVolume;
	if (const UAudioComponent* FeedbackComponent = LastFeedbackComponent.Get())
	{
		Result.LastSfxFeedbackCurrentVolumeMultiplier = FeedbackComponent->VolumeMultiplier;
		Result.bLastSfxFeedbackCurrentlyPlaying = FeedbackComponent->IsPlaying();
	}
	Result.LastSfxFeedbackSoundObjectPath = LastFeedbackSoundPath;
	Result.bLastSfxFeedbackComponentCreated = bLastFeedbackComponentCreated;
	if (BgmComponent)
	{
		Result.bBgmComponentPlaying = BgmComponent->IsPlaying();
		Result.BgmComponentVolumeMultiplier = BgmComponent->VolumeMultiplier;
		if (USoundBase* Sound = BgmComponent->GetSound())
		{
			Result.CurrentBgmTrackName = Sound->GetName();
			Result.CurrentBgmSoundObjectPath = Sound->GetPathName();
		}
	}
	return Result;
}
