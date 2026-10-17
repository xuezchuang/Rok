#include "RokGrass3SameUvProbe.h"

#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHICommandList.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"

namespace RokGrass3SameUv
{
class FVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVS);
	SHADER_USE_PARAMETER_STRUCT(FVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, ) END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
	{ return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FVS, "/Rok/Private/RokGrass3SameUv.usf", "SameUvVS", SF_Vertex);

class FPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPS);
	SHADER_USE_PARAMETER_STRUCT(FPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SourceAtlas)
		SHADER_PARAMETER_SAMPLER(SamplerState, AtlasSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& P)
	{ return IsFeatureLevelSupported(P.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FPS, "/Rok/Private/RokGrass3SameUv.usf", "SameUvPS", SF_Pixel);

void AddSamplePass(FRDGBuilder& Graph, FRDGTextureRef Target, FRHITexture* Atlas,
	bool bBlend)
{
	auto* Pass = Graph.AllocParameters<FPS::FParameters>();
	Pass->SourceAtlas = Atlas;
	Pass->AtlasSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	Pass->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
	TShaderMapRef<FVS> VS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<FPS> PS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	Graph.AddPass(RDG_EVENT_NAME("RokGrass3SameUv.%s", bBlend ? TEXT("SourceBlend") : TEXT("Raw")),
		Pass, ERDGPassFlags::Raster,
		[Pass, VS, PS, Atlas, bBlend](FRHICommandList& Cmd)
		{
			(void)Atlas;
			FGraphicsPipelineStateInitializer State;
			Cmd.ApplyCachedRenderTargets(State);
			State.BlendState = bBlend
				? TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,
					BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI()
				: TStaticBlendState<>::GetRHI();
			State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			State.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
			State.BoundShaderState.PixelShaderRHI = PS.GetPixelShader();
			State.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(Cmd, State, 0);
			Cmd.SetViewport(0, 0, 0, FRokGrass3SameUvProbe::Width, 1, 1);
			SetShaderParameters(Cmd, PS, PS.GetPixelShader(), *Pass);
			Cmd.SetStreamSource(0, nullptr, 0);
			Cmd.DrawPrimitive(0, 1, 1);
		});
}
}

bool FRokGrass3SameUvProbe::Render(FRHICommandListImmediate& RHICmdList,
	const FTextureRHIRef& Atlas, TRefCountPtr<IPooledRenderTarget>& OutRaw,
	TRefCountPtr<IPooledRenderTarget>& OutBlended, FString& OutError)
{
	check(IsInRenderingThread());
	OutRaw.SafeRelease(); OutBlended.SafeRelease(); OutError.Reset();
	if (GUsingNullRHI || GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5 || !Atlas.IsValid())
	{ OutError = TEXT("Real SM5 RHI and existing atlas texture required"); return false; }
	const FRHITextureDesc& Desc = Atlas->GetDesc();
	if (Desc.Extent.X != 2048 || Desc.Extent.Y != 2048 || Desc.NumSamples != 1
		|| Desc.NumMips < 1 || !EnumHasAnyFlags(Desc.Flags, TexCreate_ShaderResource))
	{ OutError = TEXT("Existing atlas RHI descriptor unavailable or unexpected; no replacement texture allowed"); return false; }
	FRDGBuilder Graph(RHICmdList);
	const FRDGTextureDesc TargetDesc = FRDGTextureDesc::Create2D(FIntPoint(Width, 1),
		PF_FloatRGBA, FClearValueBinding(FLinearColor::Transparent),
		TexCreate_RenderTargetable | TexCreate_ShaderResource);
	FRDGTextureRef Raw = Graph.CreateTexture(TargetDesc, TEXT("RokGrass3SameUvRawRGBA16F"));
	FRDGTextureRef Blended = Graph.CreateTexture(TargetDesc, TEXT("RokGrass3SameUvBlendedRGBA16F"));
	AddClearRenderTargetPass(Graph, Raw, FLinearColor::Transparent);
	AddClearRenderTargetPass(Graph, Blended, FLinearColor::Transparent);
	RokGrass3SameUv::AddSamplePass(Graph, Raw, Atlas.GetReference(), false);
	RokGrass3SameUv::AddSamplePass(Graph, Blended, Atlas.GetReference(), true);
	Graph.QueueTextureExtraction(Raw, &OutRaw);
	Graph.QueueTextureExtraction(Blended, &OutBlended);
	Graph.Execute();
	if (!OutRaw.IsValid() || !OutBlended.IsValid())
	{ OutError = TEXT("Two independent GPU targets were not extracted"); return false; }
	return true;
}
