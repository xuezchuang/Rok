#include "RokGrass3SameUvAuditSubsystem.h"
#include "RokGrass3SameUvProbe.h"
#include "RokTrainArmyMeshData.h"

#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "PixelFormat.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TextureResource.h"

namespace RokGrass3SameUvAudit
{
constexpr TCHAR AtlasPath[] = TEXT("/Game/RokUnityWorldMapPrototype/Textures/T_WorldSource_map_tree_atlas_7e11c882.T_WorldSource_map_tree_atlas_7e11c882");
constexpr TCHAR SourceGuid[] = TEXT("7731f26b5274e0b48a05e39faf708cdc");
constexpr TCHAR SourceMaterialGuid[] = TEXT("97169e058eacbcf4abcbbb78c1ab6ee6");
constexpr TCHAR SourceShaderGuid[] = TEXT("53f4ced3da9491a46be03e0f75d14ab1");
constexpr TCHAR SourcePngSha[] = TEXT("7e11c882c211dc8587e051d41de7617c378156638e8c219570a018eb7ad8a678");
constexpr TCHAR UnityResultSha[] = TEXT("8481ed27251b2d27e78d6725ab6d0c619e92c5e9de5b8c9fa03380e0d3fdd21c");
constexpr TCHAR UvTableSha[] = TEXT("76c66701aec3a36aa1f904f007072acacbbf653289685132aeae3b791f09bf4c");
constexpr double TimeoutSeconds = 45.0;

FString ShaFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}

TArray<TSharedPtr<FJsonValue>> Rgba(const FFloat16Color& C)
{
	TArray<TSharedPtr<FJsonValue>> V;
	V.Add(MakeShared<FJsonValueNumber>(C.R.GetFloat()));
	V.Add(MakeShared<FJsonValueNumber>(C.G.GetFloat()));
	V.Add(MakeShared<FJsonValueNumber>(C.B.GetFloat()));
	V.Add(MakeShared<FJsonValueNumber>(C.A.GetFloat()));
	return V;
}
}

struct FRokGrass3SameUvReadback
{
	enum class EState { Pending, Submitted, Ready, Failed };
	FCriticalSection Mutex;
	EState State = EState::Pending;
	bool bPollQueued = false;
	FString Error;
	TRefCountPtr<IPooledRenderTarget> RawTarget, BlendTarget;
	TUniquePtr<FRHIGPUTextureReadback> RawReadback, BlendReadback;
	TArray<FFloat16Color> Raw, Blended;
	FIntPoint SourceExtent;
	int32 SourceFormat = -1, SourceMips = 0, SourceSamples = 0;
	bool bSourceSrgb = false;
	int32 RawPitch = 0, BlendPitch = 0, RawHeight = 0, BlendHeight = 0;
	uint64 CopyFrame = 0, ReadyFrame = 0;
};

bool URokGrass3SameUvAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("RokGrass3SameUvRhiProbeV1"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}
bool URokGrass3SameUvAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Type == EWorldType::Game; }
TStatId URokGrass3SameUvAuditSubsystem::GetStatId() const
{ RETURN_QUICK_DECLARE_CYCLE_STAT(URokGrass3SameUvAuditSubsystem, STATGROUP_Tickables); }
void URokGrass3SameUvAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("RokUnityMigration/Grass3SameUvRhiProbeV1")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
			FPlatformProcess::GetCurrentProcessId()));
}
void URokGrass3SameUvAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("World teardown before GPU readbacks completed"));
	if (Job.IsValid())
	{
		const auto Shared = Job;
		ENQUEUE_RENDER_COMMAND(RokGrass3SameUvRelease)([Shared](FRHICommandListImmediate&)
		{
			FScopeLock Lock(&Shared->Mutex);
			Shared->RawReadback.Reset(); Shared->BlendReadback.Reset();
			Shared->RawTarget.SafeRelease(); Shared->BlendTarget.SafeRelease();
		});
		FlushRenderingCommands();
		Job.Reset();
	}
	Atlas.Reset();
	Super::Deinitialize();
}
void URokGrass3SameUvAuditSubsystem::Start()
{
	if (GUsingNullRHI || GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5)
	{ Finish(TEXT("NullRHI or feature level below SM5; no GPU observation")); return; }
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, RokGrass3SameUvAudit::AtlasPath);
	if (!Texture || Texture->GetPathName() != RokGrass3SameUvAudit::AtlasPath)
	{ Finish(TEXT("Existing source atlas asset absent or identity differs")); return; }
	Atlas.Reset(Texture);
	if (Texture->GetSizeX() != 2048 || Texture->GetSizeY() != 2048
		|| Texture->Filter != TF_Bilinear || Texture->AddressX != TA_Clamp
		|| Texture->AddressY != TA_Clamp || !Texture->SRGB)
	{ Finish(TEXT("Existing UE atlas asset metadata differs; no automatic reimport or retuning")); return; }
	const FTextureResource* Resource = Texture->GetResource();
	FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
	if (!TextureRHI.IsValid()) return; // wait for resident existing resource
	Job = MakeShared<FRokGrass3SameUvReadback, ESPMode::ThreadSafe>();
	bStarted = true;
	const auto Shared = Job;
	ENQUEUE_RENDER_COMMAND(RokGrass3SameUvStart)([Shared, TextureRHI](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		const FRHITextureDesc& SourceDesc = TextureRHI->GetDesc();
		Shared->SourceExtent = SourceDesc.Extent;
		Shared->SourceFormat = static_cast<int32>(SourceDesc.Format);
		Shared->SourceMips = SourceDesc.NumMips;
		Shared->SourceSamples = SourceDesc.NumSamples;
		Shared->bSourceSrgb = EnumHasAnyFlags(SourceDesc.Flags, TexCreate_SRGB);
		if (!FRokGrass3SameUvProbe::Render(RHICmdList, TextureRHI,
			Shared->RawTarget, Shared->BlendTarget, Shared->Error))
		{ Shared->State = FRokGrass3SameUvReadback::EState::Failed; return; }
		const auto ValidTarget = [](const TRefCountPtr<IPooledRenderTarget>& Target)
		{
			const FRHITextureDesc& D = Target->GetRHI()->GetDesc();
			return D.Extent == FIntPoint(FRokGrass3SameUvProbe::Width, 1)
				&& D.Format == PF_FloatRGBA && D.NumMips == 1 && D.NumSamples == 1
				&& !EnumHasAnyFlags(D.Flags, TexCreate_SRGB);
		};
		if (!ValidTarget(Shared->RawTarget) || !ValidTarget(Shared->BlendTarget))
		{ Shared->Error = TEXT("RGBA16F output descriptors differ"); Shared->State = FRokGrass3SameUvReadback::EState::Failed; return; }
		Shared->RawReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokGrass3SameUvRaw"));
		Shared->BlendReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokGrass3SameUvBlended"));
		for (auto* Target : { Shared->RawTarget.GetReference(), Shared->BlendTarget.GetReference() })
			RHICmdList.Transition(FRHITransitionInfo(Target->GetRHI(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->RawReadback->EnqueueCopy(RHICmdList, Shared->RawTarget->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(FRokGrass3SameUvProbe::Width, 1, 1));
		Shared->BlendReadback->EnqueueCopy(RHICmdList, Shared->BlendTarget->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(FRokGrass3SameUvProbe::Width, 1, 1));
		for (auto* Target : { Shared->RawTarget.GetReference(), Shared->BlendTarget.GetReference() })
			RHICmdList.Transition(FRHITransitionInfo(Target->GetRHI(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
		Shared->State = FRokGrass3SameUvReadback::EState::Submitted;
	});
}
void URokGrass3SameUvAuditSubsystem::Poll()
{
	const auto Shared = Job;
	if (!Shared) return;
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGrass3SameUvReadback::EState::Submitted || Shared->bPollQueued) return;
		Shared->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokGrass3SameUvPoll)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->bPollQueued = false;
		if (Shared->State != FRokGrass3SameUvReadback::EState::Submitted
			|| !Shared->RawReadback->IsReady() || !Shared->BlendReadback->IsReady()) return;
		const auto Copy = [Shared](TUniquePtr<FRHIGPUTextureReadback>& Readback,
			TArray<FFloat16Color>& Out, int32& Pitch, int32& Height)
		{
			const void* Mapped = Readback->Lock(Pitch, &Height);
			if (!Mapped || Pitch < FRokGrass3SameUvProbe::Width || Height < 1)
			{ if (Mapped) Readback->Unlock(); return false; }
			Out.SetNumUninitialized(FRokGrass3SameUvProbe::Width);
			FMemory::Memcpy(Out.GetData(), Mapped,
				FRokGrass3SameUvProbe::Width * sizeof(FFloat16Color));
			Readback->Unlock();
			return true;
		};
		if (!Copy(Shared->RawReadback, Shared->Raw, Shared->RawPitch, Shared->RawHeight)
			|| !Copy(Shared->BlendReadback, Shared->Blended, Shared->BlendPitch, Shared->BlendHeight))
		{ Shared->Error = TEXT("GPU staging lock/pitch/copy failed"); Shared->State = FRokGrass3SameUvReadback::EState::Failed; return; }
		Shared->RawReadback.Reset(); Shared->BlendReadback.Reset();
		Shared->ReadyFrame = GFrameCounterRenderThread;
		Shared->State = FRokGrass3SameUvReadback::EState::Ready;
	});
}
void URokGrass3SameUvAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	if (FPlatformTime::Seconds() - StartSeconds > RokGrass3SameUvAudit::TimeoutSeconds)
	{ Finish(TEXT("Atlas readiness or two GPU readbacks timed out")); return; }
	if (!bStarted) { Start(); return; }
	FRokGrass3SameUvReadback::EState State;
	FString Error;
	{
		FScopeLock Lock(&Job->Mutex); State = Job->State; Error = Job->Error;
	}
	if (State == FRokGrass3SameUvReadback::EState::Failed) Finish(Error);
	else if (State == FRokGrass3SameUvReadback::EState::Ready) Finish();
	else Poll();
}
void URokGrass3SameUvAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	bFinished = true;
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), TEXT("rok.grass3_same_uv_rhi_probe.v1"));
	J->SetStringField(TEXT("status"), Failure.IsEmpty() ? TEXT("GPU_READBACK_COMPLETE_PARITY_OPEN") : TEXT("FAILED"));
	J->SetStringField(TEXT("failure"), Failure);
	J->SetStringField(TEXT("source_shader"), TEXT("custom/landmap/tree-16-nomask"));
	J->SetStringField(TEXT("source_shader_guid"), RokGrass3SameUvAudit::SourceShaderGuid);
	J->SetStringField(TEXT("source_material_path"), TEXT("Assets/BundleAssets/land/Material/map_tree_nomask.mat"));
	J->SetStringField(TEXT("source_material_guid"), RokGrass3SameUvAudit::SourceMaterialGuid);
	J->SetStringField(TEXT("source_atlas_guid"), RokGrass3SameUvAudit::SourceGuid);
	J->SetStringField(TEXT("source_atlas_png_sha256"), RokGrass3SameUvAudit::SourcePngSha);
	J->SetStringField(TEXT("unity_result_sha256"), RokGrass3SameUvAudit::UnityResultSha);
	J->SetStringField(TEXT("uv_table_sha256"), RokGrass3SameUvAudit::UvTableSha);
	J->SetStringField(TEXT("source_atlas_object"), RokGrass3SameUvAudit::AtlasPath);
	J->SetStringField(TEXT("source_mapping_reference"), TEXT("/Game/RokUnityWorldMapStreaming/Materials/M_WorldSource_Grass3SignedCenterV5.M_WorldSource_Grass3SignedCenterV5"));
	J->SetStringField(TEXT("source_atlas_package_sha256"), RokGrass3SameUvAudit::ShaFile(
		FPaths::ProjectContentDir() / TEXT("RokUnityWorldMapPrototype/Textures/T_WorldSource_map_tree_atlas_7e11c882.uasset")));
	J->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	J->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	J->SetNumberField(TEXT("rhi_interface_type"), static_cast<int32>(RHIGetInterfaceType()));
	J->SetStringField(TEXT("sample_uv_mapping"), TEXT("direct_probe_ue_uv=(unity_u,1-unity_v); based on historical Custom node evidence"));
	J->SetBoolField(TEXT("material_evaluated"), false);
	J->SetBoolField(TEXT("direct_texture_shader_probe"), true);
	J->SetStringField(TEXT("sampler"), TEXT("RHI static bilinear clamp clamp; mip0; source asset filter/address validated"));
	J->SetStringField(TEXT("raw_semantics"), TEXT("sampled RGBA; Blend Off; transparent clear; separate target"));
	J->SetStringField(TEXT("blend_semantics"), TEXT("SrcAlpha OneMinusSrcAlpha for RGB and A; transparent clear; separate target"));
	J->SetStringField(TEXT("output_format"), TEXT("PF_FloatRGBA/RGBA16F"));
	J->SetBoolField(TEXT("source_asset_mutated"), false);
	J->SetBoolField(TEXT("full_map_visual_accepted"), false);
	J->SetNumberField(TEXT("expected_source_centers"), 16);
	J->SetNumberField(TEXT("expected_source_samples"), 144);
	J->SetNumberField(TEXT("expected_clamp_controls"), 8);
	if (Atlas.IsValid())
	{
		J->SetNumberField(TEXT("asset_width"), Atlas->GetSizeX());
		J->SetNumberField(TEXT("asset_height"), Atlas->GetSizeY());
		J->SetNumberField(TEXT("asset_pixel_format"), Atlas->GetPixelFormat());
		J->SetNumberField(TEXT("asset_filter"), Atlas->Filter);
		J->SetNumberField(TEXT("asset_address_x"), Atlas->AddressX);
		J->SetNumberField(TEXT("asset_address_y"), Atlas->AddressY);
		J->SetBoolField(TEXT("asset_srgb"), Atlas->SRGB);
		J->SetNumberField(TEXT("asset_num_mips"), Atlas->GetNumMips());
		J->SetNumberField(TEXT("asset_resident_mips"), Atlas->GetNumResidentMips());
	}
	if (Job)
	{
		FScopeLock Lock(&Job->Mutex);
		J->SetNumberField(TEXT("rhi_width"), Job->SourceExtent.X);
		J->SetNumberField(TEXT("rhi_height"), Job->SourceExtent.Y);
		J->SetNumberField(TEXT("rhi_pixel_format"), Job->SourceFormat);
		J->SetStringField(TEXT("rhi_pixel_format_name"), Job->SourceFormat >= 0
			&& Job->SourceFormat < PF_MAX ? GPixelFormats[Job->SourceFormat].Name : TEXT("UNKNOWN"));
		J->SetNumberField(TEXT("rhi_num_mips"), Job->SourceMips);
		J->SetNumberField(TEXT("rhi_num_samples"), Job->SourceSamples);
		J->SetBoolField(TEXT("rhi_srgb"), Job->bSourceSrgb);
		J->SetNumberField(TEXT("raw_staging_pitch_pixels"), Job->RawPitch);
		J->SetNumberField(TEXT("blend_staging_pitch_pixels"), Job->BlendPitch);
		J->SetNumberField(TEXT("raw_staging_height"), Job->RawHeight);
		J->SetNumberField(TEXT("blend_staging_height"), Job->BlendHeight);
		J->SetNumberField(TEXT("copy_render_frame"), static_cast<double>(Job->CopyFrame));
		J->SetNumberField(TEXT("ready_render_frame"), static_cast<double>(Job->ReadyFrame));
		TArray<TSharedPtr<FJsonValue>> Raw, Blended;
		for (const FFloat16Color& C : Job->Raw)
			Raw.Add(MakeShared<FJsonValueArray>(RokGrass3SameUvAudit::Rgba(C)));
		for (const FFloat16Color& C : Job->Blended)
			Blended.Add(MakeShared<FJsonValueArray>(RokGrass3SameUvAudit::Rgba(C)));
		J->SetArrayField(TEXT("raw_rgba"), Raw);
		J->SetArrayField(TEXT("source_blended_rgba"), Blended);
		J->SetNumberField(TEXT("readback_source_samples"), FMath::Min(Job->Raw.Num(), Job->Blended.Num()) >= 144 ? 144 : 0);
		J->SetNumberField(TEXT("readback_clamp_controls"), FMath::Min(Job->Raw.Num(), Job->Blended.Num()) == 152 ? 8 : 0);
	}
	else
	{
		J->SetArrayField(TEXT("raw_rgba"), TArray<TSharedPtr<FJsonValue>>());
		J->SetArrayField(TEXT("source_blended_rgba"), TArray<TSharedPtr<FJsonValue>>());
	}
	const bool bDirectoryReady = IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	FString Text;
	const bool bSerialized = FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	const bool bSaved = bDirectoryReady && bSerialized
		&& FFileHelper::SaveStringToFile(Text, *(OutputDirectory / TEXT("result.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bPassed = Failure.IsEmpty() && bSaved;
	if (!bSaved)
		UE_LOG(LogTemp, Error, TEXT("Grass3 same-UV result write failed: %s"), *OutputDirectory);
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("Grass3 same-UV GPU observation COMPLETE: %s"), *OutputDirectory);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Grass3 same-UV GPU observation FAILED: %s; %s"), *Failure, *OutputDirectory);
	}
	// This subsystem exists only on the explicit local probe Game path. Deinitialize
	// still releases both render-thread readbacks and flushes after this deferred request.
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
