#pragma once

#include "CoreMinimal.h"

/**
 * Runtime lookup into the Unity localization table (`Language.csv`:
 * `id,cn,en,...`). Unity tables reference text by `l_nameID`/`l_desID`; new UE
 * windows should resolve those ids here instead of hard-coding Chinese text.
 * The table is parsed once on first use (3.7 MB CSV, ~20k ids).
 */
class ROK_API FRokUiTextTable
{
public:
	// Column `cn`; returns an empty string when the id is unknown.
	static FString Resolve(int32 NameId);
	static FString Resolve(int32 NameId, const FString& Column);
	static int32 NumEntries();
	// Unity strings carry rich-text markup (<b>, <color=orange>) and {pN}
	// placeholders that UMG cannot render; this strips them for display.
	static FString SanitizeRichText(const FString& In);
	// Drops the parsed table (tests only).
	static void ResetForTesting();

private:
	struct FEntry
	{
		FString Cn;
		FString En;
	};
	static const TMap<int32, FEntry>& GetEntries();
	static TMap<int32, FEntry> Entries;
	static bool bLoaded;
};
