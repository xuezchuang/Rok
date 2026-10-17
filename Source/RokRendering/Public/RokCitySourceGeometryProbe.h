#pragma once

#include "CoreMinimal.h"
#include "RHIResources.h"

class FRHICommandListImmediate;
struct IPooledRenderTarget;

/**
 * One isolated source-mesh upload. All three buffers contain the original little-endian bytes:
 * float32 XY or XYZ positions, float32 UV0 xy or xyzw, and uint32 triangle indices.
 * The caller verifies source identity, hashes, index bounds, and full-buffer GPU readback.
 * Buffers must be in SRVMask state while this call runs, with no concurrent writes.
 * SourceKey is a diagnostic label only; this helper does not authenticate a Unity asset.
 */
struct ROKRENDERING_API FRokCitySourceGeometryProbeInput
{
	FName SourceKey;
	FBufferRHIRef Positions;
	FBufferRHIRef UV0;
	FBufferRHIRef Indices;
	uint32 VertexCount = 0;
	uint32 IndexCount = 0;
	uint32 PositionStrideBytes = 0; // 8: XY with intrinsic z=0, or 12: XYZ.
	uint32 UVStrideBytes = 0; // 8: xy, or 16: xyzw; zw remains in the uploaded buffer.
	FMatrix44f LocalToCanonicalClip = FMatrix44f::Identity;
};

/**
 * A render-thread-only geometry/UV diagnostic. It creates and extracts its own linear
 * 128x128 PF_B8G8R8A8 RDG target; there is no Scene, View, SceneColor, or viewport input.
 * The output is UV.x, UV.y, 0.25 in RGB. It is not a Unity material or alpha comparison.
 * The caller owns the returned pooled target through its asynchronous GPU readback.
 * The caller supplies the fixture's source-local canonical matrix as 16 little-endian float32
 * values in row order m00..m33 (64 bytes). VS uses mul(float4(local,1), matrix), with clip
 * z=0.5 and w=1 supplied by that matrix. This helper never applies a GameApp transform or
 * derives an unknown source axis from the mesh. Indices are read by SV_VertexID from a raw
 * buffer (vertex pulling); this does not test an input-assembler index binding or declaration.
 */
class ROKRENDERING_API FRokCitySourceGeometryProbe
{
public:
	static bool Render(FRHICommandListImmediate& RHICmdList,
		const FRokCitySourceGeometryProbeInput& Input,
		TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError);
};
