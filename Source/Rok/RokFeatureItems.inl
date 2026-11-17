void URokUnityFeatureScreenWidget::BuildItems()
{
	URokUnityEconomyCatalogSubsystem* Catalog =
		GetEconomyCatalogSubsystem();
	if (Catalog && !Catalog->IsCatalogLoaded())
	{
		Catalog->LoadCatalog();
	}
	const UGameInstance* Instance = GetGameInstance();
	const URokServerInventorySubsystem* Server = Instance
		? Instance->GetSubsystem<URokServerInventorySubsystem>() : nullptr;
	const FRokServerInventorySnapshot ServerSnapshot = Server
		? Server->GetSnapshot() : FRokServerInventorySnapshot();
	bLastServerInventoryAvailable = ServerSnapshot.bAvailable;
	bLastServerInventoryKnown = ServerSnapshot.bItemsKnown;
	LastServerInventorySignature = ServerSnapshot.Signature;

	struct FItemsPreviewStack
	{
		int64 ItemIndex = 0;
		int32 ItemId = 0;
		int64 Overlay = 0;
		bool bCatalogKnown = false;
		FRokUnityEconomyItem Item;
	};
	struct FItemsPreviewStackSeed
	{
		int64 ItemIndex;
		int32 ItemId;
		int64 Overlay;
	};
	static const FItemsPreviewStackSeed PreviewStacks[] = {
		{900001, 101050010, 3},
		{900002, 101050020, 12},
		{900003, 101040010, 5},
		{900004, 101040020, 8},
		{900005, 101030010, 4},
		{900006, 101020010, 2},
		{900007, 101050001, 10000},
		// Unity BagProxy.Items ignores stacks whose overlay is not positive.
		{900008, 101040001, 0},
	};

	TArray<FItemsPreviewStack> VisibleStacks;
	if (ServerSnapshot.bAvailable && ServerSnapshot.bItemsKnown)
	{
		for (const FRokServerInventoryStack& ServerStack : ServerSnapshot.Stacks)
		{
			const int32 BagCategory = ServerStack.bCatalogKnown
				? ServerStack.BagType : 5; // Unresolved config stays visible under Other.
			if (BagCategory != SelectedItemsCategory + 1
				|| ServerStack.ItemId <= 0 || ServerStack.ItemId > MAX_int32)
				continue;
			FItemsPreviewStack& Stack = VisibleStacks.AddDefaulted_GetRef();
			Stack.ItemIndex = ServerStack.ItemIndex;
			Stack.ItemId = static_cast<int32>(ServerStack.ItemId);
			Stack.Overlay = ServerStack.Overlay;
			Stack.bCatalogKnown = ServerStack.bCatalogKnown && Catalog
				&& Catalog->FindItemById(Stack.ItemId, Stack.Item);
			if (!Stack.bCatalogKnown)
			{
				Stack.Item.Id = Stack.ItemId;
				Stack.Item.DisplayNameZhCn = FString::Printf(TEXT("道具 %d（配置待同步）"), Stack.ItemId);
			}
		}
		VisibleStacks.Sort([](const FItemsPreviewStack& Left, const FItemsPreviewStack& Right)
		{
			return Left.Item.Rank == Right.Item.Rank
				? (Left.ItemId == Right.ItemId ? Left.ItemIndex < Right.ItemIndex : Left.ItemId < Right.ItemId)
				: Left.Item.Rank < Right.Item.Rank;
		});
	}
	else if (!ServerSnapshot.bAvailable && bItemsTestFixtureState && Catalog)
	{
		const int32 SelectedItemType = SelectedItemsCategory + 1;
		for (const FItemsPreviewStackSeed& Seed : PreviewStacks)
		{
			FRokUnityEconomyItem Item;
			if (Seed.Overlay <= 0
				|| !Catalog->FindItemById(Seed.ItemId, Item)
				|| Item.Type != SelectedItemType)
			{
				continue;
			}

			FItemsPreviewStack& Stack = VisibleStacks.AddDefaulted_GetRef();
			Stack.ItemIndex = Seed.ItemIndex;
			Stack.ItemId = Seed.ItemId;
			Stack.Overlay = Seed.Overlay;
			Stack.bCatalogKnown = true;
			Stack.Item = MoveTemp(Item);
		}
		VisibleStacks.Sort(
			[](const FItemsPreviewStack& Left,
				const FItemsPreviewStack& Right)
			{
				return Left.Item.Rank == Right.Item.Rank
					? Left.ItemId < Right.ItemId
					: Left.Item.Rank < Right.Item.Rank;
			});
	}

	VisibleItemsStacks.Reset();
	for (const FItemsPreviewStack& Stack : VisibleStacks)
	{
		FItemsInventoryStackState& State =
			VisibleItemsStacks.AddDefaulted_GetRef();
		State.ItemIndex = Stack.ItemIndex;
		State.ItemId = Stack.ItemId;
		State.Overlay = Stack.Overlay;
	}
	const int32 PreservedIndex = VisibleItemsStacks.IndexOfByPredicate([this](const FItemsInventoryStackState& Stack)
	{
		return Stack.ItemIndex == SelectedItemsStackIndex;
	});
	SelectedItemsIndex = PreservedIndex != INDEX_NONE ? PreservedIndex : FMath::Clamp(
		SelectedItemsIndex,
		0,
		FMath::Max(0, VisibleItemsStacks.Num() - 1));
	SelectedItemsStackIndex = VisibleItemsStacks.IsValidIndex(SelectedItemsIndex)
		? VisibleItemsStacks[SelectedItemsIndex].ItemIndex : 0;
	SelectedItemsUseCount = FMath::Clamp(
		SelectedItemsUseCount,
		1,
		VisibleItemsStacks.IsValidIndex(SelectedItemsIndex)
			? static_cast<int32>(FMath::Clamp<int64>(
				VisibleItemsStacks[SelectedItemsIndex].Overlay,
				1,
				9999))
			: 1);
	const bool bAnyUnresolvedServerItem = ServerSnapshot.bAvailable
		&& ServerSnapshot.Stacks.ContainsByPredicate([](const FRokServerInventoryStack& Stack)
			{ return !Stack.bCatalogKnown; });
	const bool bPartialInventory = ServerSnapshot.bAvailable
		&& (bAnyUnresolvedServerItem || !ServerSnapshot.bLoaded
			|| (ServerSnapshot.bItemsKnown && !ServerSnapshot.bRowsComplete));

	const bool bHasStacks = !VisibleStacks.IsEmpty();
	const FText EmptyStateText = ServerSnapshot.bAvailable && !ServerSnapshot.bItemsKnown
		? NSLOCTEXT("RokItems", "InventoryWaiting", "道具数据待服务器同步")
		: bPartialInventory
		? NSLOCTEXT("RokItems", "InventoryPartialEmpty", "此分类暂无可显示道具，部分道具配置待同步")
		: NSLOCTEXT("RokItems", "InventoryEmpty", "您的道具库里真是空空如也，快获取一些道具来充实它吧！");
	constexpr int32 CellsPerRow = 4;
	const auto LogInventory = [&]()
	{
		if (bHasStacks)
		{
			const FItemsPreviewStack& Stack = VisibleStacks[SelectedItemsIndex];
			UE_LOG(LogTemp, Log, TEXT("Rok items screen: inventory=%s category=%d stacks=%d selected_item=%d overlay=%lld."),
				ServerSnapshot.bAvailable ? TEXT("server") : (bItemsTestFixtureState ? TEXT("fixture") : TEXT("offline_empty")),
				SelectedItemsCategory, VisibleStacks.Num(), Stack.ItemId, static_cast<long long>(Stack.Overlay));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Rok items screen: inventory=empty fixture=%d category=%d rows=0."),
				bItemsTestFixtureState ? 1 : 0, SelectedItemsCategory);
		}
	};

	UClass* RowClass = ResolveFeaturePageClass(TEXT("ItemsInventoryRow"));
	UClass* CellClass = ResolveFeaturePageClass(TEXT("ItemsCell"));
	if (const URokUiPageWidget* Page = RowClass && CellClass ? MountModePage() : nullptr)
	{
		for (int32 Category = 0; Category < 5; ++Category)
		{
			const FString Name = FString::Printf(TEXT("ItemsCategory_%d"), Category);
			const bool bSelected = Category == SelectedItemsCategory;
			Page->SetShown(Page->Get<UCanvasPanel>(*(Name + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Name + TEXT("Normal"))), !bSelected);
			BindPageAction(*Page, *Name, 520 + Category);
		}

		auto* EmptyState = Page->Get<UTextBlock>(TEXT("ItemsInventoryEmptyState"));
		Page->SetPresent(EmptyState, !bHasStacks);
		if (!bHasStacks && EmptyState)
		{
			EmptyState->SetText(EmptyStateText);
		}
		Page->SetPresent(Page->Get<UWidget>(TEXT("ItemsInventorySyncState")), bPartialInventory);
		auto* Scroll = Page->Get<UScrollBox>(TEXT("ItemsInventoryScroll"));
		const auto* PartialBounds = Page->Get<UCanvasPanel>(TEXT("ItemsInventoryPartialBounds"));
		const auto* PartialBoundsSlot = PartialBounds ? Cast<UCanvasPanelSlot>(PartialBounds->Slot) : nullptr;
		Page->SetPresent(Page->Get<UWidget>(TEXT("ItemsListShade")), bHasStacks);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("ItemsDetailPanel")), bHasStacks);
		Page->SetPresent(Scroll, bHasStacks);
		if (!bHasStacks || !Scroll)
		{
			LogInventory();
			return;
		}
		if (bPartialInventory && PartialBoundsSlot)
		{
			if (auto* ScrollSlot = Cast<UCanvasPanelSlot>(Scroll->Slot))
			{
				ScrollSlot->SetLayout(PartialBoundsSlot->GetLayout());
			}
		}

		const auto SetAtlas = [this](UImage* Image, const TCHAR* Path, const FIntRect& Rect, const FIntPoint& AtlasSize)
		{
			if (!Image) return;
			FSlateBrush Brush = Image->GetBrush();
			const FSlateBrush Atlas = MakeAtlasBrush(LoadUiTexture(Path), Rect, AtlasSize, Brush.GetImageSize());
			Brush.SetResourceObject(Atlas.GetResourceObject());
			Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
			Image->SetBrush(Brush);
		};
		const auto SetItemArt = [&SetAtlas](const URokUiPageWidget& Layout, const FString& Prefix, const FRokUnityEconomyItem& Item)
		{
			SetAtlas(Layout.Get<UImage>(*(Prefix + TEXT("Quality"))), Item0AtlasPath,
				ResolveBagQualityRect(Item.Quality), Item0AtlasSize);
			const TCHAR* IconPath = nullptr;
			FIntRect IconRect;
			FIntPoint IconSize;
			const bool bHasIcon = ResolveEconomyItemIcon(Item, IconPath, IconRect, IconSize);
			auto* Icon = Layout.Get<UImage>(*(Prefix + TEXT("Icon")));
			Layout.SetPresent(Icon, bHasIcon);
			if (bHasIcon)
			{
				SetAtlas(Icon, IconPath, IconRect, IconSize);
			}
			const FText TopLabel = ResolveEconomyItemTopLabel(Item);
			auto* TopText = Layout.Get<UTextBlock>(*(Prefix + TEXT("TopText")));
			Layout.SetPresent(Layout.Get<UWidget>(*(Prefix + TEXT("Top"))), !TopLabel.IsEmpty());
			Layout.SetPresent(TopText, !TopLabel.IsEmpty());
			if (!TopLabel.IsEmpty() && TopText) TopText->SetText(TopLabel);
		};

		const int32 RowCount = FMath::DivideAndRoundUp(VisibleStacks.Num(), CellsPerRow);
		for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
		{
			auto* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
			if (!Row) continue;
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			auto* RowLayout = Row->Get<UCanvasPanel>(TEXT("InventoryRow"));
			const auto* RowLayoutSlot = RowLayout ? Cast<UCanvasPanelSlot>(RowLayout->Slot) : nullptr;
			if (!RowLayoutSlot) continue;
			Row->LegacyNames.Add(TEXT("InventoryRow"), FString::Printf(TEXT("ItemsInventoryRow_%d"), RowIndex));
			Scroll->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("ItemsInventoryRowSize_%d"), RowIndex),
				RowLayoutSlot->GetSize()));

			for (int32 Column = 0; Column < CellsPerRow; ++Column)
			{
				const int32 Index = RowIndex * CellsPerRow + Column;
				const FName SlotName(*FString::Printf(TEXT("CellSlot_%d"), Column));
				auto* CellHost = Row->Get<UCanvasPanel>(SlotName);
				const bool bHasCell = VisibleStacks.IsValidIndex(Index);
				Row->SetPresent(CellHost, bHasCell);
				if (!bHasCell) continue;
				Row->LegacyNames.Add(SlotName, FString::Printf(TEXT("ItemsCell_%d"), Index));
				if (const auto* Cell = MountPageItem(CellHost, CellClass, TEXT("Items"), Index))
				{
					const auto& Stack = VisibleStacks[Index];
					Cell->SetPresent(Cell->Get<UWidget>(TEXT("CellSelect")), Index == SelectedItemsIndex);
					SetItemArt(*Cell, TEXT("Cell"), Stack.Item);
					SetFeaturePageText(*Cell, TEXT("CellQuantity"), FText::AsNumber(Stack.Overlay));
					BindPageAction(*Cell, TEXT("CellHit"), 5000 + Index);
				}
			}
		}

		const auto& Selected = VisibleStacks[SelectedItemsIndex];
		const auto& Item = Selected.Item;
		SetItemArt(*Page, TEXT("ItemsDetail"), Item);
		SetFeaturePageText(*Page, TEXT("ItemsDetailName"), FText::FromString(Item.DisplayNameZhCn.IsEmpty()
			? FString::Printf(TEXT("道具 %d"), Item.Id) : Item.DisplayNameZhCn));
		SetFeaturePageText(*Page, TEXT("ItemsDescription"), FText::FromString(ResolveEconomyItemDescription(Item)));
		SetFeaturePageText(*Page, TEXT("ItemsOwned"), FText::Format(NSLOCTEXT("RokItems", "OwnedFormat", "拥有数量:{0}"),
			FText::AsNumber(Selected.Overlay)));

		const bool bCanIncrease = SelectedItemsUseCount < FMath::Min<int64>(Selected.Overlay, 9999);
		for (const auto& Control : { TPair<FName, int32>(TEXT("ItemsMinus"), 540),
			TPair<FName, int32>(TEXT("ItemsPlus"), 541), TPair<FName, int32>(TEXT("ItemsMax"), 542) })
		{
			auto* Button = Page->Get<URokUnityIndexedButton>(Control.Key);
			if (Item.bBatchUse && Button)
			{
				Button->SetIsEnabled(Control.Value == 540 ? SelectedItemsUseCount > 1 : bCanIncrease);
				BindPageAction(*Page, Control.Key, Control.Value);
			}
			Page->SetPresent(Button, Item.bBatchUse);
		}
		auto* Quantity = Page->Get<UTextBlock>(TEXT("ItemsQuantity"));
		Page->SetPresent(Quantity, Item.bBatchUse);
		Page->SetPresent(Page->Get<UWidget>(TEXT("ItemsQuantityBack")), Item.bBatchUse);
		if (Item.bBatchUse && Quantity) Quantity->SetText(FText::AsNumber(SelectedItemsUseCount));

		const bool bCanUseDirectly = Item.ButtonLocaleId > 0 && Selected.bCatalogKnown;
		Page->SetPresent(Page->Get<UButton>(TEXT("ItemsUse")), bCanUseDirectly);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ItemsRoutedUse")), !bCanUseDirectly);
		if (bCanUseDirectly) BindPageAction(*Page, TEXT("ItemsUse"), 543);

		LogInventory();
		return;
	}

	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokItems", "Title", "道具"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ItemsContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 138.1f),
		FVector2D(1088.0f, 683.8f),
		2);

	static const FText CategoryLabels[] = {
		NSLOCTEXT("RokItems", "Resources", "资源"),
		NSLOCTEXT("RokItems", "Speedups", "加速"),
		NSLOCTEXT("RokItems", "Boosts", "增益"),
		NSLOCTEXT("RokItems", "Equipment", "装备"),
		NSLOCTEXT("RokItems", "Other", "其他"),
	};
	for (int32 Category = 0; Category < UE_ARRAY_COUNT(CategoryLabels); ++Category)
	{
		const bool bSelected = Category == SelectedItemsCategory;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("ItemsCategory_%d"), Category),
				520 + Category,
				CommonAtlasPath,
				bSelected ? PageSelectedRect : PageNormalRect,
				CommonAtlasSize,
				FVector2D(214.0f, 45.0f),
				CategoryLabels[Category],
				24,
				bSelected ? FLinearColor(0.96f, 0.97f, 0.95f, 1.0f) : FLinearColor(0.72f, 0.72f, 0.70f, 1.0f)),
			CapturePoint(184.0f + Category * 214.4f, 154.0f),
			FVector2D(214.0f, 45.0f),
			6);
	}

	if (bPartialInventory)
	{
		UTextBlock* SyncState = MakeScreenText(TEXT("ItemsInventorySyncState"), 16,
			FLinearColor(0.96f, 0.78f, 0.43f, 1.0f), false, ETextJustify::Center);
		SyncState->SetText(NSLOCTEXT("RokItems", "InventoryPartial", "部分道具配置待同步"));
		PlaceOnCanvas(ModeCanvas, SyncState, CapturePoint(593.0f, 210.0f),
			FVector2D(630.0f, 32.0f), 12);
	}

	if (VisibleStacks.IsEmpty())
	{
		UTextBlock* EmptyState = MakeScreenText(
			TEXT("ItemsInventoryEmptyState"),
			26,
			FLinearColor(0.8509804f, 0.8313726f, 0.7294118f, 1.0f),
			false,
			ETextJustify::Center);
		EmptyState->SetText(EmptyStateText);
		EmptyState->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			EmptyState,
			CapturePoint(198.9f, 394.0f),
			FVector2D(1042.3f, 220.7f),
			6);
		LogInventory();
		return;
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ItemsListShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.96f)),
		CapturePoint(584.0f, 194.8f),
		FVector2D(660.0f, 619.0f),
		3);
	UScrollBox* InventoryScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("ItemsInventoryScroll"));
	InventoryScroll->SetOrientation(Orient_Vertical);
	InventoryScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	InventoryScroll->SetAllowOverscroll(true);
	InventoryScroll->SetAnimateWheelScrolling(true);
	InventoryScroll->SetWheelScrollMultiplier(12.0f);
	PlaceOnCanvas(
		ModeCanvas,
		InventoryScroll,
		CapturePoint(584.0f, bPartialInventory ? 242.5f : 210.5f),
		FVector2D(660.0f, bPartialInventory ? 555.7f : 587.7f),
		5);

	const int32 RowCount = FMath::DivideAndRoundUp(VisibleStacks.Num(), CellsPerRow);
	for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ItemsInventoryRow_%d"), RowIndex));
		for (int32 ColumnIndex = 0; ColumnIndex < CellsPerRow; ++ColumnIndex)
		{
			const int32 LocalIndex = RowIndex * CellsPerRow + ColumnIndex;
			if (!VisibleStacks.IsValidIndex(LocalIndex))
			{
				break;
			}
			const FItemsPreviewStack& Stack = VisibleStacks[LocalIndex];
			const FRokUnityEconomyItem& Item = Stack.Item;
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("ItemsCell_%d"), LocalIndex));
			if (LocalIndex == SelectedItemsIndex)
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellSelect_%d"), LocalIndex),
						Item0AtlasPath,
						ItemSelectedRect,
						Item0AtlasSize,
						FVector2D(142.0f)),
					FVector2D::ZeroVector,
					FVector2D(142.0f),
					0);
			}
			PlaceOnCanvas(
				Cell,
				MakeAtlasImage(
					*FString::Printf(TEXT("ItemsCellQuality_%d"), LocalIndex),
					Item0AtlasPath,
					ResolveBagQualityRect(Item.Quality),
					Item0AtlasSize,
					FVector2D(130.0f)),
				FVector2D(6.0f),
				FVector2D(130.0f),
				1);

			const TCHAR* IconAtlasPath = nullptr;
			FIntRect IconRect;
			FIntPoint IconAtlasSize;
			if (ResolveEconomyItemIcon(Item, IconAtlasPath, IconRect, IconAtlasSize))
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellIcon_%d"), LocalIndex),
						IconAtlasPath,
						IconRect,
						IconAtlasSize,
						FVector2D(110.0f)),
					FVector2D(16.0f, 20.0f),
					FVector2D(110.0f),
					2);
			}

			const FText TopLabel = ResolveEconomyItemTopLabel(Item);
			if (!TopLabel.IsEmpty())
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellTop_%d"), LocalIndex),
						CommonAtlasPath,
						ItemTopRect,
						CommonAtlasSize,
						FVector2D(120.0f, 27.0f)),
					FVector2D(11.0f, 7.0f),
					FVector2D(120.0f, 27.0f),
					3);
				UTextBlock* TopText = MakeScreenText(
					*FString::Printf(TEXT("ItemsCellTopText_%d"), LocalIndex),
					15,
					FLinearColor(0.93f, 0.91f, 0.81f, 1.0f),
					false,
					ETextJustify::Center);
				TopText->SetText(TopLabel);
				PlaceOnCanvas(
					Cell,
					TopText,
					FVector2D(11.0f, 8.0f),
					FVector2D(120.0f, 25.0f),
					4);
			}

			UTextBlock* Quantity = MakeScreenText(
				*FString::Printf(TEXT("ItemsCellQuantity_%d"), LocalIndex),
				16,
				FLinearColor::White,
				true,
				ETextJustify::Right);
			Quantity->SetText(FText::AsNumber(Stack.Overlay));
			PlaceOnCanvas(
				Cell,
				Quantity,
				FVector2D(77.0f, 105.0f),
				FVector2D(51.0f, 26.0f),
				4);
			PlaceOnCanvas(
				Cell,
				MakeActionButton(
					*FString::Printf(TEXT("ItemsCellHit_%d"), LocalIndex),
					5000 + LocalIndex,
					nullptr,
					FIntRect(),
					FIntPoint::ZeroValue,
					FVector2D(142.0f)),
				FVector2D::ZeroVector,
				FVector2D(142.0f),
				8);
			PlaceOnCanvas(
				Row,
				Cell,
				FVector2D(8.0f + ColumnIndex * 160.0f, 4.0f),
				FVector2D(142.0f),
				1);
		}
		InventoryScroll->AddChild(WrapFixedSize(
			Row,
			*FString::Printf(TEXT("ItemsInventoryRowSize_%d"), RowIndex),
			FVector2D(660.0f, 150.0f)));
	}

	const FItemsPreviewStack& SelectedStack = VisibleStacks[SelectedItemsIndex];
	const FRokUnityEconomyItem& SelectedItem = SelectedStack.Item;
	UCanvasPanel* DetailPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ItemsDetailPanel"));
	PlaceOnCanvas(
		DetailPanel,
		MakeSolidBorder(
			TEXT("ItemsDetailFill"),
			FLinearColor(0.72f, 0.70f, 0.58f, 1.0f)),
		FVector2D::ZeroVector,
		FVector2D(412.0f, 616.0f),
		0);
	PlaceOnCanvas(
		DetailPanel,
		MakeAtlasBorder(
			TEXT("ItemsDetailFrame"),
			FrameAtlasPath,
			ActivityFrameRect,
			FrameAtlasSize,
			FVector2D(412.0f, 616.0f),
			ActivityFrameMargin),
		FVector2D::ZeroVector,
		FVector2D(412.0f, 616.0f),
		1);
	PlaceOnCanvas(
		DetailPanel,
		MakeAtlasImage(
			TEXT("ItemsDetailQuality"),
			Item0AtlasPath,
			ResolveBagQualityRect(SelectedItem.Quality),
			Item0AtlasSize,
			FVector2D(142.0f)),
		FVector2D(135.0f, 28.0f),
		FVector2D(142.0f),
		2);
	const TCHAR* DetailIconAtlasPath = nullptr;
	FIntRect DetailIconRect;
	FIntPoint DetailIconAtlasSize;
	if (ResolveEconomyItemIcon(
		SelectedItem,
		DetailIconAtlasPath,
		DetailIconRect,
		DetailIconAtlasSize))
	{
		PlaceOnCanvas(
			DetailPanel,
			MakeAtlasImage(
				TEXT("ItemsDetailIcon"),
				DetailIconAtlasPath,
				DetailIconRect,
				DetailIconAtlasSize,
				FVector2D(122.0f)),
			FVector2D(145.0f, 38.0f),
			FVector2D(122.0f),
			3);
	}
	const FText DetailTopLabel = ResolveEconomyItemTopLabel(SelectedItem);
	if (!DetailTopLabel.IsEmpty())
	{
		PlaceOnCanvas(DetailPanel, MakeAtlasImage(TEXT("ItemsDetailTop"), CommonAtlasPath, ItemTopRect, CommonAtlasSize, FVector2D(120.0f, 27.0f)), FVector2D(146.0f, 31.0f), FVector2D(120.0f, 27.0f), 4);
		UTextBlock* DetailTop = MakeScreenText(TEXT("ItemsDetailTopText"), 16, FLinearColor(0.93f, 0.91f, 0.81f, 1.0f), false, ETextJustify::Center);
		DetailTop->SetText(DetailTopLabel);
		PlaceOnCanvas(DetailPanel, DetailTop, FVector2D(146.0f, 32.0f), FVector2D(120.0f, 25.0f), 5);
	}

	UTextBlock* ItemName = MakeScreenText(TEXT("ItemsDetailName"), 25, FLinearColor(0.22f, 0.19f, 0.12f, 1.0f), true, ETextJustify::Center);
	ItemName->SetText(FText::FromString(
		SelectedItem.DisplayNameZhCn.IsEmpty()
			? FString::Printf(TEXT("道具 %d"), SelectedItem.Id)
			: SelectedItem.DisplayNameZhCn));
	PlaceOnCanvas(DetailPanel, ItemName, FVector2D(31.0f, 185.0f), FVector2D(350.0f, 42.0f), 4);
	PlaceOnCanvas(DetailPanel, MakeSolidBorder(TEXT("ItemsDescriptionShade"), FLinearColor(0.31f, 0.30f, 0.25f, 0.52f)), FVector2D(28.0f, 235.0f), FVector2D(356.0f, 144.0f), 2);
	UTextBlock* Description = MakeScreenText(TEXT("ItemsDescription"), 19, FLinearColor::White, false, ETextJustify::Center);
	Description->SetText(FText::FromString(ResolveEconomyItemDescription(SelectedItem)));
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(DetailPanel, Description, FVector2D(45.0f, 255.0f), FVector2D(322.0f, 104.0f), 4);

	UTextBlock* Owned = MakeScreenText(
		TEXT("ItemsOwned"),
		18,
		FLinearColor(0.80f, 0.78f, 0.70f, 1.0f),
		false,
		ETextJustify::Center);
	Owned->SetText(FText::Format(
		NSLOCTEXT("RokItems", "OwnedFormat", "拥有数量:{0}"),
		FText::AsNumber(SelectedStack.Overlay)));
	PlaceOnCanvas(DetailPanel, Owned, FVector2D(81.0f, 401.0f), FVector2D(250.0f, 30.0f), 4);

	if (SelectedItem.bBatchUse)
	{
		URokUnityIndexedButton* Minus = MakeActionButton(TEXT("ItemsMinus"), 540, CommonAtlasPath, ItemMinusRect, CommonAtlasSize, FVector2D(46.0f), FText::FromString(TEXT("−")), 23);
		Minus->SetIsEnabled(SelectedItemsUseCount > 1);
		PlaceOnCanvas(DetailPanel, Minus, FVector2D(29.0f, 442.0f), FVector2D(46.0f), 6);
		PlaceOnCanvas(DetailPanel, MakeSolidBorder(TEXT("ItemsQuantityBack"), FLinearColor(0.55f, 0.53f, 0.44f, 0.75f)), FVector2D(82.0f, 444.0f), FVector2D(130.0f, 43.0f), 4);
		UTextBlock* Quantity = MakeScreenText(TEXT("ItemsQuantity"), 22, FLinearColor::White, true, ETextJustify::Center);
		Quantity->SetText(FText::AsNumber(SelectedItemsUseCount));
		PlaceOnCanvas(DetailPanel, Quantity, FVector2D(82.0f, 448.0f), FVector2D(130.0f, 34.0f), 6);
		URokUnityIndexedButton* Plus = MakeActionButton(TEXT("ItemsPlus"), 541, CommonAtlasPath, ItemPlusRect, CommonAtlasSize, FVector2D(46.0f), FText::FromString(TEXT("+")), 23);
		Plus->SetIsEnabled(SelectedItemsUseCount < FMath::Min<int64>(SelectedStack.Overlay, 9999));
		PlaceOnCanvas(DetailPanel, Plus, FVector2D(218.0f, 442.0f), FVector2D(46.0f), 6);
		URokUnityIndexedButton* Max = MakeActionButton(TEXT("ItemsMax"), 542, CommonAtlasPath, ItemMaxRect, CommonAtlasSize, FVector2D(87.0f, 51.0f), NSLOCTEXT("RokItems", "Max", "最大"), 19);
		Max->SetIsEnabled(SelectedItemsUseCount < FMath::Min<int64>(SelectedStack.Overlay, 9999));
		PlaceOnCanvas(DetailPanel, Max, FVector2D(270.0f, 439.0f), FVector2D(87.0f, 51.0f), 6);
	}
	if (SelectedItem.ButtonLocaleId > 0 && SelectedStack.bCatalogKnown)
	{
		PlaceOnCanvas(
			DetailPanel,
			MakeActionButton(TEXT("ItemsUse"), 543, CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokItems", "Use", "使用"), 28, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f)),
			FVector2D(105.0f, 513.0f),
			FVector2D(202.0f, 81.0f),
			6);
	}
	else
	{
		UTextBlock* RoutedUse = MakeScreenText(
			TEXT("ItemsRoutedUse"),
			17,
			FLinearColor(0.26f, 0.23f, 0.17f, 1.0f),
			false,
			ETextJustify::Center);
		RoutedUse->SetText(NSLOCTEXT(
			"RokItems",
			"RoutedUseLabel",
			"请在对应功能中使用"));
		PlaceOnCanvas(
			DetailPanel,
			RoutedUse,
			FVector2D(86.0f, 534.0f),
			FVector2D(240.0f, 40.0f),
			6);
	}
	PlaceOnCanvas(
		ModeCanvas,
		DetailPanel,
		CapturePoint(178.0f, 192.3f),
		FVector2D(412.0f, 616.0f),
		5);

	LogInventory();
}
