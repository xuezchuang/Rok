#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RokUiWidgets.generated.h"

class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UImage;
class UScrollBox;
class USizeBox;
class UTextBlock;

/**
 * Base of the Rok Widget Blueprints that replace C++-built UI pieces.
 *
 * Before the WBP conversion every widget had one flat, unique name inside its
 * host (e.g. "GuildPurviewRowTitle_0"). A WBP piece names its children
 * generically ("RowTitle"); LegacyNames maps a child name to its old flat
 * name, where {Prefix} and {Suffix} are filled from the instance. RokUiFind
 * uses it, so tests and lookups that know the old names keep working.
 */
UCLASS(Abstract)
class ROK_API URokUiReusableWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Child name -> pre-WBP name pattern ({Prefix}, {Suffix}). Written by the WBP generator. */
	UPROPERTY(EditDefaultsOnly, Category = "Rok UI|Legacy names")
	TMap<FName, FString> LegacyNames;

	/**
	 * Runtime values substituted into LegacyNames (e.g. "GuildPurview", "_0").
	 * {Index} is for items whose old names carry two numbers, a data id
	 * ({Suffix}, "PlayerHeadCell_1000") and a list index ("PlayerHeadChoiceButton_0").
	 */
	FString LegacyPrefix;
	FString LegacySuffix;
	FString LegacyIndex;

	/** The pre-WBP name of one of this widget's children (the child name itself when unmapped). */
	FName ResolveLegacyName(FName ChildName) const;
};

/** UI_Model_Window_Type2 frame: two mirrored body halves, header, title and close button. */
UCLASS(Abstract)
class ROK_API URokModalWindowFrame : public URokUiReusableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;
};

/** URokUnityModalWidget::BuildDefinitionList: header, optional search, scrolling rows, footer. */
UCLASS(Abstract)
class ROK_API URokDefinitionListWidget : public URokUiReusableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> Header;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> Search;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UScrollBox> Scroll;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<USizeBox> ScrollSize;

	/** Rows are placed here by C++ (one URokDefinitionRowWidget each). */
	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> ScrollContent;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> Footer;
};

/** One definition-list row or grid card: background, optional icon, title, detail, tag. */
UCLASS(Abstract)
class ROK_API URokDefinitionRowWidget : public URokUiReusableWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> RowIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidget))
	TObjectPtr<UTextBlock> RowTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RowDetail;

	UPROPERTY(BlueprintReadOnly, Category = "Rok UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RowTag;
};

class URokUiPageWidget;

/** One mode page: the mode's name in its host (e.g. "ResourceInventory") and its Widget Blueprint. */
USTRUCT()
struct ROK_API FRokUiPageEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rok UI")
	FName Page;

	UPROPERTY(EditAnywhere, Category = "Rok UI")
	TSoftClassPtr<URokUiPageWidget> Class;
};

/**
 * Which Widget Blueprint draws each converted UI piece. An unset entry keeps
 * the C++ build for that piece, so screens can be converted one at a time.
 * -RokUiLegacy forces the C++ build everywhere (used for before/after captures).
 */
UCLASS(Config = Game, DefaultConfig)
class ROK_API URokUiSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowType2;

	/** Barracks window header, drawn above the upgrade and level-detail content. */
	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowType2Barracks;

	/** Barracks window background, drawn below the upgrade and level-detail content. */
	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokUiPageWidget> ModalWindowType2BarracksBody;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowType1;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowType3;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowTypeMid;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokModalWindowFrame> ModalWindowCivilizationChange;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionListWidget> DefinitionList;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionListWidget> DefinitionListWithSearch;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionListWidget> DefinitionGridWithSearch;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionRowWidget> DefinitionRow;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionRowWidget> DefinitionRowWithIcon;

	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TSoftClassPtr<URokDefinitionRowWidget> DefinitionCardWithIcon;

	/**
	 * Mode bodies of URokUnityModalWidget drawn by a page WBP. Converted modes
	 * have no C++ layout left, so -RokUiLegacy does not apply to pages.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Modal")
	TArray<FRokUiPageEntry> ModalPages;

	/** Feature screen layouts sourced from designer pages. */
	UPROPERTY(Config, EditAnywhere, Category = "Feature")
	TArray<FRokUiPageEntry> FeaturePages;

	/** The page class for a mode, or null (logged once) when it is unmapped or does not load. */
	static UClass* ResolvePage(const TArray<FRokUiPageEntry>& Pages, FName Page);

	/** The class to use for a piece, or null when it is unset, missing, or -RokUiLegacy is on. */
	template <typename T>
	static UClass* Resolve(const TSoftClassPtr<T>& Class)
	{
		if (IsLegacyForced() || Class.IsNull())
		{
			return nullptr;
		}
		UClass* Loaded = Class.LoadSynchronous();
		NoteResolved(Class.ToSoftObjectPath(), Loaded);
		return Loaded;
	}

	static bool IsLegacyForced();

private:
	/** Logs once per path whether a piece is drawn by its WBP or fell back to the C++ build. */
	static void NoteResolved(const FSoftObjectPath& Path, const UClass* Loaded);
};

namespace RokUiFind
{
	/**
	 * Finds a widget by name under Host, descending into nested user widgets.
	 * Inside a URokUiReusableWidget, children also answer to their pre-WBP flat
	 * names (LegacyNames), e.g. "GuildPurviewRowTitle_0".
	 */
	ROK_API UWidget* Find(const UUserWidget* Host, FName Name);

	template <typename T>
	UWidget* Find(const TWeakObjectPtr<T>& Host, FName Name)
	{
		return Find(Host.Get(), Name);
	}

	template <typename T>
	UWidget* Find(const TObjectPtr<T>& Host, FName Name)
	{
		return Find(Host.Get(), Name);
	}

	/** Moves a removed widget out of its outer so a replacement can reuse its name. */
	ROK_API void Retire(UObject* Widget);

	/**
	 * Widget's canvas position inside Ancestor, summed over the canvas slots in
	 * between, across nested Widget Blueprints (a row placed in a page placed in
	 * the host). Assumes top-left anchors, as the converted layouts use.
	 */
	ROK_API FVector2D CanvasPosition(const UWidget* Widget, const UWidget* Ancestor);

	/** True when Widget is a descendant of Ancestor, across nested Widget Blueprints. */
	ROK_API bool IsInside(const UWidget* Widget, const UWidget* Ancestor);
}

/**
 * A whole mode body as a Widget Blueprint: the WBP owns the layout and the
 * static style, C++ fills in data and state by widget name. Widgets that only
 * some states show start collapsed in the WBP; ShownVisibility keeps the
 * visibility they were designed with, which SetShown restores.
 */
UCLASS(Abstract)
class ROK_API URokUiPageWidget : public URokUiReusableWidget
{
	GENERATED_BODY()

public:
	/** Designed visibility of widgets the WBP starts collapsed. Written by the WBP generator. */
	UPROPERTY(EditDefaultsOnly, Category = "Rok UI|Page")
	TMap<FName, ESlateVisibility> ShownVisibility;

	/** A page widget by name; logs an error when it is missing or of another type. */
	template <typename T>
	T* Get(FName Name) const
	{
		T* Widget = Cast<T>(RokUiFind::Find(this, Name));
		if (!Widget)
		{
			ReportMissing(Name, T::StaticClass());
		}
		return Widget;
	}

	/** Shows a widget with its designed visibility, or collapses it. */
	void SetShown(UWidget* Widget, bool bShown) const;

	/**
	 * Shows a widget, or removes it when the data has no such piece (a row
	 * without an icon, a level that is not the current one). Lookups then do not
	 * find it, as with the C++ build that never created it.
	 */
	void SetPresent(UWidget* Widget, bool bPresent) const;

private:
	void ReportMissing(FName Name, const UClass* Expected) const;
};
