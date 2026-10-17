#pragma once
#include "CoreMinimal.h"
namespace RokLanguageSettingsLayout
{
// Existing button/action identities stay stable; Unity LanguageSet uses SystemLanguage IDs.
inline int32 SourceId(int32 ButtonIndex) { static const int32 Ids[] = {10,1,40,37}; return ButtonIndex>=0&&ButtonIndex<4?Ids[ButtonIndex]:INDEX_NONE; }
inline int32 VisualCell(int32 ButtonIndex) { static const int32 Cells[] = {1,0,3,2}; return ButtonIndex>=0&&ButtonIndex<4?Cells[ButtonIndex]:INDEX_NONE; }
inline FVector2D FramePosition() { return FVector2D(271,123); }
inline FVector2D FrameSize() { return FVector2D(898,564); }
inline FVector2D TitlePosition() { return FVector2D(307.75,133.5); }
inline FVector2D ViewportPosition() { return FVector2D(278.5,190.65); }
inline FVector2D ViewportSize() { return FVector2D(883,370.4); }
inline FVector2D CellPosition(int32 ButtonIndex)
{ const int32 Cell=VisualCell(ButtonIndex); return FVector2D(43.5+(Cell%2)*408,-1.5+(Cell/2)*111); }
inline constexpr int32 AppliedLanguageId = 40; // Current supported runtime Chinese preview, never inferred from OS locale.
inline bool CanCloseWithoutReload(int32 DraftId) { return DraftId == AppliedLanguageId; }
}
