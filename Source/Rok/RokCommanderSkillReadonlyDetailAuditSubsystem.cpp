#include "RokCommanderSkillReadonlyDetailAuditSubsystem.h"

#include "RokBuildingUpgradeSubsystem.h"
#include "RokCommanderSkillReadonlyDetail.h"
#include "RokCommanderMeshWidget.h"
#include "RokMainHudWidget.h"
#include "RokPlayerController.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Widgets/SWindow.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
/** Read-only observation of the applied Commander presentation, for native audits. */
struct FRokCommanderUiSettledSample
{
	int32 RosterChildren = 0;
	int32 DataChildren = 0;
	int32 UnsettledChildren = 0;
	int32 NotVisibleChildren = 0;
	int32 UnsettledNotVisibleChildren = 0;
	int32 RemainingTextureCompilations = -1;
	int32 PortraitFrames = 0;
	int32 PortraitTextures = 0;
	float PortraitCameraX = 0.0f;
	bool bPortraitPresent = false;
	bool bPortraitAttached = false;
	bool bPortraitTexturesRhiReady = false;
	bool bPortraitTexturePropertyValid = false;
	bool bPortraitPlayingStandby = false;
	bool bStructureValid = false;
	bool bReady = false;
};

namespace RokCommanderUiSettledProbe
{
inline bool ReadPortraitTextureRhi(const URokCommanderMeshWidget* Portrait,
	int32& OutCount, bool& OutPropertyValid)
{
	OutCount = 0;
	OutPropertyValid = false;
	if (!Portrait) return false;
	const FArrayProperty* Array = FindFProperty<FArrayProperty>(Portrait->GetClass(), TEXT("Textures"));
	const FObjectPropertyBase* Object = Array ? CastField<FObjectPropertyBase>(Array->Inner) : nullptr;
	if (!Array || !Object || !Object->PropertyClass
		|| !Object->PropertyClass->IsChildOf(UTexture2D::StaticClass())) return false;
	OutPropertyValid = true;
	FScriptArrayHelper_InContainer Values(Array, Portrait);
	OutCount = Values.Num();
	if (OutCount <= 0) return false;
	for (int32 Index = 0; Index < OutCount; ++Index)
	{
		const UTexture2D* Texture = Cast<UTexture2D>(Object->GetObjectPropertyValue(Values.GetElementPtr(Index)));
		const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
		if (!Resource || !Resource->TextureRHI.IsValid()) return false;
	}
	return true;
}

inline FRokCommanderUiSettledSample Sample(const URokUnityFeatureScreenWidget* Screen, bool bRequireMesh)
{
	FRokCommanderUiSettledSample Result;
	if (!Screen || !Screen->IsScreenOpen()) return Result;
	const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Screen->GetWidgetFromName(TEXT("FeatureModeCanvas")));
	if (!Canvas) return Result;
	int32 FirstData = INDEX_NONE;
	int32 AfterData = INDEX_NONE;
	const URokCommanderMeshWidget* Portrait = nullptr;
	bool bFallbackPortrait = false;
	for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
	{
		const UWidget* Child = Canvas->GetChildAt(Index);
		if (!Child) return Result;
		const FString Name = Child->GetName();
		if (Name == TEXT("CommanderAnimatedPortrait")) Portrait = Cast<URokCommanderMeshWidget>(Child);
		if (Name == TEXT("CommanderHero2007") && Child->IsVisible()) bFallbackPortrait = true;
		if (Name == TEXT("CommanderAppellation"))
		{
			if (FirstData != INDEX_NONE) return Result;
			FirstData = Index;
		}
		if (Name == TEXT("CommanderPowerShade"))
		{
			if (AfterData != INDEX_NONE) return Result;
			AfterData = Index;
		}
	}
	if (FirstData == INDEX_NONE || AfterData <= FirstData) return Result;
	for (int32 Index = 0; Index < Canvas->GetChildrenCount(); ++Index)
	{
		const UWidget* Child = Canvas->GetChildAt(Index);
		const FString Name = Child->GetName();
		const bool bRoster = Name.StartsWith(TEXT("CommanderRoster"))
			|| Name.StartsWith(TEXT("CommanderSort")) || Name == TEXT("CommanderOwnedCount");
		const bool bData = Index >= FirstData && Index < AfterData;
		if (!bRoster && !bData) continue;
		if (bRoster) ++Result.RosterChildren;
		if (bData) ++Result.DataChildren;
		const bool bChildVisible = Child->IsVisible();
		const bool bChildSettled = Child->GetRenderOpacity() >= 0.995f
			&& Child->GetRenderTransform().Translation.IsNearlyZero(0.25f);
		if (!bChildVisible) ++Result.NotVisibleChildren;
		if (!bChildSettled)
		{
			++Result.UnsettledChildren;
			if (!bChildVisible) ++Result.UnsettledNotVisibleChildren;
		}
	}
	Result.bStructureValid = Result.RosterChildren > 0 && Result.DataChildren > 0;
#if WITH_EDITOR
	Result.RemainingTextureCompilations = FTextureCompilingManager::Get().GetNumRemainingTextures();
#endif
	Result.bPortraitPresent = Portrait != nullptr;
	if (Portrait)
	{
		Result.bPortraitAttached = Portrait->GetParent() == Canvas && Portrait->IsVisible();
		Result.PortraitFrames = Portrait->GetAnimationFrameCount();
		Result.PortraitCameraX = Portrait->GetSelectionCameraX();
		Result.bPortraitPlayingStandby = Portrait->IsPlayingStandby();
#if WITH_DEV_AUTOMATION_TESTS
		Result.bPortraitTexturesRhiReady = ReadPortraitTextureRhi(Portrait,
			Result.PortraitTextures, Result.bPortraitTexturePropertyValid);
#endif
	}
	const bool bPortraitReady = !Portrait
		? !bRequireMesh && bFallbackPortrait
		: Result.bPortraitAttached && Result.PortraitFrames >= 2
			&& Result.bPortraitTexturesRhiReady && !Result.bPortraitPlayingStandby
			&& FMath::Abs(Result.PortraitCameraX) <= 0.01f;
	Result.bReady = Result.bStructureValid && Result.UnsettledChildren == 0
		&& Result.RemainingTextureCompilations == 0 && bPortraitReady;
	return Result;
}
}

constexpr TCHAR CityMap[] = TEXT("/Game/Maps/L_RokUnityGameAppCity");
constexpr TCHAR AuditFlag[] = TEXT("-RokCommanderSkillReadonlyDetailAudit");

struct FInputPin { const TCHAR* Path; const TCHAR* Sha; };
// Exact source, Skill V3 product and runtime boundaries are immutable inputs. Audit/verifier
// files are also hashed at runtime and compared against the staged release report.
constexpr FInputPin Pinned[] = {
	{TEXT("Content/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.uasset"), TEXT("a3f6245b2c64f552e7b8a253e98f637d6ca0292a4aab87e5fd9183f03c76309f")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV1/IndependentReviewV1/review.json"), TEXT("3f9d139bf27a3f01370b884657b381cb23f5506e2b46469dd4dba7377bfa002f")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV3/contract.json"), TEXT("cc436ea352e745e5514b8f79e99e499fd5259136ed53bf4b2612d366966bddb1")},
	{TEXT("Saved/PendingHeroTalentReadonlyDetailV3/ready_for_root_review.json"), TEXT("99a826c860ce45686ed7d029f3ece1b96b59dda8915ebebf6d8d192bbce173a1")},
	{TEXT("Saved/RokUnityMigration/unity_gameplay_catalog.json"), TEXT("15115118db3366dbdee6f97b705dfaed11da73768694788e2abd7dd1b7a9bd6e")},
	{TEXT("Saved/RokUnityMigration/unity_runtime_bootstrap_state.json"), TEXT("07fcf9600d8e79afba470ef92c71460dd217e82e22dfaafc236a4fd58b5b0994")},
	{TEXT("Scripts/BuildRokScoutCatalog.py"), TEXT("f5a01bec3d4216b2214fdfd11f6178f5f0fe00d8e00aa43778625b55908f2c1c")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.cpp"), TEXT("b20503a0b0dfdfe552502ef307e6d8e6a79c9677e7a185fc0f589cd6c45a8ae5")},
	{TEXT("Source/Rok/RokBuildingUpgradeSubsystem.h"), TEXT("b2253b3fa8232a873e6f4f07fe4cd0f3127eb4cbd3c720a356794186e93ea7b4")},
	{TEXT("Source/Rok/RokMainHudWidget.cpp"), TEXT("6cb5e93c80e60fc2a6663d584e6d531fadd0da2f9a4c7583b2477557640da0f1")},
	{TEXT("Source/Rok/RokMainHudWidget.h"), TEXT("2740efb6b5c8a68222d5429f4d66e37e8e484302a215b6c46bbd8c783f67a2e8")},
	{TEXT("Source/Rok/RokPlayerController.cpp"), TEXT("f5c297bfb9b972db07e7725a9663a42e2c35f928625167e033e1bad97c6c5ef6")},
	{TEXT("Source/Rok/RokPlayerController.h"), TEXT("6eaa6329a03804c3af143c9777e8654d5dd83cfe24f598779b6107c988ffe772")},
	{TEXT("Source/Rok/RokServerSubsystem.cpp"), TEXT("3d73a28ff42b787c02a5c5a50ceba5bcac47056cc0945608bfea9ff9bd91bde4")},
	{TEXT("Source/Rok/RokServerSubsystem.h"), TEXT("4a07946d525ccb9eaedafcc30e68315e4ca5c52e94a88fc4c9b388833a07a5a9")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.cpp"), TEXT("e3dfd99ea973b2eba075c117d326ed16ddb44b5188c08740180c4d31c8593a9a")},
	{TEXT("Source/Rok/RokTrainArmyMeshData.h"), TEXT("e0c1e4f9f698366526ac740fd60f238076dc2c5ebc0e5ea85299a92fa9072945")},
	{TEXT("Source/Rok/RokUiAnimatedButton.cpp"), TEXT("bf8da24bdce3dfb403b7174eaa496e95550547cc47c57dccaca6e57aa39bedcf")},
	{TEXT("Source/Rok/RokUiAnimatedButton.h"), TEXT("4379cd36d5fba08d3b8c0fbf5c1076b8ce8b8cb27d2fec3c7fc2a4a65c62a3f7")},
	{TEXT("Source/Rok/RokUiWindowAnimation.cpp"), TEXT("bae58440eccb1ba826871acee2131b1223094b134cf0cfe04a730c798e2b41e4")},
	{TEXT("Source/Rok/RokUiWindowAnimation.h"), TEXT("1c39a74fd70ecec8f1e9b2fd135f65b312e86f2be60ebf323596d8a8359f9e3f")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"), TEXT("aae1f33dc3da2372afd0b591a1cc38dcab16d0e805a378316a35255e99bcb547")},
	{TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"), TEXT("a5b1328b702762dbc96912e34b350df0d61b47a4f5efc4ee69237c45f52568cf")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.cpp"), TEXT("6ae28d80202d586cca4457d55553a07b32fb6cf9bdf18d840852dccbe40596fd")},
	{TEXT("Source/Rok/RokUnityGameplayCatalogSubsystem.h"), TEXT("543dbe32ee1441288eed97bf8071eae0fa9443f04fc0bbb078e756f145c70617")},
	{TEXT("Source/Rok/RokUnityModalWidget.cpp"), TEXT("9c2fca7d169a40b8a5a10ba4ef036b07a6fc76719871f35451c6f49be800171b")},
	{TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("50cec5ddbdba9db12730edad021b8f359477d2b596a6504bc554dfda8ac220d3")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.cpp"), TEXT("3776b514262c0e25093c0d32e6c8e68fbf927dfb0040616ead4e0ae49f3b2c9e")},
	{TEXT("Source/Rok/RokUnityRuntimeStateSubsystem.h"), TEXT("e00dbdf4e20775f08d6498bcb06659d3516f29d824835c98a86533b8ca7e5112")},
	{TEXT("Source/Rok/Tests/RokCommanderTalentReadonlyDetailTests.cpp"), TEXT("b289c6715f020422786f0cb5a18b168b2d272376354f8e01fc2abe7471da4e59")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_CaptainTalent.prefab"), TEXT("c2eb5c853307e17a695c98840858b2a84853b4373ef367068f5a0fe81b2170bb")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_TalentSpecialPop.prefab"), TEXT("016729c5d3cafd155149401397fec4bb7aaf6fc96782192d5a8b5ea7b9dfbfbd")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_hero/UICaptain/UI_Item_TalentTop.prefab"), TEXT("34f1f4bfa61946d512a9836d414fc28aae967ff326315d24c902383b233609fc")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"), TEXT("3e05900aa0e68746af413c92ccd9b11212e6c0b39941e65558f6b7ec7d503bf0")},
	{TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta"), TEXT("8ec301da2729ea1fc47b1bebcf8436223f92c9193fdeed9b7a6f4644be774c97")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Captain/CaptainMediator.cs"), TEXT("c1b08245f29b8b2a6d95647e8f6d719f312dbf081fc82091bc58c8c82418c2ef")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_CaptainTalent_SubView.cs"), TEXT("c14d9b31ee61cd50a30763fc4750c8bdca85d6cde95c74b40bcd600f443d0787")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_TalentSpecialPop_SubView.cs"), TEXT("21fd26eb89322482079859cdc50ad651425dc986b48ead82ea37a120698c500e")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_TalentTop_SubView.cs"), TEXT("ba3cc59f53cb1a86bd11087f25f167f4ea985644a0e952d2b3f98b55310a31fb")},
	{TEXT("Source/Rok/RokCommanderSkillReadonlyDetail.h"), TEXT("4c337c2932e53fc7ab5b7ffe9030b6e0b85d7a5fbf60fbcca5734c7a3a8ccfce")},
	{TEXT("Source/Rok/Tests/RokCommanderSkillReadonlyDetailTests.cpp"), TEXT("89da04db7bdc971db9184f6e168e436d5777ac567524a85c63558f97c8b0ba6c")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/contract.json"), TEXT("ce50964b2550af8bc22db00c50840fba31e176e8c8653e9119455a464e3aebc7")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV4/ready_for_root_review.json"), TEXT("914c17bb8088ba29f7333491d60861eeed1499283e2906f44be77f66b0bd64e8")},
	{TEXT("Saved/PendingCommanderSkillReadonlyDetailV2/IndependentReviewV1/review.json"), TEXT("27175312f900f1511bb20e0a2e528ba3ac9e315e446591288dbbe214a1bc7574")},
	{TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Common/Logic/UI_Item_CaptainSkill_SubView.cs"), TEXT("2134c1980fe383fc53bda6056b0a93f12c488a97a8fd17d4001f61d64c7ef7b6")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_background.uasset"), TEXT("e2fd1376b752c346dd31117a5582d32aeae0cc82cfdf6d0e7bb8377058646f6a")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_head.uasset"), TEXT("c644358440e254d5f1be45508645606ba42f9d7371c9b6beb15c36d3efc77193")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_skill.uasset"), TEXT("e49fb6820a9de36aa968e684cbbb6b4c8ec367fc41935af1ac17478f1fcdfd23")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_skill2.uasset"), TEXT("4b519495eb3e5c8c2adb6b7db9aa87c69386bacd2bf5dd2eefc0f9a7283d294d")},
	{TEXT("Content/RokPrototype/UI/CommanderAtlases/rok_commander_ui.uasset"), TEXT("60514fe6be17eafc24fd25f438753686d57107e8917f3a88e85bb090f3ee23bb")},
};

bool TargetWorld(const UWorld* World)
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	FString Engine = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FPaths::NormalizeDirectoryName(Engine);
	return World && World->WorldType == EWorldType::Game
		&& World->GetOutermost()->GetName() == CityMap
		&& FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"))
		&& FPaths::IsSamePath(Engine, TEXT("D:/ue/UE_5.8/Engine"))
		&& FEngineVersion::Current().GetMajor() == 5
		&& FEngineVersion::Current().GetMinor() == 8;
}
FString FileSha(const FString& Path)
{
	TArray<uint8> Bytes;
	return FFileHelper::LoadFileToArray(Bytes, *Path) ? RokTrainArmyMesh::Sha256(Bytes) : FString();
}
FString Encode(const TSharedRef<FJsonObject>& Json)
{
	FString Text;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
	return Text;
}
TSharedRef<FJsonObject> Point(const FVector2D& Value)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("x"), Value.X);
	Json->SetNumberField(TEXT("y"), Value.Y);
	return Json;
}
bool Visible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	return true;
}
TSharedRef<FJsonObject> WidgetState(const UWidget* Widget)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Widget != nullptr);
	Json->SetBoolField(TEXT("visible"), Visible(Widget));
	Json->SetBoolField(TEXT("enabled"), Widget && Widget->GetIsEnabled());
	Json->SetStringField(TEXT("name"), Widget ? Widget->GetName() : FString());
	Json->SetStringField(TEXT("parent"), Widget && Widget->GetParent() ? Widget->GetParent()->GetName() : FString());
	Json->SetNumberField(TEXT("opacity"), Widget ? Widget->GetRenderOpacity() : 0.0);
	Json->SetObjectField(TEXT("scale"), Point(Widget ? Widget->GetRenderTransform().Scale : FVector2D::ZeroVector));
	const FGeometry Geometry = Widget ? Widget->GetCachedGeometry() : FGeometry();
	Json->SetObjectField(TEXT("local_size"), Point(Geometry.GetLocalSize()));
	Json->SetObjectField(TEXT("absolute_top_left"), Point(Geometry.LocalToAbsolute(FVector2D::ZeroVector)));
	Json->SetObjectField(TEXT("absolute_bottom_right"), Point(Geometry.LocalToAbsolute(Geometry.GetLocalSize())));
	if (const UTextBlock* Text = Cast<UTextBlock>(Widget)) Json->SetStringField(TEXT("text"), Text->GetText().ToString());
	return Json;
}
struct FSkillCenterPatch
{
	bool bExists = false;
	bool bDirectChild = false;
	bool bVisible = false;
	bool bGeometry = false;
	bool bSourceCrop = false;
	bool bTexture = false;
	bool bRhi = false;
	bool bAlpha = false;
	FVector2D Geometry = FVector2D::ZeroVector;
	FVector2D SlotSize = FVector2D::ZeroVector;
	FVector2D SlotPosition = FVector2D::ZeroVector;
	FVector2D BrushSize = FVector2D::ZeroVector;
	FBox2f Uv = FBox2f(ForceInit);
	float ImageAlpha = 0.0f;
	float BrushAlpha = 0.0f;
	FString TexturePath;
	bool Valid() const { return bExists && bDirectChild && bVisible && bGeometry
		&& bSourceCrop && bTexture && bRhi && bAlpha; }
};
FSkillCenterPatch ReadSkillCenterPatch(const UWidget* Widget)
{
	FSkillCenterPatch Result;
	const UImage* Image = Cast<UImage>(Widget);
	if (!Image) return Result;
	Result.bExists = true;
	Result.bDirectChild = Image->GetParent()
		&& Image->GetParent()->GetName() == TEXT("CommanderSkillDetailFrame");
	Result.bVisible = Visible(Image) && Image->GetIsEnabled();
	Result.Geometry = Image->GetCachedGeometry().GetLocalSize();
	const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Image->Slot);
	if (Slot)
	{
		Result.SlotSize = Slot->GetSize();
		Result.SlotPosition = Slot->GetPosition();
	}
	const FSlateBrush& Brush = Image->GetBrush();
	Result.BrushSize = FVector2D(Brush.ImageSize.X, Brush.ImageSize.Y);
	Result.Uv = static_cast<FBox2f>(Brush.GetUVRegion());
	Result.ImageAlpha = Image->GetColorAndOpacity().A;
	Result.BrushAlpha = Brush.TintColor.GetSpecifiedColor().A;
	const UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject());
	Result.TexturePath = Texture ? Texture->GetPathName() : FString();
	const FTextureResource* Resource = IsValid(Texture) ? Texture->GetResource() : nullptr;
	Result.bRhi = Resource && Resource->TextureRHI.IsValid();
	Result.bTexture = Texture && Texture->GetSizeX() == 2048 && Texture->GetSizeY() == 1024
		&& Result.TexturePath == TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
	Result.bGeometry = Slot && Result.Geometry.X > 0 && Result.Geometry.Y > 0
		&& Result.SlotSize.Equals(FVector2D(436.0f, 358.0f), 0.5f)
		&& Result.SlotPosition.Equals(FVector2D(21.0f, 23.0f), 0.5f)
		&& Result.BrushSize.Equals(FVector2D(436.0f, 358.0f), 0.5f);
	const FVector2f Min(795.0f / 2048.0f, 699.0f / 1024.0f);
	const FVector2f Max(825.0f / 2048.0f, 724.0f / 1024.0f);
	Result.bSourceCrop = Brush.DrawAs == ESlateBrushDrawType::Image && Result.Uv.bIsValid
		&& Result.Uv.Min.Equals(Min, 0.0001f) && Result.Uv.Max.Equals(Max, 0.0001f);
	Result.bAlpha = Result.ImageAlpha > 0.99f && Result.BrushAlpha > 0.99f;
	return Result;
}
TSharedRef<FJsonObject> SkillCenterPatchState(const FSkillCenterPatch& Patch)
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("exists"), Patch.bExists);
	Json->SetBoolField(TEXT("direct_child"), Patch.bDirectChild);
	Json->SetBoolField(TEXT("visible"), Patch.bVisible);
	Json->SetBoolField(TEXT("positive_geometry"), Patch.bGeometry);
	Json->SetBoolField(TEXT("source_crop_exact"), Patch.bSourceCrop);
	Json->SetBoolField(TEXT("texture_exact"), Patch.bTexture);
	Json->SetBoolField(TEXT("texture_rhi_valid"), Patch.bRhi);
	Json->SetBoolField(TEXT("positive_alpha"), Patch.bAlpha);
	Json->SetBoolField(TEXT("ready"), Patch.Valid());
	Json->SetObjectField(TEXT("geometry_local_size"), Point(Patch.Geometry));
	Json->SetObjectField(TEXT("canvas_slot_size"), Point(Patch.SlotSize));
	Json->SetObjectField(TEXT("canvas_slot_position"), Point(Patch.SlotPosition));
	Json->SetObjectField(TEXT("brush_image_size"), Point(Patch.BrushSize));
	Json->SetObjectField(TEXT("uv_min"), Point(FVector2D(Patch.Uv.Min)));
	Json->SetObjectField(TEXT("uv_max"), Point(FVector2D(Patch.Uv.Max)));
	Json->SetNumberField(TEXT("image_alpha"), Patch.ImageAlpha);
	Json->SetNumberField(TEXT("brush_alpha"), Patch.BrushAlpha);
	Json->SetStringField(TEXT("texture_path"), Patch.TexturePath);
	return Json;
}
bool HasTag(const AActor* Actor, const TCHAR* Prefix)
{
	if (!Actor) return false;
	for (const FName& Tag : Actor->Tags) if (Tag.ToString().StartsWith(Prefix)) return true;
	return false;
}
int32 PngDimension(const TArray<uint8>& Data, int32 Offset)
{
	return (static_cast<int32>(Data[Offset]) << 24) | (static_cast<int32>(Data[Offset + 1]) << 16)
		| (static_cast<int32>(Data[Offset + 2]) << 8) | static_cast<int32>(Data[Offset + 3]);
}
}

bool URokCommanderSkillReadonlyDetailAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING || !WITH_DEV_AUTOMATION_TESTS
	return false; // Pending-completion count is only exposed to audited Editor builds.
#else
	return TargetWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCommanderSkillReadonlyDetailAudit"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokCommanderSkillReadonlyDetailAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokCommanderSkillReadonlyDetailAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCommanderSkillReadonlyDetailAuditSubsystem, STATGROUP_Tickables);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()
		/ TEXT("PendingCommanderSkillReadonlyRuntimeAuditV5/RuntimeAudit")
		/ FString::Printf(TEXT("%lld_%u"), FDateTime::UtcNow().GetTicks(), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &URokCommanderSkillReadonlyDetailAuditSubsystem::ScreenshotProcessed);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Buildings.IsValid()) Buildings->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	Super::Deinitialize();
}
bool URokCommanderSkillReadonlyDetailAuditSubsystem::Identity() const { return TargetWorld(GetWorld()); }
bool URokCommanderSkillReadonlyDetailAuditSubsystem::Expect(bool Condition, const TCHAR* Reason)
{
	if (!Condition) Finish(Reason);
	return Condition;
}
bool URokCommanderSkillReadonlyDetailAuditSubsystem::PrepareOwnWindowFocus()
{
#if PLATFORM_WINDOWS
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	DWORD OwnerPid = 0;
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	RECT Client = {};
	if (!Expect(Handle && ::IsWindow(Handle) && OwnerPid == ::GetCurrentProcessId()
		&& ::GetClientRect(Handle, &Client) && Client.right - Client.left == 1440
		&& Client.bottom - Client.top == 810, TEXT("Audit requires own 1440x810 GameApp window"))) return false;
	if (::GetForegroundWindow() == Handle) return true;
	if (::IsIconic(Handle)) ::ShowWindow(Handle, SW_RESTORE);
	else if (!::IsWindowVisible(Handle)) ::ShowWindow(Handle, SW_SHOW);
	const HWND Foreground = ::GetForegroundWindow();
	const DWORD ForegroundThread = Foreground ? ::GetWindowThreadProcessId(Foreground, nullptr) : 0;
	const DWORD CurrentThread = ::GetCurrentThreadId();
	const bool bAttached = ForegroundThread && ForegroundThread != CurrentThread
		&& ::AttachThreadInput(CurrentThread, ForegroundThread, 1) != 0;
	::BringWindowToTop(Handle);
	::SetForegroundWindow(Handle);
	if (bAttached) ::AttachThreadInput(CurrentThread, ForegroundThread, 0);
	return ::GetForegroundWindow() == Handle;
#else
	return Expect(false, TEXT("Audit requires Windows game window"));
#endif
}

bool URokCommanderSkillReadonlyDetailAuditSubsystem::Begin()
{
	if (!Expect(Identity() && FSlateApplication::IsInitialized(), TEXT("Wrong world/project or Slate unavailable"))) return false;
	const TCHAR* Cursor = FCommandLine::Get();
	FString Token;
	int32 AuditFlags = 0, LocalFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (Token.Equals(AuditFlag, ESearchCase::IgnoreCase)) { ++AuditFlags; continue; }
		if (Token.Equals(TEXT("-RokLocalOnly"), ESearchCase::IgnoreCase)) { ++LocalFlags; continue; }
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase))
			return Expect(false, TEXT("Unexpected Rok fixture/diagnostic flag"));
	}
	if (!Expect(AuditFlags == 1 && LocalFlags == 1, TEXT("Audit and local-only flags must occur exactly once"))) return false;
	Controller = Cast<ARokPlayerController>(GetWorld()->GetFirstPlayerController());
	Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Runtime = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	Catalog = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	Server = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<URokServerSubsystem>() : nullptr;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets) if (Widget && Widget->GetWorld() == GetWorld())
	{
		if (Hud.IsValid()) return Expect(false, TEXT("More than one GameApp HUD"));
		Hud = Cast<URokMainHudWidget>(Widget);
	}
	Screen = Hud.IsValid() ? Hud->GetFeatureScreenWidgetForPlacement() : nullptr;
	Modal = Hud.IsValid() ? Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget"))) : nullptr;
	if (!Expect(Controller.IsValid() && Controller->GetPawn() && Buildings.IsValid() && Runtime.IsValid()
		&& Runtime->IsRuntimeStateLoaded() && Catalog.IsValid() && Server.IsValid() && Server->IsLocalOnlyRun()
		&& Server->GetHttpDispatchAttemptCount() == 0 && Server->GetSuppressedLocalRequestCount() == 0
		&& Hud.IsValid() && Screen.IsValid() && Modal.IsValid() && !Screen->IsScreenOpen()
		&& !Modal->IsModalOpen() && FVector2D(1440, 810).Equals(UWidgetLayoutLibrary::GetViewportSize(this), .1f),
		TEXT("Expected idle local-only 1440x810 GameApp HUD and catalog"))) return false;
	const auto Profile = Runtime->GetPlayerProfileState();
	if (!Expect(Profile.StateKind == TEXT("local_preview") && !Profile.bHasRoleId && Profile.RoleId.IsEmpty()
		&& !Profile.bHasCivilization && Profile.CivilizationId == 0 && Profile.CivilizationName.IsEmpty(),
		TEXT("Expected anonymous local-preview account boundary"))) return false;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (It->ActorHasTag(TEXT("UnityInitId_5")))
		{
			if (Hall.IsValid()) return Expect(false, TEXT("Ambiguous UnityInitId_5 Hall"));
			Hall = *It;
		}
	if (!Expect(Hall.IsValid() && Hall->ActorHasTag(TEXT("UnityBuildingType_1"))
		&& Hall->GetWorld() == GetWorld(), TEXT("Source Hall identity missing"))) return false;
	if (!Expect(Runtime->GetStarterHeroes().Num() == 5, TEXT("Five local bootstrap heroes required"))) return false;
	for (const TCHAR* Asset : {
		TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"),
		TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_background.rok_commander_background"),
		TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head.rok_commander_head"),
		TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill.rok_commander_skill"),
		TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill2.rok_commander_skill2"),
		TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui.rok_commander_ui")})
		if (!Expect(LoadObject<UTexture2D>(nullptr, Asset) != nullptr,
			TEXT("Required Commander texture asset failed to load"))) return false;
	OriginalPointer = FSlateApplication::Get().GetCursorPos();
	LastPointer = OriginalPointer;
	bPointerSaved = true;
	Inputs = MakeShared<FJsonObject>();
	for (const FInputPin& Pin : Pinned)
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Pin.Path);
		if (!Expect(Actual == Pin.Sha, TEXT("Pinned source/product SHA differs from reviewed input"))) return false;
		Inputs->SetStringField(Pin.Path, Actual);
	}
	for (const TCHAR* Path : {TEXT("Source/Rok/RokCommanderSkillReadonlyDetailAuditSubsystem.h"),
		TEXT("Source/Rok/RokCommanderSkillReadonlyDetailAuditSubsystem.cpp"),
		TEXT("Scripts/VerifyRokCommanderSkillReadonlyDetailRuntime.py")})
	{
		const FString Actual = FileSha(FPaths::ProjectDir() / Path);
		if (!Expect(Actual.Len() == 64, TEXT("Integrated audit/verifier input missing"))) return false;
		Inputs->SetStringField(Path, Actual);
	}
	CompletionHandle = Buildings->OnLocalUpgradeCompleted.AddUObject(
		this, &URokCommanderSkillReadonlyDetailAuditSubsystem::OnUpgradeCompleted);
	return true;
}

TSharedRef<FJsonObject> URokCommanderSkillReadonlyDetailAuditSubsystem::BusinessState() const
{
	auto Json = MakeShared<FJsonObject>();
	if (!Runtime.IsValid() || !Buildings.IsValid() || !Server.IsValid() || !Hall.IsValid()) return Json;
	FRokBuildingState HallState;
	const bool bCached = Buildings->TryPeekExistingBuildingStateForActor(Hall.Get(), HallState);
	Json->SetBoolField(TEXT("hall_cached_exact_binding"), bCached);
	Json->SetStringField(TEXT("hall_actor_path"), Hall->GetPathName());
	Json->SetStringField(TEXT("hall_actor_class"), Hall->GetClass()->GetPathName());
	TArray<FString> Tags;
	for (const FName& Tag : Hall->Tags) Tags.Add(Tag.ToString());
	Tags.Sort();
	TArray<TSharedPtr<FJsonValue>> TagValues;
	for (const FString& Tag : Tags) TagValues.Add(MakeShared<FJsonValueString>(Tag));
	Json->SetArrayField(TEXT("hall_tags"), TagValues);
	Json->SetStringField(TEXT("hall_key"), HallState.BuildingKey.ToString());
	Json->SetStringField(TEXT("hall_display_name"), HallState.DisplayName.ToString());
	Json->SetNumberField(TEXT("hall_type"), static_cast<int32>(HallState.Type));
	Json->SetNumberField(TEXT("hall_init_id"), HallState.UnityInitId);
	Json->SetNumberField(TEXT("hall_source_type"), HallState.UnityBuildingTypeId);
	Json->SetNumberField(TEXT("hall_grid_x"), HallState.UnityGridX);
	Json->SetNumberField(TEXT("hall_grid_y"), HallState.UnityGridY);
	Json->SetNumberField(TEXT("hall_level"), HallState.Level);
	Json->SetNumberField(TEXT("hall_power"), HallState.Power);
	Json->SetNumberField(TEXT("hall_produce_per_hour"), HallState.ProducePerHour);
	Json->SetNumberField(TEXT("hall_gather_min"), HallState.GatherMin);
	Json->SetNumberField(TEXT("hall_gather_max"), HallState.GatherMax);
	Json->SetBoolField(TEXT("hall_upgrade_in_progress"), HallState.bUpgradeInProgress);
	Json->SetNumberField(TEXT("hall_upgrade_started_at"), HallState.UpgradeStartedAt);
	Json->SetNumberField(TEXT("hall_upgrade_finishes_at"), HallState.UpgradeFinishesAt);
	Json->SetStringField(TEXT("hall_icon"), HallState.Icon.ToString());
	const auto Wallet = Buildings->GetCurrentResources();
	Json->SetNumberField(TEXT("wallet_food"), Wallet.Food);
	Json->SetNumberField(TEXT("wallet_wood"), Wallet.Wood);
	Json->SetNumberField(TEXT("wallet_stone"), Wallet.Stone);
	Json->SetNumberField(TEXT("wallet_gold"), Wallet.Gold);
	const auto Rates = Buildings->GetProductionRates();
	Json->SetNumberField(TEXT("rate_food"), Rates.FoodPerHour);
	Json->SetNumberField(TEXT("rate_wood"), Rates.WoodPerHour);
	Json->SetNumberField(TEXT("rate_stone"), Rates.StonePerHour);
	Json->SetNumberField(TEXT("rate_gold"), Rates.GoldPerHour);
	Json->SetNumberField(TEXT("owned_queues"), Buildings->GetOwnedBuildQueueCount());
	Json->SetNumberField(TEXT("active_upgrades"), Buildings->GetActiveUpgradeCount());
	Json->SetNumberField(TEXT("active_construction"), Buildings->GetActiveLocalConstructionCount());
#if WITH_DEV_AUTOMATION_TESTS
	Json->SetNumberField(TEXT("pending_completed_upgrades"), Buildings->GetPendingCompletedUpgradeCountForTesting());
#else
	Json->SetNumberField(TEXT("pending_completed_upgrades"), -1);
#endif
	Json->SetNumberField(TEXT("completed_upgrade_events"), CompletionEvents);
	// IDs are consecutive from one and tickets are never removed. Absence of ID 1
	// plus zero active constructions proves this fresh-city fixture has no ticket.
	FRokLocalConstructionTicket Ticket;
	Json->SetBoolField(TEXT("construction_ticket_1_exists"), Buildings->GetLocalConstructionTicket(1, Ticket));
	TArray<FString> CityActors;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (HasTag(*It, TEXT("UnityInitId_")) || It->ActorHasTag(TEXT("RokLocalConstructedBuilding")))
			CityActors.Add(It->GetPathName());
	CityActors.Sort();
	TArray<TSharedPtr<FJsonValue>> CityValues;
	for (const FString& Path : CityActors) CityValues.Add(MakeShared<FJsonValueString>(Path));
	Json->SetArrayField(TEXT("city_actor_paths"), CityValues);
	const auto Task = Runtime->GetActiveTask();
	Json->SetNumberField(TEXT("task_id"), Task.TaskId);
	Json->SetNumberField(TEXT("task_type"), Task.Type);
	Json->SetNumberField(TEXT("task_param1"), Task.Param1);
	Json->SetNumberField(TEXT("task_param2"), Task.Param2);
	Json->SetNumberField(TEXT("task_current"), Task.Current);
	Json->SetNumberField(TEXT("task_target"), Task.Target);
	Json->SetBoolField(TEXT("task_complete"), Task.bComplete);
	Json->SetNumberField(TEXT("task_next_id"), Task.NextId);
	Json->SetNumberField(TEXT("task_reward_group"), Task.Reward);
	Json->SetStringField(TEXT("task_progress_rule"), Task.ProgressRule);
	Json->SetStringField(TEXT("task_name"), Task.Name);
	Json->SetStringField(TEXT("task_description"), Task.Desc);
	Json->SetNumberField(TEXT("task_require"), Task.Require);
	Json->SetStringField(TEXT("task_icon"), Task.Icon);
	TArray<TSharedPtr<FJsonValue>> TaskChain;
	for (const auto& Row : Runtime->GetActiveTaskChain())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Row.TaskId);
		Entry->SetStringField(TEXT("name"), Row.Name);
		Entry->SetStringField(TEXT("description"), Row.Desc);
		Entry->SetNumberField(TEXT("type"), Row.Type);
		Entry->SetNumberField(TEXT("param1"), Row.Param1);
		Entry->SetNumberField(TEXT("param2"), Row.Param2);
		Entry->SetNumberField(TEXT("require"), Row.Require);
		Entry->SetNumberField(TEXT("reward"), Row.Reward);
		Entry->SetNumberField(TEXT("next_id"), Row.NextId);
		Entry->SetStringField(TEXT("progress_rule"), Row.ProgressRule);
		Entry->SetNumberField(TEXT("current"), Row.Current);
		Entry->SetNumberField(TEXT("target"), Row.Target);
		Entry->SetBoolField(TEXT("complete"), Row.bComplete);
		Entry->SetStringField(TEXT("icon"), Row.Icon);
		TaskChain.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("task_chain"), TaskChain);
	const auto Profile = Runtime->GetPlayerProfileState();
	Json->SetStringField(TEXT("profile_kind"), Profile.StateKind);
	Json->SetBoolField(TEXT("profile_has_role_id"), Profile.bHasRoleId);
	Json->SetStringField(TEXT("profile_role_id"), Profile.RoleId);
	Json->SetBoolField(TEXT("profile_has_display_name"), Profile.bHasDisplayName);
	Json->SetStringField(TEXT("profile_display_name"), Profile.DisplayName);
	Json->SetBoolField(TEXT("profile_has_civilization"), Profile.bHasCivilization);
	Json->SetNumberField(TEXT("profile_civilization_id"), Profile.CivilizationId);
	Json->SetStringField(TEXT("profile_civilization_name"), Profile.CivilizationName);
	Json->SetNumberField(TEXT("profile_local_hero_score"), Profile.LocalHeroScore);
	Json->SetNumberField(TEXT("profile_local_troop_power"), Profile.LocalTroopPower);
	Json->SetNumberField(TEXT("profile_local_preview_score"), Profile.LocalPreviewScore);
	Json->SetNumberField(TEXT("profile_local_building_upgrade_score"), Profile.LocalBuildingUpgradeScore);
	TArray<TSharedPtr<FJsonValue>> Heroes;
	for (const auto& Hero : Runtime->GetStarterHeroes())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Hero.HeroId);
		Entry->SetNumberField(TEXT("level"), Hero.Level);
		Entry->SetNumberField(TEXT("star"), Hero.Star);
		Entry->SetNumberField(TEXT("max_star"), Hero.MaxStar);
		Entry->SetNumberField(TEXT("rare"), Hero.Rare);
		Entry->SetNumberField(TEXT("score"), Hero.Score);
		Entry->SetNumberField(TEXT("skill_count"), Hero.SkillCount);
		Entry->SetNumberField(TEXT("talent_count"), Hero.TalentCount);
		Entry->SetStringField(TEXT("icon"), Hero.Icon);
		Entry->SetStringField(TEXT("model"), Hero.Model);
		Heroes.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_heroes"), Heroes);
	TArray<TSharedPtr<FJsonValue>> Troops;
	for (const auto& Troop : Runtime->GetStarterTroopStacks())
	{
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("id"), Troop.TroopId);
		Entry->SetNumberField(TEXT("arms_type"), Troop.ArmsType);
		Entry->SetNumberField(TEXT("level"), Troop.Level);
		Entry->SetNumberField(TEXT("count"), Troop.Count);
		Entry->SetNumberField(TEXT("power_per_unit"), Troop.PowerPerUnit);
		Entry->SetNumberField(TEXT("power"), Troop.TotalPower);
		Entry->SetNumberField(TEXT("train_seconds"), Troop.TrainSeconds);
		Entry->SetStringField(TEXT("icon"), Troop.Icon);
		Entry->SetStringField(TEXT("model"), Troop.Model);
		Troops.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Json->SetArrayField(TEXT("starter_troops"), Troops);
	TArray<TSharedPtr<FJsonValue>> Trainable;
	for (const int32 Id : Runtime->GetTrainableTroopIds()) Trainable.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("trainable_troop_ids"), Trainable);
	Json->SetBoolField(TEXT("server_local_only"), Server->IsLocalOnlyRun());
	Json->SetNumberField(TEXT("http_dispatch_attempts"), static_cast<double>(Server->GetHttpDispatchAttemptCount()));
	Json->SetNumberField(TEXT("http_suppressed_requests"), static_cast<double>(Server->GetSuppressedLocalRequestCount()));
	return Json;
}

bool URokCommanderSkillReadonlyDetailAuditSubsystem::Boundary() const
{
	if (!Identity() || !bBaselineSet || !Controller.IsValid() || !Hall.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun()
		|| Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0
		|| CompletionEvents != 0 || !Controller->GetPawn()
		|| !Controller->GetPawn()->GetActorTransform().Equals(BaselineCamera, .001f)
		|| Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount != BaselinePrimary) return false;
	return Encode(BusinessState()) == BaselineBusiness;
}

TSharedRef<FJsonObject> URokCommanderSkillReadonlyDetailAuditSubsystem::State() const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Json->SetObjectField(TEXT("business"), BusinessState());
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
#if PLATFORM_WINDOWS
	const HWND Handle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	const HWND Foreground = ::GetForegroundWindow();
	DWORD OwnerPid = 0;
	DWORD ForegroundPid = 0;
	RECT Client = {};
	if (Handle) ::GetWindowThreadProcessId(Handle, &OwnerPid);
	if (Foreground) ::GetWindowThreadProcessId(Foreground, &ForegroundPid);
	const bool bClient = Handle && ::GetClientRect(Handle, &Client);
	Json->SetBoolField(TEXT("window_foreground"), Handle && Foreground == Handle);
	Json->SetNumberField(TEXT("window_owner_pid"), OwnerPid);
	Json->SetNumberField(TEXT("foreground_owner_pid"), ForegroundPid);
	Json->SetNumberField(TEXT("client_width"), bClient ? Client.right - Client.left : 0);
	Json->SetNumberField(TEXT("client_height"), bClient ? Client.bottom - Client.top : 0);
	Json->SetStringField(TEXT("own_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Handle)));
	Json->SetStringField(TEXT("foreground_window_identity"), FString::Printf(TEXT("0x%p"), static_cast<void*>(Foreground)));
#else
	Json->SetBoolField(TEXT("window_foreground"), false);
	Json->SetNumberField(TEXT("window_owner_pid"), 0);
	Json->SetNumberField(TEXT("foreground_owner_pid"), 0);
	Json->SetNumberField(TEXT("client_width"), 0);
	Json->SetNumberField(TEXT("client_height"), 0);
	Json->SetStringField(TEXT("own_window_identity"), TEXT(""));
	Json->SetStringField(TEXT("foreground_window_identity"), TEXT(""));
#endif
	Json->SetBoolField(TEXT("screen_open"), Screen.IsValid() && Screen->IsScreenOpen());
#if WITH_DEV_AUTOMATION_TESTS
	const FRokCommanderUiSettledSample Settled = RokCommanderUiSettledProbe::Sample(Screen.Get(),
		Screen.IsValid() && Screen->IsScreenOpen() && Screen->GetSelectedCommanderIdForAudit() != 2007);
	auto SettledJson = MakeShared<FJsonObject>();
	SettledJson->SetBoolField(TEXT("ready"), Settled.bReady);
	SettledJson->SetBoolField(TEXT("structure_valid"), Settled.bStructureValid);
	SettledJson->SetNumberField(TEXT("roster_children"), Settled.RosterChildren);
	SettledJson->SetNumberField(TEXT("data_children"), Settled.DataChildren);
	SettledJson->SetNumberField(TEXT("unsettled_children"), Settled.UnsettledChildren);
	SettledJson->SetNumberField(TEXT("not_visible_children"), Settled.NotVisibleChildren);
	SettledJson->SetNumberField(TEXT("unsettled_not_visible_children"), Settled.UnsettledNotVisibleChildren);
	SettledJson->SetNumberField(TEXT("remaining_texture_compilations"), Settled.RemainingTextureCompilations);
	SettledJson->SetBoolField(TEXT("portrait_present"), Settled.bPortraitPresent);
	SettledJson->SetBoolField(TEXT("portrait_attached"), Settled.bPortraitAttached);
	SettledJson->SetNumberField(TEXT("portrait_frames"), Settled.PortraitFrames);
	SettledJson->SetNumberField(TEXT("portrait_textures"), Settled.PortraitTextures);
	SettledJson->SetNumberField(TEXT("portrait_camera_x"), Settled.PortraitCameraX);
	SettledJson->SetBoolField(TEXT("portrait_texture_property_valid"), Settled.bPortraitTexturePropertyValid);
	SettledJson->SetBoolField(TEXT("portrait_textures_rhi_ready"), Settled.bPortraitTexturesRhiReady);
	SettledJson->SetBoolField(TEXT("portrait_playing_standby"), Settled.bPortraitPlayingStandby);
	Json->SetObjectField(TEXT("commander_ui_settled"), SettledJson);
#endif
#if WITH_DEV_AUTOMATION_TESTS
	Json->SetNumberField(TEXT("selected_hero_id"), Screen.IsValid() ? Screen->GetSelectedCommanderIdForAudit() : INDEX_NONE);
	Json->SetNumberField(TEXT("selected_skill_id"), Screen.IsValid() ? Screen->GetSelectedCommanderSkillDetailIdForAudit() : INDEX_NONE);
#else
	Json->SetNumberField(TEXT("selected_hero_id"), INDEX_NONE);
	Json->SetNumberField(TEXT("selected_skill_id"), INDEX_NONE);
#endif
	Json->SetNumberField(TEXT("expected_bootstrap_hero_id"), SelectedBootstrapHeroId);
	TArray<TSharedPtr<FJsonValue>> ExpectedSkills;
	for (const int32 Id : SkillIds) ExpectedSkills.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("expected_skill_ids"), ExpectedSkills);
	Json->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	const auto Placement = Controller.IsValid() ? Controller->GetCityPlacementPreviewSnapshot() : FRokCityPlacementRuntimeSnapshot();
	Json->SetNumberField(TEXT("primary_press_count"), static_cast<double>(Placement.PrimaryPressEventCount));
	Json->SetStringField(TEXT("selected_actor"), Placement.CurrentSelection.IsValid()
		? Placement.CurrentSelection->GetPathName() : FString());
	const APawn* Pawn = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	const FTransform Camera = Pawn ? Pawn->GetActorTransform() : FTransform::Identity;
	const FVector Location = Camera.GetLocation();
	const FRotator Rotation = Camera.Rotator();
	Json->SetNumberField(TEXT("camera_x"), Location.X);
	Json->SetNumberField(TEXT("camera_y"), Location.Y);
	Json->SetNumberField(TEXT("camera_z"), Location.Z);
	Json->SetNumberField(TEXT("camera_pitch"), Rotation.Pitch);
	Json->SetNumberField(TEXT("camera_yaw"), Rotation.Yaw);
	Json->SetNumberField(TEXT("camera_roll"), Rotation.Roll);
	Json->SetNumberField(TEXT("camera_scale_x"), Camera.GetScale3D().X);
	Json->SetNumberField(TEXT("camera_scale_y"), Camera.GetScale3D().Y);
	Json->SetNumberField(TEXT("camera_scale_z"), Camera.GetScale3D().Z);
	auto Widgets = MakeShared<FJsonObject>();
	Widgets->SetObjectField(TEXT("CommanderFeatureButton"), WidgetState(Hud.IsValid() ? Hud->GetWidgetFromName(TEXT("CommanderFeatureButton")) : nullptr));
	for (const TCHAR* Name : {TEXT("FeatureModeCanvas"), TEXT("CommanderBack"), TEXT("CommanderHeadAction_0"),
		TEXT("CommanderSkillAction_0"), TEXT("CommanderSkillAction_1"), TEXT("CommanderSkillAction_2"),
		TEXT("CommanderSkillAction_3"), TEXT("CommanderSkillAction_4"), TEXT("CommanderSkillDetailFrame"),
		TEXT("CommanderSkillDetailTitle"), TEXT("CommanderSkillDetailBody"),
		TEXT("CommanderSkillDetailScroll"), TEXT("CommanderSkillDetailClose")})
		Widgets->SetObjectField(Name, WidgetState(Screen.IsValid() ? Screen->GetWidgetFromName(Name) : nullptr));
	Json->SetObjectField(TEXT("widgets"), Widgets);
	Json->SetObjectField(TEXT("skill_detail_center_patch"), SkillCenterPatchState(ReadSkillCenterPatch(
		Screen.IsValid() ? Screen->GetWidgetFromName(TEXT("CommanderSkillDetailFrame_Center")) : nullptr)));
	return Json;
}

bool URokCommanderSkillReadonlyDetailAuditSubsystem::Ready(int32 Phase) const
{
	if (!Hud.IsValid() || !Screen.IsValid() || !Modal.IsValid() || !Runtime.IsValid() || !Catalog.IsValid()
		|| !Server.IsValid() || !Server->IsLocalOnlyRun() || Server->GetHttpDispatchAttemptCount() != 0
		|| Server->GetSuppressedLocalRequestCount() != 0 || Modal->IsModalOpen()) return false;
	if (Phase == 0 || Phase == 9)
		return !Screen->IsScreenOpen() && Visible(Hud->GetWidgetFromName(TEXT("CommanderFeatureButton")));
	const UWidget* Mode = Screen->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const bool bSettled = Mode && Visible(Mode)
		&& FMath::IsNearlyEqual(Mode->GetRenderOpacity(), 1.0, .01)
		&& Mode->GetRenderTransform().Scale.Equals(FVector2D(1), .01);
	if (!Screen->IsScreenOpen() || !bSettled || !Visible(Screen->GetWidgetFromName(TEXT("CommanderBack")))) return false;
	if (!RokCommanderUiSettledProbe::Sample(Screen.Get(), Phase >= 2).bReady) return false;
#if WITH_DEV_AUTOMATION_TESTS
	const int32 HeroId = Screen->GetSelectedCommanderIdForAudit();
	const int32 SkillId = Screen->GetSelectedCommanderSkillDetailIdForAudit();
	if (Phase == 1)
		return HeroId == 2007 && SkillId == INDEX_NONE
			&& Visible(Screen->GetWidgetFromName(TEXT("CommanderHeadAction_0")));
	const bool bBootstrap = HeroId == 1006 || HeroId == 1007 || HeroId == 1009 || HeroId == 1010 || HeroId == 1012;
	const bool bRuntimeHero = Runtime->GetStarterHeroes().ContainsByPredicate(
		[HeroId](const FRokUnityRuntimeHeroState& Hero) { return Hero.HeroId == HeroId; });
	if (!bBootstrap || !bRuntimeHero) return false;
	for (int32 Index = 0; Index < 5; ++Index)
		if (!Visible(Screen->GetWidgetFromName(*FString::Printf(TEXT("CommanderSkillAction_%d"), Index)))) return false;
	if (Phase == 2)
		return HeroId == 1006 && SkillId == INDEX_NONE
			&& !Visible(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailFrame")));
	if (HeroId != SelectedBootstrapHeroId || SkillIds.Num() != 5) return false;
	if (Phase == 8)
		return SkillId == INDEX_NONE && !Visible(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailFrame")));
	if (Phase < 3 || Phase > 7) return false;
	const int32 ExpectedSkill = SkillIds[Phase - 3];
	FRokUnityCatalogHeroSkill Skill;
	FRokCommanderSkillReadonlyDetail Detail;
	if (ExpectedSkill <= 0 || SkillId != ExpectedSkill
		|| !Catalog->GetHeroSkill(ExpectedSkill, Skill)
		|| !RokCommanderSkillReadonlyDetail::Build(Skill, Detail)) return false;
	const UTextBlock* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailTitle")));
	const UTextBlock* Body = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailBody")));
	const UWidget* Scroll = Screen->GetWidgetFromName(TEXT("CommanderSkillDetailScroll"));
	return Title && Body && Scroll && Visible(Title) && Visible(Body) && Visible(Scroll)
		&& Visible(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailFrame")))
		&& ReadSkillCenterPatch(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailFrame_Center"))).Valid()
		&& Visible(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailClose")))
		&& Scroll->GetCachedGeometry().GetLocalSize().X > 0.0f
		&& Scroll->GetCachedGeometry().GetLocalSize().Y > 0.0f
		&& Title->GetText().ToString() == Detail.Title && Body->GetText().ToString() == Detail.Body;
#else
	return false;
#endif
}

void URokCommanderSkillReadonlyDetailAuditSubsystem::OnUpgradeCompleted(const FRokLocalBuildingUpgradeCompleted&)
{
	++CompletionEvents;
	Finish(TEXT("Skill detail route emitted an upgrade completion event"));
}

bool URokCommanderSkillReadonlyDetailAuditSubsystem::Click(UButton* Target, const TCHAR* Label)
{
	auto Attempt = MakeShared<FJsonObject>();
	Attempt->SetStringField(TEXT("label"), Label);
	Attempt->SetNumberField(TEXT("step"), Step);
	Attempt->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Attempt->SetStringField(TEXT("target"), Target ? Target->GetName() : FString());
	Attempt->SetBoolField(TEXT("target_exists"), Target != nullptr);
	if (!Target || !Visible(Target) || !Target->GetIsEnabled())
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target missing, disabled, or hidden"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target unavailable"));
	}
	const FGeometry Geometry = Target->GetCachedGeometry();
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D TopLeft = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D BottomRight = Geometry.LocalToAbsolute(Size);
	if (Size.X <= 0 || Size.Y <= 0)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("target has zero cached geometry"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Slate target has zero geometry"));
	}
	if (StableTarget.Get() != Target || !StableTopLeft.Equals(TopLeft, .25f)
		|| !StableBottomRight.Equals(BottomRight, .25f))
	{
		StableTarget = Target;
		StableTopLeft = TopLeft;
		StableBottomRight = BottomRight;
		StableFrame = GFrameCounter;
		ClickStableFrames = 1;
		return false;
	}
	if (GFrameCounter <= StableFrame) return false;
	StableFrame = GFrameCounter;
	if (++ClickStableFrames < 2) return false;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Center = Geometry.LocalToAbsolute(Size * .5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-30, -30));
	App.SetCursorPos(Center);
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, LastPointer, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Center, Away, TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
	LastPointer = Center;
	const FWidgetPath Path = App.LocateWindowUnderMouse(Center, App.GetInteractiveTopLevelWindows(), false, 0);
	const TSharedRef<SWidget> ButtonSlate = Target->TakeWidget();
	int32 ButtonIndex = INDEX_NONE;
	TArray<TSharedPtr<FJsonValue>> PathTypes;
	TArray<TSharedPtr<FJsonValue>> PathNodes;
	bool bUmgPath = false;
	for (int32 Index = 0; Path.IsValid() && Index < Path.Widgets.Num(); ++Index)
	{
		const SWidget* Node = &Path.Widgets[Index].Widget.Get();
		const FString Type = Node->GetTypeAsString();
		PathTypes.Add(MakeShared<FJsonValueString>(Type));
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("index"), Index);
		Entry->SetStringField(TEXT("type"), Type);
		Entry->SetStringField(TEXT("slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Node)));
		PathNodes.Add(MakeShared<FJsonValueObject>(Entry));
		bUmgPath |= Type.Contains(TEXT("SObjectWidget"));
		if (Node == &ButtonSlate.Get()) ButtonIndex = Index;
	}
	const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr;
	const bool bOwnWindow = Window.IsValid() && Path.IsValid() && &Path.GetWindow().Get() == Window.Get();
	bool bForeground = false;
	uint32 OwnerPid = 0;
	FString OwnWindowId;
	FString ForegroundWindowId;
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> Native = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	const HWND OwnHandle = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
	if (OwnHandle) { DWORD NativePid = 0; ::GetWindowThreadProcessId(OwnHandle, &NativePid); OwnerPid = NativePid; }
	OwnWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(OwnHandle));
	ForegroundWindowId = FString::Printf(TEXT("0x%p"), static_cast<void*>(::GetForegroundWindow()));
	bForeground = OwnHandle && ::GetForegroundWindow() == OwnHandle
		&& OwnerPid == FPlatformProcess::GetCurrentProcessId();
#endif
	const bool bExactButton = ButtonIndex != INDEX_NONE
		&& Path.Widgets[ButtonIndex].Widget->GetTypeAsString().Contains(TEXT("SButton"));
	Attempt->SetStringField(TEXT("method"), TEXT("ProcessMouseMoveEvent+ProcessMouseButtonDownEvent+ProcessMouseButtonUpEvent"));
	Attempt->SetObjectField(TEXT("absolute_center"), Point(Center));
	Attempt->SetObjectField(TEXT("absolute_top_left"), Point(TopLeft));
	Attempt->SetObjectField(TEXT("absolute_bottom_right"), Point(BottomRight));
	Attempt->SetObjectField(TEXT("local_size"), Point(Size));
	Attempt->SetArrayField(TEXT("hit_path_types"), PathTypes);
	Attempt->SetArrayField(TEXT("hit_path_nodes"), PathNodes);
	Attempt->SetStringField(TEXT("terminal_type"), Path.IsValid() && Path.Widgets.Num()
		? Path.Widgets.Last().Widget->GetTypeAsString() : FString());
	Attempt->SetNumberField(TEXT("exact_button_path_index"), ButtonIndex);
	Attempt->SetBoolField(TEXT("exact_button_in_path"), bExactButton);
	Attempt->SetBoolField(TEXT("umg_path_present"), bUmgPath);
	Attempt->SetStringField(TEXT("target_widget_class"), Target->GetClass()->GetPathName());
	const URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Target);
	Attempt->SetNumberField(TEXT("action_id"), Indexed ? Indexed->ActionId : 0);
	Attempt->SetStringField(TEXT("target_widget_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(Target)));
	Attempt->SetStringField(TEXT("target_slate_identity"), FString::Printf(TEXT("0x%p"), static_cast<const void*>(&ButtonSlate.Get())));
	Attempt->SetBoolField(TEXT("target_hovered"), Target->IsHovered());
	Attempt->SetBoolField(TEXT("own_game_window"), bOwnWindow);
	Attempt->SetBoolField(TEXT("foreground"), bForeground);
	Attempt->SetStringField(TEXT("own_window_identity"), OwnWindowId);
	Attempt->SetStringField(TEXT("foreground_window_identity"), ForegroundWindowId);
	Attempt->SetNumberField(TEXT("own_window_pid"), OwnerPid);
	Attempt->SetNumberField(TEXT("stable_frames"), ClickStableFrames);
	const uint64 Before = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetNumberField(TEXT("primary_before"), static_cast<double>(Before));
	Attempt->SetObjectField(TEXT("business_before"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_before"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_before"), Modal.IsValid() && Modal->IsModalOpen());
	if (!Path.IsValid() || !bExactButton || !bUmgPath || !Target->IsHovered() || !bOwnWindow || !bForeground)
	{
		Attempt->SetStringField(TEXT("failure"), TEXT("Slate path/hover/window did not prove exact UButton"));
		Attempts.Add(MakeShared<FJsonValueObject>(Attempt));
		return Expect(false, TEXT("Exact target button was not hit in foreground game window"));
	}
	TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
	const bool Down = App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),
		FPointerEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const bool Up = App.ProcessMouseButtonUpEvent(FPointerEvent(0, Center, Center,
		TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
	const uint64 After = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
	Attempt->SetBoolField(TEXT("down_handled"), Down);
	Attempt->SetBoolField(TEXT("up_handled"), Up);
	Attempt->SetNumberField(TEXT("primary_after"), static_cast<double>(After));
	Attempt->SetObjectField(TEXT("business_after"), BusinessState());
	Attempt->SetBoolField(TEXT("screen_open_after"), Screen.IsValid() && Screen->IsScreenOpen());
	Attempt->SetBoolField(TEXT("modal_open_after"), Modal.IsValid() && Modal->IsModalOpen());
	const bool bRouteSafe = Down && Up && Before == After && (!bBaselineSet || Boundary());
	Attempt->SetStringField(TEXT("failure"), bRouteSafe ? TEXT("") : TEXT("unhandled, world input, or business mutation"));
	Actions.Add(MakeShared<FJsonValueObject>(Attempt));
	StableTarget.Reset(); ClickStableFrames = 0;
	return Expect(bRouteSafe, TEXT("Slate click failed or changed baseline state"));
}

void URokCommanderSkillReadonlyDetailAuditSubsystem::Capture(const TCHAR* Phase, int32 After)
{
	if (!Expect(!Pending.IsValid() && !FScreenshotRequest::IsScreenshotRequested()
		&& (bBaselineSet ? Boundary() : Ready(0)), TEXT("Screenshot request/boundary invalid"))) return;
	Pending = MakeShared<FJsonObject>();
	Pending->SetStringField(TEXT("phase"), Phase);
	Pending->SetNumberField(TEXT("requested_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("requested_state"), State());
	PendingPath = Directory / FString::Printf(TEXT("%s.png"), Phase);
	Pending->SetStringField(TEXT("png"), PendingPath);
	NextStep = After;
	bScreenshotDone = false;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !Pending.IsValid() || bScreenshotDone) return;
	TArray<uint8> Bytes;
	if (!Expect(FFileHelper::LoadFileToArray(Bytes, *PendingPath) && Bytes.Num() > 24
		&& FMemory::Memcmp(Bytes.GetData(), "\x89PNG\r\n\x1a\n", 8) == 0
		&& FMemory::Memcmp(Bytes.GetData() + 12, "IHDR", 4) == 0
		&& PngDimension(Bytes, 16) == 1440 && PngDimension(Bytes, 20) == 810
		&& (!bBaselineSet || Boundary()), TEXT("Processed screenshot PNG/extent/business state invalid"))) return;
	Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	Pending->SetNumberField(TEXT("png_bytes"), static_cast<double>(Bytes.Num()));
	Pending->SetNumberField(TEXT("png_width"), 1440);
	Pending->SetNumberField(TEXT("png_height"), 810);
	Pending->SetNumberField(TEXT("processed_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("processed_state"), State());
	ScreenshotFrame = GFrameCounter;
	bScreenshotDone = true;
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::AdvanceCapture()
{
	if (!bScreenshotDone || GFrameCounter <= ScreenshotFrame) return;
	if (!Expect(!bBaselineSet || Boundary(), TEXT("Post-screenshot business state changed"))) return;
	Pending->SetNumberField(TEXT("next_frame"), static_cast<double>(GFrameCounter));
	Pending->SetObjectField(TEXT("next_frame_state"), State());
	Captures.Add(MakeShared<FJsonValueObject>(Pending));
	Pending.Reset();
	Save(false);
	Enter(NextStep);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::Enter(int32 NewStep)
{
	Step = NewStep; StepSince = Elapsed; StableFrames = 0; ClickStableFrames = 0; StableTarget.Reset();
}
bool URokCommanderSkillReadonlyDetailAuditSubsystem::Save(bool Complete) const
{
	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("schema"), TEXT("rok.commander_skill_readonly_detail.runtime_audit.v1"));
	Json->SetBoolField(TEXT("complete"), Complete);
	Json->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty() && Captures.Num() == 10
		&& Actions.Num() == 9 && Attempts.IsEmpty() && CompletionEvents == 0 && bBaselineSet && Boundary());
	Json->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Json->SetStringField(TEXT("engine_dir"), FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
	Json->SetNumberField(TEXT("engine_major"), FEngineVersion::Current().GetMajor());
	Json->SetNumberField(TEXT("engine_minor"), FEngineVersion::Current().GetMinor());
	Json->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());
	Json->SetStringField(TEXT("scope"), TEXT("One actual local bootstrap commander: five catalog skill detail cards through real HUD and Slate; read-only config preview only"));
	Json->SetStringField(TEXT("baseline_phase"), TEXT("00_city_hud"));
	Json->SetBoolField(TEXT("world_time_advanced_by_audit"), false);
	Json->SetBoolField(TEXT("source_pixel_parity_claim"), false);
	Json->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
	Json->SetBoolField(TEXT("full_migration_accepted"), false);
	Json->SetBoolField(TEXT("full_commander_skill_tree_accepted"), false);
	Json->SetBoolField(TEXT("other_bootstrap_heroes_all_skills_run"), false);
	Json->SetNumberField(TEXT("selected_bootstrap_hero_id"), SelectedBootstrapHeroId);
	TArray<TSharedPtr<FJsonValue>> SelectedSkills;
	for (const int32 Id : SkillIds) SelectedSkills.Add(MakeShared<FJsonValueNumber>(Id));
	Json->SetArrayField(TEXT("skill_ids"), SelectedSkills);
	Json->SetNumberField(TEXT("completion_events"), CompletionEvents);
	Json->SetObjectField(TEXT("inputs_sha256"), Inputs.IsValid() ? Inputs.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetArrayField(TEXT("actions"), Actions);
	Json->SetArrayField(TEXT("attempts"), Attempts);
	Json->SetArrayField(TEXT("captures"), Captures);
	Json->SetObjectField(TEXT("baseline_business"), BaselineBusinessJson.IsValid()
		? BaselineBusinessJson.ToSharedRef() : MakeShared<FJsonObject>());
	Json->SetObjectField(TEXT("final_state"), State());
	TArray<TSharedPtr<FJsonValue>> Errors;
	for (const FString& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Json->SetArrayField(TEXT("failures"), Errors);
	return FFileHelper::SaveStringToFile(Encode(Json), *(Directory / TEXT("manifest.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::Finish(const FString& Error)
{
	if (bFinished) return;
	if (!Error.IsEmpty()) Failures.Add(Error);
	bFinished = true;
	if (bPointerSaved && FSlateApplication::IsInitialized()) FSlateApplication::Get().SetCursorPos(OriginalPointer);
	const bool Complete = Failures.IsEmpty() && bBaselineSet && Captures.Num() == 10
		&& Actions.Num() == 9 && Attempts.IsEmpty() && CompletionEvents == 0 && Boundary();
	if (!Complete && Failures.IsEmpty()) Failures.Add(TEXT("Commander skill read-only audit incomplete"));
	Save(Complete);
	UE_LOG(LogTemp, Display, TEXT("Rok commander skill detail audit %s: %s"),
		Complete ? TEXT("passed") : TEXT("failed"), *Directory);
	if (!Complete) UE_LOG(LogTemp, Error, TEXT("Rok commander skill detail audit: %s"), *Failures.Last());
	FPlatformMisc::RequestExitWithStatus(false, Complete ? 0 : 1);
}
void URokCommanderSkillReadonlyDetailAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !Identity()) return;
	Elapsed += FMath::Max(0.0f, DeltaTime);
	if (Elapsed < 2.0) return;
	if (Pending.IsValid())
	{
		if (Elapsed - StepSince > 30.0) Finish(TEXT("Screenshot callback timeout"));
		else AdvanceCapture();
		return;
	}
	if (Elapsed - StepSince > 35.0) { Finish(TEXT("Audit step timeout")); return; }
	auto Stable = [this](int32 Phase)
	{
		StableFrames = Ready(Phase) ? StableFrames + 1 : 0;
		return StableFrames >= 2;
	};
	switch (Step)
	{
	case 0: if (!bBegun) bBegun = Begin(); if (bBegun && PrepareOwnWindowFocus()) Enter(1); break;
	case 1:
		if (Stable(0))
		{
			FRokLocalConstructionTicket Ticket;
			int32 PendingCount = -1;
#if WITH_DEV_AUTOMATION_TESTS
			PendingCount = Buildings->GetPendingCompletedUpgradeCountForTesting();
#endif
			if (!Expect(Buildings->GetActiveUpgradeCount() == 0 && PendingCount == 0
				&& Buildings->GetActiveLocalConstructionCount() == 0
				&& !Buildings->GetLocalConstructionTicket(1, Ticket),
				TEXT("Skill audit requires idle local business queues"))) return;
			BaselineBusinessJson = BusinessState();
			BaselineBusiness = Encode(BaselineBusinessJson.ToSharedRef());
			BaselineCamera = Controller->GetPawn()->GetActorTransform();
			BaselinePrimary = Controller->GetCityPlacementPreviewSnapshot().PrimaryPressEventCount;
			bBaselineSet = true;
			Capture(TEXT("00_city_hud"), 2);
		}
		break;
	case 2: if (Boundary() && Click(Cast<UButton>(Hud->GetWidgetFromName(TEXT("CommanderFeatureButton"))), TEXT("open_commander_from_hud"))) Enter(3); break;
	case 3: if (Boundary() && Stable(1)) Capture(TEXT("01_commander_default_2007"), 4); break;
	case 4: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderHeadAction_0"))), TEXT("select_local_bootstrap_hero"))) Enter(5); break;
	case 5:
		if (Boundary() && Stable(2))
		{
#if WITH_DEV_AUTOMATION_TESTS
			SelectedBootstrapHeroId = Screen->GetSelectedCommanderIdForAudit();
			FRokUnityCatalogHero Hero;
			if (!Expect(SelectedBootstrapHeroId == 1006 && Catalog->GetHero(SelectedBootstrapHeroId, Hero)
				&& Hero.SkillIds.Num() == 5, TEXT("Expected first local hero and five catalog skills"))) return;
			SkillIds = Hero.SkillIds;
			TSet<int32> Unique;
			for (const int32 Id : SkillIds)
			{
				FRokUnityCatalogHeroSkill Skill;
				FRokCommanderSkillReadonlyDetail Detail;
				if (!Expect(Id > 0 && !Unique.Contains(Id) && Catalog->GetHeroSkill(Id, Skill)
					&& RokCommanderSkillReadonlyDetail::Build(Skill, Detail),
					TEXT("Five distinct complete skill config projections required"))) return;
				Unique.Add(Id);
			}
			Capture(TEXT("02_bootstrap_hero_1006"), 6);
#else
			Finish(TEXT("Skill audit unavailable outside dev automation build"));
#endif
		}
		break;
	case 6: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillAction_0"))), TEXT("open_skill_0"))) Enter(7); break;
	case 7: if (Boundary() && Stable(3)) Capture(TEXT("03_skill_0_detail"), 8); break;
	case 8: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillAction_1"))), TEXT("open_skill_1"))) Enter(9); break;
	case 9: if (Boundary() && Stable(4)) Capture(TEXT("04_skill_1_detail"), 10); break;
	case 10: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillAction_2"))), TEXT("open_skill_2"))) Enter(11); break;
	case 11: if (Boundary() && Stable(5)) Capture(TEXT("05_skill_2_detail"), 12); break;
	case 12: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillAction_3"))), TEXT("open_skill_3"))) Enter(13); break;
	case 13: if (Boundary() && Stable(6)) Capture(TEXT("06_skill_3_detail"), 14); break;
	case 14: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillAction_4"))), TEXT("open_skill_4"))) Enter(15); break;
	case 15: if (Boundary() && Stable(7)) Capture(TEXT("07_skill_4_detail"), 16); break;
	case 16: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderSkillDetailClose"))), TEXT("close_skill_detail"))) Enter(17); break;
	case 17: if (Boundary() && Stable(8)) Capture(TEXT("08_detail_closed"), 18); break;
	case 18: if (Boundary() && Click(Cast<UButton>(Screen->GetWidgetFromName(TEXT("CommanderBack"))), TEXT("return_to_hud"))) Enter(19); break;
	case 19: if (Boundary() && Stable(9)) Capture(TEXT("09_hud_return"), 20); break;
	case 20: Finish(); break;
	default: Finish(TEXT("Invalid audit state")); break;
	}
}
