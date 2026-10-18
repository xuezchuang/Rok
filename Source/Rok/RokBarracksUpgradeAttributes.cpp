#include "RokBarracksUpgradeAttributes.h"
#include "RokBarracksSourceTextData.h"
#include "RokBarracksSourceTextWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RokDataPaths.h"
#include "RokModalNineSlice.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool URokBarracksUpgradeAttributes::InitializeAttributes()
{
    AttributeCount=0;LoadError.Empty();FString Json;TSharedPtr<FJsonObject> Data;
    auto Fail=[this](const TCHAR* Message){LoadError=Message;return false;};
    if(!FFileHelper::LoadFileToString(Json,*(RokDataPaths::Resolve(TEXT("barracks_upgrade_attribute.json")))) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data)||!Data.IsValid())return Fail(TEXT("Missing barracks attribute source"));
    FString Schema;double Type,Before,After,RowHeight,Height;
    const TArray<TSharedPtr<FJsonValue>> *Rows=nullptr,*Sources=nullptr;
    if(!Data->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.barracks.upgrade_attribute.v1") ||
       !Data->TryGetNumberField(TEXT("building_type"),Type)||Type!=9 || !Data->TryGetNumberField(TEXT("current_level"),Before)||Before!=1 ||
       !Data->TryGetNumberField(TEXT("next_level"),After)||After!=2 || !Data->TryGetNumberField(TEXT("row_height"),RowHeight)||RowHeight!=40 ||
       !Data->TryGetNumberField(TEXT("attribute_height"),Height)||Height!=110 || !Data->TryGetArrayField(TEXT("rows"),Rows)||Rows->Num()!=2 ||
       !Data->TryGetArrayField(TEXT("sources"),Sources))return Fail(TEXT("Invalid bounded barracks attribute identity"));
    const TSet<FString> Required={
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Item_Buildinglevelup.prefab"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/BuildingDesc/UI_Win_BuildingUpdateMediator.cs"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_build/UIBuildingDesc/UI_Win_BuildingUpdate.prefab"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UIPrefabs/ui_prefabs_common/UICommon/UI_Model_AttrItem.prefab"),
        TEXT("ref/client-unity/Assets/StreamingAssets/Config/Bin/Language.bin"),
        TEXT("ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv"),
        TEXT("ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv"),
        TEXT("Scripts/BuildRokBarracksUpgradeAttribute.py"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png.meta"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.png"),
        TEXT("ref/client-unity/Assets/Scripts/Client/Compoment/ArabLayout/ArabLayoutCompment.cs")};
    if(Sources->Num()!=Required.Num())return Fail(TEXT("Incomplete barracks provenance"));
    // Recorded provenance must be complete. Its ref/ and Scripts/ inputs do not ship; the generator and
    // VerifyRokBarracksUpgradeAttributeConsumer.py check their hashes.
    TSet<FString> Seen;
    for(const auto& V:*Sources)
    {
        FString Path,Hash;
        if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid barracks source row"));
        const auto S=V->AsObject();
        if(!S->TryGetStringField(TEXT("path"),Path)||!Required.Contains(Path)||Seen.Contains(Path)||!S->TryGetStringField(TEXT("sha256"),Hash)||Hash.Len()!=64)
            return Fail(TEXT("Invalid barracks source provenance"));
        Seen.Add(Path);
    }
    TArray<FString> Labels,Values;
    const int32 Ids[]={180329,180314},Current[]={20,5},Delta[]={30,5};
    for(int32 I=0;I<2;++I)
    {
        const auto& V=(*Rows)[I];if(!V.IsValid()||V->Type!=EJson::Object)return Fail(TEXT("Invalid barracks attribute row"));
        const auto R=V->AsObject();FString Label,Value;double Id,C,D;bool Separator;
        if(!R->TryGetNumberField(TEXT("label_id"),Id)||Id!=Ids[I]||!R->TryGetNumberField(TEXT("current"),C)||C!=Current[I]||
           !R->TryGetNumberField(TEXT("delta"),D)||D!=Delta[I]||!R->TryGetStringField(TEXT("label"),Label)||Label.IsEmpty()||Label.Len()>100||
           !R->TryGetStringField(TEXT("value"),Value)||Value!=FString::Printf(TEXT("%d + <color=green>%d</color>"),Current[I],Delta[I])||
           !R->TryGetBoolField(TEXT("separator"),Separator)||Separator!=(I==0))return Fail(TEXT("Barracks attribute differs from source contract"));
        Labels.Add(Label);Values.Add(Value);
    }
    auto* Atlas=LoadObject<UTexture2D>(nullptr,TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common"));
    if(!Atlas)return Fail(TEXT("Missing source common atlas"));
    auto* GlyphData=NewObject<URokBarracksSourceTextData>(this);
    if(!GlyphData->Load(LoadError))return false;
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("BarracksUpgradeAttributeRoot"));WidgetTree->RootWidget=Canvas;
    auto Place=[Canvas](UWidget* Widget,FVector2D Position,FVector2D Size,int32 Z)
    {auto* CanvasSlot=Canvas->AddChildToCanvas(Widget);CanvasSlot->SetPosition(Position);CanvasSlot->SetSize(Size);CanvasSlot->SetZOrder(Z);};
    auto Image=[&](FName Name,FIntRect Rect,FVector2D Position,FVector2D Size,int32 Z)
    {
        auto* W=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),Name);FSlateBrush B;B.SetResourceObject(Atlas);B.DrawAs=ESlateBrushDrawType::Image;B.ImageSize=Size;
        B.SetUVRegion(FBox2f(FVector2f(Rect.Min.X/2048.f,1.f-Rect.Max.Y/1024.f),FVector2f(Rect.Max.X/2048.f,1.f-Rect.Min.Y/1024.f)));
        W->SetBrush(B);W->SetVisibility(ESlateVisibility::HitTestInvisible);Place(W,Position,Size,Z);return W;
    };
    // pl_attr uses Unity sliced pl_com_1001; fixed borders in sprite pixels.
    for(int32 I=0;I<9;++I)
    {
        const auto P=RokModalNineSlice::Patch(FIntRect(436,165,499,220),FMargin(16.f/63,13.f/55,15.f/63,13.f/55),FVector2D(536,110),I);
        if(P.IsVisible())Image(*FString::Printf(TEXT("BarracksUpgradeAttributeBackground_%d"),I),P.Source,P.Position,P.Size,0);
    }
    for(int32 I=0;I<2;++I)
    {
        // Source Chinese Arab layout: label left, combined rich value right; Middle alignment.
        const double Y=22.5+40*I;
        auto* Label=WidgetTree->ConstructWidget<URokBarracksSourceTextWidget>(URokBarracksSourceTextWidget::StaticClass(),*FString::Printf(TEXT("UpgradeStatLabel_%d"),I));
        Label->Configure(GlyphData,20,false,3,FVector2D(275.76,35),FLinearColor::White);
        auto* Value=WidgetTree->ConstructWidget<URokBarracksSourceTextWidget>(URokBarracksSourceTextWidget::StaticClass(),*FString::Printf(TEXT("UpgradeStatCurrent_%d"),I));
        Value->Configure(GlyphData,20,false,5,FVector2D(400,35),FLinearColor::White);
        // Source Color32 already carries beige/current and green/increment; white avoids double tint.
        if(!Label->SetText(FText::FromString(Labels[I])) || !Value->SetText(FText::FromString(Values[I])))
            return Fail(TEXT("Missing exact barracks attribute source glyph mesh"));
        for(int32 Column=0;Column<2;++Column)
        {
            auto* Center=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),*FString::Printf(TEXT("BarracksUpgradeTextRegion_%d_%d"),I,Column));
            Center->SetBrushColor(FLinearColor::Transparent);Center->SetPadding(FMargin(0));Center->SetVerticalAlignment(VAlign_Center);Center->SetContent(Column==0?static_cast<UWidget*>(Label):static_cast<UWidget*>(Value));
            Place(Center,FVector2D(Column==0?30.4:105.03,Y),FVector2D(Column==0?275.76:400,35),2);
        }
        auto* Line=Image(*FString::Printf(TEXT("UpgradeStatSeparator_%d"),I),FIntRect(1,611,571,613),FVector2D(0,57+40*I),FVector2D(536,3),1);
        if(I==1)Line->SetVisibility(ESlateVisibility::Collapsed);
    }
    AttributeCount=2;SetVisibility(ESlateVisibility::HitTestInvisible);return true;
}
