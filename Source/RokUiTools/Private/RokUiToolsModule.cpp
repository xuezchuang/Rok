#include "RokUiWbpGenerator.h"

#include "RokUiSpec.h"
#include "RokUiWidgets.h"
#include "RokUnityModalWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogRokUiWbp, Log, All);

namespace
{
	struct FModalEntry
	{
		void (*Open)(URokUnityModalWidget&);
		/** Widget-name prefix the mode passes to BuildDefinitionList. */
		const TCHAR* Prefix;
	};

	/** Same keys as -RokOpenModal= (URokMainHudWidget::OpenCommandLineModal). */
	const FModalEntry* FindModal(const FString& Key)
	{
		static const TMap<FString, FModalEntry> Modals = {
			{ TEXT("guild_purview"), { [](URokUnityModalWidget& M) { M.OpenGuildPurview(); }, TEXT("GuildPurview") } },
			{ TEXT("event_date"), { [](URokUnityModalWidget& M) { M.OpenEventDate(); }, TEXT("EventDate") } },
			{ TEXT("vip_privilege"), { [](URokUnityModalWidget& M) { M.OpenVipPrivilege(); }, TEXT("VipPrivilege") } },
			{ TEXT("tavern_rate"), { [](URokUnityModalWidget& M) { M.OpenTavernBoxDesc(); }, TEXT("TavernBoxDesc") } },
			{ TEXT("item_collection"), { [](URokUnityModalWidget& M) { M.OpenItemCollection(); }, TEXT("ItemCollection") } },
			{ TEXT("vip_store"), { [](URokUnityModalWidget& M) { M.OpenVipStore(); }, TEXT("VipStore") } },
			{ TEXT("guild_store"), { [](URokUnityModalWidget& M) { M.OpenGuildStore(); }, TEXT("GuildStore") } },
			{ TEXT("expedition_store"), { [](URokUnityModalWidget& M) { M.OpenExpeditionStore(); }, TEXT("ExpeditionStore") } },
		};
		return Modals.Find(Key.ToLower());
	}

	/** Open the actual modal runtime path, rooted while the tool runs. */
	TStrongObjectPtr<URokUnityModalWidget> OpenModal(const FString& Key, bool bRequireLegacy = true)
	{
		if (bRequireLegacy && !URokUiSettings::IsLegacyForced())
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: run with -RokUiLegacy so the source UI is the C++ build"));
			return nullptr;
		}
		const FModalEntry* Entry = FindModal(Key);
		if (!Entry)
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: unknown modal key %s"), *Key);
			return nullptr;
		}
		// Rooted: compiling the new blueprint runs garbage collection.
		TStrongObjectPtr<URokUnityModalWidget> Modal(NewObject<URokUnityModalWidget>(
			GetTransientPackage(), URokUnityModalWidget::ResolveShellClass().Get()));
		Modal->Initialize();
		Modal->TakeWidget();
		Entry->Open(*Modal.Get());
		if (!Modal->IsModalOpen())
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: modal %s did not open"), *Key);
			return nullptr;
		}
#if WITH_DEV_AUTOMATION_TESTS
		// Settle the open curve so captured render transforms are the resting ones.
		Modal->AdvanceWindowAnimationForTesting(5.0f);
#endif
		return Modal;
	}

	FString TreeDumpPath(const FString& Name)
	{
		return FPaths::ProjectSavedDir() / TEXT("RokUi/Trees") / (Name + TEXT(".json"));
	}

	/** Read-only asset inspection; omit -RokUiLegacy to exercise configured WBP pieces. */
	void DumpModalTree(const TArray<FString>& Args)
	{
		if (Args.Num() != 2)
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("usage: Rok.UiWbp.DumpModalTree <modal key> <dump name>"));
			return;
		}
		// Restrict output to Trees: no path traversal or arbitrary file overwrite.
		if (Args[1].IsEmpty() || Args[1] == TEXT(".") || Args[1] == TEXT("..")
			|| Args[1].Contains(TEXT("/")) || Args[1].Contains(TEXT("\\")) || Args[1].Contains(TEXT(":")))
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: dump name must be a filename stem"));
			return;
		}
		const TStrongObjectPtr<URokUnityModalWidget> Modal = OpenModal(Args[0], false);
		if (!Modal)
		{
			return;
		}
		const FString Filename = TreeDumpPath(Args[1]);
		const bool bSaved = IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true)
			&& RokUiWbp::DumpTree(Modal->GetModeCanvas(), Filename);
		if (bSaved)
		{
			UE_LOG(LogRokUiWbp, Display, TEXT("RokUiWbp: dumped modal %s (%s) to %s"), *Args[0],
				URokUiSettings::IsLegacyForced() ? TEXT("legacy forced") : TEXT("configured WBP path"), *Filename);
		}
		else
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: tree dump failed: %s"), *Filename);
		}
		Modal->CloseModal();
	}

	void LogReport(const RokUiWbp::FGenerateRequest& Request, const UWidgetBlueprint* Blueprint, const RokUiWbp::FCopyReport& Report)
	{
		for (const FString& Warning : Report.Warnings)
		{
			UE_LOG(LogRokUiWbp, Warning, TEXT("RokUiWbp: %s"), *Warning);
		}
		UE_LOG(LogRokUiWbp, Display, TEXT("RokUiWbp: %s %s widgets=%d role_texts=%d anchored_slices=%d warnings=%d"),
			Blueprint ? TEXT("generated") : TEXT("FAILED"), *Request.AssetPath,
			Report.Widgets, Report.RoleTexts, Report.AnchoredSlices, Report.Warnings.Num());
	}

	TArray<const UWidget*> ChildrenOf(const UPanelWidget* Panel)
	{
		TArray<const UWidget*> Children;
		for (int32 Index = 0; Panel && Index < Panel->GetChildrenCount(); ++Index)
		{
			Children.Add(Panel->GetChildAt(Index));
		}
		return Children;
	}

	/** Rok.UiWbp.GenerateModalPage <key> <asset>: the whole mode as one static page (spike). */
	void GenerateModalPage(const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("usage: Rok.UiWbp.GenerateModalPage <modal key> <asset path>"));
			return;
		}
		const TStrongObjectPtr<URokUnityModalWidget> Modal = OpenModal(Args[0]);
		if (!Modal)
		{
			return;
		}
		RokUiWbp::FGenerateRequest Request;
		Request.AssetPath = Args[1];
		Request.ParentClass = UUserWidget::StaticClass();
		Request.Sources = ChildrenOf(Modal->GetModeCanvas());
		RokUiWbp::FCopyReport Report;
		LogReport(Request, RokUiWbp::Generate(Request, Report), Report);
		Modal->CloseModal();
	}

	/**
	 * Rok.UiWbp.GenerateModalPiece <key> <piece> <asset>: one reusable piece of
	 * a C++-built modal mode. Pieces: frame (Type2 window), list (definition
	 * list without rows), row / row_icon / card_icon (first matching row).
	 */
	void GenerateModalPiece(const TArray<FString>& Args)
	{
		if (Args.Num() < 3)
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("usage: Rok.UiWbp.GenerateModalPiece <modal key> <frame|list|row|row_icon|card_icon> <asset path>"));
			return;
		}
		const FString& Key = Args[0];
		const FString Piece = Args[1].ToLower();
		const TStrongObjectPtr<URokUnityModalWidget> Modal = OpenModal(Key);
		if (!Modal)
		{
			return;
		}
		const FString Prefix = FindModal(Key)->Prefix;
		UCanvasPanel* ModeCanvas = Modal->GetModeCanvas();
		RokUiWbp::DumpTree(ModeCanvas, TreeDumpPath(FString::Printf(TEXT("modal_%s_legacy"), *Key)));

		RokUiWbp::FGenerateRequest Request;
		Request.AssetPath = Args[2];
		if (Piece == TEXT("frame"))
		{
			Request.ParentClass = URokModalWindowFrame::StaticClass();
			Request.RootName = TEXT("FrameRoot");
			Request.Renames = { { TEXT("Type2Title"), TEXT("Title") }, { TEXT("Type2CloseButton"), TEXT("CloseButton") } };
			for (const UWidget* Child : ChildrenOf(ModeCanvas))
			{
				if (Child && Child->GetName().StartsWith(TEXT("Type2")))
				{
					Request.Sources.Add(Child);
				}
			}
		}
		else if (Piece == TEXT("list"))
		{
			Request.ParentClass = URokDefinitionListWidget::StaticClass();
			Request.RootName = TEXT("ListRoot");
			Request.Prefix = Prefix;
			Request.SkipChildrenOf = { Prefix + TEXT("ScrollContent") };
			const TSet<FString> Parts = { Prefix + TEXT("Header"), TEXT("DefinitionSearchPlate"),
				Prefix + TEXT("Search"), Prefix + TEXT("Scroll"), Prefix + TEXT("Footer") };
			for (const UWidget* Child : ChildrenOf(ModeCanvas))
			{
				if (Child && Parts.Contains(Child->GetName()))
				{
					Request.Sources.Add(Child);
				}
			}
		}
		else if (Piece == TEXT("row") || Piece == TEXT("row_icon") || Piece == TEXT("card_icon"))
		{
			const bool bWantIcon = Piece != TEXT("row");
			const UPanelWidget* Rows = Cast<UPanelWidget>(Modal->GetWidgetFromName(*(Prefix + TEXT("ScrollContent"))));
			for (const UWidget* Row : ChildrenOf(Rows))
			{
				const FString RowName = Row ? Row->GetName() : FString();
				const FString Index = RowName.RightChop(RowName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd));
				const bool bHasIcon = Modal->GetWidgetFromName(*(Prefix + TEXT("RowIcon") + Index)) != nullptr;
				if (bHasIcon == bWantIcon)
				{
					Request.Suffix = Index;
					Request.Sources = ChildrenOf(Cast<UPanelWidget>(Row));
					break;
				}
			}
			Request.ParentClass = URokDefinitionRowWidget::StaticClass();
			Request.RootName = TEXT("RowRoot");
			Request.Prefix = Prefix;
			Request.FillParent = { TEXT("RowBackground") };
		}
		else
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: unknown piece %s"), *Piece);
			return;
		}

		RokUiWbp::FCopyReport Report;
		LogReport(Request, RokUiWbp::Generate(Request, Report), Report);
		Modal->CloseModal();
	}

	/** A bare name is Saved/RokUi/Specs/<name>.json; a .json path is used as given (relative to the project). */
	FString SpecPath(const FString& Name)
	{
		if (Name.EndsWith(TEXT(".json")))
		{
			return FPaths::IsRelative(Name) ? FPaths::ProjectDir() / Name : Name;
		}
		return FPaths::ProjectSavedDir() / TEXT("RokUi/Specs") / (Name + TEXT(".json"));
	}

	/**
	 * Rok.UiWbp.GeneratePage <asset> <spec> [more specs] [-SkipChildren=A+B]: a mode page
	 * (URokUiPageWidget) from widget specs written by the game (-RokUiExportSpec=), one
	 * per state of the mode. Later specs merge by widget name; widgets only they add
	 * start collapsed. -SkipChildren leaves out the contents of panels that C++ fills
	 * with row WBPs (+ separates names: -ExecCmds splits on commas). -Exclude leaves out
	 * widgets at any depth, e.g. a window frame that is its own WBP. Create-only.
	 * Items (one row, card or cell that C++ places N times) use the same command:
	 * -Root=<widget> takes that widget's children instead of the top level (a row
	 * canvas), and -Prefix= / -Suffix= / -Index= strip the mode prefix, the data id
	 * and the list index from names; the old flat names stay findable (LegacyNames).
	 */
	void GeneratePage(const TArray<FString>& Args)
	{
		TArray<FString> Specs;
		TSet<FString> Exclude;
		FString RootSource;
		FString ParentClassPath;
		RokUiWbp::FGenerateRequest Request;
		for (const FString& Arg : Args)
		{
			FString Value;
			if (FParse::Value(*Arg, TEXT("-Root="), Value))
			{
				RootSource = Value;
			}
			else if (FParse::Value(*Arg, TEXT("-RootName="), Value))
			{
				Request.RootName = FName(*Value);
			}
			else if (FParse::Value(*Arg, TEXT("-ParentClass="), Value))
			{
				ParentClassPath = Value;
			}
			else if (FParse::Value(*Arg, TEXT("-Prefix="), Value))
			{
				Request.Prefix = Value;
			}
			else if (FParse::Value(*Arg, TEXT("-Suffix="), Value))
			{
				Request.Suffix = Value;
			}
			else if (FParse::Value(*Arg, TEXT("-Index="), Value))
			{
				Request.IndexSuffix = Value;
			}
			else if (FParse::Value(*Arg, TEXT("-SkipChildren="), Value))
			{
				TArray<FString> Names;
				Value.ParseIntoArray(Names, TEXT("+"));
				Request.SkipChildrenOf.Append(Names);
			}
			else if (FParse::Value(*Arg, TEXT("-Exclude="), Value))
			{
				TArray<FString> Names;
				Value.ParseIntoArray(Names, TEXT("+"));
				Exclude.Append(Names);
			}
			else if (Request.AssetPath.IsEmpty())
			{
				Request.AssetPath = Arg;
			}
			else
			{
				Specs.Add(Arg);
			}
		}
		if (Request.AssetPath.IsEmpty() || Specs.IsEmpty())
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("usage: Rok.UiWbp.GeneratePage <asset path> <spec> [more specs] [-SkipChildren=A+B] [-Exclude=A+B] [-Root=W] [-Prefix=P] [-Suffix=_N] [-Index=_N] [-ParentClass=/Script/Module.Class] [-RootName=W]"));
			return;
		}
		Request.ParentClass = URokUiPageWidget::StaticClass();
		if (!ParentClassPath.IsEmpty())
		{
			UClass* ParentClass = LoadClass<UUserWidget>(nullptr, *ParentClassPath);
			if (!ParentClass)
			{
				UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: %s is not a loadable UserWidget parent class"), *ParentClassPath);
				return;
			}
			Request.ParentClass = ParentClass;
		}

		// Rooted: compiling the new blueprint runs garbage collection.
		TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>(GetTransientPackage(), NAME_None, RF_Transient));
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpecRoot"));
		Tree->RootWidget = Root;
		RokUiWbp::FCopyReport Report;
		for (int32 Index = 0; Index < Specs.Num(); ++Index)
		{
			TArray<FName> Added;
			TArray<FString> Warnings;
			const bool bRead = RokUiSpec::ImportChildren(SpecPath(Specs[Index]), Tree.Get(), Root, Added, Warnings);
			for (const FString& Warning : Warnings)
			{
				Report.Warnings.Add(FString::Printf(TEXT("spec %s: %s"), *Specs[Index], *Warning));
			}
			if (!bRead)
			{
				LogReport(Request, nullptr, Report);
				return;
			}
			int32 Collapsed = 0;
			if (Index > 0)
			{
				const TSet<FName> AddedSet(Added);
				for (const FName Name : Added)
				{
					const UWidget* Widget = Tree->FindWidget(Name);
					const UWidget* Parent = Widget ? Widget->GetParent() : nullptr;
					if (Parent && !AddedSet.Contains(Parent->GetFName()))
					{
						Request.Collapse.Add(Name.ToString());
						++Collapsed;
					}
				}
			}
			UE_LOG(LogRokUiWbp, Display, TEXT("RokUiWbp: spec %s added %d widgets (%d start collapsed)"), *Specs[Index], Added.Num(), Collapsed);
		}
		// Excluded widgets are left out wherever they are (a frame part at the top,
		// a C++-filled pane inside a panel).
		for (const FString& Name : Exclude)
		{
			if (UWidget* Widget = Tree->FindWidget(FName(*Name)))
			{
				Widget->RemoveFromParent();
			}
			else
			{
				UE_LOG(LogRokUiWbp, Warning, TEXT("RokUiWbp: -Exclude %s is not in the specs"), *Name);
			}
		}
		const UPanelWidget* SourceRoot = Root;
		if (!RootSource.IsEmpty())
		{
			SourceRoot = Cast<UPanelWidget>(Tree->FindWidget(FName(*RootSource)));
			if (!SourceRoot)
			{
				UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: no panel %s in the specs"), *RootSource);
				return;
			}
		}
		for (const UWidget* Child : ChildrenOf(SourceRoot))
		{
			if (Child)
			{
				Request.Sources.Add(Child);
			}
		}
		LogReport(Request, RokUiWbp::Generate(Request, Report), Report);
	}

	/**
	 * Rok.UiWbp.GenerateFrame <asset> <spec> <title> <close button> <part+part...>: a modal
	 * window frame (URokModalWindowFrame) from the top-level widgets of a spec. The
	 * title and close button are renamed Title / CloseButton (their old names stay
	 * findable through LegacyNames); the parts keep their names. Create-only.
	 */
	void GenerateFrame(const TArray<FString>& Args)
	{
		if (Args.Num() < 5)
		{
			UE_LOG(LogRokUiWbp, Error, TEXT("usage: Rok.UiWbp.GenerateFrame <asset path> <spec> <title> <close button> <part+part...>"));
			return;
		}
		TStrongObjectPtr<UWidgetTree> Tree(NewObject<UWidgetTree>(GetTransientPackage(), NAME_None, RF_Transient));
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpecRoot"));
		Tree->RootWidget = Root;
		RokUiWbp::FCopyReport Report;
		RokUiWbp::FGenerateRequest Request;
		Request.AssetPath = Args[0];
		TArray<FName> Added;
		TArray<FString> Warnings;
		const bool bRead = RokUiSpec::ImportChildren(SpecPath(Args[1]), Tree.Get(), Root, Added, Warnings);
		for (const FString& Warning : Warnings)
		{
			Report.Warnings.Add(FString::Printf(TEXT("spec %s: %s"), *Args[1], *Warning));
		}
		if (!bRead)
		{
			LogReport(Request, nullptr, Report);
			return;
		}
		TArray<FString> Parts;
		Args[4].ParseIntoArray(Parts, TEXT("+"));
		Parts.Add(Args[2]);
		Parts.Add(Args[3]);
		for (const FString& Part : Parts)
		{
			const UWidget* Widget = Tree->FindWidget(FName(*Part));
			if (!Widget || Widget->GetParent() != Root)
			{
				UE_LOG(LogRokUiWbp, Error, TEXT("RokUiWbp: spec %s has no top-level widget %s"), *Args[1], *Part);
				return;
			}
			Request.Sources.Add(Widget);
		}
		Request.ParentClass = URokModalWindowFrame::StaticClass();
		Request.RootName = TEXT("FrameRoot");
		Request.Renames = { { Args[2], TEXT("Title") }, { Args[3], TEXT("CloseButton") } };
		LogReport(Request, RokUiWbp::Generate(Request, Report), Report);
	}
}

class FRokUiToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rok.UiWbp.DumpModalTree"),
			TEXT("Rok.UiWbp.DumpModalTree <modal key> <dump name>: dump the live modal tree, including nested WBPs. Use -RokUiLegacy for the old path."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&DumpModalTree)));
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rok.UiWbp.GenerateModalPage"),
			TEXT("Rok.UiWbp.GenerateModalPage <modal key> <asset path>: copy a whole C++-built modal mode into a static WBP (create-only)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&GenerateModalPage)));
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rok.UiWbp.GenerateModalPiece"),
			TEXT("Rok.UiWbp.GenerateModalPiece <modal key> <frame|list|row|row_icon|card_icon> <asset path>: copy one reusable piece of a C++-built modal into a WBP (create-only)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&GenerateModalPiece)));
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rok.UiWbp.GeneratePage"),
			TEXT("Rok.UiWbp.GeneratePage <asset path> <spec> [more specs] [-SkipChildren=A+B] [-Exclude=A+B] [-Root=W] [-Prefix=P] [-Suffix=_N] [-Index=_N] [-ParentClass=/Script/Module.Class] [-RootName=W]: a mode page or item WBP from runtime widget specs (create-only)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&GeneratePage)));
		Commands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("Rok.UiWbp.GenerateFrame"),
			TEXT("Rok.UiWbp.GenerateFrame <asset path> <spec> <title> <close button> <part+part...>: a modal window frame WBP from a runtime widget spec (create-only)."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&GenerateFrame)));
	}

	virtual void ShutdownModule() override
	{
		for (IConsoleObject* Command : Commands)
		{
			IConsoleManager::Get().UnregisterConsoleObject(Command);
		}
		Commands.Reset();
	}

private:
	TArray<IConsoleObject*> Commands;
};

IMPLEMENT_MODULE(FRokUiToolsModule, RokUiTools)
