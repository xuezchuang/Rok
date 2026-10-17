#include "RokMailRewardDetailAuditSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr int32 CaptureCount = 21;
constexpr const TCHAR* AuditFlag = TEXT("-RokMailRewardDetailAudit");
constexpr const TCHAR* CityMap = TEXT("/Game/Maps/L_RokUnityGameAppCity");
// Whole-file identity pins the reviewed deferred ActionId 240 feedback-only path.
constexpr const TCHAR* ReviewedUiSourceSha256 = TEXT("2af290091b67d2fa614056caee9c7e18405953f8cee88936dee3a162790b3701");

bool TargetWorld(const UWorld* World)
{
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(Project);
    return World && World->WorldType == EWorldType::Game
        && World->GetOutermost()->GetName() == CityMap
        && FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok"));
}
template<typename T> TArray<uint8> Snapshot(T Value)
{
    TArray<uint8> Bytes; FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Archive(Writer, false);
    T::StaticStruct()->SerializeItem(Archive, &Value, nullptr);
    return Bytes;
}
bool SamePayload(const FRokMailRewardPayload& A, const FRokMailRewardPayload& B)
{
    if (A.Food != B.Food || A.Wood != B.Wood || A.Stone != B.Stone
        || A.Gold != B.Gold || A.Denar != B.Denar
        || A.Soldiers.Num() != B.Soldiers.Num() || A.Items.Num() != B.Items.Num()) return false;
    for (int32 I=0; I<A.Soldiers.Num(); ++I)
        if (A.Soldiers[I].Id != B.Soldiers[I].Id || A.Soldiers[I].Quantity != B.Soldiers[I].Quantity) return false;
    for (int32 I=0; I<A.Items.Num(); ++I)
        if (A.Items[I].Id != B.Items[I].Id || A.Items[I].Quantity != B.Items[I].Quantity) return false;
    return true;
}
bool SameRow(const FRokMailRewardRow& A, const FRokMailRewardRow& B)
{
    return A.Kind == B.Kind && A.Id == B.Id && A.Quantity == B.Quantity
        && A.QuantityText == B.QuantityText && A.DisplayName == B.DisplayName
        && A.Icon == B.Icon && A.Quality == B.Quality
        && A.TroopLevel == B.TroopLevel && A.TroopArmsType == B.TroopArmsType
        && A.bKnownMetadata == B.bKnownMetadata;
}
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent=Widget->GetParent(); Parent; Parent=Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    for (const UObject* Outer=Widget->GetOuter(); Outer; Outer=Outer->GetOuter())
        if (const auto* User=Cast<UUserWidget>(Outer); User && (!User->IsVisible() || User->GetRenderOpacity() <= 0)) return false;
    return true;
}
TSharedRef<FJsonObject> MakeMailRewardRectJson(FVector2D Origin, FVector2D Size)
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("x"),Origin.X); Result->SetNumberField(TEXT("y"),Origin.Y);
    Result->SetNumberField(TEXT("width"),Size.X); Result->SetNumberField(TEXT("height"),Size.Y);
    return Result;
}
TSharedRef<FJsonObject> WidgetGeometry(const UObject* Context, const UWidget* Widget)
{
    auto Result=MakeShared<FJsonObject>(); Result->SetBoolField(TEXT("attached"),Widget!=nullptr);
    if (!Widget) return Result;
    Result->SetBoolField(TEXT("visible"),Visible(Widget));
    Result->SetNumberField(TEXT("opacity"),Widget->GetRenderOpacity());
    if (const auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        Result->SetObjectField(TEXT("slot_rect"),MakeMailRewardRectJson(Slot->GetPosition(),Slot->GetSize()));
    if (const auto* Text=Cast<UTextBlock>(Widget))
    {
        Result->SetStringField(TEXT("text"),Text->GetText().ToString());
        const FSlateFontInfo Font=Text->GetFont();
        Result->SetNumberField(TEXT("font_points"),Font.Size);
        Result->SetStringField(TEXT("font_style"),Font.TypefaceFontName.ToString());
        const FCompositeFont* Composite=Font.GetCompositeFont();
        if (Composite && !Composite->DefaultTypeface.Fonts.IsEmpty())
            Result->SetStringField(TEXT("font_filename"),Composite->DefaultTypeface.Fonts[0].Font.GetFontFilename());
        const FColor SRGB=Text->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true);
        auto DisplayColor=MakeShared<FJsonObject>();
        DisplayColor->SetNumberField(TEXT("r"),SRGB.R);
        DisplayColor->SetNumberField(TEXT("g"),SRGB.G);
        DisplayColor->SetNumberField(TEXT("b"),SRGB.B);
        DisplayColor->SetNumberField(TEXT("a"),SRGB.A);
        Result->SetObjectField(TEXT("display_srgb"),DisplayColor);
    }
    const FGeometry Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context);
    const FVector2D Pixels=UWidgetLayoutLibrary::GetViewportSize(Context);
    const FGeometry Geometry=Widget->GetCachedGeometry();
    if (Viewport.GetLocalSize().X > 0 && Viewport.GetLocalSize().Y > 0)
    {
        const auto Pixel=[&](FVector2D P)
        {
            const FVector2D Local=Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(P));
            return FVector2D(Local.X*Pixels.X/Viewport.GetLocalSize().X,
                Local.Y*Pixels.Y/Viewport.GetLocalSize().Y);
        };
        const FVector2D A=Pixel(FVector2D::ZeroVector), B=Pixel(Geometry.GetLocalSize());
        Result->SetObjectField(TEXT("pixel_rect"),MakeMailRewardRectJson(
            FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),
            FVector2D(FMath::Abs(B.X-A.X),FMath::Abs(B.Y-A.Y))));
    }
    if (const auto* Image=Cast<UImage>(Widget))
    {
        const FSlateBrush& Brush=Image->GetBrush();
        Result->SetStringField(TEXT("texture"),Brush.GetResourceObject()?Brush.GetResourceObject()->GetPathName():FString());
        const FBox2f UV=Brush.GetUVRegion();
        auto Region=MakeShared<FJsonObject>();
        Region->SetNumberField(TEXT("min_x"),UV.Min.X); Region->SetNumberField(TEXT("min_y"),UV.Min.Y);
        Region->SetNumberField(TEXT("max_x"),UV.Max.X); Region->SetNumberField(TEXT("max_y"),UV.Max.Y);
        Result->SetObjectField(TEXT("uv"),Region);
    }
    return Result;
}
TSharedRef<FJsonObject> RowJson(const FRokMailRewardRow& Row)
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("kind"),static_cast<int32>(Row.Kind));
    Result->SetNumberField(TEXT("id"),Row.Id); Result->SetNumberField(TEXT("quantity"),Row.Quantity);
    Result->SetStringField(TEXT("quantity_text"),Row.QuantityText);
    Result->SetStringField(TEXT("name"),Row.DisplayName); Result->SetStringField(TEXT("icon"),Row.Icon);
    Result->SetNumberField(TEXT("quality"),Row.Quality);
    Result->SetNumberField(TEXT("troop_level"),Row.TroopLevel);
    Result->SetNumberField(TEXT("troop_arms_type"),Row.TroopArmsType);
    Result->SetBoolField(TEXT("known_metadata"),Row.bKnownMetadata);
    return Result;
}
}

bool URokMailRewardDetailAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(),TEXT("RokMailRewardDetailAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokMailRewardDetailAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game; }
TStatId URokMailRewardDetailAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokMailRewardDetailAuditSubsystem,STATGROUP_Tickables); }
void URokMailRewardDetailAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsTargetWorld=TargetWorld(GetWorld()); if (!bOwnsTargetWorld) return;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("RokUnityMigration/MailRewardDetailAudit")
        /FString::Printf(TEXT("%s_%u"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory,true);
    ShotHandle=FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this,&URokMailRewardDetailAuditSubsystem::ScreenshotProcessed);
}
void URokMailRewardDetailAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsTargetWorld && !bFinished) Finish(TEXT("Target city world ended before audit completion"));
    Super::Deinitialize();
}
bool URokMailRewardDetailAuditSubsystem::Expect(bool Condition,const TCHAR* Error)
{ if (!Condition) Finish(Error); return Condition; }
bool URokMailRewardDetailAuditSubsystem::Identity()
{ return Expect(TargetWorld(GetWorld()),TEXT("Wrong project/map; no mail UI action allowed")); }
void URokMailRewardDetailAuditSubsystem::Enter(int32 Next) { Step=Next; StepStarted=Elapsed; }

bool URokMailRewardDetailAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor=FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor,Token,false))
        if (Token.StartsWith(TEXT("-Rok"),ESearchCase::IgnoreCase) && !Token.Equals(AuditFlag,ESearchCase::IgnoreCase))
        { Finish(TEXT("Run MailRewardDetailAudit without any competing Rok flag")); return false; }
    Inputs=MakeShared<FJsonObject>();
    const TCHAR* SourceFiles[]={
        TEXT("Saved/RokUnityMigration/mail_reward_presentation.json"),
        TEXT("Source/Rok/RokUnityFeatureScreenWidget.h"),TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp"),
        TEXT("Source/Rok/RokMailRewardDetails.h"),TEXT("Source/Rok/RokMailRewardDetails.cpp"),
        TEXT("ref/client-unity/Assets/Scripts/Hotfix/MVC/View_Mediator/Mail/MailMediator.cs"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_common/ui_common.tpsheet"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_mail/ui_mail.tpsheet"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_troops/troops_head.tpsheet"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item0.tpsheet"),
        TEXT("ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/ui_res_item/item5.tpsheet"),
        TEXT("Saved/UnityMailRewardNativeProbe/Artifacts/native_20261008_114400_572/render_evidence.json"),
        TEXT("Saved/RokUnityMigration/MailRewardNative/verified_capture.json")
    };
    for (const TCHAR* Relative:SourceFiles)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/Relative)) && !Bytes.IsEmpty(),
            TEXT("Required mail reward source input unavailable"))) return false;
        auto File=MakeShared<FJsonObject>(); File->SetNumberField(TEXT("bytes"),Bytes.Num());
        File->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(Bytes));
        Inputs->SetObjectField(Relative,File);
        if (FString(Relative)==TEXT("Saved/RokUnityMigration/mail_reward_presentation.json"))
            FixtureSha256=RokTrainArmyMesh::Sha256(Bytes);
    }
    const FString WindowsDirectory=FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
    const FString ArialBoldPath=FPaths::Combine(WindowsDirectory,TEXT("Fonts/arialbd.ttf"));
    TArray<uint8> ArialBoldBytes;
    if (!Expect(!WindowsDirectory.IsEmpty() && FFileHelper::LoadFileToArray(ArialBoldBytes,*ArialBoldPath)
        && !ArialBoldBytes.IsEmpty(),TEXT("Local Arial Bold source-font candidate unavailable"))) return false;
    LocalCountFont=MakeShared<FJsonObject>();
    LocalCountFont->SetStringField(TEXT("path"),ArialBoldPath);
    LocalCountFont->SetNumberField(TEXT("bytes"),ArialBoldBytes.Num());
    LocalCountFont->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(ArialBoldBytes));
    LocalCountFont->SetStringField(TEXT("scope"),TEXT("Local Windows Arial Bold candidate for digit quantities; not proof of Unity glyph raster equivalence or a portable UE asset"));
    const auto Host=[&](UClass* Class)->UUserWidget*
    {
        TArray<UUserWidget*> All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,All,Class,false);
        UUserWidget* Result=nullptr; int32 Count=0;
        for (auto* Widget:All) if (Widget->GetWorld()==GetWorld()) { Result=Widget; ++Count; }
        return Count==1?Result:nullptr;
    };
    Hud=Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));
    Screen=Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Screen.IsValid() && !Screen->IsScreenOpen()
        && Visible(Hud.Get()) && Economy && Runtime,TEXT("Expected unique idle city HUD and mail host"))) return false;
    FRokMailRewardPayload Mixed,Empty,Single; FString Error;
    if (!Expect(RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::MixedAudit,Mixed,Error)
        && RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::EmptyAlternate,Empty,Error)
        && RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::ReferenceSingle,Single,Error),
        TEXT("Typed mail reward fixture unavailable"))) return false;
    FixturePayloads.Add(0,Mixed); FixturePayloads.Add(1,Empty);
    FixtureBaseline=FixturePayloads;
    if (!Expect(Single.Items.Num()==1 && Single.Items[0].Id==502090001 && Single.Items[0].Quantity==3
        && Mixed.Food==101 && Mixed.Wood==202 && Mixed.Stone==303 && Mixed.Gold==404 && Mixed.Denar==505
        && Mixed.Soldiers.Num()==1 && Mixed.Soldiers[0].Id==10304 && Mixed.Soldiers[0].Quantity==7
        && Mixed.Items.Num()==1 && Mixed.Items[0].Id==502090001 && Mixed.Items[0].Quantity==3
        && Empty.Items.IsEmpty() && Empty.Soldiers.IsEmpty() && !Empty.Food && !Empty.Wood && !Empty.Stone && !Empty.Gold && !Empty.Denar,
        TEXT("Mail reward fixture identity/quantity differs"))) return false;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        bool Tagged=false;
        for (FName Tag:It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { Tagged=true; break; }
        if (Tagged)
        {
            const auto State=Economy->GetBuildingStateForActor(*It);
            if (State.UnityInitId>0) Buildings.Add(*It,Snapshot(State));
        }
    }
    OwnedQueues=Economy->GetOwnedBuildQueueCount();
    if (!Expect(Economy->GetActiveUpgradeCount()==0,TEXT("Expected idle build/upgrade queue"))) return false;
    Profile=Snapshot(Runtime->GetPlayerProfileState());
    Resources=Snapshot(Economy->GetCurrentResources());
    for (const auto& Stack:Runtime->GetStarterTroopStacks()) TroopStacks.Add(Snapshot(Stack));
    InventoryFingerprint=Screen->GetVisibleItemsInventoryFingerprintForAudit();
    bBaseline=true;
    ExpectedPage=1;
    return Click(Hud.Get(),TEXT("MailFeatureHitButton"));
}

bool URokMailRewardDetailAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy=GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime && Snapshot(Economy->GetCurrentResources())==Resources
        && Snapshot(Runtime->GetPlayerProfileState())==Profile
        && Economy->GetOwnedBuildQueueCount()==OwnedQueues && Economy->GetActiveUpgradeCount()==0,
        TEXT("Mail navigation changed resources/profile/build queue"))) return false;
    const auto NowStacks=Runtime->GetStarterTroopStacks();
    if (!Expect(NowStacks.Num()==TroopStacks.Num(),TEXT("Mail navigation changed troop stack count"))) return false;
    for (int32 I=0; I<NowStacks.Num(); ++I)
        if (!Expect(Snapshot(NowStacks[I])==TroopStacks[I],TEXT("Mail navigation changed troops"))) return false;
    for (const auto& Pair:Buildings)
        if (!Expect(Pair.Key.IsValid() && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get()))==Pair.Value,
            TEXT("Mail navigation changed city buildings"))) return false;
    if (!Expect(Screen.IsValid() && Screen->GetVisibleItemsInventoryFingerprintForAudit()==InventoryFingerprint,
        TEXT("Mail navigation changed local inventory projection"))) return false;
#if !UE_BUILD_SHIPPING
    if ((ExpectedPage==2 || ExpectedPage==3)
        && !Expect(Screen->GetMailRewardFixtureFingerprintForAudit()==ScreenFixtureFingerprint,
            TEXT("Mail interaction changed internal screen fixture payload"))) return false;
#endif
    if (!Expect(FixturePayloads.Num()==FixtureBaseline.Num(),TEXT("Mail fixture map changed"))) return false;
    for (const auto& Pair:FixtureBaseline)
    {
        const auto* Current=FixturePayloads.Find(Pair.Key);
        if (!Expect(Current && SamePayload(*Current,Pair.Value),TEXT("Mail fixture payload changed"))) return false;
    }
    TArray<uint8> Bytes;
    return Expect(FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/TEXT("Saved/RokUnityMigration/mail_reward_presentation.json")))
        && RokTrainArmyMesh::Sha256(Bytes)==FixtureSha256,TEXT("Mail fixture source file changed during audit"));
}

bool URokMailRewardDetailAuditSubsystem::VerifyClaimIsLocalFeedbackOnly() const
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/TEXT("Source/Rok/RokUnityFeatureScreenWidget.cpp")))) return false;
    return RokTrainArmyMesh::Sha256(Bytes)==ReviewedUiSourceSha256;
}
bool URokMailRewardDetailAuditSubsystem::Click(UUserWidget* Owner,FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    const bool bAllowed=Name==TEXT("MailFeatureHitButton") || Name==TEXT("MailTab_1")
        || Name==TEXT("MailRowHit_0") || Name==TEXT("MailRowHit_1")
        || Name==TEXT("MailRewardFixtureHit_5") || Name==TEXT("MailRewardFixtureHit_6")
        || Name==TEXT("FeatureFrameClose") || Name==TEXT("MailClaim");
    if (!Expect(bAllowed,TEXT("Mail audit attempted non-read-only action"))) return false;
    if (Name==TEXT("MailClaim") && !Expect(VerifyClaimIsLocalFeedbackOnly(),
        TEXT("Claim action source hash differs from reviewed local-feedback-only code"))) return false;
    auto* Button=Owner?Cast<UButton>(Owner->GetWidgetFromName(Name)):nullptr;
    if (!Expect(Button && Visible(Button) && Button->GetIsEnabled(),TEXT("Missing visible enabled mail navigation/tooltip/boundary button"))) return false;
    auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),Name.ToString());
    Action->SetStringField(TEXT("method"),TEXT("UButton.OnClicked.Broadcast"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetBoolField(TEXT("claim_local_feedback_source_reviewed"),Name==TEXT("MailClaim") && VerifyClaimIsLocalFeedbackOnly());
    Actions.Add(MakeShared<FJsonValueObject>(Action)); Button->OnClicked.Broadcast(); return true;
}

bool URokMailRewardDetailAuditSubsystem::SlateClick(FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Screen.IsValid() && FSlateApplication::IsInitialized(),
        TEXT("Mail Slate virtual pointer route unavailable"))) return false;
    const bool bBlank=Name==TEXT("MailTooltipBlankBlocker");
    const bool bSide=Name==TEXT("MailTooltipArrowSideLeft");
    const bool bAllowed=bBlank || bSide || Name==TEXT("MailRewardTooltipBodyHit")
        || Name==TEXT("MailRewardTooltipArrowHit")
        || Name==TEXT("MailRewardFixtureHit_5") || Name==TEXT("MailRowHit_1");
    if (!Expect(bAllowed,TEXT("Mail audit attempted an unreviewed virtual pointer target"))) return false;
    UWidget* Target=bBlank?Screen->GetWidgetFromName(TEXT("FeatureScreenInputBlocker"))
        :Screen->GetWidgetFromName(bSide?FName(TEXT("MailRewardTooltipArrow")):Name);
    if (!Expect(Visible(Target),TEXT("Virtual pointer target widget unavailable"))) return false;
    const FGeometry Geometry=Target->GetCachedGeometry();
    const FVector2D Point=Geometry.LocalToAbsolute(bBlank?FVector2D(10,400)
        :bSide?FVector2D(-8,Geometry.GetLocalSize().Y-3)
        :Geometry.GetLocalSize()*0.5f);
    FSlateApplication& App=FSlateApplication::Get();
    // SButton::OnMouseButtonUp requires a genuine Slate hover for mouse input.
    // Route movement first; LocateWindowUnderMouse alone does not update hover.
    const FPointerEvent Move(0,Point,Point-FVector2D(16,16),TSet<FKey>(),
        EKeys::Invalid,0,FModifierKeysState());
    const bool bMoveHandled=App.ProcessMouseMoveEvent(Move);
    const FWidgetPath Path=App.LocateWindowUnderMouse(Point,App.GetInteractiveTopLevelWindows(),false,0);
    if (!Expect(Path.IsValid(),TEXT("Virtual pointer did not locate an interactive game Slate window"))) return false;
    const bool bTargetHit=Path.ContainsWidget(&Target->TakeWidget().Get());
    const bool bScreenHit=Path.ContainsWidget(&Screen->TakeWidget().Get());
    const bool bTargetHovered=Target->IsHovered();
    UWidget* BodyGuard=Screen->GetWidgetFromName(TEXT("MailRewardTooltipBodyHit"));
    UWidget* ArrowGuard=Screen->GetWidgetFromName(TEXT("MailRewardTooltipArrowHit"));
    const bool bBodyGuardHit=BodyGuard && Path.ContainsWidget(&BodyGuard->TakeWidget().Get());
    const bool bArrowGuardHit=ArrowGuard && Path.ContainsWidget(&ArrowGuard->TakeWidget().Get());
    if (!Expect((bSide?bScreenHit && !bBodyGuardHit && !bArrowGuardHit:bTargetHit),
        TEXT("Virtual pointer Slate hit path differs from reviewed target or blank region"))) return false;
    if (!Expect(bSide || bBlank || bTargetHovered,
        TEXT("Virtual pointer target was hit but Slate did not set button hover"))) return false;
    auto Action=MakeShared<FJsonObject>();
    Action->SetStringField(TEXT("button"),Name.ToString());
    Action->SetStringField(TEXT("method"),TEXT("FSlateApplication.ProcessMouseMoveEvent+DownEvent+UpEvent"));
    Action->SetNumberField(TEXT("step"),Step); Action->SetNumberField(TEXT("frame"),GFrameCounter);
    Action->SetNumberField(TEXT("absolute_x"),Point.X); Action->SetNumberField(TEXT("absolute_y"),Point.Y);
    Action->SetBoolField(TEXT("slate_hit_path_valid"),true);
    Action->SetBoolField(TEXT("move_handled"),bMoveHandled);
    Action->SetBoolField(TEXT("target_in_hit_path"),bTargetHit);
    Action->SetBoolField(TEXT("target_hovered_after_move"),bTargetHovered);
    Action->SetBoolField(TEXT("screen_in_hit_path"),bScreenHit);
    Action->SetBoolField(TEXT("body_guard_in_hit_path"),bBodyGuardHit);
    Action->SetBoolField(TEXT("arrow_guard_in_hit_path"),bArrowGuardHit);
    if (bSide)
    {
        const FVector2D RightPoint=Geometry.LocalToAbsolute(
            FVector2D(Geometry.GetLocalSize().X+8,Geometry.GetLocalSize().Y-3));
        const FWidgetPath Right=App.LocateWindowUnderMouse(RightPoint,App.GetInteractiveTopLevelWindows(),false,0);
        const bool bRightSafe=Right.IsValid() && Right.ContainsWidget(&Screen->TakeWidget().Get())
            && (!BodyGuard || !Right.ContainsWidget(&BodyGuard->TakeWidget().Get()))
            && (!ArrowGuard || !Right.ContainsWidget(&ArrowGuard->TakeWidget().Get()));
        if (!Expect(bRightSafe,TEXT("Tooltip right arrow-side Slate path still hits a guard"))) return false;
        Action->SetBoolField(TEXT("right_arrow_side_uncovered"),true);
        Action->SetNumberField(TEXT("right_absolute_x"),RightPoint.X);
        Action->SetNumberField(TEXT("right_absolute_y"),RightPoint.Y);
    }
    TArray<TSharedPtr<FJsonValue>> Types;
    for (int32 I=0; I<Path.Widgets.Num(); ++I)
        Types.Add(MakeShared<FJsonValueString>(Path.Widgets[I].Widget->GetTypeAsString()));
    Action->SetArrayField(TEXT("slate_widget_path_types"),Types);
    TSet<FKey> Pressed; Pressed.Add(EKeys::LeftMouseButton);
    const FPointerEvent Down(0,Point,Point,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());
    const FPointerEvent Up(0,Point,Point,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    const bool bDownHandled=App.ProcessMouseButtonDownEvent(TSharedPtr<FGenericWindow>(),Down);
    const bool bUpHandled=App.ProcessMouseButtonUpEvent(Up);
    Action->SetBoolField(TEXT("down_handled"),bDownHandled);
    Action->SetBoolField(TEXT("up_handled"),bUpHandled);
    Actions.Add(MakeShared<FJsonValueObject>(Action));
    return Expect(bBlank || bSide || (bDownHandled && bUpHandled),
        TEXT("Virtual pointer did not route both left-button events through Slate"));
}

bool URokMailRewardDetailAuditSubsystem::Validate()
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Screen.IsValid() && Hud.IsValid(),TEXT("Lost city mail UI host"))) return false;
    const FVector2D Pixels=UWidgetLayoutLibrary::GetViewportSize(this);
    if (!Expect(Pixels.Equals(FVector2D(1440,810),.1),TEXT("Mail audit viewport must be 1440x810"))) return false;
    if (ExpectedPage==0)
        return Expect(!Screen->IsScreenOpen() && !Screen->GetWidgetFromName(TEXT("MailRewardFixtureRoot_0")),
            TEXT("Closed mail retained reward presentation"));
#if !UE_BUILD_SHIPPING
    if (!Expect(Screen->IsScreenOpen() && Screen->IsMailModeForAudit()
        && Screen->GetSelectedMailTabForAudit()==((Step==3)?1:0),
        TEXT("Mail mode/tab differs from expected navigation"))) return false;
    if (!Expect(Screen->IsMailReadOnlyRewardFixtureForAudit()==(ExpectedPage==2 || ExpectedPage==3),
        TEXT("Mail explicit mixed fixture flag differs"))) return false;
    if ((ExpectedPage==2 || ExpectedPage==3)
        && !Expect(Screen->GetSelectedMailIndexForAudit()==(ExpectedPage==3?1:0),
            TEXT("Mail selected fixture row differs"))) return false;
    if (ExpectedPage==4 && !Expect(Screen->GetSelectedMailIndexForAudit()==0,
        TEXT("Legacy single-attachment reference did not reset selection"))) return false;
#endif
    const bool bNormal=ExpectedPage==1;
    const bool bMixed=ExpectedPage==2;
    const bool bEmptyAlternate=ExpectedPage==3;
    const bool bReference=ExpectedPage==4;
    const auto* Empty=Screen->GetWidgetFromName(TEXT("MailEmptyState"));
    const auto* Notice=Screen->GetWidgetFromName(TEXT("MailReferencePreviewNotice"));
    if (bNormal)
    {
        if (!Expect(Visible(Empty) && !Notice && !Screen->GetWidgetFromName(TEXT("MailRewardFixtureRoot_0"))
            && !Screen->GetWidgetFromName(TEXT("MailRewardItem")),
            TEXT("Normal HUD mail leaked fixture reward or lost empty state"))) return false;
    }
    else if (!Expect(Notice && Visible(Notice) && (bMixed || bReference || bEmptyAlternate),
        TEXT("Explicit reward fixture must remain visibly labelled"))) return false;
    const auto* Economy=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityEconomyCatalogSubsystem>();
    const auto* Gameplay=GetWorld()->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
    if (!Expect(Economy && Gameplay,TEXT("Mail metadata catalogs unavailable"))) return false;
    FRokMailRewardPayload SourcePayload;
    if (bMixed || bEmptyAlternate)
    {
        const auto* Found=FixturePayloads.Find(bMixed?0:1);
        if (!Expect(Found!=nullptr,TEXT("Current mail fixture payload missing"))) return false;
        SourcePayload=*Found;
    }
    else if (bReference)
    {
        FString Error;
        if (!Expect(RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::ReferenceSingle,SourcePayload,Error),
            TEXT("Legacy reference fixture unavailable"))) return false;
    }
    const TArray<FRokMailRewardRow> Expected=RokMailRewardDetails::Normalize(SourcePayload,Economy,Gameplay);
#if !UE_BUILD_SHIPPING
    const auto Rendered=Screen->GetRenderedMailRewardRowsForAudit();
    if (!Expect(Rendered.Num()==Expected.Num(),TEXT("Rendered mail reward count differs from source fixture"))) return false;
    for (int32 I=0; I<Expected.Num(); ++I)
        if (!Expect(SameRow(Rendered[I],Expected[I]),TEXT("Rendered mail reward metadata/order differs"))) return false;
#endif
    if (!Expect(Expected.Num()==(bMixed?7:bReference?1:0),
        TEXT("Expected mail reward payload row count differs"))) return false;
    if (bEmptyAlternate)
    {
        const auto* Grid=Screen->GetWidgetFromName(TEXT("MailRewardFixtureGrid"));
        const auto Geometry=WidgetGeometry(this,Grid);
        if (!Expect(Grid && !Screen->GetWidgetFromName(TEXT("MailRewardFixtureEmpty"))
            && !Screen->GetWidgetFromName(TEXT("MailRewardFixtureRoot_0"))
            && Geometry->GetObjectField(TEXT("pixel_rect"))->GetNumberField(TEXT("height"))<=.5,
            TEXT("Source empty RewardInfo must render a zero-height gift grid with no synthetic reward text"))) return false;
    }
    if (bReference && !Expect(Visible(Screen->GetWidgetFromName(TEXT("MailRewardItem")))
        && Visible(Screen->GetWidgetFromName(TEXT("MailRewardName")))
        && Screen->GetWidgetFromName(TEXT("MailAttachmentPulse_0")),
        TEXT("Old single-attachment reference presentation regressed"))) return false;
    if (bMixed)
    {
        const auto* Scroll=Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailDetailScroll")));
        if (!Expect(Visible(Scroll) && (bScrolledMailDetail?Scroll->GetScrollOffset()>80:Scroll->GetScrollOffset()<=1),
            TEXT("Selected-mail detail did not reach expected source scroll position"))) return false;
        for (int32 I=0; I<Expected.Num(); ++I)
        {
            const auto Root=Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureRoot_%d"),I));
            const auto Quantity=Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureQuantity_%d"),I)));
            const auto Name=Cast<UTextBlock>(Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureName_%d"),I)));
            const auto Icon=Cast<UImage>(Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureIcon_%d"),I)));
            const auto Quality=Cast<UImage>(Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureQuality_%d"),I)));
            if (!Expect(Visible(Root) && Visible(Quantity) && Visible(Name) && Visible(Icon)
                && Quantity->GetText().ToString()==Expected[I].QuantityText
                && Name->GetText().ToString()==Expected[I].DisplayName
                && Icon->GetBrush().GetResourceObject()!=nullptr && Quality
                && Visible(Quality)==(Expected[I].Kind==ERokMailRewardKind::Item && Expected[I].Quality>=1 && Expected[I].Quality<=5),
                TEXT("Mail reward cell icon/name/quantity missing or inconsistent"))) return false;
            const auto Hit=Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureHit_%d"),I));
            if (!Expect((Hit!=nullptr)==(Expected[I].Kind!=ERokMailRewardKind::Currency),
                TEXT("Mail currency gained tooltip hit or item/troop lost it"))) return false;
        }
        if (!Expect(!Screen->GetWidgetFromName(TEXT("MailRewardFixtureRoot_7")),
            TEXT("Mail reward list gained an eighth unbacked cell"))) return false;
        const auto ScrollRect=WidgetGeometry(this,Scroll)->GetObjectField(TEXT("pixel_rect"));
        const auto LastRect=WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardFixtureRoot_6")))->GetObjectField(TEXT("pixel_rect"));
        if (!Expect(LastRect->GetNumberField(TEXT("y"))+LastRect->GetNumberField(TEXT("height"))<ScrollRect->GetNumberField(TEXT("y"))+ScrollRect->GetNumberField(TEXT("height"))+.5
            && LastRect->GetNumberField(TEXT("y"))>ScrollRect->GetNumberField(TEXT("y"))-.5,
            TEXT("Both source gift rows must fit inside selected-mail Slate viewport"))) return false;
        const auto ClaimRect=WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailClaim")))->GetObjectField(TEXT("pixel_rect"));
        const float ViewBottom=ScrollRect->GetNumberField(TEXT("y"))+ScrollRect->GetNumberField(TEXT("height"));
        if (!Expect(bScrolledMailDetail
            ? ClaimRect->GetNumberField(TEXT("y"))+ClaimRect->GetNumberField(TEXT("height"))<=ViewBottom+.5
            : ClaimRect->GetNumberField(TEXT("y"))>=ViewBottom-.5,
            TEXT("Mail claim visibility must follow source detail clipping and scroll"))) return false;
    }
#if !UE_BUILD_SHIPPING
    const int32 ExpectedTooltipIndex=ExpectedTooltipKind==2?6:ExpectedTooltipKind==1?5:INDEX_NONE;
    const auto* Tip=Screen->GetWidgetFromName(TEXT("MailRewardTooltipRoot"));
    const auto* TipText=Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("MailRewardTooltipText")));
    if (!Expect(Screen->GetActiveMailRewardTooltipIndexForAudit()==ExpectedTooltipIndex
        && (ExpectedTooltipIndex==INDEX_NONE?!Visible(Tip):Visible(Tip) && Visible(TipText)
            && TipText->GetText().ToString()==Expected[ExpectedTooltipIndex].DisplayName),
        TEXT("Mail item/troop HelpTip identity, localized name or clearing differs"))) return false;
    if (ExpectedTooltipIndex!=INDEX_NONE)
    {
        const auto* Root=Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureRoot_%d"),ExpectedTooltipIndex));
        const auto Rect=WidgetGeometry(this,Root)->GetObjectField(TEXT("pixel_rect"));
        const FVector2D Anchor=Screen->GetMailRewardTooltipAnchorForAudit();
        if (!Expect(FMath::IsNearlyEqual(Rect->GetNumberField(TEXT("height")),140,.25)
            && FMath::IsNearlyEqual(Anchor.X,Rect->GetNumberField(TEXT("x"))+Rect->GetNumberField(TEXT("width"))/2,.5)
            && FMath::IsNearlyEqual(Anchor.Y,Rect->GetNumberField(TEXT("y")),.5),
            TEXT("Mail HelpTip anchor differs from source gift half-height offset"))) return false;
    }
    if (Step==26 && !Expect(Screen->GetLastMailBoundaryActionIdForAudit()==240,
        TEXT("Mail claim did not execute reviewed local feedback action 240"))) return false;
#endif
    if (Step==26)
    {
        const auto* Feedback=Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
        if (!Expect(Visible(Feedback) && Feedback->GetText().ToString().Contains(TEXT("未改变邮件或奖励状态")),
            TEXT("Claim did not show local no-change service boundary"))) return false;
    }
    return true;
}

TSharedRef<FJsonObject> URokMailRewardDetailAuditSubsystem::State() const
{
    auto Result=MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("page"),ExpectedPage==0?TEXT("closed"):ExpectedPage==1?TEXT("normal_empty")
        :ExpectedPage==2?TEXT("mixed_fixture"):ExpectedPage==3?TEXT("empty_alternate"):TEXT("reference_single"));
    Result->SetNumberField(TEXT("step"),Step); Result->SetNumberField(TEXT("frame"),GFrameCounter);
    Result->SetBoolField(TEXT("state_unchanged"),true);
    Result->SetObjectField(TEXT("viewport_pixels"),MakeMailRewardRectJson(FVector2D::ZeroVector,
        UWidgetLayoutLibrary::GetViewportSize(this)));
    if (!Screen.IsValid() || ExpectedPage==0) return Result;
#if !UE_BUILD_SHIPPING
    Result->SetBoolField(TEXT("mail_mode"),Screen->IsMailModeForAudit());
    Result->SetBoolField(TEXT("explicit_reward_fixture"),Screen->IsMailReadOnlyRewardFixtureForAudit());
    Result->SetNumberField(TEXT("selected_tab"),Screen->GetSelectedMailTabForAudit());
    Result->SetNumberField(TEXT("selected_mail_index"),Screen->GetSelectedMailIndexForAudit());
    Result->SetStringField(TEXT("inventory_projection_fingerprint"),Screen->GetVisibleItemsInventoryFingerprintForAudit());
    Result->SetStringField(TEXT("screen_fixture_fingerprint"),Screen->GetMailRewardFixtureFingerprintForAudit());
    Result->SetNumberField(TEXT("active_tooltip_index"),Screen->GetActiveMailRewardTooltipIndexForAudit());
    Result->SetNumberField(TEXT("last_mail_boundary_action_id"),Screen->GetLastMailBoundaryActionIdForAudit());
    const FVector2D Anchor=Screen->GetMailRewardTooltipAnchorForAudit();
    Result->SetObjectField(TEXT("tooltip_anchor"),MakeMailRewardRectJson(Anchor,FVector2D::ZeroVector));
#endif
    Result->SetObjectField(TEXT("empty_state"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailEmptyState"))));
    Result->SetObjectField(TEXT("fixture_notice"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailReferencePreviewNotice"))));
    Result->SetObjectField(TEXT("feedback"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("FeatureBoundaryFeedback"))));
    Result->SetObjectField(TEXT("mail_frame_left"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailFrameBodyLeft"))));
    Result->SetObjectField(TEXT("mail_frame_right"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailFrameBodyRight"))));
    Result->SetObjectField(TEXT("mail_window_title"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("FeatureFrameTitle"))));
    Result->SetObjectField(TEXT("compose"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailCompose"))));
    Result->SetObjectField(TEXT("compose_label"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailComposeLabel"))));
    Result->SetObjectField(TEXT("list_panel"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailListPanel"))));
    Result->SetObjectField(TEXT("list_scroll"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailListScroll"))));
    Result->SetObjectField(TEXT("detail_panel"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailDetailPanel"))));
    Result->SetObjectField(TEXT("detail_poster"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailDetailPoster"))));
    Result->SetObjectField(TEXT("claim_button"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailClaim"))));
    Result->SetObjectField(TEXT("claim_label"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailClaimLabel"))));
    auto DetailText=MakeShared<FJsonObject>();
    const TCHAR* DetailTextNames[]={TEXT("MailDetailTitle"),TEXT("MailDetailSub"),TEXT("MailDetailGreeting"),
        TEXT("MailDetailPrefix"),TEXT("MailDetailHighlight"),TEXT("MailDetailSuffix"),TEXT("MailDetailFinalLine")};
    for (const TCHAR* Name:DetailTextNames)
        DetailText->SetObjectField(Name,WidgetGeometry(this,Screen->GetWidgetFromName(Name)));
    Result->SetObjectField(TEXT("detail_text"),DetailText);
    TArray<TSharedPtr<FJsonValue>> Tabs;
    for (int32 I=0; I<6; ++I)
        Tabs.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailTab_%d"),I)))));
    Result->SetArrayField(TEXT("mail_tabs"),Tabs);
    TArray<TSharedPtr<FJsonValue>> TabLabels;
    for (int32 I=0; I<6; ++I)
        TabLabels.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailTab_%dLabel"),I)))));
    Result->SetArrayField(TEXT("mail_tab_labels"),TabLabels);
    TArray<TSharedPtr<FJsonValue>> TabLabelRegions;
    for (int32 I=0; I<6; ++I)
        TabLabelRegions.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailTab_%dLabelRegion"),I)))));
    Result->SetArrayField(TEXT("mail_tab_label_regions"),TabLabelRegions);
    TArray<TSharedPtr<FJsonValue>> Footers;
    for (int32 I=0; I<4; ++I)
        Footers.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailFooter_%d"),I)))));
    Result->SetArrayField(TEXT("mail_footers"),Footers);
    TArray<TSharedPtr<FJsonValue>> FooterLabels;
    for (int32 I=0; I<4; ++I)
        FooterLabels.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailFooterLabel_%d"),I)))));
    Result->SetArrayField(TEXT("mail_footer_labels"),FooterLabels);
    TArray<TSharedPtr<FJsonValue>> FooterLabelRegions;
    for (int32 I=0; I<4; ++I)
        FooterLabelRegions.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailFooterLabel_%dRegion"),I)))));
    Result->SetArrayField(TEXT("mail_footer_label_regions"),FooterLabelRegions);
    TArray<TSharedPtr<FJsonValue>> MailListRows;
    TArray<TSharedPtr<FJsonValue>> MailRowDates;
    TArray<TSharedPtr<FJsonValue>> MailRowTitles;
    TArray<TSharedPtr<FJsonValue>> MailRowSubjects;
    for (int32 I=0; I<2; ++I)
    {
        MailListRows.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRow_%d"),I)))));
        MailRowDates.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRowDate_%d"),I)))));
        MailRowTitles.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRowTitle_%d"),I)))));
        MailRowSubjects.Add(MakeShared<FJsonValueObject>(WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRowSubject_%d"),I)))));
    }
    Result->SetArrayField(TEXT("mail_list_rows"),MailListRows);
    Result->SetArrayField(TEXT("mail_row_dates"),MailRowDates);
    Result->SetArrayField(TEXT("mail_row_titles"),MailRowTitles);
    Result->SetArrayField(TEXT("mail_row_subjects"),MailRowSubjects);
    Result->SetObjectField(TEXT("legacy_reward"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardItem"))));
    Result->SetObjectField(TEXT("legacy_reward_name"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardName"))));
    Result->SetObjectField(TEXT("legacy_reward_icon"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardItemIcon"))));
    Result->SetObjectField(TEXT("legacy_reward_quality"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardItemQuality"))));
    Result->SetObjectField(TEXT("legacy_reward_count"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardItemCount"))));
    Result->SetObjectField(TEXT("legacy_attachment_pulse"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailAttachmentPulse_0"))));
    Result->SetObjectField(TEXT("reward_empty"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardFixtureEmpty"))));
    Result->SetObjectField(TEXT("detail_scroll"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailDetailScroll"))));
    Result->SetObjectField(TEXT("detail_content"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailDetailContent"))));
    Result->SetObjectField(TEXT("reward_grid"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardFixtureGrid"))));
    if (const auto* Scroll=Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailDetailScroll"))))
        Result->SetNumberField(TEXT("detail_scroll_offset"),Scroll->GetScrollOffset());
    Result->SetObjectField(TEXT("tooltip_root"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipRoot"))));
    Result->SetObjectField(TEXT("tooltip_background"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipBackground"))));
    Result->SetObjectField(TEXT("tooltip_arrow"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipArrow"))));
    Result->SetObjectField(TEXT("tooltip_body_hit"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipBodyHit"))));
    Result->SetObjectField(TEXT("tooltip_arrow_hit"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipArrowHit"))));
    Result->SetObjectField(TEXT("tooltip_text"),WidgetGeometry(this,Screen->GetWidgetFromName(TEXT("MailRewardTooltipText"))));
    TArray<TSharedPtr<FJsonValue>> Rows;
#if !UE_BUILD_SHIPPING
    for (const auto& Row:Screen->GetRenderedMailRewardRowsForAudit()) Rows.Add(MakeShared<FJsonValueObject>(RowJson(Row)));
#endif
    Result->SetArrayField(TEXT("rendered_rows"),Rows);
    TArray<TSharedPtr<FJsonValue>> Cells;
    for (int32 I=0; I<7; ++I)
    {
        auto Cell=MakeShared<FJsonObject>(); Cell->SetNumberField(TEXT("index"),I);
        Cell->SetObjectField(TEXT("root"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureRoot_%d"),I))));
        Cell->SetObjectField(TEXT("icon"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureIcon_%d"),I))));
        Cell->SetObjectField(TEXT("quantity"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureQuantity_%d"),I))));
        Cell->SetObjectField(TEXT("name"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureName_%d"),I))));
        Cell->SetObjectField(TEXT("quality"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureQuality_%d"),I))));
        Cell->SetObjectField(TEXT("hit"),WidgetGeometry(this,Screen->GetWidgetFromName(*FString::Printf(TEXT("MailRewardFixtureHit_%d"),I))));
        Cells.Add(MakeShared<FJsonValueObject>(Cell));
    }
    Result->SetArrayField(TEXT("reward_cells"),Cells);
    return Result;
}

void URokMailRewardDetailAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Identity() || !Unchanged() || Pending.IsValid() || !Validate()) return;
    PendingPath=Directory/(FString(Name)+TEXT(".png"));
    Pending=MakeShared<FJsonObject>();
    Pending->SetStringField(TEXT("file"),PendingPath);
    Pending->SetObjectField(TEXT("request"),State());
    ShotStarted=Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath,true,false,false,FIntRect(),true);
}
void URokMailRewardDetailAuditSubsystem::ScreenshotProcessed()
{
    if (bFinished || !Pending.IsValid() || !Identity() || !Validate()) return;
    Pending->SetObjectField(TEXT("processed"),State());
    TArray<uint8> Bytes; const uint8 PngSignature[]={137,80,78,71,13,10,26,10};
    if (!Expect(FFileHelper::LoadFileToArray(Bytes,*PendingPath) && Bytes.Num()>24
        && FMemory::Memcmp(Bytes.GetData(),PngSignature,8)==0,
        TEXT("Processed mail Slate screenshot missing or invalid"))) return;
    Pending->SetNumberField(TEXT("file_bytes"),Bytes.Num());
    Pending->SetStringField(TEXT("png_sha256"),RokTrainArmyMesh::Sha256(Bytes));
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset(); Enter(Step+1);
}

void URokMailRewardDetailAuditSubsystem::Tick(float DeltaTime)
{
    if (!bOwnsTargetWorld || bFinished || !GetWorld()->HasBegunPlay()) return;
    Elapsed+=FMath::Max(0.f,DeltaTime);
    if (Elapsed>90) { Finish(TEXT("Mail reward detail audit timed out")); return; }
    if (Pending.IsValid()) { if (Elapsed-ShotStarted>10) Finish(TEXT("Mail screenshot callback timed out")); return; }
    if (!Unchanged() || Elapsed-StepStarted<(Step==0?3.0:.35)) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: Capture(TEXT("01_normal_mail_empty")); break;
    case 2: if (Click(Screen.Get(),TEXT("MailTab_1"))) Enter(3); break;
    case 3: Capture(TEXT("02_normal_report_empty")); break;
    case 4: if (Click(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedPage=0; Enter(5); } break;
    case 5: Capture(TEXT("03_city_after_normal_close")); break;
    case 6:
        if (!Identity() || !Unchanged()) break;
        {
            auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),TEXT("explicit_mixed_fixture"));
            Action->SetStringField(TEXT("method"),TEXT("URokUnityFeatureScreenWidget.OpenMailReadOnlyRewardFixture"));
            Action->SetNumberField(TEXT("step"),Step); Actions.Add(MakeShared<FJsonValueObject>(Action));
        }
        Screen->OpenMailReadOnlyRewardFixture(FixturePayloads,0);
#if !UE_BUILD_SHIPPING
        ScreenFixtureFingerprint=Screen->GetMailRewardFixtureFingerprintForAudit();
#endif
        ExpectedPage=2; Enter(7); break;
    case 7: Capture(TEXT("04_mixed_reward_selected")); break;
    case 8:
        if (auto* Scroll=Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailDetailScroll"))))
        {
            auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),TEXT("MailDetailScroll"));
            Action->SetStringField(TEXT("method"),TEXT("UScrollBox.ScrollToEnd")); Action->SetNumberField(TEXT("step"),Step);
            Actions.Add(MakeShared<FJsonValueObject>(Action)); Scroll->ScrollToEnd(); bScrolledMailDetail=true; Enter(9);
        }
        else Finish(TEXT("Selected-mail detail scroll missing before claim-region inspection"));
        break;
    case 9: Capture(TEXT("05_mail_detail_scrolled_claim_visible")); break;
    case 10: if (Click(Screen.Get(),TEXT("MailRewardFixtureHit_6"))) { ExpectedTooltipKind=2; Enter(11); } break;
    case 11: Capture(TEXT("06_item_help_tip")); break;
    case 12: if (SlateClick(TEXT("MailRewardTooltipBodyHit"))) Enter(13); break;
    case 13: Capture(TEXT("07_item_tip_body_hit_retained")); break;
    case 14: if (SlateClick(TEXT("MailRewardTooltipArrowHit"))) Enter(15); break;
    case 15: Capture(TEXT("08_item_tip_arrow_hit_retained")); break;
    case 16: if (SlateClick(TEXT("MailTooltipBlankBlocker"))) Enter(17); break;
    case 17: Capture(TEXT("09_item_tip_blank_retained")); break;
    case 18: if (SlateClick(TEXT("MailRewardFixtureHit_5"))) { ExpectedTooltipKind=1; Enter(19); } break;
    case 19: Capture(TEXT("10_troop_help_tip_replaced")); break;
    case 20: if (SlateClick(TEXT("MailRowHit_1"))) { ExpectedPage=3; ExpectedTooltipKind=0; bScrolledMailDetail=false; Enter(21); } break;
    case 21: Capture(TEXT("11_alternate_mail_empty_tooltip_cleared")); break;
    case 22: if (Click(Screen.Get(),TEXT("MailRowHit_0"))) { ExpectedPage=2; bScrolledMailDetail=false; Enter(23); } break;
    case 23: Capture(TEXT("12_mixed_reward_reselected")); break;
    case 24:
        if (auto* Scroll=Cast<UScrollBox>(Screen->GetWidgetFromName(TEXT("MailDetailScroll"))))
        {
            auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),TEXT("MailDetailScroll"));
            Action->SetStringField(TEXT("method"),TEXT("UScrollBox.ScrollToEnd")); Action->SetNumberField(TEXT("step"),Step);
            Actions.Add(MakeShared<FJsonValueObject>(Action)); Scroll->ScrollToEnd(); bScrolledMailDetail=true; Enter(25);
        }
        else Finish(TEXT("Selected-mail detail scroll missing before local claim boundary"));
        break;
    case 25: if (Validate() && Click(Screen.Get(),TEXT("MailClaim"))) Enter(26); break;
    case 26: Capture(TEXT("13_claim_boundary_no_change")); break;
    case 27: if (Click(Screen.Get(),TEXT("MailRewardFixtureHit_6"))) { ExpectedTooltipKind=2; Enter(28); } break;
    case 28: Capture(TEXT("14_item_tip_reopened_after_claim")); break;
    case 29: if (SlateClick(TEXT("MailTooltipArrowSideLeft"))) { ExpectedTooltipKind=0; Enter(30); } break;
    case 30: Capture(TEXT("15_arrow_side_clicked_tip_closed")); break;
    case 31: if (Click(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedPage=0; Enter(32); } break;
    case 32: Capture(TEXT("16_city_after_fixture_close")); break;
    case 33: if (Click(Hud.Get(),TEXT("MailFeatureHitButton"))) { ExpectedPage=1; Enter(34); } break;
    case 34: Capture(TEXT("17_normal_mail_reentry_empty")); break;
    case 35: if (Click(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedPage=0; Enter(36); } break;
    case 36: if (Identity() && Unchanged())
        {
            auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),TEXT("explicit_reference_single"));
            Action->SetStringField(TEXT("method"),TEXT("URokUnityFeatureScreenWidget.OpenMail(true)"));
            Action->SetNumberField(TEXT("step"),Step); Actions.Add(MakeShared<FJsonValueObject>(Action));
            Screen->OpenMail(true); ExpectedPage=4; Enter(37);
        } break;
    case 37: Capture(TEXT("18_legacy_single_attachment")); break;
    case 38: if (Click(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedPage=0; Enter(39); } break;
    case 39: Capture(TEXT("19_city_after_single_close")); break;
    case 40: if (Identity() && Unchanged())
        {
            auto Action=MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"),TEXT("explicit_reference_single"));
            Action->SetStringField(TEXT("method"),TEXT("URokUnityFeatureScreenWidget.OpenMail(true)"));
            Action->SetNumberField(TEXT("step"),Step); Actions.Add(MakeShared<FJsonValueObject>(Action));
            Screen->OpenMail(true); ExpectedPage=4; Enter(41);
        } break;
    case 41: Capture(TEXT("20_legacy_single_reopened")); break;
    case 42: if (Click(Screen.Get(),TEXT("FeatureFrameClose"))) { ExpectedPage=0; Enter(43); } break;
    case 43: Capture(TEXT("21_final_city")); break;
    case 44: Finish(); break;
    default: Finish(TEXT("Invalid mail reward detail audit step")); break;
    }
}

bool URokMailRewardDetailAuditSubsystem::Save(bool Complete)
{
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.mail_reward_detail.runtime.v1"));
    Root->SetBoolField(TEXT("complete"),Complete);
    Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty() && Captures.Num()==CaptureCount);
    Root->SetNumberField(TEXT("expected_capture_count"),CaptureCount);
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("audit_flag"),AuditFlag);
    Root->SetStringField(TEXT("scope"),TEXT("Actual city HUD normal-mail empty state and explicit local reward fixtures, selected rows, local help tips, close/reopen and reviewed claim feedback. UI delegate broadcasts plus bounded virtual Slate pointer hit routes and real Slate captures; no hardware pointer proof, Email_TakeEnclosure request, inventory authority or reward mutation."));
    Root->SetStringField(TEXT("fixture_sha256"),FixtureSha256);
    Root->SetStringField(TEXT("screen_fixture_fingerprint"),ScreenFixtureFingerprint);
    Root->SetStringField(TEXT("claim_reviewed_ui_sha256"),ReviewedUiSourceSha256);
    Root->SetStringField(TEXT("inventory_authority"),TEXT("unsynced_local_projection_only"));
    auto Baseline=MakeShared<FJsonObject>();
    Baseline->SetStringField(TEXT("resources_sha256"),RokTrainArmyMesh::Sha256(Resources));
    Baseline->SetStringField(TEXT("profile_sha256"),RokTrainArmyMesh::Sha256(Profile));
    Baseline->SetNumberField(TEXT("owned_build_queues"),OwnedQueues);
    Baseline->SetNumberField(TEXT("active_upgrades"),0);
    Baseline->SetStringField(TEXT("inventory_projection_fingerprint"),InventoryFingerprint);
    TArray<TSharedPtr<FJsonValue>> Troops;
    for (const auto& Stack:TroopStacks) Troops.Add(MakeShared<FJsonValueString>(RokTrainArmyMesh::Sha256(Stack)));
    Baseline->SetArrayField(TEXT("troop_stack_sha256"),Troops);
    auto BuildingHashes=MakeShared<FJsonObject>();
    for (const auto& Pair:Buildings)
        if (Pair.Key.IsValid()) BuildingHashes->SetStringField(Pair.Key->GetPathName(),RokTrainArmyMesh::Sha256(Pair.Value));
    Baseline->SetObjectField(TEXT("building_state_sha256"),BuildingHashes);
    Root->SetObjectField(TEXT("baseline"),Baseline);
    Root->SetBoolField(TEXT("capture_includes_slate_ui"),true);
    Root->SetObjectField(TEXT("input_hashes"),Inputs.IsValid()?Inputs.ToSharedRef():MakeShared<FJsonObject>());
    Root->SetObjectField(TEXT("local_count_font"),LocalCountFont.IsValid()?LocalCountFont.ToSharedRef():MakeShared<FJsonObject>());
    if (FailureState.IsValid()) Root->SetObjectField(TEXT("failure_state"),FailureState.ToSharedRef());
    Root->SetArrayField(TEXT("captures"),Captures); Root->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const auto& Failure:Failures) Errors.Add(MakeShared<FJsonValueString>(Failure));
    Root->SetArrayField(TEXT("failures"),Errors);
    FString Json;
    return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))
        && FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokMailRewardDetailAuditSubsystem::Finish(const FString& Error)
{
    if (!bOwnsTargetWorld || bFinished) return;
    if (!Error.IsEmpty()) { Failures.Add(Error); if (!FailureState.IsValid()) FailureState=State(); }
    if (Failures.IsEmpty() && Captures.Num()!=CaptureCount) Failures.Add(TEXT("Expected twenty-one real mail screenshots"));
    bFinished=true; const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures) UE_LOG(LogTemp,Error,TEXT("Rok MailRewardDetailAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok MailRewardDetailAudit passed=%d captures=%d output=%s"),Failures.IsEmpty()&&Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
