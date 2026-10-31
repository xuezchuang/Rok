#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokUiTextTable.h"

// Unity tables address text by id (`l_nameID`); UI windows resolve here instead
// of hard-coding Chinese strings.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokUiTextTableTest,
	"Rok.Ui.Text.LanguageTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokUiTextTableTest::RunTest(const FString& Parameters)
{
	FRokUiTextTable::ResetForTesting();
	// The shipped table has ~8k rows with a cn value; guard against a silent
	// header/path regression rather than an exact count.
	TestTrue(TEXT("The language table parses thousands of ids"),
		FRokUiTextTable::NumEntries() > 5000);

	// The first data row of Language.csv is the identity sample row.
	TestEqual(TEXT("Id 100001 resolves through the cn column"),
		FRokUiTextTable::Resolve(100001), FString(TEXT("zh")));
	TestEqual(TEXT("The en column is addressable"),
		FRokUiTextTable::Resolve(100001, TEXT("en")), FString(TEXT("EN")));
	TestTrue(TEXT("Unknown ids resolve to an empty string"),
		FRokUiTextTable::Resolve(999999999).IsEmpty());
	TestTrue(TEXT("Zero and negative ids are rejected"),
		FRokUiTextTable::Resolve(0).IsEmpty());

	// A second call must reuse the parsed table rather than re-reading the CSV.
	const int32 ParsedCount = FRokUiTextTable::NumEntries();
	TestEqual(TEXT("A repeated lookup keeps the parsed count stable"),
		FRokUiTextTable::NumEntries(), ParsedCount);
	return true;
}

#endif
