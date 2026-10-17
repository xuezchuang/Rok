#pragma once

#include "CoreMinimal.h"

struct FRokCityAudioEvent
{
	int32 Id = 0;
	FString Name;
	bool bMusic = false;
	int32 SourceMode = 0;
	int32 SourceMaxCount = 0;
	int32 SourceMaxCountAction = 0;
	float IntervalMinSeconds = 0.0f;
	float IntervalMaxSeconds = 0.0f;
	float FadeSeconds = 0.0f;
	TArray<FString> ClipNames;
	TArray<FString> ClipObjectPaths;
};

namespace RokCityAudioCatalog
{
	ROK_API bool Load(TMap<int32, FRokCityAudioEvent>& OutEvents, FString& Error);
#if WITH_DEV_AUTOMATION_TESTS
	ROK_API bool LoadBytesForTesting(TConstArrayView<uint8> Bytes,
		TMap<int32, FRokCityAudioEvent>& OutEvents, FString& Error);
#endif
}

// Deterministic source mode-2 scheduler. The caller provides uniform draws and
// natural clip completion; no audio device, timer or server clock is involved.
class ROK_API FRokCityBgmSequence
{
public:
	bool Start(const FRokCityAudioEvent& Event, float UniformDraw, int32& OutClipIndex);
	bool FinishNaturally(const FRokCityAudioEvent& Event, float UniformDelayDraw);
	bool Advance(const FRokCityAudioEvent& Event, float DeltaSeconds,
		float UniformClipDraw, int32& OutClipIndex);
	void Reset();
	int32 GetClipIndex() const { return ClipIndex; }
	float GetPendingDelaySeconds() const { return bWaiting ? PendingDelaySeconds : 0.0f; }
	bool IsWaiting() const { return bWaiting; }
	bool IsStarted() const { return ClipIndex != INDEX_NONE; }
	int32 GetCompletedCount() const { return CompletedCount; }

private:
	int32 ClipIndex = INDEX_NONE;
	float PendingDelaySeconds = 0.0f;
	bool bWaiting = false;
	int32 CompletedCount = 0;
};
