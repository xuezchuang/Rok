#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RokCityGammaFamilyInputAuditSubsystem.generated.h"

class FJsonObject;
struct FRokGammaFamilyGpuJob;

/** Opt-in, local-only proof of the ten Unity family textures and all 77 GPU mip levels. */
UCLASS()
class ROK_API URokCityGammaFamilyInputAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;

private:
	struct FMip
	{
		int32 Level = 0, Width = 0, Height = 0;
		FString File, Sha;
		TArray<uint8> Bytes;
	};
	struct FTexture
	{
		FString Id, Guid, SourcePath, SourceSha, Filter, Wrap;
		int32 Width = 0, Height = 0;
		TArray<FMip> Mips;
	};
	bool Prepare();
	bool StartTexture();
	void StartMip();
	void PollMip();
	void RecordMip();
	void ReleaseActive();
	void Finish(const FString& Failure = FString());
	FString RunDirectory, InputSha, ContractSha, NativeSha, NativeVerifySha;
	TArray<FTexture> Textures;
	TArray<TSharedPtr<FJsonObject>> TextureRecords, MipRecords;
	TArray<FString> Failures;
	TSharedPtr<FRokGammaFamilyGpuJob, ESPMode::ThreadSafe> Active;
	int32 TextureIndex = 0, MipIndex = 0, FamilySlotCount = 0;
	uint64 InitialHttpDispatches = 0;
	double StartSeconds = 0.0, PendingSeconds = 0.0;
	bool bPrepared = false, bFinished = false;
};
