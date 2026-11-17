void URokUnityFeatureScreenWidget::BuildAllianceShopReference()
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	const bool bHasDefinitions = Catalog && Catalog->LoadAllianceShopReadonly();
	const auto* Definitions = bHasDefinitions ? &Catalog->GetAllianceShopReadonlyRows() : nullptr;
	const auto* Selected = Definitions ? Definitions->FindByPredicate([this](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return Row.Group == SelectedAllianceShopGroup && Row.ItemType == SelectedAllianceShopItemType;
	}) : nullptr;
	if (Definitions && !Selected)
	{
		Selected = Definitions->FindByPredicate([this](const FRokUnityAllianceShopReadonlyRow& Row)
		{
			return Row.Group == SelectedAllianceShopGroup;
		});
		if (Selected) SelectedAllianceShopItemType = Selected->ItemType;
	}
	const auto ResolveIconPath = [](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return Row.IconAtlas == TEXT("item1") ? Item1AtlasPath
			: Row.IconAtlas == TEXT("item2") ? Item2AtlasPath
			: Row.IconAtlas == TEXT("item3") ? Item3AtlasPath : Item5AtlasPath;
	};
	const auto LoadIcon = [](const TCHAR* Path, const FIntPoint& AtlasSize) -> UTexture2D*
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		if (!Texture) return nullptr;
		// Catalog UVs use the imported atlas dimensions. The platform resource
		// can still report placeholder dimensions when a tab is first opened.
		const FIntPoint ImportedSize = Texture->GetImportedSize();
		const FIntPoint ImageSize = ImportedSize.X > 0 && ImportedSize.Y > 0
			? ImportedSize : FIntPoint(Texture->GetSizeX(), Texture->GetSizeY());
		return ImageSize == AtlasSize ? Texture : nullptr;
	};
	const auto FormatRow = [](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("%s · ID %d · 品质 %d"), *Row.Name, Row.ItemType, Row.Quality));
	};
	const auto FormatIdentity = [](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("物品 ID %d · 配置品质 %d"), Row.ItemType, Row.Quality));
	};
	const auto FormatBuy = [](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("个人积分购买配置价：%d"), Row.SellingPrice));
	};
	const auto FormatStock = [](const FRokUnityAllianceShopReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("联盟积分补货配置价：%d"), Row.StockPrice));
	};
	const auto LogDefinitions = [this, Selected](int32 Count)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance shop static definitions: group=%d item_type=%d rows=%d authority=local_reference state_unchanged=1."),
			SelectedAllianceShopGroup, Selected->ItemType, Count);
	};
	UClass* RowClass = ResolveFeaturePageClass(TEXT("AllianceShopReferenceRow"));
	if (const URokUiPageWidget* Page = RowClass ? MountModePage(TEXT("AllianceShopReference")) : nullptr)
	{
		BindPageAction(*Page, TEXT("AllianceShopReferenceBack"), 690);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceShopReferenceDefinitions")), bHasDefinitions);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceShopReferenceUnavailable")), !bHasDefinitions);
		if (!Definitions) return;
		const auto ApplyIcon = [this](const URokUiPageWidget& Owner, FName Name, UTexture2D* Texture, const FIntRect& Rect, const FIntPoint& Size)
		{
			auto* Image = Owner.Get<UImage>(Name);
			Owner.SetPresent(Image, Texture != nullptr);
			if (!Image || !Texture) return;
			FSlateBrush Brush = Image->GetBrush();
			const FSlateBrush Atlas = MakeAtlasBrush(Texture, Rect, Size, Brush.GetImageSize());
			Brush.SetResourceObject(Texture);
			Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
			Image->SetBrush(Brush);
		};
		for (const FRokUnityAllianceShopGroupReadonlyRow& Group : Catalog->GetAllianceShopReadonlyGroups())
		{
			const bool bSelected = Group.Group == SelectedAllianceShopGroup;
			const FString GroupButton = FString::Printf(TEXT("AllianceShopGroup_%d"), Group.Group);
			BindPageAction(*Page, *GroupButton, 690 + Group.Group);
			const FName RegularLabel(*(GroupButton + TEXT("Label")));
			const FName SelectedLabel(*(GroupButton + TEXT("SelectedLabel")));
			Page->SetShown(Page->Get<UTextBlock>(RegularLabel), !bSelected);
			Page->SetShown(Page->Get<UTextBlock>(SelectedLabel), bSelected);
			SetFeaturePageText(*Page, bSelected ? SelectedLabel : RegularLabel, FText::FromString(Group.Name));
		}
		int32 VisibleIndex = 0;
		for (int32 Index = 0; Index < Definitions->Num(); ++Index)
		{
			const FRokUnityAllianceShopReadonlyRow& Definition = (*Definitions)[Index];
			if (Definition.Group != SelectedAllianceShopGroup) continue;
			auto* Host = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceShopReferenceSlot_%d"), VisibleIndex++));
			if (auto* Row = MountPageItem(Host, RowClass, TEXT("AllianceShop"), Definition.ItemType))
			{
				const bool bSelected = &Definition == Selected;
				Row->SetPresent(Row->Get<UBorder>(TEXT("Row")), !bSelected);
				Row->SetPresent(Row->Get<UBorder>(TEXT("SelectedRow")), bSelected);
				if (bSelected) Row->LegacyNames.Add(TEXT("SelectedRow"), TEXT("{Prefix}Row{Suffix}"));
				ApplyIcon(*Row, TEXT("Icon"), LoadIcon(ResolveIconPath(Definition), Definition.AtlasSize), Definition.IconRect, Definition.AtlasSize);
				SetFeaturePageText(*Row, TEXT("Name"), FormatRow(Definition));
				BindPageAction(*Row, TEXT("Select"), 18100 + Index);
			}
		}
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceShopReferenceDetail")), Selected != nullptr);
		if (Selected)
		{
			UTexture2D* Texture = LoadIcon(ResolveIconPath(*Selected), Selected->AtlasSize);
			ApplyIcon(*Page, TEXT("AllianceShopDetailIcon"), Texture, Selected->IconRect, Selected->AtlasSize);
			ApplyIcon(*Page, TEXT("AllianceShopDetailQuality"), Texture ? LoadUiTexture(Item0AtlasPath) : nullptr,
				ResolveBagQualityRect(Selected->Quality), Item0AtlasSize);
			SetFeaturePageText(*Page, TEXT("AllianceShopDetailName"), FText::FromString(Selected->Name));
			SetFeaturePageText(*Page, TEXT("AllianceShopDetailIdentity"), FormatIdentity(*Selected));
			SetFeaturePageText(*Page, TEXT("AllianceShopDetailBuyConfigPrice"), FormatBuy(*Selected));
			SetFeaturePageText(*Page, TEXT("AllianceShopDetailStockConfigPrice"), FormatStock(*Selected));
			LogDefinitions(VisibleIndex);
		}
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceShop", "ReferenceTitle", "联盟商店配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceShopReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceShop", "ReferenceBoundary",
		"仅显示商品配置定义和配置价；实际库存、积分余额、权限与购买状态均需服务器数据。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 156.0f), FVector2D(1020.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceShopReferenceBack"), 690,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceShop", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 738.0f), FVector2D(138.0f, 54.0f), 7);
	if (!Definitions)
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceShopReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAllianceShop", "ReferenceUnavailable",
			"联盟商店静态配置不可用；未读取库存或购买状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	for (const FRokUnityAllianceShopGroupReadonlyRow& Group : Catalog->GetAllianceShopReadonlyGroups())
	{
		const bool bSelected = SelectedAllianceShopGroup == Group.Group;
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceShopGroup_%d"), Group.Group), 690 + Group.Group,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(164.0f, 48.0f),
			FText::FromString(Group.Name), 18,
			bSelected ? FLinearColor(0.18f, 0.13f, 0.05f, 1.0f)
				: FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box, MiniBlueMargin),
			CapturePoint(196.0f + (Group.Group - 1) * 173.0f, 197.0f),
			FVector2D(164.0f, 48.0f), 7);
	}
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopListPanel"),
		FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)),
		CapturePoint(194.0f, 262.0f), FVector2D(636.0f, 482.0f), 3);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopDetailPanel"),
		FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
		CapturePoint(848.0f, 262.0f), FVector2D(385.0f, 458.0f), 3);
	int32 VisibleIndex = 0;
	for (int32 Index = 0; Index < Definitions->Num(); ++Index)
	{
		const FRokUnityAllianceShopReadonlyRow& Row = (*Definitions)[Index];
		if (Row.Group != SelectedAllianceShopGroup) continue;
		const float Y = 270.0f + VisibleIndex * 52.0f;
		const bool bSelected = Row.ItemType == SelectedAllianceShopItemType;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceShopRow_%d"), Row.ItemType),
			bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
				: FLinearColor(0.19f, 0.18f, 0.16f, 0.98f)),
			CapturePoint(202.0f, Y), FVector2D(618.0f, 48.0f), 4);
		const TCHAR* IconPath = ResolveIconPath(Row);
		if (LoadIcon(IconPath, Row.AtlasSize))
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
				*FString::Printf(TEXT("AllianceShopIcon_%d"), Row.ItemType),
				IconPath, Row.IconRect, Row.AtlasSize, FVector2D(40.0f)),
				CapturePoint(209.0f, Y + 4.0f), FVector2D(40.0f), 5);
		}
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("AllianceShopName_%d"), Row.ItemType), 17,
			FLinearColor(0.91f, 0.85f, 0.67f, 1.0f), true);
		Label->SetText(FormatRow(Row));
		PlaceOnCanvas(ModeCanvas, Label, CapturePoint(261.0f, Y + 9.0f),
			FVector2D(540.0f, 30.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceShopSelect_%d"), Row.ItemType), 18100 + Index,
			nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(618.0f, 48.0f)),
			CapturePoint(202.0f, Y), FVector2D(618.0f, 48.0f), 8);
		++VisibleIndex;
	}
	if (!Selected) return;
	const TCHAR* DetailIconPath = ResolveIconPath(*Selected);
	if (LoadIcon(DetailIconPath, Selected->AtlasSize))
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceShopDetailQuality"),
			Item0AtlasPath, ResolveBagQualityRect(Selected->Quality), Item0AtlasSize,
			FVector2D(100.0f)), CapturePoint(990.0f, 282.0f), FVector2D(100.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceShopDetailIcon"),
			DetailIconPath, Selected->IconRect, Selected->AtlasSize, FVector2D(88.0f)),
			CapturePoint(996.0f, 288.0f), FVector2D(88.0f), 6);
	}
	UTextBlock* Name = MakeScreenText(TEXT("AllianceShopDetailName"), 20,
		FLinearColor(0.93f, 0.87f, 0.64f, 1.0f), true, ETextJustify::Center);
	Name->SetText(FText::FromString(Selected->Name));
	PlaceOnCanvas(ModeCanvas, Name, CapturePoint(870.0f, 403.0f), FVector2D(340.0f, 36.0f), 6);
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceShopDetailIdentity"), 16,
		FLinearColor(0.82f, 0.82f, 0.77f, 1.0f), false);
	Identity->SetText(FormatIdentity(*Selected));
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(876.0f, 452.0f), FVector2D(340.0f, 27.0f), 6);
	UTextBlock* BuyPrice = MakeScreenText(TEXT("AllianceShopDetailBuyConfigPrice"), 16,
		FLinearColor(0.85f, 0.83f, 0.76f, 1.0f), false);
	BuyPrice->SetText(FormatBuy(*Selected));
	PlaceOnCanvas(ModeCanvas, BuyPrice, CapturePoint(876.0f, 500.0f), FVector2D(340.0f, 30.0f), 6);
	UTextBlock* StockPrice = MakeScreenText(TEXT("AllianceShopDetailStockConfigPrice"), 16,
		FLinearColor(0.85f, 0.83f, 0.76f, 1.0f), false);
	StockPrice->SetText(FormatStock(*Selected));
	PlaceOnCanvas(ModeCanvas, StockPrice, CapturePoint(876.0f, 540.0f), FVector2D(340.0f, 30.0f), 6);
	UTextBlock* State = MakeScreenText(TEXT("AllianceShopDetailServerBoundary"), 15,
		FLinearColor(0.77f, 0.81f, 0.71f, 1.0f), false);
	State->SetAutoWrapText(true);
	State->SetText(NSLOCTEXT("RokAllianceShop", "ServerBoundary",
		"此处不能查询实际库存、积分余额、可购买数量或购买结果。"));
	PlaceOnCanvas(ModeCanvas, State, CapturePoint(876.0f, 599.0f), FVector2D(340.0f, 74.0f), 6);
	LogDefinitions(VisibleIndex);
}
