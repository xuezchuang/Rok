#include "RokCityGammaCompositeAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokCitySpriteActor.h"
#include "RokTrainArmyMeshData.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/BlendableInterface.h"
#include "Engine/Texture2D.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StaticMeshResources.h"

namespace
{
constexpr int32 Width = 1280;
constexpr int32 Height = 720;
constexpr double VerticalFov = 7.5;
constexpr double DistanceCm = 10000.0;
constexpr TCHAR TempTag[] = TEXT("RokCityGammaCompositeAuditTemporary");
constexpr TCHAR NativePath[] = TEXT("UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560/native_render.json");
constexpr TCHAR ContractPath[] = TEXT("UnityCityGammaCompositeProbe/probe_contract.json");
constexpr TCHAR VerifyPath[] = TEXT("UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560/native_verify.json");
constexpr TCHAR NativeExpectedSha[] = TEXT("e948d7706b8d6ea2166146247993bbacb6c4cafcc673dce8c1c0a03f9d19294c");
constexpr TCHAR ContractExpectedSha[] = TEXT("b8ca67cc92f6499debc4ea6649c4b2444ca09da603c524eaf9649be89fe74fa8");
constexpr TCHAR VerifyExpectedSha[] = TEXT("2a23340b402ec83637d82d2f86c4331dfb6026ffe4a7d68a7c0c355b1be36411");
constexpr TCHAR PatternExpectedSha[] = TEXT("c98ccb2de10f61fac604813eb09107a35351bc98d3cc5d13a8c62f6b5d066705");
constexpr TCHAR ResourceMeshVerifyExpectedSha[] = TEXT("eda0b19bf82964ec0d99b35e6a4c70b0bed93079761b02dcd2db78e4c995e7a4");
constexpr TCHAR FlatMaterialPath[] = TEXT("/Game/RokDiagnostics/Academy/M_AcademyAuditBackground.M_AcademyAuditBackground");
constexpr TCHAR PatternMaterialPath[] = TEXT("/Game/RokDiagnostics/CityGammaComposite/M_CityGammaAuditPattern.M_CityGammaAuditPattern");
constexpr TCHAR PatternTexturePath[] = TEXT("/Game/RokDiagnostics/CityGammaComposite/T_CityGammaPatternSource.T_CityGammaPatternSource");
constexpr TCHAR ExactOutputPath[] = TEXT("/Game/RokDiagnostics/CityGammaComposite/M_CityGammaExactOutput.M_CityGammaExactOutput");

struct FModelAssetPaths
{
	const TCHAR* Label;
	const TCHAR* Model;
	const TCHAR* Mesh;
	const TCHAR* Material;
};
const FModelAssetPaths ModelPaths[] = {
	{ TEXT("academy"), TEXT("build_13_101_2"),
		TEXT("/Game/RokUnity/City/Academy/build_13_101_2/SM_build_13_101_2_SourceSpriteMesh.SM_build_13_101_2_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/GammaComposite/build_13_101_2/M_build_13_101_2_SourceGammaBeforeDOF.M_build_13_101_2_SourceGammaBeforeDOF") },
	{ TEXT("farm"), TEXT("build_2_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/SM_build_2_101_1_SourceSpriteMesh.SM_build_2_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/GammaComposite/build_2_101_1/M_build_2_101_1_SourceGammaBeforeDOF.M_build_2_101_1_SourceGammaBeforeDOF") },
	{ TEXT("wood"), TEXT("build_3_101_1"),
		TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/SM_build_3_101_1_SourceSpriteMesh.SM_build_3_101_1_SourceSpriteMesh"),
		TEXT("/Game/RokUnity/City/GammaComposite/build_3_101_1/M_build_3_101_1_SourceGammaBeforeDOF.M_build_3_101_1_SourceGammaBeforeDOF") }
};

FString SavedPath(const TCHAR* Relative) { return FPaths::ProjectSavedDir() / Relative; }
FString AssetFile(const TCHAR* ObjectPath)
{
	FString PackagePath(ObjectPath);
	FString ObjectName;
	PackagePath.Split(TEXT("."), &PackagePath, &ObjectName);
	return FPaths::ProjectContentDir() / PackagePath.RightChop(6) + TEXT(".uasset");
}
FString ShaFile(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out)
{
	FString Text;
	return FFileHelper::LoadFileToString(Text, *Path) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
}
bool FinishAuditTexture(UTexture* Texture, const FString& Role, FString& Error)
{
	if (!Texture) { Error = Role + TEXT(" texture is missing"); return false; }
#if WITH_EDITOR
	if (Texture->IsCompiling())
	{
		if (!IsInGameThread()) { Error = Role + TEXT(" texture compilation requires the game thread"); return false; }
		UE_LOG(LogTemp, Display, TEXT("Gamma audit waiting for %s texture compilation: %s"), *Role, *Texture->GetPathName());
		UTexture* RequestedTextures[] = { Texture };
		FTextureCompilingManager::Get().FinishCompilation(RequestedTextures);
		if (Texture->IsCompiling()) { Error = Role + TEXT(" texture remains compiling: ") + Texture->GetPathName(); return false; }
	}
#endif
	if (!Texture->GetResource()) { Error = Role + TEXT(" texture has no GPU resource: ") + Texture->GetPathName(); return false; }
	return true;
}
bool FinishAuditMaterial(UMaterialInterface* Material, const FString& Role, FString& Error)
{
	if (!Material) { Error = Role + TEXT(" material is missing"); return false; }
	const bool bInitiallyComplete = Material->IsComplete();
	Material->EnsureIsComplete();
	const bool bComplete = Material->IsComplete();
	UE_LOG(LogTemp, Display, TEXT("Gamma audit material readiness %s: initial=%d after=%d asset=%s"),
		*Role, bInitiallyComplete, bComplete, *Material->GetPathName());
	if (!bComplete) { Error = Role + TEXT(" material shader map remains incomplete: ") + Material->GetPathName(); return false; }
	return true;
}
bool HasExactOutputBlendable(USceneCaptureComponent2D* Capture, UMaterialInstanceDynamic* Expected)
{
	if (!Capture || !Expected || !Capture->bAlwaysPersistRenderingState || Capture->bCaptureEveryFrame ||
		!FMath::IsNearlyEqual(Capture->PostProcessBlendWeight, 1.0f)) return false;
	const UMaterial* Base = Expected->GetMaterial();
	const auto& Blendables = Capture->PostProcessSettings.WeightedBlendables.Array;
	return Base && Base->GetPathName() == ExactOutputPath && Base->MaterialDomain == MD_PostProcess &&
		Expected->GetBlendableLocation(Base) == BL_ReplacingTonemapper &&
		Blendables.Num() == 1 && Blendables[0].Object.Get() == Expected &&
		FMath::IsNearlyEqual(Blendables[0].Weight, 1.0f) && Capture->GetViewState(0) != nullptr;
}
FVector ReadV3(const TSharedPtr<FJsonObject>& Object)
{
	return FVector(Object->GetNumberField(TEXT("x")), Object->GetNumberField(TEXT("y")),
		Object->GetNumberField(TEXT("z")));
}
FQuat ReadQ4(const TSharedPtr<FJsonObject>& Object)
{
	return FQuat(Object->GetNumberField(TEXT("x")), Object->GetNumberField(TEXT("y")),
		Object->GetNumberField(TEXT("z")), Object->GetNumberField(TEXT("w")));
}
FLinearColor ReadColor(const TSharedPtr<FJsonObject>& Object)
{
	return FLinearColor(Object->GetNumberField(TEXT("r")), Object->GetNumberField(TEXT("g")),
		Object->GetNumberField(TEXT("b")), Object->GetNumberField(TEXT("a")));
}
FVector UnityPositionCm(const FVector& P) { return FVector(-P.X, P.Z, P.Y) * 500.0; }
FQuat UnityRotation(const FQuat& Q) { return FQuat(-Q.X, Q.Z, Q.Y, Q.W); }
TArray<TSharedPtr<FJsonValue>> V3(const FVector& P)
{
	return { MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y), MakeShared<FJsonValueNumber>(P.Z) };
}
TArray<TSharedPtr<FJsonValue>> Q4(const FQuat& Q)
{
	return { MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y),
		MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W) };
}
TArray<TSharedPtr<FJsonValue>> C4(const FLinearColor& C)
{
	return { MakeShared<FJsonValueNumber>(C.R), MakeShared<FJsonValueNumber>(C.G),
		MakeShared<FJsonValueNumber>(C.B), MakeShared<FJsonValueNumber>(C.A) };
}
int32 CountActors(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It) ++Count;
	return Count;
}
int32 CountTemporary(UWorld* World)
{
	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It) if (It->ActorHasTag(TempTag)) ++Count;
	return Count;
}
FString Economy(UWorld* World)
{
	const URokBuildingUpgradeSubsystem* Upgrade = World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	if (!Upgrade) return FString();
	const FRokResourceStock Stock = Upgrade->GetCurrentResources();
	return FString::Printf(TEXT("%d,%d,%d,%d|local=%d|upgrade=%d|queue=%d"),
		Stock.Food, Stock.Wood, Stock.Stone, Stock.Gold,
		Upgrade->GetActiveLocalConstructionCount(), Upgrade->GetActiveUpgradeCount(), Upgrade->GetOwnedBuildQueueCount());
}
uint64 Difference(uint64 After, uint64 Before) { return After >= Before ? After - Before : MAX_uint64; }
}

bool URokCityGammaCompositeAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity") &&
		FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()), TEXT("D:/ueDemo/Rok/")) &&
		FParse::Param(FCommandLine::Get(), TEXT("RokCityGammaCompositeAudit")) &&
		!IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCityGammaCompositeAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokCityGammaCompositeAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RunDirectory = FPaths::ConvertRelativePathToFull(SavedPath(TEXT("RokUnityMigration/CityGammaCompositeAudit")) /
		FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
			FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*RunDirectory, true);
}
void URokCityGammaCompositeAuditSubsystem::Deinitialize()
{
	if (!bFinished) Finish(TEXT("City Gamma audit world deinitialized before eight frames completed"));
	else Restore();
	Super::Deinitialize();
}
TStatId URokCityGammaCompositeAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityGammaCompositeAuditSubsystem, STATGROUP_Tickables);
}

bool URokCityGammaCompositeAuditSubsystem::Begin()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !PC || !World->GetGameViewport() || !PC->GetViewTarget() ||
		!FRokCityGammaCompositor::IsAvailable())
	{ Finish(TEXT("GameApp/player/viewport or Gamma compositor unavailable")); return false; }
	PlayerController = PC;
	SavedViewTarget = PC->GetViewTarget();
	if (UCameraComponent* Existing = PC->GetViewTarget()->FindComponentByClass<UCameraComponent>())
	{ SavedCameraTransform = Existing->GetComponentTransform(); SavedCameraFov = Existing->FieldOfView; }
	ActorCountBefore = CountActors(World);
	TemporaryCountBefore = CountTemporary(World);
	EconomyBefore = Economy(World);
	if (EconomyBefore.IsEmpty() || TemporaryCountBefore != 0)
	{ Finish(TEXT("City economy snapshot unavailable or temporary Gamma actor already exists")); return false; }

	const FString NativeFile = SavedPath(NativePath), ContractFile = SavedPath(ContractPath);
	const FString VerifyFile = SavedPath(VerifyPath);
	NativeRenderSha = ShaFile(NativeFile);
	NativeContractSha = ShaFile(ContractFile);
	NativeVerifySha = ShaFile(VerifyFile);
	TSharedPtr<FJsonObject> Verified;
	if (NativeRenderSha != NativeExpectedSha || NativeContractSha != ContractExpectedSha ||
		NativeVerifySha != VerifyExpectedSha || !ReadJson(NativeFile, Native) || !ReadJson(ContractFile, Contract) ||
		!ReadJson(VerifyFile, Verified) || !Verified->GetBoolField(TEXT("passed")) ||
		Verified->GetStringField(TEXT("schema")) != TEXT("rok.city_gamma_composite_native_verify.v1") ||
		Verified->GetStringField(TEXT("native_render_sha256")) != NativeRenderSha ||
		Verified->GetStringField(TEXT("contract_sha256")) != NativeContractSha ||
		Native->GetStringField(TEXT("schema")) != TEXT("rok.city_gamma_composite_native_render.v1") ||
		Contract->GetStringField(TEXT("schema")) != TEXT("rok.city_gamma_composite_native_contract.v1") ||
		Native->GetArrayField(TEXT("frames")).Num() != 8 ||
		Native->GetArrayField(TEXT("models")).Num() != 3)
	{ Finish(TEXT("Pinned native Gamma 8-frame verified chain is missing, changed, or invalid")); return false; }
	const TSharedPtr<FJsonObject>* Sources = nullptr;
	if (!Contract->TryGetObjectField(TEXT("source_files"), Sources) || !Sources || !Sources->IsValid())
	{ Finish(TEXT("Native Gamma source-file hash list missing")); return false; }
	for (const auto& Pair : (*Sources)->Values)
	{
		const FString Relative(Pair.Key);
		const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
		if (!Relative.StartsWith(TEXT("ref/")) || Relative.Contains(TEXT("..")) || !Row ||
			ShaFile(FPaths::ProjectDir() / Relative) != Row->GetStringField(TEXT("sha256")) ||
			ShaFile(FPaths::ProjectDir() / (Relative + TEXT(".meta"))) != Row->GetStringField(TEXT("meta_sha256")) ||
			ShaFile(SavedPath(TEXT("UnityCityGammaCompositeProbe")) / Row->GetStringField(TEXT("probe_asset")))
				!= Row->GetStringField(TEXT("sha256")))
		{ Finish(FString::Printf(TEXT("Native Gamma source changed: %s"), *Relative)); return false; }
	}
	const TSharedPtr<FJsonObject> Catalogs = Contract->GetObjectField(TEXT("source_catalogs"));
	if (Catalogs->GetStringField(TEXT("academy")) !=
			ShaFile(SavedPath(TEXT("RokUnityMigration/AcademyVisualSource/catalog.json"))) ||
		Catalogs->GetStringField(TEXT("resources")) !=
			ShaFile(SavedPath(TEXT("RokUnityMigration/CityResourceBuildingVisualSources/catalog.json"))) ||
		Verified->GetStringField(TEXT("renderer_sha256")) !=
			ShaFile(FPaths::ProjectDir() / TEXT("Scripts/RenderRokCityGammaCompositeNative.cs")) ||
		Contract->GetStringField(TEXT("renderer_script_sha256")) != Verified->GetStringField(TEXT("renderer_sha256")))
	{ Finish(TEXT("Gamma native source catalogs or renderer changed")); return false; }
	const TSharedPtr<FJsonObject> SourceFrameHashes = Verified->GetObjectField(TEXT("frame_png_sha256"));
	for (const TSharedPtr<FJsonValue>& Value : Native->GetArrayField(TEXT("frames")))
	{
		const TSharedPtr<FJsonObject> Frame = Value->AsObject();
		if (!Frame || ShaFile(SavedPath(TEXT("UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560")) /
				Frame->GetStringField(TEXT("png"))) !=
			SourceFrameHashes->GetStringField(Frame->GetStringField(TEXT("name"))))
		{ Finish(TEXT("One pinned native Gamma PNG changed")); return false; }
	}
	const FString PatternFile = SavedPath(TEXT("UnityCityGammaCompositeProbe/OutputRuns/20261008_171930_737_10560/pattern_source.png"));
	PatternSha = ShaFile(PatternFile);
	if (PatternSha != PatternExpectedSha)
	{ Finish(TEXT("Source Gamma diagnostic pattern bytes changed")); return false; }
	const FString ResourceVerifyFile = SavedPath(TEXT("RokUnityMigration/CityResourcePresentationAssets/ue_assets_verify.json"));
	ResourceMeshVerifySha = ShaFile(ResourceVerifyFile);
	TSharedPtr<FJsonObject> ResourceVerify;
	if (ResourceMeshVerifySha != ResourceMeshVerifyExpectedSha || !ReadJson(ResourceVerifyFile, ResourceVerify) ||
		!ResourceVerify->GetBoolField(TEXT("passed")) ||
		ResourceVerify->GetStringField(TEXT("schema")) != TEXT("rok.city_resource_presentation_assets.ue_verify.v1") ||
		ResourceVerify->GetArrayField(TEXT("models")).Num() != 2)
	{ Finish(TEXT("Pinned source resource mesh import/zero-area verification changed")); return false; }

	UMaterialInterface* FlatBase = LoadObject<UMaterialInterface>(nullptr, FlatMaterialPath);
	UMaterialInterface* PatternBase = LoadObject<UMaterialInterface>(nullptr, PatternMaterialPath);
	UMaterialInterface* ExactOutputBase = LoadObject<UMaterialInterface>(nullptr, ExactOutputPath);
	UTexture2D* Pattern = LoadObject<UTexture2D>(nullptr, PatternTexturePath);
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FString TextureError;
	if (!FinishAuditTexture(Pattern, TEXT("source pattern"), TextureError))
	{ Finish(TextureError); return false; }
	if (!FlatBase || !PatternBase || !ExactOutputBase || !Pattern || !Plane || Pattern->GetSizeX() != Width ||
		Pattern->GetSizeY() != Height || Pattern->SRGB || Pattern->Filter != TF_Nearest || Pattern->GetNumMips() != 1)
	{ Finish(TEXT("Exact source Gamma opaque flat/pattern GPU background assets missing or importer differs")); return false; }
	FlatBackgroundMaterial.Reset(UMaterialInstanceDynamic::Create(FlatBase, this));
	PatternBackgroundMaterial.Reset(UMaterialInstanceDynamic::Create(PatternBase, this));
	ExactOutputMaterial.Reset(UMaterialInstanceDynamic::Create(ExactOutputBase, this));
	PatternTexture.Reset(Pattern);
	if (!FlatBackgroundMaterial.Get() || !PatternBackgroundMaterial.Get() || !ExactOutputMaterial.Get())
	{ Finish(TEXT("Gamma background material instances unavailable")); return false; }
	PatternBackgroundMaterial->SetTextureParameterValue(TEXT("SourcePattern"), Pattern);
	// UE can defer material shader permutations until first use. Complete both
	// parent and dynamic instance resources before any measured capture.
	for (const auto& Pair : {
		TPair<UMaterialInterface*, const TCHAR*>(FlatBase, TEXT("flat/base")),
		TPair<UMaterialInterface*, const TCHAR*>(FlatBackgroundMaterial.Get(), TEXT("flat/MID")),
		TPair<UMaterialInterface*, const TCHAR*>(PatternBase, TEXT("pattern/base")),
		TPair<UMaterialInterface*, const TCHAR*>(PatternBackgroundMaterial.Get(), TEXT("pattern/MID")),
		TPair<UMaterialInterface*, const TCHAR*>(ExactOutputBase, TEXT("output/base")),
		TPair<UMaterialInterface*, const TCHAR*>(ExactOutputMaterial.Get(), TEXT("output/MID")) })
	{
		if (!FinishAuditMaterial(Pair.Key, Pair.Value, TextureError))
		{ Finish(TextureError); return false; }
	}
	const auto& ContractModels = Contract->GetArrayField(TEXT("models"));
	const auto& NativeModels = Native->GetArrayField(TEXT("models"));
	const auto& ResourceModels = ResourceVerify->GetArrayField(TEXT("models"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const TSharedPtr<FJsonObject> Source = ContractModels[Index]->AsObject();
		const TSharedPtr<FJsonObject> Measured = NativeModels[Index]->AsObject();
		const FModelAssetPaths& Asset = ModelPaths[Index];
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Asset.Mesh);
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, Asset.Material);
		const int32 NativeVertices = Measured->GetObjectField(TEXT("geometry"))->GetArrayField(TEXT("vertices")).Num();
		const int32 NativeTriangles = Measured->GetObjectField(TEXT("geometry"))->GetArrayField(TEXT("triangles")).Num() / 3;
		int32 ExpectedVertices = NativeVertices, ExpectedTriangles = NativeTriangles;
		if (Index > 0)
		{
			const TSharedPtr<FJsonObject> VerifiedModel = ResourceModels[Index - 1]->AsObject();
			const TSharedPtr<FJsonObject> Imported = VerifiedModel->GetObjectField(TEXT("imported_mesh"));
			const FString ManifestFile = VerifiedModel->GetStringField(TEXT("source_mesh_manifest"));
			if (VerifiedModel->GetStringField(TEXT("model")) != Asset.Model ||
				!FPaths::IsUnderDirectory(ManifestFile, SavedPath(TEXT("RokUnityMigration/CityResourcePresentationAssets"))) ||
				ShaFile(ManifestFile) != VerifiedModel->GetStringField(TEXT("source_mesh_manifest_sha256")) ||
				Imported->GetIntegerField(TEXT("source_nonzero_faces_proved")) +
					Imported->GetIntegerField(TEXT("source_zero_area_omitted")) != NativeTriangles ||
				Imported->GetIntegerField(TEXT("uv_channel_count_lod0")) != 1)
			{ Finish(FString::Printf(TEXT("Verified resource source mesh chain changed: %s"), Asset.Label)); return false; }
			ExpectedVertices = Imported->GetIntegerField(TEXT("vertex_count_lod0"));
			ExpectedTriangles = Imported->GetIntegerField(TEXT("triangle_count_lod0"));
			if (ExpectedTriangles != Imported->GetIntegerField(TEXT("source_nonzero_faces_proved")) ||
				(Index == 1 && (NativeVertices != 139 || NativeTriangles != 177 ||
					ExpectedVertices != 133 || ExpectedTriangles != 163 ||
					Imported->GetIntegerField(TEXT("source_zero_area_omitted")) != 14)) ||
				(Index == 2 && (NativeVertices != 43 || NativeTriangles != 41 ||
					ExpectedVertices != 43 || ExpectedTriangles != 41)))
			{ Finish(TEXT("Resource mesh effective triangle contract differs")); return false; }
		}
		if (!Source || !Measured || Source->GetStringField(TEXT("label")) != Asset.Label ||
			Source->GetStringField(TEXT("model")) != Asset.Model ||
			Measured->GetStringField(TEXT("label")) != Asset.Label || !Mesh || !Base ||
			Mesh->GetNumVertices(0) != ExpectedVertices || Mesh->GetNumTriangles(0) != ExpectedTriangles)
		{ Finish(FString::Printf(TEXT("Source Gamma mesh/material/model identity failed: %s"), Asset.Label)); return false; }
		for (const TCHAR* Role : { TEXT("Sprite"), TEXT("Mask") })
		{
			UTexture* BoundTexture = nullptr;
			if (!Base->GetTextureParameterValue(FHashedMaterialParameterInfo(Role), BoundTexture) ||
				!FinishAuditTexture(BoundTexture, FString::Printf(TEXT("%s/%s"), Asset.Label, Role), TextureError))
			{ Finish(TextureError.IsEmpty() ? FString::Printf(TEXT("Source Gamma material texture parameter missing: %s/%s"), Asset.Label, Role) : TextureError); return false; }
			const UTexture2D* Bound2D = Cast<UTexture2D>(BoundTexture);
			const bool bMask = FCString::Strcmp(Role, TEXT("Mask")) == 0;
			const TSharedPtr<FJsonObject> NativeImport = Measured->GetObjectField(bMask ? TEXT("maskImport") : TEXT("spriteImport"));
			const int32 ExpectedWidth = NativeImport->GetIntegerField(TEXT("width"));
			const int32 ExpectedHeight = NativeImport->GetIntegerField(TEXT("height"));
			if (!Bound2D || Bound2D->GetSizeX() != ExpectedWidth || Bound2D->GetSizeY() != ExpectedHeight)
			{ Finish(FString::Printf(TEXT("Source Gamma %s/%s texture dimensions differ after compilation: expected %dx%d"),
				Asset.Label, Role, ExpectedWidth, ExpectedHeight)); return false; }
		}
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		if (!MID) { Finish(TEXT("Source Gamma material instance creation failed")); return false; }
		if (!FinishAuditMaterial(Base, FString::Printf(TEXT("%s/base"), Asset.Label), TextureError) ||
			!FinishAuditMaterial(MID, FString::Printf(TEXT("%s/MID"), Asset.Label), TextureError))
		{ Finish(TextureError); return false; }
		ModelMaterials.Emplace(MID);
		const TSharedPtr<FJsonObject> Root = Source->GetObjectField(TEXT("root"))->GetObjectField(TEXT("transform"));
		const TSharedPtr<FJsonObject> Child = Source->GetObjectField(TEXT("sprite_child"))->GetObjectField(TEXT("transform"));
		const TSharedPtr<FJsonObject> Offset = Measured->GetObjectField(TEXT("geometry"))->GetObjectField(TEXT("diagnosticOffset"));
		const FVector RootLocation = UnityPositionCm(ReadV3(Root->GetObjectField(TEXT("local_position"))) + ReadV3(Offset));
		const FQuat RootRotation = UnityRotation(ReadQ4(Root->GetObjectField(TEXT("local_rotation"))));
		const FVector RootScale = ReadV3(Root->GetObjectField(TEXT("local_scale")));
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARokCitySpriteActor* Actor = World->SpawnActor<ARokCitySpriteActor>(RootLocation, RootRotation.Rotator(), Params);
		if (!Actor) { Finish(TEXT("Temporary Gamma source actor spawn failed")); return false; }
		TemporaryActors.Add(Actor);
		Actor->Tags.Add(TempTag);
		Actor->SetActorScale3D(RootScale);
		const FVector ChildPosition = UnityPositionCm(ReadV3(Child->GetObjectField(TEXT("local_position"))));
		const FQuat ChildRotation = UnityRotation(ReadQ4(Child->GetObjectField(TEXT("local_rotation"))));
		const FVector ChildScale = ReadV3(Child->GetObjectField(TEXT("local_scale"))) * 5.0;
		Actor->ConfigureSourceMesh(Mesh, MID, FTransform(ChildRotation, ChildPosition, ChildScale), 0);
		UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
		if (!Component || Component->GetStaticMesh() != Mesh || Component->GetMaterial(0) != MID)
		{ Finish(TEXT("Temporary Gamma source component binding failed")); return false; }
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetVisibility(false);
	}

	ARokCitySpriteActor* Owner = TemporaryActors[0].Get();
	UStaticMeshComponent* Background = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
	if (!Owner || !Background) { Finish(TEXT("Source Gamma GPU background plane creation failed")); return false; }
	Owner->AddInstanceComponent(Background);
	Background->SetupAttachment(Owner->GetRootComponent());
	Background->SetStaticMesh(Plane);
	Background->SetMaterial(0, FlatBackgroundMaterial.Get());
	Background->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Background->SetCastShadow(false);
	Background->RegisterComponentWithWorld(World);
	BackgroundComponent = Background;

	UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
	RT->InitCustomFormat(Width, Height, PF_B8G8R8A8, true);
	RT->TargetGamma = 2.2f;
	RT->ClearColor = FLinearColor::Black;
	RT->UpdateResourceImmediate(true);
	RenderTarget.Reset(RT);
	USceneCaptureComponent2D* SC = NewObject<USceneCaptureComponent2D>(this, NAME_None, RF_Transient);
	SC->TextureTarget = RT;
	SC->CaptureSource = SCS_FinalColorLDR;
	SC->CompositeMode = SCCM_Overwrite;
	SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SC->ShowOnlyComponents.Add(Background);
	for (const auto& Actor : TemporaryActors) SC->ShowOnlyComponents.Add(Actor->GetSpriteMeshComponent());
	SC->bCaptureEveryFrame = false;
	SC->bCaptureOnMovement = false;
	// UE5.8 UMaterialInterface::OverrideBlendableSettings skips non-reflection
	// capture views without View.State. Persist it while keeping capture manual.
	SC->bAlwaysPersistRenderingState = true;
	SC->ProjectionType = ECameraProjectionMode::Perspective;
	SC->FOVAngle = FMath::RadiansToDegrees(2.0 * FMath::Atan(
		FMath::Tan(FMath::DegreesToRadians(VerticalFov) / 2.0) * Width / Height));
	const FVector Forward = FVector(0, .7, -.7).GetSafeNormal();
	SC->SetWorldLocation(-Forward * DistanceCm);
	SC->SetWorldRotation(Forward.Rotation());
	SC->RegisterComponentWithWorld(World);
	SC->ShowFlags.SetAntiAliasing(false);
	SC->ShowFlags.SetTemporalAA(false);
	SC->ShowFlags.SetEyeAdaptation(false);
	SC->ShowFlags.SetMotionBlur(false);
	SC->ShowFlags.SetPostProcessing(true);
	SC->ShowFlags.SetPostProcessMaterial(true);
	SC->ShowFlags.SetTonemapper(true);
	SC->ShowFlags.SetDepthOfField(false);
	SC->ShowFlags.SetFog(false);
	SC->ShowFlags.SetVolumetricFog(false);
	SC->ShowFlags.SetBloom(false);
	SC->ShowFlags.SetGrain(false);
	SC->ShowFlags.SetColorGrading(false);
	SC->ShowFlags.SetVignette(false);
	SC->ShowFlags.SetAtmosphere(false);
	SC->ShowFlags.SetDeferredAtmospherePass(false);
	SC->PostProcessBlendWeight = 1.0f;
	SC->PostProcessSettings.AddBlendable(ExactOutputMaterial.Get(), 1.0f);
	if (!HasExactOutputBlendable(SC, ExactOutputMaterial.Get()))
	{ Finish(TEXT("Exact Gamma replacement blendable lacks a persistent capture view state")); return false; }
	CaptureComponent.Reset(SC);
	const FVector Right = SC->GetRightVector(), Up = SC->GetUpVector();
	const FMatrix BackgroundBasis(Right, Up, Forward, FVector::ZeroVector);
	const double PlaneDistance = DistanceCm + 2000.0;
	const double VisibleHeightCm = 2.0 * PlaneDistance * FMath::Tan(FMath::DegreesToRadians(VerticalFov) / 2.0);
	Background->SetWorldLocation(Forward * 2000.0);
	Background->SetWorldRotation(FQuat(BackgroundBasis));
	Background->SetWorldScale3D(FVector(VisibleHeightCm * Width / Height / 100.0,
		-VisibleHeightCm / 100.0, 1.0));
	FString RegistrationError;
	if (!Registration.Register(RT->GameThread_GetRenderTargetResource(), RegistrationError))
	{ Finish(FString::Printf(TEXT("Gamma compositor dedicated target registration failed: %s"), *RegistrationError)); return false; }
	InitialStats = FRokCityGammaCompositor::GetStats();
	if (InitialStats.RegisteredTargets != 1 || InitialStats.PendingGpuValidations != 0)
	{ Finish(TEXT("Gamma compositor target is not the only idle registered audit target")); return false; }
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TSharedPtr<FJsonObject> Record = RecordModel(Index);
		if (!Record) { Finish(TEXT("Gamma source mesh/projection recording failed")); return false; }
		Models.Add(Record);
	}
	Stage = 0;
	StageStart = Elapsed;
	return true;
}

TSharedPtr<FJsonObject> URokCityGammaCompositeAuditSubsystem::RecordStats(
	const FRokCityGammaCompositorStats& Stats) const
{
	auto Row = MakeShared<FJsonObject>();
	Row->SetNumberField(TEXT("before_passes"), static_cast<double>(Stats.BeforePasses));
	Row->SetNumberField(TEXT("after_passes"), static_cast<double>(Stats.AfterPasses));
	Row->SetNumberField(TEXT("valid_pre_exposure_passes"), static_cast<double>(Stats.ValidPreExposurePasses));
	Row->SetNumberField(TEXT("invalid_pre_exposure_passes"), static_cast<double>(Stats.InvalidPreExposurePasses));
	Row->SetNumberField(TEXT("eligible_views_without_before"), static_cast<double>(Stats.EligibleViewsWithoutBefore));
	Row->SetNumberField(TEXT("unpaired_before_passes"), static_cast<double>(Stats.UnpairedBeforePasses));
	Row->SetNumberField(TEXT("unpaired_after_passes"), static_cast<double>(Stats.UnpairedAfterPasses));
	Row->SetNumberField(TEXT("target_conflicts"), static_cast<double>(Stats.TargetConflicts));
	Row->SetNumberField(TEXT("rejected_textures"), static_cast<double>(Stats.RejectedTextures));
	Row->SetNumberField(TEXT("pending_gpu_validations"), static_cast<double>(Stats.PendingGpuValidations));
	Row->SetNumberField(TEXT("registered_targets"), static_cast<double>(Stats.RegisteredTargets));
	return Row;
}

TSharedPtr<FJsonObject> URokCityGammaCompositeAuditSubsystem::RecordModel(int32 Index) const
{
	const ARokCitySpriteActor* Actor = TemporaryActors.IsValidIndex(Index) ? TemporaryActors[Index].Get() : nullptr;
	const UStaticMeshComponent* Component = Actor ? Actor->GetSpriteMeshComponent() : nullptr;
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	const USceneCaptureComponent2D* SC = CaptureComponent.Get();
	if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.IsEmpty() || !SC) return nullptr;
	const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
	if (LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() != 1) return nullptr;
	const FVector CameraPosition = SC->GetComponentLocation();
	const FVector Forward = SC->GetForwardVector(), Right = SC->GetRightVector(), Up = SC->GetUpVector();
	const double TanV = FMath::Tan(FMath::DegreesToRadians(VerticalFov) / 2.0);
	const double TanH = TanV * Width / Height;
	auto Record = MakeShared<FJsonObject>();
	Record->SetStringField(TEXT("label"), ModelPaths[Index].Label);
	Record->SetStringField(TEXT("source_model"), ModelPaths[Index].Model);
	Record->SetStringField(TEXT("mesh_asset"), Mesh->GetPathName());
	Record->SetStringField(TEXT("material_asset"), ModelPaths[Index].Material);
	Record->SetStringField(TEXT("mesh_uasset_sha256"), ShaFile(AssetFile(ModelPaths[Index].Mesh)));
	Record->SetStringField(TEXT("material_uasset_sha256"), ShaFile(AssetFile(ModelPaths[Index].Material)));
	Record->SetNumberField(TEXT("vertex_count"), LOD.GetNumVertices());
	Record->SetNumberField(TEXT("triangle_count"), LOD.GetNumTriangles());
	Record->SetArrayField(TEXT("actor_world_location_cm"), V3(Actor->GetActorLocation()));
	Record->SetArrayField(TEXT("actor_world_rotation_xyzw"), Q4(Actor->GetActorQuat()));
	Record->SetArrayField(TEXT("actor_world_scale"), V3(Actor->GetActorScale3D()));
	Record->SetArrayField(TEXT("component_relative_location_cm"), V3(Component->GetRelativeLocation()));
	Record->SetArrayField(TEXT("component_relative_rotation_xyzw"), Q4(Component->GetRelativeRotation().Quaternion()));
	Record->SetArrayField(TEXT("component_relative_scale"), V3(Component->GetRelativeScale3D()));
	TArray<TSharedPtr<FJsonValue>> Vertices;
	for (int32 V = 0; V < static_cast<int32>(LOD.GetNumVertices()); ++V)
	{
		const FVector Local(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(V));
		const FVector World = Component->GetComponentTransform().TransformPosition(Local);
		const FVector Delta = World - CameraPosition;
		const double Depth = FVector::DotProduct(Delta, Forward);
		if (Depth <= 0) return nullptr;
		auto Row = MakeShared<FJsonObject>();
		Row->SetArrayField(TEXT("local_cm"), V3(Local));
		Row->SetArrayField(TEXT("world_cm"), V3(World));
		const FVector2f UV = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(V, 0);
		Row->SetArrayField(TEXT("uv"), { MakeShared<FJsonValueNumber>(UV.X), MakeShared<FJsonValueNumber>(UV.Y) });
		Row->SetArrayField(TEXT("screen_xy_top_left_depth_cm"), {
			MakeShared<FJsonValueNumber>((1.0 + FVector::DotProduct(Delta, Right) / (Depth * TanH)) * .5 * Width),
			MakeShared<FJsonValueNumber>((1.0 - FVector::DotProduct(Delta, Up) / (Depth * TanV)) * .5 * Height),
			MakeShared<FJsonValueNumber>(Depth) });
		Vertices.Add(MakeShared<FJsonValueObject>(Row));
	}
	Record->SetArrayField(TEXT("vertices"), Vertices);
	TArray<TSharedPtr<FJsonValue>> Triangles;
	for (int32 I = 0; I + 2 < LOD.IndexBuffer.GetNumIndices(); I += 3)
	{
		Triangles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I)),
			MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I + 1)),
			MakeShared<FJsonValueNumber>(LOD.IndexBuffer.GetIndex(I + 2)) }));
	}
	Record->SetArrayField(TEXT("index_buffer_triangles"), Triangles);
	return Record;
}

bool URokCityGammaCompositeAuditSubsystem::StartFrame()
{
	if (!Native || !Contract || Stage < 0 || Stage >= 8) { Finish(TEXT("Gamma capture stage invalid")); return false; }
	const TSharedPtr<FJsonObject> Frame = Native->GetArrayField(TEXT("frames"))[Stage]->AsObject();
	const TSharedPtr<FJsonObject> Fixture = Contract->GetArrayField(TEXT("fixtures"))[Stage]->AsObject();
	USceneCaptureComponent2D* SC = CaptureComponent.Get();
	UTextureRenderTarget2D* RT = RenderTarget.Get();
	UStaticMeshComponent* Background = BackgroundComponent.Get();
	if (!Frame || !Fixture || !SC || !RT || !Background ||
		Frame->GetStringField(TEXT("name")) != Fixture->GetStringField(TEXT("name")) ||
		Frame->GetStringField(TEXT("light")) != Fixture->GetStringField(TEXT("light")) ||
		Frame->GetStringField(TEXT("background")) != Fixture->GetStringField(TEXT("background")) ||
		!Registration.IsRegistered() || RT->IsSRGB() ||
		FMath::Abs(RT->GetDisplayGamma() - 2.2f) > .001f || RT->SizeX != Width || RT->SizeY != Height ||
		SC->TextureTarget != RT || SC->CaptureSource != SCS_FinalColorLDR ||
		SC->ShowOnlyComponents.Num() != 4 || !SC->ShowFlags.PostProcessing ||
		!SC->ShowFlags.PostProcessMaterial || !SC->ShowFlags.Tonemapper ||
		SC->ShowFlags.AntiAliasing || SC->ShowFlags.DepthOfField || SC->ShowFlags.Fog ||
		!HasExactOutputBlendable(SC, ExactOutputMaterial.Get()))
	{ Finish(TEXT("Gamma source-frame contract or actual RHI capture configuration differs")); return false; }
	const auto& NativeLayers = Frame->GetArrayField(TEXT("layers"));
	const auto& FixtureLayers = Fixture->GetArrayField(TEXT("layers"));
	if (NativeLayers.Num() != FixtureLayers.Num())
	{ Finish(TEXT("Native Gamma layer count differs from contract")); return false; }
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Component = TemporaryActors[Index].IsValid()
			? TemporaryActors[Index]->GetSpriteMeshComponent() : nullptr;
		if (!Component || !Component->IsRegistered() || Component->GetMaterial(0) != ModelMaterials[Index].Get())
		{ Finish(TEXT("Gamma temporary model component changed before capture")); return false; }
		Component->SetVisibility(false);
	}
	for (int32 LayerIndex = 0; LayerIndex < NativeLayers.Num(); ++LayerIndex)
	{
		const TSharedPtr<FJsonObject> Source = NativeLayers[LayerIndex]->AsObject();
		const TSharedPtr<FJsonObject> Expected = FixtureLayers[LayerIndex]->AsObject();
		if (!Source || !Expected || Source->GetStringField(TEXT("label")) != Expected->GetStringField(TEXT("label")) ||
			Source->GetIntegerField(TEXT("order")) != Expected->GetIntegerField(TEXT("order")))
		{ Finish(TEXT("Gamma native/source fixture layer order changed")); return false; }
		const FString Label = Source->GetStringField(TEXT("label"));
		int32 ModelIndex = INDEX_NONE;
		for (int32 Index = 0; Index < 3; ++Index) if (Label == ModelPaths[Index].Label) ModelIndex = Index;
		if (ModelIndex == INDEX_NONE) { Finish(TEXT("Gamma fixture contains unknown model")); return false; }
		UStaticMeshComponent* Component = TemporaryActors[ModelIndex]->GetSpriteMeshComponent();
		if (Component->IsVisible()) { Finish(TEXT("Gamma fixture repeats a model")); return false; }
		Component->TranslucencySortPriority = Source->GetIntegerField(TEXT("order"));
		Component->MarkRenderStateDirty();
		Component->SetVisibility(true);
	}
	const FLinearColor Lighting = ReadColor(Frame->GetObjectField(TEXT("spriteColor")));
	for (auto& Material : ModelMaterials)
	{
		Material->SetVectorParameterValue(TEXT("SpriteColor"), Lighting);
		Material->SetScalarParameterValue(TEXT("CityTransparency"), 1.0f);
	}
	const FString BackgroundKind = Frame->GetStringField(TEXT("background"));
	if (BackgroundKind == TEXT("flat"))
	{
		const FLinearColor SourceClear = ReadColor(Frame->GetObjectField(TEXT("flatClearColor")));
		const FColor Bytes(FMath::RoundToInt(SourceClear.R * 255.0f),
			FMath::RoundToInt(SourceClear.G * 255.0f), FMath::RoundToInt(SourceClear.B * 255.0f), 255);
		FlatBackgroundMaterial->SetVectorParameterValue(TEXT("BackgroundColor"), FLinearColor::FromSRGBColor(Bytes));
		Background->SetMaterial(0, FlatBackgroundMaterial.Get());
	}
	else if (BackgroundKind == TEXT("pattern")) Background->SetMaterial(0, PatternBackgroundMaterial.Get());
	else { Finish(TEXT("Unknown source Gamma background kind")); return false; }
	if (!Background->IsRegistered() || !Background->IsVisible())
	{ Finish(TEXT("GPU background plane is not visible to capture")); return false; }
	bPreparingFrame = true;
	StageStart = Elapsed;
	return true;
}

bool URokCityGammaCompositeAuditSubsystem::CapturePreparedFrame()
{
	USceneCaptureComponent2D* SC = CaptureComponent.Get();
	UStaticMeshComponent* Background = BackgroundComponent.Get();
	if (!SC || !Background || !bPreparingFrame || !Background->IsRegistered() || !Background->IsVisible())
	{ Finish(TEXT("Prepared Gamma background or capture component became unavailable")); return false; }
	const TSharedPtr<FJsonObject> SourceFrame = Native->GetArrayField(TEXT("frames"))[Stage]->AsObject();
	const bool bPattern = SourceFrame->GetStringField(TEXT("background")) == TEXT("pattern");
	if (Background->GetMaterial(0) != (bPattern ? PatternBackgroundMaterial.Get() : FlatBackgroundMaterial.Get()))
	{ Finish(TEXT("Prepared Gamma background material changed before capture")); return false; }
	if (!HasExactOutputBlendable(SC, ExactOutputMaterial.Get()))
	{ Finish(TEXT("Exact Gamma replacement blendable lost its persistent capture view state")); return false; }
	// StartFrame changed the primitive material on the previous game tick. Commit its
	// render-state update before enqueueing the sole measured SceneCapture pass.
	FlushRenderingCommands();
	FramePreparationSeconds = Elapsed - StageStart;
	UE_LOG(LogTemp, Display, TEXT("Gamma audit prepared frame %d (%s) for %.4fs before capture"),
		Stage, bPattern ? TEXT("pattern") : TEXT("flat"), FramePreparationSeconds);
	FrameBeforeStats = FRokCityGammaCompositor::GetStats();
	if (FrameBeforeStats.RegisteredTargets != 1 || FrameBeforeStats.PendingGpuValidations != 0)
	{ Finish(TEXT("Gamma compositor not idle before frame")); return false; }
	SC->CaptureScene();
	bPreparingFrame = false;
	bWaitingForGpu = true;
	StageStart = Elapsed;
	return true;
}

bool URokCityGammaCompositeAuditSubsystem::FinishFrame()
{
	const FRokCityGammaCompositorStats Stats = FRokCityGammaCompositor::GetStats();
	if (Stats.PendingGpuValidations != 0 ||
		Difference(Stats.BeforePasses, FrameBeforeStats.BeforePasses) < 1 ||
		Difference(Stats.AfterPasses, FrameBeforeStats.AfterPasses) < 1) return false;
	if (Difference(Stats.BeforePasses, FrameBeforeStats.BeforePasses) != 1 ||
		Difference(Stats.AfterPasses, FrameBeforeStats.AfterPasses) != 1 ||
		Difference(Stats.ValidPreExposurePasses, FrameBeforeStats.ValidPreExposurePasses) != 2 ||
		Difference(Stats.InvalidPreExposurePasses, FrameBeforeStats.InvalidPreExposurePasses) != 0 ||
		Difference(Stats.UnpairedBeforePasses, FrameBeforeStats.UnpairedBeforePasses) != 0 ||
		Difference(Stats.UnpairedAfterPasses, FrameBeforeStats.UnpairedAfterPasses) != 0 ||
		Difference(Stats.TargetConflicts, FrameBeforeStats.TargetConflicts) != 0 ||
		Difference(Stats.RejectedTextures, FrameBeforeStats.RejectedTextures) != 0 ||
		Difference(Stats.EligibleViewsWithoutBefore, FrameBeforeStats.EligibleViewsWithoutBefore) != 0)
	{ Finish(TEXT("Gamma compositor pass pairing/pre-exposure validation failed for frame")); return false; }
	UTextureRenderTarget2D* RT = RenderTarget.Get();
	FTextureRenderTargetResource* Resource = RT ? RT->GameThread_GetRenderTargetResource() : nullptr;
	TArray<FColor> Pixels;
	if (!Resource || !Resource->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm)) || Pixels.Num() != Width * Height)
	{ Finish(TEXT("Gamma actual RHI BGRA8 readback failed")); return false; }
	const TSharedPtr<FJsonObject> SourceFrame = Native->GetArrayField(TEXT("frames"))[Stage]->AsObject();
	const FString Name = SourceFrame->GetStringField(TEXT("name"));
	const FString Png = RunDirectory / FString::Printf(TEXT("%02d_%s.png"), Stage, *Name);
	if (!FImageUtils::SaveImageByExtension(*Png, FImageView(Pixels.GetData(), Width, Height)))
	{ Finish(TEXT("Gamma RHI PNG save failed")); return false; }
	TArray<uint8> Raw;
	Raw.SetNumUninitialized(Pixels.Num() * sizeof(FColor));
	FMemory::Memcpy(Raw.GetData(), Pixels.GetData(), Raw.Num());
	const FString RawPath = FPaths::ChangeExtension(Png, TEXT("bgra8"));
	if (!FFileHelper::SaveArrayToFile(Raw, *RawPath))
	{ Finish(TEXT("Gamma raw RHI bytes save failed")); return false; }
	auto Frame = MakeShared<FJsonObject>();
	Frame->SetStringField(TEXT("name"), Name);
	Frame->SetStringField(TEXT("source_png"), SourceFrame->GetStringField(TEXT("png")));
	Frame->SetStringField(TEXT("png"), Png);
	Frame->SetStringField(TEXT("png_sha256"), ShaFile(Png));
	Frame->SetStringField(TEXT("raw_bgra8"), RawPath);
	Frame->SetStringField(TEXT("raw_sha256"), RokTrainArmyMesh::Sha256(Raw));
	Frame->SetStringField(TEXT("capture_api"), TEXT("SceneCapture FinalColorLDR -> RHI ReadPixels BGRA8 -> PNG"));
	Frame->SetStringField(TEXT("background_kind"), SourceFrame->GetStringField(TEXT("background")));
	Frame->SetNumberField(TEXT("background_preparation_seconds"), FramePreparationSeconds);
	Frame->SetNumberField(TEXT("background_preparation_game_ticks"), 1);
	Frame->SetStringField(TEXT("background_material"), BackgroundComponent->GetMaterial(0)->GetPathName());
	Frame->SetArrayField(TEXT("background_plane_world_location_cm"), V3(BackgroundComponent->GetComponentLocation()));
	Frame->SetArrayField(TEXT("background_plane_world_rotation_xyzw"), Q4(BackgroundComponent->GetComponentQuat()));
	Frame->SetArrayField(TEXT("background_plane_world_scale"), V3(BackgroundComponent->GetComponentScale()));
	Frame->SetStringField(TEXT("exact_output_material"), ExactOutputMaterial->GetPathName());
	Frame->SetStringField(TEXT("exact_output_bound_blendable_material"),
		CaptureComponent->PostProcessSettings.WeightedBlendables.Array[0].Object->GetPathName());
	Frame->SetStringField(TEXT("exact_output_base_material"), ExactOutputMaterial->GetMaterial()->GetPathName());
	Frame->SetNumberField(TEXT("exact_output_blendable_count"), CaptureComponent->PostProcessSettings.WeightedBlendables.Array.Num());
	Frame->SetNumberField(TEXT("exact_output_blendable_weight"), CaptureComponent->PostProcessSettings.WeightedBlendables.Array[0].Weight);
	Frame->SetNumberField(TEXT("capture_post_process_blend_weight"), CaptureComponent->PostProcessBlendWeight);
	Frame->SetStringField(TEXT("exact_output_blendable_location"), TEXT("BL_ReplacingTonemapper"));
	Frame->SetBoolField(TEXT("capture_always_persist_rendering_state"), CaptureComponent->bAlwaysPersistRenderingState);
	Frame->SetBoolField(TEXT("capture_view_state_present"), CaptureComponent->GetViewState(0) != nullptr);
	Frame->SetBoolField(TEXT("capture_every_frame"), CaptureComponent->bCaptureEveryFrame);
	Frame->SetArrayField(TEXT("source_sprite_color_gamma"), C4(ReadColor(SourceFrame->GetObjectField(TEXT("spriteColor")))));
	Frame->SetArrayField(TEXT("camera_position_cm"), V3(CaptureComponent->GetComponentLocation()));
	Frame->SetArrayField(TEXT("camera_forward_ue"), V3(CaptureComponent->GetForwardVector()));
	Frame->SetArrayField(TEXT("camera_right_ue"), V3(CaptureComponent->GetRightVector()));
	Frame->SetArrayField(TEXT("camera_up_ue"), V3(CaptureComponent->GetUpVector()));
	Frame->SetNumberField(TEXT("camera_horizontal_fov_degrees"), CaptureComponent->FOVAngle);
	Frame->SetNumberField(TEXT("camera_vertical_fov_degrees"), VerticalFov);
	Frame->SetNumberField(TEXT("render_target_display_gamma"), RT->GetDisplayGamma());
	Frame->SetBoolField(TEXT("render_target_srgb"), RT->IsSRGB());
	Frame->SetBoolField(TEXT("render_target_force_linear_gamma"), RT->bForceLinearGamma);
	Frame->SetArrayField(TEXT("extent"), { MakeShared<FJsonValueNumber>(Width), MakeShared<FJsonValueNumber>(Height) });
	Frame->SetBoolField(TEXT("post_processing"), CaptureComponent->ShowFlags.PostProcessing);
	Frame->SetBoolField(TEXT("post_process_material"), CaptureComponent->ShowFlags.PostProcessMaterial);
	Frame->SetBoolField(TEXT("tonemapper_show_flag"), CaptureComponent->ShowFlags.Tonemapper);
	Frame->SetNumberField(TEXT("show_only_component_count"), CaptureComponent->ShowOnlyComponents.Num());
	Frame->SetObjectField(TEXT("compositor_before"), RecordStats(FrameBeforeStats));
	Frame->SetObjectField(TEXT("compositor_after"), RecordStats(Stats));
	TArray<TSharedPtr<FJsonValue>> Layers;
	for (const TSharedPtr<FJsonValue>& Value : SourceFrame->GetArrayField(TEXT("layers")))
	{
		const TSharedPtr<FJsonObject> Source = Value->AsObject();
		for (int32 Index = 0; Index < 3; ++Index)
		{
			if (Source->GetStringField(TEXT("label")) != ModelPaths[Index].Label) continue;
			const ARokCitySpriteActor* Actor = TemporaryActors[Index].Get();
			const UStaticMeshComponent* Component = Actor->GetSpriteMeshComponent();
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("label"), ModelPaths[Index].Label);
			Row->SetNumberField(TEXT("source_order"), Source->GetIntegerField(TEXT("order")));
			Row->SetNumberField(TEXT("sort_priority"), Component->TranslucencySortPriority);
			Row->SetBoolField(TEXT("registered_visible"), Component->IsRegistered() && Component->IsVisible());
			Row->SetStringField(TEXT("mesh_asset"), Component->GetStaticMesh()->GetPathName());
			Row->SetStringField(TEXT("material_asset"), Component->GetMaterial(0)->GetPathName());
			Row->SetStringField(TEXT("source_material_variant"), ModelPaths[Index].Material);
			Row->SetStringField(TEXT("source_material_variant_sha256"), ShaFile(AssetFile(ModelPaths[Index].Material)));
			Row->SetArrayField(TEXT("actor_location_cm"), V3(Actor->GetActorLocation()));
			Row->SetArrayField(TEXT("actor_rotation_xyzw"), Q4(Actor->GetActorQuat()));
			Row->SetArrayField(TEXT("component_relative_location_cm"), V3(Component->GetRelativeLocation()));
			Row->SetArrayField(TEXT("component_relative_rotation_xyzw"), Q4(Component->GetRelativeRotation().Quaternion()));
			Row->SetArrayField(TEXT("component_relative_scale"), V3(Component->GetRelativeScale3D()));
			Layers.Add(MakeShared<FJsonValueObject>(Row));
		}
	}
	Frame->SetArrayField(TEXT("layers"), Layers);
	if (Layers.Num() != SourceFrame->GetArrayField(TEXT("layers")).Num())
	{ Finish(TEXT("Gamma frame emitted incomplete visible layer list")); return false; }
	Frames.Add(Frame);
	++Stage;
	bWaitingForGpu = false;
	StageStart = Elapsed;
	return true;
}

void URokCityGammaCompositeAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Stage < 0)
	{
		if (Elapsed >= 1.0) Begin();
		return;
	}
	if (bWaitingForGpu)
	{
		if (Elapsed - StageStart > 8.0)
		{
			Finish(TEXT("Gamma compositor GPU validation/readback timed out"));
			return;
		}
		FinishFrame();
		return;
	}
	if (bPreparingFrame)
	{
		if (Elapsed - StageStart > 8.0) { Finish(TEXT("Gamma background preparation timed out")); return; }
		CapturePreparedFrame();
		return;
	}
	if (Stage >= 8)
	{
		Finish();
		return;
	}
	if (Elapsed - StageStart >= .25) StartFrame();
}

bool URokCityGammaCompositeAuditSubsystem::Restore()
{
	if (bRestored) return true;
	UWorld* World = GetWorld();
	// Render-thread unregister must complete before the UObject target resource can die.
	Registration.Reset();
	if (USceneCaptureComponent2D* SC = CaptureComponent.Get())
	{
		SC->TextureTarget = nullptr;
		SC->UnregisterComponent();
	}
	CaptureComponent.Reset();
	RenderTarget.Reset();
	FlatBackgroundMaterial.Reset();
	PatternBackgroundMaterial.Reset();
	ExactOutputMaterial.Reset();
	PatternTexture.Reset();
	BackgroundComponent.Reset();
	ModelMaterials.Empty();
	for (const auto& Actor : TemporaryActors) if (Actor.IsValid()) Actor->Destroy();
	TemporaryActors.Empty();
	bool Okay = World && Economy(World) == EconomyBefore &&
		CountTemporary(World) == TemporaryCountBefore && CountActors(World) == ActorCountBefore;
	APlayerController* PC = PlayerController.Get();
	if (!PC || PC->GetViewTarget() != SavedViewTarget.Get()) Okay = false;
	if (PC)
	{
		UCameraComponent* Existing = PC->GetViewTarget()
			? PC->GetViewTarget()->FindComponentByClass<UCameraComponent>() : nullptr;
		if (Existing && (FVector::Dist(Existing->GetComponentLocation(), SavedCameraTransform.GetLocation()) > .01 ||
			FMath::Abs(Existing->FieldOfView - SavedCameraFov) > .001f)) Okay = false;
	}
	const FRokCityGammaCompositorStats AfterReset = FRokCityGammaCompositor::GetStats();
	if (AfterReset.RegisteredTargets != 0 || AfterReset.PendingGpuValidations != 0) Okay = false;
	bRestored = Okay;
	return Okay;
}

void URokCityGammaCompositeAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	bFinished = true;
	if (!Error.IsEmpty()) Failures.Add(Error);
	const FRokCityGammaCompositorStats BeforeReset = FRokCityGammaCompositor::GetStats();
	const bool Restored = Restore();
	const FRokCityGammaCompositorStats AfterReset = FRokCityGammaCompositor::GetStats();
	if (!Restored) Failures.Add(TEXT("Gamma capture target/actors/camera/economy were not fully restored"));
	if (Frames.Num() == 8 &&
		(Difference(BeforeReset.BeforePasses, InitialStats.BeforePasses) != 8 ||
		 Difference(BeforeReset.AfterPasses, InitialStats.AfterPasses) != 8 ||
		 Difference(BeforeReset.ValidPreExposurePasses, InitialStats.ValidPreExposurePasses) != 16 ||
		 Difference(BeforeReset.InvalidPreExposurePasses, InitialStats.InvalidPreExposurePasses) != 0 ||
		 Difference(BeforeReset.TargetConflicts, InitialStats.TargetConflicts) != 0 ||
		 Difference(BeforeReset.RejectedTextures, InitialStats.RejectedTextures) != 0 ||
		 BeforeReset.PendingGpuValidations != 0 || AfterReset.RegisteredTargets != 0))
		Failures.Add(TEXT("Gamma global eight-frame compositor pass counts failed"));
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_composite_runtime.v1"));
	Report->SetStringField(TEXT("scope"), TEXT("GameApp flag-only transient source meshes + opaque GPU background; isolated source Gamma composition fixture, not actual city placement/account construction"));
	Report->SetBoolField(TEXT("complete"), Frames.Num() == 8);
	Report->SetBoolField(TEXT("passed"), Frames.Num() == 8 && Failures.IsEmpty());
	Report->SetBoolField(TEXT("temporary_state_restored"), Restored);
	Report->SetBoolField(TEXT("map_saved"), false);
	Report->SetBoolField(TEXT("construction_ledger_touched"), false);
	Report->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Report->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Report->SetStringField(TEXT("native_render_sha256"), NativeRenderSha);
	Report->SetStringField(TEXT("native_contract_sha256"), NativeContractSha);
	Report->SetStringField(TEXT("native_verify_sha256"), NativeVerifySha);
	Report->SetStringField(TEXT("resource_mesh_verify_sha256"), ResourceMeshVerifySha);
	Report->SetStringField(TEXT("pattern_source_sha256"), PatternSha);
	Report->SetStringField(TEXT("economy_before"), EconomyBefore);
	Report->SetStringField(TEXT("economy_after"), Economy(GetWorld()));
	Report->SetNumberField(TEXT("actor_count_before"), ActorCountBefore);
	Report->SetNumberField(TEXT("actor_count_after_restore"), GetWorld() ? CountActors(GetWorld()) : -1);
	Report->SetNumberField(TEXT("temporary_actor_count_before"), TemporaryCountBefore);
	Report->SetNumberField(TEXT("temporary_actor_count_after_restore"), GetWorld() ? CountTemporary(GetWorld()) : -1);
	Report->SetObjectField(TEXT("compositor_initial"), RecordStats(InitialStats));
	Report->SetObjectField(TEXT("compositor_before_reset"), RecordStats(BeforeReset));
	Report->SetObjectField(TEXT("compositor_after_reset"), RecordStats(AfterReset));
	auto Assets = MakeShared<FJsonObject>();
	for (const FModelAssetPaths& Asset : ModelPaths)
	{
		Assets->SetStringField(FString::Printf(TEXT("%s_mesh"), Asset.Label), ShaFile(AssetFile(Asset.Mesh)));
		Assets->SetStringField(FString::Printf(TEXT("%s_material"), Asset.Label), ShaFile(AssetFile(Asset.Material)));
	}
	Assets->SetStringField(TEXT("flat_background"), ShaFile(AssetFile(FlatMaterialPath)));
	Assets->SetStringField(TEXT("pattern_background"), ShaFile(AssetFile(PatternMaterialPath)));
	Assets->SetStringField(TEXT("pattern_texture"), ShaFile(AssetFile(PatternTexturePath)));
	Assets->SetStringField(TEXT("exact_output"), ShaFile(AssetFile(ExactOutputPath)));
	Report->SetObjectField(TEXT("asset_sha256"), Assets);
	auto Inputs = MakeShared<FJsonObject>();
	for (const TCHAR* Relative : { TEXT("Source/Rok/RokCityGammaCompositeAuditSubsystem.h"),
		TEXT("Source/Rok/RokCityGammaCompositeAuditSubsystem.cpp"),
		TEXT("Source/RokRendering/Public/RokCityGammaCompositor.h"),
		TEXT("Source/RokRendering/Private/RokCityGammaCompositor.cpp"),
		TEXT("Shaders/Private/RokCityGammaTransfer.usf") })
		Inputs->SetStringField(Relative, ShaFile(FPaths::ProjectDir() / Relative));
	Report->SetObjectField(TEXT("input_sha256"), Inputs);
	TArray<TSharedPtr<FJsonValue>> ModelValues;
	for (const auto& Model : Models) ModelValues.Add(MakeShared<FJsonValueObject>(Model));
	Report->SetArrayField(TEXT("models"), ModelValues);
	TArray<TSharedPtr<FJsonValue>> FrameValues;
	for (const auto& Frame : Frames) FrameValues.Add(MakeShared<FJsonValueObject>(Frame));
	Report->SetArrayField(TEXT("frames"), FrameValues);
	TArray<TSharedPtr<FJsonValue>> FailureValues;
	for (const FString& Failure : Failures) FailureValues.Add(MakeShared<FJsonValueString>(Failure));
	Report->SetArrayField(TEXT("failures"), FailureValues);
	FString Text;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));
	const FString Manifest = RunDirectory / TEXT("manifest.json");
	FFileHelper::SaveStringToFile(Text, *Manifest);
	UE_LOG(LogTemp, Display, TEXT("Rok City Gamma composite audit passed=%d frames=%d manifest=%s"),
		Frames.Num() == 8 && Failures.IsEmpty(), Frames.Num(), *Manifest);
	FPlatformMisc::RequestExitWithStatus(false, Frames.Num() == 8 && Failures.IsEmpty() ? 0 : 1);
}
