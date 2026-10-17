#include "RokTrainTierAuditSubsystem.h"
#include "Components/Slider.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokTrainArmyMeshWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible()) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible()) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && !Parent->IsVisible()) return false;
	return true;
}
}

bool URokTrainTierAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokTrainTierAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}

bool URokTrainTierAuditSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game;
}

void URokTrainTierAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/TrainTierAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokTrainTierAuditSubsystem::ScreenshotProcessed);
}

void URokTrainTierAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}

TStatId URokTrainTierAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokTrainTierAuditSubsystem, STATGROUP_Tickables);
}

bool URokTrainTierAuditSubsystem::FindRuntime()
{
	const TCHAR* Cursor = FCommandLine::Get(); FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok")) && Token.EndsWith(TEXT("Audit")) && !Token.Equals(TEXT("-RokTrainTierAudit"), ESearchCase::IgnoreCase))
		{ Fail(TEXT("TrainTier audit cannot share a process with another runtime interaction audit.")); return false; }
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	if (!Project.Equals(TEXT("D:/ueDemo/Rok"), ESearchCase::IgnoreCase)
		|| GetWorld()->GetName() != TEXT("L_RokUnityGameAppCity"))
	{ Fail(TEXT("Wrong project/world: city audit made no UI interaction.")); return false; }
	FString AutoSelect;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokAutoSelectBuilding="), AutoSelect) || AutoSelect != TEXT("UnityInitId_6"))
	{ Fail(TEXT("Requires -RokAutoSelectBuilding=UnityInitId_6.")); return false; }
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("UnityInitId_6"))) ++ActorCount;
	if (ActorCount != 1) { Fail(TEXT("Expected exactly one source UnityInitId_6 city actor.")); return false; }
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	int32 Count = 0;
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld() && Visible(Widget)
		&& Widget->GetCachedGeometry().GetLocalSize().X > 1)
	{ Hud = Cast<URokMainHudWidget>(Widget); ++Count; }
	if (Count != 1) { Fail(TEXT("Expected one attached visible city HUD.")); return false; }
	Found.Reset(); Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokUnityFeatureScreenWidget::StaticClass(), false);
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld())
	{ Feature = Cast<URokUnityFeatureScreenWidget>(Widget); ++Count; }
	if (Count != 1 || Feature->IsScreenOpen())
	{ Fail(TEXT("Expected one closed Feature host before building route.")); return false; }
	return true;
}

bool URokTrainTierAuditSubsystem::LoadTier()
{
	auto* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!Catalog) { Fail(TEXT("Missing runtime gameplay catalog.")); return false; }
	const auto Rows = Catalog->GetTroopsForArmyType(1);
	const auto* Row = Rows.FindByPredicate([this](const auto& Candidate) { return Candidate.Level == Tier; });
	FString Error;
	if (!Row || !RokTrainArmyDetail::Load(Row->Id, Detail, Error))
	{ Fail(FString::Printf(TEXT("Missing T%d source detail: %s"), Tier, *Error)); return false; }
	ExpectedName = Detail.Name;
	StudyId = Row->StudyId;
	return true;
}

bool URokTrainTierAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	// Never dispatch Start: this audit only reads/browses authoritative-unknown data.
	if (Name == TEXT("TrainArmyStart")) { Fail(TEXT("Training dispatch forbidden in browse audit.")); return false; }
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Button || Owner->GetWorld() != GetWorld() || !Visible(Button) || !Button->GetIsEnabled())
	{ Fail(FString::Printf(TEXT("Missing/hidden/disabled button %s"), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("button"), Name.ToString());
	Action->SetNumberField(TEXT("time_seconds"), Elapsed);
	Action->SetNumberField(TEXT("frame"), double(Frame));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	if (Name == TEXT("TrainArmyDetailButton") || Name == TEXT("TrainArmyDetailBackButton") || Name.ToString().StartsWith(TEXT("TrainArmyTier_")))
	{
		auto* Model = Cast<URokTrainArmyMeshWidget>(Feature->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")));
		if (!Model) { Fail(TEXT("Missing pre-action animation model.")); return false; }
		PendingAnimationAction = Name; ActionModel = Model;
		ActionArmsId = Model->GetArmsId(); ActionTime = Model->GetAnimationTime();
		ActionGeneration = Model->GetLoadGeneration(); ActionAt = Elapsed;
	}
	Button->OnClicked.Broadcast();
	return true;
}

bool URokTrainTierAuditSubsystem::ResolveAnimationAction()
{
#if !UE_BUILD_SHIPPING
	if (PendingAnimationAction.IsNone()) return true;
	if (Elapsed - ActionAt > 10) { Fail(TEXT("Deferred animation action did not complete.")); return false; }
	auto* Model = Cast<URokTrainArmyMeshWidget>(Feature->GetWidgetFromName(TEXT("TrainArmyAnimatedModel")));
	const bool bTier = PendingAnimationAction.ToString().StartsWith(TEXT("TrainArmyTier_"));
	if (bTier)
	{
		if (Feature->GetSelectedTrainTierForAudit() != Tier || !Model || Model->GetArmsId() != Detail.Id) return false;
		const bool bNewLoad = Model == ActionModel.Get() ? Model->GetLoadGeneration() > ActionGeneration : Model->GetLoadGeneration() > 0;
		if (!Model->IsModelLoaded() || (ActionArmsId != Detail.Id
			&& (!bNewLoad || !FMath::IsNearlyZero(Model->GetLastLoadStartTime()))))
		{ Fail(TEXT("Deferred tier action did not load new Arms at source time zero.")); return false; }
	}
	else
	{
		const bool bDetail = PendingAnimationAction == TEXT("TrainArmyDetailButton");
		if ((Feature->GetWidgetFromName(TEXT("TrainArmyDetailRoot")) != nullptr) != bDetail) return false;
		if (!Model || Model != ActionModel.Get() || Model->GetArmsId() != ActionArmsId || Model->GetLoadGeneration() != ActionGeneration)
		{ Fail(TEXT("Completed detail transition replaced or reloaded the animation.")); return false; }
		const float Current = Model->GetAnimationTime();
		const float Advance = Current >= ActionTime ? Current - ActionTime : Model->GetAnimationDuration() - ActionTime + Current;
		if (Advance > Elapsed - ActionAt + .15)
		{ Fail(TEXT("Completed detail transition reset the animation away from its loop boundary.")); return false; }
	}
	AnimatedModel = Model; ObservedArmsId = Detail.Id; PaintedFrames.Reset();
	LastAnimationTime = Model->GetAnimationTime(); LastAnimationObservedAt = Elapsed; LastPaintCount = Model->GetPaintCount();
	auto Check = AnimationState(); Check->SetStringField(TEXT("completed_transition"), PendingAnimationAction.ToString());
	Check->SetNumberField(TEXT("before_clock"), ActionTime); Check->SetNumberField(TEXT("before_load_generation"), double(ActionGeneration));
	Check->SetNumberField(TEXT("load_generation"), double(Model->GetLoadGeneration()));
	Check->SetNumberField(TEXT("last_load_start_time"), Model->GetLastLoadStartTime());
	AnimationChecks.Add(MakeShared<FJsonValueObject>(Check));
	PendingAnimationAction = NAME_None; StageStarted = Elapsed;
#endif
	return true;
}

TSharedRef<FJsonObject> URokTrainTierAuditSubsystem::AnimationState() const
{
	auto State = MakeShared<FJsonObject>();
	if (auto* Model = AnimatedModel.Get())
	{
		State->SetNumberField(TEXT("arms_id"), Model->GetArmsId());
		State->SetNumberField(TEXT("widget_identity"), Model->GetUniqueID());
		State->SetNumberField(TEXT("clock_seconds"), Model->GetAnimationTime());
		State->SetNumberField(TEXT("duration_seconds"), Model->GetAnimationDuration());
		State->SetNumberField(TEXT("paint_count"), double(Model->GetPaintCount()));
		State->SetNumberField(TEXT("paint_frame"), Model->GetLastPaintFrameIndex());
		State->SetNumberField(TEXT("draw_count"), Model->GetLastPaintDrawCount());
		State->SetNumberField(TEXT("distinct_painted_frames"), PaintedFrames.Num());
		State->SetNumberField(TEXT("audit_time"), Elapsed);
	}
	return State;
}

bool URokTrainTierAuditSubsystem::ObserveAnimation(bool bRequirePaint)
{
	auto* Model = Feature.IsValid() ? Cast<URokTrainArmyMeshWidget>(Feature->GetWidgetFromName(TEXT("TrainArmyAnimatedModel"))) : nullptr;
	if (!Model || !Visible(Model) || !Model->IsModelLoaded() || Model->GetArmsId() != Detail.Id || Model->GetFrameCount() < 2 || Model->GetAnimationDuration() <= 0)
	{ Fail(TEXT("Actual infantry barracks animation missing, unloaded, or wrong Arms identity.")); return false; }
	if (ObservedArmsId != Detail.Id || !AnimatedModel.IsValid())
	{
		AnimatedModel = Model; ObservedArmsId = Detail.Id; PaintedFrames.Reset();
		LastAnimationTime = Model->GetAnimationTime(); LastAnimationObservedAt = Elapsed; LastPaintCount = 0;
	}
	if (AnimatedModel.Get() != Model) { Fail(TEXT("Same-tier animation widget identity changed.")); return false; }
	const float Current = Model->GetAnimationTime(), Duration = Model->GetAnimationDuration();
	// A decrease is valid only at the clip boundary within elapsed real ticks. No manual advancement.
	if (Current + .001f < LastAnimationTime && Duration - LastAnimationTime + Current > Elapsed - LastAnimationObservedAt + .15)
	{ Fail(TEXT("Animation clock reset away from a natural source loop boundary.")); return false; }
	if (Model->GetPaintCount() > LastPaintCount && Model->GetLastPaintDrawCount() > 0)
	{
		const int32 Index = Model->GetLastPaintFrameIndex();
		if (Index < 0 || Index >= Model->GetFrameCount()) { Fail(TEXT("Real paint reported an invalid animation frame.")); return false; }
		PaintedFrames.Add(Index);
	}
	LastAnimationTime = Current; LastAnimationObservedAt = Elapsed; LastPaintCount = Model->GetPaintCount();
	if (bRequirePaint && (PaintedFrames.Num() < 2 || Model->GetLastPaintDrawCount() <= 0))
	{ Fail(TEXT("Actual paint has not demonstrated two moving nonempty animation frames.")); return false; }
	return true;
}

bool URokTrainTierAuditSubsystem::CheckText(FName Name, const FString& Expected)
{
	auto* Text = Cast<UTextBlock>(Feature->GetWidgetFromName(Name));
	if (!Visible(Text) || Text->GetText().ToString() != Expected)
	{ Fail(FString::Printf(TEXT("Text mismatch %s: expected '%s', actual '%s'"), *Name.ToString(),
		*Expected, Text ? *Text->GetText().ToString() : TEXT("<missing>"))); return false; }
	return true;
}

bool URokTrainTierAuditSubsystem::Validate(bool bDetail)
{
#if !UE_BUILD_SHIPPING
	if (!Feature.IsValid() || !Feature->IsScreenOpen()
		|| Feature->GetSelectedTrainTierForAudit() != Tier || Feature->GetSelectedTrainArmyTypeForAudit() != 1)
	{ Fail(TEXT("Selected tier/army type differs from real button route.")); return false; }
	AActor* Context = Feature->GetFeatureContextActorForAudit();
	if (!Context || Context->GetWorld() != GetWorld() || !Context->ActorHasTag(TEXT("UnityInitId_6")))
	{ Fail(TEXT("Feature route lost actual UnityInitId_6 building context.")); return false; }
	if (!CheckText(bDetail ? TEXT("TrainArmyDetailName") : TEXT("TrainArmySelectedName"), bDetail ? Detail.Name : ExpectedName)) return false;
	if (bDetail)
	{
		for (int32 Index = 0; Index < Detail.Attributes.Num(); ++Index)
		{
			const auto& Attribute = Detail.Attributes[Index];
			if (!CheckText(*FString::Printf(TEXT("TrainArmyDetailAttribute_%d"), Index),
				Attribute.Value + (Attribute.bModifierUnknown ? TEXT(" + —") : TEXT("")))) return false;
		}
		if (!CheckText(TEXT("TrainArmyDetailModifierStatus"), TEXT("—：个人属性加成待同步"))) return false;
		for (int32 Index = 0; Index < Detail.Skills.Num(); ++Index)
			if (!Visible(Feature->GetWidgetFromName(*FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), Index))))
			{ Fail(TEXT("Source skill button missing in current tier details.")); return false; }
		if (Feature->GetWidgetFromName(*FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), Detail.Skills.Num())))
		{ Fail(TEXT("Previous tier left an extra skill button.")); return false; }
	}
	else if (StudyId != 0)
	{
		for (const TCHAR* Name : {TEXT("TrainArmyStart"), TEXT("TrainArmyPlus"), TEXT("TrainArmyMinus"), TEXT("TrainArmyMax"), TEXT("TrainArmyQuantity"), TEXT("TrainArmyCostAndTime"), TEXT("TrainArmyQuantityShade"),
			TEXT("TrainArmyQuantityInput"), TEXT("TrainArmyQuantitySlider"), TEXT("TrainArmyQuantityRegion"), TEXT("TrainResourceCostRoot"),
			TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill"),
			TEXT("TrainArmyParameterQuantityBand"), TEXT("TrainArmyParameterCostBackground")})
			if (Visible(Feature->GetWidgetFromName(Name)))
			{ Fail(FString::Printf(TEXT("Unknown technology exposes training control %s"), Name)); return false; }
		if (!CheckText(TEXT("TrainArmyStudyStatus"), TEXT("当前兵阶需要科技解锁，解锁状态待同步。可查看兵种详情。"))) return false;
	}
	if (!ObserveAnimation(true)) return false;
#endif
	return true;
}

bool URokTrainTierAuditSubsystem::EditQuantityThroughSlider(int32 Count, int32 Capacity)
{
	auto* Slider = Feature.IsValid() ? Cast<USlider>(Feature->GetWidgetFromName(TEXT("TrainArmyQuantitySlider"))) : nullptr;
	if (!Visible(Slider) || !Slider->GetIsEnabled() || !Slider->OnValueChanged.IsBound()
		|| Capacity <= 1 || Count < 1 || Count > Capacity)
	{ Fail(TEXT("Source quantity slider is unavailable for the known tier.")); return false; }
	const float Value = static_cast<float>(Count) / Capacity;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("control"), TEXT("TrainArmyQuantitySlider"));
	Action->SetNumberField(TEXT("on_value_changed"), Value);
	Action->SetNumberField(TEXT("expected_count"), Count);
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	Slider->SetValue(Value);
	Slider->OnValueChanged.Broadcast(Value);
	return true;
}

void URokTrainTierAuditSubsystem::Enter(EStage Next)
{
	Stage = Next; StageStarted = Elapsed;
}

void URokTrainTierAuditSubsystem::Capture(const FString& Name)
{
	if (FScreenshotRequest::IsScreenshotRequested()) { Fail(TEXT("Another screenshot request is active.")); return; }
	ScreenshotPath = OutputDirectory / (Name + TEXT(".png"));
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("name"), Name);
	PendingCapture->SetStringField(TEXT("path"), ScreenshotPath);
	PendingCapture->SetNumberField(TEXT("tier"), Tier);
	PendingCapture->SetNumberField(TEXT("troop_id"), Detail.Id);
	PendingCapture->SetNumberField(TEXT("study_id_unknown"), StudyId);
	PendingCapture->SetNumberField(TEXT("request_time"), Elapsed);
	PendingCapture->SetNumberField(TEXT("request_frame"), double(Frame));
	PendingCapture->SetObjectField(TEXT("animation_request"), AnimationState());
#if !UE_BUILD_SHIPPING
	PendingCapture->SetNumberField(TEXT("selected_count"), Feature->GetSelectedTrainCountForAudit());
#endif
	RequestedAt = Elapsed;
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
}

void URokTrainTierAuditSubsystem::ScreenshotProcessed()
{
	if (!PendingCapture.IsValid() || bFinished) return;
	const FString CaptureName = PendingCapture->GetStringField(TEXT("name"));
	if (!Validate(CaptureName.EndsWith(TEXT("_details")))) return;
	const int64 Bytes = IFileManager::Get().FileSize(*ScreenshotPath);
	if (Bytes <= 0) { Fail(TEXT("Processed screenshot missing/empty.")); return; }
	PendingCapture->SetNumberField(TEXT("bytes"), double(Bytes));
	PendingCapture->SetNumberField(TEXT("processed_time"), Elapsed);
	PendingCapture->SetObjectField(TEXT("animation_processed"), AnimationState());
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture));
	PendingCapture.Reset();
	if (!Save(false)) Fail(TEXT("Unable to save progress manifest."));
}

void URokTrainTierAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime); ++Frame;
	if (Elapsed > 120) { Fail(TEXT("Training tier audit timed out.")); return; }
	if (!ResolveAnimationAction()) return;
	if ((Stage >= EStage::Selected && Stage <= EStage::Returned) || Stage == EStage::Reopened
		|| (Stage == EStage::FinalClosed && Feature.IsValid() && Feature->IsScreenOpen()))
		if (!ObserveAnimation(false)) return;
	if (PendingCapture.IsValid())
	{
		if (Elapsed - RequestedAt > 10) Fail(TEXT("Screenshot processing timed out."));
		return;
	}
	if (Elapsed - StageStarted < (Stage == EStage::Startup ? 3.0 : 0.5)) return;
#if !UE_BUILD_SHIPPING
	switch (Stage)
	{
	case EStage::Startup:
		if (FindRuntime() && Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"))) Enter(EStage::Opened);
		break;
	case EStage::Opened:
		if (!LoadTier()) return;
		if (Click(Feature.Get(), *FString::Printf(TEXT("TrainArmyTier_%d"), Tier - 1))) Enter(EStage::Selected);
		break;
	case EStage::Selected:
		if (!Validate(false)) return;
		// Tier selection starts at capacity. First make room, then prove both deltas.
		QuantityBeforeAdjustment = Feature->GetSelectedTrainCountForAudit();
		if (StudyId == 0)
		{
			if (QuantityBeforeAdjustment <= 1) { Fail(TEXT("Known tier capacity cannot exercise quantity adjustment.")); return; }
			if (EditQuantityThroughSlider(QuantityBeforeAdjustment - 1, QuantityBeforeAdjustment)) Enter(EStage::QuantityReduced);
			break;
		}
		Enter(EStage::ParametersReady); break;
	case EStage::QuantityReduced:
		if (Feature->GetSelectedTrainCountForAudit() != QuantityBeforeAdjustment - 1)
		{ Fail(TEXT("Source slider must reduce requested count by exactly one.")); return; }
		if (EditQuantityThroughSlider(QuantityBeforeAdjustment, QuantityBeforeAdjustment)) Enter(EStage::ParametersReady);
		break;
	case EStage::ParametersReady:
		if (!Validate(false)) return;
		SelectedCount = Feature->GetSelectedTrainCountForAudit();
		if (StudyId == 0)
		{
			if (SelectedCount != QuantityBeforeAdjustment)
			{ Fail(TEXT("Source slider must restore exactly the previous quantity.")); return; }
			auto Check = MakeShared<FJsonObject>();
			Check->SetStringField(TEXT("kind"), TEXT("quantity_slider_reduce_restore"));
			Check->SetNumberField(TEXT("tier"), Tier);
			Check->SetNumberField(TEXT("before"), QuantityBeforeAdjustment);
			Check->SetNumberField(TEXT("after_slider_reduction"), QuantityBeforeAdjustment - 1);
			Check->SetNumberField(TEXT("after_slider_restore"), SelectedCount);
			QuantityChecks.Add(MakeShared<FJsonValueObject>(Check));
		}
		Capture(FString::Printf(TEXT("tier%d_parameters"), Tier));
		Enter(EStage::Detail); break;
	case EStage::Detail:
		if (Click(Feature.Get(), TEXT("TrainArmyDetailButton"))) { SkillIndex = -1; Enter(EStage::Skill); }
		break;
	case EStage::Skill:
		if (SkillIndex == -1)
		{
			if (!Validate(true)) return;
			Capture(FString::Printf(TEXT("tier%d_details"), Tier));
			SkillIndex = 0; Enter(EStage::SkillClosed); break;
		}
		if (!CheckText(TEXT("TrainArmyDetailTipText"), Detail.Skills[SkillIndex].Tip)) return;
		{
			auto Check = MakeShared<FJsonObject>();
			Check->SetNumberField(TEXT("tier"), Tier);
			Check->SetNumberField(TEXT("troop_id"), Detail.Id);
			Check->SetNumberField(TEXT("skill_id"), Detail.Skills[SkillIndex].Id);
			Check->SetStringField(TEXT("verified_tip"), Detail.Skills[SkillIndex].Tip);
			Checks.Add(MakeShared<FJsonValueObject>(Check));
		}
		if (Click(Feature.Get(), TEXT("TrainArmyDetailTipClickAway"))) { ++SkillIndex; Enter(EStage::SkillClosed); }
		break;
	case EStage::SkillClosed:
		if (Feature->GetWidgetFromName(TEXT("TrainArmyDetailTipText"))) { Fail(TEXT("Tip failed to close.")); return; }
		if (SkillIndex < Detail.Skills.Num())
		{
			if (Click(Feature.Get(), *FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), SkillIndex))) Enter(EStage::Skill);
		}
		else if (Click(Feature.Get(), TEXT("TrainArmyDetailBackButton"))) Enter(EStage::Returned);
		break;
	case EStage::Returned:
		if (!Validate(false)) return;
		if (Feature->GetSelectedTrainCountForAudit() != SelectedCount)
		{ Fail(TEXT("Detail return changed requested quantity.")); return; }
		if (Feature->GetWidgetFromName(TEXT("TrainArmyDetailRoot"))) { Fail(TEXT("Detail page survived return.")); return; }
		if (Tier < 5) { ++Tier; Enter(EStage::Opened); }
		else if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) Enter(EStage::Closed);
		break;
	case EStage::Closed:
		if (Feature->IsScreenOpen()) { Fail(TEXT("Training window did not close.")); return; }
		if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton")))
		{ Tier = 1; if (LoadTier()) Enter(EStage::Reopened); }
		break;
	case EStage::Reopened:
		if (!Validate(false)) return;
		if (Feature->GetSelectedTrainCountForAudit() != 1) { Fail(TEXT("Reopen did not reset quantity.")); return; }
		Capture(TEXT("reopen_tier1")); Enter(EStage::FinalClosed); break;
	case EStage::FinalClosed:
		if (Feature->IsScreenOpen())
		{
			if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) StageStarted = Elapsed;
		}
		else Finish();
		break;
	}
#endif
}

bool URokTrainTierAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), bComplete);
	Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == 11);
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("world"), GetWorld()->GetName());
	Root->SetStringField(TEXT("authority_boundary"), TEXT("Config-only tier browsing; technology ownership unknown; no TrainArmyStart dispatched, no training command requested. This audit does not validate backend authority."));
	Root->SetStringField(TEXT("input_method"), TEXT("Existing visible enabled UMG button OnClicked and source quantity slider OnValueChanged delegates; real NativeTick, no TestingAdvance; physical typing, dragging and pointer hit testing are separate acceptance gates."));
	Root->SetNumberField(TEXT("audit_version"), 4);
	Root->SetStringField(TEXT("animation_scope"), TEXT("Actual UnityInitId_6 infantry barracks T1-T5. Existing parameter/detail captures sample different real times; per-tick paint frame diversity required. Other army types/civilization ownership not validated."));
	Root->SetArrayField(TEXT("animation_checks"), AnimationChecks);
	Root->SetArrayField(TEXT("actions"), Actions); Root->SetArrayField(TEXT("captures"), Captures); Root->SetArrayField(TEXT("skill_checks"), Checks);
	Root->SetArrayField(TEXT("quantity_checks"), QuantityChecks);
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))) return false;
	return FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void URokTrainTierAuditSubsystem::Fail(const FString& Reason)
{
	if (bFinished) return;
	Failures.Add(Reason); bFinished = true;
	Save(false);
	UE_LOG(LogTemp, Error, TEXT("Rok TrainTier audit FAIL: %s; %s"), *Reason, *OutputDirectory);
	FPlatformMisc::RequestExit(false);
}

void URokTrainTierAuditSubsystem::Finish()
{
	if (Captures.Num() != 11) { Fail(TEXT("Expected exactly 11 processed screenshots.")); return; }
	if (!Save(true)) { Fail(TEXT("Final manifest write failed.")); return; }
	bFinished = true;
	UE_LOG(LogTemp, Display, TEXT("Rok TrainTier audit PASS: %s"), *OutputDirectory);
	FPlatformMisc::RequestExit(false);
}
