#include "RokCitySourceGeometryProbe.h"

#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"

namespace RokCitySourceGeometryProbe
{
constexpr int32 Extent = 128;
static_assert(sizeof(FMatrix44f) == 16 * sizeof(float), "Canonical matrix must be 64 float32 bytes");

class FProbeVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FProbeVS);
	SHADER_USE_PARAMETER_STRUCT(FProbeVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(ByteAddressBuffer, Positions)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, UV0)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, Indices)
		SHADER_PARAMETER(uint32, PositionStrideBytes)
		SHADER_PARAMETER(uint32, UVStrideBytes)
		SHADER_PARAMETER(FMatrix44f, LocalToCanonicalClip)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FProbeVS, "/Rok/Private/RokCitySourceGeometryProbe.usf", "SourceGeometryProbeVS", SF_Vertex);

class FProbePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FProbePS);
	SHADER_USE_PARAMETER_STRUCT(FProbePS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FProbeVS::FParameters, VS)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FProbePS, "/Rok/Private/RokCitySourceGeometryProbe.usf", "SourceGeometryProbePS", SF_Pixel);

bool Validate(const FRokCitySourceGeometryProbeInput& Input, FString& OutError)
{
	if (Input.SourceKey.IsNone())
	{
		OutError = TEXT("Source geometry probe requires a nonempty diagnostic key");
		return false;
	}
	if (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5)
	{
		OutError = TEXT("Source geometry probe requires SM5");
		return false;
	}
	if (!Input.Positions.IsValid() || !Input.UV0.IsValid() || !Input.Indices.IsValid())
	{
		OutError = TEXT("Source geometry probe requires three source byte buffers");
		return false;
	}
	if (Input.VertexCount < 3 || Input.IndexCount < 3 || Input.IndexCount % 3 != 0 ||
		Input.IndexCount > static_cast<uint32>(MAX_int32) ||
		(Input.PositionStrideBytes != 8 && Input.PositionStrideBytes != 12) ||
		(Input.UVStrideBytes != 8 && Input.UVStrideBytes != 16))
	{
		OutError = TEXT("Source geometry probe counts or float32 strides are invalid");
		return false;
	}
	const auto HasRawSRVUsage = [](const FBufferRHIRef& Buffer)
	{
		return EnumHasAllFlags(Buffer->GetUsage(),
			EBufferUsageFlags::ShaderResource | EBufferUsageFlags::ByteAddressBuffer);
	};
	if (!HasRawSRVUsage(Input.Positions) || !HasRawSRVUsage(Input.UV0) ||
		!HasRawSRVUsage(Input.Indices))
	{
		OutError = TEXT("Source geometry probe buffers require raw shader-resource usage");
		return false;
	}
	if (static_cast<uint64>(Input.VertexCount) * Input.PositionStrideBytes != Input.Positions->GetSize() ||
		static_cast<uint64>(Input.VertexCount) * Input.UVStrideBytes != Input.UV0->GetSize() ||
		static_cast<uint64>(Input.IndexCount) * sizeof(uint32) != Input.Indices->GetSize())
	{
		OutError = TEXT("Source geometry probe buffer sizes differ from declared source bytes");
		return false;
	}
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			if (!FMath::IsFinite(Input.LocalToCanonicalClip.M[Row][Column]))
			{
				OutError = TEXT("Source geometry probe matrix is nonfinite");
				return false;
			}
		}
	}
	return true;
}

void Draw(FRHICommandList& RHICmdList, const FProbePS::FParameters* Pass,
	TShaderMapRef<FProbeVS> VertexShader, TShaderMapRef<FProbePS> PixelShader, uint32 IndexCount)
{
	FGraphicsPipelineStateInitializer PSO;
	RHICmdList.ApplyCachedRenderTargets(PSO);
	PSO.BlendState = TStaticBlendState<CW_RGB>::GetRHI();
	PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	PSO.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
	PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	PSO.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	PSO.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, PSO, 0);
	RHICmdList.SetViewport(0, 0, 0.0f, Extent, Extent, 1.0f);
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Pass->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Pass);
	RHICmdList.SetStreamSource(0, nullptr, 0);
	RHICmdList.DrawPrimitive(0, static_cast<int32>(IndexCount / 3), 1);
}
} // namespace RokCitySourceGeometryProbe

bool FRokCitySourceGeometryProbe::Render(FRHICommandListImmediate& RHICmdList,
	const FRokCitySourceGeometryProbeInput& Input,
	TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError)
{
	check(IsInRenderingThread());
	OutTarget.SafeRelease();
	OutError.Reset();
	if (!RokCitySourceGeometryProbe::Validate(Input, OutError)) return false;

	// The audit uploads original source bytes and verifies those buffers independently.
	// These raw views only interpret them for this isolated GPU raster diagnostic.
	FShaderResourceViewRHIRef PositionSRV = RHICmdList.CreateShaderResourceView(Input.Positions,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef UVSRV = RHICmdList.CreateShaderResourceView(Input.UV0,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef IndexSRV = RHICmdList.CreateShaderResourceView(Input.Indices,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	if (!PositionSRV.IsValid() || !UVSRV.IsValid() || !IndexSRV.IsValid())
	{
		OutError = TEXT("Source geometry probe could not create raw GPU views");
		return false;
	}

	FRDGBuilder GraphBuilder(RHICmdList);
	const FLinearColor Background(0.07f, 0.15f, 0.71f, 1.0f);
	FRDGTextureRef Target = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(
		FIntPoint(RokCitySourceGeometryProbe::Extent, RokCitySourceGeometryProbe::Extent),
		PF_B8G8R8A8, FClearValueBinding(Background),
		TexCreate_RenderTargetable | TexCreate_ShaderResource), TEXT("RokSourceGeometryProbe128BGRA"));
	AddClearRenderTargetPass(GraphBuilder, Target, Background);
	auto* Pass = GraphBuilder.AllocParameters<RokCitySourceGeometryProbe::FProbePS::FParameters>();
	Pass->VS.Positions = PositionSRV.GetReference();
	Pass->VS.UV0 = UVSRV.GetReference();
	Pass->VS.Indices = IndexSRV.GetReference();
	Pass->VS.PositionStrideBytes = Input.PositionStrideBytes;
	Pass->VS.UVStrideBytes = Input.UVStrideBytes;
	Pass->VS.LocalToCanonicalClip = Input.LocalToCanonicalClip;
	Pass->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
	TShaderMapRef<RokCitySourceGeometryProbe::FProbeVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<RokCitySourceGeometryProbe::FProbePS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const uint32 IndexCount = Input.IndexCount;
	GraphBuilder.AddPass(RDG_EVENT_NAME("RokSourceGeometryProbe.Isolated(%s)", *Input.SourceKey.ToString()),
		Pass, ERDGPassFlags::Raster,
		[Pass, VertexShader, PixelShader, IndexCount, PositionSRV, UVSRV, IndexSRV](FRHICommandList& DrawCmdList)
		{
			// Strong refs keep these non-RDG raw views alive through graph execution.
			(void)PositionSRV; (void)UVSRV; (void)IndexSRV;
			RokCitySourceGeometryProbe::Draw(DrawCmdList, Pass, VertexShader, PixelShader, IndexCount);
		});
	GraphBuilder.QueueTextureExtraction(Target, &OutTarget);
	GraphBuilder.Execute();
	if (!OutTarget.IsValid())
	{
		OutError = TEXT("Source geometry probe target extraction failed");
		return false;
	}
	return true;
}
