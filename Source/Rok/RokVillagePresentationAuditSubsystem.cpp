#include "RokVillagePresentationAuditSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "RokWorldMapConfigActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokVillagePresentationComponent.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
const FVector2D VillageAuditFocus(1142.0, 158.0);

TSharedPtr<FJsonObject> VillageAuditObject(const TSharedPtr<FJsonValue>& Value)
{
	return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
}

bool ReadVillageIds(const TSharedPtr<FJsonObject>& Sample, const TCHAR* Field, TArray<FString>& Items)
{
	Items.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Sample.IsValid() || !Sample->TryGetArrayField(Field, Values) || !Values || Values->IsEmpty()) return false;
	for (const auto& Value : *Values)
	{
		FString Name;
		// Unity fileIDs exceed exact JSON double precision. Never accept numeric IDs.
		if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(Name)
			|| Name.IsEmpty() || Name == TEXT("0") || Items.Contains(Name)) return false;
		for (TCHAR Character : Name) if (Character < TEXT('0') || Character > TEXT('9')) return false;
		Items.Add(Name);
	}
	return true;
}

bool VillageParentChainVisible(const USceneComponent* Component, bool CheckOwner = true)
{
	for (const USceneComponent* Parent = Component; Parent; Parent = Parent->GetAttachParent())
		if (!Parent->IsVisible() || Parent->bHiddenInGame || !Parent->IsRegistered()
			|| (CheckOwner && Parent->GetOwner() && Parent->GetOwner()->IsHidden())) return false;
	return Component != nullptr;
}
}

bool URokVillagePresentationAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokVillagePresentationAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokVillagePresentationAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokVillagePresentationAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/VillagePresentationAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokVillagePresentationAuditSubsystem::ScreenshotProcessed);
}
void URokVillagePresentationAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokVillagePresentationAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokVillagePresentationAuditSubsystem, STATGROUP_Tickables); }
bool URokVillagePresentationAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype"))
	{ Finish(TEXT("Wrong project/map; camera unchanged.")); return false; }
	return true;
}

bool URokVillagePresentationAuditSubsystem::Begin()
{
	if (!CheckIdentity()) return false;
	for (const TCHAR* Other : {TEXT("RokStoneDepositAudit"), TEXT("RokWoodDepositAudit"), TEXT("RokFoodDepositAudit"), TEXT("RokTrainQuantityAudit"), TEXT("RokLeaderboardAudit"), TEXT("RokWorldEntityAnimationAudit"), TEXT("RokUiAnimationAudit"), TEXT("RokTrainTierAudit"), TEXT("RokPlayerHeadAudit"), TEXT("RokPlayerActionTipAudit"), TEXT("RokArmyShowAudit"), TEXT("RokBuildingLevelDetailAudit"), TEXT("RokChatAnimationAudit"), TEXT("RokCommanderDataAudit"), TEXT("RokCommanderRosterAudit"), TEXT("RokScoutKeyboardAudit"), TEXT("RokScoutScreenAudit"), TEXT("RokTrainArmyDetailAudit")})
		if (FParse::Param(FCommandLine::Get(), Other)) { Finish(TEXT("Run village audit without another automatic UI/camera audit.")); return false; }
	int32 CameraCount = 0, MatchCount = 0;
	for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++CameraCount; }
	for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld()); It; ++It)
		if (It->GetEntityKind() == TEXT("village") && It->GetSourceId() == 639) { Entity = *It; ++MatchCount; }
	if (CameraCount != 1 || MatchCount != 1) { Finish(TEXT("Expected exactly one runtime camera and source village 639.")); return false; }
	Village = Entity->FindComponentByClass<URokVillagePresentationComponent>();
	if (!Village.IsValid()) { Finish(TEXT("Runtime village component missing.")); return false; }
	const auto* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Player->GetPawn() != Camera.Get() || Player->GetViewTarget() != Camera.Get()
		|| !Entity->GetUnityPosition().Equals(FVector2D(6852, 948), 0.001))
	{ Finish(TEXT("Village source actor or possessed camera identity differs; camera unchanged.")); return false; }
	if (!Source.Load() || Source.Items.Num() != 13 || !FMath::IsFinite(Source.UnitsToCentimeters) || Source.UnitsToCentimeters <= 0)
	{ Finish(TEXT("Village source geometry cannot be loaded.")); return false; }
	for (const auto& Item : Source.Items)
		if (Item.SourceName.IsEmpty() || Item.SourceTransformId.IsEmpty() || Item.SourceRendererId.IsEmpty()
			|| !FMath::IsFinite(Item.SpriteSize.X) || !FMath::IsFinite(Item.SpriteSize.Y) || Item.SpriteSize.X <= 0 || Item.SpriteSize.Y <= 0)
		{ Finish(TEXT("Village geometry must retain thirteen layers and unique source identities.")); return false; }
	if (!LoadNativeSamples() || !PrepareIntegrationSamples()) return false;
	Camera->FocusDisplayCoordinate(VillageAuditFocus);
	NextSample(); return true;
}

bool URokVillagePresentationAuditSubsystem::LoadNativeSamples()
{
	FString Json;
	TSharedPtr<FJsonObject> Native;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_village_presentation_native_probe.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Native) || !Native.IsValid())
	{ Finish(TEXT("Native Unity probe cannot be read.")); return false; }
	const TArray<TSharedPtr<FJsonValue>>* Runs = nullptr;
	if (!Native->TryGetArrayField(TEXT("runs"), Runs) || !Runs || Runs->Num() != 3)
	{ Finish(TEXT("Expected the three captured Unity instances.")); return false; }
	TArray<FString> SourceOrder;
	for (const auto& Item : Source.Items) SourceOrder.Add(Item.SourceTransformId);
	const TArray<TSharedPtr<FJsonValue>>* Mappings = nullptr;
	if (!Native->TryGetArrayField(TEXT("mappings"), Mappings) || !Mappings || Mappings->Num() != 13)
	{ Finish(TEXT("Expected thirteen native Transform/SpriteRenderer identity mappings.")); return false; }
	TSet<FString> Mapped;
	for (const auto& Value : *Mappings)
	{
		const auto Mapping = VillageAuditObject(Value);
		FString Id, Name, Renderer; int32 Sibling = -1;
		if (!Mapping || !Mapping->HasTypedField<EJson::String>(TEXT("sourceTransformId"))
			|| !Mapping->HasTypedField<EJson::String>(TEXT("sourceRendererId"))
			|| !Mapping->TryGetStringField(TEXT("sourceTransformId"), Id) || !Mapping->TryGetStringField(TEXT("sourceName"), Name)
			|| !Mapping->TryGetStringField(TEXT("sourceRendererId"), Renderer) || !Mapping->TryGetNumberField(TEXT("siblingIndex"), Sibling)
			|| Mapped.Contains(Id))
		{ Finish(TEXT("Invalid native village identity mapping schema.")); return false; }
		const auto* Item = Source.Items.FindByPredicate([&Id](const auto& Candidate) { return Candidate.SourceTransformId == Id; });
		if (!Item || Name != Item->SourceName || Renderer != Item->SourceRendererId || Sibling != Item->SiblingIndex)
		{ Finish(TEXT("Native village identities differ from source geometry.")); return false; }
		Mapped.Add(Id);
	}
	const double ExpectedDxf[] = {480, 594.4174194335938, 900, 1500, 2500, 2999, 3000, 1500, 900, 594.4174194335938, 480};
	const int32 ExpectedActiveCount[] = {13, 12, 10, 1, 1, 1, 1, 1, 10, 12, 13};
	for (int32 RunIndex = 0; RunIndex < Runs->Num(); ++RunIndex)
	{
		const auto Run = VillageAuditObject((*Runs)[RunIndex]);
		const TArray<TSharedPtr<FJsonValue>>* NativeSamples = nullptr;
		double Instance = -1;
		if (!Run || !Run->TryGetNumberField(TEXT("instance"), Instance) || Instance != RunIndex
			|| !Run->TryGetArrayField(TEXT("samples"), NativeSamples) || !NativeSamples || NativeSamples->Num() != 11)
		{ Finish(TEXT("Native village run must be an object containing eleven samples.")); return false; }
		for (int32 Index = 0; Index < NativeSamples->Num(); ++Index)
		{
			const auto Sample = VillageAuditObject((*NativeSamples)[Index]);
			TArray<FString> Active, Order;
			double Dxf = 0, Scale = 0;
			if (!ReadVillageIds(Sample, TEXT("active"), Active) || !ReadVillageIds(Sample, TEXT("order"), Order)
				|| Order != SourceOrder || !Sample->TryGetNumberField(TEXT("dxf"), Dxf) || !Sample->TryGetNumberField(TEXT("scale"), Scale)
				|| !FMath::IsFinite(Dxf) || !FMath::IsFinite(Scale) || Scale <= 0
				|| Active.Num() != ExpectedActiveCount[Index]
				|| (Index == 6 && !FMath::IsNearlyEqual(Scale, 3.299999237060547, 0.000001))
				|| !FMath::IsNearlyEqual(Dxf, ExpectedDxf[Index], 0.0001))
			{ Finish(TEXT("Invalid native village sample schema, source IDs or zoom sequence.")); return false; }
			for (const auto& Id : Active) if (!Mapped.Contains(Id))
			{ Finish(TEXT("Native village active array contains an unknown source ID.")); return false; }
			if (RunIndex > 0)
			{
				const auto First = Samples[Index]->AsObject(); TArray<FString> FirstActive;
				if (!ReadVillageIds(First, TEXT("active"), FirstActive) || Active != FirstActive
					|| Scale != First->GetNumberField(TEXT("scale")) || Dxf != First->GetNumberField(TEXT("dxf")))
				{ Finish(TEXT("The three native village runs disagree; no single replay can represent them.")); return false; }
			}
		}
		if (RunIndex == 0) Samples = *NativeSamples;
	}
	return true;
}
bool URokVillagePresentationAuditSubsystem::PrepareIntegrationSamples()
{
	// Read the production population contract, then cross-check its loaded record.
	FString Json; TSharedPtr<FJsonObject> Population;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_world_entity_population.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Population) || !Population)
	{ Finish(TEXT("Missing world entity LOD contract.")); return false; }
	const TSharedPtr<FJsonObject>* Lod = nullptr;
	double Close = 0, Tactical = 0, Far = 0;
	if (!Population->TryGetObjectField(TEXT("lod_contract"), Lod) || !Lod || !Lod->IsValid()
		|| !(*Lod)->TryGetNumberField(TEXT("close_detail_max_dxf"), Close)
		|| !(*Lod)->TryGetNumberField(TEXT("tactical_detail_max_dxf"), Tactical)
		|| !(*Lod)->TryGetNumberField(TEXT("far_icon_max_dxf"), Far)
		|| !FMath::IsFinite(Close) || !FMath::IsFinite(Tactical) || !FMath::IsFinite(Far)
		|| !(Close > 480 && Tactical >= 3000 && Far > Tactical))
	{ Finish(TEXT("Invalid source LOD boundaries for the native replay.")); return false; }
	const auto* PopulationSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<URokWorldMapEntitySubsystem>();
	const auto* Record = PopulationSubsystem ? PopulationSubsystem->GetEntities().FindByPredicate([](const auto& Row)
		{ return Row.SourceId == 639 && Row.Kind == TEXT("village"); }) : nullptr;
	if (!Record || !FMath::IsNearlyEqual(double(Record->CloseDetailMaxDxf), Close, .001)
		|| !FMath::IsNearlyEqual(double(Record->TacticalDetailMaxDxf), Tactical, .001)
		|| !FMath::IsNearlyEqual(double(Record->FarIconMaxDxf), Far, .001))
	{ Finish(TEXT("Loaded village LOD contract differs from source population.")); return false; }
	int32 ConfigCount = 0; ARokWorldMapConfigActor* Config = nullptr;
	for (TActorIterator<ARokWorldMapConfigActor> It(GetWorld()); It; ++It) { Config = *It; ++ConfigCount; }
	const double FarDxf = (Tactical + Far) / 2.0;
	const double HiddenDxf = Far + (Far - Tactical) / 2.0;
	if (ConfigCount != 1 || !FMath::IsNearlyEqual(double(Config->ClampDxf(FarDxf)), FarDxf, .001)
		|| !FMath::IsNearlyEqual(double(Config->ClampDxf(HiddenDxf)), HiddenDxf, .001))
	{ Finish(TEXT("Source camera limits cannot reach the requested Far/Hidden probes.")); return false; }
	const TCHAR* Names[] = {TEXT("far_icon"), TEXT("hidden"), TEXT("return_native_480")};
	const double Distances[] = {FarDxf, HiddenDxf, 480};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		auto Sample = MakeShared<FJsonObject>(); Sample->SetStringField(TEXT("kind"), Names[Index]);
		Sample->SetNumberField(TEXT("dxf"), Distances[Index]); Sample->SetNumberField(TEXT("tactical_max"), Tactical);
		Sample->SetNumberField(TEXT("far_max"), Far); IntegrationSamples.Add(MakeShared<FJsonValueObject>(Sample));
	}
	TArray<UStaticMeshComponent*> Meshes; Entity->GetComponents(Meshes);
	int32 Details = 0, Icons = 0;
	for (auto* Mesh : Meshes)
	{
		if (Mesh->GetFName() == TEXT("DetailVisual")) { LegacyDetail = Mesh; ++Details; }
		if (Mesh->GetFName() == TEXT("FarVisual")) { FarIcon = Mesh; ++Icons; }
	}
	if (Details != 1 || Icons != 1) { Finish(TEXT("Expected the original detail and far components exactly once.")); return false; }
	return true;
}
float URokVillagePresentationAuditSubsystem::RequestedDxf() const
{
	return SampleIndex < Samples.Num() ? Samples[SampleIndex]->AsObject()->GetNumberField(TEXT("dxf"))
		: IntegrationSamples[SampleIndex - Samples.Num()]->AsObject()->GetNumberField(TEXT("dxf"));
}
TSharedPtr<FJsonObject> URokVillagePresentationAuditSubsystem::NativeReference() const
{
	if (SampleIndex < Samples.Num()) return Samples[SampleIndex]->AsObject();
	return SampleIndex == Samples.Num() + 2 ? Samples[0]->AsObject() : nullptr;
}
void URokVillagePresentationAuditSubsystem::NextSample()
{
	if (++SampleIndex == Samples.Num() + IntegrationSamples.Num()) { Finish(); return; }
	Camera->SetCurrentDxf(RequestedDxf());
	StageStarted = Elapsed;
}
TSharedRef<FJsonObject> URokVillagePresentationAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("sample_index"), SampleIndex);
	Result->SetStringField(TEXT("phase"), SampleIndex < Samples.Num() ? TEXT("native_source_sequence") : TEXT("lod_integration_regression"));
	Result->SetBoolField(TEXT("actor_hidden"), Entity->IsHidden());
	Result->SetBoolField(TEXT("legacy_detail_visible"), LegacyDetail->IsVisible());
	Result->SetBoolField(TEXT("legacy_detail_hidden_in_game"), LegacyDetail->bHiddenInGame);
	Result->SetBoolField(TEXT("far_icon_visible"), FarIcon->IsVisible());
	Result->SetBoolField(TEXT("far_icon_hidden_in_game"), FarIcon->bHiddenInGame);
	Result->SetNumberField(TEXT("lod_band"), int32(Entity->GetCurrentLodBand()));
	Result->SetNumberField(TEXT("frame"), double(Frame));
	Result->SetNumberField(TEXT("time_seconds"), Elapsed);
	Result->SetNumberField(TEXT("source_id"), Entity->GetSourceId());
	Result->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf());
	Result->SetNumberField(TEXT("source_scale"), Village->GetSourceScale());
	Result->SetStringField(TEXT("camera_focus"), Camera->GetViewCenterDisplay().ToString());
	TArray<TSharedPtr<FJsonValue>> Active;
	for (const auto& Id : Village->GetActiveSourceItems()) Active.Add(MakeShared<FJsonValueString>(Id));
	Result->SetArrayField(TEXT("active"), Active);
	TArray<UStaticMeshComponent*> Meshes; Entity->GetComponents(Meshes);
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (auto* Mesh : Meshes)
	{
		if (!Mesh->GetName().StartsWith(TEXT("Village_"))) continue;
		const FString Id = Mesh->GetName().RightChop(8);
		const auto* Item = Source.Items.FindByPredicate([&Id](const auto& Candidate) { return Candidate.SourceTransformId == Id; });
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("name"), Mesh->GetName());
		Row->SetStringField(TEXT("source_transform_id"), Id);
		Row->SetStringField(TEXT("source_name"), Item ? Item->SourceName : TEXT("<unknown>"));
		Row->SetStringField(TEXT("source_renderer_id"), Item ? Item->SourceRendererId : TEXT("<unknown>"));
		Row->SetBoolField(TEXT("source_tags_match"), Item && Mesh->ComponentHasTag(FName(*Id))
			&& Mesh->ComponentHasTag(FName(*Item->SourceName))
			&& Mesh->ComponentHasTag(FName(*(TEXT("UnitySpriteRenderer_") + Item->SourceRendererId))));
		Row->SetBoolField(TEXT("registered"), Mesh->IsRegistered());
		Row->SetBoolField(TEXT("attached_to_village"), Mesh->GetAttachParent() == Village.Get());
		Row->SetBoolField(TEXT("parent_visible"), VillageParentChainVisible(Mesh->GetAttachParent()));
		Row->SetStringField(TEXT("mesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : TEXT("<missing>"));
		Row->SetBoolField(TEXT("visible"), Mesh->IsVisible() && !Mesh->bHiddenInGame);
		Row->SetBoolField(TEXT("local_visible"), Mesh->IsVisible());
		Row->SetBoolField(TEXT("hidden_in_game"), Mesh->bHiddenInGame);
		Row->SetNumberField(TEXT("plane_area"), FMath::Abs(Mesh->GetRelativeScale3D().X * Mesh->GetRelativeScale3D().Y));
		Row->SetNumberField(TEXT("scale_x"), Mesh->GetRelativeScale3D().X);
		Row->SetNumberField(TEXT("scale_y"), Mesh->GetRelativeScale3D().Y);
		Row->SetNumberField(TEXT("scale_z"), Mesh->GetRelativeScale3D().Z);
		Row->SetStringField(TEXT("location"), Mesh->GetRelativeLocation().ToString());
		Row->SetStringField(TEXT("material"), TEXT("<missing>"));
		Row->SetBoolField(TEXT("Sprite_raw"), false); Row->SetBoolField(TEXT("Mask_raw"), false);
		Row->SetBoolField(TEXT("source_colors_match"), false);
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
		{
			Row->SetStringField(TEXT("material"), Material->Parent ? Material->Parent->GetPathName() : TEXT("missing"));
			Row->SetBoolField(TEXT("source_colors_match"), Item
				&& Material->K2_GetVectorParameterValue(TEXT("Color1")).Equals(Item->Color1, 0.0001f)
				&& Material->K2_GetVectorParameterValue(TEXT("Color2")).Equals(Item->Color2, 0.0001f));
			for (const TCHAR* Name : {TEXT("Sprite"), TEXT("Mask")})
			{
				auto* Texture = Cast<UTexture2D>(Material->K2_GetTextureParameterValue(Name));
				Row->SetBoolField(FString(Name) + TEXT("_raw"), Texture && !Texture->SRGB);
			}
		}
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	Result->SetArrayField(TEXT("cards"), Rows);
	return Result;
}
bool URokVillagePresentationAuditSubsystem::Validate()
{
	if (!CheckIdentity()) return false;
	if (!Entity.IsValid() || !Camera.IsValid() || !Village.IsValid()) { Finish(TEXT("Runtime village objects were lost.")); return false; }
	const bool bFar = SampleIndex == Samples.Num(), bHidden = SampleIndex == Samples.Num() + 1;
	const bool bDetail = !bFar && !bHidden;
	if (Entity->IsHidden() != bHidden || !VillageParentChainVisible(Village.Get(), false))
	{ Finish(TEXT("Village parent visibility/registration or actor Hidden band mismatch.")); return false; }
	if (!LegacyDetail.IsValid() || !FarIcon.IsValid() || !LegacyDetail->IsRegistered() || !FarIcon->IsRegistered()
		|| LegacyDetail->GetAttachParent() != Entity->GetRootComponent() || FarIcon->GetAttachParent() != Entity->GetRootComponent()
		|| LegacyDetail->IsVisible() || !LegacyDetail->bHiddenInGame
		|| FarIcon->IsVisible() != bFar || FarIcon->bHiddenInGame == bFar
		|| !VillageParentChainVisible(FarIcon->GetAttachParent(), false)
		|| (bFar && (!VillageParentChainVisible(FarIcon.Get()) || !FarIcon->GetStaticMesh() || !FarIcon->GetMaterial(0)
			|| FMath::Abs(FarIcon->GetRelativeScale3D().X * FarIcon->GetRelativeScale3D().Y) <= 0)))
	{ Finish(TEXT("Legacy detail overlap or Far icon/parent visibility mismatch.")); return false; }
	if ((bFar && Entity->GetCurrentLodBand() != ERokWorldMapEntityLodBand::FarIcon)
		|| (bHidden && Entity->GetCurrentLodBand() != ERokWorldMapEntityLodBand::Hidden)
		|| (bDetail && Entity->GetCurrentLodBand() != ERokWorldMapEntityLodBand::CloseDetail
			&& Entity->GetCurrentLodBand() != ERokWorldMapEntityLodBand::TacticalDetail))
	{ Finish(TEXT("Actor LOD band differs from the requested integration phase.")); return false; }
	const auto* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Player->GetPawn() != Camera.Get() || Player->GetViewTarget() != Camera.Get()
		|| !Camera->GetViewCenterDisplay().Equals(VillageAuditFocus, 0.001))
	{ Finish(TEXT("Village screenshot camera is not the possessed view target at the source focus.")); return false; }
	const auto Expected = NativeReference();
	TArray<FString> Active;
	if (Expected && !ReadVillageIds(Expected, TEXT("active"), Active))
	{ Finish(TEXT("Native village active item array is invalid.")); return false; }
	if (!FMath::IsNearlyEqual(Camera->GetCurrentDxf(), RequestedDxf(), .001f)
		|| !FMath::IsFinite(Village->GetSourceScale()) || Village->GetSourceScale() < 0
		|| (Expected && (!FMath::IsNearlyEqual(Village->GetSourceScale(), float(Expected->GetNumberField(TEXT("scale"))), .0001f)
			|| Village->GetActiveSourceItems() != Active)))
	{ Finish(FString::Printf(TEXT("Village source sequence or integration camera mismatch at sample %d."), SampleIndex)); return false; }
	const auto Snapshot = State();
	const auto& Cards = Snapshot->GetArrayField(TEXT("cards"));
	if (Cards.Num() != 13) { Finish(TEXT("Expected thirteen real source mesh components.")); return false; }
	TSet<FString> Seen;
	for (const auto& Value : Cards)
	{
		const auto Card = Value->AsObject();
		const FString Id = Card->GetStringField(TEXT("source_transform_id"));
		const auto* Item = Source.Items.FindByPredicate([&Id](const auto& Candidate) { return Candidate.SourceTransformId == Id; });
		if (!Item || Seen.Contains(Id)) { Finish(TEXT("Missing, duplicate or invented village Transform ID in runtime meshes.")); return false; }
		Seen.Add(Id);
		// Far/Hidden check geometry consistency only; they have no native captured scale oracle.
		const double Scale = Expected ? Expected->GetNumberField(TEXT("scale")) : Village->GetSourceScale();
		const double ExpectedX = Item->SpriteSize.X * Scale * Source.UnitsToCentimeters / 100.0;
		const double ExpectedY = -Item->SpriteSize.Y * Scale * Source.UnitsToCentimeters / 100.0;
		const double ExpectedArea = FMath::Abs(ExpectedX * ExpectedY);
		if (Card->GetStringField(TEXT("material")) != TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokVillagePresentationDay.M_RokVillagePresentationDay")
			|| !Card->GetBoolField(TEXT("Sprite_raw")) || !Card->GetBoolField(TEXT("Mask_raw"))
			|| !Card->GetBoolField(TEXT("source_colors_match")) || (bDetail && ExpectedArea <= 0)
			|| !Card->GetBoolField(TEXT("source_tags_match")) || !Card->GetBoolField(TEXT("registered"))
			|| !Card->GetBoolField(TEXT("attached_to_village")) || Card->GetBoolField(TEXT("parent_visible")) == bHidden
			|| Card->GetStringField(TEXT("mesh")) != TEXT("/Engine/BasicShapes/Plane.Plane")
			|| Card->GetBoolField(TEXT("visible")) != (bDetail && Active.Contains(Id))
			|| Card->GetBoolField(TEXT("local_visible")) != (bDetail && Active.Contains(Id))
			|| Card->GetBoolField(TEXT("hidden_in_game")) == (bDetail && Active.Contains(Id))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("plane_area")), ExpectedArea, FMath::Max(0.0001, ExpectedArea * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_x")), ExpectedX, FMath::Max(0.0001, FMath::Abs(ExpectedX) * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_y")), ExpectedY, FMath::Max(0.0001, FMath::Abs(ExpectedY) * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_z")), 1.0, 0.0001))
		{ Finish(FString::Printf(TEXT("Village mesh source/visibility/raw texture/material/scale mismatch: %s, sample %d."), *Id, SampleIndex)); return false; }
	}
	return true;
}
void URokVillagePresentationAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.0f); ++Frame;
	if (Elapsed > 60) { Finish(TEXT("Village runtime audit timed out.")); return; }
	if (SampleIndex < 0) { if (Elapsed > 3) Begin(); return; }
	if (Pending.IsValid()) { if (Elapsed - StageStarted > 10) Finish(TEXT("Village screenshot processing timed out.")); return; }
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot request is active.")); return; }
	if (Elapsed - StageStarted < 0.5 || !Validate()) return;
	PendingPath = Directory / FString::Printf(TEXT("%02d_%s_dxf_%.3f.png"), SampleIndex, SampleIndex < Samples.Num() ? TEXT("native") : TEXT("integration"), Camera->GetCurrentDxf());
	Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("path"), PendingPath);
	Pending->SetObjectField(TEXT("state_at_request"), State());
	StageStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokVillagePresentationAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !Validate()) return;
	TArray<uint8> File;
	const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(File, *PendingPath) || File.Num() < 24 || FMemory::Memcmp(File.GetData(), Signature, 8) != 0)
	{ Finish(TEXT("Village screenshot callback had no valid PNG file.")); return; }
	const auto ReadBigEndian = [&File](int32 Offset) { return (uint32(File[Offset]) << 24) | (uint32(File[Offset + 1]) << 16) | (uint32(File[Offset + 2]) << 8) | uint32(File[Offset + 3]); };
	const uint32 Width = ReadBigEndian(16), Height = ReadBigEndian(20);
	if (Width < 2 || Height < 2) { Finish(TEXT("Village screenshot dimensions are invalid.")); return; }
	Pending->SetNumberField(TEXT("bytes"), File.Num());
	Pending->SetNumberField(TEXT("width"), Width); Pending->SetNumberField(TEXT("height"), Height);
	Pending->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
	if (!Save(false)) { Finish(TEXT("Village audit manifest write failed.")); return; }
	NextSample();
}
bool URokVillagePresentationAuditSubsystem::Save(bool Complete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), Complete); Root->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 14);
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetNumberField(TEXT("audit_version"), 2); Root->SetNumberField(TEXT("source_id"), 639);
	Root->SetNumberField(TEXT("expected_capture_count"), 14);
	Root->SetNumberField(TEXT("expected_native_capture_count"), 11);
	Root->SetNumberField(TEXT("expected_integration_capture_count"), 3);
	Root->SetStringField(TEXT("village_zoom_boundary"), TEXT("At dxf3000 the native main scale is 3.299999237060547 with one active positive-area layer; no resource zero-area rule applies."));
	Root->SetArrayField(TEXT("native_samples"), Samples);
	Root->SetArrayField(TEXT("lod_integration_samples"), IntegrationSamples);
	Root->SetStringField(TEXT("lod_integration_source"), TEXT("unity_world_entity_population.json lod_contract cross-checked with loaded record; Far midpoint, Hidden half-band beyond far maximum, then original native480 expected state. Far/Hidden are integration checks, not new native Unity captures."));
	Root->SetStringField(TEXT("native_probe"), TEXT("Saved/RokUnityMigration/unity_village_presentation_native_probe.json"));
	Root->SetStringField(TEXT("source_geometry"), TEXT("Saved/RokUnityMigration/unity_village_presentation.json"));
	Root->SetStringField(TEXT("identity_contract"), TEXT("Thirteen village layers are keyed by string Unity Transform fileID; SpriteRenderer IDs and native mappings are verified. All three native runs must agree."));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetStringField(TEXT("scope"), TEXT("Actual camera API and runtime ticks replay native Unity zoom sequence; saved screenshots require visual review. Native equal-priority order is captured, not a cross-context guarantee. Gamma blending/night/server exploration and discovery remain outside this check."));
	Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))
		&& FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokVillagePresentationAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	else if (Captures.Num() != 14) Failures.Add(TEXT("Expected eleven native and three integration processed village screenshots."));
	bFinished = true;
	if (!Save(true)) Failures.Add(TEXT("Final village manifest write failed."));
	UE_LOG(LogTemp, Display, TEXT("Rok VillagePresentation audit passed=%d output=%s failure=%s"), Failures.IsEmpty(), *Directory, *FString::Join(Failures, TEXT("; ")));
	FPlatformMisc::RequestExit(false);
}
