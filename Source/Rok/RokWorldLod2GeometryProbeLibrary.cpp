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
