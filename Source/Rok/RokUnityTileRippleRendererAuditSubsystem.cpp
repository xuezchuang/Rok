#include "RokUnityTileRippleRendererAuditSubsystem.h"

#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityTileRippleRenderer.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TextureResource.h"
#include "UnrealClient.h"

namespace
{
constexpr TCHAR WorldMap[] = TEXT("/Game/Maps/L_RokUnityWorldMapPrototype");
constexpr TCHAR TexturePath[] = TEXT("/Game/RokUnityWorldMapPrototype/TileRipple/T_Ring_07_Source.T_Ring_07_Source");
constexpr TCHAR MaterialPath[] = TEXT("/Game/RokUnityWorldMapPrototype/TileRipple/M_RokTileRippleSourceAlpha.M_RokTileRippleSourceAlpha");
constexpr TCHAR ContractRelative[] = TEXT("Saved/PendingUnityTileRippleRendererV8/geometry_contract.json");
constexpr TCHAR CpuRelative[] = TEXT("Saved/PendingUnityTileRippleSimulationV2/tile_ripple_source_config.json");
constexpr TCHAR ReadyRelative[] = TEXT("Saved/PendingUnityTileRippleRendererV14/ready_for_root_review.json");
constexpr double TargetTimes[] = {2.5, 4.9, 5.1, 7.5};
constexpr double MaxLag[] = {0.08, 0.08, 0.08, 0.10};
constexpr int32 PrivateExtent = 512;

bool IsRokProject()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	return FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}

FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
TSharedRef<FJsonObject> Vector(const FVector& V)
{
	auto J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("x"), V.X);
	J->SetNumberField(TEXT("y"), V.Y);
	J->SetNumberField(TEXT("z"), V.Z);
	return J;
}
TSharedRef<FJsonObject> Point(const FVector2D& P)
{
	auto J = MakeShared<FJsonObject>();
	J->SetNumberField(TEXT("x"), P.X);
	J->SetNumberField(TEXT("y"), P.Y);
	return J;
}
FString Encode(const TSharedRef<FJsonObject>& J)
{
	FString S;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&S);
	FJsonSerializer::Serialize(J, Writer);
	return S;
}
bool IsPng(const FString& Path)
{
	TArray<uint8> Bytes;
	static const uint8 Signature[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
	return FFileHelper::LoadFileToArray(Bytes, *Path) && Bytes.Num() > 24 &&
		FMemory::Memcmp(Bytes.GetData(), Signature, sizeof(Signature)) == 0;
}
}

bool URokUnityTileRippleRendererAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return IsRokProject() &&
		Cast<UWorld>(Outer) && Cast<UWorld>(Outer)->GetOutermost()->GetName() == WorldMap &&
		FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokTileRippleDiagnostic")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokTileRippleRendererAudit")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokUnityTileRippleRendererAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game;
}
void URokUnityTileRippleRendererAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RunDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PendingUnityTileRippleRendererV14/RuntimeAudit") /
		FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*RunDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokUnityTileRippleRendererAuditSubsystem::ScreenshotProcessed);
}
void URokUnityTileRippleRendererAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (!bFinished) Finish(TEXT("Tile ripple renderer audit interrupted"));
	if (PrivateCapture) PrivateCapture->DestroyComponent();
	PrivateCapture = nullptr;
	PrivateTarget = nullptr;
	if (DiagnosticActor) DiagnosticActor->Destroy();
	DiagnosticActor = nullptr;
	Super::Deinitialize();
}
TStatId URokUnityTileRippleRendererAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokUnityTileRippleRendererAuditSubsystem, STATGROUP_Tickables);
}
bool URokUnityTileRippleRendererAuditSubsystem::Boundary() const
{
	const UWorld* World = GetWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	URokServerSubsystem* Server = Game ? Game->GetSubsystem<URokServerSubsystem>() : nullptr;
	return World && World->WorldType == EWorldType::Game &&
		World->GetOutermost()->GetName() == WorldMap &&
		IsRokProject() &&
		Server && Server->IsLocalOnlyRun() && Server->GetHttpDispatchAttemptCount() == 0;
}
bool URokUnityTileRippleRendererAuditSubsystem::Begin()
{
	if (!Boundary()) return false;
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return false;
	// A camera-centred transient location proves the renderer can draw; it is not a tile/LOD placement.
	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Forward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	if (Forward.IsNearlyZero() || CameraLocation.ContainsNaN()) return false;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	DiagnosticActor = World->SpawnActor<ARokUnityTileRippleDiagnosticActor>(
		ARokUnityTileRippleDiagnosticActor::StaticClass(), CameraLocation + Forward * 1500.0, FRotator::ZeroRotator, Params);
	return DiagnosticActor && DiagnosticActor->IsSourceRendererReady();
}
bool URokUnityTileRippleRendererAuditSubsystem::CapturePrivateGpuFrames(FString& Error)
{
	if (!Boundary() || !DiagnosticActor || !DiagnosticActor->IsSourceRendererReady())
	{
		Error = TEXT("Tile ripple private GPU capture lost boundary"); return false;
	}
	// Freeze the current live CPU-generated quads. ShowOnly captures then use identical
	// geometry, camera and target, with no world/UI primitives in the render list.
	DiagnosticActor->SetActorTickEnabled(false);
	const double FrozenSeconds = DiagnosticActor->GetSimElapsedSeconds();
	if (FrozenSeconds < TargetTimes[3] || FrozenSeconds > TargetTimes[3] + MaxLag[3])
	{
		Error = TEXT("Tile ripple private GPU capture outside final sample window"); return false;
	}
	UProceduralMeshComponent* Mesh0 = DiagnosticActor->GetEmitterMesh(0);
	UProceduralMeshComponent* Mesh1 = DiagnosticActor->GetEmitterMesh(1);
	if (!Mesh0 || !Mesh1 || !Mesh0->GetProcMeshSection(0) || !Mesh1->GetProcMeshSection(0))
	{
		Error = TEXT("Tile ripple private GPU capture has no two live sections"); return false;
	}
	PrivateTargetDiagnostic = MakeShared<FJsonObject>();
	PrivateTargetDiagnostic->SetObjectField(TEXT("requested_extent"), Point(FVector2D(PrivateExtent, PrivateExtent)));
	PrivateTargetDiagnostic->SetNumberField(TEXT("requested_format"), static_cast<int32>(PF_B8G8R8A8));
	PrivateTargetDiagnostic->SetNumberField(TEXT("requested_samples"), 1);
	PrivateTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	if (!PrivateTarget)
	{
		PrivateTargetDiagnostic->SetBoolField(TEXT("resource_present"), false);
		PrivateTargetDiagnostic->SetBoolField(TEXT("rhi_valid"), false);
		PrivateTargetDiagnostic->SetBoolField(TEXT("synchronized_after_update"), false);
		PrivateTargetDiagnostic->SetObjectField(TEXT("actual_extent"), Point(FVector2D(0, 0)));
		PrivateTargetDiagnostic->SetNumberField(TEXT("actual_format"), -1);
		PrivateTargetDiagnostic->SetNumberField(TEXT("actual_samples"), 0);
		UE_LOG(LogTemp, Error, TEXT("Tile ripple private target RHI: resource=0 rhi=0 extent=0x0 format=-1 samples=0 expected=%dx%d format=%d samples=1"),
			PrivateExtent, PrivateExtent, static_cast<int32>(PF_B8G8R8A8));
		Error = TEXT("Tile ripple private target allocation failed: resource=0 rhi=0 extent=0x0 format=-1 samples=0");
		return false;
	}
	PrivateTarget->ClearColor = FLinearColor::Black;
	PrivateTarget->InitCustomFormat(PrivateExtent, PrivateExtent, PF_B8G8R8A8, false);
	PrivateTarget->UpdateResourceImmediate(true);
	// Both UpdateResource and UpdateResourceImmediate enqueue render-thread work.
	// Read the actual RHI only after both queues complete; keep the exact format gate.
	FlushRenderingCommands();
	FTextureRenderTargetResource* TargetResource = PrivateTarget->GameThread_GetRenderTargetResource();
	const FTextureRHIRef TargetRhi = TargetResource ? TargetResource->GetRenderTargetTexture() : FTextureRHIRef();
	const bool bTargetRhiValid = TargetRhi.IsValid();
	const FIntPoint ActualExtent = bTargetRhiValid ? TargetRhi->GetDesc().Extent : FIntPoint::ZeroValue;
	const int32 ActualFormat = bTargetRhiValid ? static_cast<int32>(TargetRhi->GetDesc().Format) : -1;
	const int32 ActualSamples = bTargetRhiValid ? TargetRhi->GetDesc().NumSamples : 0;
	PrivateTargetDiagnostic->SetBoolField(TEXT("resource_present"), TargetResource != nullptr);
	PrivateTargetDiagnostic->SetBoolField(TEXT("rhi_valid"), bTargetRhiValid);
	PrivateTargetDiagnostic->SetBoolField(TEXT("synchronized_after_update"), true);
	PrivateTargetDiagnostic->SetObjectField(TEXT("actual_extent"), Point(FVector2D(ActualExtent.X, ActualExtent.Y)));
	PrivateTargetDiagnostic->SetNumberField(TEXT("actual_format"), ActualFormat);
	PrivateTargetDiagnostic->SetNumberField(TEXT("actual_samples"), ActualSamples);
	UE_LOG(LogTemp, Display, TEXT("Tile ripple private target RHI: resource=%d rhi=%d extent=%dx%d format=%d samples=%d expected=%dx%d format=%d samples=1"),
		TargetResource != nullptr, bTargetRhiValid, ActualExtent.X, ActualExtent.Y, ActualFormat,
		ActualSamples, PrivateExtent, PrivateExtent, static_cast<int32>(PF_B8G8R8A8));
	if (!bTargetRhiValid || ActualExtent != FIntPoint(PrivateExtent, PrivateExtent)
		|| ActualFormat != static_cast<int32>(PF_B8G8R8A8) || ActualSamples != 1)
	{
		Error = FString::Printf(TEXT("Tile ripple private target RHI descriptor differs: resource=%d rhi=%d extent=%dx%d format=%d samples=%d expected=%dx%d format=%d samples=1"),
			TargetResource != nullptr, bTargetRhiValid, ActualExtent.X, ActualExtent.Y, ActualFormat,
			ActualSamples, PrivateExtent, PrivateExtent, static_cast<int32>(PF_B8G8R8A8));
		return false;
	}
	PrivateCapture = NewObject<USceneCaptureComponent2D>(DiagnosticActor, NAME_None, RF_Transient);
	PrivateCapture->SetupAttachment(DiagnosticActor->GetRootComponent());
	PrivateCapture->TextureTarget = PrivateTarget;
	PrivateCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	PrivateCapture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	PrivateCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	PrivateCapture->bCaptureEveryFrame = false;
	PrivateCapture->bCaptureOnMovement = false;
	PrivateCapture->RegisterComponent();
	// A BGRA8 FinalColorLDR capture otherwise adds tonemapper quantization dither
	// to black even when ShowOnly contains no primitives. Disable postprocess
	// only for this transient diagnostic capture; the viewport is untouched.
	PrivateCapture->ShowFlags.SetPostProcessing(false);
	PrivateCapture->PostProcessBlendWeight = 0.0f;
	PrivateCapture->bInheritMainViewCameraPostProcessSettings = false;
	FBox Box(EForceInit::ForceInit);
	Box += Mesh0->Bounds.GetBox();
	Box += Mesh1->Bounds.GetBox();
	if (!Box.IsValid || Box.GetSize().GetAbsMax() < 1.0 || Box.GetSize().GetAbsMax() > 100000.0)
	{
		Error = TEXT("Tile ripple private capture mesh bounds invalid"); return false;
	}
	PrivateCapture->OrthoWidth = FMath::Max(600.0, FMath::Max(Box.GetSize().X, Box.GetSize().Y) * 1.4);
	PrivateCapture->SetWorldLocation(Box.GetCenter() + FVector(0, 0, 1000));
	PrivateCapture->SetWorldRotation(FRotator(-90, 0, 0));
	TArray<TArray<uint8>> GpuRaw;
	for (int32 Variant = 0; Variant < 4; ++Variant)
	{
		PrivateCapture->ClearShowOnlyComponents();
		if (Variant == 1 || Variant == 3) PrivateCapture->ShowOnlyComponent(Mesh0);
		if (Variant == 2 || Variant == 3) PrivateCapture->ShowOnlyComponent(Mesh1);
		TArray<UPrimitiveComponent*> ExpectedComponents;
		if (Variant == 1 || Variant == 3) ExpectedComponents.Add(Mesh0);
		if (Variant == 2 || Variant == 3) ExpectedComponents.Add(Mesh1);
		TArray<TSharedPtr<FJsonValue>> ActualComponentPaths;
		if (PrivateCapture->PrimitiveRenderMode != ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList
			|| !PrivateCapture->ShowOnlyActors.IsEmpty()
			|| PrivateCapture->ShowOnlyComponents.Num() != ExpectedComponents.Num())
		{
			Error = TEXT("Tile ripple private ShowOnly actor/component set differs"); return false;
		}
		for (int32 ComponentIndex = 0; ComponentIndex < ExpectedComponents.Num(); ++ComponentIndex)
		{
			UPrimitiveComponent* Actual = PrivateCapture->ShowOnlyComponents[ComponentIndex].Get();
			if (!Actual || Actual != ExpectedComponents[ComponentIndex] || Actual->GetOwner() != DiagnosticActor)
			{
				Error = TEXT("Tile ripple private ShowOnly identity differs"); return false;
			}
			ActualComponentPaths.Add(MakeShared<FJsonValueString>(Actual->GetPathName()));
		}
		if (PrivateCapture->ShowFlags.PostProcessing || !PrivateCapture->ShowFlags.Tonemapper
			|| PrivateCapture->PostProcessBlendWeight != 0.0f
			|| PrivateCapture->bInheritMainViewCameraPostProcessSettings)
		{
			Error = TEXT("Tile ripple private capture postprocess isolation changed"); return false;
		}
		PrivateCapture->CaptureScene();
		TArray<FColor> Pixels;
		if (!TargetResource->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm)) ||
			Pixels.Num() != PrivateExtent * PrivateExtent ||
			DiagnosticActor->GetSimElapsedSeconds() != FrozenSeconds)
		{
			Error = TEXT("Tile ripple private target GPU readback/frozen clock failed"); return false;
		}
		TArray<uint8> Raw;
		Raw.Append(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor));
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(PrivateExtent, PrivateExtent,
			TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		if (Raw.Num() != PrivateExtent * PrivateExtent * 4 || Png.IsEmpty())
		{
			Error = TEXT("Tile ripple private GPU output size invalid"); return false;
		}
		const FString Name = Variant == 0 ? TEXT("baseline") :
			Variant == 1 ? TEXT("bowen_only") : Variant == 2 ? TEXT("bowen1_only") : TEXT("combined");
		const FString RawPath = RunDirectory / (Name + TEXT(".gpu.bgra8.bin"));
		const FString PngPath = RunDirectory / (Name + TEXT(".gpu.png"));
		if (!FFileHelper::SaveArrayToFile(Raw, *RawPath) ||
			!FFileHelper::SaveArrayToFile(Png, *PngPath) || !IsPng(PngPath))
		{
			Error = TEXT("Tile ripple private GPU raw/PNG save failed"); return false;
		}
		auto Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("variant"), Name);
		Frame->SetStringField(TEXT("raw_bgra8"), RawPath);
		Frame->SetStringField(TEXT("raw_sha256"), FileSha(RawPath));
		Frame->SetNumberField(TEXT("raw_bytes"), Raw.Num());
		Frame->SetStringField(TEXT("png"), PngPath);
		Frame->SetStringField(TEXT("png_sha256"), FileSha(PngPath));
		Frame->SetNumberField(TEXT("png_bytes"), Png.Num());
		Frame->SetNumberField(TEXT("frozen_sim_seconds"), FrozenSeconds);
		Frame->SetNumberField(TEXT("width"), PrivateExtent);
		Frame->SetNumberField(TEXT("height"), PrivateExtent);
		Frame->SetNumberField(TEXT("rhi_format"), static_cast<int32>(TargetRhi->GetDesc().Format));
		Frame->SetNumberField(TEXT("show_only_component_count"), PrivateCapture->ShowOnlyComponents.Num());
		Frame->SetArrayField(TEXT("show_only_component_paths"), ActualComponentPaths);
		Frame->SetNumberField(TEXT("show_only_actor_count"), PrivateCapture->ShowOnlyActors.Num());
		Frame->SetStringField(TEXT("capture_component_path"), PrivateCapture->GetPathName());
		Frame->SetStringField(TEXT("capture_owner_path"), DiagnosticActor->GetPathName());
		Frame->SetStringField(TEXT("capture_projection"),
			PrivateCapture->ProjectionType == ECameraProjectionMode::Orthographic ? TEXT("Orthographic") : TEXT("Other"));
		Frame->SetStringField(TEXT("capture_source"),
			PrivateCapture->CaptureSource == ESceneCaptureSource::SCS_FinalColorLDR ? TEXT("FinalColorLDR") : TEXT("Other"));
		Frame->SetStringField(TEXT("primitive_render_mode"), TEXT("UseShowOnlyList"));
		auto ShowFlagProvenance = MakeShared<FJsonObject>();
		ShowFlagProvenance->SetBoolField(TEXT("post_processing"), PrivateCapture->ShowFlags.PostProcessing);
		ShowFlagProvenance->SetBoolField(TEXT("tonemapper"), PrivateCapture->ShowFlags.Tonemapper);
		ShowFlagProvenance->SetBoolField(TEXT("grain"), PrivateCapture->ShowFlags.Grain);
		ShowFlagProvenance->SetBoolField(TEXT("vignette"), PrivateCapture->ShowFlags.Vignette);
		ShowFlagProvenance->SetBoolField(TEXT("anti_aliasing"), PrivateCapture->ShowFlags.AntiAliasing);
		ShowFlagProvenance->SetBoolField(TEXT("temporal_aa"), PrivateCapture->ShowFlags.TemporalAA);
		ShowFlagProvenance->SetBoolField(TEXT("atmosphere"), PrivateCapture->ShowFlags.Atmosphere);
		Frame->SetObjectField(TEXT("capture_show_flags"), ShowFlagProvenance);
		Frame->SetNumberField(TEXT("postprocess_blend_weight"), PrivateCapture->PostProcessBlendWeight);
		Frame->SetBoolField(TEXT("inherits_main_view_postprocess"),
			PrivateCapture->bInheritMainViewCameraPostProcessSettings);
		Frame->SetObjectField(TEXT("capture_world_location"), Vector(PrivateCapture->GetComponentLocation()));
		Frame->SetObjectField(TEXT("capture_world_rotation"), Vector(FVector(
			PrivateCapture->GetComponentRotation().Pitch,
			PrivateCapture->GetComponentRotation().Yaw,
			PrivateCapture->GetComponentRotation().Roll)));
		Frame->SetNumberField(TEXT("capture_ortho_width"), PrivateCapture->OrthoWidth);
		Frame->SetObjectField(TEXT("source_bounds_center"), Vector(Box.GetCenter()));
		Frame->SetObjectField(TEXT("source_bounds_size"), Vector(Box.GetSize()));
		Frame->SetObjectField(TEXT("target_rhi_extent"), Point(FVector2D(
			TargetRhi->GetDesc().Extent.X, TargetRhi->GetDesc().Extent.Y)));
		Frame->SetBoolField(TEXT("rhi_resource_ready"), true);
		Frame->SetBoolField(TEXT("gpu_readpixels_succeeded"), true);
		PrivateGpuFrames.Add(MakeShared<FJsonValueObject>(Frame));
		GpuRaw.Add(MoveTemp(Raw));
	}
	auto CountChanged = [](const TArray<uint8>& A, const TArray<uint8>& B, int32 Threshold)
	{
		int32 Count = 0;
		for (int32 Pos = 0; Pos < A.Num(); Pos += 4)
		{
			const int32 Delta = FMath::Max3(FMath::Abs(int32(A[Pos]) - int32(B[Pos])),
				FMath::Abs(int32(A[Pos + 1]) - int32(B[Pos + 1])),
				FMath::Abs(int32(A[Pos + 2]) - int32(B[Pos + 2])));
			if (Delta >= Threshold) ++Count;
		}
		return Count;
	};
	int32 BaselineSpread = 0;
	for (int32 Pos = 4; Pos < GpuRaw[0].Num(); Pos += 4)
		BaselineSpread = FMath::Max(BaselineSpread, FMath::Max3(
			FMath::Abs(int32(GpuRaw[0][Pos]) - int32(GpuRaw[0][0])),
			FMath::Abs(int32(GpuRaw[0][Pos + 1]) - int32(GpuRaw[0][1])),
			FMath::Abs(int32(GpuRaw[0][Pos + 2]) - int32(GpuRaw[0][2]))));
	const int32 BowenChanged = CountChanged(GpuRaw[0], GpuRaw[1], 8);
	const int32 Bowen1Changed = CountChanged(GpuRaw[0], GpuRaw[2], 8);
	const int32 BetweenEmitters = CountChanged(GpuRaw[1], GpuRaw[2], 8);
	const int32 CombinedFromBowen = CountChanged(GpuRaw[1], GpuRaw[3], 8);
	const int32 CombinedFromBowen1 = CountChanged(GpuRaw[2], GpuRaw[3], 8);
	PrivateGpuMetrics = MakeShared<FJsonObject>();
	PrivateGpuMetrics->SetNumberField(TEXT("baseline_rgb_spread"), BaselineSpread);
	PrivateGpuMetrics->SetNumberField(TEXT("bowen_vs_baseline_pixels_over7"), BowenChanged);
	PrivateGpuMetrics->SetNumberField(TEXT("bowen1_vs_baseline_pixels_over7"), Bowen1Changed);
	PrivateGpuMetrics->SetNumberField(TEXT("between_emitters_pixels_over7"), BetweenEmitters);
	PrivateGpuMetrics->SetNumberField(TEXT("combined_vs_bowen_pixels_over7"), CombinedFromBowen);
	PrivateGpuMetrics->SetNumberField(TEXT("combined_vs_bowen1_pixels_over7"), CombinedFromBowen1);
	if (BaselineSpread > 2 || BowenChanged < 64 || Bowen1Changed < 64 ||
		BetweenEmitters < 64 || CombinedFromBowen < 32 || CombinedFromBowen1 < 32)
	{
		Error = TEXT("Tile ripple private GPU isolated emitter pixel proof failed"); return false;
	}
	return true;
}
TSharedRef<FJsonObject> URokUnityTileRippleRendererAuditSubsystem::RenderState() const
{
	auto J = MakeShared<FJsonObject>();
	J->SetBoolField(TEXT("actor_ready"), DiagnosticActor && DiagnosticActor->IsSourceRendererReady());
	J->SetNumberField(TEXT("sim_seconds"), DiagnosticActor ? DiagnosticActor->GetSimElapsedSeconds() : -1.0);
	J->SetBoolField(TEXT("http_zero"), Boundary());
	J->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	J->SetObjectField(TEXT("actor_location"), Vector(DiagnosticActor ? DiagnosticActor->GetActorLocation() : FVector::ZeroVector));
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector2D Screen = FVector2D::ZeroVector;
	int32 Width = 0, Height = 0;
	if (PC) PC->GetViewportSize(Width, Height);
	const bool bProjected = PC && DiagnosticActor &&
		PC->ProjectWorldLocationToScreen(DiagnosticActor->GetActorLocation(), Screen) &&
		Screen.X >= 0 && Screen.Y >= 0 && Screen.X < Width && Screen.Y < Height;
	J->SetBoolField(TEXT("actor_center_on_screen"), bProjected);
	J->SetObjectField(TEXT("actor_screen_center"), Point(Screen));
	J->SetNumberField(TEXT("viewport_width"), Width);
	J->SetNumberField(TEXT("viewport_height"), Height);
	UTexture2D* Texture = DiagnosticActor ? DiagnosticActor->GetSourceTexture() : nullptr;
	const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
	const FTextureRHIRef Rhi = Resource ? Resource->TextureRHI : FTextureRHIRef();
	const bool bRhiReady = Rhi.IsValid();
	J->SetBoolField(TEXT("texture_rhi_ready"), bRhiReady);
	J->SetStringField(TEXT("texture_path"), Texture ? Texture->GetPathName() : FString());
	J->SetNumberField(TEXT("texture_width"), Texture ? Texture->GetSizeX() : 0);
	J->SetNumberField(TEXT("texture_height"), Texture ? Texture->GetSizeY() : 0);
	J->SetBoolField(TEXT("texture_srgb"), Texture && Texture->SRGB);
	J->SetNumberField(TEXT("rhi_width"), bRhiReady ? Rhi->GetDesc().Extent.X : 0);
	J->SetNumberField(TEXT("rhi_height"), bRhiReady ? Rhi->GetDesc().Extent.Y : 0);
	J->SetNumberField(TEXT("rhi_format"), bRhiReady ? static_cast<int32>(Rhi->GetDesc().Format) : -1);
	J->SetNumberField(TEXT("rhi_mips"), bRhiReady ? Rhi->GetDesc().NumMips : 0);
	TArray<TSharedPtr<FJsonValue>> Emitters;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto E = MakeShared<FJsonObject>();
		UProceduralMeshComponent* Mesh = DiagnosticActor ? DiagnosticActor->GetEmitterMesh(Index) : nullptr;
		const FProcMeshSection* Section = Mesh ? Mesh->GetProcMeshSection(0) : nullptr;
		E->SetNumberField(TEXT("index"), Index);
		E->SetStringField(TEXT("source_name"), Index == 0 ? TEXT("bowen") : TEXT("bowen (1)"));
		E->SetStringField(TEXT("component_path"), Mesh ? Mesh->GetPathName() : FString());
		E->SetStringField(TEXT("owner_path"), Mesh && Mesh->GetOwner() ? Mesh->GetOwner()->GetPathName() : FString());
		E->SetNumberField(TEXT("live_count"), DiagnosticActor ? DiagnosticActor->GetLiveParticleCount(Index) : 0);
		E->SetNumberField(TEXT("vertex_count"), Section ? Section->ProcVertexBuffer.Num() : 0);
		E->SetNumberField(TEXT("index_count"), Section ? Section->ProcIndexBuffer.Num() : 0);
		E->SetBoolField(TEXT("render_state_created"), Mesh && Mesh->IsRenderStateCreated());
		E->SetBoolField(TEXT("visible"), Mesh && Mesh->IsVisible());
		E->SetStringField(TEXT("material_path"), Mesh && Mesh->GetMaterial(0) &&
			Mesh->GetMaterial(0)->GetMaterial() ? Mesh->GetMaterial(0)->GetMaterial()->GetPathName() : FString());
		Emitters.Add(MakeShared<FJsonValueObject>(E));
	}
	J->SetArrayField(TEXT("emitters"), Emitters);
	return J;
}
void URokUnityTileRippleRendererAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || Stage != 3 || !PendingCapture.IsValid() || bScreenshotDone) return;
	if (!IsPng(PendingPath) || !Boundary()) { Finish(TEXT("Tile ripple processed screenshot PNG or local boundary absent")); return; }
	if (!DiagnosticActor || DiagnosticActor->GetSimElapsedSeconds() >
		TargetTimes[CaptureIndex] + MaxLag[CaptureIndex])
	{ Finish(TEXT("Tile ripple processed screenshot sample window missed")); return; }
	PendingCapture->SetStringField(TEXT("png_sha256"), FileSha(PendingPath));
	PendingCapture->SetNumberField(TEXT("png_bytes"), IFileManager::Get().FileSize(*PendingPath));
	PendingCapture->SetObjectField(TEXT("processed_state"), RenderState());
	PendingCapture->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
bool URokUnityTileRippleRendererAuditSubsystem::Save(bool bComplete) const
{
	auto J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("schema"), TEXT("rok.tile_ripple.renderer_diagnostic.v3"));
	J->SetBoolField(TEXT("complete"), bComplete);
	J->SetBoolField(TEXT("passed"), bComplete && Captures.Num() == 4 && PrivateGpuFrames.Num() == 4);
	J->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
	J->SetBoolField(TEXT("map_lod_accepted"), false);
	J->SetBoolField(TEXT("unity_rng_fixedstep_accepted"), false);
	J->SetStringField(TEXT("failure"), FailureReason);
	J->SetStringField(TEXT("scope"), TEXT("Two live UE CPU emitters with V5-derived quad geometry, real ProceduralMesh render resources and screenshots; diagnostic camera-relative placement"));
	J->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	J->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	J->SetStringField(TEXT("world_map"), WorldMap);
	J->SetStringField(TEXT("geometry_fixture_sha256"), FileSha(FPaths::ProjectDir() / ContractRelative));
	J->SetStringField(TEXT("cpu_fixture_sha256"), FileSha(FPaths::ProjectDir() / CpuRelative));
	FString ExternalAssetSha;
	FParse::Value(FCommandLine::Get(), TEXT("RokTileRippleAssetManifestSha="), ExternalAssetSha);
	J->SetStringField(TEXT("expected_asset_manifest_sha256"), ExternalAssetSha.ToLower());
	J->SetStringField(TEXT("asset_manifest_sha256"), FileSha(FPaths::ProjectDir() /
		TEXT("Saved/PendingUnityTileRippleRendererV8/asset_import_manifest.json")));
	FString ExternalReadySha;
	FParse::Value(FCommandLine::Get(), TEXT("RokTileRippleReadySha="), ExternalReadySha);
	J->SetStringField(TEXT("expected_ready_sha256"), ExternalReadySha.ToLower());
	J->SetStringField(TEXT("ready_sha256"), FileSha(FPaths::ProjectDir() / ReadyRelative));
	J->SetStringField(TEXT("renderer_cpp_sha256"), FileSha(FPaths::ProjectDir() / TEXT("Source/Rok/RokUnityTileRippleRenderer.cpp")));
	J->SetStringField(TEXT("audit_cpp_sha256"), FileSha(FPaths::ProjectDir() / TEXT("Source/Rok/RokUnityTileRippleRendererAuditSubsystem.cpp")));
	J->SetStringField(TEXT("texture_asset"), TexturePath);
	J->SetStringField(TEXT("material_asset"), MaterialPath);
	J->SetArrayField(TEXT("captures"), Captures);
	J->SetArrayField(TEXT("private_gpu_frames"), PrivateGpuFrames);
	J->SetObjectField(TEXT("private_gpu_metrics"), PrivateGpuMetrics.IsValid()
		? PrivateGpuMetrics.ToSharedRef() : MakeShared<FJsonObject>());
	J->SetObjectField(TEXT("private_target_diagnostic"), PrivateTargetDiagnostic.IsValid()
		? PrivateTargetDiagnostic.ToSharedRef() : MakeShared<FJsonObject>());
	J->SetStringField(TEXT("private_gpu_proof_scope"),
		TEXT("SceneCapture ShowOnly actual ProcMesh raster in private BGRA8 target, four GPU ReadPixels; no GPU vertex-buffer readback or Unity pixel parity"));
	J->SetObjectField(TEXT("final_state"), RenderState());
	return FFileHelper::SaveStringToFile(Encode(J), *(RunDirectory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokUnityTileRippleRendererAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	bFinished = true;
	FailureReason = Failure;
	const bool bComplete = Failure.IsEmpty() && Boundary() && Captures.Num() == 4 && PrivateGpuFrames.Num() == 4;
	if (!bComplete) UE_LOG(LogTemp, Error, TEXT("Tile ripple renderer audit failed: %s"), *Failure);
	Save(bComplete);
	UE_LOG(LogTemp, Display, TEXT("Tile ripple renderer audit %s: %s"),
		bComplete ? TEXT("passed") : TEXT("failed"), *RunDirectory);
	FPlatformMisc::RequestExitWithStatus(false, bComplete ? 0 : 1);
}
void URokUnityTileRippleRendererAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	WallSeconds += FMath::Max(0.0f, DeltaTime);
	if (WallSeconds > 50.0) { Finish(TEXT("Tile ripple renderer audit timeout")); return; }
	if (Stage == 0)
	{
		if (WallSeconds < 2.0) return;
		FString ExpectedReady;
		if (!FParse::Value(FCommandLine::Get(), TEXT("RokTileRippleReadySha="), ExpectedReady) ||
			ExpectedReady.Len() != 64 || FileSha(FPaths::ProjectDir() / ReadyRelative) != ExpectedReady.ToLower())
		{ Finish(TEXT("Tile ripple externally locked V3 ready report SHA missing or changed")); return; }
		if (Begin()) Stage = 1;
		else if (DiagnosticActor) Finish(TEXT("Tile ripple diagnostic actor failed source/material/CPU setup"));
		return;
	}
	if (!Boundary() || !DiagnosticActor || !DiagnosticActor->IsSourceRendererReady())
	{ Finish(TEXT("Tile ripple renderer lost source/local boundary")); return; }
	if (Stage == 1)
	{
		if (CaptureIndex == 4)
		{
			FString Error;
			if (!CapturePrivateGpuFrames(Error)) Finish(Error);
			else Finish();
			return;
		}
		if (DiagnosticActor->GetSimElapsedSeconds() > TargetTimes[CaptureIndex] + MaxLag[CaptureIndex])
		{ Finish(TEXT("Tile ripple natural CPU sample window missed")); return; }
		if (DiagnosticActor->GetSimElapsedSeconds() < TargetTimes[CaptureIndex]) return;
		const auto State = RenderState();
		if (!State->GetBoolField(TEXT("actor_center_on_screen")) ||
			!State->GetBoolField(TEXT("texture_rhi_ready")) ||
			State->GetStringField(TEXT("texture_path")) != TexturePath ||
			State->GetIntegerField(TEXT("rhi_width")) != 128 ||
			State->GetIntegerField(TEXT("rhi_height")) != 128)
		{ Finish(TEXT("Tile ripple renderer texture RHI or viewport state invalid")); return; }
		for (const auto& Value : State->GetArrayField(TEXT("emitters")))
		{
			const auto E = Value->AsObject();
			const int32 N = E->GetIntegerField(TEXT("live_count"));
			if (N <= 0 || E->GetIntegerField(TEXT("vertex_count")) != 4*N ||
				E->GetIntegerField(TEXT("index_count")) != 6*N ||
				!E->GetBoolField(TEXT("render_state_created")) || !E->GetBoolField(TEXT("visible")) ||
				E->GetStringField(TEXT("material_path")) != MaterialPath)
			{ Finish(TEXT("Tile ripple emitter mesh/render state invalid")); return; }
		}
		RenderFence.BeginFence();
		Stage = 2;
		return;
	}
	if (Stage == 2)
	{
		if (!RenderFence.IsFenceComplete()) return;
		if (DiagnosticActor->GetSimElapsedSeconds() > TargetTimes[CaptureIndex] + MaxLag[CaptureIndex])
		{ Finish(TEXT("Tile ripple render-fence sample window missed")); return; }
		if (FScreenshotRequest::IsScreenshotRequested())
		{ Finish(TEXT("Another screenshot request owns the viewport")); return; }
		PendingCapture = MakeShared<FJsonObject>();
		PendingCapture->SetNumberField(TEXT("index"), CaptureIndex);
		PendingCapture->SetNumberField(TEXT("target_sim_seconds"), TargetTimes[CaptureIndex]);
		PendingCapture->SetNumberField(TEXT("max_lag_seconds"), MaxLag[CaptureIndex]);
		PendingCapture->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
		PendingCapture->SetBoolField(TEXT("mesh_render_fence_complete"), true);
		PendingCapture->SetObjectField(TEXT("requested_state"), RenderState());
		PendingPath = RunDirectory / FString::Printf(TEXT("%02d_tile_ripple.png"), CaptureIndex);
		PendingCapture->SetStringField(TEXT("png"), PendingPath);
		bScreenshotDone = false;
		FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
		Stage = 3;
		return;
	}
	if (Stage == 3 && bScreenshotDone && GFrameCounter > ScreenshotFrame)
	{
		PendingCapture->SetObjectField(TEXT("next_frame_state"), RenderState());
		Captures.Add(MakeShared<FJsonValueObject>(PendingCapture));
		PendingCapture.Reset();
		++CaptureIndex;
		Save(false);
		Stage = 1;
	}
}
