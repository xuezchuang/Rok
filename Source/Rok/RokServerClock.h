#pragma once

#include "CoreMinimal.h"

// Server timestamps are Unix milliseconds. The local anchor uses a monotonic clock,
// so a wall-clock adjustment cannot change an active construction countdown.
class FRokServerClock
{
public:
	void Reset();
	bool HasTime() const { return bHasTime; }
	bool SeedFromServerMilliseconds(int64 ServerMilliseconds, double ReceivedMonotonicSeconds);
	bool UpdateFromHeartbeat(int64 ServerMilliseconds, double SentMonotonicSeconds, double ReceivedMonotonicSeconds);
	bool TryGetServerTimeSeconds(double MonotonicSeconds, double& OutServerSeconds) const;
	double GetPingMilliseconds() const { return PingMilliseconds; }

private:
	static bool IsValidServerMilliseconds(int64 ServerMilliseconds);
	bool SetAnchor(double ServerSeconds, double MonotonicSeconds);

	bool bHasTime = false;
	double AnchorServerSeconds = 0.0;
	double AnchorMonotonicSeconds = 0.0;
	double PingMilliseconds = -1.0;
	mutable double LastReturnedServerSeconds = 0.0;
};
