#include "RokCityHallCameraMaterialAuditSubsystem.h"

#include "RokCityHallCameraMaterialProbe.h"
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
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PixelFormat.h"
#include "Misc/ScopeLock.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIBufferInitializer.h"
#include "RHIGPUReadback.h"
#include "RHITextureInitializer.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace RokHallCameraMaterialAudit
{
constexpr TCHAR ExpandedFixtureRelative[] = TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v5/expanded/manifest.json");
constexpr TCHAR BcFixtureRelative[] = TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v5/bc/manifest.json");
constexpr TCHAR OrderFixtureRelative[] = TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v5/order/manifest.json");
constexpr TCHAR SamplingFixtureRelative[] = TEXT("Saved/RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/PendingUeSamplingV3/fixture/manifest.json");
constexpr TCHAR NativeV7Sha[] = TEXT("015824bc7c98a49a61a87a5c31d75404bdb267fadff4c2447940650e94438f36");
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR SourceGeometrySha[] = TEXT("c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273");
constexpr TCHAR SourceRowSha[] = TEXT("6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e");
constexpr TCHAR NativeSha[] = TEXT("3e2d70c5a9c2a845f5e34b8e9e80fe95d2218142b9697a9d63236beb4d6ef451");
constexpr TCHAR NativeV2Sha[] = TEXT("d70a6dfb256f5a3a95f7c81ddf2def925b7ab46ea9d187b53a9a0946dcf94f9d");
constexpr TCHAR ContractSha[] = TEXT("4bac306793ef6a94cab283ab169a3d2cd912f27649f3d093df0efb6ab260df2f");
constexpr TCHAR ContractV2Sha[] = TEXT("ac20566eb179f620cb3932baa9a095cb6961ac269ce6d7755c6aa737f4e509e3");
constexpr TCHAR HallNative3Sha[] = TEXT("01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e");
constexpr TCHAR Camera2Sha[] = TEXT("417eb43964bd2009879561194e4c5a75c7808f257dc6b84f8707ed4892b0d944");
constexpr TCHAR TextureStateSha[] = TEXT("671f18f91275fc0a5a65107eff5e44857c59310873036293b99665f84c391b71");
constexpr int32 Width = 1440, Height = 810, Inputs = 5, Cases = 4;
constexpr double StartupTimeout = 45.0, GpuTimeout = 30.0;

bool ExactWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game &&
		World->GetOutermost()->GetName() == MapName &&
		FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
bool IsSha(const FString& Value)
{
	if (Value.Len() != 64) return false;
	for (TCHAR C : Value) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
	return true;
}
FString Hash(const TArray<uint8>& Bytes) { return RokTrainArmyMesh::Sha256(Bytes); }
FString HashFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? Hash(Bytes) : FString();
}
FString SafePath(const FString& Relative)
{
	if (Relative.IsEmpty() || Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) ||
		Relative.Contains(TEXT("\\")) || Relative.StartsWith(TEXT("/"))) return FString();
	if (!(Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v1/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/PendingUeSamplingV3/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallCameraMaterialProbeV7/OutputRuns/20261009_074853_813_28048/")) ||
		Relative == TEXT("Saved/RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit/20261009_063600_fff_43648_6BF2990A42D6F8E702EB118D81E8C584/manifest.json") ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v2/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v3/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v4/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallCameraMaterialGpuFixture/v5/")) ||
		Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallCameraMaterialProbeV1/OutputRuns/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallCameraMaterialProbeV2/OutputRuns/")) ||
		Relative == TEXT("Saved/UnityCityHallCameraMaterialProbeV1/hall_camera_material_contract.json") ||
		Relative == TEXT("Saved/UnityCityHallCameraMaterialProbeV2/hall_camera_material_contract_v2.json") ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallWorldProbeV1/OutputRuns/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallCameraLodProbeV1/OutputRuns/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityGammaFamilyStateProbeV2/OutputRuns/")) ||
		Relative.StartsWith(TEXT("ref/client-unity/Assets/BundleAssets/")) ||
		Relative.StartsWith(TEXT("Source/Rok/")) || Relative.StartsWith(TEXT("Source/RokRendering/")) ||
		Relative.StartsWith(TEXT("Scripts/")) || Relative.StartsWith(TEXT("Shaders/Private/")))) return FString();
	return FPaths::ProjectDir() / Relative;
}
bool Pin(const TSharedPtr<FJsonObject>& Record, TArray<uint8>* Bytes = nullptr)
{
	if (!Record.IsValid()) return false;
	const FString Path = SafePath(Record->GetStringField(TEXT("path")));
	const FString Expected = Record->GetStringField(TEXT("sha256"));
	const int64 Size = static_cast<int64>(Record->GetNumberField(TEXT("byte_count")));
	if (Path.IsEmpty() || !IsSha(Expected) || Size <= 0 || Size > 16 * 1024 * 1024) return false;
	TArray<uint8> Local;
	if (!FFileHelper::LoadFileToArray(Local, *Path) || Local.Num() != Size || Hash(Local) != Expected) return false;
	if (Bytes) *Bytes = MoveTemp(Local);
	return true;
}
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *Path) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
TArray<TSharedPtr<FJsonValue>> Objects(const TArray<TSharedPtr<FJsonObject>>& Array)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const auto& Value : Array) Out.Add(MakeShared<FJsonValueObject>(Value));
	return Out;
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Array)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& Value : Array) Out.Add(MakeShared<FJsonValueString>(Value));
	return Out;
}
bool SaveNew(const FString& Path, const TArray<uint8>& Bytes)
{
	return !IFileManager::Get().FileExists(*Path) && FFileHelper::SaveArrayToFile(Bytes, *Path);
}
FLinearColor Color(const TArray<TSharedPtr<FJsonValue>>& Values)
{
	return FLinearColor(Values[0]->AsNumber(), Values[1]->AsNumber(),
		Values[2]->AsNumber(), Values[3]->AsNumber());
}
}

struct FRokCityHallCameraMaterialAuditJob
{
	enum class EState : uint8 { Idle, UploadPending, Uploaded, ReadPending, FramePending, SamplePending, ReadyInput, ReadyFrame, ReadySample, Failed, Cancelled };
	FCriticalSection Mutex;
	EState State = EState::Idle;
	FString Error;
	TArray<TArray<uint8>> Uploads;
	FBufferRHIRef Buffers[3];
	FTextureRHIRef Textures[2];
	TUniquePtr<FRHIGPUBufferReadback> BufferReadback;
	TUniquePtr<FRHIGPUTextureReadback> TextureReadback;
	TRefCountPtr<IPooledRenderTarget> Output;
	TArray<uint8> Bytes;
	FMatrix44f Matrix = FMatrix44f::Identity;
	FMatrix44f ObjectToWorld = FMatrix44f::Identity;
	FMatrix44f ViewProjection = FMatrix44f::Identity;
	uint32 ReadSize = 0;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	int32 Pitch = 0, StagingHeight = 0;
	uint32 CompressedBlockRowBytes[2] = {0, 0};
	uint32 CompressedInitializerStride[2] = {0, 0};
	uint32 CompressedBlockRows[2] = {0, 0};
	bool bPollQueued = false;
};

bool URokCityHallCameraMaterialAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return RokHallCameraMaterialAudit::ExactWorld(Cast<UWorld>(Outer)) &&
		(FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialAudit")) ||
		 FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialBcAudit")) ||
		 FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialOrderAudit")) ||
		 FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialSamplingAudit"))) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCityHallCameraMaterialAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}
TStatId URokCityHallCameraMaterialAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityHallCameraMaterialAuditSubsystem, STATGROUP_Tickables);
}
void URokCityHallCameraMaterialAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSamplingVariant = FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialSamplingAudit"));
	bOrderVariant = bSamplingVariant || FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialOrderAudit"));
	bBcVariant = bOrderVariant || FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialBcAudit"));
	FixtureRelative = bSamplingVariant ? RokHallCameraMaterialAudit::SamplingFixtureRelative :
		bOrderVariant ? RokHallCameraMaterialAudit::OrderFixtureRelative :
		bBcVariant ? RokHallCameraMaterialAudit::BcFixtureRelative :
		RokHallCameraMaterialAudit::ExpandedFixtureRelative;
	StartSeconds = FPlatformTime::Seconds();
	RunDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		(bSamplingVariant ? TEXT("RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/CityHallMaterialSamplingGpuAudit") :
			TEXT("RokUnityMigration/BatchReview20261009AudioSettingsAndHallMaterial/CityHallCameraMaterialGpuAudit")) /
		FString::Printf(TEXT("%s_%u_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*RunDirectory) ||
		!IFileManager::Get().MakeDirectory(*RunDirectory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Hall material audit unique run directory failed: %s"), *RunDirectory);
		bFinished = true;
	}
}
void URokCityHallCameraMaterialAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("Game world ended before four material GPU frames completed"));
	ReleaseJob();
	Super::Deinitialize();
}

bool URokCityHallCameraMaterialAuditSubsystem::Prepare()
{
	using namespace RokHallCameraMaterialAudit;
	UWorld* World = GetWorld();
	if (!ExactWorld(World) || !GEngine || !GEngine->GameViewport ||
		GEngine->GameViewport->GetWorld() != World || !GEngine->GameViewport->Viewport ||
		!World->GetFirstPlayerController() || !World->GetFirstPlayerController()->IsLocalController()) return false;
	URokServerSubsystem* Server = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0)
	{
		Finish(TEXT("Local-only zero-HTTP boundary absent")); return false;
	}
	InitialHttp = Server->GetHttpDispatchAttemptCount();
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialFixtureSha="), ExpectedFixtureSha) ||
		!IsSha(ExpectedFixtureSha))
	{
		Finish(TEXT("Explicit lowercase -RokCityHallCameraMaterialFixtureSha=<64 hex> required")); return false;
	}
	FixtureSha = HashFile(FPaths::ProjectDir() / FixtureRelative);
	if (FixtureSha != ExpectedFixtureSha ||
		!ReadJson(FPaths::ProjectDir() / FixtureRelative, Fixture) ||
		Fixture->GetStringField(TEXT("schema")) != (bSamplingVariant ?
			TEXT("rok.city_hall_camera_material_gpu_sampling_fixture.v2") :
			TEXT("rok.city_hall_camera_material_gpu_fixture.v5")) ||
		Fixture->GetStringField(TEXT("variant")) != (bSamplingVariant ? TEXT("source_bc_object_vp_sampling") :
			bOrderVariant ? TEXT("source_bc_object_vp") :
			bBcVariant ? TEXT("source_bc") : TEXT("expanded_rgba")) ||
		Fixture->GetIntegerField(TEXT("width")) != Width || Fixture->GetIntegerField(TEXT("height")) != Height ||
		Fixture->GetStringField(TEXT("target_format")) != TEXT("PF_B8G8R8A8") ||
		Fixture->GetBoolField(TEXT("target_srgb")) ||
		Fixture->GetStringField(TEXT("source_key")) != TEXT("initial:5") ||
		Fixture->GetStringField(TEXT("sprite_renderer_file_id")) != TEXT("212751466141171401") ||
		Fixture->GetStringField(TEXT("source_row_sha256")) != SourceRowSha ||
		Fixture->GetStringField(TEXT("native_sha256")) != NativeSha ||
		Fixture->GetStringField(TEXT("native_contract_sha256")) != ContractSha ||
		Fixture->GetStringField(TEXT("native_v2_sha256")) != NativeV2Sha ||
		Fixture->GetStringField(TEXT("native_v2_contract_sha256")) != ContractV2Sha ||
		Fixture->GetStringField(TEXT("hall_native3_sha256")) != HallNative3Sha ||
		Fixture->GetStringField(TEXT("native_camera2_sha256")) != Camera2Sha ||
		Fixture->GetStringField(TEXT("source_geometry_sha256")) != SourceGeometrySha ||
		Fixture->GetStringField(TEXT("source_texture_state_sha256")) != TextureStateSha ||
		Fixture->GetArrayField(TEXT("inputs")).Num() != Inputs ||
		Fixture->GetArrayField(TEXT("cases")).Num() != Cases ||
		Fixture->GetBoolField(TEXT("main_view_registered")) ||
		Fixture->GetBoolField(TEXT("lod_qualified")) || Fixture->GetBoolField(TEXT("gameapp_accepted")) ||
		Fixture->GetIntegerField(TEXT("expected_http_dispatches")) != 0)
	{
		Finish(TEXT("External fixture/source identity or isolated scope differs")); return false;
	}
	const auto Matrix = Fixture->GetObjectField(TEXT("matrix"));
	if (!Pin(Matrix) || Matrix->GetIntegerField(TEXT("byte_count")) != 64 ||
		Fixture->GetStringField(TEXT("matrix_formula")) != (bOrderVariant ?
			TEXT("row-vector local * transpose(renderer_world), then world * transpose(camera_gpu_projection_rt * camera_world_to_camera); explicit clipY adapter") :
			TEXT("transpose(camera_gpu_projection_rt * camera_world_to_camera * renderer_world); row-vector VS plus explicit clipY adapter")) ||
		!Pin(Fixture->GetObjectField(TEXT("unity_object_to_clip_source"))) ||
		Fixture->GetBoolField(TEXT("unity_vp_bytes_measured")) ||
		Fixture->GetStringField(TEXT("source_order_qualification")) !=
		TEXT("source-confirmed two GPU muls; VP bytes derived from sealed P_rt*V, not measured unity_MatrixVP") ||
		!Pin(Fixture->GetObjectField(TEXT("world_matrix_row"))) ||
		!Pin(Fixture->GetObjectField(TEXT("view_projection_matrix_row"))))
	{
		Finish(TEXT("Hall source camera matrix pin/formula differs")); return false;
	}
	for (const auto& Value : Fixture->GetArrayField(TEXT("native_pins"))) if (!Pin(Value->AsObject()))
	{
		Finish(TEXT("Unity/Hall source pin differs")); return false;
	}
	for (const auto& Value : Fixture->GetArrayField(TEXT("material_pins"))) if (!Pin(Value->AsObject()))
	{
		Finish(TEXT("Source material/shader pin differs")); return false;
	}
	if (bSamplingVariant)
	{
		const auto V7 = Fixture->GetObjectField(TEXT("native_sampling_v7"));
		const auto Baseline = Fixture->GetObjectField(TEXT("baseline_gpu4_manifest"));
		if (V7->GetStringField(TEXT("path")) !=
			TEXT("Saved/UnityCityHallCameraMaterialProbeV7/OutputRuns/20261009_074853_813_28048/hall_camera_material_native_v7.json") ||
			V7->GetStringField(TEXT("sha256")) != NativeV7Sha || !Pin(V7) || !Pin(Baseline) ||
			Fixture->GetIntegerField(TEXT("sample_mode_count")) != 6 ||
			Fixture->GetIntegerField(TEXT("sample_points_per_phase")) != 2)
		{
			Finish(TEXT("Unity V7 sampling witness or dimensions differ")); return false;
		}
	}
	TSet<FString> RequiredCode = {
		TEXT("Source/Rok/RokCityHallCameraMaterialAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityHallCameraMaterialAuditSubsystem.cpp"),
		TEXT("Source/RokRendering/Public/RokCityHallCameraMaterialProbe.h"),
		TEXT("Source/RokRendering/Private/RokCityHallCameraMaterialProbe.cpp"),
		TEXT("Shaders/Private/RokCityHallCameraMaterial.usf"),
		TEXT("Scripts/PrepareRokCityHallCameraMaterialOrderFixture.py"),
		TEXT("Scripts/VerifyRokCityHallCameraMaterialOrderRuntime.py"),
		TEXT("Scripts/PrepareRokCityHallCameraMaterialBcFixture.py"),
		TEXT("Scripts/VerifyRokCityHallCameraMaterialBcRuntime.py")
	};
	if (bSamplingVariant)
	{
		RequiredCode.Add(TEXT("Shaders/Private/RokCityHallCameraMaterialDiagnostic.usf"));
		RequiredCode.Add(TEXT("Saved/RokUnityMigration/BatchReview20261009HallSamplingDiagnostics/PendingUeSamplingV3/VerifyPendingUeSamplingV3.py"));
	}
	TSet<FString> SeenCode;
	for (const auto& Value : Fixture->GetArrayField(TEXT("code_pins")))
	{
		const auto PinRecord = Value->AsObject();
		const FString Path = PinRecord->GetStringField(TEXT("path"));
		if (!RequiredCode.Contains(Path) || SeenCode.Contains(Path) || !Pin(PinRecord))
		{
			Finish(TEXT("Code pin absent/duplicate or modified")); return false;
		}
		SeenCode.Add(Path);
	}
	if (SeenCode.Num() != RequiredCode.Num()) { Finish(TEXT("Required implementation pins incomplete")); return false; }
	const TCHAR* Kinds[Inputs] = { TEXT("positions"), TEXT("uv0"), TEXT("indices"), TEXT("sprite"), TEXT("mask") };
	for (int32 Index = 0; Index < Inputs; ++Index)
	{
		const auto Item = Fixture->GetArrayField(TEXT("inputs"))[Index]->AsObject();
		if (Item->GetStringField(TEXT("kind")) != Kinds[Index] || !Pin(Item))
		{
			Finish(TEXT("Original Hall geometry/texture input pin differs")); return false;
		}
		if (Index < 3 &&
			(Item->GetStringField(TEXT("rhi_format")) != TEXT("ByteAddressBuffer") ||
			Item->GetIntegerField(TEXT("count")) != (Index == 2 ? 714 : 240) ||
			Item->GetIntegerField(TEXT("stride_bytes")) != (Index == 0 ? 12 : Index == 1 ? 16 : 4)))
		{
			Finish(TEXT("Hall source mesh type/count differs")); return false;
		}
		if (Index >= 3 &&
			(Item->GetStringField(TEXT("rhi_format")) != (bBcVariant ?
				(Index == 3 ? TEXT("PF_DXT5") : TEXT("PF_DXT1")) : TEXT("PF_B8G8R8A8")) ||
			Item->GetBoolField(TEXT("srgb")) ||
			Item->GetIntegerField(TEXT("num_mips")) != 1 ||
			Item->GetIntegerField(TEXT("width")) != (Index == 3 ? 1024 : 512) ||
			Item->GetIntegerField(TEXT("height")) != (Index == 3 ? 888 : 444) ||
			(bBcVariant ?
				(!Pin(Item->GetObjectField(TEXT("source_blocks"))) ||
				 Item->GetIntegerField(TEXT("block_size_x")) != 4 ||
				 Item->GetIntegerField(TEXT("block_size_y")) != 4 ||
				 Item->GetIntegerField(TEXT("block_bytes")) != (Index == 3 ? 16 : 8)) :
				(!Pin(Item->GetObjectField(TEXT("source_raw"))) ||
				 !Pin(Item->GetObjectField(TEXT("source_png")))))))
		{
			Finish(TEXT("Hall decoded source texture identity/format differs")); return false;
		}
	}
	const TCHAR* Phases[Cases] = { TEXT("day_black"), TEXT("day_white"), TEXT("night_black"), TEXT("night_white") };
	for (int32 Index = 0; Index < Cases; ++Index)
	{
		const auto Item = Fixture->GetArrayField(TEXT("cases"))[Index]->AsObject();
		if (Item->GetStringField(TEXT("phase")) != Phases[Index] ||
			(bSamplingVariant && !IsSha(Item->GetStringField(TEXT("positive_control_gpu4_raw_sha256")))) ||
			Item->GetStringField(TEXT("background")) != (Index % 2 ? TEXT("white") : TEXT("black")) ||
			Item->GetIntegerField(TEXT("material_queue")) != (Index < 2 ? 2995 : 2996) ||
			!Pin(Item->GetObjectField(TEXT("native_raw"))) ||
			!Pin(Item->GetObjectField(TEXT("native_png"))))
		{
			Finish(TEXT("Native day/night black/white case pin differs")); return false;
		}
	}
	bPrepared = true;
	return true;
}

void URokCityHallCameraMaterialAuditSubsystem::StartJob()
{
	using namespace RokHallCameraMaterialAudit;
	Job = MakeShared<FRokCityHallCameraMaterialAuditJob, ESPMode::ThreadSafe>();
	for (const auto& Value : Fixture->GetArrayField(TEXT("inputs")))
	{
		TArray<uint8> Bytes;
		if (!Pin(Value->AsObject(), &Bytes)) { Finish(TEXT("Pinned source input changed before upload")); return; }
		Job->Uploads.Add(MoveTemp(Bytes));
	}
	if (Job->Uploads.Num() != Inputs) { Finish(TEXT("Source input count changed")); return; }
	for (int32 Offset = 0; Offset < Job->Uploads[2].Num(); Offset += 4)
	{
		uint32 Vertex = MAX_uint32;
		FMemory::Memcpy(&Vertex, Job->Uploads[2].GetData() + Offset, 4);
		if (Vertex >= 240) { Finish(TEXT("Source triangle index out of range")); return; }
	}
	TArray<uint8> Matrix;
	if (!Pin(Fixture->GetObjectField(TEXT("matrix")), &Matrix) || Matrix.Num() != 64)
	{
		Finish(TEXT("Source-camera matrix changed before upload")); return;
	}
	FMemory::Memcpy(&Job->Matrix, Matrix.GetData(), 64);
	if (bOrderVariant)
	{
		TArray<uint8> Object, VP;
		if (!Pin(Fixture->GetObjectField(TEXT("world_matrix_row")), &Object) || Object.Num() != 64 ||
			!Pin(Fixture->GetObjectField(TEXT("view_projection_matrix_row")), &VP) || VP.Num() != 64)
		{
			Finish(TEXT("Source object/VP matrix changed before upload")); return;
		}
		FMemory::Memcpy(&Job->ObjectToWorld, Object.GetData(), 64);
		FMemory::Memcpy(&Job->ViewProjection, VP.GetData(), 64);
	}
	PendingSeconds = FPlatformTime::Seconds();
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->State = FRokCityHallCameraMaterialAuditJob::EState::UploadPending;
	}
	const bool bCompressed = bBcVariant;
	ENQUEUE_RENDER_COMMAND(RokHallMaterialUpload)([Shared, bCompressed](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::UploadPending) return;
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const auto& Bytes = Shared->Uploads[Slot];
			const FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::CreateByteAddress(
				TEXT("RokHallMaterialSourceMesh"), Bytes.Num(), 4)
				.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::SourceCopy)
				.SetInitialState(ERHIAccess::SRVMask).SetInitActionInitializer();
			FRHIBufferInitializer Init = RHICmdList.CreateBufferInitializer(Desc);
			if (Init.GetWritableDataSize() < static_cast<size_t>(Bytes.Num()))
			{
				Shared->Error = TEXT("Source mesh RHI initializer too small");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
			}
			Init.WriteData(Bytes.GetData(), Bytes.Num());
			Shared->Buffers[Slot] = Init.Finalize();
			if (!Shared->Buffers[Slot].IsValid() || Shared->Buffers[Slot]->GetSize() != Bytes.Num())
			{
				Shared->Error = TEXT("Source mesh RHI buffer creation failed");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
			}
		}
		const FIntPoint Extents[2] = { FIntPoint(1024, 888), FIntPoint(512, 444) };
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FIntPoint Size = Extents[Slot];
			const auto& Bytes = Shared->Uploads[Slot + 3];
			const EPixelFormat Format = bCompressed ? (Slot == 0 ? PF_DXT5 : PF_DXT1) : PF_B8G8R8A8;
			const FPixelFormatInfo& Pixel = GPixelFormats[Format];
			const uint32 BlockColumns = FMath::DivideAndRoundUp(static_cast<uint32>(Size.X), static_cast<uint32>(Pixel.BlockSizeX));
			const uint32 BlockRows = FMath::DivideAndRoundUp(static_cast<uint32>(Size.Y), static_cast<uint32>(Pixel.BlockSizeY));
			const uint32 RowBytes = BlockColumns * Pixel.BlockBytes;
			if ((bCompressed && (Pixel.BlockSizeX != 4 || Pixel.BlockSizeY != 4 ||
				Pixel.BlockBytes != (Slot == 0 ? 16 : 8))) ||
				Bytes.Num() != static_cast<int32>(RowBytes * BlockRows))
			{
				Shared->Error = TEXT("Source BC block layout or upload bytes differ");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
			}
			const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(
				bCompressed ? TEXT("RokHallMaterialSourceBC") : TEXT("RokHallMaterialSourceDecoded"),
				Size.X, Size.Y, Format)
				.SetFlags(ETextureCreateFlags::ShaderResource).SetNumMips(1).SetInitActionInitializer();
			FRHITextureInitializer Init = RHICmdList.CreateTextureInitializer(Desc);
			const FRHITextureSubresourceInitializer Sub = Init.GetTexture2DSubresource(0);
			if (!Sub.Data || Sub.Stride < RowBytes || Sub.Size < Sub.Stride * BlockRows)
			{
				Shared->Error = TEXT("Source texture RHI initializer invalid");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
			}
			for (uint32 Y = 0; Y < BlockRows; ++Y)
				FMemory::Memcpy(static_cast<uint8*>(Sub.Data) + Y * Sub.Stride,
					Bytes.GetData() + Y * RowBytes, RowBytes);
			Shared->CompressedBlockRowBytes[Slot] = RowBytes;
			Shared->CompressedInitializerStride[Slot] = Sub.Stride;
			Shared->CompressedBlockRows[Slot] = BlockRows;
			Shared->Textures[Slot] = Init.Finalize();
			if (!Shared->Textures[Slot].IsValid() ||
				Shared->Textures[Slot]->GetDesc().Format != Format ||
				EnumHasAnyFlags(Shared->Textures[Slot]->GetDesc().Flags, TexCreate_SRGB))
			{
				Shared->Error = TEXT("Source texture GPU descriptor differs");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
			}
		}
		Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Uploaded;
	});
}

void URokCityHallCameraMaterialAuditSubsystem::StartInputReadback()
{
	const auto Shared = Job;
	const int32 Slot = InputIndex;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::Uploaded) return;
		Shared->State = FRokCityHallCameraMaterialAuditJob::EState::ReadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallMaterialInputCopy)([Shared, Slot](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::ReadPending) return;
		Shared->CopyFrame = GFrameCounterRenderThread;
		if (Slot < 3)
		{
			Shared->ReadSize = Shared->Uploads[Slot].Num();
			FBufferRHIRef Buffer = Shared->Buffers[Slot];
			RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			Shared->BufferReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("RokHallMaterialMeshInput"));
			Shared->BufferReadback->EnqueueCopy(RHICmdList, Buffer.GetReference(), Shared->ReadSize);
			RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		}
		else
		{
			const FIntPoint Size = Slot == 3 ? FIntPoint(1024, 888) : FIntPoint(512, 444);
			FTextureRHIRef Texture = Shared->Textures[Slot - 3];
			RHICmdList.Transition(FRHITransitionInfo(Texture.GetReference(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			Shared->TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokHallMaterialTextureInput"));
			Shared->TextureReadback->EnqueueCopy(RHICmdList, Texture.GetReference(), FIntVector::ZeroValue,
				0, FIntVector(Size.X, Size.Y, 1));
			RHICmdList.Transition(FRHITransitionInfo(Texture.GetReference(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		}
	});
}

void URokCityHallCameraMaterialAuditSubsystem::RecordCompressedInput()
{
	using namespace RokHallCameraMaterialAudit;
	if (!bBcVariant || !Job || InputIndex < 3 || InputIndex >= Inputs) return;
	const int32 TextureSlot = InputIndex - 3;
	const auto Record = Fixture->GetArrayField(TEXT("inputs"))[InputIndex]->AsObject();
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	bool bValid = false;
	{
		FScopeLock Lock(&Job->Mutex);
		const EPixelFormat Expected = TextureSlot == 0 ? PF_DXT5 : PF_DXT1;
		const FTextureRHIRef Texture = Job->Textures[TextureSlot];
		const int32 WidthNow = TextureSlot == 0 ? 1024 : 512;
		const int32 HeightNow = TextureSlot == 0 ? 888 : 444;
		const FPixelFormatInfo& Pixel = GPixelFormats[Expected];
		const uint32 Columns = FMath::DivideAndRoundUp(static_cast<uint32>(WidthNow), static_cast<uint32>(Pixel.BlockSizeX));
		const uint32 Rows = FMath::DivideAndRoundUp(static_cast<uint32>(HeightNow), static_cast<uint32>(Pixel.BlockSizeY));
		const uint32 RowBytes = Columns * Pixel.BlockBytes;
		bValid = Job->State == FRokCityHallCameraMaterialAuditJob::EState::Uploaded && Texture.IsValid() &&
			Texture->GetDesc().Format == Expected && Texture->GetDesc().NumMips == 1 &&
			!EnumHasAnyFlags(Texture->GetDesc().Flags, TexCreate_SRGB) &&
			Job->Uploads[InputIndex].Num() == static_cast<int32>(RowBytes * Rows) &&
			Job->CompressedBlockRowBytes[TextureSlot] == RowBytes &&
			Job->CompressedBlockRows[TextureSlot] == Rows &&
			Job->CompressedInitializerStride[TextureSlot] >= RowBytes;
		Result->SetNumberField(TEXT("block_row_bytes"), RowBytes);
		Result->SetNumberField(TEXT("initializer_stride_bytes"), Job->CompressedInitializerStride[TextureSlot]);
		Result->SetNumberField(TEXT("block_rows"), Rows);
		Result->SetStringField(TEXT("gpu_texture_format"), TextureSlot == 0 ? TEXT("PF_DXT5") : TEXT("PF_DXT1"));
	}
	if (!bValid) { Finish(TEXT("Compressed GPU texture upload/layout witness differs")); return; }
	Result->SetStringField(TEXT("kind"), Record->GetStringField(TEXT("kind")));
	Result->SetStringField(TEXT("source_path"), Record->GetStringField(TEXT("path")));
	Result->SetStringField(TEXT("source_sha256"), Record->GetStringField(TEXT("sha256")));
	Result->SetNumberField(TEXT("byte_count"), Job->Uploads[InputIndex].Num());
	Result->SetBoolField(TEXT("compressed_source_hash_verified"), true);
	Result->SetBoolField(TEXT("gpu_descriptor_verified"), true);
	Result->SetBoolField(TEXT("gpu_byte_exact"), false); // UE5.8 staging readback rejects BC in D3D MapStagingSurface.
	Result->SetStringField(TEXT("sampling_proof"), TEXT("Pending four private-GPU native RGBA frame comparisons"));
	InputResults.Add(Result); ++InputIndex;
}

void URokCityHallCameraMaterialAuditSubsystem::StartFrame()
{
	using namespace RokHallCameraMaterialAudit;
	const auto Shared = Job;
	const int32 Case = CaseIndex;
	const auto Record = Fixture->GetArrayField(TEXT("cases"))[Case]->AsObject();
	const FLinearColor Team1 = Color(Record->GetArrayField(TEXT("team1")));
	const FLinearColor Team2 = Color(Record->GetArrayField(TEXT("team2")));
	const FLinearColor Night3 = Color(Record->GetArrayField(TEXT("night_color3")));
	const bool bCompressed = bBcVariant;
	const bool bTwoStage = bOrderVariant;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::Uploaded) return;
		Shared->State = FRokCityHallCameraMaterialAuditJob::EState::FramePending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallMaterialFrame)([Shared, Case, Team1, Team2, Night3, bCompressed, bTwoStage](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::FramePending) return;
		FRokCityHallCameraMaterialProbeInput Input;
		Input.SourceKey = FName(TEXT("initial:5"));
		Input.Positions = Shared->Buffers[0]; Input.UV0 = Shared->Buffers[1]; Input.Indices = Shared->Buffers[2];
		Input.SpriteTexture = Shared->Textures[0]; Input.MaskTexture = Shared->Textures[1];
		Input.bSourceCompressed = bCompressed;
		Input.SourceLocalToClip = Shared->Matrix;
		Input.bObjectThenVP = bTwoStage;
		Input.SourceObjectToWorld = Shared->ObjectToWorld;
		Input.SourceViewProjection = Shared->ViewProjection;
		Input.Team1 = Team1; Input.Team2 = Team2; Input.NightColor3 = Night3;
		Input.SpriteColor = FLinearColor::White; Input.RendererTint = FLinearColor::White;
		Input.CityTransparency = 1.0f; Input.ControlledTimeY = 0.0f;
		Input.bNight = Case >= 2; Input.bWhiteBackground = (Case & 1) != 0;
		if (!FRokCityHallCameraMaterialProbe::Render(RHICmdList, Input, Shared->Output, Shared->Error))
		{
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
		}
		const FRHITextureDesc& Desc = Shared->Output->GetRHI()->GetDesc();
		if (Desc.Extent != FIntPoint(Width, Height) || Desc.Format != PF_B8G8R8A8 ||
			Desc.NumMips != 1 || Desc.NumSamples != 1 || EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB))
		{
			Shared->Error = TEXT("Hall output RT descriptor differs");
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
		}
		Shared->TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokHallMaterial1440BGRA"));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->TextureReadback->EnqueueCopy(RHICmdList, Shared->Output->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(Width, Height, 1));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCityHallCameraMaterialAuditSubsystem::StartSample()
{
	using namespace RokHallCameraMaterialAudit;
	const auto Shared = Job;
	const int32 Case = SampleIndex / 6;
	const uint32 Mode = static_cast<uint32>(SampleIndex % 6 + 1);
	const auto Record = Fixture->GetArrayField(TEXT("cases"))[Case]->AsObject();
	const FLinearColor Team1 = Color(Record->GetArrayField(TEXT("team1")));
	const FLinearColor Team2 = Color(Record->GetArrayField(TEXT("team2")));
	const FLinearColor Night3 = Color(Record->GetArrayField(TEXT("night_color3")));
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::Uploaded) return;
		Shared->State = FRokCityHallCameraMaterialAuditJob::EState::SamplePending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallMaterialSample)([Shared, Case, Mode, Team1, Team2, Night3](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallCameraMaterialAuditJob::EState::SamplePending) return;
		FRokCityHallCameraMaterialProbeInput Input;
		Input.SourceKey = FName(TEXT("initial:5"));
		Input.Positions = Shared->Buffers[0]; Input.UV0 = Shared->Buffers[1]; Input.Indices = Shared->Buffers[2];
		Input.SpriteTexture = Shared->Textures[0]; Input.MaskTexture = Shared->Textures[1];
		Input.bSourceCompressed = true; Input.bObjectThenVP = true;
		Input.SourceLocalToClip = Shared->Matrix;
		Input.SourceObjectToWorld = Shared->ObjectToWorld;
		Input.SourceViewProjection = Shared->ViewProjection;
		Input.Team1 = Team1; Input.Team2 = Team2; Input.NightColor3 = Night3;
		Input.SpriteColor = FLinearColor::White; Input.RendererTint = FLinearColor::White;
		Input.CityTransparency = 1.0f; Input.ControlledTimeY = 0.0f;
		Input.bNight = Case >= 2; Input.bWhiteBackground = (Case & 1) != 0;
		if (!FRokCityHallCameraMaterialProbe::RenderDiagnostic(RHICmdList, Input, Mode,
			Shared->Output, Shared->Error))
		{
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
		}
		const FRHITextureDesc& Desc = Shared->Output->GetRHI()->GetDesc();
		if (Desc.Extent != FIntPoint(Width, Height) || Desc.Format != PF_A32B32G32R32F ||
			Desc.NumMips != 1 || Desc.NumSamples != 1 || EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB))
		{
			Shared->Error = TEXT("Hall diagnostic float output descriptor differs");
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
		}
		Shared->TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokHallMaterial1440FloatWitness"));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->TextureReadback->EnqueueCopy(RHICmdList, Shared->Output->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(Width, Height, 1));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCityHallCameraMaterialAuditSubsystem::Poll()
{
	using namespace RokHallCameraMaterialAudit;
	const auto Shared = Job;
	const int32 Slot = InputIndex;
	{
		FScopeLock Lock(&Shared->Mutex);
		if ((Shared->State != FRokCityHallCameraMaterialAuditJob::EState::ReadPending &&
			Shared->State != FRokCityHallCameraMaterialAuditJob::EState::FramePending &&
			Shared->State != FRokCityHallCameraMaterialAuditJob::EState::SamplePending) || Shared->bPollQueued) return;
		Shared->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokHallMaterialPoll)([Shared, Slot](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->bPollQueued = false;
		if (Shared->State == FRokCityHallCameraMaterialAuditJob::EState::ReadPending && Slot < 3)
		{
			if (!Shared->BufferReadback || !Shared->BufferReadback->IsReady()) return;
			const void* Mapped = Shared->BufferReadback->Lock(Shared->ReadSize);
			if (!Mapped) { Shared->Error = TEXT("GPU source buffer readback map failed");
				Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return; }
			Shared->Bytes.SetNumUninitialized(Shared->ReadSize);
			FMemory::Memcpy(Shared->Bytes.GetData(), Mapped, Shared->ReadSize);
			Shared->BufferReadback->Unlock(); Shared->BufferReadback.Reset();
			Shared->ReadyFrame = GFrameCounterRenderThread;
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::ReadyInput;
			return;
		}
		if (!Shared->TextureReadback || !Shared->TextureReadback->IsReady()) return;
		const bool bSample = Shared->State == FRokCityHallCameraMaterialAuditJob::EState::SamplePending;
		const int32 WidthNow = (Shared->State == FRokCityHallCameraMaterialAuditJob::EState::FramePending || bSample)
			? Width : Slot == 3 ? 1024 : 512;
		const int32 HeightNow = (Shared->State == FRokCityHallCameraMaterialAuditJob::EState::FramePending || bSample)
			? Height : Slot == 3 ? 888 : 444;
		int32 Pitch = 0, StageHeight = 0;
		const void* Mapped = Shared->TextureReadback->Lock(Pitch, &StageHeight);
		if (!Mapped || Pitch < WidthNow || StageHeight < HeightNow)
		{
			if (Mapped) Shared->TextureReadback->Unlock();
			Shared->Error = TEXT("GPU material texture/frame map or row pitch invalid");
			Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Failed; return;
		}
		const int32 BytesPerPixel = bSample ? 16 : 4;
		Shared->Bytes.SetNumUninitialized(WidthNow * HeightNow * BytesPerPixel);
		for (int32 Y = 0; Y < HeightNow; ++Y)
			FMemory::Memcpy(Shared->Bytes.GetData() + Y * WidthNow * BytesPerPixel,
				static_cast<const uint8*>(Mapped) + Y * Pitch * BytesPerPixel, WidthNow * BytesPerPixel);
		Shared->TextureReadback->Unlock(); Shared->TextureReadback.Reset();
		Shared->Pitch = Pitch; Shared->StagingHeight = StageHeight;
		Shared->ReadyFrame = GFrameCounterRenderThread;
		if (Shared->State == FRokCityHallCameraMaterialAuditJob::EState::FramePending || bSample)
		{
			Shared->Output.SafeRelease();
			Shared->State = bSample ? FRokCityHallCameraMaterialAuditJob::EState::ReadySample :
				FRokCityHallCameraMaterialAuditJob::EState::ReadyFrame;
		}
		else Shared->State = FRokCityHallCameraMaterialAuditJob::EState::ReadyInput;
	});
}

void URokCityHallCameraMaterialAuditSubsystem::RecordReady()
{
	using namespace RokHallCameraMaterialAudit;
	TArray<uint8> Bytes;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	int32 Pitch = 0, StageHeight = 0;
	FRokCityHallCameraMaterialAuditJob::EState State;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State;
		if (State != FRokCityHallCameraMaterialAuditJob::EState::ReadyInput &&
			State != FRokCityHallCameraMaterialAuditJob::EState::ReadyFrame &&
			State != FRokCityHallCameraMaterialAuditJob::EState::ReadySample) return;
		Bytes = MoveTemp(Job->Bytes);
		CopyFrame = Job->CopyFrame; ReadyFrame = Job->ReadyFrame;
		Pitch = Job->Pitch; StageHeight = Job->StagingHeight;
		Job->State = FRokCityHallCameraMaterialAuditJob::EState::Uploaded;
	}
	if (State == FRokCityHallCameraMaterialAuditJob::EState::ReadySample)
	{
		const int32 PhaseIndex = SampleIndex / 6;
		const int32 Mode = SampleIndex % 6 + 1;
		const FString Phase = Fixture->GetArrayField(TEXT("cases"))[PhaseIndex]->AsObject()->GetStringField(TEXT("phase"));
		const FString RawName = FString::Printf(TEXT("sample_%s_mode%d.top_left.rgba32f.bin"), *Phase, Mode);
		if (Bytes.Num() != Width * Height * 16 || !SaveNew(RunDirectory / RawName, Bytes))
		{
			Finish(TEXT("GPU Hall float witness raw save failed")); return;
		}
		TArray<TSharedPtr<FJsonValue>> Points;
		for (int32 Y : {127, 138})
		{
			const int32 X = 598;
			uint32 Bits[4] = {};
			FMemory::Memcpy(Bits, Bytes.GetData() + (Y * Width + X) * 16, 16);
			auto Point = MakeShared<FJsonObject>();
			Point->SetNumberField(TEXT("x"), X); Point->SetNumberField(TEXT("y"), Y);
			TArray<TSharedPtr<FJsonValue>> Channels;
			for (uint32 Bit : Bits)
			{
				float Value = 0.0f; FMemory::Memcpy(&Value, &Bit, 4);
				if (!FMath::IsFinite(Value)) { Finish(TEXT("Nonfinite Hall GPU float witness")); return; }
				Channels.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Bit)));
			}
			Point->SetArrayField(TEXT("rgba_f32_bits_u32"), Channels);
			Points.Add(MakeShared<FJsonValueObject>(Point));
		}
		auto Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("phase"), Phase); Result->SetNumberField(TEXT("mode"), Mode);
		Result->SetStringField(TEXT("gpu_raw_file"), RawName);
		Result->SetStringField(TEXT("gpu_raw_sha256"), Hash(Bytes));
		Result->SetNumberField(TEXT("byte_count"), Bytes.Num());
		Result->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
		Result->SetNumberField(TEXT("staging_buffer_height"), StageHeight);
		Result->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
		Result->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
		Result->SetArrayField(TEXT("fixed_pixel_points"), Points);
		SampleResults.Add(Result); ++SampleIndex;
		return;
	}
	if (State == FRokCityHallCameraMaterialAuditJob::EState::ReadyInput)
	{
		const auto Record = Fixture->GetArrayField(TEXT("inputs"))[InputIndex]->AsObject();
		const FString Kind = Record->GetStringField(TEXT("kind"));
		const FString Name = Kind + TEXT(".gpu.bin");
		const bool Exact = Bytes == Job->Uploads[InputIndex] && Hash(Bytes) == Record->GetStringField(TEXT("sha256"));
		if (!SaveNew(RunDirectory / Name, Bytes) || !Exact)
		{
			Finish(TEXT("GPU input readback differs from pinned original bytes: ") + Kind); return;
		}
		auto Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("kind"), Kind);
		Result->SetStringField(TEXT("source_path"), Record->GetStringField(TEXT("path")));
		Result->SetStringField(TEXT("source_sha256"), Record->GetStringField(TEXT("sha256")));
		Result->SetStringField(TEXT("gpu_file"), Name);
		Result->SetStringField(TEXT("gpu_sha256"), Hash(Bytes));
		Result->SetNumberField(TEXT("byte_count"), Bytes.Num());
		Result->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
		Result->SetNumberField(TEXT("staging_buffer_height"), StageHeight);
		Result->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
		Result->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
		Result->SetBoolField(TEXT("gpu_byte_exact"), true);
		InputResults.Add(Result); ++InputIndex;
		return;
	}
	const auto Case = Fixture->GetArrayField(TEXT("cases"))[CaseIndex]->AsObject();
	const FString Phase = Case->GetStringField(TEXT("phase"));
	const FString RawName = Phase + TEXT(".gpu.bgra8.bin");
	const FString PngName = Phase + TEXT(".gpu.png");
	if (bSamplingVariant && Hash(Bytes) != Case->GetStringField(TEXT("positive_control_gpu4_raw_sha256")))
	{
		Finish(TEXT("Original Hall RGBA draw differs from pinned GPU4 positive control: ") + Phase); return;
	}
	if (Bytes.Num() != Width * Height * 4 || !SaveNew(RunDirectory / RawName, Bytes))
	{
		Finish(TEXT("GPU Hall source-camera frame save failed")); return;
	}
	TArray<FColor> Colors;
	Colors.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Colors.GetData(), Bytes.GetData(), Bytes.Num());
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Width, Height,
		TArrayView64<const FColor>(Colors.GetData(), Colors.Num()), Png);
	static constexpr uint8 Magic[8] = {137,80,78,71,13,10,26,10};
	if (Png.Num() < 8 || FMemory::Memcmp(Png.GetData(), Magic, 8) ||
		IFileManager::Get().FileExists(*(RunDirectory / PngName)) ||
		!FFileHelper::SaveArrayToFile(Png, *(RunDirectory / PngName)))
	{
		Finish(TEXT("GPU Hall true PNG encoding/save failed")); return;
	}
	const auto Native = Case->GetObjectField(TEXT("native_raw"));
	TArray<uint8> NativeRgba;
	if (!Pin(Native, &NativeRgba) || NativeRgba.Num() != Bytes.Num())
	{
		Finish(TEXT("Pinned Unity source-camera frame changed")); return;
	}
	int32 MaxRgb = 0, MaxAlpha = 0, RgbOverTwo = 0, SourceNonOpaque = 0;
	int32 Coverage = 0;
	for (int32 Pixel = 0; Pixel < Width * Height; ++Pixel)
	{
		const uint8* Source = NativeRgba.GetData() + Pixel * 4;
		const FColor& Ue = Colors[Pixel];
		const int32 Delta = FMath::Max3(FMath::Abs(int32(Source[0])-int32(Ue.R)),
			FMath::Abs(int32(Source[1])-int32(Ue.G)), FMath::Abs(int32(Source[2])-int32(Ue.B)));
		MaxRgb = FMath::Max(MaxRgb, Delta);
		MaxAlpha = FMath::Max(MaxAlpha, FMath::Abs(int32(Source[3])-int32(Ue.A)));
		RgbOverTwo += Delta > 2;
		SourceNonOpaque += Source[3] != 255;
		Coverage += Source[0] != (CaseIndex % 2 ? 255 : 0) ||
			Source[1] != (CaseIndex % 2 ? 255 : 0) || Source[2] != (CaseIndex % 2 ? 255 : 0);
	}
	const bool Passed = MaxRgb <= 2 && MaxAlpha <= 2 && RgbOverTwo == 0 &&
		SourceNonOpaque > 1000 && Coverage > 80000;
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("phase"), Phase);
	Result->SetStringField(TEXT("native_raw_sha256"), Native->GetStringField(TEXT("sha256")));
	Result->SetStringField(TEXT("gpu_raw_file"), RawName);
	Result->SetStringField(TEXT("gpu_raw_sha256"), Hash(Bytes));
	TArray<uint8> PngBytes;
	PngBytes.Append(Png.GetData(), static_cast<int32>(Png.Num()));
	Result->SetStringField(TEXT("gpu_png_file"), PngName);
	Result->SetStringField(TEXT("gpu_png_sha256"), Hash(PngBytes));
	Result->SetNumberField(TEXT("max_rgb_delta"), MaxRgb);
	Result->SetNumberField(TEXT("max_alpha_delta"), MaxAlpha);
	Result->SetNumberField(TEXT("rgb_pixels_over_2_lsb"), RgbOverTwo);
	Result->SetNumberField(TEXT("native_nonopaque_alpha_pixels"), SourceNonOpaque);
	Result->SetNumberField(TEXT("native_rgb_coverage_pixels"), Coverage);
	Result->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
	Result->SetNumberField(TEXT("staging_buffer_height"), StageHeight);
	Result->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
	Result->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
	Result->SetBoolField(TEXT("rgba_passed"), Passed);
	FrameResults.Add(Result);
	if (!Passed) Failures.Add(TEXT("Source-camera material RGBA mismatch: ") + Phase);
	++CaseIndex;
}

void URokCityHallCameraMaterialAuditSubsystem::Tick(float)
{
	using namespace RokHallCameraMaterialAudit;
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (!Prepare() && !bFinished && Now - StartSeconds > StartupTimeout)
			Finish(TEXT("Game world/viewport startup timed out"));
		return;
	}
	if (CaseIndex == Cases && (!bSamplingVariant || SampleIndex == Cases * 6)) { Finish(); return; }
	if (!Job) { StartJob(); return; }
	FRokCityHallCameraMaterialAuditJob::EState State;
	FString Error;
	{
		FScopeLock Lock(&Job->Mutex); State = Job->State; Error = Job->Error;
	}
	if (State == FRokCityHallCameraMaterialAuditJob::EState::Failed)
	{
		Finish(TEXT("GPU Hall material job failed: ") + Error); return;
	}
	if (State == FRokCityHallCameraMaterialAuditJob::EState::ReadyInput ||
		State == FRokCityHallCameraMaterialAuditJob::EState::ReadyFrame ||
		State == FRokCityHallCameraMaterialAuditJob::EState::ReadySample)
	{
		RecordReady(); return;
	}
	if (State == FRokCityHallCameraMaterialAuditJob::EState::ReadPending ||
		State == FRokCityHallCameraMaterialAuditJob::EState::FramePending ||
		State == FRokCityHallCameraMaterialAuditJob::EState::SamplePending) Poll();
	if (Now - PendingSeconds > GpuTimeout)
	{
		Finish(TEXT("GPU Hall material upload/readback timed out")); return;
	}
	if (State != FRokCityHallCameraMaterialAuditJob::EState::Uploaded) return;
	if (InputIndex < Inputs)
	{
		if (bBcVariant && InputIndex >= 3) RecordCompressedInput();
		else StartInputReadback();
	}
	else if (CaseIndex < Cases) StartFrame();
	else StartSample();
}

void URokCityHallCameraMaterialAuditSubsystem::ReleaseJob()
{
	if (!Job) return;
	const auto Shared = Job;
	ENQUEUE_RENDER_COMMAND(RokHallMaterialRelease)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->BufferReadback.Reset(); Shared->TextureReadback.Reset(); Shared->Output.SafeRelease();
		for (auto& Buffer : Shared->Buffers) Buffer.SafeRelease();
		for (auto& Texture : Shared->Textures) Texture.SafeRelease();
		Shared->Uploads.Empty(); Shared->State = FRokCityHallCameraMaterialAuditJob::EState::Cancelled;
	});
	FlushRenderingCommands(); Job.Reset();
}

void URokCityHallCameraMaterialAuditSubsystem::Finish(const FString& Failure)
{
	using namespace RokHallCameraMaterialAudit;
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	ReleaseJob(); bFinished = true;
	URokServerSubsystem* Server = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	const uint64 FinalHttp = Server ? Server->GetHttpDispatchAttemptCount() : MAX_uint64;
	if (!Server || !Server->IsLocalOnlyRun() || InitialHttp != 0 || FinalHttp != 0)
		Failures.Add(TEXT("Local-only zero-HTTP boundary changed"));
	const bool SamplingComplete = bSamplingVariant && SampleIndex == Cases * 6 &&
		SampleResults.Num() == Cases * 6;
	const bool Complete = bPrepared && InputResults.Num() == Inputs && FrameResults.Num() == Cases &&
		CaseIndex == Cases && (!bSamplingVariant || SamplingComplete);
	bool InputsExact = true, FramesExact = true;
	for (int32 Index = 0; Index < InputResults.Num(); ++Index)
	{
		const auto& Input = InputResults[Index];
		InputsExact &= bBcVariant && Index >= 3 ?
			(Input->GetBoolField(TEXT("compressed_source_hash_verified")) &&
			 Input->GetBoolField(TEXT("gpu_descriptor_verified")) &&
			 !Input->GetBoolField(TEXT("gpu_byte_exact"))) : Input->GetBoolField(TEXT("gpu_byte_exact"));
	}
	for (const auto& Frame : FrameResults) FramesExact &= Frame->GetBoolField(TEXT("rgba_passed"));
	const bool Passed = Complete && InputsExact && FramesExact && Failures.IsEmpty();
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), bSamplingVariant ?
		TEXT("rok.city_hall_camera_material_gpu_sampling_runtime.v2") :
		TEXT("rok.city_hall_camera_material_gpu_runtime.v3"));
	Report->SetStringField(TEXT("variant"), bSamplingVariant ? TEXT("source_bc_object_vp_sampling") :
		bOrderVariant ? TEXT("source_bc_object_vp") :
		bBcVariant ? TEXT("source_bc") : TEXT("expanded_rgba"));
	Report->SetBoolField(TEXT("two_stage_object_vp"), bOrderVariant);
	Report->SetBoolField(TEXT("unity_vp_bytes_measured"), false);
	Report->SetStringField(TEXT("scope"), TEXT("Hall source mesh/material and configured source camera on isolated 1440x810 GPU target; source GameApp behavior/LOD/main view unqualified"));
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("fixture_path"), FixtureRelative);
	Report->SetStringField(TEXT("fixture_sha256"), FixtureSha);
	Report->SetStringField(TEXT("expected_fixture_sha256"), ExpectedFixtureSha);
	Report->SetStringField(TEXT("native_sha256"), NativeSha);
	Report->SetStringField(TEXT("native_v2_sha256"), NativeV2Sha);
	Report->SetStringField(TEXT("source_geometry_sha256"), SourceGeometrySha);
	Report->SetBoolField(TEXT("opt_in_flag"), bSamplingVariant ?
		FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialSamplingAudit")) : bOrderVariant ?
		FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialOrderAudit")) :
		bBcVariant ? FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialBcAudit")) :
		FParse::Param(FCommandLine::Get(), TEXT("RokCityHallCameraMaterialAudit")));
	Report->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Report->SetBoolField(TEXT("scene_mutated"), false);
	Report->SetBoolField(TEXT("main_view_registered"), false);
	Report->SetBoolField(TEXT("gameapp_accepted"), false);
	Report->SetBoolField(TEXT("lod_accepted"), false);
	Report->SetNumberField(TEXT("http_dispatch_before"), static_cast<double>(InitialHttp));
	Report->SetNumberField(TEXT("http_dispatch_after"), static_cast<double>(FinalHttp));
	Report->SetNumberField(TEXT("gpu_byte_exact_inputs"), bBcVariant ? 3 : InputResults.Num());
	Report->SetNumberField(TEXT("compressed_gpu_descriptor_inputs"), bBcVariant ? 2 : 0);
	Report->SetBoolField(TEXT("compressed_gpu_sampling_verified_by_frames"), bBcVariant && Complete && FramesExact);
	Report->SetNumberField(TEXT("rgba_matched_frames"), FrameResults.Num());
	Report->SetNumberField(TEXT("rgb_tolerance_lsb"), 2);
	Report->SetNumberField(TEXT("alpha_tolerance_lsb"), 2);
	Report->SetStringField(TEXT("texture_input_rows"), bBcVariant ?
		TEXT("Unity DXT5/DXT1 raw block rows in original import order; block-row pitch copied, no row flip") :
		TEXT("Unity bottom-left RGBA8 channel-swapped BGRA8, not vertically flipped"));
	Report->SetStringField(TEXT("frame_output_rows"), TEXT("GPU staging top-left BGRA8"));
	Report->SetBoolField(TEXT("complete"), Complete);
	Report->SetBoolField(TEXT("passed"), Passed);
	if (bSamplingVariant)
	{
		Report->SetStringField(TEXT("native_sampling_v7_sha256"), NativeV7Sha);
		Report->SetBoolField(TEXT("sampling_capture_complete"), SamplingComplete);
		Report->SetStringField(TEXT("sampling_float_format"), TEXT("PF_A32B32G32R32F private linear full RT; raw 4xfloat32 per top-left pixel"));
		Report->SetNumberField(TEXT("sampling_mode_count"), 6);
		Report->SetNumberField(TEXT("sampling_points_per_phase"), 2);
		Report->SetArrayField(TEXT("samples"), Objects(SampleResults));
	}
	Report->SetArrayField(TEXT("inputs"), Objects(InputResults));
	Report->SetArrayField(TEXT("frames"), Objects(FrameResults));
	Report->SetArrayField(TEXT("failures"), Strings(Failures));
	FString Text;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
	const bool Saved = !IFileManager::Get().FileExists(*(RunDirectory / TEXT("manifest.json"))) &&
		FFileHelper::SaveStringToFile(Text, *(RunDirectory / TEXT("manifest.json")));
	UE_LOG(LogTemp, Display, TEXT("Hall source-camera material GPU audit %s inputs=%d frames=%d run=%s"),
		Passed && Saved ? TEXT("PASSED") : TEXT("FAILED"), InputResults.Num(), FrameResults.Num(), *RunDirectory);
	FPlatformMisc::RequestExitWithStatus(false, Passed && Saved ? 0 : 1);
}
