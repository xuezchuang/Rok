#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "../RokVillagePresentationData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVillageNativeZoomTest,
	"Rok.WorldMap.VillagePresentation.NativeZoomSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokVillageNativeZoomTest::RunTest(const FString& Parameters)
{
	FRokVillagePresentationData Data;
	if (!TestTrue(TEXT("Source village manifest loads"), Data.Load())) return false;
	TestEqual(TEXT("Thirteen source items"), Data.Items.Num(), 13);
	TSet<FString> SourceIds;
	int32 RepeatedWoodNames = 0;
	for (const auto& Item : Data.Items)
	{
		TestFalse(TEXT("Original Unity display name is preserved"), Item.SourceName.IsEmpty());
		if (Item.SourceName == TEXT("wood_9")) ++RepeatedWoodNames;
		SourceIds.Add(Item.SourceTransformId);
	}
	TestEqual(TEXT("Thirteen distinct string Transform identities"), SourceIds.Num(), 13);
	TestEqual(TEXT("Both distinct wood_9 source children survive name duplication"), RepeatedWoodNames, 2);
	FString Text;
	TSharedPtr<FJsonObject> Native;
	if (!TestTrue(TEXT("Native Unity sample evidence loads"),
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_village_presentation_native_probe.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Native) && Native.IsValid())) return false;
	int32 Samples = 0;
	for (const auto& Run : Native->GetArrayField(TEXT("runs")))
	{
		FRokVillagePresentationState State;
		State.Reset(Data.Items.Num());
		for (const auto& Value : Run->AsObject()->GetArrayField(TEXT("samples")))
		{
			const auto Sample = Value->AsObject();
			const float Dxf = Sample->GetNumberField(TEXT("dxf"));
			const float Scale = Data.EvaluateScale(Dxf);
			TestTrue(*FString::Printf(TEXT("Native curve at %g"), Dxf),
				FMath::IsNearlyEqual(Scale, static_cast<float>(Sample->GetNumberField(TEXT("scale"))), 0.00002f));
			State.Update(Data, Scale);
			TArray<FString> Actual;
			for (int32 I = 0; I < Data.Items.Num(); ++I)
				if (State.Active[I]) Actual.Add(Data.Items[I].SourceTransformId);
			TArray<FString> Expected;
			for (const auto& Name : Sample->GetArrayField(TEXT("active"))) Expected.Add(Name->AsString());
			TestEqual(*FString::Printf(TEXT("Native surviving source IDs at %g"), Dxf),
				FString::Join(Actual, TEXT(",")), FString::Join(Expected, TEXT(",")));
			++Samples;
		}
	}
	TestEqual(TEXT("All three native runs compared"), Samples, 33);
	TestTrue(TEXT("Village remains source size at 3000"), FMath::IsNearlyEqual(Data.EvaluateScale(3000), 3.3f, 0.0001f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVillageMalformedDataTest,
	"Rok.WorldMap.VillagePresentation.RejectMalformedObjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokVillageMalformedDataTest::RunTest(const FString& Parameters)
{
	FString Original;
	if (!FFileHelper::LoadFileToString(Original, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_village_presentation.json")))) return false;
	for (int32 Case = 0; Case < 9; ++Case)
	{
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Original), Root)) return false;
		if (Case == 0)
		{
			auto Keys = Root->GetArrayField(TEXT("scale_curve"));
			Keys[1] = MakeShared<FJsonValueNull>();
			Root->SetArrayField(TEXT("scale_curve"), Keys);
		}
		else
		{
			auto Items = Root->GetArrayField(TEXT("items"));
			if (Case == 1)
			{
				Items[0] = MakeShared<FJsonValueNull>();
				Root->SetArrayField(TEXT("items"), Items);
			}
			else
			{
				const auto Item = Items[0]->AsObject();
				auto Sprites = Item->GetArrayField(TEXT("sprites"));
				if (Case == 2)
				{
					Sprites[0] = MakeShared<FJsonValueNull>();
					Item->SetArrayField(TEXT("sprites"), Sprites);
				}
				else if (Case == 3) Sprites[0]->AsObject()->SetField(TEXT("texture"), MakeShared<FJsonValueNull>());
				else if (Case == 4) Sprites[0]->AsObject()->RemoveField(TEXT("mask"));
				else if (Case == 5) Item->RemoveField(TEXT("priority"));
				else if (Case == 6) Items[1]->AsObject()->SetStringField(TEXT("source_transform_id"), Item->GetStringField(TEXT("source_transform_id")));
				else if (Case == 7) Item->SetNumberField(TEXT("source_transform_id"), 1301552526949574372.0);
				else Sprites[0]->AsObject()->RemoveField(TEXT("source_renderer_id"));
			}
		}
		FString Broken;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Broken));
		FRokVillagePresentationData Data;
		TestTrue(TEXT("Valid baseline before rejection"), Data.LoadJson(Original));
		TestFalse(*FString::Printf(TEXT("Malformed object case %d rejected"), Case), Data.LoadJson(Broken));
		TestTrue(TEXT("Failed load publishes no partial items"), Data.Items.IsEmpty());
		TestEqual(TEXT("Failed load publishes no partial curve"), Data.ScaleCurve.GetNumKeys(), 0);
	}
	return true;
}

#endif
