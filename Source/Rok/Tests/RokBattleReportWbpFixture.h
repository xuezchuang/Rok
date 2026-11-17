#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"

class URokUnityFeatureScreenWidget;
class FJsonObject;

void RokSetBattleReportFixtureForTest(
	URokUnityFeatureScreenWidget* Screen,
	const TMap<int64, TSharedPtr<FJsonObject>>& Mails,
	const TSet<int64>& FailedDownloads,
	int64 RoleId);
void RokClearBattleReportFixtureForTest(URokUnityFeatureScreenWidget* Screen);
#endif
