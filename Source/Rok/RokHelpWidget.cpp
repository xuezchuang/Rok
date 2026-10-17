#include "RokHelpWidget.h"
#include "RokHelpSourceTextData.h"
#include "RokHelpSourceTextWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"

namespace
{
void Place(UCanvasPanel* P,UWidget* W,FVector2D XY,FVector2D Size,int32 Z=1)
{ auto* S=P->AddChildToCanvas(W);S->SetPosition(XY);S->SetSize(Size);S->SetZOrder(Z); }
FVector2D Pos(const TSharedPtr<FJsonObject>& N) { const auto& A=N->GetArrayField(TEXT("rect"));return FVector2D(A[0]->AsNumber(),A[1]->AsNumber()); }
FVector2D Size(const TSharedPtr<FJsonObject>& N) { const auto& A=N->GetArrayField(TEXT("rect"));return FVector2D(A[2]->AsNumber(),A[3]->AsNumber()); }
FName Name(const FString& Prefix,const FString& Path) { return FName(*(Prefix+Path.Replace(TEXT("/"),TEXT("_")))); }
}

bool URokHelpWidget::LoadCatalog()
{
    Entries.Reset();Groups.Reset();Layouts.Reset();LoadError.Empty();
    FString Json;TSharedPtr<FJsonObject> Data;
    if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/help_catalog.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Data) || !Data.IsValid())
    { LoadError=TEXT("帮助目录未加载，请重新生成源配置。");return false; }
    FString Schema;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;const TSharedPtr<FJsonObject>* ParsedLayouts=nullptr;
    if(!Data->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("rok.help.v1")||!Data->TryGetArrayField(TEXT("entries"),Rows)||Rows->Num()!=70||!Data->TryGetObjectField(TEXT("layouts"),ParsedLayouts))
    { LoadError=TEXT("帮助目录结构不匹配。");return false; }
    TSet<int32> Ids;
    for(const auto& V:*Rows)
    {
        const auto R=V->AsObject();FEntry E;double Id=0,Group=0,SourceGroup=0;
        if(!R.IsValid()||!R->TryGetNumberField(TEXT("id"),Id)||!R->TryGetNumberField(TEXT("display_group_id"),Group)||!R->TryGetNumberField(TEXT("group_id"),SourceGroup)||!FMath::IsFinite(SourceGroup)||SourceGroup<1||SourceGroup>MAX_int32||SourceGroup!=FMath::FloorToDouble(SourceGroup)||!FMath::IsFinite(Id)||!FMath::IsFinite(Group)||Id<1||Id>MAX_int32||Group<1||Group>MAX_int32||Id!=FMath::FloorToDouble(Id)||Group!=FMath::FloorToDouble(Group)||!R->TryGetStringField(TEXT("group"),E.Group)||!R->TryGetStringField(TEXT("question"),E.Question)||!R->TryGetStringField(TEXT("answer"),E.Answer)||E.Group.IsEmpty()||E.Question.IsEmpty()||E.Answer.IsEmpty())
        { LoadError=TEXT("帮助目录条目损坏。");Entries.Reset();return false; }
        E.Id=static_cast<int32>(Id);E.GroupId=static_cast<int32>(Group);E.SourceGroupId=static_cast<int32>(SourceGroup);
        if(Ids.Contains(E.Id)){LoadError=TEXT("帮助目录编号重复。");Entries.Reset();return false;}
        Ids.Add(E.Id);Groups.AddUnique(E.GroupId);Entries.Add(MoveTemp(E));
    }
    if(Groups.Num()!=7){LoadError=TEXT("帮助分类数量不匹配。");Entries.Reset();return false;}
    Layouts=*ParsedLayouts;
    // Reject malformed geometry before any AsNumber indexing or atlas access below.
    for(const TCHAR* Section:{TEXT("window"),TEXT("frame"),TEXT("group"),TEXT("question")})
    {
        const TSharedPtr<FJsonObject>* SectionData=nullptr;
        if(!Layouts->TryGetObjectField(Section,SectionData)){LoadError=TEXT("帮助布局缺失。");return false;}
        for(const auto& Pair:(*SectionData)->Values)
        {
            const auto N=Pair.Value->AsObject();const TArray<TSharedPtr<FJsonValue>>* Rect=nullptr;
            if(!N.IsValid()||!N->TryGetArrayField(TEXT("rect"),Rect)||Rect->Num()!=4){LoadError=TEXT("帮助矩形损坏。");return false;}
            for(const auto& X:*Rect){double F=0;if(!X->TryGetNumber(F)||!FMath::IsFinite(F)||FMath::Abs(F)>10000){LoadError=TEXT("帮助坐标无效。");return false;}}
            if((*Rect)[2]->AsNumber()<0||(*Rect)[3]->AsNumber()<0){LoadError=TEXT("帮助尺寸无效。");return false;}
            auto Numbers=[&](const TCHAR* Key,int32 Count,double Min,double Max)
            {const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(!N->TryGetArrayField(Key,Values)||Values->Num()!=Count)return false;for(const auto& V:*Values){double Number=0;if(!V->TryGetNumber(Number)||!FMath::IsFinite(Number)||Number<Min||Number>Max)return false;}return true;};
            FString Atlas;double Font=0;bool Flag=false;
            if(N->TryGetStringField(TEXT("atlas"),Atlas))
            {
                if((Atlas!=TEXT("common")&&Atlas!=TEXT("mail")&&Atlas!=TEXT("frame")&&Atlas!=TEXT("setting"))||!Numbers(TEXT("rgba"),4,0,1)||!Numbers(TEXT("uv"),4,0,16384)||!Numbers(TEXT("atlas_size"),2,1,16384)||!N->TryGetBoolField(TEXT("flip_x"),Flag)||!N->TryGetBoolField(TEXT("flip_y"),Flag))
                {LoadError=TEXT("帮助图集信息无效。");return false;}
                const auto& UV=N->GetArrayField(TEXT("uv"));const auto& AS=N->GetArrayField(TEXT("atlas_size"));
                if(UV[2]->AsNumber()<=0||UV[3]->AsNumber()<=0||UV[0]->AsNumber()+UV[2]->AsNumber()>AS[0]->AsNumber()||UV[1]->AsNumber()+UV[3]->AsNumber()>AS[1]->AsNumber())
                {LoadError=TEXT("帮助图集越界。");return false;}
            }
            if(N->HasField(TEXT("font_px")))
            {
                double Alignment=0;FString Text;
                if(!N->TryGetNumberField(TEXT("font_px"),Font)||!FMath::IsFinite(Font)||Font<1||Font>100||!N->TryGetNumberField(TEXT("alignment"),Alignment)||Alignment<0||Alignment>8||!Numbers(TEXT("rgba"),4,0,1)||!N->TryGetBoolField(TEXT("bold"),Flag)||!N->TryGetStringField(TEXT("text"),Text))
                {LoadError=TEXT("帮助文字样式无效。");return false;}
            }
        }
    }
    const TMap<FString,TArray<FString>> Required={
        {TEXT("frame"),{TEXT("win/frame1"),TEXT("win/frame2"),TEXT("win/frame3"),TEXT("win/frame4"),TEXT("title/btn_close"),TEXT("title/btn_back"),TEXT("title/lbl_title")}},
        {TEXT("window"),{TEXT("pl_list/btn_head"),TEXT("pl_list/sv_helperlist"),TEXT("pl_detial/line"),TEXT("pl_detial/lbl_question"),TEXT("pl_detial/sv_list"),TEXT("pl_detial/sv_list/v/c/lbl_detial")}},
        {TEXT("group"),{TEXT("btn_btn"),TEXT("btn_btn/lbl_title"),TEXT("btn_btn/img_arrowDown"),TEXT("btn_btn/img_arrowUp")}},
        {TEXT("question"),{TEXT("btn_btn"),TEXT("btn_btn/lbl_title"),TEXT("line")}}};
    for(const auto& Section:Required)for(const auto& Path:Section.Value)if(!Layouts->GetObjectField(Section.Key)->HasField(Path))
    {LoadError=TEXT("帮助必要节点缺失。");return false;}
    for(const auto& Pair:{TPair<FString,FString>(TEXT("window"),TEXT("pl_detial/lbl_question")),TPair<FString,FString>(TEXT("window"),TEXT("pl_detial/sv_list/v/c/lbl_detial")),TPair<FString,FString>(TEXT("group"),TEXT("btn_btn/lbl_title")),TPair<FString,FString>(TEXT("question"),TEXT("btn_btn/lbl_title"))})
        if(!Layouts->GetObjectField(Pair.Key)->GetObjectField(Pair.Value)->HasField(TEXT("font_px")))
        {LoadError=TEXT("帮助必要文本样式缺失。");return false;}
    for(int32 Index=1;Index<=4;++Index)if(!Layouts->GetObjectField(TEXT("frame"))->GetObjectField(FString::Printf(TEXT("win/frame%d"),Index))->HasField(TEXT("atlas")))
    {LoadError=TEXT("帮助框体图集缺失。");return false;}
    return true;
}

TSharedPtr<FJsonObject> URokHelpWidget::Node(const TCHAR* Section,const FString& Path) const
{ return Layouts->GetObjectField(Section)->GetObjectField(Path); }

UWidget* URokHelpWidget::DrawNode(UCanvasPanel* Parent,const FName& WidgetName,const TSharedPtr<FJsonObject>& N,FVector2D Offset,int32 Z,const FString* Override)
{
    const FVector2D XY=Pos(N)+Offset, Dimensions=Size(N);FString Atlas;
    if(N->TryGetStringField(TEXT("atlas"),Atlas))
    {
        FString Asset;
        if(Atlas==TEXT("common"))Asset=TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
        else if(Atlas==TEXT("mail"))Asset=TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail");
        else if(Atlas==TEXT("frame")||Atlas==TEXT("setting"))Asset=FString::Printf(TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_%s.rok_modal_atlas_%s"),*Atlas,*Atlas);
        UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,*Asset);
        auto* Image=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),WidgetName);
        FSlateBrush Brush;Brush.SetResourceObject(Texture);Brush.ImageSize=Dimensions;Brush.DrawAs=ESlateBrushDrawType::Image;
        const auto& UV=N->GetArrayField(TEXT("uv"));const auto& AS=N->GetArrayField(TEXT("atlas_size"));
        const double W=AS[0]->AsNumber(),H=AS[1]->AsNumber();
        Brush.SetUVRegion(FBox2D(FVector2D(UV[0]->AsNumber()/W,1-(UV[1]->AsNumber()+UV[3]->AsNumber())/H),FVector2D((UV[0]->AsNumber()+UV[2]->AsNumber())/W,1-UV[1]->AsNumber()/H)));
        Image->SetBrush(Brush);
        const auto& Tint=N->GetArrayField(TEXT("rgba"));Image->SetColorAndOpacity(FLinearColor::FromSRGBColor(FColor(FMath::RoundToInt(Tint[0]->AsNumber()*255),FMath::RoundToInt(Tint[1]->AsNumber()*255),FMath::RoundToInt(Tint[2]->AsNumber()*255),FMath::RoundToInt(Tint[3]->AsNumber()*255))));
        Image->SetRenderTransformPivot(FVector2D(.5,.5));Image->SetVisibility(ESlateVisibility::HitTestInvisible);
        Image->SetRenderScale(FVector2D(N->GetBoolField(TEXT("flip_x"))?-1:1,N->GetBoolField(TEXT("flip_y"))?-1:1));
        if(!Texture){LoadError=FString::Printf(TEXT("帮助图集缺失：%s"),*Atlas);Image->SetVisibility(ESlateVisibility::Collapsed);}
        Place(Parent,Image,XY,Dimensions,Z);return Image;
    }
    double FontPx=0;
    if(!N->TryGetNumberField(TEXT("font_px"),FontPx))return nullptr;
    auto* Text=WidgetTree->ConstructWidget<URokHelpSourceTextWidget>(URokHelpSourceTextWidget::StaticClass(),WidgetName);
    const auto& C=N->GetArrayField(TEXT("rgba"));
    const FLinearColor Tint=FLinearColor::FromSRGBColor(FColor(FMath::RoundToInt(C[0]->AsNumber()*255),FMath::RoundToInt(C[1]->AsNumber()*255),FMath::RoundToInt(C[2]->AsNumber()*255),FMath::RoundToInt(C[3]->AsNumber()*255)));
    const int32 Alignment=N->GetIntegerField(TEXT("alignment"));
    Text->Configure(SourceTextData,static_cast<int32>(FontPx),N->GetBoolField(TEXT("bold")),Alignment,Dimensions,Tint);
    const bool DeferredDetail=WidgetName==TEXT("HelpQuestionText")||WidgetName==TEXT("HelpAnswerText");
    if(!Text->SetText(DeferredDetail?FText::GetEmpty():FText::FromString(Override?*Override:N->GetStringField(TEXT("text")))))
    {LoadError=FString::Printf(TEXT("Missing exact source glyph text: %s"),*WidgetName.ToString());UE_LOG(LogTemp,Error,TEXT("%s"),*LoadError);}
    auto* Region=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name(WidgetName.ToString(),TEXT("Region")));
    Region->SetBrushColor(FLinearColor::Transparent);Region->SetPadding(FMargin(0));Region->SetHorizontalAlignment(HAlign_Fill);Region->SetVerticalAlignment(VAlign_Fill);Region->SetContent(Text);Region->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Parent,Region,XY,Dimensions,Z);return Text;
}

void URokHelpWidget::AddHit(UCanvasPanel* Parent,const FName& WidgetName,int32 Action,const TSharedPtr<FJsonObject>& N,FVector2D Offset,int32 Z)
{
    auto* Hit=WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(),WidgetName);
    FSlateBrush Empty;Empty.DrawAs=ESlateBrushDrawType::NoDrawType;FButtonStyle Style;Style.SetNormal(Empty).SetHovered(Empty).SetPressed(Empty).SetDisabled(Empty);Hit->SetStyle(Style);
    Hit->ConfigureAction(Action);Hit->OnIndexedClicked.AddUObject(this,&URokHelpWidget::HandleAction);Place(Parent,Hit,Pos(N)+Offset,Size(N),Z);
}

bool URokHelpWidget::InitializeHelp()
{ ResetHelp();bool Loaded=LoadCatalog();if(Loaded){SourceTextData=NewObject<URokHelpSourceTextData>(this);Loaded=SourceTextData->Load(LoadError);}Build();return Loaded&&LoadError.IsEmpty(); }
void URokHelpWidget::ResetHelp()
{ Expanded.Reset();VisibleQuestionIds.Reset();SelectedId=0;ListOffset=0;bAcceptActions=true;if(Root)Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
void URokHelpWidget::SuspendInput()
{ bAcceptActions=false;if(Root)Root->SetVisibility(ESlateVisibility::HitTestInvisible); }
float URokHelpWidget::GetListScrollOffset() const { return ListScroll?ListScroll->GetScrollOffset():0; }

void URokHelpWidget::Build()
{
    if(!WidgetTree)return;
    if(!Root){Root=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("HelpRoot"));WidgetTree->RootWidget=Root;}else Root->ClearChildren();
    if(!LoadError.IsEmpty())
    {auto* Error=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("HelpLoadError"));Error->SetText(FText::FromString(LoadError));Place(Root,Error,FVector2D(300,300),FVector2D(800,100));return;}
    const FVector2D FrameOffset(146,28), WindowOffset(138,11.5);
    // Source Type3 win children: frame1, frame2, frame3, frame4. Explicit layers
    // preserve the header/body overlap independently of JSON/TMap iteration order.
    for(int32 Index=1;Index<=4;++Index)
    {const FString Path=FString::Printf(TEXT("win/frame%d"),Index);DrawNode(Root,Name(TEXT("HelpFrame_"),Path),Node(TEXT("frame"),Path),FrameOffset,Index<=2?1:2);}
    DrawNode(Root,TEXT("HelpFrame_title_lbl_title"),Node(TEXT("frame"),TEXT("title/lbl_title")),FrameOffset,4);
    for(const auto& Pair:{TPair<FString,int32>(TEXT("title/btn_close"),-1),TPair<FString,int32>(TEXT("title/btn_back"),-2)})
    {DrawNode(Root,Pair.Value==-1?TEXT("HelpCloseArt"):TEXT("HelpBackArt"),Node(TEXT("frame"),Pair.Key),FrameOffset,5);AddHit(Root,Pair.Value==-1?TEXT("HelpCloseButton"):TEXT("HelpBackButton"),Pair.Value,Node(TEXT("frame"),Pair.Key),FrameOffset);}
    ListPage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("HelpListPage"));Place(Root,ListPage,FVector2D::ZeroVector,FVector2D(1440,810),3);
    DetailPage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("HelpDetailPage"));Place(Root,DetailPage,FVector2D::ZeroVector,FVector2D(1440,810),3);
    for(const auto& P:Layouts->GetObjectField(TEXT("window"))->Values)
    {const FString Path(*P.Key);if(Path.StartsWith(TEXT("pl_list/"))&&!Path.Contains(TEXT("sv_helperlist")))DrawNode(ListPage,Name(TEXT("HelpList_"),Path),P.Value->AsObject(),WindowOffset);}
    AddHit(ListPage,TEXT("HelpCustomerServiceButton"),-3,Node(TEXT("window"),TEXT("pl_list/btn_head")),WindowOffset);
    ListScroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("HelpListScroll"));ListScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);ListScroll->SetAnimateWheelScrolling(false);
    const auto ScrollNode=Node(TEXT("window"),TEXT("pl_list/sv_helperlist"));Place(ListPage,ListScroll,Pos(ScrollNode)+WindowOffset,Size(ScrollNode));
    DrawNode(DetailPage,TEXT("HelpDetailDivider"),Node(TEXT("window"),TEXT("pl_detial/line")),WindowOffset);
    QuestionText=Cast<URokHelpSourceTextWidget>(DrawNode(DetailPage,TEXT("HelpQuestionText"),Node(TEXT("window"),TEXT("pl_detial/lbl_question")),WindowOffset));
    DetailScroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("HelpAnswerScroll"));DetailScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    const auto DetailNode=Node(TEXT("window"),TEXT("pl_detial/sv_list"));Place(DetailPage,DetailScroll,Pos(DetailNode)+WindowOffset,Size(DetailNode));
    auto* AnswerCanvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("HelpAnswerConstruction"));
    AnswerText=Cast<URokHelpSourceTextWidget>(DrawNode(AnswerCanvas,TEXT("HelpAnswerText"),Node(TEXT("window"),TEXT("pl_detial/sv_list/v/c/lbl_detial")),FVector2D::ZeroVector));
    AnswerText->RemoveFromParent();auto* AnswerSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("HelpAnswerWidth"));AnswerSize->SetWidthOverride(894.7f);AnswerSize->SetContent(AnswerText);auto* AnswerInset=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("HelpAnswerInset"));AnswerInset->SetBrushColor(FLinearColor::Transparent);AnswerInset->SetPadding(FMargin(10,0));AnswerInset->SetContent(AnswerSize);DetailScroll->AddChild(AnswerInset);
    RebuildList();HandleAction(-2);
}

void URokHelpWidget::RebuildList()
{
    ListScroll->ClearChildren();VisibleQuestionIds.Reset();float Y=0;
    auto* Content=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("HelpListContent"));
    for(int32 Group:Groups)
    {
        const auto* First=Entries.FindByPredicate([Group](const FEntry& E){return E.GroupId==Group;});
        auto* Row=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),*FString::Printf(TEXT("HelpGroupRow_%d"),Group));Place(Content,Row,FVector2D(0,Y),FVector2D(1074,43));
        DrawNode(Row,*FString::Printf(TEXT("HelpGroupPlate_%d"),Group),Node(TEXT("group"),TEXT("btn_btn")),FVector2D::ZeroVector);
        DrawNode(Row,*FString::Printf(TEXT("HelpGroupText_%d"),Group),Node(TEXT("group"),TEXT("btn_btn/lbl_title")),FVector2D::ZeroVector,2,&First->Group);
        DrawNode(Row,*FString::Printf(TEXT("HelpGroupArrow_%d"),Group),Node(TEXT("group"),Expanded.Contains(Group)?TEXT("btn_btn/img_arrowDown"):TEXT("btn_btn/img_arrowUp")),FVector2D::ZeroVector,3);
        AddHit(Row,*FString::Printf(TEXT("HelpGroupButton_%d"),Group),Group,Node(TEXT("group"),TEXT("btn_btn")),FVector2D::ZeroVector);Y+=43;
        if(!Expanded.Contains(Group))continue;
        for(const auto& E:Entries)if(E.GroupId==Group)
        {
            VisibleQuestionIds.Add(E.Id);auto* Q=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),*FString::Printf(TEXT("HelpQuestionRow_%d"),E.Id));Place(Content,Q,FVector2D(17,Y),FVector2D(1040,54));
            DrawNode(Q,*FString::Printf(TEXT("HelpQuestionLabel_%d"),E.Id),Node(TEXT("question"),TEXT("btn_btn/lbl_title")),FVector2D::ZeroVector,2,&E.Question);
            DrawNode(Q,*FString::Printf(TEXT("HelpQuestionLine_%d"),E.Id),Node(TEXT("question"),TEXT("line")),FVector2D::ZeroVector);
            AddHit(Q,*FString::Printf(TEXT("HelpQuestionButton_%d"),E.Id),E.Id,Node(TEXT("question"),TEXT("btn_btn")),FVector2D::ZeroVector);Y+=54;
        }
    }
    auto* Fixed=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("HelpListContentSize"));Fixed->SetWidthOverride(1074);Fixed->SetHeightOverride(Y);Fixed->SetContent(Content);ListScroll->AddChild(Fixed);ListScroll->SetScrollOffset(ListOffset);
}

void URokHelpWidget::HandleAction(int32 Action)
{
    if(!bAcceptActions)return;
    if(Action==-1){SuspendInput();OnCloseRequested.Broadcast();return;}
    if(Action==-3)
    {
        auto* Message=Cast<UTextBlock>(GetWidgetFromName(TEXT("HelpCustomerServiceBoundary")));
        if(!Message){Message=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("HelpCustomerServiceBoundary"));Message->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),14));Place(ListPage,Message,FVector2D(460,320),FVector2D(600,30),20);}
        Message->SetText(FText::FromString(TEXT("客服平台尚未接入，未打开外部页面。")));return;
    }
    if(Action==-2)
    {SelectedId=0;DetailPage->SetVisibility(ESlateVisibility::Collapsed);ListPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);GetWidgetFromName(TEXT("HelpBackButton"))->SetVisibility(ESlateVisibility::Collapsed);GetWidgetFromName(TEXT("HelpBackArt"))->SetVisibility(ESlateVisibility::Collapsed);return;}
    if(SelectedId)return;
    if(Groups.Contains(Action))
    {ListOffset=GetListScrollOffset();if(Expanded.Contains(Action))Expanded.Remove(Action);else Expanded.Add(Action);RebuildList();return;}
    const auto* E=Entries.FindByPredicate([Action](const FEntry& Entry){return Entry.Id==Action;});
    if(!E||!VisibleQuestionIds.Contains(Action))return;
    if(!QuestionText->SetText(FText::FromString(E->Question))||!AnswerText->SetText(FText::FromString(E->Answer)))
    {LoadError=TEXT("Missing exact source FAQ glyph mesh");UE_LOG(LogTemp,Error,TEXT("%s"),*LoadError);SuspendInput();return;}
    SelectedId=E->Id;
    ListPage->SetVisibility(ESlateVisibility::Collapsed);DetailPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    GetWidgetFromName(TEXT("HelpBackButton"))->SetVisibility(ESlateVisibility::Visible);GetWidgetFromName(TEXT("HelpBackArt"))->SetVisibility(ESlateVisibility::HitTestInvisible);
}
