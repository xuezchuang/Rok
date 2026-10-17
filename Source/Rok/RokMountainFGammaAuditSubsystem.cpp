#include "RokMountainFGammaAuditSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "Slate/SlateViewportProvider.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RHIGPUReadback.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "UnrealClient.h"
#include "Widgets/SViewport.h"

struct FRokMountainFGpuPackedCapture
{
    enum class EStatus : uint8 { Queued, CopySubmitted, Ready, Failed };
    FCriticalSection Mutex;
    TUniquePtr<FRHIGPUTextureReadback> Readback; // Accessed only on the render thread.
    TArray<uint8> TightBytes;
    FString Failure;
    EStatus Status = EStatus::Queued;
    bool bPollQueued = false;
    uint64 Serial = 0;
    uint64 RequestGameFrame = 0;
    uint64 ViewportGameFrame = 0;
    uint64 CopyRenderFrame = 0;
    uint64 ReadyRenderFrame = 0;
    double RequestSeconds = 0;
    double ViewportSeconds = 0;
    double CopySeconds = 0;
    double ReadySeconds = 0;
    int32 Stage = -1;
    int32 Width = 0, Height = 0, Samples = 0;
    int32 RowPitchPixels = 0, MappedHeight = 0;
    uint32 GpuMask = 0;
    EPixelFormat Format = PF_Unknown;
};

namespace
{
const TCHAR* MaterialPath = TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_mountain_F.M_WorldSource_build_mountain_F");
const TCHAR* MeshPath = TEXT("/Game/RokUnityWorldMapPrototype/Meshes/MountainBodies/SM_RokWorldBody_build_mountain_F_build_mountain_F.SM_RokWorldBody_build_mountain_F_build_mountain_F");
const TCHAR* TexturePath = TEXT("/Game/RokUnityWorldMapPrototype/Textures/T_MountainF_UnityDecodedMips.T_MountainF_UnityDecodedMips");
FString Saved(const TCHAR* Relative) { return FPaths::ProjectSavedDir() / Relative; }
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
    FString Text;
    return FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
FString ShaFile(const FString& Path)
{
    TArray<uint8> Bytes;
    return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
bool Array3(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field, FVector& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
    double X, Y, Z;
    if (!Row.IsValid() || !Row->TryGetArrayField(Field, A) || A->Num() != 3 ||
        !(*A)[0]->TryGetNumber(X) || !(*A)[1]->TryGetNumber(Y) || !(*A)[2]->TryGetNumber(Z) ||
        !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
    Out = FVector(X, Y, Z);
    return true;
}
bool ObjectVec(const TSharedPtr<FJsonObject>& Row, const TCHAR* Field, FVector& Out)
{
    const TSharedPtr<FJsonObject>* O = nullptr;
    double X, Y, Z;
    if (!Row.IsValid() || !Row->TryGetObjectField(Field, O) || !O || !O->IsValid() ||
        !(*O)->TryGetNumberField(TEXT("x"), X) || !(*O)->TryGetNumberField(TEXT("y"), Y) ||
        !(*O)->TryGetNumberField(TEXT("z"), Z) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z)) return false;
    Out = FVector(X, Y, Z);
    return true;
}
TArray<TSharedPtr<FJsonValue>> XYZ(FVector V)
{
    return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
}
FString MakeFSourceInstanceKey(const AActor* Actor)
{
    FString Tile, Index;
    for (const FName Tag : Actor->Tags)
    {
        const FString T = Tag.ToString();
        if (T.StartsWith(TEXT("UnityTile_"))) Tile = T;
        if (T.StartsWith(TEXT("UnityObjectIndex_"))) Index = T;
    }
    return Tile.IsEmpty() || Index.IsEmpty() ? FString() : Tile + TEXT("/") + Index;
}
FString MakeFSourceInstanceKey(const TArray<TSharedPtr<FJsonValue>>& Tags)
{
    FString Tile, Index;
    for (const auto& V : Tags)
    {
        FString T;
        if (!V.IsValid() || !V->TryGetString(T)) return FString();
        if (T.StartsWith(TEXT("UnityTile_"))) Tile = T;
        if (T.StartsWith(TEXT("UnityObjectIndex_"))) Index = T;
    }
    return Tile.IsEmpty() || Index.IsEmpty() ? FString() : Tile + TEXT("/") + Index;
}
bool Near(FVector A, FVector B, double Tolerance) { return FVector::Dist(A, B) <= Tolerance; }
}

bool URokMountainFGammaAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityWorldMapPrototype") &&
        FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")) &&
        FParse::Param(FCommandLine::Get(), TEXT("RokMountainFGammaAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMountainFGammaAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokMountainFGammaAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory = FPaths::ConvertRelativePathToFull(Saved(TEXT("RokUnityMigration/MountainFGammaAudit")) /
        FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokMountainFGammaAuditSubsystem::Processed);
}
void URokMountainFGammaAuditSubsystem::Deinitialize()
{
    RestoreRuntimeState();
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    Super::Deinitialize();
}
TStatId URokMountainFGammaAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokMountainFGammaAuditSubsystem, STATGROUP_Tickables); }

bool URokMountainFGammaAuditSubsystem::LoadSourceAndProduct()
{
    const FString SourcePath = Saved(TEXT("RokUnityMigration/MountainFGamma/source_data.json"));
    const FString RenderPath = Saved(TEXT("RokUnityMigration/MountainFGamma/native_render.json"));
    const FString VerifyPath = Saved(TEXT("RokUnityMigration/MountainFGammaV2/verify.json"));
    const FString V2RenderPath = Saved(TEXT("RokUnityMigration/MountainFGammaV2/native_render.json"));
    const FString V2PreparePath = Saved(TEXT("RokUnityMigration/MountainFGammaV2/prepare.json"));
    const FString ProductPath = Saved(TEXT("RokUnityMigration/MountainFGammaProduct/prepare_manifest.json"));
    TSharedPtr<FJsonObject> Source, Native, V2Verify, V2Render, Product;
    if (!ReadJson(SourcePath, Source) || !ReadJson(RenderPath, Native) || !ReadJson(VerifyPath, V2Verify) ||
        !ReadJson(V2RenderPath, V2Render) || !ReadJson(ProductPath, Product)) { Finish(TEXT("Missing F source/native/V2/product JSON")); return false; }
    if (!V2Verify->GetBoolField(TEXT("passed")) || !V2Verify->GetBoolField(TEXT("native_checked")) ||
        Product->GetStringField(TEXT("schema")) != TEXT("rok.mountain_f_gamma_product.prepare.v1") ||
        Product->GetStringField(TEXT("status")) != TEXT("prepared-not-imported") ||
        Product->GetStringField(TEXT("source_orientation")) != TEXT("as_saved") ||
        Product->GetBoolField(TEXT("dds_extra_vertical_flip")) ||
        Product->GetStringField(TEXT("dds_format")) != TEXT("DX10 B8G8R8A8_UNORM 2D") ||
        Product->GetIntegerField(TEXT("mip_count")) != 11 ||
        Native->GetIntegerField(TEXT("mipmapCount")) != 11 || V2Render->GetIntegerField(TEXT("mipCount")) != 11)
    { Finish(TEXT("Unverified or incompatible source/V2/DDS contract")); return false; }
    const TSharedPtr<FJsonObject>* Hashes = nullptr;
    if (!Product->TryGetObjectField(TEXT("source_hashes"), Hashes) || !Hashes || !Hashes->IsValid() ||
        (*Hashes)->GetStringField(TEXT("verify_sha256")) != ShaFile(VerifyPath) ||
        (*Hashes)->GetStringField(TEXT("native_sha256")) != ShaFile(V2RenderPath) ||
        (*Hashes)->GetStringField(TEXT("prepare_sha256")) != ShaFile(V2PreparePath))
    { Finish(TEXT("DDS source provenance hashes differ from verified V2")); return false; }
    SourceDataSha = ShaFile(SourcePath); SourceRenderSha = ShaFile(RenderPath); SourceVerifySha = ShaFile(VerifyPath); ProductManifestSha = ShaFile(ProductPath);
    const TArray<TSharedPtr<FJsonValue>>* Samples = nullptr;
    if (!Source->TryGetArrayField(TEXT("samples"), Samples) || Samples->Num() != 3) { Finish(TEXT("Expected three F stable source colors")); return false; }
    const TCHAR* Expected[] = { TEXT("stable_day"), TEXT("stable_dusk"), TEXT("stable_night") };
    for (int32 I = 0; I < 3; ++I)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        FVector RGB = FVector::ZeroVector;
        if (!(*Samples)[I]->TryGetObject(Row) || !Row || !Row->IsValid() || (*Row)->GetStringField(TEXT("name")) != Expected[I] || !Array3(*Row, TEXT("spriteColor"), RGB))
        { Finish(TEXT("Malformed source F color samples")); return false; }
        Names.Add(Expected[I]); Colors.Add(FLinearColor(RGB.X, RGB.Y, RGB.Z, 1));
    }
    const TArray<TSharedPtr<FJsonValue>>* SourceInstances = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* NativeFrames = nullptr;
    if (!Source->TryGetArrayField(TEXT("instances"), SourceInstances) || SourceInstances->Num() != 18 ||
        !Native->TryGetArrayField(TEXT("frames"), NativeFrames) || NativeFrames->Num() != 3)
    { Finish(TEXT("Expected 18 F source instances and three native reference frames")); return false; }
    for (int32 I = 0; I < 3; ++I)
    {
        const TSharedPtr<FJsonObject>* Frame = nullptr;
        FVector NativeColor = FVector::ZeroVector;
        if (!(*NativeFrames)[I]->TryGetObject(Frame) || !Frame || !Frame->IsValid() || (*Frame)->GetStringField(TEXT("name")) != Expected[I] ||
            !Array3(*Frame, TEXT("spriteColor"), NativeColor) ||
            !Near(NativeColor, FVector(Colors[I].R, Colors[I].G, Colors[I].B), .0001) ||
            ShaFile((*Frame)->GetStringField(TEXT("path"))).IsEmpty())
        { Finish(TEXT("Native source reference PNG unavailable")); return false; }
    }
    const TArray<TSharedPtr<FJsonValue>>* ProductMips = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* V2Mips = nullptr;
    if (!Product->TryGetArrayField(TEXT("mips"), ProductMips) || ProductMips->Num() != 11 ||
        !V2Render->TryGetArrayField(TEXT("mips"), V2Mips) || V2Mips->Num() != 11)
    { Finish(TEXT("Expected all eleven verified F mip levels")); return false; }
    for (int32 I = 0; I < 11; ++I)
    {
        const TSharedPtr<FJsonObject>* P = nullptr;
        const TSharedPtr<FJsonObject>* N = nullptr;
        if (!(*ProductMips)[I]->TryGetObject(P) || !P || !P->IsValid() || !(*V2Mips)[I]->TryGetObject(N) || !N || !N->IsValid() ||
            (*P)->GetIntegerField(TEXT("level")) != I || (*N)->GetIntegerField(TEXT("level")) != I ||
            (*P)->GetStringField(TEXT("source_orientation")) != TEXT("as_saved") ||
            (*P)->GetStringField(TEXT("source_png_sha256")) != ShaFile((*N)->GetStringField(TEXT("rgbaPath"))) ||
            (*P)->GetStringField(TEXT("source_bc1_sha256")) != ShaFile((*N)->GetStringField(TEXT("bc1Path"))))
        { Finish(TEXT("DDS mip provenance does not match GPU-decoded V2 source")); return false; }
    }
    return true;
}

bool URokMountainFGammaAuditSubsystem::CheckTextureMips()
{
    auto* T = LoadObject<UTexture2D>(nullptr, TexturePath);
    Texture = T;
    if (!T || T->GetPathName() != TexturePath || T->GetSizeX() != 1024 || T->GetSizeY() != 1024 ||
        T->GetNumMips() != 11 || T->SRGB || T->Filter != TF_Bilinear || T->GetPixelFormat() != PF_B8G8R8A8 ||
        T->CompressionSettings != TC_VectorDisplacementmap || !T->NeverStream)
    { Finish(TEXT("F decoded texture asset flags/size/platform format invalid")); return false; }
#if WITH_EDITORONLY_DATA
    if (T->MipGenSettings != TMGS_LeaveExistingMips || T->MaxTextureSize != 1024)
    { Finish(TEXT("F decoded texture did not preserve existing eleven mips")); return false; }
#else
    Finish(TEXT("Editor-only mip-generation settings unavailable; cannot certify texture import")); return false;
#endif
    auto* Platform = T->GetPlatformData();
    if (!Platform || Platform->PixelFormat != PF_B8G8R8A8 || Platform->Mips.Num() != 11 || T->GetNumResidentMips() < 1)
    { Finish(TEXT("F decoded platform mips unavailable")); return false; }
    TextureState = MakeShared<FJsonObject>();
    TextureState->SetStringField(TEXT("path"), T->GetPathName());
    TextureState->SetNumberField(TEXT("width"), T->GetSizeX()); TextureState->SetNumberField(TEXT("height"), T->GetSizeY());
    TextureState->SetNumberField(TEXT("mips"), T->GetNumMips()); TextureState->SetNumberField(TEXT("resident_mips"), T->GetNumResidentMips());
    TextureState->SetStringField(TEXT("platform_format"), GPixelFormats[Platform->PixelFormat].Name);
    TextureState->SetBoolField(TEXT("srgb"), T->SRGB); TextureState->SetBoolField(TEXT("filter_bilinear"), T->Filter == TF_Bilinear);
    TextureState->SetBoolField(TEXT("never_stream"), T->NeverStream);
#if WITH_EDITORONLY_DATA
    TextureState->SetBoolField(TEXT("leave_existing_mips"), T->MipGenSettings == TMGS_LeaveExistingMips);
    TextureState->SetNumberField(TEXT("max_texture_size"), T->MaxTextureSize);
    TextureState->SetNumberField(TEXT("source_mips"), T->Source.GetNumMips());
    TextureState->SetNumberField(TEXT("source_width"), T->Source.GetSizeX());
    TextureState->SetNumberField(TEXT("source_height"), T->Source.GetSizeY());
    TextureState->SetNumberField(TEXT("source_format_enum"), static_cast<int32>(T->Source.GetFormat()));
    TextureState->SetBoolField(TEXT("source_has_payload"), T->Source.HasPayloadData());
#endif
    TextureState->SetNumberField(TEXT("platform_mips_in_tail"), Platform->GetNumMipsInTail());
    TSharedPtr<FJsonObject> Product;
    if (!ReadJson(Saved(TEXT("RokUnityMigration/MountainFGammaProduct/prepare_manifest.json")), Product))
    { Finish(TEXT("Missing DDS product manifest during mip audit")); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Expected = nullptr;
    if (!Product->TryGetArrayField(TEXT("mips"), Expected) || Expected->Num() != 11) { Finish(TEXT("Malformed DDS mip manifest")); return false; }
    TArray<TSharedPtr<FJsonObject>> Records;
    Records.Reserve(11);
    bool bMetadataMatches = true;
    for (int32 I = 0; I < 11; ++I)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        if (!(*Expected)[I]->TryGetObject(Row) || !Row || !Row->IsValid()) { Finish(TEXT("Malformed expected mip row")); return false; }
        auto& Mip = Platform->Mips[I];
        const int32 Edge = FMath::Max(1, 1024 >> I);
        const int64 Size = int64(Edge) * Edge * 4;
        auto Record = MakeShared<FJsonObject>();
        Record->SetNumberField(TEXT("level"), I);
        Record->SetNumberField(TEXT("expected_width"), Edge); Record->SetNumberField(TEXT("expected_height"), Edge);
        Record->SetNumberField(TEXT("width"), Mip.SizeX); Record->SetNumberField(TEXT("height"), Mip.SizeY);
        Record->SetNumberField(TEXT("expected_bytes"), double(Size));
        Record->SetNumberField(TEXT("manifest_bytes"), double((*Row)->GetIntegerField(TEXT("bytes"))));
        Record->SetNumberField(TEXT("bulk_bytes"), double(Mip.BulkData.GetBulkDataSize()));
        Record->SetNumberField(TEXT("bulk_on_disk_bytes"), double(Mip.BulkData.GetBulkDataSizeOnDisk()));
        Record->SetBoolField(TEXT("bulk_loaded"), Mip.BulkData.IsBulkDataLoaded());
        Record->SetBoolField(TEXT("bulk_can_load_from_disk"), Mip.BulkData.CanLoadFromDisk());
        Record->SetBoolField(TEXT("bulk_exists_on_disk"), Mip.BulkData.DoesExist());
        Record->SetBoolField(TEXT("bulk_separate_file"), Mip.BulkData.IsInSeparateFile());
        Record->SetBoolField(TEXT("derived_data_reference"), Mip.DerivedData.HasData());
        Record->SetStringField(TEXT("expected_sha256"), (*Row)->GetStringField(TEXT("bgra8_sha256")));
        const bool bThisMetadataMatches = Mip.SizeX == Edge && Mip.SizeY == Edge &&
            (*Row)->GetIntegerField(TEXT("level")) == I && (*Row)->GetIntegerField(TEXT("bytes")) == Size;
        Record->SetBoolField(TEXT("metadata_matches"), bThisMetadataMatches);
        bMetadataMatches &= bThisMetadataMatches;
        Records.Add(Record);
        MipRecords.Add(MakeShared<FJsonValueObject>(Record));
    }
    if (!bMetadataMatches) { Finish(TEXT("Platform F mip dimensions or DDS manifest dimensions differ; inspect platform_mips")); return false; }
    if (Platform->GetNumMipsInTail() > 1)
    { Finish(TEXT("Platform F has a packed mip tail; per-level byte hashes require unpacking")); return false; }

    // In the editor, streaming mip payloads may live only in derived data, leaving
    // FBulkData at zero bytes after resource creation. The public UTexture2D
    // virtual interface returns both the platform payload and its actual length;
    // it uses derived data or package bulk, not the imported source art.
    void* LoadedMips[11] = {};
    int64 LoadedSizes[11] = {};
    const bool bLoaded = T->GetInitialMipData(0, MakeArrayView(LoadedMips), MakeArrayView(LoadedSizes));
    bool bAllBytesMatch = bLoaded;
    for (int32 I = 0; I < 11; ++I)
    {
        const int32 Edge = FMath::Max(1, 1024 >> I);
        const int64 Size = int64(Edge) * Edge * 4;
        Records[I]->SetNumberField(TEXT("loaded_bytes"), double(LoadedSizes[I]));
        Records[I]->SetNumberField(TEXT("bytes"), double(LoadedSizes[I]));
        Records[I]->SetBoolField(TEXT("platform_payload_loaded"), LoadedMips[I] != nullptr);
        if (bLoaded && LoadedMips[I] && LoadedSizes[I] == Size)
        {
            const FString Actual = RokTrainArmyMesh::Sha256(TConstArrayView<uint8>(static_cast<const uint8*>(LoadedMips[I]), static_cast<int32>(Size)));
            Records[I]->SetStringField(TEXT("sha256"), Actual);
            const bool bMatch = Actual == Records[I]->GetStringField(TEXT("expected_sha256"));
            Records[I]->SetBoolField(TEXT("sha256_matches"), bMatch);
            bAllBytesMatch &= bMatch;
        }
        else
        {
            Records[I]->SetBoolField(TEXT("sha256_matches"), false);
            bAllBytesMatch = false;
        }
        if (LoadedMips[I]) FMemory::Free(LoadedMips[I]);
    }
    if (!bLoaded) { Finish(TEXT("Cannot load all eleven platform F mip payloads from derived data or package bulk")); return false; }
    if (!bAllBytesMatch) { Finish(TEXT("Platform F mip BGRA payload length/hash differs from verified DDS")); return false; }
    return true;
}

bool URokMountainFGammaAuditSubsystem::BindFreshActors()
{
    TSharedPtr<FJsonObject> Binding, Source;
    if (!ReadJson(Saved(TEXT("RokUnityMigration/NextWorldMaterialSlice/ue_binding_probe.json")), Binding) ||
        !ReadJson(Saved(TEXT("RokUnityMigration/MountainFGamma/source_data.json")), Source))
    { Finish(TEXT("Missing static F binding/source snapshot")); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* SourceRows = nullptr;
    if (!Binding->TryGetArrayField(TEXT("actors"), Rows) || !Source->TryGetArrayField(TEXT("instances"), SourceRows) || SourceRows->Num() != 18)
    { Finish(TEXT("Missing 18 F source/binding records")); return false; }
    TMap<FString, TSharedPtr<FJsonObject>> Expected;
    for (const auto& Value : *Rows)
    {
        const TSharedPtr<FJsonObject>* Row = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
        if (!Value->TryGetObject(Row) || !Row || !Row->IsValid() || !(*Row)->TryGetArrayField(TEXT("tags"), Tags)) continue;
        if (!(*Row)->GetStringField(TEXT("label")).EndsWith(TEXT("_build_mountain_F"))) continue;
        const FString Key = MakeFSourceInstanceKey(*Tags);
        if (Key.IsEmpty() || Expected.Contains(Key)) { Finish(TEXT("Duplicate/missing F binding key")); return false; }
        Expected.Add(Key, *Row);
    }
    if (Expected.Num() != 18) { Finish(TEXT("Expected exactly 18 historical F body bindings")); return false; }
    TSet<FString> Seen;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor->ActorHasTag(TEXT("UnityPrefab_build_mountain_F"))) continue;
        if (!Actor->ActorHasTag(TEXT("RokWorldMapExactSource")) || !Actor->ActorHasTag(TEXT("RokWorldMapSourceObject")) ||
            !Actor->ActorHasTag(TEXT("UnityTransformPolicy_PositionRotationScale")))
        { Finish(TEXT("F actor lost source identity tags")); return false; }
        const FString Key = MakeFSourceInstanceKey(Actor);
        if (!Expected.Contains(Key) || Seen.Contains(Key)) { Finish(TEXT("Unexpected/duplicate fresh F actor identity")); return false; }
        Seen.Add(Key);
        const auto& Row = Expected[Key];
        FVector Position, Rotation, Scale;
        if (!Array3(Row, TEXT("location"), Position) || !Array3(Row, TEXT("rotation"), Rotation) || !Array3(Row, TEXT("scale"), Scale) ||
            !Near(Actor->GetActorLocation(), Position, .1) || !Near(Actor->GetActorScale3D(), Scale, .001) ||
            FMath::Abs(FMath::FindDeltaAngleDegrees(Actor->GetActorRotation().Yaw, Rotation.Y)) > .01 ||
            FMath::Abs(Actor->GetActorRotation().Pitch - Rotation.X) > .01 || FMath::Abs(Actor->GetActorRotation().Roll - Rotation.Z) > .01)
        { Finish(TEXT("Fresh F actor transform differs from binding snapshot")); return false; }
        bool bSourcePositionMatched = false;
        for (const auto& SourceValue : *SourceRows)
        {
            const TSharedPtr<FJsonObject>* S = nullptr;
            const TSharedPtr<FJsonObject>* I = nullptr;
            FVector SourcePosition, SourceScale;
            if (!SourceValue->TryGetObject(S) || !S || !S->IsValid() || !(*S)->TryGetObjectField(TEXT("source_instance"), I) || !I || !I->IsValid() ||
                !ObjectVec(*I, TEXT("ue_relative_to_focus"), SourcePosition) || !ObjectVec(*I, TEXT("ue_actor_scale_for_centimeter_asset"), SourceScale)) continue;
            if (Near(Position, SourcePosition, .1) && Near(Scale, SourceScale, .001)) { bSourcePositionMatched = true; break; }
        }
        if (!bSourcePositionMatched) { Finish(TEXT("Fresh F transform absent from source-derived 18")); return false; }
        TArray<UStaticMeshComponent*> Meshes; Actor->GetComponents(Meshes);
        if (Meshes.Num() != 1 || !Meshes[0] || !Meshes[0]->GetStaticMesh() || Meshes[0]->GetStaticMesh()->GetPathName() != MeshPath ||
            Meshes[0]->GetNumMaterials() != 1 || !Meshes[0]->GetMaterial(0) ||
            !Meshes[0]->GetMaterial(0)->GetBaseMaterial() || Meshes[0]->GetMaterial(0)->GetBaseMaterial()->GetPathName() != MaterialPath ||
            !Meshes[0]->IsVisible() || Meshes[0]->bHiddenInGame || Actor->IsHidden())
        { Finish(TEXT("Fresh F mesh/material/visibility differs from source body")); return false; }
        auto* Original = Meshes[0]->GetMaterial(0);
        TArray<UTexture*> UsedTextures; Original->GetUsedTextures(UsedTextures);
        if (!UsedTextures.Contains(Texture.Get())) { Finish(TEXT("Existing F material chain does not sample dedicated decoded mips")); return false; }
        auto* MID = UMaterialInstanceDynamic::Create(Original, Meshes[0]);
        if (!MID) { Finish(TEXT("Cannot create runtime-only F color MID")); return false; }
        Meshes[0]->SetMaterial(0, MID);
        FBody Body; Body.Mesh = Meshes[0]; Body.OriginalMaterial.Reset(Original); Body.AuditMaterial.Reset(MID); Body.Key = Key; Bodies.Add(MoveTemp(Body));
        auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("key"), Key);
        Record->SetStringField(TEXT("actor"), Actor->GetName()); Record->SetStringField(TEXT("mesh"), Meshes[0]->GetStaticMesh()->GetPathName());
        Record->SetStringField(TEXT("original_material"), Original->GetPathName()); Record->SetStringField(TEXT("base_material"), Original->GetBaseMaterial()->GetPathName());
        Record->SetArrayField(TEXT("location"), XYZ(Position)); Record->SetArrayField(TEXT("rotation"), XYZ(Rotation)); Record->SetArrayField(TEXT("scale"), XYZ(Scale));
        BindingRecords.Add(MakeShared<FJsonValueObject>(Record));
    }
    if (Seen.Num() != 18 || Bodies.Num() != 18) { Finish(TEXT("Fresh world has not exactly 18 F bodies")); return false; }
    return true;
}

bool URokMountainFGammaAuditSubsystem::Begin()
{
    if (GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype") ||
        !FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")))
    { Finish(TEXT("Wrong project/world for F audit")); return false; }
    TArray<FString> Tokens; FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
    for (const FString& Token : Tokens)
    {
        if (Token.StartsWith(TEXT("-Rok")) && Token != TEXT("-RokMountainFGammaAudit") && Token != TEXT("-RokMountainFGammaIsolate") && Token != TEXT("-RokMountainFSdrOutputDiagnostic"))
        { Finish(TEXT("Conflicting Rok runtime flag during F audit")); return false; }
    }
    bIsolate = FParse::Param(FCommandLine::Get(), TEXT("RokMountainFGammaIsolate"));
    bSdrOutputDiagnostic = FParse::Param(FCommandLine::Get(), TEXT("RokMountainFSdrOutputDiagnostic"));
    if (bSdrOutputDiagnostic && !bIsolate) { Finish(TEXT("SDR output diagnostic requires F isolation")); return false; }
    if (!LoadSourceAndProduct() || !CheckTextureMips()) return false;
    int32 Cameras = 0, Configs = 0; ARokWorldMapConfigActor* Config = nullptr;
    for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++Cameras; }
    for (TActorIterator<ARokWorldMapConfigActor> It(GetWorld()); It; ++It) { Config = *It; ++Configs; }
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (Cameras != 1 || Configs != 1 || !PC || PC->GetPawn() != Camera.Get() || PC->GetViewTarget() != Camera.Get())
    { Finish(TEXT("Actual F world camera/config route mismatch")); return false; }
    if (!BindFreshActors()) return false;
    SavedViewCenter = Camera->GetViewCenterUnity(); SavedDxf = Camera->GetCurrentDxf(); bCameraSaved = true;
    const FVector CurrentFocus = Config->Projection.UnityToUe(SavedViewCenter);
    UStaticMeshComponent* Nearest = nullptr; double Best = TNumericLimits<double>::Max();
    for (const auto& Body : Bodies)
    {
        auto* Mesh = Body.Mesh.Get();
        const double D = FVector::DistSquared(Mesh->Bounds.Origin, CurrentFocus);
        if (D < Best) { Best = D; Nearest = Mesh; }
    }
    if (!Nearest) { Finish(TEXT("No representative F body")); return false; }
    Representative = Nearest;
    const FVector Forward = Camera->GetCameraComponent()->GetForwardVector();
    if (FMath::Abs(Forward.Z) < .1) { Finish(TEXT("Unexpected source F camera direction")); return false; }
    const FVector Center = Nearest->Bounds.Origin;
    const FVector GroundFocus = Center - Forward * ((Center.Z - Config->Projection.GroundZ) / Forward.Z);
    Camera->SetViewCenterUnity(Config->Projection.UeToUnity(GroundFocus)); Camera->SetCurrentDxf(1509);
    if (bSdrOutputDiagnostic) SdrFramingInitialDxf = Camera->GetCurrentDxf();
    if (bIsolate)
    {
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        {
            TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
            for (auto* Mesh : Meshes)
            {
                if (Bodies.ContainsByPredicate([Mesh](const FBody& B) { return B.Mesh.Get() == Mesh; })) continue;
                SavedVisibility.Emplace(Mesh, FIntPoint(Mesh->IsVisible() ? 1 : 0, Mesh->bHiddenInGame ? 1 : 0));
                auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("component"), Mesh->GetPathName());
                Record->SetBoolField(TEXT("was_visible"), Mesh->IsVisible()); Record->SetBoolField(TEXT("was_hidden_in_game"), Mesh->bHiddenInGame);
                VisibilityRecords.Add(MakeShared<FJsonValueObject>(Record));
                Mesh->SetVisibility(false, false); Mesh->SetHiddenInGame(true, false);
            }
        }
    }
    if (bSdrOutputDiagnostic && !SetupSdrDiagnostic()) return false;
    Stage = 0; StageStart = Elapsed; StageFrame = GFrameCounter; SetStageColor();
    return true;
}
bool URokMountainFGammaAuditSubsystem::SetupSdrDiagnostic()
{
    auto* PC = GetWorld()->GetFirstPlayerController();
    auto* Local = PC ? PC->GetLocalPlayer() : nullptr;
    auto* GameViewport = GetWorld()->GetGameViewport();
    FViewport* Viewport = GameViewport ? GameViewport->GetGameViewport() : nullptr;
    if (!PC || !Local || !GameViewport || !Viewport ||
        !GameViewport->GetEngineShowFlags() || !GameViewport->GetEngineShowFlags()->Tonemapper ||
        !GameViewport->GetEngineShowFlags()->PostProcessing ||
        UGameViewportClient::OnScreenshotCaptured().IsBound())
    { Finish(TEXT("SDR diagnostic requires unclaimed normal game viewport screenshot path")); return false; }
    IConsoleVariable* DelegateCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenshotDelegate"), false);
    if (!DelegateCvar || DelegateCvar->GetInt() != 1)
    { Finish(TEXT("r.ScreenshotDelegate must equal one for pre-PNG FColor capture")); return false; }
    if (RHIGetInterfaceType() != ERHIInterfaceType::D3D12)
    { Finish(TEXT("Packed 10-bit/FColor byte oracle requires the UE 5.8 D3D12 readback path")); return false; }
    for (const TPair<const TCHAR*, int32>& Required : {
        TPair<const TCHAR*, int32>(TEXT("r.AntiAliasingMethod"), 0),
        TPair<const TCHAR*, int32>(TEXT("r.ScreenPercentage"), 100),
        TPair<const TCHAR*, int32>(TEXT("r.BackbufferQuantizationDitheringOverride"), 16),
        TPair<const TCHAR*, int32>(TEXT("r.TonemapperGamma"), 0) })
    {
        IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Required.Key, false);
        if (!Variable || Variable->GetInt() != Required.Value)
        { Finish(FString::Printf(TEXT("SDR diagnostic requires %s=%d"), Required.Key, Required.Value)); return false; }
    }
    IConsoleVariable* LutSizeCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LUT.Size"), false);
    if (!LutSizeCvar || (LutSizeCvar->GetInt() != 32 && LutSizeCvar->GetInt() != 64))
    { Finish(TEXT("SDR diagnostic requires explicit r.LUT.Size=32 or 64")); return false; }
    const uint32 LutPriority = static_cast<uint32>(LutSizeCvar->GetFlags()) & static_cast<uint32>(ECVF_SetByMask);
    if (LutPriority != static_cast<uint32>(ECVF_SetByConsole) &&
        LutPriority != static_cast<uint32>(ECVF_SetByCommandline))
    { Finish(TEXT("SDR diagnostic LUT dimension must be set by console or command line")); return false; }
    SdrConfiguredLutDimension = LutSizeCvar->GetInt();
    FSceneViewProjectionData Projection;
    if (!Local->GetProjectionData(Viewport, Projection) || Viewport->GetSizeXY() != FIntPoint(1440, 810))
    { Finish(TEXT("Cannot obtain 1440x810 game viewport projection")); return false; }
    SdrEffectiveViewRect = Projection.GetConstrainedViewRect();
    if (SdrEffectiveViewRect.Min.X < 0 || SdrEffectiveViewRect.Min.Y < 0 ||
        SdrEffectiveViewRect.Max.X > 1440 || SdrEffectiveViewRect.Max.Y > 810 ||
        SdrEffectiveViewRect.Width() < 1000 || SdrEffectiveViewRect.Height() < 700)
    { Finish(TEXT("Unexpected constrained source-camera view rect")); return false; }
    auto* Mesh = Representative.Get();
    if (!Mesh) { Finish(TEXT("Representative F component missing")); return false; }
    SdrRepresentativeKey = MakeFSourceInstanceKey(Mesh->GetOwner());
    if (SdrRepresentativeKey.IsEmpty()) { Finish(TEXT("Representative F source key missing")); return false; }
    // The aligned HDR capture shows only the representative; use the same
    // geometry for both viewport modes after verifying all 18 bindings.
    for (const FBody& Body : Bodies)
    {
        UStaticMeshComponent* Other = Body.Mesh.Get();
        if (!Other || Body.Key.IsEmpty()) { Finish(TEXT("F diagnostic isolation lost source body")); return false; }
        if (Other == Mesh) continue;
        SavedVisibility.Emplace(Other, FIntPoint(Other->IsVisible() ? 1 : 0, Other->bHiddenInGame ? 1 : 0));
        auto Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("component"), Other->GetPathName());
        Record->SetBoolField(TEXT("was_visible"), Other->IsVisible());
        Record->SetBoolField(TEXT("was_hidden_in_game"), Other->bHiddenInGame);
        Record->SetStringField(TEXT("source_key"), Body.Key);
        Record->SetStringField(TEXT("reason"), TEXT("match representative-only HDR capture"));
        VisibilityRecords.Add(MakeShared<FJsonValueObject>(Record));
        SdrHiddenBodyKeys.Add(Body.Key);
        Other->SetVisibility(false, false);
        Other->SetHiddenInGame(true, false);
    }
    if (SdrHiddenBodyKeys.Num() != 17) { Finish(TEXT("Representative F isolation did not hide 17 other source bodies")); return false; }
    SdrSavedForcedLod = Mesh->GetForcedLodModel(); bSdrLodSaved = true;
    Mesh->SetForcedLodModel(1);
    bSdrOriginalTonemapper = GameViewport->GetEngineShowFlags()->Tonemapper;
    bSdrTonemapperSaved = true;
    FEngineShowFlags* ViewFlags = GameViewport->GetEngineShowFlags();
    bSdrOriginalAntiAliasing = ViewFlags->AntiAliasing;
    bSdrOriginalTemporalAA = ViewFlags->TemporalAA;
    bSdrOriginalEyeAdaptation = ViewFlags->EyeAdaptation;
    bSdrOriginalMotionBlur = ViewFlags->MotionBlur;
    bSdrViewportFlagsSaved = true;
    ViewFlags->SetAntiAliasing(false);
    ViewFlags->SetTemporalAA(false);
    ViewFlags->SetEyeAdaptation(false);
    ViewFlags->SetMotionBlur(false);
    SdrPreExposureCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride"), false);
    if (!SdrPreExposureCvar) { Finish(TEXT("Pre-exposure override CVar unavailable")); return false; }
    SdrSavedPreExposure = SdrPreExposureCvar->GetFloat();
    SdrPreExposureCvar->SetWithCurrentPriority(1.0f, NAME_None, ECVF_SetByConsole, ECVF_SetByConstructor);
    bSdrPreExposureChanged = true;
    if (FMath::Abs(SdrPreExposureCvar->GetFloat() - 1.0f) > .000001f)
    { Finish(TEXT("Cannot fix pre-exposure at one")); return false; }

    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
    RT->InitCustomFormat(SdrEffectiveViewRect.Width(), SdrEffectiveViewRect.Height(), PF_FloatRGBA, true);
    RT->ClearColor = FLinearColor::Black;
    RT->UpdateResourceImmediate(true);
    SdrHdrTarget.Reset(RT);
    USceneCaptureComponent2D* SC = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
    SC->TextureTarget = RT;
    SC->CaptureSource = SCS_SceneColorHDRNoAlpha;
    SC->CompositeMode = SCCM_Overwrite;
    SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    SC->ShowOnlyComponents.Add(Mesh);
    SC->bCaptureEveryFrame = false;
    SC->bCaptureOnMovement = false;
    SC->bAlwaysPersistRenderingState = false;
    SC->ProjectionType = ECameraProjectionMode::Perspective;
    SC->FOVAngle = Camera->GetCameraComponent()->FieldOfView;
    SC->RegisterComponentWithWorld(GetWorld());
    SC->ShowFlags.SetAntiAliasing(false);
    SC->ShowFlags.SetTemporalAA(false);
    SC->ShowFlags.SetEyeAdaptation(false);
    SC->ShowFlags.SetMotionBlur(false);
    SdrHdrCapture.Reset(SC);
    RawScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &URokMountainFGammaAuditSubsystem::CapturedRaw);
    SdrViewportRenderedHandle = UGameViewportClient::OnViewportRendered().AddUObject(this, &URokMountainFGammaAuditSubsystem::SdrViewportRendered);
    return true;
}

void URokMountainFGammaAuditSubsystem::SdrViewportRendered(FViewport* Viewport)
{
    if (!bSdrOutputDiagnostic || bFinished || !Pending) return;
    auto* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!GameViewport || Viewport != GameViewport->GetGameViewport()) return;
    if (!FScreenshotRequest::IsScreenshotRequested()) return;
    if (SdrRawReadback || bSdrScreenshotProcessed)
    { Finish(TEXT("Duplicate or mistimed F GPU backbuffer capture callback")); return; }
    FSceneViewport* SceneViewport = Viewport->AsSceneViewport();
    TSharedPtr<SViewport> Widget;
    if (SceneViewport) Widget = SceneViewport->GetViewportWidget().Pin();
    TSharedPtr<ISlateViewport> ViewportOwner;
    if (Widget.IsValid()) ViewportOwner = Widget->GetViewportInterface().Pin();
    if (!SceneViewport || !ViewportOwner.IsValid() || ViewportOwner.Get() != static_cast<ISlateViewport*>(SceneViewport))
    { Finish(TEXT("Cannot retain exact F scene viewport through render-thread backbuffer copy")); return; }
    SdrViewportOwner = ViewportOwner; // Never copy this non-thread-safe Slate shared pointer into a render command.
    auto Capture = MakeShared<FRokMountainFGpuPackedCapture, ESPMode::ThreadSafe>();
    Capture->Serial = ++SdrRawCaptureSerial;
    Capture->Stage = Stage;
    Capture->RequestGameFrame = SdrRawRequestGameFrame;
    Capture->ViewportGameFrame = GFrameCounter;
    Capture->RequestSeconds = SdrRawRequestSeconds;
    Capture->ViewportSeconds = FPlatformTime::Seconds();
    SdrRawReadback = Capture;
    Pending->SetNumberField(TEXT("gpu_capture_serial"), static_cast<double>(Capture->Serial));
    Pending->SetNumberField(TEXT("gpu_viewport_game_frame"), static_cast<double>(Capture->ViewportGameFrame));
    ENQUEUE_RENDER_COMMAND(RokMountainFPackedBackbufferCopy)(
        [Capture, SceneViewport](FRHICommandListImmediate& RHICmdList)
        {
            FScopeLock Lock(&Capture->Mutex);
            FRHITexture* Source = SceneViewport->GetShaderResourceTexture().GetReference();
            if (!Source)
            {
                Capture->Failure = TEXT("Screenshot source RHI texture missing on render thread");
                Capture->Status = FRokMountainFGpuPackedCapture::EStatus::Failed;
                return;
            }
            const FRHITextureDesc& Desc = Source->GetDesc();
            Capture->Format = Desc.Format;
            Capture->Width = Source->GetSizeX();
            Capture->Height = Source->GetSizeY();
            Capture->Samples = Desc.NumSamples;
            Capture->CopyRenderFrame = GFrameCounterRenderThread;
            Capture->CopySeconds = FPlatformTime::Seconds();
            if (Desc.Format != PF_A2B10G10R10 || Capture->Width != 1440 || Capture->Height != 810 || Desc.NumSamples != 1 ||
                GPixelFormats[Desc.Format].BlockBytes != 4)
            {
                Capture->Failure = TEXT("Screenshot source is not single-sample 1440x810 PF_A2B10G10R10");
                Capture->Status = FRokMountainFGpuPackedCapture::EStatus::Failed;
                return;
            }
            Capture->Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("RokMountainFPackedBackbuffer"));
            Capture->Readback->EnqueueCopy(RHICmdList, Source, FIntVector::ZeroValue, 0, FIntVector(Capture->Width, Capture->Height, 1));
            Capture->GpuMask = Capture->Readback->GetLastCopyGPUMask().GetNative();
            Capture->Status = FRokMountainFGpuPackedCapture::EStatus::CopySubmitted;
        });
}

void URokMountainFGammaAuditSubsystem::PollSdrRawReadback()
{
    const auto Capture = SdrRawReadback;
    if (!Capture) return;
    {
        FScopeLock Lock(&Capture->Mutex);
        if (Capture->Status != FRokMountainFGpuPackedCapture::EStatus::CopySubmitted || Capture->bPollQueued) return;
        Capture->bPollQueued = true;
    }
    ENQUEUE_RENDER_COMMAND(RokMountainFPackedBackbufferPoll)(
        [Capture](FRHICommandListImmediate&)
        {
            FScopeLock Lock(&Capture->Mutex);
            Capture->bPollQueued = false;
            if (Capture->Status != FRokMountainFGpuPackedCapture::EStatus::CopySubmitted) return;
            if (!Capture->Readback || !Capture->Readback->IsReady()) return;
            int32 RowPitch = 0, BufferHeight = 0;
            const void* Mapped = Capture->Readback->Lock(RowPitch, &BufferHeight);
            if (!Mapped || RowPitch < Capture->Width || BufferHeight < Capture->Height)
            {
                if (Mapped) Capture->Readback->Unlock();
                Capture->Failure = TEXT("GPU staging map missing or smaller than packed backbuffer");
                Capture->Status = FRokMountainFGpuPackedCapture::EStatus::Failed;
                return;
            }
            Capture->RowPitchPixels = RowPitch;
            Capture->MappedHeight = BufferHeight;
            const int32 TightStride = Capture->Width * 4;
            Capture->TightBytes.SetNumUninitialized(TightStride * Capture->Height);
            const uint8* Source = static_cast<const uint8*>(Mapped);
            for (int32 Y = 0; Y < Capture->Height; ++Y)
                FMemory::Memcpy(Capture->TightBytes.GetData() + Y * TightStride, Source + Y * RowPitch * 4, TightStride);
            Capture->Readback->Unlock();
            Capture->Readback.Reset();
            Capture->ReadyRenderFrame = GFrameCounterRenderThread;
            Capture->ReadySeconds = FPlatformTime::Seconds();
            Capture->Status = FRokMountainFGpuPackedCapture::EStatus::Ready;
        });
}

bool URokMountainFGammaAuditSubsystem::StoreSdrRawReadback()
{
    const auto Capture = SdrRawReadback;
    if (!Capture || !Pending || !bSdrScreenshotProcessed) { Finish(TEXT("Packed GPU readback lacks matching processed screenshot")); return false; }
    TArray<uint8> Bytes;
    TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
    bool bValidReadback = false;
    {
        FScopeLock Lock(&Capture->Mutex);
        bValidReadback = Capture->Status == FRokMountainFGpuPackedCapture::EStatus::Ready &&
            Capture->Stage == Stage && Capture->Serial == SdrRawCaptureSerial &&
            Capture->TightBytes.Num() == 1440 * 810 * 4;
        if (bValidReadback) Bytes = MoveTemp(Capture->TightBytes);
        if (bValidReadback)
        {
            Meta->SetNumberField(TEXT("capture_serial"), static_cast<double>(Capture->Serial));
            Meta->SetNumberField(TEXT("stage"), Capture->Stage);
            Meta->SetNumberField(TEXT("request_game_frame"), static_cast<double>(Capture->RequestGameFrame));
            Meta->SetNumberField(TEXT("viewport_game_frame"), static_cast<double>(Capture->ViewportGameFrame));
            Meta->SetNumberField(TEXT("copy_render_frame"), static_cast<double>(Capture->CopyRenderFrame));
            Meta->SetNumberField(TEXT("ready_render_frame"), static_cast<double>(Capture->ReadyRenderFrame));
            Meta->SetNumberField(TEXT("request_platform_seconds"), Capture->RequestSeconds);
            Meta->SetNumberField(TEXT("viewport_platform_seconds"), Capture->ViewportSeconds);
            Meta->SetNumberField(TEXT("copy_platform_seconds"), Capture->CopySeconds);
            Meta->SetNumberField(TEXT("ready_platform_seconds"), Capture->ReadySeconds);
            Meta->SetStringField(TEXT("pixel_format"), GPixelFormats[Capture->Format].Name);
            Meta->SetNumberField(TEXT("pixel_format_enum"), static_cast<int32>(Capture->Format));
            Meta->SetArrayField(TEXT("extent"), { MakeShared<FJsonValueNumber>(Capture->Width), MakeShared<FJsonValueNumber>(Capture->Height) });
            Meta->SetNumberField(TEXT("sample_count"), Capture->Samples);
            Meta->SetNumberField(TEXT("gpu_mask_native"), Capture->GpuMask);
            Meta->SetNumberField(TEXT("mapped_row_pitch_pixels"), Capture->RowPitchPixels);
            Meta->SetNumberField(TEXT("mapped_row_pitch_bytes"), Capture->RowPitchPixels * 4);
            Meta->SetNumberField(TEXT("mapped_buffer_height"), Capture->MappedHeight);
            Meta->SetNumberField(TEXT("file_row_stride_bytes"), Capture->Width * 4);
        }
    }
    if (!bValidReadback)
    { Finish(TEXT("Packed GPU readback stage, serial, or byte count differs")); return false; }
    const FString RawPath = FPaths::ChangeExtension(PendingPath, TEXT("a2b10g10r10"));
    if (!FFileHelper::SaveArrayToFile(Bytes, *RawPath))
    { Finish(TEXT("Packed GPU backbuffer save failed")); return false; }
    Meta->SetStringField(TEXT("path"), RawPath);
    Meta->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Meta->SetNumberField(TEXT("bytes"), Bytes.Num());
    Meta->SetNumberField(TEXT("bytes_per_pixel"), 4);
    Meta->SetStringField(TEXT("rhi_interface"), TEXT("D3D12"));
    Meta->SetStringField(TEXT("graphics_rhi"), FApp::GetGraphicsRHI());
    Meta->SetStringField(TEXT("source_api"), TEXT("OnViewportRendered -> render-thread FViewport::GetShaderResourceTexture -> FRHIGPUTextureReadback CopyTexture/fence/Lock"));
    Meta->SetStringField(TEXT("layout"), TEXT("Full-viewport tight rows; D3D12 DXGI_FORMAT_R10G10B10A2_UNORM packed little-endian words, R bits 0:9, G 10:19, B 20:29, A 30:31; no FColor conversion"));
    Meta->SetStringField(TEXT("row_origin"), TEXT("same RHI top-row origin as FViewport::ReadPixels; verify against same-frame FColor"));
    Meta->SetArrayField(TEXT("source_rect"), { MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0),
        MakeShared<FJsonValueNumber>(1440), MakeShared<FJsonValueNumber>(810) });
    Meta->SetArrayField(TEXT("constrained_view_rect"), { MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.X),
        MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.Y), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.X),
        MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.Y) });
    Pending->SetObjectField(TEXT("gpu_packed_backbuffer"), Meta);
    SdrRawReadback.Reset();
    SdrViewportOwner.Reset(); // Copy and poll commands both finished before Ready was observed.
    return true;
}

bool URokMountainFGammaAuditSubsystem::RecordSdrViewport(TSharedPtr<FJsonObject>& State)
{
    auto* GameViewport = GetWorld()->GetGameViewport();
    FViewport* Viewport = GameViewport ? GameViewport->GetGameViewport() : nullptr;
    if (!Viewport || !GameViewport || !GameViewport->GetEngineShowFlags())
    { Finish(TEXT("SDR diagnostic viewport disappeared")); return false; }
    FSceneViewProjectionData Projection;
    auto* PC = GetWorld()->GetFirstPlayerController();
    auto* Local = PC ? PC->GetLocalPlayer() : nullptr;
    if (!Local || !Local->GetProjectionData(Viewport, Projection) || Projection.GetConstrainedViewRect() != SdrEffectiveViewRect)
    { Finish(TEXT("SDR constrained viewport changed during six-phase capture")); return false; }
    IConsoleVariable* LutSizeCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.LUT.Size"), false);
    const uint32 LutPriority = LutSizeCvar
        ? static_cast<uint32>(LutSizeCvar->GetFlags()) & static_cast<uint32>(ECVF_SetByMask) : 0;
    if (!LutSizeCvar || LutSizeCvar->GetInt() != SdrConfiguredLutDimension ||
        (LutPriority != static_cast<uint32>(ECVF_SetByConsole) &&
         LutPriority != static_cast<uint32>(ECVF_SetByCommandline)))
    { Finish(TEXT("SDR diagnostic configured LUT dimension changed during capture")); return false; }
    // Direct-to-window FSceneViewport has no game-thread RenderTargetTextureRHI:
    // EnqueueBeginRenderFrame resolves the Slate provider backbuffer on the
    // render thread, then EnqueueEndRenderFrame releases that reference. The
    // shader-resource texture is optional on this path as well.
    const FTextureRHIRef& RT = Viewport->GetRenderTargetTexture();
    const FTextureRHIRef& SRV = Viewport->GetShaderResourceTexture();
    FSceneViewport* SceneViewport = Viewport->AsSceneViewport();
    TSharedPtr<SWindow> Window = SceneViewport ? SceneViewport->FindWindow() : nullptr;
    ISlateViewportProvider* Provider = Window.IsValid() && FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().GetRenderer()->GetViewportProvider(*Window) : nullptr;
    if (!Provider) { Finish(TEXT("Cannot resolve actual Slate viewport backbuffer provider")); return false; }
    EPixelFormat BackbufferFormat = PF_Unknown;
    FIntPoint BackbufferSize = FIntPoint::ZeroValue;
    ENQUEUE_RENDER_COMMAND(RokMountainFSdrBackbufferDescription)(
        [Provider, &BackbufferFormat, &BackbufferSize](FRHICommandListImmediate&)
        {
            if (FRHITexture* Backbuffer = Provider->GetBackBufferResource())
            {
                BackbufferFormat = Backbuffer->GetDesc().Format;
                BackbufferSize = FIntPoint(Backbuffer->GetSizeX(), Backbuffer->GetSizeY());
            }
        });
    FlushRenderingCommands();
    if (BackbufferFormat == PF_Unknown || BackbufferSize.X < 1 || BackbufferSize.Y < 1)
    { Finish(TEXT("Actual Slate viewport RHI backbuffer unavailable")); return false; }
    auto Entry = MakeShared<FJsonObject>();
    Entry->SetArrayField(TEXT("render_target_size"), { MakeShared<FJsonValueNumber>(Viewport->GetRenderTargetTextureSizeXY().X), MakeShared<FJsonValueNumber>(Viewport->GetRenderTargetTextureSizeXY().Y) });
    Entry->SetArrayField(TEXT("constrained_view_rect"), { MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.X), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.Y), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.X), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.Y) });
    Entry->SetBoolField(TEXT("game_thread_render_target_present"), bool(RT));
    Entry->SetBoolField(TEXT("game_thread_shader_resource_present"), bool(SRV));
    Entry->SetStringField(TEXT("render_target_pixel_format"), RT ? GPixelFormats[RT->GetDesc().Format].Name : TEXT("unavailable_direct_to_window"));
    Entry->SetStringField(TEXT("shader_resource_pixel_format"), SRV ? GPixelFormats[SRV->GetDesc().Format].Name : TEXT("unavailable_optional"));
    Entry->SetStringField(TEXT("backbuffer_source"), TEXT("Slate viewport provider, queried on render thread"));
    Entry->SetStringField(TEXT("actual_backbuffer_pixel_format"), GPixelFormats[BackbufferFormat].Name);
    Entry->SetArrayField(TEXT("actual_backbuffer_size"), { MakeShared<FJsonValueNumber>(BackbufferSize.X), MakeShared<FJsonValueNumber>(BackbufferSize.Y) });
    Entry->SetNumberField(TEXT("display_output_format_enum"), static_cast<int32>(Viewport->GetDisplayOutputFormat()));
    Entry->SetNumberField(TEXT("display_gamma"), Viewport->GetDisplayGamma());
    Entry->SetNumberField(TEXT("configured_lut_dimension"), SdrConfiguredLutDimension);
    Entry->SetBoolField(TEXT("scene_hdr_enabled"), Viewport->GetSceneHDREnabled());
    const FEngineShowFlags* Flags = GameViewport->GetEngineShowFlags();
    auto FlagRow = MakeShared<FJsonObject>();
    FlagRow->SetBoolField(TEXT("tonemapper"), Flags->Tonemapper);
    FlagRow->SetBoolField(TEXT("post_processing"), Flags->PostProcessing);
    FlagRow->SetBoolField(TEXT("color_grading"), Flags->ColorGrading);
    FlagRow->SetBoolField(TEXT("anti_aliasing"), Flags->AntiAliasing);
    FlagRow->SetBoolField(TEXT("temporal_aa"), Flags->TemporalAA);
    FlagRow->SetBoolField(TEXT("eye_adaptation"), Flags->EyeAdaptation);
    FlagRow->SetBoolField(TEXT("motion_blur"), Flags->MotionBlur);
    Entry->SetObjectField(TEXT("game_viewport_show_flags"), FlagRow);
    auto Cvars = MakeShared<FJsonObject>();
    for (const TCHAR* Name : { TEXT("r.AntiAliasingMethod"), TEXT("r.ScreenPercentage"),
        TEXT("r.BackbufferQuantizationDitheringOverride"), TEXT("r.LUT.Size"),
        TEXT("r.Color.Grading"), TEXT("r.TonemapperGamma"), TEXT("r.Gamma"),
        TEXT("r.EyeAdaptation.PreExposureOverride"), TEXT("r.PostProcessing.PropagateAlpha"), TEXT("r.ScreenshotDelegate"),
        TEXT("r.DefaultBackBufferPixelFormat") })
    {
        IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, false);
        auto Value = MakeShared<FJsonObject>();
        Value->SetBoolField(TEXT("exists"), Variable != nullptr);
        if (Variable)
        {
            Value->SetStringField(TEXT("value"), Variable->GetString());
            Value->SetNumberField(TEXT("flags"), static_cast<uint32>(Variable->GetFlags()));
        }
        Cvars->SetObjectField(Name, Value);
    }
    Entry->SetObjectField(TEXT("cvars"), Cvars);
    Entry->SetStringField(TEXT("effective_lut_pixel_format"), TEXT("not_captured_private_view_state"));
    State->SetObjectField(TEXT("sdr_viewport"), Entry);
    return true;
}

bool URokMountainFGammaAuditSubsystem::CaptureSdrHdr(TSharedPtr<FJsonObject>& Frame)
{
    auto* SC = SdrHdrCapture.Get(); auto* RT = SdrHdrTarget.Get(); auto* Mesh = Representative.Get();
    auto* PC = GetWorld()->GetFirstPlayerController();
    UCameraComponent* SourceCamera = Camera.IsValid() ? Camera->GetCameraComponent() : nullptr;
    if (!SC || !RT || !Mesh || !PC || !SourceCamera || Mesh->GetForcedLodModel() != 1 ||
        RT->GetFormat() != PF_FloatRGBA || !SdrPreExposureCvar ||
        FMath::Abs(SdrPreExposureCvar->GetFloat() - 1.0f) > .000001f)
    { Finish(TEXT("SDR diagnostic HDR source state changed")); return false; }
    SC->SetWorldLocationAndRotation(SourceCamera->GetComponentLocation(), SourceCamera->GetComponentRotation());
    SC->FOVAngle = SourceCamera->FieldOfView;
    const FStaticMeshRenderData* RenderData = Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetRenderData() : nullptr;
    if (!RenderData || RenderData->LODResources.IsEmpty())
    { Finish(TEXT("Representative F LOD0 unavailable to align HDR projection")); return false; }
    const auto& Positions = RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
    const FVector Origin = SC->GetComponentLocation();
    const FVector Forward = SC->GetForwardVector(), Right = SC->GetRightVector(), Up = SC->GetUpVector();
    const double Scale = SdrEffectiveViewRect.Width() /
        (2.0 * FMath::Tan(FMath::DegreesToRadians(double(SC->FOVAngle)) * .5));
    double MaxDrift = 0;
    int32 Compared = 0;
    for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
    {
        const FVector World = Mesh->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(I)));
        const FVector Relative = World - Origin;
        const double Depth = FVector::DotProduct(Relative, Forward);
        FVector2D Screen;
        if (Depth <= 0 || !PC->ProjectWorldLocationToScreen(World, Screen)) continue;
        const double X = SdrEffectiveViewRect.Min.X + SdrEffectiveViewRect.Width() * .5 + FVector::DotProduct(Relative, Right) * Scale / Depth;
        const double Y = SdrEffectiveViewRect.Min.Y + SdrEffectiveViewRect.Height() * .5 - FVector::DotProduct(Relative, Up) * Scale / Depth;
        if (!FMath::IsFinite(X) || !FMath::IsFinite(Y))
        { Finish(TEXT("Nonfinite HDR projection")); return false; }
        MaxDrift = FMath::Max(MaxDrift, FMath::Max(FMath::Abs(X - Screen.X), FMath::Abs(Y - Screen.Y)));
        ++Compared;
    }
    if (Compared < int32(Positions.GetNumVertices() * .9) || MaxDrift > .25)
    { Finish(FString::Printf(TEXT("HDR/viewport F vertex alignment failed: %d vertices, max %.6f px"), Compared, MaxDrift)); return false; }
    SC->CaptureScene();
    FTextureRenderTargetResource* Resource = RT->GameThread_GetRenderTargetResource();
    TArray<FFloat16Color> Pixels;
    const int32 W = SdrEffectiveViewRect.Width(), H = SdrEffectiveViewRect.Height();
    if (!Resource || !Resource->ReadFloat16Pixels(Pixels, FReadSurfaceDataFlags(RCM_MinMax)) || Pixels.Num() != W * H)
    { Finish(TEXT("Aligned PF_FloatRGBA HDR readback failed")); return false; }
    static_assert(sizeof(FFloat16Color) == 8, "RGBA16F raw layout changed");
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Pixels.Num() * sizeof(FFloat16Color));
    FMemory::Memcpy(Bytes.GetData(), Pixels.GetData(), Bytes.Num());
    const FString HdrPath = Directory / FString::Printf(TEXT("%02d_%s_aligned_scene_color.rgba16f"), Stage, *Names[Stage % 3]);
    if (!FFileHelper::SaveArrayToFile(Bytes, *HdrPath))
    { Finish(TEXT("Aligned HDR raw save failed")); return false; }
    Frame = MakeShared<FJsonObject>();
    Frame->SetStringField(TEXT("path"), HdrPath);
    Frame->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Frame->SetNumberField(TEXT("bytes"), Bytes.Num());
    Frame->SetArrayField(TEXT("size"), { MakeShared<FJsonValueNumber>(W), MakeShared<FJsonValueNumber>(H) });
    Frame->SetArrayField(TEXT("view_rect_offset"), { MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.X), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.Y) });
    Frame->SetNumberField(TEXT("max_projection_drift_pixels"), MaxDrift);
    Frame->SetNumberField(TEXT("compared_vertices"), Compared);
    Frame->SetNumberField(TEXT("camera_horizontal_fov"), SC->FOVAngle);
    Frame->SetArrayField(TEXT("camera_position"), XYZ(Origin));
    Frame->SetStringField(TEXT("capture_source"), TEXT("SCS_SceneColorHDRNoAlpha"));
    Frame->SetStringField(TEXT("target_format"), TEXT("PF_FloatRGBA"));
    Frame->SetBoolField(TEXT("show_only_representative"), SC->ShowOnlyComponents.Num() == 1 && SC->ShowOnlyComponents[0].Get() == Mesh);
    Frame->SetBoolField(TEXT("aa_disabled"), !SC->ShowFlags.AntiAliasing);
    Frame->SetBoolField(TEXT("eye_adaptation_disabled"), !SC->ShowFlags.EyeAdaptation);
    Frame->SetNumberField(TEXT("pre_exposure_override"), SdrPreExposureCvar->GetFloat());
    return true;
}
void URokMountainFGammaAuditSubsystem::SetStageColor()
{
    for (const auto& Body : Bodies) if (auto* MID = Body.AuditMaterial.Get()) MID->SetVectorParameterValue(TEXT("UnitySpriteColor"), Colors[Stage % 3]);
}
bool URokMountainFGammaAuditSubsystem::Inspect(TSharedPtr<FJsonObject>& Out, bool& bFramed)
{
    if (!Camera.IsValid() || !Texture.IsValid() || Bodies.Num() != 18) { Finish(TEXT("Lost F camera/texture/body set")); return false; }
    if (bSdrOutputDiagnostic && bSdrFramingLocked)
    {
        const UCameraComponent* Component = Camera->GetCameraComponent();
        if (!Component || FMath::Abs(Camera->GetCurrentDxf() - SdrLockedDxf) > .01f ||
            FMath::Abs(Component->FieldOfView - SdrLockedFov) > .0001f ||
            !Component->GetComponentTransform().Equals(SdrLockedCameraTransform, .001f))
        { Finish(TEXT("SDR diagnostic source camera moved after framing was frozen")); return false; }
    }
    for (const auto& Body : Bodies)
    {
        auto* Mesh = Body.Mesh.Get(); auto* MID = Body.AuditMaterial.Get(); FLinearColor Actual;
        const bool bExpectedVisible = !bSdrOutputDiagnostic || Mesh == Representative.Get();
        if (!Mesh || !MID || Mesh->GetMaterial(0) != MID || Mesh->GetOwner()->IsHidden() ||
            Mesh->IsVisible() != bExpectedVisible || Mesh->bHiddenInGame == bExpectedVisible ||
            !MID->GetBaseMaterial() || MID->GetBaseMaterial()->GetPathName() != MaterialPath ||
            !MID->GetVectorParameterValue(FMaterialParameterInfo(TEXT("UnitySpriteColor")), Actual) || !Actual.Equals(Colors[Stage % 3], .0001f))
        { Finish(TEXT("F material/color/visibility changed during capture")); return false; }
        TArray<UTexture*> Used; MID->GetUsedTextures(Used);
        if (!Used.Contains(Texture.Get())) { Finish(TEXT("F runtime MID lost dedicated texture")); return false; }
    }
    if (bIsolate) for (const auto& SavedRow : SavedVisibility)
    {
        auto* Mesh = SavedRow.Key.Get();
        if (!Mesh || Mesh->IsVisible() || !Mesh->bHiddenInGame) { Finish(TEXT("F diagnostic isolation changed during capture")); return false; }
    }
    auto* PC = GetWorld()->GetFirstPlayerController(); int32 W = 0, H = 0;
    if (!PC || PC->GetViewTarget() != Camera.Get()) { Finish(TEXT("F camera route changed")); return false; }
    PC->GetViewportSize(W, H);
    if (W != 1440 || H != 810) { Finish(TEXT("F Gamma numeric audit requires actual 1440x810 viewport")); return false; }
    const FVector Focus = Camera->GetCameraComponent()->GetComponentLocation();
    auto* FocusMesh = Representative.Get();
    if (!FocusMesh) { Finish(TEXT("No representative F bounds")); return false; }
    const FBox Box = FocusMesh->Bounds.GetBox();
    FVector2D Lo(DBL_MAX, DBL_MAX), Hi(-DBL_MAX, -DBL_MAX); bFramed = true;
    for (int32 I = 0; I < 8; ++I)
    {
        const FVector P((I & 1) ? Box.Max.X : Box.Min.X, (I & 2) ? Box.Max.Y : Box.Min.Y, (I & 4) ? Box.Max.Z : Box.Min.Z);
        FVector2D Screen;
        if (!PC->ProjectWorldLocationToScreen(P, Screen)) { bFramed = false; continue; }
        Lo.X = FMath::Min(Lo.X, Screen.X); Lo.Y = FMath::Min(Lo.Y, Screen.Y); Hi.X = FMath::Max(Hi.X, Screen.X); Hi.Y = FMath::Max(Hi.Y, Screen.Y);
    }
    bFramed = bFramed && Lo.X > W * .05 && Hi.X < W * .95 && Lo.Y > H * .08 && Hi.Y < H * (bIsolate ? .85 : .92) &&
        (Hi.X - Lo.X) > 80 && (Hi.Y - Lo.Y) > 60;
    const auto* RenderData = FocusMesh->GetStaticMesh()->GetRenderData();
    if (!RenderData || RenderData->LODResources.IsEmpty() || RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() < 1)
    { Finish(TEXT("F body missing source mesh UV data")); return false; }
    const auto& Lod = RenderData->LODResources[0];
    const auto& Positions = Lod.VertexBuffers.PositionVertexBuffer;
    const int32 IndexCount = Lod.IndexBuffer.GetNumIndices();
    if (Positions.GetNumVertices() < 100 || IndexCount < 3000 || IndexCount % 3 != 0)
    { Finish(TEXT("F actual LOD0 vertex/triangle data invalid")); return false; }
    TArray<TSharedPtr<FJsonValue>> ScreenVertices, Triangles;
    ScreenVertices.Reserve(Positions.GetNumVertices()); Triangles.Reserve(IndexCount / 3);
    const FVector CameraForward = Camera->GetCameraComponent()->GetForwardVector();
    int32 ValidVertices = 0;
    for (uint32 I = 0; I < Positions.GetNumVertices(); ++I)
    {
        const FVector WorldVertex = FocusMesh->GetComponentTransform().TransformPosition(FVector(Positions.VertexPosition(I)));
        const double Depth = FVector::DotProduct(WorldVertex - Focus, CameraForward);
        FVector2D Screen = FVector2D::ZeroVector;
        const bool bValid = Depth > 0 && FMath::IsFinite(Depth) && PC->ProjectWorldLocationToScreen(WorldVertex, Screen) &&
            FMath::IsFinite(Screen.X) && FMath::IsFinite(Screen.Y);
        if (bValid) ++ValidVertices;
        ScreenVertices.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(bValid ? Screen.X : 0.0), MakeShared<FJsonValueNumber>(bValid ? Screen.Y : 0.0),
            MakeShared<FJsonValueNumber>(bValid ? 1.0 : 0.0), MakeShared<FJsonValueNumber>(FMath::IsFinite(Depth) ? Depth : 0.0) }));
    }
    for (int32 I = 0; I < IndexCount; I += 3)
    {
        const uint32 A = Lod.IndexBuffer.GetIndex(I), B = Lod.IndexBuffer.GetIndex(I + 1), C = Lod.IndexBuffer.GetIndex(I + 2);
        if (A >= Positions.GetNumVertices() || B >= Positions.GetNumVertices() || C >= Positions.GetNumVertices())
        { Finish(TEXT("F actual LOD0 triangle index out of range")); return false; }
        Triangles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(A), MakeShared<FJsonValueNumber>(B), MakeShared<FJsonValueNumber>(C) }));
    }
    if (ValidVertices < int32(Positions.GetNumVertices() * .9))
    { Finish(TEXT("F representative LOD0 projection has too many invalid vertices")); return false; }
    auto Projection = MakeShared<FJsonObject>();
    Projection->SetStringField(TEXT("component"), FocusMesh->GetPathName());
    Projection->SetStringField(TEXT("mesh"), FocusMesh->GetStaticMesh()->GetPathName());
    Projection->SetArrayField(TEXT("viewport_size"), { MakeShared<FJsonValueNumber>(W), MakeShared<FJsonValueNumber>(H) });
    Projection->SetNumberField(TEXT("vertex_count"), Positions.GetNumVertices());
    Projection->SetNumberField(TEXT("valid_vertex_count"), ValidVertices);
    Projection->SetNumberField(TEXT("triangle_count"), IndexCount / 3);
    Projection->SetArrayField(TEXT("lod0_vertices"), ScreenVertices);
    Projection->SetArrayField(TEXT("lod0_triangles"), Triangles);
    const FTransform ComponentTransform = FocusMesh->GetComponentTransform();
    const FRotator ComponentRotation = ComponentTransform.GetRotation().Rotator();
    auto TransformRecord = MakeShared<FJsonObject>();
    TransformRecord->SetArrayField(TEXT("location"), XYZ(ComponentTransform.GetLocation()));
    TransformRecord->SetArrayField(TEXT("rotation"), XYZ(FVector(ComponentRotation.Pitch, ComponentRotation.Yaw, ComponentRotation.Roll)));
    TransformRecord->SetArrayField(TEXT("scale"), XYZ(ComponentTransform.GetScale3D()));
    Projection->SetObjectField(TEXT("component_transform"), TransformRecord);
    Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("representative_component"), FocusMesh->GetPathName());
    Out->SetStringField(TEXT("representative_mesh"), FocusMesh->GetStaticMesh()->GetPathName());
    Out->SetArrayField(TEXT("screen_rect"), { MakeShared<FJsonValueNumber>(Lo.X), MakeShared<FJsonValueNumber>(Lo.Y), MakeShared<FJsonValueNumber>(Hi.X), MakeShared<FJsonValueNumber>(Hi.Y) });
    Out->SetArrayField(TEXT("bounds_min"), XYZ(Box.Min)); Out->SetArrayField(TEXT("bounds_max"), XYZ(Box.Max));
    Out->SetArrayField(TEXT("camera_position"), XYZ(Focus)); Out->SetArrayField(TEXT("actual_sprite_color"), XYZ(FVector(Colors[Stage % 3].R, Colors[Stage % 3].G, Colors[Stage % 3].B)));
    Out->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf()); Out->SetNumberField(TEXT("frame"), double(GFrameCounter));
    Out->SetNumberField(TEXT("elapsed"), Elapsed); Out->SetNumberField(TEXT("uv_channels"), RenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
    Out->SetNumberField(TEXT("vertices"), RenderData->LODResources[0].GetNumVertices());
    Out->SetObjectField(TEXT("mesh_projection"), Projection);
    auto Cvars = MakeShared<FJsonObject>();
    for (const TCHAR* Name : { TEXT("r.AntiAliasingMethod"), TEXT("r.PostProcessAAQuality"),
        TEXT("r.ScreenPercentage"), TEXT("r.TonemapperGamma"), TEXT("r.Tonemapper.GrainQuantization"),
        TEXT("sg.AntiAliasingQuality"), TEXT("r.BackbufferQuantizationDitheringOverride"), TEXT("r.LUT.Size") })
    {
        auto Entry = MakeShared<FJsonObject>();
        IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name, false);
        Entry->SetBoolField(TEXT("exists"), Variable != nullptr);
        if (Variable)
        {
            Entry->SetStringField(TEXT("value"), Variable->GetString());
            Entry->SetNumberField(TEXT("flags"), static_cast<uint32>(Variable->GetFlags()));
        }
        else Entry->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
        Cvars->SetObjectField(Name, Entry);
    }
    auto RenderInputs = MakeShared<FJsonObject>();
    RenderInputs->SetStringField(TEXT("sample_point"), TEXT("game_thread_before_screenshot_request"));
    RenderInputs->SetObjectField(TEXT("console_variables"), Cvars);
    const UCameraComponent* CameraComponent = Camera->GetCameraComponent();
    const FPostProcessSettings& PP = CameraComponent->PostProcessSettings;
    auto CameraPP = MakeShared<FJsonObject>();
    CameraPP->SetNumberField(TEXT("blend_weight"), CameraComponent->PostProcessBlendWeight);
    CameraPP->SetBoolField(TEXT("override_auto_exposure_method"), PP.bOverride_AutoExposureMethod);
    CameraPP->SetNumberField(TEXT("auto_exposure_method_enum"), PP.AutoExposureMethod.GetValue());
    CameraPP->SetBoolField(TEXT("override_auto_exposure_bias"), PP.bOverride_AutoExposureBias);
    CameraPP->SetNumberField(TEXT("auto_exposure_bias"), PP.AutoExposureBias);
    CameraPP->SetBoolField(TEXT("override_physical_camera_exposure"), PP.bOverride_AutoExposureApplyPhysicalCameraExposure);
    CameraPP->SetBoolField(TEXT("physical_camera_exposure"), PP.AutoExposureApplyPhysicalCameraExposure);
    CameraPP->SetBoolField(TEXT("override_tone_curve_amount"), PP.bOverride_ToneCurveAmount);
    CameraPP->SetNumberField(TEXT("tone_curve_amount"), PP.ToneCurveAmount);
    CameraPP->SetBoolField(TEXT("override_blue_correction"), PP.bOverride_BlueCorrection);
    CameraPP->SetNumberField(TEXT("blue_correction"), PP.BlueCorrection);
    CameraPP->SetBoolField(TEXT("override_expand_gamut"), PP.bOverride_ExpandGamut);
    CameraPP->SetNumberField(TEXT("expand_gamut"), PP.ExpandGamut);
    CameraPP->SetBoolField(TEXT("override_film_slope"), PP.bOverride_FilmSlope);
    CameraPP->SetNumberField(TEXT("film_slope"), PP.FilmSlope);
    CameraPP->SetBoolField(TEXT("override_film_toe"), PP.bOverride_FilmToe);
    CameraPP->SetNumberField(TEXT("film_toe"), PP.FilmToe);
    CameraPP->SetBoolField(TEXT("override_film_shoulder"), PP.bOverride_FilmShoulder);
    CameraPP->SetNumberField(TEXT("film_shoulder"), PP.FilmShoulder);
    CameraPP->SetBoolField(TEXT("override_film_black_clip"), PP.bOverride_FilmBlackClip);
    CameraPP->SetNumberField(TEXT("film_black_clip"), PP.FilmBlackClip);
    CameraPP->SetBoolField(TEXT("override_film_white_clip"), PP.bOverride_FilmWhiteClip);
    CameraPP->SetNumberField(TEXT("film_white_clip"), PP.FilmWhiteClip);
    CameraPP->SetBoolField(TEXT("override_color_gamma"), PP.bOverride_ColorGamma);
    CameraPP->SetArrayField(TEXT("color_gamma"), { MakeShared<FJsonValueNumber>(PP.ColorGamma.X), MakeShared<FJsonValueNumber>(PP.ColorGamma.Y),
        MakeShared<FJsonValueNumber>(PP.ColorGamma.Z), MakeShared<FJsonValueNumber>(PP.ColorGamma.W) });
    CameraPP->SetBoolField(TEXT("override_color_gain"), PP.bOverride_ColorGain);
    CameraPP->SetArrayField(TEXT("color_gain"), { MakeShared<FJsonValueNumber>(PP.ColorGain.X), MakeShared<FJsonValueNumber>(PP.ColorGain.Y),
        MakeShared<FJsonValueNumber>(PP.ColorGain.Z), MakeShared<FJsonValueNumber>(PP.ColorGain.W) });
    RenderInputs->SetObjectField(TEXT("camera_post_process"), CameraPP);
    Out->SetObjectField(TEXT("render_inputs"), RenderInputs);
    if (bSdrOutputDiagnostic && !RecordSdrViewport(Out)) return false;
    return true;
}
void URokMountainFGammaAuditSubsystem::Tick(float DeltaTime)
{
    if (bFinished || !GetWorld()->HasBegunPlay()) return;
    Elapsed += FMath::Max(0.f, DeltaTime);
    if (Elapsed > (bSdrOutputDiagnostic ? 90 : 45))
    {
        Finish(Pending && bSdrOutputDiagnostic
            ? TEXT("F screenshot or packed GPU readback did not complete before 90-second limit")
            : TEXT("F runtime audit timeout"));
        return;
    }
    if (Stage < 0) { if (Elapsed > .75) Begin(); return; }
    if (Pending && bSdrOutputDiagnostic)
    {
        const auto Capture = SdrRawReadback;
        if (Capture)
        {
            FRokMountainFGpuPackedCapture::EStatus Status;
            FString Failure;
            {
                FScopeLock Lock(&Capture->Mutex);
                Status = Capture->Status;
                Failure = Capture->Failure;
            }
            if (Status == FRokMountainFGpuPackedCapture::EStatus::Failed)
            { Finish(FString::Printf(TEXT("Packed GPU backbuffer readback failed: %s"), *Failure)); return; }
            if (Status == FRokMountainFGpuPackedCapture::EStatus::Ready && bSdrScreenshotProcessed)
            {
                if (StoreSdrRawReadback()) CompleteCapture();
                return;
            }
            if (Status == FRokMountainFGpuPackedCapture::EStatus::CopySubmitted) PollSdrRawReadback();
        }
        else if (bSdrScreenshotProcessed)
        { Finish(TEXT("Processed F screenshot has no matching GPU backbuffer capture")); return; }
        return;
    }
    if (Pending || Elapsed - StageStart < .5 || GFrameCounter < StageFrame + 3) return;
    TSharedPtr<FJsonObject> State; bool bFramed = false;
    if (!Inspect(State, bFramed)) return;
    if (!bFramed)
    {
        if (Stage != 0 || Camera->GetCurrentDxf() > 8000) { Finish(TEXT("F body does not fit source-camera viewport")); return; }
        Camera->SetCurrentDxf(Camera->GetCurrentDxf() * 1.15f);
        if (bSdrOutputDiagnostic) ++SdrFramingSteps;
        StageStart = Elapsed; StageFrame = GFrameCounter; return;
    }
    if (bSdrOutputDiagnostic && !bSdrFramingLocked)
    {
        const UCameraComponent* Component = Camera->GetCameraComponent();
        if (Stage != 0 || !Component) { Finish(TEXT("SDR diagnostic could not freeze first framed source camera")); return; }
        SdrLockedDxf = Camera->GetCurrentDxf();
        SdrLockedFov = Component->FieldOfView;
        SdrLockedCameraTransform = Component->GetComponentTransform();
        bSdrFramingLocked = true;
    }
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Competing F screenshot writer")); return; }
    PendingPath = Directory / (bSdrOutputDiagnostic
        ? FString::Printf(TEXT("%02d_%s_%s.png"), Stage, Stage < 3 ? TEXT("normal") : TEXT("gamma_only"), *Names[Stage % 3])
        : FString::Printf(TEXT("%02d_%s.png"), Stage, *Names[Stage]));
    Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("name"), Names[Stage % 3]); Pending->SetStringField(TEXT("path"), PendingPath);
    Pending->SetObjectField(TEXT("at_request"), State);
    if (bSdrOutputDiagnostic)
    {
        bSdrScreenshotProcessed = false;
        SdrRawRequestGameFrame = GFrameCounter;
        SdrRawRequestSeconds = FPlatformTime::Seconds();
        Pending->SetStringField(TEXT("output_mode"), Stage < 3 ? TEXT("normal") : TEXT("gamma_only"));
        Pending->SetBoolField(TEXT("screenshot_show_ui"), false);
        TSharedPtr<FJsonObject> Hdr;
        if (!CaptureSdrHdr(Hdr)) return;
        Pending->SetObjectField(TEXT("aligned_scene_color"), Hdr);
    }
    FScreenshotRequest::RequestScreenshot(PendingPath, !bSdrOutputDiagnostic, false, false, FIntRect(), true);
}
void URokMountainFGammaAuditSubsystem::CapturedRaw(int32 Width, int32 Height, const TArray<FColor>& Pixels)
{
    if (!bSdrOutputDiagnostic || bFinished || !Pending) return;
    if (Pending->HasField(TEXT("pre_png_fcolor")) || !SdrRawReadback ||
        SdrRawReadback->Stage != Stage || SdrRawReadback->Serial != SdrRawCaptureSerial ||
        SdrRawReadback->ViewportGameFrame != GFrameCounter)
    { Finish(TEXT("FColor callback duplicated or differs from GPU viewport frame/stage")); return; }
    if (Width != 1440 || Height != 810 || Pixels.Num() != Width * Height)
    { Finish(TEXT("Pre-PNG viewport FColor dimensions changed")); return; }
    static_assert(sizeof(FColor) == 4, "BGRA8 raw FColor layout changed");
    TArray<uint8> Bytes;
    Bytes.SetNumUninitialized(Pixels.Num() * sizeof(FColor));
    FMemory::Memcpy(Bytes.GetData(), Pixels.GetData(), Bytes.Num());
    const FString RawPath = FPaths::ChangeExtension(PendingPath, TEXT("bgra8"));
    if (!FFileHelper::SaveArrayToFile(Bytes, *RawPath))
    { Finish(TEXT("Pre-PNG BGRA8 save failed")); return; }
    auto Raw = MakeShared<FJsonObject>();
    Raw->SetStringField(TEXT("path"), RawPath);
    Raw->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Raw->SetNumberField(TEXT("bytes"), Bytes.Num());
    Raw->SetStringField(TEXT("layout"), TEXT("FColor BGRA8 row-major; alpha forced opaque by GameViewportClient delegate path"));
    Pending->SetObjectField(TEXT("pre_png_fcolor"), Raw);
    FImageView Image(Pixels.GetData(), Width, Height);
    if (!FImageUtils::SaveImageByExtension(*PendingPath, Image))
    { Finish(TEXT("Pre-PNG FColor could not be encoded using engine ImageUtils")); return; }
}
void URokMountainFGammaAuditSubsystem::Processed()
{
    if (bFinished || !Pending) return;
    TSharedPtr<FJsonObject> State; bool bFramed = false;
    if (!Inspect(State, bFramed)) return;
    if (!bFramed) { Finish(TEXT("F screenshot callback lost framing")); return; }
    TArray<uint8> Bytes; const uint8 Signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0)
    { Finish(TEXT("Missing processed real F PNG")); return; }
    if (bSdrOutputDiagnostic && !Pending->HasField(TEXT("pre_png_fcolor")))
    { Finish(TEXT("Pre-PNG screenshot delegate did not fire")); return; }
    Pending->SetObjectField(TEXT("at_processed"), State); Pending->SetNumberField(TEXT("bytes"), Bytes.Num());
    Pending->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
    if (bSdrOutputDiagnostic)
    {
        if (bSdrScreenshotProcessed || !SdrRawReadback)
        { Finish(TEXT("Duplicate processed screenshot or missing same-frame GPU copy")); return; }
        if (SdrRawReadback->Stage != Stage || SdrRawReadback->Serial != SdrRawCaptureSerial ||
            SdrRawReadback->ViewportGameFrame != GFrameCounter)
        { Finish(TEXT("Processed screenshot differs from GPU viewport frame/stage")); return; }
        Pending->SetNumberField(TEXT("screenshot_processed_game_frame"), static_cast<double>(GFrameCounter));
        Pending->SetNumberField(TEXT("screenshot_processed_platform_seconds"), FPlatformTime::Seconds());
        bSdrScreenshotProcessed = true;
        return;
    }
    CompleteCapture();
}
void URokMountainFGammaAuditSubsystem::CompleteCapture()
{
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
    bSdrScreenshotProcessed = false;
    if (++Stage == (bSdrOutputDiagnostic ? 6 : 3)) { Finish(); return; }
    if (bSdrOutputDiagnostic && Stage == 3)
    {
        auto* Viewport = GetWorld()->GetGameViewport();
        if (!Viewport || !Viewport->GetEngineShowFlags() || !Viewport->GetEngineShowFlags()->Tonemapper)
        { Finish(TEXT("Cannot enter GammaOnly from normal tonemapper state")); return; }
        Viewport->GetEngineShowFlags()->SetTonemapper(false);
    }
    StageStart = Elapsed; StageFrame = GFrameCounter; SetStageColor();
}
bool URokMountainFGammaAuditSubsystem::RestoreRuntimeState()
{
    bool Okay = true;
    if (bSdrOutputDiagnostic)
    {
        if (SdrViewportRenderedHandle.IsValid())
        {
            UGameViewportClient::OnViewportRendered().Remove(SdrViewportRenderedHandle);
            SdrViewportRenderedHandle.Reset();
        }
        if (SdrRawReadback || SdrViewportOwner.IsValid())
        {
            // Earlier copy/poll commands run first. UE's Landscape readback
            // cancellation also Lock/Unlocks before releasing staging; Lock
            // waits for this copy's fence if the GPU has not finished yet.
            const auto CancelledCapture = SdrRawReadback;
            ENQUEUE_RENDER_COMMAND(RokMountainFPackedBackbufferCancel)(
                [CancelledCapture](FRHICommandListImmediate&)
                {
                    if (CancelledCapture)
                    {
                        FScopeLock Lock(&CancelledCapture->Mutex);
                        if (CancelledCapture->Readback)
                        {
                            int32 UnusedRowPitch = 0, UnusedHeight = 0;
                            if (CancelledCapture->Readback->Lock(UnusedRowPitch, &UnusedHeight))
                                CancelledCapture->Readback->Unlock();
                            CancelledCapture->Readback.Reset();
                        }
                    }
                });
            // This drains borrowed FSceneViewport use before its GT owner drops.
            // It is a render-command fence, not a success-path GPU idle/wait.
            FlushRenderingCommands();
            SdrRawReadback.Reset();
            SdrViewportOwner.Reset();
        }
        if (RawScreenshotHandle.IsValid())
        {
            UGameViewportClient::OnScreenshotCaptured().Remove(RawScreenshotHandle);
            RawScreenshotHandle.Reset();
        }
        if (auto* Capture = SdrHdrCapture.Get())
        {
            Capture->UnregisterComponent();
            Capture->TextureTarget = nullptr;
        }
        SdrHdrCapture.Reset();
        SdrHdrTarget.Reset();
        if (bSdrPreExposureChanged)
        {
            if (SdrPreExposureCvar)
            {
                SdrPreExposureCvar->SetWithCurrentPriority(SdrSavedPreExposure, NAME_None, ECVF_SetByConsole, ECVF_SetByConstructor);
                Okay &= FMath::Abs(SdrPreExposureCvar->GetFloat() - SdrSavedPreExposure) < .000001f;
            }
            else Okay = false;
            bSdrPreExposureChanged = false;
        }
        if (bSdrLodSaved)
        {
            if (auto* Mesh = Representative.Get())
            {
                Mesh->SetForcedLodModel(SdrSavedForcedLod);
                Okay &= Mesh->GetForcedLodModel() == SdrSavedForcedLod;
            }
            else Okay = false;
            bSdrLodSaved = false;
        }
        if (bSdrTonemapperSaved)
        {
            auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
            if (Viewport && Viewport->GetEngineShowFlags())
            {
                Viewport->GetEngineShowFlags()->SetTonemapper(bSdrOriginalTonemapper);
                Okay &= Viewport->GetEngineShowFlags()->Tonemapper == bSdrOriginalTonemapper;
            }
            else Okay = false;
            bSdrTonemapperSaved = false;
        }
        if (bSdrViewportFlagsSaved)
        {
            auto* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
            if (Viewport && Viewport->GetEngineShowFlags())
            {
                FEngineShowFlags* Flags = Viewport->GetEngineShowFlags();
                Flags->SetAntiAliasing(bSdrOriginalAntiAliasing);
                Flags->SetTemporalAA(bSdrOriginalTemporalAA);
                Flags->SetEyeAdaptation(bSdrOriginalEyeAdaptation);
                Flags->SetMotionBlur(bSdrOriginalMotionBlur);
                Okay &= Flags->AntiAliasing == bSdrOriginalAntiAliasing && Flags->TemporalAA == bSdrOriginalTemporalAA &&
                    Flags->EyeAdaptation == bSdrOriginalEyeAdaptation && Flags->MotionBlur == bSdrOriginalMotionBlur;
            }
            else Okay = false;
            bSdrViewportFlagsSaved = false;
        }
    }
    for (auto& Body : Bodies)
    {
        auto* Mesh = Body.Mesh.Get(); auto* Original = Body.OriginalMaterial.Get();
        if (!Mesh || !Original) { Okay = false; continue; }
        Mesh->SetMaterial(0, Original); Okay &= Mesh->GetMaterial(0) == Original;
    }
    Bodies.Reset();
    for (const auto& SavedRow : SavedVisibility)
    {
        auto* Mesh = SavedRow.Key.Get(); if (!Mesh) { Okay = false; continue; }
        Mesh->SetVisibility(SavedRow.Value.X != 0, false); Mesh->SetHiddenInGame(SavedRow.Value.Y != 0, false);
        Okay &= Mesh->IsVisible() == (SavedRow.Value.X != 0) && Mesh->bHiddenInGame == (SavedRow.Value.Y != 0);
    }
    SavedVisibility.Reset();
    if (bCameraSaved && Camera.IsValid())
    {
        Camera->SetViewCenterUnity(SavedViewCenter); Camera->SetCurrentDxf(SavedDxf);
        Okay &= FVector2D::Distance(Camera->GetViewCenterUnity(), SavedViewCenter) < .01 && FMath::Abs(Camera->GetCurrentDxf() - SavedDxf) < .01;
        bCameraSaved = false;
    }
    return Okay;
}
void URokMountainFGammaAuditSubsystem::Finish(const FString& Failure)
{
    if (bFinished) return;
    bFinished = true;
    if (!Failure.IsEmpty()) Failures.Add(Failure);
    const bool bRestored = RestoreRuntimeState();
    if (!bRestored) Failures.Add(TEXT("F temporary material/visibility/camera/SDR diagnostic state restoration failed"));
    auto Report = MakeShared<FJsonObject>();
    const int32 ExpectedCaptures = bSdrOutputDiagnostic ? 6 : 3;
    const bool Passed = Failures.IsEmpty() && Captures.Num() == ExpectedCaptures && BindingRecords.Num() == 18 && MipRecords.Num() == 11;
    Report->SetBoolField(TEXT("passed"), Passed);
    Report->SetStringField(TEXT("scope"), bSdrOutputDiagnostic
        ? TEXT("Scoped six-frame source-camera representative-only F SDR output-chain diagnostic: normal tonemapper vs Tonemapper=false GammaOnly, aligned scene-color HDR, original packed 10-bit GPU viewport backbuffer, post-ReadSurfaceData pre-PNG FColor and PNG. All 18 source bodies are verified before temporary isolation. GammaOnly is not Unity final SDR presentation or normal-world visual acceptance. No material, map, or asset save.")
        : TEXT("Fresh 18 F-body bindings and all 11 decoded platform mip byte hashes, plus three actual source-color world screenshots. Diagnostic isolation, when requested, is not normal-world visual acceptance. No F-edge/A/Grove geometry or map save."));
    Report->SetBoolField(TEXT("diagnostic_isolate"), bIsolate); Report->SetBoolField(TEXT("temporary_state_restored"), bRestored);
    if (bSdrOutputDiagnostic)
    {
        Report->SetBoolField(TEXT("sdr_output_diagnostic"), true);
        Report->SetBoolField(TEXT("gpu_packed_readback_diagnostic"), true);
        Report->SetNumberField(TEXT("sdr_configured_lut_dimension"), SdrConfiguredLutDimension);
        Report->SetStringField(TEXT("representative_source_key"), SdrRepresentativeKey);
        TArray<TSharedPtr<FJsonValue>> HiddenFKeys;
        for (const FString& Key : SdrHiddenBodyKeys) HiddenFKeys.Add(MakeShared<FJsonValueString>(Key));
        Report->SetArrayField(TEXT("temporarily_hidden_f_source_keys"), HiddenFKeys);
        auto Framing = MakeShared<FJsonObject>();
        Framing->SetStringField(TEXT("method"), TEXT("original F Gamma audit Stage0: start dxf 1509, multiply 1.15 only before first framed screenshot"));
        Framing->SetNumberField(TEXT("initial_dxf"), SdrFramingInitialDxf);
        Framing->SetNumberField(TEXT("pre_capture_steps"), SdrFramingSteps);
        Framing->SetBoolField(TEXT("locked_before_first_capture"), bSdrFramingLocked);
        Framing->SetNumberField(TEXT("locked_dxf"), SdrLockedDxf);
        Framing->SetNumberField(TEXT("locked_horizontal_fov"), SdrLockedFov);
        Framing->SetArrayField(TEXT("locked_camera_position"), XYZ(SdrLockedCameraTransform.GetLocation()));
        Report->SetObjectField(TEXT("source_camera_framing"), Framing);
        Report->SetArrayField(TEXT("effective_view_rect"), { MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.X),
            MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Min.Y), MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.X),
            MakeShared<FJsonValueNumber>(SdrEffectiveViewRect.Max.Y) });
    }
    Report->SetArrayField(TEXT("temporarily_hidden_components"), VisibilityRecords);
    Report->SetArrayField(TEXT("fresh_f_bodies"), BindingRecords); Report->SetArrayField(TEXT("platform_mips"), MipRecords);
    if (TextureState.IsValid()) Report->SetObjectField(TEXT("texture_state"), TextureState);
    Report->SetArrayField(TEXT("captures"), Captures);
    Report->SetStringField(TEXT("product_manifest_sha256"), ProductManifestSha); Report->SetStringField(TEXT("source_data_sha256"), SourceDataSha);
    Report->SetStringField(TEXT("source_native_render_sha256"), SourceRenderSha); Report->SetStringField(TEXT("v2_source_verify_sha256"), SourceVerifySha);
    Report->SetStringField(TEXT("texture"), TexturePath); Report->SetStringField(TEXT("material"), MaterialPath);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& E : Failures) Errors.Add(MakeShared<FJsonValueString>(E));
    Report->SetArrayField(TEXT("failures"), Errors);
    FString Text;
    if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ||
        !FFileHelper::SaveStringToFile(Text, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        Failures.Add(TEXT("F audit manifest save failed"));
    UE_LOG(LogTemp, Display, TEXT("Mountain F Gamma audit passed=%d captures=%d bodies=%d mips=%d output=%s failures=%s"),
        Passed, Captures.Num(), BindingRecords.Num(), MipRecords.Num(), *Directory, *FString::Join(Failures, TEXT("; ")));
    FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
}
