#include "RokPowerGainWidget.h"

#include "Dom/JsonObject.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SLeafWidget.h"
#if WITH_EDITOR
#include "HAL/PlatformProperties.h"
#include "MaterialShared.h"
#include "ShaderCore.h"
#endif

class SRokPowerGain : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRokPowerGain) {} SLATE_END_ARGS()
	void Construct(const FArguments&, URokPowerGainWidget* InOwner) { Owner = InOwner; }
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 360); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		if (!Owner.IsValid() || !Owner->IsPlaying() || Owner->Brushes.Num() != 2) return Layer;
		const auto& O = *Owner.Get();
		auto Draw = [&](const URokPowerGainWidget::FDraw& A, const URokPowerGainWidget::FDraw* B, float Alpha, int32 Material, float TailAlpha = 1.f)
		{
			if (A.Indices.IsEmpty()) return;
			const bool bInterpolate = B && A.Indices == B->Indices && A.Positions.Num() == B->Positions.Num();
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			const FVector2f Pivot = FVector2f(Geometry.GetLocalSize()) * 0.5f;
			for (int32 V = 0; V < A.Positions.Num(); ++V)
			{
				const FVector2f P = bInterpolate ? FMath::Lerp(A.Positions[V], B->Positions[V], Alpha) : A.Positions[V];
				FLinearColor Color = (bInterpolate ? FMath::Lerp(A.Colors[V], B->Colors[V], Alpha) : A.Colors[V]) * Style.GetColorAndOpacityTint();
				Color.A *= TailAlpha;
				// Source is UI XY about the button pivot. Do not auto-fit to animated bounds.
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
					Pivot + FVector2f(P.X, -P.Y), FVector2f(A.UVs[V].X, 1 - A.UVs[V].Y), Color.ToFColor(true)));
			}
			for (uint32 I : A.Indices) Indices.Add(static_cast<SlateIndex>(I));
			const auto Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(O.Brushes[Material]);
			// Unity renders this effect in Gamma color space. Preserve raw color and
			// alpha multiplication instead of gamma-correcting the premultiplied sum.
			FSlateDrawElement::MakeCustomVerts(Elements, ++Layer, Handle, Vertices, Indices, nullptr, 0, 0, ESlateDrawEffect::NoGamma);
		};
		Draw(O.Mesh, nullptr, 0, 0);
		for (const auto& Particle : O.ParticleTimes)
		{
			if (Particle.bPendingBirth) continue;
			// Export index 0 is pre-emission; index 1 is particle age zero.
			// Keep request time separate from the first actual particle update.
			const float FrameTime = Particle.Age * O.SampleRate + 1.f;
			const int32 Index = FMath::Clamp(FMath::FloorToInt(FrameTime), 0, O.ParticleFrames.Num() - 1);
			const auto* Next = O.ParticleFrames.IsValidIndex(Index + 1) ? &O.ParticleFrames[Index + 1] : nullptr;
			// The source ColorModule ends at alpha zero at age 1. Hold the final
			// exported geometry (no position extrapolation), fade only this known
			// linear alpha endpoint. Color32 export quantization remains a visual gate.
			const float TailAlpha = Particle.Age > O.LastParticleSampleAge
				? FMath::Clamp((1.f - Particle.Age) / (1.f - O.LastParticleSampleAge), 0.f, 1.f) : 1.f;
			Draw(O.ParticleFrames[Index], Next, FMath::Frac(FrameTime), 1, TailAlpha);
		}
		return Layer;
	}
private:
	TWeakObjectPtr<URokPowerGainWidget> Owner;
};

float URokPowerGainWidget::EvaluateUTiling(float Seconds)
{
	// ref/client-unity/Assets/BundleAssets/UI/UIAnimation/UI_10010.anim:
	// unweighted Hermite keys; the final negative tiling is intentional.
	constexpr float Mid = 0.16666667f;
	if (Seconds <= Mid)
		return FMath::CubicInterp(1.0f, -8.567013f * Mid, 0.26447147f, -0.6046871f * Mid,
			FMath::Clamp(Seconds / Mid, 0.0f, 1.0f));
	constexpr float Duration = 0.85f - Mid;
	return FMath::CubicInterp(0.26447147f, -0.6046871f * Duration, -0.09f, 0.0f,
		FMath::Clamp((Seconds - Mid) / Duration, 0.0f, 1.0f));
}

bool URokPowerGainWidget::LoadSource()
{
	if (Materials.Num() == 2) return true;
	Mesh = FDraw(); ParticleFrames.Reset(); Textures.Reset(); Materials.Reset(); Brushes.Reset();
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/PowerGain/source_geometry.json")))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return false;
	if (Root->GetStringField(TEXT("unityVersion")) != TEXT("2019.4.28f1c1") || Root->GetNumberField(TEXT("lifetime")) != 1) return false;
	auto ReadDraw = [](const TSharedPtr<FJsonObject>& Data, FDraw& Draw)
	{
		if (!Data.IsValid()) return false;
		for (const auto& Value : Data->GetArrayField(TEXT("positions")))
		{
			const auto P = Value->AsObject();
			Draw.Positions.Add(FVector2f(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y"))));
		}
		for (const auto& Value : Data->GetArrayField(TEXT("uv")))
		{
			const auto P = Value->AsObject();
			Draw.UVs.Add(FVector2f(P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y"))));
		}
		for (const auto& Value : Data->GetArrayField(TEXT("colors")))
		{
			const auto C = Value->AsObject();
			Draw.Colors.Add(FLinearColor(C->GetNumberField(TEXT("r")), C->GetNumberField(TEXT("g")), C->GetNumberField(TEXT("b")), C->GetNumberField(TEXT("a"))));
		}
		if (Draw.Positions.Num() != Draw.UVs.Num() || Draw.Positions.Num() != Draw.Colors.Num()) return false;
		for (const auto& Value : Data->GetArrayField(TEXT("indices")))
		{
			const int32 I = Value->AsNumber();
			if (!Draw.Positions.IsValidIndex(I)) return false;
			Draw.Indices.Add(I);
		}
		return Draw.Indices.Num() % 3 == 0;
	};
	if (!ReadDraw(Root->GetObjectField(TEXT("mesh")), Mesh)) return false;
	SampleRate = Root->GetNumberField(TEXT("sampleRate"));
	if (SampleRate != 120) return false;
	for (const auto& Value : Root->GetArrayField(TEXT("samples")))
	{
		FDraw Draw;
		if (!ReadDraw(Value->AsObject()->GetObjectField(TEXT("particle")), Draw)) return false;
		ParticleFrames.Add(MoveTemp(Draw));
	}
	if (ParticleFrames.Num() != 121) return false;
	const auto& Samples = Root->GetArrayField(TEXT("samples"));
	if (!ParticleFrames[0].Indices.IsEmpty() || ParticleFrames[1].Indices.IsEmpty()
		|| !FMath::IsNearlyEqual(float(Samples[1]->AsObject()->GetNumberField(TEXT("particleRemainingLifetime"))), 1.f)) return false;
	LastParticleSampleAge = 1.f - float(Samples.Last()->AsObject()->GetNumberField(TEXT("particleRemainingLifetime")));
	if (LastParticleSampleAge <= 0 || LastParticleSampleAge >= 1) return false;
	auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokPrototype/UI/Materials/M_RokPowerGainUI.M_RokPowerGainUI"));
	if (!Base) return false;
#if WITH_EDITOR
	if (!FPlatformProperties::RequiresCookedData())
	{
		// UE 5.8 can defer Slate permutations until first draw. Submit them while
		// constructing the HUD, without playing an effect or blocking gameplay.
		if (FMaterialResource* Resource = Base->GetMaterialResource(GetFeatureLevelShaderPlatform(GMaxRHIFeatureLevel)))
		{
			Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::Normal);
			bShaderPreloadRequested = true;
		}
	}
#endif
	for (const TCHAR* Name : {TEXT("tile003"), TEXT("glow002")})
	{
		const FString Path = FPaths::ProjectDir() / TEXT("ref/client-unity/Assets/BundleAssets/Effect/Textures/shared_effecttextures") / (FString(Name) + TEXT(".png"));
		auto* Texture = FImageUtils::ImportFileAsTexture2D(Path);
		if (!Texture) { Materials.Reset(); return false; }
		Texture->AddressX = TA_Wrap; Texture->AddressY = TA_Wrap;
		Texture->Filter = TF_Bilinear;
		Texture->SRGB = false;
		Texture->UpdateResource();
		auto* Material = UMaterialInstanceDynamic::Create(Base, this);
		Material->SetTextureParameterValue(TEXT("Sprite"), Texture);
		Material->SetScalarParameterValue(TEXT("UTiling"), 1);
		Textures.Add(Texture); Materials.Add(Material);
		FSlateBrush Brush; Brush.SetResourceObject(Material); Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		Brushes.Add(Brush);
	}
	return true;
}

bool URokPowerGainWidget::Play()
{
	if (!LoadSource())
	{
		UE_LOG(LogTemp, Error, TEXT("Power gain source geometry/material unavailable; run source exporter and material generator"));
		return false;
	}
	// UI_Item_PlayerPowerInfo_SubView: extend AutoDestory, restart Animator, and
	// replay emission without clearing particles already alive.
	Remaining += 1;
	bPlaying = true;
	MeshTime = 0;
	ParticleTimes.Add(FParticlePlayback());
	Materials[0]->SetScalarParameterValue(TEXT("UTiling"), 1);
	if (SlateWidget) SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
	return true;
}

void URokPowerGainWidget::Advance(float DeltaSeconds)
{
	if (!IsPlaying() || !FMath::IsFinite(DeltaSeconds)) return;
	const float Delta = FMath::Max(0, DeltaSeconds);
	const float NewRemaining = Remaining - Delta;
	// Original AutoDestory uses Time.time-live > time, not >=.
	bPlaying = NewRemaining >= 0;
	Remaining = FMath::Max(0.f, NewRemaining);
	MeshTime += Delta;
	if (Delta > 0)
		for (auto& Particle : ParticleTimes)
		{
			if (Particle.bPendingBirth) Particle.bPendingBirth = false;
			else Particle.Age += Delta;
		}
	ParticleTimes.RemoveAll([](const FParticlePlayback& Particle) { return !Particle.bPendingBirth && Particle.Age >= 1; });
	if (!IsPlaying()) ParticleTimes.Reset();
	Materials[0]->SetScalarParameterValue(TEXT("UTiling"), EvaluateUTiling(MeshTime));
	if (SlateWidget) SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
}

void URokPowerGainWidget::ObservePower(int32 Previous, int32 Current, bool bOwnerVisible)
{
	if (Current < 0) { bHasPowerObservation = false; return; }
	bool bVisible = bOwnerVisible;
	for (const UWidget* Parent = GetParent(); Parent; Parent = Parent->GetParent())
		bVisible &= Parent->IsVisible() && Parent->GetRenderOpacity() > 0;
	// Initialization is distinct from a valid zero score. Decreases do not stop
	// an existing source effect; they simply do not create a new one.
	if (bHasPowerObservation && Current > Previous && bVisible) Play();
	bHasPowerObservation = true;
}

TSharedRef<SWidget> URokPowerGainWidget::RebuildWidget()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!LoadSource())
		UE_LOG(LogTemp, Error, TEXT("Power gain preload failed; source geometry/material unavailable"));
	return SAssignNew(SlateWidget, SRokPowerGain, this);
}

void URokPowerGainWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateWidget.Reset();
}
