#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "../RokUnityFeatureScreenWidget.h"
#include "../RokWorldMapTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokWorldPositionSearchLocalValidationTest,
	"Rok.WorldMap.PositionSearch.LocalValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokWorldPositionSearchLocalValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	URokUnityFeatureScreenWidget* Screen =
		NewObject<URokUnityFeatureScreenWidget>(GetTransientPackage());
	if (!TestNotNull(TEXT("Search screen"), Screen)
		|| !TestTrue(TEXT("Search screen initializes"), Screen->Initialize()))
	{
		return false;
	}
	Screen->TakeWidget();
	const auto Open = [Screen]() { Screen->OpenWorldPositionSearch(); };
	const auto X = [Screen]() -> UEditableTextBox*
	{
		return Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionXInput")));
	};
	const auto Y = [Screen]() -> UEditableTextBox*
	{
		return Cast<UEditableTextBox>(Screen->GetWidgetFromName(TEXT("WorldPositionYInput")));
	};
	const auto Commit = [](UEditableTextBox* Input, const TCHAR* Value)
	{
		const FText Text = FText::FromString(Value);
		Input->SetText(Text);
		Input->OnTextCommitted.Broadcast(Text, ETextCommit::OnUserMovedFocus);
	};
	const auto Go = [Screen]() -> bool
	{
		UButton* Button = Cast<UButton>(Screen->GetWidgetFromName(TEXT("WorldPositionGo")));
		if (!Button) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	};

	Open();
	if (!TestNotNull(TEXT("X input"), X()) || !TestNotNull(TEXT("Y input"), Y()))
	{
		return false;
	}
	TestEqual(TEXT("Initial local X preview"), X()->GetText().ToString(), FString(TEXT("1146")));
	TestEqual(TEXT("Initial local Y preview"), Y()->GetText().ToString(), FString(TEXT("157")));
	const UTextBlock* Kingdom = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("WorldPositionKingdomValue")));
	TestTrue(TEXT("The kingdom constant is labelled local preview"),
		Kingdom && Kingdom->GetText().ToString().Contains(TEXT("本地预览")));

	Commit(X(), TEXT(""));
	TestTrue(TEXT("Focus-loss commit preserves empty X"), X()->GetText().IsEmpty());
	if (!TestTrue(TEXT("Go button exists"), Go())) return false;
	const UTextBlock* Feedback = Cast<UTextBlock>(
		Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipText")));
	TestTrue(TEXT("Empty X leaves search open"), Screen->IsScreenOpen());
	TestEqual(TEXT("Unity 100716 text"),
		Feedback ? Feedback->GetText().ToString() : FString(), FString(TEXT("未输入坐标")));
	const UCanvasPanel* MiddleTipRoot = Cast<UCanvasPanel>(
		Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipRoot")));
	const UWidget* MiddleTipBackground = Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipBackground"));
	const UBorder* MiddleTipTextHost = Cast<UBorder>(
		Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipTextHost")));
	const UWidget* GenericFeedback = Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"));
	TestTrue(TEXT("Empty coordinate uses a nonblocking Middle tip"),
		MiddleTipRoot && MiddleTipRoot->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("Source text and image share the animated group"),
		Feedback && MiddleTipBackground && MiddleTipTextHost
		&& Feedback->GetParent() == MiddleTipTextHost
		&& MiddleTipTextHost->GetParent() == MiddleTipRoot
		&& MiddleTipBackground->GetParent() == MiddleTipRoot);
	TestTrue(TEXT("Source 22 pixel text uses 16.5 Slate points"),
		Feedback && FMath::IsNearlyEqual(Feedback->GetFont().Size, 16.5f));
	TestTrue(TEXT("Source text remains centered in its fixed rectangle"),
		MiddleTipTextHost && MiddleTipTextHost->GetHorizontalAlignment() == HAlign_Center
		&& MiddleTipTextHost->GetVerticalAlignment() == VAlign_Center);
	TestTrue(TEXT("The opening source transform scales the text with its background"),
		MiddleTipRoot && MiddleTipRoot->GetRenderTransform().Scale.Equals(FVector2D::ZeroVector));
	TestTrue(TEXT("Empty coordinate hides generic feedback"),
		GenericFeedback && GenericFeedback->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("Empty X remains visible after Go"), X()->GetText().IsEmpty());
	Open();
	TestEqual(TEXT("Rejected X did not replace the stored coordinate"),
		X()->GetText().ToString(), FString(TEXT("1146")));

	Commit(X(), TEXT("25"));
	Commit(Y(), TEXT("abc"));
	if (!TestTrue(TEXT("Malformed pair Go exists"), Go())) return false;
	TestTrue(TEXT("Malformed pair stays open"), Screen->IsScreenOpen());
	TestEqual(TEXT("Malformed Y stays editable"), Y()->GetText().ToString(), FString(TEXT("abc")));
	Open();
	TestEqual(TEXT("Valid X is not committed separately from invalid Y"),
		X()->GetText().ToString(), FString(TEXT("1146")));
	TestEqual(TEXT("Invalid Y does not replace the stored coordinate"),
		Y()->GetText().ToString(), FString(TEXT("157")));

	Commit(X(), TEXT("   "));
	Commit(Y(), TEXT("50"));
	if (!TestTrue(TEXT("Whitespace pair Go exists"), Go())) return false;
	Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("WorldPositionMiddleTipText")));
	TestTrue(TEXT("Whitespace stays open"), Screen->IsScreenOpen());
	TestEqual(TEXT("Whitespace uses explicit local empty feedback"),
		Feedback ? Feedback->GetText().ToString() : FString(), FString(TEXT("未输入坐标")));
	TestEqual(TEXT("Whitespace remains raw"), X()->GetText().ToString(), FString(TEXT("   ")));

	Open();
	Commit(X(), TEXT("1201"));
	Commit(Y(), TEXT("4"));
	TestEqual(TEXT("Above-max X normalizes from the validated projection"),
		X()->GetText().ToString(), FString(TEXT("1200")));
	if (!TestTrue(TEXT("Clamped pair Go exists"), Go())) return false;
	Open();
	TestEqual(TEXT("Atomic target X at inclusive max"), X()->GetText().ToString(), FString(TEXT("1200")));
	TestEqual(TEXT("Atomic target Y"), Y()->GetText().ToString(), FString(TEXT("4")));
	Commit(X(), TEXT("-1"));
	Commit(Y(), TEXT("4"));
	TestEqual(TEXT("Negative X normalizes to inclusive zero"),
		X()->GetText().ToString(), FString(TEXT("0")));
	if (!TestTrue(TEXT("Zero-bound pair Go exists"), Go())) return false;
	Open();
	TestEqual(TEXT("Stored zero lower bound"), X()->GetText().ToString(), FString(TEXT("0")));
	TestEqual(TEXT("Stored Y after lower bound"), Y()->GetText().ToString(), FString(TEXT("4")));

	const TCHAR* InvalidIntegers[] = {
		TEXT("12abc"), TEXT("12.5"), TEXT("+"), TEXT("-"),
		TEXT("2147483648"), TEXT("-2147483649"), TEXT("99999999999999999999")};
	for (const TCHAR* Invalid : InvalidIntegers)
	{
		Open();
		Commit(X(), TEXT("100"));
		Commit(Y(), Invalid);
		TestEqual(FString::Printf(TEXT("Invalid input %s remains editable"), Invalid),
			Y()->GetText().ToString(), FString(Invalid));
		if (!TestTrue(TEXT("Invalid pair Go exists"), Go())) return false;
		Feedback = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
		TestTrue(FString::Printf(TEXT("Invalid input %s keeps modal open"), Invalid),
			Screen->IsScreenOpen());
		TestEqual(FString::Printf(TEXT("Invalid input %s shows local validation"), Invalid),
			Feedback ? Feedback->GetText().ToString() : FString(),
			FString(TEXT("请输入有效坐标。")));
		Open();
		TestEqual(FString::Printf(TEXT("Invalid input %s does not commit X"), Invalid),
			X()->GetText().ToString(), FString(TEXT("0")));
		TestEqual(FString::Printf(TEXT("Invalid input %s does not commit Y"), Invalid),
			Y()->GetText().ToString(), FString(TEXT("4")));
	}

	Commit(X(), TEXT("12abc"));
	Commit(Y(), TEXT("50"));
	if (!TestTrue(TEXT("Malformed X pair Go exists"), Go())) return false;
	TestTrue(TEXT("Malformed X keeps modal open"), Screen->IsScreenOpen());
	TestEqual(TEXT("Malformed X remains editable"), X()->GetText().ToString(),
		FString(TEXT("12abc")));
	Open();
	TestEqual(TEXT("Malformed X does not commit valid Y"), Y()->GetText().ToString(),
		FString(TEXT("4")));

	Commit(X(), TEXT("+12"));
	Commit(Y(), TEXT("0"));
	TestEqual(TEXT("Leading plus is a valid integer"), X()->GetText().ToString(),
		FString(TEXT("12")));
	if (!TestTrue(TEXT("Signed pair Go exists"), Go())) return false;
	Open();
	TestEqual(TEXT("Signed valid X stored"), X()->GetText().ToString(),
		FString(TEXT("12")));
	TestEqual(TEXT("Literal zero Y stored"), Y()->GetText().ToString(),
		FString(TEXT("0")));
	Commit(X(), TEXT("-2147483648"));
	TestEqual(TEXT("Int32 minimum parses then clamps to world minimum"),
		X()->GetText().ToString(), FString(TEXT("0")));
	Commit(X(), TEXT("+2147483647"));
	TestEqual(TEXT("Int32 maximum parses then clamps to world maximum"),
		X()->GetText().ToString(), FString(TEXT("1200")));

	const FRokWorldMapProjection Projection;
	TestTrue(TEXT("Source-derived local projection valid"), Projection.IsValid());
	TestEqual(TEXT("Display coordinates scale exactly once"),
		Projection.DisplayToUnity(FVector2D(100.0, 50.0)), FVector2D(600.0, 300.0));
	TestEqual(TEXT("Inclusive maximum maps to 7200"),
		Projection.DisplayToUnity(FVector2D(1200.0, 1200.0)), FVector2D(7200.0, 7200.0));
	return true;
}

#endif
