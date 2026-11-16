#include "RokCityStructurePresentation.h"

#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "RokCitySpriteActor.h"
#include "RokDataPaths.h"
#include "RokStrategyCameraPawn.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 SamplesPerSecond = 60;
constexpr int32 LastFrame = 2 * SamplesPerSecond;

FVector ReadVector(const TSharedPtr<FJsonObject>& Value)
{
	return FVector(Value->GetNumberField(TEXT("x")), Value->GetNumberField(TEXT("y")),
		Value->GetNumberField(TEXT("z")));
}

float GetBoundaryScale(const UWorld& World)
{
	const APlayerController* Controller = World.GetFirstPlayerController();
	const ARokStrategyCameraPawn* Camera = Controller ? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr;
	return Camera ? ARokCitySpriteActor::EvaluateUnityCityLodScale(Camera->GetCurrentCityDxf()) : 1.0f;
}
}

bool FRokCityStructurePresentation::Load()
{
	if (!Models.IsEmpty()) return true;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TSharedPtr<FJsonObject>* SourceModels = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::ResolveProjectPath(
		TEXT("Saved/RokUnityMigration/CityStructures/manifest.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)
		|| !Root || Root->GetStringField(TEXT("schema")) != TEXT("rok.city_structures.v1")
		|| !Root->TryGetObjectField(TEXT("models"), SourceModels)) return false;
	TMap<FString, TArray<FPart>> Parsed;
	for (const auto& Model : (*SourceModels)->Values)
	{
		const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
		if (!Model.Value->TryGetArray(Parts) || Parts->IsEmpty()) return false;
		auto& Target = Parsed.FindOrAdd(FString(*Model.Key));
		for (const auto& Value : *Parts)
		{
			const auto Part = Value->AsObject();
			if (!Part) return false;
			auto& Entry = Target.AddDefaulted_GetRef();
			Entry.Mesh = FSoftObjectPath(Part->GetStringField(TEXT("mesh")));
			Entry.Material = FSoftObjectPath(Part->GetStringField(TEXT("material")));
			Entry.Order = Part->GetIntegerField(TEXT("order"));
			Entry.bSprite = Part->GetBoolField(TEXT("sprite"));
			if (!Entry.Mesh.IsValid() || !Entry.Material.IsValid()) return false;
			for (const auto& Pose : Part->GetArrayField(TEXT("poses")))
			{
				const auto Data = Pose->AsObject();
				if (!Data) return false;
				const FVector Translation = ReadVector(Data->GetObjectField(TEXT("translation")));
				const FVector Scale = ReadVector(Data->GetObjectField(TEXT("scale")));
				if (Translation.ContainsNaN() || Scale.ContainsNaN()) return false;
				Entry.Frames.Emplace(FQuat::Identity, Translation, Scale);
			}
			if (!Entry.Frames.IsEmpty() && Entry.Frames.Num() != LastFrame + 1) return false;
		}
	}
	Models = MoveTemp(Parsed);
	return !Models.IsEmpty();
}

AActor* FRokCityStructurePresentation::Spawn(UWorld& World, const FString& Model, bool bBoundary, const FVector& Location)
{
	const auto* Parts = Models.Find(Model);
	if (!Parts) return nullptr;
	TArray<UStaticMesh*> Meshes;
	TArray<UMaterialInterface*> Materials;
	for (const FPart& Part : *Parts)
	{
		auto* Mesh = Cast<UStaticMesh>(Part.Mesh.TryLoad());
		auto* Material = Cast<UMaterialInterface>(Part.Material.TryLoad());
		if (!Mesh || !Material) return nullptr;
		Meshes.Add(Mesh);
		Materials.Add(Material);
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), Params);
	if (!Actor) return nullptr;
	auto* Root = NewObject<USceneComponent>(Actor, TEXT("CityStructureRoot"));
	Actor->SetRootComponent(Root);
	Actor->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Actor->SetActorLocation(Location);
	Actor->Tags.Add(FName(*Model));
	Actor->Tags.Add(bBoundary ? TEXT("RokBuilding") : TEXT("RokServerConstructionScaffold"));
	for (int32 Index = 0; Index < Parts->Num(); ++Index)
	{
		const auto& Part = (*Parts)[Index];
		auto* Mesh = NewObject<UStaticMeshComponent>(Actor);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(Root);
		Mesh->SetStaticMesh(Meshes[Index]);
		Mesh->SetMaterial(0, Materials[Index]);
		Mesh->SetCastShadow(false);
		Mesh->SetCollisionEnabled(bBoundary && !Part.bSprite ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		if (bBoundary) Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Mesh->SetTranslucentSortPriority(2996 + Part.Order);
		Mesh->RegisterComponent();
		if (!Part.Frames.IsEmpty())
		{
			Mesh->SetRelativeTransform(Part.Frames[0]);
			Animations.Add({Mesh, &Part.Frames, World.GetTimeSeconds()});
		}
	}
	Actor->SetActorScale3D(FVector(GetBoundaryScale(World)));
	ScaledActors.Add(Actor);
	return Actor;
}

void FRokCityStructurePresentation::Update(UWorld& World)
{
	const float Scale = GetBoundaryScale(World);
	ScaledActors.RemoveAll([Scale](const TWeakObjectPtr<AActor>& Weak)
	{
		AActor* Actor = Weak.Get();
		if (!Actor) return true;
		if (!FMath::IsNearlyEqual(Actor->GetActorScale3D().X, Scale)) Actor->SetActorScale3D(FVector(Scale));
		return false;
	});
	const double Now = World.GetTimeSeconds();
	Animations.RemoveAll([Now](const FAnimation& Animation)
	{
		UStaticMeshComponent* Mesh = Animation.Mesh.Get();
		if (!Mesh) return true;
		const double Frame = FMath::Clamp((Now - Animation.StartedAt) * SamplesPerSecond, 0.0, double(LastFrame));
		const int32 Start = FMath::FloorToInt(Frame);
		FTransform Pose;
		Pose.Blend((*Animation.Frames)[Start], (*Animation.Frames)[FMath::Min(Start + 1, LastFrame)], Frame - Start);
		Mesh->SetRelativeTransform(Pose);
		return Start == LastFrame;
	});
}

void FRokCityStructurePresentation::Reset()
{
	ScaledActors.Reset();
	Animations.Reset();
	Models.Reset();
}
