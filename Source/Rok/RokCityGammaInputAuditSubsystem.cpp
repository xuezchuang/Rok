#include "RokCityGammaInputAuditSubsystem.h"

#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

namespace
{
constexpr TCHAR ManifestRelative[] = TEXT("RokUnityMigration/CityGammaMaskedAssets/20261008T223046Z_11852/manifest.json");
constexpr TCHAR ManifestSha[] = TEXT("77df4b0aeb6ed4cc512eeea9b79b9037c95369ab4999afc5e8014b17b0f2ef62");
constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR TextureRoot[] = TEXT("/Game/RokUnity/City/GammaMaskedInputs/V1/");
constexpr int32 ExpectedTextures = 22;
constexpr double StartupTimeout = 45.0;
constexpr double ReadbackTimeout = 20.0;

bool ExactGameWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

FString HashFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}

FString AssetFilePath(const FString& ObjectPath)
{
	if (!ObjectPath.StartsWith(TEXT("/Game/"))) return FString();
	return FPaths::ProjectContentDir() / ObjectPath.RightChop(6) + TEXT(".uasset");
}

bool LoadJson(const FString& Path, TSharedPtr<FJsonObject>& Object)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object)
		&& Object.IsValid();
}

TArray<TSharedPtr<FJsonValue>> JsonStrings(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
	return Out;
}
}

struct FRokCityGammaInputReadback
{
	enum class EState : uint8 { Pending, CopySubmitted, Ready, Failed, Cancelled };
	FCriticalSection Mutex;
	TUniquePtr<FRHIGPUTextureReadback> Readback; // render thread only
	EState State = EState::Pending;
	FString Error;
	TArray<uint8> TightBGRA;
	int32 Width = 0, Height = 0, RowPitchPixels = 0, BufferHeight = 0;
	int32 Format = -1, Mips = 0, Samples = 0;
	bool bSrgb = false, bPollQueued = false;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	double CopySeconds = 0.0, ReadySeconds = 0.0;
};

bool URokCityGammaInputAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return ExactGameWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaInputsAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokCityGammaInputAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

TStatId URokCityGammaInputAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityGammaInputAuditSubsystem, STATGROUP_Tickables);
}

void URokCityGammaInputAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	StartFrame = GFrameCounter;
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/CityGammaInputsRuntime")
		/ FString::Printf(TEXT("%s_%u_%s"),
			*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*Directory)
		|| !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("City Gamma input audit cannot create unique directory: %s"), *Directory);
		bFinished = true;
	}
}

void URokCityGammaInputAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("Game world ended before all 22 GPU texture inputs were read"));
	CancelReadback();
	PinnedTextures.Empty();
	Super::Deinitialize();
}

bool URokCityGammaInputAuditSubsystem::Prepare()
{
	UWorld* World = GetWorld();
	if (!ExactGameWorld(World) || !GEngine || !GEngine->GameViewport
		|| GEngine->GameViewport->GetWorld() != World || !GEngine->GameViewport->Viewport
		|| !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->IsLocalController())
		return false;
	const FString ManifestPath = FPaths::ProjectSavedDir() / ManifestRelative;
	InputSha = HashFile(ManifestPath);
	if (InputSha != ManifestSha) { Finish(TEXT("Sealed 22-texture input manifest SHA differs")); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!LoadJson(ManifestPath, Root) || Root->GetStringField(TEXT("schema"))
		!= TEXT("rok.city_gamma_masked_inputs.prepared.v1")
		|| Root->GetStringField(TEXT("destination_root"))
		!= TEXT("/Game/RokUnity/City/GammaMaskedInputs/V1"))
	{
		Finish(TEXT("Masked-city prepared input manifest identity differs"));
		return false;
	}
	const auto& Resources = Root->GetArrayField(TEXT("resources"));
	if (Resources.Num() != ExpectedTextures || Root->GetObjectField(TEXT("counts"))
		->GetIntegerField(TEXT("deduplicated_texture_assets")) != ExpectedTextures)
	{
		Finish(TEXT("Prepared input resource count differs from 22"));
		return false;
	}
	TSet<FString> SeenAssets, SeenIds;
	for (const TSharedPtr<FJsonValue>& Value : Resources)
	{
		const TSharedPtr<FJsonObject> Item = Value->AsObject();
		if (!Item.IsValid()) { Finish(TEXT("Prepared input resource is not an object")); return false; }
		FRow Row;
		Row.Id = Item->GetStringField(TEXT("resource_id"));
		Row.Asset = Item->GetStringField(TEXT("asset"));
		Row.Role = Item->GetStringField(TEXT("role"));
		Row.ExpectedSha = Item->GetStringField(TEXT("bgra8_pixel_sha256"));
		Row.SourcePngSha = Item->GetStringField(TEXT("png_sha256"));
		Row.Width = Item->GetIntegerField(TEXT("width"));
		Row.Height = Item->GetIntegerField(TEXT("height"));
		if (SeenIds.Contains(Row.Id) || SeenAssets.Contains(Row.Asset)
			|| Row.Id.IsEmpty() || Row.Asset != FString(TextureRoot) + Row.Id
			|| !(Row.Role == TEXT("sprite") || Row.Role == TEXT("mask"))
			|| Row.ExpectedSha.Len() != 64 || Row.SourcePngSha.Len() != 64
			|| Row.Width <= 0 || Row.Height <= 0 || Row.Width > 2048 || Row.Height > 2048)
		{
			Finish(TEXT("Prepared input identity/extent/hash invalid or duplicated"));
			return false;
		}
		SeenIds.Add(Row.Id);
		SeenAssets.Add(Row.Asset);
		Rows.Add(MoveTemp(Row));
	}
	for (const FRow& Row : Rows)
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Row.Asset);
		if (!Texture) { Finish(TEXT("Texture asset missing: ") + Row.Asset); return false; }
		PinnedTextures.Emplace(Texture);
	}
	for (FRow& Row : Rows)
	{
		Row.UAssetSha = HashFile(AssetFilePath(Row.Asset));
		if (Row.UAssetSha.Len() != 64)
		{
			Finish(TEXT("Allowlisted texture UAsset missing or unreadable: ") + Row.Asset);
			return false;
		}
	}
#if WITH_EDITOR
	for (const TStrongObjectPtr<UTexture2D>& Pinned : PinnedTextures)
	{
		UTexture2D* Texture = Pinned.Get();
		if (Texture->IsCompiling())
		{
			UTexture* Requested[] = { Texture };
			FTextureCompilingManager::Get().FinishCompilation(Requested);
			if (Texture->IsCompiling())
			{
				Finish(TEXT("Allowlisted texture remains compiling: ") + Texture->GetPathName());
				return false;
			}
		}
	}
#endif
	bPrepared = true;
	return true;
}

bool URokCityGammaInputAuditSubsystem::StartNext()
{
	if (Index >= Rows.Num()) { Finish(); return true; }
	const FRow& Row = Rows[Index];
	UTexture2D* Texture = PinnedTextures[Index].Get();
	const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
	const FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
	if (!TextureRHI.IsValid()) return false; // newly completed texture may not be resident yet
	const FRHITextureDesc& Desc = TextureRHI->GetDesc();
	if (Texture->SRGB || Texture->GetPixelFormat() != PF_B8G8R8A8
		|| Desc.Format != PF_B8G8R8A8 || Desc.Extent.X != Row.Width || Desc.Extent.Y != Row.Height
		|| Desc.NumMips != 1 || Desc.NumSamples != 1 || EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB)
		|| GPixelFormats[Desc.Format].BlockBytes != 4)
	{
		Finish(TEXT("Allowlisted texture descriptor differs: ") + Row.Asset);
		return false;
	}
	Active = MakeShared<FRokCityGammaInputReadback, ESPMode::ThreadSafe>();
	Active->Width = Row.Width;
	Active->Height = Row.Height;
	PendingSeconds = FPlatformTime::Seconds();
	const TSharedPtr<FRokCityGammaInputReadback, ESPMode::ThreadSafe> Capture = Active;
	ENQUEUE_RENDER_COMMAND(RokCityGammaInputCopy)(
		[Capture, TextureRHI](FRHICommandListImmediate& RHICmdList)
		{
			FScopeLock Lock(&Capture->Mutex);
			FRHITexture* Source = TextureRHI.GetReference();
			if (!Source)
			{
				Capture->Error = TEXT("Pinned source RHI texture disappeared before copy");
				Capture->State = FRokCityGammaInputReadback::EState::Failed;
				return;
			}
			const FRHITextureDesc& Desc = Source->GetDesc();
			Capture->Format = static_cast<int32>(Desc.Format);
			Capture->Mips = Desc.NumMips;
			Capture->Samples = Desc.NumSamples;
			Capture->bSrgb = EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB);
			if (Desc.Format != PF_B8G8R8A8 || Desc.Extent.X != Capture->Width
				|| Desc.Extent.Y != Capture->Height || Desc.NumMips != 1 || Desc.NumSamples != 1
				|| Capture->bSrgb)
			{
				Capture->Error = TEXT("Render-thread texture descriptor differs before GPU copy");
				Capture->State = FRokCityGammaInputReadback::EState::Failed;
				return;
			}
			Capture->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokCityGammaInputBGRA8"));
			Capture->Readback->EnqueueCopy(RHICmdList, Source, FIntVector::ZeroValue, 0,
				FIntVector(Capture->Width, Capture->Height, 1));
			Capture->CopyFrame = GFrameCounterRenderThread;
			Capture->CopySeconds = FPlatformTime::Seconds();
			Capture->State = FRokCityGammaInputReadback::EState::CopySubmitted;
		});
	return true;
}

void URokCityGammaInputAuditSubsystem::Poll()
{
	if (!Active) return;
	const TSharedPtr<FRokCityGammaInputReadback, ESPMode::ThreadSafe> Capture = Active;
	{
		FScopeLock Lock(&Capture->Mutex);
		if (Capture->State != FRokCityGammaInputReadback::EState::CopySubmitted || Capture->bPollQueued) return;
		Capture->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokCityGammaInputPoll)(
		[Capture](FRHICommandListImmediate&)
		{
			FScopeLock Lock(&Capture->Mutex);
			Capture->bPollQueued = false;
			if (Capture->State != FRokCityGammaInputReadback::EState::CopySubmitted) return;
			if (!Capture->Readback || !Capture->Readback->IsReady()) return;
			int32 PitchPixels = 0, BufferHeight = 0;
			const void* Mapped = Capture->Readback->Lock(PitchPixels, &BufferHeight);
			if (!Mapped || PitchPixels < Capture->Width || BufferHeight < Capture->Height)
			{
				if (Mapped) Capture->Readback->Unlock();
				Capture->Error = TEXT("GPU staging mapping or row pitch invalid");
				Capture->Readback.Reset();
				Capture->State = FRokCityGammaInputReadback::EState::Failed;
				return;
			}
			Capture->RowPitchPixels = PitchPixels;
			Capture->BufferHeight = BufferHeight;
			const int32 TightStride = Capture->Width * 4;
			Capture->TightBGRA.SetNumUninitialized(TightStride * Capture->Height);
			const uint8* Source = static_cast<const uint8*>(Mapped);
			for (int32 Y = 0; Y < Capture->Height; ++Y)
				FMemory::Memcpy(Capture->TightBGRA.GetData() + Y * TightStride,
					Source + Y * PitchPixels * 4, TightStride);
			Capture->Readback->Unlock();
			Capture->Readback.Reset();
			Capture->ReadyFrame = GFrameCounterRenderThread;
			Capture->ReadySeconds = FPlatformTime::Seconds();
			Capture->State = FRokCityGammaInputReadback::EState::Ready;
		});
}

void URokCityGammaInputAuditSubsystem::RecordReady()
{
	if (!Active) return;
	TArray<uint8> Bytes;
	int32 Pitch = 0, Height = 0, Format = -1, Mips = 0, Samples = 0;
	bool bSrgb = false;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	double CopySeconds = 0, ReadySeconds = 0;
	{
		FScopeLock Lock(&Active->Mutex);
		if (Active->State != FRokCityGammaInputReadback::EState::Ready) return;
		Bytes = MoveTemp(Active->TightBGRA);
		Pitch = Active->RowPitchPixels;
		Height = Active->BufferHeight;
		Format = Active->Format;
		Mips = Active->Mips;
		Samples = Active->Samples;
		bSrgb = Active->bSrgb;
		CopyFrame = Active->CopyFrame;
		ReadyFrame = Active->ReadyFrame;
		CopySeconds = Active->CopySeconds;
		ReadySeconds = Active->ReadySeconds;
	}
	const FRow& Row = Rows[Index];
	if (Bytes.Num() != Row.Width * Row.Height * 4 || Format != PF_B8G8R8A8
		|| Mips != 1 || Samples != 1 || bSrgb)
	{
		Finish(TEXT("Canonical BGRA8 byte count or final RHI descriptor differs: ") + Row.Id);
		return;
	}
	const FString Name = Row.Id + TEXT(".bgra8");
	const FString Path = Directory / Name;
	if (IFileManager::Get().FileExists(*Path) || !FFileHelper::SaveArrayToFile(Bytes, *Path))
	{
		Finish(TEXT("Cannot create unique canonical GPU byte file: ") + Name);
		return;
	}
	const FString ObservedSha = RokTrainArmyMesh::Sha256(Bytes);
	const bool bMatch = ObservedSha.Equals(Row.ExpectedSha, ESearchCase::IgnoreCase);
	TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("resource_id"), Row.Id);
	Record->SetStringField(TEXT("asset"), Row.Asset);
	Record->SetStringField(TEXT("asset_uasset_sha256"), Row.UAssetSha);
	Record->SetStringField(TEXT("role"), Row.Role);
	Record->SetNumberField(TEXT("width"), Row.Width);
	Record->SetNumberField(TEXT("height"), Row.Height);
	Record->SetStringField(TEXT("rhi_format"), TEXT("PF_B8G8R8A8"));
	Record->SetNumberField(TEXT("rhi_format_enum"), Format);
	Record->SetNumberField(TEXT("rhi_mips"), Mips);
	Record->SetNumberField(TEXT("rhi_samples"), Samples);
	Record->SetBoolField(TEXT("rhi_srgb"), bSrgb);
	Record->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
	Record->SetNumberField(TEXT("staging_buffer_height"), Height);
	Record->SetNumberField(TEXT("canonical_file_row_stride_bytes"), Row.Width * 4);
	Record->SetStringField(TEXT("canonical_bgra8_file"), Name);
	Record->SetNumberField(TEXT("canonical_bytes"), Bytes.Num());
	Record->SetStringField(TEXT("expected_bgra8_sha256"), Row.ExpectedSha);
	Record->SetStringField(TEXT("observed_bgra8_sha256"), ObservedSha);
	Record->SetStringField(TEXT("prepared_png_sha256"), Row.SourcePngSha);
	Record->SetBoolField(TEXT("match"), bMatch);
	Record->SetNumberField(TEXT("gpu_copy_render_frame"), static_cast<double>(CopyFrame));
	Record->SetNumberField(TEXT("gpu_ready_render_frame"), static_cast<double>(ReadyFrame));
	Record->SetNumberField(TEXT("gpu_copy_seconds"), CopySeconds);
	Record->SetNumberField(TEXT("gpu_ready_seconds"), ReadySeconds);
	Records.Add(Record);
	if (!bMatch) Failures.Add(TEXT("GPU BGRA8 SHA mismatch: ") + Row.Id);
	Active.Reset();
	++Index;
}

void URokCityGammaInputAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (Prepare()) return;
		if (!bFinished && Now - StartSeconds > StartupTimeout)
			Finish(TEXT("Viewport/local Game world or allowlisted texture readiness timed out"));
		return;
	}
	if (!Active)
	{
		if (!StartNext() && Now - StartSeconds > StartupTimeout)
			Finish(TEXT("Allowlisted texture RHI resource did not become ready"));
		return;
	}
	FRokCityGammaInputReadback::EState State;
	FString Error;
	{
		FScopeLock Lock(&Active->Mutex);
		State = Active->State;
		Error = Active->Error;
	}
	if (State == FRokCityGammaInputReadback::EState::Failed)
	{
		Finish(TEXT("GPU texture readback failed: ") + Error);
		return;
	}
	if (State == FRokCityGammaInputReadback::EState::Ready) RecordReady();
	else if (Now - PendingSeconds > ReadbackTimeout) Finish(TEXT("GPU texture readback timed out"));
	else Poll();
}

void URokCityGammaInputAuditSubsystem::CancelReadback()
{
	if (!Active) return;
	const TSharedPtr<FRokCityGammaInputReadback, ESPMode::ThreadSafe> Capture = Active;
	ENQUEUE_RENDER_COMMAND(RokCityGammaInputRelease)(
		[Capture](FRHICommandListImmediate&)
		{
			FScopeLock Lock(&Capture->Mutex);
			Capture->Readback.Reset();
			Capture->State = FRokCityGammaInputReadback::EState::Cancelled;
		});
	FlushRenderingCommands();
	Active.Reset();
}

void URokCityGammaInputAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	CancelReadback();
	bFinished = true;
	const bool bComplete = bPrepared && Records.Num() == ExpectedTextures;
	const bool bPassed = bComplete && Failures.IsEmpty();
	TSharedPtr<FJsonObject> Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_inputs.runtime_audit.v1"));
	Manifest->SetStringField(TEXT("scope"), TEXT("Exact GameApp local Game viewport, read-only 22 allowlisted UTexture2D RHI resources; no scene-color capture, raster registration, map/asset/economy mutation, or process exit"));
	Manifest->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Manifest->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Manifest->SetStringField(TEXT("input_manifest_relative"), ManifestRelative);
	Manifest->SetStringField(TEXT("input_manifest_sha256"), InputSha);
	Manifest->SetStringField(TEXT("audit_cpp_sha256"), HashFile(FPaths::ProjectDir()
		/ TEXT("Source/Rok/RokCityGammaInputAuditSubsystem.cpp")));
	Manifest->SetStringField(TEXT("audit_h_sha256"), HashFile(FPaths::ProjectDir()
		/ TEXT("Source/Rok/RokCityGammaInputAuditSubsystem.h")));
	Manifest->SetStringField(TEXT("verifier_sha256"), HashFile(FPaths::ProjectDir()
		/ TEXT("Scripts/VerifyRokCityGammaInputsRuntime.py")));
	Manifest->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Manifest->SetBoolField(TEXT("raster_registered"), false);
	Manifest->SetBoolField(TEXT("scene_components_modified"), false);
	Manifest->SetBoolField(TEXT("complete"), bComplete);
	Manifest->SetBoolField(TEXT("passed"), bPassed);
	Manifest->SetNumberField(TEXT("expected_textures"), ExpectedTextures);
	Manifest->SetNumberField(TEXT("observed_textures"), Records.Num());
	Manifest->SetNumberField(TEXT("start_game_frame"), static_cast<double>(StartFrame));
	Manifest->SetNumberField(TEXT("finish_game_frame"), static_cast<double>(GFrameCounter));
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const TSharedPtr<FJsonObject>& Record : Records) Values.Add(MakeShared<FJsonValueObject>(Record));
	Manifest->SetArrayField(TEXT("textures"), Values);
	Manifest->SetArrayField(TEXT("failures"), JsonStrings(Failures));
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Manifest.ToSharedRef(), Writer);
	if (!FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("City Gamma input audit could not save manifest: %s"), *Directory);
	}
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("City Gamma input audit PASS: 22 GPU BGRA8 textures, %s"), *Directory);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("City Gamma input audit FAILED: %s; %s"), *Directory,
			Failures.Num() ? *Failures.Last() : TEXT("incomplete"));
	}
}
