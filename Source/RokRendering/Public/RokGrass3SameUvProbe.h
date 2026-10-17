#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

struct IPooledRenderTarget;
class FRHICommandListImmediate;

/** Two separate 152x1 RGBA16F targets: raw no-blend and source blend over clear. */
class ROKRENDERING_API FRokGrass3SameUvProbe
{
public:
	static constexpr int32 SampleCount = 144;
	static constexpr int32 ControlCount = 8;
	static constexpr int32 Width = SampleCount + ControlCount;
	static bool Render(FRHICommandListImmediate& RHICmdList, const FTextureRHIRef& Atlas,
		TRefCountPtr<IPooledRenderTarget>& OutRaw,
		TRefCountPtr<IPooledRenderTarget>& OutBlended, FString& OutError);
};
