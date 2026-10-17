#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "../RokDataPaths.h"
#include "../RokFoodDepositData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFoodNativeZoomTest,
	"Rok.WorldMap.FoodDeposit.NativeZoomSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFoodNativeZoomTest::RunTest(const FString& Parameters)
{
	FRokFoodDepositData Data;
	if (!TestTrue(TEXT("Source food manifest loads"), Data.Load())) return false;
	TestEqual(TEXT("Nine source items"), Data.Items.Num(), 9);
	FString Text;
	TSharedPtr<FJsonObject> Native;
	if (!TestTrue(TEXT("Native Unity sample evidence loads"),
		FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_food_deposit_native_probe.json")))
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Native) && Native.IsValid())) return false;
	int32 Samples = 0;
	for (const auto& Run : Native->GetArrayField(TEXT("runs")))
	{
		FRokFoodDepositState State;
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
				if (State.Active[I]) Actual.Add(Data.Items[I].Name);
			TArray<FString> Expected;
			for (const auto& Name : Sample->GetArrayField(TEXT("active"))) Expected.Add(Name->AsString());
			TestEqual(*FString::Printf(TEXT("Native surviving source IDs at %g"), Dxf),
				FString::Join(Actual, TEXT(",")), FString::Join(Expected, TEXT(",")));
			++Samples;
		}
	}
	TestEqual(TEXT("All three native runs compared"), Samples, 33);
	TestEqual(TEXT("Source zero scale at exact 3000 remains zero"), Data.EvaluateScale(3000), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokFoodMalformedDataTest,
	"Rok.WorldMap.FoodDeposit.RejectMalformedObjects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokFoodMalformedDataTest::RunTest(const FString& Parameters)
{
	FString Original;
	if (!FFileHelper::LoadFileToString(Original, *(RokDataPaths::Resolve(TEXT("unity_food_deposit.json"))))) return false;
	for (int32 Case = 0; Case < 6; ++Case)
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
				else Item->RemoveField(TEXT("priority"));
			}
		}
		FString Broken;
		FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Broken));
		FRokFoodDepositData Data;
		TestTrue(TEXT("Valid baseline before rejection"), Data.LoadJson(Original));
		TestFalse(*FString::Printf(TEXT("Malformed object case %d rejected"), Case), Data.LoadJson(Broken));
		TestTrue(TEXT("Failed load publishes no partial items"), Data.Items.IsEmpty());
		TestEqual(TEXT("Failed load publishes no partial curve"), Data.ScaleCurve.GetNumKeys(), 0);
	}
	return true;
}

#endif
