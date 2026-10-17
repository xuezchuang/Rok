#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokVillageDayNight.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVillageDayNightSamplesTest,
	"Rok.WorldMap.VillagePresentation.DayNight.SourceSamples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokVillageDayNightSamplesTest::RunTest(const FString& Parameters)
{
	FRokVillageDayNightData Data;
	if (!TestTrue(TEXT("Source light and material data loads"), Data.Load())) return false;
	FString Text; TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/unity_village_day_night.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root)) return false;
	int32 Count = 0;
	for (const auto& Value : Root->GetArrayField(TEXT("samples")))
	{
		const auto Row = Value->AsObject();
		const FString ScenarioName = Row->GetStringField(TEXT("scenario"));
		const auto* ScenarioValue = Root->GetArrayField(TEXT("scenarios")).FindByPredicate([&](const auto& Candidate)
			{return Candidate->AsObject()->GetStringField(TEXT("name")) == ScenarioName;});
		if (!TestNotNull(TEXT("Explicit source scenario"), ScenarioValue)) return false;
		const auto Scenario = (*ScenarioValue)->AsObject();
		FRokVillageDayNightSample Input;
		Input.FromPreset = Row->GetNumberField(TEXT("from_preset")); Input.ToPreset = Row->GetNumberField(TEXT("to_preset"));
		Input.DurationSeconds = Row->GetNumberField(TEXT("duration")); Input.ElapsedSeconds = Row->GetNumberField(TEXT("elapsed"));
		Input.MaterialDelaySeconds = Row->GetNumberField(TEXT("fixed_delay")); Input.ShaderTimeSeconds = Input.ElapsedSeconds;
		Input.bPreviousNight = Scenario->GetBoolField(TEXT("initial_night")); Input.bRequestedNight = Scenario->GetBoolField(TEXT("requested_night"));
		FRokVillageDayNightResult Output;
		if (!TestTrue(*Row->GetStringField(TEXT("name")), Data.Evaluate(Input, Output))) return false;
		TestEqual(TEXT("Post-deadline material choice"), Output.bNightMaterial, Row->GetBoolField(TEXT("resolved_night")));
		const auto& Color = Row->GetArrayField(TEXT("sprite_color"));
		TestTrue(TEXT("Source interpolated SpriteColor RGB"), Output.SpriteColor.Equals(
			FLinearColor(Color[0]->AsNumber(), Color[1]->AsNumber(), Color[2]->AsNumber(), 1), .000002f));
		TestTrue(TEXT("Source material team color one"), Output.Color1.Equals(Output.bNightMaterial ? FLinearColor::White : Data.DayColor1));
		TestTrue(TEXT("Source material team color two"), Output.Color2.Equals(Output.bNightMaterial ? FLinearColor::White : Data.DayColor2));
		++Count;
	}
	TestEqual(TEXT("All explicit 3 delay x 2 scenario x 4 time samples"), Count, 24);
	FRokVillageDayNightSample Invalid; Invalid.FromPreset = 4;
	FRokVillageDayNightResult Out;
	TestFalse(TEXT("Unknown preset rejected"), Data.Evaluate(Invalid, Out));
	Invalid = {}; Invalid.DurationSeconds = 0;
	TestFalse(TEXT("Undefined zero-duration interpolation rejected"), Data.Evaluate(Invalid, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokVillageDayNightMathTest,
	"Rok.WorldMap.VillagePresentation.DayNight.ShaderMath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokVillageDayNightMathTest::RunTest(const FString& Parameters)
{
	// Different intensity and direction isolate the cross term: lerping final
	// SpriteColor endpoints gives .75; actual source inputs give .5625.
	FRokVillageLightPreset From, To;
	From.Directional = FLinearColor::White; From.Intensity = 0;
	To.Directional = FLinearColor(2,2,2,1); To.Intensity = 1;
	TestTrue(TEXT("Interpolate light inputs before their product"),
		FMath::IsNearlyEqual(RokVillageDayNight::SpriteColor(From,To,.5f,.25f).R,.5625f));
	TestEqual(TEXT("Fade alpha is independent"), RokVillageDayNight::SpriteColor(From,To,.5f,.25f).A,.25f);
	TestEqual(TEXT("Night pulse at source time zero"), RokVillageDayNight::NightPulse(0), .5f);
	TestEqual(TEXT("Night pulse saturates"), RokVillageDayNight::NightPulse(PI), 1.f);
	FRokVillageDayNightResult State;
	State.SpriteColor = FLinearColor(.2f,.3f,.4f,0);
	State.NightLight = FLinearColor(.6f,.4f,.2f,1);
	const FLinearColor Main(.3f,.4f,.5f,.8f), Renderer(1,1,1,.5f), Mask(.5f,0,0,1);
	const FLinearColor NoWindows = RokVillageDayNight::ShadeGamma(Main,Mask,Renderer,State,.25f);
	TestTrue(TEXT("Zero-blue mask cannot manufacture night windows"), NoWindows.Equals(FLinearColor(.06f,.12f,.2f,.1f),.000001f));
	const FLinearColor Windows = RokVillageDayNight::ShadeGamma(Main,FLinearColor(.5f,0,1,1),Renderer,State,.25f);
	TestTrue(TEXT("Synthetic blue isolates additive light independent of SpriteColor"), Windows.Equals(FLinearColor(.66f,.52f,.4f,.1f),.000001f));
	return true;
}
#endif
