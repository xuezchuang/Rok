#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokHelpWidget.generated.h"

class UCanvasPanel;
class UScrollBox;
class UBorder;
class USizeBox;
class UTextBlock;
class URokHelpSourceTextData;
class URokHelpSourceTextWidget;
class FJsonObject;
DECLARE_MULTICAST_DELEGATE(FRokHelpCloseRequested);

/** Source FAQ presentation. The hosting Feature screen owns its T1 animation and input mask. */
UCLASS()
class ROK_API URokHelpWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static TSubclassOf<URokHelpWidget> ResolvePresentationClass();
    UWidget* GetWidgetFromName(const FName& WidgetName) const;
    bool InitializeHelp();
    void ResetHelp();
    void SuspendInput();
    FRokHelpCloseRequested OnCloseRequested;
    int32 GetSelectedQuestionId() const { return SelectedId; }
    const TSet<int32>& GetExpandedGroups() const { return Expanded; }
    const TArray<int32>& GetVisibleQuestionIds() const { return VisibleQuestionIds; }
    int32 GetQuestionCount() const { return Entries.Num(); }
    const FString& GetLoadError() const { return LoadError; }
    float GetListScrollOffset() const;
private:
    struct FEntry { int32 Id=0, GroupId=0, SourceGroupId=0; FString Group, Question, Answer; };
    TArray<FEntry> Entries;
    TArray<int32> Groups, VisibleQuestionIds;
    TSet<int32> Expanded;
    TSharedPtr<FJsonObject> Layouts;
    FString LoadError;
    int32 SelectedId=0;
    bool bAcceptActions=true;
    float ListOffset=0;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> Root;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> ListPage;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> DetailPage;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> ListScroll;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> DetailScroll;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<URokHelpSourceTextWidget> TitleText;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<URokHelpSourceTextWidget> QuestionText;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<URokHelpSourceTextWidget> AnswerText;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> QuestionTextRegion;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<UBorder> AnswerInset;
    UPROPERTY(Transient, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> AnswerWidth;
    UPROPERTY(Transient) TObjectPtr<URokHelpSourceTextData> SourceTextData;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CustomerServiceBoundaryText;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> LoadErrorPresentation;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LoadErrorText;
    bool LoadCatalog();
    void Build();
    void ShowLoadError();
    void RebuildList();
    void HandleAction(int32 Action);
    TSharedPtr<FJsonObject> Node(const TCHAR* Section,const FString& Path) const;
    UWidget* DrawNode(UCanvasPanel* Parent,const FName& Name,const TSharedPtr<FJsonObject>& Data,FVector2D Offset,int32 Z=1,const FString* Override=nullptr);
    void AddHit(UCanvasPanel* Parent,const FName& Name,int32 Action,const TSharedPtr<FJsonObject>& Data,FVector2D Offset,int32 Z=10);
};
