#pragma once

#include "CoreMinimal.h"

// Arms.icon -> UITextureStatic/ui_res_troops/troops_head.tpsheet.
// Rectangles retain Unity bottom-origin coordinates for MakeAtlasBrush.
namespace RokTroopPortrait
{
inline bool Resolve(const FString& Icon, FIntRect& Out)
{
	struct FEntry { const TCHAR* Icon; int32 X, Y, W, H; };
	static const FEntry Entries[] = {
		{TEXT("troops_head[troops_head1_1]"), 393, 513, 126, 126},
		{TEXT("troops_head[troops_head1_2]"), 785, 897, 126, 126},
		{TEXT("troops_head[troops_head1_3]"), 529, 769, 126, 126},
		{TEXT("troops_head[troops_head1_4]"), 657, 897, 126, 126},
		{TEXT("troops_head[troops_head1_5]"), 393, 641, 126, 126},
		{TEXT("troops_head[troops_head1_s1]"), 401, 769, 126, 126},
		{TEXT("troops_head[troops_head1_s2]"), 529, 897, 126, 126},
		{TEXT("troops_head[troops_head2_1]"), 401, 897, 126, 126},
		{TEXT("troops_head[troops_head2_2]"), 385, 1, 126, 126},
		{TEXT("troops_head[troops_head2_3]"), 385, 129, 126, 126},
		{TEXT("troops_head[troops_head2_4]"), 385, 257, 126, 126},
		{TEXT("troops_head[troops_head2_5]"), 257, 1, 126, 126},
		{TEXT("troops_head[troops_head2_s1]"), 257, 129, 126, 126},
		{TEXT("troops_head[troops_head2_s2]"), 257, 257, 126, 126},
		{TEXT("troops_head[troops_head2_s3]"), 265, 385, 126, 126},
		{TEXT("troops_head[troops_head3_1]"), 265, 513, 126, 126},
		{TEXT("troops_head[troops_head3_2]"), 265, 641, 126, 126},
		{TEXT("troops_head[troops_head3_3]"), 273, 769, 126, 126},
		{TEXT("troops_head[troops_head3_4]"), 129, 103, 126, 126},
		{TEXT("troops_head[troops_head3_5]"), 129, 231, 126, 126},
		{TEXT("troops_head[troops_head3_s1]"), 129, 359, 126, 126},
		{TEXT("troops_head[troops_head3_s2]"), 137, 487, 126, 126},
		{TEXT("troops_head[troops_head3_s3]"), 137, 615, 126, 126},
		{TEXT("troops_head[troops_head4_1]"), 1, 51, 126, 126},
		{TEXT("troops_head[troops_head4_2]"), 1, 179, 126, 126},
		{TEXT("troops_head[troops_head4_3]"), 1, 307, 126, 126},
		{TEXT("troops_head[troops_head4_4]"), 137, 743, 126, 126},
		{TEXT("troops_head[troops_head4_5]"), 1, 435, 126, 126},
		{TEXT("troops_head[troops_head4_s1]"), 273, 897, 126, 126},
	};
	for (const FEntry& Entry : Entries)
	{
		if (Icon == Entry.Icon)
		{
			Out = FIntRect(Entry.X, Entry.Y, Entry.X + Entry.W, Entry.Y + Entry.H);
			return true;
		}
	}
	return false;
}
}
