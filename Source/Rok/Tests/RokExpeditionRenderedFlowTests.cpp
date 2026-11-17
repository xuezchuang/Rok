#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RokUiWidgets.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWidget.h"
#include "../RokBuildingUpgradeSubsystem.h"
#include "../RokServerHeroSubsystem.h"
#include "../RokUnityGameplayCatalogSubsystem.h"
#include "../RokUnityRuntimeStateSubsystem.h"
#include "../RokMainHudWidget.h"
#include "../RokUnityFeatureScreenWidget.h"

namespace
{
bool IsMountedWbpPage(const URokUnityFeatureScreenWidget* Screen, FName ChildName, const TCHAR* AssetName)
{
	const UWidget* Child = Screen ? Screen->GetWidgetFromName(ChildName) : nullptr;
	const URokUiPageWidget* Page = Child ? Child->GetTypedOuter<URokUiPageWidget>() : nullptr;
	return Page && Page->GetClass()->GetPathName().Contains(AssetName);
}

void LogExpeditionAuditWidget(const TCHAR* Mode, const URokUnityFeatureScreenWidget* Screen, FName Name)
{
	UWidget* Widget = Screen ? RokUiFind::Find(Screen, Name) : nullptr;
	if (!Widget)
	{
		UE_LOG(LogTemp, Display, TEXT("Expedition audit [%s] widget=%s missing"), Mode, *Name.ToString());
		return;
	}

	const FGeometry Geometry = Widget->GetCachedGeometry();
	const FVector2D AbsolutePosition = Geometry.GetAbsolutePosition();
	const FVector2D LocalSize = Geometry.GetLocalSize();
	FString CanvasSlot = TEXT("none");
	if (const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		CanvasSlot = FString::Printf(TEXT("pos=%s size=%s align=%s auto=%d"),
			*Slot->GetPosition().ToString(), *Slot->GetSize().ToString(), *Slot->GetAlignment().ToString(),
			Slot->GetAutoSize() ? 1 : 0);
	}
	UE_LOG(LogTemp, Display,
		TEXT("Expedition audit [%s] widget=%s class=%s abs=%s local=%s slot={%s}"),
		Mode, *Name.ToString(), *Widget->GetClass()->GetPathName(), *AbsolutePosition.ToString(),
		*LocalSize.ToString(), *CanvasSlot);
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget))
	{
		const FSlateFontInfo& Font = Text->GetFont();
		UE_LOG(LogTemp, Display,
			TEXT("Expedition audit [%s] text=%s font=%s size=%.3f color=%s shadowOffset=%s shadowColor=%s"),
			Mode, *Name.ToString(), *Font.TypefaceFontName.ToString(), Font.Size,
			*Text->GetColorAndOpacity().GetSpecifiedColor().ToString(),
			*Text->GetShadowOffset().ToString(), *Text->GetShadowColorAndOpacity().ToString());
	}
	if (const USizeBox* SizeBox = Cast<USizeBox>(Widget))
	{
		UE_LOG(LogTemp, Display,
			TEXT("Expedition audit [%s] sizeBox=%s widthOverride=%d:%.3f heightOverride=%d:%.3f"),
			Mode, *Name.ToString(), SizeBox->IsWidthOverride() ? 1 : 0, SizeBox->GetWidthOverride(),
			SizeBox->IsHeightOverride() ? 1 : 0, SizeBox->GetHeightOverride());
	}
	if (const UScrollBox* ScrollBox = Cast<UScrollBox>(Widget))
	{
		UE_LOG(LogTemp, Display,
			TEXT("Expedition audit [%s] scroll=%s offset=%.3f consumeWheel=%d scrollbar=%d clipping=%d"),
			Mode, *Name.ToString(), ScrollBox->GetScrollOffset(),
			static_cast<int32>(ScrollBox->GetConsumeMouseWheel()),
			static_cast<int32>(ScrollBox->GetScrollBarVisibility()), static_cast<int32>(ScrollBox->GetClipping()));
	}

	const UImage* Image = Cast<UImage>(Widget);
	if (!Image) return;

	const FSlateBrush& Brush = Image->GetBrush();
	const FBox2f UV = Brush.GetUVRegion();
	const UObject* Resource = Brush.GetResourceObject();
	const UTexture2D* Texture = Cast<UTexture2D>(Resource);
	UE_LOG(LogTemp, Display,
		TEXT("Expedition audit [%s] brush=%s resource=%s draw=%d imageType=%d tile=%d imageSize=(%.3f,%.3f) margin=(%.3f,%.3f,%.3f,%.3f) uvValid=%d uvMin=(%.6f,%.6f) uvMax=(%.6f,%.6f)"),
		Mode, *Name.ToString(), Resource ? *Resource->GetPathName() : TEXT("none"),
		static_cast<int32>(Brush.DrawAs), static_cast<int32>(Brush.ImageType), static_cast<int32>(Brush.Tiling),
		Brush.ImageSize.X, Brush.ImageSize.Y, Brush.Margin.Left, Brush.Margin.Top, Brush.Margin.Right, Brush.Margin.Bottom, UV.bIsValid ? 1 : 0,
		UV.Min.X, UV.Min.Y, UV.Max.X, UV.Max.Y);
	if (Texture)
	{
		UE_LOG(LogTemp, Display,
			TEXT("Expedition audit [%s] texture=%s dimensions=%dx%d mips=%d format=%d compression=%d mipGen=%d lodGroup=%d filter=%d mipLoadOptions=%d srgb=%d"),
			Mode, *Texture->GetPathName(), Texture->GetSizeX(), Texture->GetSizeY(), Texture->GetNumMips(),
			static_cast<int32>(Texture->GetPixelFormat()), static_cast<int32>(Texture->CompressionSettings.GetValue()),
			static_cast<int32>(Texture->MipGenSettings), static_cast<int32>(Texture->LODGroup.GetValue()),
			static_cast<int32>(Texture->Filter.GetValue()), static_cast<int32>(Texture->MipLoadOptions), Texture->SRGB ? 1 : 0);
	}
}

void LogExpeditionMapAudit(const URokUnityFeatureScreenWidget* Screen)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionFlowFixture"))) return;
	const TCHAR* Mode = URokUiSettings::IsLegacyForced() ? TEXT("Legacy") : TEXT("WBP");
	const FName Names[] = {
		TEXT("ExpeditionMapScroll"), TEXT("ExpeditionMapExtent"), TEXT("ExpeditionMapContent"),
		TEXT("ExpeditionMapTile_0"), TEXT("ExpeditionTitle"), TEXT("ExpeditionRewardContent"),
		TEXT("ExpeditionStageSelectedGlow_1"), TEXT("ExpeditionStageNormalTitle_1"),
		TEXT("ExpeditionStageSelectedTitle_1"), TEXT("ExpeditionFirstRewardStamp"),
		TEXT("ExpeditionReward_0Icon"), TEXT("ExpeditionReward_0Count"), TEXT("ExpeditionReward_1Icon"),
		TEXT("ExpeditionReward_1Count") };
	for (const FName Name : Names)
	{
		LogExpeditionAuditWidget(Mode, Screen, Name);
	}
}

void LogExpeditionSoldierAudit(const TCHAR* Phase, const URokUnityFeatureScreenWidget* Screen,
	const TArray<FRokUnityRuntimeTroopStack>& StarterStacks)
{
	const auto IsHitTestVisible = [](const UWidget* Widget)
	{
		return Widget && Widget->GetVisibility() == ESlateVisibility::Visible;
	};
	const auto AreChildrenHitTestVisible = [](const UWidget* Widget)
	{
		return Widget && (Widget->GetVisibility() == ESlateVisibility::Visible
			|| Widget->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	};
	const TCHAR* Mode = URokUiSettings::IsLegacyForced() ? TEXT("Legacy") : TEXT("WBP");
	for (const FRokUnityRuntimeTroopStack& Stack : StarterStacks)
	{
		if (Stack.TroopId <= 0 || Stack.Count <= 0) continue;
		const int32 Id = Stack.TroopId;
		const auto Find = [Screen](const TCHAR* Prefix, int32 TroopId) -> UWidget*
		{
			return RokUiFind::Find(Screen, *FString::Printf(TEXT("%s_%d"), Prefix, TroopId));
		};
		const FString RowName = FString::Printf(TEXT("ExpeditionSoldierSize_%d"), Id);
		const UWidget* RowSize = RokUiFind::Find(Screen, *RowName);
		const UWidget* NameWidget = Find(TEXT("ExpeditionSoldierName"), Id);
		const UWidget* PortraitWidget = Find(TEXT("ExpeditionSoldierPortrait"), Id);
		const UWidget* QuantityWidget = Find(TEXT("ExpeditionSoldierQuantity"), Id);
		const URokUiPageWidget* RowPage = NameWidget ? NameWidget->GetTypedOuter<URokUiPageWidget>() : nullptr;
		const UWidget* ComposerRoot = RokUiFind::Find(Screen, TEXT("ExpeditionArmyRoot"));
		const URokUiPageWidget* ComposerPage = ComposerRoot ? ComposerRoot->GetTypedOuter<URokUiPageWidget>() : nullptr;
		UE_LOG(LogTemp, Display,
			TEXT("Expedition paint audit [%s/%s] troop=%d row={class=%s visible=%d hit=%d childHit=%d opacity=%.3f geo=%s slate=%d} rowPage={class=%s visible=%d opacity=%.3f geo=%s} composer={class=%s visible=%d opacity=%.3f geo=%s}"),
			Mode, Phase, Id,
			RowSize ? *RowSize->GetClass()->GetPathName() : TEXT("none"),
			RowSize && RowSize->IsVisible() ? 1 : 0,
			IsHitTestVisible(RowSize) ? 1 : 0,
			AreChildrenHitTestVisible(RowSize) ? 1 : 0,
			RowSize ? RowSize->GetRenderOpacity() : -1.0f,
			RowSize ? *RowSize->GetCachedGeometry().GetLocalSize().ToString() : TEXT("none"),
			RowSize && RowSize->GetCachedWidget().IsValid() ? 1 : 0,
			RowPage ? *RowPage->GetClass()->GetPathName() : TEXT("native"),
			RowPage && RowPage->IsVisible() ? 1 : 0,
			RowPage ? RowPage->GetRenderOpacity() : -1.0f,
			RowPage ? *RowPage->GetCachedGeometry().GetLocalSize().ToString() : TEXT("none"),
			ComposerPage ? *ComposerPage->GetClass()->GetPathName() : TEXT("native"),
			ComposerPage && ComposerPage->IsVisible() ? 1 : 0,
			ComposerPage ? ComposerPage->GetRenderOpacity() : -1.0f,
			ComposerPage ? *ComposerPage->GetCachedGeometry().GetLocalSize().ToString() : TEXT("none"));
		const UWidget* Parts[] = { NameWidget, PortraitWidget, QuantityWidget };
		const TCHAR* PartNames[] = { TEXT("name"), TEXT("portrait"), TEXT("quantity") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Parts); ++Index)
		{
			const UWidget* Part = Parts[Index];
			const TSharedPtr<SWidget> SlateWidget = Part ? Part->GetCachedWidget() : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("Expedition paint audit [%s/%s] troop=%d part=%s class=%s visible=%d hit=%d childHit=%d opacity=%.3f geo=%s slate=%d slateVisible=%d slateHit=%d slateChildHit=%d slateDesired=%s"),
				Mode, Phase, Id, PartNames[Index],
				Part ? *Part->GetClass()->GetPathName() : TEXT("none"),
				Part && Part->IsVisible() ? 1 : 0,
				IsHitTestVisible(Part) ? 1 : 0,
				AreChildrenHitTestVisible(Part) ? 1 : 0,
				Part ? Part->GetRenderOpacity() : -1.0f,
				Part ? *Part->GetCachedGeometry().GetLocalSize().ToString() : TEXT("none"),
				SlateWidget.IsValid() ? 1 : 0,
				SlateWidget.IsValid() && SlateWidget->GetVisibility().IsVisible() ? 1 : 0,
				SlateWidget.IsValid() && SlateWidget->GetVisibility().IsHitTestVisible() ? 1 : 0,
				SlateWidget.IsValid() && SlateWidget->GetVisibility().AreChildrenHitTestVisible() ? 1 : 0,
				SlateWidget.IsValid() ? *SlateWidget->GetDesiredSize().ToString() : TEXT("none"));
			for (const UWidget* Parent = Part ? Part->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
			{
				UE_LOG(LogTemp, Display,
					TEXT("Expedition paint audit [%s/%s] troop=%d part=%s parent=%s visible=%d hit=%d childHit=%d opacity=%.3f geo=%s"),
					Mode, Phase, Id, PartNames[Index], *Parent->GetClass()->GetPathName(),
					Parent->IsVisible() ? 1 : 0, IsHitTestVisible(Parent) ? 1 : 0,
					AreChildrenHitTestVisible(Parent) ? 1 : 0, Parent->GetRenderOpacity(),
					*Parent->GetCachedGeometry().GetLocalSize().ToString());
			}
		}
		if (const UTextBlock* Text = Cast<UTextBlock>(NameWidget))
		{
			UE_LOG(LogTemp, Display,
				TEXT("Expedition paint audit [%s/%s] troop=%d nameText=%s font=%s/%s/%.3f color=%s"),
				Mode, Phase, Id, *Text->GetText().ToString(), *Text->GetFont().TypefaceFontName.ToString(),
				Text->GetFont().FontObject ? *Text->GetFont().FontObject->GetPathName() : TEXT("composite"),
				Text->GetFont().Size, *Text->GetColorAndOpacity().GetSpecifiedColor().ToString());
		}
		if (const UImage* Image = Cast<UImage>(PortraitWidget))
		{
			const UTexture2D* Texture = Cast<UTexture2D>(Image->GetBrush().GetResourceObject());
			const FTextureResource* Resource = Texture ? Texture->GetResource() : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("Expedition paint audit [%s/%s] troop=%d portraitTexture=%s textureResource=%d textureRHI=%d"),
				Mode, Phase, Id, Texture ? *Texture->GetPathName() : TEXT("none"), Resource ? 1 : 0,
				Resource && Resource->GetTexture2DRHI() ? 1 : 0);
		}
		if (const UProgressBar* Progress = Cast<UProgressBar>(Find(TEXT("ExpeditionSoldierFill"), Id)))
		{
			const FProgressBarStyle& Style = Progress->GetWidgetStyle();
			const FSlateBrush& Background = Style.BackgroundImage;
			const FSlateBrush& Fill = Style.FillImage;
			const UObject* BackgroundResource = Background.GetResourceObject();
			const UObject* FillResource = Fill.GetResourceObject();
			UE_LOG(LogTemp, Display,
				TEXT("Expedition paint audit [%s/%s] troop=%d progress=%.4f tint=%s background={resource=%s tint=%s draw=%d} fill={resource=%s tint=%s draw=%d}"),
				Mode, Phase, Id, Progress->GetPercent(), *Progress->GetFillColorAndOpacity().ToString(),
				BackgroundResource ? *BackgroundResource->GetPathName() : TEXT("none"),
				*Background.TintColor.GetSpecifiedColor().ToString(), static_cast<int32>(Background.GetDrawType()),
				FillResource ? *FillResource->GetPathName() : TEXT("none"),
				*Fill.TintColor.GetSpecifiedColor().ToString(), static_cast<int32>(Fill.GetDrawType()));
		}
		for (const UWidget* Parent = RowSize ? RowSize->GetParent() : nullptr; Parent; Parent = Parent->GetParent())
		{
			UE_LOG(LogTemp, Display,
				TEXT("Expedition paint audit [%s/%s] troop=%d rowParent=%s visible=%d hit=%d childHit=%d opacity=%.3f geo=%s"),
				Mode, Phase, Id, *Parent->GetClass()->GetPathName(),
				Parent->IsVisible() ? 1 : 0, IsHitTestVisible(Parent) ? 1 : 0,
				AreChildrenHitTestVisible(Parent) ? 1 : 0, Parent->GetRenderOpacity(),
				*Parent->GetCachedGeometry().GetLocalSize().ToString());
		}
	}
}

class FCheckExpeditionFlow final : public IAutomationLatentCommand
{
public:
	explicit FCheckExpeditionFlow(FAutomationTestBase& InTest) : Test(InTest)
	{
		Deadline = FPlatformTime::Seconds() + 25.0;
		bLocked = FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionFlowLocked"));
		Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
			/ TEXT("Automation/Expedition") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!Screen.IsValid())
		{
			for (TObjectIterator<URokMainHudWidget> It; It; ++It)
			{
				if (!It->IsInViewport()) continue;
				Screen = It->GetFeatureScreenWidgetForPlacement();
				if (!Screen.IsValid()) continue;
				UWorld* World = It->GetWorld();
				if (!Test.TestNotNull(TEXT("Offline feature screen world"), World)) return true;
				TArray<AActor*> TownHalls;
				UGameplayStatics::GetAllActorsWithTag(World, TEXT("UnityInitId_5"), TownHalls);
				if (!Test.TestTrue(TEXT("Offline city has the tagged town hall fixture"), !TownHalls.IsEmpty())) return true;
				URokBuildingUpgradeSubsystem* Upgrade = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
				if (!Test.TestNotNull(TEXT("Local building-state fixture subsystem"), Upgrade)) return true;
				const FRokBuildingState OriginalHallState = Upgrade->GetBuildingStateForActor(TownHalls[0]);
				Upgrade->ApplyServerBuilding(TownHalls[0],
					OriginalHallState.UnityBuildingTypeId > 0 ? OriginalHallState.UnityBuildingTypeId : 1,
					bLocked ? 4 : 5, FIntPoint(OriginalHallState.UnityGridX, OriginalHallState.UnityGridY));
				// This in-memory world fixture crosses the real city-entry gate. No request is
				// sent; the later Start action still stops at the existing local boundary.
				if (FParse::Param(FCommandLine::Get(), TEXT("RokExpeditionFlowFixture")))
				{
					UGameInstance* GameInstance = World->GetGameInstance();
					const auto* Runtime = GameInstance
						? GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
					auto* Catalog = GameInstance
						? GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
					auto* Heroes = GameInstance
						? GameInstance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
					if (!Test.TestNotNull(TEXT("Local bootstrap runtime fixture"), Runtime)
						|| !Test.TestNotNull(TEXT("Local hero snapshot fixture"), Heroes)
						|| !Test.TestNotNull(TEXT("Hero catalog for local fixture"), Catalog)) return true;
					FRokServerHeroSnapshot Snapshot;
					Snapshot.bAvailable = true;
					Snapshot.bLoaded = true;
					Snapshot.bHeroesKnown = true;
					Snapshot.bRowsComplete = true;
					Snapshot.Signature = TEXT("expedition-local-bootstrap-fixture");
					for (const FRokUnityRuntimeHeroState& LocalHero : Runtime->GetStarterHeroes())
					{
						FRokUnityCatalogHero Hero;
						if (!Catalog->GetHero(LocalHero.HeroId, Hero)) continue;
						FRokServerHeroEntry& Row = Snapshot.Heroes.AddDefaulted_GetRef();
						Row.HeroId = LocalHero.HeroId;
						Row.bCatalogKnown = true;
						Row.Rare = Hero.Rare;
						Row.Level = LocalHero.Level;
						Row.bLevelKnown = true;
						Row.Power = Hero.Score;
						Row.bPowerKnown = true;
					}
					if (!Test.TestTrue(TEXT("Local bootstrap includes a catalog hero"), !Snapshot.Heroes.IsEmpty())) return true;
					Heroes->SetSnapshotForTesting(Snapshot);
				}
				auto* Entry = Cast<UButton>(RokUiFind::Find(*It, TEXT("CampaignFeatureButton")));
				if (!Test.TestNotNull(TEXT("City campaign entry"), Entry)) return true;
				IFileManager::Get().MakeDirectory(*Output, true);
				Entry->OnClicked.Broadcast();
				if (bLocked)
				{
					Test.TestFalse(TEXT("Level 4 stays in the city"), Screen->IsScreenOpen());
					const auto* Tip = Cast<UTextBlock>(It->GetWidgetFromName(TEXT("ExpeditionUnlockTip")));
					Test.TestTrue(TEXT("Lock tip shows level 5"), Tip && Tip->IsVisible() && Tip->GetText().ToString().Contains(TEXT("5")));
					Capture(TEXT("Locked"));
				}
				else
				{
					Test.TestTrue(TEXT("Level 5 opens Expedition"), Screen->IsExpeditionModeForAudit());
					Resume = Now + 1.0;
				}
				return false;
			}
			if (Now < Deadline) return false;
			Test.AddError(TEXT("Standalone city HUD did not initialize."));
			return true;
		}
		if (!Pending.IsEmpty())
		{
			if (IFileManager::Get().FileSize(*Pending) <= 0 && Now < Deadline) return false;
			Test.TestTrue(Pending, IFileManager::Get().FileSize(*Pending) > 0);
			Pending.Reset();
			if (bLocked) return true;
		}
		if (Now < Resume) return false;
		Resume = Now + 0.85;
		switch (Step++)
		{
		case 0:
		{
			Test.TestEqual(TEXT("All source stages"), Screen->GetExpeditionRenderedStageCountForAudit(), 80);
			Test.TestTrue(TEXT("Replay account progress is known"), Screen->IsExpeditionAccountStateKnownForAudit());
			LogExpeditionMapAudit(Screen.Get());
			const UScrollBox* MapScroll = Cast<UScrollBox>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionMapScroll")));
			Test.TestTrue(TEXT("Map scroll consumes mouse wheel even at limits"),
				MapScroll && MapScroll->GetConsumeMouseWheel() == EConsumeMouseWheel::Always);
			Test.TestTrue(TEXT("Map scroll clips content to the viewport"),
				MapScroll && MapScroll->GetClipping() == EWidgetClipping::ClipToBoundsAlways);
			const USizeBox* MapExtent = Cast<USizeBox>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionMapExtent")));
		Test.TestTrue(TEXT("Map extent preserves the source width override"),
			MapExtent && MapExtent->IsWidthOverride() && FMath::IsNearlyEqual(MapExtent->GetWidthOverride(), 1292.0f));
			if (!URokUiSettings::IsLegacyForced())
			{
				Test.TestTrue(TEXT("Map uses the registered Expedition page WBP"),
					IsMountedWbpPage(Screen.Get(), TEXT("ExpeditionMapScroll"), TEXT("WBP_RokFeature_Expedition_C")));
				const auto* Glow = Cast<UImage>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionStageSelectedGlow_1")));
				Test.TestTrue(TEXT("Selected stage glow keeps the Legacy tint and opacity"), Glow
					&& Glow->GetColorAndOpacity().Equals(FLinearColor(1.0f, 0.65f, 0.15f, 0.8f), 0.001f));
				const auto* NormalTitle = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionStageNormalTitle_1")));
				const auto* SelectedTitle = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionStageSelectedTitle_1")));
				Test.TestTrue(TEXT("Stage number keeps editable normal and selected WBP styles"), NormalTitle && SelectedTitle
					&& FMath::IsNearlyEqual(NormalTitle->GetFont().Size, 21.0f)
					&& FMath::IsNearlyEqual(SelectedTitle->GetFont().Size, 24.0f)
					&& NormalTitle->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White, 0.001f)
					&& SelectedTitle->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor(1.0f, 0.86f, 0.45f, 1.0f), 0.001f));
				const int32 SelectedStage = Screen->GetSelectedExpeditionStageIdForAudit();
				const auto* ActiveTitle = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionStageSelectedTitle_%d"), SelectedStage)));
				const auto* InactiveTitle = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionStageNormalTitle_%d"), SelectedStage)));
				Test.TestTrue(TEXT("Selected stage uses the selected number style"), ActiveTitle && ActiveTitle->IsVisible()
					&& InactiveTitle && !InactiveTitle->IsVisible());
				const auto* Stamp = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionFirstRewardStamp")));
				const FWidgetTransform StampTransform = Stamp
					? Stamp->GetRenderTransform() : FWidgetTransform();
				Test.TestTrue(TEXT("First-win stamp keeps the Legacy rotation and center pivot"), Stamp
					&& FMath::IsNearlyEqual(StampTransform.Angle, -33.0f)
					&& StampTransform.Translation.Equals(FVector2D::ZeroVector, 0.001f)
					&& StampTransform.Scale.Equals(FVector2D::UnitVector, 0.001f)
					&& StampTransform.Shear.Equals(FVector2D::ZeroVector, 0.001f)
					&& Stamp->GetRenderTransformPivot().Equals(FVector2D(0.5f), 0.001f));
			}
			for (int32 Id = 1; Id <= 80; ++Id)
			{
				const auto* Flag = Cast<UImage>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionStageFlag_%d"), Id)));
				Test.TestTrue(FString::Printf(TEXT("Stage %d has source flag art"), Id), Flag && Flag->GetBrush().GetResourceObject());
			}
			Test.TestNotNull(TEXT("Stage 1 first and normal reward group results"),
				RokUiFind::Find(Screen.Get(), TEXT("ExpeditionReward_5Count")));
			Capture(TEXT("Map"));
			return false;
		}
		case 1:
			if (!Click(TEXT("ExpeditionStageButton_2"))) return true;
			Test.TestFalse(TEXT("Stage 2 locked before stage 1 is cleared"), Enabled(TEXT("ExpeditionChallenge")));
			if (!Click(TEXT("ExpeditionStageButton_1")) || !Click(TEXT("ExpeditionChallenge"))) return true;
			return false;
		case 2:
			Test.TestTrue(TEXT("Challenge opens the objective screen"), Screen->IsExpeditionTaskConfigVisibleForAudit());
			Test.TestTrue(TEXT("Stage 1 has its second enemy commander"),
				RokUiFind::Find(Screen.Get(), TEXT("ExpeditionEnemyHero_1"))
				|| RokUiFind::Find(Screen.Get(), TEXT("ExpeditionEnemyHeroIcon_1")));
			if (!URokUiSettings::IsLegacyForced())
			{
				Test.TestTrue(TEXT("Objectives use the registered Expedition task WBP"),
					IsMountedWbpPage(Screen.Get(), TEXT("ExpeditionCenterRoot"), TEXT("WBP_RokFeature_ExpeditionTask_C")));
			}
			Test.TestFalse(TEXT("Start needs a prepared squad"), Enabled(TEXT("ExpeditionChallenge")));
			Test.TestFalse(TEXT("Second formation slot locked in stage 1"), Enabled(TEXT("ExpeditionSquadButton_1")));
			Capture(TEXT("Objectives"));
			return false;
		case 3:
			if (!Click(TEXT("ExpeditionSquadButton_0"))) return true;
			Resume = FPlatformTime::Seconds();
			if (!URokUiSettings::IsLegacyForced())
			{
				Test.TestTrue(TEXT("Composer uses the registered Expedition overlay WBP"),
					IsMountedWbpPage(Screen.Get(), TEXT("ExpeditionArmyRoot"), TEXT("WBP_RokFeature_ExpeditionArmyComposer_C")));
			}
			Test.TestNotNull(TEXT("Owned commander has its full-body portrait"),
				RokUiFind::Find(Screen.Get(), TEXT("ExpeditionArmyPortrait")));
			Test.TestNotNull(TEXT("History troops have quantity controls"),
				RokUiFind::Find(Screen.Get(), TEXT("ExpeditionSoldierSlider_10101")));
			return false;
		case 4:
		{
			const UGameInstance* GameInstance = Screen->GetGameInstance();
			const auto* Runtime = GameInstance
				? GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
			const TArray<FRokUnityRuntimeTroopStack> StarterStacks = Runtime
				? Runtime->GetStarterTroopStacks() : TArray<FRokUnityRuntimeTroopStack>();
			LogExpeditionSoldierAudit(TEXT("after-open"), Screen.Get(), StarterStacks);
			Capture(TEXT("CreateArmy_AfterOpen"));
			return false;
		}
		case 5:
		{
			const UGameInstance* GameInstance = Screen->GetGameInstance();
			const auto* Runtime = GameInstance
				? GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
			const TArray<FRokUnityRuntimeTroopStack> StarterStacks = Runtime
				? Runtime->GetStarterTroopStacks() : TArray<FRokUnityRuntimeTroopStack>();
			Test.TestTrue(TEXT("Offline fixture has troop rows to render"), !StarterStacks.IsEmpty());
			int32 CheckedRows = 0;
			for (const FRokUnityRuntimeTroopStack& Stack : StarterStacks)
			{
				if (Stack.TroopId <= 0 || Stack.Count <= 0) continue;
				++CheckedRows;
				const auto* Name = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionSoldierName_%d"), Stack.TroopId)));
				const auto* Portrait = Cast<UImage>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionSoldierPortrait_%d"), Stack.TroopId)));
				const auto* Quantity = Cast<UTextBlock>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionSoldierQuantity_%d"), Stack.TroopId)));
				const auto* Progress = Cast<UProgressBar>(RokUiFind::Find(Screen.Get(),
					*FString::Printf(TEXT("ExpeditionSoldierFill_%d"), Stack.TroopId)));
				Test.TestTrue(FString::Printf(TEXT("Troop %d name is nonempty and visible"), Stack.TroopId),
					Name && !Name->GetText().IsEmpty() && Name->IsVisible()
					&& Name->GetCachedGeometry().GetLocalSize().X > 0.0f
					&& Name->GetCachedGeometry().GetLocalSize().Y > 0.0f);
				Test.TestTrue(FString::Printf(TEXT("Troop %d portrait has visible loaded art"), Stack.TroopId),
					Portrait && Portrait->IsVisible() && Portrait->GetBrush().GetResourceObject()
					&& Portrait->GetCachedGeometry().GetLocalSize().X > 0.0f
					&& Portrait->GetCachedGeometry().GetLocalSize().Y > 0.0f);
				Test.TestTrue(FString::Printf(TEXT("Troop %d quantity is nonempty and visible"), Stack.TroopId),
					Quantity && !Quantity->GetText().IsEmpty() && Quantity->IsVisible()
					&& Quantity->GetCachedGeometry().GetLocalSize().X > 0.0f
					&& Quantity->GetCachedGeometry().GetLocalSize().Y > 0.0f);
				const FLinearColor ExpectedProgressTint(0.1f, 0.64f, 0.28f, 1.0f);
				const FProgressBarStyle* ProgressStyle = Progress ? &Progress->GetWidgetStyle() : nullptr;
				const UObject* FillBrushResource = ProgressStyle ? ProgressStyle->FillImage.GetResourceObject() : nullptr;
				Test.TestTrue(FString::Printf(TEXT("Troop %d progress fill uses the native green tint and default brush"), Stack.TroopId),
					Progress && Progress->GetFillColorAndOpacity().Equals(ExpectedProgressTint, 0.001f)
					&& Progress->IsVisible() && Progress->GetCachedGeometry().GetLocalSize().X > 0.0f
					&& Progress->GetCachedGeometry().GetLocalSize().Y > 0.0f
					&& (!FillBrushResource || !FillBrushResource->GetPathName().Contains(TEXT("rok_hud_atlas_common"))));
				Test.TestTrue(FString::Printf(TEXT("Troop %d progress reflects its selected quantity"), Stack.TroopId),
					Progress && Progress->GetPercent() > 0.95f);
			}
			Test.TestTrue(TEXT("Offline fixture renders at least one populated troop row"), CheckedRows > 0);
			LogExpeditionSoldierAudit(TEXT("settled-1"), Screen.Get(), StarterStacks);
			Capture(TEXT("CreateArmy_Settled1"));
			return false;
		}
		case 6:
		{
			const UGameInstance* GameInstance = Screen->GetGameInstance();
			const auto* Runtime = GameInstance
				? GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
			LogExpeditionSoldierAudit(TEXT("settled-2"), Screen.Get(),
				Runtime ? Runtime->GetStarterTroopStacks() : TArray<FRokUnityRuntimeTroopStack>());
			Capture(TEXT("CreateArmy_Settled2"));
			return false;
		}
		case 7:
			if (!Click(TEXT("ExpeditionArmyClear"))) return true;
			Test.TestFalse(TEXT("Empty army cannot be prepared"), Enabled(TEXT("ExpeditionArmyReady")));
			if (auto* Slider = Cast<USlider>(RokUiFind::Find(Screen.Get(), TEXT("ExpeditionSoldierSlider_10101"))))
			{
				Slider->SetValue(Slider->GetMaxValue());
				// SetValue updates the UMG control but does not model the user's
				// OnValueChanged interaction on every engine path.
				Slider->OnValueChanged.Broadcast(Slider->GetValue());
			}
			if (!Click(TEXT("ExpeditionArmyReady"))) return true;
			Test.TestTrue(TEXT("Prepared squad enables Start"), Enabled(TEXT("ExpeditionChallenge")));
			Capture(TEXT("Prepared"));
			return false;
		case 8:
			if (!Click(TEXT("ExpeditionChallenge"))) return true;
			Test.TestEqual(TEXT("Start stays on the battle integration boundary"), Screen->GetExpeditionLastBoundaryActionIdForAudit(), 1120);
			if (!Click(TEXT("ExpeditionBackButton"))) return true;
			Test.TestFalse(TEXT("Back returns to the selected map stage"), Screen->IsExpeditionTaskConfigVisibleForAudit());
			Test.TestEqual(TEXT("Selected stage preserved"), Screen->GetSelectedExpeditionStageIdForAudit(), 1);
			return true;
		default:
			return true;
		}
	}

private:
	bool Enabled(FName Name) const
	{
		const auto* Button = RokUiFind::Find(Screen.Get(), Name);
		return Button && Button->GetIsEnabled();
	}

	bool Click(FName Name)
	{
		auto* Button = Cast<UButton>(RokUiFind::Find(Screen.Get(), Name));
		if (!Test.TestTrue(Name.ToString(), Button && Button->GetIsEnabled())) return false;
		Button->OnClicked.Broadcast();
		Screen->FlushPendingActionForTesting();
		return true;
	}

	void Capture(const TCHAR* Name)
	{
		Pending = Output / (FString(Name) + TEXT(".png"));
		Deadline = FPlatformTime::Seconds() + 8.0;
		FScreenshotRequest::RequestScreenshot(Pending, true, false);
	}

	FAutomationTestBase& Test;
	TWeakObjectPtr<URokUnityFeatureScreenWidget> Screen;
	FString Output;
	FString Pending;
	double Deadline = 0;
	double Resume = 0;
	int32 Step = 0;
	bool bLocked = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokExpeditionRenderedFlowTest, "Rok.Ui.Expedition.RenderedGateAndSetup",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FRokExpeditionRenderedFlowTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FCheckExpeditionFlow(*this));
	return true;
}

#endif
