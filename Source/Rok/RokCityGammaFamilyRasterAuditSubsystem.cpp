#include "RokCityGammaFamilyRasterAuditSubsystem.h"

#include "RokCityGammaRaster.h"
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
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "RHIGPUReadback.h"
#include "RHITextureInitializer.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr TCHAR FixtureRelative[] = TEXT("Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v3/manifest.json");
constexpr TCHAR FixtureExpectedSha[] = TEXT("5734e6c8b7efeee276ec9bb712d2a03ffcc3c3430548f8cb8751f02b3d17ac1c");
constexpr TCHAR SealedV2ExpectedSha[] = TEXT("7607afededef0e985c4458020e672389b14afd6f4566d47ee10ce770babe1165");
constexpr TCHAR NativeExpectedSha[] = TEXT("671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71");
constexpr TCHAR NightNativeExpectedSha[] = TEXT("8c228f9f01cabc3bcdabc9ef09926470d38798b4a11c09c308cb5998636d99ea");
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr double StartupTimeout = 45.0, GpuTimeout = 20.0;
constexpr int32 Extent = 128;

bool ExactWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game &&
		World->GetOutermost()->GetName() == MapName && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
FString HashBytes(const TArray<uint8>& Bytes) { return RokTrainArmyMesh::Sha256(Bytes); }
FString HashFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? HashBytes(Bytes) : FString();
}
FString SafeInput(const FString& Relative)
{
	if (!(Relative.StartsWith(TEXT("Saved/UnityCityGammaFamilyStateProbeV2/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityGammaRemainingFamiliesProbe/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v3/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityNightTimeProbeV1/")) ||
		Relative == TEXT("ref/client-unity/Assets/BundleAssets/Map/Shader/cm_city_mask_night.shader") ||
		Relative == TEXT("Scripts/ExportRokCityGammaFamilyStateProbe.cs")) ||
		Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) || Relative.Contains(TEXT("\\"))) return FString();
	return FPaths::ProjectDir() / Relative;
}
bool LoadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *Path) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
TArray<TSharedPtr<FJsonValue>> Objects(const TArray<TSharedPtr<FJsonObject>>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const TSharedPtr<FJsonObject>& Value : Values) Out.Add(MakeShared<FJsonValueObject>(Value));
	return Out;
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
	return Out;
}
FLinearColor ColorJson(const TSharedPtr<FJsonObject>& Object)
{
	return FLinearColor(Object->GetNumberField(TEXT("r")), Object->GetNumberField(TEXT("g")),
		Object->GetNumberField(TEXT("b")), Object->GetNumberField(TEXT("a")));
}
}

struct FRokGammaFamilyRasterJob
{
	enum class EState : uint8 { Idle, UploadPending, Uploaded, ReadPending, Ready, Failed, Cancelled };
	FCriticalSection Mutex;
	EState State = EState::Idle;
	FString Error;
	FTextureRHIRef Textures[5];
	TArray<TArray<uint8>> Uploads;
	TUniquePtr<FRHIGPUTextureReadback> Readback; // RT only
	TRefCountPtr<IPooledRenderTarget> Output;
	TArray<uint8> Bytes;
	FRokCityGammaControlledInstance Instance;
	int32 Pitch = 0, Height = 0;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	bool bPollQueued = false, bInput = false;
};

bool URokCityGammaFamilyRasterAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return ExactWorld(Cast<UWorld>(Outer)) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaFamilyRasterAudit")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCityGammaFamilyRasterAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCityGammaFamilyRasterAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityGammaFamilyRasterAuditSubsystem, STATGROUP_Tickables);
}
void URokCityGammaFamilyRasterAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		TEXT("RokUnityMigration/BatchReview20261009NightTimeAndExpedition/CityGammaFamilyRasterAudit") /
		FString::Printf(TEXT("%s_%u_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*Directory) || !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Gamma family raster: cannot create unique run directory %s"), *Directory);
		bFinished = true;
	}
}
void URokCityGammaFamilyRasterAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("World ended before 42 fixture GPU readbacks completed"));
	ReleaseJob();
	Super::Deinitialize();
}
bool URokCityGammaFamilyRasterAuditSubsystem::Prepare()
{
	UWorld* World = GetWorld();
	if (!ExactWorld(World) || !GEngine || !GEngine->GameViewport ||
		GEngine->GameViewport->GetWorld() != World || !GEngine->GameViewport->Viewport ||
		!World->GetFirstPlayerController() || !World->GetFirstPlayerController()->IsLocalController()) return false;
	URokServerSubsystem* Server = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun()) { Finish(TEXT("Local-only server boundary unavailable")); return false; }
	InitialHttpDispatches = Server->GetHttpDispatchAttemptCount();
	if (InitialHttpDispatches != 0) { Finish(TEXT("HTTP dispatch already attempted")); return false; }
	FixtureHash = HashFile(FPaths::ProjectDir() / FixtureRelative);
	if (FixtureHash != FixtureExpectedSha || !LoadJson(FPaths::ProjectDir() / FixtureRelative, Fixture) ||
		Fixture->GetStringField(TEXT("schema")) != TEXT("rok.city_gamma_family_raster_fixture.v3") ||
		Fixture->GetArrayField(TEXT("rows")).Num() != 5 || Fixture->GetArrayField(TEXT("cases")).Num() != 42 ||
		Fixture->GetArrayField(TEXT("textures")).Num() != 5)
	{
		Finish(TEXT("Pinned 5-family/42-case fixture missing or differs")); return false;
	}
	for (const TCHAR* Prefix : { TEXT("native_state"), TEXT("material_probe"), TEXT("source_contract"), TEXT("exporter") })
	{
		const FString Path = Fixture->GetStringField(FString(Prefix) + TEXT("_path"));
		const FString Expected = Fixture->GetStringField(FString(Prefix) + TEXT("_sha256"));
		if (SafeInput(Path).IsEmpty() || HashFile(SafeInput(Path)) != Expected ||
			(FString(Prefix) == TEXT("native_state") && Expected != NativeExpectedSha))
		{
			Finish(FString::Printf(TEXT("Pinned Unity source chain differs: %s"), Prefix)); return false;
		}
	}
	const FString V2Path = Fixture->GetStringField(TEXT("sealed_v2_fixture_path"));
	if (V2Path != TEXT("Saved/RokUnityMigration/CityGammaFamilyRasterFixture/v2/manifest.json") ||
		Fixture->GetStringField(TEXT("sealed_v2_fixture_sha256")) != SealedV2ExpectedSha ||
		HashFile(SafeInput(V2Path)) != SealedV2ExpectedSha)
	{
		Finish(TEXT("Sealed V2 fixture identity differs")); return false;
	}
	if (Fixture->GetObjectField(TEXT("diagnostic"))->GetStringField(TEXT("time_y")) !=
		TEXT("one_time_gpu_measured_0_from_native_same_command_buffer") ||
		Fixture->GetObjectField(TEXT("diagnostic"))->GetStringField(TEXT("sealed_v2_night_time_scope")) !=
		TEXT("old V2 Night B1 frames had no GPU shader time proof"))
	{
		Finish(TEXT("V3 diagnostic time scope differs")); return false;
	}
	const TSharedPtr<FJsonObject> Night = Fixture->GetObjectField(TEXT("night_gpu_time"));
	const auto Pinned = [&](const TCHAR* Stem) -> bool
	{
		const FString Path = Night->GetStringField(FString(Stem) + TEXT("_path"));
		const FString Expected = Night->GetStringField(FString(Stem) + TEXT("_sha256"));
		const FString Safe = SafeInput(Path);
		return !Safe.IsEmpty() && Expected.Len() == 64 && HashFile(Safe) == Expected;
	};
	for (const TCHAR* Stem : { TEXT("native"), TEXT("native_verify"), TEXT("contract"),
		TEXT("exporter"), TEXT("time_shader"), TEXT("source_shader"),
		TEXT("time_before"), TEXT("time_after") })
	{
		if (!Pinned(Stem)) { Finish(FString::Printf(TEXT("Night GPU time chain differs: %s"), Stem)); return false; }
	}
	if (Night->GetStringField(TEXT("native_sha256")) != NightNativeExpectedSha ||
		!Night->GetBoolField(TEXT("before_after_float4_raw_equal")) ||
		Night->GetBoolField(TEXT("game_app_loaded")) ||
		Night->GetStringField(TEXT("source_material_guid")) != Fixture->GetArrayField(TEXT("rows"))[0]->AsObject()->GetStringField(TEXT("source_material_guid")) ||
		Night->GetStringField(TEXT("source_renderer_file_id")) != Fixture->GetArrayField(TEXT("rows"))[0]->AsObject()->GetStringField(TEXT("renderer_file_id")) ||
		Night->GetStringField(TEXT("source_shader_name")) != Fixture->GetArrayField(TEXT("rows"))[0]->AsObject()->GetStringField(TEXT("source_shader")) ||
		Night->GetIntegerField(TEXT("source_queue")) != 2996 ||
		Night->GetStringField(TEXT("source_mask_sprite_path")) != TEXT("Assets/Source/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_1_101_1_mask.png") ||
		Night->GetStringField(TEXT("source_mask_sprite_file_id")) != TEXT("21300000") ||
		!Night->GetBoolField(TEXT("source_mask_property_block_matches")) ||
		Night->GetIntegerField(TEXT("source_sprite_vertices")) != 240 ||
		Night->GetIntegerField(TEXT("source_sprite_triangles")) != 238 ||
		!Night->GetStringField(TEXT("time_source_scope")).Contains(TEXT("GPU vertex-stage")))
	{
		Finish(TEXT("Night GPU time identity/metadata differs")); return false;
	}
	TArray<uint8> TimeBefore, TimeAfter;
	if (!FFileHelper::LoadFileToArray(TimeBefore, *SafeInput(Night->GetStringField(TEXT("time_before_path")))) ||
		!FFileHelper::LoadFileToArray(TimeAfter, *SafeInput(Night->GetStringField(TEXT("time_after_path")))) ||
		TimeBefore.Num() != 16 || TimeBefore != TimeAfter ||
		Night->GetStringField(TEXT("gpu_shader_time_y_bits_hex")) != BytesToHex(TimeBefore.GetData(), 4).ToLower())
	{
		Finish(TEXT("Night same-command-buffer raw float4 proof differs")); return false;
	}
	FMemory::Memcpy(&MeasuredNightTimeY, TimeBefore.GetData(), sizeof(float));
	if (!FMath::IsFinite(MeasuredNightTimeY) || MeasuredNightTimeY != 0.0f ||
		static_cast<double>(MeasuredNightTimeY) != Night->GetNumberField(TEXT("gpu_shader_time_y_f32")))
	{
		Finish(TEXT("Night GPU-measured shader _Time.y differs")); return false;
	}
	TSharedPtr<FJsonObject> NightNative, NightVerify;
	if (!LoadJson(SafeInput(Night->GetStringField(TEXT("native_path"))), NightNative) ||
		!LoadJson(SafeInput(Night->GetStringField(TEXT("native_verify_path"))), NightVerify) ||
		NightNative->GetStringField(TEXT("source_contract_sha256")) != Night->GetStringField(TEXT("contract_sha256")) ||
		NightNative->GetStringField(TEXT("time_before_sha256")) != Night->GetStringField(TEXT("time_before_sha256")) ||
		NightNative->GetStringField(TEXT("time_after_sha256")) != Night->GetStringField(TEXT("time_after_sha256")) ||
		NightNative->GetStringField(TEXT("source_mask_sprite_path")) != Night->GetStringField(TEXT("source_mask_sprite_path")) ||
		NightNative->GetStringField(TEXT("source_mask_sprite_file_id")) != Night->GetStringField(TEXT("source_mask_sprite_file_id")) ||
		NightVerify->GetStringField(TEXT("native_sha256")) != NightNativeExpectedSha ||
		!NightVerify->GetBoolField(TEXT("passed")) ||
		!NightVerify->GetBoolField(TEXT("before_after_float4_raw_equal")) ||
		NightVerify->GetNumberField(TEXT("gpu_shader_time_y_f32")) != MeasuredNightTimeY)
	{
		Finish(TEXT("Night native verifier/source time proof differs")); return false;
	}
	TSharedPtr<FJsonObject> SealedV2;
	if (!LoadJson(SafeInput(V2Path), SealedV2) ||
		SealedV2->GetArrayField(TEXT("cases")).Num() != 42)
	{
		Finish(TEXT("Sealed V2 case set invalid")); return false;
	}
	int32 NightReplacements = 0;
	for (int32 Index = 0; Index < 42; ++Index)
	{
		const TSharedPtr<FJsonObject> Case = Fixture->GetArrayField(TEXT("cases"))[Index]->AsObject();
		const TSharedPtr<FJsonObject> Old = SealedV2->GetArrayField(TEXT("cases"))[Index]->AsObject();
		const FString Frame = Case->GetStringField(TEXT("frame"));
		const bool bNewNight = Case->GetIntegerField(TEXT("family_index")) == 0 &&
			(Frame == TEXT("family_a") || Frame == TEXT("family_b"));
		if (Case->GetStringField(TEXT("id")) != Old->GetStringField(TEXT("id")) ||
			Case->GetStringField(TEXT("comparison")) != TEXT("rgb_max_1_lsb") ||
			Case->GetNumberField(TEXT("controlled_time_y_f32")) != MeasuredNightTimeY ||
			Case->GetStringField(TEXT("controlled_time_proof_sha256")) != Night->GetStringField(TEXT("time_before_sha256")) ||
			SafeInput(Case->GetStringField(TEXT("source_png"))).IsEmpty() ||
			HashFile(SafeInput(Case->GetStringField(TEXT("source_png")))) != Case->GetStringField(TEXT("source_png_sha256")))
		{
			Finish(TEXT("V3 case identity/source/time pin differs")); return false;
		}
		if (bNewNight)
		{
			const FString Prefix = Frame == TEXT("family_a") ? TEXT("b1") : TEXT("b1_b");
			const FString Relative = Night->GetStringField(TEXT("native_path"));
			const FString ExpectedPath = FPaths::GetPath(Relative) / NightNative->GetStringField(Prefix + TEXT("_png"));
			if (Case->GetStringField(TEXT("source_png")) != ExpectedPath ||
				Case->GetStringField(TEXT("source_png_sha256")) != NightNative->GetStringField(Prefix + TEXT("_sha256")) ||
				Case->GetStringField(TEXT("night_gpu_time_source")) != TEXT("same_buffer_before_after_raw_float4"))
			{
				Finish(TEXT("Night B1 A/B new source replacement differs")); return false;
			}
			++NightReplacements;
		}
		else if (Case->GetStringField(TEXT("source_png")) != Old->GetStringField(TEXT("source_png")) ||
			Case->GetStringField(TEXT("source_png_sha256")) != Old->GetStringField(TEXT("source_png_sha256")))
		{
			Finish(TEXT("One of 40 sealed V2 RGB references differs")); return false;
		}
	}
	if (NightReplacements != 2) { Finish(TEXT("Expected exactly two Night B1 replacements")); return false; }
	Job = MakeShared<FRokGammaFamilyRasterJob, ESPMode::ThreadSafe>();
	for (const TSharedPtr<FJsonValue>& Value : Fixture->GetArrayField(TEXT("textures")))
	{
		const TSharedPtr<FJsonObject> Texture = Value->AsObject();
		TArray<uint8> Bytes;
		const FString Path = SafeInput(Texture->GetStringField(TEXT("file")));
		if (Path.IsEmpty() || !FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() != 16 ||
			HashBytes(Bytes) != Texture->GetStringField(TEXT("sha256")) ||
			Texture->GetIntegerField(TEXT("width")) != 2 || Texture->GetIntegerField(TEXT("height")) != 2 ||
			Texture->GetBoolField(TEXT("srgb")) || Texture->GetIntegerField(TEXT("mips")) != 1)
		{
			Finish(TEXT("Pinned 2x2 diagnostic input bytes/metadata differ")); return false;
		}
		Job->Uploads.Add(MoveTemp(Bytes));
	}
	bPrepared = true;
	return true;
}
void URokCityGammaFamilyRasterAuditSubsystem::BeginUpload()
{
	bUploadStarted = true;
	PendingSeconds = FPlatformTime::Seconds();
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->State = FRokGammaFamilyRasterJob::EState::UploadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokFamilyFixtureUpload)([Shared](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(
				TEXT("RokFamilyControlled2x2"), 2, 2, PF_B8G8R8A8)
				.SetFlags(ETextureCreateFlags::ShaderResource).SetNumMips(1).SetInitActionInitializer();
			FRHITextureInitializer Initializer = RHICmdList.CreateTextureInitializer(Desc);
			const FRHITextureSubresourceInitializer Sub = Initializer.GetTexture2DSubresource(0);
			if (!Sub.Data || Sub.Stride < 8 || Sub.Size < Sub.Stride * 2)
			{
				Shared->Error = TEXT("2x2 RHI initializer invalid"); Shared->State = FRokGammaFamilyRasterJob::EState::Failed; return;
			}
			for (int32 Y = 0; Y < 2; ++Y)
				FMemory::Memcpy(static_cast<uint8*>(Sub.Data) + Y * Sub.Stride, Shared->Uploads[Index].GetData() + Y * 8, 8);
			Shared->Textures[Index] = Initializer.Finalize();
			if (!Shared->Textures[Index].IsValid() || Shared->Textures[Index]->GetDesc().Format != PF_B8G8R8A8 ||
				EnumHasAnyFlags(Shared->Textures[Index]->GetDesc().Flags, TexCreate_SRGB))
			{
				Shared->Error = TEXT("2x2 RHI descriptor invalid"); Shared->State = FRokGammaFamilyRasterJob::EState::Failed; return;
			}
		}
		Shared->State = FRokGammaFamilyRasterJob::EState::Uploaded;
	});
}
void URokCityGammaFamilyRasterAuditSubsystem::BeginInputReadback()
{
	PendingSeconds = FPlatformTime::Seconds();
	const auto Shared = Job;
	const int32 Index = InputIndex;
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGammaFamilyRasterJob::EState::Uploaded) return;
		Shared->State = FRokGammaFamilyRasterJob::EState::ReadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokFamilyFixtureInputReadback)([Shared, Index](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGammaFamilyRasterJob::EState::ReadPending) return;
		Shared->bInput = true;
		Shared->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokFamilyControlledInput"));
		Shared->Readback->EnqueueCopy(RHICmdList, Shared->Textures[Index].GetReference(),
			FIntVector::ZeroValue, 0, FIntVector(2, 2, 1));
		Shared->CopyFrame = GFrameCounterRenderThread;
		Shared->State = FRokGammaFamilyRasterJob::EState::ReadPending;
	});
}

void URokCityGammaFamilyRasterAuditSubsystem::BeginFrame()
{
	const TSharedPtr<FJsonObject> Case = Fixture->GetArrayField(TEXT("cases"))[CaseIndex]->AsObject();
	const int32 FamilyIndex = Case->GetIntegerField(TEXT("family_index"));
	const TSharedPtr<FJsonObject> Row = Fixture->GetArrayField(TEXT("rows"))[FamilyIndex]->AsObject();
	FRokCityGammaControlledInstance Instance;
	Instance.Family = static_cast<ERokCityGammaControlledFamily>(FamilyIndex);
	Instance.SourceKey = FName(*Row->GetStringField(TEXT("source_key")));
	Instance.MaterialGuid = Row->GetStringField(TEXT("source_material_guid"));
	Instance.RendererFileId = Row->GetStringField(TEXT("renderer_file_id"));
	Instance.MeshFileId = Row->GetStringField(TEXT("mesh_file_id"));
	Instance.InstanceId = static_cast<uint64>(FamilyIndex + 1);
	Instance.SortPriority = Row->GetIntegerField(TEXT("source_queue"));
	Instance.RendererTint = FLinearColor::White;
	Instance.GlobalSpriteColor = FLinearColor::White;
	Instance.MaterialColor = ColorJson(Row->GetObjectField(TEXT("material_color")));
	Instance.Team1 = ColorJson(Row->GetObjectField(TEXT("team1")));
	Instance.Team2 = ColorJson(Row->GetObjectField(TEXT("team2")));
	Instance.NightColor3 = ColorJson(Row->GetObjectField(TEXT("night_color3")));
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		const TSharedPtr<FJsonObject> ST = Row->GetArrayField(TEXT("tex_st"))[Slot]->AsObject();
		const TSharedPtr<FJsonObject> Scale = ST->GetObjectField(TEXT("scale"));
		const TSharedPtr<FJsonObject> Offset = ST->GetObjectField(TEXT("offset"));
		Instance.TexST[Slot] = FVector4f(Scale->GetNumberField(TEXT("x")), Scale->GetNumberField(TEXT("y")),
			Offset->GetNumberField(TEXT("x")), Offset->GetNumberField(TEXT("y")));
	}
	Instance.Transparency = 1.0f;
	Instance.ControlledTimeY = static_cast<float>(Case->GetNumberField(TEXT("controlled_time_y_f32")));
	Instance.bControlledTimeQualified = Instance.ControlledTimeY == MeasuredNightTimeY;
	const float Alpha = Row->GetIntegerField(TEXT("vertex_alpha8")) / 255.0f;
	// Exact C# diagnostic quad after documented Unity -> UE (-x,z,y)*100.
	const FVector3f Points[4] = { FVector3f(100,0,-100), FVector3f(-100,0,-100),
		FVector3f(-100,0,100), FVector3f(100,0,100) };
	const FVector2f UVs[4] = { FVector2f(0,0), FVector2f(1,0), FVector2f(1,1), FVector2f(0,1) };
	for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
	{
		FRokCityGammaControlledVertex Vertex;
		Vertex.LocalCm = Points[VertexIndex];
		Vertex.UV = UVs[VertexIndex];
		Vertex.Alpha = Alpha;
		Instance.Vertices.Add(Vertex);
	}
	Instance.Indices = { 0, 2, 1, 0, 3, 2 };
	const auto AddTexture = [&](ERokCityGammaControlledSlot Slot, int32 InputIndex)
	{
		FRokCityGammaControlledTexture Texture;
		Texture.Slot = Slot;
		Texture.Texture = Job->Textures[InputIndex];
		Texture.Extent = FIntPoint(2, 2);
		Texture.NumMips = 1;
		Texture.FixtureBgraSha256 = Fixture->GetArrayField(TEXT("textures"))[InputIndex]->AsObject()->GetStringField(TEXT("sha256"));
		Instance.Textures.Add(Texture);
	};
	const FString Override = Case->GetStringField(TEXT("mask_override"));
	if (FamilyIndex == 0) { AddTexture(ERokCityGammaControlledSlot::Main, 0); AddTexture(ERokCityGammaControlledSlot::Mask, Override == TEXT("mask_no_blue") ? 3 : 2); }
	else if (FamilyIndex == 1 || FamilyIndex == 2)
	{
		for (int32 Slot = 0; Slot < 4; ++Slot) AddTexture(static_cast<ERokCityGammaControlledSlot>(Slot + 2), 0);
		AddTexture(ERokCityGammaControlledSlot::Splat, 1);
	}
	else { AddTexture(ERokCityGammaControlledSlot::Main, 0); }
	FString Error;
	if (!FRokCityGammaRaster::ValidateControlledFixture({ Instance }, Error))
	{
		Finish(TEXT("Shared raster fixture rejected: ") + Error); return;
	}
	const auto Shared = Job;
	const bool bFamily = Case->GetBoolField(TEXT("draw_family"));
	const bool bFront = Case->GetBoolField(TEXT("front"));
	const bool bLate = Case->GetBoolField(TEXT("late"));
	const bool bBackgroundB = Case->GetStringField(TEXT("background")) == TEXT("b");
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGammaFamilyRasterJob::EState::Uploaded) return;
		Shared->State = FRokGammaFamilyRasterJob::EState::ReadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokFamilyFixtureFrame)([Shared, Instance, bFamily, bFront, bLate, bBackgroundB](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGammaFamilyRasterJob::EState::ReadPending) return;
		Shared->bInput = false;
		Shared->Instance = Instance;
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef Color = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(
			FIntPoint(Extent, Extent), PF_B8G8R8A8, FClearValueBinding::Black,
			TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RokFamilyFixtureColor"));
		FRDGTextureRef Depth = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(
			FIntPoint(Extent, Extent), PF_DepthStencil, FClearValueBinding::DepthFar,
			TexCreate_DepthStencilTargetable), TEXT("RokFamilyFixtureDepth"));
		AddClearRenderTargetPass(GraphBuilder, Color, bBackgroundB
			? FLinearColor(.74f,.09f,.05f,1) : FLinearColor(.07f,.15f,.71f,1));
		AddClearDepthStencilPass(GraphBuilder, Depth, true, 0.0f, false, 0); // UE reversed-Z far.
		const FIntRect QuadRect(13, 13, 115, 115);
		const FLinearColor Green(.03f,.94f,.07f,1);
		if (bFront)
		{
			AddClearRenderTargetPass(GraphBuilder, Color, Green, QuadRect);
			AddClearDepthStencilPass(GraphBuilder, Depth, true, 1.0f, false, 0); // Front control blocks fixture.
		}
		if (bFamily)
		{
			FMatrix44f LocalToClip = FMatrix44f::Identity;
			LocalToClip.M[0][0] = -0.008f;
			LocalToClip.M[2][1] = 0.008f;
			LocalToClip.M[2][2] = 0.0f;
			LocalToClip.M[3][2] = 0.5f;
			FString Error;
			if (!FRokCityGammaRaster::AddControlledFixturePass(GraphBuilder, Color, Depth,
				FIntRect(0,0,Extent,Extent), LocalToClip, Shared->Instance, Error))
			{
				Shared->Error = TEXT("Shared fixture pass rejected: ") + Error;
				Shared->State = FRokGammaFamilyRasterJob::EState::Failed; return;
			}
		}
		if (bLate) AddClearRenderTargetPass(GraphBuilder, Color, Green, QuadRect);
		GraphBuilder.QueueTextureExtraction(Color, &Shared->Output);
		GraphBuilder.Execute();
		if (!Shared->Output.IsValid())
		{
			Shared->Error = TEXT("RDG output extraction failed"); Shared->State = FRokGammaFamilyRasterJob::EState::Failed; return;
		}
		Shared->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokFamilyFixture128BGRA"));
		Shared->Readback->EnqueueCopy(RHICmdList, Shared->Output->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(Extent,Extent,1));
		Shared->CopyFrame = GFrameCounterRenderThread;
		Shared->State = FRokGammaFamilyRasterJob::EState::ReadPending;
	});
}

void URokCityGammaFamilyRasterAuditSubsystem::Poll()
{
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokGammaFamilyRasterJob::EState::ReadPending || Shared->bPollQueued) return;
		Shared->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokFamilyFixturePoll)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->bPollQueued = false;
		if (Shared->State != FRokGammaFamilyRasterJob::EState::ReadPending ||
			!Shared->Readback || !Shared->Readback->IsReady()) return;
		int32 Pitch = 0, Height = 0;
		const void* Mapped = Shared->Readback->Lock(Pitch, &Height);
		const int32 Width = Shared->bInput ? 2 : Extent;
		if (!Mapped || Pitch < Width || Height < Width)
		{
			if (Mapped) Shared->Readback->Unlock();
			Shared->Readback.Reset(); Shared->Error = TEXT("GPU staging row pitch/height invalid");
			Shared->State = FRokGammaFamilyRasterJob::EState::Failed; return;
		}
		Shared->Pitch = Pitch; Shared->Height = Height;
		Shared->Bytes.SetNumUninitialized(Width * Width * 4);
		for (int32 Y = 0; Y < Width; ++Y)
			FMemory::Memcpy(Shared->Bytes.GetData() + Y * Width * 4,
				static_cast<const uint8*>(Mapped) + Y * Pitch * 4, Width * 4);
		Shared->Readback->Unlock(); Shared->Readback.Reset(); Shared->Output.SafeRelease();
		Shared->ReadyFrame = GFrameCounterRenderThread;
		Shared->State = FRokGammaFamilyRasterJob::EState::Ready;
	});
}

void URokCityGammaFamilyRasterAuditSubsystem::RecordReady()
{
	TArray<uint8> Bytes;
	int32 Pitch, Height;
	uint64 CopyFrame, ReadyFrame;
	bool bInput;
	{
		FScopeLock Lock(&Job->Mutex);
		if (Job->State != FRokGammaFamilyRasterJob::EState::Ready) return;
		Bytes = Job->Bytes; Pitch = Job->Pitch; Height = Job->Height;
		CopyFrame = Job->CopyFrame; ReadyFrame = Job->ReadyFrame; bInput = Job->bInput;
		Job->State = FRokGammaFamilyRasterJob::EState::Uploaded;
	}
	if (bInput)
	{
		const TSharedPtr<FJsonObject> Source = Fixture->GetArrayField(TEXT("textures"))[InputIndex]->AsObject();
		const FString Name = Source->GetStringField(TEXT("name"));
		const FString File = Name + TEXT(".gpu.bgra8.bin");
		const bool bExact = Bytes == Job->Uploads[InputIndex];
		if (!FFileHelper::SaveArrayToFile(Bytes, *(Directory / File)) || !bExact)
		{
			Finish(TEXT("2x2 input GPU readback failed: ") + Name); return;
		}
		auto Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("name"), Name); Record->SetStringField(TEXT("gpu_file"), File);
		Record->SetStringField(TEXT("gpu_sha256"), HashBytes(Bytes));
		Record->SetStringField(TEXT("expected_sha256"), Source->GetStringField(TEXT("sha256")));
		Record->SetBoolField(TEXT("byte_exact"), bExact);
		Record->SetNumberField(TEXT("row_pitch_pixels"), Pitch); Record->SetNumberField(TEXT("staging_height"), Height);
		Record->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
		Record->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
		InputResults.Add(Record); ++InputIndex;
		return;
	}
	const TSharedPtr<FJsonObject> Case = Fixture->GetArrayField(TEXT("cases"))[CaseIndex]->AsObject();
	const FString Id = Case->GetStringField(TEXT("id"));
	const FString RawName = Id + TEXT(".gpu.bgra8.bin");
	const FString PngName = Id + TEXT(".gpu.png");
	if (Bytes.Num() != Extent*Extent*4 || !FFileHelper::SaveArrayToFile(Bytes, *(Directory / RawName)))
	{
		Finish(TEXT("Cannot save tight GPU frame: ") + Id); return;
	}
	TArray<FColor> Colors;
	Colors.SetNumUninitialized(Extent*Extent);
	FMemory::Memcpy(Colors.GetData(), Bytes.GetData(), Bytes.Num());
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Extent, Extent,
		TArrayView64<const FColor>(Colors.GetData(), Colors.Num()), Png);
	if (Png.IsEmpty() || !FFileHelper::SaveArrayToFile(Png, *(Directory / PngName)))
	{
		Finish(TEXT("Cannot save GPU frame PNG: ") + Id); return;
	}
	TArray<uint8> PngHashBytes;
	PngHashBytes.Append(Png.GetData(), static_cast<int32>(Png.Num()));
	const FString SourcePath = SafeInput(Case->GetStringField(TEXT("source_png")));
	FImage Native;
	if (SourcePath.IsEmpty() || HashFile(SourcePath) != Case->GetStringField(TEXT("source_png_sha256")) ||
		!FImageUtils::LoadImage(*SourcePath, Native) || Native.SizeX != Extent || Native.SizeY != Extent)
	{
		Finish(TEXT("Pinned source PNG unreadable or changed: ") + Id); return;
	}
	FImage NativeBGRA;
	Native.CopyTo(NativeBGRA, ERawImageFormat::BGRA8, Native.GammaSpace);
	const TArrayView64<FColor> Source = NativeBGRA.AsBGRA8();
	int32 MaxRgb = 0, PixelsOverOne = 0, SumRgb = 0;
	for (int32 Index = 0; Index < Extent*Extent; ++Index)
	{
		const FColor& A = Colors[Index]; const FColor& B = Source[Index];
		const int32 Delta = FMath::Max3(FMath::Abs(int32(A.R)-int32(B.R)),
			FMath::Abs(int32(A.G)-int32(B.G)), FMath::Abs(int32(A.B)-int32(B.B)));
		MaxRgb = FMath::Max(MaxRgb, Delta); SumRgb += Delta;
		if (Delta > 1) ++PixelsOverOne;
	}
	const bool bPass = Case->GetStringField(TEXT("comparison")) == TEXT("rgb_max_1_lsb") && PixelsOverOne == 0;
	const FColor Center = Colors[64*Extent+64];
	auto Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("id"), Id); Record->SetStringField(TEXT("frame"), Case->GetStringField(TEXT("frame")));
	Record->SetNumberField(TEXT("family_index"), Case->GetIntegerField(TEXT("family_index")));
	Record->SetStringField(TEXT("source_png_sha256"), Case->GetStringField(TEXT("source_png_sha256")));
	Record->SetStringField(TEXT("gpu_bgra_file"), RawName); Record->SetStringField(TEXT("gpu_bgra_sha256"), HashBytes(Bytes));
	Record->SetStringField(TEXT("gpu_png_file"), PngName); Record->SetStringField(TEXT("gpu_png_sha256"), HashBytes(PngHashBytes));
	Record->SetStringField(TEXT("comparison"), Case->GetStringField(TEXT("comparison")));
	Record->SetNumberField(TEXT("controlled_time_y_f32"), Case->GetNumberField(TEXT("controlled_time_y_f32")));
	Record->SetStringField(TEXT("controlled_time_proof_sha256"), Case->GetStringField(TEXT("controlled_time_proof_sha256")));
	Record->SetNumberField(TEXT("max_rgb_delta"), MaxRgb);
	Record->SetNumberField(TEXT("pixels_over_1_lsb"), PixelsOverOne);
	Record->SetNumberField(TEXT("sum_pixel_max_rgb_delta"), SumRgb);
	Record->SetNumberField(TEXT("center_r"), Center.R); Record->SetNumberField(TEXT("center_g"), Center.G);
	Record->SetNumberField(TEXT("center_b"), Center.B); Record->SetNumberField(TEXT("center_a_ue_preserved"), Center.A);
	Record->SetNumberField(TEXT("row_pitch_pixels"), Pitch); Record->SetNumberField(TEXT("staging_height"), Height);
	Record->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
	Record->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
	Record->SetBoolField(TEXT("rgb_within_1_lsb"), PixelsOverOne == 0);
	Record->SetBoolField(TEXT("night_time_pending"), false);
	Record->SetBoolField(TEXT("case_passed"), bPass);
	CaseResults.Add(Record);
	if (!bPass) Failures.Add(TEXT("RGB exceeds 1 LSB: ") + Id);
	++CaseIndex;
}

void URokCityGammaFamilyRasterAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (!Prepare() && !bFinished && Now - StartSeconds > StartupTimeout)
			Finish(TEXT("Local Game viewport startup timed out"));
		return;
	}
	if (!bUploadStarted) { BeginUpload(); return; }
	FRokGammaFamilyRasterJob::EState State;
	FString Error;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State; Error = Job->Error;
	}
	if (State == FRokGammaFamilyRasterJob::EState::Failed) { Finish(TEXT("GPU fixture job failed: ") + Error); return; }
	if (State == FRokGammaFamilyRasterJob::EState::Ready) { RecordReady(); return; }
	if (State == FRokGammaFamilyRasterJob::EState::ReadPending) { Poll(); }
	if (Now - PendingSeconds > GpuTimeout) { Finish(TEXT("GPU fixture readback timed out")); return; }
	if (State != FRokGammaFamilyRasterJob::EState::Uploaded) return;
	if (InputIndex < 5) { BeginInputReadback(); return; }
	if (CaseIndex < 42) { BeginFrame(); return; }
	Finish();
}
void URokCityGammaFamilyRasterAuditSubsystem::ReleaseJob()
{
	if (!Job) return;
	const auto Shared = Job;
	ENQUEUE_RENDER_COMMAND(RokFamilyFixtureRelease)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->Readback.Reset(); Shared->Output.SafeRelease();
		for (FTextureRHIRef& Texture : Shared->Textures) Texture.SafeRelease();
		Shared->Instance.Textures.Empty();
		Shared->State = FRokGammaFamilyRasterJob::EState::Cancelled;
	});
	FlushRenderingCommands();
	Job.Reset();
}
void URokCityGammaFamilyRasterAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	ReleaseJob();
	bFinished = true;
	URokServerSubsystem* Server = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	const uint64 FinalHttp = Server ? Server->GetHttpDispatchAttemptCount() : MAX_uint64;
	if (!Server || !Server->IsLocalOnlyRun() || FinalHttp != InitialHttpDispatches || FinalHttp != 0)
		Failures.Add(TEXT("Local-only HTTP boundary changed"));
	const bool bComplete = bPrepared && InputResults.Num() == 5 && CaseResults.Num() == 42 && CaseIndex == 42;
	int32 StrictPassed = 0, PendingSourceTime = 0;
	for (const TSharedPtr<FJsonObject>& Case : CaseResults)
	{
		if (Case->GetBoolField(TEXT("night_time_pending"))) ++PendingSourceTime;
		else if (Case->GetBoolField(TEXT("rgb_within_1_lsb"))) ++StrictPassed;
	}
	const bool bPassed = bComplete && StrictPassed == 42 && PendingSourceTime == 0 && Failures.IsEmpty();
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_family_raster_runtime.v3"));
	Report->SetStringField(TEXT("scope"), TEXT("isolated controlled GPU quad at one measured Unity shader time; no GameApp/SceneCapture registration or live dynamic time acceptance"));
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("fixture_path"), FixtureRelative);
	Report->SetStringField(TEXT("fixture_sha256"), FixtureHash);
	for (const TCHAR* Relative : { TEXT("Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityGammaFamilyRasterAuditSubsystem.cpp"),
		TEXT("Scripts/PrepareRokCityGammaFamilyRasterFixtureV3.py"),
		TEXT("Scripts/VerifyRokCityGammaFamilyRasterRuntimeV3.py"),
		TEXT("Source/RokRendering/Public/RokCityGammaRaster.h"),
		TEXT("Source/RokRendering/Private/RokCityGammaRaster.cpp"),
		TEXT("Shaders/Private/RokCityGammaRaster.usf") })
		Report->SetStringField(FPaths::GetCleanFilename(Relative) + TEXT("_sha256"), HashFile(FPaths::ProjectDir() / Relative));
	Report->SetBoolField(TEXT("opt_in_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaFamilyRasterAudit")));
	Report->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Report->SetBoolField(TEXT("family_main_view_registered"), false);
	Report->SetBoolField(TEXT("scene_mutated"), false);
	Report->SetBoolField(TEXT("gameapp_accepted"), false);
	Report->SetBoolField(TEXT("mainview_accepted"), false);
	Report->SetBoolField(TEXT("live_gameapp_dynamic_time_accepted"), false);
	Report->SetBoolField(TEXT("all_42_rgb_parity_accepted"), bPassed);
	Report->SetBoolField(TEXT("all_42_controlled_rgb_parity_accepted"), bPassed);
	Report->SetBoolField(TEXT("transport_complete42"), bComplete);
	Report->SetStringField(TEXT("night_b1_scope"), TEXT("one-time isolated native GPU shader _Time.y=0 for new B1 A/B PNG; old V2 Night B1 frames were not time traceable"));
	Report->SetStringField(TEXT("sealed_v2_fixture_sha256"), SealedV2ExpectedSha);
	Report->SetStringField(TEXT("night_native_sha256"), NightNativeExpectedSha);
	Report->SetStringField(TEXT("night_gpu_time_before_sha256"), Fixture.IsValid() ? Fixture->GetObjectField(TEXT("night_gpu_time"))->GetStringField(TEXT("time_before_sha256")) : FString());
	Report->SetStringField(TEXT("night_gpu_time_after_sha256"), Fixture.IsValid() ? Fixture->GetObjectField(TEXT("night_gpu_time"))->GetStringField(TEXT("time_after_sha256")) : FString());
	Report->SetNumberField(TEXT("night_gpu_shader_time_y_f32"), MeasuredNightTimeY);
	Report->SetNumberField(TEXT("strict_rgb_tolerance_lsb"), 1);
	Report->SetNumberField(TEXT("strict_rgb_cases_passed"), StrictPassed);
	Report->SetNumberField(TEXT("pending_source_time_cases"), PendingSourceTime);
	Report->SetStringField(TEXT("alpha_scope"), TEXT("UE target alpha preserved; RGB-only comparison to Unity"));
	Report->SetNumberField(TEXT("http_dispatch_before"), static_cast<double>(InitialHttpDispatches));
	Report->SetNumberField(TEXT("http_dispatch_after"), static_cast<double>(FinalHttp));
	Report->SetNumberField(TEXT("expected_cases"), 42);
	Report->SetNumberField(TEXT("observed_cases"), CaseResults.Num());
	Report->SetNumberField(TEXT("input_gpu_byte_exact"), InputResults.Num());
	Report->SetBoolField(TEXT("complete"), bComplete);
	Report->SetBoolField(TEXT("passed"), bPassed); // Isolated one-time controlled 42 RGB cases only.
	Report->SetArrayField(TEXT("inputs"), Objects(InputResults));
	Report->SetArrayField(TEXT("cases"), Objects(CaseResults));
	Report->SetArrayField(TEXT("failures"), Strings(Failures));
	FString Text;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
	if (!FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json"))))
		UE_LOG(LogTemp, Error, TEXT("Gamma family raster failed to save manifest: %s"), *Directory);
	UE_LOG(LogTemp, Display, TEXT("Gamma family raster %s cases=%d inputs=%d run=%s"),
		bPassed ? TEXT("PASSED") : TEXT("FAILED"), CaseResults.Num(), InputResults.Num(), *Directory);
	FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
