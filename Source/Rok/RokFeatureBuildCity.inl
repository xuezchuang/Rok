void URokUnityFeatureScreenWidget::BuildBuildCity()
{
	// UI_Item_BuildCityBuilding uses Unity pixel sizes. Slate renders points
	// at 96 DPI; preserve fractional 13.5pt for the source 18px body text.
	auto ApplyCardFont = [](UTextBlock* Text, float Pixels)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Pixels * 72.0f / 96.0f;
		Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D::ZeroVector);
	};
	BuildCityAnimatedCards.Reset();
	BuildCityCardSourceIndices.Reset();
	VisibleBuildCityTypeIds.Reset();
	BuildableBuildCityTypeIds.Reset();
	URokUnityGameplayCatalogSubsystem* Catalog = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		Catalog =
			GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
		if (Catalog && !Catalog->IsCatalogLoaded())
		{
			Catalog->LoadCatalog();
		}
	}

	int32 TownHallLevel = 1;
	TMap<int32, int32> CurrentBuildingCounts;
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>()
		: nullptr;
	const URokServerCitySubsystem* ServerCity = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	if (ServerCity && ServerCity->HasAppliedServerCity())
	{
		for (const FRokServerBuilding& Row : ServerCity->GetAppliedBuildings())
		{
			if (Row.Level < 0) continue;
			++CurrentBuildingCounts.FindOrAdd(Row.Type);
			if (Row.Type == 1) TownHallLevel = FMath::Max(TownHallLevel, Row.Level);
		}
	}
	else if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			int32 TaggedTypeId = 0;
			for (const FName& Tag : Actor->Tags)
			{
				const FString TagString = Tag.ToString();
				if (TagString.StartsWith(TEXT("UnityBuildingType_")))
				{
					TaggedTypeId = FCString::Atoi(
						*TagString.Mid(18));
					break;
				}
			}
			if (TaggedTypeId <= 0)
			{
				continue;
			}
			CurrentBuildingCounts.FindOrAdd(TaggedTypeId) += 1;
			if (TaggedTypeId == 1 && UpgradeSubsystem)
			{
				TownHallLevel = FMath::Max(
					TownHallLevel,
					UpgradeSubsystem->GetBuildingStateForActor(Actor).Level);
			}
		}
	}

	auto ResolveBuildState = [&CurrentBuildingCounts, TownHallLevel](
		const FRokUnityCatalogBuildingType& BuildingType,
		int32& OutCurrentCount,
		int32& OutCurrentLimit,
		int32& OutMaximumLimit,
		int32& OutUnlockTownHallLevel)
	{
		OutCurrentCount = CurrentBuildingCounts.FindRef(BuildingType.TypeId);
		OutCurrentLimit = BuildingType.GetCountLimit(TownHallLevel);
		OutMaximumLimit = BuildingType.GetMaximumCountLimit();
		OutUnlockTownHallLevel = 0;
		for (const TPair<int32, int32>& Pair :
			BuildingType.CountLimitByTownHallLevel)
		{
			if (Pair.Key > TownHallLevel
				&& Pair.Value > OutCurrentCount
				&& (OutUnlockTownHallLevel == 0
					|| Pair.Key < OutUnlockTownHallLevel))
			{
				OutUnlockTownHallLevel = Pair.Key;
			}
		}

		if (OutMaximumLimit <= 0)
		{
			return BuildingType.Group == 3 ? 0 : 1;
		}
		if (OutCurrentCount < OutCurrentLimit)
		{
			return 0;
		}
		return OutCurrentCount >= OutMaximumLimit ? 2 : 1;
	};

	int32 CategoryBuildableCounts[3] = {0, 0, 0};
	if (Catalog && Catalog->IsCatalogLoaded())
	{
		for (const int32 TypeId : Catalog->GetBuildingTypeIds())
		{
			FRokUnityCatalogBuildingType BuildingType;
			if (!Catalog->GetBuildingType(TypeId, BuildingType)
				|| BuildingType.Group < 1
				|| BuildingType.Group > 3
				|| BuildingType.LevelCount <= 0)
			{
				continue;
			}
			int32 CurrentCount = 0;
			int32 CurrentLimit = 0;
			int32 MaximumLimit = 0;
			int32 UnlockTownHallLevel = 0;
			if (ResolveBuildState(
					BuildingType,
					CurrentCount,
					CurrentLimit,
					MaximumLimit,
					UnlockTownHallLevel)
				== 0)
			{
				CategoryBuildableCounts[BuildingType.Group - 1] += 1;
			}
		}
	}

	// BuildCityMediator selects the target's group before centering its card.
	FRokUnityCatalogBuildingType GuideBuilding;
	if (BuildCityGuideTypeId > 0 && Catalog && Catalog->GetBuildingType(BuildCityGuideTypeId, GuideBuilding)
		&& GuideBuilding.Group >= 1 && GuideBuilding.Group <= 3)
	{
		SelectedBuildCityCategory = GuideBuilding.Group - 1;
	}

	static const int32 EconomyTypes[] = {
		2, 3, 4, 5, 7, 13, 15, 16, 19, 20, 21, 30};
	static const int32 MilitaryTypes[] = {
		9, 10, 11, 12, 14, 17, 18, 28, 29, 31};
	static const int32 DecorationTypes[] = {51, 52, 53};
	const int32* OrderedTypes = EconomyTypes;
	int32 OrderedTypeCount = UE_ARRAY_COUNT(EconomyTypes);
	if (SelectedBuildCityCategory == 1)
	{
		OrderedTypes = MilitaryTypes;
		OrderedTypeCount = UE_ARRAY_COUNT(MilitaryTypes);
	}
	else if (SelectedBuildCityCategory == 2)
	{
		OrderedTypes = DecorationTypes;
		OrderedTypeCount = UE_ARRAY_COUNT(DecorationTypes);
	}
	for (int32 Index = 0; Index < OrderedTypeCount; ++Index)
	{
		FRokUnityCatalogBuildingType BuildingType;
		if (Catalog
			&& Catalog->GetBuildingType(OrderedTypes[Index], BuildingType)
			&& BuildingType.LevelCount > 0)
		{
			VisibleBuildCityTypeIds.Add(OrderedTypes[Index]);
		}
	}

	// Unity shows the available buildings before the locked and maxed cards.
	auto BuildStateForType = [&](int32 TypeId)
	{
		FRokUnityCatalogBuildingType BuildingType;
		Catalog->GetBuildingType(TypeId, BuildingType);
		int32 Count, Limit, Maximum, UnlockLevel;
		return ResolveBuildState(BuildingType, Count, Limit, Maximum, UnlockLevel);
	};
	VisibleBuildCityTypeIds.StableSort([&](int32 A, int32 B)
	{
		return BuildStateForType(A) < BuildStateForType(B);
	});


	const FIntPoint PreviewProfile = RokBuildingSourcePreview::ResolveProfile(GetGameInstance(), TownHallLevel);
	bBuildCityGuideNeedsScroll = VisibleBuildCityTypeIds.Contains(BuildCityGuideTypeId);
	const FText EmptyState = Catalog
		? NSLOCTEXT("RokBuildCity", "CategoryEmpty", "该分类没有可建造的 Unity 配置")
		: NSLOCTEXT("RokBuildCity", "CatalogUnavailable", "Unity 建筑配置运行时尚未加载");
	const auto CardName = [](const FRokUnityCatalogBuildingType& Type)
	{
		return Type.DisplayNameZhCn.IsEmpty() ? ResolveBuildingTypeName(Type.TypeId) : FText::FromString(Type.DisplayNameZhCn);
	};
	const auto LockLabel = [TownHallLevel](int32 BuildState, int32 UnlockTownHallLevel)
	{
		return BuildState == 2 ? NSLOCTEXT("RokBuildCity", "MaximumBuilt", "已达建造上限")
			: FText::Format(NSLOCTEXT("RokBuildCity", "TownHallUnlock", "市政厅达到 {0} 级后解锁"),
				FText::AsNumber(FMath::Max(TownHallLevel + 1, UnlockTownHallLevel)));
	};
	struct FBuildResourceEntry
	{
		FIntRect IconRect;
		int32 Amount;
	};
	const auto BuildResourceList = [](const FRokUnityCatalogBuildingType& Type)
	{
		TArray<FBuildResourceEntry> Resources;
		if (Type.Level1Cost.Wood > 0) Resources.Add({WoodRect, Type.Level1Cost.Wood});
		if (Type.Level1Cost.Food > 0) Resources.Add({FoodRect, Type.Level1Cost.Food});
		if (Type.Level1Cost.Stone > 0) Resources.Add({StoneRect, Type.Level1Cost.Stone});
		if (Type.Level1Cost.Gold > 0) Resources.Add({GoldRect, Type.Level1Cost.Gold});
		return Resources;
	};
	const auto FindCardPreview = [&PreviewProfile](int32 TypeId, RokBuildingSourcePreview::FVisual& Art) -> UTexture2D*
	{
		if (!RokBuildingSourcePreview::FindVisual(TypeId, PreviewProfile.X, PreviewProfile.Y, Art)
			&& !RokBuildingSourcePreview::FindVisual(TypeId, 101, 1, Art)) return nullptr;
		// UI_Item_BuildCityBuilding/pl_icon has scale 0.65.
		Art.Size *= 0.65;
		Art.Offset *= 0.65;
		return RokBuildingSourcePreview::LoadTexture(Art);
	};
	const auto GetGuideTexture = [this]()
	{
		if (!TaskGuideTexture)
			TaskGuideTexture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(TEXT("WorldSearch/finger001.png")));
		return TaskGuideTexture.Get();
	};

	UClass* AvailableClass = ResolveFeaturePageClass(TEXT("BuildCityCardAvailable"));
	UClass* LockedClass = ResolveFeaturePageClass(TEXT("BuildCityCardLocked"));
	if (const URokUiPageWidget* Page = AvailableClass && LockedClass ? MountModePage(NAME_None, false) : nullptr)
	{
		BuildCityAnimatedRoot = Page->Get<UCanvasPanel>(TEXT("BuildCityAnimatedRoot"));
		auto* Scroll = Page->Get<UScrollBox>(TEXT("BuildCityBuildingScroll"));
		BindPageAction(*Page, TEXT("BuildCityMaskClose"), 1);
		BindPageAction(*Page, TEXT("BuildCityDrawerInputShield"), 1399);
		for (int32 Category = 0; Category < 3; ++Category)
		{
			const FString TabName = FString::Printf(TEXT("BuildCityCategory_%d"), Category);
			const bool bSelected = Category == SelectedBuildCityCategory;
			if (auto* Tab = Page->Get<URokUnityIndexedButton>(*TabName)) Tab->SetPressAnimationEnabled(false);
			BindPageAction(*Page, *TabName, 1300 + Category);
			Page->SetShown(Page->Get<UImage>(*(TabName + TEXT("SelectedBackground"))), bSelected);
			Page->SetShown(Page->Get<UImage>(*(TabName + TEXT("NormalBackground"))), !bSelected);
			Page->SetShown(Page->Get<UImage>(*FString::Printf(TEXT("BuildCityCategoryIcon_%d"), Category)), bSelected);
			Page->SetShown(Page->Get<UImage>(*FString::Printf(TEXT("BuildCityCategoryNormalIcon_%d"), Category)), !bSelected);
			if (Category < 2)
			{
				auto* Badge = Page->Get<UImage>(*FString::Printf(TEXT("BuildCityCategoryRed_%d"), Category));
				auto* Count = Page->Get<UTextBlock>(*FString::Printf(TEXT("BuildCityCategoryRedCount_%d"), Category));
				const bool bHasBadge = CategoryBuildableCounts[Category] > 0;
				Page->SetPresent(Badge, bHasBadge);
				Page->SetPresent(Count, bHasBadge);
				if (bHasBadge && Count) Count->SetText(FText::AsNumber(CategoryBuildableCounts[Category]));
			}
		}
		auto* CatalogState = Page->Get<UTextBlock>(TEXT("BuildCityCatalogState"));
		Page->SetPresent(CatalogState, VisibleBuildCityTypeIds.IsEmpty());
		if (CatalogState && VisibleBuildCityTypeIds.IsEmpty()) CatalogState->SetText(EmptyState);

		if (Scroll)
		{
			for (int32 Index = 0; Index < VisibleBuildCityTypeIds.Num(); ++Index)
			{
				const int32 TypeId = VisibleBuildCityTypeIds[Index];
				FRokUnityCatalogBuildingType Type;
				if (!Catalog->GetBuildingType(TypeId, Type)) continue;
				int32 Count, Limit, Maximum, UnlockLevel;
				const int32 State = ResolveBuildState(Type, Count, Limit, Maximum, UnlockLevel);
				const bool bBuildable = State == 0;
				if (bBuildable) BuildableBuildCityTypeIds.Add(TypeId);
				auto* Item = CreateWidget<URokUiPageWidget>(this, bBuildable ? AvailableClass : LockedClass);
				if (!Item) continue;
				Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				Item->LegacyPrefix = TEXT("BuildCityCard");
				Item->LegacySuffix = FString::Printf(TEXT("_%d"), TypeId);
				Item->LegacyNames.Add(TEXT("Card"), TEXT("{Prefix}{Suffix}"));
				Item->LegacyNames.Add(TEXT("TaskGuide"), TEXT("BuildCityTaskGuide"));
				auto* Card = Item->Get<UCanvasPanel>(TEXT("Card"));
				const auto* CardSlot = Card ? Cast<UCanvasPanelSlot>(Card->Slot) : nullptr;
				if (!CardSlot) continue;
				BuildCityAnimatedCards.Add(Card);
				BuildCityCardSourceIndices.Add(Index);
				const FVector2D CardSize = CardSlot->GetSize();
				Scroll->AddChild(WrapFixedSize(Item, *FString::Printf(TEXT("BuildCityCardSize_%d"), TypeId), CardSize));

				auto* Preview = Item->Get<UImage>(TEXT("Preview"));
				auto* PreviewSlot = Preview ? Cast<UCanvasPanelSlot>(Preview->Slot) : nullptr;
				const auto* PreviewCenter = Item->Get<UCanvasPanel>(TEXT("PreviewCenter"));
				const auto* PreviewCenterSlot = PreviewCenter ? Cast<UCanvasPanelSlot>(PreviewCenter->Slot) : nullptr;
				RokBuildingSourcePreview::FVisual Art;
				UTexture2D* Texture = FindCardPreview(TypeId, Art);
				Item->SetPresent(Preview, Texture && PreviewSlot);
				if (Texture && PreviewSlot)
				{
					const FVector2D PaneOrigin = PreviewSlot->GetPosition();
					const FVector2D PaneSize = PreviewSlot->GetSize();
					const FVector2D LocalCenter = PreviewCenterSlot ? PreviewCenterSlot->GetPosition() - PaneOrigin
						: PaneSize * 0.5 + FVector2D(0, 6);
					const FBox2D Draw = Art.FitInPane(LocalCenter, PaneSize);
					Preview->SetBrushFromTexture(Texture);
					PreviewSlot->SetPosition(PaneOrigin + Draw.Min);
					PreviewSlot->SetSize(Draw.GetSize());
				}

				const FText Name = CardName(Type);
				SetFeaturePageText(*Item, TEXT("Name"), Name);
				SetFeaturePageText(*Item, TEXT("Description"), ResolveBuildCityDescription(Type));
				if (bBuildable)
				{
					const TArray<FBuildResourceEntry> Resources = BuildResourceList(Type);
					const int32 ResourceCount = FMath::Min(2, Resources.Num());
					for (const FName ResourceStem : {FName(TEXT("ResourceIcon")), FName(TEXT("ResourceAmount"))})
					{
						for (int32 ResourceIndex = 0; ResourceIndex < 2; ++ResourceIndex)
						{
							const FName WidgetName(*FString::Printf(TEXT("%s_%d"), *ResourceStem.ToString(), ResourceIndex));
							Item->SetPresent(Item->Get<UWidget>(WidgetName), ResourceCount == 2);
						}
					}
					auto* SoloIcon = Item->Get<UImage>(TEXT("SoloIcon"));
					auto* SoloAmount = Item->Get<UTextBlock>(TEXT("SoloAmount"));
					Item->SetPresent(SoloIcon, ResourceCount == 1);
					Item->SetPresent(SoloAmount, ResourceCount == 1);
					if (ResourceCount == 1)
					{
						Item->LegacyNames.Add(TEXT("SoloIcon"), TEXT("{Prefix}ResourceIcon{Suffix}_0"));
						Item->LegacyNames.Add(TEXT("SoloAmount"), TEXT("{Prefix}ResourceAmount{Suffix}_0"));
					}
					for (int32 ResourceIndex = 0; ResourceIndex < ResourceCount; ++ResourceIndex)
					{
						auto* Icon = ResourceCount == 1 ? SoloIcon : Item->Get<UImage>(
							*FString::Printf(TEXT("ResourceIcon_%d"), ResourceIndex));
						auto* Amount = ResourceCount == 1 ? SoloAmount : Item->Get<UTextBlock>(
							*FString::Printf(TEXT("ResourceAmount_%d"), ResourceIndex));
						const auto& Resource = Resources[ResourceIndex];
						if (Icon)
						{
							FSlateBrush Brush = Icon->GetBrush();
							const FSlateBrush Atlas = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
								Resource.IconRect, CommonAtlasSize, Brush.GetImageSize());
							Brush.SetResourceObject(Atlas.GetResourceObject());
							Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
							Icon->SetBrush(Brush);
						}
						if (Amount) Amount->SetText(FText::AsNumber(Resource.Amount));
					}
					SetFeaturePageText(*Item, TEXT("Count"), FText::Format(NSLOCTEXT("RokBuildCity", "CountFormat", "{0}/{1}"),
						FText::AsNumber(Count), FText::AsNumber(Limit)));
					SetFeaturePageText(*Item, TEXT("Duration"), FormatBuildDuration(Type.Level1BuildSeconds));
					if (auto* Button = Item->Get<URokUnityIndexedButton>(TEXT("Button")))
					{
						Button->SetToolTipText(FText::Format(NSLOCTEXT("RokBuildCity", "BuildableTooltip", "建造 {0}"), Name));
					}
					BindPageAction(*Item, TEXT("Button"), 1320 + Index);
				}
				else
				{
					SetFeaturePageText(*Item, TEXT("Locked"), LockLabel(State, UnlockLevel));
				}

				auto* Guide = Item->Get<UImage>(TEXT("TaskGuide"));
				UTexture2D* GuideTexture = TypeId == BuildCityGuideTypeId ? GetGuideTexture() : nullptr;
				Item->SetPresent(Guide, GuideTexture != nullptr);
				if (Guide && GuideTexture) Guide->SetBrushFromTexture(GuideTexture);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Rok build city screen: category=%d types=%d server_city=%d."),
			SelectedBuildCityCategory, VisibleBuildCityTypeIds.Num(), ServerCity && ServerCity->HasAppliedServerCity());
		return;
	}

	UWidget* MaskClose = MakeActionButton(
			TEXT("BuildCityMaskClose"),
			1,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D::ZeroVector);
	auto* MaskSlot = ModeCanvas->AddChildToCanvas(MaskClose);
	MaskSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	MaskSlot->SetOffsets(FMargin(0));
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("BuildCityDrawerBackground"),
			BuildCityLoadingAtlasPath,
			BuildCityDrawerRect,
			BuildCityLoadingAtlasSize,
			// rect/bg: bottom anchored, x=15, y=-10, delta=(22,153), pivot=(1,0).
			// Sprite border is Unity (left,bottom,right,top), not Slate order.
			FVector2D(1462.0f, 153.0f),
			FMargin(0.47f, 0.41f, 0.46f, 0.44f)),
		FVector2D(-7.0f, -130.0f),
		FVector2D(1462.0f, 153.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("BuildCityDrawerInputShield"),
			1399,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(1440.0f, 130.0f)),
		FVector2D(0.0f, -130.0f),
		FVector2D(1440.0f, 130.0f),
		3);

	UScrollBox* BuildingScroll =
		WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("BuildCityBuildingScroll"));
	BuildingScroll->SetOrientation(Orient_Horizontal);
	BuildingScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	BuildingScroll->SetAllowOverscroll(true);
	BuildingScroll->SetAnimateWheelScrolling(true);
	BuildingScroll->SetWheelScrollMultiplier(9.0f);
	// At 1440x900 the cards start at (144, 416). Anchor to the bottom
	// so the rail, card footer and mask follow Unity's expanded canvas.
	constexpr float CardHeight = 501.4f;
	constexpr float AnimationHeadroom = 241.1f;
	constexpr float VisibleCardHeight = 484.0f;
	UCanvasPanel* BuildingMask = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BuildCityListMask"));
	BuildingMask->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildingMask->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, BuildingMask,
		FVector2D(BuildCityListLeft, -VisibleCardHeight - AnimationHeadroom),
		FVector2D(UnityReferenceWidth - BuildCityListLeft, VisibleCardHeight + AnimationHeadroom), 4);
	BuildingScroll->SetClipping(EWidgetClipping::Inherit);
	auto* ScrollSlot = BuildingMask->AddChildToCanvas(BuildingScroll);
	ScrollSlot->SetAnchors(FAnchors(0, 0, 1, 0));
	ScrollSlot->SetOffsets(FMargin(0, AnimationHeadroom, 0, CardHeight));

	if (VisibleBuildCityTypeIds.IsEmpty())
	{
		UTextBlock* Empty = MakeScreenText(
			TEXT("BuildCityCatalogState"),
			23,
			FLinearColor(0.78f, 0.76f, 0.68f, 1.0f),
			false,
			ETextJustify::Center);
		Empty->SetText(EmptyState);
		PlaceOnCanvas(
			ModeCanvas,
			Empty,
			FVector2D(330.0f, -320.0f),
			FVector2D(780.0f, 50.0f),
			5);
	}
	else
	{
		for (int32 Index = 0; Index < VisibleBuildCityTypeIds.Num(); ++Index)
		{
			const int32 TypeId = VisibleBuildCityTypeIds[Index];
			FRokUnityCatalogBuildingType BuildingType;
			if (!Catalog->GetBuildingType(TypeId, BuildingType))
			{
				continue;
			}

			int32 CurrentCount = 0;
			int32 CurrentLimit = 0;
			int32 MaximumLimit = 0;
			int32 UnlockTownHallLevel = 0;
			const int32 BuildState = ResolveBuildState(
				BuildingType,
				CurrentCount,
				CurrentLimit,
				MaximumLimit,
				UnlockTownHallLevel);
			const bool bBuildable = BuildState == 0;
			if (bBuildable)
			{
				BuildableBuildCityTypeIds.Add(TypeId);
			}

			UCanvasPanel* Card =
				WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(),
					*FString::Printf(
						TEXT("BuildCityCard_%d"),
						TypeId));
			Card->SetRenderTransformPivot(FVector2D(0.5));
			BuildCityAnimatedCards.Add(Card);
			BuildCityCardSourceIndices.Add(Index);
			const FIntRect CardRect = bBuildable
				? BuildCityCardLightRect
				: BuildCityCardDarkRect;
			PlaceOnCanvas(
				Card,
				MakeAtlasImage(
					*FString::Printf(TEXT("BuildCityCardLeft_%d"), TypeId),
					BuildCityImageAtlasPath,
					CardRect,
					BuildCityImageAtlasSize,
					FVector2D(159.0f, 501.0f)),
				FVector2D(7.0f, 0.2f),
				FVector2D(159.0f, 501.0f));
			UImage* CardRight = MakeAtlasImage(
				*FString::Printf(TEXT("BuildCityCardRight_%d"), TypeId),
				BuildCityImageAtlasPath,
				CardRect,
				BuildCityImageAtlasSize,
				FVector2D(159.0f, 501.0f));
			CardRight->SetRenderTransformPivot(FVector2D(0.5f));
			CardRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
			PlaceOnCanvas(
				Card,
				CardRight,
				FVector2D(166.0f, 0.2f),
				FVector2D(159.0f, 501.0f));

			RokBuildingSourcePreview::FVisual Art;
			if (auto* Texture = FindCardPreview(TypeId, Art))
			{
				const FBox2D Draw = Art.FitInPane(FVector2D(147, 138), FVector2D(294, 264));
				auto* Preview = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
					*FString::Printf(TEXT("BuildCityCardPreview_%d"), TypeId));
				Preview->SetBrushFromTexture(Texture);
				Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
				Preview->SetColorAndOpacity(bBuildable
					? FLinearColor::White : FLinearColor(0.42f, 0.42f, 0.42f, 0.88f));
				PlaceOnCanvas(Card, Preview, FVector2D(19, 0) + Draw.Min, Draw.GetSize(), 2);
			}

			UTextBlock* Name = MakeScreenText(
				*FString::Printf(TEXT("BuildCityCardName_%d"), TypeId),
				23,
				bBuildable ? FLinearColor::Black : FLinearColor::White,
				true,
				ETextJustify::Center);
			ApplyCardFont(Name, 24);
			Name->SetText(CardName(BuildingType));
			PlaceOnCanvas(
				Card,
				Name,
				FVector2D(18.75f, 244.0f),
				FVector2D(294.5f, 40.0f),
				4);

			UTextBlock* Description = MakeScreenText(
				*FString::Printf(
					TEXT("BuildCityCardDescription_%d"),
					TypeId),
				16,
				bBuildable ? FLinearColor::Black : FLinearColor::White,
				false,
				ETextJustify::Center);
			ApplyCardFont(Description, 18);
			Description->SetText(
				ResolveBuildCityDescription(BuildingType));
			Description->SetAutoWrapText(true);
			PlaceOnCanvas(
				Card,
				Description,
				FVector2D(18.75f, 273.0f),
				FVector2D(294.5f, 67.6f),
				4);

			PlaceOnCanvas(
				Card,
				MakeAtlasBorder(
					*FString::Printf(TEXT("BuildCityCardState_%d"), TypeId),
					CommonAtlasPath,
					TaskRowRect,
					CommonAtlasSize,
					bBuildable
						? FVector2D(270.0f, 41.9f)
						: FVector2D(270.0f, 116.4f),
					FMargin(
						23.0f / 74.0f,
						22.0f / 67.0f,
						21.0f / 74.0f,
						13.0f / 67.0f)),
				bBuildable
					? FVector2D(31.0f, 430.5f)
					: FVector2D(31.0f, 347.2f),
				bBuildable
					? FVector2D(270.0f, 41.9f)
					: FVector2D(270.0f, 116.4f),
				5);

			if (bBuildable)
			{
				const TArray<FBuildResourceEntry> Resources = BuildResourceList(BuildingType);
				const int32 ResourceCount = FMath::Min(2, Resources.Num());
				for (int32 ResourceIndex = 0;
					ResourceIndex < ResourceCount;
					++ResourceIndex)
				{
					const float ResourceX =
						129.0f - (ResourceCount - 1) * 55.0f + ResourceIndex * 110.0f;
					PlaceOnCanvas(
						Card,
						MakeAtlasImage(
							*FString::Printf(
								TEXT("BuildCityCardResourceIcon_%d_%d"),
								TypeId,
								ResourceIndex),
							CommonAtlasPath,
							Resources[ResourceIndex].IconRect,
							CommonAtlasSize,
							FVector2D(42.0f)),
						FVector2D(ResourceX, 354.0f),
						FVector2D(42.0f),
						7);
					UTextBlock* Amount = MakeScreenText(
						*FString::Printf(
							TEXT("BuildCityCardResourceAmount_%d_%d"),
							TypeId,
							ResourceIndex),
						15,
						FLinearColor(0.20f, 0.17f, 0.12f, 1.0f),
						true);
					ApplyCardFont(Amount, 20);
					Amount->SetText(
						FText::AsNumber(Resources[ResourceIndex].Amount));
					PlaceOnCanvas(
						Card,
						Amount,
						FVector2D(ResourceX + 42.0f, 362.0f),
						FVector2D(65.0f, 24.0f),
						8);
				}

				// UI_Item_BuildCityBuilding/pl_claim/time/titleCount,titleTime.
				// Language 180304/180303; source bottom-aligned titles above values.
				for (int32 TitleIndex = 0; TitleIndex < 2; ++TitleIndex)
				{
					const bool bTimeTitle = TitleIndex == 1;
					const FName TitleName(*FString::Printf(TEXT("BuildCityCard%sTitle_%d"), bTimeTitle ? TEXT("Duration") : TEXT("Count"), TypeId));
					UTextBlock* Title = MakeScreenText(TitleName, 14,
						FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1), false,
						bTimeTitle ? ETextJustify::Right : ETextJustify::Left);
					ApplyCardFont(Title, 18);
					Title->SetText(bTimeTitle ? NSLOCTEXT("RokBuildCity", "Language180303", "建造时间")
						: NSLOCTEXT("RokBuildCity", "Language180304", "已建造"));
					const FVector2D TitleSize(bTimeTitle ? 155.72f : 136.9f, 38.4f);
					USizeBox* TitleBox = WrapFixedSize(Title,
						*FString::Printf(TEXT("%sLayout"), *TitleName.ToString()), TitleSize);
					CastChecked<USizeBoxSlot>(Title->Slot)->SetVerticalAlignment(VAlign_Bottom);
					PlaceOnCanvas(Card, TitleBox, FVector2D(bTimeTitle ? 139.24f : 35.75f, 387.1f), TitleSize, 8);
				}
				UTextBlock* Count = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardCount_%d"), TypeId),
					17,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Left);
				ApplyCardFont(Count, 18);
				Count->SetText(FText::Format(
					NSLOCTEXT("RokBuildCity", "CountFormat", "{0}/{1}"),
					FText::AsNumber(CurrentCount),
					FText::AsNumber(CurrentLimit)));
				PlaceOnCanvas(
					Card,
					Count,
					FVector2D(41.5f, 439.0f),
					FVector2D(86.0f, 42.0f),
					8);

				UTextBlock* Duration = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardDuration_%d"), TypeId),
					17,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Right);
				ApplyCardFont(Duration, 18);
				Duration->SetText(
					FormatBuildDuration(BuildingType.Level1BuildSeconds));
				PlaceOnCanvas(
					Card,
					Duration,
					FVector2D(106.5f, 439.0f),
					FVector2D(184.0f, 42.0f),
					8);

				URokUnityIndexedButton* CardButton = MakeActionButton(
					*FString::Printf(TEXT("BuildCityCardButton_%d"), TypeId),
					1320 + Index,
					nullptr,
					FIntRect(),
					FIntPoint::ZeroValue,
					FVector2D(318.0f, 501.0f));
				CardButton->SetClickMethod(EButtonClickMethod::PreciseClick);
				CardButton->SetToolTipText(FText::Format(
					NSLOCTEXT(
						"RokBuildCity",
						"BuildableTooltip",
						"建造 {0}"),
					Name->GetText()));
				PlaceOnCanvas(
					Card,
					CardButton,
					FVector2D(7.0f, 0.2f),
					FVector2D(318.0f, 501.0f),
					10);
			}
			else
			{
				UTextBlock* Locked = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardLocked_%d"), TypeId),
					18,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Center);
				ApplyCardFont(Locked, 18);
				Locked->SetText(LockLabel(BuildState, UnlockTownHallLevel));
				Locked->SetAutoWrapText(true);
				PlaceOnCanvas(
					Card,
					Locked,
					FVector2D(46.0f, 373.0f),
					FVector2D(240.0f, 64.0f),
					8);
			}

			BuildingScroll->AddChild(WrapFixedSize(Card,
				*FString::Printf(TEXT("BuildCityCardSize_%d"), TypeId), FVector2D(BuildCityCardStride, CardHeight)));
			if (TypeId == BuildCityGuideTypeId)
			{
				if (auto* GuideTexture = GetGuideTexture())
				{
					auto* Guide = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildCityTaskGuide"));
					Guide->SetBrushFromTexture(GuideTexture);
					Guide->SetRenderTransformAngle(180.0f);
					Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
					const FVector2D GuideSize(49.0f, 149.0f);
					PlaceOnCanvas(Card, Guide, FVector2D((BuildCityCardStride - GuideSize.X) * 0.5f, -GuideSize.Y),
						GuideSize, 10);
				}
			}
		}
	}

	// Unity's pl_buildingImg_side stretches only the strip between the
	// eagle and the base. Keep both ends at their source pixel height.
	const FVector2D RailSize(158.0f, 609.82f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("BuildCitySideDecoration"), BuildCityImageAtlasPath,
			BuildCitySideDecorationRect, BuildCityImageAtlasSize, RailSize,
			FMargin(0.0f, 232.0f / 324.0f, 0.0f, 69.0f / 324.0f)),
		FVector2D(-4.0f, -599.37f), RailSize, 5);

	const FText CategoryNames[] = {
		NSLOCTEXT("RokBuildCity", "EconomyCategory", "经济类"),
		NSLOCTEXT("RokBuildCity", "MilitaryCategory", "军事类"),
		NSLOCTEXT("RokBuildCity", "DecorationCategory", "装饰类"),
	};
	static const FIntRect NormalTabIcons[] = {
		SpriteRect(434, 961, 69, 62),
		SpriteRect(322, 816, 64, 52),
		SpriteRect(162, 491, 78, 50),
	};
	static const FIntRect SelectedTabIcons[] = {
		SpriteRect(322, 870, 75, 67),
		SpriteRect(162, 431, 67, 58),
		SpriteRect(162, 543, 84, 56),
	};
	for (int32 Category = 0; Category < 3; ++Category)
	{
		const bool bSelected = Category == SelectedBuildCityCategory;
		const FVector2D TabPosition(16.5f, -417.0f + Category * 135.27f);
		const FVector2D TabSize(116.5f, 92.0f);
		URokUnityIndexedButton* CategoryButton = MakeActionButton(
			*FString::Printf(TEXT("BuildCityCategory_%d"), Category),
			1300 + Category,
			BuildCityImageAtlasPath,
			bSelected ? BuildCitySelectedTabRect : BuildCityNormalTabRect,
			BuildCityImageAtlasSize,
			TabSize);
		// UI_Item_BuildCityBtn switches iconOn/iconOff immediately.
		CategoryButton->SetPressAnimationEnabled(false);
		PlaceOnCanvas(ModeCanvas, CategoryButton, TabPosition, TabSize, 7);
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("BuildCityCategoryName_%d"), Category),
			15, FLinearColor(0.85f, 0.83f, 0.73f, 1), true, ETextJustify::Center);
		ApplyCardFont(Label, 20);
		Label->SetText(CategoryNames[Category]);
		PlaceOnCanvas(ModeCanvas, Label, TabPosition + FVector2D(0.0f, 96.0f),
			FVector2D(TabSize.X, 32.0f), 8);
		const FIntRect IconRect = bSelected
			? SelectedTabIcons[Category]
			: NormalTabIcons[Category];
		const FVector2D IconSize(
			static_cast<float>(IconRect.Width()),
			static_cast<float>(IconRect.Height()));
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("BuildCityCategoryIcon_%d"), Category),
				BuildCityImageAtlasPath,
				IconRect,
				BuildCityImageAtlasSize,
				IconSize),
			TabPosition + (TabSize - IconSize) * 0.5f,
			IconSize,
			8);
		if (Category < 2 && CategoryBuildableCounts[Category] > 0)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("BuildCityCategoryRed_%d"), Category),
					CommonAtlasPath,
					RedPointRect,
					CommonAtlasSize,
					FVector2D(27.0f, 29.0f)),
				TabPosition + FVector2D(93.0f, -3.0f),
				FVector2D(27.0f, 29.0f),
				9);
			UTextBlock* RedCount = MakeScreenText(
				*FString::Printf(TEXT("BuildCityCategoryRedCount_%d"), Category),
				13,
				FLinearColor::White,
				true,
				ETextJustify::Center);
			RedCount->SetText(FText::AsNumber(
				CategoryBuildableCounts[Category]));
			PlaceOnCanvas(
				ModeCanvas,
				RedCount,
				TabPosition + FVector2D(93.0f, 0.0f),
				FVector2D(27.0f, 22.0f),
				10);
		}
	}

	const TArray<UWidget*> DrawerChildren = ModeCanvas->GetAllChildren();
	BuildCityAnimatedRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildCityAnimatedRoot"));
	BuildCityAnimatedRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* DrawerSlot = ModeCanvas->AddChildToCanvas(BuildCityAnimatedRoot);
	DrawerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	DrawerSlot->SetOffsets(FMargin(0));
	DrawerSlot->SetZOrder(2);
	for (UWidget* Child : DrawerChildren)
	{
		if (Child->GetFName() == TEXT("BuildCityMaskClose")) continue;
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		auto* ChildSlot = BuildCityAnimatedRoot->AddChildToCanvas(Child);
		const FName Name = Child->GetFName();
		const bool bStretch = Name == TEXT("BuildCityListMask")
			|| Name == TEXT("BuildCityDrawerBackground") || Name == TEXT("BuildCityDrawerInputShield");
		ChildSlot->SetAnchors(FAnchors(0, 1, bStretch ? 1.0f : 0.0f, 1));
		ChildSlot->SetOffsets(FMargin(Position.X, Position.Y,
			bStretch ? UnityReferenceWidth - Position.X - Size.X : Size.X, Size.Y));
		ChildSlot->SetZOrder(ZOrder);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok build city screen: category=%d types=%d server_city=%d."),
		SelectedBuildCityCategory,
		VisibleBuildCityTypeIds.Num(), ServerCity && ServerCity->HasAppliedServerCity());
}
