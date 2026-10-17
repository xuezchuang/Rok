#include "RokLocalConstructionScaffoldComponent.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityConstructionGrid.h"
#include "RokCitySpriteActor.h"
#include "RokDataPaths.h"
#include "RokTrainArmyMeshData.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr TCHAR NativeHash[] = TEXT("b0855cba2b8f68fd620fa6c00e9417672af3faa7e5de25d35a75a3955d7c9e0b");
constexpr TCHAR PreparedHash[] = TEXT("9759852ea999f2383bb5c8edf4807bd3c8d8074db68ed220f40ab8fa0d6d4886");
constexpr TCHAR AnimationHash[] = TEXT("0f427de38a415d1fd532b2719a394a14f60e423aef8c4fce5953d8780defe391");
constexpr TCHAR ImportHash[] = TEXT("ac51c5f855f0907437b05055632efdead05c8e329ce7d377fc717ed737e62b6c");
constexpr TCHAR ContractHash[] = TEXT("04e3d29acfef05aa791d262b62ff4193954a5edf2629c823aa53907a31b17013");
constexpr TCHAR AssetRoot[] = TEXT("/Game/RokUnity/City/ConstructionScaffold/V1/");
constexpr TCHAR RootNode[] = TEXT("Build_shelf_5[1]");
constexpr double StopSeconds = 2.0;

using JObject = TSharedPtr<FJsonObject>;
using JArray = TArray<TSharedPtr<FJsonValue>>;

bool Object(const JObject& Owner, const TCHAR* Key, JObject& Out)
{
	const JObject* Value = nullptr;
	if (!Owner.IsValid() || !Owner->TryGetObjectField(Key, Value) || !Value || !Value->IsValid()) return false;
	Out = *Value;
	return true;
}

bool Array(const JObject& Owner, const TCHAR* Key, const JArray*& Out, int32 Expected)
{
	return Owner.IsValid() && Owner->TryGetArrayField(Key, Out) && Out && Out->Num() == Expected;
}

bool String(const JObject& Owner, const TCHAR* Key, FString& Out)
{
	return Owner.IsValid() && Owner->TryGetStringField(Key, Out) && Out.Len() <= 1024;
}

bool Number(const JObject& Owner, const TCHAR* Key, double& Out)
{
	return Owner.IsValid() && Owner->TryGetNumberField(Key, Out) && FMath::IsFinite(Out);
}

bool Integer(const JObject& Owner, const TCHAR* Key, int32& Out)
{
	double Value = 0.0;
	if (!Number(Owner, Key, Value) || Value < MIN_int32 || Value > MAX_int32
		|| FMath::FloorToDouble(Value) != Value) return false;
	Out = static_cast<int32>(Value);
	return true;
}

bool JsonObject(const TSharedPtr<FJsonValue>& Value, JObject& Out)
{
	if (!Value.IsValid() || Value->Type != EJson::Object) return false;
	Out = Value->AsObject();
	return Out.IsValid();
}

bool Vector(const JObject& Owner, const TCHAR* Key, FVector3d& Out)
{
	JObject Value;
	return Object(Owner, Key, Value) && Number(Value, TEXT("x"), Out.X)
		&& Number(Value, TEXT("y"), Out.Y) && Number(Value, TEXT("z"), Out.Z)
		&& FMath::Abs(Out.X) < 10000 && FMath::Abs(Out.Y) < 10000 && FMath::Abs(Out.Z) < 10000;
}

bool Rotation(const JObject& Owner, FQuat4d& Out)
{
	JObject Value;
	return Object(Owner, TEXT("local_rotation"), Value)
		&& Number(Value, TEXT("x"), Out.X) && Number(Value, TEXT("y"), Out.Y)
		&& Number(Value, TEXT("z"), Out.Z) && Number(Value, TEXT("w"), Out.W)
		&& FMath::Abs(Out.SizeSquared() - 1.0) < 0.01;
}

bool LoadPinned(const FString& Relative, const TCHAR* Expected, JObject& Out, FString& Error)
{
	const FString Path = RokDataPaths::ResolveProjectPath(Relative);
	TArray<uint8> Bytes;
	FString Text;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() > 32 * 1024 * 1024
		|| RokTrainArmyMesh::Sha256(Bytes) != Expected
		|| !FFileHelper::LoadFileToString(Text, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out.IsValid())
	{
		Error = FString::Printf(TEXT("Sealed scaffold JSON missing, changed or invalid: %s"), *Relative);
		return false;
	}
	return true;
}

bool AssetHash(const FString& PackagePath, const FString& Expected)
{
	if (!PackagePath.StartsWith(AssetRoot) || Expected.Len() != 64) return false;
	TArray<uint8> Bytes;
	const FString File = FPaths::ProjectContentDir()
		/ (PackagePath.RightChop(6) + TEXT(".uasset"));
	return FFileHelper::LoadFileToArray(Bytes, *File) && RokTrainArmyMesh::Sha256(Bytes) == Expected;
}

FString ObjectPath(const FString& PackagePath)
{
	return PackagePath + TEXT(".") + FPaths::GetBaseFilename(PackagePath);
}

bool ReadNode(const JObject& Value, FRokScaffoldSourcePose& Out)
{
	bool ActiveSelf = false, ActiveHierarchy = false;
	return String(Value, TEXT("path"), Out.Path)
		&& String(Value, TEXT("parent_path"), Out.ParentPath)
		&& !Out.Path.IsEmpty() && !Out.Path.Contains(TEXT(".."))
		&& Value->TryGetBoolField(TEXT("active_self"), ActiveSelf) && ActiveSelf
		&& Value->TryGetBoolField(TEXT("active_in_hierarchy"), ActiveHierarchy) && ActiveHierarchy
		&& Vector(Value, TEXT("local_position"), Out.Position)
		&& Vector(Value, TEXT("local_scale"), Out.Scale)
		&& Rotation(Value, Out.Rotation);
}

bool ReadCurve(const JObject& Value, FRokScaffoldCurve& Out)
{
	FString Path, Interpolation;
	const JArray* Keys = nullptr;
	if (!String(Value, TEXT("path"), Path) || Path != TEXT("sprite1")
		|| !String(Value, TEXT("property"), Out.Property)
		|| !(Out.Property == TEXT("m_LocalPosition.x") || Out.Property == TEXT("m_LocalPosition.y")
			|| Out.Property == TEXT("m_LocalPosition.z") || Out.Property == TEXT("m_LocalScale.x")
			|| Out.Property == TEXT("m_LocalScale.y") || Out.Property == TEXT("m_LocalScale.z"))
		|| !String(Value, TEXT("interpolation"), Interpolation)
		|| Interpolation != TEXT("unweighted_cubic_Hermite") || !Array(Value, TEXT("keys"), Keys, 4)) return false;
	for (const TSharedPtr<FJsonValue>& Item : *Keys)
	{
		JObject Key;
		FRokScaffoldCurveKey Parsed;
		FString WeightedMode;
		if (!JsonObject(Item, Key) || !Number(Key, TEXT("time"), Parsed.Time)
			|| !Number(Key, TEXT("value"), Parsed.Value)
			|| !Number(Key, TEXT("in_tangent"), Parsed.InTangent)
			|| !Number(Key, TEXT("out_tangent"), Parsed.OutTangent)
			|| !String(Key, TEXT("weighted_mode"), WeightedMode) || WeightedMode != TEXT("None")
			|| Parsed.Time < 0.0 || FMath::Abs(Parsed.Value) > 10000.0
			|| !FMath::IsNearlyZero(Parsed.InTangent, 1e-10)
			|| !FMath::IsNearlyZero(Parsed.OutTangent, 1e-10)
			|| (Out.Keys.Num() && Parsed.Time <= Out.Keys.Last().Time)) return false;
		Out.Keys.Add(Parsed);
	}
	return true;
}

bool LoadAssetEvidence(const JObject& Report, FRokScaffoldSourceBundle& Bundle, FString& Error)
{
	const JArray* Assets = nullptr;
	if (!Array(Report, TEXT("assets"), Assets, 24)) return false;
	TMap<FString, JObject> ByPackage;
	for (const TSharedPtr<FJsonValue>& Entry : *Assets)
	{
		JObject Asset;
		FString Package, Sha;
		if (!JsonObject(Entry, Asset) || !String(Asset, TEXT("asset"), Package)
			|| !String(Asset, TEXT("uasset_sha256"), Sha) || ByPackage.Contains(Package)
			|| !AssetHash(Package, Sha)) return false;
		ByPackage.Add(Package, Asset);
	}
	for (FRokScaffoldLayerSource& Layer : Bundle.Layers)
	{
		const FString Paths[3] = {Layer.MeshPath, Layer.SpriteTexturePath, Layer.MaskTexturePath};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const JObject* Evidence = ByPackage.Find(Paths[Index]);
			FString Kind;
			if (!Evidence || !String(*Evidence, TEXT("kind"), Kind)
				|| Kind != (Index == 0 ? TEXT("mesh") : TEXT("texture"))) return false;
			if (Index == 0)
			{
				int32 Imported = 0, Source = 0, UV = 0;
				bool TopologyProved = false;
				if (!Integer(*Evidence, TEXT("imported_triangles"), Imported)
					|| !Integer(*Evidence, TEXT("source_triangles"), Source)
					|| !Integer(*Evidence, TEXT("uv_channels"), UV)
					|| !(*Evidence)->TryGetBoolField(TEXT("face_topology_proved"), TopologyProved)
					|| !TopologyProved || Source != Layer.SourceTriangles
					|| Imported <= 0 || Imported > Source || UV != 1) return false;
				Layer.ImportedTriangles = Imported;
			}
		}
	}
	if (ByPackage.Num() != 24)
	{
		Error = TEXT("Scaffold import contains unexpected assets");
		return false;
	}
	return true;
}
}

namespace RokLocalConstructionScaffold
{
bool LoadValidatedBundle(FRokScaffoldSourceBundle& Out, FString& Error)
{
	Out = FRokScaffoldSourceBundle();
	Error = TEXT("Sealed scaffold source bundle is incomplete or inconsistent");
	JObject Native, Prepared, Animation, Imported;
	if (!LoadPinned(TEXT("Saved/UnityConstructionScaffoldProbe/OutputRuns/20261008_223208_818_37556/native_scaffold_render.json"), NativeHash, Native, Error)
		|| !LoadPinned(TEXT("Saved/RokUnityMigration/ConstructionScaffoldAssets/prepared_manifest.json"), PreparedHash, Prepared, Error)
		|| !LoadPinned(TEXT("Saved/RokUnityMigration/ConstructionScaffoldAssets/animation_data.json"), AnimationHash, Animation, Error)
		|| !LoadPinned(TEXT("Saved/RokUnityMigration/ConstructionScaffoldAssets/ue_import_report.json"), ImportHash, Imported, Error)) return false;
	FString Schema, Sha;
	bool Passed = false;
	if (!String(Native, TEXT("schema"), Schema) || Schema != TEXT("rok.construction_scaffold_native_render.v1")
		|| !Native->TryGetBoolField(TEXT("passed"), Passed) || !Passed
		|| !String(Native, TEXT("source_contract_sha256"), Sha) || Sha != ContractHash
		|| !String(Prepared, TEXT("schema"), Schema) || Schema != TEXT("rok.construction_scaffold.assets_prepared.v1")
		|| !String(Prepared, TEXT("native_sha256"), Sha) || Sha != NativeHash
		|| !String(Prepared, TEXT("contract_sha256"), Sha) || Sha != ContractHash
		|| !String(Prepared, TEXT("ue_root"), Sha) || Sha + TEXT("/") != AssetRoot
		|| !String(Animation, TEXT("schema"), Schema) || Schema != TEXT("rok.construction_scaffold.animation_data.v1")
		|| !String(Animation, TEXT("native_sha256"), Sha) || Sha != NativeHash
		|| !String(Animation, TEXT("contract_sha256"), Sha) || Sha != ContractHash
		|| !String(Imported, TEXT("schema"), Schema) || Schema != TEXT("rok.construction_scaffold.ue_import.v1")
		|| !Imported->TryGetBoolField(TEXT("passed"), Passed) || !Passed
		|| !String(Imported, TEXT("prepared_sha256"), Sha) || Sha != PreparedHash
		|| !String(Imported, TEXT("animation_sha256"), Sha) || Sha != AnimationHash) return false;

	const JArray *NativeNodes = nullptr, *Layers = nullptr, *Clips = nullptr, *SourceAnims = nullptr;
	if (!Array(Native, TEXT("source_nodes"), NativeNodes, 16)
		|| !Array(Native, TEXT("source_animation_components"), SourceAnims, 4)
		|| !Array(Prepared, TEXT("layers"), Layers, 8)
		|| !Array(Animation, TEXT("clips"), Clips, 4)) return false;
	TSet<FString> Paths;
	for (const TSharedPtr<FJsonValue>& Value : *NativeNodes)
	{
		JObject Item;
		FRokScaffoldSourcePose Pose;
		if (!JsonObject(Value, Item) || !ReadNode(Item, Pose) || Paths.Contains(Pose.Path)
			|| (Pose.Path != RootNode && !Paths.Contains(Pose.ParentPath))) return false;
		Paths.Add(Pose.Path);
		Out.Nodes.Add(MoveTemp(Pose));
	}
	if (Out.Nodes[0].Path != RootNode || !Out.Nodes[0].ParentPath.IsEmpty()) return false;
	TSet<FString> LayerNodes, AssetPaths;
	for (const TSharedPtr<FJsonValue>& Value : *Layers)
	{
		JObject Row, Sources, Sprite, Mask;
		FRokScaffoldLayerSource Layer;
		FString Name;
		if (!JsonObject(Value, Row) || !String(Row, TEXT("node"), Layer.NodePath)
			|| !String(Row, TEXT("name"), Name) || !String(Row, TEXT("ue_mesh_asset"), Layer.MeshPath)
			|| !Integer(Row, TEXT("triangle_count"), Layer.SourceTriangles)
			|| !Object(Row, TEXT("sources"), Sources)
			|| !Object(Sources, TEXT("sprite"), Sprite) || !Object(Sources, TEXT("mask"), Mask)
			|| !String(Sprite, TEXT("ue_asset"), Layer.SpriteTexturePath)
			|| !String(Mask, TEXT("ue_asset"), Layer.MaskTexturePath)
			|| !Paths.Contains(Layer.NodePath) || LayerNodes.Contains(Layer.NodePath)
			|| Layer.SourceTriangles <= 0 || !Layer.MeshPath.EndsWith(FString(TEXT("/SM_")) + Name + TEXT("_SourceSpriteMesh"))
			|| Layer.SpriteTexturePath != FString(AssetRoot) + TEXT("T_") + Name + TEXT("_SpriteRaw")
			|| Layer.MaskTexturePath != FString(AssetRoot) + TEXT("T_") + Name + TEXT("_MaskRaw")) return false;
		const JArray *SpriteSize = nullptr, *MaskSize = nullptr;
		if (!Array(Sprite, TEXT("size"), SpriteSize, 2) || !Array(Mask, TEXT("size"), MaskSize, 2)) return false;
		double Width = 0.0, Height = 0.0;
		if (!(*SpriteSize)[0]->TryGetNumber(Width) || !(*SpriteSize)[1]->TryGetNumber(Height)
			|| Width < 1 || Height < 1 || Width > 4096 || Height > 4096
			|| FMath::FloorToDouble(Width) != Width || FMath::FloorToDouble(Height) != Height
			|| (*MaskSize)[0]->AsNumber() != Width || (*MaskSize)[1]->AsNumber() != Height) return false;
		Layer.TextureSize = FIntPoint(static_cast<int32>(Width), static_cast<int32>(Height));
		LayerNodes.Add(Layer.NodePath);
		if (AssetPaths.Contains(Layer.MeshPath) || AssetPaths.Contains(Layer.SpriteTexturePath)
			|| AssetPaths.Contains(Layer.MaskTexturePath)) return false;
		AssetPaths.Add(Layer.MeshPath); AssetPaths.Add(Layer.SpriteTexturePath); AssetPaths.Add(Layer.MaskTexturePath);
		Out.Layers.Add(MoveTemp(Layer));
	}
	if (AssetPaths.Num() != 24 || !LoadAssetEvidence(Imported, Out, Error)) return false;
	TSet<FString> AnimatedPaths;
	int32 CurveCount = 0, KeyCount = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		JObject ClipObject, SourceAnimation;
		FRokScaffoldClip Clip;
		FString Name, CurvePath;
		bool Enabled = true, AutoPlay = false;
		double Length = 0.0;
		if (!JsonObject((*Clips)[Index], ClipObject) || !JsonObject((*SourceAnims)[Index], SourceAnimation)
			|| !String(ClipObject, TEXT("name"), Name)
			|| !String(SourceAnimation, TEXT("node"), Clip.GroupPath)
			|| !String(SourceAnimation, TEXT("clip"), CurvePath) || CurvePath != Name
			|| !SourceAnimation->TryGetBoolField(TEXT("source_enabled"), Enabled) || Enabled
			|| !SourceAnimation->TryGetBoolField(TEXT("source_play_automatically"), AutoPlay) || !AutoPlay
			|| !Paths.Contains(Clip.GroupPath) || AnimatedPaths.Contains(Clip.GroupPath)
			|| !Clip.GroupPath.EndsWith(FString(TEXT("/")) + Name + TEXT("[") + FString::FromInt(Index) + TEXT("]"))
			|| !Number(ClipObject, TEXT("length"), Length) || Length < 1.8 || Length > 1.84) return false;
		Clip.LengthSeconds = Length;
		const JArray* Curves = nullptr;
		if (!Array(ClipObject, TEXT("curves"), Curves, Index == 0 ? 6 : 3)) return false;
		TSet<FString> Properties;
		for (const TSharedPtr<FJsonValue>& CurveValue : *Curves)
		{
			JObject CurveObject;
			FRokScaffoldCurve Curve;
			if (!JsonObject(CurveValue, CurveObject) || !ReadCurve(CurveObject, Curve)
				|| Properties.Contains(Curve.Property) || Curve.Keys.Last().Time > Length + 0.0001) return false;
			Properties.Add(Curve.Property);
			Clip.Curves.Add(MoveTemp(Curve));
			++CurveCount;
			KeyCount += 4;
		}
		for (const TCHAR* Axis : {TEXT("m_LocalPosition.x"), TEXT("m_LocalPosition.y"), TEXT("m_LocalPosition.z")})
		{
			if (!Properties.Contains(Axis)) return false;
		}
		if (Index == 0 && (!Properties.Contains(TEXT("m_LocalScale.x"))
			|| !Properties.Contains(TEXT("m_LocalScale.y")) || !Properties.Contains(TEXT("m_LocalScale.z")))) return false;
		AnimatedPaths.Add(Clip.GroupPath);
		Out.Clips.Add(MoveTemp(Clip));
	}
	if (CurveCount != 15 || KeyCount != 60) return false;
	// Editor texture/mesh accessors can report compilation placeholders. Finish
	// exactly these sealed objects before comparing their runtime render data.
	// This neither saves packages nor changes source or production scene state.
	if (!IsInGameThread())
	{
		Error = TEXT("Scaffold asset loading and targeted compilation require the game thread");
		return false;
	}
	struct FLoadedLayer
	{
		UStaticMesh* Mesh = nullptr;
		UTexture2D* Sprite = nullptr;
		UTexture2D* Mask = nullptr;
		FIntPoint SpriteBefore = FIntPoint::ZeroValue;
		FIntPoint MaskBefore = FIntPoint::ZeroValue;
		int32 MeshLodsBefore = -1;
		int32 MeshTrianglesBefore = -1;
		int32 MeshUvsBefore = -1;
	};
	TArray<FLoadedLayer> Loaded;
	Loaded.Reserve(Out.Layers.Num());
#if WITH_EDITOR
	TArray<UObject*> TargetedAssets;
	TargetedAssets.Reserve(Out.Layers.Num() * 3);
#endif
	for (const FRokScaffoldLayerSource& Layer : Out.Layers)
	{
		FLoadedLayer& Row = Loaded.AddDefaulted_GetRef();
		Row.Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath(Layer.MeshPath));
		Row.Sprite = LoadObject<UTexture2D>(nullptr, *ObjectPath(Layer.SpriteTexturePath));
		Row.Mask = LoadObject<UTexture2D>(nullptr, *ObjectPath(Layer.MaskTexturePath));
		if (!IsValid(Row.Mesh) || !IsValid(Row.Sprite) || !IsValid(Row.Mask))
		{
			Error = FString::Printf(TEXT("Scaffold asset class/load mismatch: %s mesh=%d sprite=%d mask=%d"),
				*Layer.NodePath, IsValid(Row.Mesh) ? 1 : 0, IsValid(Row.Sprite) ? 1 : 0, IsValid(Row.Mask) ? 1 : 0);
			return false;
		}
		Row.MeshLodsBefore = Row.Mesh->GetNumLODs();
		Row.MeshTrianglesBefore = Row.Mesh->GetNumTriangles(0);
		Row.MeshUvsBefore = Row.Mesh->GetNumTexCoords(0);
		Row.SpriteBefore = FIntPoint(Row.Sprite->GetSizeX(), Row.Sprite->GetSizeY());
		Row.MaskBefore = FIntPoint(Row.Mask->GetSizeX(), Row.Mask->GetSizeY());
#if WITH_EDITOR
		TargetedAssets.Add(Row.Mesh);
		TargetedAssets.Add(Row.Sprite);
		TargetedAssets.Add(Row.Mask);
#endif
	}
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishCompilationForObjects(TargetedAssets);
#endif
	for (int32 Index = 0; Index < Out.Layers.Num(); ++Index)
	{
		const FRokScaffoldLayerSource& Layer = Out.Layers[Index];
		const FLoadedLayer& Row = Loaded[Index];
		const int32 Lods = Row.Mesh->GetNumLODs();
		const int32 Triangles = Row.Mesh->GetNumTriangles(0);
		const int32 Uvs = Row.Mesh->GetNumTexCoords(0);
		const FIntPoint SpriteSize(Row.Sprite->GetSizeX(), Row.Sprite->GetSizeY());
		const FIntPoint MaskSize(Row.Mask->GetSizeX(), Row.Mask->GetSizeY());
		if (Lods != 1 || Triangles != Layer.ImportedTriangles || Uvs != 1
			|| SpriteSize != Layer.TextureSize || MaskSize != Layer.TextureSize)
		{
			Error = FString::Printf(TEXT("Scaffold asset render data differ at %s: mesh LOD/tri/UV %d/%d/%d -> %d/%d/%d expected 1/%d/1; sprite %s -> %s, mask %s -> %s expected %s"),
				*Layer.NodePath, Row.MeshLodsBefore, Row.MeshTrianglesBefore, Row.MeshUvsBefore,
				Lods, Triangles, Uvs, Layer.ImportedTriangles,
				*Row.SpriteBefore.ToString(), *SpriteSize.ToString(),
				*Row.MaskBefore.ToString(), *MaskSize.ToString(), *Layer.TextureSize.ToString());
			return false;
		}
	}
	Error.Reset();
	return true;
}

double EvaluateCurve(const FRokScaffoldCurve& Curve, double ClipSeconds)
{
	if (Curve.Keys.IsEmpty() || !FMath::IsFinite(ClipSeconds)) return 0.0;
	if (ClipSeconds <= Curve.Keys[0].Time) return Curve.Keys[0].Value;
	for (int32 Index = 1; Index < Curve.Keys.Num(); ++Index)
	{
		const FRokScaffoldCurveKey& A = Curve.Keys[Index - 1];
		const FRokScaffoldCurveKey& B = Curve.Keys[Index];
		if (ClipSeconds <= B.Time)
		{
			const double Span = B.Time - A.Time;
			return FMath::CubicInterp(A.Value, A.OutTangent * Span,
				B.Value, B.InTangent * Span, (ClipSeconds - A.Time) / Span);
		}
	}
	return Curve.Keys.Last().Value;
}

bool SampleBundle(const FRokScaffoldSourceBundle& Bundle, double ElapsedSeconds,
	FRokScaffoldSample& Out, FString& Error)
{
	Out = FRokScaffoldSample();
	if (!FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds < 0.0
		|| Bundle.Nodes.Num() != 16 || Bundle.Layers.Num() != 8 || Bundle.Clips.Num() != 4)
	{
		Error = TEXT("Scaffold sampling requires the complete source bundle and a finite nonnegative clock");
		return false;
	}
	Out.SourceElapsedSeconds = ElapsedSeconds;
	Out.bFourAnimationsEnabled = ElapsedSeconds < StopSeconds;
	Out.bParticleNodeActiveInSource = ElapsedSeconds < StopSeconds;
	Out.Nodes = Bundle.Nodes;
	for (const FRokScaffoldClip& Clip : Bundle.Clips)
	{
		const FString SpritePath = Clip.GroupPath + TEXT("/sprite1[0]");
		FRokScaffoldSourcePose* Pose = Out.Nodes.FindByPredicate(
			[&](const FRokScaffoldSourcePose& Node) { return Node.Path == SpritePath; });
		if (!Pose || Clip.Curves.Num() < 3 || Clip.LengthSeconds <= 0.0)
		{
			Error = TEXT("Scaffold source clip has no exact animated sprite1 target");
			return false;
		}
		const double Time = FMath::Min(ElapsedSeconds, Clip.LengthSeconds);
		for (const FRokScaffoldCurve& Curve : Clip.Curves)
		{
			const double Value = EvaluateCurve(Curve, Time);
			if (Curve.Property == TEXT("m_LocalPosition.x")) Pose->Position.X = Value;
			else if (Curve.Property == TEXT("m_LocalPosition.y")) Pose->Position.Y = Value;
			else if (Curve.Property == TEXT("m_LocalPosition.z")) Pose->Position.Z = Value;
			else if (Curve.Property == TEXT("m_LocalScale.x")) Pose->Scale.X = Value;
			else if (Curve.Property == TEXT("m_LocalScale.y")) Pose->Scale.Y = Value;
			else if (Curve.Property == TEXT("m_LocalScale.z")) Pose->Scale.Z = Value;
			else return false;
		}
	}
	Error.Reset();
	return true;
}
}

bool FRokScaffoldTicketLifecycle::Prepare(const UWorld* World, FString& Error)
{
	if (!IsValid(World) || Phase != EPhase::Empty)
	{
		Error = TEXT("Scaffold prepare requires a fresh world-owned component");
		return false;
	}
	OwnerWorld = World;
	Phase = EPhase::Prepared;
	Error.Reset();
	return true;
}

bool FRokScaffoldTicketLifecycle::Begin(const UWorld* World,
	const FRokLocalConstructionTicket& Ticket, FString& Error)
{
	if (Phase != EPhase::Prepared || !IsValid(World) || World != OwnerWorld
		|| Ticket.Id == 0 || (Ticket.TypeId != 2 && Ticket.TypeId != 3) || Ticket.bCompleted
		|| !FMath::IsFinite(Ticket.StartedAt) || !FMath::IsFinite(Ticket.FinishesAt)
		|| Ticket.StartedAt < 0.0 || Ticket.FinishesAt <= Ticket.StartedAt)
	{
		Error = TEXT("Scaffold ticket is duplicate, unsupported, completed or from another world");
		return false;
	}
	TicketId = Ticket.Id;
	StartedAt = Ticket.StartedAt;
	FinishesAt = Ticket.FinishesAt;
	LastSampledWorldSeconds = Ticket.StartedAt;
	Phase = EPhase::Playing;
	Error.Reset();
	return true;
}

bool FRokScaffoldTicketLifecycle::Sample(double WorldSeconds, double& OutElapsed,
	bool& bOutPlaying, FString& Error)
{
	OutElapsed = 0.0;
	bOutPlaying = false;
	if ((Phase != EPhase::Playing && Phase != EPhase::Stopped)
		|| !FMath::IsFinite(WorldSeconds) || WorldSeconds < StartedAt
		|| WorldSeconds < LastSampledWorldSeconds)
	{
		Error = TEXT("Scaffold sample precedes ticket start or ticket is not active");
		return false;
	}
	OutElapsed = WorldSeconds - StartedAt;
	LastSampledWorldSeconds = WorldSeconds;
	bOutPlaying = OutElapsed < StopSeconds;
	if (!bOutPlaying) Phase = EPhase::Stopped;
	Error.Reset();
	return true;
}

bool FRokScaffoldTicketLifecycle::Retire(const UWorld* World, uint64 Id,
	bool bExternallyRegistered, FString& Error)
{
	if ((Phase != EPhase::Playing && Phase != EPhase::Stopped) || !IsValid(World)
		|| World != OwnerWorld || Id == 0 || Id != TicketId || !bExternallyRegistered)
	{
		Error = TEXT("Scaffold retirement requires the matching, externally registered completed building");
		return false;
	}
	Phase = EPhase::Retired;
	Error.Reset();
	return true;
}

void FRokScaffoldTicketLifecycle::Reset()
{
	*this = FRokScaffoldTicketLifecycle();
}

URokLocalConstructionScaffoldComponent::URokLocalConstructionScaffoldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool URokLocalConstructionScaffoldComponent::PrepareHiddenSource(
	USceneComponent* ShelfParent, FString& Error)
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!IsValid(Owner) || !IsValid(World) || !World->IsGameWorld()
		|| !IsValid(ShelfParent) || ShelfParent->GetOwner() != Owner
		|| Lifecycle.Phase != FRokScaffoldTicketLifecycle::EPhase::Empty)
	{
		Error = TEXT("Scaffold hidden prepare requires an unbound same-actor game-world shelf parent");
		return false;
	}
	FRokScaffoldSourceBundle Candidate;
	if (!RokLocalConstructionScaffold::LoadValidatedBundle(Candidate, Error)) return false;
	TMap<FString, USceneComponent*> ByPath;
	ByPath.Add(RootNode, ShelfParent);
	for (const FRokScaffoldSourcePose& SourceNode : Candidate.Nodes)
	{
		if (SourceNode.Path == RootNode || SourceNode.Path.Contains(TEXT("/build_3003[4]"))) continue;
		USceneComponent* const* Parent = ByPath.Find(SourceNode.ParentPath);
		if (!Parent || !IsValid(*Parent))
		{
			Error = TEXT("Scaffold source hierarchy parent is missing");
			ReleaseOwnedNodes();
			return false;
		}
		USceneComponent* Node = NewObject<USceneComponent>(Owner, NAME_None, RF_Transient);
		if (!IsValid(Node))
		{
			Error = TEXT("Scaffold hidden source node allocation failed");
			ReleaseOwnedNodes();
			return false;
		}
		Node->SetupAttachment(*Parent);
		Node->SetVisibility(false, true);
		Node->RegisterComponent();
		OwnedNodes.Add(Node);
		ByPath.Add(SourceNode.Path, Node);
	}
	if (OwnedNodes.Num() != 12 || !Lifecycle.Prepare(World, Error))
	{
		if (Error.IsEmpty()) Error = TEXT("Scaffold did not create the four groups and eight hidden sprite nodes");
		ReleaseOwnedNodes();
		return false;
	}
	for (const FRokScaffoldLayerSource& Layer : Candidate.Layers)
	{
		SourceMeshes.Add(LoadObject<UStaticMesh>(nullptr, *ObjectPath(Layer.MeshPath)));
		SourceTextures.Add(LoadObject<UTexture2D>(nullptr, *ObjectPath(Layer.SpriteTexturePath)));
		SourceTextures.Add(LoadObject<UTexture2D>(nullptr, *ObjectPath(Layer.MaskTexturePath)));
	}
	if (SourceMeshes.ContainsByPredicate([](const TObjectPtr<UStaticMesh>& Asset) { return !IsValid(Asset.Get()); })
		|| SourceTextures.ContainsByPredicate([](const TObjectPtr<UTexture2D>& Asset) { return !IsValid(Asset.Get()); }))
	{
		Error = TEXT("Scaffold source assets became unavailable during hidden preparation");
		ReleaseOwnedNodes();
		SourceMeshes.Reset(); SourceTextures.Reset(); Lifecycle.Reset();
		return false;
	}
	Bundle = MoveTemp(Candidate);
	Error.Reset();
	return true;
}

bool URokLocalConstructionScaffoldComponent::BeginForCommittedTicket(
	const FRokLocalConstructionTicket& Ticket, FString& Error)
{
	FRokScaffoldSample Candidate;
	if (!RokLocalConstructionScaffold::SampleBundle(Bundle,
		FMath::Max(0.0, GetWorld() ? GetWorld()->GetTimeSeconds() - Ticket.StartedAt : 0.0),
		Candidate, Error)) return false;
	if (!Lifecycle.Begin(GetWorld(), Ticket, Error)) return false;
	LastSample = MoveTemp(Candidate);
	LastSample.TicketId = Ticket.Id;
	if (!LastSample.bFourAnimationsEnabled)
	{
		Lifecycle.Phase = FRokScaffoldTicketLifecycle::EPhase::Stopped;
	}
	SetComponentTickEnabled(Lifecycle.Phase == FRokScaffoldTicketLifecycle::EPhase::Playing);
	return true;
}

bool URokLocalConstructionScaffoldComponent::SampleAtWorldSeconds(
	double WorldSeconds, FRokScaffoldSample& Out, FString& Error)
{
	double Elapsed = 0.0;
	bool bPlaying = false;
	if (!Lifecycle.Sample(WorldSeconds, Elapsed, bPlaying, Error)
		|| !RokLocalConstructionScaffold::SampleBundle(Bundle, Elapsed, Out, Error)) return false;
	Out.TicketId = Lifecycle.TicketId;
	Out.bFourAnimationsEnabled = bPlaying;
	Out.bParticleNodeActiveInSource = bPlaying;
	return true;
}

void URokLocalConstructionScaffoldComponent::TickComponent(float DeltaTime,
	ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetWorld() && (Lifecycle.Phase == FRokScaffoldTicketLifecycle::EPhase::Playing
		|| Lifecycle.Phase == FRokScaffoldTicketLifecycle::EPhase::Stopped))
	{
		FString Error;
		if (!SampleAtWorldSeconds(GetWorld()->GetTimeSeconds(), LastSample, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Scaffold ticket source sampling stopped: %s"), *Error);
			SetComponentTickEnabled(false);
		}
		else if (Lifecycle.Phase == FRokScaffoldTicketLifecycle::EPhase::Stopped)
		{
			SetComponentTickEnabled(false);
		}
	}
}

bool URokLocalConstructionScaffoldComponent::RetireAfterRegisteredCompletion(
	URokBuildingUpgradeSubsystem* Buildings, uint64 TicketId, FString& Error)
{
	FRokLocalConstructionTicket Ticket;
	if (!IsValid(Buildings) || Buildings->GetWorld() != GetWorld()
		|| !Buildings->GetLocalConstructionTicket(TicketId, Ticket) || !Ticket.bCompleted
		|| Buildings->IsLocalConstructionPending(TicketId)
		|| Buildings->GetLocalConstructionActor(TicketId) != GetOwner()
		|| !Lifecycle.Retire(GetWorld(), TicketId, true, Error))
	{
		if (Error.IsEmpty()) Error = TEXT("Scaffold cannot retire before matching construction registration");
		return false;
	}
	SetComponentTickEnabled(false);
	ReleaseOwnedNodes();
	SourceMeshes.Reset(); SourceTextures.Reset(); Bundle = FRokScaffoldSourceBundle();
	LastSample = FRokScaffoldSample();
	return true;
}

void URokLocalConstructionScaffoldComponent::ReleaseOwnedNodes()
{
	for (int32 Index = OwnedNodes.Num() - 1; Index >= 0; --Index)
	{
		if (IsValid(OwnedNodes[Index])) OwnedNodes[Index]->DestroyComponent();
	}
	OwnedNodes.Reset();
}

bool URokLocalConstructionScaffoldComponent::AreOwnedSourceNodesHidden() const
{
	if (OwnedNodes.Num() != 12) return false;
	for (const TObjectPtr<USceneComponent>& Node : OwnedNodes)
	{
		if (!IsValid(Node.Get()) || Node->GetOwner() != GetOwner()
			|| !Node->IsRegistered() || Node->IsVisible()) return false;
	}
	return true;
}

void URokLocalConstructionScaffoldComponent::CancelPrepared()
{
	if (Lifecycle.Phase != FRokScaffoldTicketLifecycle::EPhase::Prepared) return;
	SetComponentTickEnabled(false);
	ReleaseOwnedNodes();
	SourceMeshes.Reset(); SourceTextures.Reset(); Bundle = FRokScaffoldSourceBundle();
	LastSample = FRokScaffoldSample(); Lifecycle.Reset();
}

void URokLocalConstructionScaffoldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetComponentTickEnabled(false);
	ReleaseOwnedNodes();
	SourceMeshes.Reset(); SourceTextures.Reset(); Bundle = FRokScaffoldSourceBundle();
	LastSample = FRokScaffoldSample(); Lifecycle.Reset();
	Super::EndPlay(EndPlayReason);
}
