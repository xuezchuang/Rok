#include "RokHospitalExplanationWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "RokDataPaths.h"

TSubclassOf<URokHospitalExplanationWidget> URokHospitalExplanationWidget::ResolvePresentationClass()
{
	UClass* ResolvedClass = LoadClass<URokHospitalExplanationWidget>(
		nullptr, TEXT("/Game/RokUI/Hospital/WBP_RokHospitalExplanation.WBP_RokHospitalExplanation_C"));
	return ResolvedClass ? ResolvedClass : StaticClass();
}

bool URokHospitalExplanationWidget::HasExactSourcePaths(const TArray<FString>& Paths)
{
    const TSet<FString> Required={
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIHospital/UI_Win_Hospital.prefab"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Hospital/HospitalMediator.cs"),
        TEXT("ref/config-tables/s_Language.xlsm"),
        TEXT("ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin"),
        TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/UIExtend/ArabicSupport.cs"),
        TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs"),
        TEXT("ref/client-unity/Assets/Scripts/Client/Utils/LanguageUtils.cs")};
    if(Paths.Num()!=Required.Num())return false;
    TSet<FString> Seen;
    for(const FString& Path:Paths){if(!Required.Contains(Path)||Seen.Contains(Path))return false;Seen.Add(Path);}
    return Seen.Num()==Required.Num();
}

bool URokHospitalExplanationWidget::InitializeExplanation()
{
    LoadError.Empty();FString Json;TSharedPtr<FJsonObject> Data;
    auto Fail=[this](const TCHAR* Message){LoadError=Message;return false;};
    if(!FFileHelper::LoadFileToString(Json,*(RokDataPaths::Resolve(TEXT("hospital_explanation.json"))))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data)||!Data.IsValid())return Fail(TEXT("Missing source hospital explanation"));
    FString Schema,Raw,Rich;double Id;
    const TArray<TSharedPtr<FJsonValue>>* Sources=nullptr;
    if(!Data->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.hospital.explanation.v1")||!Data->TryGetNumberField(TEXT("language_id"),Id)||Id!=181001||!Data->TryGetStringField(TEXT("source_text"),Raw)||!Data->TryGetStringField(TEXT("rich_text"),Rich)||Raw.IsEmpty()||Raw.Len()>10000||!Data->TryGetArrayField(TEXT("sources"),Sources))return Fail(TEXT("Invalid hospital source identity"));
    if(Raw.Replace(TEXT("<color=yellow>"),TEXT("<yellow>")).Replace(TEXT("</color>"),TEXT("</>"))!=Rich)return Fail(TEXT("Hospital rich text differs from source"));
    TArray<FString> SourcePaths;
    for(const auto& Value:*Sources)
    {
        FString Path;
        if(!Value.IsValid()||Value->Type!=EJson::Object||!Value->AsObject()->TryGetStringField(TEXT("path"),Path))return Fail(TEXT("Invalid hospital provenance path"));
        SourcePaths.Add(Path);
    }
    if(!HasExactSourcePaths(SourcePaths))return Fail(TEXT("Missing, duplicate or unexpected hospital source"));
    // The recorded ref/ inputs do not ship; the generator and Verify* scripts check their hashes.
    for(const auto& Value:*Sources)
    {
        if(!Value.IsValid()||Value->Type!=EJson::Object)return Fail(TEXT("Invalid hospital provenance"));
        const auto S=Value->AsObject();FString Path,Hash;
        if(!S->TryGetStringField(TEXT("path"),Path)||!Path.StartsWith(TEXT("ref/"))||Path.Contains(TEXT(".."))||!S->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64)return Fail(TEXT("Invalid hospital source provenance"));
    }
    const TSharedPtr<FJsonObject>* Nodes=nullptr;if(!Data->TryGetObjectField(TEXT("nodes"),Nodes))return Fail(TEXT("Hospital source layout missing"));
    const FString Page=TEXT("Rect/Right/pl_explain"),Label=Page/TEXT("sv_explain_view/v_explain_view/c_explain_view/lbl_explain");
    auto Rect=[&](const FString& Key,TArray<double>& Result){const TSharedPtr<FJsonObject>* Node=nullptr;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!(*Nodes)->TryGetObjectField(Key,Node)||!(*Node)->TryGetArrayField(TEXT("rect"),Values)||Values->Num()!=4)return false;
        for(const auto& V:*Values){double N;if(!V.IsValid()||!V->TryGetNumber(N)||!FMath::IsFinite(N)||FMath::Abs(N)>10000)return false;Result.Add(N);}return Result[2]>0&&Result[3]>0;};
    TArray<double> P,L;if(!Rect(Page,P)||!Rect(Label,L))return Fail(TEXT("Invalid hospital source rectangle"));
    Position=FVector2D(P[0],P[1]);Size=FVector2D(P[2],P[3]);SourceText=FText::FromString(Raw);

    const bool bWidgetBlueprint = GetClass() != URokHospitalExplanationWidget::StaticClass();
    if (bWidgetBlueprint)
    {
        // The generated WBP owns the editable viewport, source-derived geometry,
        // and base text style; native code supplies runtime content and yellow markup.
        HospitalExplanationScroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("HospitalExplanationScroll")));
        HospitalExplanationInset = Cast<UBorder>(GetWidgetFromName(TEXT("HospitalExplanationInset")));
        HospitalExplanationTextWidth = Cast<USizeBox>(GetWidgetFromName(TEXT("HospitalExplanationTextWidth")));
        HospitalExplanationText = Cast<URichTextBlock>(GetWidgetFromName(TEXT("HospitalExplanationText")));
        if (!HospitalExplanationScroll || !HospitalExplanationInset
            || !HospitalExplanationTextWidth || !HospitalExplanationText)
        {
            return Fail(TEXT("Hospital explanation WBP is missing a required named widget"));
        }
    }
    else
    {
        // Keep the native-only tree as a fallback for automation and for callers
        // which have not yet selected the generated WBP class.
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HospitalExplanationRoot"));
        WidgetTree->RootWidget = Root;
        HospitalExplanationScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("HospitalExplanationScroll"));
        UCanvasPanelSlot* ScrollSlot = Root->AddChildToCanvas(HospitalExplanationScroll);
        ScrollSlot->SetPosition(FVector2D::ZeroVector);
        ScrollSlot->SetSize(Size);
        HospitalExplanationInset = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("HospitalExplanationInset"));
        HospitalExplanationInset->SetBrushColor(FLinearColor::Transparent);
        HospitalExplanationInset->SetHorizontalAlignment(HAlign_Left);
        HospitalExplanationTextWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("HospitalExplanationTextWidth"));
        HospitalExplanationText = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(),TEXT("HospitalExplanationText"));
        HospitalExplanationInset->SetContent(HospitalExplanationTextWidth);
        HospitalExplanationTextWidth->SetContent(HospitalExplanationText);
        HospitalExplanationScroll->AddChild(HospitalExplanationInset);
    }

    URichTextBlock* Text = HospitalExplanationText;
    if (!bWidgetBlueprint)
    {
        // Legacy fallback only; the WBP serializes these source-derived values so
        // Designer edits are retained when its presentation class is selected.
        UScrollBox* Scroll = HospitalExplanationScroll;
        Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
        Scroll->SetAnimateWheelScrolling(false);
        FScrollBoxStyle ScrollStyle = Scroll->GetWidgetStyle();
        ScrollStyle.SetTopShadowBrush(FSlateNoResource()).SetBottomShadowBrush(FSlateNoResource());
        Scroll->SetWidgetStyle(ScrollStyle);
        FTextBlockStyle DefaultStyle;
        DefaultStyle.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
        DefaultStyle.SetColorAndOpacity(FLinearColor::White);
        DefaultStyle.SetShadowOffset(FVector2D::ZeroVector);
        Text->SetDefaultTextStyle(DefaultStyle);
        Text->SetJustification(ETextJustify::Left);
        Text->SetWrapTextAt(static_cast<float>(L[2]));
        Text->SetAutoWrapText(true);
        Text->SetLineHeightPercentage(1.25f);
        FShapedTextOptions Options;
        Options.bOverride_TextFlowDirection = true;
        Options.TextFlowDirection = ETextFlowDirection::LeftToRight;
        Text->SetShapedTextOptions(Options);
        HospitalExplanationTextWidth->SetWidthOverride(static_cast<float>(L[2]));
        HospitalExplanationInset->SetBrushColor(FLinearColor::Transparent);
        HospitalExplanationInset->SetPadding(FMargin(static_cast<float>(L[0] - P[0]), 0, 0, 0));
        HospitalExplanationInset->SetHorizontalAlignment(HAlign_Left);
    }

    // The WBP authors the base style; the runtime table includes its Default row
    // plus the yellow style required by the source markup.
    const FStructProperty* DefaultStyleProperty = FindFProperty<FStructProperty>(
        Text->GetClass(), TEXT("DefaultTextStyleOverride"));
    const FTextBlockStyle* AuthoredDefaultStyle = DefaultStyleProperty
        ? DefaultStyleProperty->ContainerPtrToValuePtr<FTextBlockStyle>(Text)
        : nullptr;
    if (!AuthoredDefaultStyle) return Fail(TEXT("Hospital RichTextBlock default style is unavailable"));
    auto* Styles = NewObject<UDataTable>(Text);
    Styles->RowStruct = FRichTextStyleRow::StaticStruct();
    FRichTextStyleRow Default;
    Default.TextStyle = *AuthoredDefaultStyle;
    Styles->AddRow(TEXT("Default"), Default);
    FRichTextStyleRow Yellow;
    Yellow.TextStyle = Default.TextStyle;
    Yellow.TextStyle.SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor(255, 255, 0)));
    Styles->AddRow(TEXT("yellow"), Yellow);
    Text->SetTextStyleSet(Styles);
    Text->SetText(FText::FromString(Rich));
    SetVisibility(ESlateVisibility::Collapsed);return true;
}
FText URokHospitalExplanationWidget::GetRichText() const{return HospitalExplanationText?HospitalExplanationText->GetText():FText::GetEmpty();}
float URokHospitalExplanationWidget::GetScrollOffset() const{return HospitalExplanationScroll?HospitalExplanationScroll->GetScrollOffset():0;}
