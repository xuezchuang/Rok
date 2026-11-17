void URokUnityFeatureScreenWidget::BuildAllianceGiftReference()
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	const bool bHasDefinitions = Catalog && Catalog->LoadAllianceGiftReadonly();
	const int32 Level = SelectedAllianceGiftConfiguredLevel;
	const FRokUnityAllianceGiftLevelReadonlyRow* LevelRow = bHasDefinitions ? Catalog->FindAllianceGiftLevelReadonly(Level) : nullptr;
	const FText ToggleText = bAllianceGiftSpecialDefinitions
		? NSLOCTEXT("RokAllianceGift", "RegularDefinition", "常规等级定义")
		: NSLOCTEXT("RokAllianceGift", "SpecialDefinition", "特殊类型定义");
	const FText LevelText = bAllianceGiftSpecialDefinitions
		? NSLOCTEXT("RokAllianceGift", "SpecialLevelLabel", "特殊类型 · 配置等级 0")
		: FText::FromString(FString::Printf(TEXT("配置等级 %d / 50"), Level));
	const FText ThresholdText = !bAllianceGiftSpecialDefinitions && LevelRow
		? FText::FromString(FString::Printf(TEXT("该等级配置阈值 exp=%d · 宝箱配置 ID=%d"),
			LevelRow->ExpThreshold, LevelRow->TreasureId))
		: NSLOCTEXT("RokAllianceGift", "SpecialThreshold", "特殊类型无等级阈值；下列仅为静态定义");
	const auto FormatDefinition = [](const FRokUnityAllianceGiftRewardReadonlyRow& Row)
	{
		return FText::FromString(FString::Printf(
			TEXT("类型 %d · 定义 ID %d · 奖励包 ID %d · 礼物积分 %d · 钥匙点数 %d"),
			Row.GiftType, Row.Id, Row.RewardPackageId, Row.GiftPoint, Row.KeyPoint));
	};
	const auto LogDefinitions = [this, Level](int32 RowCount)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance gift static definitions: configured_level=%d special=%d rows=%d authority=local_reference claim_state=0."),
			Level, bAllianceGiftSpecialDefinitions ? 1 : 0, RowCount);
	};
	UClass* RowClass = ResolveFeaturePageClass(TEXT("AllianceGiftReferenceRow"));
	if (const URokUiPageWidget* Page = RowClass ? MountModePage(TEXT("AllianceGiftReference")) : nullptr)
	{
		BindPageAction(*Page, TEXT("AllianceGiftReferenceBack"), 686);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceGiftReferenceDefinitions")), bHasDefinitions);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceGiftReferenceUnavailable")), !bHasDefinitions);
		if (!bHasDefinitions) return;

		BindPageAction(*Page, TEXT("AllianceGiftReferencePrevious"), 687);
		BindPageAction(*Page, TEXT("AllianceGiftReferenceNext"), 688);
		BindPageAction(*Page, TEXT("AllianceGiftReferenceSpecial"), 689);
		SetFeaturePageText(*Page, TEXT("AllianceGiftReferenceSpecialLabel"), ToggleText);
		SetFeaturePageText(*Page, TEXT("AllianceGiftReferenceLevel"), LevelText);
		SetFeaturePageText(*Page, TEXT("AllianceGiftReferenceThreshold"), ThresholdText);
		int32 RowIndex = 0;
		for (const FRokUnityAllianceGiftRewardReadonlyRow& Definition : Catalog->GetAllianceGiftRewardReadonlyRows())
		{
			if (Definition.GiftLevel != (bAllianceGiftSpecialDefinitions ? 0 : Level)) continue;
			auto* Host = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceGiftReferenceSlot_%d"), RowIndex));
			if (auto* Row = MountPageItem(Host, RowClass, TEXT("AllianceGiftReference"), Definition.Id))
			{
				const bool bOdd = RowIndex % 2 != 0;
				Row->SetPresent(Row->Get<UBorder>(TEXT("Row")), !bOdd);
				Row->SetPresent(Row->Get<UBorder>(TEXT("OddRow")), bOdd);
				if (bOdd) Row->LegacyNames.Add(TEXT("OddRow"), TEXT("{Prefix}Row{Suffix}"));
				SetFeaturePageText(*Row, TEXT("Definition"), FormatDefinition(Definition));
			}
			++RowIndex;
		}
		LogDefinitions(RowIndex);
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceGift", "ReferenceTitle", "联盟礼物配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceGiftReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceGiftReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceGift", "ReferenceBoundary",
		"仅显示静态配置定义；当前联盟等级、积分、钥匙、礼物列表和领取状态均需服务器数据。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 157.0f), FVector2D(1030.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceBack"), 686,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceGift", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(837.0f, 198.0f), FVector2D(138.0f, 54.0f), 7);
	if (!bHasDefinitions)
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceGiftReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAllianceGift", "ReferenceUnavailable",
			"礼物静态配置不可用；未生成礼物或领取状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferencePrevious"), 687,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(115.0f, 48.0f),
		NSLOCTEXT("RokAllianceGift", "PreviousLevel", "上一级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(198.0f, 200.0f), FVector2D(115.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceNext"), 688,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(115.0f, 48.0f),
		NSLOCTEXT("RokAllianceGift", "NextLevel", "下一级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(534.0f, 200.0f), FVector2D(115.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceSpecial"), 689,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(180.0f, 48.0f),
		ToggleText, 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1000.0f, 200.0f), FVector2D(180.0f, 48.0f), 7);
	UTextBlock* LevelLabel = MakeScreenText(TEXT("AllianceGiftReferenceLevel"), 21,
		FLinearColor(0.92f, 0.86f, 0.63f, 1.0f), true, ETextJustify::Center);
	LevelLabel->SetText(LevelText);
	PlaceOnCanvas(ModeCanvas, LevelLabel, CapturePoint(322.0f, 208.0f), FVector2D(205.0f, 34.0f), 6);
	UTextBlock* Threshold = MakeScreenText(TEXT("AllianceGiftReferenceThreshold"), 16,
		FLinearColor(0.78f, 0.82f, 0.75f, 1.0f), false);
	Threshold->SetText(ThresholdText);
	PlaceOnCanvas(ModeCanvas, Threshold, CapturePoint(198.0f, 267.0f), FVector2D(1000.0f, 30.0f), 6);
	int32 RowIndex = 0;
	for (const FRokUnityAllianceGiftRewardReadonlyRow& Row : Catalog->GetAllianceGiftRewardReadonlyRows())
	{
		if (Row.GiftLevel != (bAllianceGiftSpecialDefinitions ? 0 : Level)) continue;
		const float Y = 306.0f + RowIndex * 51.0f;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceGiftReferenceRow_%d"), Row.Id),
			RowIndex % 2 == 0 ? FLinearColor(0.19f, 0.18f, 0.16f, 0.98f)
			: FLinearColor(0.24f, 0.22f, 0.19f, 0.98f)),
			CapturePoint(197.0f, Y), FVector2D(1020.0f, 45.0f), 4);
		UTextBlock* Detail = MakeScreenText(
			*FString::Printf(TEXT("AllianceGiftReferenceDefinition_%d"), Row.Id), 16,
			FLinearColor(0.89f, 0.87f, 0.81f, 1.0f), false);
		Detail->SetText(FormatDefinition(Row));
		PlaceOnCanvas(ModeCanvas, Detail, CapturePoint(218.0f, Y + 8.0f),
			FVector2D(980.0f, 30.0f), 6);
		++RowIndex;
	}
	LogDefinitions(RowIndex);
}
