void URokUnityFeatureScreenWidget::BuildAllianceRankingReference()
{
	TArray<FRokLeaderboardRow> Categories;
	FString LoadError;
	const bool bHasCatalog = RokAllianceRankingReadonlyCatalog::Load(Categories, LoadError);
	const FRokLeaderboardRow* Selected = Categories.FindByPredicate([this](const FRokLeaderboardRow& Category)
	{
		return Category.Id == SelectedAllianceRankingCategoryId;
	});
	UTexture2D* IconAtlas = bHasCatalog && !Selected ? LoadUiTexture(VipPlayerAtlasPath) : nullptr;
	const FIntPoint ImportedSize = IconAtlas ? IconAtlas->GetImportedSize() : FIntPoint::ZeroValue;
	const bool bIconAtlasReady = IconAtlas && (ImportedSize.X > 0 && ImportedSize.Y > 0
		? ImportedSize : FIntPoint(IconAtlas->GetSizeX(), IconAtlas->GetSizeY())) == VipPlayerAtlasSize;
	UClass* CardClass = ResolveFeaturePageClass(TEXT("AllianceRankingReferenceCard"));
	if (const URokUiPageWidget* Page = CardClass ? MountModePage(TEXT("AllianceRankingReference")) : nullptr)
	{
		BindPageAction(*Page, TEXT("AllianceRankingBack"), 19710);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceRankingOverview")), bHasCatalog && !Selected);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceRankingDetail")), Selected != nullptr);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceRankingCatalogUnavailable")), !bHasCatalog);
		if (!bHasCatalog)
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok alliance ranking readonly catalog unavailable: %s"), *LoadError);
			return;
		}
		if (Selected)
		{
			SetFeaturePageText(*Page, TEXT("AllianceRankingDetailTitle"), FText::FromString(Selected->Name));
			return;
		}
		for (int32 Index = 0; Index < Categories.Num(); ++Index)
		{
			const FRokLeaderboardRow& Category = Categories[Index];
			auto* Host = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceRankingCategorySlot_%d"), Index));
			if (auto* Card = MountPageItem(Host, CardClass, TEXT("AllianceRanking"), Category.Id))
			{
				SetFeaturePageText(*Card, TEXT("CategoryName"), FText::FromString(Category.Name));
				auto* Image = Card->Get<UImage>(TEXT("CategoryIcon"));
				Card->SetPresent(Image, bIconAtlasReady);
				Card->SetPresent(Card->Get<UTextBlock>(TEXT("IconMissing")), !bIconAtlasReady);
				if (Image && bIconAtlasReady)
				{
					FSlateBrush Brush = Image->GetBrush();
					const FSlateBrush Atlas = MakeAtlasBrush(IconAtlas, Category.IconRect, VipPlayerAtlasSize, Brush.GetImageSize());
					Brush.SetResourceObject(IconAtlas);
					Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
					Image->SetBrush(Brush);
				}
				BindPageAction(*Card, TEXT("CategoryHit"), 19700 + Index);
			}
		}
		return;
	}

	// The source root is 1440x810. pl_Total/pl_Detail are centered 1074x664
	// at anchored (0,-18). The six fixed category slots have 340x181 source
	// RectTransforms with top-left positions derived from their pinned anchors.
	BuildSplitWindowFrame(FVector2D(146.0f, 28.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceRanking", "Title", "联盟排行榜"), FVector2D(1225.0f, 28.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceRankingContentShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		FVector2D(183.0f, 91.0f), FVector2D(1074.0f, 664.0f), 2);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceRankingBack"), 19710,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceRanking", "Back", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		FVector2D(155.0f, 32.0f), FVector2D(138.0f, 54.0f), 8);

	if (!bHasCatalog)
	{
		UTextBlock* Unavailable = MakeScreenText(TEXT("AllianceRankingCatalogUnavailable"), 22,
			FLinearColor(0.88f, 0.84f, 0.76f, 1.0f), false, ETextJustify::Center);
		Unavailable->SetText(NSLOCTEXT("RokAllianceRanking", "CatalogUnavailable",
			"排行榜分类暂不可用"));
		PlaceOnCanvas(ModeCanvas, Unavailable, FVector2D(250.0f, 360.0f),
			FVector2D(940.0f, 55.0f), 6);
		UE_LOG(LogTemp, Warning, TEXT("Rok alliance ranking readonly catalog unavailable: %s"), *LoadError);
		return;
	}

	if (Selected)
	{
		UTextBlock* CategoryTitle = MakeScreenText(TEXT("AllianceRankingDetailTitle"), 27,
			FLinearColor(0.95f, 0.89f, 0.67f, 1.0f), true, ETextJustify::Center);
		CategoryTitle->SetText(FText::FromString(Selected->Name));
		PlaceOnCanvas(ModeCanvas, CategoryTitle, FVector2D(340.0f, 170.0f),
			FVector2D(760.0f, 48.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceRankingEmptyListPanel"),
			FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)), FVector2D(192.915f, 227.23f),
			FVector2D(1054.17f, 531.22f), 3);
		UTextBlock* EmptyList = MakeScreenText(TEXT("AllianceRankingEmptyList"), 23,
			FLinearColor(0.87f, 0.85f, 0.79f, 1.0f), false, ETextJustify::Center);
		EmptyList->SetText(NSLOCTEXT("RokAllianceRanking", "NoRankData", "暂无排名数据"));
		PlaceOnCanvas(ModeCanvas, EmptyList, FVector2D(520.0f, 393.0f),
			FVector2D(400.0f, 60.0f), 6);
		UTextBlock* NoSelf = MakeScreenText(TEXT("AllianceRankingNoSelfData"), 17,
			FLinearColor(0.73f, 0.73f, 0.70f, 1.0f), false, ETextJustify::Center);
		NoSelf->SetText(NSLOCTEXT("RokAllianceRanking", "NoSelfData",
			"当前排名与得分未接入"));
		PlaceOnCanvas(ModeCanvas, NoSelf, FVector2D(515.0f, 470.0f),
			FVector2D(410.0f, 34.0f), 6);
		return;
	}

	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceRankingOverviewBoundary"), 17,
		FLinearColor(0.83f, 0.82f, 0.75f, 1.0f), false, ETextJustify::Center);
	Boundary->SetText(NSLOCTEXT("RokAllianceRanking", "CategoryPreview",
		"六类排行榜分类预览 · 排名数据未接入"));
	PlaceOnCanvas(ModeCanvas, Boundary, FVector2D(335.0f, 115.0f),
		FVector2D(770.0f, 34.0f), 6);
	// These are the six pinned pl_Total slot rectangles, in local ID order for
	// the offline preview. Unity assigns server-response entries to slots at
	// runtime; category-to-slot order is deliberately not claimed as parity.
	static const FVector2D SourceSlots[] = {
		FVector2D(200.0f, 141.0f), FVector2D(550.0f, 141.0f),
		FVector2D(900.0f, 141.0f), FVector2D(200.0f, 385.0f),
		FVector2D(550.0f, 385.0f), FVector2D(900.0f, 385.0f),
	};
	for (int32 Index = 0; Index < Categories.Num() && Index < UE_ARRAY_COUNT(SourceSlots); ++Index)
	{
		const FRokLeaderboardRow& Category = Categories[Index];
		const FVector2D Position = FVector2D(SourceSlots[Index].X, SourceSlots[Index].Y);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceRankingCategoryPanel_%d"), Category.Id),
			FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
			Position, FVector2D(340.0f, 181.0f), 3);
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(TEXT("AllianceRankingCategoryName_%d"), Category.Id), 22,
			FLinearColor(0.96f, 0.90f, 0.73f, 1.0f), true);
		Name->SetText(FText::FromString(Category.Name));
		PlaceOnCanvas(ModeCanvas, Name, Position + FVector2D(16.0f, 23.0f),
			FVector2D(164.0f, 43.0f), 5);
		if (bIconAtlasReady)
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
				*FString::Printf(TEXT("AllianceRankingCategoryIcon_%d"), Category.Id),
				VipPlayerAtlasPath, Category.IconRect, VipPlayerAtlasSize,
				FVector2D(148.0f, 176.0f)), Position + FVector2D(190.0f, 2.0f),
				FVector2D(148.0f, 176.0f), 5);
		}
		else
		{
			UTextBlock* MissingIcon = MakeScreenText(
				*FString::Printf(TEXT("AllianceRankingIconMissing_%d"), Category.Id), 15,
				FLinearColor(0.82f, 0.78f, 0.68f, 1.0f), false, ETextJustify::Center);
			MissingIcon->SetText(NSLOCTEXT("RokAllianceRanking", "IconMissing", "图标未接入"));
			PlaceOnCanvas(ModeCanvas, MissingIcon, Position + FVector2D(194.0f, 72.0f),
				FVector2D(135.0f, 30.0f), 5);
		}
		UTextBlock* Empty = MakeScreenText(
			*FString::Printf(TEXT("AllianceRankingCategoryEmpty_%d"), Category.Id), 15,
			FLinearColor(0.76f, 0.75f, 0.70f, 1.0f), false);
		Empty->SetText(NSLOCTEXT("RokAllianceRanking", "CategoryEmpty", "暂无排名数据"));
		PlaceOnCanvas(ModeCanvas, Empty, Position + FVector2D(16.0f, 86.0f),
			FVector2D(164.0f, 34.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceRankingCategoryHit_%d"), Category.Id),
			19700 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue,
			FVector2D(340.0f, 181.0f)), Position, FVector2D(340.0f, 181.0f), 8);
	}
}
