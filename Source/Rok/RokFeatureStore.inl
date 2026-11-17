void URokUnityFeatureScreenWidget::BuildStore()
{
	static const int32 StoreCategoryTypes[] = {1, 2, 3, 5};
	static const FText StoreCategoryLabels[] = {
		NSLOCTEXT("RokStore", "Resources", "资源"),
		NSLOCTEXT("RokStore", "Speedups", "加速"),
		NSLOCTEXT("RokStore", "Boosts", "增益"),
		NSLOCTEXT("RokStore", "Other", "其他")};
	URokUnityEconomyCatalogSubsystem* Catalog =
		GetEconomyCatalogSubsystem();
	TArray<FRokUnityEconomyItem> StoreItems;
	if (Catalog)
	{
		StoreItems = Catalog->GetShopItemsByCategory(
			StoreCategoryTypes[
				FMath::Clamp(
					SelectedStoreCategory,
					0,
					3)]);
	}
	constexpr int32 StoreItemsPerPage = 12;
	const int32 PageCount = FMath::Max(
		1,
		FMath::DivideAndRoundUp(
			StoreItems.Num(),
			StoreItemsPerPage));
	SelectedStorePage = FMath::Clamp(
		SelectedStorePage,
		0,
		PageCount - 1);
	const int32 FirstItemIndex =
		SelectedStorePage * StoreItemsPerPage;
	const int32 PageItemCount = FMath::Min(
		StoreItemsPerPage,
		StoreItems.Num() - FirstItemIndex);
	VisibleStoreItemIds.Reset();
	for (int32 Index = 0; Index < PageItemCount; ++Index)
	{
		VisibleStoreItemIds.Add(
			StoreItems[FirstItemIndex + Index].Id);
	}
	SelectedStoreItemIndex = FMath::Clamp(
		SelectedStoreItemIndex,
		0,
		FMath::Max(0, VisibleStoreItemIds.Num() - 1));

	auto ResolveQualityColor =
		[](int32 Quality)
		{
			switch (Quality)
			{
			case 2:
				return FLinearColor(0.14f, 0.34f, 0.58f, 1.0f);
			case 3:
				return FLinearColor(0.42f, 0.20f, 0.58f, 1.0f);
			case 4:
				return FLinearColor(0.72f, 0.39f, 0.12f, 1.0f);
			case 5:
				return FLinearColor(0.74f, 0.63f, 0.20f, 1.0f);
			default:
				return FLinearColor(0.28f, 0.31f, 0.29f, 1.0f);
			}
		};

	FRokUnityEconomyItem SelectedItem;
	const bool bHasSelectedItem =
		Catalog
		&& VisibleStoreItemIds.IsValidIndex(
			SelectedStoreItemIndex)
		&& Catalog->FindItemById(
			VisibleStoreItemIds[SelectedStoreItemIndex],
			SelectedItem);
	const int64 TotalPrice = bHasSelectedItem
		? static_cast<int64>(SelectedItem.ShopPrice)
			* SelectedStoreQuantity
		: 0;

	const auto ItemName = [](const FRokUnityEconomyItem& Item)
	{
		return Item.DisplayNameZhCn.IsEmpty()
			? FText::Format(NSLOCTEXT("RokStore", "FallbackItemName", "道具 {0}"), FText::AsNumber(Item.Id))
			: FText::FromString(Item.DisplayNameZhCn);
	};
	const auto ItemPrice = [](const FRokUnityEconomyItem& Item)
	{
		return FText::Format(NSLOCTEXT("RokStore", "PriceFormat", "宝石 {0}"), FText::AsNumber(Item.ShopPrice));
	};
	const FText DetailNameText = bHasSelectedItem
		? FText::FromString(SelectedItem.DisplayNameZhCn.IsEmpty()
			? FString::Printf(TEXT("道具 %d"), SelectedItem.Id) : SelectedItem.DisplayNameZhCn)
		: NSLOCTEXT("RokStore", "NoItem", "当前分类无商品");
	const FText DetailSourceText = bHasSelectedItem
		? FText::Format(NSLOCTEXT("RokStore", "SourceFormat", "Item {0}  ·  品质 {1}  ·  货币 {2}"),
			FText::AsNumber(SelectedItem.Id), FText::AsNumber(SelectedItem.Quality),
			FText::AsNumber(SelectedItem.ShopCurrencyTypeId))
		: FText::GetEmpty();
	const FText DetailDescriptionText = bHasSelectedItem
		? FText::FromString(ResolveEconomyItemDescription(SelectedItem)) : FText::GetEmpty();
	const FText TotalText = FText::Format(NSLOCTEXT("RokStore", "TotalPriceFormat", "总价：{0} 宝石"),
		FText::AsNumber(TotalPrice));
	const FText PageText = FText::Format(NSLOCTEXT("RokStore", "PageFormat", "{0}/{1}  ·  本类 {2} 项"),
		FText::AsNumber(SelectedStorePage + 1), FText::AsNumber(PageCount), FText::AsNumber(StoreItems.Num()));

	UClass* CellClass = ResolveFeaturePageClass(TEXT("StoreCell"));
	if (const URokUiPageWidget* Page = CellClass ? MountModePage() : nullptr)
	{
		for (int32 Category = 0; Category < UE_ARRAY_COUNT(StoreCategoryTypes); ++Category)
		{
			const FString Name = FString::Printf(TEXT("StoreCategory_%d"), Category);
			const bool bSelected = Category == SelectedStoreCategory;
			Page->SetShown(Page->Get<UCanvasPanel>(*(Name + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Name + TEXT("Normal"))), !bSelected);
			BindPageAction(*Page, *Name, 1700 + Category);
		}

		for (int32 Index = 0; Index < StoreItemsPerPage; ++Index)
		{
			auto* CellHost = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("StoreCell_%d"), Index));
			const bool bHasItem = VisibleStoreItemIds.IsValidIndex(Index);
			Page->SetPresent(CellHost, bHasItem);
			if (!bHasItem) continue;
			if (const URokUiPageWidget* Cell = MountPageItem(CellHost, CellClass, TEXT("Store"), Index))
			{
				const FRokUnityEconomyItem& Item = StoreItems[FirstItemIndex + Index];
				if (auto* Quality = Cell->Get<UBorder>(TEXT("CellQuality")))
				{
					Quality->SetBrushColor(ResolveQualityColor(Item.Quality));
				}
				Cell->SetPresent(Cell->Get<UWidget>(TEXT("CellSelected")), Index == SelectedStoreItemIndex);
				SetFeaturePageText(*Cell, TEXT("CellName"), ItemName(Item));
				SetFeaturePageText(*Cell, TEXT("CellPrice"), ItemPrice(Item));
				BindPageAction(*Cell, TEXT("CellHit"), 1720 + Index);
			}
		}

		SetFeaturePageText(*Page, TEXT("StoreDetailName"), DetailNameText);
		SetFeaturePageText(*Page, TEXT("StoreDetailSource"), DetailSourceText);
		SetFeaturePageText(*Page, TEXT("StoreDetailDescription"), DetailDescriptionText);
		SetFeaturePageText(*Page, TEXT("StoreQuantity"), FText::AsNumber(SelectedStoreQuantity));
		SetFeaturePageText(*Page, TEXT("StoreTotalPrice"), TotalText);
		SetFeaturePageText(*Page, TEXT("StorePage"), PageText);
		BindPageAction(*Page, TEXT("StorePreviousPage"), 1740);
		BindPageAction(*Page, TEXT("StoreNextPage"), 1741);
		BindPageAction(*Page, TEXT("StoreQuantityMinus"), 1750);
		BindPageAction(*Page, TEXT("StoreQuantityPlus"), 1751);
		BindPageAction(*Page, TEXT("StoreQuantityMax"), 1752);
		BindPageAction(*Page, TEXT("StoreBuyButton"), 1753);
		return;
	}

	BuildSplitWindowFrame(
		CapturePoint(170.0f, 73.0f),
		FVector2D(1100.0f, 754.0f),
		NSLOCTEXT("RokStore", "Title", "商店"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(184.0f, 153.0f),
		FVector2D(1072.0f, 664.0f),
		2);

	for (int32 Category = 0; Category < 4; ++Category)
	{
		const bool bSelected =
			Category == SelectedStoreCategory;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(
					TEXT("StoreCategory_%d"),
					Category),
				1700 + Category,
				CommonAtlasPath,
				bSelected
					? PageSelectedRect
					: PageNormalRect,
				CommonAtlasSize,
				FVector2D(260.0f, 48.0f),
				StoreCategoryLabels[Category],
				23,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.72f, 0.72f, 0.70f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(
				195.0f + Category * 260.0f,
				160.0f),
			FVector2D(260.0f, 48.0f),
			6);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreGridShade"),
			FLinearColor(0.10f, 0.11f, 0.10f, 0.98f)),
		CapturePoint(200.0f, 222.0f),
		FVector2D(670.0f, 505.0f),
		3);
	for (int32 LocalIndex = 0;
		LocalIndex < VisibleStoreItemIds.Num();
		++LocalIndex)
	{
		const FRokUnityEconomyItem& Item =
			StoreItems[FirstItemIndex + LocalIndex];
		const int32 Column = LocalIndex % 4;
		const int32 Row = LocalIndex / 4;
		const FVector2D Position(
			213.0f + Column * 162.0f,
			236.0f + Row * 155.0f);
		const bool bSelected =
			LocalIndex == SelectedStoreItemIndex;
		UCanvasPanel* Cell =
			WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(
					TEXT("StoreCell_%d"),
					LocalIndex));
		PlaceOnCanvas(
			Cell,
			MakeSolidBorder(
				*FString::Printf(
					TEXT("StoreCellQuality_%d"),
					LocalIndex),
				ResolveQualityColor(Item.Quality)),
			FVector2D::ZeroVector,
			FVector2D(148.0f, 142.0f),
			0);
		if (bSelected)
		{
			PlaceOnCanvas(
				Cell,
				MakeAtlasBorder(
					*FString::Printf(
						TEXT("StoreCellSelected_%d"),
						LocalIndex),
					CommonAtlasPath,
					TaskRowRect,
					CommonAtlasSize,
					FVector2D(148.0f, 142.0f),
					TaskRowMargin),
				FVector2D::ZeroVector,
				FVector2D(148.0f, 142.0f),
				2);
		}
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(
				TEXT("StoreCellName_%d"),
				LocalIndex),
			17,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		Name->SetText(ItemName(Item));
		Name->SetAutoWrapText(true);
		PlaceOnCanvas(
			Cell,
			Name,
			FVector2D(8.0f, 12.0f),
			FVector2D(132.0f, 72.0f),
			3);
		UTextBlock* Price = MakeScreenText(
			*FString::Printf(
				TEXT("StoreCellPrice_%d"),
				LocalIndex),
			17,
			FLinearColor(0.98f, 0.91f, 0.57f, 1.0f),
			true,
			ETextJustify::Center);
		Price->SetText(ItemPrice(Item));
		PlaceOnCanvas(
			Cell,
			Price,
			FVector2D(8.0f, 102.0f),
			FVector2D(132.0f, 30.0f),
			3);
		PlaceOnCanvas(
			Cell,
			MakeActionButton(
				*FString::Printf(
					TEXT("StoreCellHit_%d"),
					LocalIndex),
				1720 + LocalIndex,
				nullptr,
				FIntRect(),
				FIntPoint::ZeroValue,
				FVector2D(148.0f, 142.0f)),
			FVector2D::ZeroVector,
			FVector2D(148.0f, 142.0f),
			8);
		PlaceOnCanvas(
			ModeCanvas,
			Cell,
			CapturePoint(Position.X, Position.Y),
			FVector2D(148.0f, 142.0f),
			5);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreDetailShade"),
			FLinearColor(0.18f, 0.19f, 0.18f, 0.98f)),
		CapturePoint(890.0f, 222.0f),
		FVector2D(345.0f, 505.0f),
		4);
	UTextBlock* DetailName = MakeScreenText(
		TEXT("StoreDetailName"),
		24,
		FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
		true,
		ETextJustify::Center);
	DetailName->SetText(DetailNameText);
	DetailName->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		DetailName,
		CapturePoint(910.0f, 247.0f),
		FVector2D(305.0f, 68.0f),
		6);
	UTextBlock* DetailSource = MakeScreenText(
		TEXT("StoreDetailSource"),
		16,
		FLinearColor(0.74f, 0.74f, 0.71f, 1.0f),
		false,
		ETextJustify::Center);
	DetailSource->SetText(DetailSourceText);
	PlaceOnCanvas(
		ModeCanvas,
		DetailSource,
		CapturePoint(905.0f, 322.0f),
		FVector2D(315.0f, 34.0f),
		6);
	UTextBlock* Description = MakeScreenText(
		TEXT("StoreDetailDescription"),
		17,
		FLinearColor::White,
		false,
		ETextJustify::Center);
	Description->SetText(DetailDescriptionText);
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		Description,
		CapturePoint(915.0f, 365.0f),
		FVector2D(295.0f, 95.0f),
		6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityMinus"),
			1750,
			CommonAtlasPath,
			ItemMinusRect,
			CommonAtlasSize,
			FVector2D(46.0f),
			FText::FromString(TEXT("−")),
			23),
		CapturePoint(912.0f, 480.0f),
		FVector2D(46.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreQuantityShade"),
			FLinearColor(0.11f, 0.12f, 0.11f, 0.98f)),
		CapturePoint(966.0f, 482.0f),
		FVector2D(135.0f, 43.0f),
		6);
	UTextBlock* Quantity = MakeScreenText(
		TEXT("StoreQuantity"),
		22,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Quantity->SetText(FText::AsNumber(SelectedStoreQuantity));
	PlaceOnCanvas(
		ModeCanvas,
		Quantity,
		CapturePoint(966.0f, 487.0f),
		FVector2D(135.0f, 34.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityPlus"),
			1751,
			CommonAtlasPath,
			ItemPlusRect,
			CommonAtlasSize,
			FVector2D(46.0f),
			FText::FromString(TEXT("+")),
			23),
		CapturePoint(1109.0f, 480.0f),
		FVector2D(46.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityMax"),
			1752,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(67.0f, 46.0f),
			NSLOCTEXT("RokStore", "Max", "最大"),
			17),
		CapturePoint(1162.0f, 480.0f),
		FVector2D(67.0f, 46.0f),
		7);

	UTextBlock* Total = MakeScreenText(
		TEXT("StoreTotalPrice"),
		22,
		FLinearColor(0.98f, 0.91f, 0.57f, 1.0f),
		true,
		ETextJustify::Center);
	Total->SetText(TotalText);
	PlaceOnCanvas(
		ModeCanvas,
		Total,
		CapturePoint(910.0f, 550.0f),
		FVector2D(305.0f, 38.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreBuyButton"),
			1753,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(220.0f, 75.0f),
			NSLOCTEXT("RokStore", "Buy", "购买"),
			26,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(952.0f, 618.0f),
		FVector2D(220.0f, 75.0f),
		8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StorePreviousPage"),
			1740,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(110.0f, 46.0f),
			NSLOCTEXT("RokStore", "Previous", "上一页"),
			17),
		CapturePoint(395.0f, 744.0f),
		FVector2D(110.0f, 46.0f),
		7);
	UTextBlock* Page = MakeScreenText(
		TEXT("StorePage"),
		18,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Page->SetText(PageText);
	PlaceOnCanvas(
		ModeCanvas,
		Page,
		CapturePoint(510.0f, 752.0f),
		FVector2D(240.0f, 32.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreNextPage"),
			1741,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(110.0f, 46.0f),
			NSLOCTEXT("RokStore", "Next", "下一页"),
			17),
		CapturePoint(755.0f, 744.0f),
		FVector2D(110.0f, 46.0f),
		7);
}
