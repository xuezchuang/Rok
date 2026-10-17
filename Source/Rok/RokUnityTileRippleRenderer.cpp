#include "RokUnityTileRippleRenderer.h"

#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#if WITH_EDITOR
#include "RenderingThread.h"
#include "TextureCompiler.h"
#endif
#include "TextureResource.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace RokTileRippleRenderer
{
constexpr TCHAR ContractRelative[] = TEXT("Saved/PendingUnityTileRippleRendererV8/geometry_contract.json");
constexpr TCHAR SourceCpuRelative[] = TEXT("Saved/PendingUnityTileRippleSimulationV2/tile_ripple_source_config.json");
constexpr TCHAR MaterialPath[] = TEXT("/Game/RokUnityWorldMapPrototype/TileRipple/M_RokTileRippleSourceAlpha.M_RokTileRippleSourceAlpha");
constexpr TCHAR TexturePath[] = TEXT("/Game/RokUnityWorldMapPrototype/TileRipple/T_Ring_07_Source.T_Ring_07_Source");
constexpr TCHAR WorldMap[] = TEXT("/Game/Maps/L_RokUnityWorldMapPrototype");
constexpr TCHAR ContractSha[] = TEXT("a3e2303e27a42a9244dfeef27a954df7e2d3cabe97a66f5147005d386e8fc46d");
constexpr TCHAR CpuSha[] = TEXT("cc365fb2e20dfaaa8da4aa9e0766e4754bffa6e2130cec053349ccc26b8ce126");
constexpr TCHAR AssetManifestRelative[] = TEXT("Saved/PendingUnityTileRippleRendererV8/asset_import_manifest.json");

bool IsRokProject()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

bool ReadPinned(const FString& Path, const FString& Expected, TArray<uint8>& Bytes)
{
	return FFileHelper::LoadFileToArray(Bytes, *Path) && RokTrainArmyMesh::Sha256(Bytes) == Expected;
}
bool Finite(const FVector3d& V)
{
	return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
}
FVector3d SourceToUe(const FVector3d& Source)
{
	return FVector3d(-Source.X, Source.Z, Source.Y);
}
bool ValidateImportedAssets(FString& Error)
{
	FString Expected;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokTileRippleAssetManifestSha="), Expected) || Expected.Len() != 64)
	{
		Error = TEXT("Tile ripple asset import manifest external SHA missing"); return false;
	}
	TArray<uint8> Bytes;
	if (!ReadPinned(FPaths::ProjectDir() / AssetManifestRelative, Expected.ToLower(), Bytes))
	{
		Error = TEXT("Tile ripple asset import manifest external SHA differs"); return false;
	}
	FString Text;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid() ||
		Json->GetStringField(TEXT("schema")) != TEXT("rok.tile_ripple.diagnostic_assets.v1") ||
		Json->GetArrayField(TEXT("packages")).Num() != 2)
	{
		Error = TEXT("Tile ripple asset import manifest schema differs"); return false;
	}
	const TCHAR* ExpectedPaths[] = {
		TEXT("Content/RokUnityWorldMapPrototype/TileRipple/T_Ring_07_Source.uasset"),
		TEXT("Content/RokUnityWorldMapPrototype/TileRipple/M_RokTileRippleSourceAlpha.uasset")};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TSharedPtr<FJsonObject> Row = Json->GetArrayField(TEXT("packages"))[Index]->AsObject();
		const FString Relative = Row->GetStringField(TEXT("path"));
		TArray<uint8> PackageBytes;
		if (Relative != ExpectedPaths[Index] || !ReadPinned(FPaths::ProjectDir() / Relative,
			Row->GetStringField(TEXT("sha256")), PackageBytes) ||
			PackageBytes.Num() != Row->GetIntegerField(TEXT("byte_count")))
		{
			Error = TEXT("Tile ripple imported package bytes differ: ") + Relative; return false;
		}
	}
	const FString Script = TEXT("Scripts/CreateRokTileRippleDiagnosticAssets.py");
	TArray<uint8> ScriptBytes;
	if (!ReadPinned(FPaths::ProjectDir() / Script,
		Json->GetStringField(TEXT("creation_script_sha256")), ScriptBytes))
	{
		Error = TEXT("Tile ripple asset creation script changed"); return false;
	}
	return true;
}
}

bool FRokTileRippleGeometryContract::LoadPinned(const FString& AbsolutePath,
	FRokTileRippleGeometryContract& Out, FString& Error)
{
	using namespace RokTileRippleRenderer;
	TArray<uint8> Bytes;
	if (!ReadPinned(AbsolutePath, ContractSha, Bytes))
	{
		Error = TEXT("Tile ripple renderer fixture hash differs"); return false;
	}
	FString Text;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
	TSharedPtr<FJsonObject> Data;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Data) || !Data.IsValid() ||
		Data->GetStringField(TEXT("schema")) != TEXT("rok.tile_ripple.renderer_geometry_fixture.v1") ||
		Data->GetBoolField(TEXT("runtime_frame_replay")) || Data->GetBoolField(TEXT("map_lod_accepted")) ||
		Data->GetArrayField(TEXT("systems")).Num() != 2 ||
		Data->GetArrayField(TEXT("source_anchor_xyz")).Num() != 3 ||
		Data->GetNumberField(TEXT("source_units_to_ue")) != 100.0)
	{
		Error = TEXT("Tile ripple renderer fixture identity differs"); return false;
	}
	for (const auto& PinValue : Data->GetArrayField(TEXT("pins")))
	{
		const TSharedPtr<FJsonObject> Pin = PinValue->AsObject();
		const FString Relative = Pin->GetStringField(TEXT("path"));
		if (Relative.IsEmpty() || Relative.Contains(TEXT("..")) || Relative.Contains(TEXT(":")) ||
			Relative.Contains(TEXT("\\")) || Relative.StartsWith(TEXT("/")) ||
			!(Relative.StartsWith(TEXT("Saved/")) || Relative.StartsWith(TEXT("ref/client-unity/Assets/BundleAssets/"))))
		{
			Error = TEXT("Tile ripple renderer source pin path rejected"); return false;
		}
		TArray<uint8> Original;
		if (!ReadPinned(FPaths::ProjectDir() / Relative, Pin->GetStringField(TEXT("sha256")), Original) ||
			Original.Num() != Pin->GetIntegerField(TEXT("byte_count")))
		{
			Error = TEXT("Tile ripple renderer source pin changed: ") + Relative; return false;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>& Anchor = Data->GetArrayField(TEXT("source_anchor_xyz"));
	Out.SourceAnchor = FVector3d(Anchor[0]->AsNumber(), Anchor[1]->AsNumber(), Anchor[2]->AsNumber());
	Out.SourceUnitsToUe = 100.0;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TSharedPtr<FJsonObject> Row = Data->GetArrayField(TEXT("systems"))[Index]->AsObject();
		FRokTileRippleEmitterGeometry& Emitter = Out.Emitters[Index];
		Emitter.SourceName = Row->GetStringField(TEXT("name"));
		Emitter.SourceFileId = FCString::Atoi64(*Row->GetStringField(TEXT("particle_system_file_id")));
		if (Emitter.SourceName != (Index == 0 ? TEXT("bowen") : TEXT("bowen (1)")) ||
			Emitter.SourceFileId != (Index == 0 ? 2408231316787934790LL : 144114848059114004LL))
		{
			Error = TEXT("Tile ripple emitter source identity changed"); return false;
		}
		const TArray<TSharedPtr<FJsonValue>>& Matrix = Row->GetArrayField(TEXT("source_local_to_world_row_major_f32"));
		const TArray<TSharedPtr<FJsonValue>>& Origin = Row->GetArrayField(TEXT("source_origin_xyz"));
		if (Matrix.Num() != 16 || Origin.Num() != 3)
		{
			Error = TEXT("Tile ripple source matrix dimensions changed"); return false;
		}
		Emitter.RowMajor4x4.Reset(16);
		for (const auto& Value : Matrix) Emitter.RowMajor4x4.Add(Value->AsNumber());
		Emitter.SourceOrigin = FVector3d(Origin[0]->AsNumber(), Origin[1]->AsNumber(), Origin[2]->AsNumber());
		if (!Finite(Emitter.SourceOrigin) ||
			!FMath::IsNearlyEqual(Emitter.RowMajor4x4[3], Emitter.SourceOrigin.X, 1e-5) ||
			!FMath::IsNearlyEqual(Emitter.RowMajor4x4[7], Emitter.SourceOrigin.Y, 1e-5) ||
			!FMath::IsNearlyEqual(Emitter.RowMajor4x4[11], Emitter.SourceOrigin.Z, 1e-5))
		{
			Error = TEXT("Tile ripple source matrix origin differs"); return false;
		}
		for (double V : Emitter.RowMajor4x4) if (!FMath::IsFinite(V))
		{
			Error = TEXT("Tile ripple source matrix nonfinite"); return false;
		}
	}
	return true;
}

FVector3d FRokTileRippleMeshData::SourceCenter(const FRokTileRippleEmitterGeometry& E,
	const FVector3d& P)
{
	const TArray<double>& M = E.RowMajor4x4;
	return FVector3d(M[0]*P.X + M[1]*P.Y + M[2]*P.Z,
		M[4]*P.X + M[5]*P.Y + M[6]*P.Z,
		M[8]*P.X + M[9]*P.Y + M[10]*P.Z);
}

bool FRokTileRippleMeshData::Build(const FRokTileRippleEmitterGeometry& E,
	const FVector3d& Anchor, double UnitsToUe,
	const TArray<FRokTileRippleParticleSnapshot>& Particles,
	FRokTileRippleMeshData& Out, FString& Error)
{
	using namespace RokTileRippleRenderer;
	Out = FRokTileRippleMeshData();
	if (E.RowMajor4x4.Num() != 16 || !Finite(E.SourceOrigin) || !Finite(Anchor) ||
		!FMath::IsFinite(UnitsToUe) || UnitsToUe <= 0 || Particles.Num() > 1000)
	{
		Error = TEXT("Tile ripple geometry input invalid"); return false;
	}
	const TArray<double>& M = E.RowMajor4x4;
	const FVector3d AxisX(M[0], M[4], M[8]);
	const FVector3d AxisZ(M[2], M[6], M[10]);
	for (const FRokTileRippleParticleSnapshot& P : Particles)
	{
		if (!Finite(P.LocalPosition) || !FMath::IsFinite(P.CurrentSize) ||
			P.CurrentSize < 0 || P.CurrentSize > 10000 ||
			!FMath::IsFinite(P.CurrentColor.R) || !FMath::IsFinite(P.CurrentColor.G) ||
			!FMath::IsFinite(P.CurrentColor.B) || !FMath::IsFinite(P.CurrentColor.A))
		{
			Error = TEXT("Tile ripple particle snapshot invalid"); return false;
		}
		const int32 Base = Out.Vertices.Num();
		const FVector3d Center = E.SourceOrigin + SourceCenter(E, P.LocalPosition);
		const double Half = P.CurrentSize * 0.5;
		for (const FIntPoint Sign : {FIntPoint(-1,-1), FIntPoint(1,-1), FIntPoint(-1,1), FIntPoint(1,1)})
		{
			const FVector3d SourceVertex = Center + Half *
				(static_cast<double>(Sign.X) * AxisX + static_cast<double>(Sign.Y) * AxisZ);
			Out.Vertices.Add(FVector(SourceToUe(SourceVertex - Anchor) * UnitsToUe));
			Out.Normals.Add(FVector::UpVector);
			Out.UV0.Add(FVector2D(Sign.X > 0 ? 1.0 : 0.0, Sign.Y > 0 ? 0.0 : 1.0));
			Out.Colors.Add(P.CurrentColor.ToFColor(false));
		}
		for (int32 Local : {0,3,1,3,0,2}) Out.Triangles.Add(Base + Local);
	}
	return true;
}

ARokUnityTileRippleDiagnosticActor::ARokUnityTileRippleDiagnosticActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("TileRippleRoot"));
	RootComponent = Root;
	Bowen = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BowenSourceMesh"));
	Bowen1 = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Bowen1SourceMesh"));
	for (UProceduralMeshComponent* Mesh : {Bowen.Get(), Bowen1.Get()})
	{
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetReceivesDecals(false);
	}
	SetActorEnableCollision(false);
}

void ARokUnityTileRippleDiagnosticActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	URokServerSubsystem* Server = Game ? Game->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!World || World->WorldType != EWorldType::Game ||
		World->GetOutermost()->GetName() != RokTileRippleRenderer::WorldMap ||
		!RokTileRippleRenderer::IsRokProject() ||
		!FParse::Param(FCommandLine::Get(), TEXT("RokTileRippleDiagnostic")) ||
		!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) ||
		!Server || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0)
		return;
	FString Error;
	if (!FRokTileRippleGeometryContract::LoadPinned(
		FPaths::ProjectDir() / RokTileRippleRenderer::ContractRelative, Geometry, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("TileRipple diagnostic geometry: %s"), *Error); return;
	}
	TArray<uint8> CpuBytes;
	if (!RokTileRippleRenderer::ReadPinned(FPaths::ProjectDir() / RokTileRippleRenderer::SourceCpuRelative,
		RokTileRippleRenderer::CpuSha, CpuBytes)) return;
	if (!RokTileRippleRenderer::ValidateImportedAssets(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("TileRipple imported assets: %s"), *Error); return;
	}
	TArray<FRokTileRippleSourceConfig> Configs;
	if (!FRokTileRippleSourceConfig::LoadSourceFixture(
		FPaths::ProjectDir() / RokTileRippleRenderer::SourceCpuRelative, Configs, Error) ||
		Configs.Num() != 2) return;
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, RokTileRippleRenderer::MaterialPath);
	SourceTexture = LoadObject<UTexture2D>(nullptr, RokTileRippleRenderer::TexturePath);
	if (!Material || !SourceTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("TileRipple source material or Ring_07 texture missing: material=%d texture=%d"),
			Material != nullptr, SourceTexture != nullptr); return;
	}
	const int32 WidthBefore = SourceTexture->GetSizeX();
	const int32 HeightBefore = SourceTexture->GetSizeY();
	bool bCompilingBefore = false;
	bool bDefaultBefore = false;
	bool bCompilingAfter = false;
	bool bDefaultAfter = false;
#if WITH_EDITOR
	bCompilingBefore = SourceTexture->IsCompiling();
	bDefaultBefore = SourceTexture->IsDefaultTexture();
	if (bCompilingBefore || bDefaultBefore)
	{
		FTextureCompilingManager::Get().FinishCompilation({SourceTexture.Get()});
		FlushRenderingCommands();
	}
	bCompilingAfter = SourceTexture->IsCompiling();
	bDefaultAfter = SourceTexture->IsDefaultTexture();
#endif
	const FTextureResource* ResourceBeforeStreaming = SourceTexture->GetResource();
	const FTextureRHIRef RhiBeforeStreaming = ResourceBeforeStreaming ? ResourceBeforeStreaming->TextureRHI : FTextureRHIRef();
	const int32 RhiWidthBeforeStreaming = RhiBeforeStreaming.IsValid() ? RhiBeforeStreaming->GetDesc().Extent.X : 0;
	const int32 RhiHeightBeforeStreaming = RhiBeforeStreaming.IsValid() ? RhiBeforeStreaming->GetDesc().Extent.Y : 0;
	const int32 ResidentMipsBefore = SourceTexture->GetNumResidentMips();
	// This diagnostic requires mip zero from this source texture only; do not change global streaming policy.
	SourceTexture->SetForceMipLevelsToBeResident(60.0f);
	SourceTexture->WaitForStreaming();
	const FTextureResource* Resource = SourceTexture->GetResource();
	const FTextureRHIRef Rhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
	const int32 RhiWidth = Rhi.IsValid() ? Rhi->GetDesc().Extent.X : 0;
	const int32 RhiHeight = Rhi.IsValid() ? Rhi->GetDesc().Extent.Y : 0;
	const int32 ResidentMipsAfter = SourceTexture->GetNumResidentMips();
	const int32 TotalMips = SourceTexture->GetNumMips();
	const int32 LodBias = SourceTexture->GetCachedLODBias();
	const bool bForceResident = SourceTexture->ShouldMipLevelsBeForcedResident();
	const int32 WidthAfter = SourceTexture->GetSizeX();
	const int32 HeightAfter = SourceTexture->GetSizeY();
	UE_LOG(LogTemp, Display, TEXT("TileRipple Ring_07 source load: size=%dx%d -> %dx%d rhi=%dx%d -> %dx%d resident_mips=%d->%d total_mips=%d lod_bias=%d force_resident=%d compiling=%d->%d default=%d->%d srgb=%d"),
		WidthBefore, HeightBefore, WidthAfter, HeightAfter, RhiWidthBeforeStreaming, RhiHeightBeforeStreaming,
		RhiWidth, RhiHeight, ResidentMipsBefore, ResidentMipsAfter, TotalMips, LodBias, bForceResident,
		bCompilingBefore, bCompilingAfter, bDefaultBefore, bDefaultAfter, SourceTexture->SRGB);
	if (WidthAfter != 128 || HeightAfter != 128 || RhiWidth != 128 || RhiHeight != 128 ||
		bCompilingAfter || bDefaultAfter || !bForceResident || !SourceTexture->SRGB)
	{
		UE_LOG(LogTemp, Error, TEXT("TileRipple Ring_07 source texture not ready: size=%dx%d -> %dx%d rhi=%dx%d -> %dx%d resident_mips=%d->%d total_mips=%d lod_bias=%d force_resident=%d compiling=%d->%d default=%d->%d srgb=%d"),
			WidthBefore, HeightBefore, WidthAfter, HeightAfter, RhiWidthBeforeStreaming, RhiHeightBeforeStreaming,
			RhiWidth, RhiHeight, ResidentMipsBefore, ResidentMipsAfter, TotalMips, LodBias, bForceResident,
			bCompilingBefore, bCompilingAfter, bDefaultBefore, bDefaultAfter, SourceTexture->SRGB); return;
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (Configs[Index].SystemName != Geometry.Emitters[Index].SourceName ||
			Configs[Index].ParticleSystemFileId != Geometry.Emitters[Index].SourceFileId) return;
		Simulations[Index] = MakeUnique<FRokUnityTileRippleSimulation>(Configs[Index]);
		if (!Simulations[Index]->Reset(424242, Error)) return;
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Material, this);
		if (!Mid) return;
		Mid->SetTextureParameterValue(TEXT("RingTexture"), SourceTexture);
		(Index == 0 ? Bowen.Get() : Bowen1.Get())->SetMaterial(0, Mid);
		(Index == 0 ? BowenMaterial : Bowen1Material) = Mid;
	}
	bReady = true;
	SetActorTickEnabled(true);
}

void ARokUnityTileRippleDiagnosticActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bReady || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0) return;
	FString Error;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!Simulations[Index]->Advance(static_cast<double>(DeltaSeconds), Error))
		{
			UE_LOG(LogTemp, Error, TEXT("TileRipple simulation stopped: %s"), *Error);
			bReady = false; SetActorTickEnabled(false); return;
		}
		UpdateEmitter(Index);
	}
}

void ARokUnityTileRippleDiagnosticActor::UpdateEmitter(int32 Index)
{
	TArray<FRokTileRippleParticleSnapshot> Particles;
	Simulations[Index]->Snapshot(Particles);
	FRokTileRippleMeshData MeshData;
	FString Error;
	UProceduralMeshComponent* Mesh = Index == 0 ? Bowen.Get() : Bowen1.Get();
	if (!FRokTileRippleMeshData::Build(Geometry.Emitters[Index], Geometry.SourceAnchor,
		Geometry.SourceUnitsToUe, Particles, MeshData, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("TileRipple mesh stopped: %s"), *Error);
		bReady = false; SetActorTickEnabled(false); return;
	}
	Mesh->ClearAllMeshSections();
	if (!MeshData.Vertices.IsEmpty())
		Mesh->CreateMeshSection(0, MeshData.Vertices, MeshData.Triangles,
			MeshData.Normals, MeshData.UV0, MeshData.Colors, TArray<FProcMeshTangent>(), false);
}

void ARokUnityTileRippleDiagnosticActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bReady = false;
	for (UProceduralMeshComponent* Mesh : {Bowen.Get(), Bowen1.Get()}) if (Mesh) Mesh->ClearAllMeshSections();
	for (auto& Sim : Simulations) Sim.Reset();
	Super::EndPlay(EndPlayReason);
}

int32 ARokUnityTileRippleDiagnosticActor::GetLiveParticleCount(int32 Index) const
{
	return Index >= 0 && Index < 2 && Simulations[Index] ? Simulations[Index]->GetLiveCount() : 0;
}

double ARokUnityTileRippleDiagnosticActor::GetSimElapsedSeconds() const
{
	return Simulations[0] ? Simulations[0]->GetElapsedSeconds() : 0.0;
}

UProceduralMeshComponent* ARokUnityTileRippleDiagnosticActor::GetEmitterMesh(int32 Index) const
{
	return Index == 0 ? Bowen.Get() : Index == 1 ? Bowen1.Get() : nullptr;
}
