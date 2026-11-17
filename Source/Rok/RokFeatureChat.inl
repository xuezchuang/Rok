void URokUnityFeatureScreenWidget::BuildChat()
{
#if WITH_DEV_AUTOMATION_TESTS
	// Deterministic, offline states for reproducible WBP spec and capture generation.
	int32 CaptureChannel = INDEX_NONE;
	if (FParse::Value(FCommandLine::Get(), TEXT("RokChatCaptureChannel="), CaptureChannel))
		SelectedChatChannel = FMath::Clamp(CaptureChannel, 0, 2);
#endif
	// UI_Win_ChatMediator.OnPopEmojiMenu/OnHideEmojiMenu resize the
	// message region and lift pl_down by pl_emoji.rect.height immediately.
	// UI_Win_Chat.prefab pl_emoji sizeDelta.y = 289.8 (no tween).
	const float EmojiHeight = 289.8f;
	const float EmojiLift = bChatEmojiPanelOpen ? EmojiHeight : 0.0f;
	if (URokUiPageWidget* Page = MountModePage(TEXT("Chat")))
	{
		ChatContentCanvas = Page->Get<UCanvasPanel>(TEXT("ChatContentCanvas"));
		ChatInputTextBox = Page->Get<UEditableTextBox>(TEXT("ChatInputTextBox"));
		UScrollBox* MessageScroll = Page->Get<UScrollBox>(TEXT("ChatMessageScroll"));
		USizeBox* MessageExtent = Page->Get<USizeBox>(TEXT("ChatMessageExtent"));
		UWidget* MessageViewport = Page->Get<UWidget>(TEXT("ChatMessageViewport"));
		if (ChatContentCanvas && ChatInputTextBox && MessageScroll && MessageExtent && MessageViewport)
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				BindPageAction(*Page, *FString::Printf(TEXT("ChatChannel_%d"), Index), 800 + Index);
				const bool bSelected = Index == SelectedChatChannel;
				for (const TCHAR* Prefix : { TEXT("ChatChannelShade"), TEXT("ChatChannelLabel") })
				{
					Page->SetShown(Page->Get<UWidget>(*FString::Printf(TEXT("%s_%d"), Prefix, Index)), !bSelected);
					Page->SetShown(Page->Get<UWidget>(*FString::Printf(TEXT("%sSelected_%d"), Prefix, Index)), bSelected);
				}
				Page->SetPresent(Page->Get<UWidget>(*FString::Printf(TEXT("ChatChannelSelected_%d"), Index)), bSelected);
			}
			for (const TPair<const TCHAR*, int32>& Entry : {
				TPair<const TCHAR*, int32>(TEXT("ChatSendButton"), 810),
				TPair<const TCHAR*, int32>(TEXT("ChatEmojiButton"), 811),
				TPair<const TCHAR*, int32>(TEXT("ChatContactsToggleButton"), 812),
				TPair<const TCHAR*, int32>(TEXT("ChatEmojiDismissMask"), 813),
				TPair<const TCHAR*, int32>(TEXT("ChatEmojiItem_0"), 820),
				TPair<const TCHAR*, int32>(TEXT("ChatEmojiItem_1"), 821)})
			{
				BindPageAction(*Page, Entry.Key, Entry.Value);
			}
			static const FText ChannelLabels[] = {
				NSLOCTEXT("RokChat", "WorldChannel", "王国"),
				NSLOCTEXT("RokChat", "AllianceChannel", "联盟"),
				NSLOCTEXT("RokChat", "PrivateChannel", "私聊")};
			if (UTextBlock* Title = Page->Get<UTextBlock>(TEXT("ChatConversationTitle")))
				Title->SetText(ChannelLabels[SelectedChatChannel]);
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatConversationChannelIcon")), SelectedChatChannel == 0);
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatConversationAllianceIcon")), SelectedChatChannel == 1);
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatFixtureTag")), bChatTestFixtureState);
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatContactsEmptyState")), !bChatTestFixtureState);
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatMessagesEmptyState")), !bChatTestFixtureState);
			for (int32 Index = 0; Index < 2; ++Index)
				Page->SetPresent(Page->Get<UWidget>(*FString::Printf(TEXT("ChatContact_%d"), Index)), bChatTestFixtureState);
			static const TCHAR* Messages[3][3] = {
				{TEXT("[王国] 沙漠之鹰：有人一起探索遗迹吗？"), TEXT("[王国] snow：坐标已分享 X:1146 Y:157"), TEXT("[系统] 欢迎来到第1王国。")},
				{TEXT("[联盟] 盟主：今晚20:00集结联盟要塞。"), TEXT("[联盟] snow：收到，我会提前到达。"), TEXT("[联盟] 盟友：资源点已标记。")},
				{TEXT("[好友] 沙漠之鹰：你的城市升级得怎么样？"), TEXT("[我] 正在升级市政厅。"), TEXT("[好友] 沙漠之鹰：需要资源就告诉我。")} };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				UTextBlock* Message = Page->Get<UTextBlock>(*FString::Printf(TEXT("ChatMessage_%d"), Index));
				if (Message) Message->SetText(FText::FromString(Messages[SelectedChatChannel][Index]));
				Page->SetPresent(Message, bChatTestFixtureState);
				Page->SetPresent(Page->Get<UWidget>(*FString::Printf(TEXT("ChatMessageShade_%d"), Index)), bChatTestFixtureState);
			}
			Page->SetPresent(Page->Get<UWidget>(TEXT("ChatEmojiPanel")), bChatEmojiPanelOpen);
			UWidget* Dismiss = Page->Get<UWidget>(TEXT("ChatEmojiDismissMask"));
			Page->SetPresent(Dismiss, bChatEmojiPanelOpen);
			// Restore the in-memory engine font while preserving the authored style.
			FEditableTextBoxStyle InputStyle = ChatInputTextBox->GetWidgetStyle();
			InputStyle.SetFont(FCoreStyle::GetDefaultFontStyle(
				InputStyle.TextStyle.Font.TypefaceFontName, InputStyle.TextStyle.Font.Size));
			ChatInputTextBox->SetWidgetStyle(InputStyle);
			// UE 5.8 SetWidgetStyle binds Slate to the argument address when
			// already constructed. Rebind to the widget-owned style, not this stack copy.
			ChatInputTextBox->SetWidgetStyle(ChatInputTextBox->GetWidgetStyle());
			ChatInputTextBox->SetText(FText::FromString(ChatDraft));
			ChatInputTextBox->SetHintText(NSLOCTEXT("RokChat", "InputHint", "输入消息…"));
			ChatInputTextBox->SetClearKeyboardFocusOnCommit(false);
			ChatInputTextBox->OnTextCommitted.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleChatInputCommitted);
			MessageScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
			MessageScroll->SetScrollOffset(ChatMessageScrollOffset);
			const auto ResizeForEmoji = [EmojiLift](UWidget* Widget)
			{
				if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
				{
					FVector2D Size = CanvasSlot->GetSize();
					Size.Y = FMath::Max(0.0, Size.Y - EmojiLift);
					CanvasSlot->SetSize(Size);
				}
			};
			ResizeForEmoji(MessageViewport);
			ResizeForEmoji(Page->Get<UWidget>(TEXT("ChatMessageViewportBackground")));
			ResizeForEmoji(MessageScroll);
			const UCanvasPanelSlot* ViewportSlot = Cast<UCanvasPanelSlot>(MessageViewport->Slot);
			const float ViewportHeight = ViewportSlot ? ViewportSlot->GetSize().Y : 594.0f - EmojiLift;
			const float ContentHeight = bChatTestFixtureState ? FMath::Max(403.0f, ViewportHeight) : ViewportHeight;
			MessageExtent->SetHeightOverride(ContentHeight);
			if (bChatEmojiPanelOpen && Dismiss)
			{
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Dismiss->Slot))
				{
					FVector2D Size = CanvasSlot->GetSize(); Size.Y = ContentHeight; CanvasSlot->SetSize(Size);
				}
			}
			if (UWidget* InputRoot = Page->Get<UWidget>(TEXT("ChatInputRoot")))
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(InputRoot->Slot))
				{
					FVector2D Position = CanvasSlot->GetPosition(); Position.Y -= EmojiLift; CanvasSlot->SetPosition(Position);
				}
			if (!bChatTestFixtureState)
				if (UWidget* Empty = Page->Get<UWidget>(TEXT("ChatMessagesEmptyState")))
					if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Empty->Slot))
					{
						FVector2D Position = CanvasSlot->GetPosition(); Position.Y -= EmojiLift * 0.5f; CanvasSlot->SetPosition(Position);
					}
			if (UWidget* Feedback = Page->Get<UWidget>(TEXT("FeatureBoundaryFeedback")))
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Feedback->Slot))
				{
					FVector2D Position = CanvasSlot->GetPosition(); Position.Y -= bChatEmojiPanelOpen ? 260.0f : 0.0f; CanvasSlot->SetPosition(Position);
				}
			return;
		}
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		ChatContentCanvas = nullptr;
		ChatInputTextBox = nullptr;
	}
	ChatContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChatContentCanvas"));
	PlaceOnCanvas(ModeCanvas, ChatContentCanvas, FVector2D::ZeroVector,
		FVector2D(UnityReferenceWidth, UnityReferenceHeight), 0);
	// The panel blocks its own footprint, including while its controls are
	// disabled by entry/exit. Uncovered HUD is never covered by a global mask.
	const auto MakeShield = [this](const TCHAR* Name, UCanvasPanel* Parent,
		const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
	{
		UButton* Shield = WidgetTree->ConstructWidget<URokChatInputShield>(URokChatInputShield::StaticClass(), Name);
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style;
		Style.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush);
		Shield->SetStyle(Style);
		PlaceOnCanvas(Parent, Shield, Position, Size, ZOrder);
	};
	MakeShield(TEXT("ChatPanelInputShield"), ChatContentCanvas, FVector2D::ZeroVector, FVector2D(1024.0f, 810.0f), -1);
	MakeShield(TEXT("ChatAnimationInputShield"), ModeCanvas, FVector2D::ZeroVector, FVector2D(1024.0f, 810.0f), 100);
	MakeShield(TEXT("ChatCloseInputShield"), ModeCanvas, FVector2D(1019.0f, 343.0f), FVector2D(48.0f, 124.0f), 100);
	// Unity UI_Win_Chat is a 1024 px drawer: 330 px contacts on the left and
	// a 694.7 px conversation region on the right. The remaining viewport is
	// deliberately left visible and hit-testable (UI.s_chat: kNone mask).
	const FVector2D FrameSize(1024.0f, 810.0f);
	const FVector2D HalfFrameSize(FrameSize.X * 0.5f, FrameSize.Y - 60.0f);
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatMainBackground"),
			FLinearColor(0.018f, 0.026f, 0.03f, 0.99f)),
		FVector2D::ZeroVector,
		FrameSize,
		0);
	UCanvasPanel* BodyLeft = MakeAtlasBorder(
		TEXT("ChatFrameBodyLeft"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		HalfFrameSize,
		WindowBodyMargin);
	PlaceOnCanvas(
		ChatContentCanvas,
		BodyLeft,
		FVector2D(0.0f, 60.0f),
		HalfFrameSize,
		1);
	UCanvasPanel* BodyRight = MakeAtlasBorder(
		TEXT("ChatFrameBodyRight"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		HalfFrameSize,
		WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ChatContentCanvas,
		BodyRight,
		FVector2D(512.0f, 60.0f),
		HalfFrameSize,
		1);
	UCanvasPanel* HeaderLeft = MakeAtlasBorder(
		TEXT("ChatFrameHeaderLeft"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(512.0f, 60.0f),
		WindowHeaderMargin);
	PlaceOnCanvas(
		ChatContentCanvas,
		HeaderLeft,
		FVector2D::ZeroVector,
		FVector2D(512.0f, 60.0f),
		5);
	UCanvasPanel* HeaderRight = MakeAtlasBorder(
		TEXT("ChatFrameHeaderRight"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(512.0f, 60.0f),
		WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ChatContentCanvas,
		HeaderRight,
		FVector2D(512.0f, 0.0f),
		FVector2D(512.0f, 60.0f),
		5);
	UTextBlock* WindowTitle = MakeScreenText(
		TEXT("ChatWindowTitle"),
		30,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f),
		true,
		ETextJustify::Center);
	WindowTitle->SetText(NSLOCTEXT("RokChat", "ChatTitle", "聊天"));
	PlaceOnCanvas(
		ChatContentCanvas,
		WindowTitle,
		FVector2D(0.0f, 8.0f),
		FVector2D(1024.0f, 42.0f),
		7);
	if (bChatTestFixtureState)
	{
		// Chat history and contacts are server data; the fixture preview must
		// stay explicitly labeled instead of reading as account state.
		UTextBlock* ChatFixtureTag = MakeScreenText(
			TEXT("ChatFixtureTag"),
			14,
			FLinearColor(0.62f, 0.62f, 0.58f, 1.0f),
			false,
			ETextJustify::Right);
		ChatFixtureTag->SetText(NSLOCTEXT(
			"RokChat",
			"FixtureTag",
			"聊天 · 本地参考态（非账号状态）"));
		PlaceOnCanvas(
			ChatContentCanvas,
			ChatFixtureTag,
			FVector2D(704.0f, 14.0f),
			FVector2D(312.0f, 24.0f),
			7);
	}
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeActionButton(
			TEXT("FeatureFrameClose"),
			1,
			ChatAtlasPath,
			ChatCloseRect,
			ChatAtlasSize,
			FVector2D(48.0f, 124.0f)),
		FVector2D(1019.0f, 343.0f),
		FVector2D(48.0f, 124.0f),
		30);

	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatContactsPanel"),
			FLinearColor(0.045f, 0.055f, 0.06f, 0.98f)),
		FVector2D(6.0f, 66.0f),
		FVector2D(320.0f, 730.0f),
		2);
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatMessagesPanel"),
			FLinearColor(0.025f, 0.035f, 0.04f, 0.98f)),
		FVector2D(326.0f, 60.0f),
		FVector2D(695.0f, 750.0f),
		2);

	static const FText ChannelLabels[] = {
		// Unity shows Language 750011 for the world channel contact.
		NSLOCTEXT("RokChat", "WorldChannel", "王国"),
		NSLOCTEXT("RokChat", "AllianceChannel", "联盟"),
		NSLOCTEXT("RokChat", "PrivateChannel", "私聊")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bSelected = Index == SelectedChatChannel;
		const FVector2D RowSize(304.0f, 88.0f);
		UCanvasPanel* ChannelRow = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ChatChannelRow_%d"), Index));
		PlaceOnCanvas(
			ChatContentCanvas,
			ChannelRow,
			FVector2D(11.0f, 82.0f + Index * 94.0f),
			RowSize,
			6);
		PlaceOnCanvas(
			ChannelRow,
			MakeSolidBorder(
				*FString::Printf(TEXT("ChatChannelShade_%d"), Index),
				bSelected
					? FLinearColor(0.12f, 0.15f, 0.16f, 0.98f)
					: FLinearColor(0.06f, 0.075f, 0.08f, 0.94f)),
			FVector2D::ZeroVector,
			RowSize,
			0);
		if (bSelected)
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeAtlasBorder(
					*FString::Printf(TEXT("ChatChannelSelected_%d"), Index),
					ChatAtlasPath,
					ChatSelectedContactRect,
					ChatAtlasSize,
					RowSize,
					FMargin(0.16f)),
				FVector2D::ZeroVector,
				RowSize,
				1);
		}
		if (Index < 2)
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeAtlasImage(
					*FString::Printf(TEXT("ChatChannelIcon_%d"), Index),
					ChatAtlasPath,
					Index == 0 ? ChatWorldIconRect : ChatAllianceIconRect,
					ChatAtlasSize,
					FVector2D(64.0f)),
				FVector2D(12.0f, 12.0f),
				FVector2D(64.0f),
				2);
		}
		else
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeSolidBorder(
					TEXT("ChatPrivateIconShade"),
					FLinearColor(0.16f, 0.18f, 0.18f, 1.0f)),
				FVector2D(18.0f, 18.0f),
				FVector2D(52.0f),
				2);
			UTextBlock* PrivateGlyph = MakeScreenText(
				TEXT("ChatPrivateIconGlyph"),
				24,
				FLinearColor(0.95f, 0.86f, 0.55f, 1.0f),
				true,
				ETextJustify::Center);
			PrivateGlyph->SetText(NSLOCTEXT("RokChat", "PrivateGlyph", "私"));
			PlaceOnCanvas(
				ChannelRow,
				PrivateGlyph,
				FVector2D(18.0f, 25.0f),
				FVector2D(52.0f, 38.0f),
				3);
		}
		UTextBlock* ChannelLabel = MakeScreenText(
			*FString::Printf(TEXT("ChatChannelLabel_%d"), Index),
			23,
			bSelected
				? FLinearColor(1.0f, 0.88f, 0.48f, 1.0f)
				: FLinearColor(0.82f, 0.82f, 0.78f, 1.0f),
			true);
		ChannelLabel->SetText(ChannelLabels[Index]);
		PlaceOnCanvas(
			ChannelRow,
			ChannelLabel,
			FVector2D(94.0f, 25.0f),
			FVector2D(185.0f, 38.0f),
			3);
		PlaceOnCanvas(
			ChannelRow,
			MakeActionButton(
				*FString::Printf(TEXT("ChatChannel_%d"), Index),
				800 + Index,
				nullptr,
				FIntRect(),
				FIntPoint(1, 1),
				RowSize),
			FVector2D::ZeroVector,
			RowSize,
			10);
	}

	UTextBlock* ContactTitle = MakeScreenText(
		TEXT("ChatContactTitle"),
		20,
		FLinearColor(0.95f, 0.85f, 0.52f, 1.0f),
		true,
		ETextJustify::Center);
	ContactTitle->SetText(NSLOCTEXT("RokChat", "Contacts", "私聊联系人"));
	PlaceOnCanvas(
		ChatContentCanvas,
		ContactTitle,
		FVector2D(20.0f, 382.0f),
		FVector2D(292.0f, 36.0f),
		7);
	if (bChatTestFixtureState)
	{
		static const TCHAR* FixtureContacts[] = {
			TEXT("沙漠之鹰  在线"),
			TEXT("远征军  离线")};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UTextBlock* Contact = MakeScreenText(
				*FString::Printf(TEXT("ChatContact_%d"), Index),
				18,
				Index == 0
					? FLinearColor(0.52f, 0.95f, 0.62f, 1.0f)
					: FLinearColor(0.68f, 0.68f, 0.66f, 1.0f));
			Contact->SetText(FText::FromString(FixtureContacts[Index]));
			PlaceOnCanvas(
				ChatContentCanvas,
				Contact,
				FVector2D(34.0f, 436.0f + Index * 58.0f),
				FVector2D(265.0f, 34.0f),
				7);
		}
	}
	else
	{
		UTextBlock* ContactsEmpty = MakeScreenText(
			TEXT("ChatContactsEmptyState"),
			17,
			FLinearColor(0.56f, 0.58f, 0.58f, 1.0f),
			false,
			ETextJustify::Center);
		ContactsEmpty->SetText(
			NSLOCTEXT("RokChat", "NoPrivateContacts", "暂无私聊联系人"));
		PlaceOnCanvas(
			ChatContentCanvas,
			ContactsEmpty,
			FVector2D(34.0f, 442.0f),
			FVector2D(264.0f, 32.0f),
			7);
	}

	PlaceOnCanvas(ChatContentCanvas,
		MakeActionButton(TEXT("ChatContactsToggleButton"), 812,
			ChatAtlasPath, ChatShrinkRect, ChatAtlasSize, FVector2D(32.0f)),
		FVector2D(334.65f, 71.61f), FVector2D(32.0f), 9);
	UTextBlock* ConversationTitle = MakeScreenText(
		TEXT("ChatConversationTitle"),
		24,
		FLinearColor(0.96f, 0.9f, 0.7f, 1.0f),
		true);
	ConversationTitle->SetText(ChannelLabels[SelectedChatChannel]);
	PlaceOnCanvas(
		ChatContentCanvas,
		ConversationTitle,
		FVector2D(438.0f, 72.0f),
		FVector2D(420.0f, 38.0f),
		7);
	if (SelectedChatChannel < 2)
	{
		PlaceOnCanvas(
			ChatContentCanvas,
			MakeAtlasImage(
				TEXT("ChatConversationChannelIcon"),
				ChatAtlasPath,
				SelectedChatChannel == 0
					? ChatWorldIconRect
					: ChatAllianceIconRect,
				ChatAtlasSize,
				FVector2D(48.0f)),
			FVector2D(378.0f, 65.0f),
			FVector2D(48.0f),
			7);
	}
	auto* MessageViewport = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatMessageViewport"));
	MessageViewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	const FVector2D MessageViewportSize(665.0f, 594.0f - EmojiLift);
	PlaceOnCanvas(ChatContentCanvas, MessageViewport, FVector2D(340, 121), MessageViewportSize, 4);
	PlaceOnCanvas(
		MessageViewport,
		MakeSolidBorder(
			TEXT("ChatMessageViewportBackground"),
			FLinearColor(0.012f, 0.018f, 0.021f, 0.94f)),
		FVector2D::ZeroVector,
		MessageViewportSize,
		4);

	// Unity sv_chat is a ScrollRect/ListView, not a fixed collection of
	// message cards. Keep its scroll offset when the emoji dock changes size.
	auto* MessageScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("ChatMessageScroll"));
	MessageScroll->SetOrientation(Orient_Vertical);
	MessageScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	MessageScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	MessageScroll->SetScrollOffset(ChatMessageScrollOffset);
	PlaceOnCanvas(MessageViewport, MessageScroll, FVector2D::ZeroVector, MessageViewportSize, 5);
	auto* MessageExtent = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ChatMessageExtent"));
	const float ContentHeight = bChatTestFixtureState ? FMath::Max(403.0f, float(MessageViewportSize.Y)) : float(MessageViewportSize.Y);
	MessageExtent->SetHeightOverride(ContentHeight);
	MessageScroll->AddChild(MessageExtent);
	auto* MessageContent = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatMessageContent"));
	MessageExtent->AddChild(MessageContent);
	if (bChatTestFixtureState)
	{
		static const TCHAR* WorldMessages[] = {
			TEXT("[王国] 沙漠之鹰：有人一起探索遗迹吗？"),
			TEXT("[王国] snow：坐标已分享 X:1146 Y:157"),
			TEXT("[系统] 欢迎来到第1王国。")};
		static const TCHAR* AllianceMessages[] = {
			TEXT("[联盟] 盟主：今晚20:00集结联盟要塞。"),
			TEXT("[联盟] snow：收到，我会提前到达。"),
			TEXT("[联盟] 盟友：资源点已标记。")};
		static const TCHAR* PrivateMessages[] = {
			TEXT("[好友] 沙漠之鹰：你的城市升级得怎么样？"),
			TEXT("[我] 正在升级市政厅。"),
			TEXT("[好友] 沙漠之鹰：需要资源就告诉我。")};
		const TCHAR* const* Messages = SelectedChatChannel == 0
			? WorldMessages
			: (SelectedChatChannel == 1
				? AllianceMessages
				: PrivateMessages);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			PlaceOnCanvas(
				MessageContent,
				MakeSolidBorder(
					*FString::Printf(TEXT("ChatMessageShade_%d"), Index),
					Index % 2 == 0
						? FLinearColor(0.07f, 0.085f, 0.09f, 0.96f)
						: FLinearColor(0.04f, 0.052f, 0.057f, 0.96f)),
				FVector2D(16.0f, 25.0f + Index * 126.0f),
				FVector2D(632.0f, 102.0f),
				5);
			UTextBlock* Message = MakeScreenText(
				*FString::Printf(TEXT("ChatMessage_%d"), Index),
				18,
				FLinearColor(0.9f, 0.92f, 0.9f, 1.0f));
			Message->SetText(FText::FromString(Messages[Index]));
			PlaceOnCanvas(
				MessageContent,
				Message,
				FVector2D(36.0f, 49.0f + Index * 126.0f),
				FVector2D(592.0f, 56.0f),
				6);
		}
	}
	else
	{
		UTextBlock* MessagesEmpty = MakeScreenText(
			TEXT("ChatMessagesEmptyState"),
			18,
			FLinearColor(0.52f, 0.54f, 0.54f, 1.0f),
			false,
			ETextJustify::Center);
		MessagesEmpty->SetText(
			NSLOCTEXT("RokChat", "NoMessages", "暂无消息"));
		PlaceOnCanvas(
			MessageContent,
			MessagesEmpty,
			FVector2D(140.0f, 253.0f - EmojiLift * 0.5f),
			FVector2D(384.0f, 38.0f),
			6);
	}

	if (bChatEmojiPanelOpen)
	{
		// Keep the dismiss button within the ScrollBox hierarchy so wheel
		// events bubble to the message list instead of leaking to the map.
		const FVector2D DismissSize(MessageViewportSize.X, ContentHeight);
		PlaceOnCanvas(MessageContent, MakeActionButton(TEXT("ChatEmojiDismissMask"),
			813, nullptr, FIntRect(), FIntPoint(1, 1), DismissSize),
			FVector2D::ZeroVector, DismissSize, 30);
	}
	auto* InputRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatInputRoot"));
	PlaceOnCanvas(ChatContentCanvas, InputRoot, FVector2D(0, -EmojiLift), FVector2D(1024, 810), 8);
	InputRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		InputRoot,
		MakeSolidBorder(
			TEXT("ChatInputShade"),
			FLinearColor(0.08f, 0.09f, 0.09f, 1.0f)),
		FVector2D(340.0f, 736.0f),
		FVector2D(520.0f, 48.0f),
		6);
	ChatInputTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("ChatInputTextBox"));
	ChatInputTextBox->SetText(FText::FromString(ChatDraft));
	ChatInputTextBox->SetHintText(
		NSLOCTEXT("RokChat", "InputHint", "输入消息…"));
	FEditableTextBoxStyle ChatInputStyle =
		FEditableTextBoxStyle::GetDefault();
	ChatInputStyle
		.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
		.SetForegroundColor(FLinearColor::White)
		.SetFocusedForegroundColor(FLinearColor::White)
		.SetBackgroundColor(FLinearColor(0.08f, 0.09f, 0.09f, 1.0f))
		.SetPadding(FMargin(16.0f, 8.0f));
	ChatInputTextBox->SetWidgetStyle(ChatInputStyle);
	ChatInputTextBox->SetClearKeyboardFocusOnCommit(false);
	ChatInputTextBox->OnTextCommitted.AddDynamic(
		this,
		&URokUnityFeatureScreenWidget::HandleChatInputCommitted);
	PlaceOnCanvas(
		InputRoot,
		ChatInputTextBox,
		FVector2D(344.0f, 737.0f),
		FVector2D(512.0f, 46.0f),
		7);
	PlaceOnCanvas(
		InputRoot,
		MakeActionButton(
			TEXT("ChatEmojiButton"),
			811,
			EmojiAtlasPath,
			EmojiFaceRect,
			EmojiAtlasSize,
			FVector2D(60.0f)),
		FVector2D(869.0f, 730.0f),
		FVector2D(60.0f),
		8);

	UCanvasPanel* SendVisual = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ChatSendVisual"));
	PlaceOnCanvas(
		InputRoot,
		SendVisual,
		FVector2D(928.0f, 730.0f),
		FVector2D(60.0f),
		8);
	PlaceOnCanvas(
		SendVisual,
		MakeAtlasBorder(
			TEXT("ChatSendBackground"),
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(60.0f),
			MiniBlueMargin),
		FVector2D::ZeroVector,
		FVector2D(60.0f),
		0);
	PlaceOnCanvas(
		SendVisual,
		MakeAtlasImage(
			TEXT("ChatSendIcon"),
			ChatAtlasPath,
			ChatSendRect,
			ChatAtlasSize,
			FVector2D(48.0f, 30.0f)),
		FVector2D(6.0f, 15.0f),
		FVector2D(48.0f, 30.0f),
		1);
	PlaceOnCanvas(
		SendVisual,
		MakeActionButton(
			TEXT("ChatSendButton"),
			810,
			nullptr,
			FIntRect(),
			FIntPoint(1, 1),
			FVector2D(60.0f)),
		FVector2D::ZeroVector,
		FVector2D(60.0f),
		10);

	if (bChatEmojiPanelOpen)
	{
		UCanvasPanel* EmojiPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("ChatEmojiPanel"));
		PlaceOnCanvas(
			ChatContentCanvas,
			EmojiPanel,
			FVector2D(330.0f, 810.0f - EmojiHeight),
			FVector2D(694.0f, EmojiHeight),
			20);
		PlaceOnCanvas(
			EmojiPanel,
			MakeAtlasBorder(
				TEXT("ChatEmojiPanelBackground"),
				EmojiAtlasPath,
				EmojiPanelRect,
				EmojiAtlasSize,
				FVector2D(694.0f, EmojiHeight),
				FMargin(0.16f, 0.22f)),
			FVector2D::ZeroVector,
			FVector2D(694.0f, EmojiHeight),
			0);
		UTextBlock* EmojiTitle = MakeScreenText(
			TEXT("ChatEmojiPanelTitle"),
			20,
			FLinearColor(0.95f, 0.87f, 0.6f, 1.0f),
			true);
		EmojiTitle->SetText(NSLOCTEXT("RokChat", "EmojiTitle", "表情"));
		PlaceOnCanvas(
			EmojiPanel,
			EmojiTitle,
			FVector2D(24.0f, 18.0f),
			FVector2D(180.0f, 34.0f),
			2);
		PlaceOnCanvas(
			EmojiPanel,
			MakeActionButton(
				TEXT("ChatEmojiItem_0"),
				820,
				EmojiAtlasPath,
				EmojiTextRect,
				EmojiAtlasSize,
				FVector2D(88.0f)),
			FVector2D(28.0f, 72.0f),
			FVector2D(88.0f),
			3);
		PlaceOnCanvas(
			EmojiPanel,
			MakeActionButton(
				TEXT("ChatEmojiItem_1"),
				821,
				EmojiAtlasPath,
				EmojiFaceRect,
				EmojiAtlasSize,
				FVector2D(88.0f)),
			FVector2D(132.0f, 72.0f),
			FVector2D(88.0f),
			3);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok chat screen: channel=%d emoji_panel=%d fixture=%d authority=%s."),
		SelectedChatChannel,
		bChatEmojiPanelOpen ? 1 : 0,
		bChatTestFixtureState ? 1 : 0,
		bChatTestFixtureState ? TEXT("local_fixture") : TEXT("server"));
}
