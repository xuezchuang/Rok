#include "RokCityGammaCompositor.h"

#include "EngineModule.h"
#include "Engine/Engine.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RendererInterface.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "ShaderParameterStruct.h"
#include "RHIResources.h"
#include "RHIGPUReadback.h"
#include "RHIFeatureLevel.h"

namespace RokCityGamma
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
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, ValidationStatus)
		SHADER_PARAMETER(FIntPoint, RectMin)
		SHADER_PARAMETER(FIntPoint, RectSize)
		SHADER_PARAMETER(uint32, bEncode)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};
IMPLEMENT_GLOBAL_SHADER(FTransferCS, "/Rok/Private/RokCityGammaTransfer.usf", "MainCS", SF_Compute);

struct FPendingValidation
{
	TUniquePtr<FRHIGPUBufferReadback> Readback;
};

struct FViewMatch
{
	uint64 Token = 0;
	// RenderOverlayExtensions does not populate Parameters.ViewUniformBuffer in UE 5.8.
	// Keep the matching view's PostOpaque buffer alive through the paired pass.
	TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer;
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	bool bBeforeSeen = false;
	bool bBeforeScheduled = false;
	bool bAfterSeen = false;
};

class FState final
{
public:
	TMap<uint64, const FRenderTarget*> Targets;
	// PreRenderView runs before InitRHIResources creates ViewUniformBuffer.
	// The renderer's public delegate supplies Uid = &View at both render hooks.
	TMap<const void*, FViewMatch> Views;
	TArray<FPendingValidation> PendingValidations;
	FRokCityGammaCompositorStats Stats;

	void PollReadbacks()
	{
		check(IsInRenderingThread());
		for (int32 Index = PendingValidations.Num() - 1; Index >= 0; --Index)
		{
			FRHIGPUBufferReadback* Readback = PendingValidations[Index].Readback.Get();
			if (!Readback->IsReady())
			{
				continue;
			}
			const uint32 Value = *static_cast<const uint32*>(Readback->Lock(sizeof(uint32)));
			Readback->Unlock();
			if (Value == 1u)
			{
				++Stats.ValidPreExposurePasses;
			}
			else
			{
				++Stats.InvalidPreExposurePasses;
			}
			PendingValidations.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
		Stats.PendingGpuValidations = PendingValidations.Num();
	}

	void BeginFamily()
	{
		check(IsInRenderingThread());
		// A previous family must never lend its view-uniform identity to another capture.
		FinishFamily();
	}

	void AddView(const FSceneView& View)
	{
		check(IsInRenderingThread());
		if (!View.bIsSceneCapture || !View.Family ||
			View.Family->SceneCaptureSource != SCS_FinalColorLDR ||
			View.Family->Views.Num() != 1 || IsMobilePlatform(View.GetShaderPlatform()) ||
			!View.Family->RenderTarget)
		{
			return;
		}
		uint64 MatchedToken = 0;
		for (const TPair<uint64, const FRenderTarget*>& Entry : Targets)
		{
			if (Entry.Value == View.Family->RenderTarget)
			{
				if (MatchedToken != 0)
				{
					++Stats.TargetConflicts;
					return;
				}
				MatchedToken = Entry.Key;
			}
		}
		if (MatchedToken == 0)
		{
			return;
		}
		const void* Identity = &View;
		if (Views.Contains(Identity))
		{
			++Stats.TargetConflicts;
			Views.Remove(Identity);
			return;
		}
		FViewMatch& Match = Views.Add(Identity);
		Match.Token = MatchedToken;
		Match.FeatureLevel = View.GetFeatureLevel();
	}

	void FinishFamily()
	{
		check(IsInRenderingThread());
		for (const TPair<const void*, FViewMatch>& Entry : Views)
		{
			if (!Entry.Value.bBeforeSeen)
			{
				++Stats.EligibleViewsWithoutBefore;
			}
			if (Entry.Value.bBeforeScheduled && !Entry.Value.bAfterSeen)
			{
				++Stats.UnpairedBeforePasses;
			}
		}
		Views.Empty();
		PollReadbacks();
	}

	bool AddTransfer(FPostOpaqueRenderParameters& Parameters, const FViewMatch& Match, bool bEncode)
	{
		FRDGTextureRef Color = Parameters.ColorTexture;
		FRDGBuilder* GraphBuilder = Parameters.GraphBuilder;
		if (!Color || !GraphBuilder || !Match.ViewUniformBuffer.IsValid() ||
			Match.FeatureLevel < ERHIFeatureLevel::SM5 ||
			Color->Desc.Dimension != ETextureDimension::Texture2D ||
			Color->Desc.Format != PF_FloatRGBA || Color->Desc.NumSamples != 1 ||
			!EnumHasAnyFlags(Color->Desc.Flags, TexCreate_ShaderResource) ||
			Parameters.ViewportRect.Min.X < 0 || Parameters.ViewportRect.Min.Y < 0 ||
			Parameters.ViewportRect.Max.X > Color->Desc.Extent.X ||
			Parameters.ViewportRect.Max.Y > Color->Desc.Extent.Y ||
			Parameters.ViewportRect.Width() <= 0 || Parameters.ViewportRect.Height() <= 0)
		{
			++Stats.RejectedTextures;
			return false;
		}

		const FIntRect Rect = Parameters.ViewportRect;
		FRDGTextureRef Scratch = GraphBuilder->CreateTexture(
			FRDGTextureDesc::Create2D(Color->Desc.Extent, Color->Desc.Format,
				FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV),
			bEncode ? TEXT("RokCityGamma.Encoded") : TEXT("RokCityGamma.Decoded"));
		FRDGBufferRef Validation = GraphBuilder->CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("RokCityGamma.PreExposureStatus"));
		FRDGBufferUAVRef ValidationUAV = GraphBuilder->CreateUAV(Validation, PF_R32_UINT);
		AddClearUAVPass(*GraphBuilder, ValidationUAV, 0u);

		FTransferCS::FParameters* Pass = GraphBuilder->AllocParameters<FTransferCS::FParameters>();
		Pass->View = Match.ViewUniformBuffer;
		Pass->InputTexture = Color;
		Pass->OutputTexture = GraphBuilder->CreateUAV(Scratch);
		Pass->ValidationStatus = ValidationUAV;
		Pass->RectMin = Rect.Min;
		Pass->RectSize = Rect.Size();
		Pass->bEncode = bEncode ? 1u : 0u;
		TShaderMapRef<FTransferCS> Shader(GetGlobalShaderMap(Match.FeatureLevel));
		FComputeShaderUtils::AddPass(*GraphBuilder,
			bEncode ? RDG_EVENT_NAME("RokCityGamma.OETF") : RDG_EVENT_NAME("RokCityGamma.EOTF"),
			Shader, Pass, FComputeShaderUtils::GetGroupCount(Rect.Size(), 8));

		FRHICopyTextureInfo CopyInfo;
		CopyInfo.SourcePosition = FIntVector(Rect.Min.X, Rect.Min.Y, 0);
		CopyInfo.DestPosition = CopyInfo.SourcePosition;
		CopyInfo.Size = FIntVector(Rect.Width(), Rect.Height(), 1);
		AddCopyTexturePass(*GraphBuilder, Scratch, Color, CopyInfo);

		FPendingValidation& Pending = PendingValidations.AddDefaulted_GetRef();
		Pending.Readback = MakeUnique<FRHIGPUBufferReadback>(TEXT("RokCityGamma.PreExposure"));
		AddEnqueueCopyPass(*GraphBuilder, Pending.Readback.Get(), Validation, sizeof(uint32));
		Stats.PendingGpuValidations = PendingValidations.Num();
		return true;
	}

	void Before(FPostOpaqueRenderParameters& Parameters)
	{
		check(IsInRenderingThread());
		FViewMatch* Match = Views.Find(Parameters.Uid);
		if (!Match)
		{
			return; // Every unrelated main view and SceneCapture is untouched.
		}
		if (Match->bBeforeSeen)
		{
			++Stats.TargetConflicts;
			return;
		}
		Match->bBeforeSeen = true;
		if (!Parameters.ViewUniformBuffer)
		{
			++Stats.RejectedTextures;
			return;
		}
		Match->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>(Parameters.ViewUniformBuffer);
		Match->bBeforeScheduled = AddTransfer(Parameters, *Match, true);
		if (Match->bBeforeScheduled)
		{
			++Stats.BeforePasses;
		}
	}

	void After(FPostOpaqueRenderParameters& Parameters)
	{
		check(IsInRenderingThread());
		FViewMatch* Match = Views.Find(Parameters.Uid);
		if (!Match)
		{
			return;
		}
		if (Match->bAfterSeen)
		{
			++Stats.TargetConflicts;
			return;
		}
		Match->bAfterSeen = true;
		if (!Match->bBeforeScheduled)
		{
			++Stats.UnpairedAfterPasses;
			return;
		}
		if (AddTransfer(Parameters, *Match, false))
		{
			++Stats.AfterPasses;
		}
		else
		{
			++Stats.UnpairedBeforePasses;
		}
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
FDelegateHandle BeforeHandle;
FDelegateHandle AfterHandle;
uint64 NextToken = 1;
}

FRokCityGammaTargetRegistration::~FRokCityGammaTargetRegistration() { Reset(); }
FRokCityGammaTargetRegistration::FRokCityGammaTargetRegistration(FRokCityGammaTargetRegistration&& Other) noexcept
	: Token(Other.Token) { Other.Token = 0; }
FRokCityGammaTargetRegistration& FRokCityGammaTargetRegistration::operator=(FRokCityGammaTargetRegistration&& Other) noexcept
{
	if (this != &Other)
	{
		Reset();
		Token = Other.Token;
		Other.Token = 0;
	}
	return *this;
}
bool FRokCityGammaTargetRegistration::Register(const FRenderTarget* Target, FString& OutError)
{
	if (Token != 0) { OutError = TEXT("Registration already owns a target"); return false; }
	return FRokCityGammaCompositor::RegisterTarget(Target, Token, OutError);
}
void FRokCityGammaTargetRegistration::Reset()
{
	if (Token != 0)
	{
		FRokCityGammaCompositor::UnregisterTarget(Token);
		Token = 0;
	}
}

bool FRokCityGammaCompositor::IsAvailable()
{
	return IsInGameThread() && RokCityGamma::State.IsValid() &&
		RokCityGamma::Extension.IsValid() && RokCityGamma::BeforeHandle.IsValid() &&
		RokCityGamma::AfterHandle.IsValid();
}

bool FRokCityGammaCompositor::RegisterTarget(const FRenderTarget* Target, uint64& OutToken, FString& OutError)
{
	check(IsInGameThread());
	OutToken = 0;
	if (!Target || !RokCityGamma::State.IsValid())
	{
		OutError = TEXT("Null target or RokRendering module unavailable");
		return false;
	}
	const uint64 Candidate = RokCityGamma::NextToken++;
	bool bAccepted = false;
	RokCityGamma::FState* State = RokCityGamma::State.Get();
	ENQUEUE_RENDER_COMMAND(RokRegisterCityGammaTarget)([State, Target, Candidate, &bAccepted](FRHICommandListImmediate&)
	{
		if (State->Targets.FindKey(Target))
		{
			++State->Stats.TargetConflicts;
			return;
		}
		State->Targets.Add(Candidate, Target);
		State->Stats.RegisteredTargets = State->Targets.Num();
		bAccepted = true;
	});
	FlushRenderingCommands();
	if (!bAccepted)
	{
		OutError = TEXT("Target already registered; no global or ambiguous view opt-in is allowed");
		return false;
	}
	OutToken = Candidate;
	OutError.Reset();
	return true;
}

void FRokCityGammaCompositor::UnregisterTarget(uint64 Token)
{
	check(IsInGameThread());
	if (!RokCityGamma::State.IsValid()) { return; }
	RokCityGamma::FState* State = RokCityGamma::State.Get();
	ENQUEUE_RENDER_COMMAND(RokUnregisterCityGammaTarget)([State, Token](FRHICommandListImmediate&)
	{
		State->Targets.Remove(Token);
		State->Stats.RegisteredTargets = State->Targets.Num();
	});
	FlushRenderingCommands();
}

FRokCityGammaCompositorStats FRokCityGammaCompositor::GetStats()
{
	check(IsInGameThread());
	FRokCityGammaCompositorStats Result;
	if (!RokCityGamma::State.IsValid()) { return Result; }
	RokCityGamma::FState* State = RokCityGamma::State.Get();
	ENQUEUE_RENDER_COMMAND(RokReadCityGammaStats)([State, &Result](FRHICommandListImmediate&)
	{
		State->PollReadbacks();
		Result = State->Stats;
	});
	FlushRenderingCommands();
	return Result;
}

void FRokCityGammaCompositor::Startup()
{
	check(IsInGameThread());
	check(!RokCityGamma::State.IsValid());
	check(GEngine);
	RokCityGamma::State = MakeUnique<RokCityGamma::FState>();
	RokCityGamma::FState* State = RokCityGamma::State.Get();
	RokCityGamma::Extension = FSceneViewExtensions::NewExtension<RokCityGamma::FViewExtension>(State);
	IRendererModule& Renderer = GetRendererModule();
	RokCityGamma::BeforeHandle = Renderer.RegisterPostOpaqueRenderDelegate(
		FPostOpaqueRenderDelegate::CreateLambda([State](FPostOpaqueRenderParameters& Parameters) { State->Before(Parameters); }));
	RokCityGamma::AfterHandle = Renderer.RegisterOverlayRenderDelegate(
		FPostOpaqueRenderDelegate::CreateLambda([State](FPostOpaqueRenderParameters& Parameters) { State->After(Parameters); }));
}

void FRokCityGammaCompositor::Shutdown()
{
	check(IsInGameThread());
	if (!RokCityGamma::State.IsValid()) { return; }
	IRendererModule& Renderer = GetRendererModule();
	if (RokCityGamma::BeforeHandle.IsValid())
	{
		Renderer.RemovePostOpaqueRenderDelegate(RokCityGamma::BeforeHandle);
		RokCityGamma::BeforeHandle.Reset();
	}
	if (RokCityGamma::AfterHandle.IsValid())
	{
		Renderer.RemoveOverlayRenderDelegate(RokCityGamma::AfterHandle);
		RokCityGamma::AfterHandle.Reset();
	}
	RokCityGamma::Extension.Reset();
	FlushRenderingCommands();
	RokCityGamma::State.Reset();
}
