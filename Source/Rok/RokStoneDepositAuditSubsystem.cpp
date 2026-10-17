#include "RokStoneDepositAuditSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
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
#include "RokStoneDepositComponent.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
const FVector2D StoneAuditFocus(1149.1666667, 159.1666667);

TSharedPtr<FJsonObject> StoneAuditObject(const TSharedPtr<FJsonValue>& Value)
{
	return Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
}

bool ReadStoneIds(const TSharedPtr<FJsonObject>& Sample, const TCHAR* Field, TArray<FString>& Items)
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

bool StoneParentChainVisible(const USceneComponent* Component)
{
	for (const USceneComponent* Parent = Component; Parent; Parent = Parent->GetAttachParent())
		if (!Parent->IsVisible() || Parent->bHiddenInGame || !Parent->IsRegistered()
			|| (Parent->GetOwner() && Parent->GetOwner()->IsHidden())) return false;
	return Component != nullptr;
}
}

bool URokStoneDepositAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokStoneDepositAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokStoneDepositAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokStoneDepositAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/StoneDepositAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokStoneDepositAuditSubsystem::ScreenshotProcessed);
}
void URokStoneDepositAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokStoneDepositAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokStoneDepositAuditSubsystem, STATGROUP_Tickables); }
bool URokStoneDepositAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype"))
	{ Finish(TEXT("Wrong project/map; camera unchanged.")); return false; }
	return true;
}

bool URokStoneDepositAuditSubsystem::Begin()
{
	if (!CheckIdentity()) return false;
	for (const TCHAR* Other : {TEXT("RokVillagePresentationAudit"), TEXT("RokWoodDepositAudit"), TEXT("RokFoodDepositAudit"), TEXT("RokTrainQuantityAudit"), TEXT("RokLeaderboardAudit"), TEXT("RokWorldEntityAnimationAudit"), TEXT("RokUiAnimationAudit"), TEXT("RokTrainTierAudit"), TEXT("RokPlayerHeadAudit")})
		if (FParse::Param(FCommandLine::Get(), Other)) { Finish(TEXT("Run stone audit without another automatic UI/camera audit.")); return false; }
	int32 CameraCount = 0, MatchCount = 0;
	for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++CameraCount; }
	for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld()); It; ++It)
		if (It->GetEntityKind() == TEXT("stone") && It->GetSourceId() == 14928) { Entity = *It; ++MatchCount; }
	if (CameraCount != 1 || MatchCount != 1) { Finish(TEXT("Expected exactly one runtime camera and source stone 14928.")); return false; }
	Stone = Entity->FindComponentByClass<URokStoneDepositComponent>();
	if (!Stone.IsValid()) { Finish(TEXT("Runtime stone component missing.")); return false; }
	const auto* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Player->GetPawn() != Camera.Get() || Player->GetViewTarget() != Camera.Get()
		|| !Entity->GetUnityPosition().Equals(FVector2D(6895, 955), 0.001))
	{ Finish(TEXT("Stone source actor or possessed camera identity differs; camera unchanged.")); return false; }
	if (!Source.Load() || Source.Items.Num() != 18 || !FMath::IsFinite(Source.UnitsToCentimeters) || Source.UnitsToCentimeters <= 0)
	{ Finish(TEXT("Stone source geometry cannot be loaded.")); return false; }
	for (const auto& Item : Source.Items)
		if (Item.SourceName != TEXT("stone_1") || Item.SourceTransformId.IsEmpty() || Item.SourceRendererId.IsEmpty()
			|| !FMath::IsFinite(Item.SpriteSize.X) || !FMath::IsFinite(Item.SpriteSize.Y) || Item.SpriteSize.X <= 0 || Item.SpriteSize.Y <= 0)
		{ Finish(TEXT("Stone geometry must retain eighteen duplicate names and unique source identities.")); return false; }
	if (!LoadNativeSamples()) return false;
	Camera->FocusDisplayCoordinate(StoneAuditFocus);
	NextSample(); return true;
}

bool URokStoneDepositAuditSubsystem::LoadNativeSamples()
{
	FString Json;
	TSharedPtr<FJsonObject> Native;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_stone_deposit_native_probe.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Native) || !Native.IsValid())
	{ Finish(TEXT("Native Unity probe cannot be read.")); return false; }
	const TArray<TSharedPtr<FJsonValue>>* Runs = nullptr;
	if (!Native->TryGetArrayField(TEXT("runs"), Runs) || !Runs || Runs->Num() != 3)
	{ Finish(TEXT("Expected the three captured Unity instances.")); return false; }
	TArray<FString> SourceOrder;
	for (const auto& Item : Source.Items) SourceOrder.Add(Item.SourceTransformId);
	const TArray<TSharedPtr<FJsonValue>>* Mappings = nullptr;
	if (!Native->TryGetArrayField(TEXT("mappings"), Mappings) || !Mappings || Mappings->Num() != 18)
	{ Finish(TEXT("Expected eighteen native Transform/SpriteRenderer identity mappings.")); return false; }
	TSet<FString> Mapped;
	for (const auto& Value : *Mappings)
	{
		const auto Mapping = StoneAuditObject(Value);
		FString Id, Name, Renderer; int32 Sibling = -1;
		if (!Mapping || !Mapping->HasTypedField<EJson::String>(TEXT("sourceTransformId"))
			|| !Mapping->HasTypedField<EJson::String>(TEXT("sourceRendererId"))
			|| !Mapping->TryGetStringField(TEXT("sourceTransformId"), Id) || !Mapping->TryGetStringField(TEXT("sourceName"), Name)
			|| !Mapping->TryGetStringField(TEXT("sourceRendererId"), Renderer) || !Mapping->TryGetNumberField(TEXT("siblingIndex"), Sibling)
			|| Mapped.Contains(Id))
		{ Finish(TEXT("Invalid native stone identity mapping schema.")); return false; }
		const auto* Item = Source.Items.FindByPredicate([&Id](const auto& Candidate) { return Candidate.SourceTransformId == Id; });
		if (!Item || Name != Item->SourceName || Renderer != Item->SourceRendererId || Sibling != Item->SiblingIndex)
		{ Finish(TEXT("Native stone identities differ from source geometry.")); return false; }
		Mapped.Add(Id);
	}
	const double ExpectedDxf[] = {480, 594.4174194335938, 900, 1500, 2500, 2999, 3000, 1500, 900, 594.4174194335938, 480};
	for (int32 RunIndex = 0; RunIndex < Runs->Num(); ++RunIndex)
	{
		const auto Run = StoneAuditObject((*Runs)[RunIndex]);
		const TArray<TSharedPtr<FJsonValue>>* NativeSamples = nullptr;
		double Instance = -1;
		if (!Run || !Run->TryGetNumberField(TEXT("instance"), Instance) || Instance != RunIndex
			|| !Run->TryGetArrayField(TEXT("samples"), NativeSamples) || !NativeSamples || NativeSamples->Num() != 11)
		{ Finish(TEXT("Native stone run must be an object containing eleven samples.")); return false; }
		for (int32 Index = 0; Index < NativeSamples->Num(); ++Index)
		{
			const auto Sample = StoneAuditObject((*NativeSamples)[Index]);
			TArray<FString> Active, Order;
			double Dxf = 0, Scale = 0;
			if (!ReadStoneIds(Sample, TEXT("active"), Active) || !ReadStoneIds(Sample, TEXT("order"), Order)
				|| Order != SourceOrder || !Sample->TryGetNumberField(TEXT("dxf"), Dxf) || !Sample->TryGetNumberField(TEXT("scale"), Scale)
				|| !FMath::IsFinite(Dxf) || !FMath::IsFinite(Scale) || Scale < 0
				|| !FMath::IsNearlyEqual(Dxf, ExpectedDxf[Index], 0.0001))
			{ Finish(TEXT("Invalid native stone sample schema, source IDs or zoom sequence.")); return false; }
			for (const auto& Id : Active) if (!Mapped.Contains(Id))
			{ Finish(TEXT("Native stone active array contains an unknown source ID.")); return false; }
			if (RunIndex > 0)
			{
				const auto First = Samples[Index]->AsObject(); TArray<FString> FirstActive;
				if (!ReadStoneIds(First, TEXT("active"), FirstActive) || Active != FirstActive
					|| Scale != First->GetNumberField(TEXT("scale")) || Dxf != First->GetNumberField(TEXT("dxf")))
				{ Finish(TEXT("The three native stone runs disagree; no single replay can represent them.")); return false; }
			}
		}
		if (RunIndex == 0) Samples = *NativeSamples;
	}
	return true;
}
void URokStoneDepositAuditSubsystem::NextSample()
{
	if (++SampleIndex == Samples.Num()) { Finish(); return; }
	Camera->SetCurrentDxf(Samples[SampleIndex]->AsObject()->GetNumberField(TEXT("dxf")));
	StageStarted = Elapsed;
}
TSharedRef<FJsonObject> URokStoneDepositAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("sample_index"), SampleIndex);
	Result->SetNumberField(TEXT("frame"), double(Frame));
	Result->SetNumberField(TEXT("time_seconds"), Elapsed);
	Result->SetNumberField(TEXT("source_id"), Entity->GetSourceId());
	Result->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf());
	Result->SetNumberField(TEXT("source_scale"), Stone->GetSourceScale());
	Result->SetStringField(TEXT("camera_focus"), Camera->GetViewCenterDisplay().ToString());
	TArray<TSharedPtr<FJsonValue>> Active;
	for (const auto& Id : Stone->GetActiveSourceItems()) Active.Add(MakeShared<FJsonValueString>(Id));
	Result->SetArrayField(TEXT("active"), Active);
	TArray<UStaticMeshComponent*> Meshes; Entity->GetComponents(Meshes);
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (auto* Mesh : Meshes)
	{
		if (!Mesh->GetName().StartsWith(TEXT("Stone_"))) continue;
		const FString Id = Mesh->GetName().RightChop(6);
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
		Row->SetBoolField(TEXT("attached_to_stone"), Mesh->GetAttachParent() == Stone.Get());
		Row->SetBoolField(TEXT("parent_visible"), StoneParentChainVisible(Mesh->GetAttachParent()));
		Row->SetStringField(TEXT("mesh"), Mesh->GetStaticMesh() ? Mesh->GetStaticMesh()->GetPathName() : TEXT("<missing>"));
		Row->SetBoolField(TEXT("visible"), Mesh->IsVisible() && !Mesh->bHiddenInGame);
		Row->SetNumberField(TEXT("plane_area"), FMath::Abs(Mesh->GetRelativeScale3D().X * Mesh->GetRelativeScale3D().Y));
		Row->SetNumberField(TEXT("scale_x"), Mesh->GetRelativeScale3D().X);
		Row->SetNumberField(TEXT("scale_y"), Mesh->GetRelativeScale3D().Y);
		Row->SetNumberField(TEXT("scale_z"), Mesh->GetRelativeScale3D().Z);
		Row->SetStringField(TEXT("location"), Mesh->GetRelativeLocation().ToString());
		Row->SetStringField(TEXT("material"), TEXT("<missing>"));
		Row->SetBoolField(TEXT("Sprite_raw"), false); Row->SetBoolField(TEXT("Mask_raw"), false);
		if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
		{
			Row->SetStringField(TEXT("material"), Material->Parent ? Material->Parent->GetPathName() : TEXT("missing"));
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
bool URokStoneDepositAuditSubsystem::Validate()
{
	if (!CheckIdentity()) return false;
	if (!Entity.IsValid() || !Camera.IsValid() || !Stone.IsValid()) { Finish(TEXT("Runtime stone objects were lost.")); return false; }
	if (Entity->IsHidden()) { Finish(TEXT("Stone source actor is hidden in game.")); return false; }
	if (!StoneParentChainVisible(Stone.Get())) { Finish(TEXT("Stone source parent component is hidden or unregistered.")); return false; }
	const auto* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Player->GetPawn() != Camera.Get() || Player->GetViewTarget() != Camera.Get()
		|| !Camera->GetViewCenterDisplay().Equals(StoneAuditFocus, 0.001))
	{ Finish(TEXT("Stone screenshot camera is not the possessed view target at the source focus.")); return false; }
	const auto Expected = Samples[SampleIndex]->AsObject();
	TArray<FString> Active;
	if (!ReadStoneIds(Expected, TEXT("active"), Active)) { Finish(TEXT("Native stone active item array is invalid.")); return false; }
	if (!FMath::IsNearlyEqual(Camera->GetCurrentDxf(), float(Expected->GetNumberField(TEXT("dxf"))), 0.001f)
		|| !FMath::IsNearlyEqual(Stone->GetSourceScale(), float(Expected->GetNumberField(TEXT("scale"))), 0.0001f)
		|| Stone->GetActiveSourceItems() != Active)
	{ Finish(FString::Printf(TEXT("Stone source sequence mismatch at sample %d."), SampleIndex)); return false; }
	const auto Snapshot = State();
	const auto& Cards = Snapshot->GetArrayField(TEXT("cards"));
	if (Cards.Num() != 18) { Finish(TEXT("Expected eighteen real source mesh components.")); return false; }
	TSet<FString> Seen;
	for (const auto& Value : Cards)
	{
		const auto Card = Value->AsObject();
		const FString Id = Card->GetStringField(TEXT("source_transform_id"));
		const auto* Item = Source.Items.FindByPredicate([&Id](const auto& Candidate) { return Candidate.SourceTransformId == Id; });
		if (!Item || Seen.Contains(Id)) { Finish(TEXT("Missing, duplicate or invented stone Transform ID in runtime meshes.")); return false; }
		Seen.Add(Id);
		const double Scale = Expected->GetNumberField(TEXT("scale"));
		const double ExpectedX = Item->SpriteSize.X * Scale * Source.UnitsToCentimeters / 100.0;
		const double ExpectedY = -Item->SpriteSize.Y * Scale * Source.UnitsToCentimeters / 100.0;
		const double ExpectedArea = FMath::Abs(ExpectedX * ExpectedY);
		if (Card->GetStringField(TEXT("material")) != TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokStoneDepositDay.M_RokStoneDepositDay")
			|| !Card->GetBoolField(TEXT("Sprite_raw")) || !Card->GetBoolField(TEXT("Mask_raw"))
			|| !Card->GetBoolField(TEXT("source_tags_match")) || !Card->GetBoolField(TEXT("registered"))
			|| !Card->GetBoolField(TEXT("attached_to_stone")) || !Card->GetBoolField(TEXT("parent_visible"))
			|| Card->GetStringField(TEXT("mesh")) != TEXT("/Engine/BasicShapes/Plane.Plane")
			|| Card->GetBoolField(TEXT("visible")) != Active.Contains(Id)
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("plane_area")), ExpectedArea, FMath::Max(0.0001, ExpectedArea * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_x")), ExpectedX, FMath::Max(0.0001, FMath::Abs(ExpectedX) * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_y")), ExpectedY, FMath::Max(0.0001, FMath::Abs(ExpectedY) * 0.0001))
			|| !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("scale_z")), 1.0, 0.0001))
		{ Finish(FString::Printf(TEXT("Stone mesh source/visibility/raw texture/material/scale mismatch: %s, sample %d."), *Id, SampleIndex)); return false; }
	}
	return true;
}
void URokStoneDepositAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.0f); ++Frame;
	if (Elapsed > 60) { Finish(TEXT("Stone runtime audit timed out.")); return; }
	if (SampleIndex < 0) { if (Elapsed > 3) Begin(); return; }
	if (Pending.IsValid()) { if (Elapsed - StageStarted > 10) Finish(TEXT("Stone screenshot processing timed out.")); return; }
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot request is active.")); return; }
	if (Elapsed - StageStarted < 0.5 || !Validate()) return;
	PendingPath = Directory / FString::Printf(TEXT("%02d_dxf_%.3f.png"), SampleIndex, Camera->GetCurrentDxf());
	Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("path"), PendingPath);
	Pending->SetObjectField(TEXT("state_at_request"), State());
	StageStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokStoneDepositAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !Validate()) return;
	TArray<uint8> File;
	const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(File, *PendingPath) || File.Num() < 24 || FMemory::Memcmp(File.GetData(), Signature, 8) != 0)
	{ Finish(TEXT("Stone screenshot callback had no valid PNG file.")); return; }
	const auto ReadBigEndian = [&File](int32 Offset) { return (uint32(File[Offset]) << 24) | (uint32(File[Offset + 1]) << 16) | (uint32(File[Offset + 2]) << 8) | uint32(File[Offset + 3]); };
	const uint32 Width = ReadBigEndian(16), Height = ReadBigEndian(20);
	if (Width < 2 || Height < 2) { Finish(TEXT("Stone screenshot dimensions are invalid.")); return; }
	Pending->SetNumberField(TEXT("bytes"), File.Num());
	Pending->SetNumberField(TEXT("width"), Width); Pending->SetNumberField(TEXT("height"), Height);
	Pending->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
	if (!Save(false)) { Finish(TEXT("Stone audit manifest write failed.")); return; }
	NextSample();
}
bool URokStoneDepositAuditSubsystem::Save(bool Complete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), Complete); Root->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 11);
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetNumberField(TEXT("audit_version"), 1); Root->SetNumberField(TEXT("source_id"), 14928);
	Root->SetNumberField(TEXT("expected_capture_count"), 11);
	Root->SetStringField(TEXT("native_probe"), TEXT("Saved/RokUnityMigration/unity_stone_deposit_native_probe.json"));
	Root->SetStringField(TEXT("source_geometry"), TEXT("Saved/RokUnityMigration/unity_stone_deposit.json"));
	Root->SetStringField(TEXT("identity_contract"), TEXT("Eighteen stone_1 layers are keyed by string Unity Transform fileID; SpriteRenderer IDs and native mappings are verified. All three native runs must agree."));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetStringField(TEXT("scope"), TEXT("Actual camera API and runtime ticks replay native Unity zoom sequence; saved screenshots require visual review. Native equal-priority order is captured, not a cross-context guarantee. Gamma blending/night/server gathering remain outside this check."));
	Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))
		&& FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokStoneDepositAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	else if (Captures.Num() != 11) Failures.Add(TEXT("Expected eleven processed stone screenshots."));
	bFinished = true;
	if (!Save(true)) Failures.Add(TEXT("Final stone manifest write failed."));
	UE_LOG(LogTemp, Display, TEXT("Rok StoneDeposit audit passed=%d output=%s failure=%s"), Failures.IsEmpty(), *Directory, *FString::Join(Failures, TEXT("; ")));
	FPlatformMisc::RequestExit(false);
}
