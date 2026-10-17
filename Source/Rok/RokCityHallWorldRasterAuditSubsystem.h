#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityHallWorldRasterAuditSubsystem.generated.h"

class FJsonObject;
struct FRokCityHallWorldRasterAuditJob;

/** Opt-in local Game-world audit of the two sealed Hall world-pose UV raster fixtures. */
UCLASS()
class ROK_API URokCityHallWorldRasterAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
	bool Prepare();
	void StartJob();
	void StartBufferReadback();
	void StartFrame();
	void Poll();
	void RecordReady();
	void ReleaseJob();
	void Finish(const FString& Failure = FString());
	FString Directory, FixtureHash, ExpectedFixtureHash, SourceManifestHash;
	TSharedPtr<FJsonObject> Fixture, SourceManifest, NativeManifest;
	TSharedPtr<FRokCityHallWorldRasterAuditJob, ESPMode::ThreadSafe> Job;
	TArray<TSharedPtr<FJsonObject>> InputResults, FrameResults;
	TArray<FString> Failures;
	int32 BufferIndex = 0, CaseIndex = 0;
	uint64 InitialHttpDispatches = 0;
	double StartSeconds = 0.0, PendingSeconds = 0.0;
	bool bPrepared = false, bFinished = false;
};
