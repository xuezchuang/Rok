void URokUnityFeatureScreenWidget::BuildCommander()
{
	// UI_IF_Captain is a true full-screen view. CapturePoint maps the Unity
	// 1440x900 Expand capture onto the centred 1440x810 design canvas.
	UClass* CommanderChromeClass = ResolveFeaturePageClass(TEXT("CommanderChrome"));
	URokUiPageWidget* CommanderChromePage = CommanderChromeClass
		? CreateWidget<URokUiPageWidget>(this, CommanderChromeClass) : nullptr;
	if (CommanderChromePage && CommanderChromePage->Get<UButton>(TEXT("CommanderBack"))
		&& CommanderChromePage->Get<UTextBlock>(TEXT("CommanderPower"))
		&& CommanderChromePage->Get<UCanvasPanel>(TEXT("CommanderPowerFooter")))
	{
		CommanderChromePage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CommanderChromePage->Get<UCanvasPanel>(TEXT("CommanderPowerFooter"))->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* ChromePageSlot = CommanderChrome->AddChildToCanvas(CommanderChromePage);
		ChromePageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ChromePageSlot->SetOffsets(FMargin(0));
		BindPageAction(*CommanderChromePage, TEXT("CommanderBack"), 1);
	}
	else
	{
		CommanderChromePage = nullptr;
		PlaceOnCanvas(CommanderChrome, MakeActionButton(TEXT("CommanderBack"), 1, CommonAtlasPath,
			CityManagerBackRect, CommonAtlasSize, FVector2D(60)),
			FVector2D(18, 10), FVector2D(60), 12);
	}
	const auto MountCommanderStatus = [this](bool bWaiting) -> bool
	{
		UClass* StatusClass = ResolveFeaturePageClass(TEXT("CommanderStatus"));
		URokUiPageWidget* Page = StatusClass ? CreateWidget<URokUiPageWidget>(this, StatusClass) : nullptr;
		UBorder* Background = Page ? Page->Get<UBorder>(TEXT("CommanderWaitingBackground")) : nullptr;
		UTextBlock* Waiting = Page ? Page->Get<UTextBlock>(TEXT("CommanderServerWaiting")) : nullptr;
		UTextBlock* Unavailable = Page ? Page->Get<UTextBlock>(TEXT("CommanderPreviewUnavailable")) : nullptr;
		if (!Page || !Background || !Waiting || !Unavailable) return false;
		Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* StatusSlot = ModeCanvas->AddChildToCanvas(Page);
		StatusSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		StatusSlot->SetOffsets(FMargin(0));
		StatusSlot->SetZOrder(bWaiting ? 0 : 2);
		Waiting->SetText(NSLOCTEXT("RokCommander", "HeroesWaiting", "统帅数据待服务器同步"));
		Unavailable->SetText(NSLOCTEXT("RokCommander", "PreviewUnavailable", "统帅预览暂不可用"));
		Page->SetPresent(Background, bWaiting);
		Page->SetPresent(Waiting, bWaiting);
		Page->SetPresent(Unavailable, !bWaiting);
		return true;
	};
	URokUnityGameplayCatalogSubsystem* GameplayCatalog =
		GetGameplayCatalogSubsystem();
	if (GameplayCatalog && !GameplayCatalog->IsCatalogLoaded())
	{
		GameplayCatalog->LoadCatalog();
	}
	URokUnityRuntimeStateSubsystem* RuntimeState =
		GetRuntimeStateSubsystem();
	if (RuntimeState && !RuntimeState->IsRuntimeStateLoaded())
	{
		RuntimeState->LoadRuntimeState();
	}

	const UGameInstance* Instance = GetGameInstance();
	const URokServerHeroSubsystem* Server = Instance
		? Instance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	const FRokServerHeroSnapshot ServerSnapshot = Server
		? Server->GetSnapshot() : FRokServerHeroSnapshot();
	bLastServerHeroAvailable = ServerSnapshot.bAvailable;
	bLastServerHeroKnown = ServerSnapshot.bHeroesKnown;
	LastServerHeroSignature = ServerSnapshot.Signature;
	const auto* Inventory = Instance ? Instance->GetSubsystem<URokServerInventorySubsystem>() : nullptr;
	const auto Items = ServerSnapshot.bAvailable && Inventory ? Inventory->GetSnapshot() : FRokServerInventorySnapshot();
	LastCommanderInventorySignature = Items.Signature;
	TMap<int32, int64> ItemCounts;
	if (Items.bItemsKnown && Items.bRowsComplete)
	{
		for (const auto& Stack : Items.Stacks) ItemCounts.FindOrAdd(Stack.ItemId) += Stack.Overlay;
	}
	if (ServerSnapshot.bAvailable
		&& (!ServerSnapshot.bHeroesKnown
			|| (!ServerSnapshot.bRowsComplete && ServerSnapshot.Heroes.IsEmpty())))
	{
		StopCommanderSceneMotion();
		VisibleCommanderIds.Reset();
		VisibleCommanderSkillIds.Reset();
		VisibleCommanderTalentIds.Reset();
		if (!MountCommanderStatus(true))
		{
			PlaceOnCanvas(ModeCanvas,
				MakeSolidBorder(TEXT("CommanderWaitingBackground"), FLinearColor(0.075f, 0.105f, 0.105f, 1.0f)),
				CapturePoint(0.0f, 0.0f), FVector2D(1440.0f, 900.0f), 0);
			UTextBlock* Waiting = MakeScreenText(TEXT("CommanderServerWaiting"), 26,
				FLinearColor::White, false, ETextJustify::Center);
			Waiting->SetText(NSLOCTEXT("RokCommander", "HeroesWaiting", "统帅数据待服务器同步"));
			PlaceOnCanvas(ModeCanvas, Waiting, CapturePoint(390.0f, 390.0f), FVector2D(660.0f, 60.0f), 5);
		}
		return;
	}
	TArray<FRokUnityRuntimeHeroState> RuntimeHeroes;
	if (ServerSnapshot.bAvailable)
	{
		for (const FRokServerHeroEntry& Entry : ServerSnapshot.Heroes)
		{
			FRokUnityRuntimeHeroState& Hero = RuntimeHeroes.AddDefaulted_GetRef();
			Hero.HeroId = Entry.HeroId;
			Hero.Level = Entry.bLevelKnown ? Entry.Level : 0;
			Hero.Star = Entry.bStarKnown ? Entry.Star : 0;
			Hero.Rare = Entry.Rare;
			Hero.Score = Entry.bPowerKnown ? Entry.Power : 0;
			FRokUnityCatalogHero CatalogHero;
			Hero.MaxStar = GameplayCatalog && GameplayCatalog->GetHero(Entry.HeroId, CatalogHero)
				? CatalogHero.MaxStar : 0;
		}
	}
	else if (RuntimeState && bCommanderLocalRosterFixture)
	{
		RuntimeHeroes = RuntimeState->GetStarterHeroes();
	}
	TSet<int32> PreviewOwnedHeroIds;
	for (const FRokUnityRuntimeHeroState& RuntimeHero : RuntimeHeroes)
	{
		PreviewOwnedHeroIds.Add(RuntimeHero.HeroId);
	}

	TArray<int32> CandidateHeroIds;
	if (GameplayCatalog)
	{
		for (const int32 HeroId : GameplayCatalog->GetHeroIds())
		{
			FRokUnityCatalogHero Hero;
			if (!GameplayCatalog->GetHero(HeroId, Hero))
			{
				continue;
			}
			if (Hero.ListDisplay == 1)
			{
				continue;
			}
			CandidateHeroIds.Add(HeroId);
		}
	}
	else
	{
		for (const FCommanderHeadSource& Source : CommanderHeadSources)
		{
			CandidateHeroIds.Add(Source.HeroId);
		}
	}
	if (ServerSnapshot.bAvailable)
	{
		for (const FRokServerHeroEntry& Entry : ServerSnapshot.Heroes)
			CandidateHeroIds.AddUnique(Entry.HeroId);
	}
	const auto FindRuntimeHero =
		[&RuntimeHeroes](int32 HeroId) -> const FRokUnityRuntimeHeroState*
		{
			return RuntimeHeroes.FindByPredicate(
				[HeroId](const FRokUnityRuntimeHeroState& RuntimeHero)
				{
					return RuntimeHero.HeroId == HeroId;
				});
		};
	auto* CommanderConfig = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	TMap<int32, int32> PreviewPowerById;
	const auto ResolvePreviewPower = [&](const FRokUnityCatalogHero& Hero)
	{
		if (const int32* Cached = PreviewPowerById.Find(Hero.Id)) return *Cached;
		int32 Power = Hero.BasePower;
		if (CommanderConfig)
		{
			// HeroProxy.UpdatePower counts the level-one score and the initially unlocked skills.
			int32 Score = 0;
			if (CommanderConfig->GetIntFieldByKey(TEXT("HeroLevel"), TEXT("ID"), Hero.Rare * 10000, TEXT("score"), Score))
				Power += Score;
			for (int32 SkillId : Hero.SkillIds)
			{
				FRokUnityCatalogHeroSkill Skill;
				if (GameplayCatalog->GetHeroSkill(SkillId, Skill) && Skill.Open == 1
					&& CommanderConfig->GetIntFieldByKey(TEXT("HeroSkillEffect"), TEXT("ID"), SkillId * 1000 + 1, TEXT("score"), Score))
					Power += Score;
			}
		}
		PreviewPowerById.Add(Hero.Id, Power);
		return Power;
	};
	const auto ResolveCommanderSortValue =
		[this, GameplayCatalog, &FindRuntimeHero, &ResolvePreviewPower](int32 HeroId) -> int64
		{
			FRokUnityCatalogHero Hero;
			const bool bHasHero = GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, Hero);
			const FRokUnityRuntimeHeroState* RuntimeHero =
				FindRuntimeHero(HeroId);
			switch (SelectedCommanderSortType)
			{
			case 1:
				return RuntimeHero
					? RuntimeHero->Star
					: (bHasHero ? Hero.InitStar : 0);
			case 2:
				return RuntimeHero ? RuntimeHero->Level : 1;
			case 3:
				return RuntimeHero
					? RuntimeHero->Score
					: (bHasHero ? (bCommanderLocalRosterFixture ? Hero.BasePower : ResolvePreviewPower(Hero)) : 0);
			default:
				return bHasHero ? Hero.Rare : 0;
			}
		};
	CandidateHeroIds.Sort(
		[this, GameplayCatalog, &PreviewOwnedHeroIds, &FindRuntimeHero,
			&ResolveCommanderSortValue, &ResolvePreviewPower](int32 Left, int32 Right)
		{
			const bool bLeftOwned = PreviewOwnedHeroIds.Contains(Left);
			const bool bRightOwned = PreviewOwnedHeroIds.Contains(Right);
			if (bLeftOwned != bRightOwned)
			{
				return bLeftOwned;
			}
			const int64 LeftSortValue = ResolveCommanderSortValue(Left);
			const int64 RightSortValue = ResolveCommanderSortValue(Right);
			if (LeftSortValue != RightSortValue)
			{
				return LeftSortValue > RightSortValue;
			}
			if (GameplayCatalog)
			{
				FRokUnityCatalogHero LeftHero;
				FRokUnityCatalogHero RightHero;
				if (GameplayCatalog->GetHero(Left, LeftHero) && GameplayCatalog->GetHero(Right, RightHero))
				{
					if (LeftHero.Rare != RightHero.Rare) return LeftHero.Rare > RightHero.Rare;
					if (SelectedCommanderSortType == 0 && !bCommanderLocalRosterFixture)
					{
						const auto* OwnedLeft = FindRuntimeHero(Left);
						const auto* OwnedRight = FindRuntimeHero(Right);
						const int32 LeftStar = OwnedLeft ? OwnedLeft->Star : LeftHero.InitStar;
						const int32 RightStar = OwnedRight ? OwnedRight->Star : RightHero.InitStar;
						if (LeftStar != RightStar) return LeftStar > RightStar;
						if (OwnedLeft && OwnedRight && OwnedLeft->Level != OwnedRight->Level)
							return OwnedLeft->Level > OwnedRight->Level;
						const int32 LeftPower = OwnedLeft ? OwnedLeft->Score : ResolvePreviewPower(LeftHero);
						const int32 RightPower = OwnedRight ? OwnedRight->Score : ResolvePreviewPower(RightHero);
						if (LeftPower != RightPower) return LeftPower > RightPower;
					}
				}
			}
			return Left < Right;
		});
	if (CandidateHeroIds.IsEmpty())
	{
		if (GameplayCatalog)
		{
			for (const int32 HeroId : GameplayCatalog->GetHeroIds())
			{
				FRokUnityCatalogHero Hero;
				if (GameplayCatalog->GetHero(HeroId, Hero)
					&& Hero.ListDisplay != 1)
				{
					CandidateHeroIds.Add(HeroId);
				}
			}
		}
		else
		{
			for (const FCommanderHeadSource& Source : CommanderHeadSources)
			{
				CandidateHeroIds.Add(Source.HeroId);
			}
		}
	}
	VisibleCommanderIds = CandidateHeroIds;
	if (ServerSnapshot.bAvailable && ServerSnapshot.bHeroesKnown
		&& !ServerSnapshot.Heroes.IsEmpty()
		&& (!bCommanderServerSelectionInitialized || bSelectedCommanderServerOwned)
		&& !ServerSnapshot.Heroes.ContainsByPredicate([this](const FRokServerHeroEntry& Entry)
			{ return Entry.HeroId == SelectedCommanderId; }))
	{
		SelectedCommanderId = ServerSnapshot.Heroes[0].HeroId;
		SelectedCommanderSkillIndex = 0;
		SelectedCommanderSkillDetailIndex = INDEX_NONE;
		SelectedCommanderTalentIndex = INDEX_NONE;
	}
	if (!VisibleCommanderIds.Contains(SelectedCommanderId)
		&& !VisibleCommanderIds.IsEmpty())
	{
		SelectedCommanderId = VisibleCommanderIds[0];
		SelectedCommanderSkillIndex = 0;
		SelectedCommanderSkillDetailIndex = INDEX_NONE;
		SelectedCommanderTalentIndex = INDEX_NONE;
	}

	FRokUnityCatalogHero SelectedHero;
	const bool bHasSelectedHero = GameplayCatalog
		&& GameplayCatalog->GetHero(
			SelectedCommanderId,
			SelectedHero);
	const FRokUnityRuntimeHeroState* SelectedRuntimeHero =
		RuntimeHeroes.FindByPredicate(
			[this](const FRokUnityRuntimeHeroState& RuntimeHero)
			{
				return RuntimeHero.HeroId == SelectedCommanderId;
			});
	const bool bSelectedOwned = SelectedRuntimeHero != nullptr;
	if (ServerSnapshot.bAvailable)
	{
		bCommanderServerSelectionInitialized = true;
		bSelectedCommanderServerOwned = bSelectedOwned;
	}
	const FRokServerHeroEntry* SelectedServerHero = ServerSnapshot.bAvailable
		? ServerSnapshot.Heroes.FindByPredicate([this](const FRokServerHeroEntry& Entry)
			{ return Entry.HeroId == SelectedCommanderId; }) : nullptr;
	const int32 SelectedStar = SelectedRuntimeHero
		? SelectedRuntimeHero->Star
		: (bHasSelectedHero ? SelectedHero.InitStar : 1);
	const int32 SelectedMaxStar = SelectedRuntimeHero
		? SelectedRuntimeHero->MaxStar
		: (bHasSelectedHero ? SelectedHero.MaxStar : 6);
	const int32 SelectedLevel = SelectedRuntimeHero ? SelectedRuntimeHero->Level : 1;
	const int32 LevelKey = SelectedHero.Rare * 10000 + SelectedLevel - 1;
	const int32 SelectedPower = SelectedRuntimeHero ? SelectedRuntimeHero->Score
		: (bHasSelectedHero ? ResolvePreviewPower(SelectedHero) : 0);
	const FString SelectedNameString = SelectedServerHero && !SelectedServerHero->Name.IsEmpty()
		? SelectedServerHero->Name : bHasSelectedHero
		? (!SelectedHero.DisplayNameZhCn.IsEmpty()
			? SelectedHero.DisplayNameZhCn
			: SelectedHero.DisplayNameEn)
		: FString::Printf(TEXT("统帅 %d"), SelectedCommanderId);
	const FString SelectedTitleString = SelectedServerHero && !SelectedServerHero->Title.IsEmpty()
		? SelectedServerHero->Title : bHasSelectedHero
		? (!SelectedHero.DisplayTitleZhCn.IsEmpty()
			? SelectedHero.DisplayTitleZhCn
			: SelectedHero.DisplayTitleEn)
		: FString();
	const FString SelectedDescriptionString = bHasSelectedHero
		? (!SelectedHero.DisplayDescriptionZhCn.IsEmpty()
			? SelectedHero.DisplayDescriptionZhCn
			: SelectedHero.DisplayDescriptionEn)
		: FString();

	FText CommanderSortLabel;
	switch (SelectedCommanderSortType)
	{
	case 1:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"StarSort",
			"星级 ▼");
		break;
	case 2:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"LevelSort",
			"等级 ▼");
		break;
	case 3:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"PowerSort",
			"战力 ▼");
		break;
	default:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"QualitySort",
			"稀有度 ▼");
		break;
	}

	TArray<FText> CommanderRoleLabels;
	VisibleCommanderTalentIds.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 TalentId = bHasSelectedHero && SelectedHero.TalentIds.IsValidIndex(Index)
			? SelectedHero.TalentIds[Index] : INDEX_NONE;
		VisibleCommanderTalentIds.Add(TalentId);
		FRokUnityCatalogHeroTalent Talent;
		const bool bHasTalent = GameplayCatalog && TalentId > 0
			&& GameplayCatalog->GetHeroTalent(TalentId, Talent);
		const FString TalentName = bHasTalent
			? (!Talent.DisplayNameZhCn.IsEmpty() ? Talent.DisplayNameZhCn : Talent.DisplayNameEn)
			: FString();
		CommanderRoleLabels.Add(TalentName.IsEmpty()
			? NSLOCTEXT("RokCommander", "TalentPositionUnavailable", "—")
			: FText::FromString(TalentName));
	}

	VisibleCommanderSkillIds.Reset();
	if (bHasSelectedHero)
	{
		for (const int32 SkillId : SelectedHero.SkillIds)
		{
			VisibleCommanderSkillIds.Add(SkillId);
			if (VisibleCommanderSkillIds.Num() == 5)
			{
				break;
			}
		}
	}
	else if (!GameplayCatalog && SelectedCommanderId == 2007)
	{
		for (int32 SkillId = 200701; SkillId <= 200705; ++SkillId)
		{
			VisibleCommanderSkillIds.Add(SkillId);
		}
	}
	SelectedCommanderSkillIndex = VisibleCommanderSkillIds.IsEmpty()
		? 0
		: FMath::Clamp(
			SelectedCommanderSkillIndex,
			0,
			VisibleCommanderSkillIds.Num() - 1);
	FRokUnityCatalogHeroSkill SelectedCommanderSkill;
	const bool bHasSelectedCommanderSkill = GameplayCatalog
		&& VisibleCommanderSkillIds.IsValidIndex(
			SelectedCommanderSkillIndex)
		&& GameplayCatalog->GetHeroSkill(
			VisibleCommanderSkillIds[SelectedCommanderSkillIndex],
			SelectedCommanderSkill);
	const auto ResolveCommanderHeadRect =
		[GameplayCatalog](int32 HeroId, FIntRect& OutRect)
		{
			FRokUnityCatalogHero Hero;
			if (GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, Hero)
				&& Hero.IconAtlas.Equals(
					TEXT("hero_head"),
					ESearchCase::IgnoreCase)
				&& Hero.IconWidth > 0
				&& Hero.IconHeight > 0
				&& Hero.IconAtlasWidth > 0
				&& Hero.IconAtlasHeight > 0)
			{
				OutRect = FIntRect(
					Hero.IconX,
					Hero.IconY,
					Hero.IconX + Hero.IconWidth,
					Hero.IconY + Hero.IconHeight);
				return true;
			}
			return ResolveFallbackCommanderHeadRect(HeroId, OutRect);
		};

	// The Hero table selects the Unity scene independently of roster ownership.
	FString Scene;
	if (CommanderSceneImage && CommanderConfig
		&& CommanderConfig->GetStringFieldByKey(TEXT("Hero"), TEXT("ID"), SelectedCommanderId, TEXT("heroScene"), Scene)
		&& (Scene == TEXT("HeroShow_01") || Scene == TEXT("HeroShow_02")
			|| Scene == TEXT("HeroShow_03") || Scene == TEXT("HeroShow_04")))
	{
		ShowCommanderScene(Scene);
	}
	else
	{
		StopCommanderSceneMotion();
	}
	// CaptainMediator keeps the loaded skeleton while updating same-hero details.
	// Reattach that widget after rebuilding the panel instead of restarting idle.
	if (CommanderAnimatedPortraitHeroId != SelectedCommanderId)
	{
		CommanderAnimatedPortraitHeroId = SelectedCommanderId;
		CommanderAnimatedPortrait = WidgetTree->ConstructWidget<URokCommanderMeshWidget>(
			URokCommanderMeshWidget::StaticClass(), TEXT("CommanderAnimatedPortrait"));
		if (!CommanderAnimatedPortrait->LoadHero(SelectedCommanderId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Commander %d could not load its generated source animation."), SelectedCommanderId);
			CommanderAnimatedPortrait = nullptr;
		}
	}
	if (CommanderAnimatedPortrait)
	{
#if !UE_BUILD_SHIPPING
		if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
			&& FParse::Param(FCommandLine::Get(), TEXT("RokCommanderStaticCapture")))
			CommanderAnimatedPortrait->FreezeAnimationForCapture(0.75f);
#endif
		UClass* PortraitClass = ResolveFeaturePageClass(TEXT("CommanderPortrait"));
		URokUiPageWidget* PortraitPage = PortraitClass ? CreateWidget<URokUiPageWidget>(this, PortraitClass) : nullptr;
		UCanvasPanel* PortraitSlot = PortraitPage ? PortraitPage->Get<UCanvasPanel>(TEXT("CommanderPortraitSlot")) : nullptr;
		if (PortraitPage && PortraitSlot && PortraitPage->Get<UButton>(TEXT("CommanderPortraitAction")))
		{
			PortraitPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* PageSlot = ModeCanvas->AddChildToCanvas(PortraitPage);
			PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			PageSlot->SetOffsets(FMargin(0));
			PageSlot->SetZOrder(2);
			UCanvasPanelSlot* ModelSlot = PortraitSlot->AddChildToCanvas(CommanderAnimatedPortrait);
			ModelSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			ModelSlot->SetOffsets(FMargin(0));
			BindPageAction(*PortraitPage, TEXT("CommanderPortraitAction"), 499);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas,
				MakeActionButton(TEXT("CommanderPortraitAction"), 499, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(650.0f, 740.0f)),
				CapturePoint(300.0f, 105.0f), FVector2D(650.0f, 740.0f), 3);
			PlaceOnCanvas(ModeCanvas, CommanderAnimatedPortrait, CapturePoint(0.0f, 0.0f), FVector2D(1440.0f, 900.0f), 2);
		}
	}
	else
	{
		if (!MountCommanderStatus(false))
		{
			auto* Unavailable = MakeScreenText(TEXT("CommanderPreviewUnavailable"), 18,
				FLinearColor::White, true, ETextJustify::Center);
			Unavailable->SetText(NSLOCTEXT("RokCommander", "PreviewUnavailable", "统帅预览暂不可用"));
			PlaceOnCanvas(ModeCanvas, Unavailable, CapturePoint(430, 430), FVector2D(420, 36), 2);
		}
	}

	UClass* RosterClass = ResolveFeaturePageClass(TEXT("CommanderRoster"));
	URokUiPageWidget* RosterPage = RosterClass ? CreateWidget<URokUiPageWidget>(this, RosterClass) : nullptr;
	UTextBlock* OwnedCount = RosterPage ? RosterPage->Get<UTextBlock>(TEXT("CommanderOwnedCount")) : nullptr;
	UTextBlock* Sort = RosterPage ? RosterPage->Get<UTextBlock>(TEXT("CommanderSortLabel")) : nullptr;
	UScrollBox* CommanderRosterScroll = RosterPage ? RosterPage->Get<UScrollBox>(TEXT("CommanderRosterScroll")) : nullptr;
	if (RosterPage && OwnedCount && Sort && CommanderRosterScroll && RosterPage->Get<UButton>(TEXT("CommanderSortAction")))
	{
		RosterPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* RosterSlot = ModeCanvas->AddChildToCanvas(RosterPage);
		RosterSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		RosterSlot->SetOffsets(FMargin(0));
		RosterSlot->SetZOrder(3);
		BindPageAction(*RosterPage, TEXT("CommanderSortAction"), 470);
	}
	else
	{
		RosterPage = nullptr;
		PlaceOnCanvas(ModeCanvas,
			MakeSolidBorder(TEXT("CommanderRosterShade"), FLinearColor(0.035f, 0.035f, 0.035f, 0.70f)),
			CapturePoint(13.0f, 119.0f), FVector2D(304.0f, 758.0f), 3);
		PlaceOnCanvas(ModeCanvas,
			MakeSolidBorder(TEXT("CommanderRosterHeader"), FLinearColor(0.025f, 0.025f, 0.025f, 0.88f)),
			CapturePoint(13.0f, 119.0f), FVector2D(304.0f, 52.0f), 4);
		OwnedCount = MakeScreenText(TEXT("CommanderOwnedCount"), 16, FLinearColor(0.96f, 0.96f, 0.94f, 1.0f), false);
		PlaceOnCanvas(ModeCanvas, OwnedCount, CapturePoint(26.0f, 132.0f), FVector2D(150.0f, 30.0f), 7);
		Sort = MakeScreenText(TEXT("CommanderSortLabel"), 17, FLinearColor(0.92f, 0.92f, 0.9f, 1.0f), false, ETextJustify::Right);
		PlaceOnCanvas(ModeCanvas, Sort, CapturePoint(182.0f, 132.0f), FVector2D(113.0f, 30.0f), 7);
		PlaceOnCanvas(ModeCanvas,
			MakeActionButton(TEXT("CommanderSortAction"), 470, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(130.0f, 44.0f)),
			CapturePoint(170.0f, 122.0f), FVector2D(130.0f, 44.0f), 8);
		CommanderRosterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CommanderRosterScroll"));
		CommanderRosterScroll->SetOrientation(Orient_Vertical);
		CommanderRosterScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		CommanderRosterScroll->SetAllowOverscroll(false);
		CommanderRosterScroll->SetAnimateWheelScrolling(true);
		CommanderRosterScroll->SetWheelScrollMultiplier(12.0f);
		PlaceOnCanvas(ModeCanvas, CommanderRosterScroll, CapturePoint(13.0f, 171.0f), FVector2D(304.0f, 684.0f), 5);
	}
	OwnedCount->SetText(FText::Format(
		NSLOCTEXT(
			"RokCommander",
			"OwnedCount",
			"已拥有 {0}"),
		FText::AsNumber(RuntimeHeroes.Num())));
	if (!ServerSnapshot.bAvailable && !bCommanderLocalRosterFixture)
		OwnedCount->SetText(NSLOCTEXT("RokCommander", "OfflineRoster", "离线预览"));
	else if (ServerSnapshot.bAvailable && !ServerSnapshot.bHeroesKnown)
		OwnedCount->SetText(NSLOCTEXT("RokCommander", "HeroesWaiting", "统帅数据待服务器同步"));
	else if (ServerSnapshot.bAvailable && RuntimeHeroes.IsEmpty() && ServerSnapshot.bRowsComplete)
		OwnedCount->SetText(NSLOCTEXT("RokCommander", "HeroesEmpty", "尚未拥有统帅"));
	else if (ServerSnapshot.bAvailable
		&& (!ServerSnapshot.bLoaded || !ServerSnapshot.bRowsComplete
			|| ServerSnapshot.Heroes.ContainsByPredicate([](const FRokServerHeroEntry& Entry)
				{ return !Entry.bCatalogKnown; })))
		OwnedCount->SetText(FText::Format(
			NSLOCTEXT("RokCommander", "HeroesPartial", "已同步 {0} · 部分统帅数据待同步"),
			FText::AsNumber(RuntimeHeroes.Num())));
	Sort->SetText(CommanderSortLabel);

	// This detached template supplies presentation rules; stock/legacy fallbacks stay native.
	FVector2D CommanderDividerSize(304, 38);
	int32 CommanderBiographyCharacterLimit = 58;
	FString CommanderBiographyEllipsis(TEXT("…"));
	if (UClass* TextLayoutClass = ResolveFeaturePageClass(TEXT("CommanderTextLayout")))
	{
		if (auto* TextLayout = CreateWidget<URokUiPageWidget>(this, TextLayoutClass))
		{
			const UWidget* DividerExtent = RokUiFind::Find(TextLayout, TEXT("DividerExtent"));
			const auto* ExtentSlot = DividerExtent ? Cast<UCanvasPanelSlot>(DividerExtent->Slot) : nullptr;
			if (ExtentSlot && !ExtentSlot->GetAutoSize()
				&& ExtentSlot->GetAnchors().Minimum == ExtentSlot->GetAnchors().Maximum)
			{
				const FVector2D Size = ExtentSlot->GetSize();
				if (FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y) && Size.X > 0 && Size.Y > 0)
				{
					CommanderDividerSize = Size;
				}
			}
			if (const auto* Limit = Cast<USpinBox>(RokUiFind::Find(TextLayout, TEXT("BiographyCharacterLimit"))))
			{
				const double Value = Limit->GetValue();
				if (FMath::IsFinite(Value) && Value >= 0 && Value <= static_cast<double>(MAX_int32)
					&& Value == static_cast<double>(static_cast<int32>(Value)))
				{
					CommanderBiographyCharacterLimit = static_cast<int32>(Value);
				}
			}
			if (const auto* Ellipsis = Cast<UTextBlock>(RokUiFind::Find(TextLayout, TEXT("BiographyEllipsis"))))
			{
				CommanderBiographyEllipsis = Ellipsis->GetText().ToString();
			}
		}
	}
	UClass* CommanderRosterRowClass = ResolveFeaturePageClass(TEXT("CommanderRosterRow"));
	const RokArmyStateStyles::FBatch ArmyStyles(this);
	for (int32 Index = 0; Index < VisibleCommanderIds.Num();)
	{
		const int32 RowStart = Index;
		const bool bOwnedRow = PreviewOwnedHeroIds.Contains(VisibleCommanderIds[RowStart]);
		if (!bOwnedRow && RowStart == RuntimeHeroes.Num() && ServerSnapshot.bAvailable)
		{
			UClass* DividerClass = ResolveFeaturePageClass(TEXT("CommanderRosterDivider"));
			URokUiPageWidget* DividerPage = DividerClass ? CreateWidget<URokUiPageWidget>(this, DividerClass) : nullptr;
			if (DividerPage && DividerPage->Get<UTextBlock>(TEXT("CommanderUnrecruitedDivider")))
			{
				DividerPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				DividerPage->Get<UTextBlock>(TEXT("CommanderUnrecruitedDivider"))->SetText(NSLOCTEXT("RokCommander", "NotRecruited", "尚未招募"));
				CommanderRosterScroll->AddChild(WrapFixedSize(DividerPage, TEXT("CommanderUnrecruitedDividerSize"), CommanderDividerSize));
			}
			else
			{
				auto* Divider = MakeScreenText(TEXT("CommanderUnrecruitedDivider"), 18, FLinearColor::White,
					false, ETextJustify::Center);
				Divider->SetText(NSLOCTEXT("RokCommander", "NotRecruited", "尚未招募"));
				CommanderRosterScroll->AddChild(WrapFixedSize(Divider, TEXT("CommanderUnrecruitedDividerSize"), FVector2D(304, 38)));
			}
		}
		URokUiPageWidget* RowPage = CommanderRosterRowClass ? CreateWidget<URokUiPageWidget>(this, CommanderRosterRowClass) : nullptr;
		UCanvasPanel* FirstSlot = RowPage ? RowPage->Get<UCanvasPanel>(TEXT("CommanderRosterHeadSlot_0")) : nullptr;
		UCanvasPanel* SecondSlot = RowPage ? RowPage->Get<UCanvasPanel>(TEXT("CommanderRosterHeadSlot_1")) : nullptr;
		USizeBox* DesignedSize = RowPage ? RowPage->Get<USizeBox>(TEXT("CommanderRosterDesignedSize")) : nullptr;
		UCanvasPanel* Row = nullptr;
		FVector2D RowSize(304.0f, 150.0f);
		if (RowPage && FirstSlot && SecondSlot && DesignedSize)
		{
			RowPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			RowSize = FVector2D(DesignedSize->GetWidthOverride(), DesignedSize->GetHeightOverride());
		}
		else
		{
			RowPage = nullptr;
			Row = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), *FString::Printf(TEXT("CommanderRosterRow_%d"), RowStart));
		}
		for (int32 ColumnIndex = 0; ColumnIndex < 2 && VisibleCommanderIds.IsValidIndex(Index)
			&& PreviewOwnedHeroIds.Contains(VisibleCommanderIds[Index]) == bOwnedRow; ++ColumnIndex, ++Index)
		{
			const int32 HeroId = VisibleCommanderIds[Index];
			FRokUnityCatalogHero RosterHero;
			const bool bHasRosterHero = GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, RosterHero);
			const FString RosterHeroName = bHasRosterHero
				? (!RosterHero.DisplayNameZhCn.IsEmpty()
					? RosterHero.DisplayNameZhCn
					: RosterHero.DisplayNameEn)
				: FString::Printf(TEXT("统帅 %d"), HeroId);
			const FRokUnityRuntimeHeroState* RosterRuntimeHero =
				FindRuntimeHero(HeroId);
			const FRokServerHeroEntry* RosterServerHero = ServerSnapshot.bAvailable
				? ServerSnapshot.Heroes.FindByPredicate([HeroId](const FRokServerHeroEntry& Entry)
					{ return Entry.HeroId == HeroId; }) : nullptr;
			const FText RosterStateText = RosterRuntimeHero
				&& RosterServerHero && (!RosterServerHero->bLevelKnown || !RosterServerHero->bStarKnown)
				? NSLOCTEXT("RokCommander", "RosterPartial", "等级 / 星级待同步")
				: RosterRuntimeHero
				? FText::Format(
					NSLOCTEXT("RokCommander", "RosterOwnedState", "Lv.{0}  ★{1}"),
					FText::AsNumber(RosterRuntimeHero->Level),
					FText::AsNumber(RosterRuntimeHero->Star))
				: bHasRosterHero && Items.bItemsKnown && Items.bRowsComplete
				? FText::Format(NSLOCTEXT("RokCommander", "RosterShardProgress", "{0}/{1}"),
					FText::AsNumber(ItemCounts.FindRef(RosterHero.GetItem)), FText::AsNumber(RosterHero.GetItemNum))
				: FText::Format(
					NSLOCTEXT("RokCommander", "RosterShardState", "需要 {0}"),
					FText::AsNumber(bHasRosterHero
						? FMath::Max(1, RosterHero.GetItemNum)
						: 10));
			FIntRect HeadRect;
			if (!ResolveCommanderHeadRect(HeroId, HeadRect))
			{
				HeadRect = FIntRect();
			}
			UCanvasPanel* Head = MakeCommanderHead(
					Index,
					HeadRect,
					RosterRuntimeHero != nullptr,
					HeroId == SelectedCommanderId,
					FText::FromString(RosterHeroName),
					RosterStateText,
					bHasRosterHero ? RosterHero.Rare : 0,
					ArmyStyles);
			if (RowPage)
			{
				UCanvasPanelSlot* HeadSlot = (ColumnIndex == 0 ? FirstSlot : SecondSlot)->AddChildToCanvas(Head);
				HeadSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				HeadSlot->SetOffsets(FMargin(0));
			}
			else
			{
				PlaceOnCanvas(Row, Head, FVector2D(11.0f + ColumnIndex * 140.0f, 4.0f), FVector2D(128.0f, 142.0f), 1);
			}
		}
		CommanderRosterScroll->AddChild(WrapFixedSize(
			RowPage ? static_cast<UWidget*>(RowPage) : static_cast<UWidget*>(Row),
			*FString::Printf(TEXT("CommanderRosterRowSize_%d"), RowStart),
			RowSize));
	}

	if (bCommanderSortMenuOpen)
	{
		UClass* SortClass = ResolveFeaturePageClass(TEXT("CommanderSortMenu"));
		URokUiPageWidget* SortPage = SortClass ? CreateWidget<URokUiPageWidget>(this, SortClass) : nullptr;
		if (SortPage)
		{
			SortPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* SortSlot = ModeCanvas->AddChildToCanvas(SortPage);
			SortSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			SortSlot->SetOffsets(FMargin(0));
			SortSlot->SetZOrder(30);
			const UTextBlock* SelectedStyle = SortPage->Get<UTextBlock>(TEXT("CommanderSortSelectedStyle"));
			const UTextBlock* NormalStyle = SortPage->Get<UTextBlock>(TEXT("CommanderSortNormalStyle"));
			for (int32 Index = 0; Index < 4; ++Index)
			{
				BindPageAction(*SortPage, *FString::Printf(TEXT("CommanderSortOption_%d"), Index), 472 + Index);
				if (UTextBlock* Label = SortPage->Get<UTextBlock>(*FString::Printf(TEXT("CommanderSortOption_%dLabel"), Index)))
				{
					const UTextBlock* Style = Index == SelectedCommanderSortType ? SelectedStyle : NormalStyle;
					if (Style) Label->SetColorAndOpacity(Style->GetColorAndOpacity());
				}
			}
		}
		else
		{
			PlaceOnCanvas(
					ModeCanvas,
					MakeSolidBorder(
						TEXT("CommanderSortMenu"),
					FLinearColor(0.035f, 0.035f, 0.035f, 0.98f)),
				CapturePoint(156.0f, 168.0f),
				FVector2D(144.0f, 180.0f),
				30);
			static const FText SortLabels[] = {
				NSLOCTEXT("RokCommander", "SortQuality", "稀有度"),
				NSLOCTEXT("RokCommander", "SortStar", "星级"),
				NSLOCTEXT("RokCommander", "SortLevel", "等级"),
				NSLOCTEXT("RokCommander", "SortPower", "战力"),
			};
			for (int32 SortIndex = 0; SortIndex < UE_ARRAY_COUNT(SortLabels); ++SortIndex)
			{
				PlaceOnCanvas(
					ModeCanvas,
					MakeActionButton(
						*FString::Printf(TEXT("CommanderSortOption_%d"), SortIndex),
						472 + SortIndex,
						nullptr,
						FIntRect(),
						FIntPoint::ZeroValue,
						FVector2D(136.0f, 42.0f),
						SortLabels[SortIndex],
						18,
						SortIndex == SelectedCommanderSortType
							? FLinearColor(1.0f, 0.78f, 0.24f, 1.0f)
							: FLinearColor(0.94f, 0.94f, 0.92f, 1.0f)),
					CapturePoint(160.0f, 172.0f + SortIndex * 43.0f),
					FVector2D(136.0f, 42.0f),
					31);
			}
		}
	}

	// Unity UI_Item_CaptainData owns this group, separately from roster and hero.
	const int32 CommanderDataFirstChild = ModeCanvas->GetChildrenCount();
	UClass* NameplateClass = ResolveFeaturePageClass(TEXT("CommanderNameplate"));
	URokUiPageWidget* NameplatePage = NameplateClass ? CreateWidget<URokUiPageWidget>(this, NameplateClass) : nullptr;
	bool bHasNameplate = NameplatePage && NameplatePage->Get<UTextBlock>(TEXT("CommanderAppellation"))
		&& NameplatePage->Get<UTextBlock>(TEXT("CommanderName"));
	for (int32 Index = 0; bHasNameplate && Index < 3; ++Index)
	{
		bHasNameplate = NameplatePage->Get<UButton>(*FString::Printf(TEXT("CommanderTalentLabel_%d"), Index))
			&& NameplatePage->Get<UTextBlock>(*FString::Printf(TEXT("CommanderTalentLabel_%dLabel"), Index));
	}
	if (bHasNameplate)
	{
		NameplatePage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* NameplateSlot = ModeCanvas->AddChildToCanvas(NameplatePage);
		NameplateSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		NameplateSlot->SetOffsets(FMargin(0));
		NameplateSlot->SetZOrder(7);
		NameplatePage->Get<UTextBlock>(TEXT("CommanderAppellation"))->SetText(FText::FromString(
			SelectedTitleString.IsEmpty() ? FString(TEXT("统帅")) : SelectedTitleString));
		NameplatePage->Get<UTextBlock>(TEXT("CommanderName"))->SetText(FText::FromString(SelectedNameString));
		for (int32 Index = 0; Index < 3; ++Index)
		{
			BindPageAction(*NameplatePage, *FString::Printf(TEXT("CommanderTalentLabel_%d"), Index), 490 + Index);
			NameplatePage->Get<UTextBlock>(*FString::Printf(TEXT("CommanderTalentLabel_%dLabel"), Index))
				->SetText(CommanderRoleLabels[Index]);
		}
	}
	else
	{
		const FLinearColor HeadingColor(0.025f, 0.025f, 0.02f, 1.0f);
		const FLinearColor HeadingShadow(0.95f, 0.92f, 0.85f, 0.65f);
		UTextBlock* Appellation = MakeScreenText(TEXT("CommanderAppellation"), 20, HeadingColor, false);
		Appellation->SetShadowColorAndOpacity(HeadingShadow);
		Appellation->SetText(FText::FromString(
			SelectedTitleString.IsEmpty()
				? FString(TEXT("统帅"))
				: SelectedTitleString));
		PlaceOnCanvas(ModeCanvas, Appellation, CapturePoint(975.0f, 129.0f), FVector2D(340.0f, 35.0f), 7);
		UTextBlock* Name = MakeScreenText(TEXT("CommanderName"), 35, HeadingColor, true);
		Name->SetShadowColorAndOpacity(HeadingShadow);
		Name->SetText(FText::FromString(SelectedNameString));
		PlaceOnCanvas(ModeCanvas, Name, CapturePoint(975.0f, 163.0f), FVector2D(380.0f, 54.0f), 7);

		const FLinearColor RoleColors[] = {
			FLinearColor(0.55f, 0.17f, 0.13f, 0.78f),
			FLinearColor(0.18f, 0.50f, 0.31f, 0.78f),
			FLinearColor(0.16f, 0.31f, 0.55f, 0.78f),
		};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float X = 975.0f + Index * 118.0f;
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("CommanderRoleBg_%d"), Index), RoleColors[Index]), CapturePoint(X, 236.0f), FVector2D(108.0f, 31.0f), 6);
			PlaceOnCanvas(ModeCanvas,
				MakeActionButton(*FString::Printf(TEXT("CommanderTalentLabel_%d"), Index),
					490 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue,
					FVector2D(108.0f, 29.0f), CommanderRoleLabels[Index], 18,
					FLinearColor::White),
				CapturePoint(X, 237.0f), FVector2D(108.0f, 29.0f), 8);
		}
	}

	// Config-only preview of the Unity TalentTop -> TalentSpecialPop click/close path.
	// No point, current-level, account-ownership or 451 talent-tree state is inferred.
	if (VisibleCommanderTalentIds.IsValidIndex(SelectedCommanderTalentIndex)
		&& SelectedRuntimeHero && GameplayCatalog)
	{
		FRokUnityCatalogHeroTalent DetailTalent;
		const int32 DetailTalentId = VisibleCommanderTalentIds[SelectedCommanderTalentIndex];
		if (GameplayCatalog->GetHeroTalent(DetailTalentId, DetailTalent))
		{
			const FString DetailName = !DetailTalent.DisplayNameZhCn.IsEmpty()
				? DetailTalent.DisplayNameZhCn : DetailTalent.DisplayNameEn;
			const FString DetailTips = !DetailTalent.DisplayTipsZhCn.IsEmpty()
				? DetailTalent.DisplayTipsZhCn : DetailTalent.DisplayTipsEn;
			if (!DetailName.IsEmpty() && !DetailTips.IsEmpty())
			{
				UClass* DetailClass = ResolveFeaturePageClass(TEXT("CommanderTalentDetail"));
				URokUiPageWidget* DetailPage = DetailClass ? CreateWidget<URokUiPageWidget>(this, DetailClass) : nullptr;
				if (DetailPage)
				{
					DetailPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					UCanvasPanelSlot* DetailSlot = ModeCanvas->AddChildToCanvas(DetailPage);
					DetailSlot->SetAnchors(FAnchors(0, 0, 1, 1));
					DetailSlot->SetOffsets(FMargin(0));
					DetailSlot->SetZOrder(15);
					if (UTextBlock* Title = DetailPage->Get<UTextBlock>(TEXT("CommanderTalentDetailTitle")))
						Title->SetText(FText::FromString(DetailName));
					if (UTextBlock* Tips = DetailPage->Get<UTextBlock>(TEXT("CommanderTalentDetailTips")))
						Tips->SetText(FText::FromString(DetailTips));
					BindPageAction(*DetailPage, TEXT("CommanderTalentDetailClose"), 493);
				}
				else
				{
					PlaceOnCanvas(ModeCanvas,
						MakeAtlasBorder(TEXT("CommanderTalentDetailFrame"), CommonAtlasPath,
							SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(430.0f, 260.0f),
							FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)),
						CapturePoint(965.0f, 285.0f), FVector2D(430.0f, 260.0f), 15);
					UTextBlock* DetailTitle = MakeScreenText(TEXT("CommanderTalentDetailTitle"),
						22, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), true);
					DetailTitle->SetText(FText::FromString(DetailName));
					DetailTitle->SetShadowColorAndOpacity(FLinearColor::Transparent);
					PlaceOnCanvas(ModeCanvas, DetailTitle, CapturePoint(986.0f, 309.0f),
						FVector2D(335.0f, 36.0f), 16);
					UTextBlock* DetailDescription = MakeScreenText(TEXT("CommanderTalentDetailTips"),
						18, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), true);
					DetailDescription->SetText(FText::FromString(DetailTips));
					DetailDescription->SetShadowColorAndOpacity(FLinearColor::Transparent);
					DetailDescription->SetAutoWrapText(true);
					DetailDescription->SetWrapTextAt(375.0f);
					PlaceOnCanvas(ModeCanvas, DetailDescription, CapturePoint(986.0f, 357.0f),
						FVector2D(375.0f, 155.0f), 16);
					PlaceOnCanvas(ModeCanvas,
						MakeActionButton(TEXT("CommanderTalentDetailClose"), 493,
							CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(44.0f)),
						CapturePoint(1340.0f, 300.0f), FVector2D(44.0f), 17);
				}
			}
		}
	}

	UClass* CommanderProgressClass = ResolveFeaturePageClass(TEXT("CommanderProgress"));
	URokUiPageWidget* CommanderProgressPage = CommanderProgressClass
		? CreateWidget<URokUiPageWidget>(this, CommanderProgressClass) : nullptr;
	bool bHasProgressPage = CommanderProgressPage
		&& CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderLevel"))
		&& CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderExp"))
		&& CommanderProgressPage->Get<UBorder>(TEXT("CommanderExpTrack"))
		&& CommanderProgressPage->Get<UBorder>(TEXT("CommanderExpFill"))
		&& CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderSkillTitle"))
		&& CommanderProgressPage->Get<UBorder>(TEXT("CommanderSkillShade"));
	for (int32 Index = 0; bHasProgressPage && Index < 6; ++Index)
	{
		bHasProgressPage = CommanderProgressPage->Get<UImage>(*FString::Printf(TEXT("CommanderStar_%d"), Index)) != nullptr;
	}
	if (bHasProgressPage)
	{
		CommanderProgressPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderSkillTitle"))->SetText(NSLOCTEXT("RokCommander", "Skills", "技能"));
		UCanvasPanelSlot* ProgressSlot = ModeCanvas->AddChildToCanvas(CommanderProgressPage);
		ProgressSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ProgressSlot->SetOffsets(FMargin(0));
		ProgressSlot->SetZOrder(5);
		UTexture2D* Atlas = LoadUiTexture(CommonAtlasPath);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			UImage* Star = CommanderProgressPage->Get<UImage>(*FString::Printf(TEXT("CommanderStar_%d"), Index));
			Star->SetVisibility(bHasSelectedHero || bSelectedOwned ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Atlas)
			{
				FSlateBrush Brush = Star->GetBrush();
				const FSlateBrush AtlasBrush = MakeAtlasBrush(Atlas,
					Index < SelectedStar && Index < SelectedMaxStar ? CommanderStarOnRect : CommanderStarOffRect,
					CommonAtlasSize, Brush.GetImageSize());
				Brush.SetResourceObject(Atlas);
				Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
				Star->SetBrush(Brush);
			}
		}
	}
	else if (bHasSelectedHero || bSelectedOwned)
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			PlaceOnCanvas(ModeCanvas,
				MakeAtlasImage(*FString::Printf(TEXT("CommanderStar_%d"), Index), CommonAtlasPath,
					Index < SelectedStar && Index < SelectedMaxStar ? CommanderStarOnRect : CommanderStarOffRect,
					CommonAtlasSize, FVector2D(48.0f)),
				CapturePoint(976.0f + Index * 57.0f, 279.0f), FVector2D(48.0f), 7);
		}
	}

	int32 StarLevelLimit = 0, RequiredExp = 0;
	if (CommanderConfig)
	{
		CommanderConfig->GetIntFieldByKey(TEXT("HeroStar"), TEXT("ID"), SelectedStar, TEXT("starLimit"), StarLevelLimit);
		CommanderConfig->GetIntFieldByKey(TEXT("HeroLevel"), TEXT("ID"), LevelKey, TEXT("exp"), RequiredExp);
	}
	UTextBlock* Level = bHasProgressPage ? CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderLevel"))
		: MakeScreenText(TEXT("CommanderLevel"), 19, FLinearColor::White);
	Level->SetText(SelectedServerHero && !SelectedServerHero->bLevelKnown
		? NSLOCTEXT("RokCommander", "OwnedLevelWaiting", "等级待同步")
		: FText::FromString(StarLevelLimit > 0 ? FString::Printf(TEXT("等级 %d/%d"), SelectedLevel, StarLevelLimit)
			: FString::Printf(TEXT("等级 %d"), SelectedLevel)));
	if (!bHasProgressPage)
	{
		PlaceOnCanvas(ModeCanvas, Level, CapturePoint(975, 333), FVector2D(260, 30), 7);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderExpTrack"), FLinearColor(0.025f, 0.035f, 0.035f, 0.66f)),
			CapturePoint(975, 365), FVector2D(415, 37), 5);
	}
	const int64 CurrentExp = SelectedServerHero ? SelectedServerHero->Exp : 0;
	const bool bShowExpFill = SelectedServerHero && SelectedServerHero->bExpKnown && RequiredExp > 0 && CurrentExp > 0;
	if (bHasProgressPage)
	{
		UBorder* Fill = CommanderProgressPage->Get<UBorder>(TEXT("CommanderExpFill"));
		Fill->SetVisibility(bShowExpFill ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowExpFill)
		{
			const double Fraction = FMath::Clamp(static_cast<double>(CurrentExp) / RequiredExp, 0.0, 1.0);
			if (UCanvasPanelSlot* FillSlot = Cast<UCanvasPanelSlot>(Fill->Slot))
			{
				const FVector2D DesignedSize = FillSlot->GetSize();
				FillSlot->SetSize(FVector2D(DesignedSize.X * Fraction, DesignedSize.Y));
			}
		}
	}
	else if (bShowExpFill)
	{
		const double Fraction = FMath::Clamp(static_cast<double>(CurrentExp) / RequiredExp, 0.0, 1.0);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderExpFill"), FLinearColor(0.13f, 0.43f, 0.14f, 0.85f)),
			CapturePoint(975, 365), FVector2D(415.0 * Fraction, 37), 6);
	}
	UTextBlock* Exp = bHasProgressPage ? CommanderProgressPage->Get<UTextBlock>(TEXT("CommanderExp"))
		: MakeScreenText(TEXT("CommanderExp"), 18, FLinearColor::White, false, ETextJustify::Center);
	Exp->SetText(RequiredExp > 0 && (!SelectedServerHero || SelectedServerHero->bExpKnown)
		? FText::FromString(FString::Printf(TEXT("经验  %lld/%d"), CurrentExp, RequiredExp))
		: NSLOCTEXT("RokCommander", "ExpWaiting", "经验待同步"));
	if (!bHasProgressPage)
		PlaceOnCanvas(ModeCanvas, Exp, CapturePoint(975, 370), FVector2D(415, 30), 7);
	if (SelectedServerHero)
	{
		UClass* StateClass = ResolveFeaturePageClass(TEXT("CommanderServerState"));
		URokUiPageWidget* StatePage = StateClass ? CreateWidget<URokUiPageWidget>(this, StateClass) : nullptr;
		UTextBlock* Progress = StatePage ? StatePage->Get<UTextBlock>(TEXT("CommanderServerProgress")) : nullptr;
		UTextBlock* TalentState = StatePage ? StatePage->Get<UTextBlock>(TEXT("CommanderServerTalentState")) : nullptr;
		if (StatePage && Progress && TalentState)
		{
			StatePage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* StateSlot = ModeCanvas->AddChildToCanvas(StatePage);
			StateSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			StateSlot->SetOffsets(FMargin(0));
			StateSlot->SetZOrder(7);
		}
		else
		{
			StatePage = nullptr;
			Progress = MakeScreenText(TEXT("CommanderServerProgress"), 17,
				FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
			TalentState = MakeScreenText(TEXT("CommanderServerTalentState"), 17,
				FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
			PlaceOnCanvas(ModeCanvas, Progress, CapturePoint(975.0f, 407.0f), FVector2D(400.0f, 30.0f), 7);
			PlaceOnCanvas(ModeCanvas, TalentState, CapturePoint(975.0f, 429.0f), FVector2D(400.0f, 30.0f), 7);
		}
		Progress->SetText(FText::FromString(FString::Printf(TEXT("星级 %s  ·  经验 %s  ·  星级经验 %s"),
			SelectedServerHero->bStarKnown ? *LexToString(SelectedServerHero->Star) : TEXT("待同步"),
			SelectedServerHero->bExpKnown ? *LexToString(SelectedServerHero->Exp) : TEXT("待同步"),
			SelectedServerHero->bStarExpKnown ? *LexToString(SelectedServerHero->StarExp) : TEXT("待同步"))));
		const FString TreeName = SelectedServerHero->bTalentIndexKnown
			? FString::Printf(TEXT("方案 %d"), SelectedServerHero->TalentIndex)
			: TEXT("当前方案待同步");
		TalentState->SetText(FText::FromString(FString::Printf(TEXT("天赋 %s · %s"),
			*TreeName, SelectedServerHero->bTalentTreesKnown
				? *FString::Printf(TEXT("%d 个方案"), SelectedServerHero->TalentTrees.Num())
				: TEXT("方案数据待同步"))));
	}
	if (!bSelectedOwned)
	{
		UClass* SummonClass = ResolveFeaturePageClass(TEXT("CommanderSummon"));
		URokUiPageWidget* SummonPage = SummonClass ? CreateWidget<URokUiPageWidget>(this, SummonClass) : nullptr;
		UButton* SummonButton = SummonPage ? SummonPage->Get<UButton>(TEXT("CommanderSummonAction")) : nullptr;
		UTextBlock* Requirement = SummonPage ? SummonPage->Get<UTextBlock>(TEXT("CommanderSummonRequirement")) : nullptr;
		UImage* Sculpture = SummonPage ? SummonPage->Get<UImage>(TEXT("CommanderSculpture")) : nullptr;
		UImage* DisabledArt = SummonPage ? SummonPage->Get<UImage>(TEXT("CommanderRecruitDisabledArt")) : nullptr;
		UImage* EnabledArt = SummonPage ? SummonPage->Get<UImage>(TEXT("CommanderSummonActionBackground")) : nullptr;
		if (SummonPage && SummonButton && Requirement && Sculpture && DisabledArt && EnabledArt)
		{
			SummonPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* SummonSlot = ModeCanvas->AddChildToCanvas(SummonPage);
			SummonSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			SummonSlot->SetOffsets(FMargin(0));
			SummonSlot->SetZOrder(6);
			Requirement->SetText(bHasSelectedHero && Items.bItemsKnown && Items.bRowsComplete
				? FText::Format(NSLOCTEXT("RokCommander", "SummonProgress", "统帅雕像 {0}/{1}"),
					FText::AsNumber(ItemCounts.FindRef(SelectedHero.GetItem)), FText::AsNumber(SelectedHero.GetItemNum))
				: FText::Format(NSLOCTEXT("RokCommander", "SummonRequirement", "召唤需要 {0} 个统帅雕像"),
					FText::AsNumber(bHasSelectedHero ? FMath::Max(1, SelectedHero.GetItemNum) : 10)));
			FIntRect SculptureRect;
			const bool bHasSculpture = ResolveCommanderHeadRect(SelectedCommanderId, SculptureRect);
			if (bHasSculpture)
			{
				if (UTexture2D* Atlas = LoadUiTexture(CommanderHeadAtlasPath))
				{
					FSlateBrush Brush = Sculpture->GetBrush();
					const FSlateBrush Source = MakeAtlasBrush(Atlas, SculptureRect, CommanderHeadAtlasSize, Brush.GetImageSize());
					Brush.SetResourceObject(Atlas);
					Brush.SetUVRegion(static_cast<FBox2f>(Source.GetUVRegion()));
					Sculpture->SetBrush(Brush);
				}
			}
			SummonPage->SetPresent(Sculpture, bHasSculpture);
			const bool bCanRecruit = ServerSnapshot.bAvailable && ServerSnapshot.bRowsComplete
				&& bHasSelectedHero && Items.bItemsKnown && Items.bRowsComplete && SelectedHero.GetItemNum > 0
				&& ItemCounts.FindRef(SelectedHero.GetItem) >= SelectedHero.GetItemNum;
			SummonPage->SetPresent(DisabledArt, !bCanRecruit);
			SummonPage->SetPresent(EnabledArt, bCanRecruit);
			SetFeaturePageText(*SummonPage, TEXT("CommanderSummonActionLabel"), NSLOCTEXT("RokCommander", "Summon", "招募"));
			BindPageAction(*SummonPage, TEXT("CommanderSummonAction"), 452);
			SummonButton->SetIsEnabled(bCanRecruit);
		}
		else
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeSolidBorder(
					TEXT("CommanderSummonPanel"),
					FLinearColor(0.08f, 0.075f, 0.055f, 0.92f)),
				CapturePoint(975.0f, 636.0f),
				FVector2D(415.0f, 65.0f),
				6);
			UTextBlock* SummonRequirement = MakeScreenText(
				TEXT("CommanderSummonRequirement"),
				18,
				FLinearColor(0.95f, 0.91f, 0.76f, 1.0f),
				false,
				ETextJustify::Center);
			SummonRequirement->SetText(bHasSelectedHero && Items.bItemsKnown && Items.bRowsComplete
				? FText::Format(NSLOCTEXT("RokCommander", "SummonProgress", "统帅雕像 {0}/{1}"),
					FText::AsNumber(ItemCounts.FindRef(SelectedHero.GetItem)), FText::AsNumber(SelectedHero.GetItemNum))
				: FText::Format(
				NSLOCTEXT("RokCommander", "SummonRequirement", "召唤需要 {0} 个统帅雕像"),
				FText::AsNumber(bHasSelectedHero
					? FMath::Max(1, SelectedHero.GetItemNum)
					: 10)));
			FIntRect SculptureRect;
			if (ResolveCommanderHeadRect(SelectedCommanderId, SculptureRect))
				PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("CommanderSculpture"), CommanderHeadAtlasPath,
					SculptureRect, CommanderHeadAtlasSize, FVector2D(54, 60)),
					CapturePoint(980, 638), FVector2D(54, 60), 7);
			PlaceOnCanvas(ModeCanvas, SummonRequirement, CapturePoint(1045, 653), FVector2D(330, 32), 7);
			const bool bCanRecruit = ServerSnapshot.bAvailable && ServerSnapshot.bRowsComplete
				&& bHasSelectedHero && Items.bItemsKnown && Items.bRowsComplete && SelectedHero.GetItemNum > 0
				&& ItemCounts.FindRef(SelectedHero.GetItem) >= SelectedHero.GetItemNum;
			if (!bCanRecruit)
			{
				PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("CommanderRecruitDisabledArt"), CommonAtlasPath,
					SpriteRect(473, 528, 202, 81), CommonAtlasSize, FVector2D(195, 64)),
					CapturePoint(1085, 713), FVector2D(195, 64), 7);
			}
			auto* Summon = MakeActionButton(
					TEXT("CommanderSummonAction"),
					452,
					bCanRecruit ? CommonAtlasPath : nullptr,
					MediumBlueButtonRect,
					CommonAtlasSize,
					FVector2D(195.0f, 64.0f),
					NSLOCTEXT("RokCommander", "Summon", "招募"),
					22,
					FLinearColor(0.03f, 0.14f, 0.25f, 1.0f));
			Summon->SetIsEnabled(bCanRecruit);
			PlaceOnCanvas(ModeCanvas, Summon, CapturePoint(1085, 713), FVector2D(195, 64), 8);
		}
	}
	UClass* ActionsClass = ResolveFeaturePageClass(TEXT("CommanderActions"));
	URokUiPageWidget* ActionsPage = ActionsClass ? CreateWidget<URokUiPageWidget>(this, ActionsClass) : nullptr;
	if (ActionsPage && ActionsPage->Get<UButton>(TEXT("CommanderBiographyAction"))
		&& ActionsPage->Get<UTextBlock>(TEXT("CommanderSelectedSkillSummary"))
		&& ActionsPage->Get<UButton>(TEXT("CommanderSkillUpgrade")) && ActionsPage->Get<UButton>(TEXT("CommanderTalent")))
	{
		ActionsPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* ActionsSlot = ModeCanvas->AddChildToCanvas(ActionsPage);
		ActionsSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ActionsSlot->SetOffsets(FMargin(0));
		ActionsSlot->SetZOrder(8);
		ActionsPage->Get<UButton>(TEXT("CommanderBiographyAction"))->SetToolTipText(NSLOCTEXT("RokCommander", "Biography", "统帅介绍"));
		BindPageAction(*ActionsPage, TEXT("CommanderBiographyAction"), 468);
		if (!bSelectedOwned)
		{
			ActionsPage->SetPresent(ActionsPage->Get<UTextBlock>(TEXT("CommanderSelectedSkillSummary")), false);
			ActionsPage->SetPresent(ActionsPage->Get<UButton>(TEXT("CommanderSkillUpgrade")), false);
			ActionsPage->SetPresent(ActionsPage->Get<UButton>(TEXT("CommanderTalent")), false);
		}
	}
	else
	{
		ActionsPage = nullptr;
		auto* BioButton = MakeActionButton(TEXT("CommanderBiographyAction"), 468, CommonAtlasPath,
			SpriteRect(611, 227, 50, 48), CommonAtlasSize, FVector2D(38, 36));
		BioButton->SetToolTipText(NSLOCTEXT("RokCommander", "Biography", "统帅介绍"));
		PlaceOnCanvas(ModeCanvas, BioButton, CapturePoint(1352, 129), FVector2D(38), 8);
	}
	if (bCommanderBiographyOpen)
	{
		UClass* BiographyClass = ResolveFeaturePageClass(TEXT("CommanderBiography"));
		URokUiPageWidget* BiographyPage = BiographyClass ? CreateWidget<URokUiPageWidget>(this, BiographyClass) : nullptr;
		if (BiographyPage)
		{
			BiographyPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UCanvasPanelSlot* BiographySlot = ModeCanvas->AddChildToCanvas(BiographyPage);
			BiographySlot->SetAnchors(FAnchors(0, 0, 1, 1));
			BiographySlot->SetOffsets(FMargin(0));
			BiographySlot->SetZOrder(16);
			if (UTextBlock* Capacity = BiographyPage->Get<UTextBlock>(TEXT("CommanderCapacity")))
				Capacity->SetText(FText::Format(NSLOCTEXT("RokCommander", "CommanderIdentity", "稀有度 {0}  ·  文明 {1}"),
					FText::AsNumber(bHasSelectedHero ? SelectedHero.Rare : 0),
					FText::AsNumber(bHasSelectedHero ? SelectedHero.Civilization : 0)));
			FString Preview = SelectedDescriptionString.IsEmpty() ? TEXT("暂无统帅介绍。") : SelectedDescriptionString;
			if (CommanderBiographyCharacterLimit > 0 && Preview.Len() > CommanderBiographyCharacterLimit)
			{
				Preview = Preview.Left(CommanderBiographyCharacterLimit) + CommanderBiographyEllipsis;
			}
			if (UTextBlock* Description = BiographyPage->Get<UTextBlock>(TEXT("CommanderDescription")))
				Description->SetText(FText::FromString(Preview));
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderBiographyPanel"), FLinearColor(0.035f, 0.035f, 0.028f, 0.98f)),
				CapturePoint(965, 182), FVector2D(430, 255), 16);
			UTextBlock* Capacity = MakeScreenText(TEXT("CommanderCapacity"), 20, FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
			Capacity->SetText(FText::Format(
				NSLOCTEXT(
					"RokCommander",
					"CommanderIdentity",
					"稀有度 {0}  ·  文明 {1}"),
				FText::AsNumber(bHasSelectedHero ? SelectedHero.Rare : 0),
				FText::AsNumber(bHasSelectedHero ? SelectedHero.Civilization : 0)));
			PlaceOnCanvas(ModeCanvas, Capacity, CapturePoint(975, 195), FVector2D(400, 34), 17);
			UTextBlock* CommanderDescription = MakeScreenText(
				TEXT("CommanderDescription"),
				15,
				FLinearColor(0.88f, 0.87f, 0.84f, 1.0f));
			FString CommanderDescriptionPreview = SelectedDescriptionString.IsEmpty()
				? TEXT("暂无统帅介绍。")
				: SelectedDescriptionString;
			if (CommanderDescriptionPreview.Len() > 58)
			{
				CommanderDescriptionPreview = CommanderDescriptionPreview.Left(58)
					+ TEXT("…");
			}
			CommanderDescription->SetText(FText::FromString(
				CommanderDescriptionPreview));
			CommanderDescription->SetAutoWrapText(true);
			CommanderDescription->SetWrapTextAt(405.0f);
			PlaceOnCanvas(
				ModeCanvas,
				CommanderDescription,
				CapturePoint(975, 235),
				FVector2D(405, 175), 17);
		}
	}

	if (!bHasProgressPage)
	{
		UTextBlock* SkillTitle = MakeScreenText(TEXT("CommanderSkillTitle"), 20, FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
		SkillTitle->SetText(NSLOCTEXT("RokCommander", "Skills", "技能"));
		PlaceOnCanvas(ModeCanvas, SkillTitle, CapturePoint(975.0f, 474.0f), FVector2D(160.0f, 34.0f), 7);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderSkillShade"), FLinearColor(0.08f, 0.06f, 0.045f, 0.55f)), CapturePoint(972.0f, 504.0f), FVector2D(415.0f, 101.0f), 5);
	}
	UClass* CommanderSkillItemClass = ResolveFeaturePageClass(TEXT("CommanderSkillItem"));
	UCanvasPanel* SkillHost = nullptr;
	const UCanvasPanelSlot* SkillCellSlot = nullptr;
	FVector2D SkillStep = FVector2D::ZeroVector;
	if (UClass* SkillBarClass = CommanderSkillItemClass && !VisibleCommanderSkillIds.IsEmpty()
		? ResolveFeaturePageClass(TEXT("CommanderSkillBar")) : nullptr)
	{
		if (auto* SkillBar = CreateWidget<URokUiPageWidget>(this, SkillBarClass))
		{
			auto* Host = SkillBar->Get<UCanvasPanel>(TEXT("CommanderSkillHost"));
			auto* Cell = SkillBar->Get<UBorder>(TEXT("CommanderSkillCell"));
			auto* Next = SkillBar->Get<UBorder>(TEXT("CommanderSkillNext"));
			const auto* HostSlot = Host ? Cast<UCanvasPanelSlot>(Host->Slot) : nullptr;
			const auto* CellSlot = Cell ? Cast<UCanvasPanelSlot>(Cell->Slot) : nullptr;
			const auto* NextSlot = Next ? Cast<UCanvasPanelSlot>(Next->Slot) : nullptr;
			if (HostSlot && CellSlot && NextSlot && Cell->GetParent() == Host && Next->GetParent() == Host
				&& CellSlot->GetSize().X > 0 && CellSlot->GetSize().Y > 0)
			{
				SkillHost = Host;
				SkillCellSlot = CellSlot;
				SkillStep = NextSlot->GetPosition() - CellSlot->GetPosition();
				Cell->SetVisibility(ESlateVisibility::Collapsed);
				Next->SetVisibility(ESlateVisibility::Collapsed);
				SkillBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				UCanvasPanelSlot* BarSlot = ModeCanvas->AddChildToCanvas(SkillBar);
				BarSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				BarSlot->SetOffsets(FMargin(0));
				BarSlot->SetZOrder(HostSlot->GetZOrder());
			}
		}
	}
	for (int32 Index = 0; Index < VisibleCommanderSkillIds.Num(); ++Index)
	{
		const float X = 978.0f + Index * 82.5f;
		const int32 SkillId = VisibleCommanderSkillIds[Index];
		const FRokServerHeroSkill* AccountSkill = SelectedServerHero
			? SelectedServerHero->Skills.FindByPredicate([SkillId](const auto& Entry) { return Entry.SkillId == SkillId; })
			: nullptr;
		FRokUnityCatalogHeroSkill Skill;
		const bool bHasSkill = GameplayCatalog
			&& GameplayCatalog->GetHeroSkill(
				VisibleCommanderSkillIds[Index],
				Skill);
		const int32 SkillLevel = SelectedServerHero
			? (AccountSkill && AccountSkill->bLevelKnown ? AccountSkill->Level : 0)
			: (bHasSkill && Skill.Open == 1 ? 1 : 0);
		FIntRect SkillRect;
		bool bHasSkillIcon = bHasSkill
			&& (Skill.IconAtlas.Equals(
					TEXT("hero_skill"),
					ESearchCase::IgnoreCase)
				|| Skill.IconAtlas.Equals(
					TEXT("hero_skill2"),
					ESearchCase::IgnoreCase))
			&& Skill.IconWidth > 0
			&& Skill.IconHeight > 0
			&& Skill.IconAtlasWidth > 0
			&& Skill.IconAtlasHeight > 0;
		if (bHasSkillIcon)
		{
			SkillRect = FIntRect(
				Skill.IconX,
				Skill.IconY,
				Skill.IconX + Skill.IconWidth,
				Skill.IconY + Skill.IconHeight);
		}
		else if (SelectedCommanderId == 2007
			&& Index < UE_ARRAY_COUNT(CommanderSkillRects))
		{
			SkillRect = CommanderSkillRects[Index];
			bHasSkillIcon = true;
		}
		URokUiPageWidget* SkillPage = CommanderSkillItemClass
			? CreateWidget<URokUiPageWidget>(this, CommanderSkillItemClass) : nullptr;
		UImage* ItemIcon = SkillPage ? SkillPage->Get<UImage>(TEXT("CommanderSkillIcon")) : nullptr;
		UBorder* ItemPending = SkillPage ? SkillPage->Get<UBorder>(TEXT("CommanderSkillPending")) : nullptr;
		const UBorder* ItemSelectedStyle = SkillPage ? SkillPage->Get<UBorder>(TEXT("CommanderSkillPendingSelectedStyle")) : nullptr;
		UTextBlock* ItemLevel = SkillPage ? SkillPage->Get<UTextBlock>(TEXT("CommanderSkillIndex")) : nullptr;
		UButton* ItemAction = SkillPage ? SkillPage->Get<UButton>(TEXT("CommanderSkillAction")) : nullptr;
		if (ItemIcon && ItemPending && ItemSelectedStyle && ItemLevel && ItemAction)
		{
			SkillPage->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
			SkillPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (SkillHost)
			{
				UCanvasPanelSlot* Slot = SkillHost->AddChildToCanvas(SkillPage);
				Slot->SetLayout(SkillCellSlot->GetLayout());
				Slot->SetPosition(SkillCellSlot->GetPosition() + SkillStep * Index);
				Slot->SetZOrder(SkillCellSlot->GetZOrder());
			}
			else
			{
				PlaceOnCanvas(ModeCanvas, SkillPage, CapturePoint(X, 513.0f), FVector2D(74.0f), 7);
			}
			if (bHasSkillIcon)
			{
				const TCHAR* SkillTexturePath = Skill.IconAtlas.Equals(TEXT("hero_skill"), ESearchCase::IgnoreCase)
					? CommanderSkillAtlasPath : CommanderSkill2AtlasPath;
				UTexture2D* Atlas = LoadUiTexture(SkillTexturePath);
				if (SkillLevel == 0)
				{
					const TCHAR* GrayPath = SkillTexturePath == CommanderSkillAtlasPath
						? TEXT("Textures/hero_skill_gray.png") : TEXT("Textures/hero_skill2_gray.png");
					if (UTexture2D* GrayAtlas = LoadCommanderPreviewTexture(GrayPath)) Atlas = GrayAtlas;
					else ItemIcon->SetIsEnabled(false);
				}
				if (Atlas)
				{
					FSlateBrush Brush = ItemIcon->GetBrush();
					const FSlateBrush AtlasBrush = MakeAtlasBrush(Atlas, SkillRect, CommanderSkillAtlasSize, Brush.GetImageSize());
					Brush.SetResourceObject(Atlas);
					Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
					ItemIcon->SetBrush(Brush);
				}
			}
			else if (Index == SelectedCommanderSkillIndex)
			{
				ItemPending->SetBrushColor(ItemSelectedStyle->GetBrushColor());
			}
			ItemIcon->SetVisibility(bHasSkillIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			ItemPending->SetVisibility(bHasSkillIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
			ItemLevel->SetText(SkillLevel > 0 ? FText::AsNumber(SkillLevel) : FText::GetEmpty());
			BindPageAction(*SkillPage, TEXT("CommanderSkillAction"), 460 + Index);
			continue;
		}
		if (bHasSkillIcon)
		{
			const TCHAR* SkillTexturePath = Skill.IconAtlas.Equals(
				TEXT("hero_skill"),
				ESearchCase::IgnoreCase)
				? CommanderSkillAtlasPath
				: CommanderSkill2AtlasPath;
			UImage* SkillImage = MakeAtlasImage(
				*FString::Printf(TEXT("CommanderSkillIcon_%d"), Index),
				SkillTexturePath,
				SkillRect,
				CommanderSkillAtlasSize,
				FVector2D(74.0f));
			if (SkillLevel == 0)
			{
				const TCHAR* GrayPath = SkillTexturePath == CommanderSkillAtlasPath
					? TEXT("Textures/hero_skill_gray.png") : TEXT("Textures/hero_skill2_gray.png");
				if (auto* GrayAtlas = LoadCommanderPreviewTexture(GrayPath))
					SkillImage->SetBrush(MakeAtlasBrush(GrayAtlas, SkillRect, CommanderSkillAtlasSize, FVector2D(74)));
				else
					SkillImage->SetIsEnabled(false);
			}
			PlaceOnCanvas(
				ModeCanvas,
				SkillImage,
				CapturePoint(X, 513.0f),
				FVector2D(74.0f),
				7);
		}
		else
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeSolidBorder(
					*FString::Printf(TEXT("CommanderSkillPending_%d"), Index),
					Index == SelectedCommanderSkillIndex
						? FLinearColor(0.46f, 0.34f, 0.10f, 0.92f)
						: FLinearColor(0.10f, 0.10f, 0.105f, 0.92f)),
				CapturePoint(X, 513.0f),
				FVector2D(74.0f),
				7);
		}
		UTextBlock* SkillIndexLabel = MakeScreenText(
			*FString::Printf(TEXT("CommanderSkillIndex_%d"), Index),
			13,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		SkillIndexLabel->SetText(SkillLevel > 0 ? FText::AsNumber(SkillLevel) : FText::GetEmpty());
		PlaceOnCanvas(
			ModeCanvas,
			SkillIndexLabel,
			CapturePoint(X + 2.0f, 565.0f),
			FVector2D(70.0f, 20.0f),
			8);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("CommanderSkillAction_%d"), Index), 460 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(74.0f)), CapturePoint(X, 513.0f), FVector2D(74.0f), 9);
	}
	if (bSelectedOwned)
	{
		UTextBlock* SelectedSkillSummary = ActionsPage
			? ActionsPage->Get<UTextBlock>(TEXT("CommanderSelectedSkillSummary"))
			: MakeScreenText(TEXT("CommanderSelectedSkillSummary"), 15, FLinearColor(0.96f, 0.90f, 0.72f, 1.0f));
		if (bHasSelectedCommanderSkill)
		{
			const FString SkillName = !SelectedCommanderSkill.DisplayNameZhCn.IsEmpty()
				? SelectedCommanderSkill.DisplayNameZhCn
				: SelectedCommanderSkill.DisplayNameEn;
			const FString SkillType = !SelectedCommanderSkill.DisplayTypeZhCn.IsEmpty()
				? SelectedCommanderSkill.DisplayTypeZhCn
				: SelectedCommanderSkill.DisplayTypeEn;
			const FRokServerHeroSkill* ServerSkill = SelectedServerHero
				? SelectedServerHero->Skills.FindByPredicate([&](const FRokServerHeroSkill& Entry)
					{ return Entry.SkillId == VisibleCommanderSkillIds[SelectedCommanderSkillIndex]; })
				: nullptr;
			const FString SkillLevel = ServerSkill && ServerSkill->bLevelKnown && ServerSkill->Level > 0
				? FString::Printf(TEXT("Lv.%d"), ServerSkill->Level)
				: SelectedServerHero && !SelectedServerHero->bSkillsKnown ? TEXT("等级待同步")
				: !SelectedServerHero && SelectedCommanderSkill.Open == 1 ? TEXT("Lv.1")
				: SelectedCommanderSkill.DisplayOpenZhCn;
			SelectedSkillSummary->SetText(FText::FromString(FString::Printf(
				TEXT("%s  ·  %s  ·  %s"),
				SkillName.IsEmpty() ? TEXT("技能") : *SkillName,
				SkillType.IsEmpty() ? TEXT("技能") : *SkillType,
				*SkillLevel)));
		}
		else
		{
			SelectedSkillSummary->SetText(NSLOCTEXT(
				"RokCommander",
				"NoSkillInformation",
				"暂无技能信息。"));
		}
		if (ActionsPage)
		{
			SetFeaturePageText(*ActionsPage, TEXT("CommanderSkillUpgradeLabel"), NSLOCTEXT("RokCommander", "SkillUpgrade", "技能升级"));
			SetFeaturePageText(*ActionsPage, TEXT("CommanderTalentLabel"), NSLOCTEXT("RokCommander", "Talent", "天赋"));
			BindPageAction(*ActionsPage, TEXT("CommanderSkillUpgrade"), 450);
			BindPageAction(*ActionsPage, TEXT("CommanderTalent"), 451);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, SelectedSkillSummary, CapturePoint(978.0f, 606.0f), FVector2D(405.0f, 28.0f), 8);
			PlaceOnCanvas(ModeCanvas,
				MakeActionButton(TEXT("CommanderSkillUpgrade"), 450, CommonAtlasPath, LargeBlueButtonRect, CommonAtlasSize, FVector2D(195.0f, 66.0f), NSLOCTEXT("RokCommander", "SkillUpgrade", "技能升级"), 25, FLinearColor(0.03f, 0.14f, 0.25f, 1.0f)),
				CapturePoint(978.0f, 724.0f), FVector2D(195.0f, 66.0f), 8);
			PlaceOnCanvas(ModeCanvas,
				MakeActionButton(TEXT("CommanderTalent"), 451, CommonAtlasPath, LargeBlueButtonRect, CommonAtlasSize, FVector2D(195.0f, 66.0f), NSLOCTEXT("RokCommander", "Talent", "天赋"), 25, FLinearColor(0.03f, 0.14f, 0.25f, 1.0f)),
				CapturePoint(1192.0f, 724.0f), FVector2D(195.0f, 66.0f), 8);
		}
	}
	// Source UI_Item_CaptainSkill_SubView.OnSkillClick supplies configuration tips.
	// Keep the five skill actions below this overlay clickable for real switching.
	if (VisibleCommanderSkillIds.IsValidIndex(SelectedCommanderSkillDetailIndex)
		&& GameplayCatalog)
	{
		FRokUnityCatalogHeroSkill DetailSkill;
		FRokCommanderSkillReadonlyDetail Detail;
		if (GameplayCatalog->GetHeroSkill(
			VisibleCommanderSkillIds[SelectedCommanderSkillDetailIndex], DetailSkill)
			&& RokCommanderSkillReadonlyDetail::Build(DetailSkill, Detail))
		{
			UClass* DetailClass = ResolveFeaturePageClass(TEXT("CommanderSkillDetail"));
			URokUiPageWidget* DetailPage = DetailClass ? CreateWidget<URokUiPageWidget>(this, DetailClass) : nullptr;
			if (DetailPage)
			{
				DetailPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				UCanvasPanelSlot* DetailSlot = ModeCanvas->AddChildToCanvas(DetailPage);
				DetailSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				DetailSlot->SetOffsets(FMargin(0));
				DetailSlot->SetZOrder(15);
				if (UTextBlock* Title = DetailPage->Get<UTextBlock>(TEXT("CommanderSkillDetailTitle")))
					Title->SetText(FText::FromString(Detail.Title));
				if (UTextBlock* Body = DetailPage->Get<UTextBlock>(TEXT("CommanderSkillDetailBody")))
					Body->SetText(FText::FromString(Detail.Body));
				BindPageAction(*DetailPage, TEXT("CommanderSkillDetailClose"), 465);
			}
			else
			{
				PlaceOnCanvas(ModeCanvas,
					MakeAtlasBorder(TEXT("CommanderSkillDetailFrame"), CommonAtlasPath,
						SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(478.0f, 405.0f),
						FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)),
					CapturePoint(922.0f, 170.0f), FVector2D(478.0f, 405.0f), 15);
				UTextBlock* DetailTitle = MakeScreenText(TEXT("CommanderSkillDetailTitle"),
					22, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f));
				DetailTitle->SetText(FText::FromString(Detail.Title));
				DetailTitle->SetShadowColorAndOpacity(FLinearColor::Transparent);
				PlaceOnCanvas(ModeCanvas, DetailTitle, CapturePoint(943.0f, 187.0f),
					FVector2D(385.0f, 35.0f), 16);
				UTextBlock* DetailBody = MakeScreenText(TEXT("CommanderSkillDetailBody"),
					16, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f));
				DetailBody->SetText(FText::FromString(Detail.Body));
				DetailBody->SetShadowColorAndOpacity(FLinearColor::Transparent);
				DetailBody->SetAutoWrapText(true);
				DetailBody->SetWrapTextAt(418.0f);
				UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(
					UScrollBox::StaticClass(), TEXT("CommanderSkillDetailScroll"));
				DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
				DetailScroll->AddChild(DetailBody);
				PlaceOnCanvas(ModeCanvas, DetailScroll, CapturePoint(943.0f, 232.0f),
					FVector2D(432.0f, 322.0f), 16);
				PlaceOnCanvas(ModeCanvas,
					MakeActionButton(TEXT("CommanderSkillDetailClose"), 465,
						CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(44.0f)),
					CapturePoint(1340.0f, 184.0f), FVector2D(44.0f), 17);
			}
		}
	}
	for (int32 Index = CommanderDataFirstChild; Index < ModeCanvas->GetChildrenCount(); ++Index)
	{
		CommanderDataAnimationWidgets.Add(ModeCanvas->GetChildAt(Index));
	}
	UTextBlock* Power = CommanderChromePage ? CommanderChromePage->Get<UTextBlock>(TEXT("CommanderPower")) : nullptr;
	if (CommanderChromePage)
	{
		CommanderChromePage->Get<UCanvasPanel>(TEXT("CommanderPowerFooter"))->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (!Power)
	{
		auto* Footer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CommanderPowerFooter"));
		Footer->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* FooterSlot = CommanderChrome->AddChildToCanvas(Footer);
		FooterSlot->SetAnchors(FAnchors(0.5f, 1));
		FooterSlot->SetAlignment(FVector2D(0.5f, 1));
		FooterSlot->SetPosition(FVector2D(0, -6));
		FooterSlot->SetSize(FVector2D(390, 36));
		PlaceOnCanvas(Footer, MakeSolidBorder(TEXT("CommanderPowerShade"), FLinearColor(0.09f, 0.06f, 0.04f, 0.56f)),
			FVector2D::ZeroVector, FVector2D(390, 36));
		Power = MakeScreenText(TEXT("CommanderPower"), 20, FLinearColor(0.94f, 0.92f, 0.88f, 1.0f), false, ETextJustify::Center);
		PlaceOnCanvas(Footer, Power, FVector2D(0, 3), FVector2D(390, 32), 1);
	}
	Power->SetText(SelectedServerHero && !SelectedServerHero->bPowerKnown
		? NSLOCTEXT("RokCommander", "PowerWaiting", "战力数据待同步")
		: FText::Format(NSLOCTEXT("RokCommander", "TotalCommanderPower", "统帅战力：{0}"),
			FText::AsNumber(SelectedPower)));
	BuildCommanderProgression(SelectedServerHero);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok commander screen: hero=%d owned=%d sort=%d roster=%d stage=hero_show_boundary."),
		SelectedCommanderId,
		bSelectedOwned ? 1 : 0,
		SelectedCommanderSortType,
		VisibleCommanderIds.Num());
	if (RosterPage)
		CommanderRosterAnimationWidgets.Add(RosterPage);
	// Keep existing canvas slots/z-order; only the source list subtree moves.
	// Animate direct children only, so scrolling rows don't get transformed twice.
	for (UWidget* Child : ModeCanvas->GetAllChildren())
	{
		const FString ChildName = Child->GetName();
		if (ChildName.StartsWith(TEXT("CommanderRoster")) || ChildName.StartsWith(TEXT("CommanderSort"))
			|| ChildName == TEXT("CommanderOwnedCount"))
			CommanderRosterAnimationWidgets.Add(Child);
	}
	AdvanceCommanderRosterAnimation(0.0f);
}
