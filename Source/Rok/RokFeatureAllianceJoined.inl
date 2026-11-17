void URokUnityFeatureScreenWidget::BuildAllianceJoined()
{
	const FText Title = SelectedAlliancePage == 1 ? NSLOCTEXT("RokAlliance", "MembersTitle", "联盟成员")
		: SelectedAlliancePage == 2 ? NSLOCTEXT("RokAlliance", "SettingsTitle", "联盟设置")
		: NSLOCTEXT("RokAlliance", "JoinedTitle", "联盟");
	const auto OfficeName = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.Name.IsEmpty() ? NSLOCTEXT("RokAlliance", "OfficeNameUnavailable", "官职名称配置不可用")
			: FText::FromString(Office.Name);
	};
	const auto OfficeDescription = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.Description.IsEmpty() ? NSLOCTEXT("RokAlliance", "OfficeDescriptionUnavailable", "职位说明配置不可用")
			: FText::FromString(Office.Description);
	};
	const auto OfficeBonus = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.AddDescription.IsEmpty() ? NSLOCTEXT("RokAlliance", "OfficeBonusUnavailable", "配置加成说明不可用")
			: FText::FromString(FString::Printf(TEXT("配置加成说明：%s"), *Office.AddDescription));
	};
	const auto ReadOfficeRows = [this]() -> const TArray<FRokUnityAllianceOfficeReadonlyRow>*
	{
		auto* Catalog = GetGameplayCatalogSubsystem();
		return Catalog && Catalog->LoadAllianceOfficeReadonly() ? &Catalog->GetAllianceOfficeReadonlyRows() : nullptr;
	};
	UClass* OfficeRowClass = ResolveFeaturePageClass(TEXT("AllianceJoinedOfficeRow"));
	if (const URokUiPageWidget* Page = OfficeRowClass ? MountModePage(TEXT("AllianceJoined")) : nullptr)
	{
		SetFeaturePageText(*Page, TEXT("FeatureFrameTitle"), Title);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const bool bSelected = Index == SelectedAlliancePage;
			const FString Tab = FString::Printf(TEXT("AlliancePageTab_%d"), Index);
			Page->SetShown(Page->Get<UWidget>(*(Tab + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UWidget>(*(Tab + TEXT("Normal"))), !bSelected);
			Page->SetPresent(Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceJoinedPage_%d"), Index)), bSelected);
			BindPageAction(*Page, *FString::Printf(TEXT("AlliancePageHit_%d"), Index), 630 + Index);
		}
		if (SelectedAlliancePage == 0)
		{
			for (int32 Index = 0; Index < 8; ++Index)
				BindPageAction(*Page, *FString::Printf(TEXT("AllianceFeatureHit_%d"), Index), 650 + Index);
			BindPageAction(*Page, TEXT("AllianceMessageBoard"), 659);
			BindPageAction(*Page, TEXT("AllianceMail"), 660);
		}
		else if (SelectedAlliancePage == 1)
		{
			const auto* Rows = ReadOfficeRows();
			const bool bHasOfficeRows = Rows && Rows->Num() == 4;
			Page->SetPresent(Page->Get<UWidget>(TEXT("AllianceOfficeDefinitionsUnavailable")), !bHasOfficeRows);
			for (int32 Index = 0; Index < 4; ++Index)
			{
				auto* Host = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceJoinedOfficeSlot_%d"), Index));
				Page->SetPresent(Host, bHasOfficeRows);
				if (!bHasOfficeRows) continue;
				const auto& Office = (*Rows)[Index];
				if (auto* Row = MountPageItem(Host, OfficeRowClass, TEXT("AllianceOffice"), Office.Id))
				{
					const bool bSelected = Index == SelectedAllianceOfficeIndex;
					Row->SetPresent(Row->Get<UWidget>(TEXT("SelectedRow")), bSelected);
					Row->SetPresent(Row->Get<UWidget>(TEXT("Row")), !bSelected);
					if (bSelected) Row->LegacyNames.Add(TEXT("SelectedRow"), TEXT("{Prefix}Row{Suffix}"));
					const FIntRect IconRect = AllianceOfficeIconRect(Office.IconSprite);
					auto* Icon = Row->Get<UImage>(TEXT("Icon"));
					const bool bHasIcon = IconRect.Width() > 0 && IconRect.Height() > 0;
					Row->SetPresent(Icon, bHasIcon);
					if (Icon && bHasIcon)
					{
						FSlateBrush Brush = Icon->GetBrush();
						const FSlateBrush Atlas = MakeAtlasBrush(LoadUiTexture(AllianceAtlasPath), IconRect, AllianceAtlasSize, Brush.GetImageSize());
						Brush.SetResourceObject(Atlas.GetResourceObject());
						Brush.SetUVRegion(static_cast<FBox2f>(Atlas.GetUVRegion()));
						Icon->SetBrush(Brush);
					}
					SetFeaturePageText(*Row, TEXT("Name"), OfficeName(Office));
					SetFeaturePageText(*Row, TEXT("Description"), OfficeDescription(Office));
					SetFeaturePageText(*Row, TEXT("Bonus"), OfficeBonus(Office));
					BindPageAction(*Row, TEXT("DefinitionHit"), 640 + Index);
				}
			}
			BindPageAction(*Page, TEXT("AllianceMemberOfficeReferenceOpen"), 635);
		}
		else
		{
			for (int32 Index = 0; Index < 6; ++Index)
				BindPageAction(*Page, *FString::Printf(TEXT("AllianceSettingHit_%d"), Index), 670 + Index);
		}
		return;
	}

	static const FText PageLabels[] = {
		NSLOCTEXT("RokAlliance", "OverviewPage", "总览"),
		NSLOCTEXT("RokAlliance", "MemberPage", "成员"),
		NSLOCTEXT("RokAlliance", "SettingsPage", "设置"),
	};
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		Title,
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceJoinedShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);

	for (int32 Page = 0; Page < 3; ++Page)
	{
		const bool bSelected = Page == SelectedAlliancePage;
		UCanvasPanel* Tab = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("AlliancePageTab_%d"), Page));
		PlaceOnCanvas(Tab, MakeAtlasImage(*FString::Printf(TEXT("AlliancePageTabBg_%d"), Page), CommonAtlasPath, bSelected ? SideTabSelectedRect : SideTabDarkRect, CommonAtlasSize, FVector2D(88.0f, 92.0f)), FVector2D::ZeroVector, FVector2D(88.0f, 92.0f), 0);
		PlaceOnCanvas(Tab, MakeAtlasImage(*FString::Printf(TEXT("AlliancePageIcon_%d"), Page), PageAtlasPath, AlliancePageIconRects[Page][bSelected ? 1 : 0], PageAtlasSize, FVector2D(48.0f)), FVector2D(20.0f, 10.0f), FVector2D(48.0f), 1);
		UTextBlock* PageText = MakeScreenText(*FString::Printf(TEXT("AlliancePageText_%d"), Page), 14, bSelected ? FLinearColor(0.93f, 0.94f, 0.91f, 1.0f) : FLinearColor(0.62f, 0.62f, 0.59f, 1.0f), true, ETextJustify::Center);
		PageText->SetText(PageLabels[Page]);
		PlaceOnCanvas(Tab, PageText, FVector2D(4.0f, 61.0f), FVector2D(80.0f, 24.0f), 2);
		PlaceOnCanvas(Tab, MakeActionButton(*FString::Printf(TEXT("AlliancePageHit_%d"), Page), 630 + Page, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(88.0f, 92.0f)), FVector2D::ZeroVector, FVector2D(88.0f, 92.0f), 5);
		PlaceOnCanvas(ModeCanvas, Tab, CapturePoint(1216.0f, 210.0f + Page * 112.0f), FVector2D(88.0f, 92.0f), 10);
	}

	// The joined preview only exists as a fixture: alliance membership is
	// server-authoritative, so the window must say it is not account state.
	UTextBlock* JoinedFixtureTag = MakeScreenText(TEXT("AllianceJoinedFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false);
	JoinedFixtureTag->SetText(NSLOCTEXT("RokAlliance", "JoinedFixtureTag", "联盟 · 本地参考态（非账号状态）"));
	PlaceOnCanvas(ModeCanvas, JoinedFixtureTag, CapturePoint(195.0f, 748.0f), FVector2D(420.0f, 26.0f), 6);

	if (SelectedAlliancePage == 0)
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("AllianceNoticePanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(555.0f, 145.0f), CommonPanelMargin), CapturePoint(195.0f, 175.0f), FVector2D(555.0f, 145.0f), 4);
		UTextBlock* NoticeTitle = MakeScreenText(TEXT("AllianceNoticeTitle"), 20, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
		NoticeTitle->SetText(NSLOCTEXT("RokAlliance", "NoticeTitle", "联盟公告"));
		PlaceOnCanvas(ModeCanvas, NoticeTitle, CapturePoint(215.0f, 188.0f), FVector2D(180.0f, 32.0f), 6);
		UTextBlock* Notice = MakeScreenText(TEXT("AllianceJoinedNotice"), 18, FLinearColor(0.88f, 0.88f, 0.85f, 1.0f), false);
		Notice->SetText(NSLOCTEXT("RokAlliance", "JoinedNoticeText", "团结互助，共同建设联盟领土。欢迎每一位盟友！"));
		Notice->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, Notice, CapturePoint(215.0f, 230.0f), FVector2D(510.0f, 68.0f), 6);

		static const int32 FeatureOrder[] = {3, 0, 6, 2, 7, 5, 1, 4};
		static const FText FeatureLabels[] = {
			NSLOCTEXT("RokAlliance", "AllianceWar", "联盟战争"),
			NSLOCTEXT("RokAlliance", "AllianceTerritory", "联盟领土"),
			NSLOCTEXT("RokAlliance", "HolyLand", "圣地"),
			NSLOCTEXT("RokAlliance", "AllianceHelpFeature", "联盟帮助"),
			NSLOCTEXT("RokAlliance", "AllianceDepot", "联盟仓库"),
			NSLOCTEXT("RokAlliance", "AllianceTechnology", "联盟科技"),
			NSLOCTEXT("RokAlliance", "AllianceGift", "联盟礼物"),
			NSLOCTEXT("RokAlliance", "AllianceStore", "联盟商店"),
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FeatureLabels); ++Index)
		{
			const float X = 194.0f + (Index % 4) * 142.0f;
			const float Y = 338.0f + (Index / 4) * 178.0f;
			PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(*FString::Printf(TEXT("AllianceFeatureBg_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(132.0f, 158.0f), CommonPanelMargin), CapturePoint(X, Y), FVector2D(132.0f, 158.0f), 4);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceFeatureIcon_%d"), Index), AllianceAtlasPath, AllianceFeatureIconRects[FeatureOrder[Index]], AllianceAtlasSize, FVector2D(104.0f)), CapturePoint(X + 14.0f, Y + 10.0f), FVector2D(104.0f), 5);
			UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("AllianceFeatureLabel_%d"), Index), 16, FLinearColor(0.93f, 0.92f, 0.87f, 1.0f), true, ETextJustify::Center);
			Label->SetText(FeatureLabels[Index]);
			PlaceOnCanvas(ModeCanvas, Label, CapturePoint(X + 5.0f, Y + 121.0f), FVector2D(122.0f, 28.0f), 6);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFeatureHit_%d"), Index), 650 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(132.0f, 158.0f)), CapturePoint(X, Y), FVector2D(132.0f, 158.0f), 8);
		}

		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceSummaryPanel"), FLinearColor(0.23f, 0.22f, 0.19f, 0.98f)), CapturePoint(784.0f, 175.0f), FVector2D(420.0f, 570.0f), 4);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceFlagBacking"), AllianceAtlasPath, AllianceFlagBackingRect, AllianceAtlasSize, FVector2D(170.0f, 208.0f)), CapturePoint(910.0f, 188.0f), FVector2D(170.0f, 208.0f), 5);
		UImage* JoinedFlagCloth = MakeAtlasImage(TEXT("AllianceJoinedFlagCloth"), AllianceFlagAtlasPath, AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(126.0f, 148.0f));
		JoinedFlagCloth->SetColorAndOpacity(FLinearColor(0.14f, 0.36f, 0.72f, 1.0f));
		PlaceOnCanvas(ModeCanvas, JoinedFlagCloth, CapturePoint(932.0f, 215.0f), FVector2D(126.0f, 148.0f), 6);
		UImage* JoinedFlagEmblem = MakeAtlasImage(TEXT("AllianceJoinedFlagEmblem"), AllianceFlagAtlasPath, AllianceFlagEmblemRect, AllianceFlagAtlasSize, FVector2D(70.0f));
		JoinedFlagEmblem->SetColorAndOpacity(FLinearColor(0.96f, 0.76f, 0.18f, 1.0f));
		PlaceOnCanvas(ModeCanvas, JoinedFlagEmblem, CapturePoint(960.0f, 250.0f), FVector2D(70.0f), 7);

		UTextBlock* AllianceName = MakeScreenText(TEXT("AllianceJoinedName"), 25, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true, ETextJustify::Center);
		AllianceName->SetText(NSLOCTEXT("RokAlliance", "JoinedAllianceName", "[SNOW] 雪原同盟"));
		PlaceOnCanvas(ModeCanvas, AllianceName, CapturePoint(810.0f, 405.0f), FVector2D(368.0f, 42.0f), 7);
		UTextBlock* Summary = MakeScreenText(TEXT("AllianceJoinedSummary"), 19, FLinearColor(0.90f, 0.90f, 0.86f, 1.0f), false);
		Summary->SetText(NSLOCTEXT("RokAlliance", "JoinedSummary", "战力：6,100\n盟主：snow\n领土：0\n礼物等级：1\n成员：1/50"));
		PlaceOnCanvas(ModeCanvas, Summary, CapturePoint(840.0f, 462.0f), FVector2D(310.0f, 160.0f), 7);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMessageBoard"), 659, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "MessageBoard", "留言板"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(825.0f, 664.0f), FVector2D(138.0f, 54.0f), 8);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMail"), 660, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "AllianceMail", "联盟邮件"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(1015.0f, 664.0f), FVector2D(138.0f, 54.0f), 8);
	}
	else if (SelectedAlliancePage == 1)
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("AllianceLeaderCard"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1040.0f, 145.0f), CommonPanelMargin), CapturePoint(198.0f, 175.0f), FVector2D(1040.0f, 145.0f), 4);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceLeaderRank"), AllianceAtlasPath, SpriteRect(898, 969, 100, 100), AllianceAtlasSize, FVector2D(92.0f)), CapturePoint(220.0f, 199.0f), FVector2D(92.0f), 5);
		UTextBlock* Leader = MakeScreenText(TEXT("AllianceLeaderName"), 24, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
		Leader->SetText(NSLOCTEXT("RokAlliance", "LeaderPreview", "R5  snow（盟主）"));
		PlaceOnCanvas(ModeCanvas, Leader, CapturePoint(335.0f, 205.0f), FVector2D(360.0f, 38.0f), 6);
		UTextBlock* LeaderStats = MakeScreenText(TEXT("AllianceLeaderStats"), 18, FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
		LeaderStats->SetText(NSLOCTEXT("RokAlliance", "LeaderStatsPreview", "战力 6,100    击杀 0    在线"));
		PlaceOnCanvas(ModeCanvas, LeaderStats, CapturePoint(335.0f, 255.0f), FVector2D(430.0f, 30.0f), 6);
		const auto* OfficeRows = ReadOfficeRows();
		UTextBlock* DefinitionNote = MakeScreenText(TEXT("AllianceOfficeReferenceNote"), 16,
			FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
		DefinitionNote->SetText(NSLOCTEXT("RokAlliance", "OfficeDefinitionBoundary",
			"官职配置参考 · 不代表当前任命、人数、权限或已生效加成"));
		PlaceOnCanvas(ModeCanvas, DefinitionNote, CapturePoint(212.0f, 318.0f),
			FVector2D(1000.0f, 24.0f), 6);
		if (OfficeRows && OfficeRows->Num() == 4)
		{
			for (int32 Index = 0; Index < OfficeRows->Num(); ++Index)
			{
				const FRokUnityAllianceOfficeReadonlyRow& Office = (*OfficeRows)[Index];
				const float Y = 348.0f + Index * 91.0f;
				const bool bSelected = Index == SelectedAllianceOfficeIndex;
				PlaceOnCanvas(ModeCanvas,
					MakeSolidBorder(*FString::Printf(TEXT("AllianceOfficeRow_%d"), Office.Id),
						bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
						: FLinearColor(0.16f, 0.16f, 0.16f, 0.96f)),
					CapturePoint(198.0f, Y), FVector2D(1040.0f, 78.0f), 4);
				const FIntRect IconRect = AllianceOfficeIconRect(Office.IconSprite);
				if (IconRect.Width() > 0 && IconRect.Height() > 0)
				{
					PlaceOnCanvas(ModeCanvas,
						MakeAtlasImage(*FString::Printf(TEXT("AllianceOfficeIcon_%d"), Office.Id),
							AllianceAtlasPath, IconRect, AllianceAtlasSize, FVector2D(58.0f)),
						CapturePoint(210.0f, Y + 10.0f), FVector2D(58.0f), 5);
				}
				UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeName_%d"), Office.Id),
					20, FLinearColor(0.91f, 0.84f, 0.65f, 1.0f), true);
				Name->SetText(OfficeName(Office));
				PlaceOnCanvas(ModeCanvas, Name, CapturePoint(282.0f, Y + 5.0f),
					FVector2D(880.0f, 28.0f), 6);
				UTextBlock* Description = MakeScreenText(
					*FString::Printf(TEXT("AllianceOfficeDescription_%d"), Office.Id), 15,
					FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
				Description->SetText(OfficeDescription(Office));
				PlaceOnCanvas(ModeCanvas, Description, CapturePoint(282.0f, Y + 37.0f),
					FVector2D(445.0f, 28.0f), 6);
				UTextBlock* Bonus = MakeScreenText(
					*FString::Printf(TEXT("AllianceOfficeBonus_%d"), Office.Id), 14,
					FLinearColor(0.75f, 0.82f, 0.66f, 1.0f), false);
				Bonus->SetAutoWrapText(true);
				Bonus->SetText(OfficeBonus(Office));
				PlaceOnCanvas(ModeCanvas, Bonus, CapturePoint(740.0f, Y + 31.0f),
					FVector2D(474.0f, 42.0f), 6);
				PlaceOnCanvas(ModeCanvas, MakeActionButton(
					*FString::Printf(TEXT("AllianceOfficeDefinitionHit_%d"), Office.Id), 640 + Index,
					nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(1040.0f, 78.0f)),
					CapturePoint(198.0f, Y), FVector2D(1040.0f, 78.0f), 8);
			}
		}
		else
		{
			UTextBlock* Unavailable = MakeScreenText(TEXT("AllianceOfficeDefinitionsUnavailable"), 18,
				FLinearColor(0.82f, 0.80f, 0.76f, 1.0f), false, ETextJustify::Center);
			Unavailable->SetText(NSLOCTEXT("RokAlliance", "OfficeDefinitionsUnavailable",
				"官职静态配置暂不可用；未读取任命或权限状态"));
			PlaceOnCanvas(ModeCanvas, Unavailable, CapturePoint(250.0f, 440.0f),
				FVector2D(900.0f, 38.0f), 6);
		}
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMemberOfficeReferenceOpen"), 635,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(214.0f, 46.0f),
			NSLOCTEXT("RokAlliance", "MemberOfficeReferenceOpen", "官职定义说明"), 17,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
			CapturePoint(1095.0f, 735.0f), FVector2D(214.0f, 46.0f), 7);
	}
	else
	{
		static const FText SettingsLabels[] = {
			NSLOCTEXT("RokAlliance", "AllianceSettings", "联盟设置"),
			NSLOCTEXT("RokAlliance", "AllianceList", "联盟列表"),
			NSLOCTEXT("RokAlliance", "AllianceRank", "联盟排行"),
			NSLOCTEXT("RokAlliance", "ApplicationReview", "申请审核"),
			NSLOCTEXT("RokAlliance", "InviteMember", "邀请加入"),
			NSLOCTEXT("RokAlliance", "LeaveAlliance", "退出联盟"),
		};
		static const FIntRect SettingsIcons[] = {
			SpriteRect(163, 952, 145, 145),
			SpriteRect(1, 49, 145, 145),
			SpriteRect(1, 196, 145, 145),
			SpriteRect(1, 343, 145, 145),
			SpriteRect(1, 490, 145, 145),
			SpriteRect(1, 637, 145, 145),
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SettingsLabels); ++Index)
		{
			const float X = 250.0f + (Index % 3) * 305.0f;
			const float Y = 205.0f + (Index / 3) * 275.0f;
			PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(*FString::Printf(TEXT("AllianceSettingBg_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(230.0f, 220.0f), CommonPanelMargin), CapturePoint(X, Y), FVector2D(230.0f, 220.0f), 4);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceSettingIcon_%d"), Index), AllianceAtlasPath, SettingsIcons[Index], AllianceAtlasSize, FVector2D(128.0f)), CapturePoint(X + 51.0f, Y + 20.0f), FVector2D(128.0f), 5);
			UTextBlock* Setting = MakeScreenText(*FString::Printf(TEXT("AllianceSettingLabel_%d"), Index), 20, FLinearColor(0.92f, 0.90f, 0.84f, 1.0f), true, ETextJustify::Center);
			Setting->SetText(SettingsLabels[Index]);
			PlaceOnCanvas(ModeCanvas, Setting, CapturePoint(X + 10.0f, Y + 168.0f), FVector2D(210.0f, 36.0f), 6);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceSettingHit_%d"), Index), 670 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(230.0f, 220.0f)), CapturePoint(X, Y), FVector2D(230.0f, 220.0f), 8);
		}
	}
}
