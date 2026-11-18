void URokUnityFeatureScreenWidget::BuildCommanderProgression(const FRokServerHeroEntry* Hero)
{
	auto AddButton = [this](const TCHAR* Name, int32 Action, const FText& Text, FVector2D Position, FVector2D Size, int32 Z)
	{
		auto* Button = MakeActionButton(Name, Action, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize,
			Size, Text, 15, FLinearColor::White, ESlateBrushDrawType::Box, MiniBlueMargin);
		PlaceOnCanvas(ModeCanvas, Button, CapturePoint(Position.X, Position.Y), Size, Z);
		return Button;
	};
	auto AddText = [this](const TCHAR* Name, const FText& Text, FVector2D Position, FVector2D Size, int32 Z)
	{
		auto* Label = MakeScreenText(Name, 17, FLinearColor::White);
		Label->SetText(Text);
		PlaceOnCanvas(ModeCanvas, Label, CapturePoint(Position.X, Position.Y), Size, Z);
	};
	bool bTalentLayoutBuilt = false;
	if (bCommanderTalentOpen)
	{
		UClass* PageClass = ResolveFeaturePageClass(TEXT("CommanderTalentEditor"));
		UClass* RowClass = ResolveFeaturePageClass(TEXT("CommanderTalentNode"));
		URokUiPageWidget* Page = PageClass && RowClass ? CreateWidget<URokUiPageWidget>(this, PageClass) : nullptr;
		UScrollBox* Scroll = Page ? Page->Get<UScrollBox>(TEXT("CommanderTalentNodeScroll")) : nullptr;
		UEditableTextBox* NameInput = Page ? Page->Get<UEditableTextBox>(TEXT("CommanderTalentNameInput")) : nullptr;
		if (Page && Scroll && NameInput && Page->Get<UBorder>(TEXT("CommanderTalentEditor"))
			&& Page->Get<UTextBlock>(TEXT("CommanderTalentEditorWaiting")))
		{
			bTalentLayoutBuilt = true;
			Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			auto* TalentPageSlot = ModeCanvas->AddChildToCanvas(Page);
			TalentPageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			TalentPageSlot->SetOffsets(FMargin(0));
			TalentPageSlot->SetZOrder(18);
			SetFeaturePageText(*Page, TEXT("CommanderTalentEditorTitle"), FText::FromString(TEXT("天赋")));
			SetFeaturePageText(*Page, TEXT("CommanderTalentEditorBackLabel"), FText::FromString(TEXT("返回")));
			BindPageAction(*Page, TEXT("CommanderTalentEditorBack"), 19803);
			const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
			int32 Points = 0;
			CommanderTalentNodes.Reset();
			const bool bLoaded = Hero && Server && Server->GetTalentPage(*Hero, CommanderTalentPage, CommanderTalentNodes, Points);
			UTextBlock* Waiting = Page->Get<UTextBlock>(TEXT("CommanderTalentEditorWaiting"));
			Waiting->SetText(FText::FromString(TEXT("天赋数据待同步")));
			Page->SetPresent(Waiting, !bLoaded);
			const TCHAR* LoadedNames[] = { TEXT("CommanderTalentPage_1"), TEXT("CommanderTalentPage_2"),
				TEXT("CommanderTalentPage_3"), TEXT("CommanderTalentPoints"), TEXT("CommanderTalentNodeScroll"),
				TEXT("CommanderTalentNameInput"), TEXT("CommanderTalentRename"), TEXT("CommanderTalentSwitch"), TEXT("CommanderTalentReset") };
			for (const TCHAR* Name : LoadedNames)
				Page->SetPresent(RokUiFind::Find(Page, Name), bLoaded);
			if (bLoaded)
			{
				auto FindPage = [Hero](int32 Index)
				{
					return Hero->TalentTrees.FindByPredicate([Index](const auto& Tree) { return Tree.Index == Index; });
				};
				const auto* Current = FindPage(CommanderTalentPage);
				for (int32 Index = 1; Index <= 3; ++Index)
				{
					const auto* Tree = FindPage(Index);
					const FString Name = Tree && Tree->bNameKnown && !Tree->Name.IsEmpty()
						? Tree->Name : FString::Printf(TEXT("方案 %d"), Index);
					const FString ButtonName = FString::Printf(TEXT("CommanderTalentPage_%d"), Index);
					SetFeaturePageText(*Page, *(ButtonName + TEXT("Label")),
						FText::FromString(Name + (Hero->TalentIndex == Index ? TEXT(" ★") : TEXT(""))));
					BindPageAction(*Page, *ButtonName, 19800 + Index - 1);
					Page->Get<UButton>(*ButtonName)->SetIsEnabled(Index != CommanderTalentPage);
				}
				SetFeaturePageText(*Page, TEXT("CommanderTalentPoints"),
					FText::FromString(FString::Printf(TEXT("可用天赋点 %d"), Points)));
				// The detached Designer template owns the row extent without changing the node WBP.
				FVector2D TalentRowSize(402, 40);
				UClass* RowLayoutClass = ResolveFeaturePageClass(TEXT("CommanderTalentRowLayout"));
				URokUiPageWidget* RowLayout = RowLayoutClass ? CreateWidget<URokUiPageWidget>(this, RowLayoutClass) : nullptr;
				const UWidget* RowExtent = RowLayout ? RowLayout->Get<UWidget>(TEXT("RowExtent")) : nullptr;
				const auto* RowExtentSlot = RowExtent ? Cast<UCanvasPanelSlot>(RowExtent->Slot) : nullptr;
				if (RowExtentSlot && !RowExtentSlot->GetAutoSize()
					&& RowExtentSlot->GetAnchors().Minimum == RowExtentSlot->GetAnchors().Maximum)
				{
					const FVector2D Size = RowExtentSlot->GetSize();
					if (FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) && Size.X > 0 && Size.Y > 0)
					{
						TalentRowSize = Size;
					}
				}
				int32 HighestLearned = 0;
				for (const auto& Node : CommanderTalentNodes)
					if (Current && Current->TalentIds.Contains(Node.Id)) HighestLearned = FMath::Max(HighestLearned, Node.Level);
				for (int32 Index = 0; Index < CommanderTalentNodes.Num(); ++Index)
				{
					const auto& Node = CommanderTalentNodes[Index];
					const FString Prefix = FString::Printf(TEXT("CommanderTalentNode_%d"), Node.Id);
					URokUiPageWidget* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
					if (!Row) continue;
					Row->LegacyPrefix = Prefix;
					Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					const bool bLearned = Current && Current->TalentIds.Contains(Node.Id);
					UTextBlock* Label = Row->Get<UTextBlock>(TEXT("Label"));
					Label->SetText(FText::FromString(FString::Printf(TEXT("%d. %s"), Node.Level, *FRokUiTextTable::Resolve(Node.NameId))));
					Label->SetToolTipText(FText::FromString(FRokUiTextTable::SanitizeRichText(FRokUiTextTable::Resolve(Node.DescriptionId))));
					Label->SetVisibility(ESlateVisibility::Visible);
					SetFeaturePageText(*Row, TEXT("LearnLabel"), FText::FromString(bLearned ? TEXT("已学习") : TEXT("学习")));
					BindPageAction(*Row, TEXT("Learn"), 19820 + Index);
					Row->Get<UButton>(TEXT("Learn"))->SetIsEnabled(!bLearned && Points > 0 && Node.Level == HighestLearned + 1);
					Scroll->AddChild(WrapFixedSize(Row, *(Prefix + TEXT("Size")), TalentRowSize));
				}
				NameInput->SetHintText(FText::FromString(TEXT("方案名称")));
				if (Current && Current->bNameKnown) NameInput->SetText(FText::FromString(Current->Name));
				SetFeaturePageText(*Page, TEXT("CommanderTalentRenameLabel"), FText::FromString(TEXT("改名")));
				SetFeaturePageText(*Page, TEXT("CommanderTalentSwitchLabel"), FText::FromString(TEXT("启用方案")));
				SetFeaturePageText(*Page, TEXT("CommanderTalentResetLabel"), FText::FromString(TEXT("重置天赋")));
				BindPageAction(*Page, TEXT("CommanderTalentRename"), 19806);
				BindPageAction(*Page, TEXT("CommanderTalentSwitch"), 19804);
				BindPageAction(*Page, TEXT("CommanderTalentReset"), 19805);
				Page->Get<UButton>(TEXT("CommanderTalentSwitch"))->SetIsEnabled(Hero->TalentIndex != CommanderTalentPage);
				Page->Get<UButton>(TEXT("CommanderTalentReset"))->SetIsEnabled(Current && !Current->TalentIds.IsEmpty());
			}
		}
	}
	if (bCommanderTalentOpen && !bTalentLayoutBuilt)
	{
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderTalentEditor"), FLinearColor(0.08f, 0.10f, 0.11f, 1.0f)),
			CapturePoint(950, 148), FVector2D(453, 665), 18);
		AddText(TEXT("CommanderTalentEditorTitle"), FText::FromString(TEXT("天赋")), FVector2D(969, 161), FVector2D(160, 30), 19);
		AddButton(TEXT("CommanderTalentEditorBack"), 19803, FText::FromString(TEXT("返回")),
			FVector2D(1301, 155), FVector2D(85, 38), 19);
		const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
		int32 Points = 0;
		CommanderTalentNodes.Reset();
		if (!Hero || !Server || !Server->GetTalentPage(*Hero, CommanderTalentPage, CommanderTalentNodes, Points))
		{
			AddText(TEXT("CommanderTalentEditorWaiting"), FText::FromString(TEXT("天赋数据待同步")),
				FVector2D(970, 224), FVector2D(410, 32), 19);
		}
		else
		{
			auto FindPage = [Hero](int32 Index)
			{
				return Hero->TalentTrees.FindByPredicate([Index](const auto& Tree) { return Tree.Index == Index; });
			};
			const auto* Current = FindPage(CommanderTalentPage);
			for (int32 Page = 1; Page <= 3; ++Page)
			{
				const auto* Tree = FindPage(Page);
				const FString Name = Tree && Tree->bNameKnown && !Tree->Name.IsEmpty()
					? Tree->Name : FString::Printf(TEXT("方案 %d"), Page);
				auto* Button = AddButton(*FString::Printf(TEXT("CommanderTalentPage_%d"), Page), 19800 + Page - 1,
					FText::FromString(Name + (Hero->TalentIndex == Page ? TEXT(" ★") : TEXT(""))),
					FVector2D(965 + (Page - 1) * 141, 201), FVector2D(133, 37), 19);
				Button->SetIsEnabled(Page != CommanderTalentPage);
			}
			AddText(TEXT("CommanderTalentPoints"),
				FText::FromString(FString::Printf(TEXT("可用天赋点 %d"), Points)),
				FVector2D(970, 254), FVector2D(410, 30), 19);

			auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CommanderTalentNodeScroll"));
			Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
			PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(967, 286), FVector2D(422, 415), 19);
			int32 HighestLearned = 0;
			for (const auto& Node : CommanderTalentNodes)
				if (Current && Current->TalentIds.Contains(Node.Id)) HighestLearned = FMath::Max(HighestLearned, Node.Level);
			for (int32 Index = 0; Index < CommanderTalentNodes.Num(); ++Index)
			{
				const auto& Node = CommanderTalentNodes[Index];
				const FString Prefix = FString::Printf(TEXT("CommanderTalentNode_%d"), Node.Id);
				auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *Prefix);
				const bool bLearned = Current && Current->TalentIds.Contains(Node.Id);
				const FString NodeName = FRokUiTextTable::Resolve(Node.NameId);
				auto* Label = MakeScreenText(*(Prefix + TEXT("Label")), 15, FLinearColor::White);
				Label->SetText(FText::FromString(FString::Printf(TEXT("%d. %s"), Node.Level, *NodeName)));
				Label->SetToolTipText(FText::FromString(FRokUiTextTable::SanitizeRichText(FRokUiTextTable::Resolve(Node.DescriptionId))));
				Label->SetVisibility(ESlateVisibility::Visible);
				PlaceOnCanvas(Row, Label, FVector2D(6, 8), FVector2D(275, 32), 1);
				auto* Learn = MakeActionButton(*(Prefix + TEXT("Learn")), 19820 + Index, CommonAtlasPath,
					MiniBlueButtonRect, CommonAtlasSize, FVector2D(102, 34),
					FText::FromString(bLearned ? TEXT("已学习") : TEXT("学习")),
					14, FLinearColor::White, ESlateBrushDrawType::Box, MiniBlueMargin);
				Learn->SetIsEnabled(!bLearned && Points > 0 && Node.Level == HighestLearned + 1);
				PlaceOnCanvas(Row, Learn, FVector2D(290, 2), FVector2D(102, 34), 2);
				Scroll->AddChild(WrapFixedSize(Row, *(Prefix + TEXT("Size")), FVector2D(402, 40)));
			}
			auto* NameInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("CommanderTalentNameInput"));
			NameInput->SetHintText(FText::FromString(TEXT("方案名称")));
			if (Current && Current->bNameKnown) NameInput->SetText(FText::FromString(Current->Name));
			PlaceOnCanvas(ModeCanvas, NameInput, CapturePoint(970, 712), FVector2D(300, 36), 19);
			AddButton(TEXT("CommanderTalentRename"), 19806, FText::FromString(TEXT("改名")),
				FVector2D(1281, 712), FVector2D(103, 36), 19);
			AddButton(TEXT("CommanderTalentSwitch"), 19804, FText::FromString(TEXT("启用方案")),
				FVector2D(970, 760), FVector2D(190, 38), 19)->SetIsEnabled(Hero->TalentIndex != CommanderTalentPage);
			AddButton(TEXT("CommanderTalentReset"), 19805, FText::FromString(TEXT("重置天赋")),
				FVector2D(1194, 760), FVector2D(190, 38), 19)->SetIsEnabled(Current && !Current->TalentIds.IsEmpty());
		}
	}

	if (CommanderConfirmation.IsSet())
	{
		const auto& Request = *CommanderConfirmation;
		const auto& Cost = CommanderConfirmationCost;
		FString CostText;
		if (Cost.Gems > 0) CostText = FString::Printf(TEXT("消耗 %d 宝石"), Cost.Gems);
		else if (Cost.ItemCount > 0)
		{
			FRokUnityEconomyItem Item;
			const auto* Economy = GetEconomyCatalogSubsystem();
			const FString Name = Economy && Economy->FindItemById(Cost.ItemId, Item) && !Item.DisplayNameZhCn.IsEmpty()
				? Item.DisplayNameZhCn : TEXT("道具");
			CostText = FString::Printf(TEXT("消耗 %d × %s"), Cost.ItemCount, *Name);
		}
		else if (Request.Type == ERokHeroCommand::LearnTalent) CostText = TEXT("消耗 1 天赋点");
		else if (Request.Type == ERokHeroCommand::ResetTalent) CostText = TEXT("重置所选方案的天赋");
		else CostText = TEXT("保存所选方案名称");
		if (Request.Type == ERokHeroCommand::UpgradeSkill) CostText += TEXT("，服务器选择升级技能");
		UClass* ConfirmClass = ResolveFeaturePageClass(TEXT("CommanderConfirm"));
		URokUiPageWidget* ConfirmPage = ConfirmClass ? CreateWidget<URokUiPageWidget>(this, ConfirmClass) : nullptr;
		if (ConfirmPage)
		{
			ConfirmPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			auto* ConfirmSlot = ModeCanvas->AddChildToCanvas(ConfirmPage);
			ConfirmSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			ConfirmSlot->SetOffsets(FMargin(0));
			ConfirmSlot->SetZOrder(23);
			SetFeaturePageText(*ConfirmPage, TEXT("CommanderConfirmCost"), FText::FromString(CostText));
			SetFeaturePageText(*ConfirmPage, TEXT("CommanderConfirmAcceptLabel"), FText::FromString(TEXT("确认")));
			SetFeaturePageText(*ConfirmPage, TEXT("CommanderConfirmCancelLabel"), FText::FromString(TEXT("取消")));
			BindPageAction(*ConfirmPage, TEXT("CommanderConfirmAccept"), 19807);
			BindPageAction(*ConfirmPage, TEXT("CommanderConfirmCancel"), 19808);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderConfirmShade"), FLinearColor(0.06f, 0.07f, 0.08f, 1.0f)),
				CapturePoint(496, 631), FVector2D(435, 142), 23);
			AddText(TEXT("CommanderConfirmCost"), FText::FromString(CostText),
				FVector2D(514, 653), FVector2D(405, 52), 24);
			AddButton(TEXT("CommanderConfirmAccept"), 19807, FText::FromString(TEXT("确认")),
				FVector2D(517, 718), FVector2D(181, 39), 24);
			AddButton(TEXT("CommanderConfirmCancel"), 19808, FText::FromString(TEXT("取消")),
				FVector2D(727, 718), FVector2D(181, 39), 24);
		}
	}
}
