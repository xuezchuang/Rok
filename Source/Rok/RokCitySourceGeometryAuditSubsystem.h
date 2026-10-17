#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCitySourceGeometryAuditSubsystem.generated.h"

class FJsonObject;
struct FRokCitySourceGeometryAuditJob;

/** Explicit local Game-world audit of sealed source buffers in an isolated GPU target. */
UCLASS()
class ROK_API URokCitySourceGeometryAuditSubsystem : public UTickableWorldSubsystem
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
	void StartRow();
	void StartBufferReadback();
	void StartFrame();
	void Poll();
	void RecordReady();
	void ReleaseJob();
	void Finish(const FString& Failure = FString());
	FString Directory, FixtureHash, ExpectedFixtureHash, SourceManifestHash;
	TSharedPtr<FJsonObject> Fixture, SourceManifest, NativeManifest;
	TSharedPtr<FRokCitySourceGeometryAuditJob, ESPMode::ThreadSafe> Job;
	TArray<TSharedPtr<FJsonObject>> InputResults, FrameResults;
	TArray<FString> Failures;
	int32 RowIndex = 0, BufferIndex = 0;
	uint64 InitialHttpDispatches = 0;
	double StartSeconds = 0.0, PendingSeconds = 0.0;
	bool bPrepared = false, bFinished = false;
};
