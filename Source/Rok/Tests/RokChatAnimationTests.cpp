#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "../RokChatAnimation.h"
#include "../RokUnityFeatureScreenWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokChatSourceMotionTest,
	"Rok.Ui.Animation.Chat.SourceMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChatSourceMotionTest::RunTest(const FString& Parameters)
{
	FRokChatDrawerAnimation Drawer;
	Drawer.Open();
	TestEqual(TEXT("UA_Chat starts offscreen"), Drawer.GetTranslationX(), -1000.0f);
	Drawer.Advance(Drawer.Duration / 2.0f);
	TestTrue(TEXT("Midpoint alpha"), FMath::IsNearlyEqual(Drawer.Opacity, 0.5f));
	TestTrue(TEXT("Midpoint X"), FMath::IsNearlyEqual(Drawer.GetTranslationX(), -500.0f));
	Drawer.Close();
	TestFalse(TEXT("Reversing does not close immediately"), Drawer.Advance(0.0f));
	TestTrue(TEXT("Reversing preserves pose"), FMath::IsNearlyEqual(Drawer.Opacity, 0.5f));
	TestTrue(TEXT("Close completes at source duration"), Drawer.Advance(Drawer.Duration));
	Drawer.Open();
	Drawer.Advance(Drawer.Duration);
	TestEqual(TEXT("Fully open alpha"), Drawer.Opacity, 1.0f);
	FRokChatContactAnimation Contacts;
	Contacts.Toggle();
	Contacts.Advance(0.1f);
	TestTrue(TEXT("Contacts use InCubic not smoothstep"), FMath::IsNearlyEqual(Contacts.TranslationX, -41.25f));
	Contacts.Advance(0.1f);
	TestEqual(TEXT("Contacts source width"), Contacts.TranslationX, -330.0f);
	Contacts.Toggle();
	Contacts.Advance(0.2f);
	TestEqual(TEXT("Contacts restore"), Contacts.TranslationX, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokChatWidgetMotionTest,
	"Rok.Ui.Animation.Chat.WidgetLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokChatWidgetMotionTest::RunTest(const FString& Parameters)
{
	URokUnityFeatureScreenWidget* Screen = NewObject<URokUnityFeatureScreenWidget>();
	Screen->Initialize();
	const TSharedRef<SWidget> SlateWidget = Screen->TakeWidget();
	Screen->OpenChat();
	auto Widget = [Screen](const TCHAR* Name) { return Screen->GetWidgetFromName(Name); };
	auto Click = [this, Screen, &Widget](const TCHAR* Name)
	{
		UButton* Button = Cast<UButton>(Widget(Name));
		if (!TestNotNull(Name, Button)) return;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
	};
	UWidget* Root = Widget(TEXT("FeatureModeCanvas"));
	if (!TestNotNull(TEXT("Drawer root"), Root)) return false;
	TestEqual(TEXT("Screen does not block uncovered HUD"), Screen->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("No global black mask"), Widget(TEXT("FeatureScreenDimmer"))->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("No global pointer blocker"), Widget(TEXT("FeatureScreenInputBlocker"))->GetVisibility(), ESlateVisibility::Collapsed);
	UButton* Shield = Cast<UButton>(Widget(TEXT("ChatAnimationInputShield")));
	if (!TestNotNull(TEXT("Bounds-matched transition blocker"), Shield)) return false;
	TestFalse(TEXT("Invisible blocker cannot take tab focus"), Shield->GetIsFocusable());
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestTrue(TEXT("Root slides without scaling"), Root->GetRenderTransform().Scale.Equals(FVector2D(1.0f)));
	TestTrue(TEXT("Root at source midpoint"), FMath::IsNearlyEqual(Root->GetRenderTransform().Translation.X, -500.0f));
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestEqual(TEXT("Transition blocker released after opening"), Shield->GetVisibility(), ESlateVisibility::Collapsed);
	UEditableTextBox* Input = Cast<UEditableTextBox>(Widget(TEXT("ChatInputTextBox")));
	if (!TestNotNull(TEXT("Message input"), Input)) return false;
	Input->SetText(FText::FromString(TEXT("未发送草稿")));
	Click(TEXT("ChatContactsToggleButton"));
	Screen->AdvanceWindowAnimationForTesting(0.2f);
	TestTrue(TEXT("Content slides independently"), FMath::IsNearlyEqual(Widget(TEXT("ChatContentCanvas"))->GetRenderTransform().Translation.X, -330.0f));
	Click(TEXT("ChatChannel_1"));
	Input = Cast<UEditableTextBox>(Widget(TEXT("ChatInputTextBox")));
	TestEqual(TEXT("Channel rebuild keeps draft"), Input->GetText().ToString(), FString(TEXT("未发送草稿")));
	TestFalse(TEXT("Channel rebuild does not replay entrance"), Screen->GetChatAnimationForTesting().IsAnimating());
	TestTrue(TEXT("Channel rebuild keeps contacts hidden"), FMath::IsNearlyEqual(Widget(TEXT("ChatContentCanvas"))->GetRenderTransform().Translation.X, -330.0f));
	Click(TEXT("ChatEmojiButton"));
	TestEqual(TEXT("Emoji rebuild keeps draft"), CastChecked<UEditableTextBox>(Widget(TEXT("ChatInputTextBox")))->GetText().ToString(), FString(TEXT("未发送草稿")));
	auto CanvasSlot = [&Widget](const TCHAR* Name) { return CastChecked<UCanvasPanelSlot>(Widget(Name)->Slot); };
	TestTrue(TEXT("Emoji lifts input by source panel height"), FMath::IsNearlyEqual(CanvasSlot(TEXT("ChatInputRoot"))->GetPosition().Y, -289.8, 0.001));
	TestTrue(TEXT("Emoji shrinks message viewport"), FMath::IsNearlyEqual(CanvasSlot(TEXT("ChatMessageViewport"))->GetSize().Y, 304.2, 0.001));
	TestEqual(TEXT("Messages clipped instead of covered by emoji"), Widget(TEXT("ChatMessageViewport"))->GetClipping(), EWidgetClipping::ClipToBoundsAlways);
	TestTrue(TEXT("Emoji occupies bottom of drawer"), FMath::IsNearlyEqual(CanvasSlot(TEXT("ChatEmojiPanel"))->GetPosition().Y, 520.2, 0.001));
	TestFalse(TEXT("Emoji reflow does not replay drawer entrance"), Screen->GetChatAnimationForTesting().IsAnimating());
	auto* MessageScroll = Cast<UScrollBox>(Widget(TEXT("ChatMessageScroll")));
	if (!TestNotNull(TEXT("Messages use a scrollable list"), MessageScroll)) return false;
	TestEqual(TEXT("Wheel does not zoom map behind chat"), MessageScroll->GetConsumeMouseWheel(), EConsumeMouseWheel::Always);
	TestEqual(TEXT("Dismiss mask is inside scrolling content"), Widget(TEXT("ChatEmojiDismissMask"))->GetParent(), Cast<UPanelWidget>(Widget(TEXT("ChatMessageContent"))));
	MessageScroll->SetScrollOffset(45.0f);
	Click(TEXT("ChatEmojiDismissMask"));
	TestEqual(TEXT("Dock resize preserves requested list offset"), CastChecked<UScrollBox>(Widget(TEXT("ChatMessageScroll")))->GetScrollOffset(), 45.0f);
	TestNull(TEXT("Click message area dismisses emoji"), Widget(TEXT("ChatEmojiPanel")));
	TestEqual(TEXT("Input restored after dismissal"), CanvasSlot(TEXT("ChatInputRoot"))->GetPosition().Y, 0.0);
	TestEqual(TEXT("Messages recover full height"), CanvasSlot(TEXT("ChatMessageViewport"))->GetSize().Y, 594.0);
	TestEqual(TEXT("Dismissal preserves draft"), CastChecked<UEditableTextBox>(Widget(TEXT("ChatInputTextBox")))->GetText().ToString(), FString(TEXT("未发送草稿")));
	TestTrue(TEXT("Emoji dismissal preserves contact slide"), FMath::IsNearlyEqual(Widget(TEXT("ChatContentCanvas"))->GetRenderTransform().Translation.X, -330.0f));
	Click(TEXT("FeatureFrameClose"));
	TestTrue(TEXT("Close animates before releasing owner"), Screen->IsScreenOpen());
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestTrue(TEXT("Close still owns drawer at midpoint"), Screen->IsScreenOpen());
	Screen->AdvanceWindowAnimationForTesting(1.0f / 6.0f);
	TestFalse(TEXT("Close releases screen at source duration"), Screen->IsScreenOpen());
	Screen->OpenChat(true);
	Screen->AdvanceWindowAnimationForTesting(1.0f);
	Click(TEXT("ChatEmojiButton"));
	auto* Extent = Cast<USizeBox>(Widget(TEXT("ChatMessageExtent")));
	if (!TestNotNull(TEXT("Fixture messages declare their scroll extent"), Extent)) return false;
	TestEqual(TEXT("All three fixture rows remain reachable below viewport"), Extent->GetHeightOverride(), 403.0f);
	CastChecked<UScrollBox>(Widget(TEXT("ChatMessageScroll")))->SetScrollOffset(90.0f);
	Click(TEXT("ChatChannel_1"));
	TestEqual(TEXT("New conversation does not inherit previous conversation offset"), CastChecked<UScrollBox>(Widget(TEXT("ChatMessageScroll")))->GetScrollOffset(), 0.0f);
	return true;
}

#endif
