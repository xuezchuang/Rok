#include "RokBarracksLevelDetailAuditSubsystem.h"
#include "RokBarracksSourceTextWidget.h"
#include "RokBarracksSourceTextData.h"
#include "RokBuildingLevelDetails.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokDataPaths.h"
#include "RokMainHudWidget.h"
#include "RokTrainArmyMeshData.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
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
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 CaptureCount = 10;
constexpr const TCHAR* AuditFlag = TEXT("-RokBarracksLevelDetailAudit");
constexpr const TCHAR* CityMap = TEXT("/Game/Maps/L_RokUnityGameAppCity");

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
bool Visible(const UWidget* Widget)
{
    if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
    for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
        if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
    for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
        if (const auto* User = Cast<UUserWidget>(Outer); User && (!User->IsVisible() || User->GetRenderOpacity() <= 0)) return false;
    return true;
}
FString WidgetText(const UWidget* Widget)
{
    if (const auto* Text = Cast<UTextBlock>(Widget)) return Text->GetText().ToString();
    if (const auto* Text = Cast<URokBarracksSourceTextWidget>(Widget)) return Text->GetText().ToString();
    return FString();
}
TSharedRef<FJsonObject> MakeLevelDetailRectJson(FVector2D Origin, FVector2D Size)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("x"), Origin.X); Result->SetNumberField(TEXT("y"), Origin.Y);
    Result->SetNumberField(TEXT("width"), Size.X); Result->SetNumberField(TEXT("height"), Size.Y);
    return Result;
}
TSharedRef<FJsonObject> WidgetGeometry(const UObject* Context, const UWidget* Widget)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("attached"), Widget != nullptr);
    if (!Widget) return Result;
    Result->SetBoolField(TEXT("visible"), Visible(Widget));
    Result->SetNumberField(TEXT("opacity"), Widget->GetRenderOpacity());
    if (const auto* Text = Cast<UTextBlock>(Widget))
    {
        Result->SetNumberField(TEXT("font_points"),Text->GetFont().Size);
        const FColor SRGB = Text->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true);
        auto Color = MakeShared<FJsonObject>();
        Color->SetNumberField(TEXT("r"),SRGB.R); Color->SetNumberField(TEXT("g"),SRGB.G);
        Color->SetNumberField(TEXT("b"),SRGB.B); Color->SetNumberField(TEXT("a"),SRGB.A);
        Result->SetObjectField(TEXT("display_srgb"),Color);
    }
    if (const auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
    {
        Result->SetObjectField(TEXT("slot_rect"), MakeLevelDetailRectJson(Slot->GetPosition(), Slot->GetSize()));
        Result->SetNumberField(TEXT("z_order"), Slot->GetZOrder());
    }
    const FGeometry Viewport = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Context);
    const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(Context);
    const FGeometry Geometry = Widget->GetCachedGeometry();
    if (Viewport.GetLocalSize().X > 0 && Viewport.GetLocalSize().Y > 0)
    {
        const auto Pixel = [&](FVector2D P)
        {
            const FVector2D Local = Viewport.AbsoluteToLocal(Geometry.LocalToAbsolute(P));
            return FVector2D(Local.X * Pixels.X / Viewport.GetLocalSize().X,
                Local.Y * Pixels.Y / Viewport.GetLocalSize().Y);
        };
        const FVector2D A = Pixel(FVector2D::ZeroVector), B = Pixel(Geometry.GetLocalSize());
        Result->SetObjectField(TEXT("pixel_rect"), MakeLevelDetailRectJson(FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),
            FVector2D(FMath::Abs(B.X-A.X), FMath::Abs(B.Y-A.Y))));
    }
    return Result;
}
TSharedRef<FJsonObject> ImageGeometry(const UObject* Context, const UImage* Widget)
{
    auto Result = WidgetGeometry(Context,Widget);
    if (!Widget) return Result;
    const FSlateBrush& Brush = Widget->GetBrush();
    Result->SetStringField(TEXT("atlas_texture"),Brush.GetResourceObject()?Brush.GetResourceObject()->GetPathName():FString());
    const FBox2f UV = Brush.GetUVRegion();
    auto Region = MakeShared<FJsonObject>();
    Region->SetNumberField(TEXT("min_x"),UV.Min.X); Region->SetNumberField(TEXT("min_y"),UV.Min.Y);
    Region->SetNumberField(TEXT("max_x"),UV.Max.X); Region->SetNumberField(TEXT("max_y"),UV.Max.Y);
    Result->SetObjectField(TEXT("uv"),Region);
    return Result;
}
}

bool URokBarracksLevelDetailAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return TargetWorld(Cast<UWorld>(Outer)) && FParse::Param(FCommandLine::Get(), TEXT("RokBarracksLevelDetailAudit"))
        && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokBarracksLevelDetailAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokBarracksLevelDetailAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokBarracksLevelDetailAuditSubsystem, STATGROUP_Tickables); }
void URokBarracksLevelDetailAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bOwnsTargetWorld = TargetWorld(GetWorld()); if (!bOwnsTargetWorld) return;
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/BarracksLevelDetailAudit")
        / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
    IFileManager::Get().MakeDirectory(*Directory, true);
    ShotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokBarracksLevelDetailAuditSubsystem::ScreenshotProcessed);
}
void URokBarracksLevelDetailAuditSubsystem::Deinitialize()
{
    FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ShotHandle);
    if (bOwnsTargetWorld && !bFinished) Finish(TEXT("Target city world ended before completion"));
    Super::Deinitialize();
}
bool URokBarracksLevelDetailAuditSubsystem::Expect(bool Condition, const TCHAR* Error) { if (!Condition) Finish(Error); return Condition; }
void URokBarracksLevelDetailAuditSubsystem::Enter(int32 Next) { Step = Next; StepStarted = Elapsed; }
bool URokBarracksLevelDetailAuditSubsystem::Identity()
{
    return Expect(TargetWorld(GetWorld()), TEXT("Wrong project/map; no UI action allowed"));
}
bool URokBarracksLevelDetailAuditSubsystem::Begin()
{
    if (!Identity()) return false;
    const TCHAR* Cursor = FCommandLine::Get(); FString Token;
    while (FParse::Token(Cursor, Token, false))
        if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
            && !Token.Equals(AuditFlag, ESearchCase::IgnoreCase))
        { Finish(TEXT("Only the BarracksLevelDetailAudit flag is permitted")); return false; }
    Inputs = MakeShared<FJsonObject>();
    const TCHAR* SourceFiles[] = {
        TEXT("Saved/BarracksParentWindow/next_building_info_button_gap.json"),
        TEXT("Saved/RokUnityMigration/building_level_details.json"),
        TEXT("Saved/RokUnityMigration/BarracksLevelHeaderText/manifest.json"),
        TEXT("Saved/RokUnityMigration/BarracksLevelHeaderText/texts.json"),
        TEXT("Saved/RokUnityMigration/BarracksLevelHeaderText/ReferenceInputs.json"),
        TEXT("Saved/RokUnityMigration/BarracksLevelHeaderText/source_text_meshes.json"),
        TEXT("Source/Rok/RokUnityModalWidget.h"), TEXT("Source/Rok/RokUnityModalWidget.cpp"),
        TEXT("Source/Rok/RokBuildingLevelDetails.h"), TEXT("Source/Rok/RokBuildingLevelDetails.cpp"),
        TEXT("ref/config-tables/编译客户端配置/CSV/BuildingBarracks.csv"),
        TEXT("ref/config-tables/编译客户端配置/CSV/BuildingLevelData.csv")
    };
    for (const TCHAR* Relative : SourceFiles)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes, *(RokDataPaths::ResolveProjectPath(Relative))) && !Bytes.IsEmpty(),
            TEXT("Required exact source input unavailable"))) return false;
        auto File = MakeShared<FJsonObject>();
        File->SetNumberField(TEXT("bytes"), Bytes.Num());
        File->SetStringField(TEXT("sha256"), RokTrainArmyMesh::Sha256(Bytes));
        Inputs->SetObjectField(Relative, File);
    }
    const FString GlyphDirectory = RokDataPaths::Resolve(TEXT("BarracksLevelHeaderText"));
    TArray<FString> GlyphFiles;
    IFileManager::Get().FindFiles(GlyphFiles,*(GlyphDirectory / TEXT("glyph_*.png")),true,false);
    if (!Expect(!GlyphFiles.IsEmpty(),TEXT("Source header glyph atlas PNGs unavailable"))) return false;
    for (const FString& Name : GlyphFiles)
    {
        TArray<uint8> Bytes;
        if (!Expect(FFileHelper::LoadFileToArray(Bytes,*(GlyphDirectory/Name)) && !Bytes.IsEmpty(),
            TEXT("Source header glyph atlas cannot be hashed"))) return false;
        auto File=MakeShared<FJsonObject>();
        File->SetNumberField(TEXT("bytes"),Bytes.Num());
        File->SetStringField(TEXT("sha256"),RokTrainArmyMesh::Sha256(Bytes));
        Inputs->SetObjectField(FString(TEXT("Saved/RokUnityMigration/BarracksLevelHeaderText/"))+Name,File);
    }
    HeaderData = NewObject<URokBarracksSourceTextData>(this);
    FString HeaderError;
    if (!Expect(HeaderData && HeaderData->LoadLevelHeaders(HeaderError),
        TEXT("Source barracks header glyph package unavailable/invalid"))) return false;
    const auto Host = [&](UClass* Class)->UUserWidget*
    {
        TArray<UUserWidget*> All; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, All, Class, false);
        UUserWidget* Result = nullptr; int32 Count = 0;
        for (auto* Widget : All) if (Widget->GetWorld() == GetWorld()) { Result = Widget; ++Count; }
        return Count == 1 ? Result : nullptr;
    };
    Hud = Cast<URokMainHudWidget>(Host(URokMainHudWidget::StaticClass()));
    Modal = Cast<URokUnityModalWidget>(Host(URokUnityModalWidget::StaticClass()));
    const auto* Feature = Cast<URokUnityFeatureScreenWidget>(Host(URokUnityFeatureScreenWidget::StaticClass()));
    auto* Economy = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Hud.IsValid() && Visible(Hud.Get()) && Modal.IsValid() && !Modal->IsModalOpen()
        && Feature && !Feature->IsScreenOpen() && Economy && Runtime, TEXT("Expected unique idle city UI hosts"))) return false;
    int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        bool Tagged = false;
        for (FName Tag : It->Tags) if (Tag.ToString().StartsWith(TEXT("UnityInitId_"))) { Tagged = true; break; }
        if (!Tagged) continue;
        const auto State = Economy->GetBuildingStateForActor(*It);
        if (State.UnityInitId > 0) Buildings.Add(*It, Snapshot(State));
        if (It->ActorHasTag(TEXT("UnityInitId_6")))
        {
            Barracks = *It; ++Count;
            if (!Expect(It->ActorHasTag(TEXT("UnityBuildingType_9")) && State.UnityInitId == 6
                && State.UnityBuildingTypeId == 9 && State.Level == 1 && State.Power == 5
                && !State.bUpgradeInProgress, TEXT("Expected real Init6/type9 level1 barracks"))) return false;
        }
    }
    if (!Expect(Count == 1 && Economy->GetActiveUpgradeCount() == 0, TEXT("Expected unique barracks and idle queues"))) return false;
    OwnedQueues = Economy->GetOwnedBuildQueueCount();
    Profile = Snapshot(Runtime->GetPlayerProfileState()); Resources = Snapshot(Economy->GetCurrentResources()); bBaseline = true;
    Hud->SetSelectedBuilding(Barracks.Get());
    return true;
}
bool URokBarracksLevelDetailAuditSubsystem::Unchanged()
{
    if (!bBaseline) return true;
    auto* Economy = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
    auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
    if (!Expect(Economy && Runtime && Snapshot(Economy->GetCurrentResources()) == Resources
        && Snapshot(Runtime->GetPlayerProfileState()) == Profile && Economy->GetActiveUpgradeCount() == 0
        && Economy->GetOwnedBuildQueueCount() == OwnedQueues, TEXT("Level detail navigation changed economy/profile/queues"))) return false;
    for (const auto& Pair : Buildings)
        if (!Expect(Pair.Key.IsValid() && Snapshot(Economy->GetBuildingStateForActor(Pair.Key.Get())) == Pair.Value,
            TEXT("Level detail navigation changed source building state"))) return false;
    return true;
}
bool URokBarracksLevelDetailAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Name == TEXT("BuildingUpgradeButton") || Name == TEXT("BarracksLevelInfoButton")
        || Name == TEXT("BarracksLevelBackButton") || Name == TEXT("Type2CloseButton"),
        TEXT("Only read-only navigation delegates allowed"))) return false;
    auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
    if (!Expect(Button && Visible(Button) && Button->GetIsEnabled(), TEXT("Missing enabled navigation button"))) return false;
    auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"), Name.ToString());
    Action->SetNumberField(TEXT("frame"), GFrameCounter); Action->SetNumberField(TEXT("step"), Step);
    Action->SetStringField(TEXT("method"), TEXT("UButton.OnClicked.Broadcast"));
    Actions.Add(MakeShared<FJsonValueObject>(Action)); Button->OnClicked.Broadcast();
    return true;
}
bool URokBarracksLevelDetailAuditSubsystem::Validate()
{
    if (!Identity() || !Unchanged()) return false;
    if (!Expect(Modal.IsValid() && Hud.IsValid(), TEXT("Lost city UI host"))) return false;
    const FVector2D Pixels = UWidgetLayoutLibrary::GetViewportSize(this);
    if (!Expect(Pixels.Equals(FVector2D(1440,810),.1), TEXT("Real game viewport must be 1440x810"))) return false;
    const auto* Detail = Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsRoot"));
    const auto* Info = Cast<UButton>(Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButton")));
    const auto* Back = Cast<UButton>(Modal->GetWidgetFromName(TEXT("BarracksLevelBackButton")));
    if (Page == 0)
        return Expect(!Modal->IsModalOpen() && !Detail && !Info && !Back,
            TEXT("Closed preview retained barracks level controls"));
#if !UE_BUILD_SHIPPING
    if (!Expect(Modal->GetSelectedBuildingActorForAudit() == Barracks.Get(),
        TEXT("Upgrade/detail lost Init6 actor context"))) return false;
    if (!Expect(Modal->IsBarracksLevelPageOpenForAudit() == (Page == 2),
        TEXT("Barracks detail page state differs from navigation"))) return false;
#endif
    if (!Expect(Modal->IsModalOpen() && Visible(Modal.Get()) && Detail && Info && Back,
        TEXT("Barracks level controls missing from real upgrade page"))) return false;
    if (Page == 1)
    {
        const auto* Slot = Cast<UCanvasPanelSlot>(Info->Slot);
        const auto* Glyph = Cast<UImage>(Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButtonImage")));
        if (!Expect(!Visible(Detail) && Visible(Info) && !Visible(Back) && Slot
            && Slot->GetPosition().Equals(FVector2D(211,227),.02)
            && Slot->GetSize().Equals(FVector2D(60,60),.02)
            && Glyph && Info->GetContent() == Glyph && Visible(Glyph),
            TEXT("Upgrade detail hit region or glyph differs from source"))) return false;
        const auto GlyphRect = WidgetGeometry(this,Glyph)->GetObjectField(TEXT("pixel_rect"));
        const auto ButtonRect = WidgetGeometry(this,Info)->GetObjectField(TEXT("pixel_rect"));
        const auto Matches = [](const TSharedPtr<FJsonObject>& R,FVector2D Origin,FVector2D Size)
        {
            return FMath::IsNearlyEqual(R->GetNumberField(TEXT("x")),Origin.X,.25)
                && FMath::IsNearlyEqual(R->GetNumberField(TEXT("y")),Origin.Y,.25)
                && FMath::IsNearlyEqual(R->GetNumberField(TEXT("width")),Size.X,.25)
                && FMath::IsNearlyEqual(R->GetNumberField(TEXT("height")),Size.Y,.25);
        };
        if (!Matches(ButtonRect,FVector2D(211,227),FVector2D(60,60))
            || !Matches(GlyphRect,FVector2D(222,238),FVector2D(38,38)))
        {
            FailureState = State();
            Finish(FString::Printf(TEXT("Actual Slate button=(%.2f,%.2f,%.2f,%.2f), glyph=(%.2f,%.2f,%.2f,%.2f); expected (211,227,60,60)/(222,238,38,38)"),
                ButtonRect->GetNumberField(TEXT("x")),ButtonRect->GetNumberField(TEXT("y")),
                ButtonRect->GetNumberField(TEXT("width")),ButtonRect->GetNumberField(TEXT("height")),
                GlyphRect->GetNumberField(TEXT("x")),GlyphRect->GetNumberField(TEXT("y")),
                GlyphRect->GetNumberField(TEXT("width")),GlyphRect->GetNumberField(TEXT("height"))));
            return false;
        }
        return true;
    }
    if (!Expect(Visible(Detail) && !Visible(Info) && Visible(Back),
        TEXT("Detail page/return controls have wrong visibility"))) return false;
    const auto* BackSlot = Cast<UCanvasPanelSlot>(Back->Slot);
    const auto* BackGlyph = Cast<UImage>(Modal->GetWidgetFromName(TEXT("BarracksLevelBackButtonImage")));
    if (!Expect(BackSlot && BackSlot->GetPosition().Equals(FVector2D(179.5,54.5),.02)
        && BackSlot->GetSize().Equals(FVector2D(69,69),.02)
        && BackGlyph && Back->GetContent() == BackGlyph && Visible(BackGlyph),
        TEXT("Detail back button/glyph missing from source position"))) return false;
    const auto BackRect = WidgetGeometry(this,Back)->GetObjectField(TEXT("pixel_rect"));
    const auto BackGlyphRect = WidgetGeometry(this,BackGlyph)->GetObjectField(TEXT("pixel_rect"));
    if (!Expect(FMath::IsNearlyEqual(BackRect->GetNumberField(TEXT("x")),179.5,.25)
        && FMath::IsNearlyEqual(BackRect->GetNumberField(TEXT("y")),54.5,.25)
        && FMath::IsNearlyEqual(BackRect->GetNumberField(TEXT("width")),69,.25)
        && FMath::IsNearlyEqual(BackRect->GetNumberField(TEXT("height")),69,.25)
        && FMath::IsNearlyEqual(BackGlyphRect->GetNumberField(TEXT("x")),179.5,.25)
        && FMath::IsNearlyEqual(BackGlyphRect->GetNumberField(TEXT("y")),54.5,.25)
        && FMath::IsNearlyEqual(BackGlyphRect->GetNumberField(TEXT("width")),69,.25)
        && FMath::IsNearlyEqual(BackGlyphRect->GetNumberField(TEXT("height")),69,.25),
        TEXT("Actual Slate back button/glyph pixels differ from native source"))) return false;
#if !UE_BUILD_SHIPPING
    if (!Expect(Modal->GetBarracksLevelSelectedLevelForAudit() == 1
        && Modal->GetBarracksLevelPageTimeForAudit() >= .24f,
        TEXT("Current level selection or detail animation did not settle"))) return false;
#endif
    const auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsScroll")));
    if (!Expect(Scroll && Visible(Scroll), TEXT("Barracks detail scroll missing"))) return false;
    if ((Step == 5 || Step == 17) && !Expect(FMath::Abs(Scroll->GetScrollOffset()) <= 1.f,
        TEXT("New level-detail instance did not start at first row"))) return false;
    if (Step == 7 && !Expect(Scroll->GetScrollOffset() > 100.f,
        TEXT("Detail list did not scroll toward final rows"))) return false;
    if (Step == 11 && !Expect(ScrolledOffset > 100.f
        && FMath::Abs(Scroll->GetScrollOffset() - ScrolledOffset) <= 1.f,
        TEXT("Same-window back/reopen failed to retain source ListView scroll"))) return false;
    TArray<FString> Headers; TArray<FRokBarracksLevelDetailsRow> Rows; FString Error;
    if (!Expect(RokBuildingLevelDetails::LoadBarracks(Headers, Rows, Error)
        && Headers.Num() == 4 && Rows.Num() == 25,
        TEXT("Source-derived barracks 25-row table unavailable"))) return false;
    for (int32 Column = 0; Column < 4; ++Column)
    {
        const auto* Header = Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BarracksLevelHeader_%d"),Column)));
        const FString Key = FString::Printf(TEXT("header_%d"),Column);
        const auto* Mesh = HeaderData ? HeaderData->FindKey(Key) : nullptr;
        const auto* Region = Modal->GetWidgetFromName(*FString::Printf(TEXT("BarracksLevelHeaderRegion_%d"),Column));
        const auto* Slot = Region ? Cast<UCanvasPanelSlot>(Region->Slot) : nullptr;
        if (!Expect(Header && Header->GetText().ToString() == Headers[Column]
            && Header->HasSourceMesh() && Header->GetSourceKey() == Key
            && Header->GetPaintCount() > 0 && Header->GetLastPaintSourceKey() == Key
            && Mesh && Mesh->Text == Headers[Column] && Mesh->FontSize == 22
            && Mesh->EffectiveFontSize == 24
            && Mesh->Style == 0 && Mesh->Alignment == 4 && Mesh->Size.Equals(FVector2D(250,50),.02)
            && !Mesh->Colors.IsEmpty() && Mesh->Colors[0] == FColor(230,217,180)
            && Slot && Slot->GetPosition().Equals(FVector2D((Column==0?195:Column==1?465:Column==2?725:995),132.85),.02)
            && Slot->GetSize().Equals(FVector2D(250,50),.02),
            TEXT("Barracks source header mesh/text/paint or region differs"))) return false;
    }
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const auto& Row = Rows[Index];
        if (!Expect(Row.Level == Index + 1, TEXT("Barracks source row levels not 1..25"))) return false;
        const FString Values[] = {FText::AsNumber(Row.Level).ToString(), FText::AsNumber(Row.InfantryTrainNumber).ToString(),
            Row.InfantryAttackText, FText::AsNumber(Row.Power).ToString()};
        for (int32 Column = 0; Column < 4; ++Column)
        {
            const auto* Region = Modal->GetWidgetFromName(*FString::Printf(TEXT("BarracksLevelCellRegion_%d_%d"),Row.Level,Column));
            const auto* Slot = Region ? Cast<UCanvasPanelSlot>(Region->Slot) : nullptr;
            if (!Expect(Slot && Slot->GetPosition().Equals(FVector2D((Column==0?-13:Column==1?257:Column==2?517:787),(Row.Level-1)*50),.02)
                && Slot->GetSize().Equals(FVector2D(250,50),.02),
                TEXT("Barracks cell region differs from source row geometry"))) return false;
            const auto* Cell = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("BarracksLevelCell_%d_%d"),Row.Level,Column)));
            if (!Expect(Cell && Cell->GetText().ToString() == Values[Column]
                && FMath::IsNearlyEqual(Cell->GetFont().Size,16.5,.01)
                && Cell->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true) == FColor::White,
                TEXT("Barracks detail cell text/style differs from source"))) return false;
        }
    }
    const auto* Selection = Modal->GetWidgetFromName(TEXT("BarracksLevelCurrentRow"));
    if (!Expect(Selection && Visible(Selection), TEXT("Actual current-level highlight missing"))) return false;
    if (Step == 7 || Step == 11)
    {
        const auto ScrollRect = WidgetGeometry(this,Scroll)->GetObjectField(TEXT("pixel_rect"));
        const auto LastRect = WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelCellRegion_25_0")))->GetObjectField(TEXT("pixel_rect"));
        if (!Expect(LastRect->GetNumberField(TEXT("y")) < ScrollRect->GetNumberField(TEXT("y")) + ScrollRect->GetNumberField(TEXT("height"))
            && LastRect->GetNumberField(TEXT("y")) + LastRect->GetNumberField(TEXT("height")) > ScrollRect->GetNumberField(TEXT("y")),
            TEXT("Last level row remained outside real scroll viewport"))) return false;
    }
    return true;
}
TSharedRef<FJsonObject> URokBarracksLevelDetailAuditSubsystem::State() const
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("page"), Page == 0 ? TEXT("closed") : Page == 1 ? TEXT("upgrade") : TEXT("detail"));
    Result->SetNumberField(TEXT("frame"), GFrameCounter); Result->SetNumberField(TEXT("step"), Step);
    Result->SetBoolField(TEXT("state_unchanged"), true);
    Result->SetObjectField(TEXT("viewport_pixels"), MakeLevelDetailRectJson(FVector2D::ZeroVector,UWidgetLayoutLibrary::GetViewportSize(this)));
    if (Page == 0) return Result;
    const auto* Detail = Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsRoot"));
    const auto* Info = Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButton"));
    const auto* Back = Modal->GetWidgetFromName(TEXT("BarracksLevelBackButton"));
    Result->SetObjectField(TEXT("detail_root"), WidgetGeometry(this,Detail));
    Result->SetObjectField(TEXT("info_button"), WidgetGeometry(this,Info));
    Result->SetObjectField(TEXT("info_glyph"), ImageGeometry(this,Cast<UImage>(Modal->GetWidgetFromName(TEXT("BarracksLevelInfoButtonImage")))));
    Result->SetObjectField(TEXT("back_button"), WidgetGeometry(this,Back));
    Result->SetObjectField(TEXT("back_glyph"), ImageGeometry(this,Cast<UImage>(Modal->GetWidgetFromName(TEXT("BarracksLevelBackButtonImage")))));
    Result->SetObjectField(TEXT("description_root"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelDescriptionRoot"))));
    Result->SetObjectField(TEXT("mode_canvas"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("ModalModeCanvas"))));
    Result->SetObjectField(TEXT("viewport_geometry"), WidgetGeometry(this,Modal.Get()));
#if !UE_BUILD_SHIPPING
    Result->SetBoolField(TEXT("detail_open"), Modal->IsBarracksLevelPageOpenForAudit());
    Result->SetNumberField(TEXT("selected_level"), Modal->GetBarracksLevelSelectedLevelForAudit());
    Result->SetNumberField(TEXT("page_time"), Modal->GetBarracksLevelPageTimeForAudit());
    Result->SetNumberField(TEXT("diagnostic_scroll_offset"), Modal->GetBarracksLevelScrollOffsetForAudit());
#endif
    if (Page != 2) return Result;
    const auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsScroll")));
    Result->SetObjectField(TEXT("scroll"), WidgetGeometry(this,Scroll));
    Result->SetNumberField(TEXT("scroll_offset"), Scroll->GetScrollOffset());
    Result->SetObjectField(TEXT("current_row"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelCurrentRow"))));
    Result->SetObjectField(TEXT("first_row"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelCellRegion_1_0"))));
    Result->SetObjectField(TEXT("last_row"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelCellRegion_25_0"))));
    Result->SetObjectField(TEXT("first_row_text"), WidgetGeometry(this,Modal->GetWidgetFromName(TEXT("BarracksLevelCell_1_0"))));
    TArray<TSharedPtr<FJsonValue>> HeaderJson;
    for (int32 Column=0;Column<4;++Column)
    {
        const FString Name=FString::Printf(TEXT("BarracksLevelHeader_%d"),Column);
        auto H=WidgetGeometry(this,Modal->GetWidgetFromName(*Name)); H->SetStringField(TEXT("text"),WidgetText(Modal->GetWidgetFromName(*Name)));
        const auto* Header=Cast<URokBarracksSourceTextWidget>(Modal->GetWidgetFromName(*Name));
        const FString Key=FString::Printf(TEXT("header_%d"),Column);
        const auto* Mesh=HeaderData?HeaderData->FindKey(Key):nullptr;
        H->SetBoolField(TEXT("source_mesh"),Header&&Header->HasSourceMesh());
        H->SetStringField(TEXT("source_key"),Header?Header->GetSourceKey():FString());
        H->SetStringField(TEXT("paint_key"),Header?Header->GetLastPaintSourceKey():FString());
        H->SetNumberField(TEXT("paint_count"),Header?Header->GetPaintCount():0);
        if (Mesh)
        {
            H->SetNumberField(TEXT("source_font_px"),Mesh->FontSize);
            H->SetNumberField(TEXT("source_effective_font_px"),Mesh->EffectiveFontSize);
            H->SetNumberField(TEXT("source_style"),Mesh->Style);
            H->SetNumberField(TEXT("source_alignment"),Mesh->Alignment);
            if (!Mesh->Colors.IsEmpty())
            {
                auto Color=MakeShared<FJsonObject>();
                Color->SetNumberField(TEXT("r"),Mesh->Colors[0].R);Color->SetNumberField(TEXT("g"),Mesh->Colors[0].G);
                Color->SetNumberField(TEXT("b"),Mesh->Colors[0].B);Color->SetNumberField(TEXT("a"),Mesh->Colors[0].A);
                H->SetObjectField(TEXT("source_color_srgb"),Color);
            }
        }
        H->SetObjectField(TEXT("source_region"),WidgetGeometry(this,Modal->GetWidgetFromName(*FString::Printf(TEXT("BarracksLevelHeaderRegion_%d"),Column))));
        HeaderJson.Add(MakeShared<FJsonValueObject>(H));
    }
    Result->SetArrayField(TEXT("headers"),HeaderJson);
    TArray<TSharedPtr<FJsonValue>> RowJson;
    for (int32 Level=1;Level<=25;++Level)
    {
        auto R=MakeShared<FJsonObject>(); R->SetNumberField(TEXT("level"),Level);
        TArray<TSharedPtr<FJsonValue>> Cells;
        for (int32 Column=0;Column<4;++Column)
        {
            const FString Name=FString::Printf(TEXT("BarracksLevelCell_%d_%d"),Level,Column);
            Cells.Add(MakeShared<FJsonValueString>(WidgetText(Modal->GetWidgetFromName(*Name))));
        }
        R->SetArrayField(TEXT("cells"),Cells); RowJson.Add(MakeShared<FJsonValueObject>(R));
    }
    Result->SetArrayField(TEXT("rows"),RowJson);
    return Result;
}
void URokBarracksLevelDetailAuditSubsystem::Capture(const TCHAR* Name)
{
    if (!Validate()) return;
    if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Conflicting screenshot request")); return; }
    PendingPath = Directory / (FString(Name) + TEXT(".png"));
    Pending = MakeShared<FJsonObject>(); Pending->SetStringField(TEXT("file"), PendingPath);
    Pending->SetObjectField(TEXT("request"), State()); ShotStarted = Elapsed;
    FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokBarracksLevelDetailAuditSubsystem::ScreenshotProcessed()
{
    if (!bOwnsTargetWorld || bFinished || !Pending.IsValid() || !Validate()) return;
    TArray<uint8> Bytes; const uint8 Signature[] = { 137,80,78,71,13,10,26,10 };
    if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24
        || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0) { Finish(TEXT("Missing processed real UI PNG")); return; }
    const auto U32 = [&](int32 Offset) { return (uint32(Bytes[Offset])<<24) | (uint32(Bytes[Offset+1])<<16)
        | (uint32(Bytes[Offset+2])<<8) | uint32(Bytes[Offset+3]); };
    if (U32(16) != 1440 || U32(20) != 810) { Finish(TEXT("Processed viewport must be 1440x810")); return; }
    Pending->SetStringField(TEXT("png_sha256"), RokTrainArmyMesh::Sha256(Bytes));
    Pending->SetNumberField(TEXT("file_bytes"), Bytes.Num());
    Pending->SetObjectField(TEXT("processed"), State());
    Captures.Add(MakeShared<FJsonValueObject>(Pending)); Pending.Reset();
    if (!Save(false)) { Finish(TEXT("Incremental level-detail manifest write failed")); return; }
    Enter(Step+1);
}
void URokBarracksLevelDetailAuditSubsystem::Tick(float DeltaTime)
{
    if (!bOwnsTargetWorld || bFinished || !GetWorld() || !GetWorld()->HasBegunPlay()) return;
    Elapsed += FMath::Max(0.f,DeltaTime);
    if (Elapsed > 70) { Finish(TEXT("Barracks level-detail audit timeout")); return; }
    if (Pending.IsValid()) { if (Elapsed-ShotStarted > 10) Finish(TEXT("Level-detail screenshot callback timeout")); return; }
    if (Elapsed < 3 || Elapsed-StepStarted < .5) return;
    if (!Unchanged()) return;
    switch (Step)
    {
    case 0: if (Begin()) Enter(1); break;
    case 1: Capture(TEXT("01_selected_city")); break;
    case 2: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))) { Page=1; Enter(3); } break;
    case 3: Capture(TEXT("02_barracks_upgrade")); break;
    case 4: if (Click(Modal.Get(),TEXT("BarracksLevelInfoButton"))) { Page=2; Enter(5); } break;
    case 5: Capture(TEXT("03_level_detail_initial")); break;
    case 6:
        if (Validate())
        {
            auto* Scroll=Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsScroll")));
            if (!Expect(Scroll!=nullptr,TEXT("Cannot scroll missing detail list"))) break;
            Scroll->ScrollToEnd(); Enter(7);
        }
        break;
    case 7: Capture(TEXT("04_level_detail_last_rows")); break;
    case 8:
        ScrolledOffset=Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("BarracksLevelDetailsScroll")))->GetScrollOffset();
        if (Click(Modal.Get(),TEXT("BarracksLevelBackButton"))) { Page=1; Enter(9); }
        break;
    case 9: Capture(TEXT("05_back_to_upgrade")); break;
    case 10: if (Click(Modal.Get(),TEXT("BarracksLevelInfoButton"))) { Page=2; Enter(11); } break;
    case 11: Capture(TEXT("06_level_detail_reopened_retained")); break;
    case 12: if (Click(Modal.Get(),TEXT("Type2CloseButton"))) { Page=0; Enter(13); } break;
    case 13: Capture(TEXT("07_closed_city")); break;
    case 14: if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))),TEXT("BuildingUpgradeButton"))) { Page=1; Enter(15); } break;
    case 15: Capture(TEXT("08_new_upgrade_window")); break;
    case 16: if (Click(Modal.Get(),TEXT("BarracksLevelInfoButton"))) { Page=2; Enter(17); } break;
    case 17: Capture(TEXT("09_new_window_detail_reset")); break;
    case 18: if (Click(Modal.Get(),TEXT("Type2CloseButton"))) { Page=0; Enter(19); } break;
    case 19: Capture(TEXT("10_final_city")); break;
    case 20: Finish(); break;
    default: Finish(TEXT("Invalid level-detail audit step")); break;
    }
}
bool URokBarracksLevelDetailAuditSubsystem::Save(bool Complete)
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),TEXT("rok.barracks.level_detail.runtime.v3"));
    Root->SetBoolField(TEXT("complete"),Complete);
    Root->SetBoolField(TEXT("passed"),Complete && Failures.IsEmpty() && Captures.Num()==CaptureCount);
    Root->SetNumberField(TEXT("expected_capture_count"),CaptureCount);
    Root->SetStringField(TEXT("project"),FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    Root->SetStringField(TEXT("world"),GetWorld()->GetOutermost()->GetName());
    Root->SetStringField(TEXT("audit_flag"),AuditFlag);
    Root->SetStringField(TEXT("scope"),TEXT("Real Init6/type9 HUD upgrade and all-level detail. Same-window back/reopen retains ListView scroll; whole-window close/reopen resets it. Navigation delegate broadcasts and real Slate screenshots; no physical pointer hit test or economy command."));
    Root->SetStringField(TEXT("header_render_pipeline"),TEXT("Four source-mesh headers use the standard Slate vertex-color path with source Color32 E6D9B4; no extra NoGamma conversion. Runtime PNG opaque cores must match Unity source RGB per channel within 1."));
    Root->SetBoolField(TEXT("capture_includes_slate_ui"),true);
    Root->SetBoolField(TEXT("owns_target_world"),bOwnsTargetWorld);
    Root->SetObjectField(TEXT("input_hashes"), Inputs.IsValid()?Inputs.ToSharedRef():MakeShared<FJsonObject>());
    if (FailureState.IsValid()) Root->SetObjectField(TEXT("failure_state"),FailureState.ToSharedRef());
    Root->SetArrayField(TEXT("captures"),Captures); Root->SetArrayField(TEXT("actions"),Actions);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Failure : Failures) Errors.Add(MakeShared<FJsonValueString>(Failure));
    Root->SetArrayField(TEXT("failures"),Errors);
    FString Json; return FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json))
        && FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("manifest.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokBarracksLevelDetailAuditSubsystem::Finish(const FString& Error)
{
    if (!bOwnsTargetWorld || bFinished) return;
    if (!Error.IsEmpty()) Failures.Add(Error);
    if (Failures.IsEmpty() && Captures.Num()!=CaptureCount) Failures.Add(TEXT("Expected ten real UI screenshots"));
    bFinished=true; const bool Written=Save(Failures.IsEmpty());
    for (const auto& Failure:Failures) UE_LOG(LogTemp,Error,TEXT("Rok BarracksLevelDetailAudit: %s"),*Failure);
    UE_LOG(LogTemp,Display,TEXT("Rok BarracksLevelDetailAudit passed=%d captures=%d output=%s"),Failures.IsEmpty()&&Written,Captures.Num(),*Directory);
    FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()&&Written?0:1);
}
