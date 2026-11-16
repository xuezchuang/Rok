#pragma once

#include "CoreMinimal.h"
#include "RokStoneDepositData.h"

struct FRokWorldStructurePresentationGroup
{
	FString SourceId;
	FString Name;
	FVector Position = FVector::ZeroVector;
	FVector ShiftOrigin = FVector::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;
	int32 Priority = 0;
	int32 SiblingIndex = -1;
	bool bShiftPosition = false;
	float ShiftStartScale = 0;
	float ShiftEndScale = 0;
	bool bActive = true;
};

struct FRokWorldStructurePresentationItem
{
	FString SourceId;
	FString SourceRendererId;
	FString Name;
	FString GroupId;
	int32 GroupIndex = -1;
	FVector Position = FVector::ZeroVector;
	FVector SpritePosition = FVector::ZeroVector;
	FVector Right = FVector::ForwardVector;
	FVector Up = FVector::UpVector;
	FVector2D Size = FVector2D::ZeroVector;
	FVector2D Pivot = FVector2D(0.5, 0.5);
	FLinearColor Color1 = FLinearColor::White;
	FLinearColor Color2 = FLinearColor::White;
	FLinearColor RendererColor = FLinearColor::White;
	FString TexturePath;
	FString MaskPath;
	FString Shader;
	FString ShaderMode;
	int32 SortOrder = 0;
	int32 SortLayerOrder = 0;
	bool bEnabled = true;
	bool bGroundLod0Only = false;
};

struct FRokWorldStructurePresentationMesh
{
	FString SourceId;
	FString SourceRendererId;
	FString SourceMeshGuid;
	FString SourceMeshName;
	FString SourceFilterId;
	FString Name;
	FString MaterialPath;
	FString TexturePath;
	FString Shader;
	TArray<FVector> Positions;
	TArray<FVector> Normals;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colors;
	TArray<int32> Indices;
	bool bEnabled = false;
};

// A selected model from the packaged world_structure_presentation.json. The
// collision data uses the existing source GridCollide implementation, including
// its persistent zoom state; item order follows captured Unity registration.
struct FRokWorldStructurePresentationData
{
	FString ModelId;
	TArray<FRokWorldStructurePresentationItem> Items;
	TArray<FRokWorldStructurePresentationMesh> Meshes;
	TArray<FRokWorldStructurePresentationGroup> Groups;
	FRokStoneDepositData CollisionData;
	float MaxScale = 0;
	float UnitsToCentimeters = 0;
	bool Load(const FString& InModelId);
	bool LoadJson(const FString& Text, const FString& InModelId);
	float EvaluateScale(float Dxf) const;
};
