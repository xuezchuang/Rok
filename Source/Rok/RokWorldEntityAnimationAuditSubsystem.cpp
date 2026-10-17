#include "RokWorldEntityAnimationAuditSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
struct FSample { const TCHAR* Name; float Before; float After; float Seconds; int32 Cards; bool bFading; };
const FSample Samples[] = {
	{TEXT("lod501_exact_no_restart"),500,501,0.05f,43,false},
	{TEXT("lod501_cross_early"),501,501.01f,0.05f,43,true},
	{TEXT("lod501_cross_mid"),501,501.01f,0.17f,43,true},
	{TEXT("lod501_cross_normal"),501,501.01f,0.40f,43,false},
	{TEXT("lod900_exact_no_restart"),899,900,0.05f,43,false},
	{TEXT("lod900_cross_early"),900,900.01f,0.05f,15,true},
	{TEXT("lod900_cross_mid"),900,900.01f,0.17f,15,true},
	{TEXT("lod900_cross_normal"),900,900.01f,0.40f,15,false},
	{TEXT("far_icon"),1537,3500,0.40f,1,false},
	{TEXT("lod3000_exact_source_zero_scale"),2999,3000,0.05f,15,false},
	{TEXT("lod3000_cross_far_icon"),3000,3000.01f,0.05f,1,false},
	{TEXT("return_early"),3500,1537,0.05f,15,true},
	{TEXT("return_mid"),3500,1537,0.17f,15,true},
	{TEXT("return_normal"),3500,1537,0.40f,15,false},
	{TEXT("same_lod_no_restart"),899,850,0.05f,43,false},
};
}

bool URokWorldEntityAnimationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokWorldEntityAnimationAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokWorldEntityAnimationAuditSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game;
}

void URokWorldEntityAnimationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/WorldEntityAnimationAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokWorldEntityAnimationAuditSubsystem::ScreenshotProcessed);
}

void URokWorldEntityAnimationAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}

TStatId URokWorldEntityAnimationAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokWorldEntityAnimationAuditSubsystem, STATGROUP_Tickables);
}

bool URokWorldEntityAnimationAuditSubsystem::FindRuntime()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	if (!Project.Equals(TEXT("D:/ueDemo/Rok"), ESearchCase::IgnoreCase)
		|| GetWorld()->GetName() != TEXT("L_RokUnityWorldMapPrototype"))
	{
		Fail(TEXT("Wrong project or world; no camera mutation performed"));
		return false;
	}
	for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) Camera = *It;
	EntityCount = 0;
	for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld()); It; ++It)
	{
		if (It->GetEntityKind() != TEXT("barbarian")) continue;
		++EntityCount;
		if (It->GetSourceId() == 10102) Entity = *It;
	}
	if (!Camera.IsValid() || !Entity.IsValid() || EntityCount != 3)
	{
		Fail(FString::Printf(TEXT("Expected runtime camera, source 10102 and 3 barbarian actors; found %d"), EntityCount));
		return false;
	}
	Camera->FocusDisplayCoordinate(FVector2D(1148.0, 153.6666666667));
	return true;
}

void URokWorldEntityAnimationAuditSubsystem::BeginStep()
{
	++Step;
	if (Step >= UE_ARRAY_COUNT(Samples)) { Finish(); return; }
	Camera->SetCurrentDxf(Samples[Step].Before);
	Stage = 1;
	StageStarted = Elapsed;
}

TSharedRef<FJsonObject> URokWorldEntityAnimationAuditSubsystem::ReadState(bool bTrackAnimation)
{
	TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
	State->SetNumberField(TEXT("dxf"), Camera.IsValid() ? Camera->GetCurrentDxf() : -1);
	State->SetNumberField(TEXT("time_seconds"), Elapsed);
	State->SetNumberField(TEXT("frame"), static_cast<double>(Frame));
	TArray<TSharedPtr<FJsonValue>> Cards;
	float MinAlpha = 1.0f, MaxAlpha = 0.0f;
	int32 RawCards = 0;
	if (Entity.IsValid())
	{
		TArray<UStaticMeshComponent*> Components;
		Entity->GetComponents(Components);
		for (UStaticMeshComponent* Card : Components)
		{
			if (!Card->IsVisible() || Card->bHiddenInGame) continue;
			UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Card->GetMaterial(0));
			if (!Material) continue;
			float Alpha = Material->K2_GetScalarParameterValue(TEXT("FadeAmount"));
			float Multiplier = Material->K2_GetScalarParameterValue(TEXT("SpriteAlphaMultiplier"));
			UTexture* Texture = Material->K2_GetTextureParameterValue(TEXT("Sprite"));
			const FString TextureId = Texture ? Texture->GetPathName() : TEXT("missing");
			const FString CardId = Card->GetName();
			if (bTrackAnimation && Alpha >= 1.0f && Camera->GetCurrentDxf() < 3000)
			{
				if (const FString* Previous = PreviousTextures.Find(CardId); Previous && *Previous != TextureId) ++TextureTransitions;
				PreviousTextures.Add(CardId, TextureId);
			}
			else if (bTrackAnimation) PreviousTextures.Remove(CardId);
			MinAlpha = FMath::Min(MinAlpha, Alpha);
			MaxAlpha = FMath::Max(MaxAlpha, Alpha);
			if (FMath::IsNearlyEqual(Multiplier, 2.0f)) ++RawCards;
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("component"), CardId);
			Row->SetStringField(TEXT("texture"), TextureId);
			Row->SetNumberField(TEXT("fade_amount"), Alpha);
			Row->SetNumberField(TEXT("alpha_multiplier"), Multiplier);
			Row->SetStringField(TEXT("relative_scale"), Card->GetRelativeScale3D().ToString());
			Row->SetNumberField(TEXT("plane_scale_area"), FMath::Abs(Card->GetRelativeScale3D().X * Card->GetRelativeScale3D().Y));
			Cards.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	State->SetArrayField(TEXT("cards"), Cards);
	State->SetNumberField(TEXT("visible_cards"), Cards.Num());
	State->SetNumberField(TEXT("raw_fade_cards"), RawCards);
	State->SetNumberField(TEXT("min_fade"), MinAlpha);
	State->SetNumberField(TEXT("max_fade"), MaxAlpha);
	return State;
}

void URokWorldEntityAnimationAuditSubsystem::Capture()
{
	const FSample& Sample = Samples[Step];
	TSharedRef<FJsonObject> State = ReadState(false);
	if (State->GetIntegerField(TEXT("visible_cards")) != Sample.Cards
		|| !FMath::IsNearlyEqual(Camera->GetCurrentDxf(), Sample.After))
	{
		Fail(FString::Printf(TEXT("%s incorrect visible card count or camera dxf"), Sample.Name)); return;
	}
	if (Sample.After == 3000.0f || Sample.After == 3000.01f)
	{
		for (const TSharedPtr<FJsonValue>& Value : State->GetArrayField(TEXT("cards")))
		{
			const double Area = Value->AsObject()->GetNumberField(TEXT("plane_scale_area"));
			if ((Sample.After == 3000.0f && Area != 0.0)
				|| (Sample.After == 3000.01f && Area <= 0.0))
			{
				Fail(TEXT("3000 boundary does not preserve source zero-scale detail / nonzero far icon")); return;
			}
		}
	}
	const double Min = State->GetNumberField(TEXT("min_fade"));
	const double Max = State->GetNumberField(TEXT("max_fade"));
	const double ExpectedAlpha = FMath::Min(1.0, 3.0 * (Elapsed - StageStarted));
	// Actor and world-subsystem tick order can differ by one frame.
	// A tolerance larger than this source-order bound would hide bad rates.
	const double AlphaTolerance = 3.0 * SampleMaxTickDelta + 0.002;
	if (Sample.bFading && (FMath::Abs(Min - ExpectedAlpha) > AlphaTolerance
		|| FMath::Abs(Max - ExpectedAlpha) > AlphaTolerance))
	{
		Fail(FString::Printf(TEXT("%s alpha %.4f..%.4f differs from source dt*3 %.4f (+/-%.4f)"), Sample.Name, Min, Max, ExpectedAlpha, AlphaTolerance)); return;
	}
	if ((Sample.bFading && !(Min > 0 && Max < 1 && State->GetIntegerField(TEXT("raw_fade_cards")) == Sample.Cards))
		|| (!Sample.bFading && !(Min >= 1 && Max <= 1 && State->GetIntegerField(TEXT("raw_fade_cards")) == 0)))
	{
		Fail(FString::Printf(TEXT("%s expected %s but alpha range %.4f..%.4f"), Sample.Name, Sample.bFading ? TEXT("in-progress raw fade") : TEXT("normal decoded frame"), Min, Max)); return;
	}
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("sample"), Sample.Name);
	PendingCapture->SetNumberField(TEXT("requested_sample_seconds"), Sample.Seconds);
	PendingCapture->SetNumberField(TEXT("actual_sample_seconds"), Elapsed - StageStarted);
	PendingCapture->SetNumberField(TEXT("expected_source_alpha"), ExpectedAlpha);
	PendingCapture->SetNumberField(TEXT("alpha_tick_order_tolerance"), AlphaTolerance);
	PendingCapture->SetNumberField(TEXT("max_tick_delta"), SampleMaxTickDelta);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State);
	ScreenshotPath = OutputDirectory / (FString(Sample.Name) + TEXT(".png"));
	PendingCapture->SetStringField(TEXT("screenshot"), ScreenshotPath);
	RequestedAt = Elapsed;
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
}

void URokWorldEntityAnimationAuditSubsystem::ScreenshotProcessed()
{
	if (!PendingCapture.IsValid() || bFinished) return;
	const int64 Bytes = IFileManager::Get().FileSize(*ScreenshotPath);
	if (Bytes <= 0) { Fail(TEXT("Screenshot processed without a nonempty file")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), static_cast<double>(Bytes));
	TSharedRef<FJsonObject> ProcessedState = ReadState(false);
	PendingCapture->SetObjectField(TEXT("state_at_processed"), ProcessedState);
	if (Samples[Step].bFading && !(ProcessedState->GetNumberField(TEXT("min_fade")) > 0
		&& ProcessedState->GetNumberField(TEXT("max_fade")) < 1))
	{
		Fail(TEXT("Fade completed before screenshot processing; intermediate image is not accepted")); return;
	}
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture));
	PendingCapture.Reset();
	if (!Save(false)) { Fail(TEXT("Could not write incremental manifest")); return; }
	Stage = 3;
}

void URokWorldEntityAnimationAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += DeltaTime;
	if (Stage == 2) SampleMaxTickDelta = FMath::Max(SampleMaxTickDelta, DeltaTime);
	++Frame;
	if (Elapsed > 90) { Fail(TEXT("Audit timed out")); return; }
	if (PendingCapture.IsValid())
	{
		if (Elapsed - RequestedAt > 8) Fail(TEXT("Screenshot callback timed out"));
		return;
	}
	if (Step < 0)
	{
		if (Elapsed >= 6 && FindRuntime()) BeginStep();
		return;
	}
	if (!Camera.IsValid() || !Entity.IsValid()) { Fail(TEXT("Runtime actors destroyed")); return; }
	ReadState(true);
	if (Stage == 3) { BeginStep(); return; }
	if (Stage == 1 && Elapsed - StageStarted >= 0.8)
	{
		Camera->SetCurrentDxf(Samples[Step].After);
		Stage = 2;
		SampleMaxTickDelta = DeltaTime;
		StageStarted = Elapsed;
		return;
	}
	if (Stage == 2 && Elapsed - StageStarted >= Samples[Step].Seconds) Capture();
}

bool URokWorldEntityAnimationAuditSubsystem::Save(bool bComplete)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("rok.world_entity_animation_audit.v1"));
	Root->SetStringField(TEXT("card_count_semantics"), TEXT("component visibility flags, not guaranteed visible pixels; exact 3000 source unit curve has zero scale, recorded per card"));
	Root->SetBoolField(TEXT("complete"), bComplete);
	Root->SetStringField(TEXT("world"), GetWorld()->GetPathName());
	Root->SetNumberField(TEXT("barbarian_actor_count"), EntityCount);
	Root->SetNumberField(TEXT("source_id"), 10102);
	Root->SetNumberField(TEXT("observed_normal_texture_changes"), TextureTransitions);
	Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	Root->SetStringField(TEXT("open_acceptance_gates"), TEXT("Unity side-by-side visual comparison; GPU color space and polygon edges; source crowded formation threshold"));
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("manifest.json")));
}

void URokWorldEntityAnimationAuditSubsystem::Fail(const FString& Reason)
{
	Failures.Add(Reason);
	bFinished = true;
	UE_LOG(LogTemp, Error, TEXT("Rok world entity animation audit FAILED: %s"), *Reason);
	Save(false);
	FGenericPlatformMisc::RequestExit(false);
}

void URokWorldEntityAnimationAuditSubsystem::Finish()
{
	if (Captures.Num() != UE_ARRAY_COUNT(Samples) || TextureTransitions == 0)
	{
		Fail(TEXT("Missing captures or no live normal idle texture changes observed")); return;
	}
	bFinished = true;
	if (!Save(true)) { Fail(TEXT("Could not write complete manifest")); return; }
	UE_LOG(LogTemp, Display, TEXT("Rok world entity animation audit PASS: captures=%d idle_texture_changes=%d output=%s"), Captures.Num(), TextureTransitions, *OutputDirectory);
	FGenericPlatformMisc::RequestExit(false);
}
