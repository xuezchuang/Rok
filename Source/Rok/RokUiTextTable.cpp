#include "RokUiTextTable.h"

#include "Misc/FileHelper.h"
#include "RokDataPaths.h"

TMap<int32, FRokUiTextTable::FEntry> FRokUiTextTable::Entries;
bool FRokUiTextTable::bLoaded = false;

namespace
{
	// Minimal RFC 4180 splitter: quoted fields may contain commas and doubled
	// quotes. The table has no embedded newlines inside fields.
	void SplitCsvLine(const FString& Line, TArray<FString>& OutFields)
	{
		OutFields.Reset();
		FString Current;
		bool bInQuotes = false;
		for (int32 Index = 0; Index < Line.Len(); ++Index)
		{
			const TCHAR Character = Line[Index];
			if (bInQuotes)
			{
				if (Character == TEXT('"'))
				{
					if (Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
					{
						Current.AppendChar(TEXT('"'));
						++Index;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					Current.AppendChar(Character);
				}
			}
			else if (Character == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Character == TEXT(','))
			{
				OutFields.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Character);
			}
		}
		OutFields.Add(Current);
	}
}

const TMap<int32, FRokUiTextTable::FEntry>& FRokUiTextTable::GetEntries()
{
	if (bLoaded)
	{
		return Entries;
	}
	bLoaded = true;

	FString Csv;
	const FString Path = RokDataPaths::Resolve(TEXT("ConfigTables/Language.csv"));
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI text: language table unavailable at %s"), *Path);
		return Entries;
	}

	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines, false);
	if (Lines.Num() < 2)
	{
		return Entries;
	}
	TArray<FString> Header;
	SplitCsvLine(Lines[0], Header);
	int32 IdColumn = INDEX_NONE;
	int32 CnColumn = INDEX_NONE;
	int32 EnColumn = INDEX_NONE;
	for (int32 Column = 0; Column < Header.Num(); ++Column)
	{
		const FString Name = Header[Column].TrimStartAndEnd().ToLower();
		if (Name == TEXT("id")) IdColumn = Column;
		else if (Name == TEXT("cn")) CnColumn = Column;
		else if (Name == TEXT("en")) EnColumn = Column;
	}
	if (IdColumn == INDEX_NONE || CnColumn == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI text: language table header is not id/cn."));
		return Entries;
	}

	Entries.Reserve(Lines.Num());
	TArray<FString> Fields;
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (Lines[LineIndex].IsEmpty())
		{
			continue;
		}
		SplitCsvLine(Lines[LineIndex], Fields);
		if (Fields.Num() <= CnColumn)
		{
			continue;
		}
		const int32 Id = FCString::Atoi(*Fields[IdColumn]);
		if (Id <= 0)
		{
			continue;
		}
		FEntry Entry;
		Entry.Cn = Fields[CnColumn];
		if (EnColumn != INDEX_NONE && Fields.Num() > EnColumn)
		{
			Entry.En = Fields[EnColumn];
		}
		Entries.Add(Id, MoveTemp(Entry));
	}
	UE_LOG(LogTemp, Display, TEXT("Rok UI text: parsed %d language entries from %s"),
		Entries.Num(), *Path);
	return Entries;
}

FString FRokUiTextTable::SanitizeRichText(const FString& In)
{
	FString Out;
	Out.Reserve(In.Len());
	for (int32 Index = 0; Index < In.Len(); ++Index)
	{
		const TCHAR Character = In[Index];
		if (Character == TEXT('<'))
		{
			const int32 Close = In.Find(TEXT(">"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Index);
			if (Close != INDEX_NONE)
			{
				Index = Close;
				continue;
			}
		}
		if (Character == TEXT('{'))
		{
			const int32 Close = In.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Index);
			if (Close != INDEX_NONE)
			{
				Index = Close;
				continue;
			}
		}
		Out.AppendChar(Character);
	}
	return Out.TrimStartAndEnd();
}
FString FRokUiTextTable::Resolve(int32 NameId)
{
	return Resolve(NameId, TEXT("cn"));
}

FString FRokUiTextTable::Resolve(int32 NameId, const FString& Column)
{
	if (NameId <= 0)
	{
		return FString();
	}
	const TMap<int32, FEntry>& Table = GetEntries();
	const FEntry* Entry = Table.Find(NameId);
	if (!Entry)
	{
		return FString();
	}
	return Column.Equals(TEXT("en"), ESearchCase::IgnoreCase) ? Entry->En : Entry->Cn;
}

int32 FRokUiTextTable::NumEntries()
{
	return GetEntries().Num();
}

void FRokUiTextTable::ResetForTesting()
{
	Entries.Reset();
	bLoaded = false;
}
