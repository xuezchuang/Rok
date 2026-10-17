#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRHICommandListImmediate;
struct IPooledRenderTarget;

/** One isolated Hall fileID 212751466141171401 source-mesh/material frame. */
struct ROKRENDERING_API FRokCityHallCameraMaterialProbeInput
{
	FName SourceKey;
	FBufferRHIRef Positions; // original row27 float32 XYZ, 240 * 12 bytes
	FBufferRHIRef UV0;       // original row27 float32 xyzw, 240 * 16 bytes
	FBufferRHIRef Indices;   // original row27 uint32, 714 * 4 bytes
	FTextureRHIRef SpriteTexture; // expanded BGRA8 or original BC3 source mip0, row0 == Unity UV.v=0
	FTextureRHIRef MaskTexture;
	bool bSourceCompressed = false; // BC3/BC1 source blocks; private audit variant only.
	FMatrix44f SourceLocalToClip = FMatrix44f::Identity; // transpose(P_gpuRT * V * W), f32 row-major
	bool bObjectThenVP = false; // isolated UnityObjectToClipPos operation-order candidate.
	FMatrix44f SourceObjectToWorld = FMatrix44f::Identity; // transpose(native renderer W)
	FMatrix44f SourceViewProjection = FMatrix44f::Identity; // transpose(source-derived P_gpuRT * V)
	FLinearColor Team1 = FLinearColor::White;
	FLinearColor Team2 = FLinearColor::White;
	FLinearColor NightColor3 = FLinearColor::Black;
	FLinearColor SpriteColor = FLinearColor::White;
	FLinearColor RendererTint = FLinearColor::White;
	float CityTransparency = 1.0f;
	float ControlledTimeY = 0.0f;
	bool bNight = false;
	bool bWhiteBackground = false;
};

/**
 * Render-thread-only, private 1440x810 linear BGRA8 target. The original source geometry,
 * source material formula and RGBA SrcAlpha/OneMinusSrcAlpha blend are reproduced on the GPU.
 * It never registers a GameApp view, mutates a scene, or certifies the live GameApp LOD.
 * The caller must SHA-check source inputs and retain the extracted target through GPU readback.
 */
class ROKRENDERING_API FRokCityHallCameraMaterialProbe
{
public:
	static bool Render(FRHICommandListImmediate& RHICmdList,
		const FRokCityHallCameraMaterialProbeInput& Input,
		TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError);
	// Diagnostic-only source draw. Modes 1..6 emit UV/Main/Mask/preblend/sentinel/unblended
	// into a private linear float RT. Caller must first prove legacy Render() output.
	static bool RenderDiagnostic(FRHICommandListImmediate& RHICmdList,
		const FRokCityHallCameraMaterialProbeInput& Input, uint32 DiagnosticMode,
		TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError);
};
