#include "RokGraphicsSettingsAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokClientSettingsSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UnrealClient.h"
namespace
{
TSharedRef<FJsonObject> QualityState(const Scalability::FQualityLevels& Q)
{
 auto R=MakeShared<FJsonObject>();
 R->SetNumberField(TEXT("resolution"),Q.ResolutionQuality);
 R->SetNumberField(TEXT("view_distance"),Q.ViewDistanceQuality);
 R->SetNumberField(TEXT("anti_aliasing"),Q.AntiAliasingQuality);
 R->SetNumberField(TEXT("shadow"),Q.ShadowQuality);
 R->SetNumberField(TEXT("global_illumination"),Q.GlobalIlluminationQuality);
 R->SetNumberField(TEXT("reflection"),Q.ReflectionQuality);
 R->SetNumberField(TEXT("post_process"),Q.PostProcessQuality);
 R->SetNumberField(TEXT("texture"),Q.TextureQuality);
 R->SetNumberField(TEXT("effects"),Q.EffectsQuality);
 R->SetNumberField(TEXT("foliage"),Q.FoliageQuality);
 R->SetNumberField(TEXT("shading"),Q.ShadingQuality);
 R->SetNumberField(TEXT("landscape"),Q.LandscapeQuality);
 return R;
}
bool GraphicsVisible(const UWidget* Widget)
{
 if (!Widget || !Widget->IsVisible()) return false;
 for (auto* Parent=Widget->GetParent(); Parent; Parent=Parent->GetParent()) if (!Parent->IsVisible()) return false;
 for (auto* Outer=Widget->GetOuter(); Outer; Outer=Outer->GetOuter())
  if (const auto* User=Cast<UUserWidget>(Outer); User && !User->IsVisible()) return false;
 return true;
}
}
bool URokGraphicsSettingsAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 return FParse::Param(FCommandLine::Get(),TEXT("RokGraphicsSettingsAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokGraphicsSettingsAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game; }
void URokGraphicsSettingsAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/GraphicsSettingsAudit")/FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
 IFileManager::Get().MakeDirectory(*Directory,true);
 ScreenshotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokGraphicsSettingsAuditSubsystem::ScreenshotProcessed);
}
void URokGraphicsSettingsAuditSubsystem::Deinitialize()
{
 if (bSnapshot && !bRestored)
 {
  Failures.Add(TEXT("Audit interrupted by world teardown."));
  if (!Restore()) UE_LOG(LogTemp,Error,TEXT("Graphics audit teardown restore failed; use outer backups: %s"),*Directory);
  Save(false);
 }
 FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle); Super::Deinitialize();
}
TStatId URokGraphicsSettingsAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokGraphicsSettingsAuditSubsystem,STATGROUP_Tickables); }
bool URokGraphicsSettingsAuditSubsystem::Begin()
{
 FString Project=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());FPaths::NormalizeDirectoryName(Project);
 if (!FPaths::IsSamePath(Project,TEXT("D:/ueDemo/Rok")) || GetWorld()->GetOutermost()->GetName()!=TEXT("/Game/Maps/L_RokUnityGameAppCity"))
 { Finish(TEXT("Wrong project/city; no settings changed.")); return false; }
 // All audit switches end in Audit. Reject concurrent replay owners, including newly added audits.
 const TCHAR* Cursor=FCommandLine::Get(); FString Token;
 while (FParse::Token(Cursor,Token,false))
  if (Token.StartsWith(TEXT("-Rok")) && Token.EndsWith(TEXT("Audit")) && Token!=TEXT("-RokGraphicsSettingsAudit"))
  { Finish(TEXT("Use a dedicated graphics audit process.")); return false; }
 Settings=GetWorld()->GetGameInstance()->GetSubsystem<URokClientSettingsSubsystem>();
 if (!Settings.IsValid() || !Settings->AreSettingsLoaded() || !GEngine || !GEngine->GetGameUserSettings())
 { Finish(TEXT("Real local settings were not initialized.")); return false; }
 TArray<UUserWidget*> Found;
 const auto Find=[&](UClass* Class)->UUserWidget*
 {
  Found.Reset();UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Found,Class,false);
  UUserWidget* Result=nullptr; int32 Count=0;
  for (auto* Widget:Found) if (Widget->GetWorld()==GetWorld()) {Result=Widget;++Count;}
  return Count==1?Result:nullptr;
 };
 Hud=Cast<URokMainHudWidget>(Find(URokMainHudWidget::StaticClass()));
 Modal=Cast<URokUnityModalWidget>(Find(URokUnityModalWidget::StaticClass()));
 Feature=Cast<URokUnityFeatureScreenWidget>(Find(URokUnityFeatureScreenWidget::StaticClass()));
 if (!Hud.IsValid() || !Modal.IsValid() || !Feature.IsValid() || !GraphicsVisible(Hud.Get()) || Modal->IsModalOpen() || Feature->IsScreenOpen())
 { Finish(TEXT("Expected actual city HUD with closed modal/feature hosts.")); return false; }
 if (!Snapshot()) { Finish(TEXT("Cannot create complete settings backup; no audit edits performed.")); return false; }
 ExpectedGraphics=int32(Settings->GetGraphicsQualityTier());ExpectedFrame=int32(Settings->GetFrameRateOption());
 InitialState=State();
 if (!Save(false)) {Finish(TEXT("Cannot persist initial settings backup manifest."));return false;}
 return true;
}
bool URokGraphicsSettingsAuditSubsystem::Snapshot()
{
 auto* User=GEngine->GetGameUserSettings();
 // UE 5.8 known config globals are cache keys, not filenames.
 for (const FString& Key:TArray<FString>{GGameUserSettingsIni,GScalabilityIni})
 {
  auto* Branch=GConfig->FindBranch(FName(*Key),FString());
  if (!Branch || Branch->IniPath.IsEmpty() || !Branch->IniPath.EndsWith(TEXT(".ini"))) return false;
  const FString Path=FPaths::ConvertRelativePathToFull(Branch->IniPath);
  if (Files.ContainsByPredicate([&](const auto& F){return FPaths::IsSamePath(F.Path,Path);})) continue;
  FFileSnapshot F;F.Key=Key;F.Path=Path;F.Exists=IFileManager::Get().FileExists(*Path);
  if (F.Exists && !FFileHelper::LoadFileToArray(F.Bytes,*Path)) return false;
  F.Cache=Branch->InMemoryFile;F.FinalCombined=Branch->FinalCombinedLayers;
  F.SavedLayer=Branch->SavedLayer;F.RuntimeChanges=Branch->RuntimeChanges;
  const FString Backup=Directory/FString::Printf(TEXT("before_%d.bin"),Files.Num());
  if (F.Exists && !FFileHelper::SaveArrayToFile(F.Bytes,*Backup)) return false;
  Files.Add(MoveTemp(F));
 }
 if (Files.IsEmpty()) return false;
 FObjectWriter(GetMutableDefault<URokClientSettingsConfig>(),ClientBytes,false,false,false);
 FObjectWriter(User,UserBytes,false,false,false);
 OriginalQuality=Scalability::GetQualityLevels();OriginalUserQuality=User->ScalabilityQuality;
 OriginalDynamicResolution=GEngine->GetDynamicResolutionUserSetting();
 IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda([this](const TCHAR* Name,IConsoleObject* Object)
 {
  const FString Key(Name);
  if (!(Key.StartsWith(TEXT("r.")) || Key.StartsWith(TEXT("sg.")) || Key.StartsWith(TEXT("foliage.")) || Key.StartsWith(TEXT("grass.")) || Key.Equals(TEXT("t.MaxFPS"),ESearchCase::IgnoreCase))) return;
  if (auto* Variable=Object->AsVariable()) CVars.Add(Key,{Variable->GetString(),uint32(Variable->GetFlags())});
 }));
 bSnapshot=true;return true;
}
bool URokGraphicsSettingsAuditSubsystem::Restore()
{
 if (!bSnapshot || bRestored) return true;
 auto* User=GEngine?GEngine->GetGameUserSettings():nullptr;
 if (!User) return false;
 FObjectReader(GetMutableDefault<URokClientSettingsConfig>(),ClientBytes);
 FObjectReader(User,UserBytes);User->ScalabilityQuality=OriginalUserQuality;
 User->ApplyNonResolutionSettings();
 Scalability::SetQualityLevels(OriginalQuality,true);
 GEngine->SetDynamicResolutionUserSetting(OriginalDynamicResolution);
 // Restore effective render overrides too: runtime may intentionally differ from the persisted tier.
 // Values stay in memory; the manifest never dumps unrelated console variables.
 for (const auto& Pair:CVars)
  if (auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))
  {
   if (Variable->GetString()!=Pair.Value.Value) Variable->SetWithCurrentPriority(*Pair.Value.Value);
   Variable->SetFlags(EConsoleVariableFlags(Pair.Value.Flags));
  }
 IConsoleManager::Get().CallAllConsoleVariableSinks();
 bool Good=true;
 for (const auto& F:Files)
 {
  if (auto* Branch=GConfig->FindBranch(FName(*F.Key),FString()))
  {
   Branch->InMemoryFile=F.Cache.GetValue();Branch->FinalCombinedLayers=F.FinalCombined.GetValue();
   Branch->SavedLayer=F.SavedLayer.GetValue();Branch->RuntimeChanges=F.RuntimeChanges.GetValue();
  }
  else Good=false;
  if (F.Exists) Good=FFileHelper::SaveArrayToFile(F.Bytes,*F.Path)&&Good;
  else if (IFileManager::Get().FileExists(*F.Path)) Good=IFileManager::Get().Delete(*F.Path)&&Good;
  TArray<uint8> Actual;
  Good=(F.Exists?(FFileHelper::LoadFileToArray(Actual,*F.Path)&&Actual==F.Bytes):!IFileManager::Get().FileExists(*F.Path))&&Good;
 }
 for (const auto& Pair:CVars)
  if (auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))
   Good=(Variable->GetString()==Pair.Value.Value && uint32(Variable->GetFlags())==Pair.Value.Flags)&&Good;
 TArray<uint8> NowClient,NowUser;
 FObjectWriter(GetMutableDefault<URokClientSettingsConfig>(),NowClient,false,false,false);
 FObjectWriter(User,NowUser,false,false,false);
 Good=(NowClient==ClientBytes && NowUser==UserBytes && User->ScalabilityQuality==OriginalUserQuality
  && Scalability::GetQualityLevels()==OriginalQuality && GEngine->GetDynamicResolutionUserSetting()==OriginalDynamicResolution)&&Good;
 bRestored=Good;return Good;
}
void URokGraphicsSettingsAuditSubsystem::Next(){++Step;StepStarted=Elapsed;}
bool URokGraphicsSettingsAuditSubsystem::Click(UUserWidget* Owner,const TCHAR* Name)
{
 auto* Button=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
 if (!GraphicsVisible(Owner) || !GraphicsVisible(Button) || !Button->GetIsEnabled() || Button->GetCachedGeometry().GetLocalSize().X<=0)
 {Finish(FString::Printf(TEXT("Runtime button unavailable: %s"),Name));return false;}
 auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("button"),Name);Actions.Add(MakeShared<FJsonValueObject>(A));
 Button->OnClicked.Broadcast();Next();return true;
}
bool URokGraphicsSettingsAuditSubsystem::SetSlider(const TCHAR* Name,int32 Value)
{
 auto* Slider=Cast<USlider>(Feature->GetWidgetFromName(Name));
 if (!GraphicsVisible(Slider)||!Slider->GetIsEnabled()||Slider->GetCachedGeometry().GetLocalSize().X<=0)
 {Finish(TEXT("Actual graphics slider unavailable."));return false;}
 if (FString(Name).Contains(TEXT("Graphics"))) ExpectedGraphics=Value;else ExpectedFrame=Value;
 bApplied=true;
 Slider->SetValue(Value);Slider->OnValueChanged.Broadcast(Value);
 auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("slider_delegate"),Name);A->SetNumberField(TEXT("value"),Value);Actions.Add(MakeShared<FJsonValueObject>(A));
 Next();return true;
}
TSharedRef<FJsonObject> URokGraphicsSettingsAuditSubsystem::State() const
{
 auto R=MakeShared<FJsonObject>();auto* User=GEngine->GetGameUserSettings();
 R->SetNumberField(TEXT("graphics_option"),int32(Settings->GetGraphicsQualityTier()));R->SetNumberField(TEXT("frame_option"),int32(Settings->GetFrameRateOption()));
 R->SetNumberField(TEXT("overall_scalability"),User->GetOverallScalabilityLevel());R->SetNumberField(TEXT("fps_limit"),User->GetFrameRateLimit());R->SetBoolField(TEXT("vsync"),User->IsVSyncEnabled());
 R->SetObjectField(TEXT("user_scalability"),QualityState(User->ScalabilityQuality));
 R->SetObjectField(TEXT("runtime_scalability"),QualityState(Scalability::GetQualityLevels()));
 R->SetBoolField(TEXT("dynamic_resolution"),GEngine->GetDynamicResolutionUserSetting());
 if (DiskState && !bRestored) R->SetObjectField(TEXT("fresh_disk_read"),DiskState);
 R->SetNumberField(TEXT("actual_max_fps"),IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))->GetFloat());
 R->SetNumberField(TEXT("actual_vsync"),IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))->GetInt());
 if (Feature.IsValid()) if (auto* Scroll=Cast<UScrollBox>(Feature->GetWidgetFromName(TEXT("GeneralSettingsScroll"))))
 {R->SetNumberField(TEXT("scroll"),Scroll->GetScrollOffset());R->SetNumberField(TEXT("scroll_end"),Scroll->GetScrollOffsetOfEnd());}
 return R;
}
bool URokGraphicsSettingsAuditSubsystem::Validate()
{
 if (!Feature.IsValid() || !Feature->IsScreenOpen() || !Settings.IsValid()) {Finish(TEXT("General settings route disappeared."));return false;}
 const auto CheckText=[&](const FString& Name,const TCHAR* Expected)
 {
  const auto* Label=Cast<UTextBlock>(Feature->GetWidgetFromName(FName(*Name)));
  return Label && GraphicsVisible(Label) && Label->GetText().ToString()==Expected;
 };
 if (!CheckText(TEXT("GeneralSettingsGraphicsLabel"),TEXT("图像质量")) || !CheckText(TEXT("GeneralSettingsFrameRateLabel"),TEXT("帧率")))
 {Finish(TEXT("Source graphics card titles differ."));return false;}
 const TCHAR* Labels[]={TEXT("低"),TEXT("中"),TEXT("高")};
 for (const TCHAR* Prefix:{TEXT("GeneralSettingsGraphicsScale_"),TEXT("GeneralSettingsFrameRateScale_")})
  for (int32 Index=0;Index<3;++Index)
   if (!CheckText(FString(Prefix)+FString::FromInt(Index),Labels[Index]))
   {Finish(TEXT("Source low/medium/high scale labels differ."));return false;}
 for (const TCHAR* Name:{TEXT("GeneralSettingsGraphicsSlider"),TEXT("GeneralSettingsFrameRateSlider")})
 {
  auto* Slider=Cast<USlider>(Feature->GetWidgetFromName(Name));const int32 Expected=FString(Name).Contains(TEXT("Graphics"))?ExpectedGraphics:ExpectedFrame;
  if (!Slider || !FMath::IsNearlyEqual(Slider->GetValue(),float(Expected)) || Slider->GetMinValue()!=1 || Slider->GetMaxValue()!=3)
  {Finish(TEXT("Graphics slider range/value did not synchronize."));return false;}
 }
 if (int32(Settings->GetGraphicsQualityTier())!=ExpectedGraphics || int32(Settings->GetFrameRateOption())!=ExpectedFrame)
 {Finish(TEXT("Local settings enums differ from actual UMG interaction."));return false;}
 if (bApplied)
 {
  auto* User=GEngine->GetGameUserSettings();const float FPS=ExpectedFrame==3?60:30;
  Scalability::FQualityLevels Wanted;Wanted.SetFromSingleQualityLevel(ExpectedGraphics-1);
  if (User->GetOverallScalabilityLevel()!=ExpectedGraphics-1 || User->ScalabilityQuality!=Wanted || Scalability::GetQualityLevels()!=Wanted
   || User->GetFrameRateLimit()!=FPS || User->IsVSyncEnabled()
   || !FMath::IsNearlyEqual(IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))->GetFloat(),FPS)
   || IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"))->GetInt()!=0)
  {Finish(TEXT("Actual engine scalability/FPS/vsync differs from selected options."));return false;}
 }
 return !bApplied || ValidateDisk();
}
bool URokGraphicsSettingsAuditSubsystem::ValidateDisk()
{
 // Read the saved file into an independent config object, never the live GConfig cache.
 // Source: PersistProjectSettings, UGameUserSettings::SaveSettings, Scalability::SaveState.
 FConfigFile Disk;
 const auto* File=Files.FindByPredicate([](const auto& F){return F.Key==GGameUserSettingsIni;});
 if (!File || !IFileManager::Get().FileExists(*File->Path)) {Finish(TEXT("Saved graphics settings file is missing."));return false;}
 Disk.Read(File->Path);
 auto Record=MakeShared<FJsonObject>();Record->SetStringField(TEXT("path"),File->Path);Record->SetStringField(TEXT("cache_key"),File->Key);
 const FString ClientSection=URokClientSettingsConfig::StaticClass()->GetPathName();
 const FString EngineSection=GEngine->GetGameUserSettings()->GetClass()->GetPathName();
 const auto CheckInt=[&](const TCHAR* Section,const TCHAR* Key,int32 Expected)
 {
  int32 Value=0;if (!Disk.GetInt(Section,Key,Value) || Value!=Expected) return false;
  Record->SetNumberField(Key,Value);return true;
 };
 const auto CheckFloat=[&](const TCHAR* Section,const TCHAR* Key,float Expected)
 {
  float Value=0;if (!Disk.GetFloat(Section,Key,Value) || !FMath::IsNearlyEqual(Value,Expected,.001f)) return false;
  Record->SetNumberField(Key,Value);return true;
 };
 bool Good=CheckInt(*ClientSection,TEXT("GraphicsQualityTier"),ExpectedGraphics)
  && CheckInt(*ClientSection,TEXT("FrameRateOption"),ExpectedFrame)
  && CheckFloat(*EngineSection,TEXT("FrameRateLimit"),ExpectedFrame==3?60.f:30.f);
 bool VSync=true;Good=Disk.GetBool(*EngineSection,TEXT("bUseVSync"),VSync)&&!VSync&&Good;
 Record->SetBoolField(TEXT("bUseVSync"),VSync);
 Scalability::FQualityLevels Wanted;Wanted.SetFromSingleQualityLevel(ExpectedGraphics-1);
 Good=CheckFloat(TEXT("ScalabilityGroups"),TEXT("sg.ResolutionQuality"),Wanted.ResolutionQuality)&&Good;
 const TPair<const TCHAR*,int32> Groups[]={
  {TEXT("sg.ViewDistanceQuality"),Wanted.ViewDistanceQuality},{TEXT("sg.AntiAliasingQuality"),Wanted.AntiAliasingQuality},
  {TEXT("sg.ShadowQuality"),Wanted.ShadowQuality},{TEXT("sg.GlobalIlluminationQuality"),Wanted.GlobalIlluminationQuality},
  {TEXT("sg.ReflectionQuality"),Wanted.ReflectionQuality},{TEXT("sg.PostProcessQuality"),Wanted.PostProcessQuality},
  {TEXT("sg.TextureQuality"),Wanted.TextureQuality},{TEXT("sg.EffectsQuality"),Wanted.EffectsQuality},
  {TEXT("sg.FoliageQuality"),Wanted.FoliageQuality},{TEXT("sg.ShadingQuality"),Wanted.ShadingQuality},
  {TEXT("sg.LandscapeQuality"),Wanted.LandscapeQuality}};
 for (const auto& Group:Groups) Good=CheckInt(TEXT("ScalabilityGroups"),Group.Key,Group.Value)&&Good;
 if (!Good) {Finish(TEXT("Fresh disk graphics options/engine FPS/scalability differ from selected values."));return false;}
 DiskState=Record;return true;
}
void URokGraphicsSettingsAuditSubsystem::Capture(const TCHAR* Name)
{
 if (!Validate()) return;
 if (FScreenshotRequest::IsScreenshotRequested()) {Finish(TEXT("Another screenshot is pending."));return;}
 PendingPath=Directory/(FString(Name)+TEXT(".png"));Pending=MakeShared<FJsonObject>();Pending->SetStringField(TEXT("path"),PendingPath);Pending->SetObjectField(TEXT("request"),State());
 ScreenshotStarted=Elapsed;FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokGraphicsSettingsAuditSubsystem::ScreenshotProcessed()
{
 if (bFinished||!Pending.IsValid()||!Validate()) return;
 TArray<uint8> Bytes;const uint8 Signature[]={137,80,78,71,13,10,26,10};
 if (!FFileHelper::LoadFileToArray(Bytes,*PendingPath)||Bytes.Num()<24||FMemory::Memcmp(Bytes.GetData(),Signature,8)!=0)
 {Finish(TEXT("Processed graphics screenshot missing valid PNG."));return;}
 const auto ReadBE=[&](int32 Offset){return (uint32(Bytes[Offset])<<24)|(uint32(Bytes[Offset+1])<<16)|(uint32(Bytes[Offset+2])<<8)|Bytes[Offset+3];};
 const uint32 Width=ReadBE(16),Height=ReadBE(20);
 if (Width<640 || Height<360) {Finish(TEXT("Graphics screenshot has invalid runtime dimensions."));return;}
 Pending->SetNumberField(TEXT("width"),Width);Pending->SetNumberField(TEXT("height"),Height);
 Pending->SetNumberField(TEXT("bytes"),Bytes.Num());Pending->SetObjectField(TEXT("processed"),State());Captures.Add(MakeShared<FJsonValueObject>(Pending));Pending.Reset();
 if (!Save(false)){Finish(TEXT("Cannot persist graphics audit progress."));return;}Next();
}
void URokGraphicsSettingsAuditSubsystem::Tick(float DeltaTime)
{
 if (bFinished||!GetWorld()->HasBegunPlay()) return;Elapsed+=FMath::Max(0.f,DeltaTime);
 if (Elapsed>120){Finish(TEXT("Graphics audit timed out."));return;}
 if (Pending.IsValid()){if(Elapsed-ScreenshotStarted>10)Finish(TEXT("Graphics screenshot timed out."));return;}
 if (Elapsed-StepStarted<(Step==0?4:.65)) return;
 switch(Step)
 {
 case 0:if(Begin())Next();break;
 case 1:Click(Hud.Get(),TEXT("PlayerProfileHitButton"));break;
 case 2:Click(Modal.Get(),TEXT("ProfileSettingsButton"));break;
 case 3:Click(Feature.Get(),TEXT("SettingsHubGeneralButton"));break;
 case 4:Capture(TEXT("00_initial"));break;
 case 5:SetSlider(TEXT("GeneralSettingsGraphicsSlider"),1);break;
 case 6:Capture(TEXT("01_graphics_low"));break;
 case 7:SetSlider(TEXT("GeneralSettingsGraphicsSlider"),2);break;
 case 8:Capture(TEXT("02_graphics_medium"));break;
 case 9:SetSlider(TEXT("GeneralSettingsGraphicsSlider"),3);break;
 case 10:Capture(TEXT("03_graphics_high"));break;
 case 11:SetSlider(TEXT("GeneralSettingsFrameRateSlider"),1);break;
 case 12:Capture(TEXT("04_fps_low"));break;
 case 13:SetSlider(TEXT("GeneralSettingsFrameRateSlider"),2);break;
 case 14:Capture(TEXT("05_fps_medium"));break;
 case 15:SetSlider(TEXT("GeneralSettingsFrameRateSlider"),3);break;
 case 16:Capture(TEXT("06_fps_high"));break;
 case 17:{auto* S=Cast<UScrollBox>(Feature->GetWidgetFromName(TEXT("GeneralSettingsScroll")));if(!S||S->GetScrollOffsetOfEnd()<=0){Finish(TEXT("Expected overflowing settings scroll."));return;}S->ScrollToEnd();Next();break;}
 case 18:{auto* S=Cast<UScrollBox>(Feature->GetWidgetFromName(TEXT("GeneralSettingsScroll")));if(!S||!FMath::IsNearlyEqual(S->GetScrollOffset(),S->GetScrollOffsetOfEnd(),1.f)){Finish(TEXT("Settings did not scroll to end."));return;}Capture(TEXT("07_scrolled_preserves_values"));break;}
 case 19:Click(Feature.Get(),TEXT("FeatureFrameClose"));break;
 case 20:Click(Hud.Get(),TEXT("PlayerProfileHitButton"));break;
 case 21:Click(Modal.Get(),TEXT("ProfileSettingsButton"));break;
 case 22:Click(Feature.Get(),TEXT("SettingsHubGeneralButton"));break;
 case 23:Capture(TEXT("08_reopened"));break;
 case 24:Click(Feature.Get(),TEXT("FeatureFrameClose"));break;
 case 25:Finish();break;
 }
}
bool URokGraphicsSettingsAuditSubsystem::Save(bool Complete)
{
 auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("complete"),Complete);R->SetBoolField(TEXT("passed"),Complete&&Failures.IsEmpty()&&Captures.Num()==9&&bRestored);R->SetBoolField(TEXT("restored"),bRestored);
 if(InitialState)R->SetObjectField(TEXT("initial_runtime"),InitialState);
 if(bRestored && Settings.IsValid()) R->SetObjectField(TEXT("restored_runtime"),State());
 R->SetArrayField(TEXT("captures"),Captures);R->SetArrayField(TEXT("actions"),Actions);
 R->SetStringField(TEXT("input"),TEXT("Actual UMG slider/button delegates and real ticks; no physical pointer claim. No server requests."));
 TArray<TSharedPtr<FJsonValue>> F;
 for(int32 I=0;I<Files.Num();++I){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("path"),Files[I].Path);O->SetStringField(TEXT("cache_key"),Files[I].Key);O->SetBoolField(TEXT("existed"),Files[I].Exists);O->SetStringField(TEXT("backup"),Directory/FString::Printf(TEXT("before_%d.bin"),I));F.Add(MakeShared<FJsonValueObject>(O));}
 R->SetArrayField(TEXT("files"),F);TArray<TSharedPtr<FJsonValue>> Errors;for(const auto& E:Failures)Errors.Add(MakeShared<FJsonValueString>(E));R->SetArrayField(TEXT("failures"),Errors);
 FString Json;return FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokGraphicsSettingsAuditSubsystem::Finish(const FString& Error)
{
 if(bFinished)return;if(!Error.IsEmpty())Failures.Add(Error);else if(Captures.Num()!=9)Failures.Add(TEXT("Expected nine graphics screenshots."));
 if(!Restore())Failures.Add(TEXT("Original settings restoration failed; use outer file backups after exit."));
 bFinished=true;if(!Save(true))Failures.Add(TEXT("Final graphics manifest failed."));
 UE_LOG(LogTemp,Display,TEXT("Rok GraphicsSettings audit passed=%d restored=%d output=%s failures=%s"),Failures.IsEmpty(),bRestored,*Directory,*FString::Join(Failures,TEXT("; ")));
 FPlatformMisc::RequestExit(false);
}
