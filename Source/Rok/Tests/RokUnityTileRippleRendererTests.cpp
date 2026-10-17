#include "Misc/AutomationTest.h"


#if WITH_DEV_AUTOMATION_TESTS
#include "../RokDataPaths.h"
#include "../RokUnityTileRippleRenderer.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTileRippleBakedGeometryTest,
	"Rok.WorldTile.TileRipple.V5SourceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
FVector3d V3(const TSharedPtr<FJsonObject>& Object)
{
	return FVector3d(Object->GetNumberField(TEXT("x")), Object->GetNumberField(TEXT("y")),
		Object->GetNumberField(TEXT("z")));
}
FVector3d ToUe(const FVector3d& Source) { return FVector3d(-Source.X, Source.Z, Source.Y); }
}

bool FRokTileRippleBakedGeometryTest::RunTest(const FString&)
{
	FRokTileRippleGeometryContract Contract;
	FString Error;
	if (!TestTrue(TEXT("Pinned two-system V5 geometry fixture"),
		FRokTileRippleGeometryContract::LoadPinned(RokDataPaths::Resolve(TEXT("TileRippleRendererV8/geometry_contract.json")), Contract, Error))) return false;
	FString Source;
	if (!TestTrue(TEXT("Sealed V5 Native exists"), FFileHelper::LoadFileToString(Source,
		*(FPaths::ProjectDir() /
		TEXT("Saved/UnityWorldTileParticleBakeMeshProbeV5/OutputRuns/20261009_081124_054_20920/bakemesh_native.json"))))) return false;
	TSharedPtr<FJsonObject> V5;
	if (!TestTrue(TEXT("Sealed V5 Native parses"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), V5) && V5.IsValid())) return false;
	int32 Groups = 0, Vertices = 0;
	for (const auto& SampleValue : V5->GetArrayField(TEXT("samples")))
	for (const auto& ResetValue : SampleValue->AsObject()->GetArrayField(TEXT("independent_resets")))
	{
		const auto& Systems = ResetValue->AsObject()->GetArrayField(TEXT("systems"));
		if (!TestEqual(TEXT("Exactly two source systems"), Systems.Num(), 2)) return false;
		for (int32 SystemIndex = 0; SystemIndex < 2; ++SystemIndex)
		{
			const auto Row = Systems[SystemIndex]->AsObject();
			const FRokTileRippleEmitterGeometry& Emitter = Contract.Emitters[SystemIndex];
			if (!TestEqual(TEXT("Source emitter order"), Row->GetStringField(TEXT("name")), Emitter.SourceName)) return false;
			const auto Bake = Row->GetObjectField(TEXT("baked_with_transform"));
			const auto& NativeVertices = Bake->GetArrayField(TEXT("vertices"));
			const auto& NativeColors = Bake->GetArrayField(TEXT("colors32"));
			const auto& NativeUv0 = Bake->GetArrayField(TEXT("uv0"));
			const auto& NativeNormals = Bake->GetArrayField(TEXT("normals"));
			const auto& NativeIndices = Bake->GetArrayField(TEXT("triangle_indices"));
			const auto& Particles = Row->GetArrayField(TEXT("particles"));
			TArray<FRokTileRippleParticleSnapshot> Snapshot;
			TArray<FColor> SourceColors;
			for (const auto& ParticleValue : Particles)
			{
				const auto Particle = ParticleValue->AsObject();
				FRokTileRippleParticleSnapshot View;
				View.LocalPosition = V3(Particle->GetObjectField(TEXT("position")));
				View.CurrentSize = Particle->GetNumberField(TEXT("current_size"));
				const auto C = Particle->GetObjectField(TEXT("current_color"));
				View.CurrentColor = FLinearColor(C->GetNumberField(TEXT("r"))/255.0,
					C->GetNumberField(TEXT("g"))/255.0, C->GetNumberField(TEXT("b"))/255.0,
					C->GetNumberField(TEXT("a"))/255.0);
				Snapshot.Add(View);
				SourceColors.Add(FColor(C->GetIntegerField(TEXT("r")), C->GetIntegerField(TEXT("g")),
					C->GetIntegerField(TEXT("b")), C->GetIntegerField(TEXT("a"))));
			}
			FRokTileRippleMeshData UeMesh;
			if (!TestTrue(TEXT("Build live-formula quads"), FRokTileRippleMeshData::Build(
				Emitter, Contract.SourceAnchor, Contract.SourceUnitsToUe, Snapshot, UeMesh, Error))) return false;
			if (!TestEqual(TEXT("Four vertices per source particle"), UeMesh.Vertices.Num(), NativeVertices.Num())) return false;
			if (!TestEqual(TEXT("V5 source UV count"), NativeUv0.Num(), NativeVertices.Num())
				|| !TestEqual(TEXT("V5 source normal count"), NativeNormals.Num(), NativeVertices.Num())
				|| !TestEqual(TEXT("V5 source triangle count"), NativeIndices.Num(), Particles.Num() * 6)
				|| !TestEqual(TEXT("UE triangle count"), UeMesh.Triangles.Num(), Particles.Num() * 6)) return false;
			TSet<int32> Matched;
			for (int32 Offset = 0; Offset < NativeVertices.Num(); Offset += 4)
			{
				FVector3d NativeCenter = FVector3d::ZeroVector;
				for (int32 Corner = 0; Corner < 4; ++Corner)
					NativeCenter += V3(NativeVertices[Offset + Corner]->AsObject()) * .25;
				int32 Nearest = INDEX_NONE;
				double Distance = TNumericLimits<double>::Max();
				for (int32 ParticleIndex = 0; ParticleIndex < Snapshot.Num(); ++ParticleIndex)
				{
					const double Candidate = (FRokTileRippleMeshData::SourceCenter(
						Emitter, Snapshot[ParticleIndex].LocalPosition) - NativeCenter).GetAbsMax();
					if (Candidate < Distance) { Distance = Candidate; Nearest = ParticleIndex; }
				}
				if (!TestTrue(TEXT("Unique native particle centroid association"),
					Nearest != INDEX_NONE && Distance < 1e-4 && !Matched.Contains(Nearest))) return false;
				Matched.Add(Nearest);
				for (int32 Corner = 0; Corner < 4; ++Corner)
				{
					const FVector3d NativeWorld = V3(NativeVertices[Offset + Corner]->AsObject()) + Emitter.SourceOrigin;
					const FVector Expected = FVector(ToUe(NativeWorld - Contract.SourceAnchor) * Contract.SourceUnitsToUe);
					if (!TestTrue(TEXT("V5 exact quad corner formula"),
						UeMesh.Vertices[Nearest * 4 + Corner].Equals(Expected, .001))) return false;
					const auto C = NativeColors[Offset + Corner]->AsObject();
					const FColor CurrentColor = SourceColors[Nearest];
					const FColor UeColor = UeMesh.Colors[Nearest * 4 + Corner];
					if (!TestEqual(TEXT("UE Color32 equals independent Unity GetCurrentColor RGBA"),
						UeColor, CurrentColor)) return false;
					const FColor BakedColor(C->GetIntegerField(TEXT("r")), C->GetIntegerField(TEXT("g")),
						C->GetIntegerField(TEXT("b")), C->GetIntegerField(TEXT("a")));
					if (!TestTrue(TEXT("BakeMesh RGB equals associated GetCurrentColor exactly"),
						BakedColor.R == CurrentColor.R && BakedColor.G == CurrentColor.G
						&& BakedColor.B == CurrentColor.B)) return false;
					if (!TestTrue(TEXT("BakeMesh Alpha differs from GetCurrentColor by at most one LSB"),
						FMath::Abs(static_cast<int32>(BakedColor.A) - static_cast<int32>(CurrentColor.A)) <= 1)) return false;
					const auto NativeUv = NativeUv0[Offset + Corner]->AsObject();
					const FVector2D ExpectedUv(NativeUv->GetNumberField(TEXT("x")),
						1.0 - NativeUv->GetNumberField(TEXT("y")));
					if (!TestTrue(TEXT("V5 source UV0 with UE top-left PNG V conversion"),
						UeMesh.UV0[Nearest * 4 + Corner].Equals(ExpectedUv, .000001))) return false;
					const FVector3d NativeNormal = V3(NativeNormals[Offset + Corner]->AsObject());
					if (!TestTrue(TEXT("V5 source normal axis conversion"),
						UeMesh.Normals[Nearest * 4 + Corner].Equals(FVector(ToUe(NativeNormal)), .0001))) return false;
					++Vertices;
				}
				const int32 NativeTriangleBase = Offset / 4 * 6;
				const int32 UeTriangleBase = Nearest * 6;
				for (int32 TriangleIndex = 0; TriangleIndex < 6; ++TriangleIndex)
				{
				const int32 SourceLocalIndex = static_cast<int32>(NativeIndices[NativeTriangleBase + TriangleIndex]->AsNumber()) - Offset;
				const int32 UeLocalIndex = UeMesh.Triangles[UeTriangleBase + TriangleIndex] - Nearest * 4;
				if (!TestEqual(TEXT("V5 source triangle order and winding"), UeLocalIndex, SourceLocalIndex)) return false;
				}
			}
			if (!TestEqual(TEXT("All native particles associated"), Matched.Num(), Snapshot.Num())) return false;
			++Groups;
		}
	}
	TestEqual(TEXT("V5 16 independent groups"), Groups, 16);
	TestEqual(TEXT("V5 160 source vertices"), Vertices, 160);
	return !HasAnyErrors();
}
#endif
