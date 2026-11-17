void URokUnityFeatureScreenWidget::BuildAllianceTerritoryReference()
{
	const auto& Groups = RokAllianceTerritoryReadonly::GetGroups();
	const auto& Rows = RokAllianceTerritoryReadonly::GetRows();
	const auto* Selected = SelectedAllianceTerritoryType > 0 ? RokAllianceTerritoryReadonly::FindRow(SelectedAllianceTerritoryType) : nullptr;
	const auto SourceText = [](int32 LocalizationId)
	{
		return FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(LocalizationId));
	};
	const auto FormatDetailIconState = [](const RokAllianceTerritoryReadonly::FRow& Definition)
	{
		return Definition.bSourceIconMissing
			? NSLOCTEXT("RokAllianceTerritory", "DetailMalformedSourceIcon", "源图标缺失")
			: (Definition.SourceIconReference[0] == 0
				? NSLOCTEXT("RokAllianceTerritory", "DetailNoSourceIcon", "源未配置图标")
				: NSLOCTEXT("RokAllianceTerritory", "DetailPhysicalIconPending", "图标未接入"));
	};
	const auto FormatResourceIconState = [](const RokAllianceTerritoryReadonly::FRow& Definition)
	{
		return Definition.bSourceIconMissing
			? NSLOCTEXT("RokAllianceTerritory", "ResourceMalformedSourceIcon", "源图标缺失")
			: NSLOCTEXT("RokAllianceTerritory", "ResourcePhysicalIconPending", "图标未接入");
	};
	const auto BuildingIconState = [](const RokAllianceTerritoryReadonly::FRow& Definition)
	{
		return Definition.bSourceIconMissing
			? NSLOCTEXT("RokAllianceTerritory", "MalformedSourceIcon", "源图标缺失")
			: (Definition.SourceIconReference[0] == 0
				? NSLOCTEXT("RokAllianceTerritory", "NoSourceIcon", "源未配置图标")
				: NSLOCTEXT("RokAllianceTerritory", "PhysicalIconPending", "图标未接入"));
	};
	const FText SelectionText = SelectedAllianceTerritoryType > 0
		? FText::FromString(FString::Printf(TEXT("已选静态定义 type %d · 无联盟实际拥有记录"), SelectedAllianceTerritoryType))
		: NSLOCTEXT("RokAllianceTerritory", "NoSelection", "点击分组展开配置定义；实际领土记录未同步");
	if (const URokUiPageWidget* Page = MountModePage(TEXT("AllianceTerritoryReference")))
	{
		BindPageAction(*Page, TEXT("AllianceTerritoryReferenceBack"), 695);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceTerritoryOverview")), !Selected);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("AllianceTerritoryDetail")), Selected != nullptr);
		if (Selected)
		{
			BindPageAction(*Page, TEXT("AllianceTerritoryReturnToDefinitions"), 19630);
			SetFeaturePageText(*Page, TEXT("AllianceTerritoryDefinitionName"), SourceText(Selected->NameLocalizationId));
			SetFeaturePageText(*Page, TEXT("AllianceTerritoryDefinitionIconState"), FormatDetailIconState(*Selected));
			SetFeaturePageText(*Page, TEXT("AllianceTerritoryDefinitionDescription"), SourceText(Selected->DescriptionLocalizationId));
			const bool bHasRules = Selected->DetailLocalizationId > 0;
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceTerritoryDefinitionRules")), bHasRules);
			if (bHasRules)
			{
				SetFeaturePageText(*Page, TEXT("AllianceTerritoryDefinitionRules"), SourceText(Selected->DetailLocalizationId));
			}
			return;
		}
		SetFeaturePageText(*Page, TEXT("AllianceTerritoryReferenceSelection"), SelectionText);
		for (const auto& Group : Groups)
		{
			const int32 Index = Group.Index;
			const bool bExpanded = ExpandedAllianceTerritoryGroup == Index;
			SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Index), SourceText(Group.TitleLocalizationId));
			SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Index), SourceText(Group.HeaderLocalizationId));
			SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryExpand_%d"), Index), FText::FromString(bExpanded ? TEXT("−") : TEXT("+")));
			Page->SetPresent(Page->Get<UBorder>(*FString::Printf(TEXT("AllianceTerritoryGroupBg_%d"), Index)), !bExpanded);
			Page->SetPresent(Page->Get<UBorder>(*FString::Printf(TEXT("AllianceTerritoryGroupBgExpanded_%d"), Index)), bExpanded);
			Page->SetPresent(Page->Get<USizeBox>(*FString::Printf(TEXT("AllianceTerritoryExpandedSize_%d"), Index)), bExpanded);
			BindPageAction(*Page, *FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Index), 19620 + Index);
		}
		for (const auto& Definition : Rows)
		{
			if (Definition.GroupIndex != ExpandedAllianceTerritoryGroup) continue;
			const int32 Type = Definition.UnityType;
			if (Definition.GroupIndex == 3)
			{
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Type), SourceText(Definition.NameLocalizationId));
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Type), FormatResourceIconState(Definition));
			}
			else
			{
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryName_%d"), Type), SourceText(Definition.NameLocalizationId));
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryType_%d"), Type),
					FText::FromString(FString::Printf(TEXT("配置 type %d"), Type)));
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Type), BuildingIconState(Definition));
				SetFeaturePageText(*Page, *FString::Printf(TEXT("AllianceTerritoryDescription_%d"), Type), SourceText(Definition.DescriptionLocalizationId));
			}
			BindPageAction(*Page, *FString::Printf(TEXT("AllianceTerritorySelect_%d"), Type), 19600 + Type);
		}
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceTerritory", "ReferenceTitle", "联盟领土静态配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTerritoryReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceTerritoryReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceTerritory", "ReferenceBoundary",
		"仅显示12种静态配置；联盟实际领土、拥有数量、建筑状态、坐标、产出与权限未同步。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 156.0f), FVector2D(1025.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTerritoryReferenceBack"), 695,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceTerritory", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 738.0f), FVector2D(138.0f, 54.0f), 7);
	if (Selected)
	{
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTerritoryReturnToDefinitions"), 19630,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(164.0f, 54.0f),
			NSLOCTEXT("RokAllianceTerritory", "ReturnToDefinitions", "返回配置列表"), 18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
			CapturePoint(985.0f, 194.0f), FVector2D(164.0f, 54.0f), 7);
		UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailScroll"));
		DetailScroll->SetOrientation(Orient_Vertical);
		DetailScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		PlaceOnCanvas(ModeCanvas, DetailScroll, CapturePoint(198.0f, 256.0f), FVector2D(990.0f, 470.0f), 6);
		USizeBox* DetailExtent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailExtent"));
		DetailExtent->SetWidthOverride(970.0f);
		DetailExtent->SetHeightOverride(1050.0f);
		DetailScroll->AddChild(DetailExtent);
		UCanvasPanel* DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailCanvas"));
		DetailExtent->AddChild(DetailCanvas);
		UTextBlock* DetailName = MakeScreenText(TEXT("AllianceTerritoryDefinitionName"), 24,
			FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
		DetailName->SetText(SourceText(Selected->NameLocalizationId));
		PlaceOnCanvas(DetailCanvas, DetailName, FVector2D(12.0f, 4.0f), FVector2D(930.0f, 50.0f), 2);
		UTextBlock* DetailIconState = MakeScreenText(TEXT("AllianceTerritoryDefinitionIconState"), 17,
			FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
		DetailIconState->SetText(FormatDetailIconState(*Selected));
		PlaceOnCanvas(DetailCanvas, DetailIconState, FVector2D(12.0f, 64.0f), FVector2D(930.0f, 40.0f), 2);
		UTextBlock* DetailDescription = MakeScreenText(TEXT("AllianceTerritoryDefinitionDescription"), 17,
			FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
		DetailDescription->SetText(SourceText(Selected->DescriptionLocalizationId));
		DetailDescription->SetAutoWrapText(true);
		PlaceOnCanvas(DetailCanvas, DetailDescription, FVector2D(12.0f, 124.0f), FVector2D(930.0f, 210.0f), 2);
		if (Selected->DetailLocalizationId > 0)
		{
			UTextBlock* DetailRules = MakeScreenText(TEXT("AllianceTerritoryDefinitionRules"), 16,
				FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			DetailRules->SetText(SourceText(Selected->DetailLocalizationId));
			DetailRules->SetAutoWrapText(true);
			PlaceOnCanvas(DetailCanvas, DetailRules, FVector2D(12.0f, 354.0f), FVector2D(930.0f, 680.0f), 2);
		}
		return;
	}
	float ContentHeight = 0.0f;
	for (const RokAllianceTerritoryReadonly::FGroup& Group : Groups)
	{
		ContentHeight += 86.0f + 10.0f; // Unity GetItemSize header and ListView spacing.
		if (ExpandedAllianceTerritoryGroup == Group.Index)
		{
			ContentHeight += (Group.Index == 3 ? 290.0f : 370.0f) + 10.0f;
		}
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AllianceTerritoryReferenceScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(194.0f, 210.0f), FVector2D(1052.0f, 518.0f), 4);
	USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AllianceTerritoryReferenceExtent"));
	Extent->SetWidthOverride(1052.0f);
	Extent->SetHeightOverride(ContentHeight);
	Scroll->AddChild(Extent);
	UCanvasPanel* List = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AllianceTerritoryReferenceList"));
	List->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Extent->AddChild(List);
	float Y = 0.0f;
	for (const RokAllianceTerritoryReadonly::FGroup& Group : Groups)
	{
		const bool bExpanded = ExpandedAllianceTerritoryGroup == Group.Index;
		UCanvasPanel* Header = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTerritoryGroup_%d"), Group.Index));
		PlaceOnCanvas(List, Header, FVector2D(0.0f, Y), FVector2D(1052.0f, 86.0f), 1);
		PlaceOnCanvas(Header, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritoryGroupBg_%d"), Group.Index),
			bExpanded ? FLinearColor(0.31f, 0.27f, 0.20f, 0.98f) : FLinearColor(0.19f, 0.19f, 0.18f, 0.98f)),
			FVector2D::ZeroVector, FVector2D(1052.0f, 86.0f), 1);
		UTextBlock* Title = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Group.Index),
			22, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
		Title->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Group.TitleLocalizationId)));
		PlaceOnCanvas(Header, Title, FVector2D(18.0f, 9.0f), FVector2D(570.0f, 32.0f), 2);
		UTextBlock* Summary = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Group.Index),
			15, FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false);
		Summary->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Group.HeaderLocalizationId)));
		Summary->SetAutoWrapText(true);
		PlaceOnCanvas(Header, Summary, FVector2D(18.0f, 45.0f), FVector2D(975.0f, 35.0f), 2);
		UTextBlock* Expand = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryExpand_%d"), Group.Index),
			26, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true, ETextJustify::Center);
		Expand->SetText(FText::FromString(bExpanded ? TEXT("−") : TEXT("+")));
		PlaceOnCanvas(Header, Expand, FVector2D(1000.0f, 22.0f), FVector2D(44.0f, 42.0f), 3);
		PlaceOnCanvas(Header, MakeActionButton(*FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Group.Index),
			19620 + Group.Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(1052.0f, 86.0f)),
			FVector2D::ZeroVector, FVector2D(1052.0f, 86.0f), 5);
		Y += 96.0f;
		if (!bExpanded) continue;
		const bool bResourceSummary = Group.Index == 3;
		const float RowHeight = bResourceSummary ? 290.0f : 370.0f;
		const float RowWidth = bResourceSummary ? 930.0f : 1030.0f;
		// Unity building row GridLayoutGroup: four 225x300 cells, 10 horizontal spacing.
		const float SlotWidth = 225.0f;
		UCanvasPanel* RowPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTerritoryRow_%d"), Group.Index));
		PlaceOnCanvas(List, RowPanel, FVector2D(bResourceSummary ? 61.0f : 11.0f, Y),
			FVector2D(RowWidth, RowHeight), 1);
		PlaceOnCanvas(RowPanel, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritoryRowBg_%d"), Group.Index),
			FLinearColor(0.13f, 0.13f, 0.12f, 0.98f)), FVector2D::ZeroVector,
			FVector2D(RowWidth, bResourceSummary ? RowHeight : RowHeight - 10.0f), 1);
		if (bResourceSummary)
		{
			// Unity prefab_index=1 is a 930x290 summary row, not building cells.
			UTextBlock* ConfigText = MakeScreenText(TEXT("AllianceTerritoryResourceDefinitions"), 16,
				FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			ConfigText->SetText(NSLOCTEXT("RokAllianceTerritory", "ResourceDefinitionReference", "静态资源配置定义 · 非实际占领资源点"));
			PlaceOnCanvas(RowPanel, ConfigText, FVector2D(12.0f, 8.0f), FVector2D(906.0f, 40.0f), 2);
			int32 ResourceSlotIndex = 0;
			for (const RokAllianceTerritoryReadonly::FRow& Definition : Rows)
			{
				if (Definition.GroupIndex != Group.Index) continue;
				const FVector2D ResourceSlotPos(ResourceSlotIndex * (SlotWidth + 10.0f), 55.0f);
				UTextBlock* ResourceName = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Definition.UnityType),
					17, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
				ResourceName->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.NameLocalizationId)));
				ResourceName->SetAutoWrapText(true);
				PlaceOnCanvas(RowPanel, ResourceName, ResourceSlotPos + FVector2D(9.0f, 12.0f), FVector2D(207.0f, 80.0f), 3);
				UTextBlock* ResourceIconState = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Definition.UnityType),
					14, FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
				ResourceIconState->SetText(FormatResourceIconState(Definition));
				PlaceOnCanvas(RowPanel, ResourceIconState, ResourceSlotPos + FVector2D(9.0f, 107.0f), FVector2D(207.0f, 35.0f), 3);
				PlaceOnCanvas(RowPanel, MakeActionButton(*FString::Printf(TEXT("AllianceTerritorySelect_%d"), Definition.UnityType),
					19600 + Definition.UnityType, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(SlotWidth, 182.0f)),
					ResourceSlotPos, FVector2D(SlotWidth, 182.0f), 5);
				++ResourceSlotIndex;
			}
			Y += RowHeight + 10.0f;
			continue;
		}
		int32 SlotIndex = 0;
		for (const RokAllianceTerritoryReadonly::FRow& Definition : Rows)
		{
			if (Definition.GroupIndex != Group.Index) continue;
			UCanvasPanel* DetailIconSlot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("AllianceTerritorySlot_%d"), Definition.UnityType));
			PlaceOnCanvas(RowPanel, DetailIconSlot, FVector2D(SlotIndex * (SlotWidth + 10.0f), 0.0f),
				FVector2D(SlotWidth, 300.0f), 2);
			PlaceOnCanvas(DetailIconSlot, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritorySlotBg_%d"), Definition.UnityType),
				SelectedAllianceTerritoryType == Definition.UnityType
				? FLinearColor(0.30f, 0.25f, 0.16f, 0.98f) : FLinearColor(0.19f, 0.19f, 0.17f, 0.98f)),
				FVector2D::ZeroVector, FVector2D(SlotWidth, 300.0f), 1);
			UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryName_%d"), Definition.UnityType),
				18, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
			Name->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.NameLocalizationId)));
			Name->SetAutoWrapText(true);
			PlaceOnCanvas(DetailIconSlot, Name, FVector2D(10.0f, 12.0f), FVector2D(SlotWidth - 25.0f, 50.0f), 2);
			UTextBlock* Identity = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryType_%d"), Definition.UnityType),
				14, FLinearColor(0.76f, 0.80f, 0.76f, 1.0f), false);
			Identity->SetText(FText::FromString(FString::Printf(TEXT("配置 type %d"), Definition.UnityType)));
			PlaceOnCanvas(DetailIconSlot, Identity, FVector2D(10.0f, 70.0f), FVector2D(SlotWidth - 25.0f, 24.0f), 2);
			UTextBlock* IconState = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Definition.UnityType),
				14, FLinearColor(0.79f, 0.74f, 0.65f, 1.0f), false);
			IconState->SetText(BuildingIconState(Definition));
			PlaceOnCanvas(DetailIconSlot, IconState, FVector2D(10.0f, 102.0f), FVector2D(SlotWidth - 25.0f, 24.0f), 2);
			UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryDescription_%d"), Definition.UnityType),
				14, FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			Description->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.DescriptionLocalizationId)));
			Description->SetAutoWrapText(true);
			PlaceOnCanvas(DetailIconSlot, Description, FVector2D(10.0f, 139.0f),
				FVector2D(SlotWidth - 25.0f, RowHeight - 173.0f), 2);
			PlaceOnCanvas(DetailIconSlot, MakeActionButton(*FString::Printf(TEXT("AllianceTerritorySelect_%d"), Definition.UnityType),
				19600 + Definition.UnityType, nullptr, FIntRect(), FIntPoint::ZeroValue,
				FVector2D(SlotWidth, 300.0f)),
				FVector2D::ZeroVector, FVector2D(SlotWidth, 300.0f), 5);
			++SlotIndex;
		}
		Y += RowHeight + 10.0f;
	}
	UTextBlock* Selection = MakeScreenText(TEXT("AllianceTerritoryReferenceSelection"), 15,
		FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
	Selection->SetText(SelectionText);
	PlaceOnCanvas(ModeCanvas, Selection, CapturePoint(198.0f, 744.0f), FVector2D(840.0f, 42.0f), 6);
}
