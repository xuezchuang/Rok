#pragma once

#include "CoreMinimal.h"

/**
 * Sprite manifest reader for the Unity TexturePacker sheets (`*.tpsheet`).
 *
 * Unity UI icons are referenced as `atlas[sprite]` (e.g. `item4[item_equip0debris_100]`).
 * The sheets under `ref/client-unity/Assets/**` carry the exact pixel rects, so this
 * resolves a sprite to `(sheet png, x, y, w, h)` and can decode the sheet once into a
 * transient `UTexture2D` for preview rendering. Nothing is imported or cooked.
 */
struct ROK_API FRokSpriteRect
{
	FString SheetPng;
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;
	int32 SheetWidth = 0;
	int32 SheetHeight = 0;
};

class ROK_API FRokSpriteAtlas
{
public:
	/** Scans `ref/client-unity/Assets` once; returns the number of indexed sprites. */
	int32 LoadAll();

	/** `atlas[sprite]` -> rect. Accepts a bare sprite name too. */
	bool Resolve(const FString& IconReference, FRokSpriteRect& Out) const;

	/** Decodes (and caches) the sheet png into a transient texture. */
	UTexture2D* GetSheetTexture(const FString& SheetPng);

	int32 NumSprites() const { return Entries.Num(); }
	int32 NumSheets() const { return SheetsScanned.Num(); }

	// Test hooks.
	void Reset();

private:
	void IndexSheet(const FString& SheetFile);

	TMap<FString, FRokSpriteRect> Entries;
	TMap<FString, FString> TextureToSheet;
	TSet<FString> SheetsScanned;
	TMap<FString, TObjectPtr<UTexture2D>> Textures;
};
