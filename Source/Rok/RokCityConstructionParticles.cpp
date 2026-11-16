#include "RokCityConstructionParticles.h"

#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "ProceduralMeshComponent.h"
#include "RokDataPaths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 SampleRate = 60;
constexpr int32 FrameCount = SampleRate * 2;
constexpr int32 VertexStride = 9;
}

bool FRokCityConstructionParticles::Load()
{
	if (!Effects.IsEmpty()) return true;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Models = nullptr;
	const TSharedPtr<FJsonObject>* Offsets = nullptr;
	if (!FFileHelper::LoadFileToString(Text, *RokDataPaths::Resolve(TEXT("CityConstructionParticles/effects.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root
		|| Root->GetStringField(TEXT("schema")) != TEXT("rok.city_construction_particles.v1")
		|| Root->GetIntegerField(TEXT("rate")) != SampleRate || Root->GetNumberField(TEXT("duration")) != 2.0
		|| !Root->TryGetArrayField(TEXT("effects"), Models)
		|| !Root->TryGetObjectField(TEXT("completion_offsets"), Offsets)) return false;
	TMap<FName, FVector> ParsedOffsets;
	for (const auto& Pair : (*Offsets)->Values)
	{
		const TArray<TSharedPtr<FJsonValue>>* Coordinates = nullptr;
		if (!Pair.Value->TryGetArray(Coordinates) || Coordinates->Num() != 3) return false;
		const FVector Offset((*Coordinates)[0]->AsNumber(), (*Coordinates)[2]->AsNumber(),
			(*Coordinates)[1]->AsNumber());
		if (Offset.ContainsNaN()) return false;
		ParsedOffsets.Add(FName(*Pair.Key), Offset);
	}
	TMap<FName, FEffect> Parsed;
	for (const auto& Value : *Models)
	{
		const auto Source = Value->AsObject();
		if (!Source) return false;
		const FName Name(*Source->GetStringField(TEXT("name")));
		const auto& Frames = Source->GetArrayField(TEXT("frames"));
		if (Name.IsNone() || Parsed.Contains(Name) || Frames.Num() != FrameCount) return false;
		FEffect& Effect = Parsed.FindOrAdd(Name);
		for (const auto& Material : Source->GetArrayField(TEXT("materials")))
		{
			FSoftObjectPath Path(Material->AsString());
			if (!Path.IsValid()) return false;
			Effect.Materials.Add(MoveTemp(Path));
		}
		if (Effect.Materials.IsEmpty()) return false;
		for (const auto& Frame : Frames)
		{
			auto& Target = Effect.Frames.AddDefaulted_GetRef();
			const auto& Emitters = Frame->AsObject()->GetArrayField(TEXT("emitters"));
			if (Emitters.Num() != Effect.Materials.Num()) return false;
			for (const auto& Emitter : Emitters)
			{
				const auto Geometry = Emitter->AsObject();
				const auto& Packed = Geometry->GetArrayField(TEXT("vertices"));
				const auto& Indices = Geometry->GetArrayField(TEXT("indices"));
				if (Packed.Num() % VertexStride || Indices.Num() % 3) return false;
				FGeometry& Mesh = Target.AddDefaulted_GetRef();
				for (int32 Offset = 0; Offset < Packed.Num(); Offset += VertexStride)
				{
					const auto Number = [&](int32 Channel) { return Packed[Offset + Channel]->AsNumber(); };
					const FVector Point(Number(0), Number(1), Number(2));
					if (Point.ContainsNaN()) return false;
					Mesh.Points.Add(Point);
					Mesh.UVs.Emplace(Number(3), Number(4));
					Mesh.Colors.Emplace(Number(5) / 255.0, Number(6) / 255.0, Number(7) / 255.0, Number(8) / 255.0);
				}
				for (const auto& Index : Indices)
				{
					const double Number = Index->AsNumber();
					if (Number < 0 || Number >= Mesh.Points.Num() || FMath::TruncToDouble(Number) != Number) return false;
					Mesh.Indices.Add(static_cast<int32>(Number));
				}
			}
		}
	}
	Effects = MoveTemp(Parsed);
	CompletionOffsets = MoveTemp(ParsedOffsets);
	return !Effects.IsEmpty();
}

void FRokCityConstructionParticles::Play(UWorld& World, int32 BuildingIndex, FName Name, const FVector& Location, double Scale)
{
	const FEffect* Effect = Effects.Find(Name);
	if (!Effect) return;
	TArray<UMaterialInterface*> Materials;
	for (const FSoftObjectPath& Path : Effect->Materials)
	{
		auto* Material = Cast<UMaterialInterface>(Path.TryLoad());
		if (!Material) return;
		Materials.Add(Material);
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), Params);
	if (!Actor) return;
	auto* Root = NewObject<USceneComponent>(Actor);
	Actor->SetRootComponent(Root);
	Actor->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Actor->SetActorLocation(Location);
	Actor->SetActorScale3D(FVector(Scale));
	Actor->Tags.Add(Name);
	Actor->Tags.Add(TEXT("RokCityConstructionParticles"));
	FPlayback& Playback = Active.AddDefaulted_GetRef();
	Playback.BuildingIndex = BuildingIndex;
	Playback.Name = Name;
	Playback.Actor = Actor;
	Playback.StartedAt = World.GetTimeSeconds();
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		auto* Mesh = NewObject<UProceduralMeshComponent>(Actor);
		Actor->AddInstanceComponent(Mesh);
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->bReceivesDecals = false;
		Mesh->SetTranslucentSortPriority(3000 + Index);
		Mesh->SetMaterial(0, Materials[Index]);
		Mesh->RegisterComponent();
		Playback.Emitters.Add(Mesh);
	}
	UE_LOG(LogTemp, Display, TEXT("Rok construction particles: %s for building %d."), *Name.ToString(), BuildingIndex);
}

void FRokCityConstructionParticles::Stop(int32 BuildingIndex)
{
	Active.RemoveAll([BuildingIndex](const FPlayback& Playback)
	{
		if (Playback.BuildingIndex != BuildingIndex) return false;
		if (AActor* Actor = Playback.Actor.Get()) Actor->Destroy();
		return true;
	});
}

void FRokCityConstructionParticles::Update(double Now)
{
	Active.RemoveAll([this, Now](FPlayback& Playback)
	{
		AActor* Actor = Playback.Actor.Get();
		if (!Actor) return true;
		const int32 Frame = FMath::FloorToInt(FMath::Max(Now - Playback.StartedAt, 0.0) * SampleRate);
		if (Frame >= FrameCount)
		{
			Actor->Destroy();
			return true;
		}
		if (Frame == Playback.LastFrame) return false;
		Playback.LastFrame = Frame;
		const auto& Geometry = Effects.FindChecked(Playback.Name).Frames[Frame];
		for (int32 Index = 0; Index < Geometry.Num(); ++Index)
		{
			auto* Mesh = Playback.Emitters[Index].Get();
			if (!Mesh) continue;
			const auto& Data = Geometry[Index];
			if (Data.Indices.IsEmpty()) Mesh->ClearMeshSection(0);
			else Mesh->CreateMeshSection_LinearColor(0, Data.Points, Data.Indices, {}, Data.UVs,
				Data.Colors, {}, false);
		}
		return false;
	});
}

AActor* FRokCityConstructionParticles::Find(int32 BuildingIndex, FName Name) const
{
	const FPlayback* Found = Active.FindByPredicate([BuildingIndex, Name](const FPlayback& Playback)
	{
		return Playback.BuildingIndex == BuildingIndex && Playback.Name == Name;
	});
	return Found ? Found->Actor.Get() : nullptr;
}

void FRokCityConstructionParticles::Reset()
{
	for (const auto& Playback : Active) if (AActor* Actor = Playback.Actor.Get()) Actor->Destroy();
	Active.Reset();
	Effects.Reset();
	CompletionOffsets.Reset();
}
