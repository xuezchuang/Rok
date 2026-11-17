void URokUnityFeatureScreenWidget::BuildAllianceFlagEditor()
{
	using namespace RokAllianceFlagLocalPreview;
	const FEntry* Cloth = Find(1, AllianceFlagEditDraft.ClothId);
	const FEntry* ClothColor = Find(2, AllianceFlagEditDraft.ClothColorId);
	const FEntry* Emblem = Find(3, AllianceFlagEditDraft.EmblemId);
	const FEntry* EmblemColor = Find(4, AllianceFlagEditDraft.EmblemColorId);
	const FText IdentityText = FText::FromString(FString::Printf(TEXT("配置 ID  %d / %d / %d / %d"),
		AllianceFlagEditDraft.ClothId, AllianceFlagEditDraft.ClothColorId,
		AllianceFlagEditDraft.EmblemId, AllianceFlagEditDraft.EmblemColorId));
	const auto TabColor = [this](int32 Type)
	{
		return Type == SelectedAllianceFlagType ? FLinearColor(0.48f, 0.36f, 0.14f, 1.0f)
			: FLinearColor(0.22f, 0.21f, 0.20f, 1.0f);
	};
	const auto OptionColor = [this](const FEntry& Entry)
	{
		return AllianceFlagEditDraft.GetId(Entry.Type) == Entry.Id ? FLinearColor(0.67f, 0.48f, 0.13f, 1.0f)
			: FLinearColor(0.20f, 0.20f, 0.19f, 1.0f);
	};

	UClass* SpriteClass = ResolveFeaturePageClass(TEXT("AllianceFlagSpriteOption"));
	UClass* ColorClass = ResolveFeaturePageClass(TEXT("AllianceFlagColorOption"));
	if (URokUiPageWidget* Page = SpriteClass && ColorClass ? MountModePage(TEXT("AllianceFlagEditor")) : nullptr)
	{
		const auto ApplySelectionStyle = [](URokUiPageWidget& Layout, FName BorderName, bool bSelected, FLinearColor LegacyColor)
		{
			auto* Border = Layout.Get<UBorder>(BorderName);
			const FName SelectedName(*(BorderName.ToString() + TEXT("Selected")));
			if (auto* Selected = Cast<UBorder>(RokUiFind::Find(&Layout, SelectedName)))
			{
				Layout.SetPresent(Border, !bSelected);
				Layout.SetPresent(Selected, bSelected);
				if (bSelected) Layout.LegacyNames.Add(SelectedName, Layout.ResolveLegacyName(BorderName).ToString());
			}
			else if (Border)
			{
				// Older assets still display their original runtime selection colors.
				Border->SetBrushColor(LegacyColor);
			}
		};
		UTexture2D* FlagAtlas = LoadUiTexture(AllianceFlagAtlasPath);
		const auto UpdateFlagImage = [this, FlagAtlas](UImage* Image, const FIntRect& Rect, const FLinearColor& Tint)
		{
			if (Image && FlagAtlas)
			{
				FSlateBrush Brush = Image->GetBrush();
				const FSlateBrush AtlasBrush = MakeAtlasBrush(FlagAtlas, Rect, AllianceFlagAtlasSize, Brush.GetImageSize());
				Brush.SetResourceObject(FlagAtlas);
				Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
				Image->SetBrush(Brush);
				Image->SetColorAndOpacity(Tint);
			}
		};
		UImage* ClothImage = Page->Get<UImage>(TEXT("AllianceFlagEditorCloth"));
		UImage* EmblemImage = Page->Get<UImage>(TEXT("AllianceFlagEditorEmblem"));
		const bool bShowFlag = Cloth && ClothColor && Emblem && EmblemColor;
		Page->SetPresent(ClothImage, bShowFlag);
		Page->SetPresent(EmblemImage, bShowFlag);
		if (bShowFlag)
		{
			UpdateFlagImage(ClothImage, Cloth->RealityRect, LinearColor(*ClothColor));
			UpdateFlagImage(EmblemImage, Emblem->RealityRect, LinearColor(*EmblemColor));
		}
		SetFeaturePageText(*Page, TEXT("AllianceFlagEditorSourceIds"), IdentityText);
		for (int32 Type = 1; Type <= 4; ++Type)
		{
			BindPageAction(*Page, *FString::Printf(TEXT("AllianceFlagTab_%d"), Type), 19000 + Type);
			ApplySelectionStyle(*Page, *FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Type),
				Type == SelectedAllianceFlagType, TabColor(Type));
		}
		int32 ItemIndex = 0;
		for (const FEntry& Entry : Rows())
		{
			if (Entry.Type != SelectedAllianceFlagType) continue;
			UCanvasPanel* ItemSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceFlagOptionSlot_%d"), ItemIndex));
			UClass* OptionClass = Entry.bHasColor ? ColorClass : SpriteClass;
			if (URokUiPageWidget* Option = MountPageItem(ItemSlot, OptionClass, TEXT("AllianceFlagItem"), Entry.Id))
			{
				ApplySelectionStyle(*Option, TEXT("Panel"), Entry.Id == AllianceFlagEditDraft.GetId(Entry.Type), OptionColor(Entry));
				if (Entry.bHasColor)
				{
					if (UBorder* Swatch = Option->Get<UBorder>(TEXT("Swatch")))
					{
						Swatch->SetBrushColor(LinearColor(Entry));
					}
				}
				else
				{
					UpdateFlagImage(Option->Get<UImage>(TEXT("Sprite")), Entry.ShowRect, FLinearColor::White);
				}
				SetFeaturePageText(*Option, TEXT("Id"), FText::AsNumber(Entry.Id));
				BindPageAction(*Option, TEXT("Hit"), 19000 + Entry.Type * 100 + ItemIndex);
			}
			++ItemIndex;
		}
		BindPageAction(*Page, TEXT("AllianceFlagEditorCancel"), 19010);
		BindPageAction(*Page, TEXT("AllianceFlagEditorConfirm"), 19011);
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceFlag", "LocalFlagTitle", "编辑联盟旗帜"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceFlagEditorShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f),
		FVector2D(1088.0f, 664.0f), 2);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceFlagEditorPreviewPanel"),
		FLinearColor(0.23f, 0.22f, 0.19f, 1.0f)), CapturePoint(202.0f, 196.0f),
		FVector2D(300.0f, 454.0f), 4);
	UTextBlock* PreviewTitle = MakeScreenText(TEXT("AllianceFlagEditorPreviewTitle"), 22,
		FLinearColor(0.94f, 0.90f, 0.80f, 1.0f), true, ETextJustify::Center);
	PreviewTitle->SetText(NSLOCTEXT("RokAllianceFlag", "Preview", "旗帜预览"));
	PlaceOnCanvas(ModeCanvas, PreviewTitle, CapturePoint(220.0f, 212.0f), FVector2D(264.0f, 40.0f), 5);
	if (Cloth && ClothColor && Emblem && EmblemColor)
	{
		UImage* ClothImage = MakeAtlasImage(TEXT("AllianceFlagEditorCloth"), AllianceFlagAtlasPath,
			Cloth->RealityRect, AllianceFlagAtlasSize, FVector2D(184.0f, 216.0f));
		ClothImage->SetColorAndOpacity(LinearColor(*ClothColor));
		PlaceOnCanvas(ModeCanvas, ClothImage, CapturePoint(260.0f, 276.0f), FVector2D(184.0f, 216.0f), 6);
		UImage* EmblemImage = MakeAtlasImage(TEXT("AllianceFlagEditorEmblem"), AllianceFlagAtlasPath,
			Emblem->RealityRect, AllianceFlagAtlasSize, FVector2D(100.0f));
		EmblemImage->SetColorAndOpacity(LinearColor(*EmblemColor));
		PlaceOnCanvas(ModeCanvas, EmblemImage, CapturePoint(302.0f, 328.0f), FVector2D(100.0f), 7);
	}
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceFlagEditorSourceIds"), 15,
		FLinearColor(0.87f, 0.86f, 0.81f, 1.0f), false, ETextJustify::Center);
	Identity->SetText(IdentityText);
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(212.0f, 535.0f), FVector2D(280.0f, 32.0f), 8);
	UTextBlock* Local = MakeScreenText(TEXT("AllianceFlagEditorLocalOnly"), 16,
		FLinearColor(0.81f, 0.85f, 0.73f, 1.0f), false, ETextJustify::Center);
	Local->SetText(NSLOCTEXT("RokAllianceFlag", "LocalOnly", "仅编辑当前创建表单的本地预览"));
	PlaceOnCanvas(ModeCanvas, Local, CapturePoint(214.0f, 584.0f), FVector2D(278.0f, 48.0f), 8);

	static const FText TypeLabels[] = {
		NSLOCTEXT("RokAllianceFlag", "Cloth", "旗面图案"),
		NSLOCTEXT("RokAllianceFlag", "ClothColor", "旗面底色"),
		NSLOCTEXT("RokAllianceFlag", "Emblem", "联盟徽记"),
		NSLOCTEXT("RokAllianceFlag", "EmblemColor", "徽记颜色"),
	};
	for (int32 Type = 1; Type <= 4; ++Type)
	{
		const FVector2D TabPoint = CapturePoint(536.0f + 164.0f * (Type - 1), 203.0f);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Type),
			TabColor(Type)), TabPoint, FVector2D(154.0f, 54.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFlagTab_%d"), Type),
			19000 + Type, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(154.0f, 54.0f),
			TypeLabels[Type - 1], 18, FLinearColor::White), TabPoint, FVector2D(154.0f, 54.0f), 8);
	}
	int32 ItemIndex = 0;
	for (const FEntry& Entry : Rows())
	{
		if (Entry.Type != SelectedAllianceFlagType) continue;
		const int32 Column = ItemIndex % 8;
		const int32 Row = ItemIndex / 8;
		const FVector2D ItemPoint = CapturePoint(541.0f + 82.0f * Column, 302.0f + 106.0f * Row);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagItemPanel_%d"), Entry.Id),
			OptionColor(Entry)), ItemPoint, FVector2D(76.0f, 76.0f), 5);
		if (Entry.bHasColor)
		{
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagItemSwatch_%d"), Entry.Id),
				LinearColor(Entry)), ItemPoint + FVector2D(8.0f, 8.0f), FVector2D(60.0f, 60.0f), 6);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceFlagItemSprite_%d"), Entry.Id),
				AllianceFlagAtlasPath, Entry.ShowRect, AllianceFlagAtlasSize, FVector2D(64.0f, 64.0f)),
				ItemPoint + FVector2D(6.0f, 6.0f), FVector2D(64.0f, 64.0f), 6);
		}
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFlagItemHit_%d"), Entry.Id),
			19000 + Entry.Type * 100 + ItemIndex, nullptr, FIntRect(), FIntPoint::ZeroValue,
			FVector2D(76.0f, 76.0f)), ItemPoint, FVector2D(76.0f, 76.0f), 8);
		UTextBlock* IdLabel = MakeScreenText(*FString::Printf(TEXT("AllianceFlagItemId_%d"), Entry.Id), 14,
			FLinearColor(0.86f, 0.83f, 0.73f, 1.0f), false, ETextJustify::Center);
		IdLabel->SetText(FText::AsNumber(Entry.Id));
		PlaceOnCanvas(ModeCanvas, IdLabel, ItemPoint + FVector2D(0.0f, 77.0f), FVector2D(76.0f, 24.0f), 8);
		++ItemIndex;
	}
	UTextBlock* Note = MakeScreenText(TEXT("AllianceFlagEditorBoundary"), 16,
		FLinearColor(0.82f, 0.82f, 0.77f, 1.0f), false, ETextJustify::Center);
	Note->SetText(NSLOCTEXT("RokAllianceFlag", "CreateBoundary", "确认仅更新本地预览；创建联盟仍需服务器确认。"));
	PlaceOnCanvas(ModeCanvas, Note, CapturePoint(531.0f, 662.0f), FVector2D(650.0f, 34.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEditorCancel"), 19010,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(150.0f, 54.0f),
		NSLOCTEXT("RokAllianceFlag", "Cancel", "取消"), 19, FLinearColor::White,
		ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(822.0f, 727.0f), FVector2D(150.0f, 54.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEditorConfirm"), 19011,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(160.0f, 59.0f),
		NSLOCTEXT("RokAllianceFlag", "Confirm", "确定"), 21, FLinearColor::White),
		CapturePoint(1000.0f, 724.0f), FVector2D(160.0f, 59.0f), 8);
}
