#include "RokCsv.h"

namespace FRokCsv
{
	void ParseRecords(const FString& Text, TArray<TArray<FString>>& OutRecords)
	{
		OutRecords.Reset();
		TArray<FString> Record;
		FString Field;
		bool bInQuotes = false;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Character = Text[Index];
			if (bInQuotes)
			{
				if (Character == TEXT('"'))
				{
					if (Index + 1 < Text.Len() && Text[Index + 1] == TEXT('"'))
					{
						Field.AppendChar(TEXT('"'));
						++Index;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					Field.AppendChar(Character);
				}
				continue;
			}
			if (Character == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Character == TEXT(','))
			{
				Record.Add(Field);
				Field.Reset();
			}
			else if (Character == TEXT('\n'))
			{
				Record.Add(Field);
				Field.Reset();
				// Tolerate CRLF and trailing empty last record.
				if (Record.Num() > 1 || !Record[0].IsEmpty())
				{
					OutRecords.Add(MoveTemp(Record));
				}
				Record.Reset();
			}
			else if (Character != TEXT('\r'))
			{
				Field.AppendChar(Character);
			}
		}
		if (!Field.IsEmpty() || Record.Num() > 0)
		{
			Record.Add(Field);
			OutRecords.Add(MoveTemp(Record));
		}
	}

	int32 FindColumn(const TArray<FString>& Header, const FString& Name)
	{
		for (int32 Column = 0; Column < Header.Num(); ++Column)
		{
			if (Header[Column].TrimStartAndEnd() == Name)
			{
				return Column;
			}
		}
		return INDEX_NONE;
	}
}
