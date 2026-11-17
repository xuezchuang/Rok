#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "../RokLeaderboardOverviewLayout.h"
#include "../RokUnityModalWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokLeaderboardOverviewLayoutTest,
	"Rok.Ui.PlayerProfile.LeaderboardOverviewLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokLeaderboardOverviewLayoutTest::RunTest(const FString&)
{
	using namespace RokLeaderboardOverviewLayout;
	TestEqual(TEXT("Grid includes MiddleCenter vertical surplus"), TilePosition(0), FVector2D(0, 66));
	TestEqual(TEXT("Source third row and column"), TilePosition(8), FVector2D(700, 474));
	TestEqual(TEXT("Nine-slice top-left retains source pixel borders"), BackgroundSlice(0).Size, FVector2D(17, 46));
	TestEqual(TEXT("Nine-slice bottom-right retains source pixel borders"), BackgroundSlice(8).Size, FVector2D(15, 49));
	TestEqual(TEXT("Nine-slice patches cover the entire source layout"), BackgroundSlice(8).Position + BackgroundSlice(8).Size, BackgroundSize());
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerRanking(); Modal->AdvanceWindowAnimationForTesting(1);
	const UCanvasPanel* MainPanel = Cast<UCanvasPanel>(RokUiFind::Find(Modal, TEXT("PlayerRankingMainPanel")));
	TestNotNull(TEXT("Source overview is a canvas without a dark backing"), MainPanel);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		auto* Button = RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryButton_%d"), Index));
		if (!TestNotNull(TEXT("Source hit rectangle retained"), Button)) return false;
		// Each category is a tile WBP; positions are measured inside the overview panel.
		TestEqual(TEXT("Source grid placement"), RokUiFind::CanvasPosition(Button, MainPanel), TilePosition(Index));
		TestEqual(TEXT("Source hit rectangle size"), CastChecked<UCanvasPanelSlot>(Button->Slot)->GetSize(), FVector2D(340, 144));
		auto* Title = CastChecked<UTextBlock>(RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryLabel_%d"), Index)));
		auto* Score = CastChecked<UTextBlock>(RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryState_%d"), Index)));
		auto* Owner = CastChecked<UTextBlock>(RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryOwner_%d"), Index)));
		TestEqual(TEXT("Source title pixel-to-point conversion"), Title->GetFont().Size, 15.f);
		TestEqual(TEXT("Source secondary pixel-to-point conversion"), Score->GetFont().Size, 13.5f);
		TestEqual(TEXT("Source no-response score crossbar"), Score->GetText().ToString(), FString(TEXT("--")));
		TestEqual(TEXT("Source no-response owner crossbar"), Owner->GetText().ToString(), FString(TEXT("--")));
		const auto* Justification = FindFProperty<FByteProperty>(Title->GetClass(), TEXT("Justification"));
		if (!TestNotNull(TEXT("UE exposes the actual justification property"), Justification)) return false;
		TestEqual(TEXT("Chinese title is left aligned"), Justification->GetPropertyValue_InContainer(Title), uint8(ETextJustify::Left));
		auto* Tile = CastChecked<UBorder>(RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryTile_%d"), Index)));
		TestEqual(TEXT("Background uses source white tint"), Tile->GetBrushColor(), FLinearColor::White);
		TestEqual(TEXT("Background source dimensions"), CastChecked<UCanvasPanelSlot>(Tile->Slot)->GetSize(), BackgroundSize());
		for (int32 Part = 0; Part < 9; ++Part)
		{
			auto* Patch = Cast<UImage>(RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategorySlice_%d_%d"), Index, Part)));
			if (!TestNotNull(TEXT("Atlas background uses an explicit source patch"), Patch)) return false;
			TestTrue(TEXT("Patch avoids full-atlas Box margin scaling"), Patch->GetBrush().DrawAs == ESlateBrushDrawType::Image);
			TestEqual(TEXT("Patch has source-derived pixel geometry"), CastChecked<UCanvasPanelSlot>(Patch->Slot)->GetSize(), BackgroundSlice(Part).Size);
		}
		if (Index < 8)
		{
			auto* Icon = RokUiFind::Find(Modal, *FString::Printf(TEXT("PlayerRankingCategoryIcon_%d"), Index));
			if (!TestNotNull(TEXT("Source category icon"), Icon)) return false;
			TestEqual(TEXT("Icon preserves source dimensions"), CastChecked<UCanvasPanelSlot>(Icon->Slot)->GetSize(), IconSize());
			const FVector2D ActualPosition = RokUiFind::CanvasPosition(Icon, MainPanel);
			const FVector2D ExpectedPosition = TilePosition(Index) + IconPosition();
			// CanvasPanelSlot::SetPosition stores FVector2D through float FMargin offsets.
			TestTrue(*FString::Printf(TEXT("Icon follows mirrored parent flag: actual %s expected %s"),
				*ActualPosition.ToString(), *ExpectedPosition.ToString()), ActualPosition.Equals(ExpectedPosition, 0.001));
		}
		else
		{
			TestTrue(TEXT("Empty 401 still has no invented title"), Title->GetText().IsEmpty());
			TestNull(TEXT("Empty 401 still has no invented icon"), RokUiFind::Find(Modal, TEXT("PlayerRankingCategoryIcon_8")));
		}
	}
	CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("PlayerRankingCategoryButton_5")))->OnClicked.Broadcast();
	CastChecked<UButton>(RokUiFind::Find(Modal, TEXT("Type3CloseButton")))->OnClicked.Broadcast();
	TestFalse(TEXT("Source return has no invented child or exit animation"), Modal->GetWindowAnimationForTesting().IsClosing());
	TestNotNull(TEXT("Two-stage close restores source overview"), RokUiFind::Find(Modal, TEXT("PlayerRankingCategoryOwner_5")));
	return true;
}
#endif
