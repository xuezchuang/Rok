UCanvasPanel* URokUnityFeatureScreenWidget::MakeCommanderHead(
	int32 Index,
	const FIntRect& HeadRect,
	bool bRecruited,
	bool bSelected,
	const FText& DisplayName,
	const FText& StateText,
	int32 Rare)
{
	UCanvasPanel* Head = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("CommanderHead_%d"), Index));
	Head->SetClipping(EWidgetClipping::ClipToBounds);
	const bool bUsesHeroHeadFrame = Rare == 4 || Rare == 5;
	UClass* HeadClass = ResolveFeaturePageClass(TEXT("CommanderHead"));
	URokUiPageWidget* HeadPage = HeadClass ? CreateWidget<URokUiPageWidget>(this, HeadClass) : nullptr;
	UImage* FrameImage = HeadPage ? HeadPage->Get<UImage>(TEXT("CommanderHeadFrame")) : nullptr;
	UImage* PortraitImage = HeadPage ? HeadPage->Get<UImage>(TEXT("CommanderHeadPortrait")) : nullptr;
	UImage* DarkImage = HeadPage ? HeadPage->Get<UImage>(TEXT("CommanderHeadDark")) : nullptr;
	UImage* SelectionImage = HeadPage ? HeadPage->Get<UImage>(TEXT("CommanderHeadSelection")) : nullptr;
	UBorder* Missing = HeadPage ? HeadPage->Get<UBorder>(TEXT("CommanderHeadMissing")) : nullptr;
	UTextBlock* StateLabel = HeadPage ? HeadPage->Get<UTextBlock>(TEXT("CommanderHeadState")) : nullptr;
	UButton* Action = HeadPage ? HeadPage->Get<UButton>(TEXT("CommanderHeadAction")) : nullptr;
	if (FrameImage && PortraitImage && DarkImage && SelectionImage && Missing && StateLabel && Action)
	{
		HeadPage->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
		HeadPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* PageSlot = Head->AddChildToCanvas(HeadPage);
		PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		PageSlot->SetOffsets(FMargin(0));
		UTexture2D* FrameAtlas = LoadUiTexture(bUsesHeroHeadFrame ? CommanderHeadAtlasPath : CommanderUiAtlasPath);
		if (FrameAtlas)
		{
			FSlateBrush Brush = FrameImage->GetBrush();
			const FSlateBrush AtlasBrush = MakeAtlasBrush(FrameAtlas,
				Rare == 5 ? CommanderLegendaryFrameRect : (Rare == 4 ? CommanderEpicFrameRect : CommanderRareFrameRect),
				bUsesHeroHeadFrame ? CommanderHeadAtlasSize : CommanderUiAtlasSize, Brush.GetImageSize());
			Brush.SetResourceObject(FrameAtlas);
			Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
			FrameImage->SetBrush(Brush);
		}
		FrameImage->SetColorAndOpacity(Rare == 3 ? FLinearColor(0.55f, 0.78f, 1.0f, 1.0f)
			: (Rare == 2 ? FLinearColor(0.62f, 1.0f, 0.70f, 1.0f) : FLinearColor::White));
		const bool bHasPortrait = HeadRect.Width() > 0 && HeadRect.Height() > 0;
		if (bHasPortrait)
		{
			if (UTexture2D* PortraitAtlas = LoadUiTexture(CommanderHeadAtlasPath))
			{
				FSlateBrush Brush = PortraitImage->GetBrush();
				const FSlateBrush AtlasBrush = MakeAtlasBrush(PortraitAtlas, HeadRect, CommanderHeadAtlasSize, Brush.GetImageSize());
				Brush.SetResourceObject(PortraitAtlas);
				Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
				PortraitImage->SetBrush(Brush);
			}
		}
		PortraitImage->SetVisibility(bHasPortrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Missing->SetVisibility(bHasPortrait ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		DarkImage->SetVisibility(bRecruited ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		SelectionImage->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		StateLabel->SetText(StateText);
		BindPageAction(*HeadPage, TEXT("CommanderHeadAction"), 400 + Index);
		Action->SetToolTipText(FText::Format(
			bRecruited ? NSLOCTEXT("RokCommander", "OwnedCommanderTooltip", "{0} · 已拥有")
				: NSLOCTEXT("RokCommander", "UnownedCommanderTooltip", "{0} · 未招募"), DisplayName));
		return Head;
	}
	UImage* HeadFrame = MakeAtlasImage(
		*FString::Printf(TEXT("CommanderHeadFrame_%d"), Index),
		bUsesHeroHeadFrame ? CommanderHeadAtlasPath : CommanderUiAtlasPath,
		Rare == 5
			? CommanderLegendaryFrameRect
			: (Rare == 4 ? CommanderEpicFrameRect : CommanderRareFrameRect),
		bUsesHeroHeadFrame ? CommanderHeadAtlasSize : CommanderUiAtlasSize,
		FVector2D(116.0f, 132.0f));
	if (Rare == 3)
	{
		HeadFrame->SetColorAndOpacity(
			FLinearColor(0.55f, 0.78f, 1.0f, 1.0f));
	}
	else if (Rare == 2)
	{
		HeadFrame->SetColorAndOpacity(
			FLinearColor(0.62f, 1.0f, 0.70f, 1.0f));
	}
	PlaceOnCanvas(
		Head,
		HeadFrame,
		FVector2D(6.0f, 0.0f),
		FVector2D(116.0f, 132.0f),
		1);
	if (HeadRect.Width() > 0 && HeadRect.Height() > 0)
	{
		PlaceOnCanvas(Head, MakeAtlasImage(*FString::Printf(TEXT("CommanderHeadPortrait_%d"), Index), CommanderHeadAtlasPath, HeadRect, CommanderHeadAtlasSize, FVector2D(108.0f, 122.0f)), FVector2D(10.0f, 0.0f), FVector2D(108.0f, 122.0f), 2);
	}
	else
	{
		PlaceOnCanvas(
			Head,
			MakeSolidBorder(
				*FString::Printf(TEXT("CommanderHeadMissing_%d"), Index),
				FLinearColor(0.07f, 0.07f, 0.075f, 0.96f)),
			FVector2D(10.0f, 0.0f),
			FVector2D(108.0f, 122.0f),
			2);
	}
	if (!bRecruited)
	{
		PlaceOnCanvas(Head, MakeAtlasImage(*FString::Printf(TEXT("CommanderHeadDark_%d"), Index),
			CommanderUiAtlasPath, SpriteRect(378, 84, 106, 121), CommanderUiAtlasSize, FVector2D(108, 122)),
			FVector2D(10, 0), FVector2D(108, 122), 3);
	}
	if (bSelected)
	{
		PlaceOnCanvas(Head, MakeAtlasImage(*FString::Printf(TEXT("CommanderHeadSelection_%d"), Index),
			CommanderUiAtlasPath, SpriteRect(1905, 356, 141, 155), CommanderUiAtlasSize, FVector2D(124, 136)),
			FVector2D(2, 0), FVector2D(124, 136), 5);
	}
	PlaceOnCanvas(
		Head,
		MakeSolidBorder(
			*FString::Printf(TEXT("CommanderHeadStateShade_%d"), Index),
			FLinearColor(0.0f, 0.0f, 0.0f, 0.72f)),
		FVector2D(10.0f, 109.0f),
		FVector2D(108.0f, 27.0f),
		6);
	UTextBlock* State = MakeScreenText(*FString::Printf(TEXT("CommanderHeadState_%d"), Index), 16, FLinearColor::White, true, ETextJustify::Center);
	State->SetText(StateText);
	PlaceOnCanvas(Head, State, FVector2D(14.0f, 112.0f), FVector2D(100.0f, 22.0f), 7);
	UButton* HeadAction = MakeActionButton(
		*FString::Printf(TEXT("CommanderHeadAction_%d"), Index),
		400 + Index,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(128.0f, 142.0f));
	HeadAction->SetToolTipText(FText::Format(
		bRecruited
			? NSLOCTEXT(
				"RokCommander",
				"OwnedCommanderTooltip",
				"{0} · 已拥有")
			: NSLOCTEXT(
				"RokCommander",
				"UnownedCommanderTooltip",
				"{0} · 未招募"),
		DisplayName));
	PlaceOnCanvas(
		Head,
		HeadAction,
		FVector2D::ZeroVector,
		FVector2D(128.0f, 142.0f),
		9);
	return Head;
}
