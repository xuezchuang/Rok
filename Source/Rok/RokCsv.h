#pragma once

#include "CoreMinimal.h"

/**
 * Unity config CSVs quote fields that contain commas AND newlines (the
 * `RechargeLimitTimeBag.des` column is a multi-line marketing blurb). The
 * line-based splitters in the widgets cannot read those rows, so this reader
 * walks the whole text and only ends a record on a newline outside quotes.
 */
namespace FRokCsv
{
	// Splits `Text` into records of fields (RFC 4180 subset: `""` escapes a quote,
	// CRLF and LF both end a record, quoted newlines stay inside the field).
	ROK_API void ParseRecords(const FString& Text, TArray<TArray<FString>>& OutRecords);

	// Header name -> index for the first matching column (Unity tables repeat
	// names such as `type`; the first occurrence wins, matching the old splitters).
	ROK_API int32 FindColumn(const TArray<FString>& Header, const FString& Name);
}
