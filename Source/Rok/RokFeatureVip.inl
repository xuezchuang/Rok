void URokUnityFeatureScreenWidget::BuildVip()
{
	const int32 PreviewLevel = FMath::Clamp(SelectedVipPreviewLevel, 0, VipLevelCount - 1);
	const FVipPresentation State = ReadVipPresentation();
	LastVipPresentation = State;
	const bool bHasAccountVipLevel = State.CurrentLevel != INDEX_NONE;
	const int32 CurrentVipLevel = FMath::Max(0, State.CurrentLevel);
	const bool bAtMaxLevel = State.Points >= 0 && CurrentVipLevel == VipLevelCount - 1;
	const bool bDailyClaimable = State.DailyClaimed.IsSet() && !State.DailyClaimed.GetValue();
	const bool bFreeChestClaimable = State.FreeChestClaimed.IsSet() && !State.FreeChestClaimed.GetValue()
		&& State.CurrentLevel == PreviewLevel;
	const bool bSpecialChestPurchased = (State.PurchasedLevels & (1u << PreviewLevel)) != 0;
	FRokUnityCatalogVipLevel VipLevelConfig;
	TArray<FRokUnityCatalogVipAttribute> VipAttributes;
	if (const URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem())
	{
		Catalog->GetVipLevel(PreviewLevel, VipLevelConfig);
		VipAttributes = Catalog->GetVipAttributesForLevel(PreviewLevel);
	}
	if (bVipTestFixtureState && PreviewLevel == 0 && VipLevelConfig.PriceDisplay.IsEmpty())
	{
		VipLevelConfig.PriceDisplay = TEXT("$0.99");
	}

	const double Progress = bAtMaxLevel ? 1.0 : (State.LevelThreshold > 0
		? FMath::Clamp(static_cast<double>(State.Points) / State.LevelThreshold, 0.0, 1.0) : 0.0);
	FText ProgressLabel = NSLOCTEXT("RokVip", "ProgressUnavailable", "VIP进度未同步");
	if (bAtMaxLevel)
	{
		ProgressLabel = NSLOCTEXT("RokVip", "MaxLevel", "已达最高等级");
	}
	else if (State.LevelThreshold > 0)
	{
		ProgressLabel = FText::Format(NSLOCTEXT("RokVip", "ProgressFormat", "{0}/{1}"),
			FText::AsNumber(State.Points), FText::AsNumber(State.LevelThreshold));
	}
	const bool bCanPreviewPurchase = State.Points >= 0 && CurrentVipLevel >= PreviewLevel
		&& !bSpecialChestPurchased && !VipLevelConfig.PriceDisplay.IsEmpty();

	const FText LevelText = bHasAccountVipLevel ? FText::AsNumber(CurrentVipLevel) : FText::FromString(TEXT("--"));
	const FText TodayText = State.TodayPoints >= 0
		? FText::Format(NSLOCTEXT("RokVip", "TodayPointsFormat", "今日VIP点数：{0}"), FText::AsNumber(State.TodayPoints))
		: NSLOCTEXT("RokVip", "TodayPointsUnavailable", "今日VIP点数：未同步");
	const FText DaysText = State.LoginDays >= 0
		? FText::Format(NSLOCTEXT("RokVip", "LoginDaysFormat", "连续登录天数：{0}"), FText::AsNumber(State.LoginDays))
		: NSLOCTEXT("RokVip", "LoginDaysUnavailable", "连续登录天数：未同步");
	const FText DailyLabel = State.DailyClaimed.IsSet()
		? (bDailyClaimable ? NSLOCTEXT("RokVip", "Available", "可领取") : NSLOCTEXT("RokVip", "Claimed", "已领取"))
		: NSLOCTEXT("RokVip", "AwaitingLogin", "待登录同步");
	const FText ClaimLabel = bFreeChestClaimable ? NSLOCTEXT("RokVip", "Claim", "领取")
		: !State.FreeChestClaimed.IsSet() ? NSLOCTEXT("RokVip", "ClaimRequiresLogin", "需登录")
		: State.CurrentLevel != PreviewLevel ? NSLOCTEXT("RokVip", "ClaimRequiresLevel", "当前等级专享")
		: NSLOCTEXT("RokVip", "Claimed", "已领取");
	const FText BuyLabel = bSpecialChestPurchased ? NSLOCTEXT("RokVip", "Purchased", "已购买")
		: State.Points >= 0 ? FText::FromString(VipLevelConfig.PriceDisplay)
		: NSLOCTEXT("RokVip", "PurchaseRequiresLogin", "需登录");
	const FText BenefitHeader = FText::Format(NSLOCTEXT("RokVip", "BenefitTitleFormat", "VIP {0}专属增益"),
		FText::AsNumber(PreviewLevel));
	const FText ChestHeader = FText::Format(NSLOCTEXT("RokVip", "ChestTitleFormat", "VIP {0}专属宝箱"),
		FText::AsNumber(PreviewLevel));

	const auto LogPresentation = [&]()
	{
		UE_LOG(LogTemp, Display, TEXT("Rok VIP screen: account_level=%s current_level=%d preview_level=%d fixture=%d session_state=%d authority=%s."),
			bHasAccountVipLevel ? TEXT("known") : TEXT("unknown"),
			bHasAccountVipLevel ? CurrentVipLevel : INDEX_NONE, PreviewLevel,
			bVipTestFixtureState ? 1 : 0, State.Points >= 0 ? 1 : 0,
			bVipTestFixtureState ? TEXT("local_fixture") : TEXT("server"));
	};

	UClass* BenefitRowClass = ResolveFeaturePageClass(TEXT("VipBenefitRow"));
	if (const URokUiPageWidget* Page = BenefitRowClass ? MountModePage() : nullptr)
	{
		if (auto* Badge = Page->Get<UImage>(TEXT("VipCurrentLevelBadge")))
		{
			FSlateBrush Brush = Badge->GetBrush();
			const FSlateBrush Atlas = MakeAtlasBrush(LoadUiTexture(VipLevelAtlasPath),
				VipLevelIconRects[FMath::Clamp(CurrentVipLevel, 0, VipLevelCount - 1)], VipLevelAtlasSize, Brush.GetImageSize());
			Brush.SetResourceObject(Atlas.GetResourceObject());
			Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
			Badge->SetBrush(Brush);
		}
		SetFeaturePageText(*Page, TEXT("VipCurrentLevelText"), LevelText);
		SetFeaturePageText(*Page, TEXT("VipTodayPoints"), TodayText);
		SetFeaturePageText(*Page, TEXT("VipLoginDays"), DaysText);
		SetFeaturePageText(*Page, TEXT("VipProgressText"), ProgressLabel);
		SetFeaturePageText(*Page, TEXT("VipDailyPointLabel"), DailyLabel);
		SetFeaturePageText(*Page, TEXT("VipBenefitTitle"), BenefitHeader);
		SetFeaturePageText(*Page, TEXT("VipChestTitle"), ChestHeader);
		SetFeaturePageText(*Page, TEXT("VipClaimFreeChestLabel"), ClaimLabel);
		SetFeaturePageText(*Page, TEXT("VipBuySpecialChestLabel"), BuyLabel);

		auto* ProgressFill = Page->Get<UImage>(TEXT("VipProgressFill"));
		Page->SetPresent(ProgressFill, Progress > 0.0);
		if (ProgressFill && Progress > 0.0)
		{
			if (auto* FillSlot = Cast<UCanvasPanelSlot>(ProgressFill->Slot))
			{
				const FVector2D Size = FillSlot->GetSize();
				FillSlot->SetSize({Size.X * Progress, Size.Y});
			}
		}
		Page->SetPresent(Page->Get<UWidget>(TEXT("VipDailyPointRedPoint")), bDailyClaimable);
		Page->SetPresent(Page->Get<UWidget>(TEXT("VipFreeChestRedPoint")), bFreeChestClaimable);
		Page->SetPresent(Page->Get<UWidget>(TEXT("VipFixtureTag")), bVipTestFixtureState);
		Page->SetPresent(Page->Get<UWidget>(TEXT("VipPreviousPage")), PreviewLevel > 0);
		Page->SetPresent(Page->Get<UWidget>(TEXT("VipNextPage")), PreviewLevel + 1 < VipLevelCount);
		Page->SetShown(Page->Get<UWidget>(TEXT("VipFreeClaimableBackground")), bFreeChestClaimable);
		Page->SetShown(Page->Get<UWidget>(TEXT("VipFreeUnavailableBackground")), !bFreeChestClaimable);
		Page->SetShown(Page->Get<UWidget>(TEXT("VipAddPoints")), !bAtMaxLevel);

		const auto BindEnabled = [&](FName Name, int32 Action, bool bEnabled)
		{
			if (auto* Button = Page->Get<URokUnityIndexedButton>(Name))
			{
				Button->SetIsEnabled(bEnabled);
				BindPageAction(*Page, Name, Action);
			}
		};
		BindPageAction(*Page, TEXT("VipHelp"), 742);
		BindEnabled(TEXT("VipAddPoints"), 741, State.Points >= 0);
		BindEnabled(TEXT("VipDailyPointChest"), 740, bDailyClaimable && !State.bClaimPending);
		BindEnabled(TEXT("VipFreeChestHit"), 743, bFreeChestClaimable && !State.bClaimPending);
		BindEnabled(TEXT("VipClaimFreeChest"), 743, bFreeChestClaimable && !State.bClaimPending);
		BindEnabled(TEXT("VipSpecialChestHit"), 744, bCanPreviewPurchase);
		BindEnabled(TEXT("VipBuySpecialChest"), 744, bCanPreviewPurchase);
		if (PreviewLevel > 0) BindPageAction(*Page, TEXT("VipPreviousPage"), 700);
		if (PreviewLevel + 1 < VipLevelCount) BindPageAction(*Page, TEXT("VipNextPage"), 701);

		if (auto* Scroll = Page->Get<UScrollBox>(TEXT("VipBenefitsScroll")))
		{
			for (int32 Index = 0; Index < VipAttributes.Num(); ++Index)
			{
				const auto& Attribute = VipAttributes[Index];
				auto* Row = CreateWidget<URokUiPageWidget>(this, BenefitRowClass);
				if (!Row) continue;
				Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				Row->LegacyPrefix = TEXT("VipBenefit");
				Row->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
				Row->LegacyNames.Add(TEXT("Row"), TEXT("{Prefix}Row{Suffix}"));
				auto* LayoutRoot = Row->Get<UCanvasPanel>(TEXT("Row"));
				const auto* LayoutSlot = LayoutRoot ? Cast<UCanvasPanelSlot>(LayoutRoot->Slot) : nullptr;
				if (!LayoutSlot) continue;
				auto* NewText = Row->Get<UTextBlock>(TEXT("NewText"));
				auto* PlainText = Row->Get<UTextBlock>(TEXT("PlainText"));
				Row->SetPresent(NewText, Attribute.bNew);
				Row->SetPresent(PlainText, !Attribute.bNew);
				Row->SetPresent(Row->Get<UWidget>(TEXT("New")), Attribute.bNew);
				Row->SetPresent(Row->Get<UWidget>(TEXT("NewLabel")), Attribute.bNew);
				const FName TextName = Attribute.bNew ? FName(TEXT("NewText")) : FName(TEXT("PlainText"));
				Row->LegacyNames.Add(TextName, TEXT("{Prefix}{Suffix}"));
				SetFeaturePageText(*Row, TextName, FText::FromString(Attribute.DisplayTextZhCn));
				Scroll->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("VipBenefitRowSize_%d"), Index), LayoutSlot->GetSize()));
			}
		}

		if (bVipDailyPointHelpVisible) BuildVipDailyPointHelpTip();
		LogPresentation();
		return;
	}

	BuildSplitWindowFrame(
		CapturePoint(160.0f, 107.0f),
		FVector2D(1120.0f, 700.0f),
		NSLOCTEXT("RokVip", "Title", "VIP"),
		CapturePoint(1187.0f, 98.0f), nullptr, 1, 32.0f);
	if (auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("FeatureFrameTitle"))))
	{
		Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 23));
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipContentShade"), FLinearColor(0.10f, 0.10f, 0.10f, 0.80f)),
		CapturePoint(208.0f, 162.0f),
		FVector2D(1024.0f, 612.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("VipCurrentLevelBadge"),
			VipLevelAtlasPath,
			VipLevelIconRects[FMath::Clamp(CurrentVipLevel, 0, VipLevelCount - 1)],
			VipLevelAtlasSize,
			FVector2D(138.0f)),
		CapturePoint(213.0f, 168.0f),
		FVector2D(138.0f),
		5);
	UTextBlock* CurrentLevelText = MakeScreenText(TEXT("VipCurrentLevelText"), 34, FLinearColor(0.97f, 0.93f, 0.80f, 1.0f), false, ETextJustify::Center);
	CurrentLevelText->SetText(LevelText);
	PlaceOnCanvas(ModeCanvas, CurrentLevelText, CapturePoint(252.0f, 211.0f), FVector2D(60.0f), 6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("VipHelp"), 742, CommonAtlasPath, VipHelpRect, CommonAtlasSize, FVector2D(28.0f)),
		CapturePoint(371.0f, 181.0f),
		FVector2D(28.0f),
		8);
	UTextBlock* TodayPoints = MakeScreenText(TEXT("VipTodayPoints"), 17, FLinearColor(0.92f, 0.92f, 0.9f, 1.0f));
	TodayPoints->SetText(TodayText);
	PlaceOnCanvas(ModeCanvas, TodayPoints, CapturePoint(410.0f, 181.0f), FVector2D(316.0f, 30.0f), 6);

	UTextBlock* LoginDays = MakeScreenText(TEXT("VipLoginDays"), 16,
		FLinearColor(0.92f, 0.92f, 0.9f, 1.0f), false, ETextJustify::Right);
	LoginDays->SetText(DaysText);
	PlaceOnCanvas(ModeCanvas, LoginDays, CapturePoint(826.0f, 182.0f), FVector2D(240.0f, 30.0f), 6);

	const FVector2D TrackPosition = CapturePoint(404.0f, 226.0f);
	const FVector2D TrackSize(594.0f, 36.0f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipProgressBackground"), FLinearColor(0.036f, 0.036f, 0.036f, 0.98f)),
		TrackPosition,
		TrackSize,
		5);
	const FVector2D BarPosition = TrackPosition + FVector2D(0.0f, 9.0f);
	const FVector2D BarSize(TrackSize.X, 18.0f);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("VipProgressTrack"), FLinearColor(0.019f, 0.019f, 0.019f, 1.0f)),
		BarPosition, BarSize, 6);
	if (Progress > 0.0)
	{
		const FVector2D FillSize(BarSize.X * Progress, BarSize.Y);
		PlaceOnCanvas(ModeCanvas,
			MakeAtlasImage(TEXT("VipProgressFill"), CommonAtlasPath, VipProgressFillRect, CommonAtlasSize, FillSize),
			BarPosition, FillSize, 7);
	}
	UTextBlock* ProgressText = MakeScreenText(TEXT("VipProgressText"), 14, FLinearColor::White, false, ETextJustify::Center);
	ProgressText->SetText(ProgressLabel);
	PlaceOnCanvas(ModeCanvas, ProgressText, TrackPosition + FVector2D(0.0f, 8.0f), FVector2D(TrackSize.X, 23.0f), 8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipCurrentPointMarker"), CommonAtlasPath, VipCurrentPointRect, CommonAtlasSize, FVector2D(58.0f)),
		CapturePoint(360.0f, 215.0f),
		FVector2D(58.0f),
		8);
	URokUnityIndexedButton* AddPointsButton = MakeActionButton(
		TEXT("VipAddPoints"),
		741,
		CommonAtlasPath,
		VipAddRect,
		CommonAtlasSize,
		FVector2D(50.0f));
	AddPointsButton->SetIsEnabled(State.Points >= 0);
	AddPointsButton->SetVisibility(bAtMaxLevel ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	PlaceOnCanvas(
		ModeCanvas,
		AddPointsButton,
		CapturePoint(1007.0f, 220.0f),
		FVector2D(47.0f),
		8);

	URokUnityIndexedButton* DailyPointChestButton = MakeActionButton(
		TEXT("VipDailyPointChest"),
		740,
		TreasureBoxAtlasPath,
		VipDailyPointChestRect,
		TreasureBoxAtlasSize,
		FVector2D(116.0f, 94.0f));
	DailyPointChestButton->SetRenderTransformPivot(FVector2D(0.5f));
	DailyPointChestButton->SetRenderScale(FVector2D(-1.0f, 1.0f));
	DailyPointChestButton->SetIsEnabled(bDailyClaimable && !State.bClaimPending);
	PlaceOnCanvas(
		ModeCanvas,
		DailyPointChestButton,
		CapturePoint(1099.0f, 181.0f),
		FVector2D(116.0f, 94.0f),
		7);
	if (bDailyClaimable)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("VipDailyPointRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(18.0f)),
			CapturePoint(1165.0f, 202.0f),
			FVector2D(18.0f),
			9);
	}
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipDailyPointLabelShade"), FLinearColor(0.08f, 0.08f, 0.08f, 0.82f)),
		CapturePoint(1095.0f, 255.0f),
		FVector2D(100.0f, 25.0f),
		7);
	UTextBlock* DailyPointLabel = MakeScreenText(TEXT("VipDailyPointLabel"), 14, FLinearColor::White, false, ETextJustify::Center);
	DailyPointLabel->SetText(DailyLabel);
	PlaceOnCanvas(ModeCanvas, DailyPointLabel, CapturePoint(1095.0f, 255.0f), FVector2D(100.0f, 25.0f), 8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipMainPanel"), FLinearColor(0.075f, 0.075f, 0.075f, 0.98f)),
		CapturePoint(235.0f, 316.0f),
		FVector2D(973.0f, 419.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("VipMainPanelFrame"), CommonAtlasPath, TaskRowRect, CommonAtlasSize, FVector2D(973.0f, 419.0f), TaskRowMargin),
		CapturePoint(235.0f, 316.0f),
		FVector2D(973.0f, 419.0f),
		4);

	UTextBlock* BenefitTitle = MakeScreenText(TEXT("VipBenefitTitle"), 17, FLinearColor::White, false, ETextJustify::Center);
	BenefitTitle->SetText(BenefitHeader);
	PlaceOnCanvas(ModeCanvas, BenefitTitle, CapturePoint(267.0f, 337.0f), FVector2D(447.0f, 32.0f), 6);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipBenefitsPanel"), FLinearColor(0.018f, 0.018f, 0.018f, 1.0f)),
		CapturePoint(267.0f, 379.0f),
		FVector2D(447.0f, 321.0f),
		5);

	UScrollBox* BenefitsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VipBenefitsScroll"));
	BenefitsScroll->SetOrientation(Orient_Vertical);
	BenefitsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	BenefitsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(ModeCanvas, BenefitsScroll, CapturePoint(283.0f, 396.0f), FVector2D(420.0f, 292.0f), 7);

	for (int32 Index = 0; Index < VipAttributes.Num(); ++Index)
	{
		const FRokUnityCatalogVipAttribute& Attribute = VipAttributes[Index];
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("VipBenefitRow_%d"), Index));
		USizeBox* RowSize = WrapFixedSize(
			Row,
			*FString::Printf(TEXT("VipBenefitRowSize_%d"), Index),
			FVector2D(410.0f, 50.0f));
		BenefitsScroll->AddChild(RowSize);

		float TextX = 8.0f;
		if (Attribute.bNew)
		{
			PlaceOnCanvas(
				Row,
				MakeAtlasImage(*FString::Printf(TEXT("VipBenefitNew_%d"), Index), CommonAtlasPath, NewRect, CommonAtlasSize, FVector2D(43.0f, 45.0f)),
				FVector2D(0.0f, -2.0f),
				FVector2D(43.0f, 45.0f),
				1);
			UTextBlock* NewLabel = MakeScreenText(*FString::Printf(TEXT("VipBenefitNewLabel_%d"), Index), 14, FLinearColor::White, true, ETextJustify::Center);
			NewLabel->SetText(NSLOCTEXT("RokVip", "New", "新"));
			PlaceOnCanvas(Row, NewLabel, FVector2D(4.0f, 7.0f), FVector2D(34.0f, 24.0f), 2);
			TextX = 53.0f;
		}

		UTextBlock* Benefit = MakeScreenText(*FString::Printf(TEXT("VipBenefit_%d"), Index), 17, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
		Benefit->SetText(FText::FromString(Attribute.DisplayTextZhCn));
		PlaceOnCanvas(Row, Benefit, FVector2D(TextX, 6.0f), FVector2D(354.0f - TextX, 31.0f), 2);
	}

	UTextBlock* ChestTitle = MakeScreenText(TEXT("VipChestTitle"), 17, FLinearColor::White, false, ETextJustify::Center);
	ChestTitle->SetText(ChestHeader);
	PlaceOnCanvas(ModeCanvas, ChestTitle, CapturePoint(730.0f, 337.0f), FVector2D(440.0f, 32.0f), 6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipFreeChestPanel"), FLinearColor(0.018f, 0.018f, 0.018f, 1.0f)),
		CapturePoint(730.0f, 379.0f),
		FVector2D(440.0f, 155.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipDailyFreeTitle"), VipPlayerAtlasPath, VipDailyTitleRect, VipPlayerAtlasSize, FVector2D(454.0f, 75.0f)),
		CapturePoint(722.0f, 359.0f),
		FVector2D(454.0f, 75.0f),
		7);
	UTextBlock* DailyFreeText = MakeScreenText(TEXT("VipDailyFreeText"), 17, FLinearColor::White, false, ETextJustify::Center);
	DailyFreeText->SetText(NSLOCTEXT("RokVip", "DailyFree", "每日免费"));
	PlaceOnCanvas(ModeCanvas, DailyFreeText, CapturePoint(722.0f, 375.0f), FVector2D(454.0f, 30.0f), 8);
	UImage* FreeChest = MakeAtlasImage(TEXT("VipFreeChest"), TreasureBoxAtlasPath, VipFreeChestRect, TreasureBoxAtlasSize, FVector2D(92.0f, 110.0f));
	FreeChest->SetRenderTransformPivot(FVector2D(0.5f));
	FreeChest->SetRenderTransformAngle(90.0f);
	PlaceOnCanvas(ModeCanvas, FreeChest, CapturePoint(747.0f, 420.0f), FVector2D(92.0f, 115.0f), 7);
	URokUnityIndexedButton* FreeChestHitButton = MakeActionButton(
		TEXT("VipFreeChestHit"),
		743,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(109.2f, 77.3f));
	FreeChestHitButton->SetIsEnabled(bFreeChestClaimable && !State.bClaimPending);
	PlaceOnCanvas(
		ModeCanvas,
		FreeChestHitButton,
		CapturePoint(738.5f, 430.0f),
		FVector2D(109.2f, 77.3f),
		9);
	URokUnityIndexedButton* ClaimFreeChestButton = MakeActionButton(
		TEXT("VipClaimFreeChest"),
		743,
		CommonAtlasPath,
		bFreeChestClaimable ? MiniGreenButtonRect : MiniBlueButtonRect,
		CommonAtlasSize,
		FVector2D(138.0f, 54.0f),
		ClaimLabel,
		17,
		FLinearColor::White,
		ESlateBrushDrawType::Box,
		bFreeChestClaimable ? MiniGreenMargin : MiniBlueMargin);
	ClaimFreeChestButton->SetIsEnabled(bFreeChestClaimable && !State.bClaimPending);
	PlaceOnCanvas(
		ModeCanvas,
		ClaimFreeChestButton,
		CapturePoint(915.0f, 447.0f),
		FVector2D(135.0f, 50.5f),
		9);
	if (bFreeChestClaimable)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("VipFreeChestRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(18.0f)),
			CapturePoint(1036.0f, 440.0f),
			FVector2D(18.0f),
			10);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipSpecialChestPanel"), FLinearColor(0.018f, 0.018f, 0.018f, 1.0f)),
		CapturePoint(730.0f, 545.0f),
		FVector2D(440.0f, 155.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipSpecialTitle"), VipPlayerAtlasPath, VipSpecialTitleRect, VipPlayerAtlasSize, FVector2D(454.0f, 75.0f)),
		CapturePoint(722.0f, 524.0f),
		FVector2D(454.0f, 75.0f),
		7);
	UTextBlock* SpecialText = MakeScreenText(TEXT("VipSpecialText"), 17, FLinearColor::White, false, ETextJustify::Center);
	SpecialText->SetText(NSLOCTEXT("RokVip", "SpecialPrivilege", "特别尊享"));
	PlaceOnCanvas(ModeCanvas, SpecialText, CapturePoint(722.0f, 542.0f), FVector2D(454.0f, 30.0f), 8);
	UImage* SpecialChest = MakeAtlasImage(TEXT("VipSpecialChest"), TreasureBoxAtlasPath, VipSpecialChestRect, TreasureBoxAtlasSize, FVector2D(94.0f, 112.0f));
	SpecialChest->SetRenderTransformPivot(FVector2D(0.5f));
	SpecialChest->SetRenderTransformAngle(90.0f);
	PlaceOnCanvas(ModeCanvas, SpecialChest, CapturePoint(748.0f, 591.0f), FVector2D(94.0f, 112.0f), 7);
	URokUnityIndexedButton* SpecialChestHitButton = MakeActionButton(
		TEXT("VipSpecialChestHit"),
		744,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(109.2f, 77.3f));
	SpecialChestHitButton->SetIsEnabled(bCanPreviewPurchase);
	PlaceOnCanvas(
		ModeCanvas,
		SpecialChestHitButton,
		CapturePoint(740.0f, 604.0f),
		FVector2D(109.2f, 77.3f),
		9);
	URokUnityIndexedButton* BuySpecialChestButton = MakeActionButton(
		TEXT("VipBuySpecialChest"),
		744,
		CommonAtlasPath,
		MediumGoldButtonRect,
		CommonAtlasSize,
		FVector2D(138.0f, 58.0f),
		BuyLabel,
		17,
		FLinearColor::White,
		ESlateBrushDrawType::Box,
		FMargin(0.30f, 0.0f, 0.30f, 0.0f));
	BuySpecialChestButton->SetIsEnabled(bCanPreviewPurchase);
	PlaceOnCanvas(
		ModeCanvas,
		BuySpecialChestButton,
		CapturePoint(915.0f, 619.0f),
		FVector2D(135.0f, 46.0f),
		9);

	if (PreviewLevel > 0)
	{
		URokUnityIndexedButton* Previous = MakeActionButton(TEXT("VipPreviousPage"), 700, CommonAtlasPath, VipPageArrowRect, CommonAtlasSize, FVector2D(110.0f, 276.0f));
		Previous->SetRenderTransformPivot(FVector2D(0.5f));
		Previous->SetRenderScale(FVector2D(-1.0f, 1.0f));
		PlaceOnCanvas(ModeCanvas, Previous, CapturePoint(130.0f, 386.0f), FVector2D(110.0f, 276.0f), 9);
	}
	if (PreviewLevel + 1 < VipLevelCount)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(TEXT("VipNextPage"), 701, CommonAtlasPath, VipPageArrowRect, CommonAtlasSize, FVector2D(110.0f, 276.0f)),
			CapturePoint(1203.0f, 386.0f),
			FVector2D(110.0f, 276.0f),
			9);
	}

	if (bVipTestFixtureState)
	{
		// VIP points, streaks and chest flags are server role state; the
		// fixture preview must stay explicitly labeled, not read as account data.
		UTextBlock* VipFixtureTag = MakeScreenText(TEXT("VipFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false);
		VipFixtureTag->SetText(NSLOCTEXT("RokVip", "FixtureTag", "VIP · 本地参考态（非账号状态）"));
		PlaceOnCanvas(ModeCanvas, VipFixtureTag, CapturePoint(220.0f, 745.0f), FVector2D(420.0f, 26.0f), 6);
	}

	if (bVipDailyPointHelpVisible)
	{
		BuildVipDailyPointHelpTip();
	}

	LogPresentation();
}

void URokUnityFeatureScreenWidget::BuildVipDailyPointHelpTip()
{
	if (!ModeCanvas || !bVipDailyPointHelpVisible)
	{
		return;
	}
	FText HelpText;
	const URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog || !Catalog->GetVipDailyPointHelpText(HelpText))
	{
		bVipDailyPointHelpVisible = false;
		return;
	}

	VipDailyPointHelpTipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("VipDailyPointHelpTipRoot"));
	VipDailyPointHelpTipRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, VipDailyPointHelpTipRoot, FVector2D::ZeroVector,
		CurrentExpandedDesignSize, 100);

	UClass* TipClass = ActiveModePage ? ResolveFeaturePageClass(TEXT("VipDailyPointHelpTip")) : nullptr;
	auto* TipPage = TipClass ? CreateWidget<URokUiPageWidget>(this, TipClass) : nullptr;
	UTextBlock* Body = TipPage ? TipPage->Get<UTextBlock>(TEXT("VipDailyPointHelpTipText")) : nullptr;
	const auto FindLayoutSlot = [TipPage](FName Name) -> UCanvasPanelSlot*
	{
		UWidget* Widget = TipPage ? TipPage->Get<UWidget>(Name) : nullptr;
		return Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	};
	auto* BodyLayout = Body ? Cast<UCanvasPanelSlot>(Body->Slot) : nullptr;
	auto* HitLayout = FindLayoutSlot(TEXT("VipDailyPointHelpTipBodyHit"));
	auto* FrameLayout = FindLayoutSlot(TEXT("VipDailyPointHelpTipBackground"));
	auto* ArrowLayout = FindLayoutSlot(TEXT("VipDailyPointHelpTipArrow"));
	const bool bAuthoredTip = BodyLayout && HitLayout && FrameLayout && ArrowLayout;
	if (!bAuthoredTip)
	{
		Body = MakeScreenText(TEXT("VipDailyPointHelpTipText"), 14,
			FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), false, ETextJustify::Right);
		Body->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
		Body->SetShadowOffset(FVector2D::ZeroVector);
	}

	FVector2D Anchor = CapturePoint(385.0f, 195.0f);
	if (const auto* Help = ActiveModePage ? ActiveModePage->Get<UWidget>(TEXT("VipHelp")) : nullptr)
	{
		if (const auto* HelpLayout = Cast<UCanvasPanelSlot>(Help->Slot))
		{
			Anchor = HelpLayout->GetPosition() + HelpLayout->GetSize() * 0.5;
		}
	}
	const FVector2D TipPadding = bAuthoredTip ? BodyLayout->GetPosition() : FVector2D(20.0f);
	const float MaxBodyWidth = bAuthoredTip ? BodyLayout->GetSize().X : 550.0f;
	const float MinTipHeight = bAuthoredTip ? FrameLayout->GetSize().Y : 80.0f;
	Body->SetText(HelpText);
	Body->SetWrapTextAt(0.0f);
	Body->SetAutoWrapText(false);
	// Keep the newly built Slate widget alive until it is placed on the canvas.
	const TSharedRef<SWidget> BodySlate = Body->TakeWidget();
	Body->ForceLayoutPrepass();
	const float NaturalBodyWidth = BodySlate->GetDesiredSize().X;
	if (!FMath::IsFinite(NaturalBodyWidth) || NaturalBodyWidth <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok VIP daily point tip text Slate width is invalid."));
		return;
	}
	const float BodyWidth = FMath::Min(MaxBodyWidth, NaturalBodyWidth);
	Body->SetWrapTextAt(BodyWidth);
	Body->SetAutoWrapText(true);
	Body->SetClipping(EWidgetClipping::ClipToBounds);
	Body->ForceLayoutPrepass();
	const float WrappedBodyHeight = BodySlate->GetDesiredSize().Y;
	if (!FMath::IsFinite(WrappedBodyHeight) || WrappedBodyHeight <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok VIP daily point tip wrapped Slate height is invalid."));
		return;
	}
	const float TipWidth = BodyWidth + 2 * TipPadding.X;
	const float TipHeight = FMath::Max(MinTipHeight, WrappedBodyHeight + static_cast<float>(2 * TipPadding.Y));
	const FVector2D TipSize(TipWidth, TipHeight);
	const float Left = FMath::Clamp(Anchor.X - TipSize.X * 0.5f,
		0.0f, FMath::Max(0.0f, CurrentExpandedDesignSize.X - TipSize.X));
	double AnchorGap = 12.0;
	double ViewportInset = 8.0;
	if (bAuthoredTip)
	{
		UClass* LayoutClass = ResolveFeaturePageClass(TEXT("VipDailyPointHelpTipLayout"));
		const auto* Layout = LayoutClass ? CreateWidget<URokUiPageWidget>(this, LayoutClass) : nullptr;
		const auto ReadDistance = [Layout](FName Name, double& Distance)
		{
			const UWidget* Marker = RokUiFind::Find(Layout, Name);
			const auto* Slot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
			if (Slot && !Slot->GetAutoSize() && !Slot->GetAnchors().IsStretchedVertical())
			{
				const double Height = Slot->GetSize().Y;
				if (FMath::IsFinite(Height) && Height >= 0.0)
				{
					Distance = Height;
				}
			}
		};
		ReadDistance(TEXT("VipTipAnchorGap"), AnchorGap);
		ReadDistance(TEXT("VipTipViewportInset"), ViewportInset);
	}
	// Unity arrowUp places the arrow on the upper edge, with the body below its anchor.
	const double Top = FMath::Min(CurrentExpandedDesignSize.Y - TipHeight - ViewportInset, Anchor.Y + AnchorGap);
	const FVector2D Position(Left, FMath::Max(ViewportInset, Top));

	if (bAuthoredTip)
	{
		TipPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* TipPageSlot = VipDailyPointHelpTipRoot->AddChildToCanvas(TipPage);
		TipPageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		TipPageSlot->SetOffsets(FMargin(0));
		for (UCanvasPanelSlot* Layout : {HitLayout, FrameLayout})
		{
			Layout->SetPosition(Position + Layout->GetPosition());
			Layout->SetSize(TipSize);
		}
		BodyLayout->SetPosition(Position + TipPadding);
		BodyLayout->SetSize({BodyWidth, TipHeight - 2 * TipPadding.Y});
		const FVector2D ArrowPosition = ArrowLayout->GetPosition();
		ArrowLayout->SetPosition({Anchor.X - ArrowLayout->GetSize().X * 0.5 + ArrowPosition.X,
			Position.Y + ArrowPosition.Y});
		BindPageAction(*TipPage, TEXT("VipDailyPointHelpTipBodyHit"), 745);
		return;
	}

	// Transparent hit surface keeps a click on the tip itself from dismissing it.
	PlaceOnCanvas(VipDailyPointHelpTipRoot, MakeActionButton(
		TEXT("VipDailyPointHelpTipBodyHit"), 745, nullptr, FIntRect(), FIntPoint(1, 1), TipSize),
		Position, TipSize, 1);
	PlaceOnCanvas(VipDailyPointHelpTipRoot, MakeAtlasBorder(
		TEXT("VipDailyPointHelpTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, TipSize,
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), Position, TipSize, 2);
	PlaceOnCanvas(VipDailyPointHelpTipRoot, Body, Position + FVector2D(20.0f, 20.0f),
		FVector2D(BodyWidth, TipHeight - 40.0f), 3);
	UImage* Arrow = MakeAtlasImage(TEXT("VipDailyPointHelpTipArrow"), CommonAtlasPath,
		SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15.0f, 12.0f));
	Arrow->SetRenderTransformAngle(180.0f); // UI_Pop_TextTip img_arrowSideTop.
	PlaceOnCanvas(VipDailyPointHelpTipRoot, Arrow,
		Position + FVector2D(Anchor.X - Left - 7.5f, -10.6f),
		FVector2D(15.0f, 12.0f), 4);
}
