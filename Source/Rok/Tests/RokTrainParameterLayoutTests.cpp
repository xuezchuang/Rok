#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../RokTrainParameterLayout.h"
#include "../RokTrainArmyMeshWidget.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokTrainParameterLayoutTest, "Rok.Ui.TrainArmy.ParameterLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokTrainParameterLayoutTest::RunTest(const FString&)
{
	using namespace RokTrainParameterLayout;
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone(TEXT("RokTrainParameterLayoutTest"));
	UWorld* World = Instance->GetWorld();
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	auto* Screen = NewObject<URokUnityFeatureScreenWidget>(World);
	Screen->Initialize(); const auto Slate = Screen->TakeWidget(); Screen->OpenTrainArmy(nullptr, 1);
	auto* Right = Screen->GetWidgetFromName(TEXT("TrainParameterRoot"));
	if (!TestNotNull(TEXT("Source parameter animation group"), Right)) return false;
	TestTrue(TEXT("Chinese right group origin survives CanvasPanelSlot float storage"),
		CastChecked<UCanvasPanelSlot>(Right->Slot)->GetPosition().Equals(RightOrigin(), .001));
	for (int32 Index = 0; Index < 5; ++Index)
	{
		auto* Head = Screen->GetWidgetFromName(*FString::Printf(TEXT("TrainArmyTier_%d"), Index));
		TestEqual(TEXT("Head belongs to source parameter animator"), static_cast<UWidget*>(Head->GetParent()), Right);
		TestTrue(TEXT("Source head hit rect retains identity and placement"),
			(CastChecked<UCanvasPanelSlot>(Head->Slot)->GetPosition() + RightOrigin()).Equals(HeadHit(Index), .001));
	}
	for (const FName Name : { FName(TEXT("TrainArmyMinus")), FName(TEXT("TrainArmyPlus")), FName(TEXT("TrainArmyMax")) })
		TestEqual(TEXT("Non-source buttons cannot intercept input"), Screen->GetWidgetFromName(Name)->GetVisibility(), ESlateVisibility::Collapsed);
	auto* Input = CastChecked<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantityInput")));
	TestTrue(TEXT("Source numeric input rect"), (CastChecked<UCanvasPanelSlot>(Input->Slot)->GetPosition() + RightOrigin()).Equals(InputPosition(), .001));
	Input->SetText(FText::FromString(TEXT("7"))); Input->OnTextChanged.Broadcast(Input->GetText());
	TestEqual(TEXT("Source count label has no invented prefix"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainArmyQuantity")))->GetText().ToString(), FString(TEXT("7")));
	TestEqual(TEXT("Repositioned cost still updates"), CastChecked<UTextBlock>(Screen->GetWidgetFromName(TEXT("TrainResourceCostAmount_0")))->GetText().ToString(), FString(TEXT("350")));
	TestTrue(TEXT("Source model is not part of parameter animator"), static_cast<UWidget*>(Screen->GetWidgetFromName(TEXT("TrainArmyAnimatedModel"))->GetParent()) != Right);
	const TArray<FName> SharedNames = { TEXT("FeatureFrameBodyLeft"), TEXT("FeatureFrameBodyRight"),
		TEXT("FeatureFrameHeaderLeft"), TEXT("FeatureFrameHeaderRight"), TEXT("FeatureFrameTitle"),
		TEXT("TrainParameterLeftBacking"), TEXT("TrainParameterLeftArt") };
	TArray<FVector2D> Positions, Sizes;
	for (const FName Name : SharedNames)
	{
		auto* Slot = CastChecked<UCanvasPanelSlot>(Screen->GetWidgetFromName(Name)->Slot);
		Positions.Add(Slot->GetPosition()); Sizes.Add(Slot->GetSize());
	}
	auto CheckShared = [&](const FName PortraitName, const FName AnimatedRoot)
	{
		auto* Group = Screen->GetWidgetFromName(AnimatedRoot);
		for (int32 Index = 0; Index < SharedNames.Num(); ++Index)
		{
			auto* Widget = Screen->GetWidgetFromName(SharedNames[Index]);
			if (!TestNotNull(TEXT("Shared source presentation exists in both modes"), Widget)) continue;
			auto* Slot = CastChecked<UCanvasPanelSlot>(Widget->Slot);
			TestTrue(TEXT("Frame and left display keep position"), Slot->GetPosition().Equals(Positions[Index], .001));
			TestTrue(TEXT("Frame and left display keep size"), Slot->GetSize().Equals(Sizes[Index], .001));
			TestTrue(TEXT("Shared display stays outside child animator"), static_cast<UWidget*>(Widget->GetParent()) != Group);
			TestTrue(TEXT("Shared display has no slide translation"), Widget->GetRenderTransform().Translation.IsNearlyZero());
			TestEqual(TEXT("Shared display is opaque through child animation"), Widget->GetRenderOpacity(), 1.f);
		}
		auto* Portrait = Screen->GetWidgetFromName(PortraitName);
		if (TestNotNull(TEXT("Source model exists in both modes"), Portrait))
		{
			auto* Slot = CastChecked<UCanvasPanelSlot>(Portrait->Slot);
			TestTrue(TEXT("Portrait position identical across modes"), Slot->GetPosition().Equals(FVector2D::ZeroVector, .001));
			TestTrue(TEXT("Portrait size identical across modes"), Slot->GetSize().Equals(FVector2D(1440,810), .001));
			TestTrue(TEXT("Portrait outside child animator"), static_cast<UWidget*>(Portrait->GetParent()) != Group);
			TestTrue(TEXT("Portrait remains fixed"), Portrait->GetRenderTransform().Translation.IsNearlyZero());
			TestEqual(TEXT("Portrait opacity unchanged"), Portrait->GetRenderOpacity(), 1.f);
		}
	};
	auto* Model = CastChecked<URokTrainArmyMeshWidget>(Screen->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")));
	TestTrue(TEXT("Source mesh loaded"), Model->IsModelLoaded());
	Model->AdvanceAnimation(0); Model->AdvanceAnimation(.2f);
	const float BeforeDetailTime = Model->GetAnimationTime();
	CheckShared(TEXT("TrainArmyAnimatedModel"), TEXT("TrainParameterRoot"));
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailButton")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("Detail keeps the same mesh instance"), Screen->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")), static_cast<UWidget*>(Model));
	TestEqual(TEXT("Detail retains source clock"), Model->GetAnimationTime(), BeforeDetailTime);
	TestNull(TEXT("Details remove the parameter root"), Screen->GetWidgetFromName(TEXT("TrainParameterRoot")));
	TestNull(TEXT("Details contain no duplicate parameter portrait"), Screen->GetWidgetFromName(TEXT("TrainArmySelectedIcon")));
	CheckShared(TEXT("TrainArmyAnimatedModel"), TEXT("TrainArmyDetailRoot"));
	Screen->AdvanceTrainArmyDetailAnimationForTesting(.125f);
	CheckShared(TEXT("TrainArmyAnimatedModel"), TEXT("TrainArmyDetailRoot"));
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyDetailBackButton")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestNull(TEXT("Return contains no duplicate detail portrait"), Screen->GetWidgetFromName(TEXT("TrainArmyDetailPortrait")));
	CheckShared(TEXT("TrainArmyAnimatedModel"), TEXT("TrainParameterRoot"));
	Screen->AdvanceTrainArmyDetailAnimationForTesting(.125f);
	CheckShared(TEXT("TrainArmyAnimatedModel"), TEXT("TrainParameterRoot"));
	TestEqual(TEXT("Return keeps the same mesh instance"), Screen->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")), static_cast<UWidget*>(Model));
	TestEqual(TEXT("Return retains source clock"), Model->GetAnimationTime(), BeforeDetailTime);
	TestEqual(TEXT("Return preserves requested count"), Screen->GetSelectedTrainCountForAudit(), 7);
	CastChecked<UButton>(Screen->GetWidgetFromName(TEXT("TrainArmyTier_1")))->OnClicked.Broadcast(); Screen->FlushPendingActionForTesting();
	TestEqual(TEXT("New tier resolves actual Arms ID"), Model->GetArmsId(), 10102);
	TestEqual(TEXT("New tier resets idle clock"), Model->GetAnimationTime(), 0.f);
	for (const FName Name : { FName(TEXT("TrainArmyParameterCostBackground")), FName(TEXT("TrainArmyParameterQuantityBand")), FName(TEXT("TrainResourceCostRoot")) })
		TestEqual(TEXT("Unknown research hides operation decoration"), Screen->GetWidgetFromName(Name)->GetVisibility(), ESlateVisibility::Collapsed);
	Screen->CloseScreen();
	TestFalse(TEXT("Close releases loaded mesh data"), Model->IsModelLoaded());
	return true;
}
#endif
