#include "RokCityAudioCatalog.h"
#include "RokTrainArmyMeshData.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
const TCHAR* ExpectedCatalogHash = TEXT("53a61954423b3d383b3ac68a34ea0be913022e4cfd854c5033baf57ffa6c1d9d");
const TCHAR* ExpectedGainPolicy = TEXT("AudioService AudioConfig ignores AudioInfo.volume; multiply source voice once by channel slider, not by table volume");
constexpr int32 SourceIds[] = {1007, 1008, 4001, 4005, 4037, 4048, 4051};

bool Parse(TConstArrayView<uint8> Bytes, TMap<int32, FRokCityAudioEvent>& OutEvents, FString& Error)
{
	OutEvents.Reset(); Error.Reset();
	if (Bytes.IsEmpty() || Bytes.Num() > 32 * 1024 || RokTrainArmyMesh::Sha256(Bytes) != ExpectedCatalogHash)
	{
		Error = TEXT("City audio catalog source identity invalid"); return false;
	}
	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
	const FString Json(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		Error = TEXT("City audio catalog JSON invalid"); return false;
	}
	FString Schema, Policy, GainPolicy;
	int32 EventCount = 0, SourceRows = 0;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("rok.city_audio_source.v1")
		|| !Root->TryGetStringField(TEXT("policy"), Policy) || Policy != TEXT("local_audio_only_no_network_no_account_state")
		|| !Root->TryGetStringField(TEXT("gain_policy"), GainPolicy) || GainPolicy != ExpectedGainPolicy
		|| !Root->TryGetNumberField(TEXT("source_table_row_count"), SourceRows) || SourceRows != 650
		|| !Root->TryGetNumberField(TEXT("event_count"), EventCount) || EventCount != 7
		|| !Root->TryGetArrayField(TEXT("events"), Rows) || !Rows || Rows->Num() != 7)
	{
		Error = TEXT("City audio catalog schema invalid"); return false;
	}
	TMap<int32, FRokCityAudioEvent> Parsed;
	for (int32 Index = 0; Index < Rows->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* RowPointer = nullptr;
		if (!(*Rows)[Index].IsValid() || !(*Rows)[Index]->TryGetObject(RowPointer)
			|| !RowPointer || !RowPointer->IsValid())
		{
			Error = TEXT("City audio event must be an object"); return false;
		}
		const TSharedPtr<FJsonObject>& Row = *RowPointer;
		FRokCityAudioEvent Event;
		FString Channel;
		int32 GroupId = 0;
		float TableVolume = 0.0f;
		const TArray<TSharedPtr<FJsonValue>>* Intervals = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Clips = nullptr;
		if (!Row->TryGetNumberField(TEXT("id"), Event.Id) || Event.Id != SourceIds[Index]
			|| !Row->TryGetStringField(TEXT("name"), Event.Name) || Event.Name.IsEmpty()
			|| !Row->TryGetStringField(TEXT("channel"), Channel)
			|| !Row->TryGetNumberField(TEXT("source_group_id"), GroupId)
			|| !Row->TryGetNumberField(TEXT("source_mode"), Event.SourceMode)
			|| !Row->TryGetNumberField(TEXT("source_max_count"), Event.SourceMaxCount)
			|| !Row->TryGetNumberField(TEXT("source_max_count_action"), Event.SourceMaxCountAction)
			|| !Row->TryGetNumberField(TEXT("source_table_volume"), TableVolume)
			|| !Row->TryGetNumberField(TEXT("source_fade_time_seconds"), Event.FadeSeconds)
			|| !Row->TryGetArrayField(TEXT("source_interval_seconds"), Intervals) || !Intervals
			|| !Row->TryGetArrayField(TEXT("clips"), Clips) || !Clips
			|| !FMath::IsFinite(TableVolume) || !FMath::IsFinite(Event.FadeSeconds)
			|| Event.SourceMaxCount != 1 || Event.SourceMaxCountAction != 1)
		{
			Error = TEXT("City audio event fields invalid"); return false;
		}
		Event.bMusic = Index < 2;
		if (Channel != (Event.bMusic ? TEXT("music") : TEXT("sfx"))
			|| GroupId != (Event.bMusic ? 1 : 4)
			|| Event.SourceMode != (Event.bMusic ? 2 : 0)
			|| Intervals->Num() != (Event.bMusic ? 2 : 1)
			|| Clips->Num() != (Event.bMusic ? 2 : 1))
		{
			Error = TEXT("City audio event classification invalid"); return false;
		}
		const auto ReadInterval = [Intervals](int32 At, float& Out) -> bool
		{
			double Number = 0.0;
			if (!(*Intervals)[At].IsValid() || !(*Intervals)[At]->TryGetNumber(Number)
				|| !FMath::IsFinite(Number) || Number < 0.0 || Number > 60.0) return false;
			Out = static_cast<float>(Number); return true;
		};
		if (!ReadInterval(0, Event.IntervalMinSeconds)
			|| (Event.bMusic && !ReadInterval(1, Event.IntervalMaxSeconds)))
		{
			Error = TEXT("City audio interval invalid"); return false;
		}
		if (!Event.bMusic) Event.IntervalMaxSeconds = Event.IntervalMinSeconds;
		if (Event.IntervalMaxSeconds < Event.IntervalMinSeconds) { Error = TEXT("City audio interval order invalid"); return false; }
		for (const TSharedPtr<FJsonValue>& ClipValue : *Clips)
		{
			const TSharedPtr<FJsonObject>* ClipPointer = nullptr;
			FString Name, Path;
			if (!ClipValue.IsValid() || !ClipValue->TryGetObject(ClipPointer)
				|| !ClipPointer || !ClipPointer->IsValid()
				|| !(*ClipPointer)->TryGetStringField(TEXT("name"), Name)
				|| !(*ClipPointer)->TryGetStringField(TEXT("ue_object_path"), Path)
				|| Name.IsEmpty() || Event.ClipNames.Contains(Name)
				|| Path != FString::Printf(TEXT("/Game/RokPrototype/UI/Audio/%s%s.%s"),
					Event.bMusic ? TEXT("CityBgm/") : TEXT(""), *Name, *Name))
			{
				Error = TEXT("City audio clip identity invalid"); return false;
			}
			Event.ClipNames.Add(Name);
			Event.ClipObjectPaths.Add(Path);
		}
		Parsed.Add(Event.Id, MoveTemp(Event));
	}
	OutEvents = MoveTemp(Parsed);
	return true;
}
}

bool RokCityAudioCatalog::Load(TMap<int32, FRokCityAudioEvent>& OutEvents, FString& Error)
{
	TArray<uint8> Bytes;
	const FString Path = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/CityAudio/catalog.json");
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutEvents.Reset(); Error = TEXT("City audio source catalog missing"); return false;
	}
	return Parse(Bytes, OutEvents, Error);
}

#if WITH_DEV_AUTOMATION_TESTS
bool RokCityAudioCatalog::LoadBytesForTesting(TConstArrayView<uint8> Bytes,
	TMap<int32, FRokCityAudioEvent>& OutEvents, FString& Error)
{
	return Parse(Bytes, OutEvents, Error);
}
#endif

bool FRokCityBgmSequence::Start(const FRokCityAudioEvent& Event, float UniformDraw, int32& OutClipIndex)
{
	Reset(); OutClipIndex = INDEX_NONE;
	if (!Event.bMusic || Event.SourceMode != 2 || Event.ClipNames.Num() < 2
		|| !FMath::IsFinite(UniformDraw) || UniformDraw < 0.0f || UniformDraw >= 1.0f) return false;
	ClipIndex = FMath::FloorToInt(UniformDraw * Event.ClipNames.Num());
	OutClipIndex = ClipIndex;
	return true;
}

bool FRokCityBgmSequence::FinishNaturally(const FRokCityAudioEvent& Event, float UniformDelayDraw)
{
	if (!IsStarted() || bWaiting || !Event.bMusic || !FMath::IsFinite(UniformDelayDraw)
		|| UniformDelayDraw < 0.0f || UniformDelayDraw >= 1.0f
		|| Event.IntervalMinSeconds < 0.0f || Event.IntervalMaxSeconds < Event.IntervalMinSeconds) return false;
	PendingDelaySeconds = FMath::Lerp(Event.IntervalMinSeconds, Event.IntervalMaxSeconds, UniformDelayDraw);
	bWaiting = true;
	++CompletedCount;
	return true;
}

bool FRokCityBgmSequence::Advance(const FRokCityAudioEvent& Event, float DeltaSeconds,
	float UniformClipDraw, int32& OutClipIndex)
{
	OutClipIndex = INDEX_NONE;
	if (!bWaiting || !Event.bMusic || Event.ClipNames.Num() < 2
		|| !Event.ClipNames.IsValidIndex(ClipIndex)
		|| !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f
		|| !FMath::IsFinite(UniformClipDraw) || UniformClipDraw < 0.0f || UniformClipDraw >= 1.0f) return false;
	PendingDelaySeconds = FMath::Max(0.0f, PendingDelaySeconds - DeltaSeconds);
	if (PendingDelaySeconds > 0.0f) return false;
	int32 Candidate = FMath::FloorToInt(UniformClipDraw * Event.ClipNames.Num());
	if (Candidate == ClipIndex)
	{
		for (int32 Offset = 1; Offset < Event.ClipNames.Num(); ++Offset)
		{
			const int32 Alternative = (Candidate + Offset) % Event.ClipNames.Num();
			if (Event.ClipNames[Alternative] != Event.ClipNames[ClipIndex])
			{
				Candidate = Alternative; break;
			}
		}
	}
	if (Candidate == ClipIndex || !Event.ClipNames.IsValidIndex(Candidate)) return false;
	ClipIndex = Candidate;
	bWaiting = false;
	OutClipIndex = ClipIndex;
	return true;
}

void FRokCityBgmSequence::Reset()
{
	ClipIndex = INDEX_NONE;
	PendingDelaySeconds = 0.0f;
	bWaiting = false;
	CompletedCount = 0;
}
