void URokUnityFeatureScreenWidget::BindMailPageControls(const URokUiPageWidget& Page)
{
	for (int32 Tab = 0; Tab < 6; ++Tab)
	{
		const FString Prefix = FString::Printf(TEXT("MailTab_%d"), Tab);
		const bool bSelected = Tab == SelectedMailTab;
		Page.SetShown(Page.Get<UCanvasPanel>(*(Prefix + TEXT("Selected"))), bSelected);
		Page.SetShown(Page.Get<UCanvasPanel>(*(Prefix + TEXT("Normal"))), !bSelected);
		BindPageAction(Page, *Prefix, 200 + Tab);
	}
	BindPageAction(Page, TEXT("MailCompose"), 241);
}

void URokUnityFeatureScreenWidget::PreviewMailReferenceSelection()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokNoNet"))) return;
	const int32 Tab = ReadLocalUiPreviewOption(TEXT("RokMailReferencePreviewTab="), 1, 5);
	const int32 Row = ReadLocalUiPreviewOption(TEXT("RokMailReferencePreviewRow="), 1, 5);
	FName ActionName;
	if (Tab != INDEX_NONE)
	{
		ActionName = *FString::Printf(TEXT("MailTab_%d"), Tab);
	}
	else if (Row != INDEX_NONE)
	{
		ActionName = *FString::Printf(TEXT("MailRowHit_%d"), Row);
	}
	if (!ActionName.IsNone())
	{
		if (auto* Button = Cast<UButton>(RokUiFind::Find(this, ActionName)); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok local mail reference preview: clicked %s."), *ActionName.ToString());
		}
	}

	const int32 RewardIndex = ReadLocalUiPreviewOption(TEXT("RokMailRewardPreviewTooltip="), 0, 31);
	if (RewardIndex != INDEX_NONE && GetWorld())
	{
		FTimerHandle PreviewDelay;
		GetWorld()->GetTimerManager().SetTimer(PreviewDelay, FTimerDelegate::CreateWeakLambda(this, [this, RewardIndex]()
		{
			if (ActiveMode != ERokUnityFeatureScreenMode::Mail || !bReferenceMailCaptureState || SelectedMailTab != 0) return;
			const FName HitName(!bReadOnlyMailRewardFixtureState && RenderedMailRewardRows.Num() == 1
				? TEXT("MailRewardItemHit") : *FString::Printf(TEXT("MailRewardFixtureHit_%d"), RewardIndex));
			if (auto* Hit = Cast<UButton>(RokUiFind::Find(this, HitName)); Hit && Hit->GetIsEnabled())
			{
				Hit->OnClicked.Broadcast();
				UE_LOG(LogTemp, Display, TEXT("Rok local mail reward tooltip preview: clicked %s."), *HitName.ToString());
			}
		}), 0.6f, false);
	}
}

void URokUnityFeatureScreenWidget::AttachMailGiftMesh(int32 Index, UCanvasPanel& Pulse)
{
	// The authored parent pulse and the source SkeletonGraphic have separate clocks.
	// The index keeps the same mesh alive across row selection and tab rebuilds.
	auto& Mesh = MailAttachmentMeshes.FindOrAdd(Index);
	if (!Mesh)
	{
		Mesh = WidgetTree->ConstructWidget<URokMailAttachmentMeshWidget>(URokMailAttachmentMeshWidget::StaticClass(),
			*FString::Printf(TEXT("MailAttachmentMesh_%d"), Index));
		Mesh->LoadSource();
	}
	Mesh->RemoveFromParent();
	Mesh->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(&Pulse, Mesh, FVector2D::ZeroVector, FVector2D(28.0f, 25.0f), 1);
	MailAttachmentPulse.Register(Index, &Pulse);
}

bool URokUnityFeatureScreenWidget::TryBuildMailReferencePage(const FRokMailRewardTemplate& MailTemplate,
	bool bHasMailTemplate, const FString& MailTemplateError)
{
	UClass* RowClass = ResolveFeaturePageClass(TEXT("MailReferenceRow"));
	UClass* SelectedRowClass = ResolveFeaturePageClass(TEXT("MailReferenceSelectedRow"));
	auto* Page = RowClass && SelectedRowClass ? MountModePage(TEXT("MailReference")) : nullptr;
	if (!Page) return false;

	auto* List = Page->Get<UScrollBox>(TEXT("MailListScroll"));
	auto* Detail = Page->Get<UScrollBox>(TEXT("MailDetailScroll"));
	auto* DetailSize = Page->Get<USizeBox>(TEXT("MailDetailContentSize"));
	auto* DetailContent = Page->Get<UCanvasPanel>(TEXT("MailDetailContent"));
	if (!List || !Detail || !DetailSize || !DetailContent)
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		return false;
	}

	BindMailPageControls(*Page);
	SetFeaturePageText(*Page, TEXT("MailReferencePreviewNotice"), bReadOnlyMailRewardFixtureState
		? NSLOCTEXT("RokFeature", "MailRewardFixtureNotice", "只读奖励夹具 · 不是当前账号邮件")
		: NSLOCTEXT("RokFeature", "MailReferencePreviewNotice", "Unity 截图参考态 · 不是当前账号邮件"));
	const bool bHasReferenceMail = SelectedMailTab == 0;
	UE_LOG(LogTemp, Display, TEXT("Rok mail screen: source=UI_Win_Mail logical_page=%d rendered_rows=%d authority=%s."),
		SelectedMailTab, bHasReferenceMail ? 6 : 0,
		bReadOnlyMailRewardFixtureState ? TEXT("read_only_reward_fixture") : TEXT("reference_fixture"));
	Page->SetPresent(Page->Get<UTextBlock>(TEXT("MailEmptyState")), !bHasReferenceMail);
	Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("MailDetailPanel")), bHasReferenceMail);
	Page->SetPresent(Detail, bHasReferenceMail);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Footer = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("MailFooter_%d"), Index));
		Page->SetPresent(Footer, bHasReferenceMail);
		if (bHasReferenceMail)
		{
			BindPageAction(*Page, *FString::Printf(TEXT("MailFooterHit_%d"), Index), 250 + Index);
		}
	}
	if (!bHasReferenceMail)
	{
		return true;
	}

	for (int32 Index = 0; Index < 6; ++Index)
	{
		auto* Row = CreateWidget<URokUiPageWidget>(this, Index == SelectedMailIndex ? SelectedRowClass : RowClass);
		auto* AttachmentHost = Row ? Row->Get<UCanvasPanel>(TEXT("AttachmentPulse")) : nullptr;
		if (!AttachmentHost)
		{
			auto* Container = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("MailRowContainer_%d"), Index));
			PlaceOnCanvas(Container, MakeMailRow(Index, bHasMailTemplate ? &MailTemplate : nullptr),
				FVector2D(10.75f, 0.0f), FVector2D(457.0f, 114.24f), 1);
			List->AddChild(WrapFixedSize(Container,
				*FString::Printf(TEXT("MailRowSize_%d"), Index), FVector2D(478.5f, 114.0f)));
			continue;
		}
		Row->LegacyPrefix = TEXT("Mail");
		Row->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SetFeaturePageText(*Row, TEXT("RowTitle"), bHasMailTemplate && Index == 0
			? FText::FromString(MailTemplate.Title) : NSLOCTEXT("RokFeature", "MailRowReward", "活动奖励补发"));
		SetFeaturePageText(*Row, TEXT("RowSubject"), Index == 1
			? NSLOCTEXT("RokFeature", "MailSubjectBanquet", "英豪盛宴活动奖励补发")
			: bHasMailTemplate && Index == 0 ? FText::FromString(MailTemplate.Subtitle)
				: NSLOCTEXT("RokFeature", "MailSubjectConquest", "征服之始活动奖励补发"));
		Row->SetPresent(Row->Get<UImage>(TEXT("Unread")), Index > 0);
		Row->SetPresent(AttachmentHost, Index != 1);
		if (Index != 1)
		{
			AttachMailGiftMesh(Index, *AttachmentHost);
		}
		BindPageAction(*Row, TEXT("RowHit"), 220 + Index);
		List->AddChild(Row);
	}

	SetFeaturePageText(*Page, TEXT("MailDetailTitle"), bHasMailTemplate
		? FText::FromString(MailTemplate.Title) : FText::GetEmpty());
	SetFeaturePageText(*Page, TEXT("MailDetailSub"), SelectedMailIndex == 1
		? NSLOCTEXT("RokFeature", "MailBanquetDetailSubject", "英豪盛宴活动奖励补发")
		: bHasMailTemplate ? FText::FromString(MailTemplate.Subtitle) : FText::GetEmpty());
	const bool bMissingBody = SelectedMailIndex == 1 && RenderedMailRewardRows.IsEmpty();
	if (auto* Unavailable = Page->Get<UTextBlock>(TEXT("MailDetailUnavailable")))
	{
		Unavailable->SetText(bReadOnlyMailRewardFixtureState
			? NSLOCTEXT("RokFeature", "MailBanquetFixtureBodyUnavailable", "该参考邮件正文尚未迁入本地预览。")
			: NSLOCTEXT("RokFeature", "MailBanquetUnavailable", "该邮件正文与附件尚未迁入本地预览。"));
		Page->SetPresent(Unavailable, bMissingBody);
	}
	FString Suffix = bHasMailTemplate ? MailTemplate.BodySuffix : FString();
	FString FinalLine;
	if (Suffix.Len() >= 4)
	{
		FinalLine = Suffix.Right(4);
		Suffix.LeftChopInline(4);
	}
	const FName BodyFields[] = {TEXT("MailDetailGreeting"), TEXT("MailDetailPrefix"),
		TEXT("MailDetailHighlight"), TEXT("MailDetailSuffix"), TEXT("MailDetailFinalLine")};
	const FText BodyValues[] = {FText::FromString(bHasMailTemplate ? MailTemplate.BodyGreeting : MailTemplateError),
		FText::FromString(MailTemplate.BodyPrefix), FText::FromString(MailTemplate.BodyActivityQuoted),
		FText::FromString(Suffix), FText::FromString(FinalLine)};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BodyFields); ++Index)
	{
		if (auto* Label = Page->Get<UTextBlock>(BodyFields[Index]))
		{
			Label->SetText(BodyValues[Index]);
			Page->SetPresent(Label, !bMissingBody);
		}
	}
	const int32 GiftRowCount = FMath::DivideAndRoundUp(RenderedMailRewardRows.Num(), 5);
	float GiftEndY = 237.0f + (GiftRowCount > 0 ? 140.0f + (GiftRowCount - 1) * 170.0f : 0.0f);
	if (!bMissingBody || bReadOnlyMailRewardFixtureState)
	{
		const bool bSingleReferenceCard = !bReadOnlyMailRewardFixtureState && RenderedMailRewardRows.Num() == 1;
		const TOptional<float> AuthoredGiftEnd = bSingleReferenceCard ? TOptional<float>() : TryBuildMailRewardGrid(*DetailContent);
		if (AuthoredGiftEnd.IsSet())
		{
			GiftEndY = AuthoredGiftEnd.GetValue();
		}
		else
		{
			BuildMailRewards(DetailContent);
		}
	}
	const float ClaimY = GiftEndY + 44.0f;
	DetailSize->SetHeightOverride(FMath::Max(590.0f, ClaimY + 84.0f));
	const bool bHasClaimButton = SelectedMailIndex != 1 || bReadOnlyMailRewardFixtureState;
	auto* ClaimButton = Page->Get<UButton>(TEXT("MailClaim"));
	auto* ClaimLabel = Page->Get<UTextBlock>(TEXT("MailClaimLabel"));
	Page->SetPresent(ClaimButton, bHasClaimButton);
	Page->SetPresent(ClaimLabel, bHasClaimButton);
	if (bHasClaimButton)
	{
		BindPageAction(*Page, TEXT("MailClaim"), 240);
		if (ClaimLabel) ClaimLabel->SetText(FText::FromString(MailTemplate.ClaimLabel));
		if (auto* ButtonSlot = ClaimButton ? Cast<UCanvasPanelSlot>(ClaimButton->Slot) : nullptr)
		{
			ButtonSlot->SetPosition(FVector2D(ButtonSlot->GetPosition().X, ClaimY));
		}
		if (auto* LabelSlot = ClaimLabel ? Cast<UCanvasPanelSlot>(ClaimLabel->Slot) : nullptr)
		{
			LabelSlot->SetPosition(FVector2D(LabelSlot->GetPosition().X, ClaimY + 25.0f));
		}
		BuildMailRewardTooltip();
	}
	return true;
}

TOptional<float> URokUnityFeatureScreenWidget::TryBuildMailRewardGrid(UCanvasPanel& DetailContent)
{
	UClass* Class = ResolveFeaturePageClass(TEXT("MailRewardGrid"));
	auto* GridPage = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	if (!GridPage)
	{
		return {};
	}
	auto* Grid = GridPage->Get<UCanvasPanel>(TEXT("MailRewardFixtureGrid"));
	auto* Layout = Grid ? Cast<UCanvasPanelSlot>(Grid->Slot) : nullptr;
	auto* Empty = GridPage->Get<UTextBlock>(TEXT("MailRewardFixtureEmpty"));
	if (!Layout || !Empty)
	{
		return {};
	}
	const float GiftTop = Layout->GetPosition().Y;
	float GridHeight = 0.0f;
	const bool bEmptyPayload = bReadOnlyMailRewardFixtureState && MailRewardFixturePayloadByIndex.Contains(SelectedMailIndex);
	const bool bPayloadMissing = RenderedMailRewardRows.IsEmpty() && !bEmptyPayload;
	if (bPayloadMissing)
	{
		Grid->RemoveFromParent();
		Empty->SetText(MailRewardFixtureError.IsEmpty()
			? NSLOCTEXT("RokFeature", "MailNoRewardPayload", "该邮件没有附件数据。")
			: FText::FromString(MailRewardFixtureError));
	}
	else
	{
		Empty->RemoveFromParent();
		if (!RenderedMailRewardRows.IsEmpty())
		{
			const auto* RowStart = GridPage->Get<UCanvasPanel>(TEXT("MailRewardGridSecondRow"));
			const auto* RowSlot = RowStart ? Cast<UCanvasPanelSlot>(RowStart->Slot) : nullptr;
			constexpr int32 Columns = 5;
			const UCanvasPanelSlot* CellSlots[Columns];
			for (int32 Column = 0; Column < Columns; ++Column)
			{
				const auto* Marker = GridPage->Get<UCanvasPanel>(*FString::Printf(TEXT("MailRewardGridCell_%d"), Column));
				CellSlots[Column] = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
				if (!CellSlots[Column])
				{
					return {};
				}
			}
			const float RowPitch = RowSlot ? RowSlot->GetPosition().Y : 0.0f;
			UClass* ItemClass = ResolveFeaturePageClass(TEXT("MailRewardItemCard"));
			UClass* CurrencyClass = ResolveFeaturePageClass(TEXT("MailRewardCurrencyCard"));
			if (RowPitch <= 0.0f || !ItemClass || !CurrencyClass)
			{
				return {};
			}
			for (int32 Index = 0; Index < RenderedMailRewardRows.Num(); ++Index)
			{
				const auto& Reward = RenderedMailRewardRows[Index];
				auto* Card = CreateWidget<URokUiPageWidget>(this,
					Reward.Kind == ERokMailRewardKind::Currency ? CurrencyClass : ItemClass);
				if (!Card)
				{
					return {};
				}
				BindMailRewardCard(*Card, Reward, Index, false);
				const auto* Cell = CellSlots[Index % Columns];
				const FVector2D Position = Cell->GetPosition() + FVector2D(0.0, (Index / Columns) * RowPitch);
				const FVector2D Size = Cell->GetSize();
				PlaceOnCanvas(Grid, Card, Position, Size, 1);
				GridHeight = FMath::Max(GridHeight, static_cast<float>(Position.Y + Size.Y));
			}
		}
		Layout->SetSize(FVector2D(Layout->GetSize().X, GridHeight));
	}
	GridPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* PageSlot = DetailContent.AddChildToCanvas(GridPage);
	PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PageSlot->SetOffsets(FMargin(0));
	PageSlot->SetZOrder(6);
	return GiftTop + GridHeight;
}

void URokUnityFeatureScreenWidget::BindMailRewardCard(URokUiPageWidget& Card, const FRokMailRewardRow& Row,
	int32 Index, bool bReferenceCard)
{
	Card.SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (!bReferenceCard)
	{
		Card.LegacyPrefix = TEXT("MailReward");
		Card.LegacySuffix = FString::Printf(TEXT("_%d"), Index);
	}
	if (auto* Quality = Card.Get<UImage>(bReferenceCard ? TEXT("MailRewardItemQuality") : TEXT("FixtureQuality")))
	{
		const bool bShowQuality = Row.Kind == ERokMailRewardKind::Item && Row.Quality >= 1 && Row.Quality <= 5;
		if (bShowQuality)
		{
			Quality->SetBrush(MakeRewardQualityBrush(Row.Quality, Quality->GetBrush().GetImageSize()));
		}
		Card.SetShown(Quality, bShowQuality);
	}

	const TCHAR* AtlasPath = nullptr;
	FIntRect IconRect;
	FIntPoint AtlasSize;
	const bool bResolved = ResolveMailRewardIcon(Row, AtlasPath, IconRect, AtlasSize);
	if (auto* Icon = Card.Get<UImage>(bReferenceCard ? TEXT("MailRewardItemIcon") : TEXT("FixtureIcon")))
	{
		UTexture2D* Texture = bResolved ? LoadUiTexture(AtlasPath) : nullptr;
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture)
		{
			Icon->SetBrush(MakeAtlasBrush(Texture, IconRect, AtlasSize, Icon->GetBrush().GetImageSize()));
		}
	}
	if (auto* Quantity = Card.Get<UTextBlock>(bReferenceCard ? TEXT("MailRewardItemCount") : TEXT("FixtureQuantity")))
	{
		Quantity->SetText(FText::FromString(Row.QuantityText));
		Quantity->SetFont(MailSourceQuantityFont());
	}
	SetFeaturePageText(Card, bReferenceCard ? TEXT("MailRewardName") : TEXT("FixtureName"), FText::FromString(Row.DisplayName));
	if (Row.Kind != ERokMailRewardKind::Currency)
	{
		BindPageAction(Card, bReferenceCard ? TEXT("MailRewardItemHit") : TEXT("FixtureHit"), 2600 + Index);
	}
}
