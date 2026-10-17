#include "RokWorldStreamingDiagnosticGameMode.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapPlayerController.h"
#include "RokWorldMapStreamingActor.h"
#include "RokWorldMapRuntimeDataAsset.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 InitialInstances = 848;
constexpr int32 MovedInstances = 1688;
constexpr float InitialDxf = 288.31601f;
constexpr float LodOneDxf = 2000.0f;
constexpr float BlockedLodTwoDxf = 3001.0f;
const FVector2D InitialFocus(6876.0, 942.0);
const FVector2D MovedFocus(5796.0, 942.0);

TArray<TSharedPtr<FJsonValue>> TransformNumbers(const FTransform& Value)
{
	const FVector Position = Value.GetTranslation();
	const FQuat Rotation = Value.GetRotation();
	const FVector Scale = Value.GetScale3D();
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const double Number : {Position.X, Position.Y, Position.Z,
		Rotation.X, Rotation.Y, Rotation.Z, Rotation.W,
		Scale.X, Scale.Y, Scale.Z})
	{
		Result.Add(MakeShared<FJsonValueNumber>(Number));
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> Vector2Numbers(const FVector2D& Value)
{
	return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y)};
}

TSharedPtr<FJsonValue> JsonString(const FString& Value)
{
	return MakeShared<FJsonValueString>(Value);
}

TSharedPtr<FJsonValue> JsonNumber(double Value)
{
	return MakeShared<FJsonValueNumber>(Value);
}

FString SerializeState(const TSharedPtr<FJsonObject>& State)
{
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(State.ToSharedRef(), Writer);
	return Text;
}

uint32 PngU32(const TArray<uint8>& Bytes, int32 Offset)
{
	return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset + 1]) << 16)
		| (uint32(Bytes[Offset + 2]) << 8) | uint32(Bytes[Offset + 3]);
}
}

ARokWorldStreamingDiagnosticGameMode::ARokWorldStreamingDiagnosticGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARokWorldMapCameraPawn::StaticClass();
	PlayerControllerClass = ARokWorldMapPlayerController::StaticClass();
}

void ARokWorldStreamingDiagnosticGameMode::BeginPlay()
{
	Super::BeginPlay();
	StartedAtSeconds = FPlatformTime::Seconds();
	RunDirectory = FPaths::Combine(FPaths::ProjectSavedDir(),
		TEXT("RokUnityMigration/WorldStreamingDiagnosticRunsV4"),
		FString::Printf(TEXT("%s_%u_%llu"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId(),
			static_cast<unsigned long long>(FPlatformTime::Cycles64())));
	RunDirectory = FPaths::ConvertRelativePathToFull(RunDirectory);
	FPaths::NormalizeDirectoryName(RunDirectory);
	IFileManager::Get().MakeDirectory(*RunDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &ARokWorldStreamingDiagnosticGameMode::ScreenshotProcessed);
	BeginBoundary = MakeShared<FJsonObject>();
	WriteLocalOnlyBoundary(*BeginBoundary);
	UWorld* World = GetWorld();
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokWorldStreamingDiagnostic"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		|| !World
		|| World->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticV1"))
	{
		Fail(TEXT("Diagnostic command flag/map identity failed."));
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	for (TActorIterator<ARokWorldMapStreamingActor> It(World); It; ++It)
	{
		if (Streaming.IsValid())
		{
			Fail(TEXT("More than one source streaming actor in diagnostic map."));
			return;
		}
		Streaming = *It;
	}
	if (!Streaming.IsValid() || !Streaming->RuntimeData
		|| Streaming->RuntimeData->GetPathName() != TEXT("/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.DA_RokWorldStreamSourceLod01")
		|| Streaming->RuntimeData->CanonicalPayloadSha256 != TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f")
		|| Streaming->RuntimeData->bStaticVisualReady
		|| Streaming->RuntimeData->bRuntimeReady
		|| !Streaming->bEnableScopedDiagnosticLod01)
	{
		Fail(TEXT("Diagnostic actor/data/readiness identity failed."));
	}
}

void ARokWorldStreamingDiagnosticGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::EndPlay(EndPlayReason);
}

void ARokWorldStreamingDiagnosticGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished)
	{
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	if (FPlatformTime::Seconds() - StartedAtSeconds > 90.0)
	{
		Fail(FString::Printf(TEXT("Timeout at diagnostic step %d."), Step));
		return;
	}
	if (!Camera.IsValid())
	{
		for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It)
		{
			Camera = *It;
			break;
		}
		if (!Camera.IsValid())
		{
			return;
		}
	}
	if (!Streaming.IsValid())
	{
		Fail(TEXT("Streaming actor was destroyed."));
		return;
	}
	if (PendingSample.IsValid())
	{
		CheckPendingScreenshot();
		return;
	}
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	if (!State.bRegisteredMatchesActive)
	{
		Record(TEXT("registered_set_mismatch"));
		Fail(TEXT("Registered HISM set differs from ActiveComponents."));
		return;
	}
	if (Step == 0)
	{
		Camera->SetViewCenterUnity(InitialFocus);
		Camera->SetCurrentDxf(InitialDxf);
		Step = 1;
		ClearStability();
		return;
	}
	if (Step == 1 && State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 0 && State.CenterChunk == FIntPoint(19, 2)
		&& State.InstanceCount == InitialInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod0_initial_edge"));
		return;
	}
	if (Step == 2 && State.Generation > InitialGeneration
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 0 && State.CenterChunk == FIntPoint(16, 2)
		&& State.InstanceCount == MovedInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		for (const FName OldName : InitialComponentNames)
		{
			if (State.ComponentNames.Contains(OldName))
			{
				Fail(TEXT("Old HISM component survived camera movement/unload."));
				return;
			}
		}
		Record(TEXT("lod0_moved_west_three_chunks"));
		return;
	}
	if (Step == 3 && State.Generation > MovedGeneration
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 1 && State.CenterChunk == FIntPoint(16, 2)
		&& State.InstanceCount == MovedInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod1_moved"));
		return;
	}
	if (Step == 4 && State.Generation > LodOneGeneration
		&& !State.bHasRequestedState && !State.bPendingLoad
		&& State.ComponentCount == 0 && State.VisibleComponentCount == 0
		&& State.InstanceCount == 0
		&& State.RegisteredComponentCount == 0 && State.RegisteredInstanceCount == 0)
	{
		Record(TEXT("lod2_refused_unloaded"));
		return;
	}
	if (Step == 5 && State.Generation > LodOneGeneration + 1
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 1 && State.InstanceCount == MovedInstances
		&& State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod1_restored_after_refusal"));
		return;
	}
	ClearStability();
}

TSharedPtr<FJsonObject> ARokWorldStreamingDiagnosticGameMode::CaptureVisualState() const
{
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
	WriteLocalOnlyBoundary(*Sample);
	Sample->SetNumberField(TEXT("lod"), State.Lod);
	Sample->SetNumberField(TEXT("center_chunk_x"), State.CenterChunk.X);
	Sample->SetNumberField(TEXT("center_chunk_y"), State.CenterChunk.Y);
	Sample->SetNumberField(TEXT("generation"), static_cast<double>(State.Generation));
	Sample->SetNumberField(TEXT("hism_component_count"), State.ComponentCount);
	Sample->SetNumberField(TEXT("hism_visible_component_count"), State.VisibleComponentCount);
	Sample->SetNumberField(TEXT("hism_instance_count"), State.InstanceCount);
	Sample->SetNumberField(TEXT("registered_hism_component_count"), State.RegisteredComponentCount);
	Sample->SetNumberField(TEXT("registered_hism_instance_count"), State.RegisteredInstanceCount);
	Sample->SetBoolField(TEXT("registered_matches_active"), State.bRegisteredMatchesActive);
	Sample->SetBoolField(TEXT("pending_load"), State.bPendingLoad);
	Sample->SetBoolField(TEXT("has_requested_state"), State.bHasRequestedState);
	Sample->SetNumberField(TEXT("camera_dxf"), Camera->GetCurrentDxf());
	const FVector2D Focus = Camera->GetViewCenterUnity();
	Sample->SetNumberField(TEXT("camera_unity_x"), Focus.X);
	Sample->SetNumberField(TEXT("camera_unity_y"), Focus.Y);
	Sample->SetArrayField(TEXT("camera_actor_transform"), TransformNumbers(Camera->GetActorTransform()));
	const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	Sample->SetBoolField(TEXT("camera_is_player_pawn"), Controller && Controller->GetPawn() == Camera.Get());
	Sample->SetBoolField(TEXT("camera_is_view_target"), Controller && Controller->GetViewTarget() == Camera.Get());
	Sample->SetBoolField(TEXT("camera_component_active"),
		Camera->GetCameraComponent() && Camera->GetCameraComponent()->IsActive());
	Sample->SetBoolField(TEXT("streaming_root_static"), Streaming->GetRootComponent()
		&& Streaming->GetRootComponent()->Mobility == EComponentMobility::Static);
	int32 DirectionalLights = 0;
	int32 SkyLights = 0;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { ++DirectionalLights; }
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It) { ++SkyLights; }
	Sample->SetNumberField(TEXT("directional_light_count"), DirectionalLights);
	Sample->SetNumberField(TEXT("sky_light_count"), SkyLights);
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FName Name : State.ComponentNames)
	{
		Names.Add(MakeShared<FJsonValueString>(Name.ToString()));
	}
	Sample->SetArrayField(TEXT("hism_component_names"), Names);
	TArray<TSharedPtr<FJsonValue>> RegisteredNames;
	for (const FName Name : State.RegisteredComponentNames)
	{
		RegisteredNames.Add(JsonString(Name.ToString()));
	}
	Sample->SetArrayField(TEXT("registered_hism_component_names"), RegisteredNames);
	Sample->SetArrayField(TEXT("actual_hism_components"), CaptureActualComponents());
	return Sample;
}

void ARokWorldStreamingDiagnosticGameMode::ClearStability()
{
	StableStep = -1;
	StableObservationCount = 0;
	StableLastFrame = 0;
	StableStateJson.Empty();
}

void ARokWorldStreamingDiagnosticGameMode::Record(const FString& Label)
{
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	TSharedPtr<FJsonObject> State = CaptureVisualState();
	LastObservedSceneState = State;
	LastObservedSceneFrame = GFrameCounter;
	if (!State->GetBoolField(TEXT("camera_is_player_pawn"))
		|| !State->GetBoolField(TEXT("camera_is_view_target"))
		|| !State->GetBoolField(TEXT("camera_component_active"))
		|| !State->GetBoolField(TEXT("streaming_root_static")))
	{
		Fail(TEXT("Diagnostic camera/view target or streaming root is not the actual active scene."));
		return;
	}
	if (State->GetIntegerField(TEXT("hism_component_count")) > 0)
	{
		bool bRenderReady = true;
		for (const TSharedPtr<FJsonValue>& Entry : State->GetArrayField(TEXT("actual_hism_components")))
		{
			const TSharedPtr<FJsonObject> Component = Entry->AsObject();
			if (!Component.IsValid() || !Component->GetBoolField(TEXT("attached_to_streaming_root")))
			{
				Fail(TEXT("Registered HISM is detached from the static streaming root."));
				return;
			}
			bRenderReady &= Component->GetBoolField(TEXT("render_state_created"))
				&& Component->GetBoolField(TEXT("nonzero_world_bounds"));
		}
		if (!bRenderReady)
		{
			// Render proxy and bounds updates can lag RegisterComponent/AddInstances.
			// The existing 90-second diagnostic timeout remains the hard failure.
			ClearStability();
			return;
		}
	}
	const FString StateJson = SerializeState(State);
	if (StableStep != Step || StableStateJson != StateJson || GFrameCounter <= StableLastFrame)
	{
		StableStep = Step;
		StableStateJson = StateJson;
		StableLastFrame = GFrameCounter;
		StableObservationCount = 1;
		return;
	}
	StableLastFrame = GFrameCounter;
	++StableObservationCount;
	if (StableObservationCount < 3)
	{
		return;
	}
	if (!GEngine || !GEngine->GameViewport || GEngine->GameViewport->GetWorld() != GetWorld()
		|| !GEngine->GameViewport->Viewport
		|| UGameViewportClient::OnScreenshotCaptured().IsBound()
		|| FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(TEXT("Game viewport unavailable or another screenshot capture owns the frame."));
		return;
	}
	const FIntPoint Extent = GEngine->GameViewport->Viewport->GetSizeXY();
	if (Extent != FIntPoint(1440, 810))
	{
		Fail(TEXT("Diagnostic game viewport must be 1440x810 for source image comparison."));
		return;
	}
	PendingScreenshotPath = FPaths::Combine(RunDirectory, Label + TEXT(".png"));
	if (IFileManager::Get().FileExists(*PendingScreenshotPath))
	{
		Fail(TEXT("Phase screenshot target already exists; refusing overwrite."));
		return;
	}
	PendingStateJson = StateJson;
	PendingSample = State;
	PendingSample->SetStringField(TEXT("phase"), Label);
	PendingSample->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetNumberField(TEXT("stable_observations"), StableObservationCount);
	PendingSample->SetStringField(TEXT("screenshot_path"), PendingScreenshotPath);
	PendingSample->SetNumberField(TEXT("screenshot_request_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetNumberField(TEXT("viewport_width"), Extent.X);
	PendingSample->SetNumberField(TEXT("viewport_height"), Extent.Y);
	ScreenshotRequestFrame = GFrameCounter;
	ScreenshotRequestedAtSeconds = FPlatformTime::Seconds();
	bScreenshotProcessed = false;
	FScreenshotRequest::RequestScreenshot(PendingScreenshotPath, false, false, false, FIntRect(), true);
	if (!FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(TEXT("Game-viewport screenshot request was not accepted."));
	}
}

void ARokWorldStreamingDiagnosticGameMode::ScreenshotProcessed()
{
	if (bFinished || !PendingSample.IsValid() || bScreenshotProcessed)
	{
		return;
	}
	TArray<uint8> Bytes;
	constexpr uint8 PngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(Bytes, *PendingScreenshotPath)
		|| Bytes.Num() < 33 || FMemory::Memcmp(Bytes.GetData(), PngSignature, 8) != 0
		|| PngU32(Bytes, 8) != 13 || FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) != 0
		|| PngU32(Bytes, 16) != 1440 || PngU32(Bytes, 20) != 810
		|| Bytes[24] != 8 || Bytes[25] != 6 || Bytes[26] != 0
		|| Bytes[27] != 0 || Bytes[28] != 0)
	{
		Fail(TEXT("Processed real game-viewport screenshot is not 1440x810 RGBA8 PNG."));
		return;
	}
	FImage Decoded;
	if (!FImageUtils::DecompressImage(Bytes.GetData(), Bytes.Num(), Decoded)
		|| Decoded.SizeX != 1440 || Decoded.SizeY != 810)
	{
		Fail(TEXT("Processed viewport PNG cannot be decoded as the expected image."));
		return;
	}
	FImage Bgra;
	Decoded.CopyTo(Bgra, ERawImageFormat::BGRA8, Decoded.GammaSpace);
	int64 NonblackRgbPixels = 0;
	int64 NonOpaqueAlphaPixels = 0;
	for (const FColor& Pixel : Bgra.AsBGRA8())
	{
		NonOpaqueAlphaPixels += Pixel.A != 255;
		NonblackRgbPixels += Pixel.R > 2 || Pixel.G > 2 || Pixel.B > 2;
	}
	PendingSample->SetNumberField(TEXT("png_nonopaque_alpha_pixels"), static_cast<double>(NonOpaqueAlphaPixels));
	if (NonOpaqueAlphaPixels != 0)
	{
		Fail(TEXT("Processed viewport RGBA PNG contains nonopaque pixels."));
		return;
	}
	PendingSample->SetNumberField(TEXT("png_nonblack_rgb_pixels"), static_cast<double>(NonblackRgbPixels));
	if (PendingSample->GetIntegerField(TEXT("hism_instance_count")) > 0
		&& NonblackRgbPixels <= 1024)
	{
		Fail(TEXT("Nonempty streamed world produced an effectively black real viewport PNG."));
		return;
	}
	TSharedPtr<FJsonObject> ProcessedState = CaptureVisualState();
	if (SerializeState(ProcessedState) != PendingStateJson)
	{
		Fail(TEXT("Source HISM/camera/HTTP state changed before screenshot processing."));
		return;
	}
	ScreenshotProcessedFrame = GFrameCounter;
	PendingSample->SetNumberField(TEXT("screenshot_processed_frame"), static_cast<double>(ScreenshotProcessedFrame));
	PendingSample->SetNumberField(TEXT("png_bytes"), Bytes.Num());
	PendingSample->SetNumberField(TEXT("png_width"), PngU32(Bytes, 16));
	PendingSample->SetNumberField(TEXT("png_height"), PngU32(Bytes, 20));
	PendingSample->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	PendingSample->SetObjectField(TEXT("processed_state"), ProcessedState);
	bScreenshotProcessed = true;
}

void ARokWorldStreamingDiagnosticGameMode::CheckPendingScreenshot()
{
	if (!PendingSample.IsValid())
	{
		return;
	}
	if (FPlatformTime::Seconds() - ScreenshotRequestedAtSeconds > 15.0)
	{
		Fail(TEXT("Game-viewport screenshot callback/next-frame state timed out."));
		return;
	}
	TSharedPtr<FJsonObject> CurrentState = CaptureVisualState();
	if (SerializeState(CurrentState) != PendingStateJson)
	{
		Fail(TEXT("Source HISM/camera/HTTP state changed while screenshot was pending."));
		return;
	}
	if (!bScreenshotProcessed || GFrameCounter <= ScreenshotProcessedFrame)
	{
		return;
	}
	PendingSample->SetNumberField(TEXT("screenshot_observed_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetObjectField(TEXT("observed_state"), CurrentState);
	Samples.Add(PendingSample);
	PendingSample.Reset();
	PendingStateJson.Empty();
	PendingScreenshotPath.Empty();
	ClearStability();
	AdvanceAfterScreenshot();
}

void ARokWorldStreamingDiagnosticGameMode::AdvanceAfterScreenshot()
{
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	switch (Step)
	{
	case 1:
		InitialGeneration = State.Generation;
		InitialComponentNames = State.ComponentNames;
		Camera->SetViewCenterUnity(MovedFocus);
		Step = 2;
		break;
	case 2:
		MovedGeneration = State.Generation;
		Camera->SetCurrentDxf(LodOneDxf);
		Step = 3;
		break;
	case 3:
		LodOneGeneration = State.Generation;
		Camera->SetCurrentDxf(BlockedLodTwoDxf);
		Step = 4;
		break;
	case 4:
		Camera->SetCurrentDxf(LodOneDxf);
		Step = 5;
		break;
	case 5:
		Finish(true, TEXT("Five stable game-viewport captures and scoped LOD0/1 streaming passed."));
		break;
	default:
		Fail(TEXT("Invalid phase after game-viewport screenshot."));
		break;
	}
}

TSharedPtr<FJsonObject> ARokWorldStreamingDiagnosticGameMode::CaptureRuntimeData() const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	const URokWorldMapRuntimeDataAsset* Data = Streaming.IsValid()
		? Streaming->RuntimeData.Get() : nullptr;
	if (!Data)
	{
		return Result;
	}
	Result->SetStringField(TEXT("schema"), Data->Schema);
	Result->SetStringField(TEXT("canonical_payload_sha256"), Data->CanonicalPayloadSha256);
	Result->SetStringField(TEXT("source_manifest_path"), Data->SourceManifestPath);
	Result->SetStringField(TEXT("source_manifest_sha256"), Data->SourceManifestSha256);
	Result->SetNumberField(TEXT("map_width_tiles"), Data->MapWidthTiles);
	Result->SetNumberField(TEXT("map_height_tiles"), Data->MapHeightTiles);
	Result->SetNumberField(TEXT("tile_size_unity_units"), Data->TileSizeUnityUnits);
	Result->SetBoolField(TEXT("static_visual_ready"), Data->bStaticVisualReady);
	Result->SetBoolField(TEXT("runtime_ready"), Data->bRuntimeReady);
	TSharedPtr<FJsonObject> Projection = MakeShared<FJsonObject>();
	Projection->SetArrayField(TEXT("unity_bounds_min"), Vector2Numbers(Data->Projection.UnityBoundsMin));
	Projection->SetArrayField(TEXT("unity_bounds_max"), Vector2Numbers(Data->Projection.UnityBoundsMax));
	Projection->SetArrayField(TEXT("unity_rebase_origin"), Vector2Numbers(Data->Projection.UnityRebaseOrigin));
	Projection->SetNumberField(TEXT("ue_units_per_unity_unit"), Data->Projection.UeUnitsPerUnityUnit);
	Projection->SetNumberField(TEXT("ground_z"), Data->Projection.GroundZ);
	Projection->SetNumberField(TEXT("display_coordinate_scale"), Data->Projection.DisplayCoordinateScale);
	Result->SetObjectField(TEXT("projection"), Projection);
	TArray<TSharedPtr<FJsonValue>> Levels;
	for (const FRokWorldMapLodLevel& Level : Data->LodLevels)
	{
		Levels.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Level.Lod), JsonNumber(Level.MaxDxfInclusive), JsonNumber(Level.ChunkTileSpan)}));
	}
	Result->SetArrayField(TEXT("lod_levels"), Levels);
	TArray<TSharedPtr<FJsonValue>> Tiles;
	for (const FRokWorldMapTileRecord& Tile : Data->Tiles)
	{
		Tiles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Tile.TileX), JsonNumber(Tile.TileY), JsonString(Tile.TileTypeId.ToString()),
			MakeShared<FJsonValueArray>(TransformNumbers(Tile.TemplateToWorld))}));
	}
	Result->SetArrayField(TEXT("tiles"), Tiles);
	TArray<TSharedPtr<FJsonValue>> Types;
	for (const FRokWorldMapTileTypeRecord& Type : Data->TileTypes)
	{
		TArray<TSharedPtr<FJsonValue>> Bindings;
		for (const FRokWorldMapLodBinding& Binding : Type.LodBindings)
		{
			Bindings.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				JsonNumber(Binding.Lod), JsonString(Binding.RecipeId.ToString())}));
		}
		Types.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Type.TileTypeId.ToString()), MakeShared<FJsonValueArray>(Bindings)}));
	}
	Result->SetArrayField(TEXT("tile_types"), Types);
	TArray<TSharedPtr<FJsonValue>> Recipes;
	for (const FRokWorldMapRenderRecipe& Recipe : Data->Recipes)
	{
		TArray<TSharedPtr<FJsonValue>> Parts;
		for (const FRokWorldMapRenderPart& Part : Recipe.Parts)
		{
			TArray<TSharedPtr<FJsonValue>> Materials;
			for (const TSoftObjectPtr<UMaterialInterface>& Material : Part.Materials)
			{
				Materials.Add(JsonString(Material.ToSoftObjectPath().ToString()));
			}
			Parts.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				JsonString(Part.Mesh.ToSoftObjectPath().ToString()),
				MakeShared<FJsonValueArray>(Materials),
				MakeShared<FJsonValueArray>(TransformNumbers(Part.LocalTransform)),
				JsonNumber(Part.TranslucencySortPriority),
				MakeShared<FJsonValueBoolean>(Part.bTree), JsonNumber(Part.RenderFlags)}));
		}
		Recipes.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Recipe.RecipeId.ToString()), JsonString(Recipe.TileTypeId.ToString()),
			JsonNumber(Recipe.Lod), MakeShared<FJsonValueArray>(Parts)}));
	}
	Result->SetArrayField(TEXT("recipes"), Recipes);
	TArray<TSharedPtr<FJsonValue>> Blockers;
	for (const FString& Blocker : Data->ReadinessBlockers)
	{
		Blockers.Add(JsonString(Blocker));
	}
	Result->SetArrayField(TEXT("readiness_blockers"), Blockers);
	TArray<TSharedPtr<FJsonValue>> Policies;
	for (const FRokWorldMapPendingPolicy& Policy : Data->PendingPolicies)
	{
		FString Status;
		switch (Policy.Status)
		{
		case ERokWorldMapPendingPolicyStatus::Pending: Status = TEXT("Pending"); break;
		case ERokWorldMapPendingPolicyStatus::SourceExplicitNoOp: Status = TEXT("SourceExplicitNoOp"); break;
		case ERokWorldMapPendingPolicyStatus::AcceptedOmission: Status = TEXT("AcceptedOmission"); break;
		case ERokWorldMapPendingPolicyStatus::Promoted: Status = TEXT("Promoted"); break;
		default: Status = TEXT("unknown"); break;
		}
		Policies.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Policy.PolicyId.ToString()), JsonString(Status),
			JsonNumber(Policy.SourceComponentCount), JsonString(Policy.RequiredDecision)}));
	}
	Result->SetArrayField(TEXT("pending_policies"), Policies);
	return Result;
}

TArray<TSharedPtr<FJsonValue>> ARokWorldStreamingDiagnosticGameMode::CaptureActualComponents() const
{
	TArray<TSharedPtr<FJsonValue>> Result;
	if (!Streaming.IsValid())
	{
		return Result;
	}
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Streaming->GetComponents(Components);
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
	{
		if (!IsValid(Component) || Component->GetOwner() != Streaming.Get() || !Component->IsRegistered())
		{
			continue;
		}
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Component->GetName());
		Item->SetStringField(TEXT("mesh"), Component->GetStaticMesh()
			? Component->GetStaticMesh()->GetPathName() : TEXT(""));
		Item->SetNumberField(TEXT("translucency_sort_priority"), Component->TranslucencySortPriority);
		Item->SetBoolField(TEXT("visible"), Component->IsVisible());
		Item->SetStringField(TEXT("attach_parent"), Component->GetAttachParent()
			? Component->GetAttachParent()->GetName() : TEXT(""));
		Item->SetBoolField(TEXT("attached_to_streaming_root"),
			Component->GetAttachParent() == Streaming->GetRootComponent());
		Item->SetBoolField(TEXT("render_state_created"), Component->IsRenderStateCreated());
		const FVector WorldExtent = Component->Bounds.BoxExtent;
		Item->SetArrayField(TEXT("world_bounds_origin"), TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Component->Bounds.Origin.X), JsonNumber(Component->Bounds.Origin.Y),
			JsonNumber(Component->Bounds.Origin.Z)});
		Item->SetArrayField(TEXT("world_bounds_extent"), TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(WorldExtent.X), JsonNumber(WorldExtent.Y), JsonNumber(WorldExtent.Z)});
		Item->SetBoolField(TEXT("nonzero_world_bounds"),
			!WorldExtent.ContainsNaN() && WorldExtent.GetMax() > KINDA_SMALL_NUMBER);
		Item->SetArrayField(TEXT("relative_transform"), TransformNumbers(Component->GetRelativeTransform()));
		TArray<TSharedPtr<FJsonValue>> Materials;
		for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
		{
			const UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
			Materials.Add(JsonString(Material ? Material->GetPathName() : TEXT("")));
			if (Material)
			{
				TSharedPtr<FJsonObject> MaterialDiagnostic = MakeShared<FJsonObject>();
				MaterialDiagnostic->SetStringField(TEXT("path"), Material->GetPathName());
				MaterialDiagnostic->SetNumberField(TEXT("blend_mode"), static_cast<int32>(Material->GetBlendMode()));
				MaterialDiagnostic->SetBoolField(TEXT("default_lit"),
					Material->GetShadingModels().HasShadingModel(MSM_DefaultLit));
				MaterialDiagnostic->SetBoolField(TEXT("unlit"),
					Material->GetShadingModels().HasShadingModel(MSM_Unlit));
				const UMaterial* BaseMaterial = Material->GetMaterial();
				MaterialDiagnostic->SetStringField(TEXT("base_material"),
					BaseMaterial ? BaseMaterial->GetPathName() : TEXT(""));
				MaterialDiagnostic->SetBoolField(TEXT("base_instanced_static_mesh_usage"),
					BaseMaterial && BaseMaterial->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
				Item->SetObjectField(FString::Printf(TEXT("material_diagnostic_%d"), MaterialIndex), MaterialDiagnostic);
			}
		}
		Item->SetArrayField(TEXT("materials"), Materials);
		TArray<TSharedPtr<FJsonValue>> Instances;
		bool bReadAll = true;
		for (int32 InstanceIndex = 0; InstanceIndex < Component->GetInstanceCount(); ++InstanceIndex)
		{
			FTransform Transform;
			if (!Component->GetInstanceTransform(InstanceIndex, Transform, false))
			{
				bReadAll = false;
				break;
			}
			Instances.Add(MakeShared<FJsonValueArray>(TransformNumbers(Transform)));
		}
		Item->SetBoolField(TEXT("instance_transforms_read_all"), bReadAll);
		Item->SetArrayField(TEXT("instances_local"), Instances);
		Result.Add(MakeShared<FJsonValueObject>(Item));
	}
	return Result;
}

void ARokWorldStreamingDiagnosticGameMode::Fail(const FString& Reason)
{
	Finish(false, Reason);
}

bool ARokWorldStreamingDiagnosticGameMode::CheckLocalOnlyBoundary(FString& OutReason) const
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))
	{
		OutReason = TEXT("-RokLocalOnly command flag is required.");
		return false;
	}
	UGameInstance* Instance = GetGameInstance();
	const URokServerSubsystem* Server = Instance ? Instance->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun())
	{
		OutReason = TEXT("Server subsystem is absent or not in local-only mode.");
		return false;
	}
	if (Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0)
	{
		OutReason = FString::Printf(TEXT("Local-only boundary changed: HTTP dispatch=%llu suppressed=%llu."),
			static_cast<unsigned long long>(Server->GetHttpDispatchAttemptCount()),
			static_cast<unsigned long long>(Server->GetSuppressedLocalRequestCount()));
		return false;
	}
	OutReason.Empty();
	return true;
}

void ARokWorldStreamingDiagnosticGameMode::WriteLocalOnlyBoundary(FJsonObject& Object) const
{
	UGameInstance* Instance = GetGameInstance();
	const URokServerSubsystem* Server = Instance ? Instance->GetSubsystem<URokServerSubsystem>() : nullptr;
	Object.SetBoolField(TEXT("rok_local_only_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Object.SetBoolField(TEXT("rok_world_streaming_diagnostic_flag"),
		FParse::Param(FCommandLine::Get(), TEXT("RokWorldStreamingDiagnostic")));
	Object.SetBoolField(TEXT("server_subsystem_present"), Server != nullptr);
	Object.SetBoolField(TEXT("local_only"), Server && Server->IsLocalOnlyRun());
	Object.SetNumberField(TEXT("http_dispatch_attempts"),
		Server ? static_cast<double>(Server->GetHttpDispatchAttemptCount()) : -1.0);
	Object.SetNumberField(TEXT("http_suppressed_requests"),
		Server ? static_cast<double>(Server->GetSuppressedLocalRequestCount()) : -1.0);
}

void ARokWorldStreamingDiagnosticGameMode::Finish(bool bPassed, const FString& Reason)
{
	if (bFinished)
	{
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		bPassed = false;
	}
	if (bPassed && (Samples.Num() != 5 || PendingSample.IsValid()))
	{
		bPassed = false;
		BoundaryReason = TEXT("Five processed screenshots with later-frame state checks were not recorded.");
	}
	bFinished = true;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.world_streaming_diagnostic_runtime.v4"));
	Report->SetStringField(TEXT("screenshot_origin"), TEXT("FScreenshotRequest_game_viewport"));
	Report->SetStringField(TEXT("run_directory"), RunDirectory);
	Report->SetNumberField(TEXT("expected_viewport_width"), 1440);
	Report->SetNumberField(TEXT("expected_viewport_height"), 810);
	Report->SetObjectField(TEXT("begin_local_boundary"),
		BeginBoundary.IsValid() ? BeginBoundary : MakeShared<FJsonObject>());
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetStringField(TEXT("reason"), BoundaryReason.IsEmpty() ? Reason : Reason + TEXT(" ") + BoundaryReason);
	WriteLocalOnlyBoundary(*Report);
	Report->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Report->SetStringField(TEXT("runtime_data_path"),
		Streaming.IsValid() && Streaming->RuntimeData
			? Streaming->RuntimeData->GetPathName() : TEXT(""));
	Report->SetStringField(TEXT("payload_sha256"),
		Streaming.IsValid() && Streaming->RuntimeData
			? Streaming->RuntimeData->CanonicalPayloadSha256 : TEXT(""));
	Report->SetBoolField(TEXT("static_visual_ready"),
		Streaming.IsValid() && Streaming->RuntimeData
			&& Streaming->RuntimeData->bStaticVisualReady);
	Report->SetBoolField(TEXT("runtime_ready"),
		Streaming.IsValid() && Streaming->RuntimeData
			&& Streaming->RuntimeData->bRuntimeReady);
	Report->SetObjectField(TEXT("runtime_data_snapshot"), CaptureRuntimeData());
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const TSharedPtr<FJsonObject>& Sample : Samples)
	{
		Entries.Add(MakeShared<FJsonValueObject>(Sample));
	}
	Report->SetArrayField(TEXT("samples"), Entries);
	if (!bPassed && LastObservedSceneState.IsValid())
	{
		Report->SetNumberField(TEXT("last_observed_scene_frame"),
			static_cast<double>(LastObservedSceneFrame));
		Report->SetObjectField(TEXT("last_observed_scene_state"), LastObservedSceneState);
	}
	if (PendingSample.IsValid())
	{
		Report->SetObjectField(TEXT("pending_capture_at_failure"), PendingSample);
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Path = FPaths::Combine(RunDirectory, TEXT("manifest.json"));
	const bool bReportSaved = FFileHelper::SaveStringToFile(
		Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("World streaming diagnostic passed=%s report=%s reason=%s"),
		bPassed ? TEXT("true") : TEXT("false"), *Path, *Reason);
	if (!bReportSaved)
	{
		UE_LOG(LogTemp, Error,
			TEXT("World streaming diagnostic could not save report '%s'."), *Path);
	}
	FPlatformMisc::RequestExitWithStatus(false,
		static_cast<uint8>(bPassed && bReportSaved ? 0 : 1));
}
