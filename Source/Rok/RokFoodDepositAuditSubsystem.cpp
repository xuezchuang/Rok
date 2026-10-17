#include "RokFoodDepositAuditSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
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
#include "RokFoodDepositComponent.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapEntityActor.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
bool ReadActiveItems(const TSharedPtr<FJsonObject>& Sample, TArray<FString>& Items)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Sample.IsValid() || !Sample->TryGetArrayField(TEXT("active"), Values) || Values->IsEmpty()) return false;
	for (const auto& Value : *Values)
	{
		FString Name;
		if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty() || Items.Contains(Name)) return false;
		Items.Add(Name);
	}
	return true;
}
}

bool URokFoodDepositAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokFoodDepositAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokFoodDepositAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokFoodDepositAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/FoodDepositAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokFoodDepositAuditSubsystem::ScreenshotProcessed);
}
void URokFoodDepositAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokFoodDepositAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokFoodDepositAuditSubsystem, STATGROUP_Tickables); }
bool URokFoodDepositAuditSubsystem::Begin()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityWorldMapPrototype"))
	{ Finish(TEXT("Wrong project/map; camera unchanged.")); return false; }
	for (const TCHAR* Other : {TEXT("RokVillagePresentationAudit"), TEXT("RokStoneDepositAudit"), TEXT("RokWoodDepositAudit"), TEXT("RokTrainQuantityAudit"), TEXT("RokLeaderboardAudit"), TEXT("RokWorldEntityAnimationAudit"), TEXT("RokUiAnimationAudit"), TEXT("RokTrainTierAudit"), TEXT("RokPlayerHeadAudit")})
		if (FParse::Param(FCommandLine::Get(), Other)) { Finish(TEXT("Run food audit without another automatic UI/camera audit.")); return false; }
	int32 CameraCount = 0, MatchCount = 0;
	for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It) { Camera = *It; ++CameraCount; }
	for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld()); It; ++It)
		if (It->GetEntityKind() == TEXT("food") && It->GetSourceId() == 14911) { Entity = *It; ++MatchCount; }
	if (CameraCount != 1 || MatchCount != 1) { Finish(TEXT("Expected exactly one runtime camera and source food 14911.")); return false; }
	Food = Entity->FindComponentByClass<URokFoodDepositComponent>();
	if (!Food.IsValid()) { Finish(TEXT("Runtime food component missing.")); return false; }
	FRokFoodDepositData Source;
	if (!Source.Load() || Source.Items.Num() != 9) { Finish(TEXT("Food source geometry cannot be loaded.")); return false; }
	for (const auto& Item : Source.Items)
		SourcePlaneAreas.Add(Item.Name, Item.SpriteSize.X * Item.SpriteSize.Y
			* FMath::Square(double(Source.UnitsToCentimeters) / 100.0));
	FString Json;
	TSharedPtr<FJsonObject> Native;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/unity_food_deposit_native_probe.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Native) || !Native.IsValid())
	{ Finish(TEXT("Native Unity probe cannot be read.")); return false; }
	const TArray<TSharedPtr<FJsonValue>>* Runs = nullptr;
	if (!Native->TryGetArrayField(TEXT("runs"), Runs) || Runs->Num() != 3)
	{ Finish(TEXT("Expected the three captured Unity instances.")); return false; }
	const auto Run = (*Runs)[0].IsValid() && (*Runs)[0]->Type == EJson::Object ? (*Runs)[0]->AsObject() : nullptr;
	const TArray<TSharedPtr<FJsonValue>>* NativeSamples = nullptr;
	if (!Run || !Run->TryGetArrayField(TEXT("samples"), NativeSamples) || NativeSamples->Num() != 11)
	{ Finish(TEXT("Expected eleven source zoom samples.")); return false; }
	Samples = *NativeSamples;
	for (const auto& Value : Samples)
	{
		TArray<FString> Active;
		const auto Sample = Value.IsValid() && Value->Type == EJson::Object ? Value->AsObject() : nullptr;
		double Dxf = 0, Scale = 0;
		if (!ReadActiveItems(Sample, Active) || !Sample->TryGetNumberField(TEXT("dxf"), Dxf)
			|| !Sample->TryGetNumberField(TEXT("scale"), Scale) || !FMath::IsFinite(Dxf) || !FMath::IsFinite(Scale))
		{ Finish(TEXT("Invalid native food sample schema.")); return false; }
	}
	Camera->FocusDisplayCoordinate(FVector2D(1141.1666667, 155.6666667));
	NextSample(); return true;
}
void URokFoodDepositAuditSubsystem::NextSample()
{
	if (++SampleIndex == Samples.Num()) { Finish(); return; }
	Camera->SetCurrentDxf(Samples[SampleIndex]->AsObject()->GetNumberField(TEXT("dxf")));
	StageStarted = Elapsed;
}
TSharedRef<FJsonObject> URokFoodDepositAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("dxf"), Camera->GetCurrentDxf());
	Result->SetNumberField(TEXT("source_scale"), Food->GetSourceScale());
	Result->SetStringField(TEXT("camera_focus"), Camera->GetViewCenterDisplay().ToString());
	Result->SetStringField(TEXT("active"), FString::Join(Food->GetActiveSourceItems(), TEXT(" ")));
	TArray<UStaticMeshComponent*> Meshes; Entity->GetComponents(Meshes);
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (auto* Mesh : Meshes)
	{
		if (!Mesh->GetName().StartsWith(TEXT("Food_farm_"))) continue;
		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("name"), Mesh->GetName());
		Row->SetBoolField(TEXT("visible"), Mesh->IsVisible() && !Mesh->bHiddenInGame);
		Row->SetNumberField(TEXT("plane_area"), FMath::Abs(Mesh->GetRelativeScale3D().X * Mesh->GetRelativeScale3D().Y));
		Row->SetStringField(TEXT("location"), Mesh->GetRelativeLocation().ToString());
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
bool URokFoodDepositAuditSubsystem::Validate()
{
	if (!Entity.IsValid() || !Camera.IsValid() || !Food.IsValid()) { Finish(TEXT("Runtime food objects were lost.")); return false; }
	if (Entity->IsHidden()) { Finish(TEXT("Food source actor is hidden in game.")); return false; }
	for (const USceneComponent* Parent = Food.Get(); Parent; Parent = Parent->GetAttachParent())
		if (!Parent->IsVisible() || Parent->bHiddenInGame) { Finish(TEXT("Food source parent component is hidden.")); return false; }
	const auto* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || Player->GetPawn() != Camera.Get() || Player->GetViewTarget() != Camera.Get()
		|| !Camera->GetViewCenterDisplay().Equals(FVector2D(1141.1666667, 155.6666667), 0.001))
	{ Finish(TEXT("Food screenshot camera is not the possessed view target at the source focus.")); return false; }
	const auto Expected = Samples[SampleIndex]->AsObject();
	TArray<FString> Active;
	if (!ReadActiveItems(Expected, Active)) { Finish(TEXT("Native food active item array is invalid.")); return false; }
	if (!FMath::IsNearlyEqual(Camera->GetCurrentDxf(), float(Expected->GetNumberField(TEXT("dxf"))), 0.001f)
		|| !FMath::IsNearlyEqual(Food->GetSourceScale(), float(Expected->GetNumberField(TEXT("scale"))), 0.0001f)
		|| Food->GetActiveSourceItems() != Active)
	{ Finish(FString::Printf(TEXT("Food source sequence mismatch at sample %d."), SampleIndex)); return false; }
	const auto Snapshot = State();
	const auto& Cards = Snapshot->GetArrayField(TEXT("cards"));
	if (Cards.Num() != 9) { Finish(TEXT("Expected nine real source mesh components.")); return false; }
	for (const auto& Value : Cards)
	{
		const auto Card = Value->AsObject();
		const FString Name = Card->GetStringField(TEXT("name")).RightChop(5);
		const double* BaseArea = SourcePlaneAreas.Find(Name);
		const double ExpectedArea = BaseArea ? *BaseArea * FMath::Square(Expected->GetNumberField(TEXT("scale"))) : -1;
		if (Card->GetStringField(TEXT("material")) != TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_RokFoodDepositDay.M_RokFoodDepositDay")
			|| !Card->GetBoolField(TEXT("Sprite_raw")) || !Card->GetBoolField(TEXT("Mask_raw"))
			|| Card->GetBoolField(TEXT("visible")) != Active.Contains(Name)
			|| !BaseArea || !FMath::IsNearlyEqual(Card->GetNumberField(TEXT("plane_area")), ExpectedArea, FMath::Max(0.0001, ExpectedArea * 0.0001)))
		{ Finish(TEXT("Food mesh visibility, raw texture, material, or area mismatch.")); return false; }
	}
	return true;
}
void URokFoodDepositAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.0f);
	if (Elapsed > 60) { Finish(TEXT("Food runtime audit timed out.")); return; }
	if (SampleIndex < 0) { if (Elapsed > 3) Begin(); return; }
	if (Pending.IsValid()) { if (Elapsed - StageStarted > 10) Finish(TEXT("Food screenshot processing timed out.")); return; }
	if (FScreenshotRequest::IsScreenshotRequested()) return;
	if (Elapsed - StageStarted < 0.5 || !Validate()) return;
	PendingPath = Directory / FString::Printf(TEXT("%02d_dxf_%.3f.png"), SampleIndex, Camera->GetCurrentDxf());
	Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("path"), PendingPath);
	Pending->SetObjectField(TEXT("state_at_request"), State());
	StageStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokFoodDepositAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || !Validate()) return;
	const int64 Bytes = IFileManager::Get().FileSize(*PendingPath);
	if (Bytes <= 0) { Finish(TEXT("Food screenshot callback had no nonempty file.")); return; }
	Pending->SetNumberField(TEXT("bytes"), double(Bytes));
	Pending->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
	if (!Save(false)) { Finish(TEXT("Food audit manifest write failed.")); return; }
	NextSample();
}
bool URokFoodDepositAuditSubsystem::Save(bool Complete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), Complete); Root->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 11);
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetStringField(TEXT("scope"), TEXT("Actual camera API and runtime ticks replay native Unity zoom sequence; saved screenshots require visual review. Native equal-priority order is captured, not a cross-context guarantee. Gamma blending/night/server gathering remain outside this check."));
	Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))
		&& FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")));
}
void URokFoodDepositAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	else if (Captures.Num() != 11) Failures.Add(TEXT("Expected eleven processed food screenshots."));
	bFinished = true;
	if (!Save(true)) Failures.Add(TEXT("Final food manifest write failed."));
	UE_LOG(LogTemp, Display, TEXT("Rok FoodDeposit audit passed=%d output=%s failure=%s"), Failures.IsEmpty(), *Directory, *FString::Join(Failures, TEXT("; ")));
	FGenericPlatformMisc::RequestExit(false);
}
