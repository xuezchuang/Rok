void URokUnityFeatureScreenWidget::BuildAllianceTechnologyReference()
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	const bool bDefinitionsLoaded = Catalog && Catalog->LoadAllianceTechnologyReadonly();
	const auto* Rows = bDefinitionsLoaded ? &Catalog->GetAllianceTechnologyReadonlyRows() : nullptr;
	TArray<const FRokUnityAllianceTechnologyReadonlyRow*> Definitions;
	if (Rows)
	{
		for (const FRokUnityAllianceTechnologyReadonlyRow& Row : *Rows)
		{
			if (Row.CategoryType == SelectedAllianceTechnologyCategory && Row.LevelKey == 1) Definitions.Add(&Row);
		}
	}
	if (!Definitions.IsEmpty() && !Definitions.ContainsByPredicate([this](const FRokUnityAllianceTechnologyReadonlyRow* Row)
	{
		return Row->StudyType == SelectedAllianceTechnologyStudyType;
	}))
	{
		SelectedAllianceTechnologyStudyType = Definitions[0]->StudyType;
		SelectedAllianceTechnologyLevelKey = 1;
	}
	const auto* Selected = Definitions.IsEmpty() ? nullptr : Rows->FindByPredicate([this](const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		return Row.StudyType == SelectedAllianceTechnologyStudyType && Row.LevelKey == SelectedAllianceTechnologyLevelKey;
	});
	const auto ListIdentity = [](const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("studyType %d · 配置等级 1"), Row.StudyType));
	};
	const auto DetailIdentity = [](const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(TEXT("源配置行 ID %d · studyType %d · 配置等级 %d"), Row.Id, Row.StudyType, Row.LevelKey));
	};
	const auto DetailPrerequisites = [](const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		FString Prerequisites = TEXT("配置前置 ID：");
		if (Row.Prerequisites.IsEmpty()) Prerequisites += TEXT("无");
		for (int32 Index = 0; Index < Row.Prerequisites.Num(); ++Index)
		{
			if (Index > 0) Prerequisites += TEXT("、");
			Prerequisites += FString::FromInt(Row.Prerequisites[Index].SourceRowId);
		}
		return FText::FromString(Prerequisites);
	};
	const TCHAR* Atlas1 = TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1");
	const TCHAR* Atlas2 = TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2");
	const auto LoadAtlas = [](const TCHAR* Path, const FIntPoint& ExpectedSize) -> UTexture2D*
	{
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, Path);
		if (!Texture) return nullptr;
		// The catalog UVs refer to source dimensions, including while the RHI is compiling.
		const FIntPoint ImportedSize = Texture->GetImportedSize();
		const FIntPoint ImageSize = ImportedSize.X > 0 && ImportedSize.Y > 0
			? ImportedSize : FIntPoint(Texture->GetSizeX(), Texture->GetSizeY());
		return ImageSize == ExpectedSize ? Texture : nullptr;
	};
	UTexture2D* Icon1 = Definitions.IsEmpty() ? nullptr : LoadAtlas(Atlas1, FIntPoint(512, 2048));
	UTexture2D* Icon2 = Definitions.IsEmpty() ? nullptr : LoadAtlas(Atlas2, FIntPoint(1024, 2048));
	const auto ResolveIcon = [Icon1, Icon2](const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		return Row.IconAtlas == TEXT("research_icon1") ? Icon1 : Icon2;
	};
	const auto ApplyIcon = [this, &ResolveIcon](const URokUiPageWidget& Owner, FName ImageName, const FRokUnityAllianceTechnologyReadonlyRow& Row)
	{
		auto* Image = Owner.Get<UImage>(ImageName);
		UTexture2D* Texture = ResolveIcon(Row);
		Owner.SetPresent(Image, Texture != nullptr);
		if (!Image || !Texture) return;
		FSlateBrush Brush = Image->GetBrush();
		const FSlateBrush Atlas = MakeAtlasBrush(Texture, Row.IconRect, Row.AtlasSize, Brush.GetImageSize());
		Brush.SetResourceObject(Texture);
		Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
		Image->SetBrush(Brush);
	};

	UClass* RowClass = ResolveFeaturePageClass(TEXT("AllianceTechnologyReferenceRow"));
	if (const URokUiPageWidget* Page = RowClass ? MountModePage(TEXT("AllianceTechnologyReference")) : nullptr)
	{
		BindPageAction(*Page, TEXT("AllianceTechnologyReferenceBack"), 680);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceTechnologyReferenceDefinitions")), bDefinitionsLoaded);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceTechnologyReferenceUnavailable")), !bDefinitionsLoaded);
		if (!bDefinitionsLoaded) return;
		for (int32 Category = 1; Category <= 3; ++Category)
		{
			const bool bSelected = Category == SelectedAllianceTechnologyCategory;
			const FString ButtonName = FString::Printf(TEXT("AllianceTechnologyCategory_%d"), Category);
			BindPageAction(*Page, *ButtonName, 680 + Category);
			Page->SetShown(Page->Get<UTextBlock>(*(ButtonName + TEXT("Label"))), !bSelected);
			Page->SetShown(Page->Get<UTextBlock>(*(ButtonName + TEXT("SelectedLabel"))), bSelected);
		}
		auto* Scroll = Page->Get<UScrollBox>(TEXT("AllianceTechnologyDefinitionScroll"));
		Page->SetPresent(Scroll, !Definitions.IsEmpty());
		if (Scroll)
		{
			for (const auto* Definition : Definitions)
			{
				auto* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
				if (!Row) continue;
				const auto* Content = Row->Get<UCanvasPanel>(TEXT("Definition"));
				const auto* Layout = Content ? Cast<UCanvasPanelSlot>(Content->Slot) : nullptr;
				if (!Layout) continue;
				Row->LegacyPrefix = TEXT("AllianceTechnology");
				Row->LegacySuffix = FString::Printf(TEXT("_%d"), Definition->StudyType);
				Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				const bool bSelected = Definition->StudyType == SelectedAllianceTechnologyStudyType;
				Row->SetPresent(Row->Get<UBorder>(TEXT("DefinitionBg")), !bSelected);
				Row->SetPresent(Row->Get<UBorder>(TEXT("SelectedBg")), bSelected);
				if (bSelected) Row->LegacyNames.Add(TEXT("SelectedBg"), TEXT("{Prefix}DefinitionBg{Suffix}"));
				ApplyIcon(*Row, TEXT("Icon"), *Definition);
				SetFeaturePageText(*Row, TEXT("Name"), FText::FromString(Definition->Title));
				SetFeaturePageText(*Row, TEXT("Id"), ListIdentity(*Definition));
				BindPageAction(*Row, TEXT("DefinitionHit"), 17000 + Definition->StudyType);
				Scroll->AddChild(WrapFixedSize(Row, *FString::Printf(TEXT("AllianceTechnologyRowSize_%d"), Definition->StudyType), Layout->GetSize()));
			}
		}
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceTechnologyReferenceDetail")), Selected != nullptr);
		if (!Selected) return;
		ApplyIcon(*Page, TEXT("AllianceTechnologyDetailIcon"), *Selected);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceTechnologyIconUnavailable")), ResolveIcon(*Selected) == nullptr);
		SetFeaturePageText(*Page, TEXT("AllianceTechnologyDetailTitle"), FText::FromString(Selected->Title));
		SetFeaturePageText(*Page, TEXT("AllianceTechnologyDetailIdentity"), DetailIdentity(*Selected));
		SetFeaturePageText(*Page, TEXT("AllianceTechnologyDetailDescription"), FText::FromString(Selected->Description));
		SetFeaturePageText(*Page, TEXT("AllianceTechnologyDetailPrerequisites"), DetailPrerequisites(*Selected));
		BindPageAction(*Page, TEXT("AllianceTechnologyConfiguredPrevious"), 684);
		BindPageAction(*Page, TEXT("AllianceTechnologyConfiguredNext"), 685);
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAlliance", "TechnologyReferenceTitle", "联盟科技配置参考"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceTechnologyReferenceBoundary"), 17,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAlliance", "TechnologyReferenceBoundary",
		"仅显示 AllianceStudy 静态配置；当前研究等级、进度、捐献与解锁由服务器决定"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(196.0f, 158.0f), FVector2D(1040.0f, 28.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyReferenceBack"), 680,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAlliance", "TechnologyReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 740.0f), FVector2D(138.0f, 54.0f), 7);
	if (!bDefinitionsLoaded)
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceTechnologyReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAlliance", "TechnologyReferenceUnavailable",
			"联盟科技静态配置不可用；未读取账户研究状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	static const FText CategoryLabels[] = {
		NSLOCTEXT("RokAlliance", "TechnologyDevelop", "发展"),
		NSLOCTEXT("RokAlliance", "TechnologyTerritory", "领土"),
		NSLOCTEXT("RokAlliance", "TechnologyWar", "战争"),
	};
	for (int32 Category = 1; Category <= 3; ++Category)
	{
		const bool bSelected = SelectedAllianceTechnologyCategory == Category;
		PlaceOnCanvas(ModeCanvas,
			MakeActionButton(*FString::Printf(TEXT("AllianceTechnologyCategory_%d"), Category),
				680 + Category, CommonAtlasPath, MiniBlueButtonRect,
				CommonAtlasSize, FVector2D(130.0f, 48.0f), CategoryLabels[Category - 1], 19,
				bSelected ? FLinearColor(0.18f, 0.13f, 0.05f, 1.0f)
				: FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box,
				MiniBlueMargin),
			CapturePoint(198.0f + (Category - 1) * 138.0f, 194.0f), FVector2D(130.0f, 48.0f), 7);
	}
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyListPanel"),
		FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)),
		CapturePoint(194.0f, 252.0f), FVector2D(414.0f, 474.0f), 3);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyDetailPanel"),
		FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
		CapturePoint(622.0f, 194.0f), FVector2D(620.0f, 532.0f), 3);
	if (Definitions.IsEmpty()) return;
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
		TEXT("AllianceTechnologyDefinitionScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(201.0f, 261.0f), FVector2D(400.0f, 457.0f), 4);
	USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		TEXT("AllianceTechnologyDefinitionExtent"));
	Extent->SetWidthOverride(400.0f);
	Extent->SetHeightOverride(Definitions.Num() * 70.0f);
	Scroll->AddChild(Extent);
	UCanvasPanel* List = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		TEXT("AllianceTechnologyDefinitionList"));
	List->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Extent->AddChild(List);
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FRokUnityAllianceTechnologyReadonlyRow& Row = *Definitions[Index];
		const bool bSelected = Row.StudyType == SelectedAllianceTechnologyStudyType;
		UCanvasPanel* Item = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTechnologyDefinition_%d"), Row.StudyType));
		PlaceOnCanvas(List, Item, FVector2D(0.0f, Index * 70.0f), FVector2D(400.0f, 70.0f), 1);
		PlaceOnCanvas(Item, MakeSolidBorder(*FString::Printf(TEXT("AllianceTechnologyDefinitionBg_%d"), Row.StudyType),
			bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
			: FLinearColor(0.18f, 0.18f, 0.17f, 0.98f)),
			FVector2D(0.0f, 0.0f), FVector2D(392.0f, 66.0f), 1);
		if (ResolveIcon(Row))
		{
			PlaceOnCanvas(Item, MakeAtlasImage(*FString::Printf(TEXT("AllianceTechnologyIcon_%d"), Row.StudyType),
				Row.IconAtlas == TEXT("research_icon1") ? Atlas1 : Atlas2,
				Row.IconRect, Row.AtlasSize, FVector2D(52.0f)),
				FVector2D(7.0f, 7.0f), FVector2D(52.0f), 2);
		}
		UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("AllianceTechnologyName_%d"), Row.StudyType),
			18, FLinearColor(0.91f, 0.84f, 0.65f, 1.0f), true);
		Label->SetText(FText::FromString(Row.Title));
		PlaceOnCanvas(Item, Label, FVector2D(69.0f, 7.0f), FVector2D(300.0f, 27.0f), 3);
		UTextBlock* Identity = MakeScreenText(*FString::Printf(TEXT("AllianceTechnologyId_%d"), Row.StudyType),
			14, FLinearColor(0.80f, 0.80f, 0.76f, 1.0f), false);
		Identity->SetText(ListIdentity(Row));
		PlaceOnCanvas(Item, Identity, FVector2D(69.0f, 38.0f), FVector2D(300.0f, 22.0f), 3);
		PlaceOnCanvas(Item, MakeActionButton(*FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), Row.StudyType),
			17000 + Row.StudyType, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(392.0f, 66.0f)),
			FVector2D::ZeroVector, FVector2D(392.0f, 66.0f), 5);
	}
	if (!Selected) return;
	UTextBlock* Title = MakeScreenText(TEXT("AllianceTechnologyDetailTitle"), 27,
		FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
	Title->SetText(FText::FromString(Selected->Title));
	PlaceOnCanvas(ModeCanvas, Title, CapturePoint(754.0f, 216.0f), FVector2D(450.0f, 42.0f), 6);
	if (ResolveIcon(*Selected))
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceTechnologyDetailIcon"),
			Selected->IconAtlas == TEXT("research_icon1") ? Atlas1 : Atlas2,
			Selected->IconRect, Selected->AtlasSize, FVector2D(108.0f)),
			CapturePoint(640.0f, 209.0f), FVector2D(108.0f), 6);
	}
	else
	{
		UTextBlock* NoIcon = MakeScreenText(TEXT("AllianceTechnologyIconUnavailable"), 15,
			FLinearColor(0.84f, 0.76f, 0.63f, 1.0f), false, ETextJustify::Center);
		NoIcon->SetText(NSLOCTEXT("RokAlliance", "TechnologyIconUnavailable", "源图标资源不可用"));
		PlaceOnCanvas(ModeCanvas, NoIcon, CapturePoint(640.0f, 232.0f), FVector2D(108.0f, 52.0f), 6);
	}
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceTechnologyDetailIdentity"), 18,
		FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false);
	Identity->SetText(DetailIdentity(*Selected));
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(648.0f, 333.0f), FVector2D(570.0f, 30.0f), 6);
	UTextBlock* Description = MakeScreenText(TEXT("AllianceTechnologyDetailDescription"), 19,
		FLinearColor(0.90f, 0.89f, 0.84f, 1.0f), false);
	Description->SetText(FText::FromString(Selected->Description));
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(ModeCanvas, Description, CapturePoint(648.0f, 378.0f), FVector2D(565.0f, 114.0f), 6);
	UTextBlock* Precondition = MakeScreenText(TEXT("AllianceTechnologyDetailPrerequisites"), 17,
		FLinearColor(0.78f, 0.82f, 0.72f, 1.0f), false);
	Precondition->SetText(DetailPrerequisites(*Selected));
	Precondition->SetAutoWrapText(true);
	PlaceOnCanvas(ModeCanvas, Precondition, CapturePoint(648.0f, 518.0f), FVector2D(565.0f, 60.0f), 6);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyConfiguredPrevious"), 684,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 48.0f),
		NSLOCTEXT("RokAlliance", "TechnologyConfiguredPrevious", "上一配置级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(648.0f, 646.0f), FVector2D(138.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyConfiguredNext"), 685,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 48.0f),
		NSLOCTEXT("RokAlliance", "TechnologyConfiguredNext", "下一配置级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(806.0f, 646.0f), FVector2D(138.0f, 48.0f), 7);
}
