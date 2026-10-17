#include "RokGroveCenterScaleAuditSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "RokTrainArmyMeshData.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"
#include "UnrealClient.h"

namespace
{
const TCHAR* Prefab = TEXT("Grove_co_tree_01_adorning");
const TCHAR* PrefabTag = TEXT("UnityPrefab_Grove_co_tree_01_adorning");
const TCHAR* MeshPath = TEXT("/Game/RokUnityWorldMapPrototype/Meshes/grove/StaticMeshes/SM_RokGroveCenterScale.SM_RokGroveCenterScale");
const TCHAR* MaterialPath = TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_GroveCenterScale.M_WorldSource_GroveCenterScale");
constexpr float Sequence[] = {480, 900, 1509, 3000, 4200, 7000, 4200, 1509, 480};
bool IsGroveTargetWorld(const UWorld* World)
{ return World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityWorldMapPrototype"); }
FString InputPath(const TCHAR* Name) { return FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/GroveCenterScale") / Name; }
bool Number(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, double& N)
{
    const auto V = O ? O->TryGetField(Key) : nullptr;
    return V && V->Type == EJson::Number && V->TryGetNumber(N) && FMath::IsFinite(N);
}
bool Array(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, int32 Count, const TArray<TSharedPtr<FJsonValue>>*& Values)
{ return O && O->TryGetArrayField(Key, Values) && Values->Num() == Count; }
bool Numbers(const TSharedPtr<FJsonValue>& Value, int32 Count, TArray<double>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value || !Value->TryGetArray(Values) || Values->Num() != Count) return false;
    for (const auto& V : *Values) { double N; if (!V || V->Type != EJson::Number || !V->TryGetNumber(N) || !FMath::IsFinite(N)) return false; Out.Add(N); }
    return true;
}
TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonValue>& V)
{ return V && V->Type == EJson::Object ? V->AsObject() : nullptr; }
TSharedPtr<FJsonObject> Child(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
{ const TSharedPtr<FJsonObject>* Value = nullptr; return O && O->TryGetObjectField(Key, Value) ? *Value : nullptr; }
bool XYZ(const TSharedPtr<FJsonObject>& O, FVector& V)
{ return Number(O, TEXT("x"), V.X) && Number(O, TEXT("y"), V.Y) && Number(O, TEXT("z"), V.Z); }
TArray<TSharedPtr<FJsonValue>> Values(std::initializer_list<double> Numbers)
{ TArray<TSharedPtr<FJsonValue>> Out; for (double N : Numbers) Out.Add(MakeShared<FJsonValueNumber>(N)); return Out; }
TArray<TSharedPtr<FJsonValue>> Vector(FVector V) { return Values({V.X, V.Y, V.Z}); }
FString Hash(const FString& Path)
{ TArray<uint8> Bytes; return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString(); }
bool Load(const FString& Path, TSharedPtr<FJsonObject>& Out)
{ FString Text; return FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid(); }
bool Visible(const UStaticMeshComponent* Mesh)
{
    if (!Mesh || !Mesh->IsRegistered() || !Mesh->GetOwner() || Mesh->GetOwner()->IsHidden()) return false;
    for (const USceneComponent* P = Mesh; P; P = P->GetAttachParent()) if (!P->IsVisible() || P->bHiddenInGame) return false;
    return true;
}
FIntVector CanonicalTriangle(FIntVector V)
{
    // Preserve winding while allowing the importer to rotate a triangle's first vertex.
    if (V.Y < V.X && V.Y < V.Z) return FIntVector(V.Y, V.Z, V.X);
    if (V.Z < V.X && V.Z < V.Y) return FIntVector(V.Z, V.X, V.Y);
    return V;
}
}

bool URokGroveCenterScaleAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    // Startup /Temp/Untitled_0 is also Game; it must never own this global capture/exit flow.
    return IsGroveTargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(), TEXT("RokGroveCenterScaleAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokGroveCenterScaleAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokGroveCenterScaleAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokGroveCenterScaleAuditSubsystem, STATGROUP_Tickables); }
void URokGroveCenterScaleAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsTargetWorld = IsGroveTargetWorld(GetWorld());
    if (!bOwnsTargetWorld) return;
    WallStarted = FPlatformTime::Seconds();
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/GroveCenterScaleAudit") / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokGroveCenterScaleAuditSubsystem::Processed);
}
void URokGroveCenterScaleAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    // Capture ownership at initialize: target-world teardown before the first tick still fails.
    if (bOwnsTargetWorld && !bFinished) Finish(TEXT("Target world teardown before nine processed screenshots"));
    RestoreVisibility(); Super::Deinitialize();
}
bool URokGroveCenterScaleAuditSubsystem::Identity()
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
    if (!bOwnsTargetWorld || !IsGroveTargetWorld(GetWorld()) || !FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")))
    { Finish(TEXT("Wrong project/map; runtime geometry and camera must remain untouched")); return false; }
    if (GetWorld()->IsPaused() || !GetWorld()->GetWorldSettings() || !FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), 1.f, .000001f))
    { Finish(TEXT("Grove audit requires an unpaused world at normal time dilation")); return false; }
    return true;
}
bool URokGroveCenterScaleAuditSubsystem::LoadSource()
{
    if (!Load(InputPath(TEXT("manifest.json")), Source) || !Load(InputPath(TEXT("native_render.json")), Native)) return false;
    SourceHash = Hash(InputPath(TEXT("manifest.json"))); NativeHash = Hash(InputPath(TEXT("native_render.json")));
    FString S; double Bake = 0;
    if (!Source->TryGetStringField(TEXT("schema"), S) || S != TEXT("rok.grove_center_scale.v1") || !Source->TryGetStringField(TEXT("prefab"), S) || S != Prefab
        || !Native->TryGetStringField(TEXT("unityVersion"), S) || S != TEXT("2019.4.28f1c1") || !Native->TryGetStringField(TEXT("shader"), S) || S != TEXT("custom/landmap/tree-16-nomask")
        || !Native->TryGetStringField(TEXT("colorSpace"), S) || S != TEXT("Gamma") || !Number(Source, TEXT("bake_scale"), Bake) || Bake <= 0 || Bake > 10) return false;
    const TArray<TSharedPtr<FJsonValue>> *Files = nullptr, *P = nullptr, *UV = nullptr, *C = nullptr, *NP = nullptr, *NUV = nullptr, *NC = nullptr, *ST = nullptr, *NT = nullptr;
    if (!Array(Source, TEXT("source_files"), 6, Files) || !Array(Source, TEXT("positions"), 377, P) || !Array(Source, TEXT("uv0"), 377, UV) || !Array(Source, TEXT("source_color_rgba"), 377, C)
        || !Array(Native, TEXT("positions"), 377, NP) || !Array(Native, TEXT("uv0"), 377, NUV) || !Array(Native, TEXT("colors"), 377, NC)
        || !Array(Source, TEXT("triangles"), 309, ST) || !Array(Native, TEXT("triangles"), 927, NT)) return false;
    TSet<FString> SourceFiles;
    for (const auto& V : *Files)
    {
        const auto O = Object(V); FString Path, Expected;
        if (!O || !O->TryGetStringField(TEXT("path"), Path) || !Path.StartsWith(TEXT("ref/")) || Path.Contains(TEXT("..")) || SourceFiles.Contains(Path)
            || !O->TryGetStringField(TEXT("sha256"), Expected) || Expected.Len() != 64 || Hash(FPaths::ProjectDir() / Path) != Expected) return false;
        SourceFiles.Add(Path);
    }
    for (int32 I = 0; I < 377; ++I)
    {
        TArray<double> A, B, D; FVector N; double NX, NY, R, G, Blue, Alpha;
        if (!Numbers((*P)[I], 3, A) || !Numbers((*UV)[I], 2, B) || !Numbers((*C)[I], 4, D) || !XYZ(Object((*NP)[I]), N)
            || !Number(Object((*NUV)[I]), TEXT("x"), NX) || !Number(Object((*NUV)[I]), TEXT("y"), NY)
            || !Number(Object((*NC)[I]), TEXT("r"), R) || !Number(Object((*NC)[I]), TEXT("g"), G) || !Number(Object((*NC)[I]), TEXT("b"), Blue) || !Number(Object((*NC)[I]), TEXT("a"), Alpha)
            || !N.Equals(FVector(A[0], A[1], A[2]), .000001) || FMath::Abs(NX-B[0]) > .000001 || FMath::Abs(NY-B[1]) > .000001
            || float(R) != float(D[0]) || float(G) != float(D[1]) || float(Blue) != float(D[2]) || float(Alpha) != float(D[3])) return false;
        const FVector3f Center{float(D[0])*180.f, float(D[1])*180.f, float(D[2])*180.f};
        const FVector3f Raw{float(A[0]), float(A[1]), float(A[2])};
        const FVector3f Baked = Center + (Raw-Center)*float(Bake);
        const FVector3f Converted{Baked.X*100.f, Baked.Z*100.f, Baked.Y*100.f};
        if (Converted.ContainsNaN() || !FMath::IsFinite(float(B[0])) || !FMath::IsFinite(float(B[1])) || !FMath::IsFinite(float(D[3]))) return false;
        Vertices.Add({Converted, FVector2f(float(B[0]), float(B[1])), FVector2f(float(D[0]), float(D[1])), FVector2f(float(D[2]), float(D[3]))});
    }
    for (int32 I = 0; I < 309; ++I)
    {
        TArray<double> T; if (!Numbers((*ST)[I], 3, T)) return false;
        for (int32 J = 0; J < 3; ++J)
        {
            double N; if (T[J] < 0 || T[J] > 376 || T[J] != FMath::FloorToDouble(T[J]) || !(*NT)[I*3+J] || (*NT)[I*3+J]->Type != EJson::Number || !(*NT)[I*3+J]->TryGetNumber(N) || N != T[J]) return false;
        }
        // UE glTF swaps Y/Z and reverses the source triangle winding.
        Triangles.Add(CanonicalTriangle(FIntVector(int32(T[0]), int32(T[2]), int32(T[1]))));
    }
    const TArray<TSharedPtr<FJsonValue>> *Samples = nullptr, *Frames = nullptr, *Placement = nullptr;
    if (!Array(Source, TEXT("samples"), 9, Samples) || !Array(Native, TEXT("frames"), 9, Frames) || !Array(Source, TEXT("instances"), 11, Placement)) return false;
    for (int32 I = 0; I < 9; ++I)
    {
        const auto A = Object((*Samples)[I]), B = Object((*Frames)[I]); double Dxf = 0, NativeDxf = 0, Scale = 0, NativeScale = 0; FString Name, NativeName, ImagePath;
        const TArray<TSharedPtr<FJsonValue>>* Evaluated = nullptr;
        if (!Number(A, TEXT("dxf"), Dxf) || !Number(B, TEXT("dxf"), NativeDxf) || Dxf != Sequence[I] || NativeDxf != Dxf || !Number(A, TEXT("scale"), Scale) || !Number(B, TEXT("scale"), NativeScale)
            || Scale <= 0 || Scale > 10 || FMath::Abs(Scale-NativeScale) > .000002 || !A->TryGetStringField(TEXT("name"), Name) || !B->TryGetStringField(TEXT("name"), NativeName)
            || Name != FString::Printf(TEXT("%02d_dxf%.0f"), I, Dxf) || NativeName != Name || !Array(B, TEXT("evaluatedLocalPositions"), 377, Evaluated)
            || !B->TryGetStringField(TEXT("path"), ImagePath) || !FPaths::IsSamePath(ImagePath, InputPath(*(TEXT("native_")+Name+TEXT(".png"))))) return false;
        TArray<uint8> Bytes; if (!FFileHelper::LoadFileToArray(Bytes, *ImagePath) || Bytes.Num() < 24 || Bytes[0] != 137 || Bytes[1] != 80) return false;
        for (const auto& V : *Evaluated) { FVector Position; if (!XYZ(Object(V), Position)) return false; }
        SampleNames.Add(Name); Dxfs.Add(float(Dxf)); Scales.Add(float(NativeScale));
    }
    for (const auto& V : *Placement)
    {
        const auto O = Object(V), Instance = Child(O, TEXT("source_instance")); TArray<double> Tile; FVector Location, Scale; double Index = 0, Yaw = 0; FString Id;
        if (!O || !Numbers(O->TryGetField(TEXT("tile")), 2, Tile) || !Instance || !Instance->TryGetStringField(TEXT("prefab_id"), Id) || Id != Prefab
            || !XYZ(Child(Instance, TEXT("ue_relative_to_focus")), Location) || !XYZ(Child(Instance, TEXT("ue_actor_scale_for_centimeter_asset")), Scale)
            || !Number(Instance, TEXT("ue_yaw"), Yaw) || !Number(Instance, TEXT("template_index"), Index) || Index < 0 || Index > 10000 || Index != FMath::FloorToDouble(Index)
            || Tile[0] < 0 || Tile[1] < 0 || Tile[0] > 10000 || Tile[1] > 10000 || Tile[0] != FMath::FloorToDouble(Tile[0]) || Tile[1] != FMath::FloorToDouble(Tile[1])) return false;
        FInstance Row; Row.TileTag = FString::Printf(TEXT("UnityTile_%.0f_%.0f"), Tile[0], Tile[1]); Row.IndexTag = FString::Printf(TEXT("UnityObjectIndex_%.0f"), Index);
        Row.SourceTransform = FTransform(FRotator(0, Yaw, 0), Location, Scale);
        for (const auto& Existing : Instances) if (Existing.TileTag == Row.TileTag && Existing.IndexTag == Row.IndexTag) return false;
        Instances.Add(Row);
    }
    return true;
}

bool URokGroveCenterScaleAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor = FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor, Token, false))
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokGroveCenterScaleAudit"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokGroveCenterScaleIsolate"), ESearchCase::IgnoreCase))
        { Finish(TEXT("Dedicated grove audit rejects other Rok runtime writers/options")); return false; }
    if (!LoadSource()) { Finish(TEXT("Malformed, stale or incomplete grove source/native reference")); return false; }
    int32 Cameras = 0, Configs = 0, Tagged = 0;
    for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++Cameras; }
    for (TActorIterator<ARokWorldMapConfigActor> It(GetWorld()); It; ++It) { Config = *It; ++Configs; }
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (Cameras != 1 || Configs != 1 || !PC || PC->GetPawn() != Camera.Get() || PC->GetViewTarget() != Camera.Get()) { Finish(TEXT("Actual possessed world camera/config identity differs")); return false; }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("RokGroveCenterScaleV1"))) continue; ++Tagged;
        if (!It->ActorHasTag(PrefabTag) || !It->ActorHasTag(TEXT("RokWorldMapExactSource"))) { Finish(TEXT("Grove marker attached to a different source prefab")); return false; }
        FInstance* Match = nullptr;
        for (auto& Row : Instances) if (It->ActorHasTag(*Row.TileTag) && It->ActorHasTag(*Row.IndexTag)) { Match = &Row; break; }
        if (!Match || Match->Actor.IsValid() || !It->GetActorTransform().Equals(Match->SourceTransform, .01)) { Finish(TEXT("Duplicate/missing source grove identity or changed source actor placement")); return false; }
        TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
        if (Meshes.Num() != 1 || !Visible(Meshes[0]) || !Meshes[0]->GetStaticMesh() || Meshes[0]->GetStaticMesh()->GetPathName() != MeshPath) { Finish(TEXT("Expected one visible dedicated grove mesh per source actor")); return false; }
        Match->Actor = *It; Match->Mesh = Meshes[0]; Match->OriginalComponentTransform = Meshes[0]->GetComponentTransform();
        if (!Match->OriginalComponentTransform.Equals(Match->SourceTransform, .01)) { Finish(TEXT("Grove component has an unexpected offset from source actor placement")); return false; }
    }
    if (Tagged != 11) { Finish(TEXT("Expected exactly eleven source grove instances")); return false; }
    for (const auto& Row : Instances) if (!Row.Actor.IsValid()) { Finish(TEXT("One source placement has no actual grove actor")); return false; }
    if (!InspectMesh()) return false;
    double Nearest = DBL_MAX;
    const FVector InitialFocus = Config->Projection.UnityToUe(Camera->GetViewCenterUnity());
    for (const auto& Row : Instances)
    {
        const double Distance = FVector::DistSquared(Row.Mesh->Bounds.Origin, InitialFocus);
        if (Distance < Nearest) { Nearest = Distance; FocusMesh = Row.Mesh; }
    }
    const FVector Forward = Camera->GetCameraComponent()->GetForwardVector();
    if (FMath::Abs(Forward.Z) < .1) { Finish(TEXT("Unexpected world camera forward axis")); return false; }
    const FVector Center = FocusMesh->Bounds.Origin;
    FocusUnity = Config->Projection.UeToUnity(Center-Forward*((Center.Z-Config->Projection.GroundZ)/Forward.Z));
    bIsolate = FParse::Param(FCommandLine::Get(), TEXT("RokGroveCenterScaleIsolate"));
    if (bIsolate)
    {
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
        {
            TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
            for (auto* Mesh : Meshes) if (Mesh != FocusMesh.Get())
            {
                SavedVisibility.Emplace(Mesh, FIntPoint(Mesh->IsVisible(), Mesh->bHiddenInGame));
                auto Record = MakeShared<FJsonObject>(); Record->SetStringField(TEXT("component"), Mesh->GetPathName()); Record->SetBoolField(TEXT("visible"), Mesh->IsVisible()); Record->SetBoolField(TEXT("hidden_in_game"), Mesh->bHiddenInGame); VisibilityRecords.Add(MakeShared<FJsonValueObject>(Record));
                Mesh->SetVisibility(false, false); Mesh->SetHiddenInGame(true, false);
            }
        }
    }
    Camera->SetViewCenterUnity(FocusUnity); NextSample(); return true;
}

bool URokGroveCenterScaleAuditSubsystem::InspectMesh()
{
    UStaticMesh* Mesh = Instances[0].Mesh->GetStaticMesh();
    const auto* Render = Mesh->GetRenderData();
    const bool bAllowCPU = Mesh->bAllowCPUAccess;
    const bool bNanite = Mesh->GetNaniteSettings().bEnabled;
    const int32 LodCount = Render ? Render->LODResources.Num() : -1;
    // Preserve actual metadata even when the first buffer gate fails before any capture.
    MeshEvidence = MakeShared<FJsonObject>(); MeshEvidence->SetStringField(TEXT("mesh"), Mesh->GetPathName());
    MeshEvidence->SetBoolField(TEXT("allow_cpu_access"), bAllowCPU); MeshEvidence->SetBoolField(TEXT("nanite_enabled"), bNanite);
    MeshEvidence->SetBoolField(TEXT("has_render_data"), Render != nullptr); MeshEvidence->SetNumberField(TEXT("render_lod_count"), LodCount);
    MeshEvidence->SetBoolField(TEXT("buffer_source_match_complete"), false);
    if (!bAllowCPU || bNanite || !Render || LodCount != 1)
    {
        Finish(FString::Printf(TEXT("Dedicated mesh needs one readable non-Nanite actual render LOD: allow_cpu_access=%d nanite_enabled=%d has_render_data=%d render_lod_count=%d mesh=%s"),
            bAllowCPU, bNanite, Render != nullptr, LodCount, *Mesh->GetPathName()));
        return false;
    }
    const auto& LOD = Render->LODResources[0]; const auto& UV = LOD.VertexBuffers.StaticMeshVertexBuffer; const auto& P = LOD.VertexBuffers.PositionVertexBuffer;
    if (!UV.GetUseFullPrecisionUVs() || UV.GetNumTexCoords() != 3 || UV.GetNumVertices() != P.GetNumVertices() || P.GetNumVertices() < 377 || P.GetNumVertices() > 927 || LOD.IndexBuffer.GetNumIndices() != 927)
    { Finish(TEXT("Actual grove render buffer precision/count/UV channels differ")); return false; }
#if WITH_EDITOR
    if (Mesh->GetNumSourceModels() != 1 || !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs) { Finish(TEXT("Grove source build settings would quantize center UVs")); return false; }
#endif
    TArray<int32> Mapping; TSet<int32> Seen; TArray<TSharedPtr<FJsonValue>> Rows, MappedTriangles;
    for (uint32 I = 0; I < P.GetNumVertices(); ++I)
    {
        const FVector3f Position = P.VertexPosition(I); const FVector2f U0 = UV.GetVertexUV(I, 0), U1 = UV.GetVertexUV(I, 1), U2 = UV.GetVertexUV(I, 2); int32 Index = INDEX_NONE;
        for (int32 J = 0; J < Vertices.Num(); ++J)
        {
            const auto& V = Vertices[J];
            if (Position.Equals(V.Position, .005f) && U0 == V.UV0 && U1 == V.UV1 && U2 == V.UV2) { if (Index != INDEX_NONE) { Finish(TEXT("Ambiguous source vertex mapping")); return false; } Index = J; }
        }
        if (Index == INDEX_NONE) { Finish(TEXT("Render position/UV0/float32 center channels do not match any source vertex")); return false; }
        Mapping.Add(Index); Seen.Add(Index); Rows.Add(MakeShared<FJsonValueArray>(Values({double(Index), Position.X, Position.Y, Position.Z, U0.X, U0.Y, U1.X, U1.Y, U2.X, U2.Y})));
    }
    if (Seen.Num() != 377) { Finish(TEXT("Render buffer omitted source vertices")); return false; }
    MeshEvidence->SetBoolField(TEXT("full_precision_uvs"), UV.GetUseFullPrecisionUVs());
    MeshEvidence->SetNumberField(TEXT("uv_channels"), UV.GetNumTexCoords());
    MeshEvidence->SetNumberField(TEXT("index_count"), LOD.IndexBuffer.GetNumIndices());
    MeshEvidence->SetArrayField(TEXT("render_vertices_source_mapping"), Rows);
    TArray<FIntVector> Remaining = Triangles;
    for (int32 I = 0; I < LOD.IndexBuffer.GetNumIndices(); I += 3)
    {
        const uint32 A = LOD.IndexBuffer.GetIndex(I), B = LOD.IndexBuffer.GetIndex(I+1), C = LOD.IndexBuffer.GetIndex(I+2);
        if (A >= uint32(Mapping.Num()) || B >= uint32(Mapping.Num()) || C >= uint32(Mapping.Num())) { Finish(TEXT("Invalid grove GPU index buffer")); return false; }
        const FIntVector T = CanonicalTriangle(FIntVector(Mapping[A], Mapping[B], Mapping[C])); const int32 Found = Remaining.Find(T);
        if (Found == INDEX_NONE)
        {
            const FIntVector Reversed = CanonicalTriangle(FIntVector(T.X, T.Z, T.Y));
            const bool bForwardExists = Triangles.Contains(T), bReversedExists = Triangles.Contains(Reversed);
            auto Failure = MakeShared<FJsonObject>(); Failure->SetNumberField(TEXT("index_offset"), I);
            Failure->SetArrayField(TEXT("actual_render_indices"), Values({double(A), double(B), double(C)}));
            Failure->SetArrayField(TEXT("actual_mapped_triangle"), Values({double(Mapping[A]), double(Mapping[B]), double(Mapping[C])}));
            Failure->SetBoolField(TEXT("expected_forward_exists"), bForwardExists); Failure->SetBoolField(TEXT("expected_reversed_exists"), bReversedExists);
            Failure->SetBoolField(TEXT("expected_remaining_forward_exists"), false);
            MeshEvidence->SetObjectField(TEXT("triangle_failure"), Failure);
            MeshEvidence->SetArrayField(TEXT("render_triangles_checked_before_failure"), MappedTriangles);
            Finish(FString::Printf(TEXT("Actual grove triangle topology/winding differs from source: index_offset=%d mapped=(%d,%d,%d) expected_forward_exists=%d expected_reversed_exists=%d"),
                I, Mapping[A], Mapping[B], Mapping[C], bForwardExists, bReversedExists));
            return false;
        }
        Remaining.RemoveAtSwap(Found); MappedTriangles.Add(MakeShared<FJsonValueArray>(Values({double(T.X), double(T.Y), double(T.Z)})));
    }
    MeshEvidence->SetBoolField(TEXT("buffer_source_match_complete"), true); MeshEvidence->SetBoolField(TEXT("full_precision_uvs"), true);
    MeshEvidence->SetNumberField(TEXT("uv_channels"), 3); MeshEvidence->SetArrayField(TEXT("render_vertices_source_mapping"), Rows); MeshEvidence->SetArrayField(TEXT("render_triangles_source_mapping"), MappedTriangles);
    MeshEvidence->SetStringField(TEXT("scope"), TEXT("Read actual static render buffers. Positions are source 1509 bake; UV1/UV2 preserve source float COLOR. These CPU buffer reads do not read back GPU WPO vertices."));
    return true;
}

bool URokGroveCenterScaleAuditSubsystem::Inspect(TSharedPtr<FJsonObject>& Out)
{
    if (!Identity()) return false;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!Camera.IsValid() || !Config.IsValid() || !FocusMesh.IsValid() || !PC || PC->GetPawn() != Camera.Get() || PC->GetViewTarget() != Camera.Get() || !Camera->GetViewCenterUnity().Equals(FocusUnity, .001)
        || !FMath::IsNearlyEqual(Camera->GetCurrentDxf(), Dxfs[SampleIndex], .001f)) { Finish(TEXT("Camera lost exact source zoom/focus or possessed view target")); return false; }
    if (Hash(InputPath(TEXT("manifest.json"))) != SourceHash || Hash(InputPath(TEXT("native_render.json"))) != NativeHash) { Finish(TEXT("Source/native reference changed during runtime capture")); return false; }
    TArray<TSharedPtr<FJsonValue>> Rows; int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("RokGroveCenterScaleV1"))) ++Count;
    if (Count != 11) { Finish(TEXT("Grove runtime instance set changed")); return false; }
    for (const auto& Row : Instances)
    {
        auto* Actor = Row.Actor.Get(); auto* Mesh = Row.Mesh.Get();
        if (!Actor || !Mesh || Mesh->GetOwner() != Actor || !Actor->ActorHasTag(PrefabTag) || !Actor->ActorHasTag(TEXT("RokGroveCenterScaleV1")) || !Actor->ActorHasTag(TEXT("RokWorldMapExactSource"))
            || !Actor->ActorHasTag(*Row.TileTag) || !Actor->ActorHasTag(*Row.IndexTag)
            || !Actor->GetActorTransform().Equals(Row.SourceTransform, .01) || !Mesh->GetComponentTransform().Equals(Row.OriginalComponentTransform, .00001) || !Mesh->GetStaticMesh()
            || Mesh->GetStaticMesh() != Instances[0].Mesh->GetStaticMesh() || Mesh->GetStaticMesh()->GetPathName() != MeshPath)
        { Finish(TEXT("Source grove actor/component transform or identity changed during zoom")); return false; }
        auto* MID = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)); float Scale = -1;
        if (!MID || !MID->Parent || MID->Parent->GetPathName() != MaterialPath || !MID->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RokSourceTreeScale")), Scale) || !FMath::IsFinite(Scale) || !FMath::IsNearlyEqual(Scale, Scales[SampleIndex], .000002f))
        { Finish(TEXT("Actual renderer MID lacks the native source TreeScale (a ratio is not accepted)")); return false; }
        if ((!bIsolate || Mesh == FocusMesh.Get()) && !Visible(Mesh)) { Finish(TEXT("Expected source grove is not visible through actual parent chain")); return false; }
        auto R = MakeShared<FJsonObject>(); R->SetStringField(TEXT("actor"), Actor->GetName()); R->SetStringField(TEXT("tile_tag"), Row.TileTag); R->SetStringField(TEXT("index_tag"), Row.IndexTag); R->SetStringField(TEXT("component"), Mesh->GetPathName());
        R->SetArrayField(TEXT("location"), Vector(Actor->GetActorLocation())); R->SetArrayField(TEXT("scale"), Vector(Actor->GetActorScale3D())); R->SetArrayField(TEXT("rotation_quaternion"), Values({Actor->GetActorQuat().X, Actor->GetActorQuat().Y, Actor->GetActorQuat().Z, Actor->GetActorQuat().W}));
        R->SetStringField(TEXT("material_instance"), MID->GetPathName()); R->SetStringField(TEXT("material_parent"), MID->Parent->GetPathName()); R->SetNumberField(TEXT("source_tree_scale"), Scale); R->SetBoolField(TEXT("visible_parent_chain"), Visible(Mesh));
        R->SetBoolField(TEXT("source_transform_unchanged"), true); Rows.Add(MakeShared<FJsonValueObject>(R));
    }
    for (const auto& V : SavedVisibility) if (!V.Key.IsValid() || V.Key->IsVisible() || !V.Key->bHiddenInGame) { Finish(TEXT("Isolation visibility was changed by another runtime writer")); return false; }
    int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
    if (Width != 1440 || Height != 810) { Finish(TEXT("Grove audit requires actual 1440x810 viewport")); return false; }
    FVector2D FocusScreen;
    if (!PC->ProjectWorldLocationToScreen(FocusMesh->Bounds.Origin, FocusScreen) || FocusScreen.X < 144 || FocusScreen.X > 1296 || FocusScreen.Y < 81 || FocusScreen.Y > 729)
    { Finish(TEXT("Chosen existing grove center is outside usable viewport")); return false; }
    TArray<TSharedPtr<FJsonValue>> Centers; TSet<FString> Unique;
    for (const auto& V : Vertices)
    {
        const FVector3f Local32{V.UV1.X*18000.f, V.UV2.X*18000.f, V.UV1.Y*18000.f};
        const FVector Local(Local32);
        const FString Key = FString::Printf(TEXT("%.9g/%.9g/%.9g"), Local.X, Local.Y, Local.Z); if (Unique.Contains(Key)) continue; Unique.Add(Key);
        const FVector World = FocusMesh->GetComponentTransform().TransformPosition(Local); FVector2D Screen;
        if (!PC->ProjectWorldLocationToScreen(World, Screen) || !FMath::IsFinite(Screen.X) || !FMath::IsFinite(Screen.Y)) { Finish(TEXT("Source center projection failed")); return false; }
        auto C = MakeShared<FJsonObject>(); C->SetArrayField(TEXT("local_center_from_actual_matched_uv"), Vector(Local)); C->SetArrayField(TEXT("world_center"), Vector(World)); C->SetArrayField(TEXT("screen_center"), Values({Screen.X, Screen.Y})); Centers.Add(MakeShared<FJsonValueObject>(C));
    }
    if (Centers.Num() != 16) { Finish(TEXT("Expected sixteen stable per-tree source centers")); return false; }
    Out = MakeShared<FJsonObject>(); Out->SetNumberField(TEXT("sample_index"), SampleIndex); Out->SetStringField(TEXT("sample_name"), SampleNames[SampleIndex]); Out->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf()); Out->SetNumberField(TEXT("native_scale"), Scales[SampleIndex]);
    Out->SetNumberField(TEXT("frame"), double(GFrameCounter)); Out->SetNumberField(TEXT("elapsed"), Elapsed); Out->SetStringField(TEXT("view_target"), PC->GetViewTarget()->GetPathName()); Out->SetArrayField(TEXT("camera_position"), Vector(Camera->GetCameraComponent()->GetComponentLocation()));
    Out->SetArrayField(TEXT("focus_unity"), Values({FocusUnity.X, FocusUnity.Y})); Out->SetStringField(TEXT("focus_component"), FocusMesh->GetPathName()); Out->SetArrayField(TEXT("instances"), Rows); Out->SetArrayField(TEXT("source_center_projections"), Centers);
    return true;
}

void URokGroveCenterScaleAuditSubsystem::NextSample()
{
    if (++SampleIndex == 9) { Finish(); return; }
    Camera->SetCurrentDxf(Dxfs[SampleIndex]); StageStarted = Elapsed; StageFrame = GFrameCounter;
}
void URokGroveCenterScaleAuditSubsystem::Tick(float DeltaTime)
{
    if (!bOwnsTargetWorld || bFinished || !GetWorld()->HasBegunPlay()) return; Elapsed += FMath::Max(0.f, DeltaTime);
    if (FPlatformTime::Seconds()-WallStarted > 90) { Finish(TEXT("Grove real wall-clock timeout")); return; }
    if (SampleIndex < 0) { if (Elapsed > 3) Begin(); return; }
    if (Pending) { if (Elapsed-RequestStarted > 10) Finish(TEXT("Screenshot processed callback timeout")); return; }
    if (Elapsed-StageStarted < .5 || GFrameCounter < StageFrame+3) return;
    TSharedPtr<FJsonObject> State; if (!Inspect(State)) return;
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Competing screenshot request")); return; }
    PendingPath = Directory / (SampleNames[SampleIndex]+TEXT(".png"));
    if (IFileManager::Get().FileExists(*PendingPath)) { Finish(TEXT("Refusing an existing screenshot path")); return; }
    if (!IFileManager::Get().MakeDirectory(*Directory, true)) { Finish(TEXT("Cannot create audit output directory")); return; }
    Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("file"), PendingPath); Pending->SetObjectField(TEXT("request"), State); RequestStarted = Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokGroveCenterScaleAuditSubsystem::Processed()
{
    if (!bOwnsTargetWorld || bFinished || !Pending) return; TSharedPtr<FJsonObject> State; if (!Inspect(State)) return;
    TArray<uint8> Bytes; const uint8 Signature[] = {137,80,78,71,13,10,26,10};
    if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0) { Finish(TEXT("Missing processed PNG")); return; }
    auto U32 = [&Bytes](int32 N) { return uint32(Bytes[N])<<24 | uint32(Bytes[N+1])<<16 | uint32(Bytes[N+2])<<8 | uint32(Bytes[N+3]); };
    if (U32(16) != 1440 || U32(20) != 810) { Finish(TEXT("Processed screenshot dimensions differ from viewport")); return; }
    Pending->SetNumberField(TEXT("file_bytes"), Bytes.Num()); Pending->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes)); Pending->SetObjectField(TEXT("processed"), State);
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset(); if (!Save(false)) { Finish(TEXT("Intermediate manifest save failed")); return; } NextSample();
}
bool URokGroveCenterScaleAuditSubsystem::RestoreVisibility()
{
    bool Restored = true;
    for (const auto& V : SavedVisibility)
    {
        if (!V.Key.IsValid()) { Restored = false; continue; }
        V.Key->SetVisibility(V.Value.X != 0, false); V.Key->SetHiddenInGame(V.Value.Y != 0, false);
        Restored &= V.Key->IsVisible() == (V.Value.X != 0) && V.Key->bHiddenInGame == (V.Value.Y != 0);
    }
    SavedVisibility.Reset(); bVisibilityRestored &= Restored; return Restored;
}
bool URokGroveCenterScaleAuditSubsystem::Save(bool Complete)
{
    auto R = MakeShared<FJsonObject>(); R->SetNumberField(TEXT("audit_version"), 1); R->SetBoolField(TEXT("complete"), Complete); R->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 9 && bVisibilityRestored);
    R->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())); R->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : TEXT("missing"));
    R->SetStringField(TEXT("source_manifest_sha256"), SourceHash); R->SetStringField(TEXT("native_manifest_sha256"), NativeHash); R->SetBoolField(TEXT("capture_includes_slate_ui"), true);
    R->SetBoolField(TEXT("owns_target_world"), bOwnsTargetWorld);
    R->SetBoolField(TEXT("isolated_existing_asset"), bIsolate); R->SetBoolField(TEXT("visibility_restored"), bVisibilityRestored); R->SetArrayField(TEXT("temporarily_hidden_components"), VisibilityRecords);
    R->SetArrayField(TEXT("captures"), Captures); if (MeshEvidence) R->SetObjectField(TEXT("actual_render_buffers"), MeshEvidence);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& E : Failures) Errors.Add(MakeShared<FJsonValueString>(E)); R->SetArrayField(TEXT("failures"), Errors);
    R->SetBoolField(TEXT("gpu_vertex_readback"), false); R->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
    R->SetStringField(TEXT("scope"), TEXT("Nine actual camera API zooms across real ticks; all eleven source actor/component transforms remain fixed and actual MIDs carry native source TreeScale. Actual render UV/position/index buffers are checked. Source center projections are CPU metadata, NOT measured GPU WPO positions. Screenshots use the actual renderer and need image/native review. Native reference uses a fixed camera; UE uses real changing source camera, so full-image equality is not expected. Optional isolation temporarily hides other existing static components and restores both flags; it does not establish unobstructed normal-world presentation. No asset save, source placement edit, physical wheel test or authoritative data."));
    FString Text; IFileManager::Get().MakeDirectory(*Directory, true);
    return FJsonSerializer::Serialize(R, TJsonWriterFactory<>::Create(&Text)) && FFileHelper::SaveStringToFile(Text, *(Directory/TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokGroveCenterScaleAuditSubsystem::Finish(const FString& Failure)
{
    if (!bOwnsTargetWorld || bFinished) return; bFinished = true; if (!Failure.IsEmpty()) Failures.Add(Failure);
    if (!RestoreVisibility()) Failures.Add(TEXT("Temporary visibility restoration failed"));
    if (Failures.IsEmpty() && Captures.Num() != 9) Failures.Add(TEXT("Expected nine processed screenshots"));
    const bool Written = Save(Failures.IsEmpty()); if (!Written) Failures.Add(TEXT("Final manifest save failed"));
    for (const auto& E : Failures) UE_LOG(LogTemp, Error, TEXT("Rok GroveCenterScaleAudit: %s"), *E);
    UE_LOG(LogTemp, Display, TEXT("Rok GroveCenterScaleAudit passed=%d captures=%d output=%s"), Failures.IsEmpty(), Captures.Num(), *Directory);
    FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1);
}
