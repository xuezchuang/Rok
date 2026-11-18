// Unity sources: HospitalStatusGlobalMediator, GlobalCityBuidlingMediator,
// GlobalResourceMediator, ArmyTrainStatusGlobalMediator and CityBuildingProxy.
// Both the authored HUD WBP and the legacy HUD enter this shared presentation path.
#include "ImageUtils.h"
#include "RokDataPaths.h"
#include "RokHospitalTreatment.h"

namespace RokCityStatus
{
struct FSource
{
	const TCHAR* Model;
	FVector Tip;
	FVector Corners[4];
};
const FSource Sources[] = {
#include "RokCityStatusIndicatorSources.generated.inl"
};

const FSource* FindSource(const AActor& Actor)
{
	FString Model;
	for (FName Tag : Actor.Tags)
	{
		const FString Text = Tag.ToString();
		if (Text.StartsWith(TEXT("RokLocalSourceModel_")))
		{
			Model = Text.RightChop(20);
			break;
		}
		if (Text.StartsWith(TEXT("UnityModel_")))
		{
			Model = Text.RightChop(11);
		}
	}
	static const TMap<FString, const FSource*> ByModel = []
	{
		TMap<FString, const FSource*> Result;
		for (const FSource& Source : Sources)
		{
			Result.Add(Source.Model, &Source);
		}
		return Result;
	}();
	return ByModel.FindRef(Model);
}

bool QueueRunning(const TSharedPtr<FJsonObject>& Role, const TCHAR* Field, double Now, bool& bKnown)
{
	const TSharedPtr<FJsonObject>* Queue = nullptr;
	int64 Finish = 0;
	bKnown = Role && Role->TryGetObjectField(Field, Queue)
		&& RokSprotoJson::TryGetInt64(*Queue, TEXT("finishTime"), Finish);
	return bKnown && Finish > Now;
}

bool CanShowUpgrade(const FRokServerBuilding& Building, AActor* Actor,
	URokBuildingUpgradeSubsystem& Upgrades, URokUnityConfigSubsystem& Config,
	const URokGameNetSubsystem& Net, const FRokServerArmySnapshot& Army, double Now)
{
	// CityBuildingProxy.ShowUpgradeBoard excludes walls/towers, active work,
	// unmet prerequisites, unaffordable resources/items and unavailable worker queues.
	if (Building.FinishTime != -1 || Building.Level <= 0 || Building.Type == 6 || Building.Type == 8)
	{
		return false;
	}
	const auto Role = Net.GetRoleInfo();
	if (Building.Type >= 9 && Building.Type <= 12)
	{
		const auto* Queue = Army.TrainingQueues.FindByPredicate([&Building](const auto& Row)
		{
			return Row.BuildingIndex == Building.Index;
		});
		if (!Army.bTrainingKnown || !Queue || Queue->State == ERokServerTrainingState::Unknown
			|| Queue->State == ERokServerTrainingState::Training || Queue->State == ERokServerTrainingState::Upgrading)
		{
			return false;
		}
	}
	if (Building.Type == 13 || Building.Type == 14)
	{
		bool bKnown = false;
		if (QueueRunning(Role, Building.Type == 13 ? TEXT("technologyQueue") : TEXT("treatmentQueue"), Now, bKnown) || !bKnown)
		{
			return false;
		}
	}
	FString Failure;
	if (!Upgrades.CanStartServerUpgrade(Actor, false, Failure))
	{
		return false;
	}
	const auto* Rows = Config.GetTableRows(TEXT("BuildingLevelData"));
	if (!Rows)
	{
		return false;
	}
	for (const auto& Row : *Rows)
	{
		int64 Type = 0, Level = 0, Item = 0, Count = 0;
		if (RokSprotoJson::TryGetInt64(Row, TEXT("type"), Type) && Type == Building.Type
			&& RokSprotoJson::TryGetInt64(Row, TEXT("level"), Level) && Level == Building.Level + 1)
		{
			if (!RokSprotoJson::TryGetInt64(Row, TEXT("itemType1"), Item)
				|| !RokSprotoJson::TryGetInt64(Row, TEXT("itemCnt"), Count) || Item < 0 || Count < 0)
			{
				return false;
			}
			int64 Owned = 0;
			return Count == 0 || (Item > 0 && Item <= MAX_int32
				&& Net.TryGetOwnedItemCount(int32(Item), Owned) && Owned >= Count);
		}
	}
	return false;
}
}

void URokMainHudWidget::BuildCityStatusIndicators()
{
	const auto Load = [](const TCHAR* Name)
	{
		UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::ResolveProjectPath(
			FString(TEXT("Content/RokData/CityStatusIndicators/")) + Name));
		if (Texture)
		{
			Texture->Filter = TF_Bilinear;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			Texture->UpdateResource();
		}
		return Texture;
	};
	CityUpgradeFlagTexture = Load(TEXT("upgrade.png"));
	CityIdleTexture = Load(TEXT("idle.png"));
}

void URokMainHudWidget::UpdateCityStatusIndicators(float DeltaTime)
{
	CityStatusElapsed += DeltaTime;
	const auto HideIndicators = [this]()
	{
		for (auto& Pair : CityStatusIndicators)
		{
			if (auto* Root = Pair.Value.Root.Get())
			{
				Root->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	};
	TSet<int32> VisibleBuildings;
	UWorld* World = GetWorld();
	const auto* City = World ? World->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	auto* Upgrades = World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const auto* ArmySubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	auto* Controller = GetOwningPlayer();
	const float Dxf = GetCityCameraDxf();
	double Now = 0;
	if (!City || !City->HasAppliedServerCity() || !Upgrades || !Config || !Net || Net->GetState() != ERokGameNetState::RoleReady || !Net->TryGetServerTimeSeconds(Now)
		|| !Controller || !ExpandedDesignCanvas || CurrentRootSize.X <= 1 || CurrentRootSize.Y <= 1
		|| !FMath::IsFinite(Dxf) || Dxf <= 0 || Dxf > 270)
	{
		HideIndicators();
		return;
	}
	const auto Role = Net->GetRoleInfo();
	if (!Role)
	{
		HideIndicators();
		return;
	}
	CityStatusRefreshDelay -= DeltaTime;
	const bool bRefreshStates = CityStatusRefreshDelay <= 0.0f;
	if (bRefreshStates)
	{
		CityStatusRefreshDelay = 0.25f;
	}
	const auto Army = bRefreshStates && ArmySubsystem ? ArmySubsystem->GetSnapshot() : FRokServerArmySnapshot();
	const auto Hospital = bRefreshStates ? FRokHospitalState::Read(Role) : FRokHospitalState();
	const auto BuildQueues = bRefreshStates ? City->GetBuildQueueSnapshot() : FRokServerBuildQueueSnapshot();
	const bool bBuilderIdle = BuildQueues.Queues.ContainsByPredicate([](const auto& Queue)
	{
		return Queue.State == ERokServerBuildQueueState::Idle;
	});
	const FVector2D Ratio(CurrentExpandedDesignSize.X / CurrentRootSize.X, CurrentExpandedDesignSize.Y / CurrentRootSize.Y);
	const float HudScale = 172.0f / Dxf * CurrentExpandedDesignSize.Y / UnityReferenceHeight;
	const auto Project = [&](const FVector& Position, FVector2D& Out)
	{
		FVector2D Screen;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Position, Screen, true))
		{
			return false;
		}
		Out = Screen * Ratio;
		return true;
	};
	for (const auto& Building : City->GetAppliedBuildings())
	{
		auto* Sprite = Cast<ARokCitySpriteActor>(City->FindBuildingBarAnchor(Building.Index));
		const auto* Source = Sprite ? RokCityStatus::FindSource(*Sprite) : nullptr;
		if (!Sprite || Sprite->IsHidden() || !Source || Building.Level <= 0)
		{
			continue;
		}
		const float Lod = Sprite->GetUnityLodScale();
		const auto WorldPoint = [&](const FVector& Local)
		{
			return Sprite->GetActorTransform().TransformPosition(Local * Lod);
		};
		FVector2D Tip;
		if (!Project(WorldPoint(Source->Tip), Tip))
		{
			continue;
		}
		auto& Marker = CityStatusIndicators.FindOrAdd(Building.Index);
		if (!Marker.Root.IsValid())
		{
			Marker = FCityStatusIndicator();
			const auto Name = [&Building](const TCHAR* Base) { return FName(*FString::Printf(TEXT("%s_%d"), Base, Building.Index)); };
			auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("CityStatus")));
			// Projected positions are in the HUD design canvas coordinate space.
			PlaceOnCanvas(ExpandedDesignCanvas, Root, FVector2D::ZeroVector, FVector2D::ZeroVector,
				FAnchors(0, 0, 1, 1))->SetZOrder(5);
			CityStatusIndicatorRoots.Add(Root);
			Marker.Root = Root;
			static const TSoftClassPtr<URokUiPageWidget> StatusClass(FSoftObjectPath(
				TEXT("/Game/RokUI/Hud/WBP_RokCityStatusIndicator.WBP_RokCityStatusIndicator_C")));
			bool bBoundPage = false;
			if (auto* Page = AddHudPage(Root, StatusClass, NAME_None))
			{
				auto* Flag = Page->Get<UImage>(TEXT("CityUpgradeFlag"));
				auto* Pop = Page->Get<UCanvasPanel>(TEXT("CityStatusPop"));
				auto* Label = Page->Get<UTextBlock>(TEXT("CityStatusText"));
				auto* Icon = Page->Get<UImage>(TEXT("CityHospitalMark"));
				auto* Hit = Page->Get<URokUnityIndexedButton>(TEXT("CityStatusAction"));
				auto* IdleLarge = Page->Get<UImage>(TEXT("CityStatusIdleLarge"));
				auto* IdleSmall = Page->Get<UImage>(TEXT("CityStatusIdleSmall"));
				if (Flag && Pop && Label && Icon && Hit && IdleLarge && IdleSmall
					&& Cast<UCanvasPanelSlot>(Flag->Slot) && Cast<UCanvasPanelSlot>(Pop->Slot)
					&& Cast<UCanvasPanelSlot>(IdleLarge->Slot) && Cast<UCanvasPanelSlot>(IdleSmall->Slot)
					&& Flag->GetParent() == Pop->GetParent() && IdleLarge->GetParent() == Pop->GetParent()
					&& IdleSmall->GetParent() == Pop->GetParent() && Label->GetParent() == Pop
					&& Icon->GetParent() == Pop && Hit->GetParent() == Pop)
				{
					Page->LegacySuffix = FString::Printf(TEXT("_%d"), Building.Index);
					Flag->SetBrushFromTexture(CityUpgradeFlagTexture);
					IdleLarge->SetBrushFromTexture(CityIdleTexture);
					IdleSmall->SetBrushFromTexture(CityIdleTexture);
					Hit->ConfigureAction(Building.Index);
					Hit->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleCityStatusClicked);
					Marker.Flag = Flag;
					Marker.Pop = Pop;
					Marker.Label = Label;
					Marker.Icon = Icon;
					Marker.Idle = {IdleLarge, IdleSmall};
					bBoundPage = true;
				}
				else
				{
					Page->RemoveFromParent();
				}
			}
			if (!bBoundPage)
			{
				auto* Flag = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("CityUpgradeFlag")));
				Flag->SetBrushFromTexture(CityUpgradeFlagTexture);
				Flag->SetVisibility(ESlateVisibility::Collapsed);
				PlaceOnCanvas(Root, Flag, FVector2D::ZeroVector, FVector2D(100));
				Marker.Flag = Flag;
				auto* Pop = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name(TEXT("CityStatusPop")));
				PlaceOnCanvas(Root, Pop, FVector2D(0, -100), FVector2D(88, 62), FAnchors(0), FVector2D(.5f, 1));
				Pop->SetRenderTransformPivot(FVector2D(.5f, 1));
				PlaceOnCanvas(Pop, MakeHudSlicedAtlasPanel(Name(TEXT("CityStatusBackground")), MainMenuAtlasPath,
					SpriteRect(976, 941, 40, 40), MainMenuAtlasSize, FMargin(16, 18, 15, 17)),
					FVector2D::ZeroVector, FVector2D(88, 50));
				PlaceOnCanvas(Pop, MakeAtlasImage(Name(TEXT("CityStatusArrow")), MainMenuAtlasPath,
					SpriteRect(215, 703, 10, 12), MainMenuAtlasSize, FVector2D(10, 12)), FVector2D(39, 50), FVector2D(10, 12));
				auto* Label = MakeHudText(Name(TEXT("CityStatusText")), 20, FLinearColor(.35f,.33f,.27f), true);
				Label->SetJustification(ETextJustify::Center);
				PlaceOnCanvas(Pop, Label, FVector2D(6, 8), FVector2D(76, 34));
				auto* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name(TEXT("CityHospitalMark")));
				PlaceOnCanvas(Pop, Icon, FVector2D(24, 5), FVector2D(40));
				auto* Hit = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Name(TEXT("CityStatusAction")));
				Hit->ConfigureAction(Building.Index);
				Hit->OnIndexedClicked.AddUObject(this, &URokMainHudWidget::HandleCityStatusClicked);
				FButtonStyle Style;
				Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
				Style.Hovered = Style.Pressed = Style.Normal;
				Hit->SetStyle(Style);
				PlaceOnCanvas(Pop, Hit, FVector2D::ZeroVector, FVector2D(88,62));
				Marker.Pop = Pop;
				Marker.Label = Label;
				Marker.Icon = Icon;
				for (int32 Index = 0; Index < 2; ++Index)
				{
					auto* Z = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
					Z->SetBrushFromTexture(CityIdleTexture);
					PlaceOnCanvas(Root, Z, FVector2D(0, -35), FVector2D(34), FAnchors(0), FVector2D(.5f));
					Marker.Idle.Add(Z);
				}
			}
			// Designer offsets are captured once; projection moves the authored layout.
			Marker.PopBaseOffset = CastChecked<UCanvasPanelSlot>(Marker.Pop->Slot)->GetPosition();
			for (const auto& Idle : Marker.Idle)
			{
				Marker.IdleBaseOffsets.Add(CastChecked<UCanvasPanelSlot>(Idle->Slot)->GetPosition());
			}
		}
		VisibleBuildings.Add(Building.Index);
		Marker.Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		const bool bSelected = SelectedBuildingActor.Get() == Sprite;

		if (bRefreshStates)
		{
			Marker.bIdle = false;
			if (Building.FinishTime == -1 && Army.bTrainingKnown)
			{
				Marker.bIdle = Army.TrainingQueues.ContainsByPredicate([&Building](const auto& Queue)
				{
					return Queue.BuildingIndex == Building.Index && Queue.bBuildingKnown && Queue.State == ERokServerTrainingState::Idle;
				});
			}
			Marker.Caption = FText::GetEmpty();
			Marker.bHospitalWound = false;
			if (Building.Type == 7 && !bSelected && bBuilderIdle)
			{
				Marker.Caption = NSLOCTEXT("RokCityStatus", "Build", "建造");
			}
			else if (Building.Type == 18 && !bSelected)
			{
				int64 GoldFree = 0, SilverFree = 0, SilverTime = 0;
				if ((RokSprotoJson::TryGetInt64(Role,TEXT("addGoldFreeAddTime"),GoldFree) && GoldFree < Now)
					|| (RokSprotoJson::TryGetInt64(Role,TEXT("silverFreeCount"),SilverFree) && SilverFree > 0
						&& RokSprotoJson::TryGetInt64(Role,TEXT("openNextSilverTime"),SilverTime) && SilverTime < Now))
				{
					Marker.Caption = NSLOCTEXT("RokCityStatus", "Recruit", "招募");
				}
			}
			else if (Building.Type == 14 && Building.FinishTime == -1)
			{
				if (Hospital.FinishTime > Now)
				{
					Marker.Caption = FText::FromString(FormatRemainingClock(float(Hospital.FinishTime - Now)));
				}
				else if (Hospital.FinishTime > 0 && !Hospital.InTreatment.IsEmpty())
				{
					Marker.Caption = NSLOCTEXT("RokCityStatus", "HospitalReady", "领取");
				}
				else
				{
					int64 Civilization = 0;
					int32 Mark = INDEX_NONE;
					Marker.bHospitalWound = !Hospital.Wounded.IsEmpty() && RokSprotoJson::TryGetInt64(Role,TEXT("country"),Civilization) && Civilization > 0 && Civilization <= MAX_int32
						&& Config->GetIntFieldByKey(TEXT("Civilization"),TEXT("ID"),int32(Civilization),TEXT("hospitalMark"),Mark)
						&& (Mark == 0 || Mark == 1);
					if (Marker.bHospitalWound)
					{
						Marker.Icon->SetBrush(MakeAtlasBrush(LoadUiTexture(MainMenuAtlasPath),
							Mark == 0 ? SpriteRect(955,545,40,40) : SpriteRect(275,830,37,40), MainMenuAtlasSize, FVector2D(40)));
					}
				}
			}
			Marker.bCanUpgrade = CityUpgradeFlagTexture && RokCityStatus::CanShowUpgrade(Building, Sprite, *Upgrades, *Config, *Net, Army, Now);
		}

		for (int32 Index = 0; Index < Marker.Idle.Num(); ++Index)
		{
			auto* Z = Marker.Idle[Index].Get();
			const float Rise = FMath::Fmod(CityStatusElapsed / 2.6f + Index*.5f, 1.f);
			CastChecked<UCanvasPanelSlot>(Z->Slot)->SetPosition(Tip + (Marker.IdleBaseOffsets[Index] + FVector2D(14*Rise, -65*Rise))*HudScale);
			Z->SetRenderScale(FVector2D(HudScale*FMath::Lerp(.58f,1.f,Rise)));
			Z->SetRenderOpacity(FMath::SmoothStep(0.f,.22f,Rise)*(1-FMath::SmoothStep(.64f,1.f,Rise))*.78f);
			Z->SetVisibility(Marker.bIdle && CityIdleTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}

		Marker.Pop->SetVisibility(!Marker.Caption.IsEmpty() || Marker.bHospitalWound ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Marker.Label->SetText(Marker.Caption);
		Marker.Label->SetVisibility(Marker.bHospitalWound ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Marker.Icon->SetVisibility(Marker.bHospitalWound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		CastChecked<UCanvasPanelSlot>(Marker.Pop->Slot)->SetPosition(Tip + Marker.PopBaseOffset*HudScale);
		Marker.Pop->SetRenderScale(FVector2D(HudScale));
		bool bFlag = !bSelected && Marker.bCanUpgrade;
		FVector2D Corner[4];
		for (int32 Index = 0; bFlag && Index < 4; ++Index)
		{
			bFlag = Project(WorldPoint(Source->Corners[Index]), Corner[Index]);
		}
		if (bFlag)
		{
			FVector2D Min = Corner[0], Max = Corner[0];
			for (int32 Index = 1; Index < 4; ++Index)
			{
				Min = FVector2D(FMath::Min(Min.X,Corner[Index].X),FMath::Min(Min.Y,Corner[Index].Y));
				Max = FVector2D(FMath::Max(Max.X,Corner[Index].X),FMath::Max(Max.Y,Corner[Index].Y));
			}
			auto* Slot = CastChecked<UCanvasPanelSlot>(Marker.Flag->Slot);
			Slot->SetPosition(Min);
			Slot->SetSize(Max-Min);
		}
		Marker.Flag->SetVisibility(bFlag ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (auto& Pair : CityStatusIndicators)
	{
		if (!VisibleBuildings.Contains(Pair.Key) && Pair.Value.Root.IsValid())
		{
			Pair.Value.Root->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URokMainHudWidget::HandleCityStatusClicked(int32 BuildingIndex)
{
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto* Building = City ? City->GetAppliedBuildings().FindByPredicate([BuildingIndex](const auto& Row)
	{
		return Row.Index == BuildingIndex;
	}) : nullptr;
	if (Building && Building->Type == 14 && FeatureScreenWidget)
	{
		FeatureScreenWidget->OpenHospital(City->FindBuildingBarAnchor(BuildingIndex));
	}
}
