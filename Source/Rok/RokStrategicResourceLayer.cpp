#include "RokStrategicResourceLayer.h"
#include "RokDataPaths.h"
#include "RokWorldExplorationState.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

ARokStrategicResourceLayer::ARokStrategicResourceLayer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ResourceLayerRoot")));
	SetActorEnableCollision(false);
}

void ARokStrategicResourceLayer::ConfigureExploration()
{
	checkf(!bAttemptedLoad, TEXT("Configure exploration before loading the layer"));
	bExploration = true;
}

FIntPoint ARokStrategicResourceLayer::SourceCell(FVector2D Center)
{
	return FIntPoint(FMath::Clamp(FMath::FloorToInt(Center.X / 300), 0, 23),
		FMath::Clamp(FMath::FloorToInt(Center.Y / 300), 0, 23));
}

bool ARokStrategicResourceLayer::LoadSource()
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TCHAR* Manifest = bExploration ? TEXT("unity_strategic_exploration.json") : TEXT("unity_strategic_resources.json");
	const TCHAR* Schema = bExploration ? TEXT("rok.unity.strategic_exploration.v1") : TEXT("rok.unity.strategic_resources.v1");
	if (!FFileHelper::LoadFileToString(Json, *(RokDataPaths::Resolve(Manifest)))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| Root->GetStringField(TEXT("schema")) != Schema) return false;
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokWorldEntitySprite.M_RokWorldEntitySprite"));
	if (!PlaneMesh || !BaseMaterial) return false;
	const auto LoadMaterial = [&](const FString& Path, bool bPoint) -> UMaterialInstanceDynamic*
	{
		auto* Texture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::ResolveProjectPath(Path));
		if (!Texture) return nullptr;
		Texture->NeverStream = true;
		Texture->Filter = bPoint ? TF_Nearest : TF_Bilinear;
		Texture->AddressX = TA_Clamp; Texture->AddressY = TA_Clamp;
		Texture->UpdateResource();
		auto* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), Texture);
		return Material;
	};
	const auto Visuals = Root->GetObjectField(bExploration ? TEXT("runtime_visuals") : TEXT("visuals"));
	for (const auto& Pair : Visuals->Values)
	{
		const int32 Type = FCString::Atoi(*Pair.Key);
		const auto Visual = Pair.Value->AsObject();
		auto* Material = LoadMaterial(Visual->GetObjectField(TEXT("texture"))->GetStringField(TEXT("path")), false);
		if (!Material) return false;
		Materials.Add(Type, Material);
		const auto Rect = Visual->GetObjectField(TEXT("sprite"))->GetArrayField(TEXT("rect_bottom_left"));
		const float Ppu = Visual->GetNumberField(TEXT("pixels_per_unit"));
		if (Rect.Num() != 4 || Ppu <= 0) return false;
		UnitSizes.Add(Type, FVector2D(Rect[2]->AsNumber(), Rect[3]->AsNumber()) / Ppu);
		FRichCurve& Curve = Curves.Add(Type);
		for (const auto& Value : Visual->GetArrayField(TEXT("explore_curve")))
		{
			const auto Key = Value->AsArray();
			if (Key.Num() != 4) return false;
			auto Handle = Curve.AddKey(Key[0]->AsNumber(), Key[1]->AsNumber());
			auto& RichKey = Curve.GetKey(Handle);
			RichKey.InterpMode = RCIM_Cubic;
			RichKey.TangentMode = RCTM_User;
			RichKey.ArriveTangent = Key[2]->AsNumber();
			RichKey.LeaveTangent = Key[3]->AsNumber();
		}
	}
	for (const auto& Value : Root->GetArrayField(TEXT("points")))
	{
		const auto Point = Value->AsObject();
		const auto Pos = Point->GetArrayField(TEXT("unity_position"));
		if (Pos.Num() != 2) return false;
		FPoint P{static_cast<int32>(Point->GetNumberField(TEXT("id"))), static_cast<int32>(Point->GetNumberField(TEXT("type"))),
			FVector2D(Pos[0]->AsNumber(), Pos[1]->AsNumber()), FIntPoint()};
		if (bExploration) P.Type = P.Type < 70000 ? 1 : 3;
		if (!Materials.Contains(P.Type) || P.Position.ContainsNaN()) return false;
		P.Cell = SourceCell(P.Position);
		Points.Add(P);
	}
	if (!bExploration)
	{
		OverlayMaterial = LoadMaterial(Root->GetObjectField(TEXT("overlay"))->GetObjectField(TEXT("texture"))->GetStringField(TEXT("path")), true);
		if (!OverlayMaterial) return false;
		Overlay = MakePlane(OverlayMaterial, true);
	}
	UE_LOG(LogTemp, Display, TEXT("Rok strategic %s loaded: %d source fixed points, %d visual types."), bExploration ? TEXT("exploration") : TEXT("resources"), Points.Num(), Materials.Num());
	return true;
}

UStaticMeshComponent* ARokStrategicResourceLayer::MakePlane(UMaterialInstanceDynamic* Material, bool bHorizontal)
{
	auto* Mesh = NewObject<UStaticMeshComponent>(this);
	AddInstanceComponent(Mesh);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetStaticMesh(PlaneMesh);
	Mesh->SetMaterial(0, Material);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bReceivesDecals = false;
	Mesh->TranslucencySortPriority = bHorizontal ? 45 : 47;
	// Same source SpriteRenderer -> engine-plane UV/basis conversion as city_lod3.
	const FVector Up = bHorizontal ? FVector(0, 1, 0) : FVector(0, UE_INV_SQRT_2, UE_INV_SQRT_2);
	const FVector Normal = bHorizontal ? FVector(0, 0, -1) : FVector(0, UE_INV_SQRT_2, -UE_INV_SQRT_2);
	Mesh->SetRelativeRotation(FMatrix(FVector::CrossProduct(Up, Normal), Up, Normal, FVector::ZeroVector).ToQuat());
	Mesh->RegisterComponent();
	return Mesh;
}

void ARokStrategicResourceLayer::UpdateView(bool bEnabled, const FRokWorldMapProjection& Projection, FVector2D Center, float Dxf)
{
	if (!bEnabled)
	{
		if (Overlay) Overlay->SetVisibility(false);
		if (!PointMeshes.IsEmpty()) UE_LOG(LogTemp, Display, TEXT("Rok strategic resources cleared: %d points."), PointMeshes.Num());
		for (auto& Pair : PointMeshes) Pair.Value->DestroyComponent();
		PointMeshes.Reset(); CurrentCell = FIntPoint(-1, -1);
		return;
	}
	if (!bAttemptedLoad)
	{
		bAttemptedLoad = true; bLoaded = LoadSource();
		if (!bLoaded) UE_LOG(LogTemp, Error, TEXT("Cannot load strategic layer. Regenerate BuildRokUnityStrategic%s.py."), bExploration ? TEXT("Exploration") : TEXT("Resources"));
	}
	if (!bLoaded) return;
	const float WorldScale = Projection.GetSafeUeScale();
	if (Overlay)
	{
		Overlay->SetVisibility(true);
		Overlay->SetWorldLocation(Projection.UnityToUe(FVector2D(3600, 3600)));
		Overlay->SetWorldScale3D(FVector(-7200 * WorldScale / 100, -7200 * WorldScale / 100, 1));
	}
	const FIntPoint Cell = SourceCell(Center);
	if (Cell != CurrentCell)
	{
		TSet<int32> Needed;
		for (const FPoint& P : Points)
		{
			if (FMath::Abs(P.Cell.X - Cell.X) > 1 || FMath::Abs(P.Cell.Y - Cell.Y) > 1) continue;
			Needed.Add(P.Id);
			if (!PointMeshes.Contains(P.Id)) PointMeshes.Add(P.Id, MakePlane(Materials[P.Type], false));
		}
		for (auto It = PointMeshes.CreateIterator(); It; ++It)
			if (!Needed.Contains(It.Key())) { It.Value()->DestroyComponent(); It.RemoveCurrent(); }
		CurrentCell = Cell;
		UE_LOG(LogTemp, Display, TEXT("Rok strategic resources cell=(%d,%d) visible=%d"), Cell.X, Cell.Y, PointMeshes.Num());
	}
	for (const FPoint& P : Points)
	{
		auto* Entry = PointMeshes.Find(P.Id);
		if (!Entry) continue;
		const int32 Visual = P.Type + (bExploration && RokWorldExplorationState::IsOpened(P.Id, ExplorationWords) ? 1 : 0);
		if ((*Entry)->GetMaterial(0) != Materials[Visual]) (*Entry)->SetMaterial(0, Materials[Visual]);
		const FVector2D Size = UnitSizes[Visual] * Curves[Visual].Eval(Dxf) * WorldScale;
		(*Entry)->SetWorldScale3D(FVector(-Size.X / 100, -Size.Y / 100, 1));
		(*Entry)->SetWorldLocation(Projection.UnityToUe(P.Position));
	}
}
