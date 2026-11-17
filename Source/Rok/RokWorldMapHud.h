#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Layout/Margin.h"
#include "RokWorldMapHud.generated.h"

class URokUiPageWidget;

UCLASS()
class ROK_API ARokWorldMapHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	struct FCaptionStyle
	{
		FLinearColor Background;
		FLinearColor Color;
		FVector2D Offset;
		FMargin Padding;
		float TextScale = 1.0f;
	};

	void InitializeStyles();
	void DrawCaption(const FString& Text, UFont* Font, const FCaptionStyle& Style,
		const FVector2D& Anchor, bool bAboveAnchor);

	FCaptionStyle DebugStatus{FLinearColor(0.015f, 0.02f, 0.025f, 0.82f),
		FLinearColor(0.94f, 0.9f, 0.72f, 1.0f), {0.0, 16.0}, FMargin(18, 8)};
	FCaptionStyle SelectionCaption{FLinearColor(0.01f, 0.08f, 0.025f, 0.86f),
		FLinearColor(0.12f, 1.0f, 0.38f, 1.0f), {0.0, -12.0}, FMargin(8, 4)};
	FLinearColor SelectionCrossColor = FLinearColor(0.12f, 1.0f, 0.38f, 1.0f);
	FVector2D SelectionCrossOffset = FVector2D::ZeroVector;
	float SelectionRadius = 18.0f;
	float SelectionThickness = 2.0f;
	bool bStyleInitialized = false;
	UPROPERTY(Transient) TObjectPtr<URokUiPageWidget> StylePage;
};
