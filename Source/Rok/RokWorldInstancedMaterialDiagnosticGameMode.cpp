#include "RokWorldInstancedMaterialDiagnosticGameMode.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "PrimitiveComponentId.h"
#include "SceneInterface.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameters.h"
#include "Materials/Material.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Containers/StringConv.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapPlayerController.h"
#include "RokWorldMapStreamingActor.h"
#include "RokWorldMapRuntimeDataAsset.h"
#include "RokServerSubsystem.h"
#include "RokTrainArmyMeshData.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

struct FRokWorldHismProxyProbeResult
{
	// Written only by the queued render command and read after its fence retires.
	TMap<uint32, uint8> PresenceByComponentId;
};

// BeginRenderViewFamily is reached only through a submitted scene view family.
// OnViewportRendered alone also fires when GameViewportClient skips world rendering.
class FRokWorldDiagnosticViewExtension final : public FWorldSceneViewExtension
{
public:
	FRokWorldDiagnosticViewExtension(const FAutoRegister& AutoReg, UWorld* World,
		ARokWorldInstancedMaterialDiagnosticGameMode* InOwner)
		: FWorldSceneViewExtension(AutoReg, World), Owner(InOwner)
	{
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& ViewFamily) override
	{
		if (Owner.IsValid())
		{
			Owner->ObserveWorldViewSubmission(ViewFamily);
		}
	}

private:
	TWeakObjectPtr<ARokWorldInstancedMaterialDiagnosticGameMode> Owner;
};

namespace
{
constexpr int32 InitialInstances = 848;
constexpr int32 MovedInstances = 1688;
constexpr float SourceCameraAspectRatio = 16.0f / 10.0f;
constexpr float InitialDxf = 288.31601f;
// Unity CameraParamLod.showbuild: dist=33.333, vertical FOV=30, Dxf=1000.
// Opt-in tree attribution alone uses this source key so the complete tree
// bounds fit; the unflagged V10 camera/phase path retains InitialDxf.
constexpr float CoverageDxf = 1000.0f;
constexpr float LodOneDxf = 2000.0f;
constexpr float BlockedLodTwoDxf = 3001.0f;
const FVector2D InitialFocus(6876.0, 942.0);
const FVector2D MovedFocus(5796.0, 942.0);
// Derived from the pinned source instance at local (-532850,-1300,50):
// Projection.UnityToUe((5810.3,939.4)) == (532850,-1300,0).
const FVector2D CoverageFocus(5810.3, 939.4);
constexpr TCHAR CoverageComponentName[] = TEXT("WorldMapChunk_16_2_Batch_193_Gen_0");
constexpr TCHAR CoverageMeshPath[] = TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Grove_co_tree_04_adorning.SM_RokWorld_Grove_co_tree_04_adorning");
constexpr TCHAR CoverageSourceMaterial[] = TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_map_tree_nomask_1.M_WorldSource_map_tree_nomask_1");
constexpr TCHAR CoverageTargetMaterial[] = TEXT("/Game/RokDiagnostics/WorldStreaming/InstancedMaterialVariantsV1/MIV1_M_WorldSource_map_tree_nomask_1.MIV1_M_WorldSource_map_tree_nomask_1");
constexpr int32 CoverageRgbThreshold = 16;
constexpr int32 CoverageMinConnectedPixels = 64;
constexpr int32 CoverageMaxNoiseRgb = 2;

FString ExactU64(uint64 Value)
{
	return FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Value));
}

bool FileSha256(const FString& RelativePath, FString& OutSha)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes,
		*FPaths::Combine(FPaths::ProjectDir(), RelativePath)) || Bytes.IsEmpty())
	{
		return false;
	}
	OutSha = RokTrainArmyMesh::Sha256(Bytes);
	return OutSha.Len() == 64;
}

TArray<TSharedPtr<FJsonValue>> MatrixNumbers(const FMatrix& Matrix)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			Result.Add(MakeShared<FJsonValueNumber>(Matrix.M[Row][Column]));
		}
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> RectNumbers(const FIntRect& Rect)
{
	return {MakeShared<FJsonValueNumber>(Rect.Min.X), MakeShared<FJsonValueNumber>(Rect.Min.Y),
		MakeShared<FJsonValueNumber>(Rect.Max.X), MakeShared<FJsonValueNumber>(Rect.Max.Y)};
}

struct FCoveragePixelDifference
{
	int32 MaxRgbDiff = 0;
	int32 ThresholdPixels = 0;
	int32 LargestConnectedPixels = 0;
};

FCoveragePixelDifference CompareCoveragePixels(const TArray<uint8>& A,
	const TArray<uint8>& B, const FIntRect& Roi, int32 Threshold)
{
	FCoveragePixelDifference Result;
	if (A.Num() != 1440 * 810 * 4 || B.Num() != A.Num()
		|| Roi.Min.X < 0 || Roi.Min.Y < 0 || Roi.Max.X > 1440
		|| Roi.Max.Y > 810 || Roi.Width() <= 0 || Roi.Height() <= 0)
	{
		return Result;
	}
	const int32 Width = Roi.Width();
	const int32 Height = Roi.Height();
	TArray<uint8> Mask;
	Mask.Init(0, Width * Height);
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelByte = ((Y + Roi.Min.Y) * 1440 + X + Roi.Min.X) * 4;
			const int32 Diff = FMath::Max(FMath::Abs(int32(A[PixelByte]) - int32(B[PixelByte])),
				FMath::Max(FMath::Abs(int32(A[PixelByte + 1]) - int32(B[PixelByte + 1])),
					FMath::Abs(int32(A[PixelByte + 2]) - int32(B[PixelByte + 2]))));
			Result.MaxRgbDiff = FMath::Max(Result.MaxRgbDiff, Diff);
			Mask[Y * Width + X] = Diff >= Threshold ? 1 : 0;
			Result.ThresholdPixels += Mask[Y * Width + X];
		}
	}
	TArray<int32> Queue;
	for (int32 Seed = 0; Seed < Mask.Num(); ++Seed)
	{
		if (Mask[Seed] != 1) continue;
		Queue.Reset();
		Queue.Add(Seed);
		Mask[Seed] = 2;
		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 Pixel = Queue[Head++];
			const int32 PX = Pixel % Width;
			const int32 PY = Pixel / Width;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					if (DX == 0 && DY == 0) continue;
					const int32 NX = PX + DX;
					const int32 NY = PY + DY;
					if (NX < 0 || NY < 0 || NX >= Width || NY >= Height) continue;
					const int32 Neighbor = NY * Width + NX;
					if (Mask[Neighbor] == 1)
					{
						Mask[Neighbor] = 2;
						Queue.Add(Neighbor);
					}
				}
			}
		}
		Result.LargestConnectedPixels = FMath::Max(Result.LargestConnectedPixels, Queue.Num());
	}
	return Result;
}

bool ProjectCoverageInstanceBounds(const UHierarchicalInstancedStaticMeshComponent* Component,
	int32 InstanceIndex, const FMinimalViewInfo& CameraView, const FIntRect& ViewRect,
	FIntRect& OutProjectedRect, FTransform& OutInstanceWorld, bool& bOutFullyBehind)
{
	bOutFullyBehind = false;
	OutProjectedRect = FIntRect();
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	if (!Mesh || !Component->GetInstanceTransform(InstanceIndex, OutInstanceWorld, true)
		|| CameraView.ProjectionMode != ECameraProjectionMode::Perspective
		|| CameraView.FOV <= 0.0f || CameraView.FOV >= 180.0f
		|| CameraView.AspectRatio <= 0.0f || ViewRect.Width() <= 0
		|| ViewRect.Height() <= 0)
	{
		return false;
	}
	const double TanH = FMath::Tan(FMath::DegreesToRadians(CameraView.FOV * 0.5));
	const double TanV = TanH / CameraView.AspectRatio;
	if (!FMath::IsFinite(TanH) || !FMath::IsFinite(TanV) || TanH <= 0 || TanV <= 0)
	{
		return false;
	}
	const FQuat Rotation = CameraView.Rotation.Quaternion();
	const FVector Forward = Rotation.GetAxisX();
	const FVector Right = Rotation.GetAxisY();
	const FVector Up = Rotation.GetAxisZ();
	const FBox LocalBox = Mesh->GetBoundingBox();
	if (!LocalBox.IsValid) return false;
	double MinX = TNumericLimits<double>::Max(), MinY = TNumericLimits<double>::Max();
	double MaxX = -TNumericLimits<double>::Max(), MaxY = -TNumericLimits<double>::Max();
	int32 BehindCorners = 0;
	for (int32 X = 0; X < 2; ++X)
	{
		for (int32 Y = 0; Y < 2; ++Y)
		{
			for (int32 Z = 0; Z < 2; ++Z)
			{
				const FVector Point = OutInstanceWorld.TransformPosition(FVector(
					X ? LocalBox.Max.X : LocalBox.Min.X,
					Y ? LocalBox.Max.Y : LocalBox.Min.Y,
					Z ? LocalBox.Max.Z : LocalBox.Min.Z));
				const FVector Delta = Point - CameraView.Location;
				const double Depth = FVector::DotProduct(Delta, Forward);
				if (!FMath::IsFinite(Depth)) return false;
				if (Depth <= 1.0)
				{
					++BehindCorners;
					continue;
				}
				const double PixelX = ViewRect.Min.X + (1.0 + FVector::DotProduct(Delta, Right)
					/ (Depth * TanH)) * 0.5 * ViewRect.Width();
				const double PixelY = ViewRect.Min.Y + (1.0 - FVector::DotProduct(Delta, Up)
					/ (Depth * TanV)) * 0.5 * ViewRect.Height();
				if (!FMath::IsFinite(PixelX) || !FMath::IsFinite(PixelY)) return false;
				MinX = FMath::Min(MinX, PixelX); MaxX = FMath::Max(MaxX, PixelX);
				MinY = FMath::Min(MinY, PixelY); MaxY = FMath::Max(MaxY, PixelY);
			}
		}
	}
	if (BehindCorners == 8)
	{
		bOutFullyBehind = true;
		return true;
	}
	if (BehindCorners != 0) return false; // No near-plane clipping approximation.
	OutProjectedRect = FIntRect(FMath::FloorToInt(MinX), FMath::FloorToInt(MinY),
		FMath::CeilToInt(MaxX), FMath::CeilToInt(MaxY));
	return OutProjectedRect.Width() > 0 && OutProjectedRect.Height() > 0;
}

TArray<TSharedPtr<FJsonValue>> TransformNumbers(const FTransform& Value)
{
	const FVector Position = Value.GetTranslation();
	const FQuat Rotation = Value.GetRotation();
	const FVector Scale = Value.GetScale3D();
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const double Number : {Position.X, Position.Y, Position.Z,
		Rotation.X, Rotation.Y, Rotation.Z, Rotation.W,
		Scale.X, Scale.Y, Scale.Z})
	{
		Result.Add(MakeShared<FJsonValueNumber>(Number));
	}
	return Result;
}

TArray<TSharedPtr<FJsonValue>> Vector2Numbers(const FVector2D& Value)
{
	return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y)};
}

TArray<TSharedPtr<FJsonValue>> Vector3Numbers(const FVector& Value)
{
	return {MakeShared<FJsonValueNumber>(Value.X),
		MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}

TSharedPtr<FJsonValue> JsonString(const FString& Value)
{
	return MakeShared<FJsonValueString>(Value);
}

TSharedPtr<FJsonValue> JsonNumber(double Value)
{
	return MakeShared<FJsonValueNumber>(Value);
}

FString SerializeState(const TSharedPtr<FJsonObject>& State)
{
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(State.ToSharedRef(), Writer);
	return Text;
}

FString FixedProbeShaUtf8(const FString& Value)
{
	const FTCHARToUTF8 Utf8(*Value);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	return RokTrainArmyMesh::Sha256(Bytes);
}

FString FixedProbeReflectedText(const UStruct* Struct, const void* Value)
{
	// Property text follows reflection order and values, never raw struct padding.
	FString Text;
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FString PropertyText;
		It->ExportTextItem_Direct(PropertyText,
			It->ContainerPtrToValuePtr<void>(Value), nullptr, nullptr, PPF_None);
		Text += It->GetName();
		Text += TEXT("=");
		Text += PropertyText;
		Text += TEXT("\n");
	}
	return Text;
}

FString FixedProbeSubmittedComparable(const TSharedPtr<FJsonObject>& Submitted)
{
	TSharedPtr<FJsonObject> Comparable = MakeShared<FJsonObject>();
	Comparable->SetArrayField(TEXT("view_matrix"), Submitted->GetArrayField(TEXT("view_matrix")));
	Comparable->SetArrayField(TEXT("projection_no_aa_matrix"),
		Submitted->GetArrayField(TEXT("projection_no_aa_matrix")));
	return SerializeState(Comparable);
}

TSharedPtr<FJsonObject> FixedProbeTextureFact(UTexture* Texture)
{
	TSharedPtr<FJsonObject> Fact = MakeShared<FJsonObject>();
	Fact->SetStringField(TEXT("path"), Texture ? Texture->GetPathName() : TEXT(""));
	Fact->SetStringField(TEXT("class"), Texture ? Texture->GetClass()->GetName() : TEXT(""));
	const UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	Fact->SetBoolField(TEXT("is_texture_2d"), Texture2D != nullptr);
	if (Texture2D)
	{
		Fact->SetNumberField(TEXT("size_x"), Texture2D->GetSizeX());
		Fact->SetNumberField(TEXT("size_y"), Texture2D->GetSizeY());
		Fact->SetNumberField(TEXT("total_mips"), Texture2D->GetNumMips());
		Fact->SetNumberField(TEXT("resident_mips_api"), Texture2D->GetNumResidentMips());
		Fact->SetBoolField(TEXT("resource_present"), Texture2D->GetResource() != nullptr);
		Fact->SetBoolField(TEXT("virtual_texture_mips_are_abstract"),
			Texture2D->IsCurrentlyVirtualTextured());
	}
	return Fact;
}

uint32 PngU32(const TArray<uint8>& Bytes, int32 Offset){
	return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset + 1]) << 16)
		| (uint32(Bytes[Offset + 2]) << 8) | uint32(Bytes[Offset + 3]);
}

bool ReadViewportPng(const FString& Path, TArray<uint8>& OutFileBytes,
	TArray<uint8>& OutRgbaPixels, int64& OutNonblackRgbPixels)
{
	constexpr uint8 PngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(OutFileBytes, *Path)
		|| OutFileBytes.Num() < 33
		|| FMemory::Memcmp(OutFileBytes.GetData(), PngSignature, 8) != 0
		|| PngU32(OutFileBytes, 8) != 13
		|| FMemory::Memcmp(OutFileBytes.GetData() + 12, "IHDR", 4) != 0
		|| PngU32(OutFileBytes, 16) != 1440 || PngU32(OutFileBytes, 20) != 810
		|| OutFileBytes[24] != 8 || OutFileBytes[25] != 6
		|| OutFileBytes[26] != 0 || OutFileBytes[27] != 0 || OutFileBytes[28] != 0)
	{
		return false;
	}
	FImage Decoded;
	if (!FImageUtils::DecompressImage(OutFileBytes.GetData(), OutFileBytes.Num(), Decoded)
		|| Decoded.SizeX != 1440 || Decoded.SizeY != 810)
	{
		return false;
	}
	FImage Bgra;
	Decoded.CopyTo(Bgra, ERawImageFormat::BGRA8, Decoded.GammaSpace);
	OutRgbaPixels.Reset();
	OutRgbaPixels.Reserve(1440 * 810 * 4);
	OutNonblackRgbPixels = 0;
	for (const FColor& Pixel : Bgra.AsBGRA8())
	{
		if (Pixel.A != 255)
		{
			return false;
		}
		OutRgbaPixels.Add(Pixel.R);
		OutRgbaPixels.Add(Pixel.G);
		OutRgbaPixels.Add(Pixel.B);
		OutRgbaPixels.Add(Pixel.A);
		OutNonblackRgbPixels += Pixel.R > 2 || Pixel.G > 2 || Pixel.B > 2;
	}
	return OutRgbaPixels.Num() == 1440 * 810 * 4;
}

constexpr TCHAR AliasSchema[] = TEXT("rok.world_instanced_material_alias.v7");
constexpr TCHAR AliasTemplateSha[] = TEXT("428f287389c01f71e3da45aa54b9be520c96a4d9b020aaac25a509b9f3232480");
constexpr TCHAR ActualAliasSha[] = TEXT("0a78887c5326cdf994c491cbac3900db2bbd752be2d27289c1afca9fb49fe2fd");
constexpr TCHAR VariantsPlanSha[] = TEXT("21b714660c5cd0c3f0577b3c553f4c28f687b7bbd76e030045fd524156690aae");
constexpr TCHAR RiverContractSha[] = TEXT("c7fac29e53c4f6180b8557d00f27a206a8596b6d9f006f9b9dd7bd8529b70709");
constexpr TCHAR BridgePlanSha[] = TEXT("3825d7f164a545ba38dd55f59347edf1176bd2ebcbd5829807eb4593435370d4");
constexpr TCHAR VariantsReadyPath[] = TEXT("Saved/PendingWorldMaterialRootNativeBridgeV2/ready_for_root_review.json");
constexpr TCHAR RiverReadyPath[] = TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/ready_for_root_review.json");
constexpr TCHAR BridgeReadyPath[] = TEXT("Saved/PendingWorldBridgeInstancedMaterialV4/ready_for_root_review.json");
constexpr TCHAR VariantsReadySha[] = TEXT("f2adb9a2a5254079e83bf2088abdde8b7d56363062ce420c14df747402c98861");
constexpr TCHAR RiverReadySha[] = TEXT("f2b5ce4a29994d906cbf9ab12c92b1db47a09b3f7f5769bf251252e3431fa01f");
constexpr TCHAR BridgeReadySha[] = TEXT("4d0db09de14952fd41e7527a49c1d9482173539058a7625da6a1302629bba393");
constexpr TCHAR VariantsReviewSha[] = TEXT("16bcca3124b1d23d73f4fcd985edb46376447bff0fffb80f49fe36bcb72b625e");
constexpr TCHAR RiverV2HistoricalReviewSha[] = TEXT("4ae7d8437616c7e59f556bfc11b0e9b83b925d7956f7020f49e9645542d45638");
constexpr TCHAR RiverV2TextureSha[] = TEXT("b4ab70d2ca3e66ecd8db5cb070c2ceaf76368b4a7558d64362e58cde656d4385");
constexpr TCHAR RiverV2FailedMaterialSha[] = TEXT("687a65a28831cae49c54c12c2d0ac72fb5e7438d589184191a3c7ea34687d61c");
constexpr TCHAR RiverV3CreatorSha[] = TEXT("6cff8120a7ee82d533386393dba70684ef2a379463364db0a854336e0370d2dc");
constexpr TCHAR RiverV3VerifierSha[] = TEXT("32a5dbd292149bafae8ad34aba03d7257ea88c61d57b00b18effa35da98ff71a");
constexpr TCHAR RiverV3BlockedReviewSha[] = TEXT("e7773ec4abfdacf5e89abc43002fbf0a9cac82af08f242fe22be862d62f48d46");
constexpr TCHAR RiverSupplementHelperSha[] = TEXT("d3db721642bc2cc5945fe3bb454fe3bc55a84f3330e5c74ad0f94c713a1b68bf");
constexpr TCHAR RiverSupplementReviewSha[] = TEXT("5f672de51f363a3ef7415922bb1697cfd6232e6bd4051c5b9182950be63cd3c1");
constexpr TCHAR RiverSupplementResultSha[] = TEXT("455a01f04f1e2ecd8e3422d5e2b22e43d8d1f817603a3dc6fae99bf1812fc482");
constexpr TCHAR RiverSupplementBeforeSha[] = TEXT("b67bf5eb5df7e57e34b3b4b2347eca0a053d19488eb09f7bdf20ffd45dbf014d");
constexpr TCHAR RiverSupplementAfterSha[] = TEXT("b7d1e41db909df46d244b105fc98f1abc0e5d1ccd0c73069ce0ffd5e57490eb1");
constexpr TCHAR RiverSupplementTerminalSha[] = TEXT("3401d41b6329c7c1848b0919bde69ccdab9657a0662b285558b849aa6e3380c1");
constexpr TCHAR RiverSupplementPreflightReceiptSha[] = TEXT("bb8b0d45038cac6864b6404f2fce72527efe71e04593bf1ea1487c1044b623ba");
constexpr TCHAR RiverSupplementMaterialSha[] = TEXT("880c36b5ea49bf56cc029b10fc88da730046ee2fa05ff8a95e544892eb8abac8");
constexpr TCHAR RiverSupplementLogSha[] = TEXT("18389e8c3bf5127d727952a83dc6de537d128ca3449c365cb0746abc2d552834");
constexpr TCHAR RiverSupplementRelative[] = TEXT("Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/RiverV3RootSupplementV2_20261009T192113498016Z/");
constexpr TCHAR BridgeReviewSha[] = TEXT("590a47d5e436964f5210eced3b713079e942db27ed52b3429848d72516e67027");
constexpr TCHAR VariantsGraphModuleSha[] = TEXT("3d95ecd290dd7eab03b82176f9262dee605b1c65ff138fde2c2de9a0aa524fcc");
constexpr TCHAR OriginalContentPinsetSha[] = TEXT("07161b934c07ffcb221d6f330eb9ab11244578ead7c15a97cb7b70c1b97a2e7a");
constexpr TCHAR AliasRelativePath[] = TEXT("Saved/PendingWorldInstancedVisibilityDiagnosticV7/actual_alias_v7.json");
constexpr TCHAR BridgeSource[] = TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldSource_build_bridge_A.M_WorldSource_build_bridge_A");
constexpr TCHAR BridgeSourceSha[] = TEXT("07b79a8751ed8cc4a2567010f229b2db177420d6170c40151963b7d8953305d9");
constexpr int64 BridgeSourceBytes = 6806;

bool JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, FString& Out)
{
	return Object.IsValid() && Object->TryGetStringField(Key, Out);
}

bool JsonIntegerField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, int64& Out)
{
	double Value = 0.0;
	if (!Object.IsValid() || !Object->TryGetNumberField(Key, Value)
		|| !FMath::IsFinite(Value) || Value < 0.0 || Value > 9007199254740991.0
		|| FMath::FloorToDouble(Value) != Value)
	{
		return false;
	}
	Out = static_cast<int64>(Value);
	return true;
}

bool PinnedJson(const FString& Path, const FString& ExpectedSha, int64 ExpectedBytes,
	TSharedPtr<FJsonObject>& Out)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)
		|| (ExpectedBytes >= 0 && Bytes.Num() != ExpectedBytes)
		|| RokTrainArmyMesh::Sha256(Bytes) != ExpectedSha)
	{
		return false;
	}
	FString Text;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
}

TSharedPtr<FJsonObject> JsonObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	const TSharedPtr<FJsonObject>* Result = nullptr;
	return Object.IsValid() && Object->TryGetObjectField(Key, Result) && Result
		? *Result : nullptr;
}

const TArray<TSharedPtr<FJsonValue>>* JsonArrayField(
	const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
{
	const TArray<TSharedPtr<FJsonValue>>* Result = nullptr;
	return Object.IsValid() && Object->TryGetArrayField(Key, Result) ? Result : nullptr;
}

bool PackageFileForObject(const FString& ObjectPath, FString& Out)
{
	FString Package, Object;
	if (!ObjectPath.Split(TEXT("."), &Package, &Object)
		|| !Package.StartsWith(TEXT("/Game/"))
		|| Object != FPackageName::GetShortName(Package)
		|| Package.Contains(TEXT("..")))
	{
		return false;
	}
	Out = FPaths::Combine(FPaths::ProjectContentDir(), Package.Mid(6) + TEXT(".uasset"));
	Out = FPaths::ConvertRelativePathToFull(Out);
	FPaths::NormalizeFilename(Out);
	return true;
}

bool FileMatches(const FString& Path, const FString& Digest, int64 Count)
{
	TArray<uint8> Bytes;
	return Count > 0 && FFileHelper::LoadFileToArray(Bytes, *Path)
		&& Bytes.Num() == Count && RokTrainArmyMesh::Sha256(Bytes) == Digest;
}

bool IsHexSha(const FString& Value)
{
	if (Value.Len() != 64)
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!FChar::IsHexDigit(Character))
		{
			return false;
		}
	}
	return true;
}

bool NativeMaterialReady(const UMaterial* Material)
{
	if (!Material || !Material->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes))
	{
		return false;
	}
	const FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
	return Resource && Resource->GetCompileErrors().IsEmpty()
		&& Resource->GetGameThreadShaderMap() != nullptr;
}

FString NormalPath(const FString& Value)
{
	FString Result = FPaths::ConvertRelativePathToFull(Value);
	FPaths::NormalizeFilename(Result);
	FPaths::CollapseRelativeDirectories(Result);
	FPaths::NormalizeDirectoryName(Result);
	return Result;
}

bool PinnedRows(const TSharedPtr<FJsonObject>& Baseline, const TCHAR* Key,
	TMap<FString, FString>& Out)
{
	const auto* Items = JsonArrayField(Baseline, Key);
	if (!Items) return false;
	for (const auto& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Path, Digest;
		int64 Bytes = 0;
		if (!Item.IsValid() || Item->Values.Num() != 3
			|| !JsonStringField(Item, TEXT("path"), Path)
			|| !JsonStringField(Item, TEXT("sha256"), Digest)
			|| !JsonIntegerField(Item, TEXT("bytes"), Bytes)
			|| FPaths::IsRelative(Path) || !IsHexSha(Digest) || Bytes <= 0)
			return false;
		Path = NormalPath(Path);
		if (Out.Contains(Path)) return false;
		Out.Add(Path, FString::Printf(TEXT("%s:%lld"), *Digest, Bytes));
	}
	return Out.Num() == Items->Num();
}

bool SamePinnedRows(const TMap<FString, FString>& Before, const TMap<FString, FString>& After)
{
	if (Before.Num() != After.Num()) return false;
	for (const auto& Row : Before)
		if (After.FindRef(Row.Key) != Row.Value) return false;
	return true;
}

bool ValidateRiverRootSupplement(const FString& Project, const FString& TerminalPath,
	const FString& MaterialFile, FString& OutReason)
{
	const FString Batch = FPaths::Combine(Project, RiverSupplementRelative);
	const FString ResultPath = FPaths::Combine(Batch, TEXT("result.json"));
	const FString SourceEditorLogPath = FPaths::Combine(Batch, TEXT("editor.log"));
	const FString BeforePath = FPaths::Combine(Project,
		TEXT("Saved/RokUnityMigration/BatchReview20261010CommanderSkillReadonlyDetail/SkillBuildV5_20261009T191525Z/postbuild_baseline.json"));
	const FString AfterPath = FPaths::Combine(Batch, TEXT("next_inputpins_v1.json"));
	const FString HelperPath = FPaths::Combine(Project,
		TEXT("Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/RootRiverV3SupplementalExecutionV2.py"));
	const FString ReviewPath = FPaths::Combine(Project,
		TEXT("Saved/RokUnityMigration/BatchReview20261010WorldNativeMaterialBridge/RootRiverV3EnvelopeReviewV1/review.json"));
	const FString BlockedPath = FPaths::Combine(Project,
		TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/IndependentReviewV1/review.json"));
	const FString PreflightReceiptPath = FPaths::Combine(FPaths::GetPath(TerminalPath), TEXT("receipt.json"));
	TSharedPtr<FJsonObject> Result, Before, After;
	if (!FileMatches(HelperPath, RiverSupplementHelperSha, 11707)
		|| !FileMatches(ReviewPath, RiverSupplementReviewSha, 3890)
		|| !FileMatches(BlockedPath, RiverV3BlockedReviewSha, 2381)
		|| !FileMatches(PreflightReceiptPath, RiverSupplementPreflightReceiptSha, 1593)
		|| !PinnedJson(ResultPath, RiverSupplementResultSha, 4356, Result)
		|| !FileMatches(SourceEditorLogPath, RiverSupplementLogSha, 280348)
		|| !PinnedJson(BeforePath, RiverSupplementBeforeSha, 852108, Before)
		|| !PinnedJson(AfterPath, RiverSupplementAfterSha, 852658, After))
	{
		OutReason = TEXT("Root River V3 supplemental proof/helper/review/baselines changed.");
		return false;
	}
	int64 Pid = 0, Exit = 0, MaterialBytes = 0;
	FString Schema, TerminalSha, NewMaterialSha, NewMaterialPath;
	const auto Plan = JsonObjectField(Result, TEXT("plan"));
	const auto Creator = JsonObjectField(Result, TEXT("creator_terminal"));
	const auto PreflightReceipt = JsonObjectField(Result, TEXT("creator_receipt"));
	const auto Material = JsonObjectField(Result, TEXT("material"));
	const auto Log = JsonObjectField(Result, TEXT("log"));
	const auto ResultBefore = JsonObjectField(Result, TEXT("before"));
	const auto ResultAfter = JsonObjectField(Result, TEXT("after"));
	const auto Protected = JsonObjectField(Result, TEXT("protected_after_observation"));
	bool bPassed = false, bProtected = false;
	if (!JsonStringField(Result, TEXT("schema"), Schema)
		|| Schema != TEXT("rok.root_river_v3_supplemental_execution.v2")
		|| !Result->TryGetBoolField(TEXT("passed"), bPassed) || !bPassed
		|| !JsonIntegerField(Result, TEXT("pid"), Pid) || Pid <= 0
		|| !JsonIntegerField(Result, TEXT("exit_code"), Exit) || Exit != 0
		|| !Plan.IsValid() || !Creator.IsValid() || !PreflightReceipt.IsValid()
		|| !Material.IsValid() || !Log.IsValid()
		|| !ResultBefore.IsValid() || !ResultAfter.IsValid() || !Protected.IsValid()
		|| !JsonStringField(Plan, TEXT("blocked_upstream_review_sha256"), Schema)
		|| Schema != RiverV3BlockedReviewSha
		|| !JsonStringField(Creator, TEXT("sha256"), TerminalSha)
		|| TerminalSha != RiverSupplementTerminalSha
		|| !JsonStringField(Creator, TEXT("path"), Schema)
		|| NormalPath(Schema) != NormalPath(TerminalPath)
		|| !JsonStringField(PreflightReceipt, TEXT("sha256"), Schema)
		|| Schema != RiverSupplementPreflightReceiptSha
		|| !JsonStringField(PreflightReceipt, TEXT("path"), Schema)
		|| NormalPath(Schema) != NormalPath(PreflightReceiptPath)
		|| !JsonStringField(Material, TEXT("path"), NewMaterialPath)
		|| NormalPath(NewMaterialPath) != NormalPath(MaterialFile)
		|| !JsonStringField(Material, TEXT("sha256"), NewMaterialSha)
		|| NewMaterialSha != RiverSupplementMaterialSha
		|| !JsonIntegerField(Material, TEXT("bytes"), MaterialBytes) || MaterialBytes != 36251
		|| !JsonStringField(Log, TEXT("sha256"), Schema) || Schema != RiverSupplementLogSha
		|| !JsonStringField(Log, TEXT("path"), Schema) || NormalPath(Schema) != NormalPath(SourceEditorLogPath)
		|| !Protected->TryGetBoolField(TEXT("full_source_content_dll_sets_and_bytes_verified"), bProtected)
		|| !bProtected)
	{
		OutReason = TEXT("Root River V3 supplemental process or creator binding differs.");
		return false;
	}
	const TCHAR* Groups[] = {TEXT("source_files"), TEXT("unaccepted_source_files"),
		TEXT("accepted_asset_files"), TEXT("actual_dll_files")};
	for (const TCHAR* Group : Groups)
	{
		TMap<FString, FString> Old, New;
		if (!PinnedRows(Before, Group, Old) || !PinnedRows(After, Group, New)
			|| Old.Num() == 0 || !SamePinnedRows(Old, New))
		{
			OutReason = TEXT("Root River V3 supplemental source/asset/actual DLL byte set differs.");
			return false;
		}
		if (FCString::Strcmp(Group, TEXT("actual_dll_files")) == 0)
		{
			TSet<FString> Names;
			for (const auto& Row : Old) Names.Add(FPaths::GetCleanFilename(Row.Key));
			if (Names.Num() != 3 || !Names.Contains(TEXT("UnrealEditor-Rok.dll"))
				|| !Names.Contains(TEXT("UnrealEditor-Rok-GridValidation.dll"))
				|| !Names.Contains(TEXT("UnrealEditor-RokRendering.dll")))
			{
				OutReason = TEXT("Root River V3 actual DLL name set differs.");
				return false;
			}
		}
	}
	TMap<FString, FString> OldContent, NewContent;
	if (!PinnedRows(Before, TEXT("all_content_files"), OldContent)
		|| !PinnedRows(After, TEXT("all_content_files"), NewContent)
		|| OldContent.Num() != 2448 || NewContent.Num() != 2449
		|| NewContent.FindRef(NormalPath(MaterialFile)) !=
			FString::Printf(TEXT("%s:%lld"), RiverSupplementMaterialSha, MaterialBytes))
	{
		OutReason = TEXT("Root River V3 Content set/addition differs.");
		return false;
	}
	for (const auto& Row : OldContent)
		if (NewContent.FindRef(Row.Key) != Row.Value)
		{
			OutReason = TEXT("Root River V3 changed an existing Content package.");
			return false;
		}
	return true;
}
}

ARokWorldInstancedMaterialDiagnosticGameMode::ARokWorldInstancedMaterialDiagnosticGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ARokWorldMapCameraPawn::StaticClass();
	PlayerControllerClass = ARokWorldMapPlayerController::StaticClass();
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::LoadPinnedAlias(FString& OutReason)
{
	const FString Project = NormalPath(FPaths::ProjectDir());
	const FString Engine = NormalPath(FPaths::EngineDir());
	if (Project != NormalPath(TEXT("D:/ueDemo/Rok"))
		|| Engine != NormalPath(TEXT("D:/ue/UE_5.8/Engine")))
	{
		OutReason = TEXT("Project or UE5.8 engine identity differs.");
		return false;
	}
	const FString ExpectedAlias = NormalPath(FPaths::Combine(Project, AliasRelativePath));
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokWorldInstancedAliasJson="), AliasInputPath)
		|| !FParse::Value(FCommandLine::Get(), TEXT("RokWorldInstancedAliasSha="), AliasInputSha256)
		|| NormalPath(AliasInputPath) != ExpectedAlias || AliasInputSha256 != ActualAliasSha)
	{
		OutReason = TEXT("External exact alias JSON path/SHA is required.");
		return false;
	}
	AliasInputPath = ExpectedAlias;
	TSharedPtr<FJsonObject> Alias;
	TSharedPtr<FJsonObject> Template;
	const FString TemplatePath = FPaths::Combine(Project,
		TEXT("Saved/PendingWorldInstancedVisibilityDiagnosticV7/alias_template.json"));
	if (!PinnedJson(AliasInputPath, AliasInputSha256, -1, Alias)
		|| !PinnedJson(TemplatePath, AliasTemplateSha, 14800, Template))
	{
		OutReason = TEXT("Alias JSON or fixed 24-source template is missing or byte-drifted.");
		return false;
	}
	if (Alias->Values.Num() != Template->Values.Num())
	{
		OutReason = TEXT("Alias top-level field set differs from frozen schema.");
		return false;
	}
	for (const TCHAR* Key : {TEXT("schema"), TEXT("project"), TEXT("engine"), TEXT("world"),
		TEXT("game_mode"), TEXT("canonical_payload_sha256"), TEXT("alias_path"),
		TEXT("frozen_variants_plan_sha256"), TEXT("frozen_river_contract_sha256"),
		TEXT("frozen_bridge_plan_sha256")})
	{
		FString Actual, Expected;
		if (!JsonStringField(Alias, Key, Actual) || !JsonStringField(Template, Key, Expected)
			|| Actual != Expected)
		{
			OutReason = FString::Printf(TEXT("Alias identity field differs: %s"), Key);
			return false;
		}
	}
	bool bFalse = true;
	for (const TCHAR* Key : {TEXT("runtime_accepted"), TEXT("formal_map_lod_accepted"),
		TEXT("unity_pixel_parity_accepted"), TEXT("unity_gamma_parity_accepted"),
		TEXT("full_migration_accepted")})
	{
		if (!Alias->TryGetBoolField(Key, bFalse) || bFalse)
		{
			OutReason = TEXT("Alias input falsely asserts acceptance.");
			return false;
		}
	}
	const auto* ExpectedEntries = JsonArrayField(Template, TEXT("entries"));
	const auto* ActualEntries = JsonArrayField(Alias, TEXT("entries"));
	TMap<FString, FString> CreatorByAlias;
	if (!ExpectedEntries || !ActualEntries || ExpectedEntries->Num() != 24
		|| ActualEntries->Num() != 24)
	{
		OutReason = TEXT("Alias source whitelist is not exactly 24 entries.");
		return false;
	}
	for (int32 Index = 0; Index < 24; ++Index)
	{
		const TSharedPtr<FJsonObject> Expected = (*ExpectedEntries)[Index]->AsObject();
		const TSharedPtr<FJsonObject> Actual = (*ActualEntries)[Index]->AsObject();
		if (!Expected.IsValid() || !Actual.IsValid()
			|| Actual->Values.Num() != Expected->Values.Num())
		{
			OutReason = TEXT("Alias entry field set differs from frozen schema.");
			return false;
		}
		FString Source, Target, Creator, SourceSha, TargetSha;
		for (const TCHAR* Key : {TEXT("source_object"), TEXT("source_package_sha256"),
			TEXT("target_object"), TEXT("creator_key")})
		{
			FString Value, Frozen;
			if (!JsonStringField(Actual, Key, Value) || !JsonStringField(Expected, Key, Frozen)
				|| Value != Frozen)
			{
				OutReason = FString::Printf(TEXT("Frozen alias pair differs at entry %d: %s"), Index, Key);
				return false;
			}
		}
		int64 SourceBytes = 0, FrozenSourceBytes = 0, TargetBytes = 0;
		if (!JsonIntegerField(Actual, TEXT("source_package_bytes"), SourceBytes)
			|| !JsonIntegerField(Expected, TEXT("source_package_bytes"), FrozenSourceBytes)
			|| SourceBytes != FrozenSourceBytes
			|| !JsonIntegerField(Actual, TEXT("target_package_bytes"), TargetBytes)
			|| !JsonStringField(Actual, TEXT("target_package_sha256"), TargetSha)
			|| !IsHexSha(TargetSha))
		{
			OutReason = TEXT("Alias package bytes/SHA missing or source pin differs.");
			return false;
		}
		JsonStringField(Actual, TEXT("source_object"), Source);
		JsonStringField(Actual, TEXT("target_object"), Target);
		JsonStringField(Actual, TEXT("creator_key"), Creator);
		JsonStringField(Actual, TEXT("source_package_sha256"), SourceSha);
		FString SourceFile, TargetFile;
		if (!PackageFileForObject(Source, SourceFile)
			|| !PackageFileForObject(Target, TargetFile)
			|| !FileMatches(SourceFile, SourceSha, SourceBytes)
			|| !FileMatches(TargetFile, TargetSha, TargetBytes)
			|| AliasBySource.Contains(Source) || SourceByAlias.Contains(Target)
		|| (Creator != TEXT("variants") && Creator != TEXT("riverwater")
			&& Creator != TEXT("bridge")))
		{
			OutReason = TEXT("Alias source/target package pin, path or bijection differs.");
			return false;
		}
		UMaterialInterface* TargetMaterial = LoadObject<UMaterialInterface>(nullptr, *Target);
		const UMaterial* Base = TargetMaterial ? TargetMaterial->GetMaterial() : nullptr;
		if (!TargetMaterial || TargetMaterial->GetClass() != UMaterial::StaticClass()
			|| !NativeMaterialReady(Base))
		{
			OutReason = TEXT("Alias target material lacks instancing usage or native compiled shader map.");
			return false;
		}
		AliasBySource.Add(Source, Target);
		SourceByAlias.Add(Target, Source);
		CreatorByAlias.Add(Target, Creator);
		TargetPackageShaByObject.Add(Target, TargetSha);
		TargetPackageBytesByObject.Add(Target, TargetBytes);
	}
	if (AliasBySource.Num() != 24 || SourceByAlias.Num() != 24)
	{
		OutReason = TEXT("Alias mapping is not a 24-pair bijection.");
		return false;
	}
	FString BridgeFile;
	if (!PackageFileForObject(BridgeSource, BridgeFile)
		|| !FileMatches(BridgeFile, BridgeSourceSha, BridgeSourceBytes))
	{
		OutReason = TEXT("Original source bridge material package changed.");
		return false;
	}
	const TSharedPtr<FJsonObject> Proofs = JsonObjectField(Alias, TEXT("creator_receipts"));
	if (!Proofs.IsValid() || Proofs->Values.Num() != 3)
	{
		OutReason = TEXT("Creator receipt proofs absent.");
		return false;
	}
	const TCHAR* ReviewPaths[] = {
		TEXT("Saved/PendingWorldMaterialRootNativeBridgeV2/IndependentReviewV1/review.json"),
		TEXT("Saved/PendingWorldRiverWaterMaterialSourceV2/IndependentReviewV1/review.json"),
		TEXT("Saved/PendingWorldBridgeInstancedMaterialV4/IndependentReviewV1/review.json")};
	const TCHAR* ReviewShas[] = {VariantsReviewSha, RiverV2HistoricalReviewSha, BridgeReviewSha};
	for (int32 ReviewIndex = 0; ReviewIndex < 3; ++ReviewIndex)
	{
		const FString ReviewPath = FPaths::Combine(Project, ReviewPaths[ReviewIndex]);
		if (!FileMatches(ReviewPath, ReviewShas[ReviewIndex], IFileManager::Get().FileSize(*ReviewPath)))
		{
			OutReason = TEXT("Creator or historical River V2 source review pin differs.");
			return false;
		}
	}
	for (const TCHAR* Key : {TEXT("variants"), TEXT("riverwater"), TEXT("bridge")})
	{
		const TSharedPtr<FJsonObject> Proof = JsonObjectField(Proofs, Key);
		FString TerminalPath, TerminalSha, ReadyPath, ReadySha;
		int64 TerminalBytes = 0;
		const bool bVariants = FCString::Strcmp(Key, TEXT("variants")) == 0;
		const bool bRiver = FCString::Strcmp(Key, TEXT("riverwater")) == 0;
		const FString ExpectedReady = NormalPath(FPaths::Combine(Project,
			bVariants ? VariantsReadyPath : bRiver ? RiverReadyPath : BridgeReadyPath));
		const FString ExpectedRunRoot = NormalPath(FPaths::Combine(Project,
			bVariants ? TEXT("Saved/RokUnityMigration/WorldInstancedMaterialVariantsV8")
				: bRiver ? TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/ApplyRuns")
				: TEXT("Saved/RokUnityMigration/WorldBridgeInstancedMaterialV4")));
		if (!Proof.IsValid() || Proof->Values.Num() != 5
			|| !JsonStringField(Proof, TEXT("terminal_path"), TerminalPath)
			|| !JsonStringField(Proof, TEXT("terminal_sha256"), TerminalSha)
			|| !JsonIntegerField(Proof, TEXT("terminal_bytes"), TerminalBytes)
			|| !JsonStringField(Proof, TEXT("ready_path"), ReadyPath)
			|| !JsonStringField(Proof, TEXT("ready_sha256"), ReadySha)
			|| !IsHexSha(TerminalSha) || !IsHexSha(ReadySha)
			|| ReadySha != (bVariants ? VariantsReadySha : bRiver ? RiverReadySha : BridgeReadySha)
			|| NormalPath(ReadyPath) != ExpectedReady
			|| NormalPath(TerminalPath) != TerminalPath
			|| FPaths::GetCleanFilename(TerminalPath) != TEXT("terminal.json")
			|| NormalPath(FPaths::GetPath(FPaths::GetPath(TerminalPath))) != ExpectedRunRoot
			|| !FileMatches(ExpectedReady, ReadySha, IFileManager::Get().FileSize(*ExpectedReady)))
		{
			OutReason = TEXT("Creator terminal or ready proof path/SHA differs.");
			return false;
		}
		TSharedPtr<FJsonObject> Terminal;
		if (!PinnedJson(TerminalPath, TerminalSha, TerminalBytes, Terminal))
		{
			OutReason = TEXT("Creator terminal receipt bytes/SHA differ.");
			return false;
		}
		FString Status, Schema, PlanOrContract;
		const auto* Created = JsonArrayField(Terminal, TEXT("created"));
		bool bApplied = false;
		if (!JsonStringField(Terminal, TEXT("status"), Status)
			|| !JsonStringField(Terminal, TEXT("schema"), Schema) || !Created
			|| Status != (bVariants ? TEXT("created_pending_runtime_validation")
				: bRiver ? TEXT("created_pending_native_gpu_comparison_and_binding")
				: TEXT("created_pending_native_runtime_and_visual_validation"))
			|| Schema != (bVariants ? TEXT("rok.world_instanced_material_variants_editor_run.native_bridge_v2")
				: bRiver ? TEXT("rok.world_river_water.editor_attempt.v3")
				: TEXT("rok.world_bridge_instanced_material_editor_run.v4"))
			|| Created->Num() != (bVariants ? 22 : 1)
			|| !JsonStringField(Terminal, bRiver ? TEXT("contract_sha256")
				: TEXT("plan_sha256"), PlanOrContract)
			|| PlanOrContract != (bVariants ? VariantsPlanSha : bRiver ? RiverContractSha : BridgePlanSha)
			|| !Terminal->TryGetBoolField(TEXT("apply_requested"), bApplied) || !bApplied)
		{
			OutReason = TEXT("Creator terminal schema/status/plan or contract differs.");
			return false;
		}
		if (bRiver)
		{
			FString ReceiptReadySha;
			if (!JsonStringField(Terminal, TEXT("ready_sha256"), ReceiptReadySha)
				|| ReceiptReadySha != ReadySha)
			{
				OutReason = TEXT("RiverWater receipt does not pin the reviewed ready bytes.");
				return false;
			}
		}
		if (bVariants)
		{
			FString GraphModuleSha;
			int64 BeforeCount = 0, AfterCount = 0;
			if (!JsonStringField(Terminal, TEXT("graph_fingerprint_module_sha256"), GraphModuleSha)
				|| GraphModuleSha != VariantsGraphModuleSha
				|| !JsonIntegerField(Terminal, TEXT("protected_content_files_checked_before"), BeforeCount)
				|| !JsonIntegerField(Terminal, TEXT("protected_content_files_checked_after"), AfterCount)
				|| BeforeCount != 2423 || AfterCount != 2423)
			{
				OutReason = TEXT("Variant creator graph/protected Content proof differs.");
				return false;
			}
		}
		else if (bRiver)
		{
			FString TextureSha, FailedMaterialSha, NewSha, NewPath;
			int64 BeforeCount = 0, AfterCount = 0, DllCount = 0, NewBytes = 0;
			bool bRuntimeAccepted = true, bVisualAccepted = true, bFullMigrationAccepted = true;
			const TSharedPtr<FJsonObject> After = JsonObjectField(Terminal, TEXT("after"));
			const TSharedPtr<FJsonObject> NewMaterial = JsonObjectField(After, TEXT("new_material"));
			const FString V3Object = TEXT("/Game/RokDiagnostics/WorldStreaming/RiverWaterSourceV3/M_WorldRiver_Water_VertexInterpolatedV3.M_WorldRiver_Water_VertexInterpolatedV3");
			const FString FailedV2Object = TEXT("/Game/RokDiagnostics/WorldStreaming/RiverWaterSourceV2/M_WorldRiver_Water_SourceFixedV2.M_WorldRiver_Water_SourceFixedV2");
			const FString TextureV2Object = TEXT("/Game/RokDiagnostics/WorldStreaming/RiverWaterSourceV2/T_WorldRiver_Bump_RGBA8CandidateV2.T_WorldRiver_Bump_RGBA8CandidateV2");
			FString V3File, FailedV2File, TextureV2File;
			const FString V3ContractFile = FPaths::Combine(Project,
				TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/frozen_contract.json"));
			const FString V3CreatorFile = FPaths::Combine(Project,
				TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/CreateWorldRiverWaterMaterialV3.py"));
			const FString V3VerifierFile = FPaths::Combine(Project,
				TEXT("Saved/PendingWorldRiverWaterMaterialSourceV3/VerifyWorldRiverWaterSourceV3.py"));
			if (!JsonStringField(Terminal, TEXT("v2_partial_reused_texture_sha256"), TextureSha)
				|| TextureSha != RiverV2TextureSha
				|| !JsonStringField(Terminal, TEXT("v2_failed_material_protected_sha256"), FailedMaterialSha)
				|| FailedMaterialSha != RiverV2FailedMaterialSha
				|| !Terminal->TryGetBoolField(TEXT("runtime_accepted"), bRuntimeAccepted) || bRuntimeAccepted
				|| !Terminal->TryGetBoolField(TEXT("visual_accepted"), bVisualAccepted) || bVisualAccepted
				|| !Terminal->TryGetBoolField(TEXT("full_migration_accepted"), bFullMigrationAccepted) || bFullMigrationAccepted
				|| !JsonIntegerField(After, TEXT("content_before_count"), BeforeCount)
				|| !JsonIntegerField(After, TEXT("content_after_count"), AfterCount)
				|| !JsonIntegerField(After, TEXT("actual_dll_count"), DllCount)
				|| BeforeCount != 2448 || AfterCount != 2449 || DllCount != 3
				|| !JsonStringField(NewMaterial, TEXT("sha256"), NewSha)
				|| !JsonIntegerField(NewMaterial, TEXT("bytes"), NewBytes)
				|| !JsonStringField(NewMaterial, TEXT("path"), NewPath)
				|| !PackageFileForObject(V3Object, V3File)
				|| NormalPath(NewPath) != NormalPath(V3File)
				|| !SourceByAlias.Contains(V3Object)
				|| SourceByAlias.Contains(FailedV2Object)
				|| SourceByAlias.Contains(TextureV2Object)
				|| NewSha != TargetPackageShaByObject[V3Object]
				|| NewBytes != TargetPackageBytesByObject[V3Object]
				|| !PackageFileForObject(FailedV2Object, FailedV2File)
				|| !PackageFileForObject(TextureV2Object, TextureV2File)
				|| !FileMatches(FailedV2File, RiverV2FailedMaterialSha, 36522)
				|| !FileMatches(TextureV2File, RiverV2TextureSha, 790032)
				|| !FileMatches(V3ContractFile, RiverContractSha, IFileManager::Get().FileSize(*V3ContractFile))
				|| !FileMatches(V3CreatorFile, RiverV3CreatorSha, IFileManager::Get().FileSize(*V3CreatorFile))
				|| !FileMatches(V3VerifierFile, RiverV3VerifierSha, IFileManager::Get().FileSize(*V3VerifierFile)))
			{
				OutReason = TEXT("River V3 one-material creator or protected V2 partial proof differs.");
				return false;
			}
			if (!ValidateRiverRootSupplement(Project, TerminalPath, V3File, OutReason)) return false;
		}
		else
		{
			FString SourceObject, SourceSha, GraphSha, RootPinsPath, RootPinsSha;
			int64 BeforeCount = 0, AfterCount = 0;
			if (!JsonStringField(Terminal, TEXT("source_object"), SourceObject)
				|| SourceObject != BridgeSource
				|| !JsonStringField(Terminal, TEXT("source_package_sha256"), SourceSha)
				|| SourceSha != BridgeSourceSha
				|| !JsonStringField(Terminal, TEXT("graph_module_sha256"), GraphSha)
				|| GraphSha != VariantsGraphModuleSha
				|| !JsonStringField(Terminal, TEXT("root_inputpins_path"), RootPinsPath)
				|| !JsonStringField(Terminal, TEXT("root_inputpins_sha256"), RootPinsSha)
				|| FPaths::IsRelative(RootPinsPath)
				|| !NormalPath(RootPinsPath).StartsWith(NormalPath(FPaths::Combine(Project,
					TEXT("Saved/RokUnityMigration"))) + TEXT("/"))
				|| !IsHexSha(RootPinsSha)
				|| !FileMatches(RootPinsPath, RootPinsSha, IFileManager::Get().FileSize(*RootPinsPath))
				|| !JsonIntegerField(Terminal, TEXT("protected_content_files_checked_before"), BeforeCount)
				|| !JsonIntegerField(Terminal, TEXT("protected_content_files_checked_after"), AfterCount)
				|| BeforeCount < 2423 || AfterCount != BeforeCount)
			{
				OutReason = TEXT("Bridge creator source/graph/full current Content proof differs.");
				return false;
			}
		}
		TSet<FString> ProvenTargets;
		for (const TSharedPtr<FJsonValue>& Value : *Created)
		{
			const TSharedPtr<FJsonObject> CreatedEntry = Value->AsObject();
			FString Source, Target, Sha;
			int64 Bytes = 0;
			bool bUsage = false;
			if (!JsonStringField(CreatedEntry, bRiver ? TEXT("object") : TEXT("target_object"), Target))
			{
				OutReason = TEXT("Creator receipt target missing.");
				return false;
			}
			if (!SourceByAlias.Contains(Target))
			{
				OutReason = TEXT("Creator receipt contains foreign material target.");
				return false;
			}
			if (ProvenTargets.Contains(Target) || CreatorByAlias.FindRef(Target) != Key
				|| !JsonStringField(CreatedEntry, bRiver ? TEXT("sha256") : TEXT("target_sha256"), Sha)
				|| !JsonIntegerField(CreatedEntry, bRiver ? TEXT("bytes") : TEXT("target_bytes"), Bytes)
				|| Sha != TargetPackageShaByObject[Target] || Bytes != TargetPackageBytesByObject[Target]
				|| !CreatedEntry->TryGetBoolField(bRiver ? TEXT("instanced_usage")
					: TEXT("instanced_static_mesh_usage"), bUsage) || !bUsage)
			{
				OutReason = TEXT("Creator material hash/bytes/instancing proof differs.");
				return false;
			}
			if (bVariants)
			{
				FString SourceGraphSha, VariantGraphSha, ComparisonSha;
				bool bSameCapturedSubset = false;
				if (!JsonStringField(CreatedEntry, TEXT("source_object"), Source)
					|| Source != SourceByAlias[Target]
					|| !JsonStringField(CreatedEntry, TEXT("source_graph_sha256"), SourceGraphSha)
					|| !JsonStringField(CreatedEntry, TEXT("variant_graph_sha256"), VariantGraphSha)
					|| !JsonStringField(CreatedEntry, TEXT("semantic_comparison_sha256"), ComparisonSha)
					|| !IsHexSha(SourceGraphSha) || !IsHexSha(VariantGraphSha) || !IsHexSha(ComparisonSha)
					|| !CreatedEntry->TryGetBoolField(TEXT("same_captured_reflected_subset_except_instancing_flag"), bSameCapturedSubset)
					|| !bSameCapturedSubset)
				{
					OutReason = TEXT("Variant receipt source-target/graph comparison proof differs.");
					return false;
				}
			}
			else if (bRiver)
			{
				const auto* Errors = JsonArrayField(CreatedEntry, TEXT("compile_errors"));
				const TSharedPtr<FJsonObject> Hits = JsonObjectField(CreatedEntry,
					TEXT("stage_bridge_and_channel_hits"));
				FString ReusedTexture, RhiFormat;
				int64 VertexInterpolator = 0, Ba = 0, Alpha = 0, Spec = 0, AlphaPlusSpec = 0;
				bool bParityAccepted = true;
				if (!Errors || !Errors->IsEmpty()
					|| !JsonStringField(CreatedEntry, TEXT("texture_reused"), ReusedTexture)
					|| ReusedTexture != TEXT("/Game/RokDiagnostics/WorldStreaming/RiverWaterSourceV2/T_WorldRiver_Bump_RGBA8CandidateV2.T_WorldRiver_Bump_RGBA8CandidateV2")
					|| !JsonStringField(CreatedEntry, TEXT("actual_rhi_format"), RhiFormat)
					|| RhiFormat != TEXT("pending_native_readback")
					|| !CreatedEntry->TryGetBoolField(TEXT("native_gpu_and_unity_parity_accepted"), bParityAccepted)
					|| bParityAccepted
					|| !JsonIntegerField(Hits, TEXT("vertex_interpolator"), VertexInterpolator)
					|| !JsonIntegerField(Hits, TEXT("ba"), Ba)
					|| !JsonIntegerField(Hits, TEXT("alpha"), Alpha)
					|| !JsonIntegerField(Hits, TEXT("spec"), Spec)
					|| !JsonIntegerField(Hits, TEXT("alpha_plus_spec"), AlphaPlusSpec)
					|| VertexInterpolator != 1 || Ba != 2 || Alpha != 2
					|| Spec != 1 || AlphaPlusSpec != 1)
				{
					OutReason = TEXT("River V3 material compile/vertex interpolation/channel proof differs.");
					return false;
				}
			}
			else
			{
				FString ComparisonSha, SourceGraphSha, TargetGraphSha;
				bool bSameSubset = false;
				if (!JsonStringField(CreatedEntry, TEXT("semantic_comparison_sha256"), ComparisonSha)
					|| !JsonStringField(CreatedEntry, TEXT("source_graph_file_sha256"), SourceGraphSha)
					|| !JsonStringField(CreatedEntry, TEXT("target_graph_file_sha256"), TargetGraphSha)
					|| !IsHexSha(ComparisonSha) || !IsHexSha(SourceGraphSha)
					|| !IsHexSha(TargetGraphSha)
					|| !FileMatches(FPaths::Combine(FPaths::GetPath(TerminalPath), TEXT("source_graph.json")),
						SourceGraphSha, IFileManager::Get().FileSize(
							*FPaths::Combine(FPaths::GetPath(TerminalPath), TEXT("source_graph.json"))))
					|| !FileMatches(FPaths::Combine(FPaths::GetPath(TerminalPath), TEXT("target_graph.json")),
						TargetGraphSha, IFileManager::Get().FileSize(
							*FPaths::Combine(FPaths::GetPath(TerminalPath), TEXT("target_graph.json"))))
					|| !CreatedEntry->TryGetBoolField(
						TEXT("same_captured_reflected_subset_except_instancing_flag"), bSameSubset)
					|| !bSameSubset)
				{
					OutReason = TEXT("Bridge creator reflected graph comparison differs.");
					return false;
				}
			}
			ProvenTargets.Add(Target);
		}
		if (ProvenTargets.Num() != (bVariants ? 22 : 1))
		{
			OutReason = TEXT("Creator receipt does not prove every alias target.");
			return false;
		}
		if (bVariants) { VariantsReceiptSha256 = TerminalSha; }
		else if (bRiver) { RiverReceiptSha256 = TerminalSha; }
		else { BridgeReceiptSha256 = TerminalSha; }
	}
	bAliasLoaded = true;
	return true;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::BeginPlay()
{
	Super::BeginPlay();
	bCoverageMode = FParse::Param(FCommandLine::Get(), TEXT("RokWorldAliasPixelCoverageDiagnostic"));
	bFixedCameraProbeMode = FParse::Param(FCommandLine::Get(), TEXT("RokWorldLodFixedCameraMaterialProbe"));
	StartedAtSeconds = FPlatformTime::Seconds();
	const FString RunUtc = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	CoverageProcessId = FString::Printf(TEXT("%u"), FPlatformProcess::GetCurrentProcessId());
	CoverageRunStartTicks = ExactU64(FPlatformTime::Cycles64());
	CoverageRunId = FString::Printf(TEXT("%s_%s_%s"), *RunUtc,
		*CoverageProcessId, *CoverageRunStartTicks);
	RunDirectory = FPaths::Combine(FPaths::ProjectSavedDir(),
		bFixedCameraProbeMode
		? TEXT("RokUnityMigration/WorldFixedCameraGroundMaterialProbeRunsV4")
			: bCoverageMode
				? TEXT("RokUnityMigration/WorldAliasPixelCoverageDiagnosticRunsV14")
				: TEXT("RokUnityMigration/WorldInstancedMaterialDiagnosticRunsV10"),
		CoverageRunId);
	RunDirectory = FPaths::ConvertRelativePathToFull(RunDirectory);
	FPaths::NormalizeDirectoryName(RunDirectory);
	IFileManager::Get().MakeDirectory(*RunDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &ARokWorldInstancedMaterialDiagnosticGameMode::ScreenshotProcessed);
	ViewportRenderedHandle = UGameViewportClient::OnViewportRendered().AddUObject(
		this, &ARokWorldInstancedMaterialDiagnosticGameMode::ViewportRendered);
	BeginBoundary = MakeShared<FJsonObject>();
	WriteLocalOnlyBoundary(*BeginBoundary);
	UWorld* World = GetWorld();
	if ((bCoverageMode && bFixedCameraProbeMode)
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokWorldStreamingDiagnostic"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		|| !FParse::Param(FCommandLine::Get(), TEXT("RokWorldInstancedMaterialDiagnostic"))
		|| !World
		|| GetClass() != StaticClass()
		|| !FString(FCommandLine::Get()).Contains(
			TEXT("?game=/Script/Rok.RokWorldInstancedMaterialDiagnosticGameMode"),
			ESearchCase::IgnoreCase)
		|| World->GetOutermost()->GetName() != (bCoverageMode
			? TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticGroundSortV2")
			: TEXT("/Game/Maps/L_RokWorldStreamingDiagnosticV1")))
	{
		Fail(TEXT("Diagnostic command flag/map identity failed."));
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	ViewExtension = FSceneViewExtensions::NewExtension<FRokWorldDiagnosticViewExtension>(
		World, this);
	for (TActorIterator<ARokWorldMapStreamingActor> It(World); It; ++It)
	{
		if (Streaming.IsValid())
		{
			Fail(TEXT("More than one source streaming actor in diagnostic map."));
			return;
		}
		Streaming = *It;
	}
	if (!Streaming.IsValid() || !Streaming->RuntimeData
		|| Streaming->RuntimeData->GetPathName() != (bCoverageMode
			? TEXT("/Game/RokDiagnostics/WorldStreaming/V2/DA_RokWorldStreamGroundSortLod01V2.DA_RokWorldStreamGroundSortLod01V2")
			: TEXT("/Game/RokDiagnostics/WorldStreaming/V1/DA_RokWorldStreamSourceLod01.DA_RokWorldStreamSourceLod01"))
		|| Streaming->RuntimeData->CanonicalPayloadSha256 != (bCoverageMode
			? TEXT("31862a9e4847eb2856fd1c1801ecc6dfd5b6b77673cb2cc6315fb6c85d2ea1ad")
			: TEXT("09298c671a05b3c220e24b25c7c7362c4d7f0cff1218a29d7c305e7471f86b6f"))
		|| Streaming->RuntimeData->bStaticVisualReady
		|| Streaming->RuntimeData->bRuntimeReady
		|| !Streaming->bEnableScopedDiagnosticLod01)
	{
		Fail(TEXT("Diagnostic actor/data/readiness identity failed."));
		return;
	}
	FString AliasReason;
	if (!LoadPinnedAlias(AliasReason))
	{
		Fail(AliasReason);
		return;
	}
	int32 ExistingDirectional = 0;
	int32 ExistingSky = 0;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It) { ++ExistingDirectional; }
	for (TActorIterator<ASkyLight> It(World); It; ++It) { ++ExistingSky; }
	if (ExistingDirectional != 0 || ExistingSky != 0)
	{
		Fail(TEXT("Diagnostic map lighting baseline changed; expected zero preexisting lights."));
		return;
	}
	ADirectionalLight* DiagnosticLight = World->SpawnActor<ADirectionalLight>();
	if (!DiagnosticLight || !DiagnosticLight->GetLightComponent())
	{
		Fail(TEXT("Could not create local transient diagnostic directional light."));
		return;
	}
	DiagnosticLight->SetActorRotation(FRotator(-50.0f, -35.0f, 0.0f));
	DiagnosticLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	DiagnosticLight->GetLightComponent()->SetIntensity(3.0f);
	DiagnosticLight->GetLightComponent()->SetLightColor(FLinearColor::White);
}

void ARokWorldInstancedMaterialDiagnosticGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bCoverageMode && bCoverageTargetHidden)
	{
		FString RestoreReason;
		if (!SetCoverageTargetHidden(false, RestoreReason))
		{
			UE_LOG(LogTemp, Error, TEXT("Coverage target EndPlay restore failed: %s"), *RestoreReason);
		}
	}
	if (bFixedCameraProbeMode && Streaming.IsValid())
	{
		Streaming->SetFixedCameraProbeLod(INDEX_NONE);
	}
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	UGameViewportClient::OnViewportRendered().Remove(ViewportRenderedHandle);
	ViewExtension.Reset();
	// The render-thread probe captures this world's FSceneInterface.  Retire even
	// a probe whose result was discarded before world teardown releases Scene.
	if (bHismProxyProbeEverQueued)
	{
		HismProxyProbeFence.Wait();
		HismProxyProbeResult.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ARokWorldInstancedMaterialDiagnosticGameMode::ObserveWorldViewSubmission(
	const FSceneViewFamily& ViewFamily)
{
	const UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
	if (bFinished || !ViewportClient || !ViewportClient->Viewport
		|| ViewportClient->GetWorld() != GetWorld()
		|| ViewFamily.Scene != GetWorld()->Scene
		|| ViewFamily.RenderTarget != ViewportClient->Viewport)
	{
		return;
	}
	LastWorldViewSubmissionFrame = ViewFamily.FrameCounter;
	LastWorldViewSubmissionStep = Step;
	LastWorldViewCount = ViewFamily.Views.Num();
	LastWorldViewRect = ViewFamily.Views.IsEmpty() || !ViewFamily.Views[0]
		? FIntRect() : ViewFamily.Views[0]->UnscaledViewRect;
	bLastWorldViewRendering = ViewFamily.EngineShowFlags.Rendering;
	bLastWorldViewStaticMeshes = ViewFamily.EngineShowFlags.StaticMeshes;
	bLastWorldViewInstancedStaticMeshes = ViewFamily.EngineShowFlags.InstancedStaticMeshes;
	if (bFixedCameraProbeMode)
	{
		LastFixedProbeSubmittedView.Reset();
		if (ViewFamily.Views.Num() == 1 && ViewFamily.Views[0])
		{
			const FViewMatrices& Matrices = ViewFamily.Views[0]->ViewMatrices;
			LastFixedProbeSubmittedView = MakeShared<FJsonObject>();
			LastFixedProbeSubmittedView->SetNumberField(TEXT("frame"),
				static_cast<double>(ViewFamily.FrameCounter));
			LastFixedProbeSubmittedView->SetNumberField(TEXT("step"), Step);
			LastFixedProbeSubmittedView->SetArrayField(TEXT("view_matrix"),
				MatrixNumbers(Matrices.GetWorldToView()));
			LastFixedProbeSubmittedView->SetArrayField(TEXT("projection_no_aa_matrix"),
				MatrixNumbers(Matrices.GetViewToClipNoAA()));
		}
	}
}

void ARokWorldInstancedMaterialDiagnosticGameMode::ViewportRendered(FViewport* Viewport)
{
	const UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
	if (bFinished || !ViewportClient || ViewportClient->Viewport != Viewport
		|| ViewportClient->GetWorld() != GetWorld())
	{
		return;
	}
	LastViewportRenderedFrame = GFrameCounter;
	if (PendingSample.IsValid() || !Streaming.IsValid()
		|| LastWorldViewSubmissionFrame != GFrameCounter
		|| LastWorldViewSubmissionStep != Step
		|| GFrameCounter < LastAliasMutationFrame
		|| LastWorldViewCount != 1 || !IsExpectedSubmittedViewRect()
		|| !bLastWorldViewRendering || !bLastWorldViewStaticMeshes
		|| !bLastWorldViewInstancedStaticMeshes)
	{
		return;
	}
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	if (HismProxyProbeStep != Step || HismProxyProbeGeneration != State.Generation
		|| HismProxyProbeAliasSerial != AliasMutationSerial)
	{
		bHismProxyProbeArmed = false;
		HismProxyProbeResult.Reset();
		HismProxyProbeIds.Reset();
		HismProxyProbeStep = Step;
		HismProxyProbeGeneration = State.Generation;
		HismProxyProbeAliasSerial = AliasMutationSerial;
		HismReadyObservedFrame = 0;
		bSubmittedViewFenceArmed = false;
		++HismProxyProbeSerial;
	}
	const TSharedPtr<FJsonObject> HismReady = CaptureHismRenderReadiness();
	if (!HismReady->GetBoolField(TEXT("cpu_ready")))
	{
		bHismProxyProbeArmed = false;
		HismProxyProbeResult.Reset();
		HismProxyProbeIds.Reset();
		HismReadyObservedFrame = 0;
		bSubmittedViewFenceArmed = false;
		return;
	}
	if (!HismReady->GetBoolField(TEXT("intentional_empty_lod2"))
		&& !bHismProxyProbeArmed)
	{
		HismProxyProbeIds.Reset();
		TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		Streaming->GetComponents(Components);
		for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
		{
			if (IsValid(Component) && Component->GetOwner() == Streaming.Get()
				&& Component->IsRegistered())
			{
				HismProxyProbeIds.Add(Component->GetPrimitiveSceneId().PrimIDValue);
			}
		}
		FSceneInterface* Scene = GetWorld() ? GetWorld()->Scene : nullptr;
		if (!Scene || HismProxyProbeIds.Num() != State.ComponentCount)
		{
			return;
		}
		HismProxyProbeResult = MakeShared<FRokWorldHismProxyProbeResult, ESPMode::ThreadSafe>();
		const TSharedRef<FRokWorldHismProxyProbeResult, ESPMode::ThreadSafe> Probe =
			HismProxyProbeResult.ToSharedRef();
		const TArray<uint32> Ids = HismProxyProbeIds;
		ENQUEUE_RENDER_COMMAND(RokWorldHismSceneProxyProbe)(
			[Scene, Probe, Ids](FRHICommandListImmediate&)
			{
				for (const uint32 IdValue : Ids)
				{
					FPrimitiveComponentId Id;
					Id.PrimIDValue = IdValue;
					Probe->PresenceByComponentId.Add(IdValue,
						Scene->GetPrimitiveSceneInfo(Id) != nullptr ? 1 : 0);
				}
			});
		HismProxyProbeFence.BeginFence();
		bHismProxyProbeArmed = true;
		bHismProxyProbeEverQueued = true;
		++HismProxyProbeSerial;
		bSubmittedViewFenceArmed = false;
		return;
	}
	if (!HismReady->GetBoolField(TEXT("all_ready")))
	{
		if (bHismProxyProbeArmed && HismProxyProbeFence.IsFenceComplete())
		{
			// A missing proxy may appear after a later render-state rebuild.
			// Reprobe on a future matching viewport frame; never retry a black PNG.
			bHismProxyProbeArmed = false;
			HismProxyProbeResult.Reset();
			HismProxyProbeIds.Reset();
		}
		bSubmittedViewFenceArmed = false;
		return;
	}
	if (HismReadyObservedFrame == 0)
	{
		HismReadyObservedFrame = GFrameCounter;
	}
	if (!bSubmittedViewFenceArmed)
	{
		// Draw() submitted this view after the current HISM tree/PSO/proxy
		// readiness was observed. Retire its commands, not the earlier alias fence.
		SubmittedViewFence.BeginFence();
		bSubmittedViewFenceArmed = true;
		SubmittedViewFenceFrame = GFrameCounter;
		SubmittedViewFenceStep = Step;
		SubmittedViewFenceAliasSerial = AliasMutationSerial;
		SubmittedViewFenceHismProxySerial = HismProxyProbeSerial;
	}
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CaptureExpectedConstrainedViewRect(
	FIntRect& OutRect, FMinimalViewInfo& OutViewInfo, float& OutEffectiveAspect,
	float& OutViewportDesiredAspect) const
{
	OutRect = FIntRect();
	OutEffectiveAspect = 0.0f;
	OutViewportDesiredAspect = 0.0f;
	const UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	UCameraComponent* Component = Camera.IsValid() ? Camera->GetCameraComponent() : nullptr;
	if (!Viewport || !Component || ViewportClient->GetWorld() != GetWorld()
		|| Viewport->GetSizeXY() != FIntPoint(1440, 810))
	{
		return false;
	}
	Component->GetCameraView(0.0f, OutViewInfo);
	const FVector4f& Crop = OutViewInfo.AsymmetricCropFraction;
	const float CropDenominator = Crop.Z + Crop.W;
	if (CropDenominator <= 0.0f || !FMath::IsFinite(CropDenominator))
	{
		return false;
	}
	OutEffectiveAspect = OutViewInfo.AspectRatio * (Crop.X + Crop.Y) / CropDenominator;
	OutViewportDesiredAspect = Viewport->GetDesiredAspectRatio();
	if (OutViewInfo.ProjectionMode != ECameraProjectionMode::Perspective
		|| !OutViewInfo.bConstrainAspectRatio
		|| !Component->bConstrainAspectRatio
		|| !FMath::IsNearlyEqual(OutViewInfo.AspectRatio, SourceCameraAspectRatio, 0.001f)
		|| !FMath::IsNearlyEqual(Component->AspectRatio, SourceCameraAspectRatio, 0.001f)
		|| !FMath::IsNearlyEqual(OutEffectiveAspect, SourceCameraAspectRatio, 0.001f)
		|| !FMath::IsNearlyEqual(OutViewportDesiredAspect, 1440.0f / 810.0f, 0.001f)
		|| !FMath::IsFinite(OutEffectiveAspect) || OutEffectiveAspect <= 0.0f
		|| !FMath::IsFinite(OutViewInfo.FOV) || OutViewInfo.FOV <= 0.0f
		|| OutViewInfo.FOV >= 180.0f)
	{
		return false;
	}
	const FIntRect FullViewportRect(0, 0, 1440, 810);
	OutRect = Viewport->CalculateViewExtents(OutEffectiveAspect, FullViewportRect);
	// UE's own constrained-rect algorithm must give the pinned source camera's
	// centered result in the full 1440x810 native viewport.
	const FIntRect SourceRect = FViewport::CalculateViewExtents(SourceCameraAspectRatio,
		1440.0f / 810.0f, FullViewportRect, FIntPoint(1440, 810));
	return OutRect == SourceRect && OutRect.Width() > 0 && OutRect.Height() > 0
		&& OutRect.Min.X >= FullViewportRect.Min.X
		&& OutRect.Min.Y >= FullViewportRect.Min.Y
		&& OutRect.Max.X <= FullViewportRect.Max.X
		&& OutRect.Max.Y <= FullViewportRect.Max.Y;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::IsExpectedSubmittedViewRect() const
{
	FIntRect ExpectedRect;
	FMinimalViewInfo CameraView;
	float EffectiveAspect = 0.0f;
	float ViewportDesiredAspect = 0.0f;
	return CaptureExpectedConstrainedViewRect(ExpectedRect, CameraView,
		EffectiveAspect, ViewportDesiredAspect)
		&& LastWorldViewRect == ExpectedRect;
}

TSharedPtr<FJsonObject> ARokWorldInstancedMaterialDiagnosticGameMode::CaptureHismRenderReadiness() const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Entries;
	if (!Streaming.IsValid())
	{
		Result->SetBoolField(TEXT("all_ready"), false);
		Result->SetArrayField(TEXT("components"), Entries);
		return Result;
	}
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	const bool bExpectedEmpty = !bCoverageMode && !bFixedCameraProbeMode && Step == 4
		&& !State.bHasRequestedState && !State.bPendingLoad
		&& State.bRegisteredMatchesActive
		&& State.ComponentCount == 0 && State.RegisteredComponentCount == 0
		&& State.InstanceCount == 0 && State.RegisteredInstanceCount == 0;
	const bool bExpectedNonempty = Step > 0 && (bCoverageMode || bFixedCameraProbeMode || Step != 4)
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.ComponentCount > 0 && State.VisibleComponentCount > 0
		&& State.InstanceCount > 0 && State.bRegisteredMatchesActive
		&& State.RegisteredComponentCount == State.ComponentCount
		&& State.RegisteredInstanceCount == State.InstanceCount;
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Streaming->GetComponents(Components);
	TArray<const UHierarchicalInstancedStaticMeshComponent*> Current;
	TArray<uint32> CurrentIds;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
	{
		if (IsValid(Component) && Component->GetOwner() == Streaming.Get()
			&& Component->IsRegistered())
		{
			Current.Add(Component);
			CurrentIds.Add(Component->GetPrimitiveSceneId().PrimIDValue);
		}
	}
	bool bProbeMatches = bHismProxyProbeArmed && HismProxyProbeResult.IsValid()
		&& HismProxyProbeFence.IsFenceComplete()
		&& HismProxyProbeStep == Step
		&& HismProxyProbeGeneration == State.Generation
		&& HismProxyProbeAliasSerial == AliasMutationSerial
		&& HismProxyProbeIds.Num() == CurrentIds.Num();
	if (bProbeMatches)
	{
		for (const uint32 Id : CurrentIds)
		{
			bProbeMatches &= Id != 0 && HismProxyProbeIds.Contains(Id);
		}
	}
	bool bAllCpuReady = bExpectedEmpty || bExpectedNonempty;
	bool bAllVisibleProxiesPresent = true;
	int32 ObservedInstances = 0;
	int32 ObservedVisible = 0;
	int32 ObservedSceneProxies = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Current)
	{
		const UStaticMesh* Mesh = Component->GetStaticMesh();
		const int32 LogicalCount = Component->GetInstanceCount();
		const int32 BuiltCount = Component->NumBuiltInstances;
		const int32 BuiltRenderCount = Component->NumBuiltRenderInstances;
		const int32 SortedRenderCount = Component->GetNumRenderInstances();
		const uint32 Id = Component->GetPrimitiveSceneId().PrimIDValue;
		const bool bVisible = Component->IsVisible();
		const bool bMeshCompiling = Mesh && Mesh->IsCompiling();
		const bool bMeshRenderDataValid = Mesh && !bMeshCompiling
			&& Mesh->HasValidRenderData();
		const bool bTreeReady = !Component->IsAsyncBuilding()
			&& Component->IsTreeFullyBuilt() && BuiltCount == LogicalCount
			&& BuiltRenderCount == SortedRenderCount
			&& (!bVisible || BuiltRenderCount > 0);
		const bool bInstanceDataProxyPresent = Component->GetInstanceDataSceneProxy().IsValid();
		const bool bCpuReady = LogicalCount > 0 && Id != 0
			&& Component->IsRenderStateCreated()
			&& !Component->IsRenderStateDirty()
			&& !Component->IsRenderInstancesDirty()
			&& Component->GetAttachParent() == Streaming->GetRootComponent()
			&& bMeshRenderDataValid && !Component->IsPSOPrecaching()
			&& bTreeReady && (!bVisible || bInstanceDataProxyPresent);
		const bool bSceneProxyPresent = bProbeMatches
			&& HismProxyProbeResult->PresenceByComponentId.FindRef(Id) != 0;
		ObservedInstances += LogicalCount;
		ObservedVisible += bVisible ? 1 : 0;
		ObservedSceneProxies += bVisible && bSceneProxyPresent ? 1 : 0;
		bAllCpuReady &= bCpuReady;
		bAllVisibleProxiesPresent &= !bVisible || bSceneProxyPresent;
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Component->GetName());
		Entry->SetStringField(TEXT("mesh"), Mesh ? Mesh->GetPathName() : TEXT(""));
		Entry->SetNumberField(TEXT("primitive_component_id"), Id);
		Entry->SetBoolField(TEXT("visible"), bVisible);
		Entry->SetBoolField(TEXT("scene_proxy_required"), bVisible);
		Entry->SetNumberField(TEXT("logical_instances"), LogicalCount);
		Entry->SetNumberField(TEXT("num_built_instances"), BuiltCount);
		Entry->SetNumberField(TEXT("num_built_render_instances"), BuiltRenderCount);
		Entry->SetNumberField(TEXT("num_render_instances"), SortedRenderCount);
		Entry->SetBoolField(TEXT("mesh_compiling"), bMeshCompiling);
		Entry->SetBoolField(TEXT("mesh_render_data_valid"), bMeshRenderDataValid);
		Entry->SetBoolField(TEXT("pso_precaching"), Component->IsPSOPrecaching());
		Entry->SetBoolField(TEXT("async_tree_building"), Component->IsAsyncBuilding());
		Entry->SetBoolField(TEXT("tree_fully_built"), Component->IsTreeFullyBuilt());
		Entry->SetBoolField(TEXT("instance_data_scene_proxy_present"), bInstanceDataProxyPresent);
		Entry->SetBoolField(TEXT("render_state_dirty"), Component->IsRenderStateDirty());
		Entry->SetBoolField(TEXT("render_instances_dirty"), Component->IsRenderInstancesDirty());
		Entry->SetBoolField(TEXT("scene_proxy_probe_complete"), bProbeMatches);
		Entry->SetBoolField(TEXT("scene_proxy_present"), bSceneProxyPresent);
		Entry->SetBoolField(TEXT("cpu_ready"), bCpuReady);
		Entries.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bAllCpuReady &= Current.Num() == State.ComponentCount
		&& ObservedInstances == State.InstanceCount
		&& ObservedVisible == State.VisibleComponentCount;
	const bool bAllReady = bAllCpuReady
		&& (bExpectedEmpty || (bExpectedNonempty && bProbeMatches
			&& bAllVisibleProxiesPresent));
	Result->SetNumberField(TEXT("step"), Step);
	Result->SetNumberField(TEXT("generation"), static_cast<double>(State.Generation));
	Result->SetNumberField(TEXT("alias_serial"), static_cast<double>(AliasMutationSerial));
	Result->SetBoolField(TEXT("intentional_empty_lod2"), bExpectedEmpty);
	Result->SetBoolField(TEXT("expected_nonempty"), bExpectedNonempty);
	Result->SetNumberField(TEXT("expected_components"), State.ComponentCount);
	Result->SetNumberField(TEXT("observed_components"), Current.Num());
	Result->SetNumberField(TEXT("observed_instances"), ObservedInstances);
	Result->SetNumberField(TEXT("observed_visible_components"), ObservedVisible);
	Result->SetNumberField(TEXT("observed_visible_scene_proxies"), ObservedSceneProxies);
	Result->SetBoolField(TEXT("cpu_ready"), bAllCpuReady);
	Result->SetBoolField(TEXT("scene_proxy_probe_complete"), bExpectedEmpty || bProbeMatches);
	Result->SetBoolField(TEXT("all_visible_scene_proxies_present"), bAllVisibleProxiesPresent);
	Result->SetNumberField(TEXT("scene_proxy_probe_serial"), static_cast<double>(HismProxyProbeSerial));
	Result->SetNumberField(TEXT("ready_observed_frame"), static_cast<double>(HismReadyObservedFrame));
	Result->SetBoolField(TEXT("all_ready"), bAllReady);
	Result->SetArrayField(TEXT("components"), Entries);
	return Result;
}

TSharedPtr<FJsonObject> ARokWorldInstancedMaterialDiagnosticGameMode::CaptureReadiness() const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("hism_render_readiness"), CaptureHismRenderReadiness());
	const UGameViewportClient* ViewportClient = GEngine ? GEngine->GameViewport : nullptr;
	const bool bPlatformAllowed = FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().GetPlatformApplication().IsValid()
		&& FSlateApplication::Get().GetPlatformApplication()->IsAllowedToRender();
	Result->SetNumberField(TEXT("alias_mutation_frame"), static_cast<double>(LastAliasMutationFrame));
	Result->SetNumberField(TEXT("alias_mutation_serial"), static_cast<double>(AliasMutationSerial));
	Result->SetBoolField(TEXT("platform_allowed_to_render"), bPlatformAllowed);
	Result->SetBoolField(TEXT("world_rendering_enabled"), ViewportClient
		&& !ViewportClient->bDisableWorldRendering);
	Result->SetNumberField(TEXT("game_player_count"), GEngine && GetWorld()
		? GEngine->GetNumGamePlayers(GetWorld()) : 0);
	Result->SetNumberField(TEXT("world_view_submission_frame"),
		static_cast<double>(LastWorldViewSubmissionFrame));
	Result->SetNumberField(TEXT("world_view_submission_step"), LastWorldViewSubmissionStep);
	Result->SetNumberField(TEXT("world_view_count"), LastWorldViewCount);
	Result->SetNumberField(TEXT("world_view_rect_width"), LastWorldViewRect.Width());
	Result->SetNumberField(TEXT("world_view_rect_height"), LastWorldViewRect.Height());
	Result->SetNumberField(TEXT("world_view_rect_min_x"), LastWorldViewRect.Min.X);
	Result->SetNumberField(TEXT("world_view_rect_min_y"), LastWorldViewRect.Min.Y);
	Result->SetNumberField(TEXT("world_view_rect_max_x"), LastWorldViewRect.Max.X);
	Result->SetNumberField(TEXT("world_view_rect_max_y"), LastWorldViewRect.Max.Y);
	const FIntPoint FullViewportSize = ViewportClient && ViewportClient->Viewport
		? ViewportClient->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
	Result->SetNumberField(TEXT("full_viewport_width"), FullViewportSize.X);
	Result->SetNumberField(TEXT("full_viewport_height"), FullViewportSize.Y);
	FIntRect ExpectedRect;
	FMinimalViewInfo CameraView;
	float EffectiveAspect = 0.0f;
	float ViewportDesiredAspect = 0.0f;
	const bool bExpectedRectValid = CaptureExpectedConstrainedViewRect(ExpectedRect,
		CameraView, EffectiveAspect, ViewportDesiredAspect);
	Result->SetBoolField(TEXT("expected_view_rect_valid"), bExpectedRectValid);
	Result->SetNumberField(TEXT("expected_view_rect_min_x"), ExpectedRect.Min.X);
	Result->SetNumberField(TEXT("expected_view_rect_min_y"), ExpectedRect.Min.Y);
	Result->SetNumberField(TEXT("expected_view_rect_max_x"), ExpectedRect.Max.X);
	Result->SetNumberField(TEXT("expected_view_rect_max_y"), ExpectedRect.Max.Y);
	Result->SetNumberField(TEXT("camera_view_aspect_ratio"), CameraView.AspectRatio);
	Result->SetBoolField(TEXT("camera_view_constrain_aspect_ratio"),
		CameraView.bConstrainAspectRatio);
	Result->SetNumberField(TEXT("camera_effective_aspect_ratio"), EffectiveAspect);
	Result->SetNumberField(TEXT("viewport_desired_aspect_ratio"), ViewportDesiredAspect);
	Result->SetBoolField(TEXT("world_view_rendering_showflag"), bLastWorldViewRendering);
	Result->SetBoolField(TEXT("world_view_static_meshes_showflag"), bLastWorldViewStaticMeshes);
	Result->SetBoolField(TEXT("world_view_instanced_static_meshes_showflag"),
		bLastWorldViewInstancedStaticMeshes);
	Result->SetNumberField(TEXT("viewport_rendered_frame"),
		static_cast<double>(LastViewportRenderedFrame));
	Result->SetBoolField(TEXT("submitted_view_fence_armed"), bSubmittedViewFenceArmed);
	Result->SetNumberField(TEXT("submitted_view_fence_frame"),
		static_cast<double>(SubmittedViewFenceFrame));
	Result->SetNumberField(TEXT("submitted_view_fence_step"), SubmittedViewFenceStep);
	Result->SetNumberField(TEXT("submitted_view_fence_alias_serial"),
		static_cast<double>(SubmittedViewFenceAliasSerial));
	Result->SetNumberField(TEXT("submitted_view_fence_hism_proxy_serial"),
		static_cast<double>(SubmittedViewFenceHismProxySerial));
	Result->SetNumberField(TEXT("hism_ready_observed_frame"),
		static_cast<double>(HismReadyObservedFrame));
	Result->SetBoolField(TEXT("submitted_view_fence_complete"),
		bSubmittedViewFenceArmed && SubmittedViewFence.IsFenceComplete());
	const UCameraComponent* Component = Camera.IsValid() ? Camera->GetCameraComponent() : nullptr;
	Result->SetNumberField(TEXT("camera_horizontal_fov"), Component ? Component->FieldOfView : 0.0);
	Result->SetNumberField(TEXT("camera_aspect_ratio"), Component ? Component->AspectRatio : 0.0);
	Result->SetBoolField(TEXT("camera_perspective"), Component
		&& Component->ProjectionMode == ECameraProjectionMode::Perspective);
	return Result;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::IsSubmittedViewReady() const
{
	const TSharedPtr<FJsonObject> Ready = CaptureReadiness();
	return Ready->GetBoolField(TEXT("platform_allowed_to_render"))
		&& Ready->GetBoolField(TEXT("world_rendering_enabled"))
		&& Ready->GetIntegerField(TEXT("game_player_count")) == 1
		&& Ready->GetIntegerField(TEXT("world_view_count")) == 1
		&& Ready->GetBoolField(TEXT("expected_view_rect_valid"))
		&& IsExpectedSubmittedViewRect()
		&& Ready->GetIntegerField(TEXT("world_view_rect_width"))
			== Ready->GetIntegerField(TEXT("expected_view_rect_max_x"))
				- Ready->GetIntegerField(TEXT("expected_view_rect_min_x"))
		&& Ready->GetIntegerField(TEXT("world_view_rect_height"))
			== Ready->GetIntegerField(TEXT("expected_view_rect_max_y"))
				- Ready->GetIntegerField(TEXT("expected_view_rect_min_y"))
		&& Ready->GetBoolField(TEXT("world_view_rendering_showflag"))
		&& Ready->GetBoolField(TEXT("world_view_static_meshes_showflag"))
		&& Ready->GetBoolField(TEXT("world_view_instanced_static_meshes_showflag"))
		&& Ready->GetObjectField(TEXT("hism_render_readiness"))->GetBoolField(TEXT("all_ready"))
		&& Ready->GetBoolField(TEXT("camera_perspective"))
		&& Ready->GetNumberField(TEXT("camera_horizontal_fov")) > 0.0
		&& Ready->GetBoolField(TEXT("submitted_view_fence_complete"))
		&& SubmittedViewFenceStep == Step
		&& SubmittedViewFenceAliasSerial == AliasMutationSerial
		&& SubmittedViewFenceHismProxySerial == HismProxyProbeSerial
		&& SubmittedViewFenceFrame >= LastAliasMutationFrame
		&& HismReadyObservedFrame != 0
		&& SubmittedViewFenceFrame >= HismReadyObservedFrame
		&& LastWorldViewSubmissionStep == Step
		&& LastWorldViewSubmissionFrame >= SubmittedViewFenceFrame
		&& LastViewportRenderedFrame >= SubmittedViewFenceFrame
		&& GFrameCounter > SubmittedViewFenceFrame;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::ApplyAliasesToActiveComponents(FString& OutReason)
{
	if (!bAliasLoaded || !Streaming.IsValid())
	{
		OutReason = TEXT("Pinned diagnostic alias was not loaded.");
		return false;
	}
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Streaming->GetComponents(Components);
	for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
	{
		if (!IsValid(Component) || Component->GetOwner() != Streaming.Get()
			|| !Component->IsRegistered())
		{
			continue;
		}
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			const UMaterialInterface* Before = Component->GetMaterial(Index);
			const FString BeforePath = Before ? Before->GetPathName() : TEXT("");
			const FString* Target = AliasBySource.Find(BeforePath);
			const FString* Source = SourceByAlias.Find(BeforePath);
			if (!Target && !Source)
			{
				OutReason = FString::Printf(TEXT("Unexpected source HISM material slot %s[%d]=%s"),
					*Component->GetName(), Index, *BeforePath);
				return false;
			}
			const FString ExpectedTarget = Target ? *Target : BeforePath;
			const FString SlotKey = Component->GetName() + TEXT("#") + FString::FromInt(Index);
			if (!ObservedBeforeAliasBySlot.Contains(SlotKey))
			{
				ObservedBeforeAliasBySlot.Add(SlotKey, BeforePath);
			}
			if (Target)
			{
				UMaterialInterface* Replacement = LoadObject<UMaterialInterface>(nullptr, **Target);
				const UMaterial* Base = Replacement ? Replacement->GetMaterial() : nullptr;
				if (!Replacement || Replacement->GetClass() != UMaterial::StaticClass()
					|| !NativeMaterialReady(Base))
				{
					OutReason = TEXT("Alias target load or instancing usage failed before SetMaterial.");
					return false;
				}
				Component->SetMaterial(Index, Replacement);
				LastAliasMutationFrame = GFrameCounter;
				++AliasMutationSerial;
				bSubmittedViewFenceArmed = false;
			}
			const UMaterialInterface* After = Component->GetMaterial(Index);
			if (!After || After->GetPathName() != ExpectedTarget)
			{
				OutReason = TEXT("HISM material alias did not bind to its exact slot target.");
				return false;
			}
		}
	}
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CaptureCoveragePinnedSources(
	TSharedPtr<FJsonObject>& OutPins, FString& OutReason) const
{
	OutPins = MakeShared<FJsonObject>();
	const auto AddFile = [&OutPins, &OutReason](const TCHAR* Field, const TCHAR* Relative)
	{
		FString Digest;
		if (!FileSha256(Relative, Digest))
		{
			OutReason = FString::Printf(TEXT("Coverage source pin unreadable: %s"), Relative);
			return false;
		}
		OutPins->SetStringField(Field, Digest);
		return true;
	};
	if (!AddFile(TEXT("candidate_header_sha256"),
		TEXT("Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.h"))
		|| !AddFile(TEXT("candidate_cpp_sha256"),
			TEXT("Source/Rok/RokWorldInstancedMaterialDiagnosticGameMode.cpp"))
		|| !AddFile(TEXT("candidate_verifier_sha256"),
			TEXT("Scripts/VerifyRokWorldAliasPixelCoverageDiagnosticV14.py"))
		|| !AddFile(TEXT("camera_header_sha256"), TEXT("Source/Rok/RokWorldMapCameraPawn.h"))
		|| !AddFile(TEXT("camera_cpp_sha256"), TEXT("Source/Rok/RokWorldMapCameraPawn.cpp"))
		|| !AddFile(TEXT("city_view_cpp_sha256"), TEXT("Source/Rok/RokCityViewTypes.cpp"))
		|| !AddFile(TEXT("unity_camera_param_csv_sha256"),
			TEXT("ref/config-tables/编译客户端配置/CSV/CameraParamLod.csv"))
		|| !AddFile(TEXT("camera_param_table_json_sha256"),
			TEXT("Saved/RokUnityMigration/ConfigTables/CameraParamLod.json"))
		|| !AddFile(TEXT("streaming_header_sha256"),
			TEXT("Source/Rok/RokWorldMapStreamingActor.h"))
		|| !AddFile(TEXT("streaming_cpp_sha256"),
			TEXT("Source/Rok/RokWorldMapStreamingActor.cpp")))
	{
		return false;
	}
	const URokWorldMapRuntimeDataAsset* Data = Streaming.IsValid()
		? Streaming->RuntimeData.Get() : nullptr;
	if (!Data || Data->CanonicalPayloadSha256.IsEmpty() || AliasInputSha256.IsEmpty()
		|| VariantsReceiptSha256.IsEmpty() || RiverReceiptSha256.IsEmpty()
		|| BridgeReceiptSha256.IsEmpty())
	{
		OutReason = TEXT("Coverage payload or alias creator source pin missing.");
		return false;
	}
	OutPins->SetStringField(TEXT("runtime_payload_sha256"), Data->CanonicalPayloadSha256);
	OutPins->SetStringField(TEXT("alias_input_sha256"), AliasInputSha256);
	OutPins->SetStringField(TEXT("variants_creator_receipt_sha256"), VariantsReceiptSha256);
	OutPins->SetStringField(TEXT("river_creator_receipt_sha256"), RiverReceiptSha256);
	OutPins->SetStringField(TEXT("bridge_creator_receipt_sha256"), BridgeReceiptSha256);
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CaptureCoverageProjection(
	TSharedPtr<FJsonObject>& OutProjection, FString& OutReason) const
{
	FIntRect Rect;
	FMinimalViewInfo View;
	float EffectiveAspect = 0.0f, DesiredAspect = 0.0f;
	if (!CaptureExpectedConstrainedViewRect(Rect, View, EffectiveAspect, DesiredAspect)
		|| Rect != FIntRect(72, 0, 1368, 810))
	{
		OutReason = TEXT("Coverage camera constrained view rect changed.");
		return false;
	}
	const FVector4f& Crop = View.AsymmetricCropFraction;
	if (View.OffCenterProjectionOffset != FVector2D::ZeroVector
		|| View.OverscanResolutionFraction != 1.0f
		|| View.CropFraction != 1.0f
		|| Crop.X != 1.0f || Crop.Y != 1.0f
		|| Crop.Z != 1.0f || Crop.W != 1.0f
		|| View.GetOverscan() != 0.0f)
	{
		OutReason = TEXT("Coverage camera public projection modifier is non-neutral.");
		return false;
	}
	FMinimalViewInfo WithoutPrivateOverscan = View;
	WithoutPrivateOverscan.ClearOverscan();
	const FMatrix ActualProjection = View.CalculateProjectionMatrix();
	const FMatrix NeutralProjection = WithoutPrivateOverscan.CalculateProjectionMatrix();
	if (!ActualProjection.Equals(NeutralProjection, 0.000001))
	{
		OutReason = TEXT("Coverage camera private asymmetric overscan changes projection.");
		return false;
	}
	OutProjection = MakeShared<FJsonObject>();
	OutProjection->SetArrayField(TEXT("view_rect_xyxy"), RectNumbers(Rect));
	OutProjection->SetArrayField(TEXT("camera_view_transform"),
		TransformNumbers(FTransform(View.Rotation.Quaternion(), View.Location)));
	OutProjection->SetNumberField(TEXT("fov"), View.FOV);
	OutProjection->SetNumberField(TEXT("aspect"), View.AspectRatio);
	OutProjection->SetArrayField(TEXT("off_center_projection_offset"),
		Vector2Numbers(View.OffCenterProjectionOffset));
	OutProjection->SetNumberField(TEXT("overscan_resolution_fraction"),
		View.OverscanResolutionFraction);
	OutProjection->SetNumberField(TEXT("crop_fraction"), View.CropFraction);
	OutProjection->SetArrayField(TEXT("asymmetric_crop_fraction"),
		TArray<TSharedPtr<FJsonValue>>{JsonNumber(Crop.X), JsonNumber(Crop.Y),
			JsonNumber(Crop.Z), JsonNumber(Crop.W)});
	OutProjection->SetNumberField(TEXT("uniform_overscan"), View.GetOverscan());
	OutProjection->SetArrayField(TEXT("actual_projection_matrix"),
		MatrixNumbers(ActualProjection));
	OutProjection->SetArrayField(TEXT("neutral_projection_matrix"),
		MatrixNumbers(NeutralProjection));
	OutProjection->SetBoolField(TEXT("private_asymmetric_overscan_matrix_neutral"), true);
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CheckCoverageStableInputs(
	TSharedPtr<FJsonObject>& OutProjection, FString& OutReason) const
{
	TSharedPtr<FJsonObject> Pins;
	if (!CaptureCoverageProjection(OutProjection, OutReason)
		|| !CaptureCoveragePinnedSources(Pins, OutReason))
	{
		return false;
	}
	if (SerializeState(OutProjection) != CoverageProjectionRegistrationJson
		|| SerializeState(Pins) != CoveragePinnedSourcesJson)
	{
		OutReason = TEXT("Coverage projection modifier or pinned source bytes drifted.");
		return false;
	}
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::LocateCoverageTarget(FString& OutReason)
{
	const URokWorldMapRuntimeDataAsset* Data = Streaming.IsValid() ? Streaming->RuntimeData.Get() : nullptr;
	if (!Data || !Data->Projection.IsValid()
		|| !Data->Projection.UnityRebaseOrigin.Equals(FVector2D(6876.0, 942.0), 0.001)
		|| !FMath::IsNearlyEqual(Data->Projection.UeUnitsPerUnityUnit, 500.0f, 0.001f))
	{
		OutReason = TEXT("Source projection for the registered tree focus changed.");
		return false;
	}
	FIntRect ViewRect;
	FMinimalViewInfo CameraView;
	float EffectiveAspect = 0.0f, DesiredAspect = 0.0f;
	if (!CaptureExpectedConstrainedViewRect(ViewRect, CameraView, EffectiveAspect, DesiredAspect)
		|| ViewRect != FIntRect(72, 0, 1368, 810))
	{
		OutReason = TEXT("Source-derived constrained camera view is unavailable before ROI registration.");
		return false;
	}
	if (!CaptureCoverageProjection(CoverageProjectionRegistration, OutReason)
		|| !CaptureCoveragePinnedSources(CoveragePinnedSources, OutReason))
	{
		return false;
	}
	CoverageProjectionRegistrationJson = SerializeState(CoverageProjectionRegistration);
	CoveragePinnedSourcesJson = SerializeState(CoveragePinnedSources);
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Streaming->GetComponents(Components);
	int32 Matches = 0;
	for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
	{
		if (!IsValid(Component) || Component->GetOwner() != Streaming.Get()
			|| !Component->IsRegistered() || Component->GetName() != CoverageComponentName)
		{
			continue;
		}
		const UStaticMesh* Mesh = Component->GetStaticMesh();
		const UMaterialInterface* Material = Component->GetMaterial(0);
		if (!Mesh || Mesh->GetPathName() != CoverageMeshPath
			|| Component->GetNumMaterials() != 1 || !Material
			|| Material->GetPathName() != CoverageTargetMaterial
			|| SourceByAlias.FindRef(Material->GetPathName()) != CoverageSourceMaterial
			|| ObservedBeforeAliasBySlot.FindRef(Component->GetName() + TEXT("#0"))
				!= CoverageSourceMaterial
			|| Component->GetInstanceCount() != 7 || !Component->IsVisible())
		{
			OutReason = TEXT("Source tree mesh, seven transforms or exact pinned alias slot changed.");
			return false;
		}
		for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
		{
			FTransform Local;
			if (!Component->GetInstanceTransform(Index, Local, false))
			{
				OutReason = TEXT("Source tree instance transform could not be read.");
				return false;
			}
			const FVector P = Local.GetTranslation();
			if (FMath::Abs(P.X + 532850.0005) <= 0.05
				&& FMath::Abs(P.Y + 1299.999) <= 0.05
				&& FMath::Abs(P.Z - 50.0) <= 0.05)
			{
				CoverageTarget = Component;
				CoverageInstanceIndex = Index;
				++Matches;
			}
		}
	}
	if (Matches != 1 || !CoverageTarget.IsValid())
	{
		OutReason = TEXT("Pre-registered source tree instance is absent or ambiguous.");
		return false;
	}
	CoverageMeshLocalBounds = CoverageTarget->GetStaticMesh()->GetBoundingBox();
	CoverageCameraViewTransform = FTransform(CameraView.Rotation.Quaternion(), CameraView.Location);
	CoverageCameraViewFov = CameraView.FOV;
	CoverageCameraViewAspect = CameraView.AspectRatio;
	CoveragePreflightObservation = MakeShared<FJsonObject>();
	CoveragePreflightObservation->SetArrayField(TEXT("mesh_local_bounds_min"),
		Vector3Numbers(CoverageMeshLocalBounds.Min));
	CoveragePreflightObservation->SetArrayField(TEXT("mesh_local_bounds_max"),
		Vector3Numbers(CoverageMeshLocalBounds.Max));
	CoveragePreflightObservation->SetArrayField(TEXT("camera_view_transform"),
		TransformNumbers(CoverageCameraViewTransform));
	CoveragePreflightObservation->SetNumberField(TEXT("camera_view_fov"), CameraView.FOV);
	CoveragePreflightObservation->SetNumberField(TEXT("camera_view_aspect"), CameraView.AspectRatio);
	CoveragePreflightObservation->SetNumberField(TEXT("camera_dxf"), Camera->GetCurrentDxf());
	const TArray<FRokCityViewCameraKey> SourceKeys = FRokCityViewMath::MakeDefaultCameraKeys();
	const float SourceDxf = FRokWorldMapMath::GetNamedDxf(
		SourceKeys, FName(TEXT("showbuild")), -1.0f);
	float SourceDistance = 0.0f, SourceVerticalFov = 0.0f;
	const bool bSourceKeyValid = FMath::IsNearlyEqual(SourceDxf, CoverageDxf, 0.001f)
		&& FRokCityViewMath::GetDistFovByDxf(
			SourceKeys, SourceDxf, SourceDistance, SourceVerticalFov)
		&& FMath::IsNearlyEqual(SourceDistance, 33.333f, 0.001f)
		&& FMath::IsNearlyEqual(SourceVerticalFov, 30.0f, 0.001f);
	CoveragePreflightObservation->SetBoolField(TEXT("source_showbuild_key_valid"), bSourceKeyValid);
	CoveragePreflightObservation->SetNumberField(TEXT("source_showbuild_dxf"), SourceDxf);
	CoveragePreflightObservation->SetNumberField(TEXT("source_showbuild_distance_unity"), SourceDistance);
	CoveragePreflightObservation->SetNumberField(TEXT("source_showbuild_vertical_fov"), SourceVerticalFov);
	const FVector ExpectedGround = Data->Projection.UnityToUe(CoverageFocus);
	const double Diagonal = FMath::Sqrt(0.5);
	const FVector ExpectedForward(0.0, Diagonal, -Diagonal);
	const FVector ExpectedCameraLocation = ExpectedGround
		- ExpectedForward * (SourceDistance * Data->Projection.GetSafeUeScale());
	const bool bSourceViewValid = bSourceKeyValid
		&& FMath::IsNearlyEqual(Camera->GetCurrentDxf(), CoverageDxf, 0.001f)
		&& Camera->GetViewCenterUnity().Equals(CoverageFocus, 0.001)
		&& CameraView.Location.Equals(ExpectedCameraLocation, 2.0)
		&& CameraView.Rotation.Quaternion().GetAxisX().Equals(ExpectedForward, 0.0001)
		&& FMath::IsNearlyEqual(CameraView.FOV,
			FRokCityViewMath::VerticalToHorizontalFov(SourceVerticalFov, SourceCameraAspectRatio),
			0.001f);
	CoveragePreflightObservation->SetArrayField(TEXT("source_expected_camera_location"),
		Vector3Numbers(ExpectedCameraLocation));
	CoveragePreflightObservation->SetBoolField(TEXT("source_camera_view_valid"), bSourceViewValid);
	if (!bSourceViewValid)
	{
		OutReason = TEXT("Actual coverage camera does not follow pinned Unity showbuild distance/FOV/focus.");
		return false;
	}
	bool bFullyBehind = false;
	if (!ProjectCoverageInstanceBounds(CoverageTarget.Get(), CoverageInstanceIndex,
		CameraView, ViewRect, CoverageProjectedRect, CoverageInstanceWorld, bFullyBehind)
		|| bFullyBehind)
	{
		OutReason = TEXT("Target tree mesh world bounds do not project wholly in front of the source camera.");
		return false;
	}
	CoveragePreflightObservation->SetArrayField(TEXT("projected_rect_xyxy"),
		RectNumbers(CoverageProjectedRect));
	CoveragePreflightObservation->SetArrayField(TEXT("constrained_view_rect_xyxy"),
		RectNumbers(ViewRect));
	const FVector ActualPivot = CoverageInstanceWorld.GetTranslation();
	const bool bPivotMatchesSource = FMath::Abs(ExpectedGround.X - ActualPivot.X) <= 0.1
		&& FMath::Abs(ExpectedGround.Y - ActualPivot.Y) <= 0.1
		&& FMath::Abs(ActualPivot.Z - 50.0) <= 0.1
		&& Data->Projection.UeToUnity(FVector(ActualPivot.X, ActualPivot.Y, 0))
			.Equals(CoverageFocus, 0.001);
	const bool bLeftInside = CoverageProjectedRect.Min.X >= ViewRect.Min.X;
	const bool bTopInside = CoverageProjectedRect.Min.Y >= ViewRect.Min.Y;
	const bool bRightInside = CoverageProjectedRect.Max.X <= ViewRect.Max.X;
	const bool bBottomInside = CoverageProjectedRect.Max.Y <= ViewRect.Max.Y;
	const bool bAreaEnough = CoverageProjectedRect.Width() * CoverageProjectedRect.Height()
		>= CoverageMinConnectedPixels;
	CoveragePreflightObservation->SetBoolField(TEXT("pivot_matches_source"), bPivotMatchesSource);
	CoveragePreflightObservation->SetBoolField(TEXT("left_inside_view"), bLeftInside);
	CoveragePreflightObservation->SetBoolField(TEXT("top_inside_view"), bTopInside);
	CoveragePreflightObservation->SetBoolField(TEXT("right_inside_view"), bRightInside);
	CoveragePreflightObservation->SetBoolField(TEXT("bottom_inside_view"), bBottomInside);
	CoveragePreflightObservation->SetBoolField(TEXT("projected_area_enough"), bAreaEnough);
	if (!bPivotMatchesSource || !bLeftInside || !bTopInside || !bRightInside
		|| !bBottomInside || !bAreaEnough)
	{
		OutReason = TEXT("Source-derived target pivot or full-mesh ROI failed a recorded pre-capture bound.");
		return false;
	}
	CoverageRoi = CoverageProjectedRect; // No crop or rewrite of the full-size PNG.
	for (int32 Index = 0; Index < CoverageTarget->GetInstanceCount(); ++Index)
	{
		if (Index == CoverageInstanceIndex) continue;
		FIntRect Other;
		FTransform OtherWorld;
		bool bOtherFullyBehind = false;
		if (!ProjectCoverageInstanceBounds(CoverageTarget.Get(), Index, CameraView,
			ViewRect, Other, OtherWorld, bOtherFullyBehind))
		{
			OutReason = TEXT("Another instance of the same HISM has an ambiguous near-plane bound.");
			return false;
		}
		if (!bOtherFullyBehind && Other.Min.X < CoverageRoi.Max.X
			&& Other.Max.X > CoverageRoi.Min.X
			&& Other.Min.Y < CoverageRoi.Max.Y && Other.Max.Y > CoverageRoi.Min.Y)
		{
			OutReason = TEXT("Another of seven HISM tree instances overlaps the target ROI.");
			return false;
		}
		CoverageOtherInstanceWorldTransforms.Add(OtherWorld);
	}
	bCoverageOtherSixExcluded = true;
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CoverageComparableState(
	const TSharedPtr<FJsonObject>& State, FString& OutComparable) const
{
	TSharedPtr<FJsonObject> Copy;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializeState(State));
	if (!FJsonSerializer::Deserialize(Reader, Copy) || !Copy.IsValid()) return false;
	// Native readiness is phase-local evidence, not part of the A1/A2/B/A3
	// cross-phase scene identity comparison. State itself remains untouched.
	Copy->RemoveField(TEXT("hism_render_readiness"));
	Copy->SetNumberField(TEXT("hism_visible_component_count"),
		Copy->GetIntegerField(TEXT("hism_component_count")));
	int32 Matches = 0;
	for (const TSharedPtr<FJsonValue>& Value : Copy->GetArrayField(TEXT("actual_hism_components")))
	{
		const TSharedPtr<FJsonObject> Item = Value->AsObject();
		if (Item.IsValid() && Item->GetStringField(TEXT("name")) == CoverageComponentName)
		{
			Item->SetBoolField(TEXT("visible"), true);
			++Matches;
		}
	}
	if (Matches != 1) return false;
	OutComparable = SerializeState(Copy);
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::SetCoverageTargetHidden(
	bool bShouldHideCoverageTarget, FString& OutReason)
{
	if (bShouldHideCoverageTarget == bCoverageTargetHidden || !CoverageTarget.IsValid()
		|| CoverageTarget->GetOwner() != Streaming.Get()
		|| !CoverageTarget->IsRegistered()
		|| CoverageTarget->IsVisible() != !bCoverageTargetHidden)
	{
		OutReason = TEXT("Coverage target lost identity, registration or expected visibility.");
		return false;
	}
	if (!bShouldHideCoverageTarget) bCoverageRestoreAttempted = true;
	CoverageTarget->SetVisibility(!bShouldHideCoverageTarget, true);
	if (CoverageTarget->IsVisible() == bShouldHideCoverageTarget)
	{
		OutReason = TEXT("Target HISM visibility transition did not take effect.");
		return false;
	}
	bCoverageTargetHidden = bShouldHideCoverageTarget;
	if (!bShouldHideCoverageTarget) bCoverageRestored = true;
	LastAliasMutationFrame = GFrameCounter;
	++AliasMutationSerial;
	ClearStability(); // Force a new submitted world view/fence after visibility mutation.
	return true;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::TickCoverage(
	const FRokWorldMapStreamingDiagnosticState& State)
{
	if (Step == 0)
	{
		Camera->SetViewCenterUnity(CoverageFocus);
		Camera->SetCurrentDxf(CoverageDxf);
		Step = 1;
		ClearStability();
		return;
	}
	if (!State.bHasRequestedState || State.bPendingLoad || State.Lod != 0
		|| State.CenterChunk != FIntPoint(16, 2) || State.InstanceCount != MovedInstances
		|| State.ComponentCount <= 0)
	{
		ClearStability();
		return;
	}
	if (Step == 1 && !CoverageTarget.IsValid())
	{
		if (State.VisibleComponentCount != State.ComponentCount) return;
		FString Reason;
		if (!LocateCoverageTarget(Reason)) { Fail(Reason); return; }
		CoverageGeneration = State.Generation;
	}
	if (!CoverageTarget.IsValid() || State.Generation != CoverageGeneration
		|| CoverageTarget->GetName() != CoverageComponentName
		|| CoverageTarget->IsVisible() == bCoverageTargetHidden
		|| State.VisibleComponentCount != State.ComponentCount - (bCoverageTargetHidden ? 1 : 0)
		|| !FMath::IsNearlyEqual(Camera->GetCurrentDxf(), CoverageDxf, 0.001f)
		|| !Camera->GetViewCenterUnity().Equals(CoverageFocus, 0.001))
	{
		Fail(TEXT("Coverage camera, streaming generation, source tree or visibility changed."));
		return;
	}
	switch (Step)
	{
	case 1: Record(TEXT("coverage_a1")); break;
	case 2: Record(TEXT("coverage_a2")); break;
	case 3: Record(TEXT("coverage_b_target_hidden")); break;
	case 4: Record(TEXT("coverage_a3_restored")); break;
	default: Fail(TEXT("Invalid opt-in coverage phase.")); break;
	}
}

void ARokWorldInstancedMaterialDiagnosticGameMode::TickFixedCameraProbe(
	const FRokWorldMapStreamingDiagnosticState& State)
{
	if (Step == 0)
	{
		Camera->SetViewCenterUnity(MovedFocus);
		Camera->SetCurrentDxf(InitialDxf);
		if (!Streaming->SetFixedCameraProbeLod(0))
		{
			Fail(TEXT("Fixed-camera probe LOD0 request rejected by scoped actor."));
			return;
		}
		Step = 1;
		ClearStability();
		return;
	}
	if (!FMath::IsNearlyEqual(Camera->GetCurrentDxf(), InitialDxf, 0.001f)
		|| !Camera->GetViewCenterUnity().Equals(MovedFocus, 0.001))
	{
		Fail(TEXT("Fixed-camera probe camera pose or DXF changed."));
		return;
	}
	const int32 ExpectedLod = Step == 3 ? 1 : 0;
	if (!State.bHasRequestedState || State.bPendingLoad
		|| State.Lod != ExpectedLod || State.CenterChunk != FIntPoint(16, 2)
		|| State.InstanceCount != MovedInstances || State.ComponentCount <= 0
		|| State.VisibleComponentCount != State.ComponentCount
		|| (Step == 3 && State.Generation <= FixedProbeLod0Generation)
		|| (Step == 4 && State.Generation <= FixedProbeLod1Generation))
	{
		ClearStability();
		return;
	}
	switch (Step)
	{
	case 1: Record(TEXT("fixed_lod0_a1")); break;
	case 2:
		if (State.Generation != FixedProbeLod0Generation)
		{
			Fail(TEXT("LOD0 generation changed between fixed-camera controls."));
			return;
		}
		Record(TEXT("fixed_lod0_a2"));
		break;
	case 3: Record(TEXT("fixed_lod1_b")); break;
	case 4: Record(TEXT("fixed_lod0_a3")); break;
	default: Fail(TEXT("Invalid fixed-camera probe step.")); break;
	}
}

void ARokWorldInstancedMaterialDiagnosticGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFinished)
	{
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	if (FPlatformTime::Seconds() - StartedAtSeconds > 90.0)
	{
		Fail(FString::Printf(TEXT("Timeout at diagnostic step %d."), Step));
		return;
	}
	if (!Camera.IsValid())
	{
		for (TActorIterator<ARokWorldMapCameraPawn> It(GetWorld()); It; ++It)
		{
			Camera = *It;
			break;
		}
		if (!Camera.IsValid())
		{
			return;
		}
	}
	if (!Streaming.IsValid())
	{
		Fail(TEXT("Streaming actor was destroyed."));
		return;
	}
	if (PendingSample.IsValid())
	{
		CheckPendingScreenshot();
		return;
	}
	FString AliasReason;
	if (!ApplyAliasesToActiveComponents(AliasReason))
	{
		Fail(AliasReason);
		return;
	}
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	if (!State.bRegisteredMatchesActive)
	{
		Record(TEXT("registered_set_mismatch"));
		Fail(TEXT("Registered HISM set differs from ActiveComponents."));
		return;
	}
	if (bCoverageMode)
	{
		TickCoverage(State);
		return;
	}
	if (bFixedCameraProbeMode)
	{
		TickFixedCameraProbe(State);
		return;
	}
	if (Step == 0)
	{
		Camera->SetViewCenterUnity(InitialFocus);
		Camera->SetCurrentDxf(InitialDxf);
		Step = 1;
		ClearStability();
		return;
	}
	if (Step == 1 && State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 0 && State.CenterChunk == FIntPoint(19, 2)
		&& State.InstanceCount == InitialInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod0_initial_edge"));
		return;
	}
	if (Step == 2 && State.Generation > InitialGeneration
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 0 && State.CenterChunk == FIntPoint(16, 2)
		&& State.InstanceCount == MovedInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		for (const FName OldName : InitialComponentNames)
		{
			if (State.ComponentNames.Contains(OldName))
			{
				Fail(TEXT("Old HISM component survived camera movement/unload."));
				return;
			}
		}
		Record(TEXT("lod0_moved_west_three_chunks"));
		return;
	}
	if (Step == 3 && State.Generation > MovedGeneration
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 1 && State.CenterChunk == FIntPoint(16, 2)
		&& State.InstanceCount == MovedInstances && State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod1_moved"));
		return;
	}
	if (Step == 4 && State.Generation > LodOneGeneration
		&& !State.bHasRequestedState && !State.bPendingLoad
		&& State.ComponentCount == 0 && State.VisibleComponentCount == 0
		&& State.InstanceCount == 0
		&& State.RegisteredComponentCount == 0 && State.RegisteredInstanceCount == 0)
	{
		Record(TEXT("lod2_refused_unloaded"));
		return;
	}
	if (Step == 5 && State.Generation > LodOneGeneration + 1
		&& State.bHasRequestedState && !State.bPendingLoad
		&& State.Lod == 1 && State.InstanceCount == MovedInstances
		&& State.ComponentCount > 0
		&& State.VisibleComponentCount == State.ComponentCount)
	{
		Record(TEXT("lod1_restored_after_refusal"));
		return;
	}
	ClearStability();
}

TSharedPtr<FJsonObject> ARokWorldInstancedMaterialDiagnosticGameMode::CaptureVisualState() const
{
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	TSharedPtr<FJsonObject> Sample = MakeShared<FJsonObject>();
	WriteLocalOnlyBoundary(*Sample);
	Sample->SetNumberField(TEXT("lod"), State.Lod);
	Sample->SetNumberField(TEXT("center_chunk_x"), State.CenterChunk.X);
	Sample->SetNumberField(TEXT("center_chunk_y"), State.CenterChunk.Y);
	Sample->SetNumberField(TEXT("generation"), static_cast<double>(State.Generation));
	Sample->SetNumberField(TEXT("hism_component_count"), State.ComponentCount);
	Sample->SetNumberField(TEXT("hism_visible_component_count"), State.VisibleComponentCount);
	Sample->SetNumberField(TEXT("hism_instance_count"), State.InstanceCount);
	Sample->SetNumberField(TEXT("registered_hism_component_count"), State.RegisteredComponentCount);
	Sample->SetNumberField(TEXT("registered_hism_instance_count"), State.RegisteredInstanceCount);
	Sample->SetBoolField(TEXT("registered_matches_active"), State.bRegisteredMatchesActive);
	Sample->SetBoolField(TEXT("pending_load"), State.bPendingLoad);
	Sample->SetBoolField(TEXT("has_requested_state"), State.bHasRequestedState);
	Sample->SetNumberField(TEXT("camera_dxf"), Camera->GetCurrentDxf());
	const FVector2D Focus = Camera->GetViewCenterUnity();
	Sample->SetNumberField(TEXT("camera_unity_x"), Focus.X);
	Sample->SetNumberField(TEXT("camera_unity_y"), Focus.Y);
	Sample->SetArrayField(TEXT("camera_actor_transform"), TransformNumbers(Camera->GetActorTransform()));
	const APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	Sample->SetBoolField(TEXT("camera_is_player_pawn"), Controller && Controller->GetPawn() == Camera.Get());
	Sample->SetBoolField(TEXT("camera_is_view_target"), Controller && Controller->GetViewTarget() == Camera.Get());
	Sample->SetBoolField(TEXT("camera_component_active"),
		Camera->GetCameraComponent() && Camera->GetCameraComponent()->IsActive());
	Sample->SetBoolField(TEXT("streaming_root_static"), Streaming->GetRootComponent()
		&& Streaming->GetRootComponent()->Mobility == EComponentMobility::Static);
	Sample->SetBoolField(TEXT("instanced_alias_loaded"), bAliasLoaded);
	Sample->SetStringField(TEXT("instanced_alias_sha256"), AliasInputSha256);
	int32 DirectionalLights = 0;
	int32 SkyLights = 0;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It) { ++DirectionalLights; }
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It) { ++SkyLights; }
	Sample->SetNumberField(TEXT("directional_light_count"), DirectionalLights);
	Sample->SetNumberField(TEXT("sky_light_count"), SkyLights);
	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FName Name : State.ComponentNames)
	{
		Names.Add(MakeShared<FJsonValueString>(Name.ToString()));
	}
	Sample->SetArrayField(TEXT("hism_component_names"), Names);
	TArray<TSharedPtr<FJsonValue>> RegisteredNames;
	for (const FName Name : State.RegisteredComponentNames)
	{
		RegisteredNames.Add(JsonString(Name.ToString()));
	}
	Sample->SetArrayField(TEXT("registered_hism_component_names"), RegisteredNames);
	Sample->SetArrayField(TEXT("actual_hism_components"), CaptureActualComponents());
	Sample->SetObjectField(TEXT("hism_render_readiness"), CaptureHismRenderReadiness());
	return Sample;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::ClearStability()
{
	StableStep = -1;
	StableObservationCount = 0;
	StableLastFrame = 0;
	StableStateJson.Empty();
	bSubmittedViewFenceArmed = false;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::Record(const FString& Label)
{
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		Fail(BoundaryReason);
		return;
	}
	TSharedPtr<FJsonObject> State = CaptureVisualState();
	LastObservedSceneState = State;
	LastObservedSceneFrame = GFrameCounter;
	if (!State->GetBoolField(TEXT("camera_is_player_pawn"))
		|| !State->GetBoolField(TEXT("camera_is_view_target"))
		|| !State->GetBoolField(TEXT("camera_component_active"))
		|| !State->GetBoolField(TEXT("streaming_root_static"))
		|| !State->GetBoolField(TEXT("instanced_alias_loaded"))
		|| State->GetIntegerField(TEXT("directional_light_count")) != 1
		|| State->GetIntegerField(TEXT("sky_light_count")) != 0)
	{
		Fail(TEXT("Diagnostic camera/view target or streaming root is not the actual active scene."));
		return;
	}
	if (State->GetIntegerField(TEXT("hism_component_count")) > 0)
	{
		bool bRenderReady = State->GetObjectField(TEXT("hism_render_readiness"))
			->GetBoolField(TEXT("all_ready"));
		for (const TSharedPtr<FJsonValue>& Entry : State->GetArrayField(TEXT("actual_hism_components")))
		{
			const TSharedPtr<FJsonObject> Component = Entry->AsObject();
			if (!Component.IsValid() || !Component->GetBoolField(TEXT("attached_to_streaming_root")))
			{
				Fail(TEXT("Registered HISM is detached from the static streaming root."));
				return;
			}
			bRenderReady &= Component->GetBoolField(TEXT("render_state_created"))
				&& Component->GetBoolField(TEXT("nonzero_world_bounds"));
			for (const TSharedPtr<FJsonValue>& SlotValue : Component->GetArrayField(TEXT("material_aliases")))
			{
				const TSharedPtr<FJsonObject> Slot = SlotValue->AsObject();
				if (!Slot.IsValid() || Slot->GetStringField(TEXT("source_object")).IsEmpty()
					|| Slot->GetStringField(TEXT("actual_object"))
						!= Slot->GetStringField(TEXT("target_object"))
					|| !Slot->GetBoolField(TEXT("base_instanced_static_mesh_usage"))
					|| !Slot->GetBoolField(TEXT("native_shader_map_available"))
					|| Slot->GetStringField(TEXT("base_material"))
						!= Slot->GetStringField(TEXT("actual_object"))
					|| Slot->GetBoolField(TEXT("source_passthrough"))
					|| Slot->GetStringField(TEXT("creator_terminal_sha256")).IsEmpty())
				{
					Fail(TEXT("A live HISM slot lacks its pinned instanced-material alias or creator proof."));
					return;
				}
			}
		}
		if (!bRenderReady)
		{
			// Render proxy and bounds updates can lag RegisterComponent/AddInstances.
			// The existing 90-second diagnostic timeout remains the hard failure.
			ClearStability();
			return;
		}
	}
	else if (!State->GetObjectField(TEXT("hism_render_readiness"))
		->GetBoolField(TEXT("all_ready")))
	{
		ClearStability();
		return;
	}
	const FString StateJson = SerializeState(State);
	if (StableStep != Step || StableStateJson != StateJson || GFrameCounter <= StableLastFrame)
	{
		StableStep = Step;
		StableStateJson = StateJson;
		StableLastFrame = GFrameCounter;
		StableObservationCount = 1;
		return;
	}
	StableLastFrame = GFrameCounter;
	++StableObservationCount;
	if (StableObservationCount < 3)
	{
		return;
	}
	if (!IsSubmittedViewReady())
	{
		// No timer-only waiver: the matching game viewport must submit a world
		// view after the last alias change, and its RT fence must retire.
		return;
	}
	if (bCoverageMode)
	{
		FString Comparable;
		if (!CoverageComparableState(State, Comparable)
			|| (!CoverageComparableBaseline.IsEmpty()
				&& CoverageComparableBaseline != Comparable))
		{
			Fail(TEXT("Coverage camera, generation, transforms, alias, lighting or HTTP state drifted across captures."));
			return;
		}
		if (CoverageComparableBaseline.IsEmpty()) CoverageComparableBaseline = Comparable;
	}
	if (!GEngine || !GEngine->GameViewport || GEngine->GameViewport->GetWorld() != GetWorld()
		|| !GEngine->GameViewport->Viewport
		|| UGameViewportClient::OnScreenshotCaptured().IsBound()
		|| FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(TEXT("Game viewport unavailable or another screenshot capture owns the frame."));
		return;
	}
	const FIntPoint Extent = GEngine->GameViewport->Viewport->GetSizeXY();
	if (Extent != FIntPoint(1440, 810))
	{
		Fail(TEXT("Diagnostic game viewport must be 1440x810 for source image comparison."));
		return;
	}
	TSharedPtr<FJsonObject> RequestProjection;
	FString CoverageInputReason;
	if (bCoverageMode && !CheckCoverageStableInputs(RequestProjection, CoverageInputReason))
	{
		Fail(CoverageInputReason);
		return;
	}
	const int32 CaptureSeq = Samples.Num() + 1;
	const FString CaptureId = FString::Printf(TEXT("%s_%02d"),
		*CoverageRunId, CaptureSeq);
	PendingScreenshotPath = FPaths::Combine(RunDirectory,
		bCoverageMode ? Label + TEXT("_") + CaptureId + TEXT(".png")
			: Label + TEXT(".png"));
	if (IFileManager::Get().FileExists(*PendingScreenshotPath))
	{
		Fail(TEXT("Phase screenshot target already exists; refusing overwrite."));
		return;
	}
	PendingStateJson = StateJson;
	PendingSample = State;
	PendingSample->SetStringField(TEXT("phase"), Label);
	PendingSample->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetNumberField(TEXT("stable_observations"), StableObservationCount);
	PendingSample->SetStringField(TEXT("screenshot_path"), PendingScreenshotPath);
	PendingSample->SetNumberField(TEXT("screenshot_request_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetNumberField(TEXT("viewport_width"), Extent.X);
	PendingSample->SetNumberField(TEXT("viewport_height"), Extent.Y);
	PendingSample->SetObjectField(TEXT("pre_request_render_readiness"), CaptureReadiness());
	if (bFixedCameraProbeMode)
	{
		TSharedPtr<FJsonObject> GroundSlot;
		FString GroundReason;
		if (!CaptureFixedProbeGroundSlot(GroundSlot, GroundReason))
		{
			Fail(GroundReason);
			return;
		}
		PendingSample->SetObjectField(TEXT("ground_slot_pre_request"), GroundSlot);
		if (!LastFixedProbeSubmittedView.IsValid()
			|| LastFixedProbeSubmittedView->GetIntegerField(TEXT("step")) != Step
			|| LastFixedProbeSubmittedView->GetNumberField(TEXT("frame")) > GFrameCounter)
		{
			Fail(TEXT("Fixed probe has no matching submitted world view before screenshot request."));
			return;
		}
		PendingSample->SetObjectField(TEXT("submitted_view_pre_request"), LastFixedProbeSubmittedView);
	}
	if (bCoverageMode)
	{
		CoveragePendingRequestTicks = FPlatformTime::Cycles64();
		CoveragePendingProcessedTicks = 0;
		PendingSample->SetStringField(TEXT("run_id"), CoverageRunId);
		PendingSample->SetStringField(TEXT("process_id"), CoverageProcessId);
		PendingSample->SetStringField(TEXT("run_start_ticks"), CoverageRunStartTicks);
		PendingSample->SetStringField(TEXT("capture_id"), CaptureId);
		PendingSample->SetNumberField(TEXT("capture_seq"), CaptureSeq);
		PendingSample->SetStringField(TEXT("capture_request_ticks"),
			ExactU64(CoveragePendingRequestTicks));
		PendingSample->SetObjectField(TEXT("capture_source_pins"), CoveragePinnedSources);
		PendingSample->SetObjectField(TEXT("registered_projection_modifiers"),
			CoverageProjectionRegistration);
		PendingSample->SetObjectField(TEXT("pre_request_projection_modifiers"),
			RequestProjection);
		PendingSample->SetArrayField(TEXT("coverage_roi_xyxy"), RectNumbers(CoverageRoi));
		PendingSample->SetArrayField(TEXT("coverage_projected_rect_xyxy"),
			RectNumbers(CoverageProjectedRect));
		PendingSample->SetStringField(TEXT("coverage_target_component"), CoverageComponentName);
		PendingSample->SetNumberField(TEXT("coverage_target_instance_index"), CoverageInstanceIndex);
		PendingSample->SetBoolField(TEXT("coverage_target_visible"), !bCoverageTargetHidden);
	}
	ScreenshotRequestFrame = GFrameCounter;
	ScreenshotRequestedAtSeconds = FPlatformTime::Seconds();
	bScreenshotProcessed = false;
	FScreenshotRequest::RequestScreenshot(PendingScreenshotPath, false, false, false, FIntRect(), true);
	if (!FScreenshotRequest::IsScreenshotRequested())
	{
		Fail(TEXT("Game-viewport screenshot request was not accepted."));
	}
}

void ARokWorldInstancedMaterialDiagnosticGameMode::ScreenshotProcessed()
{
	if (bFinished || !PendingSample.IsValid() || bScreenshotProcessed)
	{
		return;
	}
	PendingSample->SetObjectField(TEXT("processed_render_readiness"), CaptureReadiness());
	if (bFixedCameraProbeMode)
	{
		TSharedPtr<FJsonObject> GroundSlot;
		FString GroundReason;
		if (!CaptureFixedProbeGroundSlot(GroundSlot, GroundReason))
		{
			Fail(GroundReason);
			return;
		}
		PendingSample->SetObjectField(TEXT("ground_slot_processed"), GroundSlot);
	}
	if (LastWorldViewSubmissionFrame != GFrameCounter
		|| LastWorldViewSubmissionStep != Step
		|| LastViewportRenderedFrame != GFrameCounter
		|| LastWorldViewCount != 1 || !IsExpectedSubmittedViewRect()
		|| !bLastWorldViewRendering || !bLastWorldViewStaticMeshes
		|| !bLastWorldViewInstancedStaticMeshes)
	{
		Fail(TEXT("Screenshot frame did not submit and render the matching world view."));
		return;
	}
	if (bFixedCameraProbeMode)
	{
		if (!LastFixedProbeSubmittedView.IsValid()
			|| LastFixedProbeSubmittedView->GetIntegerField(TEXT("step")) != Step
			|| LastFixedProbeSubmittedView->GetNumberField(TEXT("frame")) != GFrameCounter
			|| FixedProbeSubmittedComparable(LastFixedProbeSubmittedView)
				!= FixedProbeSubmittedComparable(PendingSample->GetObjectField(TEXT("submitted_view_pre_request"))))
		{
			Fail(TEXT("Fixed probe submitted view matrix drifted before the screenshot frame."));
			return;
		}
		PendingSample->SetObjectField(TEXT("submitted_view_processed"), LastFixedProbeSubmittedView);
	}
	TArray<uint8> Bytes;
	TArray<uint8> RgbaPixels;
	int64 NonblackRgbPixels = 0;
	if (!ReadViewportPng(PendingScreenshotPath, Bytes, RgbaPixels, NonblackRgbPixels))
	{
		Fail(TEXT("Processed real game-viewport screenshot is not decodable, opaque 1440x810 RGBA8 PNG."));
		return;
	}
	PendingSample->SetNumberField(TEXT("png_nonopaque_alpha_pixels"), 0);
	PendingSample->SetNumberField(TEXT("png_nonblack_rgb_pixels"), static_cast<double>(NonblackRgbPixels));
	if (PendingSample->GetIntegerField(TEXT("hism_instance_count")) > 0
		&& NonblackRgbPixels <= 1024)
	{
		Fail(TEXT("Nonempty streamed world produced an effectively black real viewport PNG."));
		return;
	}
	TSharedPtr<FJsonObject> ProcessedState = CaptureVisualState();
	if (SerializeState(ProcessedState) != PendingStateJson)
	{
		Fail(TEXT("Source HISM/camera/HTTP state changed before screenshot processing."));
		return;
	}
	if (bCoverageMode)
	{
		TSharedPtr<FJsonObject> Projection;
		FString InputReason;
		if (!CheckCoverageStableInputs(Projection, InputReason))
		{
			Fail(InputReason);
			return;
		}
		CoveragePendingProcessedTicks = FPlatformTime::Cycles64();
		if (CoveragePendingProcessedTicks <= CoveragePendingRequestTicks)
		{
			Fail(TEXT("Coverage processed clock tick did not advance."));
			return;
		}
		PendingSample->SetStringField(TEXT("capture_processed_ticks"),
			ExactU64(CoveragePendingProcessedTicks));
		PendingSample->SetObjectField(TEXT("processed_projection_modifiers"), Projection);
	}
	ScreenshotProcessedFrame = GFrameCounter;
	PendingSample->SetNumberField(TEXT("screenshot_processed_frame"), static_cast<double>(ScreenshotProcessedFrame));
	PendingSample->SetNumberField(TEXT("png_bytes"), Bytes.Num());
	PendingSample->SetNumberField(TEXT("png_width"), PngU32(Bytes, 16));
	PendingSample->SetNumberField(TEXT("png_height"), PngU32(Bytes, 20));
	PendingSample->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
	PendingSample->SetStringField(TEXT("png_decoded_rgba_sha256"), RokTrainArmyMesh::Sha256(RgbaPixels));
	PendingSample->SetObjectField(TEXT("processed_state"), ProcessedState);
	bScreenshotProcessed = true;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::CheckPendingScreenshot()
{
	if (!PendingSample.IsValid())
	{
		return;
	}
	if (FPlatformTime::Seconds() - ScreenshotRequestedAtSeconds > 15.0)
	{
		Fail(TEXT("Game-viewport screenshot callback/next-frame state timed out."));
		return;
	}
	TSharedPtr<FJsonObject> CurrentState = CaptureVisualState();
	if (SerializeState(CurrentState) != PendingStateJson)
	{
		Fail(TEXT("Source HISM/camera/HTTP state changed while screenshot was pending."));
		return;
	}
	if (!bScreenshotProcessed || GFrameCounter <= ScreenshotProcessedFrame)
	{
		return;
	}
	if (bFixedCameraProbeMode
		&& (LastWorldViewSubmissionFrame <= ScreenshotProcessedFrame
			|| LastWorldViewSubmissionStep != Step
			|| LastWorldViewCount != 1 || !IsExpectedSubmittedViewRect()
			|| !bLastWorldViewRendering || !bLastWorldViewStaticMeshes
			|| !bLastWorldViewInstancedStaticMeshes
			|| LastViewportRenderedFrame < LastWorldViewSubmissionFrame))
	{
		// Wait for a later real world view/render, within the existing 15 s callback bound.
		return;
	}
	if (bCoverageMode)
	{
		TSharedPtr<FJsonObject> Projection;
		FString InputReason;
		if (!CheckCoverageStableInputs(Projection, InputReason))
		{
			Fail(InputReason);
			return;
		}
		const uint64 ObservedTicks = FPlatformTime::Cycles64();
		if (ObservedTicks <= CoveragePendingProcessedTicks)
		{
			Fail(TEXT("Coverage observed clock tick did not advance."));
			return;
		}
		PendingSample->SetStringField(TEXT("capture_observed_ticks"), ExactU64(ObservedTicks));
		PendingSample->SetObjectField(TEXT("observed_projection_modifiers"), Projection);
	}
	PendingSample->SetNumberField(TEXT("screenshot_observed_frame"), static_cast<double>(GFrameCounter));
	PendingSample->SetObjectField(TEXT("observed_state"), CurrentState);
	if (bFixedCameraProbeMode)
	{
		TSharedPtr<FJsonObject> GroundSlot;
		FString GroundReason;
		if (!CaptureFixedProbeGroundSlot(GroundSlot, GroundReason))
		{
			Fail(GroundReason);
			return;
		}
		PendingSample->SetObjectField(TEXT("ground_slot_observed"), GroundSlot);
		if (!LastFixedProbeSubmittedView.IsValid()
			|| LastFixedProbeSubmittedView->GetIntegerField(TEXT("step")) != Step
			|| LastFixedProbeSubmittedView->GetNumberField(TEXT("frame"))
				<= PendingSample->GetNumberField(TEXT("screenshot_processed_frame"))
			|| LastFixedProbeSubmittedView->GetNumberField(TEXT("frame"))
				!= LastWorldViewSubmissionFrame
			|| LastViewportRenderedFrame < LastWorldViewSubmissionFrame
			|| FixedProbeSubmittedComparable(LastFixedProbeSubmittedView)
				!= FixedProbeSubmittedComparable(PendingSample->GetObjectField(TEXT("submitted_view_processed"))))
		{
			Fail(TEXT("Fixed probe submitted view matrix drifted after the screenshot frame."));
			return;
		}
		PendingSample->SetObjectField(TEXT("submitted_view_observed"), LastFixedProbeSubmittedView);
		PendingSample->SetNumberField(TEXT("observed_world_view_submission_frame"),
			static_cast<double>(LastWorldViewSubmissionFrame));
		PendingSample->SetNumberField(TEXT("observed_viewport_rendered_frame"),
			static_cast<double>(LastViewportRenderedFrame));
		const FString Pre = SerializeState(PendingSample->GetObjectField(TEXT("ground_slot_pre_request")));
		const FString Processed = SerializeState(PendingSample->GetObjectField(TEXT("ground_slot_processed")));
		const FString Observed = SerializeState(GroundSlot);
		PendingSample->SetBoolField(TEXT("ground_slot_and_mips_stable_during_capture"),
			Pre == Processed && Pre == Observed);
		if (Pre != Processed || Pre != Observed)
		{
			Fail(TEXT("Ground slot or texture residency changed during fixed-camera capture."));
			return;
		}
	}
	Samples.Add(PendingSample);
	PendingSample.Reset();
	PendingStateJson.Empty();
	PendingScreenshotPath.Empty();
	ClearStability();
	AdvanceAfterScreenshot();
}

void ARokWorldInstancedMaterialDiagnosticGameMode::AdvanceAfterScreenshot()
{
	if (bFixedCameraProbeMode)
	{
		const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
		switch (Step)
		{
		case 1: FixedProbeLod0Generation = State.Generation; Step = 2; break;
		case 2:
			if (!Streaming->SetFixedCameraProbeLod(1)) { Fail(TEXT("Fixed probe LOD1 request rejected.")); return; }
			Step = 3;
			break;
		case 3:
			FixedProbeLod1Generation = State.Generation;
			if (!Streaming->SetFixedCameraProbeLod(0)) { Fail(TEXT("Fixed probe LOD0 restore rejected.")); return; }
			Step = 4;
			break;
		case 4: Finish(true, TEXT("Four controlled fixed-camera ground captures processed.")); break;
		default: Fail(TEXT("Invalid fixed-camera probe phase.")); break;
		}
		return;
	}
	if (bCoverageMode)
	{
		FString TransitionReason;
		switch (Step)
		{
		case 1: Step = 2; break;
		case 2:
			if (!SetCoverageTargetHidden(true, TransitionReason)) { Fail(TransitionReason); return; }
			Step = 3;
			break;
		case 3:
			if (!SetCoverageTargetHidden(false, TransitionReason)) { Fail(TransitionReason); return; }
			Step = 4;
			break;
		case 4:
			Finish(true, TEXT("Four fixed-source-tree A1/A2/B/A3 captures processed."));
			break;
		default: Fail(TEXT("Invalid coverage phase after screenshot.")); break;
		}
		return;
	}
	const FRokWorldMapStreamingDiagnosticState State = Streaming->GetDiagnosticState();
	switch (Step)
	{
	case 1:
		InitialGeneration = State.Generation;
		InitialComponentNames = State.ComponentNames;
		Camera->SetViewCenterUnity(MovedFocus);
		Step = 2;
		break;
	case 2:
		MovedGeneration = State.Generation;
		Camera->SetCurrentDxf(LodOneDxf);
		Step = 3;
		break;
	case 3:
		LodOneGeneration = State.Generation;
		Camera->SetCurrentDxf(BlockedLodTwoDxf);
		Step = 4;
		break;
	case 4:
		Camera->SetCurrentDxf(LodOneDxf);
		Step = 5;
		break;
	case 5:
		Finish(true, TEXT("Five stable game-viewport captures and scoped LOD0/1 streaming passed."));
		break;
	default:
		Fail(TEXT("Invalid phase after game-viewport screenshot."));
		break;
	}
}

TSharedPtr<FJsonObject> ARokWorldInstancedMaterialDiagnosticGameMode::CaptureRuntimeData() const
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	const URokWorldMapRuntimeDataAsset* Data = Streaming.IsValid()
		? Streaming->RuntimeData.Get() : nullptr;
	if (!Data)
	{
		return Result;
	}
	Result->SetStringField(TEXT("schema"), Data->Schema);
	Result->SetStringField(TEXT("canonical_payload_sha256"), Data->CanonicalPayloadSha256);
	Result->SetStringField(TEXT("source_manifest_path"), Data->SourceManifestPath);
	Result->SetStringField(TEXT("source_manifest_sha256"), Data->SourceManifestSha256);
	Result->SetNumberField(TEXT("map_width_tiles"), Data->MapWidthTiles);
	Result->SetNumberField(TEXT("map_height_tiles"), Data->MapHeightTiles);
	Result->SetNumberField(TEXT("tile_size_unity_units"), Data->TileSizeUnityUnits);
	Result->SetBoolField(TEXT("static_visual_ready"), Data->bStaticVisualReady);
	Result->SetBoolField(TEXT("runtime_ready"), Data->bRuntimeReady);
	TSharedPtr<FJsonObject> Projection = MakeShared<FJsonObject>();
	Projection->SetArrayField(TEXT("unity_bounds_min"), Vector2Numbers(Data->Projection.UnityBoundsMin));
	Projection->SetArrayField(TEXT("unity_bounds_max"), Vector2Numbers(Data->Projection.UnityBoundsMax));
	Projection->SetArrayField(TEXT("unity_rebase_origin"), Vector2Numbers(Data->Projection.UnityRebaseOrigin));
	Projection->SetNumberField(TEXT("ue_units_per_unity_unit"), Data->Projection.UeUnitsPerUnityUnit);
	Projection->SetNumberField(TEXT("ground_z"), Data->Projection.GroundZ);
	Projection->SetNumberField(TEXT("display_coordinate_scale"), Data->Projection.DisplayCoordinateScale);
	Result->SetObjectField(TEXT("projection"), Projection);
	TArray<TSharedPtr<FJsonValue>> Levels;
	for (const FRokWorldMapLodLevel& Level : Data->LodLevels)
	{
		Levels.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Level.Lod), JsonNumber(Level.MaxDxfInclusive), JsonNumber(Level.ChunkTileSpan)}));
	}
	Result->SetArrayField(TEXT("lod_levels"), Levels);
	TArray<TSharedPtr<FJsonValue>> Tiles;
	for (const FRokWorldMapTileRecord& Tile : Data->Tiles)
	{
		Tiles.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Tile.TileX), JsonNumber(Tile.TileY), JsonString(Tile.TileTypeId.ToString()),
			MakeShared<FJsonValueArray>(TransformNumbers(Tile.TemplateToWorld))}));
	}
	Result->SetArrayField(TEXT("tiles"), Tiles);
	TArray<TSharedPtr<FJsonValue>> Types;
	for (const FRokWorldMapTileTypeRecord& Type : Data->TileTypes)
	{
		TArray<TSharedPtr<FJsonValue>> Bindings;
		for (const FRokWorldMapLodBinding& Binding : Type.LodBindings)
		{
			Bindings.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				JsonNumber(Binding.Lod), JsonString(Binding.RecipeId.ToString())}));
		}
		Types.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Type.TileTypeId.ToString()), MakeShared<FJsonValueArray>(Bindings)}));
	}
	Result->SetArrayField(TEXT("tile_types"), Types);
	TArray<TSharedPtr<FJsonValue>> Recipes;
	for (const FRokWorldMapRenderRecipe& Recipe : Data->Recipes)
	{
		TArray<TSharedPtr<FJsonValue>> Parts;
		for (const FRokWorldMapRenderPart& Part : Recipe.Parts)
		{
			TArray<TSharedPtr<FJsonValue>> Materials;
			for (const TSoftObjectPtr<UMaterialInterface>& Material : Part.Materials)
			{
				Materials.Add(JsonString(Material.ToSoftObjectPath().ToString()));
			}
			Parts.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
				JsonString(Part.Mesh.ToSoftObjectPath().ToString()),
				MakeShared<FJsonValueArray>(Materials),
				MakeShared<FJsonValueArray>(TransformNumbers(Part.LocalTransform)),
				JsonNumber(Part.TranslucencySortPriority),
				MakeShared<FJsonValueBoolean>(Part.bTree), JsonNumber(Part.RenderFlags)}));
		}
		Recipes.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Recipe.RecipeId.ToString()), JsonString(Recipe.TileTypeId.ToString()),
			JsonNumber(Recipe.Lod), MakeShared<FJsonValueArray>(Parts)}));
	}
	Result->SetArrayField(TEXT("recipes"), Recipes);
	TArray<TSharedPtr<FJsonValue>> Blockers;
	for (const FString& Blocker : Data->ReadinessBlockers)
	{
		Blockers.Add(JsonString(Blocker));
	}
	Result->SetArrayField(TEXT("readiness_blockers"), Blockers);
	TArray<TSharedPtr<FJsonValue>> Policies;
	for (const FRokWorldMapPendingPolicy& Policy : Data->PendingPolicies)
	{
		FString Status;
		switch (Policy.Status)
		{
		case ERokWorldMapPendingPolicyStatus::Pending: Status = TEXT("Pending"); break;
		case ERokWorldMapPendingPolicyStatus::SourceExplicitNoOp: Status = TEXT("SourceExplicitNoOp"); break;
		case ERokWorldMapPendingPolicyStatus::AcceptedOmission: Status = TEXT("AcceptedOmission"); break;
		case ERokWorldMapPendingPolicyStatus::Promoted: Status = TEXT("Promoted"); break;
		default: Status = TEXT("unknown"); break;
		}
		Policies.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{
			JsonString(Policy.PolicyId.ToString()), JsonString(Status),
			JsonNumber(Policy.SourceComponentCount), JsonString(Policy.RequiredDecision)}));
	}
	Result->SetArrayField(TEXT("pending_policies"), Policies);
	return Result;
}

TArray<TSharedPtr<FJsonValue>> ARokWorldInstancedMaterialDiagnosticGameMode::CaptureActualComponents() const
{
	TArray<TSharedPtr<FJsonValue>> Result;
	if (!Streaming.IsValid())
	{
		return Result;
	}
	TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components;
	Streaming->GetComponents(Components);
	for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
	{
		if (!IsValid(Component) || Component->GetOwner() != Streaming.Get() || !Component->IsRegistered())
		{
			continue;
		}
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Component->GetName());
		Item->SetStringField(TEXT("mesh"), Component->GetStaticMesh()
			? Component->GetStaticMesh()->GetPathName() : TEXT(""));
		Item->SetNumberField(TEXT("translucency_sort_priority"), Component->TranslucencySortPriority);
		Item->SetBoolField(TEXT("visible"), Component->IsVisible());
		Item->SetStringField(TEXT("attach_parent"), Component->GetAttachParent()
			? Component->GetAttachParent()->GetName() : TEXT(""));
		Item->SetBoolField(TEXT("attached_to_streaming_root"),
			Component->GetAttachParent() == Streaming->GetRootComponent());
		Item->SetBoolField(TEXT("render_state_created"), Component->IsRenderStateCreated());
		const FVector WorldExtent = Component->Bounds.BoxExtent;
		Item->SetArrayField(TEXT("world_bounds_origin"), TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(Component->Bounds.Origin.X), JsonNumber(Component->Bounds.Origin.Y),
			JsonNumber(Component->Bounds.Origin.Z)});
		Item->SetArrayField(TEXT("world_bounds_extent"), TArray<TSharedPtr<FJsonValue>>{
			JsonNumber(WorldExtent.X), JsonNumber(WorldExtent.Y), JsonNumber(WorldExtent.Z)});
		Item->SetBoolField(TEXT("nonzero_world_bounds"),
			!WorldExtent.ContainsNaN() && WorldExtent.GetMax() > KINDA_SMALL_NUMBER);
		Item->SetArrayField(TEXT("relative_transform"), TransformNumbers(Component->GetRelativeTransform()));
		TArray<TSharedPtr<FJsonValue>> Materials;
		TArray<TSharedPtr<FJsonValue>> MaterialAliases;
		for (int32 MaterialIndex = 0; MaterialIndex < Component->GetNumMaterials(); ++MaterialIndex)
		{
			const UMaterialInterface* Material = Component->GetMaterial(MaterialIndex);
			Materials.Add(JsonString(Material ? Material->GetPathName() : TEXT("")));
			TSharedPtr<FJsonObject> AliasSlot = MakeShared<FJsonObject>();
			const FString Actual = Material ? Material->GetPathName() : TEXT("");
			const FString* Original = SourceByAlias.Find(Actual);
			const FString SlotKey = Component->GetName() + TEXT("#") + FString::FromInt(MaterialIndex);
			AliasSlot->SetNumberField(TEXT("slot"), MaterialIndex);
			AliasSlot->SetStringField(TEXT("source_object"), Original ? *Original : TEXT(""));
			AliasSlot->SetStringField(TEXT("observed_before_alias"),
				ObservedBeforeAliasBySlot.FindRef(SlotKey));
			AliasSlot->SetStringField(TEXT("target_object"), Original ? AliasBySource.FindRef(*Original)
				: TEXT(""));
			AliasSlot->SetStringField(TEXT("actual_object"), Actual);
			AliasSlot->SetStringField(TEXT("target_package_sha256"), TargetPackageShaByObject.FindRef(Actual));
			AliasSlot->SetNumberField(TEXT("target_package_bytes"),
				static_cast<double>(TargetPackageBytesByObject.FindRef(Actual)));
			AliasSlot->SetStringField(TEXT("creator_terminal_sha256"),
				Original && *Original == BridgeSource ? BridgeReceiptSha256
				: Original && *Original == TEXT("/Game/RokUnityWorldMapPrototype/Materials/M_WorldRiver_Water.M_WorldRiver_Water")
					? RiverReceiptSha256 : VariantsReceiptSha256);
			AliasSlot->SetBoolField(TEXT("source_passthrough"), false);
			AliasSlot->SetBoolField(TEXT("render_proxy_material_verified"), false);
			MaterialAliases.Add(MakeShared<FJsonValueObject>(AliasSlot));
			if (Material)
			{
				TSharedPtr<FJsonObject> MaterialDiagnostic = MakeShared<FJsonObject>();
				MaterialDiagnostic->SetStringField(TEXT("path"), Material->GetPathName());
				MaterialDiagnostic->SetNumberField(TEXT("blend_mode"), static_cast<int32>(Material->GetBlendMode()));
				MaterialDiagnostic->SetBoolField(TEXT("default_lit"),
					Material->GetShadingModels().HasShadingModel(MSM_DefaultLit));
				MaterialDiagnostic->SetBoolField(TEXT("unlit"),
					Material->GetShadingModels().HasShadingModel(MSM_Unlit));
				const UMaterial* BaseMaterial = Material->GetMaterial();
				MaterialDiagnostic->SetStringField(TEXT("base_material"),
					BaseMaterial ? BaseMaterial->GetPathName() : TEXT(""));
				MaterialDiagnostic->SetBoolField(TEXT("base_instanced_static_mesh_usage"),
					BaseMaterial && BaseMaterial->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
				AliasSlot->SetBoolField(TEXT("base_instanced_static_mesh_usage"),
					BaseMaterial && BaseMaterial->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
				AliasSlot->SetBoolField(TEXT("native_shader_map_available"), NativeMaterialReady(BaseMaterial));
				AliasSlot->SetStringField(TEXT("base_material"),
					BaseMaterial ? BaseMaterial->GetPathName() : TEXT(""));
				Item->SetObjectField(FString::Printf(TEXT("material_diagnostic_%d"), MaterialIndex), MaterialDiagnostic);
			}
		}
		Item->SetArrayField(TEXT("materials"), Materials);
		Item->SetArrayField(TEXT("material_aliases"), MaterialAliases);
		TArray<TSharedPtr<FJsonValue>> Instances;
		bool bReadAll = true;
		for (int32 InstanceIndex = 0; InstanceIndex < Component->GetInstanceCount(); ++InstanceIndex)
		{
			FTransform Transform;
			if (!Component->GetInstanceTransform(InstanceIndex, Transform, false))
			{
				bReadAll = false;
				break;
			}
			Instances.Add(MakeShared<FJsonValueArray>(TransformNumbers(Transform)));
		}
		Item->SetBoolField(TEXT("instance_transforms_read_all"), bReadAll);
		Item->SetArrayField(TEXT("instances_local"), Instances);
		Result.Add(MakeShared<FJsonValueObject>(Item));
	}
	return Result;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CaptureFixedProbeActualView(
	const FMinimalViewInfo& View, const FIntRect& ViewRect,
	TSharedPtr<FJsonObject>& OutView, FString& OutReason) const
{
	const UCameraComponent* Component = Camera.IsValid() ? Camera->GetCameraComponent() : nullptr;
	if (!Component || !FMath::IsFinite(View.FOV) || !FMath::IsFinite(View.DesiredFOV)
		|| !FMath::IsFinite(View.AspectRatio) || !FMath::IsFinite(View.PostProcessBlendWeight))
	{
		OutReason = TEXT("Fixed probe actual CameraComponent view is unavailable or nonfinite.");
		return false;
	}
	OutView = MakeShared<FJsonObject>();
	OutView->SetArrayField(TEXT("location_xyz"), Vector3Numbers(View.Location));
	OutView->SetArrayField(TEXT("rotation_pitch_yaw_roll"), TArray<TSharedPtr<FJsonValue>>{
		JsonNumber(View.Rotation.Pitch), JsonNumber(View.Rotation.Yaw), JsonNumber(View.Rotation.Roll)});
	OutView->SetNumberField(TEXT("fov"), View.FOV);
	OutView->SetNumberField(TEXT("desired_fov"), View.DesiredFOV);
	OutView->SetNumberField(TEXT("first_person_fov"), View.FirstPersonFOV);
	OutView->SetNumberField(TEXT("first_person_scale"), View.FirstPersonScale);
	OutView->SetBoolField(TEXT("use_first_person_parameters"), View.bUseFirstPersonParameters);
	OutView->SetBoolField(TEXT("use_fov_for_lod"), View.bUseFieldOfViewForLOD);
	OutView->SetNumberField(TEXT("projection_mode"), static_cast<int32>(View.ProjectionMode));
	OutView->SetNumberField(TEXT("aspect_ratio"), View.AspectRatio);
	OutView->SetBoolField(TEXT("constrain_aspect_ratio"), View.bConstrainAspectRatio);
	OutView->SetBoolField(TEXT("axis_constraint_is_set"), View.AspectRatioAxisConstraint.IsSet());
	OutView->SetNumberField(TEXT("axis_constraint"), View.AspectRatioAxisConstraint.IsSet()
		? static_cast<int32>(View.AspectRatioAxisConstraint.GetValue()) : -1);
	OutView->SetNumberField(TEXT("ortho_width"), View.OrthoWidth);
	OutView->SetBoolField(TEXT("auto_calculate_ortho_planes"), View.bAutoCalculateOrthoPlanes);
	OutView->SetNumberField(TEXT("auto_plane_shift"), View.AutoPlaneShift);
	OutView->SetBoolField(TEXT("update_ortho_planes"), View.bUpdateOrthoPlanes);
	OutView->SetBoolField(TEXT("use_camera_height_as_view_target"), View.bUseCameraHeightAsViewTarget);
	OutView->SetNumberField(TEXT("ortho_near"), View.OrthoNearClipPlane);
	OutView->SetNumberField(TEXT("ortho_far"), View.OrthoFarClipPlane);
	OutView->SetNumberField(TEXT("perspective_near"), View.PerspectiveNearClipPlane);
	OutView->SetArrayField(TEXT("off_center_offset"), Vector2Numbers(View.OffCenterProjectionOffset));
	OutView->SetNumberField(TEXT("overscan"), View.GetOverscan());
	OutView->SetNumberField(TEXT("overscan_resolution_fraction"), View.OverscanResolutionFraction);
	OutView->SetNumberField(TEXT("crop_fraction"), View.CropFraction);
	OutView->SetArrayField(TEXT("asymmetric_crop_fraction"), TArray<TSharedPtr<FJsonValue>>{
		JsonNumber(View.AsymmetricCropFraction.X), JsonNumber(View.AsymmetricCropFraction.Y),
		JsonNumber(View.AsymmetricCropFraction.Z), JsonNumber(View.AsymmetricCropFraction.W)});
	OutView->SetArrayField(TEXT("projection_matrix"), MatrixNumbers(View.CalculateProjectionMatrix()));
	OutView->SetArrayField(TEXT("view_rect_xyxy"), RectNumbers(ViewRect));
	OutView->SetBoolField(TEXT("previous_view_transform_is_set"), View.PreviousViewTransform.IsSet());
	if (View.PreviousViewTransform.IsSet())
	{
		OutView->SetArrayField(TEXT("previous_view_transform"),
			TransformNumbers(View.PreviousViewTransform.GetValue()));
	}
	OutView->SetNumberField(TEXT("post_process_blend_weight"), View.PostProcessBlendWeight);
	OutView->SetStringField(TEXT("post_process_settings_sha256"), FixedProbeShaUtf8(
		FixedProbeReflectedText(FPostProcessSettings::StaticStruct(), &View.PostProcessSettings)));
	TArray<FPostProcessSettings> ExtraSettings;
	TArray<float> ExtraWeights;
	Component->GetExtraPostProcessBlends(ExtraSettings, ExtraWeights);
	if (ExtraSettings.Num() != ExtraWeights.Num())
	{
		OutReason = TEXT("Fixed probe extra post-process blend arrays differ in length.");
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> ExtraBlends;
	for (int32 Index = 0; Index < ExtraSettings.Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Blend = MakeShared<FJsonObject>();
		Blend->SetNumberField(TEXT("weight"), ExtraWeights[Index]);
		Blend->SetStringField(TEXT("settings_sha256"), FixedProbeShaUtf8(
			FixedProbeReflectedText(FPostProcessSettings::StaticStruct(), &ExtraSettings[Index])));
		ExtraBlends.Add(MakeShared<FJsonValueObject>(Blend));
	}
	OutView->SetArrayField(TEXT("extra_post_process_blends"), ExtraBlends);
	return true;
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CaptureFixedProbeGroundSlot(
	TSharedPtr<FJsonObject>& OutSlot, FString& OutReason)
{
	OutSlot.Reset();
	TArray<FRokWorldFixedProbeBatch> Batches;
	FIntRect ViewRect;
	FMinimalViewInfo CameraView;
	float EffectiveAspect = 0.0f, DesiredAspect = 0.0f;
	if (!Streaming.IsValid() || !Streaming->GetFixedCameraProbeBatches(Batches)
		|| !CaptureExpectedConstrainedViewRect(ViewRect, CameraView,
			EffectiveAspect, DesiredAspect) || Batches.IsEmpty())
	{
		OutReason = TEXT("Fixed probe cannot inspect the active keyed HISM set or viewport.");
		return false;
	}
	struct FCandidate
	{
		FString Identity;
		FString TransformSha;
		UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
		FRokWorldMapStreamingBatchKey Key;
		FIntRect Roi;
	};
	TArray<FCandidate> Candidates;
	TArray<FString> AllGroundIdentities;
	constexpr TCHAR GroundMesh[] = TEXT("/Game/RokUnityWorldMapPrototype/Meshes/SM_RokWorld_Plane60.SM_RokWorld_Plane60");
	for (const FRokWorldFixedProbeBatch& Batch : Batches)
	{
		UHierarchicalInstancedStaticMeshComponent* Component = Batch.Component.Get();
		if (!IsValid(Component) || !Component->IsRegistered()
			|| Component->GetOwner() != Streaming.Get()
			|| Batch.Key.bTree || Batch.Key.MeshPath.ToString() != GroundMesh)
		{
			continue;
		}
		TArray<FString> InstanceTexts;
		for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
		{
			FTransform Transform;
			if (!Component->GetInstanceTransform(Index, Transform, false))
			{
				OutReason = TEXT("Fixed probe ground instance transform read failed.");
				return false;
			}
			TSharedPtr<FJsonObject> TransformObject = MakeShared<FJsonObject>();
			TransformObject->SetArrayField(TEXT("local"), TransformNumbers(Transform));
			InstanceTexts.Add(SerializeState(TransformObject));
		}
		InstanceTexts.Sort();
		const FString TransformSha = FixedProbeShaUtf8(FString::Join(InstanceTexts, TEXT("\n")));
		TArray<FString> SourceMaterials;
		for (const FSoftObjectPath& Path : Batch.Key.MaterialPaths)
		{
			SourceMaterials.Add(Path.ToString());
		}
		const FString Identity = FixedProbeShaUtf8(FString::Printf(
			TEXT("%d,%d|%s|%s|%d|%d|%s"), Batch.Key.ChunkKey.X,
			Batch.Key.ChunkKey.Y, *Batch.Key.MeshPath.ToString(),
			*FString::Join(SourceMaterials, TEXT("|")),
			Batch.Key.TranslucencySortPriority, Batch.Key.RenderFlags, *TransformSha));
		AllGroundIdentities.Add(Identity);
		if (Batch.Key.ChunkKey != FIntPoint(16, 2)
			|| Batch.Key.MaterialPaths.IsEmpty() || Component->GetNumMaterials() < 1)
		{
			continue;
		}
		FIntRect Roi;
		bool bHasProjectedInstance = false;
		for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
		{
			FIntRect Projected;
			FTransform WorldTransform;
			bool bBehind = false;
			if (!ProjectCoverageInstanceBounds(Component, Index, CameraView, ViewRect,
				Projected, WorldTransform, bBehind) || bBehind)
			{
				continue;
			}
			const FIntRect Clipped(FMath::Max(Projected.Min.X, ViewRect.Min.X),
				FMath::Max(Projected.Min.Y, ViewRect.Min.Y),
				FMath::Min(Projected.Max.X, ViewRect.Max.X),
				FMath::Min(Projected.Max.Y, ViewRect.Max.Y));
			if (Clipped.Width() <= 0 || Clipped.Height() <= 0) continue;
			if (!bHasProjectedInstance) Roi = Clipped;
			else Roi = FIntRect(FMath::Min(Roi.Min.X, Clipped.Min.X),
				FMath::Min(Roi.Min.Y, Clipped.Min.Y),
				FMath::Max(Roi.Max.X, Clipped.Max.X),
				FMath::Max(Roi.Max.Y, Clipped.Max.Y));
			bHasProjectedInstance = true;
		}
		if (bHasProjectedInstance)
		{
			FCandidate Candidate;
			Candidate.Identity = Identity;
			Candidate.TransformSha = TransformSha;
			Candidate.Component = Component;
			Candidate.Key = Batch.Key;
			Candidate.Roi = Roi;
			Candidates.Add(MoveTemp(Candidate));
		}
	}
	if (AllGroundIdentities.IsEmpty() || Candidates.IsEmpty())
	{
		OutReason = TEXT("Fixed probe found no visible center-chunk ground batch.");
		return false;
	}
	AllGroundIdentities.Sort();
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{ return A.Identity < B.Identity; });
	if (FixedProbeTargetKey.IsEmpty())
	{
		if (Step != 1)
		{
			OutReason = TEXT("Fixed probe target was not established in A1.");
			return false;
		}
		FixedProbeTargetKey = Candidates[0].Identity;
		FixedProbeRoi = Candidates[0].Roi;
	}
	FCandidate* Target = nullptr;
	int32 Matches = 0;
	for (FCandidate& Candidate : Candidates)
	{
		if (Candidate.Identity == FixedProbeTargetKey)
		{
			Target = &Candidate;
			++Matches;
		}
	}
	if (Matches != 1 || !Target || Target->Roi != FixedProbeRoi)
	{
		OutReason = TEXT("Fixed probe target key is missing, duplicated or projected differently.");
		return false;
	}
	const UMaterialInterface* Material = Target->Component->GetMaterial(0);
	const FString MaterialPath = Material ? Material->GetPathName() : TEXT("");
	const FString SourcePath = Target->Key.MaterialPaths[0].ToString();
	const FString SlotKey = Target->Component->GetName() + TEXT("#0");
	if (!Material || SourceByAlias.FindRef(MaterialPath) != SourcePath
		|| ObservedBeforeAliasBySlot.FindRef(SlotKey) != SourcePath
		|| AliasBySource.FindRef(SourcePath) != MaterialPath
		|| TargetPackageShaByObject.FindRef(MaterialPath).IsEmpty())
	{
		OutReason = TEXT("Fixed probe target ground slot lacks pinned source-to-alias binding.");
		return false;
	}
	TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
	Slot->SetStringField(TEXT("identity_sha256"), Target->Identity);
	Slot->SetStringField(TEXT("transform_multiset_sha256"), Target->TransformSha);
	Slot->SetNumberField(TEXT("chunk_x"), Target->Key.ChunkKey.X);
	Slot->SetNumberField(TEXT("chunk_y"), Target->Key.ChunkKey.Y);
	Slot->SetStringField(TEXT("component"), Target->Component->GetName());
	Slot->SetStringField(TEXT("mesh"), Target->Key.MeshPath.ToString());
	Slot->SetNumberField(TEXT("instance_count"), Target->Component->GetInstanceCount());
	Slot->SetNumberField(TEXT("sort_priority"), Target->Key.TranslucencySortPriority);
	Slot->SetNumberField(TEXT("render_flags"), Target->Key.RenderFlags);
	Slot->SetNumberField(TEXT("material_slot"), 0);
	Slot->SetStringField(TEXT("source_material"), SourcePath);
	Slot->SetStringField(TEXT("actual_alias_material"), MaterialPath);
	Slot->SetStringField(TEXT("alias_package_sha256"), TargetPackageShaByObject.FindRef(MaterialPath));
	Slot->SetNumberField(TEXT("ground_batch_count"), AllGroundIdentities.Num());
	Slot->SetStringField(TEXT("ground_population_sha256"),
		FixedProbeShaUtf8(FString::Join(AllGroundIdentities, TEXT("\n"))));
	Slot->SetArrayField(TEXT("roi_xyxy"), RectNumbers(FixedProbeRoi));
	TSharedPtr<FJsonObject> ActualView;
	if (!CaptureFixedProbeActualView(CameraView, ViewRect, ActualView, OutReason))
	{
		return false;
	}
	Slot->SetObjectField(TEXT("actual_camera_view"), ActualView);
	ADirectionalLight* DiagnosticLight = nullptr;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (DiagnosticLight)
		{
			OutReason = TEXT("Fixed probe has more than one directional light.");
			return false;
		}
		DiagnosticLight = *It;
	}
	if (!DiagnosticLight || !DiagnosticLight->GetLightComponent())
	{
		OutReason = TEXT("Fixed probe diagnostic light is missing.");
		return false;
	}
	TSharedPtr<FJsonObject> Light = MakeShared<FJsonObject>();
	Light->SetArrayField(TEXT("transform"), TransformNumbers(DiagnosticLight->GetActorTransform()));
	Light->SetNumberField(TEXT("intensity"), DiagnosticLight->GetLightComponent()->Intensity);
	Light->SetArrayField(TEXT("linear_color"), TArray<TSharedPtr<FJsonValue>>{
		JsonNumber(DiagnosticLight->GetLightComponent()->GetLightColor().R),
		JsonNumber(DiagnosticLight->GetLightComponent()->GetLightColor().G),
		JsonNumber(DiagnosticLight->GetLightComponent()->GetLightColor().B)});
	Slot->SetObjectField(TEXT("directional_light"), Light);
	TArray<FMaterialParameterInfo> Parameters;
	TArray<FGuid> ParameterIds;
	Material->GetAllTextureParameterInfo(Parameters, ParameterIds);
	Parameters.Sort([](const FMaterialParameterInfo& A, const FMaterialParameterInfo& B)
		{ return A.ToString() < B.ToString(); });
	TArray<TSharedPtr<FJsonValue>> ParameterFacts;
	for (const FMaterialParameterInfo& Info : Parameters)
	{
		UTexture* Value = nullptr;
		const bool bFound = Material->GetTextureParameterValue(
			FHashedMaterialParameterInfo(Info), Value, false);
		TSharedPtr<FJsonObject> Fact = MakeShared<FJsonObject>();
		Fact->SetStringField(TEXT("name"), Info.Name.ToString());
		Fact->SetNumberField(TEXT("association"), static_cast<int32>(Info.Association.GetValue()));
		Fact->SetNumberField(TEXT("index"), Info.Index);
		Fact->SetBoolField(TEXT("resolved"), bFound);
		Fact->SetObjectField(TEXT("texture"), FixedProbeTextureFact(Value));
		ParameterFacts.Add(MakeShared<FJsonValueObject>(Fact));
	}
	Slot->SetArrayField(TEXT("texture_parameters"), ParameterFacts);
	TArray<UTexture*> UsedTextures;
	Material->GetUsedTextures(UsedTextures);
	TMap<FString, UTexture*> UsedByPath;
	for (UTexture* Texture : UsedTextures)
	{
		if (Texture) UsedByPath.Add(Texture->GetPathName(), Texture);
	}
	TArray<FString> UsedPaths;
	UsedByPath.GetKeys(UsedPaths);
	UsedPaths.Sort();
	TArray<TSharedPtr<FJsonValue>> UsedFacts;
	for (const FString& Path : UsedPaths)
	{
		UsedFacts.Add(MakeShared<FJsonValueObject>(FixedProbeTextureFact(UsedByPath[Path])));
	}
	Slot->SetArrayField(TEXT("used_texture_inventory_all_quality_platforms"), UsedFacts);
	OutSlot = Slot;
	OutReason.Empty();
	return true;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::Fail(const FString& Reason)
{
	Finish(false, Reason);
}

bool ARokWorldInstancedMaterialDiagnosticGameMode::CheckLocalOnlyBoundary(FString& OutReason) const
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))
	{
		OutReason = TEXT("-RokLocalOnly command flag is required.");
		return false;
	}
	UGameInstance* Instance = GetGameInstance();
	const URokServerSubsystem* Server = Instance ? Instance->GetSubsystem<URokServerSubsystem>() : nullptr;
	if (!Server || !Server->IsLocalOnlyRun())
	{
		OutReason = TEXT("Server subsystem is absent or not in local-only mode.");
		return false;
	}
	if (Server->GetHttpDispatchAttemptCount() != 0 || Server->GetSuppressedLocalRequestCount() != 0)
	{
		OutReason = FString::Printf(TEXT("Local-only boundary changed: HTTP dispatch=%llu suppressed=%llu."),
			static_cast<unsigned long long>(Server->GetHttpDispatchAttemptCount()),
			static_cast<unsigned long long>(Server->GetSuppressedLocalRequestCount()));
		return false;
	}
	OutReason.Empty();
	return true;
}

void ARokWorldInstancedMaterialDiagnosticGameMode::WriteLocalOnlyBoundary(FJsonObject& Object) const
{
	UGameInstance* Instance = GetGameInstance();
	const URokServerSubsystem* Server = Instance ? Instance->GetSubsystem<URokServerSubsystem>() : nullptr;
	Object.SetBoolField(TEXT("rok_local_only_flag"), FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")));
	Object.SetBoolField(TEXT("rok_world_streaming_diagnostic_flag"),
		FParse::Param(FCommandLine::Get(), TEXT("RokWorldStreamingDiagnostic")));
	Object.SetBoolField(TEXT("rok_world_instanced_material_diagnostic_flag"),
		FParse::Param(FCommandLine::Get(), TEXT("RokWorldInstancedMaterialDiagnostic")));
	Object.SetBoolField(TEXT("server_subsystem_present"), Server != nullptr);
	Object.SetBoolField(TEXT("local_only"), Server && Server->IsLocalOnlyRun());
	Object.SetNumberField(TEXT("http_dispatch_attempts"),
		Server ? static_cast<double>(Server->GetHttpDispatchAttemptCount()) : -1.0);
	Object.SetNumberField(TEXT("http_suppressed_requests"),
		Server ? static_cast<double>(Server->GetSuppressedLocalRequestCount()) : -1.0);
	if (bCoverageMode)
	{
		Object.SetBoolField(TEXT("coverage_opt_in_flag"),
			FParse::Param(FCommandLine::Get(), TEXT("RokWorldAliasPixelCoverageDiagnostic")));
	}
}

void ARokWorldInstancedMaterialDiagnosticGameMode::FinishCoverage(
	bool bPassed, const FString& Reason)
{
	if (bFinished) return;
	FString Failure;
	if (!CheckLocalOnlyBoundary(Failure)) bPassed = false;
	if (bPassed)
	{
		TSharedPtr<FJsonObject> Projection;
		if (!CheckCoverageStableInputs(Projection, Failure)) bPassed = false;
	}
	if (bPassed && (Samples.Num() != 4 || PendingSample.IsValid()
		|| CoverageComparableBaseline.IsEmpty() || !CoverageTarget.IsValid()
		|| bCoverageTargetHidden || !CoverageTarget->IsVisible()
		|| !bCoverageRestoreAttempted || !bCoverageRestored
		|| !bCoverageOtherSixExcluded
		|| CoverageOtherInstanceWorldTransforms.Num() != 6
		|| !CoverageMeshLocalBounds.IsValid
		|| CoverageRoi != CoverageProjectedRect))
	{
		bPassed = false;
		Failure = TEXT("Four ordered captures or verified target restoration are missing.");
	}
	TArray<TArray<uint8>> Images;
	const TCHAR* Names[] = {TEXT("coverage_a1"), TEXT("coverage_a2"),
		TEXT("coverage_b_target_hidden"), TEXT("coverage_a3_restored")};
	for (int32 Index = 0; bPassed && Index < 4; ++Index)
	{
		const TSharedPtr<FJsonObject>& Sample = Samples[Index];
		const FString CaptureId = FString::Printf(TEXT("%s_%02d"),
			*CoverageRunId, Index + 1);
		const FString ExpectedPath = FPaths::Combine(RunDirectory,
			FString(Names[Index]) + TEXT("_") + CaptureId + TEXT(".png"));
		const bool bShouldBeVisible = Index != 2;
		FString Comparable;
		const TSharedPtr<FJsonObject> Observed = Sample->GetObjectField(TEXT("observed_state"));
		if (Sample->GetStringField(TEXT("phase")) != Names[Index]
			|| Sample->GetStringField(TEXT("run_id")) != CoverageRunId
			|| Sample->GetStringField(TEXT("process_id")) != CoverageProcessId
			|| Sample->GetStringField(TEXT("run_start_ticks")) != CoverageRunStartTicks
			|| Sample->GetStringField(TEXT("capture_id")) != CaptureId
			|| Sample->GetIntegerField(TEXT("capture_seq")) != Index + 1
			|| Sample->GetStringField(TEXT("screenshot_path")) != ExpectedPath
			|| SerializeState(Sample->GetObjectField(TEXT("capture_source_pins")))
				!= CoveragePinnedSourcesJson
			|| SerializeState(Sample->GetObjectField(TEXT("registered_projection_modifiers")))
				!= CoverageProjectionRegistrationJson
			|| SerializeState(Sample->GetObjectField(TEXT("pre_request_projection_modifiers")))
				!= CoverageProjectionRegistrationJson
			|| SerializeState(Sample->GetObjectField(TEXT("processed_projection_modifiers")))
				!= CoverageProjectionRegistrationJson
			|| SerializeState(Sample->GetObjectField(TEXT("observed_projection_modifiers")))
				!= CoverageProjectionRegistrationJson
			|| Sample->GetBoolField(TEXT("coverage_target_visible")) != bShouldBeVisible
			|| Sample->GetArrayField(TEXT("coverage_roi_xyxy")).Num() != 4
			|| Sample->GetIntegerField(TEXT("screenshot_request_frame"))
				> Sample->GetIntegerField(TEXT("screenshot_processed_frame"))
			|| Sample->GetIntegerField(TEXT("screenshot_processed_frame"))
				>= Sample->GetIntegerField(TEXT("screenshot_observed_frame"))
			|| !CoverageComparableState(Observed, Comparable)
			|| Comparable != CoverageComparableBaseline)
		{
			bPassed = false;
			Failure = TEXT("Coverage phase, ROI, stable source state or frame ordering changed.");
			break;
		}
		TArray<uint8> FileBytes;
		TArray<uint8> Pixels;
		int64 Nonblack = 0;
		if (!ReadViewportPng(ExpectedPath, FileBytes, Pixels, Nonblack)
			|| FileBytes.Num() != Sample->GetIntegerField(TEXT("png_bytes"))
			|| RokTrainArmyMesh::Sha256(FileBytes) != Sample->GetStringField(TEXT("png_sha256"))
			|| RokTrainArmyMesh::Sha256(Pixels)
				!= Sample->GetStringField(TEXT("png_decoded_rgba_sha256"))
			|| Nonblack != Sample->GetIntegerField(TEXT("png_nonblack_rgb_pixels"))
			|| Nonblack <= 1024)
		{
			bPassed = false;
			Failure = TEXT("Full-size real coverage screenshot is missing, changed or black.");
			break;
		}
		Images.Add(MoveTemp(Pixels));
	}
	FCoveragePixelDifference NoiseA2, HiddenB, RestoredA3;
	if (bPassed)
	{
		NoiseA2 = CompareCoveragePixels(Images[0], Images[1], CoverageRoi,
			CoverageRgbThreshold);
		HiddenB = CompareCoveragePixels(Images[0], Images[2], CoverageRoi,
			CoverageRgbThreshold);
		RestoredA3 = CompareCoveragePixels(Images[0], Images[3], CoverageRoi,
			CoverageRgbThreshold);
		if (NoiseA2.MaxRgbDiff > CoverageMaxNoiseRgb
			|| RestoredA3.MaxRgbDiff > CoverageMaxNoiseRgb
			|| HiddenB.ThresholdPixels < CoverageMinConnectedPixels
			|| HiddenB.LargestConnectedPixels < CoverageMinConnectedPixels)
		{
			bPassed = false;
			Failure = TEXT("Target ROI lacks stable A1/A2/A3 and >=64 connected original-alias A1/B pixels at RGB delta >=16.");
		}
	}
	if (bCoverageTargetHidden)
	{
		FString RestoreReason;
		if (!SetCoverageTargetHidden(false, RestoreReason))
		{
			bPassed = false;
			Failure += TEXT(" Emergency target restore failed: ") + RestoreReason;
		}
		else
		{
			bPassed = false; // Emergency restoration is not a captured A3 success.
			Failure += TEXT(" Emergency target restore occurred without A3 proof.");
		}
	}
	bFinished = true;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.world_alias_pixel_coverage_diagnostic_runtime.v14"));
	Report->SetStringField(TEXT("game_mode"), TEXT("/Script/Rok.RokWorldInstancedMaterialDiagnosticGameMode"));
	Report->SetBoolField(TEXT("coverage_opt_in_flag"), bCoverageMode);
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetStringField(TEXT("reason"), Failure.IsEmpty() ? Reason : Reason + TEXT(" ") + Failure);
	Report->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Report->SetStringField(TEXT("run_directory"), RunDirectory);
	Report->SetStringField(TEXT("run_id"), CoverageRunId);
	Report->SetStringField(TEXT("process_id"), CoverageProcessId);
	Report->SetStringField(TEXT("run_start_ticks"), CoverageRunStartTicks);
	Report->SetObjectField(TEXT("capture_source_pins"),
		CoveragePinnedSources.IsValid() ? CoveragePinnedSources : MakeShared<FJsonObject>());
	Report->SetObjectField(TEXT("registered_projection_modifiers"),
		CoverageProjectionRegistration.IsValid()
			? CoverageProjectionRegistration : MakeShared<FJsonObject>());
	Report->SetStringField(TEXT("screenshot_origin"), TEXT("FScreenshotRequest_game_viewport"));
	Report->SetNumberField(TEXT("expected_viewport_width"), 1440);
	Report->SetNumberField(TEXT("expected_viewport_height"), 810);
	Report->SetObjectField(TEXT("begin_local_boundary"),
		BeginBoundary.IsValid() ? BeginBoundary : MakeShared<FJsonObject>());
	WriteLocalOnlyBoundary(*Report);
	Report->SetStringField(TEXT("alias_path"), AliasInputPath);
	Report->SetStringField(TEXT("alias_sha256"), AliasInputSha256);
	Report->SetStringField(TEXT("variants_creator_terminal_sha256"), VariantsReceiptSha256);
	Report->SetStringField(TEXT("river_creator_terminal_sha256"), RiverReceiptSha256);
	Report->SetStringField(TEXT("bridge_creator_terminal_sha256"), BridgeReceiptSha256);
	Report->SetStringField(TEXT("runtime_data_path"), Streaming.IsValid() && Streaming->RuntimeData
		? Streaming->RuntimeData->GetPathName() : TEXT(""));
	Report->SetStringField(TEXT("payload_sha256"), Streaming.IsValid() && Streaming->RuntimeData
		? Streaming->RuntimeData->CanonicalPayloadSha256 : TEXT(""));
	Report->SetObjectField(TEXT("runtime_data_snapshot"), CaptureRuntimeData());
	Report->SetBoolField(TEXT("static_visual_ready"), false);
	Report->SetBoolField(TEXT("runtime_ready"), false);
	Report->SetBoolField(TEXT("render_proxy_material_verified"), false);
	Report->SetBoolField(TEXT("formal_map_lod_accepted"), false);
	Report->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
	Report->SetBoolField(TEXT("unity_gamma_parity_accepted"), false);
	Report->SetBoolField(TEXT("full_migration_accepted"), false);
	Report->SetBoolField(TEXT("single_tree_alias_pixel_coverage_verified"), bPassed);
	Report->SetStringField(TEXT("target_component"), CoverageComponentName);
	Report->SetStringField(TEXT("target_mesh"), CoverageMeshPath);
	Report->SetStringField(TEXT("target_source_material"), CoverageSourceMaterial);
	Report->SetStringField(TEXT("target_alias_material"), CoverageTargetMaterial);
	Report->SetNumberField(TEXT("target_instance_index"), CoverageInstanceIndex);
	Report->SetNumberField(TEXT("target_streaming_generation"), static_cast<double>(CoverageGeneration));
	Report->SetArrayField(TEXT("target_source_focus_unity"), Vector2Numbers(CoverageFocus));
	Report->SetStringField(TEXT("source_camera_key"), TEXT("showbuild"));
	Report->SetNumberField(TEXT("source_camera_dxf"), CoverageDxf);
	Report->SetObjectField(TEXT("coverage_preflight_observation"),
		CoveragePreflightObservation.IsValid() ? CoveragePreflightObservation : MakeShared<FJsonObject>());
	Report->SetArrayField(TEXT("target_instance_world_transform"), TransformNumbers(CoverageInstanceWorld));
	Report->SetArrayField(TEXT("target_mesh_local_bounds_min"), TArray<TSharedPtr<FJsonValue>>{
		JsonNumber(CoverageMeshLocalBounds.Min.X), JsonNumber(CoverageMeshLocalBounds.Min.Y),
		JsonNumber(CoverageMeshLocalBounds.Min.Z)});
	Report->SetArrayField(TEXT("target_mesh_local_bounds_max"), TArray<TSharedPtr<FJsonValue>>{
		JsonNumber(CoverageMeshLocalBounds.Max.X), JsonNumber(CoverageMeshLocalBounds.Max.Y),
		JsonNumber(CoverageMeshLocalBounds.Max.Z)});
	Report->SetArrayField(TEXT("registered_camera_view_transform"), TransformNumbers(CoverageCameraViewTransform));
	Report->SetNumberField(TEXT("registered_camera_view_fov"), CoverageCameraViewFov);
	Report->SetNumberField(TEXT("registered_camera_view_aspect"), CoverageCameraViewAspect);
	Report->SetArrayField(TEXT("registered_camera_view_asymmetric_crop_fraction"),
		CoverageProjectionRegistration.IsValid()
			? CoverageProjectionRegistration->GetArrayField(TEXT("asymmetric_crop_fraction"))
			: TArray<TSharedPtr<FJsonValue>>());
	TArray<TSharedPtr<FJsonValue>> OtherTransforms;
	for (const FTransform& Other : CoverageOtherInstanceWorldTransforms)
	{
		OtherTransforms.Add(MakeShared<FJsonValueArray>(TransformNumbers(Other)));
	}
	Report->SetArrayField(TEXT("other_six_same_hism_instance_world_transforms"), OtherTransforms);
	Report->SetArrayField(TEXT("target_projected_rect_xyxy"), RectNumbers(CoverageProjectedRect));
	Report->SetArrayField(TEXT("target_roi_xyxy"), RectNumbers(CoverageRoi));
	Report->SetBoolField(TEXT("other_six_same_hism_instances_excluded_from_roi"),
		bCoverageOtherSixExcluded);
	Report->SetBoolField(TEXT("target_restore_attempted"), bCoverageRestoreAttempted);
	Report->SetBoolField(TEXT("target_restored"), bCoverageRestored
		&& CoverageTarget.IsValid() && CoverageTarget->IsVisible());
	Report->SetNumberField(TEXT("noise_max_rgb"), CoverageMaxNoiseRgb);
	Report->SetNumberField(TEXT("coverage_rgb_delta_threshold"), CoverageRgbThreshold);
	Report->SetNumberField(TEXT("coverage_min_connected_pixels"), CoverageMinConnectedPixels);
	Report->SetNumberField(TEXT("a1_a2_roi_max_rgb_diff"), NoiseA2.MaxRgbDiff);
	Report->SetNumberField(TEXT("a1_b_roi_threshold_pixels"), HiddenB.ThresholdPixels);
	Report->SetNumberField(TEXT("a1_b_roi_largest_connected_pixels"), HiddenB.LargestConnectedPixels);
	Report->SetNumberField(TEXT("a1_a3_roi_max_rgb_diff"), RestoredA3.MaxRgbDiff);
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const TSharedPtr<FJsonObject>& Sample : Samples)
	{
		Entries.Add(MakeShared<FJsonValueObject>(Sample));
	}
	Report->SetArrayField(TEXT("samples"), Entries);
	if (!bPassed && LastObservedSceneState.IsValid())
	{
		Report->SetNumberField(TEXT("last_observed_scene_frame"), static_cast<double>(LastObservedSceneFrame));
		Report->SetObjectField(TEXT("last_observed_scene_state"), LastObservedSceneState);
		Report->SetObjectField(TEXT("last_render_readiness_at_failure"), CaptureReadiness());
	}
	if (PendingSample.IsValid()) Report->SetObjectField(TEXT("pending_capture_at_failure"), PendingSample);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Path = FPaths::Combine(RunDirectory, TEXT("manifest.json"));
	const bool bSaved = FFileHelper::SaveStringToFile(
		Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("World alias pixel coverage passed=%s report=%s reason=%s"),
		bPassed ? TEXT("true") : TEXT("false"), *Path, *Report->GetStringField(TEXT("reason")));
	FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(bPassed && bSaved ? 0 : 1));
}

void ARokWorldInstancedMaterialDiagnosticGameMode::FinishFixedCameraProbe(
	bool bPassed, const FString& Reason)
{
	if (bFinished) return;
	FString Failure;
	if (!CheckLocalOnlyBoundary(Failure)) bPassed = false;
	if (bPassed && (Samples.Num() != 4 || PendingSample.IsValid()
		|| FixedProbeTargetKey.IsEmpty() || FixedProbeRoi.Width() <= 0
		|| FixedProbeRoi.Height() <= 0))
	{
		bPassed = false;
		Failure = TEXT("Four complete fixed-camera phases and one projected ground slot are required.");
	}
	TArray<TArray<uint8>> Decoded;
	constexpr const TCHAR* PhaseNames[] = {
		TEXT("fixed_lod0_a1"), TEXT("fixed_lod0_a2"),
		TEXT("fixed_lod1_b"), TEXT("fixed_lod0_a3")};
	FString BaselineCameraTransform, BaselineActualView, BaselineSubmittedView;
	FString BaselinePopulation, BaselineSource, BaselineAlias, BaselineLight;
	uint64 PreviousGeneration = 0;
	for (int32 Index = 0; bPassed && Index < 4; ++Index)
	{
		const TSharedPtr<FJsonObject>& Sample = Samples[Index];
		TArray<uint8> FileBytes, Pixels;
		int64 Nonblack = 0;
		if (!Sample.IsValid() || Sample->GetStringField(TEXT("phase")) != PhaseNames[Index]
			|| Sample->GetIntegerField(TEXT("lod")) != (Index == 2 ? 1 : 0)
			|| Sample->GetIntegerField(TEXT("center_chunk_x")) != 16
			|| Sample->GetIntegerField(TEXT("center_chunk_y")) != 2
			|| !FMath::IsNearlyEqual(Sample->GetNumberField(TEXT("camera_dxf")), InitialDxf, 0.001)
			|| !FMath::IsNearlyEqual(Sample->GetNumberField(TEXT("camera_unity_x")), MovedFocus.X, 0.001)
			|| !FMath::IsNearlyEqual(Sample->GetNumberField(TEXT("camera_unity_y")), MovedFocus.Y, 0.001)
			|| Sample->GetIntegerField(TEXT("directional_light_count")) != 1
			|| Sample->GetIntegerField(TEXT("sky_light_count")) != 0
			|| !Sample->GetBoolField(TEXT("registered_matches_active"))
			|| !Sample->GetObjectField(TEXT("pre_request_render_readiness"))
				->GetObjectField(TEXT("hism_render_readiness"))->GetBoolField(TEXT("all_ready"))
			|| !Sample->GetBoolField(TEXT("ground_slot_and_mips_stable_during_capture"))
			|| !ReadViewportPng(Sample->GetStringField(TEXT("screenshot_path")),
				FileBytes, Pixels, Nonblack)
			|| RokTrainArmyMesh::Sha256(FileBytes) != Sample->GetStringField(TEXT("png_sha256"))
			|| RokTrainArmyMesh::Sha256(Pixels) != Sample->GetStringField(TEXT("png_decoded_rgba_sha256"))
			|| Nonblack <= 1024
			|| Sample->GetObjectField(TEXT("ground_slot_observed"))
				->GetStringField(TEXT("identity_sha256")) != FixedProbeTargetKey)
		{
			bPassed = false;
			Failure = TEXT("Fixed-camera phase, target ground slot, or real nonblack PNG proof changed.");
			break;
		}
		TSharedPtr<FJsonObject> CameraComparable = MakeShared<FJsonObject>();
		CameraComparable->SetArrayField(TEXT("transform"),
			Sample->GetArrayField(TEXT("camera_actor_transform")));
		const FString CameraTransform = SerializeState(CameraComparable);
		const TSharedPtr<FJsonObject> Ground = Sample->GetObjectField(TEXT("ground_slot_observed"));
		const FString ActualView = SerializeState(Ground->GetObjectField(TEXT("actual_camera_view")));
		const FString SubmittedView = FixedProbeSubmittedComparable(
			Sample->GetObjectField(TEXT("submitted_view_processed")));
		if (FixedProbeSubmittedComparable(Sample->GetObjectField(TEXT("submitted_view_pre_request")))
			!= SubmittedView
			|| FixedProbeSubmittedComparable(Sample->GetObjectField(TEXT("submitted_view_observed")))
				!= SubmittedView)
		{
			bPassed = false;
			Failure = TEXT("Actual submitted camera view changed within a fixed-camera phase.");
			break;
		}
		const FString Population = Ground->GetStringField(TEXT("ground_population_sha256"));
		const FString Source = Ground->GetStringField(TEXT("source_material"));
		const FString Alias = Ground->GetStringField(TEXT("actual_alias_material"));
		const FString Light = SerializeState(Ground->GetObjectField(TEXT("directional_light")));
		const uint64 Generation = static_cast<uint64>(Sample->GetNumberField(TEXT("generation")));
		if (Index == 0)
		{
			BaselineCameraTransform = CameraTransform;
			BaselineActualView = ActualView;
			BaselineSubmittedView = SubmittedView;
			BaselinePopulation = Population;
			BaselineSource = Source;
			BaselineAlias = Alias;
			BaselineLight = Light;
		}
		if (CameraTransform != BaselineCameraTransform
			|| ActualView != BaselineActualView || SubmittedView != BaselineSubmittedView
			|| Light != BaselineLight
			|| Population != BaselinePopulation
			|| Source != BaselineSource || Alias != BaselineAlias
			|| (Index == 1 && Generation != PreviousGeneration)
			|| (Index >= 2 && Generation <= PreviousGeneration))
		{
			bPassed = false;
			Failure = TEXT("Fixed-camera component/submitted view, ground population/material, or build generation drifted.");
			break;
		}
		PreviousGeneration = Generation;
		Decoded.Add(MoveTemp(Pixels));
	}
	FCoveragePixelDifference NoiseA2, LodDelta, RestoreA3;
	if (bPassed)
	{
		NoiseA2 = CompareCoveragePixels(Decoded[0], Decoded[1], FixedProbeRoi, 16);
		LodDelta = CompareCoveragePixels(Decoded[0], Decoded[2], FixedProbeRoi, 16);
		RestoreA3 = CompareCoveragePixels(Decoded[0], Decoded[3], FixedProbeRoi, 16);
		if (NoiseA2.MaxRgbDiff > 2 || RestoreA3.MaxRgbDiff > 2)
		{
			bPassed = false;
			Failure = TEXT("Fixed-camera A1/A2 or A1/A3 ground ROI drift exceeds two RGB levels.");
		}
	}
	bFinished = true;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.world_lod_fixed_camera_ground_material_probe.v4"));
	Report->SetBoolField(TEXT("fixed_camera_probe_opt_in"), bFixedCameraProbeMode);
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetStringField(TEXT("reason"), Failure.IsEmpty() ? Reason : Reason + TEXT(" ") + Failure);
	Report->SetStringField(TEXT("run_directory"), RunDirectory);
	Report->SetStringField(TEXT("run_id"), CoverageRunId);
	Report->SetStringField(TEXT("process_id"), CoverageProcessId);
	Report->SetStringField(TEXT("run_start_ticks"), CoverageRunStartTicks);
	Report->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : TEXT(""));
	Report->SetStringField(TEXT("runtime_data_path"),
		Streaming.IsValid() && Streaming->RuntimeData ? Streaming->RuntimeData->GetPathName() : TEXT(""));
	Report->SetStringField(TEXT("payload_sha256"),
		Streaming.IsValid() && Streaming->RuntimeData
			? Streaming->RuntimeData->CanonicalPayloadSha256 : TEXT(""));
	Report->SetStringField(TEXT("alias_path"), AliasInputPath);
	Report->SetStringField(TEXT("alias_sha256"), AliasInputSha256);
	Report->SetStringField(TEXT("target_ground_identity_sha256"), FixedProbeTargetKey);
	Report->SetArrayField(TEXT("ground_roi_xyxy"), RectNumbers(FixedProbeRoi));
	Report->SetNumberField(TEXT("fixed_camera_dxf"), InitialDxf);
	Report->SetArrayField(TEXT("fixed_view_center_unity"), Vector2Numbers(MovedFocus));
	Report->SetNumberField(TEXT("lod0_generation"), static_cast<double>(FixedProbeLod0Generation));
	Report->SetNumberField(TEXT("lod1_generation"), static_cast<double>(FixedProbeLod1Generation));
	Report->SetNumberField(TEXT("noise_a1_a2_max_rgb"), NoiseA2.MaxRgbDiff);
	Report->SetNumberField(TEXT("lod_a1_b_threshold_pixels"), LodDelta.ThresholdPixels);
	Report->SetNumberField(TEXT("lod_a1_b_largest_connected_pixels"), LodDelta.LargestConnectedPixels);
	Report->SetNumberField(TEXT("lod_a1_b_max_rgb"), LodDelta.MaxRgbDiff);
	Report->SetNumberField(TEXT("restore_a1_a3_max_rgb"), RestoreA3.MaxRgbDiff);
	Report->SetBoolField(TEXT("formal_map_lod_accepted"), false);
	Report->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
	Report->SetBoolField(TEXT("full_migration_accepted"), false);
	Report->SetObjectField(TEXT("begin_local_boundary"),
		BeginBoundary.IsValid() ? BeginBoundary : MakeShared<FJsonObject>());
	WriteLocalOnlyBoundary(*Report);
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const TSharedPtr<FJsonObject>& Sample : Samples)
	{
		Entries.Add(MakeShared<FJsonValueObject>(Sample));
	}
	Report->SetArrayField(TEXT("samples"), Entries);
	if (!bPassed && LastObservedSceneState.IsValid())
	{
		Report->SetObjectField(TEXT("last_observed_scene_state"), LastObservedSceneState);
		if (Streaming.IsValid() && Camera.IsValid())
		{
			Report->SetObjectField(TEXT("last_render_readiness_at_failure"), CaptureReadiness());
		}
	}
	if (PendingSample.IsValid()) Report->SetObjectField(TEXT("pending_capture_at_failure"), PendingSample);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Path = FPaths::Combine(RunDirectory, TEXT("manifest.json"));
	const bool bSaved = FFileHelper::SaveStringToFile(
		Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("Fixed-camera ground probe passed=%s report=%s reason=%s"),
		bPassed ? TEXT("true") : TEXT("false"), *Path, *Report->GetStringField(TEXT("reason")));
	FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(bPassed && bSaved ? 0 : 1));
}

void ARokWorldInstancedMaterialDiagnosticGameMode::Finish(bool bPassed, const FString& Reason)
{
	if (bFixedCameraProbeMode)
	{
		FinishFixedCameraProbe(bPassed, Reason);
		return;
	}
	if (bCoverageMode)
	{
		FinishCoverage(bPassed, Reason);
		return;
	}
	if (bFinished)
	{
		return;
	}
	FString BoundaryReason;
	if (!CheckLocalOnlyBoundary(BoundaryReason))
	{
		bPassed = false;
	}
	if (bPassed && (Samples.Num() != 5 || PendingSample.IsValid()))
	{
		bPassed = false;
		BoundaryReason = TEXT("Five processed screenshots with later-frame state checks were not recorded.");
	}
	if (bPassed)
	{
		TArray<uint8> EmptyFileBytes;
		TArray<uint8> EmptyPixels;
		int64 EmptyNonblack = 0;
		if (!ReadViewportPng(Samples[3]->GetStringField(TEXT("screenshot_path")),
			EmptyFileBytes, EmptyPixels, EmptyNonblack))
		{
			bPassed = false;
			BoundaryReason = TEXT("Empty LOD2 screenshot could not be decoded for pixel comparison.");
		}
		for (int32 SampleIndex = 0; bPassed && SampleIndex < Samples.Num(); ++SampleIndex)
		{
			TArray<uint8> FileBytes;
			TArray<uint8> Pixels;
			int64 Nonblack = 0;
			const TSharedPtr<FJsonObject>& Sample = Samples[SampleIndex];
			if (!ReadViewportPng(Sample->GetStringField(TEXT("screenshot_path")),
				FileBytes, Pixels, Nonblack)
				|| Pixels.Num() != EmptyPixels.Num()
				|| RokTrainArmyMesh::Sha256(FileBytes) != Sample->GetStringField(TEXT("png_sha256"))
				|| RokTrainArmyMesh::Sha256(Pixels) != Sample->GetStringField(TEXT("png_decoded_rgba_sha256"))
				|| Nonblack != Sample->GetIntegerField(TEXT("png_nonblack_rgb_pixels")))
			{
				bPassed = false;
				BoundaryReason = TEXT("Viewport screenshot changed or cannot be decoded before final pixel comparison.");
				break;
			}
			int64 ChangedRgbPixels = 0;
			int32 MaxRgbDiff = 0;
			for (int32 PixelByte = 0; PixelByte < Pixels.Num(); PixelByte += 4)
			{
				const int32 RedDiff = FMath::Abs(int32(Pixels[PixelByte]) - int32(EmptyPixels[PixelByte]));
				const int32 GreenDiff = FMath::Abs(int32(Pixels[PixelByte + 1]) - int32(EmptyPixels[PixelByte + 1]));
				const int32 BlueDiff = FMath::Abs(int32(Pixels[PixelByte + 2]) - int32(EmptyPixels[PixelByte + 2]));
				const int32 PixelMaxDiff = FMath::Max(RedDiff, FMath::Max(GreenDiff, BlueDiff));
				MaxRgbDiff = FMath::Max(MaxRgbDiff, PixelMaxDiff);
				ChangedRgbPixels += PixelMaxDiff > 2;
			}
			Sample->SetNumberField(TEXT("png_changed_rgb_pixels_vs_empty"), static_cast<double>(ChangedRgbPixels));
			Sample->SetNumberField(TEXT("png_max_rgb_diff_vs_empty"), MaxRgbDiff);
			if (SampleIndex != 3 && (ChangedRgbPixels <= 1024 || MaxRgbDiff <= 2
				|| Nonblack <= 1024))
			{
				bPassed = false;
				BoundaryReason = TEXT("Active streamed viewport has too few decoded RGB pixels differing from empty LOD2 viewport.");
				break;
			}
		}
	}
	bFinished = true;
	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("rok.world_instanced_material_diagnostic_runtime.v10"));
	Report->SetStringField(TEXT("game_mode"), TEXT("/Script/Rok.RokWorldInstancedMaterialDiagnosticGameMode"));
	Report->SetStringField(TEXT("alias_path"), AliasInputPath);
	Report->SetStringField(TEXT("alias_sha256"), AliasInputSha256);
	Report->SetStringField(TEXT("variants_creator_terminal_sha256"), VariantsReceiptSha256);
	Report->SetStringField(TEXT("river_creator_terminal_sha256"), RiverReceiptSha256);
	Report->SetStringField(TEXT("bridge_creator_terminal_sha256"), BridgeReceiptSha256);
	Report->SetBoolField(TEXT("diagnostic_directional_light_only"), true);
	Report->SetBoolField(TEXT("render_proxy_material_verified"), false);
	Report->SetBoolField(TEXT("formal_map_lod_accepted"), false);
	Report->SetBoolField(TEXT("unity_pixel_parity_accepted"), false);
	Report->SetBoolField(TEXT("unity_gamma_parity_accepted"), false);
	Report->SetBoolField(TEXT("full_migration_accepted"), false);
	Report->SetStringField(TEXT("screenshot_origin"), TEXT("FScreenshotRequest_game_viewport"));
	Report->SetStringField(TEXT("run_directory"), RunDirectory);
	Report->SetNumberField(TEXT("expected_viewport_width"), 1440);
	Report->SetNumberField(TEXT("expected_viewport_height"), 810);
	Report->SetObjectField(TEXT("begin_local_boundary"),
		BeginBoundary.IsValid() ? BeginBoundary : MakeShared<FJsonObject>());
	Report->SetBoolField(TEXT("passed"), bPassed);
	Report->SetStringField(TEXT("reason"), BoundaryReason.IsEmpty() ? Reason : Reason + TEXT(" ") + BoundaryReason);
	WriteLocalOnlyBoundary(*Report);
	Report->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Report->SetStringField(TEXT("runtime_data_path"),
		Streaming.IsValid() && Streaming->RuntimeData
			? Streaming->RuntimeData->GetPathName() : TEXT(""));
	Report->SetStringField(TEXT("payload_sha256"),
		Streaming.IsValid() && Streaming->RuntimeData
			? Streaming->RuntimeData->CanonicalPayloadSha256 : TEXT(""));
	Report->SetBoolField(TEXT("static_visual_ready"),
		Streaming.IsValid() && Streaming->RuntimeData
			&& Streaming->RuntimeData->bStaticVisualReady);
	Report->SetBoolField(TEXT("runtime_ready"),
		Streaming.IsValid() && Streaming->RuntimeData
			&& Streaming->RuntimeData->bRuntimeReady);
	Report->SetObjectField(TEXT("runtime_data_snapshot"), CaptureRuntimeData());
	TArray<TSharedPtr<FJsonValue>> Entries;
	for (const TSharedPtr<FJsonObject>& Sample : Samples)
	{
		Entries.Add(MakeShared<FJsonValueObject>(Sample));
	}
	Report->SetArrayField(TEXT("samples"), Entries);
	if (!bPassed && LastObservedSceneState.IsValid())
	{
		Report->SetNumberField(TEXT("last_observed_scene_frame"),
			static_cast<double>(LastObservedSceneFrame));
		Report->SetObjectField(TEXT("last_observed_scene_state"), LastObservedSceneState);
	}
	if (!bPassed)
	{
		Report->SetObjectField(TEXT("last_render_readiness_at_failure"), CaptureReadiness());
	}
	if (PendingSample.IsValid())
	{
		Report->SetObjectField(TEXT("pending_capture_at_failure"), PendingSample);
	}
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Path = FPaths::Combine(RunDirectory, TEXT("manifest.json"));
	const bool bReportSaved = FFileHelper::SaveStringToFile(
		Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogTemp, Display, TEXT("World streaming diagnostic passed=%s report=%s reason=%s"),
		bPassed ? TEXT("true") : TEXT("false"), *Path, *Reason);
	if (!bReportSaved)
	{
		UE_LOG(LogTemp, Error,
			TEXT("World streaming diagnostic could not save report '%s'."), *Path);
	}
	FPlatformMisc::RequestExitWithStatus(false,
		static_cast<uint8>(bPassed && bReportSaved ? 0 : 1));
}
