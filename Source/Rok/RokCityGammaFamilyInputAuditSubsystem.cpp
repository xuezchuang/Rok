#include "RokCityGammaFamilyInputAuditSubsystem.h"

#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
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
#include "RHITextureInitializer.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr TCHAR InputRelative[] = TEXT("Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/manifest.json");
constexpr TCHAR InputExpectedSha[] = TEXT("b02d0568745fd0c26b073202496028bf2d81c2e0ae9dffef2e18083782b101e2");
constexpr TCHAR NativeExpectedSha[] = TEXT("671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71");
constexpr TCHAR ContractExpectedSha[] = TEXT("9555b7e77ff67dad29789923109cde5574358dbc4cc7072e174b0f3987f27a49");
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr int32 ExpectedTextures = 10;
constexpr int32 ExpectedMips = 77;
constexpr int32 ExpectedSlotReferences = 110;
constexpr double StartupTimeout = 45.0;
constexpr double GpuTimeout = 20.0;

bool ExactWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == MapName
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

FString HashBytes(const TArray<uint8>& Bytes) { return RokTrainArmyMesh::Sha256(Bytes); }
FString HashFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? HashBytes(Bytes) : FString();
}
FString SafeRepoFile(const FString& Relative)
{
	if (!(Relative.StartsWith(TEXT("Saved/UnityCityGammaFamilyStateProbeV2/"))
		|| Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityGammaFamilyRhiInputs/v1/"))
		|| Relative.StartsWith(TEXT("ref/client-unity/")))
		|| Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) || Relative.Contains(TEXT("\\")))
		return FString();
	return FPaths::ProjectDir() / Relative;
}
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Object)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *Path)
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) && Object.IsValid();
}
TArray<TSharedPtr<FJsonValue>> StringValues(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
	return Out;
}
TArray<TSharedPtr<FJsonValue>> ObjectValues(const TArray<TSharedPtr<FJsonObject>>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const TSharedPtr<FJsonObject>& Value : Values) Out.Add(MakeShared<FJsonValueObject>(Value));
	return Out;
}
}

struct FRokGammaFamilyGpuJob
{
	enum class EState : uint8 { UploadPending, Uploaded, CopyQueued, CopyPending, Ready, Failed, Cancelled };
	FCriticalSection Mutex;
	FTextureRHIRef Texture; // strong render-resource reference until ReleaseActive flushes the RT
	FTextureRHIRef SingleMipCopy; // destination kept alive through GPU fence/readback completion
	TUniquePtr<FRHIGPUTextureReadback> Readback; // render thread only
	TArray<TArray<uint8>> UploadBytes;
	TArray<FIntPoint> Sizes;
	TArray<uint8> GpuBytes;
	FString Error;
	EState State = EState::UploadPending;
	int32 Mip = -1, PitchPixels = 0, BufferHeight = 0, Format = -1, NumMips = 0, NumSamples = 0;
	bool bSrgb = false, bPollQueued = false;
	uint64 CopyFrame = 0, ReadyFrame = 0;
};

bool URokCityGammaFamilyInputAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return ExactWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaFamilyInputAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCityGammaFamilyInputAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCityGammaFamilyInputAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityGammaFamilyInputAuditSubsystem, STATGROUP_Tickables);
}
void URokCityGammaFamilyInputAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	RunDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/CityGammaFamilyInputAudit")
		/ FString::Printf(TEXT("%s_%u_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*RunDirectory) || !IFileManager::Get().MakeDirectory(*RunDirectory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Gamma family input audit cannot create unique run: %s"), *RunDirectory);
		bFinished = true;
	}
}
void URokCityGammaFamilyInputAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("Game world ended before all 77 GPU mip readbacks completed"));
	ReleaseActive();
	Super::Deinitialize();
}

bool URokCityGammaFamilyInputAuditSubsystem::Prepare()
{
	UWorld* World = GetWorld();
	if (!ExactWorld(World) || !GEngine || !GEngine->GameViewport
		|| GEngine->GameViewport->GetWorld() != World || !GEngine->GameViewport->Viewport
		|| !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->IsLocalController()) return false;
	URokServerSubsystem* Server = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun()) { Finish(TEXT("Local-only server boundary unavailable")); return false; }
	InitialHttpDispatches = Server->GetHttpDispatchAttemptCount();
	if (InitialHttpDispatches != 0) { Finish(TEXT("HTTP dispatch already attempted before input audit")); return false; }
	InputSha = HashFile(FPaths::ProjectDir() / InputRelative);
	if (InputSha != InputExpectedSha) { Finish(TEXT("Pinned prepared family input SHA differs")); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!ReadJson(FPaths::ProjectDir() / InputRelative, Root)
		|| Root->GetStringField(TEXT("schema")) != TEXT("rok.city_gamma_family_rhi_inputs.v1"))
	{
		Finish(TEXT("Prepared family input schema invalid")); return false;
	}
	ContractSha = HashFile(SafeRepoFile(Root->GetStringField(TEXT("state_contract_path"))));
	NativeSha = HashFile(SafeRepoFile(Root->GetStringField(TEXT("native_state_path"))));
	NativeVerifySha = HashFile(SafeRepoFile(Root->GetStringField(TEXT("native_verify_path"))));
	if (ContractSha != ContractExpectedSha || NativeSha != NativeExpectedSha
		|| ContractSha != Root->GetStringField(TEXT("state_contract_sha256"))
		|| NativeSha != Root->GetStringField(TEXT("native_state_sha256"))
		|| NativeVerifySha != Root->GetStringField(TEXT("native_verify_sha256")))
	{
		Finish(TEXT("Pinned Unity family source/verification hash chain differs")); return false;
	}
	const TSharedPtr<FJsonObject> Target = Root->GetObjectField(TEXT("rhi_target"));
	const TSharedPtr<FJsonObject> Counts = Root->GetObjectField(TEXT("counts"));
	if (Target->GetStringField(TEXT("format")) != TEXT("PF_B8G8R8A8")
		|| Target->GetBoolField(TEXT("srgb")) || Target->GetBoolField(TEXT("generate_mips"))
		|| Target->GetStringField(TEXT("upload_row_order")) != TEXT("top_down")
		|| Counts->GetIntegerField(TEXT("textures")) != ExpectedTextures
		|| Counts->GetIntegerField(TEXT("unique_mips")) != ExpectedMips
		|| Counts->GetIntegerField(TEXT("source_row_mip_references")) != ExpectedSlotReferences)
	{
		Finish(TEXT("Prepared family RHI format/count contract differs")); return false;
	}
	TSet<FString> SeenIds, SeenGuids;
	int32 TotalMips = 0;
	const auto& TextureValues = Root->GetArrayField(TEXT("textures"));
	if (TextureValues.Num() != ExpectedTextures) { Finish(TEXT("Prepared family texture array count differs")); return false; }
	for (const TSharedPtr<FJsonValue>& Value : TextureValues)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object.IsValid()) { Finish(TEXT("Prepared texture row is not an object")); return false; }
		FTexture Item;
		Item.Id = Object->GetStringField(TEXT("texture_id"));
		Item.Guid = Object->GetStringField(TEXT("source_guid"));
		Item.SourcePath = Object->GetStringField(TEXT("source_repo_path"));
		Item.SourceSha = Object->GetStringField(TEXT("source_png_sha256"));
		Item.Filter = Object->GetStringField(TEXT("source_filter"));
		Item.Wrap = Object->GetStringField(TEXT("source_wrap"));
		Item.Width = Object->GetIntegerField(TEXT("width"));
		Item.Height = Object->GetIntegerField(TEXT("height"));
		if (SeenIds.Contains(Item.Id) || SeenGuids.Contains(Item.Guid)
			|| Item.Id != FString::Printf(TEXT("texture_%02d"), Textures.Num())
			|| Item.Guid.Len() != 32 || Item.Width <= 0 || Item.Height <= 0
			|| Item.Width > 2048 || Item.Height > 2048
			|| !(Item.Filter == TEXT("Bilinear") || Item.Filter == TEXT("Point") || Item.Filter == TEXT("Trilinear"))
			|| !(Item.Wrap == TEXT("Clamp") || Item.Wrap == TEXT("Repeat"))
			|| Object->GetBoolField(TEXT("source_srgb_importer")) != true)
		{
			Finish(TEXT("Prepared texture identity/importer metadata invalid")); return false;
		}
		SeenIds.Add(Item.Id); SeenGuids.Add(Item.Guid);
		if (HashFile(SafeRepoFile(Item.SourcePath)) != Item.SourceSha)
		{
			Finish(TEXT("Pinned source PNG hash differs: ") + Item.Id); return false;
		}
		const auto& Levels = Object->GetArrayField(TEXT("levels"));
		if (Levels.Num() != Object->GetIntegerField(TEXT("mip_count")) || Levels.Num() < 1 || Levels.Num() > 12)
		{
			Finish(TEXT("Prepared texture mip count invalid")); return false;
		}
		for (const TSharedPtr<FJsonValue>& LevelValue : Levels)
		{
			const TSharedPtr<FJsonObject> LevelObject = LevelValue->AsObject();
			FMip Mip;
			Mip.Level = LevelObject->GetIntegerField(TEXT("level"));
			Mip.Width = LevelObject->GetIntegerField(TEXT("width"));
			Mip.Height = LevelObject->GetIntegerField(TEXT("height"));
			Mip.File = LevelObject->GetStringField(TEXT("prepared_bgra_file"));
			Mip.Sha = LevelObject->GetStringField(TEXT("prepared_bgra_sha256"));
			const FString Path = SafeRepoFile(Mip.File);
			if (Mip.Level != Item.Mips.Num() || Mip.Width != FMath::Max(1, Item.Width >> Mip.Level)
				|| Mip.Height != FMath::Max(1, Item.Height >> Mip.Level)
				|| LevelObject->GetIntegerField(TEXT("byte_count")) != Mip.Width * Mip.Height * 4
				|| LevelObject->GetStringField(TEXT("prepared_row_order")) != TEXT("top_down")
				|| Path.IsEmpty() || !FFileHelper::LoadFileToArray(Mip.Bytes, *Path)
				|| Mip.Bytes.Num() != Mip.Width * Mip.Height * 4 || HashBytes(Mip.Bytes) != Mip.Sha)
			{
				Finish(TEXT("Prepared BGRA mip data/hash/extent differs: ") + Item.Id); return false;
			}
			Item.Mips.Add(MoveTemp(Mip)); ++TotalMips;
		}
		Textures.Add(MoveTemp(Item));
	}
	if (TotalMips != ExpectedMips || Root->GetArrayField(TEXT("families")).Num() != 5)
	{
		Finish(TEXT("Prepared family mip/family count differs")); return false;
	}
	for (const TSharedPtr<FJsonValue>& FamilyValue : Root->GetArrayField(TEXT("families")))
	{
		const TSharedPtr<FJsonObject> Family = FamilyValue->AsObject();
		for (const TSharedPtr<FJsonValue>& SlotValue : Family->GetArrayField(TEXT("textures")))
		{
			const TSharedPtr<FJsonObject> Slot = SlotValue->AsObject();
			const int32 Index = Slot->GetIntegerField(TEXT("native_texture_index"));
			const FString SlotId = Slot->GetStringField(TEXT("texture_id"));
			const FTexture* Item = Textures.FindByPredicate([&SlotId](const FTexture& Candidate) { return Candidate.Id == SlotId; });
			if (Index < 0 || Index >= Family->GetArrayField(TEXT("textures")).Num() || !Item
				|| Slot->GetStringField(TEXT("source_guid")) != Item->Guid
				|| Slot->GetIntegerField(TEXT("mip_count")) != Item->Mips.Num())
			{
				Finish(TEXT("Family slot texture identity invalid")); return false;
			}
			FamilySlotCount += Item->Mips.Num();
		}
	}
	if (FamilySlotCount != ExpectedSlotReferences)
	{
		Finish(TEXT("Family slot mip references differ from 110")); return false;
	}
	bPrepared = true;
	return true;
}

bool URokCityGammaFamilyInputAuditSubsystem::StartTexture()
{
	if (TextureIndex >= Textures.Num()) { Finish(); return true; }
	const FTexture& Item = Textures[TextureIndex];
	Active = MakeShared<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe>();
	for (const FMip& Mip : Item.Mips)
	{
		Active->Sizes.Add(FIntPoint(Mip.Width, Mip.Height));
		Active->UploadBytes.Add(Mip.Bytes);
	}
	const TSharedPtr<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe> Job = Active;
	const FString DebugName = FString::Printf(TEXT("RokFamily_%s"), *Item.Id);
	PendingSeconds = FPlatformTime::Seconds();
	ENQUEUE_RENDER_COMMAND(RokGammaFamilyUpload)([Job, DebugName](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Guard(&Job->Mutex);
		const FIntPoint Base = Job->Sizes[0];
		const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(*DebugName,
			Base.X, Base.Y, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetNumMips(static_cast<uint8>(Job->Sizes.Num()))
			.SetInitActionInitializer();
		FRHITextureInitializer Initializer = RHICmdList.CreateTextureInitializer(Desc);
		for (int32 Mip = 0; Mip < Job->Sizes.Num(); ++Mip)
		{
			const FRHITextureSubresourceInitializer Subresource = Initializer.GetTexture2DSubresource(Mip);
			const int32 RowBytes = Job->Sizes[Mip].X * 4;
			if (!Subresource.Data || Subresource.Stride < static_cast<uint64>(RowBytes)
				|| Subresource.Size < Subresource.Stride * Job->Sizes[Mip].Y)
			{
				Job->Error = TEXT("RHI mip initializer stride/capacity invalid");
				Job->State = FRokGammaFamilyGpuJob::EState::Failed;
				return;
			}
			for (int32 Y = 0; Y < Job->Sizes[Mip].Y; ++Y)
				FMemory::Memcpy(static_cast<uint8*>(Subresource.Data) + Y * Subresource.Stride,
					Job->UploadBytes[Mip].GetData() + Y * RowBytes, RowBytes);
		}
		Job->Texture = Initializer.Finalize();
		if (!Job->Texture.IsValid())
		{
			Job->Error = TEXT("RHI multi-mip texture creation returned null");
			Job->State = FRokGammaFamilyGpuJob::EState::Failed;
			return;
		}
		const FRHITextureDesc& Actual = Job->Texture->GetDesc();
		Job->Format = Actual.Format; Job->NumMips = Actual.NumMips;
		Job->NumSamples = Actual.NumSamples;
		Job->bSrgb = EnumHasAnyFlags(Actual.Flags, TexCreate_SRGB);
		if (Actual.Format != PF_B8G8R8A8 || Actual.Extent != Base
			|| Actual.NumMips != Job->Sizes.Num() || Actual.NumSamples != 1 || Job->bSrgb)
		{
			Job->Error = TEXT("Created RHI texture descriptor differs");
			Job->State = FRokGammaFamilyGpuJob::EState::Failed;
			return;
		}
		Job->State = FRokGammaFamilyGpuJob::EState::Uploaded;
	});
	return true;
}

void URokCityGammaFamilyInputAuditSubsystem::StartMip()
{
	const TSharedPtr<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe> Job = Active;
	const int32 Level = MipIndex;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Guard(&Job->Mutex);
		if (Job->State != FRokGammaFamilyGpuJob::EState::Uploaded) return;
		Job->State = FRokGammaFamilyGpuJob::EState::CopyQueued;
	}
	ENQUEUE_RENDER_COMMAND(RokGammaFamilyReadMip)([Job, Level](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Guard(&Job->Mutex);
		if (Job->State != FRokGammaFamilyGpuJob::EState::CopyQueued || !Job->Texture.IsValid()) return;
		Job->SingleMipCopy.SafeRelease(); // previous mip has already completed and unlocked
		Job->Mip = Level;
		const FIntPoint Extent = Job->Sizes[Level];
		const FRHITextureCreateDesc CopyDesc = FRHITextureCreateDesc::Create2D(
			TEXT("RokGammaFamilySingleMipCopy"), Extent.X, Extent.Y, PF_B8G8R8A8)
			.SetFlags(ETextureCreateFlags::ShaderResource)
			.SetNumMips(1)
			.SetInitialState(ERHIAccess::CopyDest);
		Job->SingleMipCopy = RHICmdList.CreateTexture(CopyDesc);
		if (!Job->SingleMipCopy.IsValid())
		{
			Job->Error = TEXT("Single-mip copy target creation failed");
			Job->State = FRokGammaFamilyGpuJob::EState::Failed;
			return;
		}
		RHICmdList.Transition({
			FRHITransitionInfo(Job->Texture.GetReference(), ERHIAccess::Unknown, ERHIAccess::CopySrc),
			FRHITransitionInfo(Job->SingleMipCopy.GetReference(), ERHIAccess::Unknown, ERHIAccess::CopyDest)
		});
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.SourceMipIndex = Level;
		CopyInfo.DestMipIndex = 0;
		CopyInfo.SourceSliceIndex = 0;
		CopyInfo.DestSliceIndex = 0;
		CopyInfo.NumMips = 1;
		CopyInfo.NumSlices = 1;
		CopyInfo.Size = FIntVector(Extent.X, Extent.Y, 1);
		RHICmdList.CopyTexture(Job->Texture.GetReference(), Job->SingleMipCopy.GetReference(), CopyInfo);
		RHICmdList.Transition({
			FRHITransitionInfo(Job->Texture.GetReference(), ERHIAccess::CopySrc, ERHIAccess::SRVMask),
			FRHITransitionInfo(Job->SingleMipCopy.GetReference(), ERHIAccess::CopyDest, ERHIAccess::CopySrc)
		});
		Job->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokGammaFamilyMipBGRA"));
		// UE5.8 EnqueueCopy's fourth argument is an array slice, not a mip index.
		Job->Readback->EnqueueCopy(RHICmdList, Job->SingleMipCopy.GetReference(),
			FIntVector::ZeroValue, 0, FIntVector(Extent.X, Extent.Y, 1));
		Job->CopyFrame = GFrameCounterRenderThread;
		Job->State = FRokGammaFamilyGpuJob::EState::CopyPending;
	});
}
void URokCityGammaFamilyInputAuditSubsystem::PollMip()
{
	const TSharedPtr<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe> Job = Active;
	{
		FScopeLock Guard(&Job->Mutex);
		if (Job->State != FRokGammaFamilyGpuJob::EState::CopyPending || Job->bPollQueued) return;
		Job->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokGammaFamilyPollMip)([Job](FRHICommandListImmediate&)
	{
		FScopeLock Guard(&Job->Mutex);
		Job->bPollQueued = false;
		if (Job->State != FRokGammaFamilyGpuJob::EState::CopyPending
			|| !Job->Readback || !Job->Readback->IsReady()) return;
		int32 Pitch = 0, Height = 0;
		const void* Mapped = Job->Readback->Lock(Pitch, &Height);
		const FIntPoint Extent = Job->Sizes[Job->Mip];
		if (!Mapped || Pitch < Extent.X || Height < Extent.Y)
		{
			if (Mapped) Job->Readback->Unlock();
			Job->Readback.Reset();
			Job->Error = TEXT("GPU mip staging mapping or row pitch invalid");
			Job->State = FRokGammaFamilyGpuJob::EState::Failed;
			return;
		}
		Job->PitchPixels = Pitch; Job->BufferHeight = Height;
		Job->GpuBytes.SetNumUninitialized(Extent.X * Extent.Y * 4);
		for (int32 Y = 0; Y < Extent.Y; ++Y)
			FMemory::Memcpy(Job->GpuBytes.GetData() + Y * Extent.X * 4,
				static_cast<const uint8*>(Mapped) + Y * Pitch * 4, Extent.X * 4);
		Job->Readback->Unlock(); Job->Readback.Reset();
		Job->ReadyFrame = GFrameCounterRenderThread;
		Job->State = FRokGammaFamilyGpuJob::EState::Ready;
	});
}

void URokCityGammaFamilyInputAuditSubsystem::RecordMip()
{
	const FTexture& Item = Textures[TextureIndex];
	const FMip& Mip = Item.Mips[MipIndex];
	TArray<uint8> Bytes;
	int32 Pitch = 0, StagingHeight = 0, Format = -1, NumMips = 0, NumSamples = 0;
	bool bSrgb = false;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	{
		FScopeLock Guard(&Active->Mutex);
		if (Active->State != FRokGammaFamilyGpuJob::EState::Ready) return;
		Bytes = MoveTemp(Active->GpuBytes);
		Pitch = Active->PitchPixels; StagingHeight = Active->BufferHeight;
		Format = Active->Format; NumMips = Active->NumMips; NumSamples = Active->NumSamples;
		bSrgb = Active->bSrgb; CopyFrame = Active->CopyFrame; ReadyFrame = Active->ReadyFrame;
		Active->State = FRokGammaFamilyGpuJob::EState::Uploaded;
	}
	const FString Sha = HashBytes(Bytes);
	const bool bExact = Bytes.Num() == Mip.Bytes.Num() && Bytes == Mip.Bytes && Sha == Mip.Sha;
	const FString File = FString::Printf(TEXT("%s_mip_%02d.readback.bgra8.bin"), *Item.Id, Mip.Level);
	if (IFileManager::Get().FileExists(*(RunDirectory / File))
		|| !FFileHelper::SaveArrayToFile(Bytes, *(RunDirectory / File)))
	{
		Finish(TEXT("Cannot write unique GPU mip readback file: ") + File); return;
	}
	TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("texture_id"), Item.Id);
	Record->SetStringField(TEXT("source_guid"), Item.Guid);
	Record->SetNumberField(TEXT("level"), Mip.Level);
	Record->SetNumberField(TEXT("width"), Mip.Width);
	Record->SetNumberField(TEXT("height"), Mip.Height);
	Record->SetNumberField(TEXT("byte_count"), Bytes.Num());
	Record->SetStringField(TEXT("expected_bgra_sha256"), Mip.Sha);
	Record->SetStringField(TEXT("gpu_readback_bgra_sha256"), Sha);
	Record->SetStringField(TEXT("gpu_readback_file"), File);
	Record->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
	Record->SetNumberField(TEXT("staging_buffer_height"), StagingHeight);
	Record->SetNumberField(TEXT("rhi_format_enum"), Format);
	Record->SetNumberField(TEXT("rhi_mip_count"), NumMips);
	Record->SetStringField(TEXT("readback_method"), TEXT("explicit_source_mip_copy_to_single_mip_texture"));
	Record->SetNumberField(TEXT("copy_source_mip_index"), Mip.Level);
	Record->SetNumberField(TEXT("copy_dest_mip_index"), 0);
	Record->SetNumberField(TEXT("readback_source_slice_index"), 0);
	Record->SetNumberField(TEXT("rhi_samples"), NumSamples);
	Record->SetBoolField(TEXT("rhi_srgb"), bSrgb);
	Record->SetNumberField(TEXT("gpu_copy_frame"), static_cast<double>(CopyFrame));
	Record->SetNumberField(TEXT("gpu_ready_frame"), static_cast<double>(ReadyFrame));
	Record->SetBoolField(TEXT("byte_exact"), bExact);
	MipRecords.Add(Record);
	if (!bExact || Format != PF_B8G8R8A8 || NumMips != Item.Mips.Num() || NumSamples != 1 || bSrgb)
	{
		Finish(TEXT("GPU readback bytes or RHI descriptor mismatch: ") + File); return;
	}
	++MipIndex;
	if (MipIndex == Item.Mips.Num())
	{
		TSharedPtr<FJsonObject> TextureRecord = MakeShared<FJsonObject>();
		TextureRecord->SetStringField(TEXT("texture_id"), Item.Id);
		TextureRecord->SetStringField(TEXT("source_guid"), Item.Guid);
		TextureRecord->SetStringField(TEXT("source_repo_path"), Item.SourcePath);
		TextureRecord->SetStringField(TEXT("source_png_sha256"), Item.SourceSha);
		TextureRecord->SetStringField(TEXT("source_filter"), Item.Filter);
		TextureRecord->SetStringField(TEXT("source_wrap"), Item.Wrap);
		TextureRecord->SetNumberField(TEXT("width"), Item.Width);
		TextureRecord->SetNumberField(TEXT("height"), Item.Height);
		TextureRecord->SetNumberField(TEXT("mip_count"), Item.Mips.Num());
		TextureRecord->SetStringField(TEXT("rhi_format"), TEXT("PF_B8G8R8A8"));
		TextureRecord->SetBoolField(TEXT("rhi_srgb"), false);
		TextureRecord->SetBoolField(TEXT("mips_generated_by_ue"), false);
		TextureRecords.Add(TextureRecord);
		ReleaseActive();
		++TextureIndex; MipIndex = 0;
	}
}

void URokCityGammaFamilyInputAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (Prepare()) return;
		if (!bFinished && Now - StartSeconds > StartupTimeout)
			Finish(TEXT("Local Game viewport startup timed out"));
		return;
	}
	if (!Active) { StartTexture(); return; }
	FRokGammaFamilyGpuJob::EState State;
	FString Error;
	{
		FScopeLock Guard(&Active->Mutex);
		State = Active->State; Error = Active->Error;
	}
	if (State == FRokGammaFamilyGpuJob::EState::Failed) { Finish(TEXT("RHI mip job failed: ") + Error); return; }
	if (State == FRokGammaFamilyGpuJob::EState::Uploaded) { StartMip(); return; }
	if (State == FRokGammaFamilyGpuJob::EState::Ready) { RecordMip(); return; }
	if (Now - PendingSeconds > GpuTimeout) { Finish(TEXT("GPU family mip job timed out")); return; }
	if (State == FRokGammaFamilyGpuJob::EState::CopyPending) PollMip();
}

void URokCityGammaFamilyInputAuditSubsystem::ReleaseActive()
{
	if (!Active) return;
	const TSharedPtr<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe> Job = Active;
	ENQUEUE_RENDER_COMMAND(RokGammaFamilyRelease)([Job](FRHICommandListImmediate&)
	{
		FScopeLock Guard(&Job->Mutex);
		Job->Readback.Reset(); Job->Texture.SafeRelease();
		Job->SingleMipCopy.SafeRelease();
		Job->State = FRokGammaFamilyGpuJob::EState::Cancelled;
	});
	FlushRenderingCommands();
	Active.Reset();
}

void URokCityGammaFamilyInputAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	ReleaseActive();
	bFinished = true;
	URokServerSubsystem* Server = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	const uint64 FinalHttp = Server ? Server->GetHttpDispatchAttemptCount() : MAX_uint64;
	if (!Server || !Server->IsLocalOnlyRun() || FinalHttp != InitialHttpDispatches || FinalHttp != 0)
		Failures.Add(TEXT("Local-only HTTP dispatch boundary changed or unavailable"));
	const bool bComplete = bPrepared && TextureRecords.Num() == ExpectedTextures
		&& MipRecords.Num() == ExpectedMips && TextureIndex == ExpectedTextures;
	const bool bPassed = bComplete && Failures.IsEmpty();
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_family_input_runtime.v1"));
	Report->SetStringField(TEXT("scope"), TEXT("Local-only diagnostic explicit multi-mip RHI upload and per-mip GPU byte readback; no family shader/raster qualification or production caller"));
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("prepared_input_path"), InputRelative);
	Report->SetStringField(TEXT("prepared_input_sha256"), InputSha);
	Report->SetStringField(TEXT("native_state_sha256"), NativeSha);
	Report->SetStringField(TEXT("state_contract_sha256"), ContractSha);
	Report->SetStringField(TEXT("native_verify_sha256"), NativeVerifySha);
	Report->SetStringField(TEXT("audit_cpp_sha256"), HashFile(FPaths::ProjectDir() / TEXT("Source/Rok/RokCityGammaFamilyInputAuditSubsystem.cpp")));
	Report->SetStringField(TEXT("audit_h_sha256"), HashFile(FPaths::ProjectDir() / TEXT("Source/Rok/RokCityGammaFamilyInputAuditSubsystem.h")));
	Report->SetStringField(TEXT("verifier_sha256"), HashFile(FPaths::ProjectDir() / TEXT("Scripts/VerifyRokCityGammaFamilyInputRuntime.py")));
	Report->SetBoolField(TEXT("opt_in_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaFamilyInputAudit")));
	Report->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Report->SetBoolField(TEXT("rhi_srgb"), false);
	Report->SetBoolField(TEXT("mips_generated_by_ue"), false);
	Report->SetStringField(TEXT("readback_method"), TEXT("explicit_source_mip_copy_to_single_mip_texture"));
	Report->SetBoolField(TEXT("scene_mutated"), false);
	Report->SetBoolField(TEXT("family_raster_registered"), false);
	Report->SetNumberField(TEXT("http_dispatch_before"), static_cast<double>(InitialHttpDispatches));
	Report->SetNumberField(TEXT("http_dispatch_after"), static_cast<double>(FinalHttp));
	Report->SetNumberField(TEXT("family_slot_mip_references"), FamilySlotCount);
	Report->SetNumberField(TEXT("expected_textures"), ExpectedTextures);
	Report->SetNumberField(TEXT("expected_mips"), ExpectedMips);
	Report->SetNumberField(TEXT("observed_textures"), TextureRecords.Num());
	Report->SetNumberField(TEXT("observed_mips"), MipRecords.Num());
	Report->SetBoolField(TEXT("complete"), bComplete);
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetArrayField(TEXT("textures"), ObjectValues(TextureRecords));
	Report->SetArrayField(TEXT("mips"), ObjectValues(MipRecords));
	Report->SetArrayField(TEXT("failures"), StringValues(Failures));
	FString Text;
	FJsonSerializer::Serialize(Report.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	if (!FFileHelper::SaveStringToFile(Text, *(RunDirectory / TEXT("manifest.json"))))
		UE_LOG(LogTemp, Error, TEXT("Gamma family input audit failed to save manifest: %s"), *RunDirectory);
	UE_LOG(LogTemp, Log, TEXT("Gamma family input audit %s: %d textures / %d mips, %s"),
		bPassed ? TEXT("PASS") : TEXT("FAILED"), TextureRecords.Num(), MipRecords.Num(), *RunDirectory);
	FPlatformMisc::RequestExit(false);
}
