#include "RokUiWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"

FName URokUiReusableWidget::ResolveLegacyName(FName ChildName) const
{
	const FString* Pattern = LegacyNames.Find(ChildName);
	if (!Pattern)
	{
		return ChildName;
	}
	FString Name = *Pattern;
	Name.ReplaceInline(TEXT("{Prefix}"), *LegacyPrefix, ESearchCase::CaseSensitive);
	Name.ReplaceInline(TEXT("{Suffix}"), *LegacySuffix, ESearchCase::CaseSensitive);
	Name.ReplaceInline(TEXT("{Index}"), *LegacyIndex, ESearchCase::CaseSensitive);
	return FName(*Name);
}

bool URokUiSettings::IsLegacyForced()
{
	static const bool bLegacy = FParse::Param(FCommandLine::Get(), TEXT("RokUiLegacy"));
	return bLegacy;
}

void URokUiSettings::NoteResolved(const FSoftObjectPath& Path, const UClass* Loaded)
{
	static TSet<FSoftObjectPath> Noted;
	bool bAlreadyNoted = false;
	Noted.Add(Path, &bAlreadyNoted);
	if (bAlreadyNoted)
	{
		return;
	}
	if (Loaded)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok UI: drawing with WBP %s"), *Path.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: WBP %s did not load; using the C++ build"), *Path.ToString());
	}
}

UClass* URokUiSettings::ResolvePage(const TArray<FRokUiPageEntry>& Pages, FName Page)
{
	const FRokUiPageEntry* Entry = Pages.FindByPredicate([Page](const FRokUiPageEntry& Candidate) { return Candidate.Page == Page; });
	if (!Entry || Entry->Class.IsNull())
	{
		static TSet<FName> Reported;
		bool bAlreadyReported = false;
		Reported.Add(Page, &bAlreadyReported);
		if (!bAlreadyReported)
		{
			UE_LOG(LogTemp, Error, TEXT("Rok UI: page %s has no Widget Blueprint in URokUiSettings"), *Page.ToString());
		}
		return nullptr;
	}
	UClass* Loaded = Entry->Class.LoadSynchronous();
	NoteResolved(Entry->Class.ToSoftObjectPath(), Loaded);
	return Loaded;
}

void URokUiPageWidget::SetShown(UWidget* Widget, bool bShown) const
{
	if (!Widget)
	{
		return;
	}
	if (!bShown)
	{
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (const ESlateVisibility* Designed = ShownVisibility.Find(Widget->GetFName()))
	{
		Widget->SetVisibility(*Designed);
	}
	else if (Widget->GetVisibility() == ESlateVisibility::Collapsed || Widget->GetVisibility() == ESlateVisibility::Hidden)
	{
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void URokUiPageWidget::SetPresent(UWidget* Widget, bool bPresent) const
{
	if (!Widget)
	{
		return;
	}
	if (bPresent)
	{
		SetShown(Widget, true);
	}
	else
	{
		Widget->RemoveFromParent();
	}
}

void URokUiPageWidget::ReportMissing(FName Name, const UClass* Expected) const
{
	const UWidget* Found = RokUiFind::Find(this, Name);
	UE_LOG(LogTemp, Error, TEXT("Rok UI: page %s has no %s named %s%s"), *GetClass()->GetName(), *Expected->GetName(),
		*Name.ToString(), Found ? *FString::Printf(TEXT(" (found a %s)"), *Found->GetClass()->GetName()) : TEXT(""));
}

namespace RokUiFind
{
namespace
{
	UWidget* FindInTree(const UWidgetTree* Tree, FName Name, const URokUiReusableWidget* Owner)
	{
		UWidget* Found = nullptr;
		if (!Tree)
		{
			return nullptr;
		}
		Tree->ForEachWidget([&](UWidget* Widget)
		{
			if (Found)
			{
				return;
			}
			const FName Effective = Owner ? Owner->ResolveLegacyName(Widget->GetFName()) : Widget->GetFName();
			if (Effective == Name)
			{
				Found = Widget;
			}
			else if (const UUserWidget* Nested = Cast<UUserWidget>(Widget))
			{
				Found = FindInTree(Nested->WidgetTree, Name, Cast<URokUiReusableWidget>(Nested));
			}
		});
		return Found;
	}
}

UWidget* Find(const UUserWidget* Host, FName Name)
{
	if (!Host)
	{
		return nullptr;
	}
	if (UWidget* Direct = Host->GetWidgetFromName(Name))
	{
		return Direct;
	}
	return FindInTree(Host->WidgetTree, Name, Cast<URokUiReusableWidget>(Host));
}

/** The widget's parent panel, or for a Widget Blueprint's root, the user widget that holds it. */
static const UWidget* OuterParent(const UWidget* Widget)
{
	if (const UWidget* Parent = Widget->GetParent())
	{
		return Parent;
	}
	const UWidgetTree* Tree = Cast<UWidgetTree>(Widget->GetOuter());
	return Tree ? Cast<UUserWidget>(Tree->GetOuter()) : nullptr;
}

FVector2D CanvasPosition(const UWidget* Widget, const UWidget* Ancestor)
{
	FVector2D Position = FVector2D::ZeroVector;
	for (const UWidget* Current = Widget; Current && Current != Ancestor; Current = OuterParent(Current))
	{
		if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Current->Slot))
		{
			Position += CanvasSlot->GetPosition();
		}
	}
	return Position;
}

bool IsInside(const UWidget* Widget, const UWidget* Ancestor)
{
	for (const UWidget* Current = Widget ? OuterParent(Widget) : nullptr; Current; Current = OuterParent(Current))
	{
		if (Current == Ancestor)
		{
			return true;
		}
	}
	return false;
}

void Retire(UObject* Widget)
{
	if (Widget)
	{
		Widget->Rename(nullptr, GetTransientPackage(),
			REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
	}
}
}
