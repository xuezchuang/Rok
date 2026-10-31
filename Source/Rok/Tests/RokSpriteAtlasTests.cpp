#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../RokSpriteAtlas.h"

// Unity icons are `atlas[sprite]`; the `.tpsheet` files carry the exact rects.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRokSpriteAtlasTest,
	"Rok.Ui.SpriteAtlas.Resolve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRokSpriteAtlasTest::RunTest(const FString& Parameters)
{
	FRokSpriteAtlas Atlas;
	const int32 Sprites = Atlas.LoadAll();
	TestTrue(TEXT("The manifest indexes sprites from the tpsheet files"), Sprites > 1000);
	TestTrue(TEXT("Hundreds of sheets are scanned"), Atlas.NumSheets() > 100);

	FRokSpriteRect Rect;
	if (!TestTrue(TEXT("item4[item_equip0debris_100] resolves"),
		Atlas.Resolve(TEXT("item4[item_equip0debris_100]"), Rect)))
	{
		return false;
	}
	TestEqual(TEXT("The sprite x matches the sheet"), Rect.X, 897);
	TestEqual(TEXT("The sprite y matches the sheet"), Rect.Y, 257);
	TestEqual(TEXT("The sprite is 126px wide"), Rect.Width, 126);
	TestEqual(TEXT("The sheet is 2048px wide"), Rect.SheetWidth, 2048);
	TestTrue(TEXT("The sheet png path ends with item4.png"), Rect.SheetPng.EndsWith(TEXT("item4.png")));

	FRokSpriteRect Bare;
	TestTrue(TEXT("A bare sprite name also resolves"),
		Atlas.Resolve(TEXT("item_equip0debris_100"), Bare));

	FRokSpriteRect Medal;
	if (TestTrue(TEXT("item5[item_5_1008] resolves"),
		Atlas.Resolve(TEXT("item5[item_5_1008]"), Medal)))
	{
		TestTrue(TEXT("The medal sheet decodes into a texture"),
			Atlas.GetSheetTexture(Medal.SheetPng) != nullptr);
	}

	FRokSpriteRect HeroHead;
	if (TestTrue(TEXT("hero_head[hero_head_3002] resolves"),
		Atlas.Resolve(TEXT("hero_head[hero_head_3002]"), HeroHead)))
	{
		TestEqual(TEXT("The commander head is 134px wide"), HeroHead.Width, 134);
	}

	FRokSpriteRect Missing;
	TestFalse(TEXT("An unknown sprite does not resolve"),
		Atlas.Resolve(TEXT("item4[nope_missing_sprite]"), Missing));
	return true;
}

#endif
