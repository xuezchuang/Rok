#include "RokCommanderMeshWidget.h"
#include "RokCommanderCamera.h"

#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SLeafWidget.h"

class SRokCommanderMesh : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRokCommanderMesh) {} SLATE_END_ARGS()
	void Construct(const FArguments&, URokCommanderMeshWidget* InOwner)
	{
		Owner = InOwner;
		bAudit = FParse::Param(FCommandLine::Get(), TEXT("RokCommanderMeshAudit"));
		SetCanTick(true);
	}
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1440, 900); }
	virtual void Tick(const FGeometry&, double, float Delta) override
	{
		if (Owner.IsValid() && Owner->Duration > 0)
		{
			Owner->AdvanceAnimation(Delta);
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		if (!Owner.IsValid() || Owner->Frames.IsEmpty()) return Layer;
		const auto& O = *Owner.Get();
		const float FrameTime = O.Time / O.Duration * (O.bPlayingStandby ? O.Frames.Num() - 1 : O.Frames.Num());
		const int32 FrameIndex = FMath::FloorToInt(FrameTime) % O.Frames.Num();
		const auto& Frame = O.Frames[FrameIndex];
		const auto& Next = O.Frames[O.bPlayingStandby ? FMath::Min(FrameIndex + 1, O.Frames.Num() - 1) : (FrameIndex + 1) % O.Frames.Num()];
		if (bAudit && ((!bReportedStart) || (!bReportedLater && FrameIndex >= O.Frames.Num() / 2)))
		{
			UE_LOG(LogTemp, Display, TEXT("Commander mesh painted: time=%.6f sample=%d draws=%d"), O.Time, FrameIndex, Frame.Draws.Num());
			if (bReportedStart) bReportedLater = true;
			bReportedStart = true;
		}
		const float Alpha = FMath::Frac(FrameTime);
		const FVector2f Size(Geometry.GetLocalSize());
		for (int32 D = 0; D < Frame.Draws.Num(); ++D)
		{
			const auto& Draw = Frame.Draws[D];
			const auto* NextDraw = Next.Draws.IsValidIndex(D) ? &Next.Draws[D] : nullptr;
			const bool bInterpolate = NextDraw && NextDraw->Identity == Draw.Identity
				&& NextDraw->Indices == Draw.Indices && NextDraw->UVs == Draw.UVs
				&& NextDraw->Positions.Num() == Draw.Positions.Num();
			TArray<FSlateVertex> Vertices;
			TArray<SlateIndex> Indices;
			const FColor Color = (Draw.Color * Style.GetColorAndOpacityTint()).ToFColor(true);
			for (int32 V = 0; V < Draw.Positions.Num(); ++V)
			{
				const FVector2f P = bInterpolate ? FMath::Lerp(Draw.Positions[V], NextDraw->Positions[V], Alpha) : Draw.Positions[V];
				const FVector2f Local = O.bCivilizationProjection
					? RokCommanderCamera::ProjectCivilization(P, Size, O.CivilizationAboutOffset)
					: RokCommanderCamera::Project(P, Size, O.CameraX);
				Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), Local, Draw.UVs[V], Color));
			}
			for (uint32 Index : Draw.Indices) Indices.Add(static_cast<SlateIndex>(Index));
			const auto Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(O.Brushes[Draw.TextureIndex]);
			FSlateDrawElement::MakeCustomVerts(Elements, ++Layer, Handle, Vertices, Indices, nullptr, 0, 0);
		}
		return Layer;
	}
private:
	TWeakObjectPtr<URokCommanderMeshWidget> Owner;
	bool bAudit = false;
	mutable bool bReportedStart = false;
	mutable bool bReportedLater = false;
};

bool URokCommanderMeshWidget::LoadHero(int32 HeroId)
{
	bPlayingStandby = false;
	StandbyFrames.Reset(); StandbyDuration = 0;
	CameraX = 0;
	if (!LoadClip(HeroId, TEXT("idle"))) return false;
	const FString StandbyPath = FPaths::ProjectSavedDir() / FString::Printf(TEXT("RokUnityMigration/Commander/hero_%d_standby_mesh.json"), HeroId);
	if (FPaths::FileExists(StandbyPath))
	{
		auto* Clip = NewObject<URokCommanderMeshWidget>(this);
		if (!Clip->LoadClip(HeroId, TEXT("standby"))) return false;
		const int32 TextureOffset = Textures.Num();
		for (auto& Frame : Clip->Frames)
			for (auto& Draw : Frame.Draws) Draw.TextureIndex += TextureOffset;
		Textures.Append(Clip->Textures); Brushes.Append(Clip->Brushes);
		StandbyFrames = MoveTemp(Clip->Frames); StandbyDuration = Clip->Duration;
		// Keep the idle coordinate frame/scale while the gesture changes its bounds.
		// Per-animation auto-fit would make the whole character jump in size.
	}
	return true;
}

bool URokCommanderMeshWidget::PlayStandby()
{
	// CaptainMediator.OnCharClickEvent: ignore repeated clicks until standby completes.
	if (bPlayingStandby || StandbyFrames.IsEmpty()) return false;
	Swap(Frames, StandbyFrames); Swap(Duration, StandbyDuration);
	bPlayingStandby = true; Time = 0;
	UE_LOG(LogTemp, Display, TEXT("Commander gesture: standby started duration=%.6f"), Duration);
	return true;
}

void URokCommanderMeshWidget::AdvanceAnimation(float Delta)
{
	CameraX = RokCommanderCamera::Advance(CameraX, Delta);
	if (Duration <= 0 || !FMath::IsFinite(Delta)) return;
	Time += FMath::Max(Delta, 0.0f);
	if (bPlayingStandby && Time >= Duration)
	{
		const float Remaining = Time - Duration;
		Swap(Frames, StandbyFrames); Swap(Duration, StandbyDuration);
		bPlayingStandby = false; Time = Remaining;
		UE_LOG(LogTemp, Display, TEXT("Commander gesture: returned to idle"));
	}
	if (!bPlayingStandby) Time = FMath::Fmod(Time, Duration);
}

bool URokCommanderMeshWidget::LoadClip(int32 HeroId, const FString& Animation)
{
	Frames.Reset(); Brushes.Reset(); Textures.Reset(); Time = 0;
	FString Json;
	const FString Path = FPaths::ProjectSavedDir() / FString::Printf(TEXT("RokUnityMigration/Commander/hero_%d_%s_mesh.json"), HeroId, *Animation);
	if (!FFileHelper::LoadFileToString(Json, *Path)) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) return false;
	// This file is generated migration input; reject unsupported render semantics.
	Duration = Root->GetNumberField(TEXT("duration"));
	const auto& Bounds = Root->GetArrayField(TEXT("bounds"));
	if (Bounds.Num() != 4 || Duration <= 0 || !FMath::IsFinite(Duration)) return false;
	BoundsMin = FVector2f(Bounds[0]->AsNumber(), Bounds[1]->AsNumber());
	BoundsMax = FVector2f(Bounds[2]->AsNumber(), Bounds[3]->AsNumber());
	if (BoundsMax.X <= BoundsMin.X || BoundsMax.Y <= BoundsMin.Y) return false;
	TMap<FString, int32> TextureIndices;
	for (const auto& FrameValue : Root->GetArrayField(TEXT("frames")))
	{
		FFrame Frame;
		for (const auto& DrawValue : FrameValue->AsObject()->GetArrayField(TEXT("draws")))
		{
			const auto& Data = DrawValue->AsObject();
			if (Data->GetStringField(TEXT("blend")) != TEXT("Normal"))
			{
				UE_LOG(LogTemp, Error, TEXT("Commander mesh %d: unsupported source blend mode"), HeroId);
				return false;
			}
			FDraw Draw;
			Draw.Identity = Data->GetStringField(TEXT("slot")) + TEXT("/") + Data->GetStringField(TEXT("attachment"));
			const FString TexturePath = Data->GetStringField(TEXT("texture"));
			if (!TextureIndices.Contains(TexturePath))
			{
				UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectDir() / TexturePath);
				if (!Texture) return false;
				Texture->Filter = TF_Bilinear;
				Texture->UpdateResource();
				TextureIndices.Add(TexturePath, Textures.Num()); Textures.Add(Texture);
				FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
				Brushes.Add(Brush);
			}
			Draw.TextureIndex = TextureIndices[TexturePath];
			const auto& XY = Data->GetArrayField(TEXT("xy"));
			const auto& UV = Data->GetArrayField(TEXT("uv"));
			if (XY.Num() != UV.Num() || XY.Num() % 2 || XY.Num() > 65534) return false;
			for (int32 I = 0; I < XY.Num(); I += 2)
			{
				Draw.Positions.Emplace(XY[I]->AsNumber(), XY[I+1]->AsNumber());
				Draw.UVs.Emplace(UV[I]->AsNumber(), UV[I+1]->AsNumber());
			}
			for (const auto& Value : Data->GetArrayField(TEXT("triangles")))
			{
				const int32 Index = Value->AsNumber();
				if (Index < 0 || Index >= Draw.Positions.Num()) return false;
				Draw.Indices.Add(Index);
			}
			const auto& RGBA = Data->GetArrayField(TEXT("rgba"));
			if (RGBA.Num() != 4) return false;
			Draw.Color = FLinearColor(RGBA[0]->AsNumber(), RGBA[1]->AsNumber(), RGBA[2]->AsNumber(), RGBA[3]->AsNumber());
			Frame.Draws.Add(MoveTemp(Draw));
		}
		Frames.Add(MoveTemp(Frame));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UE_LOG(LogTemp, Display, TEXT("Commander source mesh loaded: hero=%d frames=%d duration=%.6f"), HeroId, Frames.Num(), Duration);
	return Frames.Num() >= 2;
}

TSharedRef<SWidget> URokCommanderMeshWidget::RebuildWidget()
{
	return SAssignNew(MeshWidget, SRokCommanderMesh, this);
}

void URokCommanderMeshWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MeshWidget.Reset();
}

#if WITH_DEV_AUTOMATION_TESTS
bool URokCommanderMeshWidget::HasDeformedSamplesForTesting() const
{
	if (Frames.Num() < 2) return false;
	for (const FFrame& Frame : Frames)
	{
		if (Frame.Draws.Num() != Frames[0].Draws.Num()) return true;
		for (int32 D = 0; D < Frame.Draws.Num(); ++D)
			if (Frame.Draws[D].Positions != Frames[0].Draws[D].Positions) return true;
	}
	return false;
}
#endif
