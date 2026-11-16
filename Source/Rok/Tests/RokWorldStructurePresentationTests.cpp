#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "../RokDataPaths.h"
#include "../RokWorldStructurePresentationData.h"

namespace
{
FString Encode(const TSharedPtr<FJsonObject>& Root)
{
	FString Text;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	return Text;
}

TSharedPtr<FJsonObject> Decode(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
	return Root;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldStructurePresentationSourceTest,
	"Rok.WorldMap.StructurePresentation.SourceModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldStructurePresentationSourceTest::RunTest(const FString& Parameters)
{
	FString Text;
	if (!TestTrue(TEXT("Packaged manifest exists"),
		FFileHelper::LoadFileToString(Text, *RokDataPaths::Resolve(TEXT("world_structure_presentation.json"))))) return false;
	const auto Root = Decode(Text);
	if (!TestTrue(TEXT("Manifest parses"), Root.IsValid())) return false;
	const auto Models = Root->GetObjectField(TEXT("models"));
	TMap<FString, int32> Counts;
	Counts.Add(TEXT("build_BarbarianCitadel_Big"), 1);
	Counts.Add(TEXT("build_alliance_1003"), 1);
	Counts.Add(TEXT("build_alliance_1004"), 1);
	Counts.Add(TEXT("build_alliance_1005"), 1);
	Counts.Add(TEXT("build_alliance_1006"), 1);
	Counts.Add(TEXT("build_Checkpoint_Big"), 4);
	Counts.Add(TEXT("build_HolyLand1_Big"), 1);
	Counts.Add(TEXT("build_HolyLand2_Big"), 1);
	Counts.Add(TEXT("build_Rssgold_Big"), 13);
	TestEqual(TEXT("Nine source models"), Models->Values.Num(), Counts.Num());
	for (const auto& Pair : Counts)
	{
		const auto Model = Models->GetObjectField(Pair.Key);
		TestEqual(TEXT("Exact Unity prefab provenance"), Model->GetObjectField(TEXT("source"))->GetStringField(TEXT("path")),
			FString(TEXT("ref/client-unity/Assets/BundleAssets/Scene/Map_building/map_building/")) + Pair.Key + TEXT(".prefab"));
		const bool bHasOrder = Model->GetArrayField(TEXT("groups")).Num() <= 1
			|| Model->HasTypedField<EJson::Array>(TEXT("grid_order"));
		const bool bHasEvidence = Model->GetArrayField(TEXT("groups")).Num() <= 1
			|| Model->HasTypedField<EJson::Object>(TEXT("native_order_evidence"));
		FRokWorldStructurePresentationData Data;
		if (!bHasOrder || !bHasEvidence)
		{
			TestFalse(*FString::Printf(TEXT("%s remains gated without native order"), *Pair.Key), Data.LoadJson(Text, Pair.Key));
			continue;
		}
		if (!TestTrue(*FString::Printf(TEXT("%s source model loads"), *Pair.Key), Data.LoadJson(Text, Pair.Key))) continue;
		TestEqual(TEXT("Source sprite count"), Data.Items.Num(), Pair.Value);
		if (Pair.Key == TEXT("build_Checkpoint_Big"))
		{
			TestEqual(TEXT("Checkpoint source mesh count"), Data.Meshes.Num(), 4);
			int32 Vertices = 0, Triangles = 0;
			TSet<FString> RendererIds;
			for (const auto& Mesh : Data.Meshes)
			{
				Vertices += Mesh.Positions.Num();
				Triangles += Mesh.Indices.Num() / 3;
				RendererIds.Add(Mesh.SourceRendererId);
				TestEqual(TEXT("Mesh normals complete"), Mesh.Normals.Num(), Mesh.Positions.Num());
				TestEqual(TEXT("Mesh UV complete"), Mesh.UV.Num(), Mesh.Positions.Num());
				TestEqual(TEXT("Mesh colors complete"), Mesh.Colors.Num(), Mesh.Positions.Num());
				TestTrue(TEXT("Mesh source renderer active"), Mesh.bEnabled);
				TestTrue(TEXT("Mesh texture packaged"), IFileManager::Get().FileExists(*Mesh.TexturePath));
			}
			TestEqual(TEXT("Four distinct source mesh renderers"), RendererIds.Num(), 4);
			TestEqual(TEXT("Exact checkpoint source vertices"), Vertices, 8724);
			TestEqual(TEXT("Exact checkpoint source triangles"), Triangles, 5299);
		}
		else TestTrue(TEXT("No mesh attached to sprite-only model"), Data.Meshes.IsEmpty());
		TestEqual(TEXT("Collision mirrors source groups"), Data.CollisionData.Items.Num(), Data.Groups.Num());
		TestTrue(TEXT("Valid unit conversion"), Data.UnitsToCentimeters > 0);
		TestTrue(TEXT("Scale bounded"), Data.EvaluateScale(0) >= 0 && Data.EvaluateScale(0) <= Data.MaxScale);
		if (Pair.Key == TEXT("build_BarbarianCitadel_Big") || Pair.Key.StartsWith(TEXT("build_HolyLand")))
		{
			TestFalse(TEXT("Serialized GridCollide m_is_active starts false"), Data.Groups[0].bActive);
			TestTrue(TEXT("Source GameObject remains visible until GridCollide transition"), Data.Items[0].bEnabled);
		}
		for (const auto& Item : Data.Items)
		{
			TestTrue(TEXT("Sprite is a packaged path"), Item.TexturePath.Contains(TEXT("RokData")));
			TestTrue(TEXT("Packaged sprite exists"), IFileManager::Get().FileExists(*Item.TexturePath));
			TestTrue(TEXT("Mask either packaged or builtin shader"),
				!Item.MaskPath.IsEmpty() || Item.ShaderMode == TEXT("unity_builtin_10750"));
			if (!Item.MaskPath.IsEmpty()) TestTrue(TEXT("Packaged mask exists"), IFileManager::Get().FileExists(*Item.MaskPath));
			TestTrue(TEXT("Source item has stable identity"), !Item.SourceId.IsEmpty() && !Item.SourceRendererId.IsEmpty());
		}
		if (Data.Groups.Num() > 0)
		{
			FRokStoneDepositState State;
			State.Reset(Data.Groups.Num());
			State.Update(Data.CollisionData, Data.EvaluateScale(0));
			TestEqual(TEXT("Persistent collision state initialized"), State.Active.Num(), Data.Groups.Num());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldStructurePresentationGoldNativeTest,
	"Rok.WorldMap.StructurePresentation.GoldNativeZoomSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldStructurePresentationGoldNativeTest::RunTest(const FString& Parameters)
{
	FRokWorldStructurePresentationData Data;
	if (!TestTrue(TEXT("Gold requires captured native group order"), Data.Load(TEXT("build_Rssgold_Big")))) return false;
	const FRokWorldStructurePresentationItem* PinnedSprite = Data.Items.FindByPredicate([](const FRokWorldStructurePresentationItem& Item)
	{
		return Item.SourceId.EndsWith(TEXT("#113980432"));
	});
	if (!TestTrue(TEXT("Pinned Unity gold sprite present"), PinnedSprite != nullptr)) return false;
	TestTrue(TEXT("Gold group anchor is source fixed position"),
		PinnedSprite->Position.Equals(FVector(-0.1304610464396978, 0, -1.6408414519089598), 0.000001));
	TestTrue(TEXT("Gold sprite offset is before GridCollide scale"),
		PinnedSprite->SpritePosition.Equals(FVector(-0.34471457580662024, 0.096, 0.09652003140971632), 0.000001));
	FString Text;
	if (!TestTrue(TEXT("Native Unity gold probe exists"),
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_gold_deposit_native_probe.json"))))) return false;
	const auto Native = Decode(Text);
	if (!TestTrue(TEXT("Native probe parses"), Native.IsValid())) return false;
	int32 Samples = 0;
	for (const auto& RunValue : Native->GetArrayField(TEXT("runs")))
	{
		FRokStoneDepositState State;
		State.Reset(Data.Groups.Num());
		for (const auto& SampleValue : RunValue->AsObject()->GetArrayField(TEXT("samples")))
		{
			const auto Sample = SampleValue->AsObject();
			const float Dxf = Sample->GetNumberField(TEXT("dxf"));
			const float Scale = Data.EvaluateScale(Dxf);
			TestTrue(TEXT("Unity native curve scale"), FMath::IsNearlyEqual(Scale,
				static_cast<float>(Sample->GetNumberField(TEXT("scale"))), 0.00002f));
			State.Update(Data.CollisionData, Scale);
			const FVector PinnedPosition = PinnedSprite->Position + PinnedSprite->SpritePosition * Scale;
			TestTrue(TEXT("Scale changes gold sprite offset around fixed group anchor"),
				PinnedPosition.Equals(PinnedSprite->Position + FVector(-0.34471457580662024, 0.096, 0.09652003140971632) * Scale, 0.000001));
			TArray<FString> ActualOrder, ActualActive, ExpectedOrder, ExpectedActive;
			for (int32 I = 0; I < Data.Groups.Num(); ++I)
			{
				FString Left, Id;
				if (!Data.Groups[I].SourceId.Split(TEXT("#"), &Left, &Id, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) return false;
				ActualOrder.Add(Id);
				if (State.Active[I]) ActualActive.Add(Id);
				TestTrue(TEXT("Gold collision anchor remains fixed across zoom"),
					Data.CollisionData.Items[I].Position.Equals(Data.Groups[I].Position, 0.00001));
			}
			for (const auto& Id : Sample->GetArrayField(TEXT("order"))) ExpectedOrder.Add(Id->AsString());
			for (const auto& Id : Sample->GetArrayField(TEXT("active"))) ExpectedActive.Add(Id->AsString());
			TestEqual(TEXT("Native gold registration order"), FString::Join(ActualOrder, TEXT(",")), FString::Join(ExpectedOrder, TEXT(",")));
			TestEqual(TEXT("Native persistent zoom survivors"), FString::Join(ActualActive, TEXT(",")), FString::Join(ExpectedActive, TEXT(",")));
			++Samples;
		}
	}
	TestEqual(TEXT("Three native runs by eleven zoom samples"), Samples, 33);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldStructurePresentationMalformedTest,
	"Rok.WorldMap.StructurePresentation.RejectIncompleteModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldStructurePresentationMalformedTest::RunTest(const FString& Parameters)
{
	FString Original;
	if (!FFileHelper::LoadFileToString(Original, *RokDataPaths::Resolve(TEXT("world_structure_presentation.json")))) return false;
	const FString Id = TEXT("build_BarbarianCitadel_Big");
	FRokWorldStructurePresentationData Data;
	if (!TestTrue(TEXT("Baseline model loads"), Data.LoadJson(Original, Id))) return false;
	for (int32 Case = 0; Case < 11; ++Case)
	{
		const auto Root = Decode(Original);
		if (!Root.IsValid()) return false;
		const auto Model = Root->GetObjectField(TEXT("models"))->GetObjectField(Id);
		auto Items = Model->GetArrayField(TEXT("items"));
		const auto Item = Items[0]->AsObject();
		switch (Case)
		{
		case 0: Model->RemoveField(TEXT("scale_curve")); break;
		case 1: Model->SetArrayField(TEXT("unsupported"), {MakeShared<FJsonValueString>(TEXT("unresolved"))}); break;
		case 2: Item->SetArrayField(TEXT("right"), {MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)}); break;
		case 3: Item->SetStringField(TEXT("shader_guid"), TEXT("unknown")); break;
		case 4: Item->GetObjectField(TEXT("texture"))->SetStringField(TEXT("export"), TEXT("ref/client-unity/not-packaged.png")); break;
		case 5: Item->GetObjectField(TEXT("mask"))->SetStringField(TEXT("export"), TEXT("Content/RokData/missing-mask.png")); break;
		case 6: Item->RemoveField(TEXT("renderer_color")); break;
		case 7:
		{
			const auto Duplicate = Items[0];
			Items.Add(Duplicate);
			Model->SetArrayField(TEXT("items"), Items);
			break;
		}
		case 8: Item->SetField(TEXT("group_id"), MakeShared<FJsonValueString>(TEXT("nonexistent"))); break;
		case 9: Item->SetArrayField(TEXT("sprite_position"),
			{MakeShared<FJsonValueNumber>(1.0e100), MakeShared<FJsonValueNumber>(0), MakeShared<FJsonValueNumber>(0)}); break;
		case 10: Item->RemoveField(TEXT("active_in_hierarchy")); break;
		}
		TestFalse(*FString::Printf(TEXT("Malformed case %d rejected"), Case), Data.LoadJson(Encode(Root), Id));
		TestTrue(TEXT("Failed load leaves no partial model"), Data.Items.IsEmpty() && Data.Groups.IsEmpty()
			&& Data.CollisionData.ScaleCurve.GetNumKeys() == 0 && Data.ModelId.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldStructurePresentationCheckpointMeshGateTest,
	"Rok.WorldMap.StructurePresentation.CheckpointMeshGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldStructurePresentationCheckpointMeshGateTest::RunTest(const FString& Parameters)
{
	FString Original;
	if (!FFileHelper::LoadFileToString(Original, *RokDataPaths::Resolve(TEXT("world_structure_presentation.json")))) return false;
	const FString Id = TEXT("build_Checkpoint_Big");
	FRokWorldStructurePresentationData Data;
	if (!TestTrue(TEXT("Complete checkpoint sprites plus source FBX mesh load"), Data.LoadJson(Original, Id))) return false;
	for (int32 Case = 0; Case < 6; ++Case)
	{
		const auto Root = Decode(Original);
		if (!Root.IsValid()) return false;
		const auto Model = Root->GetObjectField(TEXT("models"))->GetObjectField(Id);
		switch (Case)
		{
		case 0: Model->RemoveField(TEXT("mesh_asset")); break;
		case 1: Model->GetObjectField(TEXT("mesh_asset"))->SetStringField(TEXT("sha256"), FString::ChrN(64, TEXT('0'))); break;
		case 2: Model->GetObjectField(TEXT("mesh_asset"))->SetStringField(TEXT("export"), TEXT("ref/client-unity/mesh.json")); break;
		case 3: Model->SetNumberField(TEXT("mesh_vertex_count"), 1); break;
		case 4: Model->GetObjectField(TEXT("mesh_source"))->GetObjectField(TEXT("fbx"))->SetStringField(TEXT("sha256"), FString::ChrN(64, TEXT('0'))); break;
		case 5: Model->SetArrayField(TEXT("unsupported"), {MakeShared<FJsonValueString>(TEXT("unresolved child"))}); break;
		}
		TestFalse(*FString::Printf(TEXT("Incomplete checkpoint case %d rejected"), Case), Data.LoadJson(Encode(Root), Id));
		TestTrue(TEXT("Failed mesh load leaves no partial model"), Data.Items.IsEmpty() && Data.Meshes.IsEmpty()
			&& Data.Groups.IsEmpty() && Data.ModelId.IsEmpty());
	}
	return true;
}

#endif
