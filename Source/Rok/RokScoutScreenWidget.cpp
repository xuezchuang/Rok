#include "RokScoutScreenWidget.h"
#include "RokArmyShowPanelAnimation.h"
#include "RokUiAnimatedButton.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

namespace
{
	const TCHAR* Common = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
	const TCHAR* Frame = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame");
	FIntRect ScoutSpriteRect(int32 X, int32 Y, int32 W, int32 H) { return FIntRect(X, Y, X + W, Y + H); }
	void Place(UCanvasPanel* Parent, UWidget* Widget, FVector2D Position, FVector2D Size, int32 Z = 0)
	{
		auto* Slot = Parent->AddChildToCanvas(Widget);
		Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetZOrder(Z);
	}
	FSlateBrush Atlas(const TCHAR* Path, FIntRect Region, FIntPoint Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(Region.Width(), Region.Height());
		Brush.SetUVRegion(FBox2f(FVector2f(float(Region.Min.X) / Size.X, float(Size.Y - Region.Max.Y) / Size.Y),
			FVector2f(float(Region.Max.X) / Size.X, float(Size.Y - Region.Min.Y) / Size.Y)));
		return Brush;
	}
	UImage* Image(UWidgetTree* Tree, FName Name, const TCHAR* Path, FIntRect Region, FIntPoint Size)
	{
		auto* Result = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Result->SetBrush(Atlas(Path, Region, Size)); Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}
	UTextBlock* Text(UWidgetTree* Tree, FName Name, const FString& Value, float Pixels, ETextJustify::Type Justify = ETextJustify::Left)
	{
		auto* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15);
		Font.Size = Pixels * 0.75f; // Unity pixels; Slate fonts use points at 96 DPI.
		Result->SetFont(Font); Result->SetText(FText::FromString(Value)); Result->SetJustification(Justify);
		Result->SetColorAndOpacity(FSlateColor(FLinearColor(0.851f, 0.831f, 0.729f)));
		Result->SetAutoWrapText(true); Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}
	UButton* Button(UWidgetTree* Tree, FName Name, const FSlateBrush* Brush = nullptr, bool bAnimated = true)
	{
		UButton* Result = bAnimated
			? Tree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), Name)
			: Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style;
		Style.SetNormal(Brush ? *Brush : Empty).SetHovered(Brush ? *Brush : Empty).SetPressed(Brush ? *Brush : Empty)
			.SetDisabled(Brush ? *Brush : Empty).SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
		Result->SetStyle(Style); Result->SetBackgroundColor(FLinearColor::White);
		return Result;
	}
	// Atlas UV regions cannot be nine-sliced directly by Slate Box brushes.
	UCanvasPanel* FramePiece(UWidgetTree* Tree, FName Name, FIntRect Region, FVector2D Size, FIntPoint LeftRight, FIntPoint TopBottom)
	{
		auto* Panel = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
		Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
		const int32 SX[] = {Region.Min.X, Region.Min.X + LeftRight.X, Region.Max.X - LeftRight.Y, Region.Max.X};
		const int32 SY[] = {Region.Max.Y, Region.Max.Y - TopBottom.X, Region.Min.Y + TopBottom.Y, Region.Min.Y};
		const double DX[] = {0, double(LeftRight.X), Size.X - LeftRight.Y, Size.X};
		const double DY[] = {0, double(TopBottom.X), Size.Y - TopBottom.Y, Size.Y};
		for (int32 Y = 0; Y < 3; ++Y) for (int32 X = 0; X < 3; ++X)
		{
			if (SX[X + 1] <= SX[X] || SY[Y] <= SY[Y + 1]) continue;
			auto* Patch = Image(Tree, FName(*FString::Printf(TEXT("%s_%d_%d"), *Name.ToString(), X, Y)), Frame,
				FIntRect(SX[X], SY[Y + 1], SX[X + 1], SY[Y]), FIntPoint(1024, 2048));
			Place(Panel, Patch, FVector2D(DX[X], DY[Y]), FVector2D(DX[X + 1] - DX[X], DY[Y + 1] - DY[Y]));
		}
		return Panel;
	}
}

TSharedRef<SWidget> URokScoutScreenWidget::RebuildWidget()
{
	if (!RootCanvas)
	{
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScoutRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;
		SetIsFocusable(true);
		SetVisibility(ESlateVisibility::Collapsed);
	}
	return Super::RebuildWidget();
}

void URokScoutScreenWidget::Open(int32 KnownCampLevel)
{
	TakeWidget();
	if (!bOpen && GetOwningPlayer() && FSlateApplication::IsInitialized())
	{
		PreviousKeyboardFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	CampLevel = KnownCampLevel;
	Catalog.Load(CatalogError);
	bOpen = true; bDescription = false; PanelSeconds = 0.25f;
	WindowAnimation.Reset(); WindowAnimation.Open(ERokUiWindowAnimationStyle::Type1);
	Build(); SetVisibility(ESlateVisibility::Visible); ApplyAnimation();
	bPendingKeyboardFocus = GetOwningPlayer() != nullptr;
	AcquireKeyboardFocus();
}

void URokScoutScreenWidget::Build()
{
	RootCanvas->ClearChildren();
	// kTouchClose mask is outside the root Animator so scale cannot expose the world to input.
	auto* Mask = Button(WidgetTree, TEXT("ScoutBackgroundClose"), nullptr, false);
	FSlateBrush MaskBrush; MaskBrush.DrawAs = ESlateBrushDrawType::Image; MaskBrush.TintColor = FLinearColor(0, 0, 0, 0.55f);
	FButtonStyle MaskStyle; MaskStyle.SetNormal(MaskBrush).SetHovered(MaskBrush).SetPressed(MaskBrush);
	Mask->SetStyle(MaskStyle); Mask->OnClicked.AddDynamic(this, &URokScoutScreenWidget::RequestClose);
	auto* MaskSlot = RootCanvas->AddChildToCanvas(Mask); MaskSlot->SetAnchors(FAnchors(0, 0, 1, 1)); MaskSlot->SetOffsets(FMargin(0));
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ScoutScaleBox"));
	Scale->SetStretch(EStretch::ScaleToFit);
	auto* ScaleSlot = RootCanvas->AddChildToCanvas(Scale); ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1)); ScaleSlot->SetOffsets(FMargin(0));
	Scale->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScoutDesignSize"));
	Size->SetWidthOverride(1440); Size->SetHeightOverride(810); Scale->AddChild(Size);
	MotionCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScoutMotionCanvas"));
	Size->AddChild(MotionCanvas); MotionCanvas->SetRenderTransformPivot(FVector2D(0.5));
	MotionCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// UI_Model_Window_Type1 exact four mirrored source pieces, not generic frame dimensions.
	auto* BodyLeft = FramePiece(WidgetTree, TEXT("ScoutBodyLeft"), ScoutSpriteRect(1, 822, 524, 659), FVector2D(570, 698), FIntPoint(493, 15), FIntPoint(278, 291));
	Place(MotionCanvas, BodyLeft, FVector2D(166.5, 71.5), FVector2D(570, 698));
	auto* BodyRight = FramePiece(WidgetTree, TEXT("ScoutBodyRight"), ScoutSpriteRect(1, 822, 524, 659), FVector2D(570, 698), FIntPoint(493, 15), FIntPoint(278, 291));
	BodyRight->SetRenderTransformPivot(FVector2D(0.5)); BodyRight->SetRenderScale(FVector2D(-1, 1));
	Place(MotionCanvas, BodyRight, FVector2D(703.5, 71.5), FVector2D(570, 698));
	auto* HeaderLeft = FramePiece(WidgetTree, TEXT("ScoutHeaderLeft"), ScoutSpriteRect(1, 329, 560, 60), FVector2D(587, 60), FIntPoint(539, 8), FIntPoint(0, 0));
	Place(MotionCanvas, HeaderLeft, FVector2D(133.5, 45.5), FVector2D(587, 60), 2);
	auto* HeaderRight = FramePiece(WidgetTree, TEXT("ScoutHeaderRight"), ScoutSpriteRect(1, 329, 560, 60), FVector2D(586, 60), FIntPoint(539, 8), FIntPoint(0, 0));
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5)); HeaderRight->SetRenderScale(FVector2D(-1, 1));
	Place(MotionCanvas, HeaderRight, FVector2D(720.5, 45.5), FVector2D(586, 60), 2);
	// A transparent hit shield prevents clicks on the empty panel from dismissing the window.
	Place(MotionCanvas, Button(WidgetTree, TEXT("ScoutWindowInputShield"), nullptr, false), FVector2D(166.5, 71.5), FVector2D(1107, 698), 3);
	auto* Title = Text(WidgetTree, TEXT("ScoutTitle"), Catalog.Text.Contains(180021) ? Catalog.Text[180021] : TEXT("斥候营地"), 30, ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	// UI_Model_Window_Type1.lbl_title Shadow: Unity (0,-2) is two pixels down in Slate.
	Title->SetShadowColorAndOpacity(FLinearColor(0.11372549f, 0.08627451f, 0.08627451f, 1));
	Title->SetShadowOffset(FVector2D(0, 2));
	Place(MotionCanvas, Title, FVector2D(140, 55), FVector2D(1160, 40), 4);
	const auto CloseBrush = Atlas(Common, ScoutSpriteRect(883, 589, 69, 69), FIntPoint(2048, 1024));
	auto* Close = Button(WidgetTree, TEXT("ScoutCloseButton"), &CloseBrush);
	Close->OnClicked.AddDynamic(this, &URokScoutScreenWidget::RequestClose);
	Place(MotionCanvas, Close, FVector2D(1223, 39.5), FVector2D(69), 10);
	const auto BackBrush = Atlas(Common, ScoutSpriteRect(703, 282, 69, 69), FIntPoint(2048, 1024));
	BackButton = Button(WidgetTree, TEXT("ScoutBackButton"), &BackBrush);
	BackButton->OnClicked.AddDynamic(this, &URokScoutScreenWidget::ShowList);
	BackButton->SetVisibility(ESlateVisibility::Collapsed);
	Place(MotionCanvas, BackButton, FVector2D(149, 39.5), FVector2D(69), 10);
	List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ScoutListScroll"));
	List->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Place(MotionCanvas, List, FVector2D(212, 96), FVector2D(1016, 613), 5);
	auto* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScoutListSize"));
	ListSize->SetWidthOverride(1016); ListSize->SetHeightOverride(613); List->AddChild(ListSize);
	auto* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScoutListContent"));
	ListSize->AddChild(Content);
	Place(Content, Image(WidgetTree, TEXT("ScoutSummaryBar"), Common, ScoutSpriteRect(1003, 976, 964, 47), FIntPoint(2048, 1024)), FVector2D(14.3, 26), FVector2D(987.4, 43));
	auto* Progress = Text(WidgetTree, TEXT("ScoutExplorationProgress"), TEXT("王国探索进度待同步"), 24, ETextJustify::Center);
	Progress->SetColorAndOpacity(FSlateColor(FLinearColor(0.886f, 0.820f, 0.608f)));
	Place(Content, Progress, FVector2D(40, 32), FVector2D(936, 32), 1);
	// ArabLayout flags 10: Chinese flips X from -25 to +25 and text right->left.
	auto* SummarySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScoutSummaryTextSize"));
	auto* Summary = Text(WidgetTree, TEXT("ScoutSummaryText"), Catalog.Text.FindRef(181144), 20);
	Summary->SetLineHeightPercentage(1.25f); Summary->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CastChecked<USizeBoxSlot>(SummarySize->AddChild(Summary))->SetVerticalAlignment(VAlign_Center);
	Place(Content, SummarySize, FVector2D(72, 81.45), FVector2D(922, 67.5), 1);
	auto* Info = Button(WidgetTree, TEXT("ScoutInfoButton"));
	auto* InfoContent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ScoutInfoContent"));
	Info->AddChild(InfoContent);
	Place(InfoContent, Image(WidgetTree, TEXT("ScoutInfoImage"), Common, ScoutSpriteRect(782, 237, 37, 37), FIntPoint(2048, 1024)), FVector2D(9.87), FVector2D(28.26));
	Info->OnClicked.AddDynamic(this, &URokScoutScreenWidget::ShowDescription);
	// btn_info source flags 2: anchored right -25.4 becomes anchored left +25.4.
	Place(Content, Info, FVector2D(1.4, 91), FVector2D(48), 2);
	Place(Content, Image(WidgetTree, TEXT("ScoutSummaryLine"), Common, ScoutSpriteRect(1, 611, 570, 2), FIntPoint(2048, 1024)), FVector2D(0, 171), FVector2D(1016, 3));
	FString CampText;
	if (const auto* Level = Catalog.FindLevel(CampLevel))
	{
		CampText = FString::Printf(TEXT("斥候营地  等级 %d\n该等级可容纳 %d 名斥候"), Level->Level, Level->Capacity);
	}
	else if (CampLevel == 0) CampText = TEXT("斥候营地尚未建造");
	else CampText = TEXT("营地等级待同步");
	Place(Content, Text(WidgetTree, TEXT("ScoutCampConfiguration"), CampText, 24, ETextJustify::Center), FVector2D(130, 210), FVector2D(756, 90));
	// GetScoutNum reads Role_ScoutsInfo.scoutsQueue. Capacity is NOT an idle roster.
	Place(Content, Text(WidgetTree, TEXT("ScoutQueueUnavailable"), TEXT("斥候状态待同步\n同步后可查看斥候动态并进行派遣。"), 22, ETextJustify::Center), FVector2D(110, 334), FVector2D(796, 100));
	if (!CatalogError.IsEmpty())
	{
		Place(Content, Text(WidgetTree, TEXT("ScoutCatalogUnavailable"), TEXT("斥候资料未加载"), 20, ETextJustify::Center), FVector2D(130, 465), FVector2D(756, 40));
		UE_LOG(LogTemp, Error, TEXT("Rok Scout: %s"), *CatalogError);
	}
	// lbl_Desc is a fixed LanguageText, not a ScrollRect: wrap horizontally and
	// truncate at its 928x520 source rectangle. Retain the existing audit widget name.
	Description = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScoutDescriptionScroll"));
	Description->SetWidthOverride(928); Description->SetHeightOverride(520);
	Description->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	Description->SetVisibility(ESlateVisibility::Collapsed);
	Place(MotionCanvas, Description, FVector2D(256, 141), FVector2D(928, 520), 5);
	// lbl_Desc has no ArabLayout component; source remains upper-right aligned.
	auto* DescriptionText = Text(WidgetTree, TEXT("ScoutDescriptionText"), Catalog.Text.FindRef(181002), 20, ETextJustify::Right);
	DescriptionText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
	DescriptionText->SetLineHeightPercentage(1.3f);
	CastChecked<USizeBoxSlot>(Description->AddChild(DescriptionText))->SetVerticalAlignment(VAlign_Top);
	AnimationBlocker = Button(WidgetTree, TEXT("ScoutAnimationInputBlocker"), nullptr, false);
	Place(MotionCanvas, AnimationBlocker, FVector2D(133.5, 39.5), FVector2D(1173, 730), 100);
}

void URokScoutScreenWidget::ShowDescription()
{
	if (!bOpen || WindowAnimation.IsClosing() || !Description || !List) return;
	bDescription = true; PanelSeconds = 0;
	Description->SetVisibility(ESlateVisibility::Visible); List->SetVisibility(ESlateVisibility::Collapsed);
	BackButton->SetVisibility(ESlateVisibility::Visible); ApplyAnimation();
}

void URokScoutScreenWidget::ShowList()
{
	if (!bOpen || WindowAnimation.IsClosing() || !Description || !List) return;
	bDescription = false; PanelSeconds = 0;
	Description->SetVisibility(ESlateVisibility::Collapsed); List->SetVisibility(ESlateVisibility::Visible);
	BackButton->SetVisibility(ESlateVisibility::Collapsed); ApplyAnimation();
}

void URokScoutScreenWidget::RequestClose()
{
	if (!bOpen || WindowAnimation.IsClosing()) return;
	WindowAnimation.Close(); ApplyAnimation();
}

void URokScoutScreenWidget::CloseImmediately()
{
	if (!bOpen) return;
	bOpen = false; bDescription = false; WindowAnimation.Reset();
	bPendingKeyboardFocus = false;
	SetVisibility(ESlateVisibility::Collapsed);
	if (RootCanvas) RootCanvas->ClearChildren();
	MotionCanvas = nullptr; List = nullptr; Description = nullptr; BackButton = nullptr; AnimationBlocker = nullptr;
	if (GetOwningPlayer() && FSlateApplication::IsInitialized())
	{
		auto Previous = PreviousKeyboardFocus.Pin();
		if (Previous.IsValid() && Previous->GetVisibility().IsVisible() && Previous->IsEnabled()
			&& FSlateApplication::Get().FindWidgetWindow(Previous.ToSharedRef()).IsValid())
		{
			FSlateApplication::Get().SetKeyboardFocus(Previous, EFocusCause::SetDirectly);
		}
		else FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
	PreviousKeyboardFocus.Reset();
	OnClosed.Broadcast();
}

void URokScoutScreenWidget::Advance(float Seconds)
{
	if (!bOpen) return;
	if (WindowAnimation.Advance(Seconds)) { CloseImmediately(); return; }
	PanelSeconds = FMath::Min(0.25f, PanelSeconds + FMath::Max(0.0f, Seconds));
	ApplyAnimation();
}

void URokScoutScreenWidget::ApplyAnimation()
{
	if (!MotionCanvas) return;
	const auto Window = WindowAnimation.GetSample();
	MotionCanvas->SetRenderScale(FVector2D(Window.Scale)); MotionCanvas->SetRenderOpacity(Window.Opacity);
	AnimationBlocker->SetVisibility(WindowAnimation.IsAnimating() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// Exact same source LevelShow/ContentShow clips as ArmyShow, verified per-prefab.
	const auto Panel = RokArmyShowPanelAnimation::Evaluate(PanelSeconds, bDescription);
	UWidget* Target = bDescription ? static_cast<UWidget*>(Description.Get()) : static_cast<UWidget*>(List.Get());
	Target->SetRenderTranslation(FVector2D(Panel.X, 0)); Target->SetRenderOpacity(Panel.Opacity);
}

void URokScoutScreenWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds); AcquireKeyboardFocus(); Advance(DeltaSeconds);
}

void URokScoutScreenWidget::AcquireKeyboardFocus()
{
	if (!bOpen || !bPendingKeyboardFocus || !FSlateApplication::IsInitialized()) return;
	const auto SlateWidget = GetCachedWidget();
	// Open may be called while the HUD is still constructing. Retry once attached.
	if (SlateWidget.IsValid() && FSlateApplication::Get().FindWidgetWindow(SlateWidget.ToSharedRef()).IsValid())
	{
		SetKeyboardFocus(); bPendingKeyboardFocus = false;
	}
}

FReply URokScoutScreenWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bOpen && Event.GetKey() == EKeys::Escape) { RequestClose(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}

void URokScoutScreenWidget::NativeDestruct()
{
	bOpen = false; bPendingKeyboardFocus = false; PreviousKeyboardFocus.Reset(); WindowAnimation.Reset(); Super::NativeDestruct();
}
