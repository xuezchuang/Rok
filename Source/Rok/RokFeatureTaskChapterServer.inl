FText URokUnityFeatureScreenWidget::GetServerTaskChapterTitle(const FRokServerTaskSnapshot& Snapshot) const
{
	FString Title = Snapshot.ChapterName;
	FRokTaskChapterPreviewRow Metadata;
	if (RokTaskChapterConfigPreviewData::FindChapterById(TaskChapterGroups, Snapshot.ChapterId, Metadata)
		&& Metadata.bMetadataKnown && Title == Metadata.Title.ZhCn)
	{
		Title += Metadata.Description1.ZhCn;
	}
	if (Title.IsEmpty())
	{
		Title = Snapshot.ChapterId > 0 ? FString::Printf(TEXT("第%d章"), Snapshot.ChapterId) : TEXT("任务");
	}
	return FText::FromString(Title);
}

bool URokUnityFeatureScreenWidget::TryBuildServerTaskChapterPage(const FRokServerTaskSnapshot& Snapshot)
{
	UClass* RowClass = ResolveFeaturePageClass(TEXT("TaskChapterServerRow"));
	UClass* AggregateRewardClass = ResolveFeaturePageClass(TEXT("TaskChapterServerAggregateReward"));
	UClass* TaskRewardClass = ResolveFeaturePageClass(TEXT("TaskChapterServerTaskReward"));
	auto* Page = RowClass && AggregateRewardClass && TaskRewardClass ? MountModePage(TEXT("TaskChapterServer")) : nullptr;
	if (!Page)
	{
		return false;
	}

	const auto RestoreNative = [this, Page]()
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		VisibleTaskStates.Reset();
		RenderedTaskChapterTaskIds.Reset();
		RenderedTaskChapterTaskRewardPackageIds.Reset();
		RenderedTaskChapterAggregateRewardPackageIds.Reset();
		return false;
	};

	auto* Content = Page->Get<UCanvasPanel>(TEXT("TaskChapterContent"));
	auto* GuideLayout = Page->Get<UCanvasPanel>(TEXT("TaskChapterGuideLayout"));
	auto* Pending = Page->Get<UTextBlock>(TEXT("TaskChapterPendingMessage"));
	if (!Content || !GuideLayout || !Pending)
	{
		return RestoreNative();
	}
	BindServerTaskPageActions(*Page, Snapshot);
	if (auto* Title = Page->Get<UTextBlock>(TEXT("FeatureFrameTitle")))
	{
		Title->SetText(GetServerTaskChapterTitle(Snapshot));
	}

	SelectedTaskChapterGroupId = Snapshot.ChapterId;
	bSelectedTaskChapterMetadataKnown = Snapshot.bLoaded && Snapshot.ChapterId > 0;
	if (!bSelectedTaskChapterMetadataKnown)
	{
		Content->RemoveFromParent();
		GuideLayout->RemoveFromParent();
		Page->LegacyNames.Add(TEXT("TaskChapterPendingMessage"), TEXT("TaskChapterMetaNotice"));
		Pending->SetText(Snapshot.bLoaded
			? NSLOCTEXT("RokFeature", "TaskServerChapterEmpty", "暂无当前章节")
			: NSLOCTEXT("RokFeature", "TaskServerChapterLoading", "等待服务器同步章节进度"));
		return true;
	}
	Pending->RemoveFromParent();

	auto* Progress = Page->Get<UTextBlock>(TEXT("TaskChapterMetaNotice"));
	auto* ClaimProgress = Page->Get<UTextBlock>(TEXT("TaskChapterClaimProgress"));
	auto* Claim = Page->Get<URokUnityIndexedButton>(TEXT("TaskChapterClaimAction"));
	auto* Scroll = Page->Get<UScrollBox>(TEXT("TaskChapterScroll"));
	auto* AggregateHost = Page->Get<UCanvasPanel>(TEXT("TaskChapterAggregateRewardHost"));
	auto* EmptyRow = Page->Get<USizeBox>(TEXT("TaskChapterServerEmptySize"));
	auto* Dialogue = Page->Get<URichTextBlock>(TEXT("TaskChapterDialogue"));
	auto* ModelHost = Page->Get<UCanvasPanel>(TEXT("TaskChapterGuideModelHost"));
	if (!Progress || !ClaimProgress || !Claim || !Scroll || !AggregateHost || !EmptyRow || !Dialogue || !ModelHost)
	{
		return RestoreNative();
	}
	if (Snapshot.bChapterClaimable)
	{
		BindPageAction(*Page, TEXT("TaskChapterClaimAction"), 6100);
		Progress->RemoveFromParent();
		Progress = ClaimProgress;
		Page->LegacyNames.Add(TEXT("TaskChapterClaimProgress"), TEXT("TaskChapterMetaNotice"));
	}
	else
	{
		Claim->RemoveFromParent();
		ClaimProgress->RemoveFromParent();
	}
	Progress->SetText(Snapshot.bChapterProgressKnown
		? FText::Format(NSLOCTEXT("RokFeature", "TaskChapterProgress", "进度:{0}/{1}"),
			FText::AsNumber(Snapshot.ChapterCurrent), FText::AsNumber(Snapshot.ChapterTarget))
		: NSLOCTEXT("RokFeature", "TaskChapterPendingProgress", "进度待同步"));
	Page->SetPresent(EmptyRow, Snapshot.ChapterTasks.IsEmpty());

	const auto PlaceRewards = [this, AggregateRewardClass, TaskRewardClass](UCanvasPanel& Host,
		const FRokTaskChapterRewardGroup& Group, bool bAggregate, int32 OwnerId)
	{
		for (int32 Index = 0; Index < Group.Rows.Num(); ++Index)
		{
			const auto& Reward = Group.Rows[Index];
			auto* Cell = CreateWidget<URokUiPageWidget>(this, bAggregate ? AggregateRewardClass : TaskRewardClass);
			auto* Art = Cell ? Cell->Get<UImage>(TEXT("Icon")) : nullptr;
			auto* Quantity = Cell ? Cell->Get<UTextBlock>(TEXT("Quantity")) : nullptr;
			if (!Art || !Quantity)
			{
				return false;
			}
			const FString Prefix = bAggregate ? FString::Printf(TEXT("TaskChapterAggregateReward_%d_%d"), OwnerId, Index)
				: FString::Printf(TEXT("TaskChapterTaskReward_%d_%d"), OwnerId, Index);
			Cell->LegacyNames.Add(TEXT("Icon"), Prefix + TEXT("Icon"));
			Cell->LegacyNames.Add(TEXT("Quantity"), Prefix + TEXT("Quantity"));
			Cell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			Quantity->SetText(FText::AsNumber(Reward.Quantity));
			UTexture2D* Texture = LoadUiTexture(*Reward.AtlasObjectPath);
			if (Texture)
			{
				Art->SetBrush(MakeAtlasBrush(Texture, Reward.IconRect, Reward.IconAtlasSize, Art->GetBrush().GetImageSize()));
			}
			else
			{
				Art->SetBrush(FSlateBrush());
			}
			Art->SetVisibility(bAggregate ? ESlateVisibility::Visible
				: Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (bAggregate)
			{
				auto* QualityIcon = Cell->Get<UImage>(TEXT("Quality"));
				if (!QualityIcon)
				{
					return false;
				}
				int32 Quality = 2;
				if (Reward.RewardType == 200)
				{
					auto* Config = GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>();
					if (Config) Config->GetIntFieldByKey(TEXT("Item"), TEXT("ID"), Reward.TypeData, TEXT("quality"), Quality);
				}
				if (UTexture2D* QualityTexture = LoadUiTexture(Item0AtlasPath))
				{
					QualityIcon->SetBrush(MakeAtlasBrush(QualityTexture, ResolveBagQualityRect(Quality),
						Item0AtlasSize, QualityIcon->GetBrush().GetImageSize()));
				}
				else
				{
					QualityIcon->SetVisibility(ESlateVisibility::Collapsed);
				}
				Cell->LegacyNames.Add(TEXT("Quality"), Prefix + TEXT("Quality"));
				Art->SetToolTipText(FText::FromString(Reward.DisplayNameZhCn));
				if (!PlaceTaskChapterRewardFromLayout(Host, *Cell, TEXT("ServerAggregate"), Index))
				{
					PlaceOnCanvas(&Host, Cell, FVector2D(Index * 69, 0), FVector2D(55));
				}
				RenderedTaskChapterAggregateRewardPackageIds.Add(Reward.PackageRowId);
			}
			else
			{
				if (!PlaceTaskChapterRewardFromLayout(Host, *Cell, TEXT("ServerTask"), Index))
				{
					PlaceOnCanvas(&Host, Cell, FVector2D(Index * 103, 0), FVector2D(103, 29));
				}
				RenderedTaskChapterTaskRewardPackageIds.Add(Reward.PackageRowId);
			}
		}
		return true;
	};

	FRokTaskChapterRewardGroup Aggregate;
	if (Snapshot.ChapterRewardGroupId > 0
		&& RokTaskChapterRewardPreviewData::FindForChapter(TaskChapterRewardGroups, Snapshot.ChapterId, Aggregate)
		&& !PlaceRewards(*AggregateHost, Aggregate, true, Snapshot.ChapterId))
	{
		return RestoreNative();
	}

	for (const auto& Task : Snapshot.ChapterTasks)
	{
		auto* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
		auto* Description = Row ? Row->Get<UTextBlock>(TEXT("Description")) : nullptr;
		auto* RewardHost = Row ? Row->Get<UCanvasPanel>(TEXT("TaskRewardHost")) : nullptr;
		auto* Go = Row ? Row->Get<URokUnityIndexedButton>(TEXT("Go")) : nullptr;
		auto* ClaimTask = Row ? Row->Get<URokUnityIndexedButton>(TEXT("Claim")) : nullptr;
		auto* StateLabel = Row ? Row->Get<UTextBlock>(TEXT("State")) : nullptr;
		if (!Description || !RewardHost || !Go || !ClaimTask || !StateLabel)
		{
			return RestoreNative();
		}
		const FString Prefix = FString::Printf(TEXT("TaskChapterTask_%d"), Task.TaskId);
		Row->LegacyNames.Add(TEXT("Description"), FString::Printf(TEXT("TaskChapterTaskDescription_%d"), Task.TaskId));
		for (const TCHAR* Name : {TEXT("Row"), TEXT("Size"), TEXT("Background"), TEXT("RewardBand"), TEXT("RewardLabel"),
			TEXT("Go"), TEXT("GoLabel"), TEXT("Claim"), TEXT("ClaimLabel"), TEXT("State")})
		{
			Row->LegacyNames.Add(Name, Prefix + Name);
		}
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Description->SetText(Task.bProgressKnown
			? FText::Format(NSLOCTEXT("RokFeature", "TaskChapterObjectiveProgress", "{0} ({1}/{2})"),
				FText::FromString(Task.Description), FText::AsNumber(Task.Current), FText::AsNumber(Task.Target))
			: FText::FromString(Task.Description));
		Description->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Description->SetToolTipText(Description->GetText());

		FRokTaskChapterRewardGroup TaskRewards;
		if (Task.RewardGroupId > 0 && RokTaskChapterRewardPreviewData::FindForTask(TaskChapterRewardGroups, Task.TaskId, TaskRewards)
			&& !PlaceRewards(*RewardHost, TaskRewards, false, Task.TaskId))
		{
			return RestoreNative();
		}

		const bool bClaim = Task.State == ERokServerTaskState::Finished;
		const bool bGo = Task.State == ERokServerTaskState::Unfinished;
		Row->SetPresent(Go, bGo);
		Row->SetPresent(ClaimTask, bClaim);
		Row->SetPresent(StateLabel, !bGo && !bClaim);
		if (bClaim || bGo)
		{
			FRokTaskRowState Action;
			Action.TaskId = Task.TaskId;
			Action.Page = FRokTaskRowState::EPage::ChapterTask;
			Action.SourceAuthority = TEXT("server");
			Action.Type = Task.Type;
			Action.Param1 = Task.Param1;
			Action.Param2 = Task.Param2;
			Action.Require = Task.Require;
			Action.State = bClaim ? FRokTaskRowState::EState::Finished : FRokTaskRowState::EState::Unfinished;
			Action.Intent = bClaim ? FRokTaskRowState::EIntent::Claim : FRokTaskRowState::EIntent::Go;
			BindPageAction(*Row, bClaim ? TEXT("Claim") : TEXT("Go"), 6000 + VisibleTaskStates.Add(Action));
		}
		else
		{
			StateLabel->SetText(Task.State == ERokServerTaskState::Received
				? NSLOCTEXT("RokFeature", "TaskChapterCompleted", "已完成")
				: NSLOCTEXT("RokFeature", "TaskChapterUnknownState", "状态待同步"));
		}
		Scroll->AddChild(Row);
		RenderedTaskChapterTaskIds.Add(Task.TaskId);
	}

	FRokTaskChapterPreviewRow Metadata;
	if (RokTaskChapterConfigPreviewData::FindChapterById(TaskChapterGroups, Snapshot.ChapterId, Metadata))
	{
		PopulateServerTaskChapterGuide(Metadata, *Dialogue, ModelHost);
	}
	else
	{
		GuideLayout->RemoveFromParent();
	}
	return true;
}
