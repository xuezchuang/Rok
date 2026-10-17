#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../RokChangeNameLayout.h"
#include "../RokUnityModalWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokChangeNameLayoutTest,
	"Rok.Ui.PlayerProfile.ChangeNameSourceLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRokChangeNameLayoutTest::RunTest(const FString&)
{
	using namespace RokChangeNameLayout;
	auto* Modal = NewObject<URokUnityModalWidget>(); Modal->Initialize();
	const TSharedRef<SWidget> Slate = Modal->TakeWidget();
	Modal->OpenPlayerProfile(); Modal->AdvanceWindowAnimationForTesting(1);
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ProfileRenameButton")))->OnClicked.Broadcast();
	auto* Input = CastChecked<UEditableTextBox>(Modal->GetWidgetFromName(TEXT("ChangeNameInput")));
	TestTrue(TEXT("Opening retains empty editable draft"), Input->GetText().IsEmpty());
	TestEqual(TEXT("Source input location"), CastChecked<UCanvasPanelSlot>(Input->Slot)->GetPosition(), FVector2D(329, 270));
	TestEqual(TEXT("Source full input hit region"), CastChecked<UCanvasPanelSlot>(Input->Slot)->GetSize(), FVector2D(782, 54));
	TestEqual(TEXT("Source 24 pixels becomes 18 points"), Input->GetWidgetStyle().TextStyle.Font.Size, 18.f);
	TestEqual(TEXT("Source text inset twenty pixels per side"), Input->GetWidgetStyle().Padding, FMargin(20, 0));
	TestEqual(TEXT("Chinese input left alignment"), Input->GetJustification(), ETextJustify::Left);
	TestTrue(TEXT("Focus does not paint a stock input frame over atlas"), Input->GetWidgetStyle().BackgroundImageFocused.DrawAs == ESlateBrushDrawType::NoDrawType);
	TestEqual(TEXT("Source placeholder text"), Input->GetHintText().ToString(), FString(TEXT("点击此处输入城主昵称...")));
	auto* Desc = CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("ChangeNameDescription")));
	TestTrue(TEXT("Source description position tolerates float canvas storage"), CastChecked<UCanvasPanelSlot>(Desc->Slot)->GetPosition().Equals(FVector2D(329, 371.9), .001));
	TestTrue(TEXT("Source description height retained"), CastChecked<UCanvasPanelSlot>(Desc->Slot)->GetSize().Equals(FVector2D(782, 103.6), .001));
	TestEqual(TEXT("Source explanation twenty pixels becomes fifteen points"), Desc->GetFont().Size, 15.f);
	TestEqual(TEXT("Source explanation has no shadow"), Desc->GetShadowOffset(), FVector2D::ZeroVector);
	const auto* Justification = FindFProperty<FByteProperty>(Desc->GetClass(), TEXT("Justification"));
	if (!TestNotNull(TEXT("Actual text alignment property"), Justification)) return false;
	TestEqual(TEXT("Chinese description upper-left alignment"), Justification->GetPropertyValue_InContainer(Desc), uint8(ETextJustify::Left));
	auto* Cost = Modal->GetWidgetFromName(TEXT("ChangeNameCostDescription"));
	TestTrue(TEXT("Boundary diagnostics no longer overlap source explanation"), CastChecked<UCanvasPanelSlot>(Cost->Slot)->GetPosition().Y >= DescriptionPosition().Y + DescriptionSize().Y);
	auto* Confirm = CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ChangeNameConfirmButton")));
	Input->SetText(FText::FromString(TEXT("ab"))); Input->OnTextChanged.Broadcast(Input->GetText());
	TestFalse(TEXT("Invalid short input cannot submit"), Confirm->GetIsEnabled());
	Input->SetText(FText::FromString(TEXT("NewGovernor"))); Input->OnTextChanged.Broadcast(Input->GetText());
	TestFalse(TEXT("Valid local input cannot bypass authority boundary"), Confirm->GetIsEnabled());
	Confirm->OnClicked.Broadcast();
	TestTrue(TEXT("Direct delegate remains guarded"), CastChecked<UTextBlock>(Modal->GetWidgetFromName(TEXT("ChangeNameValidationFeedback")))->GetText().ToString().Contains(TEXT("没有发送请求")));
	Modal->AdvanceWindowAnimationForTesting(1);
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("TypeMidCloseButton")))->OnClicked.Broadcast();
	Modal->AdvanceWindowAnimationForTesting(1);
	CastChecked<UButton>(Modal->GetWidgetFromName(TEXT("ProfileRenameButton")))->OnClicked.Broadcast();
	TestTrue(TEXT("Reopen clears draft through existing lifecycle"), CastChecked<UEditableTextBox>(Modal->GetWidgetFromName(TEXT("ChangeNameInput")))->GetText().IsEmpty());
	Modal->CloseModal();
	return true;
}
#endif
