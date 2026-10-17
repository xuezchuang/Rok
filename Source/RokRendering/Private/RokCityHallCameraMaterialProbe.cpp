#include "RokCityHallCameraMaterialProbe.h"

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

namespace RokHallMaterialProbe
{
constexpr int32 Width = 1440, Height = 810;
constexpr uint32 VertexCount = 240, IndexCount = 714;

class FHallVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHallVS);
	SHADER_USE_PARAMETER_STRUCT(FHallVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(ByteAddressBuffer, Positions)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, UV0)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, Indices)
		SHADER_PARAMETER(FMatrix44f, SourceLocalToClip)
		SHADER_PARAMETER(FMatrix44f, SourceObjectToWorld)
		SHADER_PARAMETER(FMatrix44f, SourceViewProjection)
		SHADER_PARAMETER(uint32, bObjectThenVP)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FHallVS, "/Rok/Private/RokCityHallCameraMaterial.usf", "HallMaterialVS", SF_Vertex);

class FHallPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHallPS);
	SHADER_USE_PARAMETER_STRUCT(FHallPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHallVS::FParameters, VS)
		SHADER_PARAMETER_TEXTURE(Texture2D, SpriteTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaskTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, MaskSampler)
		SHADER_PARAMETER(FVector4f, RendererTint)
		SHADER_PARAMETER(FVector4f, Team1)
		SHADER_PARAMETER(FVector4f, Team2)
		SHADER_PARAMETER(FVector4f, NightColor3)
		SHADER_PARAMETER(FVector4f, SpriteColor)
		SHADER_PARAMETER(float, CityTransparency)
		SHADER_PARAMETER(float, ControlledTimeY)
		SHADER_PARAMETER(uint32, bNight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FHallPS, "/Rok/Private/RokCityHallCameraMaterial.usf", "HallMaterialPS", SF_Pixel);

class FHallDiagnosticPS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FHallDiagnosticPS);
	SHADER_USE_PARAMETER_STRUCT(FHallDiagnosticPS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHallVS::FParameters, VS)
		SHADER_PARAMETER_TEXTURE(Texture2D, SpriteTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaskTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, MaskSampler)
		SHADER_PARAMETER(FVector4f, RendererTint)
		SHADER_PARAMETER(FVector4f, Team1)
		SHADER_PARAMETER(FVector4f, Team2)
		SHADER_PARAMETER(FVector4f, NightColor3)
		SHADER_PARAMETER(FVector4f, SpriteColor)
		SHADER_PARAMETER(float, CityTransparency)
		SHADER_PARAMETER(float, ControlledTimeY)
		SHADER_PARAMETER(uint32, bNight)
		SHADER_PARAMETER(uint32, DiagnosticMode)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FHallDiagnosticPS, "/Rok/Private/RokCityHallCameraMaterialDiagnostic.usf", "HallMaterialDiagnosticPS", SF_Pixel);


bool Valid(const FRokCityHallCameraMaterialProbeInput& In, FString& Error)
{
	if (GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5 || In.SourceKey.IsNone() ||
		!In.Positions.IsValid() || !In.UV0.IsValid() || !In.Indices.IsValid() ||
		!In.SpriteTexture.IsValid() || !In.MaskTexture.IsValid())
	{
		Error = TEXT("Hall material probe requires SM5, source identity and five GPU inputs"); return false;
	}
	const auto Raw = [](const FBufferRHIRef& Buffer, uint32 Size)
	{
		return Buffer->GetSize() == Size && EnumHasAllFlags(Buffer->GetUsage(),
			EBufferUsageFlags::ShaderResource | EBufferUsageFlags::ByteAddressBuffer);
	};
	if (!Raw(In.Positions, VertexCount * 12) || !Raw(In.UV0, VertexCount * 16) ||
		!Raw(In.Indices, IndexCount * 4))
	{
		Error = TEXT("Hall original 240-vertex/714-index byte layout differs"); return false;
	}
	const auto Texture = [](const FTextureRHIRef& Value, const FIntPoint Size, EPixelFormat Format)
	{
		const FRHITextureDesc& Desc = Value->GetDesc();
		return Desc.Extent == Size && Desc.Format == Format && Desc.NumMips == 1 &&
			Desc.NumSamples == 1 && !EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB);
	};
	if (!Texture(In.SpriteTexture, FIntPoint(1024, 888), In.bSourceCompressed ? PF_DXT5 : PF_B8G8R8A8) ||
		!Texture(In.MaskTexture, FIntPoint(512, 444), In.bSourceCompressed ? PF_DXT1 : PF_B8G8R8A8))
	{
		Error = TEXT("Hall source-decoded texture GPU descriptor differs"); return false;
	}
	for (int32 R = 0; R != 4; ++R)
		for (int32 C = 0; C != 4; ++C)
			if (!FMath::IsFinite(In.SourceLocalToClip.M[R][C]) ||
				(In.bObjectThenVP && (!FMath::IsFinite(In.SourceObjectToWorld.M[R][C]) ||
					!FMath::IsFinite(In.SourceViewProjection.M[R][C]))))
			{
				Error = TEXT("Hall source-camera matrix is nonfinite"); return false;
			}
	if (!FMath::IsFinite(In.CityTransparency) || !FMath::IsFinite(In.ControlledTimeY) ||
		In.CityTransparency != 1.0f || In.ControlledTimeY != 0.0f)
	{
		Error = TEXT("Hall controlled global transparency/time fixture differs"); return false;
	}
	return true;
}

void Draw(FRHICommandList& RHICmdList, const FHallPS::FParameters* Pass,
	TShaderMapRef<FHallVS> VS, TShaderMapRef<FHallPS> PS)
{
	FGraphicsPipelineStateInitializer State;
	RHICmdList.ApplyCachedRenderTargets(State);
	State.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha,
		BF_InverseSourceAlpha, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
	State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
	State.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
	State.BoundShaderState.PixelShaderRHI = PS.GetPixelShader();
	State.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, State, 0);
	RHICmdList.SetViewport(0, 0, 0, Width, Height, 1);
	SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), Pass->VS);
	SetShaderParameters(RHICmdList, PS, PS.GetPixelShader(), *Pass);
	RHICmdList.SetStreamSource(0, nullptr, 0);
	RHICmdList.DrawPrimitive(0, IndexCount / 3, 1);
}
}

bool FRokCityHallCameraMaterialProbe::Render(FRHICommandListImmediate& RHICmdList,
	const FRokCityHallCameraMaterialProbeInput& Input,
	TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError)
{
	check(IsInRenderingThread());
	OutTarget.SafeRelease(); OutError.Reset();
	if (!RokHallMaterialProbe::Valid(Input, OutError)) return false;
	FShaderResourceViewRHIRef P = RHICmdList.CreateShaderResourceView(Input.Positions,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef U = RHICmdList.CreateShaderResourceView(Input.UV0,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef I = RHICmdList.CreateShaderResourceView(Input.Indices,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	if (!P || !U || !I) { OutError = TEXT("Hall raw RHI SRV creation failed"); return false; }
	FRDGBuilder Graph(RHICmdList);
	const FLinearColor Background = Input.bWhiteBackground ? FLinearColor::White : FLinearColor::Black;
	FRDGTextureRef Target = Graph.CreateTexture(FRDGTextureDesc::Create2D(
		FIntPoint(RokHallMaterialProbe::Width, RokHallMaterialProbe::Height), PF_B8G8R8A8,
		FClearValueBinding(Background), TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("RokHallMaterialSourceCameraBGRA"));
	AddClearRenderTargetPass(Graph, Target, Background);
	auto* Pass = Graph.AllocParameters<RokHallMaterialProbe::FHallPS::FParameters>();
	Pass->VS.Positions = P.GetReference(); Pass->VS.UV0 = U.GetReference(); Pass->VS.Indices = I.GetReference();
	Pass->VS.SourceLocalToClip = Input.SourceLocalToClip;
	Pass->VS.SourceObjectToWorld = Input.SourceObjectToWorld;
	Pass->VS.SourceViewProjection = Input.SourceViewProjection;
	Pass->VS.bObjectThenVP = Input.bObjectThenVP ? 1u : 0u;
	Pass->SpriteTexture = Input.SpriteTexture.GetReference(); Pass->MaskTexture = Input.MaskTexture.GetReference();
	Pass->SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	Pass->MaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	const auto Vec = [](const FLinearColor& C) { return FVector4f(C.R, C.G, C.B, C.A); };
	Pass->RendererTint = Vec(Input.RendererTint);
	Pass->Team1 = Vec(Input.Team1); Pass->Team2 = Vec(Input.Team2);
	Pass->NightColor3 = Vec(Input.NightColor3); Pass->SpriteColor = Vec(Input.SpriteColor);
	Pass->CityTransparency = Input.CityTransparency; Pass->ControlledTimeY = Input.ControlledTimeY;
	Pass->bNight = Input.bNight ? 1u : 0u;
	Pass->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
	TShaderMapRef<RokHallMaterialProbe::FHallVS> VS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<RokHallMaterialProbe::FHallPS> PS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const FTextureRHIRef Main = Input.SpriteTexture, Mask = Input.MaskTexture;
	Graph.AddPass(RDG_EVENT_NAME("RokHallCameraMaterial.Isolated(%s)", *Input.SourceKey.ToString()),
		Pass, ERDGPassFlags::Raster,
		[Pass, VS, PS, P, U, I, Main, Mask](FRHICommandList& DrawCmd)
		{
			(void)P; (void)U; (void)I; (void)Main; (void)Mask;
			RokHallMaterialProbe::Draw(DrawCmd, Pass, VS, PS);
		});
	Graph.QueueTextureExtraction(Target, &OutTarget); Graph.Execute();
	if (!OutTarget.IsValid()) { OutError = TEXT("Hall material target extraction failed"); return false; }
	return true;
}

// Proposed isolated witness path. It intentionally does not mutate the legacy BGRA8 path.
bool FRokCityHallCameraMaterialProbe::RenderDiagnostic(FRHICommandListImmediate& RHICmdList,
	const FRokCityHallCameraMaterialProbeInput& Input, uint32 DiagnosticMode,
	TRefCountPtr<IPooledRenderTarget>& OutTarget, FString& OutError)
{
	check(IsInRenderingThread());
	OutTarget.SafeRelease(); OutError.Reset();
	if (DiagnosticMode < 1u || DiagnosticMode > 6u || !RokHallMaterialProbe::Valid(Input, OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Hall diagnostic mode must be in [1,6]");
		return false;
	}
	FShaderResourceViewRHIRef P = RHICmdList.CreateShaderResourceView(Input.Positions,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef U = RHICmdList.CreateShaderResourceView(Input.UV0,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	FShaderResourceViewRHIRef I = RHICmdList.CreateShaderResourceView(Input.Indices,
		FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw));
	if (!P || !U || !I) { OutError = TEXT("Hall diagnostic source SRV creation failed"); return false; }
	FRDGBuilder Graph(RHICmdList);
	const FLinearColor SentinelClear(0.07f, 0.15f, 0.71f, 1.0f);
	FRDGTextureRef Target = Graph.CreateTexture(FRDGTextureDesc::Create2D(
		FIntPoint(RokHallMaterialProbe::Width, RokHallMaterialProbe::Height), PF_A32B32G32R32F,
		FClearValueBinding(SentinelClear), TexCreate_RenderTargetable | TexCreate_ShaderResource),
		TEXT("RokHallMaterialDiagnosticRGBA32F"));
	AddClearRenderTargetPass(Graph, Target, SentinelClear);
	auto* Pass = Graph.AllocParameters<RokHallMaterialProbe::FHallDiagnosticPS::FParameters>();
	Pass->VS.Positions = P.GetReference(); Pass->VS.UV0 = U.GetReference(); Pass->VS.Indices = I.GetReference();
	Pass->VS.SourceLocalToClip = Input.SourceLocalToClip;
	Pass->VS.SourceObjectToWorld = Input.SourceObjectToWorld;
	Pass->VS.SourceViewProjection = Input.SourceViewProjection;
	Pass->VS.bObjectThenVP = Input.bObjectThenVP ? 1u : 0u;
	Pass->SpriteTexture = Input.SpriteTexture.GetReference(); Pass->MaskTexture = Input.MaskTexture.GetReference();
	Pass->SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	Pass->MaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
	const auto Vec = [](const FLinearColor& C) { return FVector4f(C.R, C.G, C.B, C.A); };
	Pass->RendererTint = Vec(Input.RendererTint); Pass->Team1 = Vec(Input.Team1);
	Pass->Team2 = Vec(Input.Team2); Pass->NightColor3 = Vec(Input.NightColor3);
	Pass->SpriteColor = Vec(Input.SpriteColor);
	Pass->CityTransparency = Input.CityTransparency;
	Pass->ControlledTimeY = Input.ControlledTimeY;
	Pass->bNight = Input.bNight ? 1u : 0u;
	Pass->DiagnosticMode = DiagnosticMode;
	Pass->RenderTargets[0] = FRenderTargetBinding(Target, ERenderTargetLoadAction::ELoad);
	TShaderMapRef<RokHallMaterialProbe::FHallVS> VS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<RokHallMaterialProbe::FHallDiagnosticPS> PS(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const FTextureRHIRef Main = Input.SpriteTexture, Mask = Input.MaskTexture;
	Graph.AddPass(RDG_EVENT_NAME("RokHallMaterial.SampleWitnessMode%u", DiagnosticMode), Pass,
		ERDGPassFlags::Raster, [Pass, VS, PS, P, U, I, Main, Mask](FRHICommandList& DrawCmd)
		{
			(void)P; (void)U; (void)I; (void)Main; (void)Mask;
			FGraphicsPipelineStateInitializer State;
			DrawCmd.ApplyCachedRenderTargets(State);
			State.BlendState = TStaticBlendState<>::GetRHI();
			State.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			State.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			State.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			State.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
			State.BoundShaderState.PixelShaderRHI = PS.GetPixelShader();
			State.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(DrawCmd, State, 0);
			DrawCmd.SetViewport(0, 0, 0, RokHallMaterialProbe::Width, RokHallMaterialProbe::Height, 1);
			SetShaderParameters(DrawCmd, VS, VS.GetVertexShader(), Pass->VS);
			SetShaderParameters(DrawCmd, PS, PS.GetPixelShader(), *Pass);
			DrawCmd.SetStreamSource(0, nullptr, 0);
			DrawCmd.DrawPrimitive(0, RokHallMaterialProbe::IndexCount / 3, 1);
		});
	Graph.QueueTextureExtraction(Target, &OutTarget); Graph.Execute();
	if (!OutTarget.IsValid()) { OutError = TEXT("Hall diagnostic float RT extraction failed"); return false; }
	return true;
}
