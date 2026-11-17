#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "../RokBuildingPanelWidget.h"
#include "../RokBuildingPreviewGlowWidget.h"
#include "../RokBuildingSourcePreviewData.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokUnityModalWidget.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../RokBuildingPreviewGlowData.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGlowNativeSamples,"Rok.Ui.BuildingPreviewGlow.NativeSamples",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokGlowNativeSamples::RunTest(const FString& Parameters)
{
    using namespace RokBuildingPreviewGlow;FData Data;FString Error;
    const bool Loaded=Data.Load(FData::DefaultPath(),Error);if(!TestTrue(TEXT("Source data loads"),Loaded)){AddError(Error);return false;}
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*FData::DefaultPath())||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root))return false;
    double MaxAlpha=0,MaxAngle=0,MaxPixel=0;int32 Count=0,Projected=0;
    for(const auto& Value:Root->GetArrayField(TEXT("native_samples")))
    {
        const auto R=Value->AsObject();TArray<FLayerSample> Sample;
        if(!Data.Sample(R->GetNumberField(TEXT("time")),Sample))return false;
        const auto& Alpha=R->GetArrayField(TEXT("alpha"));const auto& Rotation=R->GetArrayField(TEXT("rotation"));const auto& Quad=R->GetArrayField(TEXT("screen_quad"));
        for(int32 I=0;I<2;++I)
        {
            MaxAlpha=FMath::Max(MaxAlpha,FMath::Abs(Sample[I].Alpha-Alpha[I]->AsNumber()));
            const auto& Q=Rotation[I]->AsArray();FQuat Native(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber());Native.Normalize();
            MaxAngle=FMath::Max(MaxAngle,FMath::RadiansToDegrees(Sample[I].LocalRotation.AngularDistance(Native)));
            if(R->GetBoolField(TEXT("projection_valid")))for(int32 V=0;V<4;++V){const auto& XY=Quad[I]->AsArray()[V]->AsArray();MaxPixel=FMath::Max(MaxPixel,FVector2D::Distance(Sample[I].ScreenVertices[V],FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber())));}
        }
        ++Count;if(R->GetBoolField(TEXT("projection_valid")))++Projected;
    }
    TestEqual(TEXT("All actual native alpha/quaternion samples"),Count,1400);TestEqual(TEXT("Projection excludes known pre-first-Camera.Render sample only"),Projected,1399);
    TestTrue(TEXT("Source alpha vs native"),MaxAlpha<.000002);
    TestTrue(TEXT("Source integrated local quaternion vs native"),MaxAngle<.001);
    TestTrue(TEXT("Source geometry and stable actual native projection"),MaxPixel<.02);
    AddInfo(FString::Printf(TEXT("max alpha %.9g angle %.9g pixel %.9g"),MaxAlpha,MaxAngle,MaxPixel));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokGlowClock,"Rok.Ui.BuildingPreviewGlow.ClockAndCurve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRokGlowClock::RunTest(const FString& Parameters)
{
    using namespace RokBuildingPreviewGlow;FData Data;FString Error;if(!Data.Load(FData::DefaultPath(),Error)){AddError(Error);return false;}
    TArray<FLayerSample> A,B;Data.Sample(.25,A);
    TestTrue(TEXT("Hermite first quarter is not linear interpolation"),FMath::IsNearlyEqual(A[0].Alpha,.1359375,1e-9));
    Data.Sample(0,A);Data.Sample(2,B);TestTrue(TEXT("Alpha loops exactly at 2 seconds"),FMath::IsNearlyEqual(A[0].Alpha,B[0].Alpha,1e-12));
    TestFalse(TEXT("Rotation does not reset at alpha loop"),A[0].LocalRotation.Equals(B[0].LocalRotation,1e-6));
    FPlayback Clock;TestTrue(TEXT("Clock advances explicit local delta"),Clock.Advance(.5));Clock.Reset();TestEqual(TEXT("Reset returns explicit time zero"),Clock.GetTime(),0.0);
    TestFalse(TEXT("Negative delta refused"),Clock.Advance(-1));TestFalse(TEXT("Infinite delta refused"),Clock.Advance(std::numeric_limits<double>::infinity()));
    TestFalse(TEXT("Negative sample refused"),Data.Sample(-1,A));TestFalse(TEXT("NaN sample refused"),Data.Sample(std::numeric_limits<double>::quiet_NaN(),A));
    Clock.Sample(Data,A);Data.Sample(0,B);TestTrue(TEXT("Reset reproduces source initial quaternion"),A[1].LocalRotation.Equals(B[1].LocalRotation,1e-12));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokBuilderHutPreviewTest, "Rok.Ui.BuildingPreviewGlow.BuilderHut",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokBuilderHutPreviewTest::RunTest(const FString&)
{
    auto* Instance = NewObject<UGameInstance>(GEngine);
    Instance->InitializeStandalone(TEXT("RokBuilderHutPreviewTest"));
    UWorld* World = Instance->GetWorld();
    ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
    auto* Hut = World->SpawnActor<AActor>();
    auto* Barracks = World->SpawnActor<AActor>();
    Hut->Tags = {TEXT("UnityBuildingType_7"), TEXT("UnityBuildingLevel_1")};
    Barracks->Tags = {TEXT("UnityBuildingType_9"), TEXT("UnityBuildingLevel_1")};

    auto* Hud = NewObject<URokMainHudWidget>(World);
    if (!TestTrue(TEXT("City HUD initializes"), Hud->Initialize())) return false;
    Hud->TakeWidget();
    Hud->SetSelectedBuilding(Hut);
    auto* Panel = Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget")));
    auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
    if (!TestNotNull(TEXT("Building menu"), Panel) || !TestNotNull(TEXT("Building modal"), Modal)) return false;
    auto* Info = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingInfoButton")));
    auto* Feature = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingFeatureButton")));
    auto* Upgrade = Cast<UButton>(Panel->GetWidgetFromName(TEXT("BuildingUpgradeButton")));
    if (!TestNotNull(TEXT("Info command"), Info) || !TestNotNull(TEXT("Worker command"), Feature)
        || !TestNotNull(TEXT("Shared upgrade command"), Upgrade)) return false;
    TestEqual(TEXT("Worker hut hides upgrade"), Upgrade->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Worker hut shows info"), Info->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("Worker hut shows queue action"), Feature->GetVisibility(), ESlateVisibility::Visible);
    const auto* InfoSlot = CastChecked<UCanvasPanelSlot>(Info->Slot);
    const auto* FeatureSlot = CastChecked<UCanvasPanelSlot>(Feature->Slot);
    TestEqual(TEXT("Both commands sit on the same row"), InfoSlot->GetPosition().Y, FeatureSlot->GetPosition().Y);
    Info->OnClicked.Broadcast();
    TestTrue(TEXT("Info opens for the selected worker hut"), Modal->IsBuildingInfoOpenForActor(Hut));
    auto* Image = Cast<UImage>(RokUiFind::Find(Modal, TEXT("BuildingInfoImage")));
    auto* Glow = Cast<URokBuildingPreviewGlowWidget>(RokUiFind::Find(Modal, TEXT("BuildingPreviewGlowWidget")));
    if (!TestNotNull(TEXT("Worker sprite"), Image) || !TestNotNull(TEXT("Worker glow"), Glow)) return false;
    const auto* ImageSlot = CastChecked<UCanvasPanelSlot>(Image->Slot);
    const auto* GlowSlot = CastChecked<UCanvasPanelSlot>(Glow->Slot);
    TestTrue(TEXT("Worker sprite keeps source dimensions at 70 percent"), ImageSlot->GetSize().Equals(FVector2D(358.4, 309.4), 0.01));
    TestTrue(TEXT("Source glow plays behind the sprite"), Glow->IsPlaybackActive() && Glow->IsAutoAdvance()
        && GlowSlot->GetZOrder() < ImageSlot->GetZOrder());

    Modal->CloseModal();
    Hud->SetSelectedBuilding(Barracks);
    TestEqual(TEXT("Barracks still shows the upgrade command"), Upgrade->GetVisibility(), ESlateVisibility::Visible);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokAllBuildingPreviewWindows, "Rok.Ui.BuildingPreviewGlow.AllWindows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRokAllBuildingPreviewWindows::RunTest(const FString&)
{
	auto* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokAllBuildingPreviewWindows"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Hud = NewObject<URokMainHudWidget>(World);
	if (!TestNotNull(TEXT("Temporary city subsystem"), Upgrades)
		|| !TestTrue(TEXT("City HUD initializes"), Hud->Initialize())) return false;
	Hud->TakeWidget();
	auto* Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	if (!TestNotNull(TEXT("City building modal"), Modal)) return false;

	for (int32 Type : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
		28, 29, 30, 31, 51, 52, 53})
	{
		auto* Actor = World->SpawnActor<AActor>();
		Upgrades->ApplyServerBuilding(Actor, Type, 1, FIntPoint::ZeroValue);
		RokBuildingSourcePreview::FVisual Art;
		if (!TestTrue(FString::Printf(TEXT("Type %d source prefab"), Type),
			RokBuildingSourcePreview::FindVisual(Type, 101, 1, Art))) continue;
		const auto* Texture = RokBuildingSourcePreview::LoadTexture(Art);
		TestTrue(FString::Printf(TEXT("Type %d samples RGBA8 in sRGB"), Type),
			Texture && Texture->SRGB && Texture->GetPixelFormat() == PF_B8G8R8A8);
		for (bool bUpgrade : {false, true})
		{
			if (bUpgrade) Modal->OpenBuildingUpgrade(Actor);
			else Modal->OpenBuildingInfo(Actor);
			const FString Label = FString::Printf(TEXT("Type %d %s"), Type, bUpgrade ? TEXT("upgrade") : TEXT("info"));
			const auto* Image = Cast<UImage>(RokUiFind::Find(Modal, bUpgrade
				? TEXT("BuildingUpgradeImage") : TEXT("BuildingInfoImage")));
			auto* Glow = Cast<URokBuildingPreviewGlowWidget>(RokUiFind::Find(Modal, TEXT("BuildingPreviewGlowWidget")));
			TestTrue(Label + TEXT(" uses authored texture"), Image && Image->GetBrush().GetResourceObject()
				&& Image->GetBrush().GetResourceObject() == Texture);
			TestTrue(Label + TEXT(" uses active source glow"), Glow && Glow->IsPlaybackActive() && Glow->IsAutoAdvance()
				&& Glow->AdvancePlayback(0.25) && Glow->GetSourcePhase() > 0);
			if (Image)
			{
				const auto* Pane = Image->GetParent();
				const auto* PaneSlot = Pane ? Cast<UCanvasPanelSlot>(Pane->Slot) : nullptr;
				const auto* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot);
				if (TestTrue(Label + TEXT(" has a clipped preview pane"), PaneSlot && ImageSlot
					&& Pane->GetClipping() == EWidgetClipping::ClipToBoundsAlways))
				{
					const FVector2D Min = ImageSlot->GetPosition() + ImageSlot->GetSize() * Art.Visible.Min;
					const FVector2D Max = ImageSlot->GetPosition() + ImageSlot->GetSize() * Art.Visible.Max;
					TestTrue(Label + TEXT(" visible sprite fits the pane"), Min.X >= 0 && Min.Y >= 0
						&& Max.X <= PaneSlot->GetSize().X && Max.Y <= PaneSlot->GetSize().Y);
				}
			}
			Modal->CloseModal();
		}
	}

	auto* Feature = Hud->GetFeatureScreenWidgetForPlacement();
	if (!TestNotNull(TEXT("Build City screen"), Feature)) return false;
	Feature->OpenBuildCity();
	const TArray<TArray<int32>> CategoryTypes = {
		{2, 3, 4, 5, 7, 13, 15, 16, 19, 20, 21, 30}, {9, 10, 11, 12, 14, 17, 18, 28, 29, 31}, {51, 52, 53}};
	for (int32 Category = 0; Category < CategoryTypes.Num(); ++Category)
	{
		auto* Tab = Cast<UButton>(Feature->GetWidgetFromName(*FString::Printf(TEXT("BuildCityCategory_%d"), Category)));
		if (!TestNotNull(TEXT("Build City category route"), Tab)) continue;
		Tab->OnClicked.Broadcast();
		Feature->FlushPendingActionForTesting();
		for (const int32 Type : CategoryTypes[Category])
		{
			const auto* Image = Cast<UImage>(Feature->GetWidgetFromName(*FString::Printf(TEXT("BuildCityCardPreview_%d"), Type)));
			TestTrue(FString::Printf(TEXT("Build City card %d uses source art"), Type),
				Image && Image->GetBrush().GetResourceObject());
		}
	}
	Feature->CloseScreen();
	return true;
}
#endif
