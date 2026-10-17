#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRenderTarget;
class FSceneInterface;
class FRDGBuilder;
class FRDGTexture;

/** Source mesh coordinates are centimetres, with Unity Sprite UVs preserved. */
struct ROKRENDERING_API FRokCityGammaRasterVertex
{
	FVector3f LocalCm = FVector3f::ZeroVector;
	FVector2f UV = FVector2f::ZeroVector;
};

/**
 * A value snapshot for one explicitly approved city sprite. The caller keeps any UObject
 * asset owners alive while registered; only these values and strong RHI texture refs reach RT.
 * The caller must suppress the corresponding ordinary mesh draw to avoid rendering it twice,
 * and must supply the source sorting order and exact source texture/mask RHI resources. RHI
 * descriptors cannot prove source asset identity: the Rok caller verifies asset path/content
 * provenance and import settings before obtaining and submitting these references.
 */
struct ROKRENDERING_API FRokCityGammaRasterInstance
{
	FName ModelId;
	uint64 InstanceId = 0;
	TArray<FRokCityGammaRasterVertex> Vertices;
	TArray<uint32> Indices;
	FMatrix44d LocalToWorld = FMatrix44d::Identity;
	FTextureRHIRef SpriteTexture;
	FTextureRHIRef MaskTexture;
	int32 SortPriority = 0;
	int32 SourceOrder = 0;
	FLinearColor RendererTint = FLinearColor::White;
	FLinearColor Team1 = FLinearColor::White;
	FLinearColor Team2 = FLinearColor::White;
	FLinearColor Lighting = FLinearColor::White;
	float Transparency = 1.0f;
};

/** Isolated Unity 2019 control fixtures only. This is not a 32-row GameApp packet. */
enum class ERokCityGammaControlledFamily : uint8
{
	NightMaskedRGB,
	GroundOpaqueSplat,
	GroundAlphaSplat,
	RoadDefaultWhiteMask,
	WallShadowTransparent
};

enum class ERokCityGammaControlledSlot : uint8
{
	Main, Mask, Tex0, Tex1, Tex2, Tex3, Splat
};

struct ROKRENDERING_API FRokCityGammaControlledVertex
{
	FVector3f LocalCm = FVector3f::ZeroVector;
	FVector2f UV = FVector2f::ZeroVector;
	float Alpha = 1.0f;
};

struct ROKRENDERING_API FRokCityGammaControlledTexture
{
	ERokCityGammaControlledSlot Slot = ERokCityGammaControlledSlot::Main;
	FTextureRHIRef Texture;
	FIntPoint Extent = FIntPoint::ZeroValue;
	int32 NumMips = 0;
	bool bRepeat = false;
	bool bBilinear = false;
	FString FixtureBgraSha256; // Declared fixture bytes; only the independent GPU readback can prove them.
	// The first controlled gate accepts only the source probe's explicit 2x2 constant inputs.
	// Native multi-mip admission remains closed until independent upload/readback proof is pinned.
};

struct ROKRENDERING_API FRokCityGammaControlledInstance
{
	ERokCityGammaControlledFamily Family = ERokCityGammaControlledFamily::NightMaskedRGB;
	FName SourceKey;
	FString MaterialGuid;
	FString RendererFileId;
	FString MeshFileId;
	uint64 InstanceId = 0;
	TArray<FRokCityGammaControlledVertex> Vertices;
	TArray<uint32> Indices;
	FMatrix44d LocalToWorld = FMatrix44d::Identity;
	TArray<FRokCityGammaControlledTexture> Textures;
	int32 SortPriority = 0;
	int32 SourceOrder = 0;
	FLinearColor RendererTint = FLinearColor::White;
	FLinearColor GlobalSpriteColor = FLinearColor::White;
	FLinearColor MaterialColor = FLinearColor::White;
	FLinearColor Team1 = FLinearColor::White;
	FLinearColor Team2 = FLinearColor::White;
	FLinearColor NightColor3 = FLinearColor::Black;
	FVector4f TexST[4] = { FVector4f(1, 1, 0, 0), FVector4f(1, 1, 0, 0),
		FVector4f(1, 1, 0, 0), FVector4f(1, 1, 0, 0) };
	float Transparency = 1.0f;
	float ControlledTimeY = 0.0f;
	bool bControlledTimeQualified = false;
};

enum class ERokCityGammaRasterReject : uint8
{
	None,
	Unavailable,
	NullIdentity,
	DuplicateTarget,
	InvalidSnapshot,
	SourceGeometryMismatch,
	UnsupportedView,
	UnsupportedTarget,
	MissingUniformBuffer,
	DuplicateView
};

struct ROKRENDERING_API FRokCityGammaRasterStats
{
	uint64 RegisteredViews = 0;
	uint64 ScheduledFrames = 0;
	uint64 DrawnInstances = 0;
	uint64 UnsupportedViews = 0;
	uint64 UnsupportedTargets = 0;
	uint64 MissingUniformBuffers = 0;
	uint64 DuplicateViews = 0;
	uint64 RejectedSnapshots = 0;
	uint64 SourceGeometryMismatches = 0;
	uint64 RegisteredTargets = 0;
	uint64 ActiveInstances = 0;
	ERokCityGammaRasterReject LastReject = ERokCityGammaRasterReject::None;
};

/**
 * Main-view opt-in is empty until Register succeeds. Scene is UWorld::Scene and Target is
 * the exact game FViewport/FRenderTarget used in the view family. No SceneCapture is accepted.
 * All calls are game-thread-only; Reset waits for RT to discard the snapshot before owners
 * release their textures or render target. This is separate from the accepted capture compositor.
 * This pass runs after opaque and before ordinary translucency. Source sprites may be sorted
 * against one another here, but cannot interleave with unrelated translucent primitives.
 * SceneColor alpha is preserved. Source alpha controls RGB blending only; this is not a
 * Unity RGBA render-target equivalence contract.
 */
class ROKRENDERING_API FRokCityGammaRasterRegistration
{
public:
	FRokCityGammaRasterRegistration() = default;
	~FRokCityGammaRasterRegistration();
	FRokCityGammaRasterRegistration(const FRokCityGammaRasterRegistration&) = delete;
	FRokCityGammaRasterRegistration& operator=(const FRokCityGammaRasterRegistration&) = delete;
	FRokCityGammaRasterRegistration(FRokCityGammaRasterRegistration&& Other) noexcept;
	FRokCityGammaRasterRegistration& operator=(FRokCityGammaRasterRegistration&& Other) noexcept;

	bool Register(const FSceneInterface* Scene, const FRenderTarget* Target, FString& OutError);
	/** A failed update clears the prior instance snapshot. Caller restores ordinary components and Reset()s. */
	bool UpdateSnapshot(const TArray<FRokCityGammaRasterInstance>& Instances, FString& OutError);
	void Reset();
	bool IsRegistered() const { return Token != 0; }

private:
	uint64 Token = 0;
};

class ROKRENDERING_API FRokCityGammaRaster
{
public:
	static bool IsAvailable();
	static FRokCityGammaRasterStats GetStats();
	static const TCHAR* RejectName(ERokCityGammaRasterReject Reason);
	/** GT fixture qualification; never promotes a GameApp or source-mesh packet. */
	static bool ValidateControlledFixture(const TArray<FRokCityGammaControlledInstance>& Instances, FString& OutError);
	/** RT-only: add one qualified fixture draw into a 128x128 Gamma BGRA8 target with read-only depth.
	 * No registered Scene/Viewport is used. Caller must supply isolated fixture textures and target.
	 */
	static bool AddControlledFixturePass(FRDGBuilder& GraphBuilder, FRDGTexture* Color, FRDGTexture* Depth,
		const FIntRect& Rect, const FMatrix44f& LocalToClip,
		const FRokCityGammaControlledInstance& Instance, FString& OutError);

private:
	friend class FRokCityGammaRasterRegistration;
	friend class FRokRenderingModule;
	static bool RegisterTarget(const FSceneInterface* Scene, const FRenderTarget* Target, uint64& OutToken, FString& OutError);
	static bool UpdateTarget(uint64 Token, const TArray<FRokCityGammaRasterInstance>& Instances, FString& OutError);
	static void UnregisterTarget(uint64 Token);
	static void Startup();
	static void Shutdown();
};
