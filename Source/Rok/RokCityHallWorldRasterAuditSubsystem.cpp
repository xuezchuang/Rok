#include "RokCityHallWorldRasterAuditSubsystem.h"

#include "RokCitySourceGeometryProbe.h"
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
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIBufferInitializer.h"
#include "RHIGPUReadback.h"
#include "RenderTargetPool.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace RokHallWorldRasterAudit
{
constexpr TCHAR FixtureRelative[] = TEXT("Saved/RokUnityMigration/CityHallWorldRasterGpuFixture/v1/manifest.json");
constexpr TCHAR SourceRelative[] = TEXT("Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json");
constexpr TCHAR SourceSha[] = TEXT("c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273");
constexpr TCHAR HallNative3Sha[] = TEXT("01b924d764395708e20766834595fa7943f1ec61c70215b6058914c2cc80ab5e");
constexpr TCHAR HallContractSha[] = TEXT("60bfd58cc2c4897a49cb4440474825c641cc30685e40dc1fa6e404063fd56dce");
constexpr TCHAR NativeRasterContractSha[] = TEXT("b18e03e5caa2ac6a8e06d31756fda318cebd0c05c8e7baeb9ec23fc2b404591d");
constexpr TCHAR SourceRowSha[] = TEXT("6b2844db3726051827252fe03f5892b2d99d54675c3d9e673f78a05197cb2a5e");
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr int32 Extent = 128, CaseCount = 2, SourceBufferCount = 3, BufferCount = 5;
constexpr double StartupTimeout = 45.0, GpuTimeout = 30.0;

bool ExactWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return World && World->WorldType == EWorldType::Game && World->GetOutermost()->GetName() == MapName &&
		FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
FString Hash(const TArray<uint8>& Bytes) { return RokTrainArmyMesh::Sha256(Bytes); }
FString HashFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? Hash(Bytes) : FString();
}
FString SafeInput(const FString& Relative)
{
	if (Relative.IsEmpty() || Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) ||
		Relative.Contains(TEXT("\\")) || Relative.StartsWith(TEXT("/")) ||
		!(Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/")) ||
		  Relative.StartsWith(TEXT("Saved/RokUnityMigration/CityHallWorldRasterGpuFixture/v1/")) ||
		  Relative.StartsWith(TEXT("Saved/UnityCityHallWorldProbeV1/")) ||
		  Relative.StartsWith(TEXT("Saved/UnityCityHallWorldRasterProbeV1/")) ||
		  Relative.StartsWith(TEXT("Saved/RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/")))) return FString();
	return FPaths::ProjectDir() / Relative;
}
FString SafePinned(const FString& Relative)
{
	if (Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) ||
		Relative.Contains(TEXT("\\")) || Relative.StartsWith(TEXT("/"))) return FString();
	if (Relative.StartsWith(TEXT("Source/Rok/")) || Relative.StartsWith(TEXT("Source/RokRendering/")) ||
		Relative.StartsWith(TEXT("Shaders/Private/")) || Relative.StartsWith(TEXT("Scripts/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallWorldProbeV1/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCityHallWorldRasterProbeV1/")))
		return FPaths::ProjectDir() / Relative;
	return FString();
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
	for (const auto& Value : Values) Out.Add(MakeShared<FJsonValueObject>(Value));
	return Out;
}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const FString& Value : Values) Out.Add(MakeShared<FJsonValueString>(Value));
	return Out;
}
bool LoadPinned(const TSharedPtr<FJsonObject>& Record, TArray<uint8>& Out)
{
	const FString Relative = Record->GetStringField(TEXT("path"));
	const FString Path = SafeInput(Relative);
	const int32 ByteCount = Record->GetIntegerField(TEXT("byte_count"));
	return !Path.IsEmpty() && ByteCount > 0 && ByteCount <= 8 * 1024 * 1024 &&
		FFileHelper::LoadFileToArray(Out, *Path) && Out.Num() == ByteCount &&
		Hash(Out) == Record->GetStringField(TEXT("sha256"));
}
bool SameBinary(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
{
	return A.IsValid() && B.IsValid() && A->GetStringField(TEXT("path")) == B->GetStringField(TEXT("path")) &&
		A->GetStringField(TEXT("sha256")) == B->GetStringField(TEXT("sha256")) &&
		A->GetIntegerField(TEXT("count")) == B->GetIntegerField(TEXT("count")) &&
		A->GetIntegerField(TEXT("stride_bytes")) == B->GetIntegerField(TEXT("stride_bytes")) &&
		A->GetIntegerField(TEXT("byte_count")) == B->GetIntegerField(TEXT("byte_count"));
}
bool SaveNew(const FString& Path, const TArray<uint8>& Bytes)
{
	return !IFileManager::Get().FileExists(*Path) && FFileHelper::SaveArrayToFile(Bytes, *Path);
}
bool IsSha256(const FString& Value)
{
	if (Value.Len() != 64) return false;
	for (const TCHAR Ch : Value)
		if (!((Ch >= TEXT('0') && Ch <= TEXT('9')) ||
			(Ch >= TEXT('a') && Ch <= TEXT('f')) || (Ch >= TEXT('A') && Ch <= TEXT('F')))) return false;
	return true;
}
}

struct FRokCityHallWorldRasterAuditJob
{
	enum class EState : uint8 { Idle, UploadPending, Uploaded, BufferPending, FramePending, ReadyBuffer, ReadyFrame, Failed, Cancelled };
	FCriticalSection Mutex;
	EState State = EState::Idle;
	FString Error;
	TArray<FBufferRHIRef> Buffers;
	TArray<TArray<uint8>> Uploads;
	TUniquePtr<FRHIGPUBufferReadback> BufferReadback;
	TUniquePtr<FRHIGPUTextureReadback> TextureReadback;
	TRefCountPtr<IPooledRenderTarget> Output;
	TArray<uint8> Bytes;
	FMatrix44f Matrices[2] = { FMatrix44f::Identity, FMatrix44f::Identity };
	FName SourceKey;
	uint32 VertexCount = 0, IndexCount = 0, PositionStride = 0, UVStride = 0;
	uint32 ReadSize = 0;
	int32 Pitch = 0, Height = 0;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	bool bPollQueued = false;
};

bool URokCityHallWorldRasterAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return RokHallWorldRasterAudit::ExactWorld(Cast<UWorld>(Outer)) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokCityHallWorldRasterAudit")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCityHallWorldRasterAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCityHallWorldRasterAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityHallWorldRasterAuditSubsystem, STATGROUP_Tickables);
}
void URokCityHallWorldRasterAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		TEXT("RokUnityMigration/BatchReview20261009QuestChapterAndWorldHall/CityHallWorldRasterAudit") /
		FString::Printf(TEXT("%s_%u_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*Directory) || !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Hall world raster audit cannot create unique run: %s"), *Directory);
		bFinished = true;
	}
}
void URokCityHallWorldRasterAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("World ended before five input and two frame GPU readbacks completed"));
	ReleaseJob();
	Super::Deinitialize();
}

bool URokCityHallWorldRasterAuditSubsystem::Prepare()
{
	using namespace RokHallWorldRasterAudit;
	UWorld* World = GetWorld();
	if (!ExactWorld(World) || !GEngine || !GEngine->GameViewport ||
		GEngine->GameViewport->GetWorld() != World || !GEngine->GameViewport->Viewport ||
		!World->GetFirstPlayerController() || !World->GetFirstPlayerController()->IsLocalController()) return false;
	URokServerSubsystem* Server = World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0)
	{
		Finish(TEXT("Local-only zero-HTTP boundary unavailable")); return false;
	}
	InitialHttpDispatches = Server->GetHttpDispatchAttemptCount();
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokCityHallWorldRasterFixtureSha="), ExpectedFixtureHash) ||
		!IsSha256(ExpectedFixtureHash))
	{
		Finish(TEXT("Explicit 64-hex -RokCityHallWorldRasterFixtureSha is required")); return false;
	}
	ExpectedFixtureHash.ToLowerInline();
	FixtureHash = HashFile(FPaths::ProjectDir() / FixtureRelative);
	SourceManifestHash = HashFile(FPaths::ProjectDir() / SourceRelative);
	if (FixtureHash != ExpectedFixtureHash || SourceManifestHash != SourceSha ||
		!LoadJson(FPaths::ProjectDir() / FixtureRelative, Fixture) ||
		!LoadJson(FPaths::ProjectDir() / SourceRelative, SourceManifest) ||
		Fixture->GetStringField(TEXT("schema")) != TEXT("rok.city_hall_world_raster_gpu_fixture.v1") ||
		Fixture->GetStringField(TEXT("source_manifest_sha256")) != SourceSha ||
		Fixture->GetStringField(TEXT("hall_native3_sha256")) != HallNative3Sha ||
		Fixture->GetStringField(TEXT("hall_contract_sha256")) != HallContractSha ||
		Fixture->GetObjectField(TEXT("native_contract"))->GetStringField(TEXT("sha256")) != NativeRasterContractSha ||
		Fixture->GetStringField(TEXT("source_row_sha256")) != SourceRowSha ||
		Fixture->GetArrayField(TEXT("inputs")).Num() != SourceBufferCount ||
		Fixture->GetArrayField(TEXT("cases")).Num() != CaseCount ||
		SourceManifest->GetArrayField(TEXT("rows")).Num() != 32)
	{
		Finish(TEXT("Externally pinned Hall world raster fixture or sealed source differs")); return false;
	}
	for (const TCHAR* Stem : { TEXT("hall_native3"), TEXT("hall_verify"), TEXT("native_contract"),
		TEXT("native_manifest"), TEXT("native_verify") })
	{
		const auto Pin = Fixture->GetObjectField(FString(Stem));
		const FString Relative = Pin->GetStringField(TEXT("path"));
		const FString Expected = Pin->GetStringField(TEXT("sha256"));
		if (!IsSha256(Expected) || SafeInput(Relative).IsEmpty() || HashFile(SafeInput(Relative)) != Expected)
		{
			Finish(FString::Printf(TEXT("Pinned Hall source/oracle file differs: %s"), Stem)); return false;
		}
	}
	TSharedPtr<FJsonObject> HallNative, HallVerify, NativeContract, NativeVerify;
	if (!LoadJson(SafeInput(Fixture->GetObjectField(TEXT("hall_native3"))->GetStringField(TEXT("path"))), HallNative) ||
		!LoadJson(SafeInput(Fixture->GetObjectField(TEXT("hall_verify"))->GetStringField(TEXT("path"))), HallVerify) ||
		!LoadJson(SafeInput(Fixture->GetObjectField(TEXT("native_contract"))->GetStringField(TEXT("path"))), NativeContract) ||
		!LoadJson(SafeInput(Fixture->GetObjectField(TEXT("native_manifest"))->GetStringField(TEXT("path"))), NativeManifest) ||
		!LoadJson(SafeInput(Fixture->GetObjectField(TEXT("native_verify"))->GetStringField(TEXT("path"))), NativeVerify) ||
		HallNative->GetStringField(TEXT("schema")) != TEXT("rok.unity_city_hall_world_native.v3") ||
		HallVerify->GetBoolField(TEXT("passed")) != true ||
		HallVerify->GetObjectField(TEXT("native"))->GetIntegerField(TEXT("node_count")) != 38 ||
		NativeContract->GetStringField(TEXT("schema")) != TEXT("rok.unity_city_hall_world_raster_contract.v4") ||
		NativeManifest->GetStringField(TEXT("schema")) != TEXT("rok.unity_city_hall_world_raster_native.v1") ||
		NativeManifest->GetStringField(TEXT("contract_sha256")) != Fixture->GetObjectField(TEXT("native_contract"))->GetStringField(TEXT("sha256")) ||
		!NativeManifest->GetBoolField(TEXT("complete")) ||
		NativeManifest->GetIntegerField(TEXT("case_count")) != CaseCount ||
		NativeManifest->GetIntegerField(TEXT("width")) != Extent ||
		NativeManifest->GetIntegerField(TEXT("height")) != Extent ||
		NativeManifest->GetIntegerField(TEXT("anti_aliasing")) != 1 ||
		NativeManifest->GetBoolField(TEXT("target_srgb")) ||
		NativeManifest->GetStringField(TEXT("readback_row_order")) != TEXT("top_left_rgba8") ||
		NativeManifest->GetStringField(TEXT("graphics_device")) != TEXT("Direct3D11") ||
		NativeManifest->GetStringField(TEXT("color_space")) != TEXT("Gamma") ||
		NativeManifest->GetBoolField(TEXT("game_app_loaded")) ||
		NativeManifest->GetArrayField(TEXT("cases")).Num() != CaseCount ||
		!NativeVerify->GetBoolField(TEXT("passed")) ||
		NativeVerify->GetStringField(TEXT("contract_sha256")) != Fixture->GetObjectField(TEXT("native_contract"))->GetStringField(TEXT("sha256")) ||
		NativeVerify->GetObjectField(TEXT("native"))->GetStringField(TEXT("native_sha256")) != Fixture->GetObjectField(TEXT("native_manifest"))->GetStringField(TEXT("sha256")) ||
		NativeVerify->GetObjectField(TEXT("native"))->GetIntegerField(TEXT("cases_checked")) != CaseCount ||
		NativeVerify->GetObjectField(TEXT("native"))->GetIntegerField(TEXT("png_raw_exact_cases")) != CaseCount)
	{
		Finish(TEXT("Hall native world-pose/raster chain differs")); return false;
	}
	const auto Source = SourceManifest->GetArrayField(TEXT("rows"))[27]->AsObject();
	if (Source->GetIntegerField(TEXT("index")) != 27 || Source->GetStringField(TEXT("source_key")) != TEXT("initial:5") ||
		Source->GetStringField(TEXT("source_row_sha256")) != SourceRowSha ||
		NativeContract->GetObjectField(TEXT("geometry"))->GetStringField(TEXT("source_key")) != TEXT("initial:5"))
	{
		Finish(TEXT("Hall source geometry row27 identity differs")); return false;
	}
	const auto SourcePos = Source->GetObjectField(TEXT("positions"));
	const auto SourceUV = Source->GetArrayField(TEXT("uv_channels"))[0]->AsObject()->GetObjectField(TEXT("binary"));
	const auto SourceIndex = Source->GetObjectField(TEXT("indices"))->GetObjectField(TEXT("binary"));
	const TSharedPtr<FJsonObject> ExpectedInputs[3] = { SourcePos, SourceUV, SourceIndex };
	for (int32 Slot = 0; Slot < SourceBufferCount; ++Slot)
	{
		const auto Input = Fixture->GetArrayField(TEXT("inputs"))[Slot]->AsObject();
		const TCHAR* Kind = Slot == 0 ? TEXT("positions") : Slot == 1 ? TEXT("uv0") : TEXT("indices");
		if (Input->GetStringField(TEXT("kind")) != Kind || !SameBinary(Input, ExpectedInputs[Slot]))
		{
			Finish(TEXT("Hall GPU input differs from sealed typed row27 binary")); return false;
		}
	}
	const auto Pins = Fixture->GetArrayField(TEXT("pinned_files"));
	const TSet<FString> RequiredPins = {
		TEXT("Source/Rok/RokCityHallWorldRasterAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityHallWorldRasterAuditSubsystem.cpp"),
		TEXT("Scripts/PrepareRokCityHallWorldRasterGpuFixture.py"),
		TEXT("Scripts/VerifyRokCityHallWorldRasterGpuRuntime.py"),
		TEXT("Source/RokRendering/Public/RokCitySourceGeometryProbe.h"),
		TEXT("Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp"),
		TEXT("Shaders/Private/RokCitySourceGeometryProbe.usf")
	};
	TSet<FString> SeenPins;
	for (const auto& Value : Pins)
	{
		const auto Pin = Value->AsObject();
		const FString Relative = Pin->GetStringField(TEXT("path"));
		if (SeenPins.Contains(Relative) || SafePinned(Relative).IsEmpty() ||
			HashFile(SafePinned(Relative)) != Pin->GetStringField(TEXT("sha256")))
		{
			Finish(TEXT("Hall raster source/implementation hash pin differs")); return false;
		}
		SeenPins.Add(Relative);
	}
	for (const FString& Required : RequiredPins) if (!SeenPins.Contains(Required))
	{
		Finish(TEXT("Hall raster required implementation pin absent")); return false;
	}
	const auto WorldToClip = Fixture->GetObjectField(TEXT("shared_world_to_clip"));
	TArray<uint8> WorldToClipBytes;
	if (!LoadPinned(WorldToClip, WorldToClipBytes) || WorldToClipBytes.Num() != 64)
	{
		Finish(TEXT("Hall shared union world-to-clip matrix differs")); return false;
	}
	if (WorldToClip->GetStringField(TEXT("sha256")) != NativeContract->GetObjectField(TEXT("shared_fit"))
		->GetObjectField(TEXT("world_to_clip_column"))->GetStringField(TEXT("sha256")) ||
		NativeContract->GetObjectField(TEXT("shared_fit"))->GetArrayField(TEXT("axes")).Num() != 2 ||
		NativeContract->GetObjectField(TEXT("shared_fit"))->GetArrayField(TEXT("axes"))[0]->AsNumber() != 0 ||
		NativeContract->GetObjectField(TEXT("shared_fit"))->GetArrayField(TEXT("axes"))[1]->AsNumber() != 2)
	{
		Finish(TEXT("Hall union clip-fit source proof differs")); return false;
	}
	TSet<FString> MatrixHashes;
	for (int32 Case = 0; Case < CaseCount; ++Case)
	{
		const auto Item = Fixture->GetArrayField(TEXT("cases"))[Case]->AsObject();
		const auto ContractCase = NativeContract->GetArrayField(TEXT("cases"))[Case]->AsObject();
		const auto NativeCase = NativeManifest->GetArrayField(TEXT("cases"))[Case]->AsObject();
		const FString Phase = Case == 0 ? TEXT("prefab_default_hall") : TEXT("runtime_replay_hall");
		if (Item->GetIntegerField(TEXT("index")) != Case || Item->GetStringField(TEXT("phase")) != Phase ||
			ContractCase->GetStringField(TEXT("phase")) != Phase || NativeCase->GetStringField(TEXT("phase")) != Phase ||
			Item->GetStringField(TEXT("source_key")) != TEXT("initial:5") ||
			NativeCase->GetStringField(TEXT("source_key")) != TEXT("initial:5") ||
			NativeCase->GetStringField(TEXT("renderer_file_id")) != TEXT("212751466141171401") ||
			NativeCase->GetStringField(TEXT("sprite_guid")) != TEXT("57ec7ddf0b743a140b2707fe14b536e2") ||
			!NativeCase->GetBoolField(TEXT("png_decodes_to_raw_scope")) ||
			!NativeCase->GetBoolField(TEXT("opaque_alpha")) ||
			Item->GetStringField(TEXT("source_row_sha256")) != SourceRowSha ||
			Item->GetIntegerField(TEXT("width")) != Extent || Item->GetIntegerField(TEXT("height")) != Extent ||
			!Item->GetBoolField(TEXT("native_raw_top_left")))
		{
			Finish(TEXT("Hall native raster case identity or target differs")); return false;
		}
		for (const TCHAR* Stem : { TEXT("world_matrix"), TEXT("matrix") })
		{
			TArray<uint8> Matrix;
			if (!LoadPinned(Item->GetObjectField(FString(Stem)), Matrix) || Matrix.Num() != 64)
			{
				Finish(TEXT("Hall world/final matrix bytes differ")); return false;
			}
		}
		if (Item->GetObjectField(TEXT("world_matrix"))->GetStringField(TEXT("sha256")) !=
			ContractCase->GetObjectField(TEXT("world_matrix"))->GetStringField(TEXT("sha256")) ||
			Item->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256")) !=
			ContractCase->GetObjectField(TEXT("source_to_clip_row_major"))->GetStringField(TEXT("sha256")) ||
			NativeCase->GetStringField(TEXT("world_matrix_sha256")) != Item->GetObjectField(TEXT("world_matrix"))->GetStringField(TEXT("sha256")) ||
			NativeCase->GetStringField(TEXT("source_to_clip_sha256")) != Item->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256")) ||
			NativeCase->GetStringField(TEXT("shared_fit_sha256")) != WorldToClip->GetStringField(TEXT("sha256")))
		{
			Finish(TEXT("Hall two world/final matrix contract join differs")); return false;
		}
		bool bFoundWorldSource = false;
		for (const auto& NodeValue : HallNative->GetArrayField(TEXT("nodes")))
		{
			const auto Node = NodeValue->AsObject();
			if (Node->GetStringField(TEXT("phase")) == Phase &&
				Node->GetStringField(TEXT("component_type")) == TEXT("SpriteRenderer") &&
				Node->GetStringField(TEXT("source_file_id")) == TEXT("212751466141171401") &&
				Node->GetStringField(TEXT("source_guid")) == TEXT("c825e8897df8e424e817e4ed05eef53e") &&
				Node->GetStringField(TEXT("matrix_sha256")) == Item->GetObjectField(TEXT("world_matrix"))->GetStringField(TEXT("sha256")))
			bFoundWorldSource = true;
		}
		if (!bFoundWorldSource)
		{
			Finish(TEXT("Hall pose not anchored to measured Native3 source renderer")); return false;
		}
		if (MatrixHashes.Contains(Item->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256"))))
		{
			Finish(TEXT("Hall world poses collapsed into the same matrix")); return false;
		}
		MatrixHashes.Add(Item->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256")));
		for (const TCHAR* Stem : { TEXT("native_raw"), TEXT("native_png") })
		{
			const FString Relative = Item->GetStringField(FString(Stem) + TEXT("_path"));
			if (SafeInput(Relative).IsEmpty() ||
				HashFile(SafeInput(Relative)) != Item->GetStringField(FString(Stem) + TEXT("_sha256")))
			{
				Finish(TEXT("Hall native raster reference changed")); return false;
			}
		}
		const FString NativeDir = FPaths::GetPath(Fixture->GetObjectField(TEXT("native_manifest"))->GetStringField(TEXT("path")));
		if (Item->GetStringField(TEXT("native_raw_path")) != NativeDir / NativeCase->GetObjectField(TEXT("raw_top_left"))->GetStringField(TEXT("path")) ||
			Item->GetStringField(TEXT("native_raw_sha256")) != NativeCase->GetObjectField(TEXT("raw_top_left"))->GetStringField(TEXT("sha256")) ||
			Item->GetStringField(TEXT("native_png_path")) != NativeDir / NativeCase->GetObjectField(TEXT("png"))->GetStringField(TEXT("path")) ||
			Item->GetStringField(TEXT("native_png_sha256")) != NativeCase->GetObjectField(TEXT("png"))->GetStringField(TEXT("sha256")))
		{
			Finish(TEXT("Hall native raw/PNG output join differs")); return false;
		}
	}
	bPrepared = true;
	return true;
}

void URokCityHallWorldRasterAuditSubsystem::StartJob()
{
	using namespace RokHallWorldRasterAudit;
	const auto Source = SourceManifest->GetArrayField(TEXT("rows"))[27]->AsObject();
	Job = MakeShared<FRokCityHallWorldRasterAuditJob, ESPMode::ThreadSafe>();
	Job->SourceKey = FName(TEXT("initial:5"));
	Job->VertexCount = Source->GetObjectField(TEXT("positions"))->GetIntegerField(TEXT("count"));
	Job->IndexCount = Source->GetObjectField(TEXT("indices"))->GetObjectField(TEXT("binary"))->GetIntegerField(TEXT("count"));
	Job->PositionStride = Source->GetObjectField(TEXT("positions"))->GetIntegerField(TEXT("stride_bytes"));
	Job->UVStride = Source->GetArrayField(TEXT("uv_channels"))[0]->AsObject()->GetObjectField(TEXT("binary"))->GetIntegerField(TEXT("stride_bytes"));
	if (Job->VertexCount != 240 || Job->IndexCount != 714 || Job->PositionStride != 12 || Job->UVStride != 16)
	{
		Finish(TEXT("Pinned Hall renderer mesh count/stride differs")); return;
	}
	for (const auto& Value : Fixture->GetArrayField(TEXT("inputs")))
	{
		const auto Record = Value->AsObject();
		TArray<uint8> Bytes;
		if (!LoadPinned(Record, Bytes) ||
			Record->GetIntegerField(TEXT("count")) * Record->GetIntegerField(TEXT("stride_bytes")) != Bytes.Num() ||
			(Bytes.Num() & 3) != 0)
		{
			Finish(TEXT("Pinned Hall source buffer changed or misaligned")); return;
		}
		Job->Uploads.Add(MoveTemp(Bytes));
	}
	for (int32 Offset = 0; Offset < Job->Uploads[2].Num(); Offset += 4)
	{
		uint32 Vertex = 0;
		FMemory::Memcpy(&Vertex, Job->Uploads[2].GetData() + Offset, 4);
		if (Vertex >= Job->VertexCount)
		{
			Finish(TEXT("Hall source index exceeds source vertex count")); return;
		}
	}
	for (int32 Case = 0; Case < CaseCount; ++Case)
	{
		const auto Matrix = Fixture->GetArrayField(TEXT("cases"))[Case]->AsObject()->GetObjectField(TEXT("matrix"));
		TArray<uint8> Bytes;
		if (!LoadPinned(Matrix, Bytes) || Bytes.Num() != sizeof(FMatrix44f))
		{
			Finish(TEXT("Pinned Hall source-to-clip matrix changed")); return;
		}
		FMemory::Memcpy(&Job->Matrices[Case], Bytes.GetData(), sizeof(FMatrix44f));
		for (int32 R = 0; R < 4; ++R)
			for (int32 C = 0; C < 4; ++C)
				if (!FMath::IsFinite(Job->Matrices[Case].M[R][C]))
				{
					Finish(TEXT("Hall source-to-clip matrix nonfinite")); return;
				}
		Job->Uploads.Add(MoveTemp(Bytes));
	}
	if (Job->Uploads.Num() != BufferCount)
	{
		Finish(TEXT("Hall source plus two matrix upload count differs")); return;
	}
	PendingSeconds = FPlatformTime::Seconds();
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::UploadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallWorldRasterUpload)([Shared](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallWorldRasterAuditJob::EState::UploadPending) return;
		for (int32 Slot = 0; Slot < Shared->Uploads.Num(); ++Slot)
		{
			const TArray<uint8>& Bytes = Shared->Uploads[Slot];
			const FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::CreateByteAddress(
				TEXT("RokHallWorldOriginalBytes"), Bytes.Num(), 4)
				.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::SourceCopy)
				.SetInitialState(ERHIAccess::SRVMask)
				.SetInitActionInitializer();
			FRHIBufferInitializer Initializer = RHICmdList.CreateBufferInitializer(Desc);
			if (Initializer.GetWritableDataSize() < static_cast<size_t>(Bytes.Num()))
			{
				Shared->Error = TEXT("RHI Hall buffer initializer too small");
				Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
			}
			Initializer.WriteData(Bytes.GetData(), Bytes.Num());
			FBufferRHIRef Buffer = Initializer.Finalize();
			if (!Buffer.IsValid() || Buffer->GetSize() != static_cast<uint32>(Bytes.Num()))
			{
				Shared->Error = TEXT("RHI Hall source/matrix buffer creation failed");
				Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
			}
			Shared->Buffers.Add(Buffer);
		}
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::Uploaded;
	});
}

void URokCityHallWorldRasterAuditSubsystem::StartBufferReadback()
{
	const auto Shared = Job;
	const int32 Slot = BufferIndex;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallWorldRasterAuditJob::EState::Uploaded) return;
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::BufferPending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallWorldRasterBufferCopy)([Shared, Slot](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallWorldRasterAuditJob::EState::BufferPending) return;
		FBufferRHIRef& Buffer = Shared->Buffers[Slot];
		Shared->ReadSize = Shared->Uploads[Slot].Num();
		RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->BufferReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("RokHallWorldOriginalBytes"));
		Shared->BufferReadback->EnqueueCopy(RHICmdList, Buffer.GetReference(), Shared->Uploads[Slot].Num());
		RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCityHallWorldRasterAuditSubsystem::StartFrame()
{
	const auto Shared = Job;
	const int32 Case = CaseIndex;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Case < 0 || Case >= RokHallWorldRasterAudit::CaseCount ||
			Shared->State != FRokCityHallWorldRasterAuditJob::EState::Uploaded ||
			Shared->Buffers.Num() != RokHallWorldRasterAudit::BufferCount) return;
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::FramePending;
	}
	ENQUEUE_RENDER_COMMAND(RokHallWorldRasterFrame)([Shared, Case](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCityHallWorldRasterAuditJob::EState::FramePending) return;
		FRokCitySourceGeometryProbeInput Input;
		Input.SourceKey = Shared->SourceKey;
		Input.Positions = Shared->Buffers[0];
		Input.UV0 = Shared->Buffers[1];
		Input.Indices = Shared->Buffers[2];
		Input.VertexCount = Shared->VertexCount;
		Input.IndexCount = Shared->IndexCount;
		Input.PositionStrideBytes = Shared->PositionStride;
		Input.UVStrideBytes = Shared->UVStride;
		Input.LocalToCanonicalClip = Shared->Matrices[Case];
		if (!FRokCitySourceGeometryProbe::Render(RHICmdList, Input, Shared->Output, Shared->Error))
		{
			Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
		}
		const FRHITextureDesc& Desc = Shared->Output->GetRHI()->GetDesc();
		if (Desc.Format != PF_B8G8R8A8 || Desc.Extent != FIntPoint(RokHallWorldRasterAudit::Extent) ||
			Desc.NumSamples != 1 || Desc.NumMips != 1 || EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB))
		{
			Shared->Error = TEXT("Isolated helper target descriptor differs from linear 128 BGRA8 non-MSAA");
			Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
		}
		Shared->TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokHallWorld128BGRA"));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(),
			ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->TextureReadback->EnqueueCopy(RHICmdList, Shared->Output->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(RokHallWorldRasterAudit::Extent, RokHallWorldRasterAudit::Extent, 1));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(),
			ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCityHallWorldRasterAuditSubsystem::Poll()
{
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		if ((Shared->State != FRokCityHallWorldRasterAuditJob::EState::BufferPending &&
			Shared->State != FRokCityHallWorldRasterAuditJob::EState::FramePending) || Shared->bPollQueued) return;
		Shared->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokHallWorldRasterPoll)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->bPollQueued = false;
		if (Shared->State == FRokCityHallWorldRasterAuditJob::EState::BufferPending)
		{
			if (!Shared->BufferReadback || !Shared->BufferReadback->IsReady()) return;
			const uint32 Size = Shared->ReadSize;
			if (Size == 0 || Size > 8 * 1024 * 1024)
			{
				Shared->Error = TEXT("GPU buffer readback staging size invalid");
				Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
			}
			const void* Mapped = Shared->BufferReadback->Lock(Size);
			if (!Mapped)
			{
				Shared->Error = TEXT("GPU buffer staging map failed");
				Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
			}
			Shared->Bytes.SetNumUninitialized(Size);
			FMemory::Memcpy(Shared->Bytes.GetData(), Mapped, Size);
			Shared->BufferReadback->Unlock(); Shared->BufferReadback.Reset();
			Shared->ReadyFrame = GFrameCounterRenderThread;
			Shared->State = FRokCityHallWorldRasterAuditJob::EState::ReadyBuffer;
			return;
		}
		if (Shared->State != FRokCityHallWorldRasterAuditJob::EState::FramePending ||
			!Shared->TextureReadback || !Shared->TextureReadback->IsReady()) return;
		int32 Pitch = 0, Height = 0;
		const void* Mapped = Shared->TextureReadback->Lock(Pitch, &Height);
		if (!Mapped || Pitch < RokHallWorldRasterAudit::Extent || Height < RokHallWorldRasterAudit::Extent)
		{
			if (Mapped) Shared->TextureReadback->Unlock();
			Shared->Error = TEXT("GPU frame staging map/pitch invalid");
			Shared->State = FRokCityHallWorldRasterAuditJob::EState::Failed; return;
		}
		Shared->Pitch = Pitch; Shared->Height = Height;
		Shared->Bytes.SetNumUninitialized(RokHallWorldRasterAudit::Extent * RokHallWorldRasterAudit::Extent * 4);
		for (int32 Y = 0; Y < RokHallWorldRasterAudit::Extent; ++Y)
			FMemory::Memcpy(Shared->Bytes.GetData() + Y * RokHallWorldRasterAudit::Extent * 4,
				static_cast<const uint8*>(Mapped) + Y * Pitch * 4, RokHallWorldRasterAudit::Extent * 4);
		Shared->TextureReadback->Unlock(); Shared->TextureReadback.Reset(); Shared->Output.SafeRelease();
		Shared->ReadyFrame = GFrameCounterRenderThread;
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::ReadyFrame;
	});
}

void URokCityHallWorldRasterAuditSubsystem::RecordReady()
{
	using namespace RokHallWorldRasterAudit;
	TArray<uint8> Bytes;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	int32 Pitch = 0, Height = 0;
	FRokCityHallWorldRasterAuditJob::EState State;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State;
		if (State != FRokCityHallWorldRasterAuditJob::EState::ReadyBuffer &&
			State != FRokCityHallWorldRasterAuditJob::EState::ReadyFrame) return;
		Bytes = MoveTemp(Job->Bytes);
		CopyFrame = Job->CopyFrame; ReadyFrame = Job->ReadyFrame;
		Pitch = Job->Pitch; Height = Job->Height;
		Job->State = FRokCityHallWorldRasterAuditJob::EState::Uploaded;
	}
	const auto Case = Fixture->GetArrayField(TEXT("cases"))[CaseIndex]->AsObject();
	if (State == FRokCityHallWorldRasterAuditJob::EState::ReadyBuffer)
	{
		const auto Input = BufferIndex < SourceBufferCount
			? Fixture->GetArrayField(TEXT("inputs"))[BufferIndex]->AsObject()
			: Fixture->GetArrayField(TEXT("cases"))[BufferIndex - SourceBufferCount]->AsObject()->GetObjectField(TEXT("matrix"));
		const FString Kind = BufferIndex < SourceBufferCount
			? Input->GetStringField(TEXT("kind"))
			: FString::Printf(TEXT("matrix_%s"), *Fixture->GetArrayField(TEXT("cases"))[BufferIndex - SourceBufferCount]->AsObject()->GetStringField(TEXT("phase")));
		const FString Filename = Kind + TEXT(".gpu.bin");
		const bool bExact = Bytes == Job->Uploads[BufferIndex] && Hash(Bytes) == Input->GetStringField(TEXT("sha256"));
		if (!SaveNew(Directory / Filename, Bytes) || !bExact)
		{
			Finish(TEXT("GPU original-byte buffer readback differs: ") + Filename); return;
		}
		auto Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("source_key"), TEXT("initial:5"));
		Result->SetStringField(TEXT("kind"), Kind);
		Result->SetStringField(TEXT("source_path"), Input->GetStringField(TEXT("path")));
		Result->SetStringField(TEXT("source_sha256"), Input->GetStringField(TEXT("sha256")));
		Result->SetStringField(TEXT("gpu_file"), Filename);
		Result->SetStringField(TEXT("gpu_sha256"), Hash(Bytes));
		Result->SetNumberField(TEXT("byte_count"), Bytes.Num());
		Result->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
		Result->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
		Result->SetBoolField(TEXT("byte_exact"), bExact);
		InputResults.Add(Result);
		++BufferIndex;
		return;
	}
	const FString Id = FString::Printf(TEXT("case_%d_%s"), CaseIndex, *Case->GetStringField(TEXT("phase")));
	const FString RawName = Id + TEXT(".gpu.bgra8.bin");
	const FString PngName = Id + TEXT(".gpu.png");
	if (Bytes.Num() != Extent * Extent * 4 || !SaveNew(Directory / RawName, Bytes))
	{
		Finish(TEXT("GPU isolated frame save failed: ") + Id); return;
	}
	TArray<FColor> Colors;
	Colors.SetNumUninitialized(Extent * Extent);
	FMemory::Memcpy(Colors.GetData(), Bytes.GetData(), Bytes.Num());
	TArray64<uint8> Png;
	FImageUtils::PNGCompressImageArray(Extent, Extent,
		TArrayView64<const FColor>(Colors.GetData(), Colors.Num()), Png);
	static constexpr uint8 PngMagic[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	if (Png.Num() < 8 || FMemory::Memcmp(Png.GetData(), PngMagic, 8) != 0 ||
		IFileManager::Get().FileExists(*(Directory / PngName)) ||
		!FFileHelper::SaveArrayToFile(Png, *(Directory / PngName)))
	{
		Finish(TEXT("GPU isolated PNG encoding/save failed: ") + Id); return;
	}
	TArray<uint8> PngHashBytes;
	PngHashBytes.Append(Png.GetData(), static_cast<int32>(Png.Num()));
	const FString NativeRawPath = SafeInput(Case->GetStringField(TEXT("native_raw_path")));
	const FString NativePngPath = SafeInput(Case->GetStringField(TEXT("native_png_path")));
	TArray<uint8> NativeRgba, NativePngBytes;
	if (NativeRawPath.IsEmpty() || NativePngPath.IsEmpty() ||
		!FFileHelper::LoadFileToArray(NativeRgba, *NativeRawPath) ||
		!FFileHelper::LoadFileToArray(NativePngBytes, *NativePngPath) ||
		NativeRgba.Num() != Bytes.Num() || Hash(NativeRgba) != Case->GetStringField(TEXT("native_raw_sha256")) ||
		Hash(NativePngBytes) != Case->GetStringField(TEXT("native_png_sha256")) ||
		NativePngBytes.Num() < 8 || FMemory::Memcmp(NativePngBytes.GetData(), PngMagic, 8) != 0)
	{
		Finish(TEXT("Pinned Unity top-left RGBA/PNG frame differs: ") + Id); return;
	}
	FImage NativeImage, NativeBgra;
	if (!FImageUtils::LoadImage(*NativePngPath, NativeImage) || NativeImage.SizeX != Extent ||
		NativeImage.SizeY != Extent)
	{
		Finish(TEXT("Unity source PNG decode failed: ") + Id); return;
	}
	NativeImage.CopyTo(NativeBgra, ERawImageFormat::BGRA8, NativeImage.GammaSpace);
	const TArrayView64<FColor> NativeColors = NativeBgra.AsBGRA8();
	if (NativeColors.Num() != Extent * Extent)
	{
		Finish(TEXT("Unity source PNG pixel count differs: ") + Id); return;
	}
	int32 MaxRgb = 0, OverOne = 0, CoverageMismatch = 0, NativeCoverage = 0, UeCoverage = 0;
	bool bPngEqualsRaw = true;
	for (int32 Pixel = 0; Pixel < Extent * Extent; ++Pixel)
	{
		const uint8* Source = NativeRgba.GetData() + Pixel * 4;
		const FColor& Decoded = NativeColors[Pixel];
		if (Source[0] != Decoded.R || Source[1] != Decoded.G || Source[2] != Decoded.B || Source[3] != Decoded.A)
			bPngEqualsRaw = false;
		const FColor& Ue = Colors[Pixel];
		const int32 Delta = FMath::Max3(FMath::Abs(int32(Source[0]) - int32(Ue.R)),
			FMath::Abs(int32(Source[1]) - int32(Ue.G)), FMath::Abs(int32(Source[2]) - int32(Ue.B)));
		MaxRgb = FMath::Max(MaxRgb, Delta);
		if (Delta > 1) ++OverOne;
		const bool bNativeDrawn = Source[2] < 122;
		const bool bUeDrawn = Ue.B < 122;
		NativeCoverage += bNativeDrawn; UeCoverage += bUeDrawn;
		CoverageMismatch += bNativeDrawn != bUeDrawn;
	}
	const bool bPass = bPngEqualsRaw && OverOne == 0 && CoverageMismatch == 0 &&
		NativeCoverage > 0 && NativeCoverage < Extent * Extent &&
		UeCoverage == NativeCoverage;
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("case_index"), CaseIndex);
	Result->SetStringField(TEXT("phase"), Case->GetStringField(TEXT("phase")));
	Result->SetStringField(TEXT("source_key"), Case->GetStringField(TEXT("source_key")));
	Result->SetStringField(TEXT("source_row_sha256"), Case->GetStringField(TEXT("source_row_sha256")));
	Result->SetStringField(TEXT("world_matrix_sha256"), Case->GetObjectField(TEXT("world_matrix"))->GetStringField(TEXT("sha256")));
	Result->SetStringField(TEXT("matrix_sha256"), Case->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256")));
	Result->SetStringField(TEXT("native_raw_sha256"), Case->GetStringField(TEXT("native_raw_sha256")));
	Result->SetStringField(TEXT("native_png_sha256"), Case->GetStringField(TEXT("native_png_sha256")));
	Result->SetStringField(TEXT("gpu_bgra_file"), RawName);
	Result->SetStringField(TEXT("gpu_bgra_sha256"), Hash(Bytes));
	Result->SetStringField(TEXT("gpu_png_file"), PngName);
	Result->SetStringField(TEXT("gpu_png_sha256"), Hash(PngHashBytes));
	Result->SetNumberField(TEXT("max_rgb_delta"), MaxRgb);
	Result->SetNumberField(TEXT("pixels_over_1_lsb"), OverOne);
	Result->SetNumberField(TEXT("native_coverage_pixels"), NativeCoverage);
	Result->SetNumberField(TEXT("ue_coverage_pixels"), UeCoverage);
	Result->SetNumberField(TEXT("coverage_mismatch_pixels"), CoverageMismatch);
	Result->SetBoolField(TEXT("native_png_equals_raw"), bPngEqualsRaw);
	Result->SetBoolField(TEXT("rgb_and_coverage_passed"), bPass);
	Result->SetNumberField(TEXT("staging_row_pitch_pixels"), Pitch);
	Result->SetNumberField(TEXT("staging_buffer_height"), Height);
	Result->SetNumberField(TEXT("copy_frame"), static_cast<double>(CopyFrame));
	Result->SetNumberField(TEXT("ready_frame"), static_cast<double>(ReadyFrame));
	FrameResults.Add(Result);
	if (!bPass) Failures.Add(TEXT("Isolated geometry RGB/coverage mismatch: ") + Id);
	++CaseIndex;
	if (CaseIndex == CaseCount) ReleaseJob();
}

void URokCityHallWorldRasterAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (!Prepare() && !bFinished && Now - StartSeconds > RokHallWorldRasterAudit::StartupTimeout)
			Finish(TEXT("Local Game world/viewport readiness timed out"));
		return;
	}
	if (CaseIndex == RokHallWorldRasterAudit::CaseCount) { Finish(); return; }
	if (!Job) { StartJob(); return; }
	FRokCityHallWorldRasterAuditJob::EState State;
	FString Error;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State; Error = Job->Error;
	}
	if (State == FRokCityHallWorldRasterAuditJob::EState::Failed)
	{
		Finish(TEXT("Source geometry GPU job failed: ") + Error); return;
	}
	if (State == FRokCityHallWorldRasterAuditJob::EState::ReadyBuffer ||
		State == FRokCityHallWorldRasterAuditJob::EState::ReadyFrame)
	{
		RecordReady(); return;
	}
	if (State == FRokCityHallWorldRasterAuditJob::EState::BufferPending ||
		State == FRokCityHallWorldRasterAuditJob::EState::FramePending) Poll();
	if (Now - PendingSeconds > RokHallWorldRasterAudit::GpuTimeout)
	{
		Finish(TEXT("Source geometry GPU upload/readback timed out")); return;
	}
	if (State != FRokCityHallWorldRasterAuditJob::EState::Uploaded) return;
	if (BufferIndex < RokHallWorldRasterAudit::BufferCount) StartBufferReadback();
	else StartFrame();
}

void URokCityHallWorldRasterAuditSubsystem::ReleaseJob()
{
	if (!Job) return;
	const auto Shared = Job;
	ENQUEUE_RENDER_COMMAND(RokHallWorldRasterRelease)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->BufferReadback.Reset(); Shared->TextureReadback.Reset(); Shared->Output.SafeRelease();
		Shared->Buffers.Empty(); Shared->Uploads.Empty();
		Shared->State = FRokCityHallWorldRasterAuditJob::EState::Cancelled;
	});
	FlushRenderingCommands();
	Job.Reset();
}

void URokCityHallWorldRasterAuditSubsystem::Finish(const FString& Failure)
{
	using namespace RokHallWorldRasterAudit;
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	ReleaseJob();
	bFinished = true;
	URokServerSubsystem* Server = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	const uint64 FinalHttp = Server ? Server->GetHttpDispatchAttemptCount() : MAX_uint64;
	if (!Server || !Server->IsLocalOnlyRun() || InitialHttpDispatches != 0 || FinalHttp != 0)
		Failures.Add(TEXT("Local-only zero-HTTP boundary changed"));
	int32 StrictFrames = 0, ExactBuffers = 0, ExactSourceBuffers = 0, ExactMatrixBuffers = 0;
	for (const auto& Frame : FrameResults) if (Frame->GetBoolField(TEXT("rgb_and_coverage_passed"))) ++StrictFrames;
	for (const auto& Input : InputResults) if (Input->GetBoolField(TEXT("byte_exact")))
	{
		++ExactBuffers;
		if (Input->GetStringField(TEXT("kind")).StartsWith(TEXT("matrix_"))) ++ExactMatrixBuffers;
		else ++ExactSourceBuffers;
	}
	const bool bComplete = bPrepared && CaseIndex == CaseCount && InputResults.Num() == BufferCount &&
		FrameResults.Num() == CaseCount;
	const bool bPassed = bComplete && StrictFrames == CaseCount && ExactSourceBuffers == SourceBufferCount &&
		ExactMatrixBuffers == CaseCount && Failures.IsEmpty();
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.city_hall_world_raster_gpu_runtime.v1"));
	Report->SetStringField(TEXT("scope"), TEXT("original Hall row27 geometry multiplied by two measured Unity world matrices in isolated UV raster targets; no live actor, source material, alpha, LOD, camera or GameApp main-view acceptance"));
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("fixture_path"), FixtureRelative);
	Report->SetStringField(TEXT("fixture_sha256"), FixtureHash);
	Report->SetStringField(TEXT("expected_fixture_sha256"), ExpectedFixtureHash);
	Report->SetStringField(TEXT("source_manifest_path"), SourceRelative);
	Report->SetStringField(TEXT("source_manifest_sha256"), SourceManifestHash);
	if (Fixture.IsValid())
	{
		for (const TCHAR* Stem : { TEXT("hall_native3"), TEXT("hall_verify"), TEXT("native_contract"),
			TEXT("native_manifest"), TEXT("native_verify") })
			Report->SetStringField(FString(Stem) + TEXT("_sha256"),
				Fixture->GetObjectField(FString(Stem))->GetStringField(TEXT("sha256")));
	}
	for (const TCHAR* Relative : { TEXT("Source/Rok/RokCityHallWorldRasterAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityHallWorldRasterAuditSubsystem.cpp"),
		TEXT("Scripts/PrepareRokCityHallWorldRasterGpuFixture.py"),
		TEXT("Scripts/VerifyRokCityHallWorldRasterGpuRuntime.py"),
		TEXT("Source/RokRendering/Public/RokCitySourceGeometryProbe.h"),
		TEXT("Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp"),
		TEXT("Shaders/Private/RokCitySourceGeometryProbe.usf") })
		Report->SetStringField(FPaths::GetCleanFilename(Relative) + TEXT("_sha256"), HashFile(FPaths::ProjectDir() / Relative));
	Report->SetBoolField(TEXT("opt_in_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokCityHallWorldRasterAudit")));
	Report->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Report->SetBoolField(TEXT("scene_mutated"), false);
	Report->SetBoolField(TEXT("main_view_registered"), false);
	Report->SetBoolField(TEXT("gameapp_accepted"), false);
	Report->SetBoolField(TEXT("material_alpha_accepted"), false);
	Report->SetBoolField(TEXT("isolated_native_world_pose_applied"), bPassed);
	Report->SetBoolField(TEXT("live_world_actor_accepted"), false);
	Report->SetBoolField(TEXT("lod_accepted"), false);
	Report->SetBoolField(TEXT("camera_accepted"), false);
	Report->SetBoolField(TEXT("input_assembler_accepted"), false);
	Report->SetNumberField(TEXT("http_dispatch_before"), static_cast<double>(InitialHttpDispatches));
	Report->SetNumberField(TEXT("http_dispatch_after"), static_cast<double>(FinalHttp));
	Report->SetNumberField(TEXT("expected_source_buffers"), SourceBufferCount);
	Report->SetNumberField(TEXT("expected_matrix_buffers"), CaseCount);
	Report->SetNumberField(TEXT("gpu_byte_exact_buffers"), ExactBuffers);
	Report->SetNumberField(TEXT("gpu_byte_exact_source_buffers"), ExactSourceBuffers);
	Report->SetNumberField(TEXT("gpu_byte_exact_matrix_buffers"), ExactMatrixBuffers);
	Report->SetNumberField(TEXT("expected_frames"), CaseCount);
	Report->SetNumberField(TEXT("strict_rgb_coverage_frames"), StrictFrames);
	Report->SetNumberField(TEXT("rgb_tolerance_lsb"), 1);
	Report->SetStringField(TEXT("native_row_order"), TEXT("top_left_rgba8"));
	Report->SetStringField(TEXT("ue_row_order"), TEXT("GPU staging rows tight top-left BGRA8"));
	Report->SetBoolField(TEXT("complete"), bComplete);
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetArrayField(TEXT("inputs"), Objects(InputResults));
	Report->SetArrayField(TEXT("frames"), Objects(FrameResults));
	Report->SetArrayField(TEXT("failures"), Strings(Failures));
	FString Text;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text));
	const bool bManifestSaved = FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json")));
	if (!bManifestSaved)
		UE_LOG(LogTemp, Error, TEXT("Hall world raster audit cannot save manifest: %s"), *Directory);
	UE_LOG(LogTemp, Display, TEXT("Hall world raster audit %s buffers=%d frames=%d run=%s"),
		bPassed && bManifestSaved ? TEXT("PASSED") : TEXT("FAILED"), InputResults.Num(), FrameResults.Num(), *Directory);
	FPlatformMisc::RequestExitWithStatus(false, bPassed && bManifestSaved ? 0 : 1);
}
