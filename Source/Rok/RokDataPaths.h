#pragma once

#include "CoreMinimal.h"

// Generated, non-asset runtime data (JSON/CSV/PNG) produced by the Scripts/ migration
// generators. It lives in Content/RokData and is staged as UFS through
// DirectoriesToAlwaysStageAsUFS in DefaultGame.ini, so it ships with the game.
// Verify reports and Unity evidence stay in Saved/RokUnityMigration.
namespace RokDataPaths
{
	// Project-relative root as written by the generators: "Content/RokData/".
	ROK_API const TCHAR* ProjectRelativeRoot();

	// <ProjectContentDir>/RokData
	ROK_API FString Root();

	// Root() / Relative, e.g. Resolve(TEXT("unity_gameplay_catalog.json")).
	ROK_API FString Resolve(const FString& Relative);

	// Resolves a project-relative path read from generated JSON or a provenance list.
	// Data generated before the move still names its inputs "Saved/RokUnityMigration/..."
	// or "Saved/PendingUnityTileRipple*/..."; when that file now lives under Root() the
	// moved copy is returned, so pinned bytes and hashes stay valid. Any other path
	// resolves against FPaths::ProjectDir() exactly as before.
	ROK_API FString ResolveProjectPath(const FString& ProjectRelative);

	// True for a project-relative path under ProjectRelativeRoot() without "..".
	ROK_API bool IsDataPath(const FString& ProjectRelative);
}
