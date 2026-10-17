#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../RokTrainArmyMeshData.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderingThread.h"
#include "TextureCompiler.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRokCityGammaTextureDescriptorTest,
	"Rok.Rendering.CityGammaRaster.SourceTextureDescriptors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokCityGammaTextureDescriptorTest::RunTest(const FString& Parameters)
{
	struct FTextureRow
	{
		const TCHAR* Model;
		const TCHAR* Role;
		const TCHAR* Asset;
		const TCHAR* Source;
		const TCHAR* SourceMeta;
		int32 Width;
		int32 Height;
	};
	static const FTextureRow Rows[] = {
		{TEXT("build_13_101_2"), TEXT("sprite"), TEXT("/Game/RokUnity/City/Academy/build_13_101_2/T_build_13_101_2_SpriteRaw"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_2/build_13_101_2.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_2/build_13_101_2.png.meta"), 512, 426},
		{TEXT("build_13_101_2"), TEXT("mask"), TEXT("/Game/RokUnity/City/Academy/build_13_101_2/T_build_13_101_2_MaskUnityGpu256"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_2/build_13_101_2_mask.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_2/build_13_101_2_mask.png.meta"), 256, 213},
		{TEXT("build_2_101_1"), TEXT("sprite"), TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/T_build_2_101_1_SpriteRaw"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_2_101_1.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_2_101_1.png.meta"), 512, 512},
		{TEXT("build_2_101_1"), TEXT("mask"), TEXT("/Game/RokUnity/City/ResourceBuildings/build_2_101_1/T_build_2_101_1_MaskUnityGpu256"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_2_101_1_mask.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_2_101_1_mask.png.meta"), 256, 256},
		{TEXT("build_3_101_1"), TEXT("sprite"), TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/T_build_3_101_1_SpriteRaw"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_3_101_1.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_3_101_1.png.meta"), 512, 459},
		{TEXT("build_3_101_1"), TEXT("mask"), TEXT("/Game/RokUnity/City/ResourceBuildings/build_3_101_1/T_build_3_101_1_MaskUnityGpu256"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_3_101_1_mask.png"), TEXT("ref/client-unity/Assets/BundleAssets/Map/Building/Building_use/shared_building/Building_101/city_building_1/build_3_101_1_mask.png.meta"), 256, 230},
	};

	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	TestTrue(TEXT("Runs in the editor"), GIsEditor);
	TestEqual(TEXT("Exact project identity"), FPaths::ConvertRelativePathToFull(ProjectDir),
		FPaths::ConvertRelativePathToFull(TEXT("D:/ueDemo/Rok/")));
	if (!GIsEditor || !FPaths::ConvertRelativePathToFull(ProjectDir).Equals(
		FPaths::ConvertRelativePathToFull(TEXT("D:/ueDemo/Rok/")), ESearchCase::IgnoreCase))
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> TextureRecords;
	for (const FTextureRow& Row : Rows)
	{
		const FString AssetPath(Row.Asset);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *AssetPath);
		TestNotNull(*FString::Printf(TEXT("Load %s"), Row.Asset), Texture);
		if (!Texture) return false;

		UTexture* Requested[] = {Texture};
		FTextureCompilingManager::Get().FinishCompilation(Requested);
		TestFalse(*FString::Printf(TEXT("Compilation finished: %s"), Row.Asset), Texture->IsCompiling());
		FlushRenderingCommands();

		const FString AssetFile = ProjectDir / TEXT("Content") / (AssetPath.Mid(6) + TEXT(".uasset"));
		TArray<uint8> AssetBytes;
		const bool bAssetRead = FFileHelper::LoadFileToArray(AssetBytes, *AssetFile);
		TestTrue(*FString::Printf(TEXT("UAsset readable: %s"), *AssetFile), bAssetRead);
		if (!bAssetRead) return false;

		const FString SourceFile = ProjectDir / Row.Source;
		const FString SourceMetaFile = ProjectDir / Row.SourceMeta;
		TArray<uint8> SourceBytes, SourceMetaBytes;
		const bool bSourceRead = FFileHelper::LoadFileToArray(SourceBytes, *SourceFile);
		const bool bMetaRead = FFileHelper::LoadFileToArray(SourceMetaBytes, *SourceMetaFile);
		TestTrue(*FString::Printf(TEXT("Source readable: %s"), *SourceFile), bSourceRead);
		TestTrue(*FString::Printf(TEXT("Source meta readable: %s"), *SourceMetaFile), bMetaRead);
		if (!bSourceRead || !bMetaRead) return false;

		const FTextureResource* Resource = Texture->GetResource();
		const FTextureRHIRef TextureRHI = Resource ? Resource->TextureRHI : FTextureRHIRef();
		TestTrue(*FString::Printf(TEXT("RHI resource ready: %s"), Row.Asset), TextureRHI.IsValid());
		if (!TextureRHI.IsValid()) return false;
		const FRHITextureDesc& Desc = TextureRHI->GetDesc();
		const bool bRhiSrgb = EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB);
		const EPixelFormat PixelFormat = Texture->GetPixelFormat();
		const EPixelFormat RhiFormat = Desc.Format;
		const FString Label = FString::Printf(TEXT("%s/%s"), Row.Model, Row.Role);
		TestEqual(*(Label + TEXT(" width")), Desc.Extent.X, Row.Width);
		TestEqual(*(Label + TEXT(" height")), Desc.Extent.Y, Row.Height);
		TestEqual(*(Label + TEXT(" mip count")), static_cast<int32>(Desc.NumMips), 1);
		TestEqual(*(Label + TEXT(" sample count")), static_cast<int32>(Desc.NumSamples), 1);
		TestFalse(*(Label + TEXT(" SRGB disabled")), Texture->SRGB);
		TestFalse(*(Label + TEXT(" RHI SRGB disabled")), bRhiSrgb);

		TSharedPtr<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetStringField(TEXT("model"), Row.Model);
		Record->SetStringField(TEXT("role"), Row.Role);
		Record->SetStringField(TEXT("asset"), AssetPath);
		Record->SetStringField(TEXT("asset_uasset_sha256"), RokTrainArmyMesh::Sha256(AssetBytes));
		Record->SetStringField(TEXT("unity_source"), Row.Source);
		Record->SetStringField(TEXT("unity_source_sha256"), RokTrainArmyMesh::Sha256(SourceBytes));
		Record->SetStringField(TEXT("unity_source_meta"), Row.SourceMeta);
		Record->SetStringField(TEXT("unity_source_meta_sha256"), RokTrainArmyMesh::Sha256(SourceMetaBytes));
		Record->SetStringField(TEXT("pixel_format_observed"), PixelFormat < PF_MAX ? GPixelFormats[PixelFormat].Name : TEXT("unknown"));
		Record->SetNumberField(TEXT("pixel_format_enum_observed"), static_cast<int32>(PixelFormat));
		Record->SetStringField(TEXT("rhi_format_observed"), RhiFormat < PF_MAX ? GPixelFormats[RhiFormat].Name : TEXT("unknown"));
		Record->SetNumberField(TEXT("rhi_format_enum_observed"), static_cast<int32>(RhiFormat));
		Record->SetNumberField(TEXT("width"), Desc.Extent.X);
		Record->SetNumberField(TEXT("height"), Desc.Extent.Y);
		Record->SetNumberField(TEXT("mips"), Desc.NumMips);
		Record->SetNumberField(TEXT("samples"), Desc.NumSamples);
		Record->SetNumberField(TEXT("rhi_flags"), static_cast<int64>(Desc.Flags));
		Record->SetBoolField(TEXT("srgb"), Texture->SRGB);
		Record->SetBoolField(TEXT("rhi_srgb"), bRhiSrgb);
		Record->SetNumberField(TEXT("compression_settings"), static_cast<int32>(Texture->CompressionSettings));
		Record->SetNumberField(TEXT("filter"), static_cast<int32>(Texture->Filter));
		Record->SetNumberField(TEXT("address_x"), static_cast<int32>(Texture->AddressX));
		Record->SetNumberField(TEXT("address_y"), static_cast<int32>(Texture->AddressY));
		Record->SetBoolField(TEXT("never_stream"), Texture->NeverStream);
		TextureRecords.Add(MakeShared<FJsonValueObject>(Record));
	}

	const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%S"));
	const FString RunDir = ProjectDir / TEXT("Saved/RokUnityMigration/CityGammaRasterTextureDescriptors") /
		FString::Printf(TEXT("%s_%u_%llu"), *Stamp, FPlatformProcess::GetCurrentProcessId(), FPlatformTime::Cycles64());
	IFileManager::Get().MakeDirectory(*RunDir, true);
	const FString OutputFile = RunDir / TEXT("texture_descriptors.json");
	if (IFileManager::Get().FileExists(*OutputFile))
	{
		AddError(TEXT("Refusing to overwrite an existing descriptor report"));
		return false;
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("rok.city_gamma_texture_descriptors.v1"));
	Root->SetStringField(TEXT("automation"), TEXT("Rok.Rendering.CityGammaRaster.SourceTextureDescriptors"));
	Root->SetStringField(TEXT("project"), ProjectDir);
	Root->SetBoolField(TEXT("editor"), GIsEditor);
	Root->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Root->SetBoolField(TEXT("asset_write"), false);
	Root->SetStringField(TEXT("academy_asset_report"), TEXT("Saved/RokUnityMigration/AcademyVisualAssets/ue_asset_verify.json"));
	Root->SetStringField(TEXT("resource_asset_report"), TEXT("Saved/RokUnityMigration/CityResourcePresentationAssets/ue_assets_verify.json"));
	Root->SetStringField(TEXT("gamma_material_source"), TEXT("Scripts/CreateRokCityGammaMaterials.py"));
	Root->SetArrayField(TEXT("textures"), TextureRecords);
	FString Json;
	FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
	TestTrue(TEXT("Descriptor output written"), FFileHelper::SaveStringToFile(Json, *OutputFile));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
