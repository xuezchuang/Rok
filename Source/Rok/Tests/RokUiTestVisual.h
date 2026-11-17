#pragma once

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"

namespace RokUiTestVisual
{
inline const UWidget* Parent(const UWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}
	if (const UWidget* Panel = Widget->GetParent())
	{
		return Panel;
	}
	const UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter());
	return Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
}

inline float Opacity(const UWidget* Widget)
{
	float Result = 1.0f;
	for (const UWidget* Current = Widget; Current; Current = Parent(Current))
	{
		Result *= Current->GetRenderOpacity();
	}
	return Result;
}

inline FVector2D Translation(const UWidget* Widget)
{
	FVector2D Result = FVector2D::ZeroVector;
	for (const UWidget* Current = Widget; Current; Current = Parent(Current))
	{
		Result += Current->GetRenderTransform().Translation;
	}
	return Result;
}
}
