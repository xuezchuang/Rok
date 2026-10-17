#include "RokMountainFLinearAuditSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 Width = 1440;
constexpr int32 Height = 900; // The source pawn constrains its camera to 16:10.
const TCHAR* WorldPath = TEXT("/Game/Maps/L_RokUnityWorldMapPrototype");
const TCHAR* MaterialPath = TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F");
const TCHAR* MeshPath = TEXT("/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/SM_RokWorldBody_build_mountain_F_build_mountain_F.SM_RokWorldBody_build_mountain_F_build_mountain_F");
const TCHAR* TexturePath = TEXT("/Game/RokUnityWorldMapPrototype/Textures/T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips");
FString Input(const TCHAR* Relative) { return FPaths::ProjectSavedDir() / Relative; }
bool LoadJson(const FString& Path, TSharedPtr<FJsonObject>& Result)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result) && Result.IsValid();
}
bool SaveJson(const FString& Path, const TSharedPtr<FJsonObject>& Value)
{
    FString Text;
    return FJsonSerializer::Serialize(Value.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)) &&
        FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
FString ShaFile(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
TArray<TSharedPtr<FJsonValue>> Array3(const FVector& Value)
{
    return { MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z) };
}
FString SourceKey(const AActor* Actor)
{
    FString Tile, Index;
    for (FName Tag : Actor->Tags)
    {
        const FString Value = Tag.ToString();
        if (Value.StartsWith(TEXT("UnityTile_"))) Tile = Value;
        if (Value.StartsWith(TEXT("UnityObjectIndex_"))) Index = Value;
    }
    return Tile.IsEmpty() || Index.IsEmpty() ? FString() : Tile + TEXT("/") + Index;
}
}

bool URokMountainFLinearAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->GetOutermost()->GetName() == WorldPath &&
        FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")) &&
        FParse::Param(FCommandLine::Get(), TEXT("RokMountainFLinearAudit")) &&
        !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMountainFLinearAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokMountainFLinearAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(Input(TEXT("RokUnityMigration/MountainFLinearAudit")) /
        FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
}
void URokMountainFLinearAuditSubsystem::Deinitialize()
{
    Restore();
    Super::Deinitialize();
}
TStatId URokMountainFLinearAuditSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URokMountainFLinearAuditSubsystem, STATGROUP_Tickables);
}

void URokMountainFLinearAuditSubsystem::Begin()
{
    if (GetWorld()->GetOutermost()->GetName() != WorldPath ||
        !FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")))
    { Finish(TEXT("Wrong project or world")); return; }
    TArray<FString> Tokens; FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
    for (const FString& Token : Tokens)
        if (Token.StartsWith(TEXT("-Rok")) && Token != TEXT("-RokMountainFLinearAudit"))
        { Finish(TEXT("Conflicting Rok runtime audit flag")); return; }

    const FString SourcePath = Input(TEXT("RokUnityMigration/MountainFGamma/source_data.json"));
    const FString ProductPath = Input(TEXT("RokUnityMigration/MountainFGammaProduct/prepare_manifest.json"));
    TSharedPtr<FJsonObject> Source, Product;
    if (!LoadJson(SourcePath, Source) || !LoadJson(ProductPath, Product) ||
        Source->GetStringField(TEXT("schema")) != TEXT("rok.mountain_f_gamma.v1") ||
        Product->GetStringField(TEXT("schema")) != TEXT("rok.mountain_f_gamma_product.prepare.v1") ||
        Product->GetIntegerField(TEXT("mip_count")) != 11)
    { Finish(TEXT("Unverified F source/product input")); return; }
    SourceSha = ShaFile(SourcePath); ProductSha = ShaFile(ProductPath);
    const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
    if (!Source->TryGetArrayField(TEXT("samples"), Samples) || Samples->Num() != 3)
    { Finish(TEXT("Expected three source-native F colors")); return; }
    const TCHAR* ExpectedNames[] = { TEXT("stable_day"), TEXT("stable_dusk"), TEXT("stable_night") };
    for (int32 I = 0; I < 3; ++I)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Color = nullptr;
        double R = 0, G = 0, B = 0;
        if (!(*Samples)[I]->TryGetObject(Row) || !Row || !Row->IsValid() ||
            (*Row)->GetStringField(TEXT("name")) != ExpectedNames[I] ||
            !(*Row)->TryGetArrayField(TEXT("spriteColor"), Color) || Color->Num() != 3 ||
            !(*Color)[0]->TryGetNumber(R) || !(*Color)[1]->TryGetNumber(G) || !(*Color)[2]->TryGetNumber(B) ||
            !FMath::IsFinite(R) || !FMath::IsFinite(G) || !FMath::IsFinite(B) ||
            R <= 0 || G <= 0 || B <= 0)
        { Finish(TEXT("Invalid source-native F sample color")); return; }
        Names.Add(ExpectedNames[I]); Colors.Add(FLinearColor(R, G, B, 1));
    }

    UTexture2D* FTexture = LoadObject<UTexture2D>(nullptr, TexturePath);
    if (!FTexture || FTexture->GetPathName() != TexturePath || FTexture->GetSizeX() != 1024 ||
        FTexture->GetSizeY() != 1024 || FTexture->GetNumMips() != 11 || FTexture->SRGB ||
        FTexture->GetPixelFormat() != PF_B8G8R8A8 || FTexture->Filter != TF_Bilinear || !FTexture->NeverStream)
    { Finish(TEXT("F decoded texture format or sampling state changed")); return; }
    Texture.Reset(FTexture);
    auto* Platform = FTexture->GetPlatformData();
    const TArray<TSharedPtr<FJsonValue>>* ProductMips = nullptr;
    if (!Platform || Platform->PixelFormat != PF_B8G8R8A8 || Platform->Mips.Num() != 11 ||
        !Product->TryGetArrayField(TEXT("mips"), ProductMips) || ProductMips->Num() != 11)
    { Finish(TEXT("F platform mip chain unavailable")); return; }
    // Recheck the actual loaded mip bytes, not just product manifest metadata.
    void* MipData[11] = {};
    int64 MipSizes[11] = {};
    const bool bLoaded = FTexture->GetInitialMipData(0, MakeArrayView(MipData), MakeArrayView(MipSizes));
    bool bMipsMatch = bLoaded;
    for (int32 I = 0; I < 11; ++I)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        const int32 Edge = FMath::Max(1, 1024 >> I);
        const int64 Size = int64(Edge) * Edge * 4;
        if (!(*ProductMips)[I]->TryGetObject(Row) || !Row || !Row->IsValid() ||
            Platform->Mips[I].SizeX != Edge || Platform->Mips[I].SizeY != Edge ||
            !MipData[I] || MipSizes[I] != Size)
            bMipsMatch = false;
        else
        {
            const FString Hash = RokTrainArmyMesh::Sha256(TConstArrayView<uint8>(static_cast<const uint8*>(MipData[I]), int32(Size)));
            bMipsMatch &= Hash == (*Row)->GetStringField(TEXT("bgra8_sha256"));
            PlatformMipShas.Add(Hash);
            if (I == 0) TextureMip0Sha = Hash;
        }
        if (MipData[I]) FMemory::Free(MipData[I]);
    }
    if (!bMipsMatch) { Finish(TEXT("Loaded platform F mips differ from source product")); return; }

    ARokWorldMapConfigActor* Config = nullptr;
    int32 CameraCount = 0, ConfigCount = 0, BodyCount = 0;
    for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++CameraCount; }
    for (TActorIterator<ARokWorldMapConfigActor> It(GetWorld()); It; ++It) { Config = *It; ++ConfigCount; }
    const FVector InitialFocus = Config ? Config->Projection.UnityToUe(Camera.IsValid() ? Camera->GetViewCenterUnity() : FVector2D::ZeroVector) : FVector::ZeroVector;
    double BestDistance = TNumericLimits<double>::Max();
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor->ActorHasTag(TEXT("UnityPrefab_build_mountain_F"))) continue;
        ++BodyCount;
        TArray<UStaticMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        if (!Actor->ActorHasTag(TEXT("RokWorldMapExactSource")) || SourceKey(Actor).IsEmpty() ||
            Meshes.Num() != 1 || !Meshes[0] || !Meshes[0]->GetStaticMesh() ||
            Meshes[0]->GetStaticMesh()->GetPathName() != MeshPath || !Meshes[0]->GetMaterial(0) ||
            !Meshes[0]->GetMaterial(0)->GetBaseMaterial() ||
            Meshes[0]->GetMaterial(0)->GetBaseMaterial()->GetPathName() != MaterialPath ||
            !Meshes[0]->IsVisible() || Meshes[0]->bHiddenInGame || Actor->IsHidden())
        { Finish(TEXT("F source body identity/material changed")); return; }
        const double Distance = FVector::DistSquared(Meshes[0]->Bounds.Origin, InitialFocus);
        if (Distance < BestDistance) { BestDistance = Distance; Body = Meshes[0]; }
    }
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (CameraCount != 1 || ConfigCount != 1 || BodyCount != 18 || !Body.IsValid() || !PC ||
        PC->GetPawn() != Camera.Get() || PC->GetViewTarget() != Camera.Get())
    { Finish(TEXT("F source camera or 18-body world route changed")); return; }

    UCameraComponent* SourceCamera = Camera->GetCameraComponent();
    if (!SourceCamera || SourceCamera->ProjectionMode != ECameraProjectionMode::Perspective ||
        !SourceCamera->bConstrainAspectRatio || FMath::Abs(SourceCamera->AspectRatio - double(Width) / Height) > .0001)
    { Finish(TEXT("F source camera is not constrained 16:10 perspective")); return; }
    SavedCenter = Camera->GetViewCenterUnity(); SavedDxf = Camera->GetCurrentDxf(); bCameraSaved = true;
    const FVector Forward = SourceCamera->GetForwardVector();
    if (FMath::Abs(Forward.Z) < .1) { Finish(TEXT("Unexpected F camera direction")); return; }
    const FVector Center = Body->Bounds.Origin;
    const FVector GroundFocus = Center - Forward * ((Center.Z - Config->Projection.GroundZ) / Forward.Z);
    Camera->SetViewCenterUnity(Config->Projection.UeToUnity(GroundFocus));
    Camera->SetCurrentDxf(1509);

    UStaticMeshComponent* Mesh = Body.Get();
    OriginalMaterial.Reset(Mesh->GetMaterial(0)); bMaterialSaved = true;
    TArray<UTexture*> Used; OriginalMaterial->GetUsedTextures(Used);
    if (!Used.Contains(FTexture)) { Finish(TEXT("F material does not use decoded texture")); return; }
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(OriginalMaterial.Get(), Mesh);
    if (!MID) { Finish(TEXT("Cannot create transient F color MID")); return; }
    AuditMaterial.Reset(MID); Mesh->SetMaterial(0, MID);
    SavedForcedLod = Mesh->GetForcedLodModel(); bLodSaved = true; Mesh->SetForcedLodModel(1);

    PreExposureCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride"));
    if (!PreExposureCvar) { Finish(TEXT("UE pre-exposure override CVar missing")); return; }
    SavedPreExposure = PreExposureCvar->GetFloat();
    PreExposureCvar->SetWithCurrentPriority(1.0f, NAME_None, ECVF_SetByConsole, ECVF_SetByConstructor);
    bPreExposureChanged = true;
    if (FMath::Abs(PreExposureCvar->GetFloat() - 1.0f) > .000001f)
    { Finish(TEXT("Unable to fix pre-exposure at one")); return; }

    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
    RT->InitCustomFormat(Width, Height, PF_FloatRGBA, true);
    RT->ClearColor = FLinearColor::Black;
    RT->UpdateResourceImmediate(true);
    Target.Reset(RT);
    USceneCaptureComponent2D* SC = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
    SC->TextureTarget = RT;
    SC->CaptureSource = SCS_SceneColorHDRNoAlpha;
    SC->CompositeMode = SCCM_Overwrite;
    SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SC->ShowOnlyComponents.Add(Mesh);
    SC->bCaptureEveryFrame = false;
    SC->bCaptureOnMovement = false;
    SC->bAlwaysPersistRenderingState = false;
    SC->FOVAngle = SourceCamera->FieldOfView;
    SC->ProjectionType = ECameraProjectionMode::Perspective;
    SC->PostProcessSettings = SourceCamera->PostProcessSettings;
    SC->PostProcessBlendWeight = SourceCamera->PostProcessBlendWeight;
    SC->RegisterComponentWithWorld(GetWorld());
    // OnRegister calls UpdateShowFlags and resets direct flag edits to the CDO.
    SC->ShowFlags.SetAntiAliasing(false);
    SC->ShowFlags.SetTemporalAA(false);
    SC->ShowFlags.SetEyeAdaptation(false);
    SC->ShowFlags.SetMotionBlur(false);
    Capture.Reset(SC);
    Stage = 0; StageFrame = GFrameCounter;
}

void URokMountainFLinearAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !GetWorld()->HasBegunPlay()) return;
    Elapsed += FMath::Max(DeltaTime, 0.f);
    if (Elapsed > 60) { Finish(TEXT("Linear F audit timed out")); return; }
    if (Stage < 0) { Begin(); return; }
    if (GFrameCounter < StageFrame + 4) return;
    CaptureStage();
}

void URokMountainFLinearAuditSubsystem::CaptureStage()
{
    auto* SC = Capture.Get(); auto* RT = Target.Get(); auto* Mesh = Body.Get(); auto* MID = AuditMaterial.Get();
    UCameraComponent* SourceCamera = Camera.IsValid() ? Camera->GetCameraComponent() : nullptr;
    if (!SC || !RT || !Mesh || !MID || !SourceCamera || Stage < 0 || Stage >= 3 ||
        Mesh->GetMaterial(0) != MID || Mesh->GetForcedLodModel() != 1 ||
        FMath::Abs(PreExposureCvar->GetFloat() - 1.0f) > .000001f)
    { Finish(TEXT("Transient F capture state changed")); return; }
    MID->SetVectorParameterValue(TEXT("UnitySpriteColor"), Colors[Stage]);
    SC->SetWorldLocationAndRotation(SourceCamera->GetComponentLocation(), SourceCamera->GetComponentRotation());
    SC->FOVAngle = SourceCamera->FieldOfView;
    if (Stage == 0)
    {
        const FStaticMeshRenderData* RenderData = Mesh->GetStaticMesh()->GetRenderData();
        if (!RenderData || RenderData->LODResources.IsEmpty()) { Finish(TEXT("Representative F LOD0 render data missing")); return; }
        const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
        const auto& Positions = Lod.VertexBuffers.PositionVertexBuffer;
        const auto& UVs = Lod.VertexBuffers.StaticMeshVertexBuffer;
        if (Positions.GetNumVertices() < 100 || UVs.GetNumTexCoords() < 1 || Lod.IndexBuffer.GetNumIndices() < 3000 ||
            Lod.IndexBuffer.GetNumIndices() % 3 != 0)
        { Finish(TEXT("Representative F LOD0 mesh/UV topology invalid")); return; }
        const FVector Origin = SC->GetComponentLocation();
        const FVector Forward = SC->GetForwardVector(), Right = SC->GetRightVector(), Up = SC->GetUpVector();
        const double Scale = Width / (2.0 * FMath::Tan(FMath::DegreesToRadians(double(SC->FOVAngle)) * .5));
        TArray<TSharedPtr<FJsonValue>> Vertices, Triangles;
        int32 ValidCount = 0;
        double MinX = Width, MinY = Height, MaxX = 0, MaxY = 0;
        Vertices.Reserve(Positions.GetNumVertices());
        Triangles.Reserve(Lod.IndexBuffer.GetNumIndices() / 3);
        for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
        {
            const FVector Relative = Mesh->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(I))) - Origin;
            const double Depth = FVector::DotProduct(Relative, Forward);
            const bool bValid = FMath::IsFinite(Depth) && Depth > 0;
            const double X = bValid ? Width * .5 + FVector::DotProduct(Relative, Right) * Scale / Depth : 0;
            const double Y = bValid ? Height * .5 - FVector::DotProduct(Relative, Up) * Scale / Depth : 0;
            const FVector2f UV = UVs.GetVertexUV(I, 0);
            if (bValid && FMath::IsFinite(X) && FMath::IsFinite(Y))
            {
                ++ValidCount; MinX = FMath::Min(MinX, X); MinY = FMath::Min(MinY, Y);
                MaxX = FMath::Max(MaxX, X); MaxY = FMath::Max(MaxY, Y);
            }
            Vertices.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                MakeShared<FJsonValueNumber>(X), MakeShared<FJsonValueNumber>(Y),
                MakeShared<FJsonValueNumber>(bValid ? Depth : 0),
                MakeShared<FJsonValueNumber>(UV.X), MakeShared<FJsonValueNumber>(UV.Y) }));
        }
        for (int32 I = 0; I < Lod.IndexBuffer.GetNumIndices(); I += 3)
        {
            const uint32 A = Lod.IndexBuffer.GetIndex(I), B = Lod.IndexBuffer.GetIndex(I + 1), C = Lod.IndexBuffer.GetIndex(I + 2);
            if (A >= Positions.GetNumVertices() || B >= Positions.GetNumVertices() || C >= Positions.GetNumVertices())
            { Finish(TEXT("Representative F triangle index out of range")); return; }
            Triangles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
                MakeShared<FJsonValueNumber>(A), MakeShared<FJsonValueNumber>(B), MakeShared<FJsonValueNumber>(C) }));
        }
        if (ValidCount < int32(Positions.GetNumVertices() * .9))
        { Finish(TEXT("Representative F has invalid source-lens projection")); return; }
        if (MinX < 10 || MinY < 10 || MaxX > Width - 10 || MaxY > Height - 10)
        {
            if (Camera->GetCurrentDxf() > 8000)
            { Finish(TEXT("Representative F is not fully framed in source lens")); return; }
            Camera->SetCurrentDxf(Camera->GetCurrentDxf() * 1.15f);
            StageFrame = GFrameCounter;
            return;
        }
        auto Geometry = MakeShared<FJsonObject>();
        Geometry->SetStringField(TEXT("schema"), TEXT("rok.mountain_f_linear_geometry.v1"));
        Geometry->SetStringField(TEXT("projection"), TEXT("perspective, horizontal FOV, pixel centers at x+0.5/y+0.5, top-left origin"));
        Geometry->SetArrayField(TEXT("size"), { MakeShared<FJsonValueNumber>(Width), MakeShared<FJsonValueNumber>(Height) });
        Geometry->SetArrayField(TEXT("camera_position"), Array3(Origin));
        Geometry->SetArrayField(TEXT("camera_forward"), Array3(Forward));
        Geometry->SetArrayField(TEXT("camera_right"), Array3(Right));
        Geometry->SetArrayField(TEXT("camera_up"), Array3(Up));
        Geometry->SetNumberField(TEXT("horizontal_fov"), SC->FOVAngle);
        Geometry->SetNumberField(TEXT("valid_vertices"), ValidCount);
        Geometry->SetArrayField(TEXT("vertices_xy_depth_uv"), Vertices);
        Geometry->SetArrayField(TEXT("triangles"), Triangles);
        GeometryPath = Directory / TEXT("geometry.json");
        if (!SaveJson(GeometryPath, Geometry)) { Finish(TEXT("Independent F geometry evidence save failed")); return; }
        GeometrySha = ShaFile(GeometryPath);
    }
    // This is a separate scene view but uses the live source pawn's exact pose and lens.
    SC->CaptureScene();
    FTextureRenderTargetResource* Resource = RT->GameThread_GetRenderTargetResource();
    TArray<FFloat16Color> Pixels;
    if (!Resource || !Resource->ReadFloat16Pixels(Pixels, FReadSurfaceDataFlags(RCM_MinMax)) || Pixels.Num() != Width * Height ||
        RT->GetFormat() != PF_FloatRGBA)
    { Finish(TEXT("PF_FloatRGBA scene color readback failed")); return; }
    static_assert(sizeof(FFloat16Color) == 8, "Raw RGBA16F layout changed");
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Pixels.Num() * sizeof(FFloat16Color));
    FMemory::Memcpy(Bytes.GetData(), Pixels.GetData(), Bytes.Num());
    const FString RawPath = Directory / FString::Printf(TEXT("%02d_%s.rgba16f"), Stage, *Names[Stage]);
    if (!FFileHelper::SaveArrayToFile(Bytes, *RawPath)) { Finish(TEXT("Raw RGBA16F save failed")); return; }

    auto Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("name"), Names[Stage]);
    Frame->SetStringField(TEXT("path"), RawPath);
    Frame->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Frame->SetNumberField(TEXT("bytes"), Bytes.Num());
    Frame->SetNumberField(TEXT("frame_counter"), double(GFrameCounter));
    Frame->SetArrayField(TEXT("source_color"), Array3(FVector(Colors[Stage].R, Colors[Stage].G, Colors[Stage].B)));
    Frame->SetArrayField(TEXT("camera_position"), Array3(SourceCamera->GetComponentLocation()));
    const FRotator Rotation = SourceCamera->GetComponentRotation();
    Frame->SetArrayField(TEXT("camera_rotation_pyr"), Array3(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
    Frame->SetNumberField(TEXT("camera_horizontal_fov"), SourceCamera->FieldOfView);
    Frame->SetNumberField(TEXT("camera_aspect"), SourceCamera->AspectRatio);
    Frame->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf());
    Frame->SetNumberField(TEXT("pre_exposure_override"), PreExposureCvar->GetFloat());
    Frames.Add(Frame);
    if (++Stage == 3) { Finish(); return; }
    StageFrame = GFrameCounter;
}

bool URokMountainFLinearAuditSubsystem::Restore()
{
    bool Okay = true;
    if (auto* SC = Capture.Get()) SC->DestroyComponent();
    Capture.Reset(); Target.Reset();
    if (bPreExposureChanged && PreExposureCvar)
    {
        PreExposureCvar->SetWithCurrentPriority(SavedPreExposure, NAME_None, ECVF_SetByConsole, ECVF_SetByConstructor);
        Okay &= FMath::Abs(PreExposureCvar->GetFloat() - SavedPreExposure) < .000001f;
        bPreExposureChanged = false;
    }
    if (bLodSaved)
    {
        if (auto* Mesh = Body.Get()) { Mesh->SetForcedLodModel(SavedForcedLod); Okay &= Mesh->GetForcedLodModel() == SavedForcedLod; }
        else Okay = false;
        bLodSaved = false;
    }
    if (bMaterialSaved)
    {
        if (auto* Mesh = Body.Get()) { Mesh->SetMaterial(0, OriginalMaterial.Get()); Okay &= Mesh->GetMaterial(0) == OriginalMaterial.Get(); }
        else Okay = false;
        bMaterialSaved = false;
    }
    if (bCameraSaved)
    {
        if (auto* Pawn = Camera.Get())
        {
            Pawn->SetViewCenterUnity(SavedCenter); Pawn->SetCurrentDxf(SavedDxf);
            Okay &= FVector2D::Distance(Pawn->GetViewCenterUnity(), SavedCenter) < .01 && FMath::Abs(Pawn->GetCurrentDxf() - SavedDxf) < .01;
        }
        else Okay = false;
        bCameraSaved = false;
    }
    return Okay;
}

void URokMountainFLinearAuditSubsystem::Finish(const FString& Error)
{
    if (bFinished) return;
    bFinished = true;
    if (!Error.IsEmpty()) Failures.Add(Error);
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("schema"), TEXT("rok.mountain_f_linear_audit.v1"));
    Report->SetStringField(TEXT("scope"), TEXT("Representative F body, raw linear HDR SceneColor response to three source-native colors; same-pixel material multiply/sRGB-transfer only. Not absolute texture UV/LOD parity, not final viewport parity, not normal-world visual acceptance."));
    Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Report->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
    Report->SetStringField(TEXT("capture_source"), TEXT("SCS_SceneColorHDRNoAlpha"));
    Report->SetStringField(TEXT("target_format"), TEXT("PF_FloatRGBA"));
    Report->SetStringField(TEXT("raw_layout"), TEXT("row-major top-left RGBA16F little-endian, width*height*8 bytes"));
    Report->SetArrayField(TEXT("size"), { MakeShared<FJsonValueNumber>(Width), MakeShared<FJsonValueNumber>(Height) });
    Report->SetStringField(TEXT("render_mode"), TEXT("PRM_UseShowOnlyList, representative F mesh only"));
    Report->SetStringField(TEXT("geometry_lod"), TEXT("ForcedLodModel=1 (LOD0); texture GPU mip implicit, fixed camera across phases"));
    if (auto* SC = Capture.Get())
    {
        auto Settings = MakeShared<FJsonObject>();
        Settings->SetBoolField(TEXT("capture_every_frame"), SC->bCaptureEveryFrame);
        Settings->SetBoolField(TEXT("capture_on_movement"), SC->bCaptureOnMovement);
        Settings->SetBoolField(TEXT("persist_render_state"), SC->bAlwaysPersistRenderingState);
        Settings->SetBoolField(TEXT("anti_aliasing"), SC->ShowFlags.AntiAliasing);
        Settings->SetBoolField(TEXT("temporal_aa"), SC->ShowFlags.TemporalAA);
        Settings->SetBoolField(TEXT("eye_adaptation"), SC->ShowFlags.EyeAdaptation);
        Settings->SetBoolField(TEXT("motion_blur"), SC->ShowFlags.MotionBlur);
        Settings->SetNumberField(TEXT("show_only_components"), SC->ShowOnlyComponents.Num());
        Settings->SetNumberField(TEXT("capture_horizontal_fov"), SC->FOVAngle);
        Report->SetObjectField(TEXT("capture_settings"), Settings);
    }
    if (auto* RT = Target.Get())
    {
        Report->SetBoolField(TEXT("target_force_linear_gamma"), RT->bForceLinearGamma);
        Report->SetNumberField(TEXT("target_gamma"), RT->TargetGamma);
        Report->SetNumberField(TEXT("target_pixel_format_enum"), RT->GetFormat());
    }
    Report->SetStringField(TEXT("material"), MaterialPath); Report->SetStringField(TEXT("texture"), TexturePath);
    Report->SetStringField(TEXT("source_data_sha256"), SourceSha); Report->SetStringField(TEXT("product_manifest_sha256"), ProductSha);
    Report->SetStringField(TEXT("platform_mip0_sha256"), TextureMip0Sha);
    TArray<TSharedPtr<FJsonValue>> MipHashValues;
    for (const FString& Hash : PlatformMipShas) MipHashValues.Add(MakeShared<FJsonValueString>(Hash));
    Report->SetArrayField(TEXT("platform_mip_sha256"), MipHashValues);
    Report->SetStringField(TEXT("geometry_path"), GeometryPath); Report->SetStringField(TEXT("geometry_sha256"), GeometrySha);
    if (auto* Mesh = Body.Get())
    {
        Report->SetStringField(TEXT("representative_component"), Mesh->GetPathName());
        Report->SetStringField(TEXT("representative_source_key"), SourceKey(Mesh->GetOwner()));
        Report->SetStringField(TEXT("representative_original_material"), OriginalMaterial.Get() ? OriginalMaterial->GetPathName() : FString());
        Report->SetStringField(TEXT("representative_mesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : FString());
        const FTransform Transform = Mesh->GetComponentTransform();
        Report->SetArrayField(TEXT("component_location"), Array3(Transform.GetLocation()));
        Report->SetArrayField(TEXT("component_scale"), Array3(Transform.GetScale3D()));
        const FRotator Rotation = Transform.GetRotation().Rotator();
        Report->SetArrayField(TEXT("component_rotation_pyr"), Array3(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
    }
    TArray<TSharedPtr<FJsonValue>> FrameValues;
    for (const auto& Frame : Frames) FrameValues.Add(MakeShared<FJsonValueObject>(Frame));
    Report->SetArrayField(TEXT("frames"), FrameValues);
    const bool bRestored = Restore();
    Report->SetBoolField(TEXT("temporary_state_restored"), bRestored);
    if (!bRestored) Failures.Add(TEXT("Temporary camera/material/LOD/CVar restoration failed"));
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString& Failure : Failures) ErrorValues.Add(MakeShared<FJsonValueString>(Failure));
    Report->SetArrayField(TEXT("failures"), ErrorValues);
    Report->SetBoolField(TEXT("capture_completed"), Failures.IsEmpty() && Frames.Num() == 3);
    if (!SaveJson(Directory / TEXT("manifest.json"), Report))
        UE_LOG(LogTemp, Error, TEXT("Mountain F linear audit manifest save failed: %s"), *Directory);
    UE_LOG(LogTemp, Display, TEXT("Mountain F linear audit captures=%d restored=%d errors=%s output=%s"),
        Frames.Num(), bRestored, *FString::Join(Failures, TEXT("; ")), *Directory);
    FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() && Frames.Num() == 3 ? 0 : 1);
}
