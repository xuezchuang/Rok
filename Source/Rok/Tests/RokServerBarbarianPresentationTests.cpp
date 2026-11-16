#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokDataPaths.h"
#include "../RokTrainArmyMeshData.h"
#include "../RokWorldMapEntitySubsystem.h"

namespace
{
TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& Root, const FString& Key)
{
	const TSharedPtr<FJsonValue>* Value = Root.IsValid() ? Root->Values.Find(*Key) : nullptr;
	return Value && Value->IsValid() ? (*Value)->AsObject() : nullptr;
}

bool VerifySource(const TSharedPtr<FJsonObject>& Source, FAutomationTestBase& Test)
{
	if (!Source.IsValid()) return false;
	FString Relative, Hash;
	if (!Source->TryGetStringField(TEXT("path"), Relative)
		|| !Source->TryGetStringField(TEXT("sha256"), Hash)
		|| Relative.IsEmpty() || Hash.Len() != 64) return false;
	const FString Path = RokDataPaths::ResolveProjectPath(Relative);
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return false;
	return Test.TestEqual(*FString::Printf(TEXT("Source SHA256 %s"), *Relative),
		RokTrainArmyMesh::Sha256(Bytes), Hash);
}

TSharedPtr<FJsonObject> MonsterRow(int32 Id)
{
	TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("objectId"), 900000 + Id);
	Row->SetNumberField(TEXT("objectType"), 2);
	Row->SetNumberField(TEXT("monsterId"), Id);
	TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), 10000);
	Position->SetNumberField(TEXT("y"), 20000);
	Row->SetObjectField(TEXT("objectPos"), Position);
	return Row;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokServerBarbarianPresentationTest,
	"Rok.WorldMap.ServerEntity.BarbarianFormations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokServerBarbarianPresentationTest::RunTest(const FString&)
{
	FString Json;
	if (!TestTrue(TEXT("Supplement exists"), FFileHelper::LoadFileToString(
		Json, *RokDataPaths::Resolve(TEXT("server_barbarian_presentation.json"))))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!TestTrue(TEXT("Supplement JSON parses"), FJsonSerializer::Deserialize(
		TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid())) return false;
	TestEqual(TEXT("Supplement schema"), Root->GetStringField(TEXT("schema")),
		FString(TEXT("rok.server_barbarian_presentation.v1")));
	TestTrue(TEXT("Monster source hash"), VerifySource(Object(Root, TEXT("monster_table")), *this));
	TestTrue(TEXT("Sample presentation source hash"), VerifySource(Object(Root, TEXT("source_presentation")), *this));
	const TSharedPtr<FJsonObject> Formations = Object(Root, TEXT("formations"));
	const TSharedPtr<FJsonObject> Units = Object(Root, TEXT("unit_library"));
	if (!TestTrue(TEXT("Formation and unit libraries"), Formations.IsValid() && Units.IsValid())) return false;
	TestEqual(TEXT("Actual ordinary formation model count"), Formations->Values.Num(), 25);
	TestEqual(TEXT("Actual unit prefab count"), Units->Values.Num(), 14);
	for (const auto& Pair : Formations->Values)
	{
		const TSharedPtr<FJsonObject> Formation = Pair.Value->AsObject();
		TestEqual(TEXT("Model matches source prefab"), Formation->GetStringField(TEXT("prefab_id")), Pair.Key);
		TestTrue(TEXT("Formation source hash"), VerifySource(Object(Formation, TEXT("source_prefab")), *this));
		TestTrue(TEXT("Source formation has units"), Formation->GetIntegerField(TEXT("unit_count")) > 0);
	}
	for (const auto& Pair : Units->Values)
	{
		const TSharedPtr<FJsonObject> Unit = Pair.Value->AsObject();
		TestEqual(TEXT("Unit prefab identity"), Unit->GetStringField(TEXT("prefab_name")), Pair.Key);
		TestTrue(TEXT("Unit prefab source hash"), VerifySource(Object(Unit, TEXT("source_prefab")), *this));
		TestTrue(TEXT("Unit sheet source hash"), VerifySource(Object(Unit, TEXT("sprite_sheet")), *this));
		TestTrue(TEXT("Unit polygon source hash"), VerifySource(Object(Unit, TEXT("sprite_geometry_source")), *this));
		const TArray<TSharedPtr<FJsonValue>>* Crops = nullptr;
		if (!TestTrue(TEXT("Eight idle crops"), Unit->TryGetArrayField(TEXT("frame_crops"), Crops) && Crops && Crops->Num() == 8)) continue;
		for (const auto& CropValue : *Crops)
		{
			const TSharedPtr<FJsonObject> Crop = CropValue->AsObject();
			for (const TCHAR* Field : {TEXT("path"), TEXT("fade_path")})
			{
				const FString Path = RokDataPaths::ResolveProjectPath(Crop->GetStringField(Field));
				TestTrue(TEXT("Generated frame exists"), FPaths::FileExists(Path));
				TArray<uint8> Bytes;
				if (FFileHelper::LoadFileToArray(Bytes, *Path))
				{
					const TCHAR* HashField = FCString::Strcmp(Field, TEXT("path")) == 0
						? TEXT("sha256") : TEXT("fade_sha256");
					TestEqual(TEXT("Generated frame hash"), RokTrainArmyMesh::Sha256(Bytes),
						Crop->GetStringField(HashField));
				}
			}
		}
	}
	UGameInstance* Instance = NewObject<UGameInstance>();
	URokWorldMapEntitySubsystem* Population = NewObject<URokWorldMapEntitySubsystem>(Instance);
	if (!TestTrue(TEXT("Population loads"), Population->LoadPopulation())) return false;
	const TArray<TSharedPtr<FJsonValue>>* MonsterRows = nullptr;
	if (!TestTrue(TEXT("Monster source rows"), Root->TryGetArrayField(TEXT("monsters"), MonsterRows) && MonsterRows)) return false;
	TestEqual(TEXT("Ordinary Monster ID count"), MonsterRows->Num(), 47);
	for (const auto& Value : *MonsterRows)
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		const int32 Id = Entry->GetIntegerField(TEXT("monster_id"));
		const int32 Level = Entry->GetIntegerField(TEXT("level"));
		const FString Model = Entry->GetStringField(TEXT("model_id"));
		FRokWorldMapEntityRecord Record;
		TestEqual(TEXT("Source row is explicitly supported"), Entry->GetStringField(TEXT("status")), FString(TEXT("supported")));
		if (!TestTrue(*FString::Printf(TEXT("Monster %d resolves"), Id), Population->ResolveServerEntity(MonsterRow(Id), Record))) continue;
		TestEqual(TEXT("Server source ID"), Record.SourceId, Id);
		TestEqual(TEXT("Server source level"), Record.Level, Level);
		TestEqual(TEXT("Exact server source model"), Record.SourceModelId, Model);
		TestFalse(TEXT("Full formation is not icon-only"), Record.bIconOnlyVisual);
		TestEqual(TEXT("Full source formation count"), Record.FormationUnits.Num(),
			Object(Formations, Model)->GetIntegerField(TEXT("unit_count")));
		for (const FRokWorldMapFormationUnit& Unit : Record.FormationUnits)
		{
			TestTrue(TEXT("Actual unit prefab exists in manifest"), Units->HasField(Unit.UnitPrefab));
			TestEqual(TEXT("All animation frames"), Unit.FrameTexturePaths.Num(), 8);
			TestEqual(TEXT("All fade frames"), Unit.FadeFrameTexturePaths.Num(), 8);
		}
	}
	for (const int32 Level : {7, 18, 19, 20, 21, 22})
	{
		FRokWorldMapEntityRecord Record;
		TestTrue(*FString::Printf(TEXT("Live level %d has a full formation"), Level),
			Population->ResolveServerEntity(MonsterRow(1000 + Level), Record)
			&& !Record.FormationUnits.IsEmpty() && !Record.bIconOnlyVisual);
	}
	return !HasAnyErrors();
}

#endif
