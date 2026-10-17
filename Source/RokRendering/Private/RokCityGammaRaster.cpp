#include "RokCityGammaRaster.h"

#include "CommonRenderResources.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/Engine.h"
#include "EngineModule.h"
#include "GlobalShader.h"
#include "Misc/SecureHash.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RendererInterface.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ShaderParameterStruct.h"

namespace RokCityGammaRaster
{
class FTransferCS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTransferCS);
	SHADER_USE_PARAMETER_STRUCT(FTransferCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
		SHADER_PARAMETER(FIntPoint, RectMin)
		SHADER_PARAMETER(FIntPoint, RectSize)
		SHADER_PARAMETER(uint32, bEncode)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FTransferCS, "/Rok/Private/RokCityGammaRaster.usf", "TransferCS", SF_Compute);

class FSpriteVS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpriteVS);
	SHADER_USE_PARAMETER_STRUCT(FSpriteVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, VertexData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IndexData)
		SHADER_PARAMETER(FMatrix44f, LocalToClip)
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSpriteVS, "/Rok/Private/RokCityGammaRaster.usf", "SpriteVS", SF_Vertex);

class FSpritePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpritePS);
	SHADER_USE_PARAMETER_STRUCT(FSpritePS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_TEXTURE(Texture2D, SpriteTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, MaskTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SpriteSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, MaskSampler)
		SHADER_PARAMETER(FVector4f, RendererTint)
		SHADER_PARAMETER(FVector4f, Team1)
		SHADER_PARAMETER(FVector4f, Team2)
		SHADER_PARAMETER(FVector4f, Lighting)
		SHADER_PARAMETER(float, Transparency)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSpriteVS::FParameters, VS)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSpritePS, "/Rok/Private/RokCityGammaRaster.usf", "SpritePS", SF_Pixel);

class FControlledFixturePS final : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FControlledFixturePS);
	SHADER_USE_PARAMETER_STRUCT(FControlledFixturePS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, MainTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, ControlledMaskTexture)
		SHADER_PARAMETER_TEXTURE(Texture2D, Tex0Texture)
		SHADER_PARAMETER_TEXTURE(Texture2D, Tex1Texture)
		SHADER_PARAMETER_TEXTURE(Texture2D, Tex2Texture)
		SHADER_PARAMETER_TEXTURE(Texture2D, Tex3Texture)
		SHADER_PARAMETER_TEXTURE(Texture2D, SplatTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, MainSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, ControlledMaskSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, Tex0Sampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, Tex1Sampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, Tex2Sampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, Tex3Sampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, SplatSampler)
		SHADER_PARAMETER(uint32, FamilyMode)
		SHADER_PARAMETER(FVector4f, ControlledRendererTint)
		SHADER_PARAMETER(FVector4f, GlobalSpriteColor)
		SHADER_PARAMETER(FVector4f, MaterialColor)
		SHADER_PARAMETER(FVector4f, ControlledTeam1)
		SHADER_PARAMETER(FVector4f, ControlledTeam2)
		SHADER_PARAMETER(FVector4f, NightColor3)
		SHADER_PARAMETER(FVector4f, Tex0ST)
		SHADER_PARAMETER(FVector4f, Tex1ST)
		SHADER_PARAMETER(FVector4f, Tex2ST)
		SHADER_PARAMETER(FVector4f, Tex3ST)
		SHADER_PARAMETER(float, ControlledTransparency)
		SHADER_PARAMETER(float, ControlledTimeY)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSpriteVS::FParameters, VS)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FControlledFixturePS, "/Rok/Private/RokCityGammaRaster.usf", "ControlledFixturePS", SF_Pixel);

template <typename TParameters>
TArray<FTextureRHIRef> FillControlledParameters(TParameters* Pass,
	const FRokCityGammaControlledInstance& Instance)
{
	FTextureRHIRef SlotTextures[7];
	FRHISamplerState* SlotSamplers[7];
	for (int32 Slot = 0; Slot < 7; ++Slot)
	{
		SlotTextures[Slot] = Instance.Textures[0].Texture; // Unused shader slots still bind a live resource.
		SlotSamplers[Slot] = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI();
	}
	TArray<FTextureRHIRef> Lifetimes;
	Lifetimes.Reserve(Instance.Textures.Num());
	for (const FRokCityGammaControlledTexture& Texture : Instance.Textures)
	{
		const int32 Slot = static_cast<int32>(Texture.Slot);
		SlotTextures[Slot] = Texture.Texture;
		SlotSamplers[Slot] = Texture.bRepeat
			? (Texture.bBilinear ? TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap>::GetRHI()
				: TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap>::GetRHI())
			: (Texture.bBilinear ? TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI()
				: TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp>::GetRHI());
		Lifetimes.Add(Texture.Texture);
	}
	Pass->MainTexture = SlotTextures[0].GetReference();
	Pass->ControlledMaskTexture = SlotTextures[1].GetReference();
	Pass->Tex0Texture = SlotTextures[2].GetReference();
	Pass->Tex1Texture = SlotTextures[3].GetReference();
	Pass->Tex2Texture = SlotTextures[4].GetReference();
	Pass->Tex3Texture = SlotTextures[5].GetReference();
	Pass->SplatTexture = SlotTextures[6].GetReference();
	Pass->MainSampler = SlotSamplers[0];
	Pass->ControlledMaskSampler = SlotSamplers[1];
	Pass->Tex0Sampler = SlotSamplers[2];
	Pass->Tex1Sampler = SlotSamplers[3];
	Pass->Tex2Sampler = SlotSamplers[4];
	Pass->Tex3Sampler = SlotSamplers[5];
	Pass->SplatSampler = SlotSamplers[6];
	Pass->FamilyMode = static_cast<uint32>(Instance.Family);
	const auto AsVector = [](const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	};
	Pass->ControlledRendererTint = AsVector(Instance.RendererTint);
	Pass->GlobalSpriteColor = AsVector(Instance.GlobalSpriteColor);
	Pass->MaterialColor = AsVector(Instance.MaterialColor);
	Pass->ControlledTeam1 = AsVector(Instance.Team1);
	Pass->ControlledTeam2 = AsVector(Instance.Team2);
	Pass->NightColor3 = AsVector(Instance.NightColor3);
	Pass->Tex0ST = Instance.TexST[0];
	Pass->Tex1ST = Instance.TexST[1];
	Pass->Tex2ST = Instance.TexST[2];
	Pass->Tex3ST = Instance.TexST[3];
	Pass->ControlledTransparency = Instance.Transparency;
	Pass->ControlledTimeY = Instance.ControlledTimeY;
	return Lifetimes;
}

template <typename TPixelShader>
void DrawControlledTriangles(FRHICommandList& RHICmdList, const FIntRect& Rect,
	int32 TriangleCount, bool bOverwrite, TShaderMapRef<FSpriteVS> VertexShader,
	TShaderMapRef<TPixelShader> PixelShader, const typename TPixelShader::FParameters* Pass)
{
	FGraphicsPipelineStateInitializer PSO;
	RHICmdList.ApplyCachedRenderTargets(PSO);
	PSO.BlendState = bOverwrite ? TStaticBlendState<CW_RGB>::GetRHI()
		: TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
	PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	PSO.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
	PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
	PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	PSO.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	PSO.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, PSO, 0);
	RHICmdList.SetViewport(Rect.Min.X, Rect.Min.Y, 0.0f, Rect.Max.X, Rect.Max.Y, 1.0f);
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Pass->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Pass);
	RHICmdList.SetStreamSource(0, nullptr, 0);
	RHICmdList.DrawPrimitive(0, TriangleCount, 1);
}

struct FTarget
{
	const FSceneInterface* Scene = nullptr; // Identity only; never dereferenced on RT.
	const FRenderTarget* RenderTarget = nullptr; // Identity only; owner held by GT through Reset.
	TArray<FRokCityGammaRasterInstance> Instances;
};

struct FViewMatch
{
	uint64 Token = 0;
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	TArray<FRokCityGammaRasterInstance> Instances;
	bool bSeen = false;
};

struct FState
{
	TMap<uint64, FTarget> Targets;
	TMap<const void*, FViewMatch> Views;
	FRokCityGammaRasterStats Stats;

	void BeginFamily() { check(IsInRenderingThread()); Views.Empty(); }
	void FinishFamily() { check(IsInRenderingThread()); Views.Empty(); }

	void AddView(const FSceneView& View)
	{
		check(IsInRenderingThread());
		if (!View.Family || !View.Family->RenderTarget || !View.Family->Scene) return;
		for (const TPair<uint64, FTarget>& Entry : Targets)
		{
			if (Entry.Value.Scene != View.Family->Scene ||
				Entry.Value.RenderTarget != View.Family->RenderTarget) continue;
			if (View.bIsSceneCapture || View.Family->Views.Num() != 1 || View.PlayerIndex != 0 ||
				IsMobilePlatform(View.GetShaderPlatform()))
			{
				++Stats.UnsupportedViews;
				Stats.LastReject = ERokCityGammaRasterReject::UnsupportedView;
				return;
			}
			if (Views.Contains(&View))
			{
				++Stats.DuplicateViews;
				Stats.LastReject = ERokCityGammaRasterReject::DuplicateView;
				return;
			}
			FViewMatch& Match = Views.Add(&View);
			Match.Token = Entry.Key;
			Match.FeatureLevel = View.GetFeatureLevel();
			Match.Instances = Entry.Value.Instances; // Immutable for this view family.
			++Stats.RegisteredViews;
			return;
		}
	}

	void Draw(FPostOpaqueRenderParameters& Parameters)
	{
		check(IsInRenderingThread());
		FViewMatch* Match = Views.Find(Parameters.Uid);
		if (!Match || Match->Instances.IsEmpty()) return;
		if (Match->bSeen)
		{
			++Stats.DuplicateViews;
			Stats.LastReject = ERokCityGammaRasterReject::DuplicateView;
			return;
		}
		Match->bSeen = true;
		if (!Parameters.ViewUniformBuffer)
		{
			++Stats.MissingUniformBuffers;
			Stats.LastReject = ERokCityGammaRasterReject::MissingUniformBuffer;
			return;
		}
		FRDGBuilder* GraphBuilder = Parameters.GraphBuilder;
		FRDGTextureRef Color = Parameters.ColorTexture;
		FRDGTextureRef Depth = Parameters.DepthTexture;
		const FIntRect Rect = Parameters.ViewportRect;
		if (!GraphBuilder || !Color || !Depth || Match->FeatureLevel < ERHIFeatureLevel::SM5 ||
			Color->Desc.Dimension != ETextureDimension::Texture2D ||
			Color->Desc.Format != PF_FloatRGBA || Color->Desc.NumSamples != 1 ||
			Depth->Desc.NumSamples != 1 ||
			!EnumHasAnyFlags(Color->Desc.Flags, TexCreate_ShaderResource) ||
			!EnumHasAnyFlags(Depth->Desc.Flags, TexCreate_DepthStencilTargetable) ||
			Rect.Min.X < 0 || Rect.Min.Y < 0 || Rect.Width() <= 0 || Rect.Height() <= 0 ||
			Rect.Max.X > Color->Desc.Extent.X || Rect.Max.Y > Color->Desc.Extent.Y ||
			Rect.Max.X > Depth->Desc.Extent.X || Rect.Max.Y > Depth->Desc.Extent.Y)
		{
			++Stats.UnsupportedTargets;
			Stats.LastReject = ERokCityGammaRasterReject::UnsupportedTarget;
			return;
		}

		const TUniformBufferRef<FViewUniformShaderParameters> ViewBuffer(Parameters.ViewUniformBuffer);
		FRDGTextureRef Encoded = GraphBuilder->CreateTexture(
			FRDGTextureDesc::Create2D(Color->Desc.Extent, PF_FloatRGBA,
				FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable),
			TEXT("RokCityGammaRaster.Encoded"));
		FTransferCS::FParameters* Encode = GraphBuilder->AllocParameters<FTransferCS::FParameters>();
		Encode->View = ViewBuffer;
		Encode->InputTexture = Color;
		Encode->OutputTexture = GraphBuilder->CreateUAV(Encoded);
		Encode->RectMin = Rect.Min;
		Encode->RectSize = Rect.Size();
		Encode->bEncode = 1;
		TShaderMapRef<FTransferCS> TransferShader(GetGlobalShaderMap(Match->FeatureLevel));
		FComputeShaderUtils::AddPass(*GraphBuilder, RDG_EVENT_NAME("RokCityGammaRaster.OETF"),
			TransferShader, Encode, FComputeShaderUtils::GetGroupCount(Rect.Size(), 8));

		TShaderMapRef<FSpriteVS> VertexShader(GetGlobalShaderMap(Match->FeatureLevel));
		TShaderMapRef<FSpritePS> PixelShader(GetGlobalShaderMap(Match->FeatureLevel));
		for (const FRokCityGammaRasterInstance& Instance : Match->Instances)
		{
			TArray<FVector4f> VertexData;
			VertexData.Reserve(Instance.Vertices.Num() * 2);
			for (const FRokCityGammaRasterVertex& Vertex : Instance.Vertices)
			{
				VertexData.Add(FVector4f(Vertex.LocalCm.X, Vertex.LocalCm.Y, Vertex.LocalCm.Z, Vertex.UV.X));
				VertexData.Add(FVector4f(Vertex.UV.Y, 0, 0, 0));
			}
			FRDGBufferRef Vertices = CreateStructuredBuffer(*GraphBuilder, TEXT("RokCityGammaRaster.Vertices"), VertexData);
			FRDGBufferRef Indices = CreateStructuredBuffer(*GraphBuilder, TEXT("RokCityGammaRaster.Indices"), Instance.Indices);
			FSpritePS::FParameters* Pass = GraphBuilder->AllocParameters<FSpritePS::FParameters>();
			Pass->VS.VertexData = GraphBuilder->CreateSRV(Vertices);
			Pass->VS.IndexData = GraphBuilder->CreateSRV(Indices);
			Pass->VS.LocalToClip = FMatrix44f(Instance.LocalToWorld * Parameters.ViewMatrix * Parameters.ProjMatrix);
			Pass->View = ViewBuffer;
			Pass->SpriteTexture = Instance.SpriteTexture.GetReference();
			Pass->MaskTexture = Instance.MaskTexture.GetReference();
			Pass->SpriteSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
			Pass->MaskSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();
			Pass->RendererTint = FVector4f(Instance.RendererTint.R, Instance.RendererTint.G,
				Instance.RendererTint.B, Instance.RendererTint.A);
			Pass->Team1 = FVector4f(Instance.Team1.R, Instance.Team1.G, Instance.Team1.B, Instance.Team1.A);
			Pass->Team2 = FVector4f(Instance.Team2.R, Instance.Team2.G, Instance.Team2.B, Instance.Team2.A);
			Pass->Lighting = FVector4f(Instance.Lighting.R, Instance.Lighting.G,
				Instance.Lighting.B, Instance.Lighting.A);
			Pass->Transparency = Instance.Transparency;
			Pass->RenderTargets[0] = FRenderTargetBinding(Encoded, ERenderTargetLoadAction::ELoad);
			Pass->RenderTargets.DepthStencil = FDepthStencilBinding(Depth, ERenderTargetLoadAction::ELoad,
				ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);
			const int32 TriangleCount = Instance.Indices.Num() / 3;
			const FTextureRHIRef SpriteLifetime = Instance.SpriteTexture;
			const FTextureRHIRef MaskLifetime = Instance.MaskTexture;
			GraphBuilder->AddPass(RDG_EVENT_NAME("RokCityGammaRaster.Sprite(%s)", *Instance.ModelId.ToString()),
				Pass, ERDGPassFlags::Raster,
				[Pass, VertexShader, PixelShader, Rect, TriangleCount, SpriteLifetime, MaskLifetime](FRHICommandList& RHICmdList)
				{
					(void)SpriteLifetime;
					(void)MaskLifetime; // Retain both RHI textures through RDG pass execution.
					FGraphicsPipelineStateInitializer PSO;
					RHICmdList.ApplyCachedRenderTargets(PSO);
					PSO.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI();
					PSO.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					PSO.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
					PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					PSO.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					PSO.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					PSO.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, PSO, 0);
					RHICmdList.SetViewport(Rect.Min.X, Rect.Min.Y, 0.0f, Rect.Max.X, Rect.Max.Y, 1.0f);
					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Pass->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Pass);
					RHICmdList.SetStreamSource(0, nullptr, 0);
					RHICmdList.DrawPrimitive(0, TriangleCount, 1);
				});
			++Stats.DrawnInstances;
		}
		FRDGTextureRef Decoded = GraphBuilder->CreateTexture(
			FRDGTextureDesc::Create2D(Color->Desc.Extent, PF_FloatRGBA,
				FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV),
			TEXT("RokCityGammaRaster.Decoded"));
		FTransferCS::FParameters* Decode = GraphBuilder->AllocParameters<FTransferCS::FParameters>();
		Decode->View = ViewBuffer;
		Decode->InputTexture = Encoded;
		Decode->OutputTexture = GraphBuilder->CreateUAV(Decoded);
		Decode->RectMin = Rect.Min;
		Decode->RectSize = Rect.Size();
		Decode->bEncode = 0;
		FComputeShaderUtils::AddPass(*GraphBuilder, RDG_EVENT_NAME("RokCityGammaRaster.EOTF"),
			TransferShader, Decode, FComputeShaderUtils::GetGroupCount(Rect.Size(), 8));
		FRHICopyTextureInfo Copy;
		Copy.SourcePosition = FIntVector(Rect.Min.X, Rect.Min.Y, 0);
		Copy.DestPosition = Copy.SourcePosition;
		Copy.Size = FIntVector(Rect.Width(), Rect.Height(), 1);
		AddCopyTexturePass(*GraphBuilder, Decoded, Color, Copy);
		++Stats.ScheduledFrames;
	}
};

class FViewExtension final : public FSceneViewExtensionBase
{
public:
	FViewExtension(const FAutoRegister& AutoRegister, FState* InState)
		: FSceneViewExtensionBase(AutoRegister), State(InState) {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder&, FSceneViewFamily&) override { State->BeginFamily(); }
	virtual void PreRenderView_RenderThread(FRDGBuilder&, FSceneView& View) override { State->AddView(View); }
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder&, FSceneViewFamily&) override { State->FinishFamily(); }
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext&) const override { return true; }
private:
	FState* State;
};

TUniquePtr<FState> State;
TSharedPtr<FViewExtension, ESPMode::ThreadSafe> Extension;
FDelegateHandle PassHandle;
uint64 NextToken = 1;

void RecordReject(ERokCityGammaRasterReject Reason)
{
	check(IsInGameThread());
	if (!State.IsValid()) return;
	FState* StatePtr = State.Get();
	ENQUEUE_RENDER_COMMAND(RokRejectCityGammaRaster)([StatePtr, Reason](FRHICommandListImmediate&)
	{
		StatePtr->Stats.LastReject = Reason;
		if (Reason == ERokCityGammaRasterReject::InvalidSnapshot ||
			Reason == ERokCityGammaRasterReject::SourceGeometryMismatch)
		{
			++StatePtr->Stats.RejectedSnapshots;
			if (Reason == ERokCityGammaRasterReject::SourceGeometryMismatch)
			{
				++StatePtr->Stats.SourceGeometryMismatches;
			}
		}
	});
	FlushRenderingCommands();
}

void ClearSnapshot(uint64 Token)
{
	check(IsInGameThread());
	if (!Token || !State.IsValid()) return;
	FState* StatePtr = State.Get();
	ENQUEUE_RENDER_COMMAND(RokClearCityGammaRasterSnapshot)([StatePtr, Token](FRHICommandListImmediate&)
	{
		if (FTarget* Target = StatePtr->Targets.Find(Token))
		{
			Target->Instances.Empty();
			StatePtr->Stats.ActiveInstances = 0;
			for (const TPair<uint64, FTarget>& Entry : StatePtr->Targets)
			{
				StatePtr->Stats.ActiveInstances += Entry.Value.Instances.Num();
			}
		}
	});
	FlushRenderingCommands();
}

bool Validate(const TArray<FRokCityGammaRasterInstance>& Instances, FString& OutError,
	ERokCityGammaRasterReject& OutReason)
{
	OutReason = ERokCityGammaRasterReject::InvalidSnapshot;
	TSet<uint64> Ids;
	for (const FRokCityGammaRasterInstance& Instance : Instances)
	{
		int32 Vertices = 0, Triangles = 0;
		const TCHAR* ApprovedSha1 = nullptr;
		if (Instance.ModelId == TEXT("build_13_101_2"))
		{ Vertices = 49; Triangles = 47; ApprovedSha1 = TEXT("88e2ddab5e5d21a5c69b536e51edafbae3b3ad70"); }
		else if (Instance.ModelId == TEXT("build_2_101_1"))
		{ Vertices = 133; Triangles = 163; ApprovedSha1 = TEXT("16d149aac2384232657add120b70e481842783ca"); }
		else if (Instance.ModelId == TEXT("build_3_101_1"))
		{ Vertices = 43; Triangles = 41; ApprovedSha1 = TEXT("ff631607fec2ed47a5447fe73168a6514061e91e"); }
		if (Vertices == 0 || Instance.InstanceId == 0 || Ids.Contains(Instance.InstanceId) ||
			Instance.Vertices.Num() != Vertices || Instance.Indices.Num() != Triangles * 3 ||
			!Instance.SpriteTexture.IsValid() || !Instance.MaskTexture.IsValid() ||
			!Instance.SpriteTexture->GetTexture2D() || !Instance.MaskTexture->GetTexture2D() ||
			!FMath::IsFinite(Instance.Transparency) || Instance.Transparency < 0 || Instance.Transparency > 1)
		{
			OutError = TEXT("Source Gamma instance identity, topology, textures or alpha differs");
			return false;
		}
		const FIntPoint SpriteSize = Instance.SpriteTexture->GetDesc().Extent;
		const FIntPoint MaskSize = Instance.MaskTexture->GetDesc().Extent;
		const FIntPoint ApprovedSpriteSize = Instance.ModelId == TEXT("build_13_101_2")
			? FIntPoint(512, 426) : Instance.ModelId == TEXT("build_2_101_1")
				? FIntPoint(512, 512) : FIntPoint(512, 459);
		const FIntPoint ApprovedMaskSize = Instance.ModelId == TEXT("build_13_101_2")
			? FIntPoint(256, 213) : Instance.ModelId == TEXT("build_2_101_1")
				? FIntPoint(256, 256) : FIntPoint(256, 230);
		const FRHITextureDesc& SpriteDesc = Instance.SpriteTexture->GetDesc();
		const FRHITextureDesc& MaskDesc = Instance.MaskTexture->GetDesc();
		if (SpriteSize != ApprovedSpriteSize || MaskSize != ApprovedMaskSize ||
			SpriteDesc.Format != PF_B8G8R8A8 || MaskDesc.Format != PF_B8G8R8A8 ||
			SpriteDesc.NumMips != 1 || MaskDesc.NumMips != 1 ||
			SpriteDesc.NumSamples != 1 || MaskDesc.NumSamples != 1 ||
			EnumHasAnyFlags(SpriteDesc.Flags, TexCreate_SRGB) ||
			EnumHasAnyFlags(MaskDesc.Flags, TexCreate_SRGB))
		{
			OutError = FString::Printf(TEXT("Source Gamma %s texture RHI dimensions, B8G8R8A8 format, mips, samples or sRGB flag differ"),
				*Instance.ModelId.ToString());
			return false;
		}
		Ids.Add(Instance.InstanceId);
		for (const FRokCityGammaRasterVertex& Vertex : Instance.Vertices)
		{
			if (!FMath::IsFinite(Vertex.LocalCm.X) || !FMath::IsFinite(Vertex.LocalCm.Y) ||
				!FMath::IsFinite(Vertex.LocalCm.Z) || !FMath::IsFinite(Vertex.UV.X) ||
				!FMath::IsFinite(Vertex.UV.Y))
			{
				OutError = TEXT("Source Gamma vertex or UV is nonfinite");
				return false;
			}
		}
		for (uint32 Index : Instance.Indices)
		{
			if (Index >= static_cast<uint32>(Vertices))
			{
				OutError = TEXT("Source Gamma triangle index exceeds approved mesh");
				return false;
			}
		}
		// Canonical little-endian float32 (x,y,z,u,v) and uint32 triangle indices
		// from the sealed three-model Runtime3 manifest. Counts alone are insufficient.
		TArray<uint8> GeometryBytes;
		GeometryBytes.Reserve(Vertices * 20 + Triangles * 12);
		const auto AppendFloat = [&GeometryBytes](float Value)
		{
			GeometryBytes.Append(reinterpret_cast<const uint8*>(&Value), sizeof(Value));
		};
		for (const FRokCityGammaRasterVertex& Vertex : Instance.Vertices)
		{
			AppendFloat(Vertex.LocalCm.X); AppendFloat(Vertex.LocalCm.Y); AppendFloat(Vertex.LocalCm.Z);
			AppendFloat(Vertex.UV.X); AppendFloat(Vertex.UV.Y);
		}
		GeometryBytes.Append(reinterpret_cast<const uint8*>(Instance.Indices.GetData()),
			Instance.Indices.Num() * sizeof(uint32));
		uint8 Digest[FSHA1::DigestSize];
		FSHA1::HashBuffer(GeometryBytes.GetData(), GeometryBytes.Num(), Digest);
		if (!FString(BytesToHex(Digest, FSHA1::DigestSize)).Equals(ApprovedSha1, ESearchCase::IgnoreCase))
		{
			OutReason = ERokCityGammaRasterReject::SourceGeometryMismatch;
			OutError = FString::Printf(TEXT("Source Gamma %s vertex/UV/index digest differs"), *Instance.ModelId.ToString());
			return false;
		}
		const auto IsFiniteColor = [](const FLinearColor& Color)
		{
			return FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) &&
				FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A);
		};
		if (!IsFiniteColor(Instance.RendererTint) || !IsFiniteColor(Instance.Team1) ||
			!IsFiniteColor(Instance.Team2) || !IsFiniteColor(Instance.Lighting))
		{
			OutError = TEXT("Source Gamma color parameter is nonfinite");
			return false;
		}
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (!FMath::IsFinite(Instance.LocalToWorld.M[Row][Column]))
				{
					OutError = TEXT("Source Gamma transform is nonfinite");
					return false;
				}
			}
		}
	}
	return true;
}

bool ValidateControlled(const TArray<FRokCityGammaControlledInstance>& Instances, FString& OutError,
	ERokCityGammaRasterReject& OutReason)
{
	OutReason = ERokCityGammaRasterReject::InvalidSnapshot;
	if (Instances.IsEmpty() || Instances.Num() > 5)
	{
		OutError = TEXT("Controlled family fixture requires one to five pinned rows");
		return false;
	}
	TSet<uint64> Ids;
	TSet<FName> Keys;
	for (const FRokCityGammaControlledInstance& Instance : Instances)
	{
		const TCHAR* Key = nullptr;
		const TCHAR* Material = nullptr;
		const TCHAR* Renderer = nullptr;
		const TCHAR* Mesh = nullptr;
		int32 Queue = 0;
		const ERokCityGammaControlledSlot* ExpectedSlots = nullptr;
		int32 ExpectedSlotCount = 0;
		static constexpr ERokCityGammaControlledSlot MaskSlots[] =
			{ ERokCityGammaControlledSlot::Main, ERokCityGammaControlledSlot::Mask };
		static constexpr ERokCityGammaControlledSlot GroundSlots[] =
			{ ERokCityGammaControlledSlot::Tex0, ERokCityGammaControlledSlot::Tex1,
			  ERokCityGammaControlledSlot::Tex2, ERokCityGammaControlledSlot::Tex3,
			  ERokCityGammaControlledSlot::Splat };
		static constexpr ERokCityGammaControlledSlot MainSlot[] = { ERokCityGammaControlledSlot::Main };
		switch (Instance.Family)
		{
		case ERokCityGammaControlledFamily::NightMaskedRGB:
			Key = TEXT("initial:5"); Material = TEXT("4e9db9e5983dd8e46bcbc881f14fe50c");
			Renderer = TEXT("212751466141171401"); Mesh = TEXT("21300000"); Queue = 2996;
			ExpectedSlots = MaskSlots; ExpectedSlotCount = UE_ARRAY_COUNT(MaskSlots); break;
		case ERokCityGammaControlledFamily::GroundOpaqueSplat:
			Key = TEXT("ground:ground_1"); Material = TEXT("af7338d551b4740658b15697721fb87b");
			Renderer = TEXT("1638635697907350552"); Mesh = TEXT("-4480367285411028635"); Queue = 2992;
			ExpectedSlots = GroundSlots; ExpectedSlotCount = UE_ARRAY_COUNT(GroundSlots); break;
		case ERokCityGammaControlledFamily::GroundAlphaSplat:
			Key = TEXT("ground:ground_2"); Material = TEXT("0ee9eb958a229684d8fdf9d3da44d308");
			Renderer = TEXT("9199177364033884911"); Mesh = TEXT("-3292925553009838691"); Queue = 2991;
			ExpectedSlots = GroundSlots; ExpectedSlotCount = UE_ARRAY_COUNT(GroundSlots); break;
		case ERokCityGammaControlledFamily::RoadDefaultWhiteMask:
			Key = TEXT("support:boundary_support_road"); Material = TEXT("cdaecaffccd495d44842ac8984697f2f");
			Renderer = TEXT("90046407004177856"); Mesh = TEXT("10209"); Queue = 2992;
			ExpectedSlots = MainSlot; ExpectedSlotCount = UE_ARRAY_COUNT(MainSlot); break;
		case ERokCityGammaControlledFamily::WallShadowTransparent:
			Key = TEXT("support:boundary_support_wall_shadow"); Material = TEXT("59fd34ad3fc10624991a82306c6fa02f");
			Renderer = TEXT("5179472259213045137"); Mesh = TEXT("10209"); Queue = 2993;
			ExpectedSlots = MainSlot; ExpectedSlotCount = UE_ARRAY_COUNT(MainSlot); break;
		default: break;
		}
		if (!Key || !Instance.SourceKey.ToString().Equals(Key, ESearchCase::CaseSensitive) ||
			Instance.MaterialGuid != Material || Instance.RendererFileId != Renderer ||
			Instance.MeshFileId != Mesh || Instance.SortPriority != Queue ||
			Instance.InstanceId == 0 || Ids.Contains(Instance.InstanceId) || Keys.Contains(Instance.SourceKey) ||
			Instance.Textures.Num() != ExpectedSlotCount || Instance.Vertices.Num() != 4 ||
			Instance.Indices.Num() != 6)
		{
			OutError = TEXT("Controlled family identity, source slot count or quad topology differs");
			return false;
		}
		Ids.Add(Instance.InstanceId);
		Keys.Add(Instance.SourceKey);
		TArray<uint8> GeometryBytes;
		GeometryBytes.Reserve(4 * 24 + 6 * 4);
		for (const FRokCityGammaControlledVertex& Vertex : Instance.Vertices)
		{
			if (!FMath::IsFinite(Vertex.LocalCm.X) || !FMath::IsFinite(Vertex.LocalCm.Y) ||
				!FMath::IsFinite(Vertex.LocalCm.Z) || !FMath::IsFinite(Vertex.UV.X) ||
				!FMath::IsFinite(Vertex.UV.Y) || !FMath::IsFinite(Vertex.Alpha))
			{
				OutError = TEXT("Controlled quad has nonfinite vertex or alpha"); return false;
			}
			const float Values[] = { Vertex.LocalCm.X, Vertex.LocalCm.Y, Vertex.LocalCm.Z,
				Vertex.UV.X, Vertex.UV.Y, Vertex.Alpha };
			GeometryBytes.Append(reinterpret_cast<const uint8*>(Values), sizeof(Values));
		}
		for (uint32 Index : Instance.Indices)
		{
			if (Index >= 4) { OutError = TEXT("Controlled quad index out of range"); return false; }
		}
		GeometryBytes.Append(reinterpret_cast<const uint8*>(Instance.Indices.GetData()), 6 * sizeof(uint32));
		uint8 Digest[FSHA1::DigestSize];
		FSHA1::HashBuffer(GeometryBytes.GetData(), GeometryBytes.Num(), Digest);
		const TCHAR* ExpectedGeometry = Instance.Family == ERokCityGammaControlledFamily::GroundAlphaSplat
			? TEXT("ebe2830e8a8e0db42ca9a829003afdb11917dcef")
			: TEXT("1b802936fcf8725b5bbfc2bc1f8b1ef17e78f7b3");
		if (!FString(BytesToHex(Digest, FSHA1::DigestSize)).Equals(ExpectedGeometry, ESearchCase::IgnoreCase))
		{
			OutReason = ERokCityGammaRasterReject::SourceGeometryMismatch;
			OutError = TEXT("Controlled diagnostic quad digest differs; no source mesh is qualified by this gate");
			return false;
		}
		for (int32 Slot = 0; Slot < ExpectedSlotCount; ++Slot)
		{
			const FRokCityGammaControlledTexture& Texture = Instance.Textures[Slot];
			const bool bMask = Texture.Slot == ERokCityGammaControlledSlot::Mask;
			const bool bSplat = Texture.Slot == ERokCityGammaControlledSlot::Splat;
			const TCHAR* ExpectedSha = bSplat ? TEXT("d37a4c2900bb85dfd24592c0b52e8dcd72e0ea45c47fa2db0cf1c4c132c80c66")
				: bMask ? TEXT("1e9d4133111b4fede2d49c35c4106dc9bb9f68dc79913dc7c8a0cc1ba7f1ea37")
				: TEXT("2e287a5c6d94f60c01aa516692106823f30d8a48195afcc5fd3cb25e8e781dea");
			const bool bNightBZero = bMask && Texture.FixtureBgraSha256.Equals(
				TEXT("d37a4c2900bb85dfd24592c0b52e8dcd72e0ea45c47fa2db0cf1c4c132c80c66"), ESearchCase::IgnoreCase);
			if (Texture.Slot != ExpectedSlots[Slot] || !Texture.Texture.IsValid() ||
				!Texture.Texture->GetTexture2D() || Texture.Extent != FIntPoint(2, 2) ||
				Texture.NumMips != 1 || Texture.bRepeat || Texture.bBilinear ||
				(!bNightBZero && !Texture.FixtureBgraSha256.Equals(ExpectedSha, ESearchCase::IgnoreCase)))
			{
				OutError = TEXT("Controlled fixture slot or declared constant bytes differ"); return false;
			}
			const FRHITextureDesc& Desc = Texture.Texture->GetDesc();
			if (Desc.Dimension != ETextureDimension::Texture2D || Desc.Extent != Texture.Extent ||
				Desc.Format != PF_B8G8R8A8 || Desc.NumMips != Texture.NumMips || Desc.NumSamples != 1 ||
				EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB))
			{
				OutError = TEXT("Controlled fixture RHI format/dimensions/mips/sRGB differ"); return false;
			}
		}
		const auto FiniteColor = [](const FLinearColor& Color)
		{
			return FMath::IsFinite(Color.R) && FMath::IsFinite(Color.G) &&
				FMath::IsFinite(Color.B) && FMath::IsFinite(Color.A);
		};
		if (!FiniteColor(Instance.RendererTint) || !FiniteColor(Instance.GlobalSpriteColor) ||
			!FiniteColor(Instance.MaterialColor) || !FiniteColor(Instance.Team1) ||
			!FiniteColor(Instance.Team2) || !FiniteColor(Instance.NightColor3) ||
			!FMath::IsFinite(Instance.Transparency) || Instance.Transparency < 0 || Instance.Transparency > 1 ||
			!FMath::IsFinite(Instance.ControlledTimeY) ||
			(Instance.Family == ERokCityGammaControlledFamily::NightMaskedRGB && !Instance.bControlledTimeQualified))
		{
			OutError = TEXT("Controlled family parameters or explicit night time qualification missing"); return false;
		}
		for (const FVector4f& ST : Instance.TexST)
		{
			if (!FMath::IsFinite(ST.X) || !FMath::IsFinite(ST.Y) ||
				!FMath::IsFinite(ST.Z) || !FMath::IsFinite(ST.W))
			{
				OutError = TEXT("Controlled source UV transform is nonfinite"); return false;
			}
		}
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (!FMath::IsFinite(Instance.LocalToWorld.M[Row][Column]))
				{
					OutError = TEXT("Controlled source transform is nonfinite"); return false;
				}
			}
		}
	}
	return true;
}
}

FRokCityGammaRasterRegistration::~FRokCityGammaRasterRegistration() { Reset(); }
FRokCityGammaRasterRegistration::FRokCityGammaRasterRegistration(FRokCityGammaRasterRegistration&& Other) noexcept
	: Token(Other.Token) { Other.Token = 0; }
FRokCityGammaRasterRegistration& FRokCityGammaRasterRegistration::operator=(FRokCityGammaRasterRegistration&& Other) noexcept
{
	if (this != &Other) { Reset(); Token = Other.Token; Other.Token = 0; }
	return *this;
}
bool FRokCityGammaRasterRegistration::Register(const FSceneInterface* Scene, const FRenderTarget* Target, FString& OutError)
{
	check(IsInGameThread());
	if (Token) { OutError = TEXT("Gamma raster registration already active"); return false; }
	return FRokCityGammaRaster::RegisterTarget(Scene, Target, Token, OutError);
}
bool FRokCityGammaRasterRegistration::UpdateSnapshot(const TArray<FRokCityGammaRasterInstance>& Instances, FString& OutError)
{
	check(IsInGameThread());
	return FRokCityGammaRaster::UpdateTarget(Token, Instances, OutError);
}
void FRokCityGammaRasterRegistration::Reset()
{
	check(IsInGameThread());
	if (Token) { FRokCityGammaRaster::UnregisterTarget(Token); Token = 0; }
}

bool FRokCityGammaRaster::IsAvailable()
{
	return IsInGameThread() && RokCityGammaRaster::State.IsValid() &&
		RokCityGammaRaster::Extension.IsValid() && RokCityGammaRaster::PassHandle.IsValid();
}
const TCHAR* FRokCityGammaRaster::RejectName(ERokCityGammaRasterReject Reason)
{
	switch (Reason)
	{
	case ERokCityGammaRasterReject::None: return TEXT("none");
	case ERokCityGammaRasterReject::Unavailable: return TEXT("unavailable");
	case ERokCityGammaRasterReject::NullIdentity: return TEXT("null_identity");
	case ERokCityGammaRasterReject::DuplicateTarget: return TEXT("duplicate_target");
	case ERokCityGammaRasterReject::InvalidSnapshot: return TEXT("invalid_snapshot");
	case ERokCityGammaRasterReject::SourceGeometryMismatch: return TEXT("source_geometry_mismatch");
	case ERokCityGammaRasterReject::UnsupportedView: return TEXT("unsupported_view");
	case ERokCityGammaRasterReject::UnsupportedTarget: return TEXT("unsupported_target");
	case ERokCityGammaRasterReject::MissingUniformBuffer: return TEXT("missing_uniform_buffer");
	case ERokCityGammaRasterReject::DuplicateView: return TEXT("duplicate_view");
	}
	return TEXT("unknown");
}
bool FRokCityGammaRaster::ValidateControlledFixture(
	const TArray<FRokCityGammaControlledInstance>& Instances, FString& OutError)
{
	check(IsInGameThread());
	ERokCityGammaRasterReject Reason = ERokCityGammaRasterReject::None;
	return RokCityGammaRaster::ValidateControlled(Instances, OutError, Reason);
}
bool FRokCityGammaRaster::AddControlledFixturePass(FRDGBuilder& GraphBuilder,
	FRDGTexture* Color, FRDGTexture* Depth, const FIntRect& Rect,
	const FMatrix44f& LocalToClip, const FRokCityGammaControlledInstance& Instance,
	FString& OutError)
{
	check(IsInRenderingThread());
	// No registered Scene/Viewport matching occurs here; caller owns isolated fixture targets.
	if (!Color || !Depth || GMaxRHIFeatureLevel < ERHIFeatureLevel::SM5 ||
		Rect.Min != FIntPoint::ZeroValue || Rect.Max != FIntPoint(128, 128) ||
		Color->Desc.Dimension != ETextureDimension::Texture2D ||
		Color->Desc.Extent != FIntPoint(128, 128) || Color->Desc.Format != PF_B8G8R8A8 ||
		Color->Desc.NumSamples != 1 || EnumHasAnyFlags(Color->Desc.Flags, TexCreate_SRGB) ||
		!EnumHasAnyFlags(Color->Desc.Flags, TexCreate_RenderTargetable) ||
		Depth->Desc.Dimension != ETextureDimension::Texture2D ||
		Depth->Desc.Extent != FIntPoint(128, 128) || Depth->Desc.NumSamples != 1 ||
		!EnumHasAnyFlags(Depth->Desc.Flags, TexCreate_DepthStencilTargetable))
	{
		OutError = TEXT("Controlled family fixture requires isolated 128x128 Gamma BGRA8 RT and read-only depth");
		return false;
	}
	for (int32 Row = 0; Row < 4; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			if (!FMath::IsFinite(LocalToClip.M[Row][Column]))
			{
				OutError = TEXT("Controlled fixture local-to-clip is nonfinite"); return false;
			}
		}
	}
	ERokCityGammaRasterReject Reason = ERokCityGammaRasterReject::None;
	TArray<FRokCityGammaControlledInstance> One;
	One.Add(Instance);
	if (!RokCityGammaRaster::ValidateControlled(One, OutError, Reason)) return false;
	TArray<FVector4f> VertexData;
	VertexData.Reserve(Instance.Vertices.Num() * 2);
	for (const FRokCityGammaControlledVertex& Vertex : Instance.Vertices)
	{
		VertexData.Add(FVector4f(Vertex.LocalCm.X, Vertex.LocalCm.Y, Vertex.LocalCm.Z, Vertex.UV.X));
		VertexData.Add(FVector4f(Vertex.UV.Y, Vertex.Alpha, 0, 0));
	}
	FRDGBufferRef Vertices = CreateStructuredBuffer(GraphBuilder, TEXT("RokCityGammaRaster.FixtureVertices"), VertexData);
	FRDGBufferRef Indices = CreateStructuredBuffer(GraphBuilder, TEXT("RokCityGammaRaster.FixtureIndices"), Instance.Indices);
	RokCityGammaRaster::FControlledFixturePS::FParameters* Pass =
		GraphBuilder.AllocParameters<RokCityGammaRaster::FControlledFixturePS::FParameters>();
	Pass->VS.VertexData = GraphBuilder.CreateSRV(Vertices);
	Pass->VS.IndexData = GraphBuilder.CreateSRV(Indices);
	Pass->VS.LocalToClip = LocalToClip;
	TArray<FTextureRHIRef> TextureLifetimes = RokCityGammaRaster::FillControlledParameters(Pass, Instance);
	Pass->RenderTargets[0] = FRenderTargetBinding(Color, ERenderTargetLoadAction::ELoad);
	Pass->RenderTargets.DepthStencil = FDepthStencilBinding(Depth, ERenderTargetLoadAction::ELoad,
		ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop);
	TShaderMapRef<RokCityGammaRaster::FSpriteVS> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	TShaderMapRef<RokCityGammaRaster::FControlledFixturePS> PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	const bool bOverwrite = Instance.Family == ERokCityGammaControlledFamily::GroundOpaqueSplat;
	const int32 TriangleCount = Instance.Indices.Num() / 3;
	GraphBuilder.AddPass(RDG_EVENT_NAME("RokCityGammaRaster.IsolatedFixture(%s)", *Instance.SourceKey.ToString()),
		Pass, ERDGPassFlags::Raster,
		[Pass, VertexShader, PixelShader, Rect, TriangleCount, bOverwrite,
			TextureLifetimes = MoveTemp(TextureLifetimes)](FRHICommandList& RHICmdList)
		{
			(void)TextureLifetimes;
			RokCityGammaRaster::DrawControlledTriangles(RHICmdList, Rect, TriangleCount,
				bOverwrite, VertexShader, PixelShader, Pass);
		});
	return true;
}
bool FRokCityGammaRaster::RegisterTarget(const FSceneInterface* Scene, const FRenderTarget* Target,
	uint64& OutToken, FString& OutError)
{
	check(IsInGameThread());
	OutToken = 0;
	if (!IsAvailable()) { OutError = TEXT("Gamma raster module unavailable"); return false; }
	if (!Scene || !Target) { OutError = TEXT("Gamma raster requires explicit scene and game viewport target"); return false; }
	const uint64 Candidate = RokCityGammaRaster::NextToken++;
	bool bAccepted = false;
	RokCityGammaRaster::FState* State = RokCityGammaRaster::State.Get();
	ENQUEUE_RENDER_COMMAND(RokRegisterCityGammaRaster)([State, Scene, Target, Candidate, &bAccepted](FRHICommandListImmediate&)
	{
		for (const TPair<uint64, RokCityGammaRaster::FTarget>& Entry : State->Targets)
		{
			if (Entry.Value.Scene == Scene || Entry.Value.RenderTarget == Target)
			{
				State->Stats.LastReject = ERokCityGammaRasterReject::DuplicateTarget;
				return;
			}
		}
		RokCityGammaRaster::FTarget& Entry = State->Targets.Add(Candidate);
		Entry.Scene = Scene;
		Entry.RenderTarget = Target;
		State->Stats.RegisteredTargets = State->Targets.Num();
		bAccepted = true;
	});
	FlushRenderingCommands();
	if (!bAccepted) { OutError = TEXT("Gamma raster scene or target already registered"); return false; }
	OutToken = Candidate;
	return true;
}
bool FRokCityGammaRaster::UpdateTarget(uint64 Token, const TArray<FRokCityGammaRasterInstance>& Instances,
	FString& OutError)
{
	check(IsInGameThread());
	if (!Token || !IsAvailable())
	{
		OutError = TEXT("Gamma raster registration unavailable");
		RokCityGammaRaster::RecordReject(ERokCityGammaRasterReject::Unavailable);
		RokCityGammaRaster::ClearSnapshot(Token);
		return false;
	}
	ERokCityGammaRasterReject Reason = ERokCityGammaRasterReject::None;
	if (!RokCityGammaRaster::Validate(Instances, OutError, Reason))
	{
		RokCityGammaRaster::RecordReject(Reason);
		RokCityGammaRaster::ClearSnapshot(Token);
		return false;
	}
	TArray<FRokCityGammaRasterInstance> Copy = Instances;
	Copy.Sort([](const FRokCityGammaRasterInstance& A, const FRokCityGammaRasterInstance& B)
	{
		if (A.SortPriority != B.SortPriority) return A.SortPriority < B.SortPriority;
		if (A.SourceOrder != B.SourceOrder) return A.SourceOrder < B.SourceOrder;
		return A.InstanceId < B.InstanceId;
	});
	bool bAccepted = false;
	RokCityGammaRaster::FState* State = RokCityGammaRaster::State.Get();
	ENQUEUE_RENDER_COMMAND(RokUpdateCityGammaRaster)([State, Token, Copy = MoveTemp(Copy), &bAccepted](FRHICommandListImmediate&) mutable
	{
		if (RokCityGammaRaster::FTarget* Target = State->Targets.Find(Token))
		{
			Target->Instances = MoveTemp(Copy);
			State->Stats.ActiveInstances = 0;
			for (const TPair<uint64, RokCityGammaRaster::FTarget>& Entry : State->Targets)
				State->Stats.ActiveInstances += Entry.Value.Instances.Num();
			bAccepted = true;
		}
	});
	FlushRenderingCommands();
	if (!bAccepted) OutError = TEXT("Gamma raster target was unregistered before snapshot update");
	return bAccepted;
}
void FRokCityGammaRaster::UnregisterTarget(uint64 Token)
{
	check(IsInGameThread());
	if (!Token || !RokCityGammaRaster::State.IsValid()) return;
	RokCityGammaRaster::FState* State = RokCityGammaRaster::State.Get();
	ENQUEUE_RENDER_COMMAND(RokUnregisterCityGammaRaster)([State, Token](FRHICommandListImmediate&)
	{
		State->Targets.Remove(Token);
		State->Stats.RegisteredTargets = State->Targets.Num();
		State->Stats.ActiveInstances = 0;
		for (const TPair<uint64, RokCityGammaRaster::FTarget>& Entry : State->Targets)
			State->Stats.ActiveInstances += Entry.Value.Instances.Num();
	});
	FlushRenderingCommands();
}
FRokCityGammaRasterStats FRokCityGammaRaster::GetStats()
{
	check(IsInGameThread());
	FRokCityGammaRasterStats Result;
	if (!RokCityGammaRaster::State.IsValid()) return Result;
	RokCityGammaRaster::FState* State = RokCityGammaRaster::State.Get();
	ENQUEUE_RENDER_COMMAND(RokReadCityGammaRasterStats)([State, &Result](FRHICommandListImmediate&)
	{
		Result = State->Stats;
	});
	FlushRenderingCommands();
	return Result;
}
void FRokCityGammaRaster::Startup()
{
	check(IsInGameThread());
	check(GEngine && !RokCityGammaRaster::State.IsValid());
	RokCityGammaRaster::State = MakeUnique<RokCityGammaRaster::FState>();
	RokCityGammaRaster::FState* State = RokCityGammaRaster::State.Get();
	RokCityGammaRaster::Extension = FSceneViewExtensions::NewExtension<RokCityGammaRaster::FViewExtension>(State);
	RokCityGammaRaster::PassHandle = GetRendererModule().RegisterPostOpaqueRenderDelegate(
		FPostOpaqueRenderDelegate::CreateLambda([State](FPostOpaqueRenderParameters& Parameters) { State->Draw(Parameters); }));
}
void FRokCityGammaRaster::Shutdown()
{
	check(IsInGameThread());
	if (!RokCityGammaRaster::State.IsValid()) return;
	if (RokCityGammaRaster::PassHandle.IsValid())
	{
		GetRendererModule().RemovePostOpaqueRenderDelegate(RokCityGammaRaster::PassHandle);
		RokCityGammaRaster::PassHandle.Reset();
	}
	RokCityGammaRaster::Extension.Reset();
	FlushRenderingCommands();
	RokCityGammaRaster::State.Reset();
}
