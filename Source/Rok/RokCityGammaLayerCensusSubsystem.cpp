#include "RokCityGammaLayerCensusSubsystem.h"

#include "RokTrainArmyMeshData.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "TextureResource.h"
#include "UnrealClient.h"

namespace RokCityGammaLayerCensus
{
constexpr TCHAR MapName[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR ProjectName[] = TEXT("D:/ueDemo/Rok/");

TArray<TSharedPtr<FJsonValue>> Vector3(const FVector& V)
{
	return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
}

TSharedPtr<FJsonObject> Transform(const FTransform& T)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("location_cm"), Vector3(T.GetLocation()));
	Result->SetArrayField(TEXT("scale"), Vector3(T.GetScale3D()));
	const FQuat Q = T.GetRotation();
	Result->SetArrayField(TEXT("rotation_xyzw"), { MakeShared<FJsonValueNumber>(Q.X),
		MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W) });
	return Result;
}

void AddTags(const TArray<FName>& Tags, const TSharedRef<FJsonObject>& Result)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FName Tag : Tags) Values.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	Result->SetArrayField(TEXT("tags"), Values);
}

bool HasTagPrefix(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName Tag : Actor->Tags)
		if (Tag.ToString().StartsWith(Prefix)) return true;
	return false;
}

bool EffectiveVisible(const UPrimitiveComponent* Component)
{
	if (!Component || !Component->IsRegistered() || !Component->GetOwner() || Component->GetOwner()->IsHidden()) return false;
	for (const USceneComponent* Node = Component; Node; Node = Node->GetAttachParent())
		if (!Node->IsRegistered() || !Node->IsVisible() || Node->bHiddenInGame) return false;
	return true;
}

int32 TaggedId(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return INDEX_NONE;
	for (const FName Tag : Actor->Tags)
	{
		const FString Value = Tag.ToString();
		if (Value.StartsWith(Prefix)) return FCString::Atoi(*Value.RightChop(FCString::Strlen(Prefix)));
	}
	return INDEX_NONE;
}

FString Category(const AActor* Actor)
{
	if (!Actor) return TEXT("unknown");
	if (Actor->ActorHasTag(TEXT("RokPlacementPreviewTemporary"))) return TEXT("placement_preview");
	if (Actor->ActorHasTag(TEXT("RokLocalConstructionPending")) || HasTagPrefix(Actor, TEXT("RokLocalBuildingId_")) ||
		Actor->ActorHasTag(TEXT("RokLocalSourceLevel_1"))) return TEXT("local_new_construction");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeInteriorPreview"))) return TEXT("supplemental_interior_preview");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeCitySupportLayer"))) return TEXT("city_support_layer");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeCityGateFlag"))) return TEXT("city_gate_flag");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeGroundSource"))) return TEXT("source_ground");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeBoundarySource"))) return TEXT("source_boundary");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeCameraSource"))) return TEXT("source_camera");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimePlayerStart"))) return TEXT("source_player_start");
	if (Actor->ActorHasTag(TEXT("RokUnityBuildingVisualLayer"))) return TEXT("legacy_building_visual_support");
	if (Actor->ActorHasTag(TEXT("RokUnityConfigInitBuilding"))) return TEXT("initial_source_building");
	if (Actor->ActorHasTag(TEXT("RokUnityRuntimeCityAllBuildings"))) return TEXT("other_city_source_or_diagnostic");
	return TEXT("other_world_primitive");
}

bool Identity(const UWorld* World)
{
	return World && World->WorldType == EWorldType::Game && World->GetOutermost()->GetName() == MapName &&
		FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), ProjectName) &&
		World->GetGameViewport() && World->GetGameViewport()->Viewport &&
		World->GetGameViewport()->Viewport->GetSizeXY().X > 0 &&
		World->GetGameViewport()->Viewport->GetSizeXY().Y > 0 && World->GetFirstPlayerController();
}

template <typename T> void AppendRaw(TArray<uint8>& Bytes, const T& Value)
{
	Bytes.Append(reinterpret_cast<const uint8*>(&Value), sizeof(T));
}

FString Sha256(const TArray<uint8>& Bytes)
{
	return RokTrainArmyMesh::Sha256(Bytes);
}

FString Sha256File(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? Sha256(Bytes) : FString();
}

void AddAssetFileHash(const UObject* Asset, const TSharedRef<FJsonObject>& Result)
{
	if (!Asset || !Asset->GetOutermost()) return;
	const FString Package = Asset->GetOutermost()->GetName();
	if (!FPackageName::IsValidLongPackageName(Package))
	{
		Result->SetStringField(TEXT("asset_file_hash_status"), TEXT("non_package_object"));
		return;
	}
	const FString Path = FPackageName::LongPackageNameToFilename(Package, TEXT(".uasset"));
	const int64 Size = IFileManager::Get().FileSize(*Path);
	Result->SetNumberField(TEXT("asset_file_bytes"), static_cast<double>(Size));
	if (Size <= 0 || Size > 64 * 1024 * 1024)
	{
		Result->SetStringField(TEXT("asset_file_hash_status"), Size < 0 ? TEXT("file_unavailable") : TEXT("over_64_mib_read_limit"));
		return;
	}
	Result->SetStringField(TEXT("asset_file_sha256"), Sha256File(Path));
}

TSharedPtr<FJsonObject> MeshRecord(UStaticMesh* Mesh)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset"), GetPathNameSafe(Mesh));
	AddAssetFileHash(Mesh, Result);
	Result->SetBoolField(TEXT("cpu_access_enabled"), Mesh && Mesh->bAllowCPUAccess);
	const FStaticMeshRenderData* Render = Mesh ? Mesh->GetRenderData() : nullptr;
	if (!Render || Render->LODResources.IsEmpty())
	{
		Result->SetStringField(TEXT("geometry_status"), TEXT("missing_lod0_render_data"));
		return Result;
	}
	const FStaticMeshLODResources& LOD = Render->LODResources[0];
	const int32 VertexCount = LOD.GetNumVertices();
	const int32 IndexCount = LOD.IndexBuffer.GetNumIndices();
	const int32 UvCount = LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
	Result->SetNumberField(TEXT("lod0_vertices"), VertexCount);
	Result->SetNumberField(TEXT("lod0_indices"), IndexCount);
	Result->SetNumberField(TEXT("lod0_triangles"), LOD.GetNumTriangles());
	Result->SetNumberField(TEXT("lod0_uv_channels"), UvCount);
	TArray<TSharedPtr<FJsonValue>> Sections;
	for (int32 Index = 0; Index < LOD.Sections.Num(); ++Index)
	{
		const FStaticMeshSection& Section = LOD.Sections[Index];
		auto Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("section"), Index);
		Item->SetNumberField(TEXT("material_slot"), Section.MaterialIndex);
		Item->SetNumberField(TEXT("first_index"), Section.FirstIndex);
		Item->SetNumberField(TEXT("triangles"), Section.NumTriangles);
		Item->SetNumberField(TEXT("min_vertex"), Section.MinVertexIndex);
		Item->SetNumberField(TEXT("max_vertex"), Section.MaxVertexIndex);
		Item->SetBoolField(TEXT("casts_shadow"), Section.bCastShadow);
		Item->SetBoolField(TEXT("force_opaque"), Section.bForceOpaque);
		Sections.Add(MakeShared<FJsonValueObject>(Item));
	}
	Result->SetArrayField(TEXT("lod0_sections"), Sections);
	const bool bCpuBuffersPresent = LOD.VertexBuffers.PositionVertexBuffer.GetVertexData() &&
		LOD.VertexBuffers.StaticMeshVertexBuffer.GetTexCoordData() &&
		LOD.IndexBuffer.GetArrayView().Num() == IndexCount;
	if (!bCpuBuffersPresent || VertexCount <= 0 || IndexCount <= 0 || UvCount <= 0)
	{
		Result->SetStringField(TEXT("geometry_status"), bCpuBuffersPresent ? TEXT("empty_or_no_uv") : TEXT("cpu_vertex_uv_or_index_buffer_unavailable"));
		return Result;
	}
	TArray<uint8> PositionBytes, UvBytes, IndexBytes;
	for (int32 Vertex = 0; Vertex < VertexCount; ++Vertex)
	{
		const FVector3f Position = LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Vertex);
		AppendRaw(PositionBytes, Position.X); AppendRaw(PositionBytes, Position.Y); AppendRaw(PositionBytes, Position.Z);
		for (int32 Channel = 0; Channel < UvCount; ++Channel)
		{
			const FVector2f UV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Vertex, Channel);
			AppendRaw(UvBytes, UV.X); AppendRaw(UvBytes, UV.Y);
		}
	}
	for (int32 Index = 0; Index < IndexCount; ++Index)
	{
		const uint32 Value = LOD.IndexBuffer.GetIndex(Index);
		AppendRaw(IndexBytes, Value);
	}
	Result->SetStringField(TEXT("geometry_status"), TEXT("cpu_lod0_hashed"));
	Result->SetStringField(TEXT("position_f32_xyz_sha256"), Sha256(PositionBytes));
	Result->SetStringField(TEXT("uv_f32_per_vertex_channel_sha256"), Sha256(UvBytes));
	Result->SetStringField(TEXT("index_u32_sha256"), Sha256(IndexBytes));
	Result->SetStringField(TEXT("hash_encoding"), TEXT("little_endian IEEE754 float32 positions and UV; uint32 indices; render LOD0 order"));
	return Result;
}

TSharedPtr<FJsonObject> TextureRecord(UTexture* Texture)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("asset"), GetPathNameSafe(Texture));
	AddAssetFileHash(Texture, Result);
	Result->SetStringField(TEXT("class"), Texture ? Texture->GetClass()->GetPathName() : FString());
	if (!Texture) return Result;
	Result->SetBoolField(TEXT("asset_srgb"), Texture->SRGB);
	Result->SetNumberField(TEXT("asset_filter"), static_cast<int32>(Texture->Filter.GetValue()));
	Result->SetNumberField(TEXT("asset_compression"), static_cast<int32>(Texture->CompressionSettings.GetValue()));
	Result->SetNumberField(TEXT("asset_lod_group"), static_cast<int32>(Texture->LODGroup.GetValue()));
	if (const UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
	{
		Result->SetNumberField(TEXT("asset_width"), Texture2D->GetSizeX());
		Result->SetNumberField(TEXT("asset_height"), Texture2D->GetSizeY());
		Result->SetNumberField(TEXT("asset_mips"), Texture2D->GetNumMips());
		Result->SetNumberField(TEXT("asset_pixel_format"), static_cast<int32>(Texture2D->GetPixelFormat()));
	}
	else Result->SetStringField(TEXT("support"), TEXT("non_texture2d_recorded_without_2d_descriptor"));
	const FTextureResource* Resource = Texture->GetResource();
	const FTextureRHIRef Rhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
	if (Rhi.IsValid())
	{
		const FRHITextureDesc& Desc = Rhi->GetDesc();
		Result->SetNumberField(TEXT("rhi_width"), Desc.Extent.X);
		Result->SetNumberField(TEXT("rhi_height"), Desc.Extent.Y);
		Result->SetNumberField(TEXT("rhi_format"), static_cast<int32>(Desc.Format));
		Result->SetNumberField(TEXT("rhi_mips"), Desc.NumMips);
		Result->SetNumberField(TEXT("rhi_samples"), Desc.NumSamples);
		Result->SetBoolField(TEXT("rhi_srgb"), !!(Desc.Flags & TexCreate_SRGB));
	}
	else Result->SetStringField(TEXT("rhi_status"), TEXT("resource_or_rhi_unavailable_at_capture"));
	return Result;
}

TSharedPtr<FJsonObject> MaterialRecord(UMaterialInterface* Material, TMap<FString, TSharedPtr<FJsonObject>>& Textures)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("interface"), GetPathNameSafe(Material));
	AddAssetFileHash(Material, Result);
	if (!Material) return Result;
	const UMaterial* Base = Material->GetMaterial();
	Result->SetStringField(TEXT("interface_class"), Material->GetClass()->GetPathName());
	Result->SetStringField(TEXT("base_material"), GetPathNameSafe(Base));
	if (Base && Base != Material)
	{
		auto BaseFile = MakeShared<FJsonObject>();
		AddAssetFileHash(Base, BaseFile);
		Result->SetObjectField(TEXT("base_asset_file"), BaseFile);
	}
	Result->SetNumberField(TEXT("effective_blend_mode"), static_cast<int32>(Material->GetBlendMode()));
	Result->SetBoolField(TEXT("effective_two_sided"), Material->IsTwoSided());
	if (Base)
	{
		Result->SetNumberField(TEXT("base_translucency_pass"), static_cast<int32>(Base->TranslucencyPass.GetValue()));
		Result->SetBoolField(TEXT("base_disable_depth_test"), Base->bDisableDepthTest);
		Result->SetBoolField(TEXT("base_write_only_alpha"), Base->bWriteOnlyAlpha);
	}
	else Result->SetStringField(TEXT("pass_status"), TEXT("base_material_unavailable"));
	TArray<UTexture*> UsedTextures;
	Material->GetUsedTextures(UsedTextures);
	TArray<TSharedPtr<FJsonValue>> TexturePaths;
	for (UTexture* Texture : UsedTextures)
	{
		const FString Path = GetPathNameSafe(Texture);
		TexturePaths.Add(MakeShared<FJsonValueString>(Path));
		if (Texture && !Textures.Contains(Path)) Textures.Add(Path, TextureRecord(Texture));
	}
	Result->SetArrayField(TEXT("used_texture_assets"), TexturePaths);
	Result->SetStringField(TEXT("texture_scope"), TEXT("GetUsedTextures across quality/platform; actual runtime binding may differ for material instances/parameters"));
	return Result;
}

FString StableSignature(UWorld* World)
{
	TArray<FString> Paths;
	const FIntPoint Size = World->GetGameViewport()->Viewport->GetSizeXY();
	Paths.Add(FString::Printf(TEXT("viewport:%d,%d"), Size.X, Size.Y));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("RokUnityRuntimeCityAllBuildings"))) continue;
		Paths.Add(It->GetPathName());
		TInlineComponentArray<UPrimitiveComponent*> Components;
		It->GetComponents(Components);
		for (const UPrimitiveComponent* Component : Components)
			if (Component && Component->IsRegistered()) Paths.Add(Component->GetPathName());
	}
	Paths.Sort();
	const FString Joined = FString::Join(Paths, TEXT("\n"));
	FTCHARToUTF8 Utf8(*Joined);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	return Sha256(Bytes);
}

}

bool URokCityGammaLayerCensusSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->GetOutermost()->GetName() == RokCityGammaLayerCensus::MapName &&
		FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), RokCityGammaLayerCensus::ProjectName) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaLayerCensus")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokCityGammaLayerCensusSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}

void URokCityGammaLayerCensusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

TStatId URokCityGammaLayerCensusSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityGammaLayerCensusSubsystem, STATGROUP_Tickables);
}

void URokCityGammaLayerCensusSubsystem::Tick(float DeltaTime)
{
	if (bAutomaticCaptureDone) return;
	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	UWorld* World = GetWorld();
	if (!RokCityGammaLayerCensus::Identity(World)) return;
	const FString Signature = RokCityGammaLayerCensus::StableSignature(World);
	if (Signature == BaselineSignature) StableSeconds += FMath::Max(0.0f, DeltaTime);
	else { BaselineSignature = Signature; StableSeconds = 0.0f; }
	if (ElapsedSeconds >= 2.0f && StableSeconds >= 0.5f)
	{
		FString Path;
		const bool bCaptured = CaptureNow(TEXT("automatic_stable_gameapp"), Path);
		bAutomaticCaptureDone = true;
		UE_LOG(LogTemp, Display, TEXT("Rok City Gamma layer census %s: %s"), bCaptured ? TEXT("written") : TEXT("failed"), *Path);
	}
	else if (ElapsedSeconds > 30.0f)
	{
		FString Path;
		CaptureNow(TEXT("automatic_timeout_unstable_components"), Path);
		bAutomaticCaptureDone = true;
		UE_LOG(LogTemp, Error, TEXT("Rok City Gamma layer census timed out waiting for stable GameApp components: %s"), *Path);
	}
}

bool URokCityGammaLayerCensusSubsystem::CaptureNow(const FString& Reason, FString& OutManifestPath)
{
	OutManifestPath.Reset();
	UWorld* World = GetWorld();
	if (!RokCityGammaLayerCensus::Identity(World)) return false;
	using namespace RokCityGammaLayerCensus;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/CityGammaLayerCensus") /
		FString::Printf(TEXT("%s_%u_%02d"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId(), ++CaptureSerial);
	if (!IFileManager::Get().MakeDirectory(*Directory, true)) return false;
	OutManifestPath = Directory / TEXT("manifest.json");
	auto Manifest = MakeShared<FJsonObject>();
	Manifest->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_layer_census.v1"));
	Manifest->SetStringField(TEXT("scope"), TEXT("Actual GameApp read-only registered primitive census; not Unity equivalent rendering or Gamma raster acceptance"));
	Manifest->SetStringField(TEXT("reason"), Reason);
	Manifest->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
	Manifest->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Manifest->SetStringField(TEXT("map"), World->GetOutermost()->GetName());
	const FIntPoint ViewportSize = World->GetGameViewport()->Viewport->GetSizeXY();
	Manifest->SetArrayField(TEXT("viewport_xy"), { MakeShared<FJsonValueNumber>(ViewportSize.X),
		MakeShared<FJsonValueNumber>(ViewportSize.Y) });
	auto Inputs = MakeShared<FJsonObject>();
	bool bInputsPresent = true;
	for (const TCHAR* Relative : { TEXT("Source/Rok/RokCityGammaLayerCensusSubsystem.h"),
		TEXT("Source/Rok/RokCityGammaLayerCensusSubsystem.cpp"),
		TEXT("Content/Maps/L_RokUnityGameAppCity.umap") })
	{
		const FString Hash = Sha256File(FPaths::ProjectDir() / Relative);
		bInputsPresent &= !Hash.IsEmpty();
		Inputs->SetStringField(Relative, Hash);
	}
	Manifest->SetObjectField(TEXT("input_sha256"), Inputs);
	Manifest->SetBoolField(TEXT("input_hashes_complete"), bInputsPresent);
	auto ReferenceLinks = MakeShared<FJsonObject>();
	for (const TCHAR* Relative : { TEXT("Saved/RokUnityMigration/unity_initial_city_state.json"),
		TEXT("Saved/RokUnityMigration/CityConstructionSources/catalog.json"),
		TEXT("Scripts/CreateRokUnityRuntimeCityAllBuildingsScene.py"),
		TEXT("Scripts/VerifyRokUnityRuntimeCityAllBuildingsScene.py"),
		TEXT("Saved/RokUnityMigration/unity_runtime_city_all_buildings_verify.json"),
		TEXT("Saved/RokUnityMigration/unity_runtime_city_all_buildings_verify_GameAppRuntimeV6Color.json") })
		ReferenceLinks->SetStringField(Relative, Sha256File(FPaths::ProjectDir() / Relative));
	Manifest->SetObjectField(TEXT("candidate_source_reference_sha256"), ReferenceLinks);
	Manifest->SetStringField(TEXT("source_reference_limit"), TEXT("Path/hash links only; captured UE materials and textures are not declared semantically identical to Unity source"));
	Manifest->SetStringField(TEXT("component_set_sha256"), StableSignature(World));
	Manifest->SetNumberField(TEXT("stable_seconds"), StableSeconds);
	Manifest->SetBoolField(TEXT("no_scene_mutations_or_gamma_registration"), true);
	TArray<TSharedPtr<FJsonValue>> ComponentRows;
	TMap<FString, TSharedPtr<FJsonObject>> Meshes, Materials, Textures;
	TMap<FString, int32> Categories;
	TMap<FString, int32> ActorCategories;
	TSet<int32> InitialIds;
	TSet<int32> InteriorPreviewIds;
	TArray<TSharedPtr<FJsonValue>> SourceActorRows;
	int32 SourceActorCount = 0;
	int32 InvalidOrOverlappingSourceIds = 0;
	int32 VisibleCount = 0;
	int32 UnsupportedVisibleCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		const FString Role = Category(Actor);
		const bool bCitySource = Actor->ActorHasTag(TEXT("RokUnityRuntimeCityAllBuildings"));
		if (bCitySource)
		{
			++SourceActorCount;
			ActorCategories.FindOrAdd(Role)++;
			auto ActorRow = MakeShared<FJsonObject>();
			ActorRow->SetStringField(TEXT("actor"), Actor->GetPathName());
			ActorRow->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetPathName());
			ActorRow->SetStringField(TEXT("category"), Role);
			AddTags(Actor->Tags, ActorRow);
			if (Role == TEXT("initial_source_building"))
			{
				const int32 Id = TaggedId(Actor, TEXT("UnityInitId_"));
				ActorRow->SetNumberField(TEXT("unity_init_id"), Id);
				if (Id > 0 && !InitialIds.Contains(Id)) InitialIds.Add(Id);
				else ++InvalidOrOverlappingSourceIds;
			}
			if (Role == TEXT("supplemental_interior_preview"))
			{
				const int32 Id = TaggedId(Actor, TEXT("UnityInteriorPreviewId_"));
				ActorRow->SetNumberField(TEXT("unity_interior_preview_id"), Id);
				if (Id > 0 && !InteriorPreviewIds.Contains(Id) &&
					!Actor->ActorHasTag(TEXT("RokUnityConfigInitBuilding")) &&
					!HasTagPrefix(Actor, TEXT("UnityInitId_"))) InteriorPreviewIds.Add(Id);
				else ++InvalidOrOverlappingSourceIds;
			}
			SourceActorRows.Add(MakeShared<FJsonValueObject>(ActorRow));
		}
		TInlineComponentArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (!Component || !Component->IsRegistered()) continue;
			const bool bVisible = EffectiveVisible(Component);
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("actor"), Actor->GetPathName());
			Row->SetStringField(TEXT("actor_class"), Actor->GetClass()->GetPathName());
			Row->SetStringField(TEXT("component"), Component->GetPathName());
			Row->SetStringField(TEXT("component_class"), Component->GetClass()->GetPathName());
			Row->SetStringField(TEXT("category"), Role);
			Row->SetBoolField(TEXT("effective_visible_attach_chain"), bVisible);
			Row->SetBoolField(TEXT("actor_hidden"), Actor->IsHidden());
			Row->SetBoolField(TEXT("component_visible"), Component->IsVisible());
			Row->SetBoolField(TEXT("component_hidden_in_game"), Component->bHiddenInGame);
			Row->SetBoolField(TEXT("render_in_main_pass"), Component->bRenderInMainPass);
			Row->SetNumberField(TEXT("translucency_sort_priority"), Component->TranslucencySortPriority);
			Row->SetBoolField(TEXT("render_custom_depth"), Component->bRenderCustomDepth);
			Row->SetBoolField(TEXT("casts_shadow"), Component->CastShadow);
			Row->SetBoolField(TEXT("receives_decals"), Component->bReceivesDecals);
			Row->SetObjectField(TEXT("world_transform"), Transform(Component->GetComponentTransform()));
			const FBoxSphereBounds Bounds = Component->Bounds;
			Row->SetArrayField(TEXT("bounds_origin_cm"), Vector3(Bounds.Origin));
			Row->SetArrayField(TEXT("bounds_extent_cm"), Vector3(Bounds.BoxExtent));
			Row->SetNumberField(TEXT("bounds_sphere_radius_cm"), Bounds.SphereRadius);
			AddTags(Actor->Tags, Row);
			TArray<TSharedPtr<FJsonValue>> ComponentTags;
			TArray<TSharedPtr<FJsonValue>> Unsupported;
			for (const FName Tag : Component->ComponentTags) ComponentTags.Add(MakeShared<FJsonValueString>(Tag.ToString()));
			Row->SetArrayField(TEXT("component_tags"), ComponentTags);
			TArray<TSharedPtr<FJsonValue>> MaterialBindings;
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
			UMaterialInterface* Material = Component->GetMaterial(Slot);
			const FString Path = GetPathNameSafe(Material);
			auto Binding = MakeShared<FJsonObject>();
			Binding->SetNumberField(TEXT("slot"), Slot);
			Binding->SetStringField(TEXT("interface"), Path);
				Binding->SetStringField(TEXT("status"), Material ? TEXT("resolved") : TEXT("null_effective_material"));
			MaterialBindings.Add(MakeShared<FJsonValueObject>(Binding));
			if (Material && !Materials.Contains(Path)) Materials.Add(Path, MaterialRecord(Material, Textures));
				if (!Material) Unsupported.Add(MakeShared<FJsonValueString>(TEXT("null_effective_material")));
		}
			Row->SetArrayField(TEXT("material_bindings"), MaterialBindings);
			if (UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Component))
			{
				Row->SetBoolField(TEXT("reverse_culling"), Static->IsReverseCulling());
				UStaticMesh* Mesh = Static->GetStaticMesh();
				const FString Path = GetPathNameSafe(Mesh);
				Row->SetStringField(TEXT("mesh_asset"), Path);
				Row->SetStringField(TEXT("geometry_support"), Mesh ? TEXT("static_mesh_lod0") : TEXT("static_mesh_null"));
				if (Mesh && !Meshes.Contains(Path)) Meshes.Add(Path, MeshRecord(Mesh));
				if (!Mesh) Unsupported.Add(MakeShared<FJsonValueString>(TEXT("static_mesh_null")));
			}
			else
			{
				Row->SetStringField(TEXT("geometry_support"), TEXT("unsupported_primitive_class_explicitly_recorded"));
				Unsupported.Add(MakeShared<FJsonValueString>(TEXT("non_static_mesh_geometry_requires_separate_extractor")));
				if (bVisible) ++UnsupportedVisibleCount;
			}
			Row->SetArrayField(TEXT("unsupported"), Unsupported);
			if (bVisible) ++VisibleCount;
			Categories.FindOrAdd(Role)++;
			ComponentRows.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	ComponentRows.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("component")) < B->AsObject()->GetStringField(TEXT("component"));
	});
	Manifest->SetArrayField(TEXT("components"), ComponentRows);
	SourceActorRows.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		return A->AsObject()->GetStringField(TEXT("actor")) < B->AsObject()->GetStringField(TEXT("actor"));
	});
	Manifest->SetArrayField(TEXT("city_source_actors"), SourceActorRows);
	auto ToObject = [](const TMap<FString, TSharedPtr<FJsonObject>>& Rows)
	{
		auto Result = MakeShared<FJsonObject>();
		for (const TPair<FString, TSharedPtr<FJsonObject>>& Entry : Rows) Result->SetObjectField(Entry.Key, Entry.Value);
		return Result;
	};
	Manifest->SetObjectField(TEXT("meshes"), ToObject(Meshes));
	Manifest->SetObjectField(TEXT("materials"), ToObject(Materials));
	Manifest->SetObjectField(TEXT("textures"), ToObject(Textures));
	auto Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("registered_primitive_components"), ComponentRows.Num());
	Summary->SetNumberField(TEXT("basic_visible_components"), VisibleCount);
	Summary->SetNumberField(TEXT("visible_non_static_mesh_components"), UnsupportedVisibleCount);
	Summary->SetNumberField(TEXT("tagged_city_source_actors"), SourceActorCount);
	Summary->SetNumberField(TEXT("unique_initial_ids"), InitialIds.Num());
	Summary->SetNumberField(TEXT("unique_interior_preview_ids"), InteriorPreviewIds.Num());
	Summary->SetNumberField(TEXT("invalid_or_overlapping_source_ids"), InvalidOrOverlappingSourceIds);
	Summary->SetNumberField(TEXT("mesh_assets"), Meshes.Num());
	Summary->SetNumberField(TEXT("material_interfaces"), Materials.Num());
	Summary->SetNumberField(TEXT("used_texture_assets"), Textures.Num());
	auto CategorySummary = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Entry : Categories) CategorySummary->SetNumberField(Entry.Key, Entry.Value);
	Summary->SetObjectField(TEXT("category_component_counts"), CategorySummary);
	auto ActorCategorySummary = MakeShared<FJsonObject>();
	for (const TPair<FString, int32>& Entry : ActorCategories) ActorCategorySummary->SetNumberField(Entry.Key, Entry.Value);
	Summary->SetObjectField(TEXT("category_actor_counts"), ActorCategorySummary);
	Manifest->SetObjectField(TEXT("summary"), Summary);
	const bool bSharedSource = InitialIds.Num() == 24 && InvalidOrOverlappingSourceIds == 0 &&
		ActorCategories.FindRef(TEXT("initial_source_building")) == 24 &&
		ActorCategories.FindRef(TEXT("source_boundary")) == 10 &&
		ActorCategories.FindRef(TEXT("source_ground")) == 2 &&
		ActorCategories.FindRef(TEXT("source_camera")) == 1 &&
		ActorCategories.FindRef(TEXT("source_player_start")) == 1 &&
		ActorCategories.FindRef(TEXT("other_city_source_or_diagnostic")) == 0 &&
		ActorCategories.FindRef(TEXT("legacy_building_visual_support")) == 0;
	const bool bCurrentPromoted42 = bSharedSource && SourceActorCount == 42 &&
		ActorCategories.FindRef(TEXT("supplemental_interior_preview")) == 0 && InteriorPreviewIds.Num() == 0 &&
		ActorCategories.FindRef(TEXT("city_support_layer")) == 2 &&
		ActorCategories.FindRef(TEXT("city_gate_flag")) == 2;
	const bool bHistoricalDense62 = bSharedSource && SourceActorCount == 62 &&
		ActorCategories.FindRef(TEXT("supplemental_interior_preview")) == 24 && InteriorPreviewIds.Num() == 24 &&
		ActorCategories.FindRef(TEXT("city_support_layer")) == 0 &&
		ActorCategories.FindRef(TEXT("city_gate_flag")) == 0;
	Manifest->SetBoolField(TEXT("current_promoted_source_actor_contract_matched"), bCurrentPromoted42);
	Manifest->SetBoolField(TEXT("historical_dense_source_actor_contract_observed"), bHistoricalDense62);
	Manifest->SetStringField(TEXT("source_actor_contract_status"), bCurrentPromoted42 ? TEXT("current_promoted_gameapp_42") :
		(bHistoricalDense62 ? TEXT("historical_dense_62_not_current_promoted") : TEXT("observed_not_mapped_to_known_contract")));
	Manifest->SetStringField(TEXT("source_actor_contract_evidence"),
		TEXT("current: unity_runtime_city_all_buildings_verify.json (42=24 initial+10 boundary+2 ground+2 support+2 flags+camera+player, no interior previews); historical dense: unity_runtime_city_all_buildings_verify_GameAppRuntimeV6Color.json (62=24 initial+24 interior preview+10 boundary+2 ground+camera+player)"));
	Manifest->SetBoolField(TEXT("capture_complete"), true);
	Manifest->SetStringField(TEXT("visibility_limit"), TEXT("actor/component attach-chain visibility only; occlusion, material pass and actual GPU inclusion require a separate runtime render audit"));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	const bool bSerialized = FJsonSerializer::Serialize(Manifest, Writer);
	return bSerialized && FFileHelper::SaveStringToFile(Json, *OutManifestPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
