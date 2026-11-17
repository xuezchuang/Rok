void URokUnityFeatureScreenWidget::BindServerTaskPageActions(const URokUiPageWidget& Page,
	const FRokServerTaskSnapshot& Snapshot)
{
	const auto CountFinished = [](const auto& Rows)
	{
		return Algo::CountIf(Rows, [](const auto& Row) { return Row.State == ERokServerTaskState::Finished; });
	};
	const int32 RedCounts[] = {
		Snapshot.bChapterClaimable ? 1 : CountFinished(Snapshot.ChapterTasks),
		CountFinished(Snapshot.MainTasks) + CountFinished(Snapshot.SideTasks),
		CountFinished(Snapshot.DailyTasks) + CountFinished(Snapshot.DailyMilestones)
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		BindPageAction(Page, *FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index);
		const int32 Count = RedCounts[Index];
		Page.SetPresent(Page.Get<UImage>(*FString::Printf(TEXT("TaskPageBadge_%d"), Index)), Count > 0);
		if (auto* Label = Page.Get<UTextBlock>(*FString::Printf(TEXT("TaskPageBadgeCount_%d"), Index)))
		{
			Page.SetPresent(Label, Count > 0);
			Label->SetText(Count > 99 ? FText::FromString(TEXT("99+")) : FText::AsNumber(Count));
			if (Count > 99)
			{
				Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 7));
				if (auto* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot))
				{
					LabelSlot->SetPosition(LabelSlot->GetPosition() + FVector2D(0, 4));
				}
			}
		}
	}
}

bool URokUnityFeatureScreenWidget::TryBuildServerDailyTaskPage(const FRokServerTaskSnapshot& Snapshot)
{
	UClass* RowClass = ResolveFeaturePageClass(TEXT("TaskDailyServerRow"));
	UClass* RewardClass = ResolveFeaturePageClass(TEXT("TaskDailyServerReward"));
	UClass* MilestoneClass = ResolveFeaturePageClass(TEXT("TaskDailyServerMilestone"));
	UClass* ClaimMilestoneClass = ResolveFeaturePageClass(TEXT("TaskDailyServerClaimMilestone"));
	if (!RowClass || !RewardClass || !MilestoneClass || !ClaimMilestoneClass || Snapshot.DailyMilestones.Num() > 5)
	{
		return false;
	}
	auto* Page = MountModePage(TEXT("TaskDailyServer"));
	if (!Page)
	{
		return false;
	}
	auto* Scroll = Page->Get<UScrollBox>(TEXT("TaskDailyCatalogScroll"));
	if (!Scroll)
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		return false;
	}

	BindServerTaskPageActions(*Page, Snapshot);
	SetFeaturePageText(*Page, TEXT("TaskDailyCatalogHeader"), Snapshot.bLoaded && Snapshot.bDailyLoaded
		? NSLOCTEXT("RokFeature", "TaskServerDailyHeader", "每日任务")
		: NSLOCTEXT("RokFeature", "TaskServerDailyWaiting", "等待服务器同步每日任务"));
	if (Snapshot.DailyMilestones.IsEmpty())
	{
		Page->SetPresent(Page->Get<USizeBox>(TEXT("TaskDailyMilestonesSize")), false);
	}
	else
	{
		SetFeaturePageText(*Page, TEXT("TaskDailyPoints"), FText::Format(
			NSLOCTEXT("RokFeature", "TaskDailyActivePoints", "今日活跃度 {0}"), FText::AsNumber(Snapshot.DailyActivePoints)));
		for (int32 Index = 0; Index < Snapshot.DailyMilestones.Num(); ++Index)
		{
			const auto& Milestone = Snapshot.DailyMilestones[Index];
			const bool bClaimable = Milestone.State == ERokServerTaskState::Finished;
			const TCHAR* Status = bClaimable ? TEXT("领取")
				: Milestone.State == ERokServerTaskState::Received ? TEXT("已领取") : TEXT("未达成");
			auto* Host = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("TaskDailyMilestoneSlot_%d"), Index));
			const auto* Item = MountPageItem(Host, bClaimable ? ClaimMilestoneClass : MilestoneClass,
				TEXT("TaskDaily"), Milestone.Points);
			if (!Item) continue;

			BindPageAction(*Item, TEXT("Milestone"), 17800 + Index);
			SetFeaturePageText(*Item, TEXT("MilestoneLabel"),
				FText::FromString(FString::Printf(TEXT("%d · %s"), Milestone.Points, Status)));
			if (auto* Button = Item->Get<UButton>(TEXT("Milestone")))
			{
				Button->SetIsEnabled(bClaimable);
				TArray<FString> RewardText;
				for (const auto& Reward : GetConfiguredRewardPreview(Milestone.RewardGroupId))
				{
					RewardText.Add(FString::Printf(TEXT("%s ×%lld"), *Reward.DisplayName, Reward.Quantity));
				}
				Button->SetToolTipText(FText::FromString(FString::Join(RewardText, TEXT("、"))));
			}
		}
	}
	const int32 VisibleCount = AddServerDailyTaskRows(Snapshot, *Scroll, RowClass, RewardClass);
	Page->SetPresent(Page->Get<USizeBox>(TEXT("TaskDailyServerEmptySize")), Snapshot.bDailyLoaded && VisibleCount == 0);
	return true;
}

URokUiPageWidget* URokUnityFeatureScreenWidget::BuildTaskRowPage(UClass* RowClass, UClass* RewardClass,
	int32 RowIndex, const FText& Title, const FText& Description, const FIntRect& TaskIconRect,
	int32 RewardGroupId, const TArray<FRokTaskRewardPreviewRow>& Rewards, bool bServerRow, bool bClaimable,
	const FText& ButtonLabel)
{
	auto* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
	if (!Row || !Row->Get<UCanvasPanel>(TEXT("Row")))
	{
		return nullptr;
	}
	Row->LegacyPrefix = TEXT("Task");
	Row->LegacySuffix = FString::Printf(TEXT("_%d"), RowIndex);
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BindPageAction(*Row, TEXT("Action"), (bServerRow ? 6000 : 320) + RowIndex);
	SetFeaturePageText(*Row, TEXT("ActionLabel"), ButtonLabel);
	Row->SetShown(Row->Get<UCanvasPanel>(TEXT("ActionBackground")), bClaimable);
	Row->SetShown(Row->Get<UCanvasPanel>(TEXT("ActionGoBackground")), !bClaimable);

	if (auto* Label = Row->Get<UTextBlock>(TEXT("Title")))
	{
		Label->SetText(Title);
		if (bServerRow)
		{
			Label->SetFont(URokMainHudWidget::MakeUnitySourceFont(24, false));
		}
	}
	if (auto* Label = Row->Get<UTextBlock>(TEXT("Description")))
	{
		Label->SetText(Description);
		if (bServerRow)
		{
			Label->SetFont(URokMainHudWidget::MakeUnitySourceFont(22, false));
			Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
			Label->SetToolTipText(Description);
		}
	}
	if (auto* Icon = Row->Get<UImage>(TEXT("Icon")))
	{
		UTexture2D* Texture = LoadUiTexture(TaskIconAtlasPath);
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture)
		{
			Icon->SetBrush(MakeAtlasBrush(Texture, TaskIconRect, TaskIconAtlasSize, Icon->GetBrush().GetImageSize()));
		}
	}

	constexpr int32 VisibleRewardSlots = 4;
	const bool bOverflow = Rewards.Num() > VisibleRewardSlots;
	const int32 VisibleRewardCount = bOverflow ? VisibleRewardSlots - 1 : Rewards.Num();
	for (int32 Index = 0; Index < VisibleRewardCount; ++Index)
	{
		auto* Host = Row->Get<UCanvasPanel>(*FString::Printf(TEXT("DailyRewardSlot%d"), Index));
		auto* Cell = MountPageItem(Host, RewardClass, TEXT("Task"), RowIndex);
		if (!Cell) continue;

		Cell->LegacySuffix = FString::Printf(TEXT("_%d_%d"), RowIndex, Index);
		const auto& Reward = Rewards[Index];
		const FText Quantity = FText::AsNumber(Reward.Quantity);
		if (auto* Container = Cell->Get<UCanvasPanel>(TEXT("RewardPreview")))
		{
			Container->SetToolTipText(FText::Format(
				bServerRow
					? NSLOCTEXT("RokFeature", "TaskServerRewardPreviewTooltip", "{0} ×{1} · 奖励配置预览，领取以服务器为准")
					: NSLOCTEXT("RokFeature", "TaskRewardPreviewTooltip", "{0} ×{1} · 任务奖励预览，服务器状态待同步"),
				FText::FromString(Reward.DisplayName), Quantity));
		}

		const bool bHasIcon = Reward.IconRect.Width() > 0 && Reward.IconRect.Height() > 0;
		if (auto* Icon = Cell->Get<UImage>(TEXT("RewardPreviewIcon")))
		{
			Cell->SetPresent(Icon, bHasIcon);
			if (bHasIcon)
			{
				UTexture2D* Texture = LoadUiTexture(Reward.AtlasPath.IsEmpty() ? CommonAtlasPath : *Reward.AtlasPath);
				Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				if (Texture)
				{
					Icon->SetBrush(MakeAtlasBrush(Texture, Reward.IconRect, Reward.IconAtlasSize, Icon->GetBrush().GetImageSize()));
				}
			}
		}
		if (auto* Count = Cell->Get<UTextBlock>(TEXT("RewardPreviewCount")))
		{
			Count->SetText(bHasIcon ? Quantity : FText::Format(
				NSLOCTEXT("RokFeature", "TaskRewardTextOnly", "{0} ×{1}"), FText::FromString(Reward.DisplayName), Quantity));
			if (!bHasIcon)
			{
				if (auto* CountSlot = Cast<UCanvasPanelSlot>(Count->Slot))
				{
					CountSlot->SetPosition(FVector2D(0, 3));
					CountSlot->SetSize(FVector2D(152, 27));
				}
			}
		}
	}
	if (auto* More = Row->Get<UTextBlock>(TEXT("RewardMore")))
	{
		Row->SetPresent(More, bOverflow);
		if (bOverflow)
		{
			More->SetText(FText::Format(NSLOCTEXT("RokFeature", "TaskRewardMore", "另有 {0} 项"), FText::AsNumber(Rewards.Num() - VisibleRewardCount)));
			TArray<FString> Items;
			for (int32 Index = VisibleRewardCount; Index < Rewards.Num(); ++Index)
			{
				const auto& Reward = Rewards[Index];
				Items.Add(FString::Printf(TEXT("%s ×%lld"), *Reward.DisplayName, Reward.Quantity));
			}
			More->SetToolTipText(FText::FromString(FString::Join(Items, TEXT("、"))));
		}
	}
	Row->SetPresent(Row->Get<UTextBlock>(TEXT("RewardUnavailable")), Rewards.IsEmpty() && RewardGroupId > 0);
	return Row;
}
