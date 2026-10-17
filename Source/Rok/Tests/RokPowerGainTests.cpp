#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokDataPaths.h"
#include "../RokPowerGainWidget.h"
#include "../RokMainHudWidget.h"
#include "../RokWorldMapHudWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "Rendering/SlateRenderer.h"
#include "HAL/PlatformProperties.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainPreloadTest, "Rok.Ui.Animation.PowerGain.Preload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerGainPreloadTest::RunTest(const FString&)
{
	auto* Effect = NewObject<URokPowerGainWidget>();
	const auto Slate = Effect->TakeWidget();
	TestEqual(TEXT("Rebuild loads both source textures before any gain"), Effect->Textures.Num(), 2);
	TestEqual(TEXT("Rebuild creates both additive materials before any gain"), Effect->Materials.Num(), 2);
	TestFalse(TEXT("Preloading does not start playback"), Effect->IsPlaying());
	TestEqual(TEXT("Preloading leaves the animation clock at zero"), Effect->GetPlaybackTime(), 0.0f);
	TestEqual(TEXT("Preloading creates no particle emissions"), Effect->ParticleTimes.Num(), 0);
	TestFalse(TEXT("Preloading does not fabricate a power observation"), Effect->bHasPowerObservation);
#if WITH_EDITOR
	if (!FPlatformProperties::RequiresCookedData())
		TestTrue(TEXT("Rebuild submits the base material shader preload request"), Effect->bShaderPreloadRequested);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainSourceTest, "Rok.Ui.Animation.PowerGain.SourceCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerGainSourceTest::RunTest(const FString&)
{
	FString Json;
	if (!TestTrue(TEXT("Native Unity export exists"), FFileHelper::LoadFileToString(Json,
		*(RokDataPaths::Resolve(TEXT("PowerGain/source_geometry.json")))))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)) return false;
	for (const auto& Value : Root->GetArrayField(TEXT("samples")))
	{
		const auto Sample = Value->AsObject();
		const float Time = Sample->GetNumberField(TEXT("time"));
		TestTrue(FString::Printf(TEXT("UV matches native AnimationCurve at %.6f"), Time),
			FMath::IsNearlyEqual(URokPowerGainWidget::EvaluateUTiling(Time), float(Sample->GetNumberField(TEXT("uTiling"))), 0.00001f));
	}
	TestEqual(TEXT("Source final key holds after clip end"), URokPowerGainWidget::EvaluateUTiling(2), -0.09f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainHudTest, "Rok.Ui.Animation.PowerGain.HudLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerGainHudTest::RunTest(const FString&)
{
	auto* City = NewObject<URokMainHudWidget>();
	auto* World = NewObject<URokWorldMapHudWidget>();
	City->Initialize(); World->Initialize();
	const auto CitySlate = City->TakeWidget();
	const auto WorldSlate = World->TakeWidget();
	for (bool bWorld : {false, true})
	{
		UUserWidget* Hud = bWorld ? static_cast<UUserWidget*>(World) : City;
		auto* Effect = Cast<URokPowerGainWidget>(Hud->GetWidgetFromName(bWorld ? TEXT("WorldMapPowerGain") : TEXT("PlayerPowerGain")));
		if (!TestNotNull(TEXT("Actual HUD owns the effect widget"), Effect)) return false;
		auto Set = [&](int32 Value) { if (bWorld) World->SetPowerPresentation(Value); else City->SetPowerPresentation(Value); };
		auto Advance = [&](float Delta) { if (bWorld) World->AdvancePowerPresentation(Delta); else City->AdvanceHudAnimations(Delta); };
		Set(1000);
		TestFalse(TEXT("Initial snapshot does not create a gain"), Effect->IsPlaying());
		Set(1100);
		TestTrue(TEXT("Positive visible update starts real effect"), Effect->IsPlaying());
		Advance(0.25f);
		Set(1100);
		TestEqual(TEXT("Repeated refresh does not restart the effect"), Effect->GetPlaybackTime(), 0.25f);
		Set(1200);
		TestEqual(TEXT("New gain restarts mesh animation"), Effect->GetPlaybackTime(), 0.0f);
		TestEqual(TEXT("Source AutoDestory extends absolute deadline"), Effect->GetRemainingTime(), 1.75f);
		Set(0);
		TestTrue(TEXT("Decrease to zero does not kill existing effect"), Effect->IsPlaying());
		Advance(1.75f);
		TestTrue(TEXT("Source root survives the exact extended deadline"), Effect->IsPlaying());
		TestEqual(TEXT("Exact deadline has zero remaining duration"), Effect->GetRemainingTime(), 0.f);
		Advance(.0001f);
		TestFalse(TEXT("Source root ends strictly after the deadline"), Effect->IsPlaying());
		Set(100);
		TestTrue(TEXT("Initialized zero to positive is a gain"), Effect->IsPlaying());
		Advance(1.0001f);
		Hud->SetVisibility(ESlateVisibility::Hidden);
		Set(200);
		TestFalse(TEXT("Hidden owner cannot trigger gain"), Effect->IsPlaying());
		Hud->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Set(200);
		TestFalse(TEXT("Revealing does not replay hidden updates"), Effect->IsPlaying());
		Hud->SetRenderOpacity(0);
		Set(300);
		TestFalse(TEXT("Transparent owner cannot trigger gain"), Effect->IsPlaying());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainParticleClockTest, "Rok.Ui.Animation.PowerGain.ParticleBirthAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerGainParticleClockTest::RunTest(const FString&)
{
	auto* Effect = NewObject<URokPowerGainWidget>();
	if (!TestTrue(TEXT("Actual exported source assets load"), Effect->Play())) return false;
	TestTrue(TEXT("Play requests emission without fabricating a born particle"), Effect->ParticleTimes[0].bPendingBirth);
	Effect->Advance(0);
	TestTrue(TEXT("Zero time is not a particle update"), Effect->ParticleTimes[0].bPendingBirth);
	Effect->Advance(.031f);
	TestFalse(TEXT("First positive update emits"), Effect->ParticleTimes[0].bPendingBirth);
	TestEqual(TEXT("New particle starts at age zero even with a long first update"), Effect->ParticleTimes[0].Age, 0.f);
	TestTrue(TEXT("Tape index zero is empty"), Effect->ParticleFrames[0].Indices.IsEmpty());
	TestFalse(TEXT("Tape index one is the birth mesh"), Effect->ParticleFrames[1].Indices.IsEmpty());
	TestEqual(TEXT("Birth mesh begins transparent"), Effect->ParticleFrames[1].Colors[0].A, 0.f);
	Effect->Advance(.95f);
	Effect->Play();
	TestEqual(TEXT("Replay retains the older emission"), Effect->ParticleTimes.Num(), 2);
	TestTrue(TEXT("Replay appends a pending burst"), Effect->ParticleTimes[1].bPendingBirth);
	Effect->Advance(.031f);
	TestEqual(TEXT("Old burst survives request-time one-second boundary"), Effect->ParticleTimes.Num(), 2);
	TestTrue(TEXT("Older age advances independently"), FMath::IsNearlyEqual(Effect->ParticleTimes[0].Age, .981f));
	TestEqual(TEXT("Replay birth did not inherit first update delta"), Effect->ParticleTimes[1].Age, 0.f);
	Effect->Advance(.02f);
	TestEqual(TEXT("Only genuinely expired age-one burst retires"), Effect->ParticleTimes.Num(), 1);
	TestTrue(TEXT("New burst remains independently timed"), FMath::IsNearlyEqual(Effect->ParticleTimes[0].Age, .02f));
	TestTrue(TEXT("Extended root remains alive after old burst retirement"), Effect->IsPlaying());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokPowerGainRenderTest, "Rok.Ui.Animation.PowerGain.RenderReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokPowerGainRenderTest::RunTest(const FString&)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokPowerGainVisual")))
	{
		AddInfo(TEXT("Visual capture not requested; run with -RokPowerGainVisual and a real RHI"));
		return true;
	}
	auto* Effect = NewObject<URokPowerGainWidget>();
	Effect->AddToRoot();
	const auto Body = SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(9, 9, 9))) [Effect->TakeWidget()];
	const auto Window = SNew(SWindow).ClientSize(FVector2D(640, 360)).SizingRule(ESizingRule::FixedSize)
		.CreateTitleBar(false).UseOSWindowBorder(false).SupportsMaximize(false).SupportsMinimize(false) [Body];
	FSlateApplication::Get().AddWindow(Window);
	Window->SetDPIScaleFactor(1);
	FSlateApplication::Get().Tick();
	if (!TestTrue(TEXT("Source assets load into Slate player"), Effect->Play()))
	{
		FSlateApplication::Get().RequestDestroyWindow(Window); Effect->RemoveFromRoot(); return false;
	}
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	FSlateApplication::Get().GetRenderer()->FlushCommands();
	// Let the engine publish newly loaded material render resources before capture.
	ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this, Effect, Body, Window]()
	{
		// First paint can submit on-demand Slate material permutations. Drain those
		// after an engine tick, not only the queue that existed before first paint.
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FSlateApplication::Get().GetRenderer()->FlushCommands();
		int32 PreviousFrame = 0;
		for (int32 Frame : {0, 9, 20, 51, 102, 119})
		{
			// Match the export's actual update schedule: index 1 is the first
			// particle birth, not a .075s first update with a pre-aged particle.
			while (PreviousFrame < Frame) { Effect->Advance(1.f / 120.f); ++PreviousFrame; }
			FSlateApplication::Get().Tick();
			TArray<FColor> Pixels; FIntVector Size;
			TestTrue(TEXT("Actual Slate renderer returns pixels"), FSlateApplication::Get().TakeScreenshot(Body, Pixels, Size));
			if (Size.X > 0 && Size.Y > 0 && Pixels.Num() == Size.X * Size.Y)
			{
				if (Frame == 9 || Frame == 20)
					TestTrue(TEXT("Reference capture contains the effect, not only a background"),
						Pixels.ContainsByPredicate([](FColor Pixel) { return Pixel.R > 100 && Pixel.G > 40; }));
				TArray64<uint8> Png;
				FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
				TestTrue(TEXT("Reference capture saved"), FFileHelper::SaveArrayToFile(Png,
					*(FPaths::ProjectSavedDir() / FString::Printf(TEXT("RokUnityMigration/PowerGain/ue_%03d.png"), Frame))));
			}
		}
		FSlateApplication::Get().RequestDestroyWindow(Window);
		Effect->RemoveFromRoot();
	}, 1.0f));
	return true;
}
#endif
