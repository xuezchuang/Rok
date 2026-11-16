#include "RokWorldStructurePresentationData.h"

#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FObject = TSharedPtr<FJsonObject>;
using FValue = TSharedPtr<FJsonValue>;

FObject Object(const FObject& Parent, const TCHAR* Key)
{
	const FObject* Result = nullptr;
	return Parent.IsValid() && Parent->TryGetObjectField(Key, Result) && Result ? *Result : nullptr;
}

bool Array(const FObject& Parent, const TCHAR* Key, const TArray<FValue>*& Out)
{
	return Parent.IsValid() && Parent->TryGetArrayField(Key, Out) && Out;
}

bool String(const FObject& Parent, const TCHAR* Key, FString& Out, bool bAllowEmpty = false)
{
	return Parent.IsValid() && Parent->TryGetStringField(Key, Out) && (bAllowEmpty || !Out.IsEmpty());
}

bool Number(const FObject& Parent, const TCHAR* Key, double& Out)
{
	return Parent.IsValid() && Parent->TryGetNumberField(Key, Out) && FMath::IsFinite(Out)
		&& FMath::Abs(Out) <= FLT_MAX;
}

bool Integer(const FObject& Parent, const TCHAR* Key, int32& Out)
{
	double Value = 0;
	if (!Number(Parent, Key, Value) || Value < MIN_int32 || Value > MAX_int32 || FMath::FloorToDouble(Value) != Value) return false;
	Out = static_cast<int32>(Value);
	return true;
}

bool Boolean(const FObject& Parent, const TCHAR* Key, bool& Out)
{
	return Parent.IsValid() && Parent->TryGetBoolField(Key, Out);
}

bool Vector(const FObject& Parent, const TCHAR* Key, int32 Count, double* Out)
{
	const TArray<FValue>* Values = nullptr;
	if (!Array(Parent, Key, Values) || Values->Num() != Count) return false;
	for (int32 I = 0; I < Count; ++I)
		if (!(*Values)[I].IsValid() || !(*Values)[I]->TryGetNumber(Out[I])
			|| !FMath::IsFinite(Out[I]) || FMath::Abs(Out[I]) > FLT_MAX) return false;
	return true;
}

bool Vec3(const FObject& Parent, const TCHAR* Key, FVector& Out)
{
	double V[3];
	if (!Vector(Parent, Key, 3, V)) return false;
	Out = FVector(V[0], V[1], V[2]);
	return true;
}

bool Vec2(const FObject& Parent, const TCHAR* Key, FVector2D& Out)
{
	double V[2];
	if (!Vector(Parent, Key, 2, V)) return false;
	Out = FVector2D(V[0], V[1]);
	return true;
}

bool Color(const FObject& Parent, const TCHAR* Key, FLinearColor& Out)
{
	double V[4];
	if (!Vector(Parent, Key, 4, V)) return false;
	Out = FLinearColor(V[0], V[1], V[2], V[3]);
	return true;
}

bool ObjectVec3(const FValue& Value, FVector& Out)
{
	if (!Value.IsValid() || Value->Type != EJson::Object) return false;
	const FObject O = Value->AsObject();
	double X = 0, Y = 0, Z = 0;
	if (!Number(O, TEXT("x"), X) || !Number(O, TEXT("y"), Y) || !Number(O, TEXT("z"), Z)) return false;
	Out = FVector(X, Y, Z);
	return true;
}

bool ObjectVec2(const FValue& Value, FVector2D& Out)
{
	if (!Value.IsValid() || Value->Type != EJson::Object) return false;
	const FObject O = Value->AsObject();
	double X = 0, Y = 0;
	if (!Number(O, TEXT("x"), X) || !Number(O, TEXT("y"), Y)) return false;
	Out = FVector2D(X, Y);
	return true;
}

bool ObjectColor(const FValue& Value, FLinearColor& Out)
{
	if (!Value.IsValid() || Value->Type != EJson::Object) return false;
	const FObject O = Value->AsObject();
	double R = 0, G = 0, B = 0, A = 0;
	if (!Number(O, TEXT("r"), R) || !Number(O, TEXT("g"), G)
		|| !Number(O, TEXT("b"), B) || !Number(O, TEXT("a"), A)) return false;
	Out = FLinearColor(R, G, B, A);
	return true;
}

bool SourceRecord(const FObject& Parent, const TCHAR* Key)
{
	FString Path, Hash;
	const FObject Record = Object(Parent, Key);
	if (!String(Record, TEXT("path"), Path) || !String(Record, TEXT("sha256"), Hash) || Hash.Len() != 64) return false;
	for (TCHAR C : Hash)
		if (!FChar::IsHexDigit(C)) return false;
	return true;
}

bool ExportPath(const FObject& Parent, const TCHAR* Key, FString& Out)
{
	const FObject Record = Object(Parent, Key);
	FString Relative, SourcePath, Hash, Mode;
	int32 Width = 0, Height = 0;
	if (!String(Record, TEXT("path"), SourcePath) || !String(Record, TEXT("sha256"), Hash)
		|| Hash.Len() != 64 || !String(Record, TEXT("export"), Relative)
		|| !Integer(Record, TEXT("width"), Width) || Width <= 0
		|| !Integer(Record, TEXT("height"), Height) || Height <= 0
		|| !String(Record, TEXT("mode"), Mode) || Mode != TEXT("RGBA")
		|| !RokDataPaths::IsPackagedDataPath(Relative)) return false;
	Out = RokDataPaths::ResolveProjectPath(Relative);
	return IFileManager::Get().FileExists(*Out);
}

bool Curve(const FObject& Model, FRokStoneDepositData& Collision)
{
	const TArray<FValue>* Keys = nullptr;
	if (!Array(Model, TEXT("scale_curve"), Keys) || Keys->Num() < 2) return false;
	double PreviousTime = -DBL_MAX;
	for (int32 I = 0; I < Keys->Num(); ++I)
	{
		const FObject Key = (*Keys)[I].IsValid() && (*Keys)[I]->Type == EJson::Object ? (*Keys)[I]->AsObject() : nullptr;
		double Time = 0, Value = 0, In = 0, Out = 0;
		if (!Number(Key, TEXT("time"), Time) || Time <= PreviousTime || !Number(Key, TEXT("value"), Value)
			|| !Number(Key, TEXT("out"), Out)
			|| (!Number(Key, TEXT("in"), In) && !(I == 0 && Key.IsValid() && Key->HasTypedField<EJson::Null>(TEXT("in"))))) return false;
		const FKeyHandle Handle = Collision.ScaleCurve.AddKey(Time, Value);
		FRichCurveKey& Target = Collision.ScaleCurve.GetKey(Handle);
		Target.InterpMode = RCIM_Cubic;
		Target.TangentMode = RCTM_User;
		Target.ArriveTangent = In;
		Target.LeaveTangent = Out;
		PreviousTime = Time;
	}
	return true;
}

bool CheckpointMeshes(const FObject& Model, FRokWorldStructurePresentationData& Result)
{
	const FObject Record = Object(Model, TEXT("mesh_asset"));
	FString Relative, Hash;
	if (!String(Record, TEXT("export"), Relative)
		|| Relative != TEXT("Content/RokData/WorldStructurePresentation/checkpoint_mesh.json")
		|| !String(Record, TEXT("sha256"), Hash) || Hash.Len() != 64
		|| !RokDataPaths::IsDataPath(Relative)
		|| !SourceRecord(Model, TEXT("native_material_evidence"))) return false;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *RokDataPaths::ResolveProjectPath(Relative))
		|| RokTrainArmyMesh::Sha256(Bytes) != Hash.ToLower()) return false;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::ResolveProjectPath(Relative))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	FString Schema, Version, Space;
	int32 MeshCount = 0, VertexCount = 0, TriangleCount = 0;
	const TArray<FValue>* Meshes = nullptr;
	if (!String(Root, TEXT("schema"), Schema) || Schema != TEXT("rok.checkpoint_mesh.v1")
		|| !String(Root, TEXT("unity_version"), Version) || Version != TEXT("2019.4.28f1c1")
		|| !String(Root, TEXT("coordinate_space"), Space) || !Space.StartsWith(TEXT("Unity native prefab root"))
		|| !SourceRecord(Root, TEXT("native_witness"))
		|| !SourceRecord(Root, TEXT("native_screenshot"))
		|| !RokDataPaths::IsDataPath(Object(Root, TEXT("native_screenshot"))->GetStringField(TEXT("path")))
		|| !IFileManager::Get().FileExists(*RokDataPaths::ResolveProjectPath(Object(Root, TEXT("native_screenshot"))->GetStringField(TEXT("path"))))
		|| !Integer(Root, TEXT("mesh_count"), MeshCount) || MeshCount != 4
		|| !Integer(Root, TEXT("vertex_count"), VertexCount) || VertexCount != 8724
		|| !Integer(Root, TEXT("triangle_count"), TriangleCount) || TriangleCount != 5299
		|| !Array(Root, TEXT("meshes"), Meshes) || Meshes->Num() != MeshCount) return false;
	const FObject Sources = Object(Root, TEXT("source"));
	const FObject ManifestSources = Object(Model, TEXT("mesh_source"));
	for (const TCHAR* Key : {TEXT("prefab"), TEXT("fbx"), TEXT("material"), TEXT("texture"), TEXT("shader")})
	{
		if (!SourceRecord(Sources, Key) || !SourceRecord(ManifestSources, Key)) return false;
		if (Object(Sources, Key)->GetStringField(TEXT("path")) != Object(ManifestSources, Key)->GetStringField(TEXT("path"))
			|| Object(Sources, Key)->GetStringField(TEXT("sha256")) != Object(ManifestSources, Key)->GetStringField(TEXT("sha256"))) return false;
	}
	if (Object(Sources, TEXT("prefab"))->GetStringField(TEXT("path")) != Object(Model, TEXT("source"))->GetStringField(TEXT("path"))) return false;
	TSet<FString> RendererIds, FilterIds;
	int32 TotalVertices = 0, TotalTriangles = 0;
	for (const FValue& Value : *Meshes)
	{
		const FObject Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FRokWorldStructurePresentationMesh Mesh;
		FString FilterId;
		int32 Vertices = 0, Triangles = 0, SubMeshes = 0;
		bool bSelf = false, bHierarchy = false, bRenderer = false, bGroundOnly = false;
		const TArray<FValue>* Positions = nullptr;
		const TArray<FValue>* Normals = nullptr;
		const TArray<FValue>* UV = nullptr;
		const TArray<FValue>* Colors = nullptr;
		const TArray<FValue>* Indices = nullptr;
		const TArray<FValue>* Materials = nullptr;
		if (!String(Entry, TEXT("gameObject"), Mesh.Name)
			|| !String(Entry, TEXT("sourceMeshName"), Mesh.SourceMeshName)
			|| !String(Entry, TEXT("sourceMeshGuid"), Mesh.SourceMeshGuid)
			|| Mesh.SourceMeshGuid != TEXT("8051e1d671e7ca64e904093aadf2f537")
			|| !String(Entry, TEXT("sourceMeshFileId"), FilterId)
			|| !String(Entry, TEXT("sourceFilterFileId"), FilterId) || FilterIds.Contains(FilterId)
			|| !String(Entry, TEXT("sourceRendererFileId"), Mesh.SourceRendererId) || RendererIds.Contains(Mesh.SourceRendererId)
			|| !Boolean(Entry, TEXT("activeSelf"), bSelf)
			|| !Boolean(Entry, TEXT("activeInHierarchy"), bHierarchy)
			|| !Boolean(Entry, TEXT("rendererEnabled"), bRenderer)
			|| !Boolean(Entry, TEXT("groundLod0Only"), bGroundOnly) || bGroundOnly
			|| !Integer(Entry, TEXT("vertexCount"), Vertices) || Vertices <= 0 || Vertices > 20000
			|| !Integer(Entry, TEXT("triangleCount"), Triangles) || Triangles <= 0 || Triangles > 20000
			|| !Integer(Entry, TEXT("subMeshCount"), SubMeshes) || SubMeshes != 1
			|| !Array(Entry, TEXT("vertices"), Positions) || Positions->Num() != Vertices
			|| !Array(Entry, TEXT("normals"), Normals) || Normals->Num() != Vertices
			|| !Array(Entry, TEXT("uv0"), UV) || UV->Num() != Vertices
			|| !Array(Entry, TEXT("colors"), Colors) || Colors->Num() != Vertices
			|| !Array(Entry, TEXT("triangles"), Indices) || Indices->Num() != Triangles * 3
			|| !Array(Entry, TEXT("materials"), Materials) || Materials->Num() != 1) return false;
		Mesh.SourceId = Mesh.Name + TEXT("#") + FilterId;
		Mesh.SourceFilterId = FilterId;
		Mesh.bEnabled = bSelf && bHierarchy && bRenderer;
		const FObject Material = (*Materials)[0].IsValid() && (*Materials)[0]->Type == EJson::Object ? (*Materials)[0]->AsObject() : nullptr;
		FString MaterialName, SourceTexture, ShaderPath;
		int32 Queue = 0;
		if (!String(Material, TEXT("name"), MaterialName)
			|| !String(Material, TEXT("path"), Mesh.MaterialPath)
			|| !String(Material, TEXT("mainTexturePath"), SourceTexture)
			|| !String(Material, TEXT("shader"), Mesh.Shader)
			|| !String(Material, TEXT("shaderPath"), ShaderPath)
			|| !String(Material, TEXT("main_texture_export"), Mesh.TexturePath)
			|| Mesh.TexturePath != TEXT("Content/RokData/WorldStructurePresentation/checkpoint_mesh_albedo.png")
			|| !IFileManager::Get().FileExists(*RokDataPaths::ResolveProjectPath(Mesh.TexturePath))
			|| !Integer(Material, TEXT("renderQueue"), Queue) || Queue != 2993
			|| Mesh.Shader != TEXT("custom/city/city_wall")
			|| Mesh.MaterialPath != TEXT("Assets/BundleAssets/Map/Materials/shared_mapmaterials/build_Checkpoint_Big.mat")
			|| SourceTexture != TEXT("Assets/BundleAssets/Map/Materials/shared_mapmaterials/build_Checkpoint_Big.png")
			|| ShaderPath != TEXT("Assets/BundleAssets/Map/Building/customcitycity_wall.shader")) return false;
		Mesh.TexturePath = RokDataPaths::ResolveProjectPath(Mesh.TexturePath);
		Mesh.Positions.Reserve(Vertices);
		Mesh.Normals.Reserve(Vertices);
		Mesh.UV.Reserve(Vertices);
		Mesh.Colors.Reserve(Vertices);
		for (int32 I = 0; I < Vertices; ++I)
		{
			FVector P, N;
			FVector2D T;
			FLinearColor C;
			if (!ObjectVec3((*Positions)[I], P) || !ObjectVec3((*Normals)[I], N)
				|| N.IsNearlyZero() || !ObjectVec2((*UV)[I], T) || !ObjectColor((*Colors)[I], C)) return false;
			Mesh.Positions.Add(P);
			Mesh.Normals.Add(N);
			Mesh.UV.Add(T);
			Mesh.Colors.Add(C);
		}
		for (const FValue& Index : *Indices)
		{
			double NumberValue = 0;
			if (!Index.IsValid() || !Index->TryGetNumber(NumberValue) || !FMath::IsFinite(NumberValue)
				|| NumberValue < 0 || NumberValue >= Vertices || FMath::FloorToDouble(NumberValue) != NumberValue) return false;
			Mesh.Indices.Add(static_cast<int32>(NumberValue));
		}
		RendererIds.Add(Mesh.SourceRendererId);
		FilterIds.Add(FilterId);
		TotalVertices += Vertices;
		TotalTriangles += Triangles;
		Result.Meshes.Add(MoveTemp(Mesh));
	}
	return TotalVertices == VertexCount && TotalTriangles == TriangleCount;
}

int32 ExpectedItems(const FString& ModelId)
{
	if (ModelId == TEXT("build_Checkpoint_Big")) return 4;
	if (ModelId == TEXT("build_Rssgold_Big")) return 13;
	if (ModelId == TEXT("build_BarbarianCitadel_Big") || ModelId == TEXT("build_HolyLand1_Big")
		|| ModelId == TEXT("build_HolyLand2_Big") || ModelId.StartsWith(TEXT("build_alliance_100"))) return 1;
	return 0;
}

bool Parse(const FString& Text, const FString& RequestedModelId, FRokWorldStructurePresentationData& Result)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	int32 Version = 0, Units = 0;
	FString Space, Material;
	if (!Integer(Root, TEXT("version"), Version) || Version != 1
		|| !String(Root, TEXT("coordinate_space"), Space) || Space != TEXT("unity_native")
		|| !Integer(Root, TEXT("unity_units_to_ue_cm"), Units) || Units <= 0
		|| !String(Root, TEXT("material_contract"), Material) || Material != TEXT("M_RokVillagePresentationDay")) return false;
	const FObject Model = Object(Object(Root, TEXT("models")), *RequestedModelId);
	if (!Model.IsValid() || !SourceRecord(Model, TEXT("source"))) return false;
	const TArray<FValue>* Groups = nullptr;
	const TArray<FValue>* Items = nullptr;
	const TArray<FValue>* Unsupported = nullptr;
	if (!Array(Model, TEXT("groups"), Groups) || !Array(Model, TEXT("items"), Items)
		|| !Array(Model, TEXT("unsupported"), Unsupported) || !Unsupported->IsEmpty()
		|| Items->Num() != ExpectedItems(RequestedModelId)
		|| !Vec2(Model, TEXT("bounds_size"), Result.CollisionData.BoundsSize)
		|| Result.CollisionData.BoundsSize.X <= 0 || Result.CollisionData.BoundsSize.Y <= 0) return false;
	double MaxScale = 0;
	if (!Number(Model, TEXT("max_scale"), MaxScale) || MaxScale <= 0 || !Curve(Model, Result.CollisionData)) return false;
	Result.ModelId = RequestedModelId;
	Result.MaxScale = MaxScale;
	Result.UnitsToCentimeters = Units;
	Result.CollisionData.MaxScale = MaxScale;
	Result.CollisionData.UnitsToCentimeters = Units;

	TMap<FString, int32> GroupIndices;
	bool bHasInitiallyInactiveGroup = false;
	for (const FValue& Value : *Groups)
	{
		const FObject Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FRokWorldStructurePresentationGroup Group;
		int32 Shift = 0;
		double Start = 0, End = 0;
		bool bAutoRegister = false, bEnabled = false;
		if (!String(Entry, TEXT("id"), Group.SourceId) || GroupIndices.Contains(Group.SourceId)
			|| !String(Entry, TEXT("name"), Group.Name)
			|| !SourceRecord(Entry, TEXT("source"))
			|| !Integer(Entry, TEXT("sibling_index"), Group.SiblingIndex)
			|| !Vec3(Entry, TEXT("position"), Group.Position)
			|| !Vec3(Entry, TEXT("shift_origin"), Group.ShiftOrigin)
			|| !Vec2(Entry, TEXT("size"), Group.Size) || Group.Size.X <= 0 || Group.Size.Y <= 0
			|| !Integer(Entry, TEXT("priority"), Group.Priority)
			|| !Integer(Entry, TEXT("shift_pos"), Shift) || (Shift != 0 && Shift != 1)
			|| !Number(Entry, TEXT("shift_start_scale"), Start)
			|| !Number(Entry, TEXT("shift_end_scale"), End)
			|| !Boolean(Entry, TEXT("auto_register"), bAutoRegister)
			|| !Boolean(Entry, TEXT("active"), Group.bActive)
			|| !Boolean(Entry, TEXT("enabled"), bEnabled)) return false;
		Group.bShiftPosition = Shift == 1;
		Group.ShiftStartScale = Start;
		Group.ShiftEndScale = End;
		if (!bAutoRegister || !bEnabled) return false;
		bHasInitiallyInactiveGroup |= !Group.bActive;
		GroupIndices.Add(Group.SourceId, Result.Groups.Num());
		Result.Groups.Add(MoveTemp(Group));
	}
	if (bHasInitiallyInactiveGroup && !SourceRecord(Model, TEXT("native_activity_evidence"))) return false;

	// Native registration order is required whenever GridCollide has more than
	// one candidate. An absent probe cannot be replaced by prefab sibling order.
	if (Result.Groups.Num() > 1)
	{
		const TArray<FValue>* Order = nullptr;
		if (!Array(Model, TEXT("grid_order"), Order) || Order->Num() != Result.Groups.Num()
			|| !SourceRecord(Model, TEXT("native_order_evidence"))) return false;
		TArray<FRokWorldStructurePresentationGroup> Ordered;
		TSet<FString> Seen;
		for (const FValue& Value : *Order)
		{
			FString Id;
			if (!Value.IsValid() || !Value->TryGetString(Id) || Seen.Contains(Id) || !GroupIndices.Contains(Id)) return false;
			Ordered.Add(Result.Groups[GroupIndices[Id]]);
			Seen.Add(Id);
		}
		Result.Groups = MoveTemp(Ordered);
		GroupIndices.Reset();
		for (int32 I = 0; I < Result.Groups.Num(); ++I) GroupIndices.Add(Result.Groups[I].SourceId, I);
	}
	Result.CollisionData.Items.Reserve(Result.Groups.Num());
	for (const auto& Group : Result.Groups)
	{
		FRokStoneDepositItem Collision;
		Collision.SourceTransformId = Group.SourceId;
		Collision.SourceName = Group.Name;
		Collision.Position = Group.Position;
		Collision.Size = Group.Size;
		Collision.Priority = Group.Priority;
		Collision.SiblingIndex = Group.SiblingIndex;
		Result.CollisionData.Items.Add(MoveTemp(Collision));
	}
	TSet<FString> SeenItems;
	for (const FValue& Value : *Items)
	{
		const FObject Entry = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		FRokWorldStructurePresentationItem Item;
		FString ShaderGuid;
		int32 SortLayerId = 0;
		bool bFlipX = false, bFlipY = false;
		bool bActiveInHierarchy = false;
		if (!String(Entry, TEXT("source_id"), Item.SourceId) || SeenItems.Contains(Item.SourceId)
			|| !String(Entry, TEXT("source_renderer_id"), Item.SourceRendererId)
			|| !String(Entry, TEXT("name"), Item.Name)
			|| !SourceRecord(Entry, TEXT("source")) || !SourceRecord(Entry, TEXT("material"))
			|| !String(Entry, TEXT("shader_mode"), Item.ShaderMode)
			|| !String(Entry, TEXT("shader_guid"), ShaderGuid)
			|| !Vec3(Entry, TEXT("position"), Item.Position)
			|| !Vec3(Entry, TEXT("sprite_position"), Item.SpritePosition)
			|| !Vec3(Entry, TEXT("right"), Item.Right) || Item.Right.IsNearlyZero()
			|| !Vec3(Entry, TEXT("up"), Item.Up) || Item.Up.IsNearlyZero()
			|| !Vec2(Entry, TEXT("size"), Item.Size) || Item.Size.X <= 0 || Item.Size.Y <= 0
			|| !Vec2(Entry, TEXT("pivot"), Item.Pivot)
			|| !Color(Entry, TEXT("color1"), Item.Color1)
			|| !Color(Entry, TEXT("color2"), Item.Color2)
			|| !Color(Entry, TEXT("renderer_color"), Item.RendererColor)
			|| !ExportPath(Entry, TEXT("texture"), Item.TexturePath)
			|| !Integer(Entry, TEXT("sort_layer_id"), SortLayerId)
			|| !Integer(Entry, TEXT("sort_layer_order"), Item.SortLayerOrder)
			|| !Integer(Entry, TEXT("sort_order"), Item.SortOrder)
			|| !Boolean(Entry, TEXT("enabled"), Item.bEnabled)
			|| !Boolean(Entry, TEXT("active_in_hierarchy"), bActiveInHierarchy)
			|| !Boolean(Entry, TEXT("ground_lod0_only"), Item.bGroundLod0Only)
			|| !Boolean(Entry, TEXT("flip_x"), bFlipX)
			|| !Boolean(Entry, TEXT("flip_y"), bFlipY)
			|| bFlipX || bFlipY) return false;
		Item.bEnabled = Item.bEnabled && bActiveInHierarchy;
		if (Item.ShaderMode == TEXT("custom_map_building_mask"))
		{
			if (ShaderGuid != TEXT("45d662578936c924fbe7a09119b72a9c")
				|| !SourceRecord(Entry, TEXT("shader")) || !ExportPath(Entry, TEXT("mask"), Item.MaskPath)) return false;
			Item.Shader = Object(Entry, TEXT("shader"))->GetStringField(TEXT("path"));
			if (Item.Shader != TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/map_building_with_mask-5.shader")
				|| !Object(Entry, TEXT("mask"))->GetStringField(TEXT("path")).EndsWith(TEXT("_mask.png"))) return false;
		}
		else if (Item.ShaderMode == TEXT("unity_builtin_10750"))
		{
			FString FileId;
			const FValue* Mask = Entry->Values.Find(TEXT("mask"));
			const FValue* Shader = Entry->Values.Find(TEXT("shader"));
			if (ShaderGuid != TEXT("0000000000000000f000000000000000")
				|| !String(Entry, TEXT("shader_file_id"), FileId) || FileId != TEXT("10750")
				|| !Mask || !Mask->IsValid() || (*Mask)->Type != EJson::Null
				|| !Shader || !Shader->IsValid() || (*Shader)->Type != EJson::Null) return false;
		}
		else return false;
		const FValue* GroupId = Entry->Values.Find(TEXT("group_id"));
		if (!GroupId || !GroupId->IsValid()) return false;
		if ((*GroupId)->Type == EJson::String)
		{
			Item.GroupId = (*GroupId)->AsString();
			const int32* Index = GroupIndices.Find(Item.GroupId);
			if (!Index || !Item.Position.Equals(Result.Groups[*Index].Position, 0.00001)) return false;
			Item.GroupIndex = *Index;
		}
		else if ((*GroupId)->Type != EJson::Null || !Item.Position.IsNearlyZero()) return false;
		SeenItems.Add(Item.SourceId);
		Result.Items.Add(MoveTemp(Item));
	}
	if (RequestedModelId == TEXT("build_Checkpoint_Big"))
	{
		int32 MeshCount = 0, VertexCount = 0, TriangleCount = 0;
		if (!Integer(Model, TEXT("mesh_count"), MeshCount) || MeshCount != 4
			|| !Integer(Model, TEXT("mesh_vertex_count"), VertexCount) || VertexCount != 8724
			|| !Integer(Model, TEXT("mesh_triangle_count"), TriangleCount) || TriangleCount != 5299
			|| !CheckpointMeshes(Model, Result)) return false;
	}
	else if (Model->HasField(TEXT("mesh_asset"))) return false;
	return true;
}
}

bool FRokWorldStructurePresentationData::Load(const FString& InModelId)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::Resolve(TEXT("world_structure_presentation.json"))))
	{
		*this = FRokWorldStructurePresentationData();
		return false;
	}
	return LoadJson(Text, InModelId);
}

bool FRokWorldStructurePresentationData::LoadJson(const FString& Text, const FString& InModelId)
{
	FRokWorldStructurePresentationData Parsed;
	if (ExpectedItems(InModelId) == 0 || !Parse(Text, InModelId, Parsed))
	{
		*this = FRokWorldStructurePresentationData();
		return false;
	}
	*this = MoveTemp(Parsed);
	return true;
}

float FRokWorldStructurePresentationData::EvaluateScale(float Dxf) const
{
	return FMath::IsFinite(Dxf) ? CollisionData.EvaluateScale(Dxf) : 0;
}
