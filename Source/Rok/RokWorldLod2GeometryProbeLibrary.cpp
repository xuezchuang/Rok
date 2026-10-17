#include "RokWorldLod2GeometryProbeLibrary.h"

#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"

namespace
{
TSharedPtr<FJsonValue> Number(double Value)
{
    return MakeShared<FJsonValueNumber>(Value);
}

TSharedPtr<FJsonValue> PositionUv(const FVector3f& Position, const FVector2f& Uv)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(5);
    Values.Add(Number(Position.X));
    Values.Add(Number(Position.Y));
    Values.Add(Number(Position.Z));
    Values.Add(Number(Uv.X));
    Values.Add(Number(Uv.Y));
    return MakeShared<FJsonValueArray>(MoveTemp(Values));
}

TArray<TSharedPtr<FJsonValue>> Vector3(const FVector& Value)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(3);
    Values.Add(Number(Value.X));
    Values.Add(Number(Value.Y));
    Values.Add(Number(Value.Z));
    return Values;
}
}

FString URokWorldLod2GeometryProbeLibrary::CaptureStaticMeshLod0(UStaticMesh* Mesh)
{
#if WITH_EDITOR
    if (!IsInGameThread() || !Mesh)
    {
        return TEXT("");
    }
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() != 1)
    {
        return TEXT("");
    }
    const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
    const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& UVs = LOD.VertexBuffers.StaticMeshVertexBuffer;
    const uint32 VertexCount = Positions.GetNumVertices();
    const int32 IndexCount = LOD.IndexBuffer.GetNumIndices();
    if (VertexCount == 0 || VertexCount != UVs.GetNumVertices() ||
        UVs.GetNumTexCoords() < 1 || IndexCount <= 0 || IndexCount % 3 != 0 ||
        LOD.Sections.Num() != 1 || LOD.Sections[0].FirstIndex != 0 ||
        LOD.Sections[0].NumTriangles * 3 != IndexCount)
    {
        return TEXT("");
    }

    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("target"), Mesh->GetPathName());
    Result->SetStringField(TEXT("ue_class"), Mesh->GetClass()->GetName());
    // UStaticMesh::GetBounds returns ExtendedBounds, not bounds inferred from vertices.
    const FBoxSphereBounds AssetBounds = Mesh->GetBounds();
    TSharedRef<FJsonObject> Bounds = MakeShared<FJsonObject>();
    Bounds->SetStringField(TEXT("source"), TEXT("UStaticMesh::GetBounds"));
    Bounds->SetArrayField(TEXT("origin_cm"), Vector3(AssetBounds.Origin));
    Bounds->SetArrayField(TEXT("box_extent_cm"), Vector3(AssetBounds.BoxExtent));
    Bounds->SetNumberField(TEXT("sphere_radius_cm"), AssetBounds.SphereRadius);
    Result->SetObjectField(TEXT("asset_bounds"), Bounds);
    Result->SetNumberField(TEXT("render_vertex_count"), VertexCount);
    Result->SetNumberField(TEXT("triangle_count"), IndexCount / 3);
    Result->SetNumberField(TEXT("index_count"), IndexCount);
    Result->SetNumberField(TEXT("section_count"), LOD.Sections.Num());
    Result->SetNumberField(TEXT("uv_channel_count"), UVs.GetNumTexCoords());
    Result->SetBoolField(TEXT("uv0_full_precision"), UVs.GetUseFullPrecisionUVs());
    Result->SetNumberField(TEXT("section_first_index"), LOD.Sections[0].FirstIndex);
    Result->SetNumberField(TEXT("section_triangle_count"), LOD.Sections[0].NumTriangles);
    Result->SetNumberField(TEXT("section_material_index"), LOD.Sections[0].MaterialIndex);
    TArray<TSharedPtr<FJsonValue>> Vertices;
    Vertices.Reserve(VertexCount);
    for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
    {
        Vertices.Add(PositionUv(Positions.VertexPosition(Vertex), UVs.GetVertexUV(Vertex, 0)));
    }
    TArray<TSharedPtr<FJsonValue>> Indices;
    Indices.Reserve(IndexCount);
    for (int32 Index = 0; Index < IndexCount; ++Index)
    {
        const uint32 Vertex = LOD.IndexBuffer.GetIndex(Index);
        if (Vertex >= VertexCount)
        {
            return TEXT("");
        }
        Indices.Add(Number(Vertex));
    }
    Result->SetArrayField(TEXT("vertices"), MoveTemp(Vertices));
    Result->SetArrayField(TEXT("indices"), MoveTemp(Indices));
    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Result, Writer))
    {
        return TEXT("");
    }
    return Output;
#else
    return TEXT("");
#endif
}

FString URokWorldLod2GeometryProbeLibrary::CaptureStaticMeshLod0FullUvSigned(UStaticMesh* Mesh)
{
#if WITH_EDITOR
    if (!IsInGameThread() || !Mesh || !Mesh->bAllowCPUAccess || Mesh->GetNaniteSettings().bEnabled)
    {
        return TEXT("");
    }
    const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() != 1 || Mesh->GetNumSourceModels() != 1 ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs)
    {
        return TEXT("");
    }
    const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
    const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
    const FStaticMeshVertexBuffer& UVs = LOD.VertexBuffers.StaticMeshVertexBuffer;
    const uint32 VertexCount = Positions.GetNumVertices();
    const int32 IndexCount = LOD.IndexBuffer.GetNumIndices();
    if (VertexCount == 0 || VertexCount != UVs.GetNumVertices() || !UVs.GetUseFullPrecisionUVs() ||
        UVs.GetNumTexCoords() != 3 || IndexCount <= 0 || IndexCount % 3 != 0 ||
        LOD.Sections.Num() != 1 || LOD.Sections[0].FirstIndex != 0 ||
        LOD.Sections[0].NumTriangles * 3 != IndexCount)
    {
        return TEXT("");
    }
    TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("schema"), TEXT("rok.world_lod2.full_uv_signed_native_capture.v1"));
    Result->SetStringField(TEXT("target"), Mesh->GetPathName());
    Result->SetStringField(TEXT("ue_class"), Mesh->GetClass()->GetName());
    Result->SetBoolField(TEXT("allow_cpu_access"), Mesh->bAllowCPUAccess);
    Result->SetBoolField(TEXT("nanite_enabled"), Mesh->GetNaniteSettings().bEnabled);
    Result->SetBoolField(TEXT("full_precision_uvs"), UVs.GetUseFullPrecisionUVs());
    Result->SetNumberField(TEXT("uv_channel_count"), UVs.GetNumTexCoords());
    Result->SetNumberField(TEXT("render_vertex_count"), VertexCount);
    Result->SetNumberField(TEXT("triangle_count"), IndexCount / 3);
    Result->SetNumberField(TEXT("index_count"), IndexCount);
    Result->SetNumberField(TEXT("section_count"), LOD.Sections.Num());
    Result->SetNumberField(TEXT("section_material_index"), LOD.Sections[0].MaterialIndex);
    const FBoxSphereBounds AssetBounds = Mesh->GetBounds();
    TSharedRef<FJsonObject> Asset = MakeShared<FJsonObject>();
    Asset->SetArrayField(TEXT("origin_cm"), Vector3(AssetBounds.Origin));
    Asset->SetArrayField(TEXT("box_extent_cm"), Vector3(AssetBounds.BoxExtent));
    Result->SetObjectField(TEXT("asset_bounds"), Asset);
    FBox NativeBox(ForceInit);
    TArray<TSharedPtr<FJsonValue>> Vertices;
    Vertices.Reserve(VertexCount);
    for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
    {
        const FVector3f P = Positions.VertexPosition(Vertex);
        const FVector2f U0 = UVs.GetVertexUV(Vertex, 0);
        const FVector2f U1 = UVs.GetVertexUV(Vertex, 1);
        const FVector2f U2 = UVs.GetVertexUV(Vertex, 2);
        const FVector D(P);
        NativeBox += D;
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(9);
        Values.Add(Number(P.X)); Values.Add(Number(P.Y)); Values.Add(Number(P.Z));
        Values.Add(Number(U0.X)); Values.Add(Number(U0.Y));
        Values.Add(Number(U1.X)); Values.Add(Number(U1.Y));
        Values.Add(Number(U2.X)); Values.Add(Number(U2.Y));
        Vertices.Add(MakeShared<FJsonValueArray>(MoveTemp(Values)));
    }
    TSharedRef<FJsonObject> Native = MakeShared<FJsonObject>();
    Native->SetArrayField(TEXT("min_cm"), Vector3(NativeBox.Min));
    Native->SetArrayField(TEXT("max_cm"), Vector3(NativeBox.Max));
    Result->SetObjectField(TEXT("native_vertex_bounds"), Native);
    TArray<TSharedPtr<FJsonValue>> Indices;
    Indices.Reserve(IndexCount);
    for (int32 Index = 0; Index < IndexCount; ++Index)
    {
        const uint32 Vertex = LOD.IndexBuffer.GetIndex(Index);
        if (Vertex >= VertexCount)
        {
            return TEXT("");
        }
        Indices.Add(Number(Vertex));
    }
    Result->SetArrayField(TEXT("vertices"), MoveTemp(Vertices));
    Result->SetArrayField(TEXT("indices"), MoveTemp(Indices));
    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    return FJsonSerializer::Serialize(Result, Writer) ? Output : TEXT("");
#else
    return TEXT("");
#endif
}
