#include "RokServerClock.h"

void FRokServerClock::Reset()
{
	bHasTime = false;
	AnchorServerSeconds = 0.0;
	AnchorMonotonicSeconds = 0.0;
	PingMilliseconds = -1.0;
	LastReturnedServerSeconds = 0.0;
}

bool FRokServerClock::IsValidServerMilliseconds(int64 ServerMilliseconds)
{
	// Reject zero, seconds, microseconds, and implausible epochs instead of
	// exposing an apparently synchronized but unusable countdown.
	return ServerMilliseconds >= 946684800000LL && ServerMilliseconds < 4102444800000LL;
}

bool FRokServerClock::SetAnchor(double ServerSeconds, double MonotonicSeconds)
{
	if (!FMath::IsFinite(ServerSeconds) || !FMath::IsFinite(MonotonicSeconds) || MonotonicSeconds < 0.0)
	{
		return false;
	}
	if (bHasTime)
	{
		double Previous = 0.0;
		TryGetServerTimeSeconds(MonotonicSeconds, Previous);
	}
	// Preserve the last observed value while allowing a slow server correction
	// to catch up. Clamping the anchor itself would retain a fast-clock error.
	AnchorServerSeconds = ServerSeconds;
	AnchorMonotonicSeconds = MonotonicSeconds;
	bHasTime = true;
	LastReturnedServerSeconds = FMath::Max(LastReturnedServerSeconds, ServerSeconds);
	return true;
}

bool FRokServerClock::SeedFromServerMilliseconds(int64 ServerMilliseconds, double ReceivedMonotonicSeconds)
{
	if (bHasTime || !IsValidServerMilliseconds(ServerMilliseconds)) return false;
	return SetAnchor(double(ServerMilliseconds) / 1000.0, ReceivedMonotonicSeconds);
}

bool FRokServerClock::UpdateFromHeartbeat(int64 ServerMilliseconds, double SentMonotonicSeconds, double ReceivedMonotonicSeconds)
{
	if (!IsValidServerMilliseconds(ServerMilliseconds) || !FMath::IsFinite(SentMonotonicSeconds)
		|| !FMath::IsFinite(ReceivedMonotonicSeconds) || SentMonotonicSeconds < 0.0
		|| ReceivedMonotonicSeconds < SentMonotonicSeconds
		|| ReceivedMonotonicSeconds - SentMonotonicSeconds > 60.0)
	{
		return false;
	}
	const double RoundTripSeconds = ReceivedMonotonicSeconds - SentMonotonicSeconds;
	if (!SetAnchor(double(ServerMilliseconds) / 1000.0 + RoundTripSeconds * 0.5, ReceivedMonotonicSeconds)) return false;
	PingMilliseconds = RoundTripSeconds * 1000.0;
	return true;
}

bool FRokServerClock::TryGetServerTimeSeconds(double MonotonicSeconds, double& OutServerSeconds) const
{
	if (!bHasTime || !FMath::IsFinite(MonotonicSeconds)) return false;
	const double ElapsedSeconds = FMath::Max(0.0, MonotonicSeconds - AnchorMonotonicSeconds);
	LastReturnedServerSeconds = FMath::Max(LastReturnedServerSeconds, AnchorServerSeconds + ElapsedSeconds);
	OutServerSeconds = LastReturnedServerSeconds;
	return true;
}
