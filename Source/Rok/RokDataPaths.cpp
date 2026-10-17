#include "RokDataPaths.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
struct FRokLegacyDataPrefix
{
	const TCHAR* Legacy;
	const TCHAR* Data;
};

// Pre-move locations that generated JSON and pinned provenance still record.
const FRokLegacyDataPrefix LegacyDataPrefixes[] = {
	{TEXT("Saved/RokUnityMigration/"), TEXT("")},
	{TEXT("Saved/PendingUnityTileRippleRendererV8/"), TEXT("TileRippleRendererV8/")},
	{TEXT("Saved/PendingUnityTileRippleSimulationV2/"), TEXT("TileRippleSimulationV2/")},
};
}

const TCHAR* RokDataPaths::ProjectRelativeRoot()
{
	return TEXT("Content/RokData/");
}

FString RokDataPaths::Root()
{
	return FPaths::ProjectContentDir() / TEXT("RokData");
}

FString RokDataPaths::Resolve(const FString& Relative)
{
	return Root() / Relative;
}

FString RokDataPaths::ResolveProjectPath(const FString& ProjectRelative)
{
	for (const FRokLegacyDataPrefix& Prefix : LegacyDataPrefixes)
	{
		if (ProjectRelative.StartsWith(Prefix.Legacy, ESearchCase::CaseSensitive))
		{
			const FString Moved = Resolve(
				Prefix.Data + ProjectRelative.RightChop(FCString::Strlen(Prefix.Legacy)));
			if (IFileManager::Get().FileExists(*Moved))
			{
				return Moved;
			}
			break;
		}
	}
	return FPaths::ProjectDir() / ProjectRelative;
}

bool RokDataPaths::IsDataPath(const FString& ProjectRelative)
{
	return ProjectRelative.StartsWith(ProjectRelativeRoot(), ESearchCase::CaseSensitive)
		&& !ProjectRelative.Contains(TEXT(".."));
}
