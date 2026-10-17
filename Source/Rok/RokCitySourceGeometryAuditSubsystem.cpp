#include "RokCitySourceGeometryAuditSubsystem.h"

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

namespace RokSourceGeometryAudit
{
constexpr TCHAR FixtureRelative[] = TEXT("Saved/RokUnityMigration/CitySourceGeometryGpuFixture/v5/manifest.json");
constexpr TCHAR SourceRelative[] = TEXT("Saved/RokUnityMigration/CityGammaSourceGeometryInputs/v1/manifest.json");
constexpr TCHAR SourceSha[] = TEXT("c30078bedd2f8a3a92a2f0c4fbc1f0ef4833e49855551c9e9f0d61aee70c3273");
constexpr TCHAR NativeSha[] = TEXT("0532db0fb42c4ccac68e83845a9f95ed108385256985d1975939df31d183f935");
constexpr TCHAR ContractSha[] = TEXT("cd69da3bf9dbf3d7d79ca09b53b3d72371301d41b25b5faa75123a30af47a18e");
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr int32 Extent = 128, RowCount = 32, BufferCount = 97;
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
		  Relative.StartsWith(TEXT("Saved/RokUnityMigration/CitySourceGeometryGpuFixture/v5/")) ||
		  Relative.StartsWith(TEXT("Saved/UnityCitySourceGeometryProbeV1/")))) return FString();
	return FPaths::ProjectDir() / Relative;
}
FString SafePinned(const FString& Relative)
{
	if (Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) ||
		Relative.Contains(TEXT("\\")) || Relative.StartsWith(TEXT("/"))) return FString();
	if (Relative.StartsWith(TEXT("Source/Rok/")) || Relative.StartsWith(TEXT("Source/RokRendering/")) ||
		Relative.StartsWith(TEXT("Shaders/Private/")) || Relative.StartsWith(TEXT("Scripts/")) ||
		Relative.StartsWith(TEXT("Saved/UnityCitySourceGeometryProbeV1/")))
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

struct FRokCitySourceGeometryAuditJob
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
	FMatrix44f Matrix = FMatrix44f::Identity;
	FName SourceKey;
	uint32 VertexCount = 0, IndexCount = 0, PositionStride = 0, UVStride = 0;
	uint32 ReadSize = 0;
	int32 Pitch = 0, Height = 0;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	bool bPollQueued = false;
};

bool URokCitySourceGeometryAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return RokSourceGeometryAudit::ExactWorld(Cast<UWorld>(Outer)) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokCitySourceGeometryAudit")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCitySourceGeometryAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCitySourceGeometryAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCitySourceGeometryAuditSubsystem, STATGROUP_Tickables);
}
void URokCitySourceGeometryAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartSeconds = FPlatformTime::Seconds();
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
		TEXT("RokUnityMigration/BatchReview20261009ExpeditionArtAndSourceGeometry/CitySourceGeometryAudit") /
		FString::Printf(TEXT("%s_%u_%s"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%fff")),
			FPlatformProcess::GetCurrentProcessId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	if (IFileManager::Get().DirectoryExists(*Directory) || !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		UE_LOG(LogTemp, Error, TEXT("Source geometry audit cannot create unique run: %s"), *Directory);
		bFinished = true;
	}
}
void URokCitySourceGeometryAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("World ended before 97 buffer and 32 frame GPU readbacks completed"));
	ReleaseJob();
	Super::Deinitialize();
}

bool URokCitySourceGeometryAuditSubsystem::Prepare()
{
	using namespace RokSourceGeometryAudit;
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
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokCitySourceGeometryFixtureSha="), ExpectedFixtureHash) ||
		!IsSha256(ExpectedFixtureHash))
	{
		Finish(TEXT("Explicit 64-hex -RokCitySourceGeometryFixtureSha is required")); return false;
	}
	ExpectedFixtureHash.ToLowerInline();
	FixtureHash = HashFile(FPaths::ProjectDir() / FixtureRelative);
	SourceManifestHash = HashFile(FPaths::ProjectDir() / SourceRelative);
	if (FixtureHash != ExpectedFixtureHash || SourceManifestHash != SourceSha ||
		!LoadJson(FPaths::ProjectDir() / FixtureRelative, Fixture) ||
		!LoadJson(FPaths::ProjectDir() / SourceRelative, SourceManifest) ||
		Fixture->GetStringField(TEXT("schema")) != TEXT("rok.city_source_geometry_gpu_fixture.v1") ||
		Fixture->GetStringField(TEXT("fixture_revision")) != TEXT("v5") ||
		Fixture->GetStringField(TEXT("source_manifest_sha256")) != SourceSha ||
		Fixture->GetStringField(TEXT("source_manifest_path")) != SourceRelative ||
		Fixture->GetArrayField(TEXT("cases")).Num() != RowCount ||
		SourceManifest->GetArrayField(TEXT("rows")).Num() != RowCount)
	{
		Finish(TEXT("Pinned 32-row geometry fixture or sealed source manifest differs")); return false;
	}
	if (Fixture->GetStringField(TEXT("native_manifest_sha256")) != NativeSha ||
		Fixture->GetStringField(TEXT("native_contract_sha256")) != ContractSha)
	{
		Finish(TEXT("Native2 manifest/contract anchor differs")); return false;
	}
	for (const TCHAR* Stem : { TEXT("native_manifest"), TEXT("native_verify"), TEXT("native_contract") })
	{
		const FString Path = Fixture->GetStringField(FString(Stem) + TEXT("_path"));
		const FString Expected = Fixture->GetStringField(FString(Stem) + TEXT("_sha256"));
		if (SafeInput(Path).IsEmpty() || Expected.Len() != 64 || HashFile(SafeInput(Path)) != Expected)
		{
			Finish(FString::Printf(TEXT("Pinned Unity geometry chain differs: %s"), Stem)); return false;
		}
	}
	TSharedPtr<FJsonObject> NativeVerify;
	if (!LoadJson(SafeInput(Fixture->GetStringField(TEXT("native_manifest_path"))), NativeManifest) ||
		!LoadJson(SafeInput(Fixture->GetStringField(TEXT("native_verify_path"))), NativeVerify) ||
		NativeManifest->GetStringField(TEXT("schema")) != TEXT("rok.unity_city_source_geometry_native.v1") ||
		NativeManifest->GetArrayField(TEXT("cases")).Num() != RowCount ||
		!NativeVerify->GetBoolField(TEXT("passed")) ||
		NativeVerify->GetStringField(TEXT("native_sha256")) != Fixture->GetStringField(TEXT("native_manifest_sha256")) ||
		NativeVerify->GetIntegerField(TEXT("cases_checked")) != RowCount ||
		NativeVerify->GetIntegerField(TEXT("source_typed_binary_inputs_checked")) != BufferCount)
	{
		Finish(TEXT("Native 32-case independent GPU verification differs")); return false;
	}
	const auto Pinned = Fixture->GetArrayField(TEXT("pinned_files"));
	const TSet<FString> ExpectedPinned = {
		TEXT("Scripts/PrepareRokCitySourceGeometryGpuFixture.py"),
		TEXT("Scripts/VerifyRokCitySourceGeometryGpuRuntime.py"),
		TEXT("Source/Rok/RokCitySourceGeometryAuditSubsystem.h"),
		TEXT("Source/Rok/RokCitySourceGeometryAuditSubsystem.cpp"),
		TEXT("Source/RokRendering/Public/RokCitySourceGeometryProbe.h"),
		TEXT("Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp"),
		TEXT("Shaders/Private/RokCitySourceGeometryProbe.usf"),
		TEXT("Scripts/PrepareRokUnityCitySourceGeometryProbe.py"),
		TEXT("Scripts/VerifyRokUnityCitySourceGeometryProbe.py"),
		TEXT("Saved/UnityCitySourceGeometryProbeV1/Assets/Editor/ExportRokUnityCitySourceGeometryProbe.cs"),
		TEXT("Saved/UnityCitySourceGeometryProbeV1/Assets/ProbeLocalUv.shader"),
		TEXT("Scripts/PrepareRokCityGammaSourceGeometryInputs.py"),
		TEXT("Scripts/VerifyRokCityGammaSourceGeometryInputs.py")
	};
	if (Pinned.Num() != ExpectedPinned.Num())
	{
		Finish(TEXT("Implementation/source pin path set count differs")); return false;
	}
	TSet<FString> SeenPins;
	for (const auto& Value : Pinned)
	{
		const auto Record = Value->AsObject();
		const FString Relative = Record->GetStringField(TEXT("path"));
		const FString Path = SafePinned(Relative);
		if (!ExpectedPinned.Contains(Relative) || SeenPins.Contains(Relative) || Path.IsEmpty() ||
			HashFile(Path) != Record->GetStringField(TEXT("sha256")))
		{
			Finish(TEXT("Pinned implementation/source file changed")); return false;
		}
		SeenPins.Add(Relative);
	}
	TSet<FString> Keys;
	int32 Inputs = 0;
	for (int32 Row = 0; Row < RowCount; ++Row)
	{
		const auto Case = Fixture->GetArrayField(TEXT("cases"))[Row]->AsObject();
		const auto Source = SourceManifest->GetArrayField(TEXT("rows"))[Row]->AsObject();
		const auto Native = NativeManifest->GetArrayField(TEXT("cases"))[Row]->AsObject();
		if (!Case.IsValid() || !Source.IsValid() || Case->GetIntegerField(TEXT("index")) != Row ||
			!Native.IsValid() || Native->GetIntegerField(TEXT("index")) != Row ||
			Native->GetStringField(TEXT("source_key")) != Case->GetStringField(TEXT("source_key")) ||
			Native->GetStringField(TEXT("source_row_sha256")) != Case->GetStringField(TEXT("source_row_sha256")) ||
			Source->GetIntegerField(TEXT("index")) != Row ||
			Case->GetStringField(TEXT("source_key")) != Source->GetStringField(TEXT("source_key")) ||
			Case->GetStringField(TEXT("source_group")) != Source->GetStringField(TEXT("source_group")) ||
			Case->GetStringField(TEXT("source_row_sha256")) != Source->GetStringField(TEXT("source_row_sha256")) ||
			Keys.Contains(Case->GetStringField(TEXT("source_key"))) ||
			Case->GetArrayField(TEXT("inputs")).Num() != (Row == 29 ? 4 : 3))
		{
			Finish(TEXT("Fixture source identity, ordering, or input count differs")); return false;
		}
		Keys.Add(Case->GetStringField(TEXT("source_key")));
		const auto SourcePos = Source->GetObjectField(TEXT("positions"));
		const auto SourceUV = Source->GetArrayField(TEXT("uv_channels"))[0]->AsObject()->GetObjectField(TEXT("binary"));
		const auto SourceIndex = Source->GetObjectField(TEXT("indices"))->GetObjectField(TEXT("binary"));
		const auto SourceColors = Source->GetObjectField(TEXT("colors"));
		const TSharedPtr<FJsonObject>* SourceColorPtr = nullptr;
		SourceColors->TryGetObjectField(TEXT("binary"), SourceColorPtr);
		const TSharedPtr<FJsonObject> SourceColor = SourceColorPtr ? *SourceColorPtr : nullptr;
		const TSharedPtr<FJsonObject> Expected[4] = { SourcePos, SourceUV, SourceIndex, SourceColor };
		for (int32 Slot = 0; Slot < Case->GetArrayField(TEXT("inputs")).Num(); ++Slot)
		{
			const auto Input = Case->GetArrayField(TEXT("inputs"))[Slot]->AsObject();
			if (!Input.IsValid() || Input->GetStringField(TEXT("kind")) !=
				(Slot == 0 ? TEXT("positions") : Slot == 1 ? TEXT("uv0") : Slot == 2 ? TEXT("indices") : TEXT("colors")) ||
				!SameBinary(Input, Expected[Slot]))
			{
				Finish(TEXT("Fixture input binary does not match sealed geometry row")); return false;
			}
			++Inputs;
		}
		TArray<uint8> Matrix;
		if (!LoadPinned(Case->GetObjectField(TEXT("matrix")), Matrix) || Matrix.Num() != 64 ||
			Native->GetStringField(TEXT("matrix_sha256")) != Case->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("sha256")) ||
			Case->GetObjectField(TEXT("matrix"))->GetStringField(TEXT("path")).Contains(TEXT("..")) ||
			Case->GetIntegerField(TEXT("width")) != Extent || Case->GetIntegerField(TEXT("height")) != Extent ||
			!Case->GetBoolField(TEXT("native_raw_top_left")))
		{
			Finish(TEXT("Fixture matrix or native frame convention differs")); return false;
		}
		for (const TCHAR* Stem : { TEXT("native_png"), TEXT("native_raw") })
		{
			const FString Relative = Case->GetStringField(FString(Stem) + TEXT("_path"));
			if (SafeInput(Relative).IsEmpty() || HashFile(SafeInput(Relative)) !=
				Case->GetStringField(FString(Stem) + TEXT("_sha256")))
			{
				Finish(TEXT("Pinned native geometry frame differs")); return false;
			}
		}
		const FString NativeDirectory = FPaths::GetPath(Fixture->GetStringField(TEXT("native_manifest_path")));
		if (Case->GetStringField(TEXT("native_png_path")) != NativeDirectory / Native->GetStringField(TEXT("png")) ||
			Case->GetStringField(TEXT("native_raw_path")) != NativeDirectory / Native->GetStringField(TEXT("raw_top_left")) ||
			Case->GetStringField(TEXT("native_png_sha256")) != Native->GetStringField(TEXT("png_sha256")) ||
			Case->GetStringField(TEXT("native_raw_sha256")) != Native->GetStringField(TEXT("raw_top_left_sha256")))
		{
			Finish(TEXT("Native 32-case PNG/raw fixture join differs")); return false;
		}
	}
	if (Inputs != BufferCount || Keys.Num() != RowCount)
	{
		Finish(TEXT("Fixture does not close exactly 97 unique source inputs")); return false;
	}
	bPrepared = true;
	return true;
}

void URokCitySourceGeometryAuditSubsystem::StartRow()
{
	using namespace RokSourceGeometryAudit;
	const auto Case = Fixture->GetArrayField(TEXT("cases"))[RowIndex]->AsObject();
	const auto Source = SourceManifest->GetArrayField(TEXT("rows"))[RowIndex]->AsObject();
	const auto Inputs = Case->GetArrayField(TEXT("inputs"));
	Job = MakeShared<FRokCitySourceGeometryAuditJob, ESPMode::ThreadSafe>();
	Job->SourceKey = FName(*Case->GetStringField(TEXT("source_key")));
	Job->VertexCount = Source->GetObjectField(TEXT("positions"))->GetIntegerField(TEXT("count"));
	Job->IndexCount = Source->GetObjectField(TEXT("indices"))->GetObjectField(TEXT("binary"))->GetIntegerField(TEXT("count"));
	Job->PositionStride = Source->GetObjectField(TEXT("positions"))->GetIntegerField(TEXT("stride_bytes"));
	Job->UVStride = Source->GetArrayField(TEXT("uv_channels"))[0]->AsObject()->GetObjectField(TEXT("binary"))->GetIntegerField(TEXT("stride_bytes"));
	if (Job->SourceKey.IsNone() || Job->VertexCount < 3 || Job->IndexCount < 3 || Job->IndexCount % 3 != 0 ||
		(Job->PositionStride != 8 && Job->PositionStride != 12) ||
		(Job->UVStride != 8 && Job->UVStride != 16))
	{
		Finish(TEXT("Sealed source vertex/index counts or strides invalid")); return;
	}
	for (const auto& Value : Inputs)
	{
		const auto Record = Value->AsObject();
		TArray<uint8> Bytes;
		if (!LoadPinned(Record, Bytes) ||
			Record->GetIntegerField(TEXT("count")) * Record->GetIntegerField(TEXT("stride_bytes")) != Bytes.Num() ||
			(Bytes.Num() & 3) != 0)
		{
			Finish(TEXT("Sealed source buffer changed or misaligned")); return;
		}
		Job->Uploads.Add(MoveTemp(Bytes));
	}
	const TArray<uint8>& Indices = Job->Uploads[2];
	for (int32 Offset = 0; Offset < Indices.Num(); Offset += 4)
	{
		uint32 Vertex = 0;
		FMemory::Memcpy(&Vertex, Indices.GetData() + Offset, 4);
		if (Vertex >= Job->VertexCount)
		{
			Finish(TEXT("Sealed source index exceeds this row's vertex count")); return;
		}
	}
	TArray<uint8> MatrixBytes;
	if (!LoadPinned(Case->GetObjectField(TEXT("matrix")), MatrixBytes) || MatrixBytes.Num() != sizeof(FMatrix44f))
	{
		Finish(TEXT("Canonical row-vector float32 matrix changed")); return;
	}
	FMemory::Memcpy(&Job->Matrix, MatrixBytes.GetData(), sizeof(FMatrix44f));
	for (int32 R = 0; R < 4; ++R)
		for (int32 C = 0; C < 4; ++C)
			if (!FMath::IsFinite(Job->Matrix.M[R][C]))
			{
				Finish(TEXT("Canonical float32 matrix has nonfinite element")); return;
			}
	PendingSeconds = FPlatformTime::Seconds();
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->State = FRokCitySourceGeometryAuditJob::EState::UploadPending;
	}
	ENQUEUE_RENDER_COMMAND(RokSourceGeometryUpload)([Shared](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::UploadPending) return;
		for (int32 Slot = 0; Slot < Shared->Uploads.Num(); ++Slot)
		{
			const TArray<uint8>& Bytes = Shared->Uploads[Slot];
			const FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::CreateByteAddress(
				TEXT("RokSourceGeometryOriginalBytes"), Bytes.Num(), 4)
				.AddUsage(EBufferUsageFlags::ShaderResource | EBufferUsageFlags::SourceCopy)
				.SetInitialState(ERHIAccess::SRVMask)
				.SetInitActionInitializer();
			FRHIBufferInitializer Initializer = RHICmdList.CreateBufferInitializer(Desc);
			if (Initializer.GetWritableDataSize() < static_cast<size_t>(Bytes.Num()))
			{
				Shared->Error = TEXT("RHI buffer initializer too small");
				Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
			}
			Initializer.WriteData(Bytes.GetData(), Bytes.Num());
			FBufferRHIRef Buffer = Initializer.Finalize();
			if (!Buffer.IsValid() || Buffer->GetSize() != static_cast<uint32>(Bytes.Num()))
			{
				Shared->Error = TEXT("RHI original-byte buffer creation failed");
				Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
			}
			Shared->Buffers.Add(Buffer);
		}
		Shared->State = FRokCitySourceGeometryAuditJob::EState::Uploaded;
	});
}

void URokCitySourceGeometryAuditSubsystem::StartBufferReadback()
{
	const auto Shared = Job;
	const int32 Slot = BufferIndex;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::Uploaded) return;
		Shared->State = FRokCitySourceGeometryAuditJob::EState::BufferPending;
	}
	ENQUEUE_RENDER_COMMAND(RokSourceGeometryBufferCopy)([Shared, Slot](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::BufferPending) return;
		FBufferRHIRef& Buffer = Shared->Buffers[Slot];
		Shared->ReadSize = Shared->Uploads[Slot].Num();
		RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->BufferReadback = MakeUnique<FRHIGPUBufferReadback>(TEXT("RokSourceGeometryOriginalBytes"));
		Shared->BufferReadback->EnqueueCopy(RHICmdList, Buffer.GetReference(), Shared->Uploads[Slot].Num());
		RHICmdList.Transition(FRHITransitionInfo(Buffer.GetReference(), ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCitySourceGeometryAuditSubsystem::StartFrame()
{
	const auto Shared = Job;
	PendingSeconds = FPlatformTime::Seconds();
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::Uploaded || Shared->Buffers.Num() < 3) return;
		Shared->State = FRokCitySourceGeometryAuditJob::EState::FramePending;
	}
	ENQUEUE_RENDER_COMMAND(RokSourceGeometryFrame)([Shared](FRHICommandListImmediate& RHICmdList)
	{
		FScopeLock Lock(&Shared->Mutex);
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::FramePending) return;
		FRokCitySourceGeometryProbeInput Input;
		Input.SourceKey = Shared->SourceKey;
		Input.Positions = Shared->Buffers[0];
		Input.UV0 = Shared->Buffers[1];
		Input.Indices = Shared->Buffers[2];
		Input.VertexCount = Shared->VertexCount;
		Input.IndexCount = Shared->IndexCount;
		Input.PositionStrideBytes = Shared->PositionStride;
		Input.UVStrideBytes = Shared->UVStride;
		Input.LocalToCanonicalClip = Shared->Matrix;
		if (!FRokCitySourceGeometryProbe::Render(RHICmdList, Input, Shared->Output, Shared->Error))
		{
			Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
		}
		const FRHITextureDesc& Desc = Shared->Output->GetRHI()->GetDesc();
		if (Desc.Format != PF_B8G8R8A8 || Desc.Extent != FIntPoint(RokSourceGeometryAudit::Extent) ||
			Desc.NumSamples != 1 || Desc.NumMips != 1 || EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB))
		{
			Shared->Error = TEXT("Isolated helper target descriptor differs from linear 128 BGRA8 non-MSAA");
			Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
		}
		Shared->TextureReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokSourceGeometry128BGRA"));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(),
			ERHIAccess::SRVMask, ERHIAccess::CopySrc));
		Shared->TextureReadback->EnqueueCopy(RHICmdList, Shared->Output->GetRHI(),
			FIntVector::ZeroValue, 0, FIntVector(RokSourceGeometryAudit::Extent, RokSourceGeometryAudit::Extent, 1));
		RHICmdList.Transition(FRHITransitionInfo(Shared->Output->GetRHI(),
			ERHIAccess::CopySrc, ERHIAccess::SRVMask));
		Shared->CopyFrame = GFrameCounterRenderThread;
	});
}

void URokCitySourceGeometryAuditSubsystem::Poll()
{
	const auto Shared = Job;
	{
		FScopeLock Lock(&Shared->Mutex);
		if ((Shared->State != FRokCitySourceGeometryAuditJob::EState::BufferPending &&
			Shared->State != FRokCitySourceGeometryAuditJob::EState::FramePending) || Shared->bPollQueued) return;
		Shared->bPollQueued = true;
	}
	ENQUEUE_RENDER_COMMAND(RokSourceGeometryPoll)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->bPollQueued = false;
		if (Shared->State == FRokCitySourceGeometryAuditJob::EState::BufferPending)
		{
			if (!Shared->BufferReadback || !Shared->BufferReadback->IsReady()) return;
			const uint32 Size = Shared->ReadSize;
			if (Size == 0 || Size > 8 * 1024 * 1024)
			{
				Shared->Error = TEXT("GPU buffer readback staging size invalid");
				Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
			}
			const void* Mapped = Shared->BufferReadback->Lock(Size);
			if (!Mapped)
			{
				Shared->Error = TEXT("GPU buffer staging map failed");
				Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
			}
			Shared->Bytes.SetNumUninitialized(Size);
			FMemory::Memcpy(Shared->Bytes.GetData(), Mapped, Size);
			Shared->BufferReadback->Unlock(); Shared->BufferReadback.Reset();
			Shared->ReadyFrame = GFrameCounterRenderThread;
			Shared->State = FRokCitySourceGeometryAuditJob::EState::ReadyBuffer;
			return;
		}
		if (Shared->State != FRokCitySourceGeometryAuditJob::EState::FramePending ||
			!Shared->TextureReadback || !Shared->TextureReadback->IsReady()) return;
		int32 Pitch = 0, Height = 0;
		const void* Mapped = Shared->TextureReadback->Lock(Pitch, &Height);
		if (!Mapped || Pitch < RokSourceGeometryAudit::Extent || Height < RokSourceGeometryAudit::Extent)
		{
			if (Mapped) Shared->TextureReadback->Unlock();
			Shared->Error = TEXT("GPU frame staging map/pitch invalid");
			Shared->State = FRokCitySourceGeometryAuditJob::EState::Failed; return;
		}
		Shared->Pitch = Pitch; Shared->Height = Height;
		Shared->Bytes.SetNumUninitialized(RokSourceGeometryAudit::Extent * RokSourceGeometryAudit::Extent * 4);
		for (int32 Y = 0; Y < RokSourceGeometryAudit::Extent; ++Y)
			FMemory::Memcpy(Shared->Bytes.GetData() + Y * RokSourceGeometryAudit::Extent * 4,
				static_cast<const uint8*>(Mapped) + Y * Pitch * 4, RokSourceGeometryAudit::Extent * 4);
		Shared->TextureReadback->Unlock(); Shared->TextureReadback.Reset(); Shared->Output.SafeRelease();
		Shared->ReadyFrame = GFrameCounterRenderThread;
		Shared->State = FRokCitySourceGeometryAuditJob::EState::ReadyFrame;
	});
}

void URokCitySourceGeometryAuditSubsystem::RecordReady()
{
	using namespace RokSourceGeometryAudit;
	TArray<uint8> Bytes;
	uint64 CopyFrame = 0, ReadyFrame = 0;
	int32 Pitch = 0, Height = 0;
	FRokCitySourceGeometryAuditJob::EState State;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State;
		if (State != FRokCitySourceGeometryAuditJob::EState::ReadyBuffer &&
			State != FRokCitySourceGeometryAuditJob::EState::ReadyFrame) return;
		Bytes = MoveTemp(Job->Bytes);
		CopyFrame = Job->CopyFrame; ReadyFrame = Job->ReadyFrame;
		Pitch = Job->Pitch; Height = Job->Height;
		Job->State = FRokCitySourceGeometryAuditJob::EState::Uploaded;
	}
	const auto Case = Fixture->GetArrayField(TEXT("cases"))[RowIndex]->AsObject();
	if (State == FRokCitySourceGeometryAuditJob::EState::ReadyBuffer)
	{
		const auto Input = Case->GetArrayField(TEXT("inputs"))[BufferIndex]->AsObject();
		const FString Kind = Input->GetStringField(TEXT("kind"));
		const FString Filename = FString::Printf(TEXT("row_%02d_%s.gpu.bin"), RowIndex, *Kind);
		const bool bExact = Bytes == Job->Uploads[BufferIndex] && Hash(Bytes) == Input->GetStringField(TEXT("sha256"));
		if (!SaveNew(Directory / Filename, Bytes) || !bExact)
		{
			Finish(TEXT("GPU original-byte buffer readback differs: ") + Filename); return;
		}
		auto Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("row_index"), RowIndex);
		Result->SetStringField(TEXT("source_key"), Case->GetStringField(TEXT("source_key")));
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
	const FString Id = FString::Printf(TEXT("row_%02d"), RowIndex);
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
	Result->SetNumberField(TEXT("row_index"), RowIndex);
	Result->SetStringField(TEXT("source_key"), Case->GetStringField(TEXT("source_key")));
	Result->SetStringField(TEXT("source_row_sha256"), Case->GetStringField(TEXT("source_row_sha256")));
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
	ReleaseJob();
	++RowIndex;
	BufferIndex = 0;
}

void URokCitySourceGeometryAuditSubsystem::Tick(float)
{
	if (bFinished) return;
	const double Now = FPlatformTime::Seconds();
	if (!bPrepared)
	{
		if (!Prepare() && !bFinished && Now - StartSeconds > RokSourceGeometryAudit::StartupTimeout)
			Finish(TEXT("Local Game world/viewport readiness timed out"));
		return;
	}
	if (RowIndex == RokSourceGeometryAudit::RowCount) { Finish(); return; }
	if (!Job) { StartRow(); return; }
	FRokCitySourceGeometryAuditJob::EState State;
	FString Error;
	{
		FScopeLock Lock(&Job->Mutex);
		State = Job->State; Error = Job->Error;
	}
	if (State == FRokCitySourceGeometryAuditJob::EState::Failed)
	{
		Finish(TEXT("Source geometry GPU job failed: ") + Error); return;
	}
	if (State == FRokCitySourceGeometryAuditJob::EState::ReadyBuffer ||
		State == FRokCitySourceGeometryAuditJob::EState::ReadyFrame)
	{
		RecordReady(); return;
	}
	if (State == FRokCitySourceGeometryAuditJob::EState::BufferPending ||
		State == FRokCitySourceGeometryAuditJob::EState::FramePending) Poll();
	if (Now - PendingSeconds > RokSourceGeometryAudit::GpuTimeout)
	{
		Finish(TEXT("Source geometry GPU upload/readback timed out")); return;
	}
	if (State != FRokCitySourceGeometryAuditJob::EState::Uploaded) return;
	const int32 Count = Fixture->GetArrayField(TEXT("cases"))[RowIndex]->AsObject()->GetArrayField(TEXT("inputs")).Num();
	if (BufferIndex < Count) StartBufferReadback();
	else StartFrame();
}

void URokCitySourceGeometryAuditSubsystem::ReleaseJob()
{
	if (!Job) return;
	const auto Shared = Job;
	ENQUEUE_RENDER_COMMAND(RokSourceGeometryRelease)([Shared](FRHICommandListImmediate&)
	{
		FScopeLock Lock(&Shared->Mutex);
		Shared->BufferReadback.Reset(); Shared->TextureReadback.Reset(); Shared->Output.SafeRelease();
		Shared->Buffers.Empty(); Shared->Uploads.Empty();
		Shared->State = FRokCitySourceGeometryAuditJob::EState::Cancelled;
	});
	FlushRenderingCommands();
	Job.Reset();
}

void URokCitySourceGeometryAuditSubsystem::Finish(const FString& Failure)
{
	using namespace RokSourceGeometryAudit;
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	ReleaseJob();
	bFinished = true;
	URokServerSubsystem* Server = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	const uint64 FinalHttp = Server ? Server->GetHttpDispatchAttemptCount() : MAX_uint64;
	if (!Server || !Server->IsLocalOnlyRun() || InitialHttpDispatches != 0 || FinalHttp != 0)
		Failures.Add(TEXT("Local-only zero-HTTP boundary changed"));
	int32 StrictFrames = 0, ExactBuffers = 0;
	for (const auto& Frame : FrameResults) if (Frame->GetBoolField(TEXT("rgb_and_coverage_passed"))) ++StrictFrames;
	for (const auto& Input : InputResults) if (Input->GetBoolField(TEXT("byte_exact"))) ++ExactBuffers;
	const bool bComplete = bPrepared && RowIndex == RowCount && InputResults.Num() == BufferCount &&
		FrameResults.Num() == RowCount;
	const bool bPassed = bComplete && StrictFrames == RowCount && ExactBuffers == BufferCount && Failures.IsEmpty();
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.city_source_geometry_gpu_runtime.v1"));
	Report->SetStringField(TEXT("scope"), TEXT("isolated source-local source geometry/UV0 GPU diagnostic only; no material, alpha, world transform, GameApp main view, or input assembler acceptance"));
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("fixture_path"), FixtureRelative);
	Report->SetStringField(TEXT("fixture_sha256"), FixtureHash);
	Report->SetStringField(TEXT("expected_fixture_sha256"), ExpectedFixtureHash);
	Report->SetStringField(TEXT("source_manifest_path"), SourceRelative);
	Report->SetStringField(TEXT("source_manifest_sha256"), SourceManifestHash);
	for (const TCHAR* Relative : { TEXT("Source/Rok/RokCitySourceGeometryAuditSubsystem.h"),
		TEXT("Source/Rok/RokCitySourceGeometryAuditSubsystem.cpp"),
		TEXT("Scripts/PrepareRokCitySourceGeometryGpuFixture.py"),
		TEXT("Scripts/VerifyRokCitySourceGeometryGpuRuntime.py"),
		TEXT("Source/RokRendering/Public/RokCitySourceGeometryProbe.h"),
		TEXT("Source/RokRendering/Private/RokCitySourceGeometryProbe.cpp"),
		TEXT("Shaders/Private/RokCitySourceGeometryProbe.usf") })
		Report->SetStringField(FPaths::GetCleanFilename(Relative) + TEXT("_sha256"), HashFile(FPaths::ProjectDir() / Relative));
	Report->SetBoolField(TEXT("opt_in_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokCitySourceGeometryAudit")));
	Report->SetBoolField(TEXT("local_only"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Report->SetBoolField(TEXT("scene_mutated"), false);
	Report->SetBoolField(TEXT("main_view_registered"), false);
	Report->SetBoolField(TEXT("gameapp_accepted"), false);
	Report->SetBoolField(TEXT("material_alpha_accepted"), false);
	Report->SetBoolField(TEXT("world_transform_accepted"), false);
	Report->SetBoolField(TEXT("input_assembler_accepted"), false);
	Report->SetNumberField(TEXT("http_dispatch_before"), static_cast<double>(InitialHttpDispatches));
	Report->SetNumberField(TEXT("http_dispatch_after"), static_cast<double>(FinalHttp));
	Report->SetNumberField(TEXT("expected_source_buffers"), BufferCount);
	Report->SetNumberField(TEXT("gpu_byte_exact_buffers"), ExactBuffers);
	Report->SetNumberField(TEXT("expected_frames"), RowCount);
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
		UE_LOG(LogTemp, Error, TEXT("Source geometry audit cannot save manifest: %s"), *Directory);
	UE_LOG(LogTemp, Display, TEXT("Source geometry audit %s buffers=%d frames=%d run=%s"),
		bPassed && bManifestSaved ? TEXT("PASSED") : TEXT("FAILED"), InputResults.Num(), FrameResults.Num(), *Directory);
	FPlatformMisc::RequestExitWithStatus(false, bPassed && bManifestSaved ? 0 : 1);
}
