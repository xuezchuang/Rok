#include "RokAcademyPresentationAuditSubsystem.h"

#include "RokCitySpriteActor.h"
#include "RokCityConstructionGrid.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokStrategyCameraPawn.h"
#include "RokTrainArmyMeshData.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"

namespace
{
constexpr int32 ImageWidth = 1280;
constexpr int32 ImageHeight = 720;
constexpr double SourceVerticalFovDegrees = 7.5;
constexpr double SourceCameraDistanceCm = 10000.0;
constexpr TCHAR NativeSha[] = TEXT("b00ac7d63d572c71f93ac30e0e446e88ef28a4c1e6af5df79e888d8540fda2e8");
constexpr TCHAR CatalogSha[] = TEXT("ce5569c8d92584710359fafb31652b4e3864eb114cf0668984c9776942d5ef5b");
constexpr TCHAR PreparedManifestSha[] = TEXT("d3f812c61a47f021b0209d8066df8cc1b7525bd5b0ddf35881d187c85e5771f0");
constexpr TCHAR MeshPath[] = TEXT("/Game/RokUnity/City/Academy/build_13_101_2/SM_build_13_101_2_SourceSpriteMesh.SM_build_13_101_2_SourceSpriteMesh");
constexpr TCHAR MaterialPath[] = TEXT("/Game/RokUnity/City/Academy/build_13_101_2/M_build_13_101_2_SourceRG.M_build_13_101_2_SourceRG");
constexpr TCHAR BackgroundMaterialPath[] = TEXT("/Game/RokDiagnostics/Academy/M_AcademyAuditBackground.M_AcademyAuditBackground");
constexpr TCHAR TemporaryTag[] = TEXT("RokAcademyPresentationTemporary");
constexpr TCHAR SourceMapSha[] = TEXT("26609d28ca4f42f3c9d0d7cb735c88a71862d1df1fd00f362da6693111b0ddc2");

FString SavePath(const TCHAR* Relative) { return FPaths::ProjectSavedDir() / Relative; }
FString ShaFile(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *Path) &&
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
TArray<TSharedPtr<FJsonValue>> Vec3(const FVector& V)
{
    return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
}
TArray<TSharedPtr<FJsonValue>> Vec2(const FVector2D& V)
{
    return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y) };
}
TArray<TSharedPtr<FJsonValue>> Color4(const FLinearColor& V)
{
    return { MakeShared<FJsonValueNumber>(V.R), MakeShared<FJsonValueNumber>(V.G),
        MakeShared<FJsonValueNumber>(V.B), MakeShared<FJsonValueNumber>(V.A) };
}
FLinearColor GammaColorToLinear(const FLinearColor& Gamma)
{
    const FColor EightBit(FMath::RoundToInt(Gamma.R * 255.0f), FMath::RoundToInt(Gamma.G * 255.0f),
        FMath::RoundToInt(Gamma.B * 255.0f), 255);
    return FLinearColor::FromSRGBColor(EightBit);
}
int32 CountActors(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<AActor> It(World); It; ++It) ++Count;
    return Count;
}
int32 CountTemporaryActors(UWorld* World)
{
    int32 Count = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
        if (It->ActorHasTag(TemporaryTag)) ++Count;
    return Count;
}
FString EconomyFingerprint(UWorld* World)
{
    const URokBuildingUpgradeSubsystem* Upgrade = World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
    if (!Upgrade) return FString();
    const FRokResourceStock Stock = Upgrade->GetCurrentResources();
    return FString::Printf(TEXT("%d,%d,%d,%d|local=%d|upgrade=%d|queue=%d"),
        Stock.Food, Stock.Wood, Stock.Stone, Stock.Gold,
        Upgrade->GetActiveLocalConstructionCount(), Upgrade->GetActiveUpgradeCount(), Upgrade->GetOwnedBuildQueueCount());
}
}

bool URokAcademyPresentationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity") &&
        FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")) &&
        FParse::Param(FCommandLine::Get(), TEXT("RokAcademyPresentationAudit")) &&
        !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokAcademyPresentationAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokAcademyPresentationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RunDirectory = FPaths::ConvertRelativePathToFull(SavePath(TEXT("RokUnityMigration/AcademyPresentationAudit")) /
        FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
            FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*RunDirectory, true);
}
void URokAcademyPresentationAuditSubsystem::Deinitialize()
{
    if (!bFinished) Finish(TEXT("Academy presentation world deinitialized before audit completed"));
    else Restore();
    Super::Deinitialize();
}
TStatId URokAcademyPresentationAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokAcademyPresentationAuditSubsystem, STATGROUP_Tickables);
}

bool URokAcademyPresentationAuditSubsystem::Begin()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    UCameraComponent* ExistingCamera = PC && PC->GetViewTarget()
        ? PC->GetViewTarget()->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!World || !PC || !World->GetGameViewport() || !PC->GetViewTarget())
    { Finish(TEXT("GameApp world/player/viewport identity incomplete")); return false; }
    PlayerController = PC;
    SavedViewTarget = PC->GetViewTarget();
    if (ExistingCamera) { SavedCameraTransform = ExistingCamera->GetComponentTransform(); SavedCameraFov = ExistingCamera->FieldOfView; }
    InitialActorCount = CountActors(World);
    InitialTemporaryActorCount = CountTemporaryActors(World);
    EconomicSnapshot = EconomyFingerprint(World);
    if (EconomicSnapshot.IsEmpty())
    { Finish(TEXT("Academy cannot snapshot read-only resources and construction queue")); return false; }
    if (InitialTemporaryActorCount != 0)
    { Finish(TEXT("Another Academy temporary preview actor already exists")); return false; }

    const FString NativePath = SavePath(TEXT("UnityAcademyVisualProbe/OutputRuns/20261008_153501_742_16460/native_render.json"));
    const FString VerifyPath = SavePath(TEXT("UnityAcademyVisualProbe/OutputRuns/20261008_153501_742_16460/native_verify.json"));
    const FString CatalogPath = SavePath(TEXT("RokUnityMigration/AcademyVisualSource/catalog.json"));
    const FString ManifestPath = SavePath(TEXT("RokUnityMigration/AcademyPresentationMesh/20261008_153501_742_16460/source_mesh_manifest.json"));
    TSharedPtr<FJsonObject> Native, Verify, Catalog, Manifest;
    NativeRenderSha = ShaFile(NativePath); NativeVerifySha = ShaFile(VerifyPath);
    SourceCatalogSha = ShaFile(CatalogPath); MeshManifestSha = ShaFile(ManifestPath);
    if (NativeRenderSha != NativeSha || SourceCatalogSha != CatalogSha || MeshManifestSha != PreparedManifestSha ||
        !ReadJson(NativePath, Native) || !ReadJson(VerifyPath, Verify) || !ReadJson(CatalogPath, Catalog) ||
        !ReadJson(ManifestPath, Manifest) || !Verify->GetBoolField(TEXT("passed")) ||
        Native->GetStringField(TEXT("schema")) != TEXT("rok.academy_visual_native_render.v2") ||
        Catalog->GetStringField(TEXT("schema")) != TEXT("rok.academy_visual_source.v1") ||
        Manifest->GetStringField(TEXT("schema")) != TEXT("rok.academy_source_sprite_mesh.v1") ||
        Manifest->GetStringField(TEXT("native_render_sha256")) != NativeRenderSha ||
        Manifest->GetStringField(TEXT("native_verify_sha256")) != NativeVerifySha ||
        Manifest->GetStringField(TEXT("source_catalog_sha256")) != SourceCatalogSha ||
        Manifest->GetIntegerField(TEXT("vertex_count")) != 49 ||
        Manifest->GetIntegerField(TEXT("triangle_count")) != 47)
    { Finish(TEXT("Pinned Academy native/catalog/mesh chain changed or failed verification")); return false; }
    const TSharedPtr<FJsonObject>* SourceFiles = nullptr;
    if (!Catalog->TryGetObjectField(TEXT("sources"), SourceFiles) || !SourceFiles || !SourceFiles->IsValid())
    { Finish(TEXT("Academy source input list missing")); return false; }
    for (const auto& Entry : (*SourceFiles)->Values)
    {
        const FString SourcePath(*Entry.Key);
        FString Expected;
        if (!(SourcePath.StartsWith(TEXT("ref/")) ||
                SourcePath == TEXT("Saved/RokUnityMigration/CityConstructionSources/catalog.json")) ||
            SourcePath.Contains(TEXT("..")) ||
            !Entry.Value.IsValid() || !Entry.Value->TryGetString(Expected) ||
            ShaFile(FPaths::ProjectDir() / SourcePath) != Expected)
        { Finish(FString::Printf(TEXT("Academy source input changed: %s"), *SourcePath)); return false; }
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
    UStaticMesh* BackgroundPlane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    UMaterialInterface* BackgroundBase = LoadObject<UMaterialInterface>(nullptr, BackgroundMaterialPath);
    if (!Mesh || !Material || !BackgroundPlane || !BackgroundBase ||
        Mesh->GetNumVertices(0) != 49 || Mesh->GetNumTriangles(0) != 47)
    { Finish(TEXT("Dedicated Academy source SpriteMesh/material missing or topology changed")); return false; }
    SourceMesh.Reset(Mesh);
    MeshAssetSha = ShaFile(FPaths::ProjectContentDir() / TEXT("RokUnity/City/Academy/build_13_101_2/SM_build_13_101_2_SourceSpriteMesh.uasset"));
    MaterialAssetSha = ShaFile(FPaths::ProjectContentDir() / TEXT("RokUnity/City/Academy/build_13_101_2/M_build_13_101_2_SourceRG.uasset"));
    BackgroundMaterialAssetSha = ShaFile(FPaths::ProjectContentDir() / TEXT("RokDiagnostics/Academy/M_AcademyAuditBackground.uasset"));
    if (MeshAssetSha.IsEmpty() || MaterialAssetSha.IsEmpty() || BackgroundMaterialAssetSha.IsEmpty())
    { Finish(TEXT("Academy mesh/material asset bytes unavailable")); return false; }
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
    UMaterialInstanceDynamic* BackgroundMID = UMaterialInstanceDynamic::Create(BackgroundBase, this);
    if (!MID || !BackgroundMID) { Finish(TEXT("Cannot create transient Academy material instances")); return false; }
    AuditMaterial.Reset(MID);
    BackgroundMaterial.Reset(BackgroundMID);

    FActorSpawnParameters Params;
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>(FVector::ZeroVector, FRotator(0, -45, 0), Params);
    if (!Actor) { Finish(TEXT("Cannot spawn temporary Academy presentation actor")); return false; }
    TemporaryActor = Actor;
    Actor->Tags.Add(TemporaryTag);
    const FQuat ChildRotation(-.3535533845424652, -.1464466005563736, .3535533845424652, .8535534739494324);
    const FTransform Child(ChildRotation, FVector(-16.00000075995922, 10.999999940395355, 18.99999938905239),
        FVector(.6630877405405045));
    Actor->ConfigureSourceMesh(Mesh, MID, Child, 0);
    UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
    if (!Component || Component->GetStaticMesh() != Mesh || Component->GetMaterial(0) != MID ||
        Component->TranslucencySortPriority != 0 || Component->GetRelativeTransform().GetScale3D().Equals(Child.GetScale3D(), .00001) == false)
    { Finish(TEXT("Temporary Academy actor did not bind source mesh/material/pose")); return false; }
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!RecordMesh(MeshRecord)) return false;

    // UE scene capture initializes its view background to black regardless of the RT clear color.
    // Draw the Unity camera clear color through the same GPU capture, behind the source sprite.
    UStaticMeshComponent* Background = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
    if (!Background) { Finish(TEXT("Cannot create transient source-camera background plane")); return false; }
    Actor->AddInstanceComponent(Background);
    Background->SetupAttachment(Actor->GetRootComponent());
    Background->SetStaticMesh(BackgroundPlane);
    Background->SetMaterial(0, BackgroundMID);
    Background->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Background->SetCastShadow(false);
    Background->RegisterComponentWithWorld(World);
    const FVector Forward = FVector(0, .7, -.7).GetSafeNormal();
    Background->SetWorldLocation(Forward * 2000.0);
    Background->SetWorldRotation(FRotationMatrix::MakeFromZ(-Forward).Rotator());
    Background->SetWorldScale3D(FVector(40.0));
    BackgroundComponent = Background;

    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
    // GammaOnly writes display-encoded RGB8; the RT itself must not sRGB-encode it again.
    RT->InitCustomFormat(ImageWidth, ImageHeight, PF_B8G8R8A8, true);
    RT->TargetGamma = 2.2f;
    RT->ClearColor = FLinearColor::Black;
    RT->UpdateResourceImmediate(true);
    RenderTarget.Reset(RT);
    USceneCaptureComponent2D* SC = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
    SC->TextureTarget = RT;
    SC->CaptureSource = SCS_FinalColorLDR;
    SC->CompositeMode = SCCM_Overwrite;
    SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SC->ShowOnlyComponents.Add(Component);
    SC->ShowOnlyComponents.Add(Background);
    SC->bCaptureEveryFrame = false;
    SC->bCaptureOnMovement = false;
    SC->ProjectionType = ECameraProjectionMode::Perspective;
    SC->FOVAngle = FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(SourceVerticalFovDegrees) / 2.0) *
        static_cast<double>(ImageWidth) / ImageHeight));
    SC->SetWorldLocation(-Forward * SourceCameraDistanceCm);
    SC->SetWorldRotation(Forward.Rotation());
    SC->RegisterComponentWithWorld(World);
    // OnRegister calls UpdateShowFlags; all capture flags must be set afterwards.
    SC->ShowFlags.SetAntiAliasing(false);
    SC->ShowFlags.SetTemporalAA(false);
    SC->ShowFlags.SetEyeAdaptation(false);
    SC->ShowFlags.SetMotionBlur(false);
    SC->ShowFlags.SetPostProcessing(true);
    SC->ShowFlags.SetTonemapper(false);
    SC->ShowFlags.SetColorGrading(false);
    SC->ShowFlags.SetBloom(false);
    SC->ShowFlags.SetVignette(false);
    SC->ShowFlags.SetGrain(false);
    SC->ShowFlags.SetDepthOfField(false);
    SC->ShowFlags.SetAmbientOcclusion(false);
    SC->ShowFlags.SetScreenSpaceReflections(false);
    SC->ShowFlags.SetAtmosphere(false);
    SC->ShowFlags.SetDeferredAtmospherePass(false);
    SC->ShowFlags.SetFog(false);
    SC->ShowFlags.SetVolumetricFog(false);
    SC->ShowFlags.SetCloud(false);
    CaptureComponent.Reset(SC);
    Stage = 0;
    StageStart = Elapsed;
    return true;
}

bool URokAcademyPresentationAuditSubsystem::RecordMesh(TSharedPtr<FJsonObject>& Out)
{
    UStaticMesh* Mesh = SourceMesh.Get();
    UStaticMeshComponent* Component = TemporaryActor.IsValid() ? TemporaryActor->GetSpriteMeshComponent() : nullptr;
    if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty() || !Component)
    { Finish(TEXT("Academy imported LOD0 render geometry unavailable")); return false; }
    const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
    if (LOD.GetNumVertices() != 49 || LOD.GetNumTriangles() != 47 ||
        LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() != 1)
    { Finish(TEXT("Academy LOD0 render vertex/triangle/UV counts differ")); return false; }
    Out = MakeShared<FJsonObject>();
    Out->SetNumberField(TEXT("vertex_count"), LOD.GetNumVertices());
    Out->SetNumberField(TEXT("triangle_count"), LOD.GetNumTriangles());
    Out->SetNumberField(TEXT("uv_channel_count"), LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
    Out->SetStringField(TEXT("mesh_asset"), Mesh->GetPathName());
    Out->SetStringField(TEXT("material_asset"), MaterialPath);
    Out->SetNumberField(TEXT("sort_priority"), Component->TranslucencySortPriority);
    Out->SetArrayField(TEXT("actor_location"), Vec3(TemporaryActor->GetActorLocation()));
    Out->SetArrayField(TEXT("actor_rotation_euler"), Vec3(TemporaryActor->GetActorRotation().Euler()));
    Out->SetArrayField(TEXT("component_relative_location"), Vec3(Component->GetRelativeLocation()));
    Out->SetArrayField(TEXT("component_relative_scale"), Vec3(Component->GetRelativeScale3D()));
    const FQuat Q = Component->GetRelativeRotation().Quaternion();
    Out->SetArrayField(TEXT("component_relative_quaternion_xyzw"), {
        MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y),
        MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W) });
    TArray<TSharedPtr<FJsonValue>> Vertices;
    for (int32 Index = 0; Index < static_cast<int32>(LOD.GetNumVertices()); ++Index)
    {
        const FVector Local(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Index));
        const FVector World = Component->GetComponentTransform().TransformPosition(Local);
        const FVector2f UV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(Index, 0);
        auto Row = MakeShared<FJsonObject>();
        Row->SetArrayField(TEXT("local_cm"), Vec3(Local));
        Row->SetArrayField(TEXT("world_cm"), Vec3(World));
        Row->SetArrayField(TEXT("uv"), Vec2(FVector2D(UV)));
        Vertices.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetArrayField(TEXT("vertices"), Vertices);
    TArray<TSharedPtr<FJsonValue>> Triangles;
    for (int32 I = 0; I + 2 < LOD.IndexBuffer.GetNumIndices(); I += 3)
        Triangles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I)),
            MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I + 1)),
            MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I + 2)) }));
    Out->SetArrayField(TEXT("triangles"), Triangles);
    return true;
}

bool URokAcademyPresentationAuditSubsystem::Capture(const TCHAR* Name,
    const FLinearColor& SourceLighting, const FLinearColor& SourceClear)
{
    USceneCaptureComponent2D* SC = CaptureComponent.Get();
    UTextureRenderTarget2D* RT = RenderTarget.Get();
    UMaterialInstanceDynamic* MID = AuditMaterial.Get();
    UMaterialInstanceDynamic* BackgroundMID = BackgroundMaterial.Get();
    UStaticMeshComponent* Background = BackgroundComponent.Get();
    UStaticMeshComponent* Component = TemporaryActor.IsValid() ? TemporaryActor->GetSpriteMeshComponent() : nullptr;
    if (!SC || !RT || !MID || !BackgroundMID || !Background || !Component ||
        !Background->IsRegistered() || !Background->IsVisible() ||
        !Component->IsRegistered() || !Component->IsVisible() ||
        Background->GetMaterial(0) != BackgroundMID ||
        SC->ShowOnlyComponents.Num() != 2 || SC->ShowOnlyComponents[0].Get() != Component ||
        SC->ShowOnlyComponents[1].Get() != Background ||
        !SC->ShowFlags.PostProcessing || SC->ShowFlags.Tonemapper ||
        RT->IsSRGB() || FMath::Abs(RT->GetDisplayGamma() - 2.2f) > .001f ||
        RT->SizeX != ImageWidth || RT->SizeY != ImageHeight)
    { Finish(TEXT("Academy source-camera RHI capture objects disappeared")); return false; }
    MID->SetVectorParameterValue(TEXT("SpriteColor"), SourceLighting);
    MID->SetScalarParameterValue(TEXT("CityTransparency"), 1.0f);
    const FLinearColor ClearLinear = GammaColorToLinear(SourceClear);
    BackgroundMID->SetVectorParameterValue(TEXT("BackgroundColor"), ClearLinear);
    RT->ClearColor = ClearLinear;
    RT->UpdateResourceImmediate(true);
    SC->CaptureScene();
    FTextureRenderTargetResource* Resource = RT->GameThread_GetRenderTargetResource();
    TArray<FColor> Pixels;
    FReadSurfaceDataFlags Flags(RCM_UNorm);
    if (!Resource || !Resource->ReadPixels(Pixels, Flags) || Pixels.Num() != ImageWidth * ImageHeight)
    { Finish(TEXT("Academy actual RHI render-target readback failed")); return false; }
    const FString Path = RunDirectory / FString::Printf(TEXT("%02d_%s.png"), Stage, Name);
    const FImageView View(Pixels.GetData(), ImageWidth, ImageHeight);
    if (!FImageUtils::SaveImageByExtension(*Path, View))
    { Finish(TEXT("Academy actual RHI pixels could not be saved to PNG")); return false; }
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Pixels.Num() * sizeof(FColor));
    FMemory::Memcpy(Bytes.GetData(), Pixels.GetData(), Bytes.Num());
    const FString RawPath = FPaths::ChangeExtension(Path, TEXT("bgra8"));
    if (!FFileHelper::SaveArrayToFile(Bytes, *RawPath))
    { Finish(TEXT("Academy raw RHI FColor bytes could not be saved")); return false; }
    auto Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("name"), Name);
    Frame->SetStringField(TEXT("png"), Path);
    Frame->SetStringField(TEXT("png_sha256"), ShaFile(Path));
    Frame->SetStringField(TEXT("raw_bgra8"), RawPath);
    Frame->SetStringField(TEXT("raw_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Frame->SetStringField(TEXT("capture_api"), TEXT("USceneCaptureComponent2D CaptureScene -> FTextureRenderTargetResource ReadPixels RHI -> FColor BGRA8 + PNG"));
    Frame->SetStringField(TEXT("capture_source"), TEXT("SCS_FinalColorLDR with postprocess GammaOnly; no Tonemapper/AA/EyeAdaptation/MotionBlur"));
    Frame->SetStringField(TEXT("background_method"), TEXT("GPU unlit opaque plane, ShowOnly with Academy SpriteMesh; no CPU compositing"));
    Frame->SetStringField(TEXT("background_material"), BackgroundMaterialPath);
    Frame->SetArrayField(TEXT("background_material_linear_color"), Color4(ClearLinear));
    Frame->SetArrayField(TEXT("background_plane_world_location_cm"), Vec3(Background->GetComponentLocation()));
    Frame->SetArrayField(TEXT("background_plane_world_scale"), Vec3(Background->GetComponentScale()));
    Frame->SetNumberField(TEXT("show_only_component_count"), SC->ShowOnlyComponents.Num());
    Frame->SetBoolField(TEXT("post_processing_enabled"), SC->ShowFlags.PostProcessing);
    Frame->SetBoolField(TEXT("tonemapper_enabled"), SC->ShowFlags.Tonemapper);
    Frame->SetBoolField(TEXT("source_mesh_registered_visible"), Component->IsRegistered() && Component->IsVisible());
    Frame->SetBoolField(TEXT("background_plane_registered_visible"), Background->IsRegistered() && Background->IsVisible());
    Frame->SetArrayField(TEXT("rhi_corner_bgra8"), { MakeShared<FJsonValueNumber>(Pixels[0].B),
        MakeShared<FJsonValueNumber>(Pixels[0].G), MakeShared<FJsonValueNumber>(Pixels[0].R),
        MakeShared<FJsonValueNumber>(Pixels[0].A) });
    Frame->SetNumberField(TEXT("render_target_display_gamma"), RT->GetDisplayGamma());
    Frame->SetBoolField(TEXT("render_target_srgb"), RT->IsSRGB());
    Frame->SetBoolField(TEXT("render_target_force_linear_gamma"), RT->bForceLinearGamma);
    Frame->SetArrayField(TEXT("source_sprite_color_gamma"), Color4(SourceLighting));
    Frame->SetArrayField(TEXT("source_clear_color_gamma"), Color4(SourceClear));
    Frame->SetArrayField(TEXT("capture_clear_color_linear"), Color4(RT->ClearColor));
    Frame->SetArrayField(TEXT("camera_position_cm"), Vec3(SC->GetComponentLocation()));
    Frame->SetArrayField(TEXT("camera_forward_ue"), Vec3(SC->GetForwardVector()));
    Frame->SetArrayField(TEXT("camera_right_ue"), Vec3(SC->GetRightVector()));
    Frame->SetArrayField(TEXT("camera_up_ue"), Vec3(SC->GetUpVector()));
    Frame->SetNumberField(TEXT("camera_horizontal_fov_degrees"), SC->FOVAngle);
    Frame->SetNumberField(TEXT("camera_vertical_fov_degrees"), SourceVerticalFovDegrees);
    Frame->SetArrayField(TEXT("extent"), { MakeShared<FJsonValueNumber>(ImageWidth), MakeShared<FJsonValueNumber>(ImageHeight) });
    Frame->SetNumberField(TEXT("world_actor_count_at_capture"), CountActors(GetWorld()));
    Frame->SetNumberField(TEXT("temporary_actor_count_at_capture"), CountTemporaryActors(GetWorld()));
    TArray<TSharedPtr<FJsonValue>> Projection;
    const TArray<TSharedPtr<FJsonValue>>& Vertices = MeshRecord->GetArrayField(TEXT("vertices"));
    const FVector CamPos = SC->GetComponentLocation();
    const FVector CamForward = SC->GetForwardVector(), CamRight = SC->GetRightVector(), CamUp = SC->GetUpVector();
    const double TanV = FMath::Tan(FMath::DegreesToRadians(SourceVerticalFovDegrees) / 2.0);
    const double TanH = TanV * static_cast<double>(ImageWidth) / ImageHeight;
    for (const TSharedPtr<FJsonValue>& Value : Vertices)
    {
        const TArray<TSharedPtr<FJsonValue>>& W = Value->AsObject()->GetArrayField(TEXT("world_cm"));
        const FVector Relative(W[0]->AsNumber() - CamPos.X, W[1]->AsNumber() - CamPos.Y, W[2]->AsNumber() - CamPos.Z);
        const double Depth = FVector::DotProduct(Relative, CamForward);
        if (Depth <= 0) { Finish(TEXT("Academy source mesh vertex behind source camera")); return false; }
        const double X = (1.0 + FVector::DotProduct(Relative, CamRight) / (Depth * TanH)) * .5 * ImageWidth;
        const double Y = (1.0 - FVector::DotProduct(Relative, CamUp) / (Depth * TanV)) * .5 * ImageHeight;
        Projection.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y), MakeShared<FJsonValueNumber>(Depth) }));
    }
    Frame->SetArrayField(TEXT("projected_mesh_vertices_xy_top_left_depth_cm"), Projection);
    Frames.Add(Frame);
    return true;
}

bool URokAcademyPresentationAuditSubsystem::CaptureGameAppContext()
{
    ContextRecord = MakeShared<FJsonObject>();
    ContextRecord->SetStringField(TEXT("scope"), TEXT("Temporary age-1 source-grid preview in actual GameApp city; not a construction, initial-state building, or native full-city pixel comparison"));
    ContextRecord->SetBoolField(TEXT("constructed_building"), false);
    ContextRecord->SetBoolField(TEXT("map_saved"), false);
    UWorld* World = GetWorld();
    APlayerController* PC = PlayerController.Get();
    ARokCitySpriteActor* Actor = TemporaryActor.Get();
    USceneCaptureComponent2D* SC = CaptureComponent.Get();
    UTextureRenderTarget2D* RT = RenderTarget.Get();
    auto* Upgrade = World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
    if (!World || !PC || !Actor || !SC || !RT || !Upgrade)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_missing_runtime_context")); return true; }
    if (Upgrade->GetActiveLocalConstructionCount() != 0)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_active_local_construction_reservation")); return true; }

    const FString MapPath = FPaths::ProjectContentDir() / TEXT("Maps/L_RokUnityGameAppCity.umap");
    const FString PosePath = SavePath(TEXT("RokUnityMigration/BatchReview20261009/root_city_pose_crosscheck.json"));
    const FString ReadbackPath = SavePath(TEXT("RokUnityMigration/BatchReview20261009/current_city_pose_readback.json"));
    const FString ModelsPath = SavePath(TEXT("RokUnityMigration/CityConstructionSources/catalog.json"));
    TSharedPtr<FJsonObject> Pose, Models;
    const TArray<TSharedPtr<FJsonValue>>* PoseActors = nullptr;
    if (ShaFile(MapPath) != SourceMapSha || !ReadJson(PosePath, Pose) || !ReadJson(ModelsPath, Models) ||
        !Pose->GetBoolField(TEXT("passed")) ||
        Pose->GetStringField(TEXT("schema")) != TEXT("rok.current_city_anchor_source_grid_crosscheck.v1") ||
        Pose->GetStringField(TEXT("source_readback_sha256")) != ShaFile(ReadbackPath) ||
        Pose->GetStringField(TEXT("source_grid_catalog_sha256")) !=
            ShaFile(SavePath(TEXT("RokUnityMigration/CityConstructionGridSources/catalog.json"))) ||
        !Pose->TryGetArrayField(TEXT("actors"), PoseActors) || !PoseActors || PoseActors->Num() != 24)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_map_pose_source_chain")); return true; }
    for (const TSharedPtr<FJsonValue>& Value : *PoseActors)
        if (!Value.IsValid() || !Value->AsObject().IsValid() ||
            Value->AsObject()->GetNumberField(TEXT("error")) > 1.e-8)
        { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_map_pose_actor_error")); return true; }
    const TArray<TSharedPtr<FJsonValue>>* ModelRows = nullptr;
    bool bAgeOneAcademyModel = false;
    if (Models->TryGetArrayField(TEXT("models"), ModelRows) && ModelRows)
        for (const TSharedPtr<FJsonValue>& Value : *ModelRows)
        {
            const TSharedPtr<FJsonObject> Row = Value->AsObject();
            if (Row && Row->GetIntegerField(TEXT("type")) == 13 &&
                Row->GetIntegerField(TEXT("civilization")) == 101 && Row->GetIntegerField(TEXT("age")) == 1 &&
                Row->GetStringField(TEXT("model_id")) == TEXT("build_13_101_2")) bAgeOneAcademyModel = true;
        }
    if (!bAgeOneAcademyModel)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_age_one_model_source_key")); return true; }
    FRokCityConstructionGrid Grid;
    FString GridError;
    if (!Grid.Load(GridError))
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_source_grid_invalid")); ContextRecord->SetStringField(TEXT("grid_error"), GridError); return true; }
    FRokCityConstructionFootprint Footprint;
    Footprint.Size = FIntPoint(6, 6);
    TArray<int32> SourceCells;
    bool bFound = false;
    int32 RadiusFound = -1;
    for (int32 Radius = 0; Radius <= 20 && !bFound; ++Radius)
        for (int32 DX = -Radius; DX <= Radius && !bFound; ++DX)
            for (int32 DY = -Radius; DY <= Radius && !bFound; ++DY)
            {
                if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
                Footprint.Origin = FIntPoint(DX - 3, DY - 3);
                if (Grid.CheckFootprint(1, Footprint, TConstArrayView<FRokCityConstructionFootprint>(), GridError, &SourceCells))
                { bFound = true; RadiusFound = Radius; }
            }
    if (!bFound)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_no_age_one_six_by_six_source_free_grid")); return true; }
    if (Footprint.Origin != FIntPoint(-2, -7) || RadiusFound != 4 || SourceCells.Num() != 36)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_grid_candidate_differs_from_pinned_deterministic_first_free")); return true; }

    const double LocalX = .1 * Footprint.Origin.X + .05 * Footprint.Size.X - .05;
    const double LocalZ = .1 * Footprint.Origin.Y + .05 * Footprint.Size.Y - .05;
    const double Cos45 = FMath::Sqrt(.5);
    const double WorldUnityX = Cos45 * LocalX - Cos45 * LocalZ;
    const double WorldUnityZ = Cos45 * LocalX + Cos45 * LocalZ;
    const FVector Anchor(-WorldUnityX * 500., WorldUnityZ * 500., 5.);
    Actor->SetActorLocation(Anchor);
    const AActor* ViewTarget = PC->GetViewTarget();
    const ARokStrategyCameraPawn* CityCamera = Cast<ARokStrategyCameraPawn>(ViewTarget);
    if (!CityCamera || !PC->PlayerCameraManager)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_actual_city_camera_missing")); return true; }
    const float CityDxf = CityCamera->GetCurrentCityDxf();
    Actor->ConfigureUnityCameraLod(FVector::ZeroVector, CityDxf);
    UStaticMeshComponent* Background = BackgroundComponent.Get();
    if (!Background)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_transient_source_camera_background_missing")); return true; }
    Background->SetHiddenInGame(true);
    ContextRecord->SetBoolField(TEXT("source_camera_background_hidden_for_city"), true);
    SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    SC->ShowOnlyComponents.Reset();
    SC->SetWorldLocation(PC->PlayerCameraManager->GetCameraLocation());
    SC->SetWorldRotation(PC->PlayerCameraManager->GetCameraRotation());
    SC->FOVAngle = PC->PlayerCameraManager->GetFOVAngle();
    SC->ShowFlags.SetPostProcessing(true);
    SC->ShowFlags.SetTonemapper(true);
    SC->ShowFlags.SetAtmosphere(true);
    SC->ShowFlags.SetDeferredAtmospherePass(true);
    SC->ShowFlags.SetFog(true);
    SC->ShowFlags.SetVolumetricFog(true);
    SC->ShowFlags.SetCloud(true);
    RT->ClearColor = FLinearColor::Black;
    RT->UpdateResourceImmediate(true);
    SC->CaptureScene();
    FTextureRenderTargetResource* Resource = RT->GameThread_GetRenderTargetResource();
    TArray<FColor> Pixels;
    if (!Resource || !Resource->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm)) ||
        Pixels.Num() != ImageWidth * ImageHeight)
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_context_rhi_readback_failed")); return true; }
    const FString Path = RunDirectory / TEXT("02_gameapp_temporary_age1_grid.png");
    if (!FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), ImageWidth, ImageHeight)))
    { ContextRecord->SetStringField(TEXT("status"), TEXT("pending_context_png_save_failed")); return true; }
    ContextRecord->SetStringField(TEXT("status"), TEXT("captured_temporary_source_age_one_grid"));
    ContextRecord->SetStringField(TEXT("png"), Path);
    ContextRecord->SetStringField(TEXT("png_sha256"), ShaFile(Path));
    ContextRecord->SetStringField(TEXT("map_sha256"), SourceMapSha);
    ContextRecord->SetStringField(TEXT("pose_crosscheck_sha256"), ShaFile(PosePath));
    ContextRecord->SetStringField(TEXT("pose_readback_sha256"), ShaFile(ReadbackPath));
    ContextRecord->SetStringField(TEXT("construction_models_sha256"), ShaFile(ModelsPath));
    ContextRecord->SetStringField(TEXT("grid_catalog_sha256"), ShaFile(SavePath(TEXT("RokUnityMigration/CityConstructionGridSources/catalog.json"))));
    ContextRecord->SetNumberField(TEXT("age"), 1);
    ContextRecord->SetNumberField(TEXT("model_key"), 131011);
    ContextRecord->SetNumberField(TEXT("first_free_radius"), RadiusFound);
    ContextRecord->SetArrayField(TEXT("footprint_origin"), { MakeShared<FJsonValueNumber>(Footprint.Origin.X), MakeShared<FJsonValueNumber>(Footprint.Origin.Y) });
    ContextRecord->SetArrayField(TEXT("footprint_size"), { MakeShared<FJsonValueNumber>(6), MakeShared<FJsonValueNumber>(6) });
    ContextRecord->SetArrayField(TEXT("unity_grid_center_local_xzy"), { MakeShared<FJsonValueNumber>(LocalX), MakeShared<FJsonValueNumber>(.01), MakeShared<FJsonValueNumber>(LocalZ) });
    ContextRecord->SetArrayField(TEXT("actor_world_anchor_cm"), Vec3(Anchor));
    ContextRecord->SetArrayField(TEXT("actual_city_camera_position_cm"), Vec3(SC->GetComponentLocation()));
    ContextRecord->SetArrayField(TEXT("actual_city_camera_forward"), Vec3(SC->GetForwardVector()));
    ContextRecord->SetNumberField(TEXT("actual_city_camera_horizontal_fov"), SC->FOVAngle);
    ContextRecord->SetNumberField(TEXT("city_dxf"), CityDxf);
    ContextRecord->SetNumberField(TEXT("source_cell_count"), SourceCells.Num());
    ContextRecord->SetNumberField(TEXT("active_construction_count"), Upgrade->GetActiveLocalConstructionCount());
    return true;
}

void URokAcademyPresentationAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished) return;
    Elapsed += FMath::Max(0.0f, DeltaTime);
    if (Stage < 0) { if (Elapsed >= 1.0) Begin(); return; }
    if (Elapsed - StageStart < .5) return;
    static const FLinearColor Lighting[] = { FLinearColor::White,
        FLinearColor(1.0720397233963013, 1.0720397233963013, 1.0720397233963013, 1) };
    static const FLinearColor Clear[] = { FLinearColor(.5254999995231628, .6313999891281128, .3215999901294708, 1),
        FLinearColor(.563356876373291, .6768858432769775, .3447679579257965, 1) };
    static const TCHAR* Names[] = { TEXT("neutral"), TEXT("day") };
    if (Stage == 2) { CaptureGameAppContext(); Finish(); return; }
    if (Stage < 0 || Stage >= 2) { Finish(TEXT("Academy capture stage out of range")); return; }
    if (!Capture(Names[Stage], Lighting[Stage], Clear[Stage])) return;
    ++Stage;
    StageStart = Elapsed;
}

bool URokAcademyPresentationAuditSubsystem::Restore()
{
    if (bRestored) return true;
    bool Okay = true;
    if (USceneCaptureComponent2D* SC = CaptureComponent.Get())
    {
        SC->TextureTarget = nullptr;
        SC->UnregisterComponent();
    }
    CaptureComponent.Reset(); RenderTarget.Reset(); AuditMaterial.Reset(); BackgroundMaterial.Reset();
    BackgroundComponent.Reset(); SourceMesh.Reset();
    if (ARokCitySpriteActor* Actor = TemporaryActor.Get())
    {
        Actor->Destroy();
        TemporaryActor.Reset();
    }
    UWorld* World = GetWorld();
    if (!World || CountTemporaryActors(World) != InitialTemporaryActorCount) Okay = false;
    APlayerController* PC = PlayerController.Get();
    if (!PC || PC->GetViewTarget() != SavedViewTarget.Get()) Okay = false;
    if (PC)
    {
        UCameraComponent* ExistingCamera = PC->GetViewTarget()
            ? PC->GetViewTarget()->FindComponentByClass<UCameraComponent>() : nullptr;
        if (ExistingCamera && (FVector::Dist(ExistingCamera->GetComponentLocation(), SavedCameraTransform.GetLocation()) > .01 ||
            FMath::Abs(ExistingCamera->FieldOfView - SavedCameraFov) > .001)) Okay = false;
    }
    if (World && EconomyFingerprint(World) != EconomicSnapshot) Okay = false;
    bRestored = Okay;
    return Okay;
}

void URokAcademyPresentationAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    bFinished = true;
    if (!Error.IsEmpty()) Failures.Add(Error);
    const bool Restored = Restore();
    if (!Restored) Failures.Add(TEXT("Academy temporary actor/camera/capture state restoration failed"));
    auto InputHashes = MakeShared<FJsonObject>();
    const TCHAR* InputFiles[] = {
        TEXT("Source/Rok/RokAcademyPresentationAuditSubsystem.h"),
        TEXT("Source/Rok/RokAcademyPresentationAuditSubsystem.cpp"),
        TEXT("Source/Rok/RokCitySpriteActor.h"),
        TEXT("Source/Rok/RokCitySpriteActor.cpp"),
        TEXT("Scripts/VerifyRokAcademyPresentationRuntime.py"),
        TEXT("Scripts/PrepareRokAcademySourceMesh.py"),
        TEXT("Scripts/CreateRokAcademyPresentationMesh.py") };
    for (const TCHAR* Relative : InputFiles)
    {
        const FString Hash = ShaFile(FPaths::ProjectDir() / Relative);
        if (Hash.IsEmpty()) Failures.Add(FString::Printf(TEXT("Academy capture input missing: %s"), Relative));
        InputHashes->SetStringField(Relative, Hash);
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("rok.academy_presentation_runtime.v1"));
    Report->SetStringField(TEXT("scope"), TEXT("GameApp temporary source Academy SpriteMesh; isolated 1280x720 neutral/day UE SceneCapture RHI readback and age-one source-grid context diagnostic, not a constructed building or full-city pixel acceptance."));
    Report->SetBoolField(TEXT("complete"), Frames.Num() == 2 && Failures.IsEmpty());
    Report->SetBoolField(TEXT("passed"), Frames.Num() == 2 && Failures.IsEmpty());
    Report->SetBoolField(TEXT("temporary_state_restored"), Restored);
    Report->SetBoolField(TEXT("map_saved"), false);
    Report->SetBoolField(TEXT("construction_ledger_touched"), false);
    Report->SetBoolField(TEXT("source_gameapp_context_accepted"), false);
    Report->SetStringField(TEXT("source_gameapp_context_status"), ContextRecord.IsValid()
        ? ContextRecord->GetStringField(TEXT("status")) : TEXT("pending_context_not_attempted"));
    Report->SetStringField(TEXT("economy_before"), EconomicSnapshot);
    Report->SetStringField(TEXT("economy_after"), EconomyFingerprint(GetWorld()));
    auto Budget = MakeShared<FJsonObject>();
    Budget->SetNumberField(TEXT("uv_absolute"), .00001);
    Budget->SetNumberField(TEXT("local_vertex_cm"), .05);
    Budget->SetNumberField(TEXT("projection_median_px"), 1);
    Budget->SetNumberField(TEXT("projection_max_px"), 2);
    Budget->SetNumberField(TEXT("bbox_max_edge_px"), 2);
    Budget->SetNumberField(TEXT("mask_iou_min"), .98);
    Budget->SetNumberField(TEXT("opaque_core_mae_rgb8"), 8);
    Budget->SetNumberField(TEXT("opaque_core_p95_rgb8"), 16);
    Budget->SetNumberField(TEXT("semi_analytic_mae_rgb8"), 12);
    Budget->SetNumberField(TEXT("semi_analytic_p95_rgb8"), 24);
    Budget->SetNumberField(TEXT("background_max_rgb8"), 2);
    Report->SetObjectField(TEXT("fixed_pre_capture_budget"), Budget);
    Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
    Report->SetStringField(TEXT("source_catalog_sha256"), SourceCatalogSha);
    Report->SetStringField(TEXT("native_render_sha256"), NativeRenderSha);
    Report->SetStringField(TEXT("native_verify_sha256"), NativeVerifySha);
    Report->SetStringField(TEXT("mesh_manifest_sha256"), MeshManifestSha);
    Report->SetStringField(TEXT("mesh_asset_sha256"), MeshAssetSha);
    Report->SetStringField(TEXT("material_asset_sha256"), MaterialAssetSha);
    Report->SetStringField(TEXT("background_material_asset_sha256"), BackgroundMaterialAssetSha);
    Report->SetObjectField(TEXT("input_hashes"), InputHashes);
    Report->SetNumberField(TEXT("actor_count_before"), InitialActorCount);
    Report->SetNumberField(TEXT("actor_count_after_restore"), GetWorld() ? CountActors(GetWorld()) : -1);
    Report->SetNumberField(TEXT("temporary_actor_count_before"), InitialTemporaryActorCount);
    Report->SetNumberField(TEXT("temporary_actor_count_after_restore"), GetWorld() ? CountTemporaryActors(GetWorld()) : -1);
    if (MeshRecord.IsValid()) Report->SetObjectField(TEXT("mesh"), MeshRecord);
    if (ContextRecord.IsValid()) Report->SetObjectField(TEXT("gameapp_temporary_context"), ContextRecord);
    TArray<TSharedPtr<FJsonValue>> Captures;
    for (const auto& Frame : Frames) Captures.Add(MakeShared<FJsonValueObject>(Frame));
    Report->SetArrayField(TEXT("frames"), Captures);
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Failure : Failures) Errors.Add(MakeShared<FJsonValueString>(Failure));
    Report->SetArrayField(TEXT("failures"), Errors);
    FString Text;
    const auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text);
    FJsonSerializer::Serialize(Report, Writer);
    FFileHelper::SaveStringToFile(Text, *(RunDirectory / TEXT("manifest.json")));
    UE_LOG(LogTemp, Display, TEXT("Rok Academy presentation audit passed=%d frames=%d manifest=%s"),
        Failures.IsEmpty() && Frames.Num() == 2, Frames.Num(), *(RunDirectory / TEXT("manifest.json")));
    FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() && Frames.Num() == 2 ? 0 : 1);
}
