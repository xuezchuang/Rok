#include "RokHelpAuditSubsystem.h"
#include "RokDataPaths.h"
#include "RokHelpSourceTextWidget.h"
#include "RokHelpWidget.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
template<typename T> TArray<uint8> Snapshot(T Value)
{TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FObjectAndNameAsStringProxyArchive Archive(Writer,false);T::StaticStruct()->SerializeItem(Archive,&Value,nullptr);return Bytes;}
constexpr int32 ExpectedCaptures=14;
}
bool URokHelpAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return FParse::Param(FCommandLine::Get(),TEXT("RokHelpAudit"))&&Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokHelpAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const{return Type==EWorldType::Game;}
TStatId URokHelpAuditSubsystem::GetStatId() const {RETURN_QUICK_DECLARE_CYCLE_STAT(URokHelpAuditSubsystem,STATGROUP_Tickables);}
void URokHelpAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/HelpAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokHelpAuditSubsystem::ScreenshotProcessed);
}
void URokHelpAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
    if(bBaseline&&!bFinished){Failures.Add(TEXT("World teardown before audit completion"));Save(false);}
    Super::Deinitialize();
}
bool URokHelpAuditSubsystem::Expect(bool Condition,const TCHAR* Failure){if(!Condition)Finish(Failure);return Condition;}
void URokHelpAuditSubsystem::Enter(int32 NewStage){Stage=NewStage;StageStarted=Elapsed;}
bool URokHelpAuditSubsystem::Begin()
{
    FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
    if(!Expect(FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok"))&&GetWorld()->GetOutermost()->GetName()==TEXT("/Game/Maps/L_RokUnityGameAppCity"),TEXT("Wrong project/map before any UI action")))return false;
    const TCHAR* Cursor=FCommandLine::Get();FString Token;
    while(FParse::Token(Cursor,Token,false))if(Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase)&&!Token.Equals(TEXT("-RokHelpAudit"),ESearchCase::IgnoreCase)&&(Token.Contains(TEXT("Audit"),ESearchCase::IgnoreCase)||Token.StartsWith(TEXT("-RokOpen"))||Token.StartsWith(TEXT("-RokAuto"))))
    {Finish(TEXT("Dedicated process required without other audits/auto-open flags"));return false;}
    const auto Find=[&](UClass* Class)->UUserWidget*
    {TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Widgets,Class,false);UUserWidget* Result=nullptr;int32 Count=0;for(auto* W:Widgets)if(W->GetWorld()==GetWorld()){Result=W;++Count;}return Count==1?Result:nullptr;};
    Hud=Cast<URokMainHudWidget>(Find(URokMainHudWidget::StaticClass()));Modal=Cast<URokUnityModalWidget>(Find(URokUnityModalWidget::StaticClass()));Feature=Cast<URokUnityFeatureScreenWidget>(Find(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    if(!Expect(Hud.IsValid()&&Hud->IsVisible()&&Modal.IsValid()&&Feature.IsValid()&&!Modal->IsModalOpen()&&!Feature->IsScreenOpen()&&Runtime&&Economy,TEXT("City runtime hosts or source-state subsystems unavailable")))return false;
    FString CatalogText;TSharedPtr<FJsonObject> Catalog;
    if(!FFileHelper::LoadFileToString(CatalogText,*(RokDataPaths::Resolve(TEXT("help_catalog.json"))))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CatalogText),Catalog)||!Catalog.IsValid())
    {Finish(TEXT("Cannot independently read source FAQ text expectations"));return false;}
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(!Catalog->TryGetArrayField(TEXT("entries"),Rows)||Rows->Num()!=70){Finish(TEXT("Expected exactly 70 source FAQ texts"));return false;}
    for(const auto& Row:*Rows){const auto Entry=Row->AsObject();SourceQuestions.Add(Entry->GetIntegerField(TEXT("id")),TPair<FString,FString>(Entry->GetStringField(TEXT("question")),Entry->GetStringField(TEXT("answer"))));}
    InitialProfile=Snapshot(Runtime->GetPlayerProfileState());InitialResources=Snapshot(Economy->GetCurrentResources());bBaseline=true;return true;
}
bool URokHelpAuditSubsystem::Unchanged()
{
    if(!bBaseline)return true;
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    return Expect(Runtime&&Economy&&Snapshot(Runtime->GetPlayerProfileState())==InitialProfile&&Snapshot(Economy->GetCurrentResources())==InitialResources,TEXT("Profile or resource stock changed during local FAQ"));
}
bool URokHelpAuditSubsystem::Click(UUserWidget* Owner,const TCHAR* Name)
{
    auto* B=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
    if(!Expect(B&&B->IsVisible()&&B->GetIsEnabled(),TEXT("Required real UMG button absent or disabled")))return false;
    auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("button"),Name);Record->SetNumberField(TEXT("frame"),Frame);Record->SetNumberField(TEXT("stage"),Stage);Actions.Add(MakeShared<FJsonValueObject>(Record));B->OnClicked.Broadcast();return true;
}
TSharedRef<FJsonObject> URokHelpAuditSubsystem::State() const
{
    auto S=MakeShared<FJsonObject>();S->SetNumberField(TEXT("stage"),Stage);S->SetNumberField(TEXT("frame"),Frame);S->SetNumberField(TEXT("seconds"),Elapsed);
    if(Feature.IsValid()){S->SetBoolField(TEXT("feature_open"),Feature->IsScreenOpen());}
    if(Help.IsValid())
    {
        if(const auto* Parent=Help->GetParent()){S->SetNumberField(TEXT("host_animation_opacity"),Parent->GetRenderOpacity());S->SetNumberField(TEXT("host_animation_scale"),Parent->GetRenderTransform().Scale.X);}
        S->SetNumberField(TEXT("selected_question"),Help->GetSelectedQuestionId());S->SetNumberField(TEXT("question_count"),Help->GetQuestionCount());S->SetNumberField(TEXT("scroll_offset"),Help->GetListScrollOffset());
        TArray<TSharedPtr<FJsonValue>> Groups;for(int32 Id:Help->GetExpandedGroups())Groups.Add(MakeShared<FJsonValueNumber>(Id));S->SetArrayField(TEXT("expanded_groups"),Groups);
        TArray<TSharedPtr<FJsonValue>> Geometry;
        for(const TCHAR* N:{TEXT("HelpFrame_win_frame1"),TEXT("HelpFrame_win_frame2"),TEXT("HelpFrame_win_frame3"),TEXT("HelpFrame_win_frame4"),TEXT("HelpListScroll"),TEXT("HelpAnswerScroll"),TEXT("HelpQuestionTextRegion"),TEXT("HelpCloseButton"),TEXT("HelpBackButton")})
        {
            const auto* W=Help->GetWidgetFromName(N);if(!W)continue;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("widget"),N);
            if(const auto* Slot=Cast<UCanvasPanelSlot>(W->Slot)){R->SetNumberField(TEXT("x"),Slot->GetPosition().X);R->SetNumberField(TEXT("y"),Slot->GetPosition().Y);R->SetNumberField(TEXT("width"),Slot->GetSize().X);R->SetNumberField(TEXT("height"),Slot->GetSize().Y);R->SetNumberField(TEXT("z"),Slot->GetZOrder());}
            const auto& G=W->GetCachedGeometry();R->SetNumberField(TEXT("absolute_x"),G.GetAbsolutePosition().X);R->SetNumberField(TEXT("absolute_y"),G.GetAbsolutePosition().Y);R->SetBoolField(TEXT("visible"),W->IsVisible());Geometry.Add(MakeShared<FJsonValueObject>(R));
        }
        S->SetArrayField(TEXT("geometry"),Geometry);
        for(const TCHAR* N:{TEXT("HelpQuestionText"),TEXT("HelpAnswerText")})if(const auto* T=Cast<URokHelpSourceTextWidget>(Help->GetWidgetFromName(N))){S->SetStringField(N,T->GetText().ToString());S->SetStringField(FString(N)+TEXT("_source_key"),T->GetSourceKey());S->SetStringField(FString(N)+TEXT("_last_paint_key"),T->GetLastPaintSourceKey());S->SetNumberField(FString(N)+TEXT("_paint_count"),static_cast<double>(T->GetPaintCount()));}
    }
    return S;
}
void URokHelpAuditSubsystem::Capture(const TCHAR* Name)
{
    if(!Unchanged())return;if(FScreenshotRequest::IsScreenshotRequested()){Finish(TEXT("Another screenshot request is pending"));return;}
    PendingPath=Directory/(FString(Name)+TEXT(".png"));if(IFileManager::Get().FileExists(*PendingPath)){Finish(TEXT("Screenshot destination already exists"));return;}
    Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("name"),Name);Pending->SetStringField(TEXT("path"),PendingPath);Pending->SetObjectField(TEXT("request"),State());ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokHelpAuditSubsystem::ScreenshotProcessed()
{
    if(bFinished||!Pending.IsValid()||!Unchanged())return;
    if(Help.IsValid()&&Help->GetSelectedQuestionId()!=0)
    {
        for(const TCHAR* Name:{TEXT("HelpQuestionText"),TEXT("HelpAnswerText")})
        {const auto* Text=Cast<URokHelpSourceTextWidget>(Help->GetWidgetFromName(Name));
            if(!Text||!Text->HasSourceMesh()||Text->GetPaintCount()==0||Text->GetLastPaintSourceKey()!=Text->GetSourceKey())
            {Finish(TEXT("Screenshot current semantic glyph mesh was not painted"));return;}}
    }
    TArray<uint8> Bytes;const uint8 Sig[]={137,80,78,71,13,10,26,10};
    if(!FFileHelper::LoadFileToArray(Bytes,*PendingPath)||Bytes.Num()<24||FMemory::Memcmp(Bytes.GetData(),Sig,8)!=0){Finish(TEXT("Processed screenshot missing or invalid PNG"));return;}
    auto BE=[&](int32 O){return(uint32(Bytes[O])<<24)|(uint32(Bytes[O+1])<<16)|(uint32(Bytes[O+2])<<8)|uint32(Bytes[O+3]);};
    if(!Expect(BE(16)==1440&&BE(20)==810,TEXT("Audit requires source 1440x810 viewport")))return;
    Pending->SetNumberField(TEXT("width"),BE(16));Pending->SetNumberField(TEXT("height"),BE(20));Pending->SetNumberField(TEXT("file_bytes"),Bytes.Num());Pending->SetObjectField(TEXT("processed"),State());Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();
    if(!Save(false)){Finish(TEXT("Manifest progress write failed"));return;}Enter(Stage+1);
}
void URokHelpAuditSubsystem::Tick(float DeltaTime)
{
    if(bFinished||!GetWorld()->HasBegunPlay())return;Elapsed+=FMath::Max(0.f,DeltaTime);++Frame;
    if(Elapsed>90){Finish(TEXT("FAQ audit timed out"));return;}if(!Unchanged())return;
    if(Pending.IsValid()){if(Elapsed-ShotStarted>10)Finish(TEXT("Screenshot callback timed out"));return;}
    const double Delay=Stage==0?3.0:(Stage==4||Stage==28?.05:.5);if(Elapsed-StageStarted<Delay)return;
    auto NextClick=[&](UUserWidget* Owner,const TCHAR* N){if(Click(Owner,N))Enter(Stage+1);};
    auto DetailMatches=[&](int32 Id)
    {const auto* Expected=SourceQuestions.Find(Id);const auto* Question=Help.IsValid()?Cast<URokHelpSourceTextWidget>(Help->GetWidgetFromName(TEXT("HelpQuestionText"))):nullptr;const auto* Answer=Help.IsValid()?Cast<URokHelpSourceTextWidget>(Help->GetWidgetFromName(TEXT("HelpAnswerText"))):nullptr;
    return Help.IsValid()&&Help->GetSelectedQuestionId()==Id&&Expected&&Question&&Answer&&Question->HasSourceMesh()&&Answer->HasSourceMesh()&&Question->GetPaintCount()>0&&Answer->GetPaintCount()>0&&Question->GetLastPaintSourceKey()==Question->GetSourceKey()&&Answer->GetLastPaintSourceKey()==Answer->GetSourceKey()&&Question->GetText().ToString()==Expected->Key&&Answer->GetText().ToString()==Expected->Value;};
    switch(Stage)
    {
    case 0:if(Begin())Capture(TEXT("01_city_before"));break;
    case 1:NextClick(Hud.Get(),TEXT("PlayerProfileHitButton"));break;
    case 2:NextClick(Modal.Get(),TEXT("ProfileSettingsButton"));break;
    case 3:NextClick(Feature.Get(),TEXT("SettingsHubHelpButton"));break;
    case 4:Help=Cast<URokHelpWidget>(Feature->GetWidgetFromName(TEXT("SettingsHelpWidget")));if(Expect(Help.IsValid(),TEXT("Nested help did not open")))Capture(TEXT("02_t1_opening"));break;
    case 5:if(Expect(Help->GetQuestionCount()==70&&Help->GetExpandedGroups().Num()==0&&Help->GetLoadError().IsEmpty(),TEXT("Initial FAQ source/reset mismatch")))Capture(TEXT("03_source_collapsed"));break;
    case 6:NextClick(Help.Get(),TEXT("HelpGroupButton_900005"));break;
    case 7:NextClick(Help.Get(),TEXT("HelpGroupButton_900004"));break;
    case 8:if(Expect(Help->GetExpandedGroups().Num()==2&&Help->GetVisibleQuestionIds().Num()==29,TEXT("Two groups did not independently expand")))Capture(TEXT("04_source_expanded"));break;
    case 9:OriginalScroll=Cast<UScrollBox>(Help->GetWidgetFromName(TEXT("HelpListScroll")));if(Expect(OriginalScroll.IsValid(),TEXT("List scroll missing"))){OriginalScroll->SetScrollOffset(25);Enter(10);}break;
    case 10:NextClick(Help.Get(),TEXT("HelpQuestionButton_5001"));break;
    case 11:if(Expect(DetailMatches(5001),TEXT("Source question 5001 identity mismatch")))Capture(TEXT("05_source_detail_5001"));break;
    case 12:NextClick(Help.Get(),TEXT("HelpBackButton"));break;
    case 13:if(Expect(Help->GetSelectedQuestionId()==0&&Help->GetExpandedGroups().Num()==2&&Help->GetWidgetFromName(TEXT("HelpListScroll"))==OriginalScroll.Get()&&FMath::IsNearlyEqual(Help->GetListScrollOffset(),25.f,.01f),TEXT("Back lost list/expanded/scroll state")))Capture(TEXT("06_source_return"));break;
    case 14:for(int32 Id:{900003,900002,900007,900001,900006})if(!Click(Help.Get(),*FString::Printf(TEXT("HelpGroupButton_%d"),Id)))return;Enter(15);break;
    case 15:if(Expect(Help->GetExpandedGroups().Num()==7&&Help->GetVisibleQuestionIds().Num()==70,TEXT("Full FAQ coverage mismatch"))){OriginalScroll->ScrollToEnd();Enter(16);}break;
    case 16:if(Expect(Help->GetListScrollOffset()>1000,TEXT("Scroll-to-end did not advance")))Capture(TEXT("07_all_questions_bottom"));break;
    case 17:NextClick(Help.Get(),TEXT("HelpQuestionButton_4025"));break;
    case 18:if(Expect(DetailMatches(4025),TEXT("Final question identity mismatch")))Capture(TEXT("08_last_question"));break;
    case 19:NextClick(Help.Get(),TEXT("HelpBackButton"));break;
    case 20:OriginalScroll->ScrollWidgetIntoView(Help->GetWidgetFromName(TEXT("HelpQuestionButton_4004")),false,EDescendantScrollDestination::Center);Enter(21);break;
    case 21:NextClick(Help.Get(),TEXT("HelpQuestionButton_4004"));break;
    case 22:if(Expect(DetailMatches(4004),TEXT("Longest answer identity mismatch")))Capture(TEXT("09_long_answer"));break;
    case 23:NextClick(Help.Get(),TEXT("HelpBackButton"));break;
    case 24:NextClick(Help.Get(),TEXT("HelpCustomerServiceButton"));break;
    case 25:if(Expect(Help->GetWidgetFromName(TEXT("HelpCustomerServiceBoundary"))!=nullptr&&Help->GetSelectedQuestionId()==0,TEXT("SDK boundary absent or changed FAQ")))Capture(TEXT("10_sdk_boundary"));break;
    case 26:NextClick(Help.Get(),TEXT("HelpGroupButton_900005"));break;
    case 27:NextClick(Help.Get(),TEXT("HelpCloseButton"));break;
    case 28:{auto* B=Cast<UButton>(Help->GetWidgetFromName(TEXT("HelpGroupButton_900005")));if(!Expect(B!=nullptr,TEXT("Child disappeared before close midpoint")))return;B->OnClicked.Broadcast();if(Expect(Help->GetExpandedGroups().Num()==6&&Feature->IsScreenOpen()&&Help->GetParent()->GetRenderOpacity()>0&&Help->GetParent()->GetRenderOpacity()<1,TEXT("Close midpoint missing or child action mutated state while closing")))Capture(TEXT("11_t1_closing_guard"));break;}
    case 29:if(Expect(!Feature->IsScreenOpen()&&!Modal->IsModalOpen(),TEXT("Close did not restore city")))Capture(TEXT("12_city_after_close"));break;
    case 30:NextClick(Hud.Get(),TEXT("PlayerProfileHitButton"));break;
    case 31:NextClick(Modal.Get(),TEXT("ProfileSettingsButton"));break;
    case 32:NextClick(Feature.Get(),TEXT("SettingsHubHelpButton"));break;
    case 33:Help=Cast<URokHelpWidget>(Feature->GetWidgetFromName(TEXT("SettingsHelpWidget")));if(Expect(Help.IsValid()&&Help->GetExpandedGroups().Num()==0&&Help->GetSelectedQuestionId()==0&&FMath::IsNearlyZero(Help->GetListScrollOffset()),TEXT("Reopen did not reset FAQ")))Capture(TEXT("13_reopen_reset"));break;
    case 34:NextClick(Help.Get(),TEXT("HelpCloseButton"));break;
    case 35:if(Expect(!Feature->IsScreenOpen()&&!Modal->IsModalOpen(),TEXT("Final close did not restore city")))Capture(TEXT("14_city_final"));break;
    case 36:Finish();break;
    default:Finish(TEXT("Unknown audit stage"));break;
    }
}
bool URokHelpAuditSubsystem::Save(bool Complete)
{
    auto Data=MakeShared<FJsonObject>();Data->SetBoolField(TEXT("complete"),Complete);Data->SetBoolField(TEXT("passed"),Complete&&Failures.IsEmpty()&&Captures.Num()==ExpectedCaptures);Data->SetNumberField(TEXT("expected_captures"),ExpectedCaptures);Data->SetStringField(TEXT("project"),FPaths::ProjectDir());Data->SetStringField(TEXT("world"),GetWorld()?GetWorld()->GetOutermost()->GetName():TEXT("none"));
    Data->SetStringField(TEXT("input_method"),TEXT("Real UMG delegates and actual ticks; no physical hit-test or synthetic T1 advance claim. Scroll uses real UScrollBox methods."));Data->SetBoolField(TEXT("profile_resource_baseline_unchanged"),bBaseline&&Failures.IsEmpty());Data->SetBoolField(TEXT("native_visual_comparison_complete"),false);Data->SetStringField(TEXT("boundary"),TEXT("Local FAQ only, no SDK/browser/request/persistence actions."));Data->SetArrayField(TEXT("captures"),Captures);Data->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& Error:Failures)Errors.Add(MakeShared<FJsonValueString>(Error));Data->SetArrayField(TEXT("failures"),Errors);FString Json;
    return FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokHelpAuditSubsystem::Finish(const FString& Failure)
{
    if(bFinished)return;if(!Failure.IsEmpty())Failures.Add(Failure);if(Failure.IsEmpty()&&Captures.Num()!=ExpectedCaptures)Failures.Add(TEXT("Capture count incomplete"));bFinished=true;
    const bool Written=Save(Failures.IsEmpty());UE_LOG(LogTemp,Display,TEXT("Rok HelpAudit finished passed=%d captures=%d manifest_written=%d directory=%s"),Failures.IsEmpty()&&Written,Captures.Num(),Written,*Directory);FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
